// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "realtek_frames.h"
#include <array>
namespace esphome::ethernet::rtl_frames {
// A full USB transfer may split an aggregated record. A short transfer terminates
// an aggregate; a truncated descriptor/frame at that boundary is rejected.
template<size_t Capacity> class StreamDecoder {
 public:
  void reset() { used_=0;discarding_=false; }
  size_t pending() const { return used_; }
  template<class Sink> bool feed(const uint8_t *data,size_t size,bool end,Sink sink) {
    if(discarding_){if(end)reset();return true;}
    if(size>Capacity-used_)return fail_(end);
    if(size)memcpy(buffer_.data()+used_,data,size);
    used_+=size;size_t pos=0;
    while(used_-pos>=24) {
      const auto *p=buffer_.data()+pos;size_t count=le32(p)&0x7fff;
      if(count<18 || count>1522){return fail_(end);}
      if(used_-pos<24+count)break;
      uint32_t protocol=le32(p+4),errors=le32(p+8);
      bool bad=((protocol&(1u<<19))&&(errors&(1u<<23))) ||
        ((protocol&(1u<<22))&&(errors&(1u<<21))) || ((protocol&(1u<<23))&&(errors&(1u<<22)));
      sink(p+24,count-4,bad);
      size_t after=pos+24+count,next=(after+7)&~size_t(7);
      if(next>used_) {
        // Only the final record can omit its last alignment bytes.
        if(!end){return fail_(end);}
        for(size_t i=after;i<used_;i++)if(buffer_[i]){return fail_(end);}
        pos=used_;break;
      }
      pos=next;
    }
    if(pos){used_-=pos;memmove(buffer_.data(),buffer_.data()+pos,used_);}
    if(end && used_) {
      // At most seven zero alignment bytes are legal after a complete record.
      bool padding=used_<8;for(size_t i=0;i<used_;i++)padding=padding && buffer_[i]==0;
      reset();if(!padding)return false;
    }
    return true;
  }
 private:
  bool fail_(bool end) {used_=0;discarding_=!end;return false;}
  bool discarding_{false};
  std::array<uint8_t,Capacity> buffer_{};
  size_t used_{0};
};
// TX records are four-byte aligned. Never finish on a full-speed max-packet
// boundary: leave the next record queued instead of appending a dummy record or
// changing an offloaded packet's Ethernet/IP length.
inline size_t append_tx_record(uint8_t *out,size_t capacity,size_t used,const uint8_t *record,size_t bytes) {
  if(bytes<8 || (le32(record)&0xc0000000u)!=0xc0000000u)return 0;
  size_t frame=le32(record)&0x3ffff;
  if(frame<14 || frame>1518 || frame+8!=bytes)return 0;
  size_t start=(used+3)&~size_t(3);
  if(start>capacity || bytes>capacity-start || (start+bytes)%64==0)return 0;
  memset(out+used,0,start-used);memcpy(out+start,record,bytes);return start+bytes;
}
}
