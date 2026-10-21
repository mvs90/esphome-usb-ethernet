// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_tx_drain.h"
#include <cassert>
#include <cstdio>
using namespace esphome::ethernet::rtl_tx_drain;
int main(){
  assert(decide(true,1000,1000)==Decision::WAIT);
  assert(decide(true,10999,1000)==Decision::WAIT);
  assert(decide(true,11000,1000)==Decision::EXPIRED);
  assert(decide(false,11000,1000)==Decision::READY);
  assert(decide(false,1000,1000)==Decision::READY);
  uint32_t start=0xfffffff0;
  assert(decide(true,uint32_t(start+9999),start)==Decision::WAIT);
  assert(decide(true,uint32_t(start+10000),start)==Decision::EXPIRED);
  assert(decide(false,uint32_t(start+10001),start)==Decision::READY);
  std::puts("PASS suspend drain: complete records first; bounded refusal preserves pending ownership; millis wrap");
}
