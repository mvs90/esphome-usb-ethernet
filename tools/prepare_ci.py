#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare a build-only configuration with synthetic keys; never flash it."""
import argparse,base64
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('example',type=Path);p.add_argument('output',type=Path);p.add_argument('--ethernet-path',type=Path);p.add_argument('--lldp-path',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1];text=a.example.read_text()
for name,tag,path in [('esphome-usb-ethernet','v0.1.0-rc.5',a.ethernet_path),('esphome-lldp','v0.1.0-rc.1',a.lldp_path)]:
 component='ethernet' if name=='esphome-usb-ethernet' else 'lldp'
 if path is None and (root/'components'/component).exists():path=root/'components'
 if path is not None:
  text=text.replace('- source: github://mvs90/'+name+'@'+tag,'- source:\n      type: local\n      path: '+str(path.resolve()))
text=text.replace('PASTE_YOUR_API_KEY_HERE',base64.b64encode(bytes(range(32))).decode()).replace('CHOOSE_YOUR_OWN_OTA_PASSWORD','synthetic-ci-only-never-flash')
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text)
(a.output.parent/'secrets.yaml').write_text('api_encryption_key: '+base64.b64encode(bytes(range(32))).decode()+'\nota_password: synthetic-ci-only-never-flash\n')
print('Prepared build-only configuration:',a.output)
