// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace esphome::ethernet::rtl_multicast {
// Ethernet serial bit order, IEEE polynomial, no final complement. The NIC
// selects six high CRC bits. This is a hash filter; collisions are accepted.
constexpr unsigned index(const uint8_t *mac) {
  uint32_t remainder=0xffffffffu;
  for(unsigned octet=0;octet<6;octet++) {
    for(unsigned bit=0;bit<8;bit++) {
      bool feedback=((remainder>>31)^(mac[octet]>>bit))&1;
      remainder<<=1;
      if(feedback)remainder^=0x04c11db7u;
    }
  }
  return remainder>>26;
}
constexpr uint64_t mask(const uint8_t *mac) {return uint64_t(1)<<index(mac);}
// PLA MAR serializes the logical 64-bit bitmap most-significant byte first.
constexpr std::array<uint8_t,8> wire_bytes(uint64_t bitmap) {
  std::array<uint8_t,8> bytes{};
  for(unsigned i=0;i<8;i++)bytes[i]=uint8_t(bitmap>>(56-8*i));
  return bytes;
}
// Minimum groups for the tested EUI-64 IPv6 / IGMP / mDNS configuration.
// This is not dynamic IGMP/MLD membership tracking.
constexpr uint64_t basic_groups(const uint8_t *mac) {
  uint8_t groups[5][6]={{0x33,0x33,0,0,0,1},{0x33,0x33,0xff,mac[3],mac[4],mac[5]},
                      {1,0,0x5e,0,0,1},{1,0,0x5e,0,0,0xfb},{0x33,0x33,0,0,0,0xfb}};
  uint64_t bitmap=0;
  for(auto &group:groups)bitmap|=mask(group);
  return bitmap;
}
}
