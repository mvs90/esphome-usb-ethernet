// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_transfer_lease.h"
#include <cassert>
#include <thread>
#include <future>
#include <cstdio>
using esphome::ethernet::RealtekTransferLease;
int main(){
 using Start=RealtekTransferLease::Start;
 for(unsigned i=0;i<64;i++) {
  RealtekTransferLease lease;assert(lease.begin()==Start::STARTED);
  std::promise<void> release;auto ready=release.get_future();
  std::thread callback([&](){ready.wait();lease.complete();});
  lease.timeout();for(unsigned n=0;n<100;n++)assert(lease.begin()==Start::BUSY);
  release.set_value();callback.join();assert(lease.begin()==Start::RECOVERED);
  assert(lease.in_flight());lease.complete();assert(lease.begin()==Start::STARTED);lease.complete();
 }
 for(unsigned i=0;i<100000;i++) {
  RealtekTransferLease lease;assert(lease.begin()==Start::STARTED);
  lease.complete();lease.timeout();assert(lease.begin()==Start::RECOVERED);lease.complete();
 }
 std::puts("PASS control ownership: timeout never releases pending USB allocation; late callback recovery and completion/timeout race");
}
