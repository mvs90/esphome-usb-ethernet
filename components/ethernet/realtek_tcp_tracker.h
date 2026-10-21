// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "realtek_tso.h"
#include <array>
namespace esphome::ethernet::rtl_frames {
// Diagnostic only: bounded recent-flow history detects repeated outgoing TCP
// payload sequence ranges. Never changes packet contents or the TCP state.
// Call from the serialized network output path. Unsupported headers are skipped.
class TcpTracker {
 public:
  bool repeated(const uint8_t *f,size_t bytes) {
    if(bytes<54)return false;
    std::array<uint8_t,40> key{};size_t ip=14,tcp=0,end=0;uint16_t type=be16(f+12);
    if(type==0x8100){if(bytes<58)return false;key[37]=f[14];key[38]=f[15];type=be16(f+16);ip=18;}
    if(type==0x0800) {
      size_t ihl=(f[ip]&15)*4,total=be16(f+ip+2);
      if((f[ip]>>4)!=4 || ihl<20 || total<ihl+20 || total>bytes-ip || f[ip+9]!=6 || (be16(f+ip+6)&0x3fff))return false;
      key[0]=4;memcpy(key.data()+1,f+ip+12,8);tcp=ip+ihl;end=ip+total;
    } else if(type==0x86dd) {
      if(bytes<ip+60 || (f[ip]>>4)!=6 || f[ip+6]!=6)return false;
      end=ip+40+be16(f+ip+4);if(end>bytes || end<ip+60)return false;
      key[0]=6;memcpy(key.data()+1,f+ip+8,32);tcp=ip+40;
    } else return false;
    size_t header=(f[tcp+12]>>4)*4;if(header<20 || tcp+header>end)return false;
    memcpy(key.data()+33,f+tcp,4);uint8_t flags=f[tcp+13];size_t payload=end-tcp-header;
    if(!payload && !(flags&6))return false;
    Flow *flow=nullptr;
    for(auto &candidate:flows_)if(candidate.valid && candidate.key==key){flow=&candidate;break;}
    bool known=flow!=nullptr;
    if(!flow){flow=&flows_[next_++%flows_.size()];flow->key=key;flow->valid=true;}
    uint32_t seq=be32(f+tcp+4),after=seq+uint32_t(payload)+bool(flags&2);
    if(flags&4){flow->valid=false;return false;}
    bool repeat=known && !(flags&2) && payload && int32_t(seq-flow->end)<0;
    if(!known || (flags&2) || int32_t(after-flow->end)>0)flow->end=after;
    return repeat;
  }
 private:
  struct Flow {std::array<uint8_t,40> key{};uint32_t end{0};bool valid{false};};
  std::array<Flow,8> flows_{};unsigned next_{0};
};
}
