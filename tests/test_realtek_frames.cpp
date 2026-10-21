// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_frames.h"
#include <array>
#include <cassert>
#include <random>
#include <iostream>
using namespace esphome::ethernet::rtl_frames;
int main(){
 std::array<uint8_t,1600> wire{},frame{};
 for(size_t n: {size_t(14),size_t(54),size_t(60),size_t(120),size_t(1514),size_t(1518)}){
  for(size_t i=0;i<n;i++)frame[i]=i;
  size_t encoded=encode(wire.data(),wire.size(),frame.data(),n);
  assert(encoded&&encoded%64!=0&&(le32(wire.data())&0xc0000000)==0xc0000000);
  assert(!memcmp(wire.data()+8,frame.data(),n));
  assert((le32(wire.data())&0x3ffff)==encoded-8);
 }
 assert(encode(wire.data(),wire.size(),frame.data(),1519)==0);
 assert(encode(wire.data(),20,frame.data(),60)==0);
 std::array<uint8_t,176> rx{};
 put32(rx.data(),64);put32(rx.data()+4,1u<<19);
 put32(rx.data()+88,64);put32(rx.data()+92,1u<<22);put32(rx.data()+96,1u<<21);
 unsigned count=0;
 assert(decode(rx.data(),rx.size(),[&](const uint8_t*,size_t n,bool bad){assert(n==60);assert(bad==(count==1));count++;}));assert(count==2);
 put32(rx.data(),1523);assert(!decode(rx.data(),rx.size(),[](const uint8_t*,size_t,bool){assert(false);}));
 // IPv4/IPv6 partial checksum descriptors and unsupported/fragment fallback.
 frame.fill(0);frame[12]=8;frame[14]=0x45;frame[16]=0;frame[17]=46;frame[23]=6;
 frame[26]=192;frame[27]=0;frame[28]=2;frame[29]=1;frame[30]=192;frame[31]=0;frame[32]=2;frame[33]=2;
 size_t encoded=encode(wire.data(),wire.size(),frame.data(),60);
 assert(checksum_offload(wire.data(),encoded,2));
 assert(le32(wire.data()+4)==((1u<<29)|(1u<<30)|(34u<<17)));
 assert(be16(wire.data()+8+50)==0x8424); // pseudoheader seed
 frame[20]=0x20;encoded=encode(wire.data(),wire.size(),frame.data(),54);
 assert(!checksum_offload(wire.data(),encoded,1));
 frame.fill(0);frame[12]=0x86;frame[13]=0xdd;frame[14]=0x60;frame[19]=8;frame[20]=17;
 encoded=encode(wire.data(),wire.size(),frame.data(),62);assert(checksum_offload(wire.data(),encoded,1));
 assert(le32(wire.data()+4)==((1u<<28)|(1u<<31)|(54u<<17)));
 // Padding changes the hardware L4 length on this revision: retain software checksums.
 frame.fill(0);frame[12]=8;frame[14]=0x45;frame[17]=40;frame[23]=6;
 frame[24]=0x12;frame[25]=0x34;
 encoded=encode(wire.data(),wire.size(),frame.data(),54);
 assert(!checksum_offload(wire.data(),encoded,2));
 assert(wire[8+24]==0x12 && wire[8+25]==0x34 && le32(wire.data()+4)==0);
 // Bounds exercise for hostile short/malformed USB records (ASan/UBSan).
 std::mt19937 random(8152);
 for(unsigned trial=0;trial<100000;trial++){
  size_t n=random()%wire.size();for(size_t i=0;i<n;i++)wire[i]=random();
  decode(wire.data(),n,[&](const uint8_t *p,size_t length,bool){assert(p>=wire.data()&&p+length<=wire.data()+n);});
 }
 std::cout<<"PASS native frame vectors, padding, RX aggregation/checksum flags, 100000 malformed records\n";
}
