#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run protocol/frame checks with sanitizers and both management transports."""
from pathlib import Path
import subprocess,tempfile,os,argparse
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--components',type=Path,default=root/'components');args=parser.parse_args();components=args.components.resolve()
with tempfile.TemporaryDirectory(prefix='rtl8152-tests-') as folder:
 for name,native in [('control-ecm',False),('control-native',True),('frames',False),('aggregation',False),('tso',False),('transfer',False),('deadline',False),('probe',False),('pm',False),('tx_drain',False),('native_usb_init_test-ecm',False),('native_usb_init_test-native',True),('register_snapshot_test-ecm',False),('register_snapshot_test-native',True),('control_multicast_test-ecm',False),('control_multicast_test-native',True),('tx_integrity',False)]:
  binary=str(Path(folder)/name)
  command=[os.environ.get('CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(components/'ethernet')]
  if name in ('frames','aggregation','tso','transfer','deadline','probe','pm','tx_drain'): command += [str(root/f'tests/test_realtek_{name}.cpp')]
  elif name=='tx_integrity':command += [str(root/'tests/tx_integrity_test.cpp')]
  elif '_test-' in name:
   command += ['-I',str(root/'tests/realtek_stubs'),str(root/('tests/'+name.split('-')[0]+'.cpp')),str(components/'ethernet/realtek_control.cpp')]
   if native:command += ['-DTEST_NATIVE_TRANSPORT']
  else:
   command += ['-I',str(root/'tests/realtek_stubs'),str(root/'tests/test_realtek.cpp'),str(components/'ethernet/realtek_control.cpp')]
   if native:command += ['-DTEST_NATIVE_TRANSPORT']
  subprocess.run(command+['-o',binary],check=True)
  subprocess.run([binary]+([str(root/'tests/fixtures/tx-integrity-ipv4.bin'),str(root/'tests/fixtures/tx-integrity-ipv6.bin')] if name=='tx_integrity' else []),check=True)
  print('PASS',name,flush=True)

