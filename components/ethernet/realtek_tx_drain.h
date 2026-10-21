// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace esphome::ethernet::rtl_tx_drain {
enum class Decision {WAIT,READY,EXPIRED};
// Do not interrupt a partially submitted native frame to enter USB suspend.
// Refuse an indefinitely blocked request without surrendering USB ownership.
constexpr Decision decide(bool pending,uint32_t now,uint32_t started,uint32_t budget=10000) {
  if(!pending)return Decision::READY;
  return uint32_t(now-started)>=budget?Decision::EXPIRED:Decision::WAIT;
}
}
