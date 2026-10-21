// SPDX-License-Identifier: GPL-3.0-only
#include "esphome/core/defines.h"
#include "realtek_control.h"
#include "realtek_adapter.h"
#include "realtek_multicast.h"
#include "realtek_pause.h"
#include "usb/usb_host.h"
#ifdef USE_ETHERNET_REALTEK_BULK
#include "realtek_usb_control.h"
#endif
#include <cstring>
#include <cstdio>

namespace esphome::ethernet {
#define TRY_OP(expr) do { esp_err_t err = (expr); if (err != ESP_OK) return err; } while (0)
static unsigned long long le(const uint8_t *p, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; i++) v |= uint64_t(p[i]) << (8 * i);
  return v;
}
esp_err_t RealtekControl::request_(uint8_t direction,uint8_t request,uint16_t value,uint16_t index,uint16_t length,uint8_t *data) {
#ifdef USE_ETHERNET_REALTEK_BULK
  return realtek_native_request(port_,direction,request,value,index,length,data);
#else
  return usbh_cdc_send_custom_request(port_,direction,request,value,index,length,data);
#endif
}
esp_err_t RealtekControl::read_(uint16_t a, uint8_t *p, uint16_t n) {
  if (!port_ || (a & 3) || (n & 3)) return ESP_ERR_INVALID_ARG;
  return request_(0xc0, 5, a, 0x0100, n, p);
}
esp_err_t RealtekControl::write_(uint16_t a, const uint8_t *p, uint16_t n, uint8_t mask) {
  if (!port_ || (a & 3) || (n & 3)) return ESP_ERR_INVALID_ARG;
  return request_(0x40, 5, a, 0x0100 | mask, n, const_cast<uint8_t *>(p));
}
esp_err_t RealtekControl::word_(uint16_t a, uint16_t &v) {
  uint8_t b[4]{};
  TRY_OP(read_(a & ~3, b));
  v = le(b + (a & 2), 2);
  return ESP_OK;
}
esp_err_t RealtekControl::put_word_(uint16_t a, uint16_t v) {
  uint8_t b[4]{};
  unsigned offset = a & 2;
  b[offset] = v; b[offset + 1] = v >> 8;
  return write_(a & ~3, b, 4, 0x33 << offset);
}
esp_err_t RealtekControl::byte_(uint16_t a, uint8_t v) {
  uint8_t b[4]{};
  b[a & 3] = v;
  return write_(a & ~3, b, 4, 0x11 << (a & 3));
}
esp_err_t RealtekControl::open(usbh_cdc_port_handle_t port) {
  port_ = nullptr;
  if (!port) return ESP_ERR_INVALID_STATE;
  usb_device_handle_t device = nullptr;
  const usb_device_desc_t *d = nullptr;
#ifdef USE_ETHERNET_REALTEK_BULK
  TRY_OP(realtek_native_device(port, &device));
#else
  TRY_OP(usbh_cdc_get_dev_handle(port, &device));
#endif
  TRY_OP(usb_host_get_device_descriptor(device, &d));
  if (!d || !rtl_adapter::supported(d->idVendor, d->idProduct, d->bcdDevice))
    return ESP_ERR_NOT_SUPPORTED;
  port_ = port;
  uint16_t revision;
  esp_err_t error = word_(0xe612, revision);
  if (error == ESP_OK && (revision & 0x7cf0) != 0x4c10) error = ESP_ERR_NOT_SUPPORTED;
  if (error != ESP_OK) port_ = nullptr;
  return error;
}
esp_err_t RealtekControl::phy_status(uint8_t &status) {
  uint8_t bytes[4]{}; TRY_OP(read_(0xe908, bytes)); status=bytes[0]; return ESP_OK;
}
esp_err_t RealtekControl::set_leds(uint8_t led0,uint8_t led1,uint8_t period,uint8_t duty) {
  if((led0 & ~0x0b)||(led1 & ~0x0b)||period>3||duty>3)return ESP_ERR_INVALID_ARG;
  uint16_t selected, feature;
  TRY_OP(word_(0xdd90,selected)); TRY_OP(word_(0xdd92,feature));
  selected=(selected & 0xff00)|led0|(led1<<4);
  feature=(feature & ~0x000f)|(period<<2)|duty;
  TRY_OP(put_word_(0xdd90,selected)); TRY_OP(put_word_(0xdd92,feature));
  uint16_t a,b;TRY_OP(word_(0xdd90,a));TRY_OP(word_(0xdd92,b));
  return a==selected&&b==feature ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
std::string RealtekControl::led_state() {
  uint16_t select,feature;esp_err_t e=word_(0xdd90,select);
  if(e==ESP_OK)e=word_(0xdd92,feature);
  if(e!=ESP_OK)return std::string("error: ")+esp_err_to_name(e);
  char out[70];snprintf(out,sizeof(out),"LEDSEL=%04X LEDFEATURE=%04X",select,feature);return out;
}
esp_err_t RealtekControl::initialize_native(bool rx_aggregation) {
  // Full-speed FIFO limits and packet framing are revision-specific register facts.
  // No EEPROM/OTP, firmware patch, or undocumented power cut is written.
  auto dword=[this](uint16_t address,uint32_t value)->esp_err_t {
    uint8_t b[4]={uint8_t(value),uint8_t(value>>8),uint8_t(value>>16),uint8_t(value>>24)};
    return this->write_(address,b,4,0xff);
  };
  auto usb_word=[this](uint16_t address,uint16_t clear,uint16_t set)->esp_err_t {
    uint8_t b[4]{};uint16_t base=address&~3;
    esp_err_t error=request_(0xc0,5,base,0,4,b);
    if(error!=ESP_OK)return error;
    unsigned offset=address&2;uint16_t value=(uint16_t(b[offset])|(uint16_t(b[offset+1])<<8));
    value=(value&~clear)|set;b[offset]=value;b[offset+1]=value>>8;
    return request_(0x40,5,base,0x33<<offset,4,b);
  };
  auto usb_byte=[this](uint16_t address,uint8_t value)->esp_err_t {
    uint8_t bytes[4]{};unsigned lane=address&3;bytes[lane]=value;
    return this->request_(0x40,5,address&~3,uint16_t(0x11u<<lane),4,bytes);
  };
  auto usb_dword=[this](uint16_t address,uint32_t value)->esp_err_t {
    uint8_t bytes[4]={uint8_t(value),uint8_t(value>>8),uint8_t(value>>16),uint8_t(value>>24)};
    return this->request_(0x40,5,address,0x00ff,4,bytes);
  };
  uint16_t aldps=0x0310; TRY_OP(ocp_(0x2010,aldps,true));
  TRY_OP(byte_(0xe813,0)); TRY_OP(dword(0xc010,0));
  uint16_t value; TRY_OP(word_(0xe84e,value));
  TRY_OP(byte_(0xe84f,uint8_t(value>>8)&~0x80));
  TRY_OP(word_(0xe8de,value));TRY_OP(put_word_(0xe8de,(value&~0x4000)|0x8000));
  TRY_OP(dword(0xc0a0,0x00080002)); TRY_OP(dword(0xc0a4,0x00000060));
  TRY_OP(dword(0xc0a8,0x00000078)); TRY_OP(dword(0xe618,0x01000008));
  TRY_OP(put_word_(0xc016,1522));
  TRY_OP(word_(0xe610,value));TRY_OP(put_word_(0xe610,value|0x80));
  TRY_OP(usb_word(0xd406,0x0098,rx_aggregation?0:0x0010)); // native framing, optional RX aggregation, no zero packets
  // RTL8152B 4C10 native USB FIFO/DMA parameters. ROM/ECM defaults measured
  // on this adapter are 06/3d090180/00000001. Configure the native engine
  // explicitly instead of inheriting a previous host's controller state.
  // Register facts: Linux r8152 v6.17 r8152b_exit_oob; original transaction code.
  TRY_OP(usb_byte(0xd40a,0x03));
  TRY_OP(usb_dword(0xd40c,0x7a120180));
  TRY_OP(usb_dword(0xd434,0x00000101));
  uint8_t aggregation[8]{},dma[4]{};
  TRY_OP(request_(0xc0,5,0xd408,0,8,aggregation));
  TRY_OP(request_(0xc0,5,0xd434,0,4,dma));
  if(aggregation[2]!=0x03 || le(aggregation+4,4)!=0x7a120180 || le(dma,4)!=0x00000101)
    return ESP_ERR_INVALID_RESPONSE;
  TRY_OP(word_(0xe854,value));TRY_OP(put_word_(0xe854,value&~0x0040)); // keep VLAN bytes in frame
  uint8_t all[8];memset(all,0xff,sizeof(all));TRY_OP(write_(0xcd00,all,8,0xff));
  TRY_OP(word_(0xc0b4,value));TRY_OP(put_word_(0xc0b4,value&~1));TRY_OP(put_word_(0xc0b4,value|1));
  TRY_OP(dword(0xc010,0x0e));TRY_OP(byte_(0xe813,0x0c));
  TRY_OP(word_(0xe85a,value));TRY_OP(put_word_(0xe85a,value&~8));
  return ESP_OK;
}
esp_err_t RealtekControl::set_mac(const uint8_t *mac) {
  if (!mac || (mac[0] & 1) || !(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]))
    return ESP_ERR_INVALID_ARG;
  uint8_t b[8]{};
  TRY_OP(read_(0xc000, b, 8));
  if (memcmp(b, mac, 6) == 0) return ESP_OK;
  memcpy(b, mac, 6); b[6] = b[7] = 0;
  esp_err_t unlock = byte_(0xe81c, 0xc0);
  if (unlock != ESP_OK) { byte_(0xe81c, 0); return unlock; }
  esp_err_t error = write_(0xc000, b, 8, 0x3f);
  esp_err_t restore = byte_(0xe81c, 0);
  if (error != ESP_OK) return error;
  TRY_OP(restore);
  TRY_OP(read_(0xc000, b, 8));
  return memcmp(b, mac, 6) == 0 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
// Each OCP operation saves/restores the bank, including error exits. Only this
// main-loop management layer accesses OCP; ECM itself doesn't access this bank.
esp_err_t RealtekControl::ocp_(uint16_t a, uint16_t &v, bool write) {
  uint16_t saved;
  TRY_OP(word_(0xe86c, saved));
  esp_err_t select = put_word_(0xe86c, a & 0xf000);
  if (select != ESP_OK) { put_word_(0xe86c, saved); return select; }
  esp_err_t error = write ? put_word_(0xb000 | (a & 0xfff), v) : word_(0xb000 | (a & 0xfff), v);
  esp_err_t restore = put_word_(0xe86c, saved);
  return error != ESP_OK ? error : restore;
}
esp_err_t RealtekControl::mmd_(uint8_t device, uint16_t a, uint16_t &v, bool write) {
  uint16_t dev = device, reg = a, mode = 0x4000 | device, zero = 0;
  esp_err_t select = ocp_(0xa41a, dev, true);
  if (select != ESP_OK) { ocp_(0xa41a, zero, true); return select; }
  esp_err_t error = ocp_(0xa41c, reg, true);
  if (error == ESP_OK) error = ocp_(0xa41a, mode, true);
  if (error == ESP_OK) error = ocp_(0xa41c, v, write);
  esp_err_t restore = ocp_(0xa41a, zero, true);
  return error != ESP_OK ? error : restore;
}
esp_err_t RealtekControl::set_link(uint16_t adv, bool autoneg, unsigned speed, bool full) {
  if ((adv & ~0x01e0) || (autoneg && !adv) || (speed != 10 && speed != 100)) return ESP_ERR_INVALID_ARG;
  if (autoneg) {
    uint16_t v;
    TRY_OP(ocp_(0xa408, v));
    v = (v & ~0x01e0) | adv;
    TRY_OP(ocp_(0xa408, v, true));
    uint16_t check;
    TRY_OP(ocp_(0xa408, check));
    if (check != v) return ESP_ERR_INVALID_RESPONSE;
  }
  uint16_t bmcr = autoneg ? 0x1200 : (speed == 100 ? 0x2000 : 0) | (full ? 0x0100 : 0);
  return ocp_(0xa400, bmcr, true);
}
esp_err_t RealtekControl::set_pause(bool rx, bool tx) {
  uint16_t adv;
  TRY_OP(ocp_(0xa408, adv));
  // IEEE 802.3 symmetric/asymmetric advertisement. Actual pause is negotiated.
  adv = (adv & ~0x0c00) | (rx ? 0x0400 : 0) | (rx != tx ? 0x0800 : 0);
  TRY_OP(ocp_(0xa408, adv, true));
  uint16_t check=0;TRY_OP(ocp_(0xa408,check));
  if(check!=adv)return ESP_ERR_INVALID_RESPONSE;
  uint16_t bmcr = 0x1200;
  return ocp_(0xa400, bmcr, true);
}
esp_err_t RealtekControl::set_eee(bool on) {
  uint16_t ability = 0;
  if (on) {
    TRY_OP(mmd_(3, 20, ability));
    ability &= 2;
    if (!ability) return ESP_ERR_NOT_SUPPORTED;
  }
  uint16_t mac, c1, c2, c3;
  TRY_OP(word_(0xe040, mac));
  TRY_OP(ocp_(0x2080, c1)); TRY_OP(ocp_(0x2092, c2)); TRY_OP(ocp_(0x2094, c3));
  mac = (mac & ~3) | (on ? 3 : 0);
  c1 = (c1 & ~0x3370) | (on ? 0x3310 : 0x0070);
  c2 = (c2 & ~0x0600) | (on ? 0x0600 : 0);
  c3 = (c3 & ~0xff80) | (on ? (42 << 7) : (511 << 7));
  TRY_OP(put_word_(0xe040, mac));
  TRY_OP(ocp_(0x2080, c1, true)); TRY_OP(ocp_(0x2092, c2, true)); TRY_OP(ocp_(0x2094, c3, true));
  TRY_OP(mmd_(7, 60, ability, true));
  uint16_t check;
  TRY_OP(mmd_(7, 60, check));
  if (check != ability) return ESP_ERR_INVALID_RESPONSE;
  uint16_t bmcr = 0x1200;
  return ocp_(0xa400, bmcr, true);
}
esp_err_t RealtekControl::set_wake(uint8_t filters) {
  if (filters & ~31) return ESP_ERR_INVALID_ARG;
  uint16_t link, packets, magic;
  TRY_OP(word_(0xe820, link)); TRY_OP(word_(0xe822, packets)); TRY_OP(word_(0xc0b6, magic));
  link = (link & ~0x10) | ((filters & 2) ? 0x10 : 0);
  packets = (packets & ~0x70) | ((filters & 4) ? 0x10 : 0) | ((filters & 8) ? 0x40 : 0) | ((filters & 16) ? 0x20 : 0);
  magic = (magic & ~1) | (filters & 1);
  esp_err_t unlock = byte_(0xe81c, 0xc0);
  if (unlock != ESP_OK) { byte_(0xe81c, 0); return unlock; }
  esp_err_t error = put_word_(0xe820, link);
  if (error == ESP_OK) error = put_word_(0xe822, packets);
  esp_err_t restore = byte_(0xe81c, 0);
  if (error != ESP_OK) return error;
  TRY_OP(restore); TRY_OP(put_word_(0xc0b6, magic));
  uint16_t a, b, c;
  TRY_OP(word_(0xe820, a)); TRY_OP(word_(0xe822, b)); TRY_OP(word_(0xc0b6, c));
  return ((a ^ link) & 0x10) || ((b ^ packets) & 0x70) || ((c ^ magic) & 1) ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}
esp_err_t RealtekControl::set_packet_filter(bool promiscuous, bool native) {
  if (native) {
    uint8_t value[4]{};
    TRY_OP(read_(0xc010, value));
    value[0] = (value[0] & ~0x0f) | (promiscuous ? 0x0f : 0x0e);
    TRY_OP(write_(0xc010, value, 4, 0x0f));
    uint8_t check[4]{}; TRY_OP(read_(0xc010, check));
    return memcmp(value, check, 4) ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
  }
  // CDC ECM interface 0 on the exact descriptor tuple accepted by open().
  return request_(0x21, 0x43, promiscuous ? 0x0f : 0x0e, 0, 0, nullptr);
}
esp_err_t RealtekControl::set_multicast_hash(uint64_t bitmap) {
  auto bytes=rtl_multicast::wire_bytes(bitmap);
  uint8_t rcr[4]{};TRY_OP(read_(0xc010,rcr));
  rcr[0]|=4; // Enable the multicast class; the hash decides admission.
  TRY_OP(write_(0xcd00,bytes.data(),8,0xff));
  TRY_OP(write_(0xc010,rcr,4,0xff));
  uint8_t actual[8]{},check[4]{};
  TRY_OP(read_(0xcd00,actual,8));TRY_OP(read_(0xc010,check));
  return memcmp(actual,bytes.data(),8) || memcmp(check,rcr,4) ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}
std::string RealtekControl::multicast_state() {
  uint8_t bytes[8]{},rcr[4]{};esp_err_t error=read_(0xcd00,bytes,8);
  if(error==ESP_OK)error=read_(0xc010,rcr);
  if(error!=ESP_OK)return std::string("error: ")+esp_err_to_name(error);
  uint64_t bitmap=0;for(auto byte:bytes)bitmap=(bitmap<<8)|byte;
  char out[60];snprintf(out,sizeof(out),"MAR=%016llX RCR=%08llX",(unsigned long long)bitmap,(unsigned long long)le(rcr,4));return out;
}
std::string RealtekControl::diagnostics() {
  uint8_t phy[4]{}, mac[8]{}, rcr[4]{};
  uint16_t bmcr, bmsr, adv, partner, ability, eee, peer, eee_mac, wake_link, wake_packet, wake_magic;
  esp_err_t error = read_(0xe908, phy);
#define READ_DIAG(expr) if (error == ESP_OK) error = (expr)
  READ_DIAG(read_(0xc000, mac, 8)); READ_DIAG(read_(0xc010, rcr));
  READ_DIAG(ocp_(0xa400, bmcr)); READ_DIAG(ocp_(0xa402, bmsr)); READ_DIAG(ocp_(0xa402, bmsr));
  READ_DIAG(ocp_(0xa408, adv)); READ_DIAG(ocp_(0xa40a, partner));
  READ_DIAG(mmd_(3, 20, ability)); READ_DIAG(mmd_(7, 60, eee)); READ_DIAG(mmd_(7, 61, peer));
  READ_DIAG(word_(0xe040, eee_mac)); READ_DIAG(word_(0xe820, wake_link));
  READ_DIAG(word_(0xe822, wake_packet)); READ_DIAG(word_(0xc0b6, wake_magic));
  if (error != ESP_OK) return std::string("error: ") + esp_err_to_name(error);
  char out[400];
  snprintf(out, sizeof(out), "hw=4C10 PHY=%02X link=%u speed=%u full=%u MAC=%02X:%02X:%02X:%02X:%02X:%02X BMCR=%04X BMSR=%04X ADV=%04X LPA=%04X EEE(cap/adv/peer/mac)=%04X/%04X/%04X/%04X EEE_negotiated=%u WOL=%04X/%04X/%04X RCR=%08llX",
           phy[0], !!(phy[0] & 2), (phy[0] & 8) ? 100 : 10, !!(phy[0] & 1),
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], bmcr, bmsr, adv, partner,
           ability, eee, peer, eee_mac, !!((bmcr & 0x1000) && (bmsr & 0x20) && (phy[0] & 2) && (phy[0] & 8) && (phy[0] & 1) && (eee_mac & 3) == 3 && (eee & peer & 2)), wake_link, wake_packet, wake_magic, le(rcr,4));
  return out;
}
// Passive snapshot of revision-bound FIFO/DMA/EEE register facts. This does
// not write chip configuration; OCP address-bank selection is restored by ocp_.
std::string RealtekControl::native_registers() {
  uint8_t usb_ctrl[4]{},aggregation[8]{},dma[4]{},ups[4]{},clock[4]{},fifo[4]{},cr[4]{},power_oob[4]{};
  esp_err_t error=request_(0xc0,5,0xd404,0,4,usb_ctrl);
  if(error==ESP_OK)error=request_(0xc0,5,0xd408,0,8,aggregation);
  if(error==ESP_OK)error=request_(0xc0,5,0xd434,0,4,dma);
  if(error==ESP_OK)error=request_(0xc0,5,0xd800,0,4,ups);
  if(error==ESP_OK)error=read_(0xe0c0,clock);
  if(error==ESP_OK)error=read_(0xe618,fifo);
  uint16_t plus=0,sff=0,eee1=0,eee2=0,eee3=0;
  if(error==ESP_OK)error=word_(0xe080,plus);
  if(error==ESP_OK)error=read_(0xe84c,power_oob);
  if(error==ESP_OK)error=read_(0xe810,cr);
  if(error==ESP_OK)error=word_(0xe8de,sff);
  if(error==ESP_OK)error=ocp_(0x2080,eee1);
  if(error==ESP_OK)error=ocp_(0x2092,eee2);
  if(error==ESP_OK)error=ocp_(0x2094,eee3);
  if(error!=ESP_OK)return std::string("error: ")+esp_err_to_name(error);
  char out[320];
  snprintf(out,sizeof(out),"USB_CTRL=%04X TX_AGG_LIMIT=%02X RX_BUF_TH=%08llX TX_DMA=%08llX UPS=%04X MAC_CLOCK=%08llX TX_FIFO=%08llX PHY_PWR=%04X MAC_CR=%02X OOB=%02X SFF=%04X EEE_PLUS=%04X EEE_PHY=%04X/%04X/%04X",
    unsigned(le(usb_ctrl+2,2)),aggregation[2],le(aggregation+4,4),le(dma,4),unsigned(le(ups,2)),le(clock,4),le(fifo,4),unsigned(le(power_oob,2)),cr[3],power_oob[3],sff,plus,eee1,eee2,eee3);
  return out;
}
std::string RealtekControl::link_capabilities() {
  uint16_t bmcr=0,bmsr=0,adv=0,peer=0,eee=0,wake=0;uint8_t phy=0;
  esp_err_t e=phy_status(phy);
  if(e==ESP_OK)e=ocp_(0xa400,bmcr);
  if(e==ESP_OK)e=ocp_(0xa402,bmsr); // BMSR link status is latch-low; read current again.
  if(e==ESP_OK)e=ocp_(0xa402,bmsr);
  if(e==ESP_OK)e=ocp_(0xa408,adv);
  if(e==ESP_OK)e=ocp_(0xa40a,peer);
  if(e==ESP_OK)e=mmd_(3,20,eee);
  if(e==ESP_OK)e=mmd_(3,22,wake); // Standard PCS EEE wake-error register, raw evidence.
  if(e!=ESP_OK)return std::string("error: ")+esp_err_to_name(e);
  bool operational=(phy&3)==3&&(bmcr&0x1000)&&(bmsr&0x20);
  auto pause=rtl_pause::resolve(adv,peer,operational);char out[220];
  snprintf(out,sizeof(out),"PHY=%02X BMCR=%04X BMSR=%04X ADV=%04X LPA=%04X pause_valid=%u pause_rx=%u pause_tx=%u EEE_cap=%04X EEE_wake_error_raw=%04X",phy,bmcr,bmsr,adv,peer,operational,!!(pause&1),!!(pause&2),eee,wake);
  return out;
}
std::string RealtekControl::counters() {
  uint8_t b[64]{};
  esp_err_t error = read_(0xe890, b, sizeof(b));
  if (error != ESP_OK) return std::string("error: ") + esp_err_to_name(error);
  char out[400];
  snprintf(out, sizeof(out), "tx=%llu rx=%llu txerr=%llu rxerr=%llu missed=%llu align=%llu onecoll=%llu multicoll=%llu unicast=%llu broadcast=%llu multicast=%llu aborted=%llu underrun=%llu",
           le(b,8), le(b+8,8), le(b+16,8), le(b+24,4), le(b+28,2), le(b+30,2), le(b+32,4), le(b+36,4), le(b+40,8), le(b+48,8), le(b+56,4), le(b+60,2), le(b+62,2));
  return out;
}
#undef TRY_OP
#undef READ_DIAG
}
