// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_test_datagrams.h"
#include <cassert>
#include <vector>
#include <random>
#include <iostream>
using namespace esphome::ethernet::rtl_probe;
static void put(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=n>>(24-8*i);}
int main() {
  uint8_t payload[12]={'L','O','A','D'};Burst b;
  put(payload+4,3);put(payload+8,0);b.note(payload,12);assert(b.text()=="00000003:0000000000000001");
  put(payload+8,63);b.note(payload,12);assert(b.text()=="00000003:8000000000000001");
  put(payload+8,64);b.note(payload,12);assert(b.text()=="00000003:8000000000000001");
  put(payload+4,4);put(payload+8,2);b.note(payload,12);assert(b.text()=="00000004:0000000000000004");
  Burst stage[2];std::vector<uint8_t> v4(54);v4[12]=8;v4[14]=0x45;v4[23]=17;v4[36]=0x0d;v4[37]=0x05;
  put(payload+4,7);put(payload+8,31);memcpy(v4.data()+42,payload,12);frame(stage,v4.data(),v4.size());assert(stage[0].text()=="00000007:0000000080000000");
  std::vector<uint8_t> v6(74);v6[12]=0x86;v6[13]=0xdd;v6[14]=0x60;v6[20]=17;v6[56]=0x0d;v6[57]=0x06;
  put(payload+4,9);put(payload+8,32);memcpy(v6.data()+62,payload,12);
  for(size_t n=0;n<v6.size();n++)frame(stage,v6.data(),n);
  assert(stage[1].cycle==UINT32_MAX);frame(stage,v6.data(),v6.size());assert(stage[1].text()=="00000009:0000000100000000");
  std::mt19937 random(8152);std::vector<uint8_t> fuzz(2000);
  for(unsigned i=0;i<10000;i++){size_t n=random()%fuzz.size();for(size_t k=0;k<n;k++)fuzz[k]=random();frame(stage,fuzz.data(),n);}
  std::cout<<"PASS diagnostic burst epochs, bit boundaries, IPv4/IPv6 frame bounds and 10000 malformed inputs\n";
}
