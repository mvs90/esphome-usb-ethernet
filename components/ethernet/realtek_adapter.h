// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace esphome::ethernet::rtl_adapter {
// Both measured InLine and P2C5V units expose this tuple. It identifies the
// transport, not a retail brand; register access separately checks revision.
constexpr bool supported(uint16_t vendor, uint16_t product, uint16_t release) {
  return vendor == 0x0bda && product == 0x8152 && release == 0x2000;
}
constexpr int hex_digit(uint8_t ch) {
  return ch >= '0' && ch <= '9' ? ch - '0' :
         ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 :
         ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}
// The measured alternate ECM function uses string index 3 (12 hex digits,
// UTF-16LE). Never retain any bytes of the old adapter on a failed read.
inline bool factory_mac(const uint8_t *descriptor, size_t available, uint8_t *out) {
  if (!descriptor || !out || available < 26 || descriptor[0] != 26 || descriptor[1] != 3)
    return false;
  uint8_t mac[6]{};
  for (unsigned i = 0; i < 12; i++) {
    int digit = hex_digit(descriptor[2 + i * 2]);
    if (digit < 0 || descriptor[3 + i * 2] != 0) return false;
    mac[i / 2] |= uint8_t(digit << (i % 2 ? 0 : 4));
  }
  if ((mac[0] & 1) || !(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5])) return false;
  memcpy(out, mac, 6);
  return true;
}
}
