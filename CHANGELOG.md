# Changelog

## v0.1.0-rc.6 (test release)

- One native YAML for both measured InLine/P2C5V adapters; shared chip identification, strict factory-MAC descriptor validation.
- Bind USB data/control access to the opened device, invalidate removed-device state and defer reopening until pending transfers retire.
- Updated browser-only YAML guides and examples, optional fixed MAC and explicit cold/live replacement test plan.
- 18 sanitizer host suites and native/ECM builds pass. No hardware flash or replacement test performed for this version. Earlier hardware results do not qualify this new reconnect path.

## v0.1.0-rc.5

- First public Ethernet repository for measured InLine33380J POE01 and P2C5V RTL8152-4C10 adapters.
- Native bulk/offload, IPv4/IPv6, API/OTA and power-management paths; pinned ESPHome2026.9.1 / IDF5.5.5.
- LLDP implementation moved to its own pinned repository; runtime sources unchanged.
- New English/German guides, minimal configurations, optional diagnostics and reproducible synthetic fixtures.
- Qualification includes an operator-accepted external P2C5V host-link exception, not an uninterrupted loss-free data hour.
