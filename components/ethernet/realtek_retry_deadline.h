// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace esphome::ethernet {
// Deadlines are at most 500 ms ahead; zero means no retry is deferred.
constexpr bool realtek_retry_ready(uint32_t now,uint32_t deadline) {
  return !deadline || int32_t(now-deadline)>=0;
}
}
