// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace esphome::ethernet::rtl_frames {
inline uint32_t le32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
inline void put32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;i++)p[i]=n>>(8*i); }
// Native RTL8152 RX records: 24-byte descriptor, frame including FCS, 8-byte alignment.
// Preserve software IP/L4 checksum checks; only classify hardware results here.
template<class Sink> bool decode(const uint8_t *p,size_t n,Sink sink) {
  size_t pos=0;
  while(pos<n) {
    if(n-pos<24) { for(size_t i=pos;i<n;i++) if(p[i]) return false; return true; }
    uint32_t count=le32(p+pos)&0x7fff;
    if(count<18 || count>1522 || count>n-pos-24) return false;
    uint32_t protocol=le32(p+pos+4), errors=le32(p+pos+8);
    bool bad=((protocol&(1u<<19)) && (errors&(1u<<23))) ||
      ((protocol&(1u<<22)) && (errors&(1u<<21))) || ((protocol&(1u<<23)) && (errors&(1u<<22)));
    sink(p+pos+24,count-4,bad);
    size_t end=pos+24+count;
    if(end==n)return true;
    pos=(end+7)&~size_t(7);
    if(pos>n)return false;
  }
  return true;
}
inline size_t encode(uint8_t *out,size_t capacity,const uint8_t *frame,size_t length) {
  if(length<14 || length>1518)return 0;
  size_t padded=length<60?60:length;
  // One Ethernet padding byte belongs to the same declared frame. Appending a
  // zero native descriptor here would instead produce an invalid second record.
  if((padded+8)%64==0)padded++;
  size_t total=padded+8;
  if(capacity<total)return 0;
  memset(out,0,total); put32(out,uint32_t(padded)|0xc0000000); memcpy(out+8,frame,length);
  return total;
}
inline uint16_t be16(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline void put16(uint8_t *p,uint16_t n){p[0]=n>>8;p[1]=n;}
// mode1: NIC full-checksum calculation; mode2: pseudoheader seed (partial checksum).
// Ethernet-padded frames, unknown headers and fragments retain the stack's complete software checksum.
inline bool checksum_offload(uint8_t *record,size_t n,unsigned mode) {
  if(n<8+14 || !mode)return false;
  uint8_t *f=record+8;size_t frame=n-8,ip=14;
  uint16_t type=be16(f+12);
  if(type==0x8100 || type==0x88a8){if(frame<18)return false;type=be16(f+16);ip=18;}
  size_t transport=0, length=0;uint8_t protocol=0;uint32_t sum=0,flags=0;
  if(type==0x0800) {
    if(frame<ip+20 || (f[ip]>>4)!=4)return false;
    size_t header=(f[ip]&15)*4, total=be16(f+ip+2);
    if(header<20||total<header||ip+total!=frame||(be16(f+ip+6)&0x3fff))return false;
    protocol=f[ip+9];transport=ip+header;length=total-header;
    for(size_t i=ip+12;i<ip+20;i+=2)sum+=be16(f+i);
    sum+=protocol+length;flags=1u<<29;
  } else if(type==0x86dd) {
    if(frame<ip+40 || (f[ip]>>4)!=6)return false;
    protocol=f[ip+6];length=be16(f+ip+4);transport=ip+40;
    if(transport+length!=frame)return false;
    for(size_t i=ip+8;i<ip+40;i+=2)sum+=be16(f+i);
    sum+=protocol+length;flags=1u<<28;
  } else return false;
  size_t check;
  if(protocol==6 && length>=20){flags|=1u<<30;check=transport+16;}
  else if(protocol==17 && length>=8){flags|=1u<<31;check=transport+6;}
  else return false;
  // IPV4_CS selects the L3 protocol; retain the stack's IPv4 header checksum.
  while(sum>>16)sum=(sum&0xffff)+(sum>>16);
  put16(f+check,mode==2?uint16_t(sum):0);
  put32(record+4,flags|(uint32_t(transport)<<17));return true;
}

}
