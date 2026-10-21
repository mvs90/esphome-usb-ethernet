// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_tx_integrity.h"
#include <cassert>
#include <vector>
#include <fstream>
#include <iterator>
#include <random>
#include <iostream>
using namespace esphome::ethernet::rtl_integrity;
uint64_t bits(const Bits &b){return uint64_t(b.hi.load())<<32|b.lo.load();}
void reset(unsigned f){family[f].cycle.store(UINT32_MAX-1);family[f].epoch(UINT32_MAX-2);}
int main(int argc,char **argv){
  assert(argc==3);const uint8_t mac[6]={2,0,0,0,0,2};
  const uint8_t own[6]={2,0,0,0,0,1};expected_peer(mac,own);
  for(unsigned which=0;which<2;which++){
    std::ifstream f(argv[which+1],std::ios::binary);assert(f);
    std::vector<uint8_t> original((std::istreambuf_iterator<char>(f)),{});assert(original.size()>1400);
    auto &e=family[which];unsigned sequence=which?63:0;uint64_t want=uint64_t(1)<<sequence;
    reset(which);inspect(original.data(),original.size(),false);
    assert(bits(e.seen)==want&&!bits(e.l2)&&!bits(e.ip)&&!bits(e.udp)&&!bits(e.payload)&&!bits(e.offloaded));
    assert(e.text().find("off=0000000000000000")!=std::string::npos);
    reset(which);inspect(original.data(),original.size(),false,0xc0000000u|unsigned(original.size()));assert(!bits(e.descriptor));
    reset(which);inspect(original.data(),original.size(),false,0x80000000u|unsigned(original.size()));assert(bits(e.descriptor)==want);
    reset(which);inspect(original.data(),original.size(),false,0xc0000000u|unsigned(original.size()-1));assert(bits(e.descriptor)==want);
    auto frame=original;frame[0]^=1;reset(which);inspect(frame.data(),frame.size(),false);
    assert(bits(e.l2)==want&&!bits(e.ip)&&!bits(e.udp)&&!bits(e.payload));
    frame=original;frame[6]^=1;reset(which);inspect(frame.data(),frame.size(),false);assert(bits(e.l2)==want);
    frame=original;if(!which)frame[22]^=1;else frame[15]^=1;
    reset(which);inspect(frame.data(),frame.size(),false);assert(bits(e.ip)==(which?0:want));assert(!bits(e.udp));
    frame=original;size_t udp=which?54:34;frame[udp+6]^=1;
    reset(which);inspect(frame.data(),frame.size(),false);assert(bits(e.udp)==want&&!bits(e.payload));
    reset(which);inspect(frame.data(),frame.size(),true);assert(!bits(e.udp)&&bits(e.offloaded)==want);
    frame=original;frame[udp+6]=0;frame[udp+7]=0;
    reset(which);inspect(frame.data(),frame.size(),false);assert(bits(e.udp)==(which?want:0));
    frame=original;frame.back()^=1;reset(which);inspect(frame.data(),frame.size(),false);
    assert(bits(e.udp)==want&&bits(e.payload)==want&&!bits(e.l2));
    for(size_t n=0;n<original.size();n++){reset(which);inspect(original.data(),n,false);assert(!bits(e.seen));}
    frame=original;frame[udp+19]=64;reset(which);inspect(frame.data(),frame.size(),false);assert(!bits(e.seen));
  }
  std::mt19937 random(8152);std::vector<uint8_t> fuzz(2000);
  for(unsigned i=0;i<10000;i++){size_t n=random()%fuzz.size();for(size_t k=0;k<n;k++)fuzz[k]=random();inspect(fuzz.data(),n,false);}
  std::cout<<"PASS synthetic independent IPv4 / IPv6 golden, MAC/checksum/payload faults, partial-checksum distinction, sequence63/64, every truncation and 10000 malformed frames\n";
}
