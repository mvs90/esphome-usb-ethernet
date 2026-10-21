// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_adapter.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace esphome::ethernet::rtl_adapter;
static std::array<uint8_t,64> descriptor(const char *hex) {
  std::array<uint8_t,64> d{};d[0]=26;d[1]=3;
  for(unsigned i=0;i<12;i++)d[2+i*2]=hex[i];
  return d;
}
int main() {
  assert(supported(0x0bda,0x8152,0x2000));
  assert(!supported(0x0bda,0x8153,0x2000));
  assert(!supported(0x0bda,0x8152,0x3000));
  assert(!supported(0x1234,0x8152,0x2000));
  // Two units with the same chip are accepted without a vendor-name/MAC list.
  auto a=descriptor("02AABB000001"),b=descriptor("02aabb000002");
  uint8_t out[6]{};
  for(unsigned cycle=0;cycle<100;cycle++) {
    assert(factory_mac(a.data(),a.size(),out));assert(out[5]==1);
    assert(factory_mac(b.data(),b.size(),out));assert(out[5]==2);
  }
  const uint8_t expected[]={2,0xaa,0xbb,0,0,2};assert(!memcmp(out,expected,6));
  auto reject=[&](std::array<uint8_t,64> d,size_t size=64) {
    uint8_t before[6];memcpy(before,out,6);
    assert(!factory_mac(d.data(),size,out));assert(!memcmp(out,before,6));
  };
  for(size_t length=0;length<26;length++)reject(b,length);
  for(unsigned length: {0u,24u,25u,27u,64u,255u}){auto d=b;d[0]=length;reject(d);}
  auto d=b;d[1]=2;reject(d);
  for(unsigned i=0;i<12;i++) {
    d=b;d[2+i*2]='G';reject(d);
    d=b;d[3+i*2]=1;reject(d);
    d=b;d[2+i*2]=0;reject(d);
  }
  reject(descriptor("000000000000"));reject(descriptor("FFFFFFFFFFFF"));
  reject(descriptor("03AABB000002"));
  assert(!factory_mac(nullptr,64,out));assert(!factory_mac(b.data(),64,nullptr));
  std::puts("PASS adapter matching, repeated replacement MACs, malformed UTF-16 and transactional rejection");
}
