#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ETHERNET
#include "ethernet_component.h"
#include "esphome/core/log.h"

namespace esphome::ethernet {

template<typename... Ts> class EthernetConnectedCondition final : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_eth_component->is_connected(); }
};

template<typename... Ts> class EthernetEnabledCondition final : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_eth_component->is_enabled(); }
};

template<typename... Ts> class EthernetEnableAction final : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_eth_component->enable(); }
};

template<typename... Ts> class EthernetDisableAction final : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { global_eth_component->disable(); }
};

// Values are schema-validated, static parameters. Errors remain visible in logs.
template<typename... Ts> class RealtekControlAction final : public Action<Ts...> {
 public:
  void set_parameters(int operation, int a, int b, int c, int d) {
    this->operation_ = operation; this->a_ = a; this->b_ = b; this->c_ = c; this->d_ = d;
  }
  void play(const Ts &...x) override {
    auto *eth = global_eth_component;
    esp_err_t error = ESP_ERR_INVALID_ARG;
    switch (this->operation_) {
      case 0: error = eth->realtek_set_link(this->a_, this->b_, this->c_, this->d_); break;
      case 1: error = eth->realtek_set_eee(this->a_); break;
      case 2: error = eth->realtek_set_wake(this->a_); break;
      case 3: error = eth->realtek_set_pause(this->a_, this->b_); break;
      case 4: error = eth->realtek_set_promiscuous(this->a_); break;
      case 5: error = eth->realtek_set_leds(this->a_, this->b_, this->c_, this->d_); break;
      case 6: error = eth->suspend_native(this->a_, this->b_, this->c_); break;
      case 7: error = eth->set_native_tx_mode(this->a_ ? 2 : 0); break;
      case 8: error = eth->resume_native(); break;
      case 9: error = eth->set_native_tso(this->a_); break;
    }
    if (error != ESP_OK) ESP_LOGE("ethernet.realtek", "Control operation %d failed: %s", this->operation_, esp_err_to_name(error));
  }
 protected:
  int operation_{0}, a_{0}, b_{0}, c_{100}, d_{1};
};

// Explicit static multicast hash selection. All-multicast is the boot default.
template<typename... Ts> class RealtekMulticastAction final : public Action<Ts...> {
 public:
  void set_parameters(uint64_t bitmap,bool preserve_basic_groups) {this->bitmap_=bitmap;this->preserve_basic_groups_=preserve_basic_groups;}
  void play(const Ts &...x) override {
    esp_err_t error=global_eth_component->realtek_set_multicast_filter(this->bitmap_,this->preserve_basic_groups_);
    if(error!=ESP_OK)ESP_LOGE("ethernet.realtek","Multicast filter failed: %s",esp_err_to_name(error));
  }
 protected:
  uint64_t bitmap_{UINT64_MAX};bool preserve_basic_groups_{true};
};

}  // namespace esphome::ethernet
#endif
