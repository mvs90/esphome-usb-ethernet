// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_tso.h"
#include "realtek_tcp_tracker.h"
#include <vector>
#include <cassert>
#include <cstdio>
using namespace esphome::ethernet::rtl_frames;
std::vector<uint8_t> packet(bool v6,unsigned seq,unsigned id,size_t payload,uint8_t flags=0x10) {
 size_t ip=14,tcp=v6?54:34;std::vector<uint8_t> frame(tcp+20+payload,0);put16(frame.data()+12,v6?0x86dd:0x800);
 auto *p=frame.data()+ip;p[0]=v6?0x60:0x45;p[v6?6:9]=6;p[v6?7:8]=64;
 put16(p+(v6?4:2),frame.size()-ip-(v6?40:0));if(!v6)put16(p+4,id);
 auto *t=frame.data()+tcp;t[0]=1;t[2]=2;t[4]=seq>>24;t[5]=seq>>16;t[6]=seq>>8;t[7]=seq;t[12]=0x50;t[13]=flags;t[14]=0x40;
 for(size_t i=0;i<payload;i++)t[20+i]=(seq+i)%251;
 std::vector<uint8_t> result(frame.size()+20);size_t n=encode(result.data(),result.size(),frame.data(),frame.size());assert(n);result.resize(n);return result;
}
int main() {
 uint8_t buffer[8192];
 for(bool v6:{false,true}) {
  auto a=packet(v6,1000,20,1000),b=packet(v6,2000,21,1000),c=packet(v6,3000,22,500,0x18);
  TcpTracker tracker;assert(!tracker.repeated(a.data()+8,a.size()-8));assert(!tracker.repeated(b.data()+8,b.size()-8));assert(tracker.repeated(a.data()+8,a.size()-8));assert(!tracker.repeated(c.data()+8,c.size()-8));
  auto syn=packet(v6,1000,20,1,0x12);assert(!tracker.repeated(syn.data()+8,syn.size()-8));assert(!tracker.repeated(b.data()+8,b.size()-8));
  TsoBuilder t(buffer,sizeof(buffer));assert(t.begin(a.data(),a.size()));assert(t.append(b.data(),b.size()));assert(t.append(c.data(),c.size()));assert(!t.append(c.data(),c.size()));size_t n=t.finish();assert(n==8+(v6?54:34)+20+2500);assert(t.segments()==3);assert(le32(buffer)&(1u<<(v6?27:28)));assert(le32(buffer+4)==1000u<<17);
  size_t tcp=v6?54:34;for(unsigned i=0;i<2500;i++)assert(buffer[8+tcp+20+i]==(1000+i)%251);
  assert(be16(buffer+8+tcp+16)==6);assert(buffer[8+tcp+13]==0x18);if(v6)assert(be16(buffer+8+14+4)==0);
  TsoBuilder negative(buffer,sizeof(buffer));assert(negative.begin(a.data(),a.size()));auto wrong=packet(v6,2001,21,1000);assert(!negative.append(wrong.data(),wrong.size()));assert(negative.finish()==0);
  wrong=packet(v6,2000,21,1000,0x11);assert(!negative.append(wrong.data(),wrong.size()));
  for(size_t length=0;length<a.size();length++){TsoBuilder short_input(buffer,sizeof(buffer));assert(!short_input.begin(a.data(),length));}
 }
 TcpTracker fuzz_tracker;
 uint32_t random=0x12345678;std::vector<uint8_t> malformed(1600);
 for(unsigned iteration=0;iteration<50000;iteration++) {
  for(auto &byte:malformed){random=random*1664525+1013904223;byte=random>>24;}
  size_t length=random%malformed.size();fuzz_tracker.repeated(malformed.data(),length);TsoBuilder fuzz(buffer,sizeof(buffer));
  if(fuzz.begin(malformed.data(),length)){fuzz.append(malformed.data(),length);assert(fuzz.finish()<=sizeof(buffer));}
 }
 std::puts("PASS IPv4/IPv6 TSO coalescing, seeds, sequencing, flags, truncated input guards");
}
