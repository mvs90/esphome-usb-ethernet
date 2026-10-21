#pragma once
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_ERR_INVALID_ARG=1, ESP_ERR_INVALID_STATE=2, ESP_ERR_NOT_SUPPORTED=3, ESP_ERR_INVALID_RESPONSE=4, ESP_ERR_TIMEOUT=5;
inline const char *esp_err_to_name(int) { return "test error"; }
