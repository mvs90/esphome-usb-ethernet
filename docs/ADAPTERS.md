# Adapter compatibility

| Tested adapter | Measured identity | USB topology | Configuration |
| --- | --- | --- | --- |
| InLine33380J POE01 | 0bda:8152 / bcd2000 / masked RTL revision4C10 | Internal hub | examples/inline.yaml |
| P2C5V unit referenced by ASIN B09M2N3NHX | 0bda:8152 / bcd2000 / masked RTL revision4C10 | Single directly attached NIC | examples/p2c5v.yaml |

Both measured adapters expose vendor bulk and a separate ECM configuration. Native mode is `USB_RTL8152`; `USB_ECM` is an alternative transport, not a route to Realtek-native offload. The driver reads the adapter MAC per device. Product names do not select chip revision, USB configuration, or hub quirks.

Tested host: Waveshare ESP32-S3-LCD-Driver-Board, 8 MB flash, no display/battery. Other boards, including Touch-LCD-4 Rev.4.0, need their own validation. No native USB-host support for ESP32-C6 Super Mini is provided.

P2C5V passed clean v101 OTA/boot/API/Home Assistant, three true PoE cold starts and final error observation. Its long test was accepted by the operator with recorded external Mac-link interruptions excluded; no uninterrupted loss-free P2C5V hour is confirmed. See [TEST_STATUS.md](../TEST_STATUS.md) and [setup](SETUP.md).
