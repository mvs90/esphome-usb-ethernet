#pragma once
#include <cstdint>
#include "esp_err.h"
using usb_device_handle_t = void *;
struct usb_device_desc_t { uint16_t idVendor, idProduct, bcdDevice; };
esp_err_t usb_host_get_device_descriptor(void *, const usb_device_desc_t **);
