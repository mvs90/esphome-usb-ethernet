// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <atomic>
namespace esphome::ethernet {
// A timeout changes the caller's result, never USB-controller ownership.
// Only the actual completion callback makes this allocation reusable again.
class RealtekTransferLease {
 public:
  enum class Start {BUSY,STARTED,RECOVERED};
  Start begin() {
    bool expected=false;
    if(!pending_.compare_exchange_strong(expected,true,std::memory_order_acq_rel))return Start::BUSY;
    return quarantined_.exchange(false,std::memory_order_acq_rel)?Start::RECOVERED:Start::STARTED;
  }
  void complete() {pending_.store(false,std::memory_order_release);}
  void timeout() {quarantined_.store(true,std::memory_order_release);}
  bool in_flight() const {return pending_.load(std::memory_order_acquire);}
 private:
  std::atomic<bool> pending_{false},quarantined_{false};
};
}
