// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "esp_err.h"
#include "iot_usbh_cdc.h"
#include "usb/usb_host.h"
namespace esphome::ethernet {
#ifdef USE_RTL_FAULT_INJECTION
void realtek_native_test_timeout_next();
void realtek_native_test_hold_next_tx();
esp_err_t realtek_native_test_queue_pressure(const uint8_t *peer);
unsigned realtek_native_test_ndp_mask(bool reset=false);
#endif
esp_err_t realtek_native_device(usbh_cdc_port_handle_t port,usb_device_handle_t *device);
esp_err_t realtek_native_request(usbh_cdc_port_handle_t port,uint8_t direction,uint8_t request,uint16_t value,uint16_t index,uint16_t length,uint8_t *data);
}
