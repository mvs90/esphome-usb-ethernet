# USB Ethernet + PoE for ESPHome on ESP32-S3

**Browser-only setup:** [Paste YAML into the Device Builder and first-flash with ESPHome Web](docs/WEB_EDITOR.md), with no component files or `secrets.yaml` to copy.

[Deutsch](README.de.md) · [Setup guide](docs/SETUP.md) · [Hardware/test status](TEST_STATUS.md) · [Separate LLDP component](https://github.com/mvs90/esphome-lldp)

Use one USB-C connection for **PoE power and native ESPHome Ethernet**, including encrypted API and OTA, without Wi-Fi. This is a version-pinned external component overlay, not an official ESPHome Ethernet driver.

## Supported and tested setup

| Adapter | Measured USB/chip identity | Topology / example |
| --- | --- | --- |
| InLine **33380J POE01** | `0bda:8152`, bcdDevice `2000`, masked RTL8152 revision `4C10` | Internal hub; [inline.yaml](examples/inline.yaml) |
| **P2C5V**, tested unit referenced by ASIN B09M2N3NHX | Same measured identity | Direct USB device; [p2c5v.yaml](examples/p2c5v.yaml) |

Both use **`type: USB_RTL8152`**. Do not select a configuration or assume compatibility from a product name alone: other production batches may contain different hardware.

Tested host: **Waveshare ESP32-S3-LCD-Driver-Board, 8 MB flash**, without display/battery. This is a different board from ESP32-S3-Touch-LCD-4 Rev.4.0. ESP32-C6 Super Mini has no suitable native USB host for this setup. A generic ESP32-S3 board also needs its own USB wiring/power check.

**Pinned toolchain:** ESPHome **2026.9.1**, ESP-IDF **5.5.5**, release **v0.1.0-rc.5**. Other ESPHome versions require validation because `ethernet`, `ethernet_info`, and `logger` override built-in components. No ESPHome/IDF source fork is required; this is not a promise of compatibility with every future release.

## Quick start

1. Use ESPHome 2026.9.1. Download [inline.yaml](examples/inline.yaml) or [p2c5v.yaml](examples/p2c5v.yaml), and put your `secrets.yaml` beside it. See [secrets.example.yaml](examples/secrets.example.yaml). Generate an API key with `openssl rand -base64 32` and choose your own OTA password.
2. Set a unique `esphome.name`. The example board/8 MB/UART pins match the tested Waveshare board; adapt these only after checking your own board.
3. Validate and compile: `esphome config inline.yaml`, then `esphome compile inline.yaml` (or the P2C5V file).
4. First flash: identify the board's USB download port, disconnect the PoE adapter, connect the board to the computer, then run `esphome upload inline.yaml --device <BOARD_PORT>`. Enter the board's BOOT/download mode if necessary.
5. Remove the computer-to-board USB cable. Connect **PoE switch/injector → RJ45 adapter → adapter USB-C → ESP32-S3 board**. Do not connect the USB-C adapter to the computer at this stage.
6. Find the DHCP address. Add the node through Home Assistant's ESPHome integration, supplying your API encryption key. Future updates use `esphome upload inline.yaml --device <NODE_IP>` over Ethernet.

The adapter is the USB **device**, the ESP32-S3 is the USB **host**. Native USB logging cannot use the same PHY while Ethernet is attached. On the tested board, UART0 TX is GPIO43, RX GPIO44, 115200 baud. An optional **3.3 V logic** USB-UART receiver connects board GND → GND and TX/GPIO43 → adapter RX. Leave all UART power pins open while powered by PoE. Normal API/OTA operation does not require an external UART.

## Add it to an existing project

Merge these sections into your existing YAML; do not create a second `esp32`, `ethernet`, or USB-host configuration. Copy the UART/framework settings from the complete example if your project does not already provide them. Remove Wi-Fi and USB-serial logging for this tested configuration.

```yaml
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
```

Keep a release tag or commit SHA instead of `main`. The complete examples also configure encrypted API/OTA, UART logging, and IP/MAC diagnostic entities. They do not contain disruptive test buttons. [diagnostics.yaml](examples/diagnostics.yaml) is an optional package for Realtek/native error counters. [SETUP.md](docs/SETUP.md) explains static addressing, fixed MAC, diagnostics, and troubleshooting.

`USB_ECM` is the separate Espressif ECM path, shown in [ecm.yaml](examples/ecm.yaml). Native Realtek offload settings do not apply to ECM. All options and their limits are described in [REALTEK_NATIVE.md](docs/REALTEK_NATIVE.md) and [REALTEK_CONTROL.md](docs/REALTEK_CONTROL.md).

## LLDP is a separate repository

For neighbor information in Home Assistant, an enable switch, and editable local identity fields, use [mvs90/esphome-lldp](https://github.com/mvs90/esphome-lldp). It is optional. Its examples load this Ethernet overlay and `lldp` from separate pinned sources. LLDP currently depends on this overlay's Layer-2 API and does not support arbitrary stock ESPHome Ethernet backends.

## Qualification and limits

Native bulk, checksum offload, aggregation/TSO, IPv4/IPv6, API/OTA, PHY controls, LEDs, selective wake, and real pause-frame behavior were tested. P2C5V's clean v101 firmware passed OTA, dual-stack API/Home Assistant, three genuine PoE cold starts, and final error-counter observation.

**A full uninterrupted loss-free P2C5V data hour is not confirmed.** The operator accepted the independently recorded Mac-link interruptions as an external qualification exception after no new driver/RTL errors were found. Original failed results are preserved; a passed InLine hour is not substituted for a P2C5V hour. [TEST_STATUS.md](TEST_STATUS.md) records the scope.

Half-duplex traffic, a purely asymmetric pause partner, and independent physical EEE-LPI duration/energy measurement remain unqualified. No temperature/current register interface is established for this revision. USB Full Speed is 12 Mbit/s; a 100-Mbit Ethernet link does not imply 100-Mbit application throughput. This is a release candidate, not an unrestricted 1.0 hardware-family certification.

## Development and license

`python3 tools/run_cpp_tests.py` runs 17 Ethernet suites with Clang/ASan/UBSan. The three LLDP suites live in its repository. CI builds pinned examples with synthetic keys; **never flash CI/test images**. Protocol test fixture `.bin` files are synthetic packet data, not board firmware.

C/C++ runtime additions are GPL-3.0-only; Python additions are MIT. Preserve ESPHome-derived notices. See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md), and [CONTRIBUTING.md](CONTRIBUTING.md).

Reference: [ESPHome external-components documentation](https://esphome.io/components/external_components/).
