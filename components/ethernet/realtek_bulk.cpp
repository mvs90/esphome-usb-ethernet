// SPDX-License-Identifier: GPL-3.0-only
// Original native transport; uses Espressif CDC only for generic vendor bulk I/O.
#include "esphome/core/defines.h"
#ifdef USE_ETHERNET_REALTEK_BULK
#include "realtek_bulk.h"
#include "realtek_control.h"
#include "realtek_pm_observation.h"
#include "realtek_frames.h"
#include "realtek_aggregation.h"
#include "realtek_tso.h"
#include "realtek_tcp_tracker.h"
#include "realtek_transfer_lease.h"
#include "realtek_tx_drain.h"
#ifdef USE_RTL_FAULT_INJECTION
#include "realtek_test_datagrams.h"
#include "realtek_tx_integrity.h"
#endif
#include "iot_eth_interface.h"
#include "iot_eth_types.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "usb/cdc_acm_host.h"
#include "freertos/task.h"
namespace esphome::ethernet {
static constexpr unsigned TX_QUEUE_CAPACITY=32;
static TickType_t tx_queue_wait_ticks(){TickType_t ticks=pdMS_TO_TICKS(5);return ticks?ticks:1;}
struct TxRecord { size_t count; uint32_t generation; uint8_t data[1600]; };
struct Bulk {
  iot_eth_driver_t base{};  // Must be first: callbacks receive &base.
  QueueHandle_t tx_queue{nullptr},retired_devices{nullptr};
  SemaphoreHandle_t port_mutex{nullptr};
  iot_eth_mediator_t *mediator{nullptr};
  std::atomic<usbh_cdc_port_handle_t> port{nullptr};
  std::atomic<uint8_t> address{0};
  std::atomic<bool> data_resumed{false};
  std::atomic<cdc_acm_dev_hdl_t> disconnected{nullptr};
  uint8_t mac[6]{};
  std::atomic<bool> initialized{false}, linked{false};
  usb_host_client_handle_t monitor{nullptr};
  usb_transfer_t *control{nullptr};
  RealtekTransferLease control_lease;
  // OUT ownership is independent of a caller's deadline. Ethernet PAUSE can
  // legitimately keep this URB pending indefinitely; never cancel/reuse it
  // merely because a software wait expired.
  usb_transfer_t *tx_transfer{nullptr};
  RealtekTransferLease tx_lease;
  SemaphoreHandle_t tx_done{nullptr};
  uint8_t tx_endpoint{0};
  bool tx_result_pending{false}; // TX-worker-only
  uint32_t tx_submission_generation{0};
  unsigned tx_submission_frames{0};
  bool tx_submission_segmented{false};
  std::atomic<unsigned> tx_usb_waits{0};
  std::atomic<unsigned> control_timeouts{0},control_recovered{0};
#ifdef USE_RTL_FAULT_INJECTION
  std::atomic<bool> inject_control_timeout{false};
  std::atomic<bool> hold_next_tx{false},queue_pressure_hold{false};
  std::atomic<unsigned> test_ndp_rx_mask{0};
#endif
  SemaphoreHandle_t control_done{nullptr};
  std::atomic<usb_device_handle_t> monitored{nullptr};
  std::atomic<bool> paused{false}, resumed_event{false}, resume_ready{false};
  uint32_t suspend_requested{0}, suspend_entered{0}, last_suspended_ms{0}, maximum_ms{0};
  bool forced{false}, seen_suspended{false}, hub_remote{false}, nic_remote{false};
  std::atomic<bool> suspend_pending{false};
  unsigned suspend_count{0}, resume_count{0}, unsampled_resumes{0};
  std::string last_resume{"none"}, force_reason{"timer"};
  uint32_t last_poll{0},phy_hold_until{0};
  std::atomic<unsigned> tx_mode{0},tx_hw{0},tx_sw{0},rx_count{0},rx_bad{0},rx_invalid{0};
  std::atomic<bool> tx_aggregation{false};
  bool rx_aggregation{false};
  rtl_frames::StreamDecoder<6144> decoder;
  uint8_t tx_buffer[8192]{};
  std::atomic<unsigned> tx_urbs{0},tx_multi{0},rx_urbs{0},rx_multi{0};
  std::atomic<bool> tso{false};
  std::atomic<unsigned> tso4{0},tso6{0},tso_segments{0},tcp_repeat{0};
  rtl_frames::TcpTracker tcp_tracker;
  std::atomic<unsigned> tx_drop{0},tx_errors{0},rx_drop{0},rx_stack_drop{0};
  std::atomic<unsigned> tx_waited{0},tx_wait_timeout{0},tx_alloc_fail{0},tx_peak{0};
  std::atomic<uint32_t> tx_generation{1};
  std::atomic<unsigned> tx_discard{0},tx_stale{0};
  std::atomic<bool> reset_decoder{true},reinitialized{false};
#ifdef USE_RTL_BOOT_TALLY_PROBE
  std::string boot_tally[8];uint32_t probe_initialized_at{0};
#endif
};
static_assert(offsetof(Bulk,base)==0,"iot_eth callback context must point to Bulk start");
static Bulk *self(iot_eth_driver_t *d) { return reinterpret_cast<Bulk *>(d); }
static Bulk *active_bulk=nullptr;
static void control_done(usb_transfer_t *transfer) {
  auto *b=static_cast<Bulk *>(transfer->context);b->control_lease.complete();xSemaphoreGive(b->control_done);
}
static void native_tx_done(usb_transfer_t *transfer) {
  auto *b=static_cast<Bulk *>(transfer->context);
  b->tx_lease.complete();
  xSemaphoreGive(b->tx_done);
}
// Only the TX worker consumes an OUT result. The callback has returned hardware
// ownership before this can inspect/reuse the buffer. Suspend/disconnect
// cancellation belongs to the old generation and is an expected discard.
static bool finish_native_tx(Bulk *b) {
  if(!b->tx_result_pending)return true;
  if(b->tx_lease.in_flight())return false;
  b->tx_result_pending=false;
  auto *t=b->tx_transfer;
  if(t->status!=USB_TRANSFER_STATUS_COMPLETED || t->actual_num_bytes!=t->num_bytes) {
    if(b->tx_submission_generation!=b->tx_generation.load())b->tx_discard+=b->tx_submission_frames;
    else {b->tx_errors++;ESP_LOGW("ethernet.rtl","Native OUT completion status=%u bytes=%u/%u",unsigned(t->status),unsigned(t->actual_num_bytes),unsigned(t->num_bytes));}
  }
#ifdef USE_RTL_FAULT_INJECTION
  else if(!b->tx_submission_segmented) {
    const uint8_t *data=t->data_buffer;size_t bytes=t->actual_num_bytes;
    for(size_t pos=0;pos+8<=bytes;) {
      size_t length=rtl_frames::le32(data+pos)&0x3ffff;
      if(length>bytes-pos-8)break;
      rtl_probe::frame(rtl_probe::usb_tx,data+pos+8,length);
      rtl_integrity::inspect(data+pos+8,length,rtl_frames::le32(data+pos+4)!=0,rtl_frames::le32(data+pos));
      pos=(pos+8+length+3)&~size_t(3);
    }
  }
#endif
  return true;
}
static esp_err_t submit_native_tx(Bulk *b,const uint8_t *data,size_t bytes,unsigned frames,uint32_t generation,bool segmented) {
  auto device=b->monitored.load();
  if(!device || !b->tx_endpoint || !b->tx_transfer)return ESP_ERR_INVALID_STATE;
  if(!bytes || bytes>size_t(b->tx_transfer->data_buffer_size))return ESP_ERR_INVALID_SIZE;
  if(b->tx_lease.begin()==RealtekTransferLease::Start::BUSY)return ESP_ERR_NOT_FINISHED;
  while(xSemaphoreTake(b->tx_done,0)==pdTRUE){}
  auto *t=b->tx_transfer;
  memcpy(t->data_buffer,data,bytes);
  t->device_handle=device;t->bEndpointAddress=b->tx_endpoint;
  t->num_bytes=bytes;t->timeout_ms=0;
  b->tx_submission_generation=generation;b->tx_submission_frames=frames;
  b->tx_submission_segmented=segmented;
  b->tx_result_pending=true;
  esp_err_t error=usb_host_transfer_submit(t);
  if(error!=ESP_OK){b->tx_result_pending=false;b->tx_lease.complete();}
  return error;
}
#ifdef USE_RTL_FAULT_INJECTION
void realtek_native_test_timeout_next() {if(active_bulk)active_bulk->inject_control_timeout.store(true);}
esp_err_t realtek_native_test_queue_pressure(const uint8_t *peer) {
  auto *b=active_bulk;
  if(!b || !peer || !b->linked.load() || b->paused.load())return ESP_ERR_INVALID_STATE;
  uint8_t frame[60]{};memcpy(frame,peer,6);memcpy(frame+6,b->mac,6);
  frame[12]=0x88;frame[13]=0xb5;memcpy(frame+14,"RTL-QTEST",9);
  b->queue_pressure_hold.store(true);unsigned accepted=0,refused=0;
  for(unsigned i=0;i<64;i++) {
    frame[23]=i;
    esp_err_t result=b->base.transmit(&b->base,frame,sizeof(frame));
    if(result==ESP_OK)accepted++;else refused++;
  }
  ESP_LOGI("rtl.queue.test","QUEUE_BACKPRESSURE_TEST accepted=%u refused=%u",accepted,refused);
  return refused?ESP_OK:ESP_ERR_INVALID_STATE;
}
void realtek_native_test_hold_next_tx() {if(active_bulk)active_bulk->hold_next_tx.store(true);}
unsigned realtek_native_test_ndp_mask(bool reset) {
  if(!active_bulk)return 0;
  return reset?active_bulk->test_ndp_rx_mask.exchange(0):active_bulk->test_ndp_rx_mask.load();
}
#endif
#ifdef USE_RTL_BOOT_TALLY_PROBE
void realtek_native_boot_tally_record(unsigned slot,const char *stage) {
  auto *b=active_bulk;if(!b||slot>=8||!b->boot_tally[slot].empty())return;
  auto port=b->port.load();RealtekControl control;
  esp_err_t error=control.open(port);if(error!=ESP_OK)return;
  uint8_t phy=0;error=control.phy_status(phy);
  auto tally=control.counters();char prefix[100];
  snprintf(prefix,sizeof(prefix),"%s ms=%u phy=%02X phy_read=%d usb_tx=%u ",stage,unsigned(esphome::millis()),phy,int(error),b->tx_urbs.load());
  ESP_LOGI("rtl.boot.tally","%s%s",prefix,tally.c_str());
  if(tally.rfind("error:",0)==0){b->boot_tally[slot]=std::string(prefix)+tally;return;}
  auto value=[&tally](const char *key)->unsigned long long {
    auto at=tally.find(std::string(key)+"=");return at==std::string::npos?0:strtoull(tally.c_str()+at+strlen(key)+1,nullptr,10);
  };
  char compact[120];snprintf(compact,sizeof(compact),"tx=%llu txerr=%llu abort=%llu under=%llu",value("tx"),value("txerr"),value("aborted"),value("underrun"));
  b->boot_tally[slot]=std::string(prefix)+compact;
}
std::string realtek_native_boot_tally(unsigned slot) {
  return active_bulk&&slot<8?active_bulk->boot_tally[slot]:"unavailable";
}
#endif
esp_err_t realtek_native_device(usbh_cdc_port_handle_t port,usb_device_handle_t *device) {
  if(!active_bulk || !port || port!=active_bulk->port.load() || !device)return ESP_ERR_INVALID_STATE;
  *device=active_bulk->monitored.load();return *device?ESP_OK:ESP_ERR_INVALID_STATE;
}
// A dedicated client/transfer stays open across suspend/resume.
// Management is main-loop-only, so there is exactly one control request at a time.
static esp_err_t native_device_request(Bulk *b,usb_device_handle_t device,uint8_t direction,uint8_t request,uint16_t value,uint16_t index,uint16_t length,uint8_t *data) {
  if(!b || !b->control || !device)return ESP_ERR_INVALID_STATE;
  if(length>128 || (length && !data))return ESP_ERR_INVALID_ARG;
  auto lease=b->control_lease.begin();
  if(lease==RealtekTransferLease::Start::BUSY)return ESP_ERR_INVALID_STATE;
  if(lease==RealtekTransferLease::Start::RECOVERED){b->control_recovered++;ESP_LOGI("ethernet.rtl","Late control completion acknowledged; allocation reusable");}
  auto *t=b->control;
  while(xSemaphoreTake(b->control_done,0)==pdTRUE){}
  auto *setup=reinterpret_cast<usb_setup_packet_t *>(t->data_buffer);
  setup->bmRequestType=direction;setup->bRequest=request;setup->wValue=value;setup->wIndex=index;setup->wLength=length;
  if(length && !(direction&0x80))memcpy(t->data_buffer+8,data,length);
  t->device_handle=device;t->bEndpointAddress=0;t->num_bytes=8+length;t->timeout_ms=500;
  esp_err_t e=usb_host_transfer_submit_control(b->monitor,t);
  if(e!=ESP_OK){b->control_lease.complete();return e;}
  bool acknowledged=false;
#ifdef USE_RTL_FAULT_INJECTION
  if(!b->inject_control_timeout.exchange(false))
#endif
    acknowledged=xSemaphoreTake(b->control_done,pdMS_TO_TICKS(500))==pdTRUE;
  if(!acknowledged || b->control_lease.in_flight()) {
    b->control_lease.timeout();b->control_timeouts++;
    ESP_LOGE("ethernet.rtl","Control wait timed out; allocation stays quarantined until real completion");
    return ESP_ERR_TIMEOUT;
  }
  if(t->status!=USB_TRANSFER_STATUS_COMPLETED)return ESP_ERR_INVALID_RESPONSE;
  size_t actual=t->actual_num_bytes>=8?size_t(t->actual_num_bytes-8):0;
  if(length && (direction&0x80)) {
    if(actual<length && request!=6)return ESP_ERR_INVALID_RESPONSE;
    memset(data,0,length);memcpy(data,t->data_buffer+8,actual<length?actual:length);
  }
  return ESP_OK;
}
esp_err_t realtek_native_request(usbh_cdc_port_handle_t port,uint8_t direction,uint8_t request,uint16_t value,uint16_t index,uint16_t length,uint8_t *data) {
  auto *b=active_bulk;
  if(!b || !port || port!=b->port.load())return ESP_ERR_INVALID_STATE;
  return native_device_request(b,b->monitored.load(),direction,request,value,index,length,data);
}
static esp_err_t set_hub_remote(Bulk *b,bool enabled) {
  // USB1.6.0 hides hub NEW_DEV events and leaves public parent.dev_hdl NULL
  // (hub.c IDF-10023). Select exactly one measured hub from the public address list.
  uint8_t addresses[16]{};int count=0;uint8_t selected=0;unsigned matches=0;
  esp_err_t e=usb_host_device_addr_list_fill(16,addresses,&count);if(e!=ESP_OK)return e;
  for(int i=0;i<count;i++) {
    if(addresses[i]==b->address)continue;
    usb_device_handle_t candidate=nullptr;
    if(usb_host_device_open(b->monitor,addresses[i],&candidate)!=ESP_OK)continue;
    const usb_device_desc_t *d=nullptr;usb_host_get_device_descriptor(candidate,&d);
    if(d && d->idVendor==0x05e3 && d->idProduct==0x0608 && d->bcdDevice==0x8536){selected=addresses[i];matches++;}
    usb_host_device_close(b->monitor,candidate);
  }
  ESP_LOGI("ethernet.rtl.pm","Measured hub matches=%u",matches);
  // A directly attached NIC is the only enumerated device on the root port.
  // Do not infer this merely from a missing known hub: unknown/multiple-device
  // topologies must still be rejected rather than skipping upstream wake setup.
  if(matches==0 && count==1 && addresses[0]==b->address) {
    b->hub_remote=false;
    ESP_LOGI("ethernet.rtl.pm","Direct USB device: no upstream hub remote-wake required");
    return ESP_OK;
  }
  if(matches!=1)return matches?ESP_ERR_NOT_SUPPORTED:ESP_ERR_NOT_FOUND;
  usb_device_handle_t hub=nullptr;e=usb_host_device_open(b->monitor,selected,&hub);if(e!=ESP_OK)return e;
  auto configure=[&]()->esp_err_t {
    const usb_config_desc_t *config=nullptr;
    esp_err_t result=usb_host_get_active_config_descriptor(hub,&config);if(result!=ESP_OK)return result;
    ESP_LOGI("ethernet.rtl.pm","Hub 05E3:0608 config attributes=%02X",config?config->bmAttributes:0);
    if(!config || !(config->bmAttributes&0x20))return enabled?ESP_ERR_NOT_SUPPORTED:ESP_OK;
    result=native_device_request(b,hub,0x00,enabled?3:1,1,0,0,nullptr);if(result!=ESP_OK)return result;
    uint8_t status[2]{};result=native_device_request(b,hub,0x80,0,0,0,2,status);if(result!=ESP_OK)return result;
    b->hub_remote=bool(status[0]&2);
    ESP_LOGI("ethernet.rtl.pm","Hub remote-wake readback=%u",b->hub_remote);
    return b->hub_remote==enabled?ESP_OK:ESP_ERR_INVALID_RESPONSE;
  };
  e=configure();
  if(b->control_lease.in_flight()) {
    if(xQueueSend(b->retired_devices,&hub,0)!=pdTRUE)ESP_LOGE("ethernet.rtl","Pending hub retirement requires host restart");
    return e;
  }
  esp_err_t close=usb_host_device_close(b->monitor,hub);return e==ESP_OK?close:e;
}
static bool rx(const uint8_t *data,size_t size,void *arg) {
  auto *b=static_cast<Bulk *>(arg);
  if(b->reset_decoder.exchange(false))b->decoder.reset();
  if(!b->initialized.load() || b->paused.load()){b->decoder.reset();return true;}
  b->rx_urbs++;unsigned frames=0;
  auto sink=[b,&frames](const uint8_t *frame,size_t length,bool bad) {
    frames++;
    b->rx_count++;
    if(bad){b->rx_bad++;return;}
#ifdef USE_RTL_FAULT_INJECTION
    rtl_probe::frame(rtl_probe::usb_rx,frame,length);
    // Count exact test datagrams before allocating/delivering to the IP stack.
    if(length>=70 && frame[12]==0x86 && frame[13]==0xdd && frame[20]==17 &&
       frame[56]==0x0d && frame[57]==0x06 && memcmp(frame+62,"NDPT",4)==0) {
      uint32_t seq=(uint32_t(frame[66])<<24)|(uint32_t(frame[67])<<16)|(uint32_t(frame[68])<<8)|frame[69];
      if(seq<8)b->test_ndp_rx_mask.fetch_or(1u<<seq);
    }
#endif
    auto *owned=static_cast<uint8_t *>(malloc(length));if(!owned){b->rx_drop++;return;}
    memcpy(owned,frame,length);
    esp_err_t delivered=ESP_OK;
    if(b->mediator->stack_input_info)delivered=b->mediator->stack_input_info(b->mediator,owned,length,nullptr);
    else if(b->mediator->stack_input)delivered=b->mediator->stack_input(b->mediator,owned,length);
    else {free(owned);return;}
    // esp_netif consumes/frees the buffer even on rejection: never free twice.
    if(delivered!=ESP_OK)b->rx_stack_drop++;
  };
  bool valid=b->rx_aggregation ? b->decoder.feed(data,size,size<4096,sink) : rtl_frames::decode(data,size,sink);
  if(frames>1)b->rx_multi++;
  if(!valid){b->rx_invalid++;ESP_LOGW("ethernet.rtl","Invalid native RX record, bytes=%u",unsigned(size));}
  return true;
}
static void device_event(const cdc_acm_host_dev_event_data_t *event,void *arg) {
  auto *b=static_cast<Bulk *>(arg);
  if(event->type==CDC_ACM_HOST_DEVICE_DISCONNECTED) {
    b->suspend_pending=false;
    b->port.store(nullptr);b->initialized=false;b->linked=false;
    b->tx_generation.fetch_add(1);
    if(b->paused.exchange(false))b->resume_ready.store(true);
    b->disconnected.store(event->data.cdc_hdl);
    iot_eth_link_t down=IOT_ETH_LINK_DOWN;b->mediator->on_stage_changed(b->mediator,IOT_ETH_STAGE_LINK,&down);
  } else if(event->type==CDC_ACM_HOST_DEVICE_RESUMED) {
    b->data_resumed.store(true);ESP_LOGI("ethernet.rtl.pm","Data polling RESUMED");
  } else if(event->type==CDC_ACM_HOST_DEVICE_SUSPENDED) {
    ESP_LOGI("ethernet.rtl.pm","Data polling SUSPENDED");
  } else if(event->type==CDC_ACM_HOST_ERROR) {
    ESP_LOGW("ethernet.rtl","Vendor bulk transfer error %d",event->data.error);
  }
}
static void monitor_event(const usb_host_client_event_msg_t *event,void *arg) {
  auto *b=static_cast<Bulk *>(arg);
  if(event->event==USB_HOST_CLIENT_EVENT_NEW_DEV) {
    usb_device_handle_t device=nullptr;
    if(usb_host_device_open(b->monitor,event->new_dev.address,&device)!=ESP_OK)return;
    const usb_device_desc_t *d=nullptr;usb_host_get_device_descriptor(device,&d);
    if(d && d->idVendor==0x0bda && d->idProduct==0x8152 && d->bcdDevice==0x2000) {
      const usb_config_desc_t *config=nullptr;usb_host_get_active_config_descriptor(device,&config);
      if(config && config->bConfigurationValue==1){b->address=event->new_dev.address;b->monitored.store(device);}
      else usb_host_device_close(b->monitor,device);
    }
    else usb_host_device_close(b->monitor,device);
  } else if(event->event==USB_HOST_CLIENT_EVENT_DEV_GONE) {
    auto device=event->dev_gone.dev_hdl;
    if(device==b->monitored.load())b->monitored.store(nullptr);
    if(xQueueSend(b->retired_devices,&device,0)!=pdTRUE)
      ESP_LOGE("ethernet.rtl","Retired device queue full; host restart required");
  } else if(event->event==USB_HOST_CLIENT_EVENT_DEV_SUSPENDED && event->dev_suspend_resume.dev_hdl==b->monitored.load()) {
    ESP_LOGI("ethernet.rtl.pm","Device SUSPENDED");
  } else if(event->event==USB_HOST_CLIENT_EVENT_DEV_RESUMED && event->dev_suspend_resume.dev_hdl==b->monitored.load()) {
    b->resumed_event.store(true);b->reset_decoder.store(true);ESP_LOGI("ethernet.rtl.pm","Device RESUMED");
  }
}
static void monitor_task(void *arg) {
  auto *b=static_cast<Bulk *>(arg);
  for(;;) {
    usb_host_client_handle_events(b->monitor,pdMS_TO_TICKS(50));
  }
}
static esp_err_t open_data_port(Bulk *b) {
  cdc_acm_host_open_config_t config{};config.vid=0x0bda;config.pid=0x8152;
  config.interface_idx=0;config.dev_addr=b->address;config.connection_timeout_ms=100;
  config.in_buffer_size=b->rx_aggregation?4096:2048;config.out_buffer_size=8192;config.data_cb=rx;config.event_cb=device_event;config.user_arg=b;
  cdc_acm_dev_hdl_t port=nullptr;esp_err_t e=cdc_acm_host_open(&config,&port);
  if(e==ESP_OK) {
    // CDC owns interface lifetime and IN polling. OUT is submitted exclusively
    // by this driver using the public Host API and the shared opened device.
    // USB 1.6.0 dispatches completion through the endpoint owner's event task.
    const usb_config_desc_t *desc=nullptr;
    auto device=b->monitored.load();
    e=device?usb_host_get_active_config_descriptor(device,&desc):ESP_ERR_INVALID_STATE;
    unsigned outputs=0;uint8_t endpoint=0;
    if(e==ESP_OK && desc) {
      const uint8_t *raw=reinterpret_cast<const uint8_t *>(desc);
      size_t total=desc->wTotalLength;bool selected=false;
      for(size_t offset=0;offset+2<=total;) {
        unsigned size=raw[offset],type=raw[offset+1];
        if(size<2 || size>total-offset){e=ESP_ERR_INVALID_RESPONSE;break;}
        if(type==4){if(size<9){e=ESP_ERR_INVALID_RESPONSE;break;}selected=raw[offset+2]==0 && raw[offset+3]==0;}
        if(type==5 && selected) {
          if(size<7){e=ESP_ERR_INVALID_RESPONSE;break;}
          if((raw[offset+3]&3)==2 && !(raw[offset+2]&0x80)){outputs++;endpoint=raw[offset+2];}
        }
        offset+=size;
      }
    }
    if(e==ESP_OK && outputs!=1)e=ESP_ERR_NOT_SUPPORTED;
    if(e==ESP_OK){b->tx_endpoint=endpoint;b->port.store(reinterpret_cast<usbh_cdc_port_handle_t>(port));}
    else cdc_acm_host_close(port);
  }
  ESP_LOGI("ethernet.rtl","Open native vendor bulk: %s",esp_err_to_name(e));return e;
}
// Never wait for USB completion in lwIP's TCP/IP thread. Its RX mailbox can
// otherwise block the USB callback that must release the TX semaphore.
static void tx_task(void *arg) {
  auto *b=static_cast<Bulk *>(arg); TxRecord *record=nullptr;
  for(;;) {
    // A soft wait keeps one URB owned by USB and the bounded queue intact.
    // No port mutex is held, so management and lifecycle remain serviceable.
    if(!finish_native_tx(b)) {
      if(xSemaphoreTake(b->tx_done,pdMS_TO_TICKS(100))!=pdTRUE)b->tx_usb_waits++;
      continue;
    }
    if(xQueueReceive(b->tx_queue,&record,portMAX_DELAY)!=pdTRUE)continue;
#ifdef USE_RTL_FAULT_INJECTION
    if(b->queue_pressure_hold.exchange(false)) {
      ESP_LOGI("rtl.queue.test","One-second sender hold for bounded admission test");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // Force a real queued record to span suspend/resume, outside the USB lock.
    if(b->hold_next_tx.exchange(false))vTaskDelay(pdMS_TO_TICKS(8000));
#endif
    if(xSemaphoreTake(b->port_mutex,pdMS_TO_TICKS(200))==pdTRUE) {
      auto port=b->port.load();
      if(record->generation!=b->tx_generation.load()) {
        b->tx_discard++;b->tx_stale++;
      } else if(port && b->initialized.load() && b->linked.load() && !b->paused.load()) {
        const uint8_t *data=record->data;size_t bytes=record->count;unsigned frames=1;
        bool segmented=false;
        if(b->tso.load()) {
          rtl_frames::TsoBuilder tso(b->tx_buffer,sizeof(b->tx_buffer));
          if(tso.begin(record->data,record->count)) {
            TxRecord *next=nullptr;
            while(xQueuePeek(b->tx_queue,&next,0)==pdTRUE && next->generation==record->generation && tso.append(next->data,next->count)) {
              if(xQueueReceive(b->tx_queue,&next,0)!=pdTRUE)break;
              free(next);
            }
            size_t combined=tso.finish();
            if(combined) {
              segmented=true;bytes=combined;data=b->tx_buffer;frames=tso.segments();
              if(rtl_frames::le32(data)&(1u<<27))b->tso6++;else b->tso4++;
              b->tso_segments+=frames;
            }
          }
        }
        if(b->tx_aggregation.load() && !segmented) {
          bytes=rtl_frames::append_tx_record(b->tx_buffer,sizeof(b->tx_buffer),0,record->data,record->count);
          TxRecord *next=nullptr;
          while(bytes && xQueuePeek(b->tx_queue,&next,0)==pdTRUE) {
            if(next->generation!=record->generation)break;
            size_t combined=rtl_frames::append_tx_record(b->tx_buffer,sizeof(b->tx_buffer),bytes,next->data,next->count);
            if(!combined)break;
            if(xQueueReceive(b->tx_queue,&next,0)!=pdTRUE)break;
            bytes=combined;frames++;free(next);
          }
          data=b->tx_buffer;
        }
        // A link event can arrive while assembling a batch. Already submitted
        // USB transfers retain their normal completion/ownership semantics.
        bool stale=record->generation!=b->tx_generation.load();
        bool ready=b->initialized.load() && b->linked.load() && !b->paused.load();
        esp_err_t e=ESP_OK;
        if(stale || !ready) {
          b->tx_discard+=frames;if(stale)b->tx_stale+=frames;
        } else {
          b->tx_urbs++;if(frames>1)b->tx_multi++;
          e=submit_native_tx(b,data,bytes,frames,record->generation,segmented);
        }
        if(e!=ESP_OK){b->tx_errors++;ESP_LOGW("ethernet.rtl","TX failed: %s",esp_err_to_name(e));}
      } else b->tx_discard++;
      xSemaphoreGive(b->port_mutex);
    } else b->tx_discard++;
    free(record);
  }
}
esp_err_t new_realtek_bulk(iot_eth_driver_t **out) {
  auto *b=new Bulk;
  b->retired_devices=xQueueCreate(8,sizeof(usb_device_handle_t));
  b->tx_queue=xQueueCreate(TX_QUEUE_CAPACITY,sizeof(TxRecord *));b->port_mutex=xSemaphoreCreateMutex();
  if(!b->tx_queue || !b->port_mutex || !b->retired_devices){if(b->retired_devices)vQueueDelete(b->retired_devices);if(b->tx_queue)vQueueDelete(b->tx_queue);if(b->port_mutex)vSemaphoreDelete(b->port_mutex);delete b;return ESP_ERR_NO_MEM;}
  if(xTaskCreate(tx_task,"rtl_bulk_tx",4096,b,4,nullptr)!=pdPASS){vQueueDelete(b->retired_devices);vQueueDelete(b->tx_queue);vSemaphoreDelete(b->port_mutex);delete b;return ESP_ERR_NO_MEM;}
  b->base.name="RTL8152 native";
  b->base.set_mediator=[](iot_eth_driver_t *d,iot_eth_mediator_t *m){self(d)->mediator=m;return ESP_OK;};
  b->base.init=[](iot_eth_driver_t *d){
    auto *b=self(d);usb_host_client_config_t config{};
    config.max_num_event_msg=8;config.async.client_event_callback=monitor_event;config.async.callback_arg=b;
    esp_err_t start=usb_host_client_register(&config,&b->monitor);if(start!=ESP_OK)return start;
    if(usb_host_transfer_alloc(136,0,&b->control)!=ESP_OK)return ESP_ERR_NO_MEM;
    if(usb_host_transfer_alloc(8192,0,&b->tx_transfer)!=ESP_OK)return ESP_ERR_NO_MEM;
    b->tx_done=xSemaphoreCreateBinary();if(!b->tx_done)return ESP_ERR_NO_MEM;
    b->tx_transfer->callback=native_tx_done;b->tx_transfer->context=b;
    b->control_done=xSemaphoreCreateBinary();if(!b->control_done)return ESP_ERR_NO_MEM;
    b->control->callback=control_done;b->control->context=b;active_bulk=b;
    if(xTaskCreate(monitor_task,"rtl_pm_observer",4096,b,5,nullptr)!=pdPASS)return ESP_ERR_NO_MEM;
    cdc_acm_host_driver_config_t cdc{};cdc.driver_task_stack_size=4096;cdc.driver_task_priority=5;cdc.xCoreID=0;
    esp_err_t e=cdc_acm_host_install(&cdc);
    if(e==ESP_OK)b->mediator->on_stage_changed(b->mediator,IOT_ETH_STAGE_LL_INIT,nullptr);
    return e;
  };
  b->base.deinit=[](iot_eth_driver_t *){return ESP_ERR_NOT_SUPPORTED;};
  b->base.get_addr=[](iot_eth_driver_t *d,uint8_t *mac){memcpy(mac,self(d)->mac,6);return ESP_OK;};
  b->base.transmit=[](iot_eth_driver_t *d,uint8_t *data,size_t size){
    auto *b=self(d);
    uint32_t generation=b->tx_generation.load();
    if(!b->port.load() || !b->initialized.load() || !b->linked.load() || b->paused.load())return ESP_ERR_INVALID_STATE;
    if(b->tcp_tracker.repeated(data,size))b->tcp_repeat++;
    auto *record=static_cast<TxRecord *>(malloc(sizeof(TxRecord)));if(!record){b->tx_drop++;b->tx_alloc_fail++;return ESP_ERR_NO_MEM;}
    record->generation=generation;
    record->count=rtl_frames::encode(record->data,sizeof(record->data),data,size);
    if(!record->count){free(record);return ESP_ERR_INVALID_SIZE;}
    bool hardware=rtl_frames::checksum_offload(record->data,record->count,b->tx_mode.load());
    // Called from task context by the serialized network output path. A full
    // queue applies bounded backpressure, yielding to the USB sender instead
    // of immediately dropping a transient TCP/UDP burst. Never wait forever.
    if(xQueueSend(b->tx_queue,&record,0)!=pdTRUE) {
      b->tx_waited++;b->tx_peak.store(TX_QUEUE_CAPACITY);
      if(xQueueSend(b->tx_queue,&record,tx_queue_wait_ticks())!=pdTRUE) {
        b->tx_drop++;b->tx_wait_timeout++;free(record);return ESP_ERR_NO_MEM;
      }
    }
    unsigned observed=uxQueueMessagesWaiting(b->tx_queue),peak=b->tx_peak.load();
    while(observed>peak&&!b->tx_peak.compare_exchange_weak(peak,observed)){}

    if(hardware)b->tx_hw++;else b->tx_sw++;
    return ESP_OK;
  };
  *out=&b->base;return ESP_OK;
}
void realtek_bulk_tx_mode(iot_eth_driver_t *d,unsigned mode) {if(mode<=2)self(d)->tx_mode.store(mode);}
void realtek_bulk_tx_aggregation(iot_eth_driver_t *d,bool tx) {self(d)->tx_aggregation.store(tx);}
void realtek_bulk_aggregation(iot_eth_driver_t *d,bool tx,bool rx) {self(d)->tx_aggregation=tx;self(d)->rx_aggregation=rx;}
void realtek_bulk_tso(iot_eth_driver_t *d,bool enabled) {self(d)->tso.store(enabled);}
std::string realtek_bulk_statistics(iot_eth_driver_t *d) {
  auto *b=self(d);char out[250];snprintf(out,sizeof(out),"mode=%u tx_hw=%u tx_sw=%u rx=%u rx_bad=%u invalid=%u tx_urbs=%u tx_multi=%u rx_urbs=%u rx_multi=%u tso4=%u tso6=%u tso_segments=%u tcp_repeat=%u",b->tx_mode.load(),b->tx_hw.load(),b->tx_sw.load(),b->rx_count.load(),b->rx_bad.load(),b->rx_invalid.load(),b->tx_urbs.load(),b->tx_multi.load(),b->rx_urbs.load(),b->rx_multi.load(),b->tso4.load(),b->tso6.load(),b->tso_segments.load(),b->tcp_repeat.load());return out;
}
std::string realtek_bulk_queue_statistics(iot_eth_driver_t *d) {
  auto *b=self(d);char out[300];
  snprintf(out,sizeof(out),"tx_aggregation=%u rx_aggregation=%u tso_enabled=%u capacity=%u pending=%u peak_sampled=%u waited=%u wait_timeout=%u alloc_fail=%u wait_budget_ticks=%u tick_hz=%u usb_pending=%u usb_waits=%u",
    unsigned(b->tx_aggregation.load()),unsigned(b->rx_aggregation),unsigned(b->tso.load()),TX_QUEUE_CAPACITY,unsigned(uxQueueMessagesWaiting(b->tx_queue)),b->tx_peak.load(),b->tx_waited.load(),b->tx_wait_timeout.load(),b->tx_alloc_fail.load(),unsigned(tx_queue_wait_ticks()),unsigned(configTICK_RATE_HZ),unsigned(b->tx_lease.in_flight()),b->tx_usb_waits.load());
  return out;
}
std::string realtek_bulk_errors(iot_eth_driver_t *d) {
  auto *b=self(d);char out[220];snprintf(out,sizeof(out),"tx_queue_drop=%u tx_usb_error=%u rx_alloc_drop=%u rx_stack_drop=%u control_timeout=%u control_recovered=%u tx_discard=%u tx_stale=%u",b->tx_drop.load(),b->tx_errors.load(),b->rx_drop.load(),b->rx_stack_drop.load(),b->control_timeouts.load(),b->control_recovered.load(),b->tx_discard.load(),b->tx_stale.load());return out;
}
esp_err_t realtek_bulk_resume(iot_eth_driver_t *driver,bool timer) {
  auto *b=self(driver);if(!b->paused.load())return ESP_ERR_INVALID_STATE;
  if(b->suspend_pending) {
    b->suspend_pending=false;b->paused.store(false);b->resume_ready.store(true);
    b->last_resume="drain-cancel";b->last_suspended_ms=0;return ESP_OK;
  }
  b->force_reason=timer?"timer":"manual";b->forced=true;
  esp_err_t e=usb_host_lib_root_port_resume();if(e!=ESP_OK)b->forced=false;
  return e;
}
esp_err_t realtek_bulk_suspend(iot_eth_driver_t *driver,uint32_t max_ms,bool remote) {
  auto *b=self(driver);auto port=b->port.load();
  if(!port || !b->monitored.load() || b->paused.load())return ESP_ERR_INVALID_STATE;
  if((max_ms && max_ms<1000) || max_ms>86400000 || (!max_ms && !remote))return ESP_ERR_INVALID_ARG;
  // Standard DEVICE_REMOTE_WAKEUP feature is independent from NIC wake filters.
  esp_err_t e=realtek_native_request(port,0x00,remote?3:1,1,0,0,nullptr);
  if(e!=ESP_OK)return e;
  uint8_t status[2]{};e=realtek_native_request(port,0x80,0,0,0,2,status);if(e!=ESP_OK)return e;
  b->nic_remote=bool(status[0]&2);if(b->nic_remote!=remote)return ESP_ERR_INVALID_RESPONSE;
  e=set_hub_remote(b,remote);if(e!=ESP_OK)return e;
  b->paused.store(true);b->resumed_event.store(false);b->resume_ready.store(false);
  b->tx_generation.fetch_add(1);
  b->data_resumed.store(false);
  // Keep the interface and transfers: CDC-ACM 2.4.1 handles flush/resubmit events.
  if(xSemaphoreTake(b->port_mutex,pdMS_TO_TICKS(500))!=pdTRUE){b->paused.store(false);return ESP_ERR_TIMEOUT;}
  xSemaphoreGive(b->port_mutex);b->linked.store(false);
  iot_eth_link_t down=IOT_ETH_LINK_DOWN;b->mediator->on_stage_changed(b->mediator,IOT_ETH_STAGE_LINK,&down);
  b->maximum_ms=max_ms;b->suspend_requested=esphome::millis();b->suspend_entered=0;
  b->forced=false;b->seen_suspended=false;
  b->suspend_pending=true;
  ESP_LOGI("ethernet.rtl.pm","Suspend queued; draining native OUT before root suspend remote=%u timeout=%u",remote,unsigned(max_ms));
  return ESP_OK;
}
bool realtek_bulk_reinitialized(iot_eth_driver_t *d) {return self(d)->reinitialized.exchange(false);}
bool realtek_bulk_resume_ready(iot_eth_driver_t *d) {return self(d)->resume_ready.exchange(false);}
std::string realtek_bulk_pm_status(iot_eth_driver_t *d) {
  auto *b=self(d);char out[260];snprintf(out,sizeof(out),"paused=%u suspended_count=%u resumed_count=%u suspended_ms=%u reason=%s hub_remote=%u nic_remote=%u max_ms=%u unsampled_resumes=%u draining=%u",b->paused.load(),b->suspend_count,b->resume_count,unsigned(b->last_suspended_ms),b->last_resume.c_str(),b->hub_remote,b->nic_remote,unsigned(b->maximum_ms),b->unsampled_resumes,b->suspend_pending.load());return out;
}
esp_err_t realtek_bulk_quiesce_phy(iot_eth_driver_t *driver) {
  auto *b=self(driver);if(!b->port.load()||!b->initialized.load()||b->paused.load())return ESP_ERR_INVALID_STATE;
  b->linked.store(false);b->tx_generation.fetch_add(1);
  iot_eth_link_t state=IOT_ETH_LINK_DOWN;
  b->mediator->on_stage_changed(b->mediator,IOT_ETH_STAGE_LINK,&state);
  // No new TX can start; wait for any already submitted transfer to finish
  // before PHY restart. Never restart underneath an active USB transmission.
  if(xSemaphoreTake(b->port_mutex,pdMS_TO_TICKS(200))!=pdTRUE)return ESP_ERR_TIMEOUT;
  xSemaphoreGive(b->port_mutex);
  uint32_t until=esphome::millis()+200;
  while(b->tx_lease.in_flight() && int32_t(esphome::millis()-until)<0)vTaskDelay(1);
  if(b->tx_lease.in_flight())return ESP_ERR_TIMEOUT;
  b->phy_hold_until=esphome::millis()+250;
  return ESP_OK;
}
void realtek_bulk_poll(iot_eth_driver_t *driver) {
  auto *b=self(driver);
  if(esphome::millis()-b->last_poll<250)return;
  b->last_poll=esphome::millis();
  usb_device_handle_t retired=nullptr;
  if(!b->control_lease.in_flight() && !b->tx_lease.in_flight() && xQueuePeek(b->retired_devices,&retired,0)==pdTRUE) {
    if(usb_host_device_close(b->monitor,retired)==ESP_OK)xQueueReceive(b->retired_devices,&retired,0);
  }
  if(b->paused.load()) {
    if(b->suspend_pending) {
      auto decision=rtl_tx_drain::decide(b->tx_lease.in_flight(),esphome::millis(),b->suspend_requested);
      if(decision==rtl_tx_drain::Decision::WAIT)return;
      if(decision==rtl_tx_drain::Decision::EXPIRED) {
        b->suspend_pending=false;b->paused.store(false);b->resume_ready.store(true);
        b->last_resume="drain-timeout";b->last_suspended_ms=0;
        ESP_LOGW("ethernet.rtl.pm","Suspend refused: native OUT still pending after 10s; buffer retained");
        return;
      }
      // Admission has been paused and the port lock fenced any old submission.
      // The last complete native record has reached USB before root flushes.
      b->suspend_pending=false;b->suspend_requested=esphome::millis();
      esp_err_t e=usb_host_lib_root_port_suspend();
      ESP_LOGI("ethernet.rtl.pm","OUT drained; root suspend request: %s",esp_err_to_name(e));
      if(e!=ESP_OK){b->last_resume="suspend-error";b->paused.store(false);b->resume_ready.store(true);}
      return;
    }
    usb_host_lib_info_t info{};usb_host_lib_info(&info);
    if(info.root_port_suspended && !b->seen_suspended) {
      b->seen_suspended=true;b->suspend_entered=esphome::millis();b->suspend_count++;
      ESP_LOGI("ethernet.rtl.pm","Root port actually suspended");
    }
    if(b->maximum_ms && !b->forced && esphome::millis()-b->suspend_requested>=b->maximum_ms) {
      esp_err_t e=realtek_bulk_resume(driver,true);
      ESP_LOGI("ethernet.rtl.pm","Timed resume request: %s",esp_err_to_name(e));
    }
    if(b->resumed_event.load() && b->data_resumed.load()) {
      b->resumed_event.store(false);
      auto observation=rtl_pm::resume_observation(b->seen_suspended,b->forced,b->force_reason=="manual",b->suspend_entered,esphome::millis());
      b->last_suspended_ms=observation.milliseconds;b->last_resume=observation.reason;
      if(observation.root_observed)b->resume_count++;else b->unsampled_resumes++;
      if(!observation.root_observed)ESP_LOGI("ethernet.rtl.pm","Device resume before root suspend sample; no sleep duration measured");
      b->paused.store(false);b->resume_ready.store(true);
      ESP_LOGI("ethernet.rtl.pm","Resume with retained data interface");
    }
    return;
  }
  auto gone=b->disconnected.exchange(nullptr);
  if(gone) {
    if(b->tx_lease.in_flight()){b->disconnected.store(gone);return;}
    if(xSemaphoreTake(b->port_mutex,pdMS_TO_TICKS(200))==pdTRUE){
      // Submission may have started after the optimistic check above.
      if(b->tx_lease.in_flight())b->disconnected.store(gone);
      else cdc_acm_host_close(gone);
      xSemaphoreGive(b->port_mutex);
    }
    else b->disconnected.store(gone);
    return;
  }
  if(!b->port.load() && b->monitored.load()){open_data_port(b);return;}
  auto port=b->port.load();if(!port)return;
  RealtekControl control;
  if(control.open(port)!=ESP_OK)return;
  if(!b->initialized) {
    uint8_t descriptor[64]{};
    // iMAC=3 was measured in the alternate ECM function descriptor of this exact tuple.
    esp_err_t error=realtek_native_request(port,0x80,6,0x0303,0x0409,64,descriptor);
    if(error!=ESP_OK||descriptor[0]<26)return;
    for(unsigned i=0;i<6;i++) {
      char digits[3]={char(descriptor[2+i*4]),char(descriptor[4+i*4]),0};char *end=nullptr;
      unsigned long v=strtoul(digits,&end,16);if(end!=digits+2)return;b->mac[i]=v;
    }
#ifdef USE_RTL_BOOT_TALLY_PROBE
    for(auto &sample:b->boot_tally)sample.clear();
    realtek_native_boot_tally_record(0,"before_native");
#endif
    b->reset_decoder.store(true);error=control.initialize_native(b->rx_aggregation);
#ifdef USE_RTL_BOOT_TALLY_PROBE
    realtek_native_boot_tally_record(1,"after_native");
    b->probe_initialized_at=esphome::millis();
#endif
    ESP_LOGI("ethernet.rtl","Native initialization: %s",esp_err_to_name(error));
    if(error!=ESP_OK)return;
    b->initialized=true;b->reinitialized.store(true);
  }
#ifdef USE_RTL_BOOT_TALLY_PROBE
  if(b->boot_tally[4].empty()&&b->tx_urbs.load())realtek_native_boot_tally_record(4,"after_first_usb_tx");
  const uint32_t probe_ms[3]={3000,10000,30000};
  for(unsigned i=0;i<3;i++)if(b->boot_tally[5+i].empty()&&esphome::millis()-b->probe_initialized_at>=probe_ms[i])realtek_native_boot_tally_record(5+i,"settled");
#endif
  if(b->phy_hold_until) {
    if(int32_t(esphome::millis()-b->phy_hold_until)<0)return;
    b->phy_hold_until=0;
  }
  uint8_t phy=0;if(control.phy_status(phy)!=ESP_OK)return;
  bool up=phy&2;
  if(up!=b->linked) {
    b->linked=up;if(!up)b->tx_generation.fetch_add(1);
    iot_eth_link_t state=up?IOT_ETH_LINK_UP:IOT_ETH_LINK_DOWN;
    b->mediator->on_stage_changed(b->mediator,IOT_ETH_STAGE_LINK,&state);
  }
}
usbh_cdc_port_handle_t realtek_bulk_port(iot_eth_driver_t *driver) {return self(driver)->port.load();}
}

#endif  // USE_ETHERNET_REALTEK_BULK
