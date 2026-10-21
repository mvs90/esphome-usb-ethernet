// SPDX-License-Identifier: GPL-3.0-only
// Test-only evidence at successful USB completion, restricted to synthetic LOAD
// datagrams. This does not modify any packet or prove delivery on Ethernet.
#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
namespace esphome::ethernet::rtl_integrity {
struct Bits {
  std::atomic<uint32_t> lo{0},hi{0};
  void clear(){lo.store(0);hi.store(0);}
  void set(unsigned n){if(n<32)lo.fetch_or(1u<<n);else if(n<64)hi.fetch_or(1u<<(n-32));}
};
struct Evidence {
  std::atomic<uint32_t> cycle{UINT32_MAX};
  Bits seen,l2,ip,udp,payload,offloaded,descriptor;
  void epoch(uint32_t n){if(cycle.load()!=n){seen.clear();l2.clear();ip.clear();udp.clear();payload.clear();offloaded.clear();descriptor.clear();cycle.store(n);}}
  std::string text() const {
    char out[220];
    snprintf(out,sizeof(out),"cycle=%08lX seen=%08lX%08lX l2=%08lX%08lX ip=%08lX%08lX udp=%08lX%08lX payload=%08lX%08lX off=%08lX%08lX desc=%08lX%08lX",
      (unsigned long)cycle.load(),(unsigned long)seen.hi.load(),(unsigned long)seen.lo.load(),
      (unsigned long)l2.hi.load(),(unsigned long)l2.lo.load(),(unsigned long)ip.hi.load(),(unsigned long)ip.lo.load(),
      (unsigned long)udp.hi.load(),(unsigned long)udp.lo.load(),(unsigned long)payload.hi.load(),(unsigned long)payload.lo.load(),
      (unsigned long)offloaded.hi.load(),(unsigned long)offloaded.lo.load(),(unsigned long)descriptor.hi.load(),(unsigned long)descriptor.lo.load());
    return out;
  }
};
inline Evidence family[2];
inline uint8_t peer[6]{},source[6]{};
inline std::atomic<bool> source_ready{false};
inline std::atomic<bool> peer_ready{false};
// Set exactly once during on_boot, before issuing LOAD traffic.
inline void expected_peer(const uint8_t *mac,const uint8_t *own=nullptr){memcpy(peer,mac,6);if(own){memcpy(source,own,6);source_ready.store(true,std::memory_order_release);}peer_ready.store(true,std::memory_order_release);}
inline uint16_t be16(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline uint32_t be32(const uint8_t *p){return uint32_t(be16(p))<<16|be16(p+2);}
inline uint32_t sum(const uint8_t *p,size_t n,uint32_t initial=0) {
  while(n>=2){initial+=be16(p);p+=2;n-=2;}
  if(n)initial+=uint16_t(*p)<<8;
  return initial;
}
inline bool valid_sum(uint32_t n){while(n>>16)n=(n&0xffff)+(n>>16);return n==0xffff;}
inline void inspect(const uint8_t *f,size_t bytes,bool hardware_checksum,uint32_t descriptor0=0) {
  if(bytes<14)return;
  unsigned which;size_t start=14,transport,end,ihl=0;uint32_t pseudo;
  uint16_t type=be16(f+12);
  if(type==0x0800) {
    if(bytes<42 || (f[start]>>4)!=4 || f[start+9]!=17)return;
    ihl=(f[start]&15)*4;size_t total=be16(f+start+2);
    if(ihl<20 || total<ihl+8 || total>bytes-start || (be16(f+start+6)&0x3fff))return;
    transport=start+ihl;end=start+total;which=0;pseudo=sum(f+start+12,8);
  } else if(type==0x86dd) {
    if(bytes<62 || (f[start]>>4)!=6 || f[start+6]!=17)return;
    size_t length=be16(f+start+4);transport=start+40;end=transport+length;
    if(length<8 || end>bytes)return;
    which=1;pseudo=sum(f+start+8,32);
  } else return;
  uint16_t length=be16(f+transport+4);
  if(be16(f+transport)!=(which?3334:3333) || length<20 || length>end-transport)return;
  const uint8_t *data=f+transport+8;
  if(memcmp(data,"LOAD",4))return;
  uint32_t cycle=be32(data+4),sequence=be32(data+8);if(sequence>=64)return;
  Evidence &e=family[which];e.epoch(cycle);e.seen.set(sequence);
  if(descriptor0&&((descriptor0&0xfffc0000u)!=0xc0000000u || (descriptor0&0x3ffff)!=bytes))e.descriptor.set(sequence);
  if((peer_ready.load(std::memory_order_acquire)&&memcmp(f,peer,6)) || (source_ready.load(std::memory_order_acquire)&&memcmp(f+6,source,6)))e.l2.set(sequence);
  if(!which&&!valid_sum(sum(f+start,ihl)))e.ip.set(sequence);
  if(hardware_checksum)e.offloaded.set(sequence);
  else {
    bool omitted=!which&&be16(f+transport+6)==0;
    if(!omitted&&((which && !be16(f+transport+6)) || !valid_sum(sum(f+transport,length,pseudo+17+length))))e.udp.set(sequence);
  }
  bool good=length==1408 && end-transport==length;
  if(good){uint8_t expected=(uint64_t(cycle)+sequence)%251;for(size_t n=12;n<1400;n++)if(data[n]!=expected){good=false;break;}}
  if(!good)e.payload.set(sequence);
}
}
