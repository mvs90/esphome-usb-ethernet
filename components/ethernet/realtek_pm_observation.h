// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace esphome::ethernet::rtl_pm {
struct ResumeObservation {uint32_t milliseconds;const char *reason;bool root_observed;};
// Device resume may precede the periodic root-state sample. Do not reuse a
// preceding cycle's duration or claim a sampled sleep interval in that case.
inline ResumeObservation resume_observation(bool root_observed,bool forced,bool manual,uint32_t entered,uint32_t now) {
  if(!root_observed)return {0,forced?(manual?"manual-unsampled":"timer-unsampled"):"device-unsampled",false};
  return {now-entered,forced?(manual?"manual":"timer"):"device",true};
}
}
