// SPDX-License-Identifier: GPL-3.0-only
// Protocol emulator verifies production C++ byte lanes, cleanup, revision guards,
// and settings. It is not a substitute for hardware observations.
#include "esphome/core/defines.h"
#include "realtek_control.h"
#include "usb/usb_host.h"
#include <array>
#include <map>
#include <cassert>
#include <cstring>
#include <iostream>
using namespace esphome::ethernet;
std::array<uint8_t,65536> pla{};
std::map<uint16_t,uint16_t> ocp;
std::map<uint32_t,uint16_t> mmd;
uint16_t mmd_mode=0, mmd_addr=0;
usb_device_desc_t desc{0x0bda,0x8152,0x2000};
int calls=0, writes=0, fail_addr=-1, corrupt_mar=0;
uint16_t get16(unsigned a) { return pla[a] | (uint16_t(pla[a+1])<<8); }
void set16(unsigned a, uint16_t v) { pla[a]=v; pla[a+1]=v>>8; }
esp_err_t usbh_cdc_get_dev_handle(void *p, void **out) { *out=p; return ESP_OK; }
esp_err_t usb_host_get_device_descriptor(void *, const usb_device_desc_t **out) { *out=&desc; return ESP_OK; }
esp_err_t usbh_cdc_send_custom_request(void *, uint8_t dir, uint8_t req, uint16_t a, uint16_t idx, uint16_t n, uint8_t *b) {
  calls++;
  if (dir==0x21) { assert(req==0x43 && idx==0 && n==0 && (a==14||a==15)); return ESP_OK; }
  assert(req==5 && !(a&3) && !(n&3) && (idx&0xff00)==0x100);
  if (a==fail_addr) { fail_addr=-1; return ESP_ERR_TIMEOUT; }
  auto load = [](unsigned loc)->uint8_t {
    if ((loc & 0xf000)==0xb000) {
      uint16_t target=get16(0xe86c)|(loc&0xffe);
      uint16_t v=ocp[target];
      if (target==0xa41c && (mmd_mode&0x4000)) v=mmd[((mmd_mode&31)<<16)|mmd_addr];
      return v >> (8*(loc&1));
    }
    return pla[loc];
  };
  if (dir==0xc0) { for(unsigned i=0;i<n;i++) b[i]=load(a+i); if(a==0xcd00&&corrupt_mar&&!--corrupt_mar)b[0]^=1; return ESP_OK; }
  assert(dir==0x40); writes++;
  // Configuration register and OCP are the only writable ranges here.
  assert(a==0xcd00||a==0xc010||a==0xdd90||a==0xc000||a==0xe81c||a==0xe820||a==0xc0b4||a==0xe86c||a==0xe040||(a&0xf000)==0xb000);
  for (unsigned i=0;i<n;i++) {
    uint8_t lanes = n==4 ? idx&15 : (i<4 ? idx&15 : (idx>>4)&15);
    if (!(lanes&(1<<(i&3)))) continue;
    unsigned loc=a+i;
    if ((loc&0xf000)==0xb000) {
      uint16_t target=get16(0xe86c)|(loc&0xffe);
      uint16_t &v=ocp[target];
      v=(v & ~(0xff<<(8*(loc&1)))) | (uint16_t(b[i])<<(8*(loc&1)));
      if (loc&1) {
        if(target==0xa41a) mmd_mode=v;
        if(target==0xa41c) {
          if(mmd_mode&0x4000) mmd[((mmd_mode&31)<<16)|mmd_addr]=v;
          else mmd_addr=v;
        }
      }
    } else pla[loc]=b[i];
  }
  return ESP_OK;
}
#ifdef USE_ETHERNET_REALTEK_BULK
unsigned native_control_calls=0;
namespace esphome::ethernet {
esp_err_t realtek_native_device(usbh_cdc_port_handle_t port,usb_device_handle_t *device) {return usbh_cdc_get_dev_handle(port,device);}
esp_err_t realtek_native_request(usbh_cdc_port_handle_t port,uint8_t direction,uint8_t request,uint16_t value,uint16_t index,uint16_t length,uint8_t *data) {
  native_control_calls++;
  return usbh_cdc_send_custom_request(port,direction,request,value,index,length,data);
}
}
#endif
int main() {
  set16(0xe612,0x4f04); // wrong revision is rejected without writing
  RealtekControl r;
  assert(r.open((void*)1)==ESP_ERR_NOT_SUPPORTED && writes==0);
  set16(0xe612,0x4f10); // mask 7cf0 -> 4c10
  assert(r.open((void*)1)==ESP_OK);
  pla[0xc011]=0x55; pla[0xc012]=0xaa;
  assert(r.set_packet_filter(true,true)==ESP_OK && pla[0xc010]==15 && pla[0xc011]==0x55 && pla[0xc012]==0xaa);
  assert(r.set_packet_filter(false,true)==ESP_OK && pla[0xc010]==14);
  pla[0xcd00-1]=0x67;pla[0xcd08]=0x89;
  pla[0xc010]=0x0a;
  assert(r.set_multicast_hash(0x0123456789abcdefULL)==ESP_OK);
  const uint8_t mar_expected[8]={1,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
  assert(!memcmp(pla.data()+0xcd00,mar_expected,8));
  assert(pla[0xc010]==0x0e&&pla[0xc011]==0x55&&pla[0xc012]==0xaa);
  assert(pla[0xccff]==0x67&&pla[0xcd08]==0x89);
  assert(r.multicast_state().find("MAR=0123456789ABCDEF")!=std::string::npos);
  fail_addr=0xcd00;
  assert(r.set_multicast_hash(0)==ESP_ERR_TIMEOUT);
  assert(!memcmp(pla.data()+0xcd00,mar_expected,8));
  corrupt_mar=1;assert(r.set_multicast_hash(0)==ESP_ERR_INVALID_RESPONSE);
  assert(r.set_multicast_hash(~uint64_t(0))==ESP_OK);
  uint8_t mac[6]={2,0xe0,0x4c,0x36,4,0x79};
  pla[0xc006]=0x55; pla[0xc007]=0xaa;
  assert(r.set_mac(mac)==ESP_OK && !memcmp(pla.data()+0xc000,mac,6));
  assert(pla[0xc006]==0x55 && pla[0xc007]==0xaa && pla[0xe81c]==0);
  mac[5]++; fail_addr=0xc000;
  assert(r.set_mac(mac)==ESP_ERR_TIMEOUT && pla[0xe81c]==0);
  mac[0]=3;
  int before=calls;
  assert(r.set_mac(mac)==ESP_ERR_INVALID_ARG && calls==before);
  set16(0xe86c,0x5000); ocp[0xa408]=0x0401;
  assert(r.set_link(0x60)==ESP_OK && ocp[0xa408]==0x461 && ocp[0xa400]==0x1200);
  assert(get16(0xe86c)==0x5000);
  assert(r.set_link(0,false,10,false)==ESP_OK && ocp[0xa400]==0);
  assert(r.set_link(0,false,100,true)==ESP_OK && ocp[0xa400]==0x2100);
  assert(r.set_link(0x1000)==ESP_ERR_INVALID_ARG);
  fail_addr=0xb408;
  assert(r.set_link(0x1e0)==ESP_ERR_TIMEOUT && get16(0xe86c)==0x5000);
  assert(r.set_pause(true,false)==ESP_OK && (ocp[0xa408]&0xc00)==0xc00);
  assert(r.set_pause(false,true)==ESP_OK && (ocp[0xa408]&0xc00)==0x800);
  mmd[(3<<16)|20]=0;
  assert(r.set_eee(true)==ESP_ERR_NOT_SUPPORTED);
  mmd[(3<<16)|20]=2;
  assert(r.set_eee(true)==ESP_OK && mmd[(7<<16)|60]==2 && (get16(0xe040)&3)==3);
  pla[0xe908]=0x8b; ocp[0xa402]=0x786d; mmd[(7<<16)|61]=2;
  assert(r.diagnostics().find("EEE_negotiated=1")!=std::string::npos);
  ocp[0xa400]=0x2100;
  assert(r.diagnostics().find("EEE_negotiated=0")!=std::string::npos);
  assert(r.set_eee(false)==ESP_OK && mmd[(7<<16)|60]==0 && (get16(0xe040)&3)==0);
  assert(get16(0xe86c)==0x5000 && mmd_mode==0);
  set16(0xe820,0xa501); set16(0xe822,0x8002); set16(0xc0b6,0x4000);
  assert(r.set_wake(31)==ESP_OK && get16(0xe820)==0xa511 && get16(0xe822)==0x8072 && get16(0xc0b6)==0x4001);
  assert(r.set_wake(0)==ESP_OK && get16(0xe820)==0xa501 && get16(0xe822)==0x8002 && get16(0xc0b6)==0x4000);
  set16(0xdd90,0xb236);set16(0xdd92,0x5a06);
  assert(r.set_leds(0,0,1,2)==ESP_OK&&get16(0xdd90)==0xb200&&get16(0xdd92)==0x5a06);
  assert(r.set_leds(3,10,2,3)==ESP_OK&&get16(0xdd90)==0xb2a3&&get16(0xdd92)==0x5a0b);
  assert(r.set_leds(4,0,1,2)==ESP_ERR_INVALID_ARG);
  assert(r.set_packet_filter(true)==ESP_OK && r.set_packet_filter(false)==ESP_OK);
  pla[0xe890]=1; pla[0xe898]=2; pla[0xe8a8]=3;
  assert(r.counters().find("tx=1 rx=2")!=std::string::npos);
#ifdef USE_ETHERNET_REALTEK_BULK
  assert(native_control_calls>0); // A missing native branch must fail this build.
#endif
  std::cout<<"PASS Realtek protocol: revision guards, MAC lanes/readback, OCP bank cleanup, PHY modes, pause, EEE, wake filters, packet filter, counters\n";
}
