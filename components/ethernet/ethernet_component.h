// SPDX-License-Identifier: GPL-3.0-only
// Local USB-only replacement of the ESPHome 2026.9.1 Ethernet public interface.
#pragma once
#include <atomic>
#include <array>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"
#include "esphome/components/network/ip_address.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "iot_eth.h"
#include "realtek_control.h"

namespace esphome::ethernet {
#ifdef USE_ETHERNET_IP_STATE_LISTENERS
class EthernetIPStateListener {
 public:
  virtual void on_ip_state(const network::IPAddresses &, const network::IPAddress &,
                           const network::IPAddress &) = 0;
};
#endif

enum EthernetType : uint8_t { ETHERNET_TYPE_USB_ECM };
#ifdef USE_ETHERNET_MANUAL_IP
struct ManualIP {
  network::IPAddress static_ip, gateway, subnet, dns1, dns2;
};
#endif
enum class EthernetComponentState : uint8_t { STOPPED, CONNECTING, CONNECTED };
class EthernetComponent final : public Component {
 public:
  EthernetComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  void on_powerdown() override { this->powerdown(); }
  bool is_connected() { return this->state_ == EthernetComponentState::CONNECTED; }
  bool is_disabled() { return this->disabled_; }
  bool is_enabled() { return !this->disabled_; }
  void enable();
  void disable();
  bool powerdown();
  void set_type(EthernetType) {}
#ifdef USE_ETHERNET_MANUAL_IP
  void set_manual_ip(const ManualIP &ip) { this->manual_ip_ = ip; }
#endif
  esp_netif_t *get_esp_netif() { return this->eth_netif_; }
  network::IPAddresses get_ip_addresses();
  network::IPAddress get_dns_address(uint8_t num);
  const char *get_use_address() const { return this->use_address_; }
  void set_use_address(const char *address) { this->use_address_ = address; }
  // Optional layer-2 consumers borrow RX bytes only for the callback. Returning
  // true transfers disposal to this driver, never to the consumer. Setup-only
  // registration is published atomically; no allocation in the RX path.
  using L2Handler=bool (*)(void *,const uint8_t *,size_t);
  bool add_l2_listener(uint16_t type,L2Handler handler,void *context) {
    unsigned n=this->l2_count_.load();if(!handler||n>=4)return false;
    for(unsigned i=0;i<n;i++)if(this->l2_[i].type==type)return false;
    this->l2_[n]={type,handler,context};this->l2_count_.store(n+1);return true;
  }
  bool has_l2_listener(uint16_t type) const {
    unsigned n=this->l2_count_.load();for(unsigned i=0;i<n;i++)if(this->l2_[i].type==type)return true;return false;
  }
  bool is_l2_ready() const {return !this->disabled_&&this->adapter_ready_.load()&&this->usb_link_.load();}
  esp_err_t transmit_l2(const uint8_t *data,size_t length);
  void get_eth_mac_address_raw(uint8_t *mac);
  void set_fixed_mac(const std::array<uint8_t, 6> &mac) { this->fixed_mac_ = mac; this->has_fixed_mac_ = true; }
  void set_realtek_options(int speed, int duplex, int eee, int wake) {
    this->has_realtek_config_ = true; this->link_speed_ = speed; this->link_duplex_ = duplex; this->eee_ = eee; this->wake_ = wake;
  }
  std::string read_realtek_diagnostics();
  std::string read_realtek_counters();
  std::string read_realtek_link_capabilities();
  std::string native_tx_queue_status();
  esp_err_t set_native_tx_aggregation(bool enabled);
  std::string read_realtek_native_registers();
  esp_err_t realtek_set_multicast_filter(uint64_t bitmap,bool preserve_basic_groups=true);
  std::string read_realtek_multicast_filter();
  esp_err_t resume_native();
  esp_err_t suspend_native(uint32_t maximum_ms,bool remote,uint8_t wake_filters=1);
  std::string native_pm_status();
  void set_native_aggregation(bool tx,bool rx) { this->native_tx_aggregation_=tx; this->native_rx_aggregation_=rx; }
  void set_ipv4_enabled(bool enabled) { this->ipv4_enabled_=enabled; }
  void set_native_tx_offload(bool enabled) { this->native_tx_offload_ = enabled; }
  esp_err_t set_native_tso(bool enabled);
  void set_native_tso_on_boot(bool enabled) { this->native_tso_=enabled; }
  esp_err_t set_native_tx_mode(unsigned mode);
  std::string native_statistics();
  std::string native_errors();
#ifdef USE_RTL_FAULT_INJECTION
  void test_configure_timeout_once() { this->inject_configure_timeout_=true; }
#endif
  esp_err_t realtek_set_leds(uint8_t a,uint8_t b,uint8_t period,uint8_t duty);
  std::string read_realtek_leds();
  esp_err_t realtek_set_link(uint16_t advertisement, bool autoneg = true, unsigned speed = 100, bool full = true);
  esp_err_t realtek_set_eee(bool enabled);
  esp_err_t realtek_set_pause(bool rx, bool tx);
  esp_err_t realtek_set_wake(uint8_t filters);
  esp_err_t realtek_set_promiscuous(bool enabled);
  const char *get_eth_mac_address_pretty_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf);
#ifdef USE_ETHERNET_IP_STATE_LISTENERS
  void add_ip_state_listener(EthernetIPStateListener *listener) { this->ip_state_listeners_.push_back(listener); }
#endif
#ifdef USE_ETHERNET_CONNECT_TRIGGER
  Trigger<> *get_connect_trigger() { return &this->connect_trigger_; }
#endif
#ifdef USE_ETHERNET_DISCONNECT_TRIGGER
  Trigger<> *get_disconnect_trigger() { return &this->disconnect_trigger_; }
#endif
 protected:
  static void usb_event_(void *arg, esp_event_base_t base, int32_t id, void *data);
  struct NetifGlue { esp_netif_driver_base_t base{}; EthernetComponent *owner{nullptr}; } glue_;
  static esp_err_t attach_(esp_netif_t *netif, void *arg);
  static esp_err_t receive_(iot_eth_handle_t, uint8_t *data, size_t length, void *arg);
  bool configure_adapter_();
  esp_err_t open_realtek_();
  struct L2Listener {uint16_t type;L2Handler handler;void *context;};
  std::array<L2Listener,4> l2_{};std::atomic<unsigned> l2_count_{0};
  RealtekControl realtek_;
  std::array<uint8_t, 6> fixed_mac_{};
  bool has_fixed_mac_{false};
  bool adapter_configured_{false};
  bool ipv4_enabled_{true};
  bool native_tx_offload_{false},native_tso_{false};
  bool native_tx_aggregation_{false},native_rx_aggregation_{false};
  bool has_realtek_config_{false};
  int link_speed_{0}, link_duplex_{1}, eee_{-1}, wake_{-1};
  std::atomic<bool> pending_connect_{false};
  bool check_error_(esp_err_t error, const char *operation);
  esp_netif_t *eth_netif_{nullptr};
  iot_eth_handle_t usb_eth_{nullptr};
  iot_eth_driver_t *usb_driver_{nullptr};
  std::atomic<bool> usb_link_{false};
  std::atomic<bool> usb_ip_{false};
  uint32_t usb_last_poll_{0},adapter_retry_at_{0};
  std::atomic<bool> adapter_ready_{false};
  uint32_t configuration_retries_{0};
#ifdef USE_RTL_FAULT_INJECTION
  bool inject_configure_timeout_{false};
#endif
  network::IPAddresses last_ips_{};
  network::IPAddress last_dns1_{}, last_dns2_{};
#ifdef USE_ETHERNET_MANUAL_IP
  ManualIP manual_ip_{};
#endif
  EthernetComponentState state_{EthernetComponentState::STOPPED};
  bool disabled_{false};
  bool connected_{false};
  bool got_ipv4_address_{false};
  bool ethernet_initialized_{false};
  bool started_{false};
#ifdef USE_ETHERNET_IP_STATE_LISTENERS
  void notify_ip_state_listeners_();
  StaticVector<EthernetIPStateListener *, ESPHOME_ETHERNET_IP_STATE_LISTENERS> ip_state_listeners_;
#endif
#ifdef USE_ETHERNET_CONNECT_TRIGGER
  Trigger<> connect_trigger_;
#endif
#ifdef USE_ETHERNET_DISCONNECT_TRIGGER
  Trigger<> disconnect_trigger_;
#endif
 private:
  const char *use_address_{nullptr};
};
extern EthernetComponent *global_eth_component;
}  // namespace esphome::ethernet
