// SPDX-License-Identifier: GPL-3.0-only
#include "ethernet_info_text_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ETHERNET

namespace esphome::ethernet_info {

static const char *const TAG = "ethernet_info";

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
void IPAddressEthernetInfo::setup() { ethernet::global_eth_component->add_ip_state_listener(this); }

void IPAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo IPAddress", this); }

void IPAddressEthernetInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                        const network::IPAddress &dns2) {
  char buf[network::IP_ADDRESS_BUFFER_SIZE];
  ips[0].str_to(buf);
  this->publish_state(buf);
  // Keep backend slots stable when IPv4 disappears. Compacting only set
  // addresses moved IPv6 into the preceding sensor during IPv6-only operation.
  for (size_t sensor=0;sensor<this->ip_sensors_.size();sensor++) {
    if (this->ip_sensors_[sensor] == nullptr) continue;
    if (sensor<ips.size() && ips[sensor].is_set()) {
      ips[sensor].str_to(buf);
      this->ip_sensors_[sensor]->publish_state(buf);
    } else this->ip_sensors_[sensor]->publish_state("");
  }
}

void DNSAddressEthernetInfo::setup() { ethernet::global_eth_component->add_ip_state_listener(this); }

void DNSAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo DNS Address", this); }

void DNSAddressEthernetInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                         const network::IPAddress &dns2) {
  // IP_ADDRESS_BUFFER_SIZE (40) = max IP (39) + null; space reuses first null's slot
  char buf[network::IP_ADDRESS_BUFFER_SIZE * 2];
  dns1.str_to(buf);
  size_t len1 = strlen(buf);
  buf[len1] = ' ';
  dns2.str_to(buf + len1 + 1);
  this->publish_state(buf);
}
#endif  // USE_ETHERNET_IP_STATE_LISTENERS

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
void MACAddressEthernetInfo::setup() {
  ethernet::global_eth_component->add_ip_state_listener(this);
  this->on_ip_state({}, {}, {});
}
void MACAddressEthernetInfo::on_ip_state(const network::IPAddresses &, const network::IPAddress &,
                                        const network::IPAddress &) {
  if (!ethernet::global_eth_component->is_connected()) return;
  uint8_t mac[MAC_ADDRESS_SIZE]{};
  ethernet::global_eth_component->get_eth_mac_address_raw(mac);
  if (!(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5])) return;
  char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(mac, buf);
  this->publish_state(buf);
}

void MACAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo MAC Address", this); }

#endif  // USE_ETHERNET_IP_STATE_LISTENERS

}  // namespace esphome::ethernet_info

#endif  // USE_ETHERNET
