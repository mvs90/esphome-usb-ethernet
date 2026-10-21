// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_retry_deadline.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using esphome::ethernet::realtek_retry_ready;
int main() {
  // A normal reconnect must work throughout the 32-bit uptime range.
  for(uint32_t now:{0u,65000u,0x7fffffffu,0x80000000u,0xffffffffu})
    assert(realtek_retry_ready(now,0));
  // Retry scheduled before the signed half-range boundary: exactly 500 ms.
  assert(!realtek_retry_ready(0x7fffff00u,0x800000f4u));
  assert(!realtek_retry_ready(0x800000f3u,0x800000f4u));
  assert(realtek_retry_ready(0x800000f4u,0x800000f4u));
  assert(realtek_retry_ready(0x800000f5u,0x800000f4u));
  // The same delay across the actual millis() wrap at 49.7 days.
  assert(!realtek_retry_ready(0xffffff00u,0x000000f4u));
  assert(!realtek_retry_ready(0x000000f3u,0x000000f4u));
  assert(realtek_retry_ready(0x000000f4u,0x000000f4u));
  assert(realtek_retry_ready(0x00000100u,0x000000f4u));
  std::puts("PASS ordinary reconnect beyond 24.9 days and deferred retry across millis wrap");
}
