// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "realtek_frames.h"
namespace esphome::ethernet::rtl_frames {
inline uint32_t be32(const uint8_t *p) { return (uint32_t(be16(p))<<16)|be16(p+2); }
// Conservative re-coalescing of already segmented lwIP TCP packets. Unsupported
// headers/flags/flows stay ordinary records; no lwIP GSO contract is assumed.
class TsoBuilder {
 public:
  TsoBuilder(uint8_t *buffer,size_t capacity):out_(buffer),capacity_(capacity){}
  bool begin(const uint8_t *record,size_t bytes) {
    segments_=0;
    View v;if(!view(record,bytes,v) || v.flags!=0x10 || v.payload<128 || v.payload>2047 || bytes>capacity_)return false;
    memcpy(out_,record,bytes);first_=v;mss_=v.payload;length_=bytes;segments_=1;stopped_=false;return true;
  }
  bool append(const uint8_t *record,size_t bytes) {
    View v;if(!segments_ || stopped_ || !view(record,bytes,v))return false;
    auto *first=out_+8;auto *next=record+8;
    if(v.ip!=first_.ip || v.tcp!=first_.tcp || v.header!=first_.header || v.v6!=first_.v6 || v.payload>mss_ ||
       (v.flags!=0x10 && v.flags!=0x18) || length_+v.payload>capacity_ || (length_+v.payload)%64==0)return false;
    if(memcmp(first,next,v.ip)!=0)return false; // Ethernet and optional VLAN
    if(v.v6) {
      if(memcmp(first+v.ip,next+v.ip,4) || memcmp(first+v.ip+6,next+v.ip+6,34))return false;
    } else {
      if(memcmp(first+v.ip,next+v.ip,2) || memcmp(first+v.ip+6,next+v.ip+6,4) ||
         memcmp(first+v.ip+12,next+v.ip+12,8) || be16(next+v.ip+4)!=uint16_t(be16(first+v.ip+4)+segments_))return false;
    }
    const uint8_t *a=first+v.tcp,*b=next+v.tcp;
    if(memcmp(a,b,4) || be32(b+4)!=be32(a+4)+payload() || memcmp(a+8,b+8,5) ||
       memcmp(a+14,b+14,2) || memcmp(a+18,b+18,v.header-18))return false;
    memcpy(out_+length_,next+v.tcp+v.header,v.payload);length_+=v.payload;segments_++;
    out_[8+v.tcp+13]=v.flags;stopped_=v.payload<mss_ || (v.flags&8);return true;
  }
  size_t finish() {
    if(segments_<2)return 0;
    uint8_t *frame=out_+8,*ip=frame+first_.ip;
    uint32_t sum=6;
    if(first_.v6) {
      put16(ip+4,0); // RTL IPv6 TSO inserts each segment's payload length.
      for(unsigned i=8;i<40;i+=2)sum+=be16(ip+i);
    } else {
      put16(ip+2,length_-8-first_.ip);put16(ip+10,0);
      uint32_t header_sum=0;for(unsigned i=0;i<20;i+=2)header_sum+=be16(ip+i);
      while(header_sum>>16)header_sum=(header_sum&0xffff)+(header_sum>>16);
      put16(ip+10,uint16_t(~header_sum));
      for(unsigned i=12;i<20;i+=2)sum+=be16(ip+i);
    }
    while(sum>>16)sum=(sum&0xffff)+(sum>>16);
    put16(frame+first_.tcp+16,uint16_t(sum)); // pseudoheader seed, zero TCP length
    put32(out_,0xc0000000u|(first_.v6?(1u<<27):(1u<<28))|(uint32_t(first_.tcp)<<18)|uint32_t(length_-8));
    put32(out_+4,uint32_t(mss_)<<17);return length_;
  }
  unsigned segments() const { return segments_; }
 private:
  struct View {size_t ip,tcp,header,payload;bool v6;uint8_t flags;};
  static bool view(const uint8_t *record,size_t bytes,View &v) {
    if(bytes<8+14+20+20 || (le32(record)&0xc0000000u)!=0xc0000000u || (le32(record)&0x3ffff)!=bytes-8)return false;
    const uint8_t *f=record+8;size_t n=bytes-8;v.ip=14;uint16_t type=be16(f+12);
    if(type==0x8100){v.ip=18;if(n<18+40)return false;type=be16(f+16);}
    if(type==0x0800) {
      if(f[v.ip]!=0x45 || f[v.ip+9]!=6 || (be16(f+v.ip+6)&0x3fff) || be16(f+v.ip+2)!=n-v.ip)return false;
      v.v6=false;v.tcp=v.ip+20;
    } else if(type==0x86dd) {
      if(n<v.ip+60 || (f[v.ip]>>4)!=6 || f[v.ip+6]!=6 || be16(f+v.ip+4)!=n-v.ip-40)return false;
      v.v6=true;v.tcp=v.ip+40;
    } else return false;
    v.header=(f[v.tcp+12]>>4)*4;
    if(v.tcp>127 || v.header<20 || v.tcp+v.header>=n || (f[v.tcp+12]&15))return false;
    v.flags=f[v.tcp+13];v.payload=n-v.tcp-v.header;return true;
  }
  size_t payload() const {return length_-8-first_.tcp-first_.header;}
  uint8_t *out_;size_t capacity_,length_{0},mss_{0};unsigned segments_{0};bool stopped_{false};View first_{};
};
}
