// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <string>
#include "esp_err.h"
#include "iot_usbh_cdc.h"

namespace esphome::ethernet {
// Main-loop-only management, restricted to the measured RTL8152 revision.
// Register/protocol facts: docs/REALTEK_DIAGNOSTICS.md. No Linux source copied.
class RealtekControl {
 public:
  esp_err_t open(usbh_cdc_port_handle_t port);
  esp_err_t initialize_native(bool rx_aggregation = false);
  esp_err_t phy_status(uint8_t &status);
  esp_err_t set_leds(uint8_t led0, uint8_t led1, uint8_t period, uint8_t duty);
  std::string led_state();
  esp_err_t set_mac(const uint8_t *mac);
  esp_err_t set_link(uint16_t advertisement, bool autoneg = true, unsigned speed = 100, bool full = true);
  esp_err_t set_pause(bool rx, bool tx);
  esp_err_t set_eee(bool enabled);
  esp_err_t set_wake(uint8_t filters);  // bit0 magic, bit1 link, bit2 unicast, bit3 broadcast, bit4 multicast
  esp_err_t set_packet_filter(bool promiscuous, bool native = false);
  esp_err_t set_multicast_hash(uint64_t bitmap);
  std::string multicast_state();
  std::string diagnostics();
  std::string counters();
  std::string link_capabilities();
  std::string native_registers();
 protected:
  esp_err_t request_(uint8_t direction, uint8_t request, uint16_t value, uint16_t index, uint16_t length, uint8_t *data);
  esp_err_t read_(uint16_t addr, uint8_t *data, uint16_t length = 4);
  esp_err_t write_(uint16_t addr, const uint8_t *data, uint16_t length, uint8_t enables);
  esp_err_t word_(uint16_t addr, uint16_t &value);
  esp_err_t put_word_(uint16_t addr, uint16_t value);
  esp_err_t byte_(uint16_t addr, uint8_t value);
  esp_err_t ocp_(uint16_t addr, uint16_t &value, bool write = false);
  esp_err_t mmd_(uint8_t device, uint16_t addr, uint16_t &value, bool write = false);
  usbh_cdc_port_handle_t port_{nullptr};
};
}
