#!/usr/bin/env python3
"""Run inside the existing ESPHome container: verify meaningful configuration guards."""
from pathlib import Path
import subprocess
import argparse

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base',type=Path,default=root/('examples' if (root/'examples').exists() else 'firmware/esphome'))
base=parser.parse_args().base.resolve()
source = (base/'usb-ecm.yaml').read_text().replace('  realtek:\n    speed: AUTO\n    eee: true\n    wake_filters: 0\n', '')
cases = {
    'ipv4-only': (source.replace('enable_ipv6: true', 'enable_ipv6: false'), True, ''),
    'dhcp': (source, True, ''),
    'native-bulk': (source.replace('type: USB_ECM', 'type: USB_RTL8152'), True, ''),
    'static': (source.replace('  type: USB_ECM', '  type: USB_ECM\n  manual_ip:\n    static_ip: 192.0.2.2\n    gateway: 192.0.2.1\n    subnet: 255.255.255.0'), True, ''),
    'wifi': (source+'\nwifi:\n  ssid: test\n  password: abcdefgh12345\n', False, 'Wi-Fi'),
    'second-host': (source+'\nusb_host:\n', False, 'sole USB host'),
    'ipv6': (source.replace('enable_ipv6: false', 'enable_ipv6: true'), True, ''),
    'usb-console': (source.replace('hardware_uart: UART0', 'hardware_uart: USB_SERIAL_JTAG'), False, 'UART0'),
    'fixed-mac': (source.replace('  type: USB_ECM', '  type: USB_ECM\n  mac_address: "02:00:00:00:00:01"'), True, ''),
    'multicast-mac': (source.replace('  type: USB_ECM', '  type: USB_ECM\n  mac_address: "03:00:00:00:00:01"'), False, 'unicast'),
    'zero-mac': (source.replace('  type: USB_ECM', '  type: USB_ECM\n  mac_address: "00:00:00:00:00:00"'), False, 'unicast'),
    'realtek': (source.replace('  type: USB_ECM', '  type: USB_ECM\n  realtek:\n    speed: "100"\n    duplex: HALF\n    eee: false\n    wake_filters: 31'), True, ''),
    'gigabit': (source.replace('  type: USB_ECM', '  type: USB_ECM\n  realtek:\n    speed: "1000"'), False, 'speed'),
    'wrong-phy': (source.replace('type: USB_ECM', 'type: W5500'), False, 'USB_ECM'),
    'disabled-at-boot': (source.replace('  type: USB_ECM', '  type: USB_ECM\n  enable_on_boot: false'), False, 'enable_on_boot'),
}
action_prefix = "\n  - platform: template\n    name: Schema Actions\n    on_press:\n"
all_actions = """      - ethernet.realtek.link:
          speed: AUTO
          advertisement: 480
      - ethernet.realtek.eee:
          enabled: true
      - ethernet.realtek.wake:
          filters: 31
      - ethernet.realtek.pause:
          tx: false
          rx: true
      - ethernet.realtek.promiscuous:
          enabled: false
"""
cases['native-offload'] = (source.replace('  type: USB_ECM', '  type: USB_RTL8152\n  realtek:\n    tx_checksum_offload: true'), True, '')
cases['ecm-offload-rejected'] = (source.replace('  type: USB_ECM', '  type: USB_ECM\n  realtek:\n    tx_checksum_offload: true'), False, 'requires USB_RTL8152')
cases['leds'] = (source + action_prefix + '      - ethernet.realtek.leds:\n          led0: 3\n          led1: 8\n', True, '')
cases['bad-led'] = (source + action_prefix + '      - ethernet.realtek.leds:\n          led0: 4\n          led1: 8\n', False, 'LED bits')
cases['suspend'] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          maximum_duration: 5s\n          remote_wake: true\n', True, '')
cases['bad-suspend'] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          maximum_duration: 86401s\n', False, 'maximum_duration')
cases['indefinite-suspend'] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          maximum_duration: 0s\n          remote_wake: true\n      - ethernet.realtek.usb_resume:\n', True, '')
cases['indefinite-no-wake'] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          maximum_duration: 0s\n', False, 'requires remote_wake')
cases['short-suspend'] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          maximum_duration: 500ms\n', False, 'at least 1s')
for option in ('tx_aggregation','rx_aggregation','tcp_segmentation'):
    cases['native-'+option] = (source.replace('  type: USB_ECM', '  type: USB_RTL8152\n  realtek:\n    '+option+': true'), True, '')
    cases['ecm-'+option] = (source.replace('  type: USB_ECM', '  type: USB_ECM\n  realtek:\n    '+option+': true'), False, 'requires USB_RTL8152')
cases['tso-action'] = (source + action_prefix + '      - ethernet.realtek.tcp_segmentation:\n          enabled: true\n', True, '')
cases['ipv6-only'] = (source.replace('  type: USB_ECM','  type: USB_RTL8152\n  enable_ipv4: false'),True,'')
cases['no-ip-family'] = (source.replace('  type: USB_ECM','  type: USB_RTL8152\n  enable_ipv4: false').replace('enable_ipv6: true','enable_ipv6: false'),False,'requires network.enable_ipv6')
cases['ipv6-only-manual'] = (source.replace('  type: USB_ECM','  type: USB_RTL8152\n  enable_ipv4: false\n  manual_ip:\n    static_ip: 192.0.2.2\n    gateway: 192.0.2.1\n    subnet: 255.255.255.0'),False,'cannot be combined')
cases['actions'] = (source + action_prefix + all_actions, True, '')
cases['bad-advertisement'] = (source + action_prefix + all_actions.replace('advertisement: 480', 'advertisement: 33'), False, 'advertisement')

for mask in (0,1,2,4,8,16,31):
    cases['suspend-wake-'+str(mask)] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          remote_wake: true\n          wake_filters: '+str(mask)+'\n', True, '')
for mask in (-1,32):
    cases['bad-suspend-wake-'+str(mask)] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          remote_wake: true\n          wake_filters: '+str(mask)+'\n', False, 'wake_filters')
cases['suspend-wake-without-remote'] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          remote_wake: false\n          wake_filters: 4\n', False, 'requires remote_wake')

cases['indefinite-zero-wake'] = (source + action_prefix + '      - ethernet.realtek.usb_suspend:\n          maximum_duration: 0s\n          remote_wake: true\n          wake_filters: 0\n', False, 'at least one wake filter')
cases['multicast-all'] = (source + action_prefix + '      - ethernet.realtek.multicast:\n', True, '')
cases['multicast-filtered'] = (source + action_prefix + '      - ethernet.realtek.multicast:\n          mode: FILTERED\n          addresses: ["01:00:5E:00:00:FB", "33:33:00:00:00:FB"]\n', True, '')
cases['multicast-no-basic'] = (source + action_prefix + '      - ethernet.realtek.multicast:\n          mode: FILTERED\n          preserve_basic_groups: false\n          addresses: []\n', True, '')
cases['multicast-unicast-rejected'] = (source + action_prefix + '      - ethernet.realtek.multicast:\n          mode: FILTERED\n          addresses: ["02:00:00:00:00:01"]\n', False, 'multicast MACs')
cases['multicast-broadcast-rejected'] = (source + action_prefix + '      - ethernet.realtek.multicast:\n          mode: FILTERED\n          addresses: ["FF:FF:FF:FF:FF:FF"]\n', False, 'excluding broadcast')
cases['multicast-all-addresses-rejected'] = (source + action_prefix + '      - ethernet.realtek.multicast:\n          addresses: ["33:33:00:00:00:FB"]\n', False, 'require mode')

for name, (text, valid, expected) in cases.items():
    file = base/('schema-'+name+'.yaml')
    assert not file.exists(), 'Refusing to overwrite existing file: '+str(file)
    try:
        file.write_text(text)
        r = subprocess.run(['esphome', 'config', str(file)], capture_output=True, text=True)
        assert (r.returncode == 0) == valid, f'{name}: unexpected status {r.returncode}'
        assert valid or expected in r.stdout+r.stderr, f'{name}: wrong rejection'
        print('PASS', name, 'accepted' if valid else 'rejected')
    finally:
        file.unlink(missing_ok=True)
