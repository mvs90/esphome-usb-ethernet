// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/ethernet/ethernet_component.h"

#ifdef USE_ETHERNET

namespace esphome::ethernet_info {

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
class IPAddressEthernetInfo final : public Component,
                                    public text_sensor::TextSensor,
                                    public ethernet::EthernetIPStateListener {
 public:
  void setup() override;
  void dump_config() override;
  void add_ip_sensors(uint8_t index, text_sensor::TextSensor *s) { this->ip_sensors_[index] = s; }

  // EthernetIPStateListener interface
  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;

 protected:
  std::array<text_sensor::TextSensor *, 5> ip_sensors_{};
};

class DNSAddressEthernetInfo final : public Component,
                                     public text_sensor::TextSensor,
                                     public ethernet::EthernetIPStateListener {
 public:
  void setup() override;
  void dump_config() override;

  // EthernetIPStateListener interface
  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;
};
#endif  // USE_ETHERNET_IP_STATE_LISTENERS

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
class MACAddressEthernetInfo final : public Component, public text_sensor::TextSensor, public ethernet::EthernetIPStateListener {
 public:
  void setup() override;
  void on_ip_state(const network::IPAddresses &, const network::IPAddress &,
                   const network::IPAddress &) override;
  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  void dump_config() override;
};

#endif  // USE_ETHERNET_IP_STATE_LISTENERS

}  // namespace esphome::ethernet_info

#endif  // USE_ETHERNET
