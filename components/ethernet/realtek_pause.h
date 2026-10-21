// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace esphome::ethernet::rtl_pause {
// IEEE802.3 Annex28B full-duplex resolution. Result bit0 RX, bit1 TX.
// Row/column encode symmetric bit10 and asymmetric bit11 advertisements.
constexpr uint8_t resolve(uint16_t local,uint16_t peer,bool operational){
  constexpr uint8_t table[4][4]={{0,0,0,0},{0,3,0,3},{0,0,0,2},{0,3,1,3}};
  return operational?table[(local>>10)&3][(peer>>10)&3]:0;
}
}
