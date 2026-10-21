# Use in your own ESPHome project

## Configuration layout

Keep your device YAML and `secrets.yaml` together. Copy a complete example first; change the node name, then build it with ESPHome2026.9.1. Add your own sensors/display configuration only after validating the network baseline. The 8-MB flash setting and UART GPIO43/44 describe the tested Driver-Board, not every S3 board.

There must be exactly one `ethernet` block and one native USB host. Do not add `usb_host:`, Wi-Fi, or `USB_SERIAL_JTAG` logging to these examples. Their runtime shares the S3 USB PHY with the Ethernet adapter. Keep `logger.hardware_uart: UART0` and the matching board console settings. `external_components` replaces only `[ethernet, ethernet_info, logger]`; this is why the ESPHome version is pinned.

## DHCP, static IP, and MAC

DHCP is the default and uses each adapter's own MAC. For static IPv4 add the normal ESPHome `manual_ip` block under `ethernet` (values below are documentation addresses; replace them with your LAN settings):

```yaml
ethernet:
  id: usb_ethernet
  type: USB_RTL8152
  manual_ip:
    static_ip: 192.0.2.20
    gateway: 192.0.2.1
    subnet: 255.255.255.0
    dns1: 192.0.2.1
```

Merge this with the native `realtek` options from the full example. An optional locally administered unicast `mac_address: "02:00:00:00:00:20"` under `ethernet` is volatile and must be unique on your LAN. Do not reuse the sample address on multiple devices. It is not an EEPROM/OTP write.

## Optional diagnostics

Copy `examples/diagnostics.yaml` beside your device YAML and add:

```yaml
packages:
  usb_diagnostics: !include diagnostics.yaml
```

The package expects `id: usb_ethernet` and the native `USB_RTL8152` backend. It exposes `Realtek Status`, `Realtek Counters`, and `Native Errors` without adding restart/suspend test buttons. Compare counters over time; startup/disconnect drops are different from new errors during steady traffic.

## Home Assistant and OTA

After boot, connect Home Assistant's ESPHome integration to the DHCP/static IP and enter your own API encryption key. Address/MAC text sensors identify the selected adapter. No new Home Assistant instance or custom HA integration is needed. Subsequent builds upload over Ethernet with the same configuration's OTA password.

If moving between two adapters changes the DHCP address, update the existing ESPHome integration's address or use working mDNS. Do not copy a laboratory IP into your project.

## LLDP

Follow [the separate LLDP repository](https://github.com/mvs90/esphome-lldp). Load `[lldp]` from that repository and keep `[ethernet, ethernet_info, logger]` from this one. Do not declare duplicate Ethernet instances. Its full example includes editable Home Assistant identity text fields. Its runtime edits do not persist across a reboot; YAML values are the defaults.

## Troubleshooting

| Observation | Check |
| --- | --- |
| No board power | Active compatible PoE source, cable and adapter; do not power through UART while PoE is connected. |
| USB enumeration fails | S3 USB host wiring, direct USB-C connection, supported measured revision; obtain UART logs. |
| Build fails after an ESPHome upgrade | Restore the tested ESPHome2026.9.1 / IDF5.5.5 combination; the overlay is version-bound. |
| No API after first flash | Remove the computer USB cable, connect the PoE adapter to the board, verify DHCP/IPv6 and your own API key. |
| Brief UDP loss in a host test | Correlate board errors, switch mirror and host link/IP events; a host link reset is not proof of a board TX defect. |
| Boot loop or wrong board memory | Check exact board variant and flash size; Driver-Board and Touch-LCD-4 are not interchangeable. |
| Recovery needed | Unplug PoE adapter USB-C, connect board to computer in BOOT/download mode, identify the correct serial target, and flash your own compiled image. Never flash CI/synthetic-key binaries. |

Full tested scope: [TEST_STATUS.md](../TEST_STATUS.md). Advanced Realtek actions: [REALTEK_CONTROL.md](REALTEK_CONTROL.md).
