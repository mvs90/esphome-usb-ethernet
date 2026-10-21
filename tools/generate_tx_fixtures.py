#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate independent synthetic LOAD packet vectors using documentation addresses."""
from pathlib import Path
import ipaddress,struct
ROOT=Path(__file__).resolve().parents[1]
def checksum(data):
 if len(data)%2:data+=b'\0'
 value=sum(struct.unpack('!'+str(len(data)//2)+'H',data))
 while value>>16:value=(value&65535)+(value>>16)
 return (~value)&65535
for version,sequence in ((4,0),(6,63)):
 cycle=0x01020304;payload=b'LOAD'+struct.pack('!II',cycle,sequence)+bytes([(cycle+sequence)%251])*1388
 src=ipaddress.ip_address('192.0.2.1' if version==4 else '2001:db8::1').packed
 dst=ipaddress.ip_address('192.0.2.2' if version==4 else '2001:db8::2').packed
 udp=struct.pack('!HHHH',3333 if version==4 else 3334,40000,1408,0)+payload
 pseudo=src+dst+(struct.pack('!BBH',0,17,len(udp)) if version==4 else struct.pack('!I3xB',len(udp),17))
 udp=udp[:6]+struct.pack('!H',checksum(pseudo+udp) or 65535)+udp[8:]
 if version==4:
  ip=struct.pack('!BBHHHBBH4s4s',0x45,0,20+len(udp),0x1111,0,64,17,0,src,dst)
  ip=ip[:10]+struct.pack('!H',checksum(ip))+ip[12:]
 else:ip=struct.pack('!IHBB16s16s',6<<28,len(udp),17,64,src,dst)
 eth=bytes.fromhex('020000000002020000000001')+struct.pack('!H',0x0800 if version==4 else 0x86dd)
 (ROOT/f'tests/fixtures/tx-integrity-ipv{version}.bin').write_bytes(eth+ip+udp)
