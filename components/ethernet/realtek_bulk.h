// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "iot_eth.h"
#include <string>
#include "iot_usbh_cdc.h"
#include "realtek_usb_control.h"
namespace esphome::ethernet {
#ifdef USE_RTL_BOOT_TALLY_PROBE
void realtek_native_boot_tally_record(unsigned slot,const char *stage);
std::string realtek_native_boot_tally(unsigned slot);
#endif
esp_err_t new_realtek_bulk(iot_eth_driver_t **out);
usbh_cdc_port_handle_t realtek_bulk_port(iot_eth_driver_t *driver);
esp_err_t realtek_bulk_resume(iot_eth_driver_t *driver,bool timer=false);
esp_err_t realtek_bulk_suspend(iot_eth_driver_t *driver,uint32_t maximum_ms,bool remote);
bool realtek_bulk_reinitialized(iot_eth_driver_t *driver);
bool realtek_bulk_resume_ready(iot_eth_driver_t *driver);
std::string realtek_bulk_pm_status(iot_eth_driver_t *driver);
void realtek_bulk_tx_mode(iot_eth_driver_t *driver,unsigned mode);
void realtek_bulk_tx_aggregation(iot_eth_driver_t *driver,bool enabled);
std::string realtek_bulk_queue_statistics(iot_eth_driver_t *driver);
std::string realtek_bulk_errors(iot_eth_driver_t *driver);
std::string realtek_bulk_statistics(iot_eth_driver_t *driver);
void realtek_bulk_aggregation(iot_eth_driver_t *driver,bool tx,bool rx);
void realtek_bulk_tso(iot_eth_driver_t *driver,bool enabled);
esp_err_t realtek_bulk_quiesce_phy(iot_eth_driver_t *driver);
void realtek_bulk_poll(iot_eth_driver_t *driver);
}
