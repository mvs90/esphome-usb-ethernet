// SPDX-License-Identifier: GPL-3.0-only
#include "realtek_pm_observation.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace esphome::ethernet::rtl_pm;
int main(){
 auto device=resume_observation(true,false,false,100,1350);assert(device.root_observed&&device.milliseconds==1250&&!strcmp(device.reason,"device"));
 auto timer=resume_observation(true,true,false,100,12100);assert(timer.milliseconds==12000&&!strcmp(timer.reason,"timer"));
 auto manual=resume_observation(true,true,true,100,1600);assert(manual.milliseconds==1500&&!strcmp(manual.reason,"manual"));
 auto wrap=resume_observation(true,false,false,0xfffffff0u,16);assert(wrap.milliseconds==32);
 auto fast=resume_observation(false,false,false,100,99999);assert(!fast.root_observed&&fast.milliseconds==0&&!strcmp(fast.reason,"device-unsampled"));
 auto no_timer=resume_observation(false,true,false,100,12100);assert(!no_timer.root_observed&&no_timer.milliseconds==0&&!strcmp(no_timer.reason,"timer-unsampled"));
 auto no_manual=resume_observation(false,true,true,100,300);assert(!no_manual.root_observed&&no_manual.milliseconds==0&&!strcmp(no_manual.reason,"manual-unsampled"));
 std::cout<<"PASS sampled device/timer/manual, millis wrap, unsampled resume never reuses prior duration or counts measured root sleep\n";
}
