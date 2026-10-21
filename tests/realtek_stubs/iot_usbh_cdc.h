#pragma once
#include <cstdint>
#include "esp_err.h"
using usbh_cdc_port_handle_t = void *;
esp_err_t usbh_cdc_send_custom_request(void *, uint8_t, uint8_t, uint16_t, uint16_t, uint16_t, uint8_t *);
esp_err_t usbh_cdc_get_dev_handle(void *, void **);
