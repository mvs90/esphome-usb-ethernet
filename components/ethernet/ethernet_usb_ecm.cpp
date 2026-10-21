// SPDX-License-Identifier: GPL-3.0-only
// USB ECM backend for the local ESPHome 2026.9.1 Ethernet overlay.
#include "ethernet_component.h"
#include "realtek_multicast.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include "iot_usbh_ecm.h"
#include "realtek_bulk.h"
#include "realtek_adapter.h"
#include "realtek_retry_deadline.h"
#include "iot_usbh_cdc.h"
#include "usb/usb_host.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdlib>

namespace esphome::ethernet {
static const char *const TAG = "ethernet.usb_ecm";

bool EthernetComponent::check_error_(esp_err_t error, const char *operation) {
  if (error == ESP_OK) return true;
  ESP_LOGE(TAG, "%s: %s", operation, esp_err_to_name(error));
  this->mark_failed();
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    ESP_LOGE(TAG, "Network initialization failed in unconfirmed OTA image; requesting rollback");
    esp_err_t rollback = esp_ota_mark_app_invalid_rollback_and_reboot();
    ESP_LOGE(TAG, "Rollback unavailable: %s", esp_err_to_name(rollback));
  }
  return false;
}

static bool select_configuration(const usb_device_desc_t *d, uint8_t *configuration) {
  if (rtl_adapter::supported(d->idVendor, d->idProduct, d->bcdDevice) &&
      d->bNumConfigurations == 2)
#ifdef USE_ETHERNET_REALTEK_BULK
    *configuration = 1;
#else
    *configuration = 2;
#endif  // This exact descriptor tuple was measured on the adapter.
  ESP_LOGI(TAG, "USB %04x:%04x bcd=%04x config=%u", d->idVendor, d->idProduct,
           d->bcdDevice, *configuration);
  return true;
}

static void host_events(void *) {
  for (;;) {
    uint32_t flags = 0;
    ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      esp_err_t err = usb_host_device_free_all();
      ESP_ERROR_CHECK(err == ESP_ERR_NOT_FINISHED ? ESP_OK : err);
    }
  }
}

void EthernetComponent::setup() {
  this->status_set_warning(LOG_STR("waiting for USB ECM DHCP"));
  // network component owns esp_netif_init() and the default event loop.
  esp_netif_config_t netcfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_inherent_config_t inherent=*netcfg.base;
  if(!this->ipv4_enabled_)inherent.flags=esp_netif_flags_t(inherent.flags & ~ESP_NETIF_DHCP_CLIENT);
  netcfg.base=&inherent;
  this->eth_netif_ = esp_netif_new(&netcfg);
  if (!this->eth_netif_) {
    this->mark_failed();
    return;
  }
#ifdef USE_ETHERNET_MANUAL_IP
  // Configure before START/CONNECTED so the glue never starts DHCP.
  if (!this->check_error_(esp_netif_dhcpc_stop(this->eth_netif_), "esp_netif_dhcpc_stop")) return;
  esp_netif_ip_info_t ip{};
  ip.ip = static_cast<esp_ip4_addr_t>(this->manual_ip_.static_ip);
  ip.gw = static_cast<esp_ip4_addr_t>(this->manual_ip_.gateway);
  ip.netmask = static_cast<esp_ip4_addr_t>(this->manual_ip_.subnet);
  if (!this->check_error_(esp_netif_set_ip_info(this->eth_netif_, &ip), "esp_netif_set_ip_info")) return;
  esp_netif_dns_info_t dns{};
  dns.ip.type = ESP_IPADDR_TYPE_V4;
  if (this->manual_ip_.dns1.is_set()) {
    dns.ip.u_addr.ip4 = static_cast<esp_ip4_addr_t>(this->manual_ip_.dns1);
    if (!this->check_error_(esp_netif_set_dns_info(this->eth_netif_, ESP_NETIF_DNS_MAIN, &dns), "set DNS1")) return;
  }
  if (this->manual_ip_.dns2.is_set()) {
    dns.ip.u_addr.ip4 = static_cast<esp_ip4_addr_t>(this->manual_ip_.dns2);
    if (!this->check_error_(esp_netif_set_dns_info(this->eth_netif_, ESP_NETIF_DNS_BACKUP, &dns), "set DNS2")) return;
  }
#endif
  if (!this->check_error_(esp_netif_set_hostname(this->eth_netif_, App.get_name().c_str()), "esp_netif_set_hostname")) return;
  if (!this->check_error_(esp_event_handler_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, usb_event_, this), "esp_event_handler_register")) return;
  if (!this->check_error_(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, usb_event_, this), "esp_event_handler_register")) return;
  if (!this->check_error_(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, usb_event_, this), "esp_event_handler_register")) return;

  usb_host_config_t host{};
  host.root_port_unpowered = true;
  host.intr_flags = ESP_INTR_FLAG_LEVEL1;
  host.enum_filter_cb = select_configuration;
  if (!this->check_error_(usb_host_install(&host), "usb_host_install")) return;  // Sole owner; usb_host YAML is rejected.
  if (!this->check_error_(xTaskCreate(host_events, "ecm_usb_host", 4096, nullptr, 5, nullptr) == pdPASS
                      ? ESP_OK : ESP_ERR_NO_MEM, "xTaskCreate")) return;
#ifndef USE_ETHERNET_REALTEK_BULK
  usbh_cdc_driver_config_t cdc{};
  cdc.task_stack_size = 4096;
  cdc.task_priority = 5;
  cdc.task_coreid = 0;
  cdc.skip_init_usb_host_driver = true;
  if (!this->check_error_(usbh_cdc_driver_install(&cdc), "usbh_cdc_driver_install")) return;

#endif

  // Static match table must outlive the driver. ECM validates the interfaces.
  static const usb_device_match_id_t match[] = {
      {.match_flags = USB_DEVICE_ID_MATCH_VID_PID,
       .idVendor = USB_DEVICE_VENDOR_ANY, .idProduct = USB_DEVICE_PRODUCT_ANY}, {}};
  iot_usbh_ecm_config_t ecm{};
  ecm.match_id_list = match;
  iot_eth_driver_t *driver = nullptr;
#ifdef USE_ETHERNET_REALTEK_BULK
  if (!this->check_error_(new_realtek_bulk(&driver), "new_realtek_bulk")) return;
  realtek_bulk_tx_mode(driver, this->native_tx_offload_ ? 2 : 0);
  realtek_bulk_aggregation(driver,this->native_tx_aggregation_,this->native_rx_aggregation_);
  realtek_bulk_tso(driver,this->native_tso_);
#else
  if (!this->check_error_(iot_eth_new_usb_ecm(&ecm, &driver), "iot_eth_new_usb_ecm")) return;
#endif
  this->usb_driver_ = driver;
  iot_eth_config_t eth{};
  eth.driver = driver;
  if (!this->check_error_(iot_eth_install(&eth, &this->usb_eth_), "iot_eth_install")) return;
  this->glue_.base.post_attach = attach_;
  this->glue_.owner = this;
  if (!this->check_error_(esp_netif_attach(this->eth_netif_, &this->glue_), "esp_netif_attach")) return;
  if (!this->check_error_(esp_netif_set_default_netif(this->eth_netif_), "esp_netif_set_default_netif")) return;
  this->ethernet_initialized_ = true;
  this->started_ = true;
  this->state_ = EthernetComponentState::CONNECTING;
  if (!this->check_error_(iot_eth_start(this->usb_eth_), "iot_eth_start")) return;
  // Cold-boot devices cannot enumerate until the ECM callback is registered.
  if (!this->check_error_(usb_host_lib_set_root_port_power(true), "usb_host_lib_set_root_port_power")) return;
}

void EthernetComponent::usb_event_(void *arg, esp_event_base_t base, int32_t id, void *data) {
  auto *self = static_cast<EthernetComponent *>(arg);
  if (base == IOT_ETH_EVENT) {
    if (data == nullptr || *static_cast<iot_eth_handle_t *>(data) != self->usb_eth_) return;
    if (id == IOT_ETH_EVENT_START) esp_netif_action_start(self->eth_netif_, base, id, data);
    if (id == IOT_ETH_EVENT_CONNECTED) {
      self->adapter_ready_.store(false);
      self->usb_link_.store(true);
      self->pending_connect_.store(true);
    }
    if (id == IOT_ETH_EVENT_DISCONNECTED) {
      self->adapter_ready_.store(false);
      self->pending_connect_.store(false);
      esp_netif_action_disconnected(self->eth_netif_, base, id, data);
      self->usb_link_.store(false);
      self->usb_ip_.store(false);
    }
    if (id == IOT_ETH_EVENT_STOP) {
      self->adapter_ready_.store(false);
      self->pending_connect_.store(false);
      self->usb_ip_.store(false);
      esp_netif_action_stop(self->eth_netif_, base, id, data);
    }
  } else if (id == IP_EVENT_ETH_GOT_IP) {
    auto *event = static_cast<ip_event_got_ip_t *>(data);
    if (event->esp_netif == self->eth_netif_) self->usb_ip_.store(event->ip_info.ip.addr != 0);
  } else if (id == IP_EVENT_ETH_LOST_IP) {
    // Only one Ethernet netif is supported by the schema.
    self->usb_ip_.store(false);
  }
}

void EthernetComponent::loop() {
#ifdef USE_ETHERNET_REALTEK_BULK
  if(this->usb_driver_) {
    realtek_bulk_poll(this->usb_driver_);
    if(realtek_bulk_reinitialized(this->usb_driver_)){this->adapter_configured_=false;this->adapter_ready_=false;}
    if(realtek_bulk_resume_ready(this->usb_driver_))this->enable();
  }
  if(millis()>25000 && !this->is_connected()) {
    esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY)
      esp_ota_mark_app_invalid_rollback_and_reboot();
  }
#endif
  if (millis() - this->usb_last_poll_ < 100) return;
  this->usb_last_poll_ = millis();
  if (!this->disabled_ && this->usb_link_.load() &&
      // Zero means no deferred retry. Do not compare that sentinel as an actual
      // deadline: after 2^31 ms it would prevent ordinary reconnects.
      realtek_retry_ready(millis(),this->adapter_retry_at_) && this->pending_connect_.exchange(false)) {
    this->adapter_ready_=false;
#ifdef USE_ETHERNET_REALTEK_BULK
    const bool configured_before=this->adapter_configured_;
#endif
    if(this->configure_adapter_() && this->usb_link_.load()) {
#ifdef USE_ETHERNET_REALTEK_BULK
      // Initial EEE/link configuration restarts PHY autonegotiation after the
      // first LINK_UP. Never expose that stale carrier to DHCP/IPv6. Return
      // to the event loop once, then read current PHY carrier before IP start.
      uint8_t phy=0;
      const bool current_link=configured_before && this->open_realtek_()==ESP_OK &&
          this->realtek_.phy_status(phy)==ESP_OK && (phy&2);
      if(!current_link) {
        this->adapter_retry_at_=millis()+250;
        this->pending_connect_.store(true);
        ESP_LOGD(TAG,"Waiting for fresh PHY carrier after adapter configuration");
      } else
#endif
      {
        this->adapter_retry_at_=0;this->adapter_ready_=true;
        esp_netif_action_connected(this->eth_netif_, IOT_ETH_EVENT, IOT_ETH_EVENT_CONNECTED, nullptr);
#if USE_NETWORK_IPV6
        if (!this->check_error_(esp_netif_create_ip6_linklocal(this->eth_netif_), "create IPv6 link-local")) return;
#endif
      }
    }
  }
  esp_netif_ip_info_t current_ip{};
  bool ipv4_ready=this->adapter_ready_.load() && this->ipv4_enabled_ && this->usb_ip_.load() &&
      esp_netif_get_ip_info(this->eth_netif_,&current_ip)==ESP_OK && current_ip.ip.addr!=0;
  bool ipv6_ready = true,has_ipv6=false;
#if USE_NETWORK_IPV6
  esp_ip6_addr_t preferred[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
  int preferred_count=esp_netif_get_all_preferred_ip6(this->eth_netif_, preferred);
  has_ipv6=preferred_count>0;
  ipv6_ready=preferred_count>=USE_NETWORK_MIN_IPV6_ADDR_COUNT;
#endif
  bool connected = this->adapter_ready_.load() && ipv6_ready && !this->disabled_ && this->usb_link_.load() && (ipv4_ready || has_ipv6);
  bool changed = connected != this->is_connected();
  this->connected_ = connected;
  this->got_ipv4_address_ = ipv4_ready;
  this->state_ = connected ? EthernetComponentState::CONNECTED : EthernetComponentState::CONNECTING;
  if (changed && connected) {
    this->status_clear_warning();
    auto ips = this->get_ip_addresses();
    char ip[network::IP_ADDRESS_BUFFER_SIZE]{};
    for(const auto &address:ips)if(address.is_set()){address.str_to(ip);break;}
    ESP_LOGI(TAG, "Connected via USB Ethernet, IP=%s", ip);
#ifdef USE_ETHERNET_CONNECT_TRIGGER
    this->connect_trigger_.trigger();
#endif
  } else if (changed) {
    this->status_set_warning(LOG_STR("USB ECM link/IP lost"));
    ESP_LOGW(TAG, "USB ECM disconnected");
#ifdef USE_ETHERNET_DISCONNECT_TRIGGER
    this->disconnect_trigger_.trigger();
#endif
  }
#ifdef USE_ETHERNET_IP_STATE_LISTENERS
  auto ips = this->get_ip_addresses();
  auto dns1 = this->get_dns_address(0);
  auto dns2 = this->get_dns_address(1);
  if (changed || ips != this->last_ips_ || dns1 != this->last_dns1_ || dns2 != this->last_dns2_) {
    this->last_ips_ = ips;
    this->last_dns1_ = dns1;
    this->last_dns2_ = dns2;
    this->notify_ip_state_listeners_();
  }
#endif
}

network::IPAddresses EthernetComponent::get_ip_addresses() {
  network::IPAddresses addresses{};
  esp_netif_ip_info_t ip{};
  if (!this->disabled_ && this->eth_netif_ && this->usb_ip_.load() &&
      esp_netif_get_ip_info(this->eth_netif_, &ip) == ESP_OK)
    addresses[0] = network::IPAddress(&ip.ip);
#if USE_NETWORK_IPV6
  if (!this->disabled_ && this->usb_link_.load() && this->eth_netif_) {
    esp_ip6_addr_t ip6[CONFIG_LWIP_IPV6_NUM_ADDRESSES]{};
    int count = esp_netif_get_all_preferred_ip6(this->eth_netif_, ip6);
    for (int i = 0; i < count && size_t(i + 1) < addresses.size(); i++)
      addresses[i + 1] = network::IPAddress(&ip6[i]);
  }
#endif
  return addresses;
}

network::IPAddress EthernetComponent::get_dns_address(uint8_t num) {
  esp_netif_dns_info_t dns{};
  if (!this->disabled_ && this->usb_link_.load() && this->eth_netif_ &&
      esp_netif_get_dns_info(this->eth_netif_, num == 0 ? ESP_NETIF_DNS_MAIN : ESP_NETIF_DNS_BACKUP, &dns) == ESP_OK)
    return network::IPAddress(&dns.ip);
  return {};
}

void EthernetComponent::get_eth_mac_address_raw(uint8_t *mac) {
  memset(mac, 0, MAC_ADDRESS_SIZE);
  if (this->eth_netif_) esp_netif_get_mac(this->eth_netif_, mac);
}

esp_err_t EthernetComponent::receive_(iot_eth_handle_t, uint8_t *data, size_t length, void *arg) {
  auto *self=static_cast<EthernetComponent *>(arg);
  if(length>=14) {
    uint16_t type=(uint16_t(data[12])<<8)|data[13];unsigned n=self->l2_count_.load();
    for(unsigned i=0;i<n;i++)if(self->l2_[i].type==type&&self->l2_[i].handler(self->l2_[i].context,data,length)) {
      free(data);return ESP_OK;
    }
  }
  return esp_netif_receive(self->eth_netif_, data, length, nullptr);
}
esp_err_t EthernetComponent::transmit_l2(const uint8_t *data,size_t length) {
  if(!data||length<60||length>1514)return ESP_ERR_INVALID_ARG;
  if(!this->is_l2_ready())return ESP_ERR_INVALID_STATE;
  return iot_eth_transmit(this->usb_eth_,const_cast<uint8_t *>(data),length);
}
esp_err_t EthernetComponent::attach_(esp_netif_t *netif, void *arg) {
  auto *glue = static_cast<NetifGlue *>(arg);
  glue->base.netif = netif;
  esp_err_t error = iot_eth_update_input_path(glue->owner->usb_eth_, receive_, glue->owner);
  if (error != ESP_OK) return error;
  esp_netif_driver_ifconfig_t driver{};
  driver.handle = glue->owner->usb_eth_;
  driver.transmit = iot_eth_transmit;
  driver.driver_free_rx_buffer = [](void *, void *buffer) { free(buffer); };
  return esp_netif_set_driver_config(netif, &driver);
}
esp_err_t EthernetComponent::open_realtek_() {
  // Carrier loss must not block USB management: restoring a PHY setting and
  // reading its down-state are precisely needed while Ethernet is unavailable.
  // The backend validates that its USB port/device is still present.
  if (this->disabled_ || !this->usb_driver_) return ESP_ERR_INVALID_STATE;
#ifdef USE_ETHERNET_REALTEK_BULK
  return this->realtek_.open(realtek_bulk_port(this->usb_driver_));
#else
  return this->realtek_.open(usb_ecm_get_cdc_port_handle(this->usb_driver_));
#endif
}
bool EthernetComponent::configure_adapter_() {
#ifdef USE_RTL_BOOT_TALLY_PROBE
  realtek_native_boot_tally_record(2,"before_configuration");
#endif
#if defined(USE_RTL_FAULT_INJECTION) && defined(USE_ETHERNET_REALTEK_BULK)
  if(this->inject_configure_timeout_){this->inject_configure_timeout_=false;realtek_native_test_timeout_next();}
#endif
  auto verified=[this](esp_err_t error,const char *operation)->bool {
    if(error==ESP_OK)return true;
    if(error==ESP_ERR_NOT_SUPPORTED)return this->check_error_(error,operation);
    ESP_LOGW(TAG,"Adapter configuration will retry: %s (%s)",operation,esp_err_to_name(error));
    this->status_set_warning(LOG_STR("waiting for adapter configuration retry"));
    this->configuration_retries_++;this->adapter_retry_at_=millis()+500;this->pending_connect_=true;return false;
  };
  uint8_t mac[6]{};
  if (!verified(iot_eth_get_addr(this->usb_eth_, mac), "get ECM descriptor MAC")) return false;
  if (this->has_fixed_mac_) memcpy(mac, this->fixed_mac_.data(), 6);
  esp_err_t error = this->open_realtek_();
  if (error == ESP_OK) {
    // Restore the descriptor MAC as well: the USB device may remain powered
    // across an ESP32 OTA reboot after a previous custom-MAC firmware.
    if (!verified(this->realtek_.set_mac(mac), "program/read back volatile MAC")) return false;
    if (!this->adapter_configured_) {
      if (this->wake_ >= 0 && !verified(this->realtek_.set_wake(this->wake_), "set wake filters")) return false;
      if (this->eee_ >= 0 && !verified(this->realtek_.set_eee(this->eee_), "set EEE")) return false;
      if (this->has_realtek_config_ && !verified(this->realtek_.set_link(0x1e0, this->link_speed_ == 0, this->link_speed_ == 0 ? 100 : this->link_speed_, this->link_duplex_), "set link mode")) return false;
    }
  } else if (this->has_fixed_mac_ || this->link_speed_ != 0 || this->eee_ >= 0 || this->wake_ >= 0) {
    return verified(error, "Realtek configuration requires measured RTL8152 revision 4C10");
  } else if (error != ESP_ERR_NOT_SUPPORTED) {
    return verified(error, "Realtek identification");
  }
#ifdef USE_RTL_BOOT_TALLY_PROBE
  realtek_native_boot_tally_record(3,"after_configuration_before_ip");
#endif
  this->adapter_configured_ = true;
  return verified(esp_netif_set_mac(this->eth_netif_, mac), "set netif MAC before DHCP/IPv6");
}
std::string EthernetComponent::read_realtek_diagnostics() {
  esp_err_t error = this->open_realtek_();
  return error == ESP_OK ? this->realtek_.diagnostics() : std::string("error: ") + esp_err_to_name(error);
}
std::string EthernetComponent::read_realtek_native_registers() {
  esp_err_t error=this->open_realtek_();
  return error==ESP_OK?this->realtek_.native_registers():std::string("error: ")+esp_err_to_name(error);
}
esp_err_t EthernetComponent::set_native_tx_aggregation(bool enabled) {
#ifdef USE_ETHERNET_REALTEK_BULK
  if(!this->usb_driver_)return ESP_ERR_INVALID_STATE;
  realtek_bulk_tx_aggregation(this->usb_driver_,enabled);this->native_tx_aggregation_=enabled;return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}
std::string EthernetComponent::native_tx_queue_status() {
#ifdef USE_ETHERNET_REALTEK_BULK
  return this->usb_driver_?realtek_bulk_queue_statistics(this->usb_driver_):"not ready";
#else
  return "not a native transport";
#endif
}
std::string EthernetComponent::read_realtek_link_capabilities() {
  esp_err_t e=this->open_realtek_();return e==ESP_OK?this->realtek_.link_capabilities():std::string("error: ")+esp_err_to_name(e);
}
std::string EthernetComponent::read_realtek_counters() {
  esp_err_t error = this->open_realtek_();
  return error == ESP_OK ? this->realtek_.counters() : std::string("error: ") + esp_err_to_name(error);
}
esp_err_t EthernetComponent::realtek_set_link(uint16_t adv, bool autoneg, unsigned speed, bool full) {
  if((adv&~0x01e0)||(autoneg&&!adv)||(speed!=10&&speed!=100))return ESP_ERR_INVALID_ARG;
  esp_err_t error = this->open_realtek_();
#ifdef USE_ETHERNET_REALTEK_BULK
  if(error==ESP_OK)error=realtek_bulk_quiesce_phy(this->usb_driver_);
#endif
  return error == ESP_OK ? this->realtek_.set_link(adv, autoneg, speed, full) : error;
}
esp_err_t EthernetComponent::realtek_set_eee(bool enabled) {
  esp_err_t error = this->open_realtek_();
#ifdef USE_ETHERNET_REALTEK_BULK
  if(error==ESP_OK)error=realtek_bulk_quiesce_phy(this->usb_driver_);
#endif
  return error == ESP_OK ? this->realtek_.set_eee(enabled) : error;
}
esp_err_t EthernetComponent::realtek_set_pause(bool rx, bool tx) {
  esp_err_t error = this->open_realtek_();
#ifdef USE_ETHERNET_REALTEK_BULK
  if(error==ESP_OK)error=realtek_bulk_quiesce_phy(this->usb_driver_);
#endif
  return error == ESP_OK ? this->realtek_.set_pause(rx, tx) : error;
}
esp_err_t EthernetComponent::realtek_set_wake(uint8_t filters) {
  esp_err_t error = this->open_realtek_();
  return error == ESP_OK ? this->realtek_.set_wake(filters) : error;
}
esp_err_t EthernetComponent::realtek_set_promiscuous(bool enabled) {
  esp_err_t error = this->open_realtek_();
  #ifdef USE_ETHERNET_REALTEK_BULK
  return error == ESP_OK ? this->realtek_.set_packet_filter(enabled, true) : error;
#else
  return error == ESP_OK ? this->realtek_.set_packet_filter(enabled) : error;
#endif
}

esp_err_t EthernetComponent::realtek_set_multicast_filter(uint64_t bitmap,bool preserve_basic_groups) {
#ifdef USE_ETHERNET_REALTEK_BULK
  esp_err_t error=this->open_realtek_();if(error!=ESP_OK)return error;
  if(preserve_basic_groups) {
    uint8_t mac[6]{};this->get_eth_mac_address_raw(mac);
    if((mac[0]&1) || !(mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5]))return ESP_ERR_INVALID_STATE;
    bitmap|=rtl_multicast::basic_groups(mac);
    if(this->has_l2_listener(0x88cc)) {
      const uint8_t lldp[6]={1,0x80,0xc2,0,0,0x0e};bitmap|=rtl_multicast::mask(lldp);
    }
  }
  error=this->realtek_.set_multicast_hash(bitmap);
  if(error==ESP_OK)ESP_LOGI("ethernet.rtl","Verified multicast %s",this->realtek_.multicast_state().c_str());
  return error;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}
std::string EthernetComponent::read_realtek_multicast_filter() {
#ifdef USE_ETHERNET_REALTEK_BULK
  esp_err_t error=this->open_realtek_();return error==ESP_OK?this->realtek_.multicast_state():std::string("error: ")+esp_err_to_name(error);
#else
  return "not a native transport";
#endif
}
esp_err_t EthernetComponent::resume_native() {
#ifdef USE_ETHERNET_REALTEK_BULK
  return this->usb_driver_?realtek_bulk_resume(this->usb_driver_):ESP_ERR_INVALID_STATE;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t EthernetComponent::suspend_native(uint32_t maximum_ms,bool remote,uint8_t wake_filters) {
#ifdef USE_ETHERNET_REALTEK_BULK
  if((wake_filters&~31) || (remote && !wake_filters && !maximum_ms))return ESP_ERR_INVALID_ARG;
  if(!this->is_connected() || millis()<65000)return ESP_ERR_INVALID_STATE;
  if(this->open_realtek_()!=ESP_OK)return ESP_ERR_INVALID_STATE;
  esp_err_t e=this->realtek_.set_wake(remote?wake_filters:0);if(e!=ESP_OK)return e;
  ESP_LOGI("ethernet.rtl.pm","Armed verified NIC wake filters=0x%02X",remote?wake_filters:0);
  this->disable();
  e=realtek_bulk_suspend(this->usb_driver_,maximum_ms,remote);
  if(e!=ESP_OK)this->enable();
  return e;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}
std::string EthernetComponent::native_pm_status() {
#ifdef USE_ETHERNET_REALTEK_BULK
  return this->usb_driver_?realtek_bulk_pm_status(this->usb_driver_):"not initialized";
#else
  return "not a native transport";
#endif
}
esp_err_t EthernetComponent::set_native_tso(bool enabled) {
#ifdef USE_ETHERNET_REALTEK_BULK
  if(!this->usb_driver_)return ESP_ERR_INVALID_STATE;
  realtek_bulk_tso(this->usb_driver_,enabled);return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}
esp_err_t EthernetComponent::set_native_tx_mode(unsigned mode) {
#ifdef USE_ETHERNET_REALTEK_BULK
  if(!this->usb_driver_ || mode>2)return ESP_ERR_INVALID_ARG;
  realtek_bulk_tx_mode(this->usb_driver_,mode);
  return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}
std::string EthernetComponent::native_errors() {
#ifdef USE_ETHERNET_REALTEK_BULK
  return this->usb_driver_?realtek_bulk_errors(this->usb_driver_)+" config_retry="+std::to_string(this->configuration_retries_):"not initialized";
#else
  return "ECM";
#endif
}
std::string EthernetComponent::native_statistics() {
#ifdef USE_ETHERNET_REALTEK_BULK
  return this->usb_driver_?realtek_bulk_statistics(this->usb_driver_):"not initialized";
#else
  return "ECM; no native descriptor offload";
#endif
}
esp_err_t EthernetComponent::realtek_set_leds(uint8_t a,uint8_t b,uint8_t period,uint8_t duty) {
  esp_err_t error=this->open_realtek_();return error==ESP_OK?this->realtek_.set_leds(a,b,period,duty):error;
}
std::string EthernetComponent::read_realtek_leds() {
  esp_err_t error=this->open_realtek_();return error==ESP_OK?this->realtek_.led_state():"not connected";
}
const char *EthernetComponent::get_eth_mac_address_pretty_into_buffer(
    std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf) {
  uint8_t mac[MAC_ADDRESS_SIZE];
  this->get_eth_mac_address_raw(mac);
  format_mac_addr_upper(mac, buf.data());
  return buf.data();
}

void EthernetComponent::enable() {
  if (!this->disabled_ || !this->usb_eth_) return;
  if (!this->check_error_(iot_eth_start(this->usb_eth_), "iot_eth_start")) return;
  this->disabled_ = false;
  // ECM 0.4.0 has no start/stop callbacks. Replay the physical link after
  // START in the same event queue, so the glue brings up DHCP/static IP again.
  if (this->usb_link_.load()) {
    if (!this->check_error_(esp_event_post(IOT_ETH_EVENT, IOT_ETH_EVENT_CONNECTED,
                                  &this->usb_eth_, sizeof(this->usb_eth_), portMAX_DELAY), "esp_event_post")) return;
  }
}
void EthernetComponent::disable() {
  if (this->disabled_ || !this->usb_eth_) return;
  if (!this->check_error_(iot_eth_stop(this->usb_eth_), "iot_eth_stop")) return;
  this->disabled_ = true;
  this->usb_ip_.store(false);
}
bool EthernetComponent::powerdown() {
  this->disable();
  return true;
}
void EthernetComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "USB ECM Ethernet: measured InLine configuration filter, no Wi-Fi");
}
}  // namespace esphome::ethernet
