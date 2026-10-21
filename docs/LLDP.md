# LLDP has its own repository

Use [mvs90/esphome-lldp](https://github.com/mvs90/esphome-lldp) for the component, complete examples and options.

Load `[ethernet, ethernet_info, logger]` from this repository at `v0.1.0-rc.5` and `[lldp]` from the LLDP repository at `v0.1.0-rc.1`. LLDP requires this Ethernet overlay's Layer-2 interface; arbitrary built-in backends are not currently supported.
