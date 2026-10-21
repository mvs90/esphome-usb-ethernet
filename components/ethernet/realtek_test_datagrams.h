// SPDX-License-Identifier: GPL-3.0-only
// Test-only last-burst tracing; no instances or calls in a production build.
#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <cstring>
namespace esphome::ethernet::rtl_probe {
inline uint16_t be16(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline uint32_t be32(const uint8_t *p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
struct Burst {
  std::atomic<uint32_t> cycle{UINT32_MAX},lo{0},hi{0};
  void note(const uint8_t *data,size_t size) {
    if(size<12 || memcmp(data,"LOAD",4))return;
    uint32_t c=be32(data+4),n=be32(data+8);if(n>=64)return;
    if(cycle.load()!=c){lo=0;hi=0;cycle=c;}
    if(n<32)lo.fetch_or(1u<<n);else hi.fetch_or(1u<<(n-32));
  }
  std::string text() const {
    char b[40];snprintf(b,sizeof(b),"%08lX:%08lX%08lX",(unsigned long)cycle.load(),(unsigned long)hi.load(),(unsigned long)lo.load());return b;
  }
};
inline Burst usb_rx[2],app_rx[2],app_tx[2],usb_tx[2];
inline std::atomic<unsigned> send_errors[2]{},last_errno[2]{};
inline void frame(Burst (&stage)[2],const uint8_t *data,size_t size) {
  if(size<14)return;
  size_t ip=14,udp=0;unsigned family=0;uint16_t type=be16(data+12);
  if(type==0x0800 && size>=42 && (data[ip]>>4)==4 && data[ip+9]==17 && !(be16(data+ip+6)&0x3fff)) {
    size_t ihl=(data[ip]&15)*4;if(ihl<20 || size<ip+ihl+8)return;udp=ip+ihl;
  } else if(type==0x86dd && size>=62 && data[ip+6]==17){family=1;udp=ip+40;}
  else return;
  uint16_t server=family?3334:3333;
  if(size<udp+20 || (be16(data+udp)!=server && be16(data+udp+2)!=server))return;
  stage[family].note(data+udp+8,size-udp-8);
}
inline std::string report() {
  std::string out;
  for(unsigned f=0;f<2;f++) {
    auto suffix=f?"6=":"4=";
    out+="rx"+std::string(suffix)+usb_rx[f].text()+" ar"+suffix+app_rx[f].text()+" at"+suffix+app_tx[f].text()+" tx"+suffix+usb_tx[f].text();
    out+=" err"+std::string(suffix)+std::to_string(send_errors[f].load())+":"+std::to_string(last_errno[f].load())+" ";
  }
  return out;
}
}
