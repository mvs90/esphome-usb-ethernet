# Browser-only setup: paste YAML, compile, then use ESPHome Web

[Full step-by-step German guide, including the complete pasteable YAML](WEB_EDITOR.de.md).

No Git clone, manually copied component files, terminal, `!include`, or `secrets.yaml` is needed. The Builder fetches the pinned external components itself. The one downloaded file is your compiled factory image, selected in the browser flasher.

1. Open **ESPHome Device Builder2026.9.1**, create an **ESP32-S3** node, skip immediate installation, then choose **Edit**. Keep its generated API key if present.
2. Replace the new node's YAML with the complete block below. Set your unique node name, own API key and own OTA password under `substitutions`. A random browser-generated API key is available on the official [Native API page](https://esphome.io/components/api/#configuration-variables). No shared test key is supplied.
3. Save, validate, then select **Install → Manual download → Factory format**. Compile and download your factory BIN, including bootloader/partition table; do not choose an OTA-only image.
4. Unplug the PoE adapter from the board. Connect the S3 board directly to the computer with a USB data cable. Enter BOOT/download mode if necessary.
5. Open **[web.esphome.io](https://web.esphome.io/)** in a desktop WebSerial browser such as Chrome/Edge. **Connect** to the S3 board's port, then **Install** your downloaded factory BIN. Do not install generic preparation firmware. No additional UART adapter is required.
6. After successful installation disconnect the browser port, unplug the computer USB cable, then connect **PoE source → RJ45 adapter → adapter USB-C → S3 board**.
7. Find the DHCP IP and add the existing ESPHome device to Home Assistant with your own API key. Future Builder **network/Wirelessly/OTA** uploads travel over Ethernet, not Wi-Fi; use the device IP if mDNS is unavailable.

These board/flash/UART settings describe the tested Waveshare ESP32-S3-LCD-Driver-Board with8MB flash. They do not qualify other boards. The tested overlay remains pinned to ESPHome2026.9.1 / IDF5.5.5. Component runtime and known test limitations are unchanged.

```yaml
# Paste this complete file into the ESPHome Device Builder web editor.
# Replace all three values below. No secrets.yaml or local component copies needed.
substitutions:
  device_name: "usb-poe-web"
  api_key: "PASTE_YOUR_API_KEY_HERE"
  ota_password: "CHOOSE_YOUR_OWN_OTA_PASSWORD"

esphome:
  name: ${device_name}
  min_version: 2026.9.1

# Tested board: Waveshare ESP32-S3-LCD-Driver-Board, 8 MB flash.
esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  flash_size: 8MB
  toolchain: esp-idf
  framework:
    type: esp-idf
    version: 5.5.5
    sdkconfig_options:
      CONFIG_ESP_CONSOLE_UART_CUSTOM: "y"
      CONFIG_ESP_CONSOLE_UART_CUSTOM_NUM_0: "y"
      CONFIG_ESP_CONSOLE_UART_TX_GPIO: "43"
      CONFIG_ESP_CONSOLE_UART_RX_GPIO: "44"
      CONFIG_ESP_CONSOLE_UART_BAUDRATE: "115200"
      CONFIG_ESP_CONSOLE_SECONDARY_NONE: "y"
      CONFIG_USJ_ENABLE_USB_SERIAL_JTAG: "n"

external_components:
  - source: github://mvs90/esphome-usb-ethernet@v0.1.0-rc.5
    components: [ethernet, ethernet_info, logger]

ethernet:
  id: usb_ethernet
  type: USB_RTL8152
  realtek:
    speed: AUTO
    eee: true
    tx_checksum_offload: true
    tx_aggregation: true
    rx_aggregation: true
    tcp_segmentation: true
    wake_filters: 0

network:
  enable_ipv6: true
  min_ipv6_addr_count: 1

# The native USB PHY is occupied by the Ethernet host; log via UART0.
logger:
  hardware_uart: UART0
  baud_rate: 115200
  level: INFO

api:
  reboot_timeout: 0s
  encryption:
    key: "${api_key}"

ota:
  - platform: esphome
    password: "${ota_password}"

text_sensor:
  - platform: ethernet_info
    ip_address:
      name: Ethernet IP
      address_1:
        name: Ethernet IPv6 Link Local
      address_2:
        name: Ethernet IPv6 Global
    mac_address:
      name: Ethernet MAC
```

The `web-editor.yaml` file is also available under `examples` for viewing/copying its text; downloading it is optional. Key/password placeholders must be replaced before building. The secrets remain in your private Builder YAML, not on GitHub. ESPHome Web flashes the compiled image; it does not compile YAML.
