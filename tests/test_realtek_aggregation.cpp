// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_aggregation.h"
#include <cassert>
#include <vector>
#include <random>
#include <iostream>
using namespace esphome::ethernet::rtl_frames;
int main(){
 {StreamDecoder<6144> decoder;uint8_t bad[64]{};unsigned frames=0;
  assert(!decoder.feed(bad,64,false,[&](auto*,auto,auto){frames++;}));
  uint8_t continuation[128]{};put32(continuation,64);
  assert(decoder.feed(continuation,128,false,[&](auto*,auto,auto){frames++;}));
  assert(frames==0);
  assert(decoder.feed(continuation,88,true,[&](auto*,auto,auto){frames++;}));assert(frames==0);
  assert(decoder.feed(continuation,88,true,[&](auto*,auto,auto){frames++;}));assert(frames==1);
 }

 std::vector<uint8_t> aggregate;
 std::vector<std::vector<uint8_t>> expected;
 for(unsigned n=0;n<50;n++){
  size_t frame=60+(n*37)%1455;std::vector<uint8_t> record(24+frame+4);
  put32(record.data(),frame+4);for(size_t i=0;i<frame;i++)record[24+i]=(i+n)&255;
  expected.emplace_back(record.begin()+24,record.end()-4);
  aggregate.insert(aggregate.end(),record.begin(),record.end());
  if(n!=49)aggregate.resize((aggregate.size()+7)&~size_t(7),0);
 }
 for(size_t chunk:{size_t(64),size_t(128),size_t(4096)}){
  StreamDecoder<6144> parser;size_t count=0;
  for(size_t pos=0;pos<aggregate.size();pos+=chunk){
   size_t size=std::min(chunk,aggregate.size()-pos);
   assert(parser.feed(aggregate.data()+pos,size,pos+size==aggregate.size(),[&](const uint8_t *p,size_t n,bool bad){
    assert(!bad&&count<expected.size());assert(std::vector<uint8_t>(p,p+n)==expected[count++]);
   }));
  }
  assert(count==50&&parser.pending()==0);
 }
 uint8_t invalid[30]{};put32(invalid,1523);StreamDecoder<6144> parser;
 assert(!parser.feed(invalid,sizeof(invalid),true,[](const uint8_t*,size_t,bool){assert(false);}));
 put32(invalid,64);assert(!parser.feed(invalid,sizeof(invalid),true,[](const uint8_t*,size_t,bool){assert(false);}));
 std::array<uint8_t,1600> frame{},record{};std::array<uint8_t,8192> tx{};
 size_t encoded=encode(record.data(),record.size(),frame.data(),1514);
 size_t first=append_tx_record(tx.data(),tx.size(),0,record.data(),encoded);assert(first==encoded);
 size_t second=append_tx_record(tx.data(),tx.size(),first,record.data(),encoded);
 assert(second==((first+3)&~size_t(3))+encoded);
 assert(append_tx_record(tx.data(),second-1,first,record.data(),encoded)==0);
 std::mt19937 random(8152);
 for(unsigned i=0;i<20000;i++){
  size_t size=random()%4097;for(size_t n=0;n<size;n++)tx[n]=random();
  parser.feed(tx.data(),size,random()%2,[](const uint8_t*,size_t n,bool){assert(n<=1518);});
 }
 std::cout<<"PASS RX streaming across USB boundaries, TX record alignment/bounds, 20000 malformed streams\n";
}
