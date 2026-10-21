# InLine und P2C5V automatisch verwenden

Eine einzige Firmware kann beide bisher geprüften Adapter verwenden. In der YAML wird der Chipsatz ausgewählt, kein Produktname:

```yaml
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
```

Diesen Block in die vollständige [Webeditor-Konfiguration](WEB_EDITOR.de.md) integrieren. Kein zweites `ethernet` und kein zusätzliches `usb_host` anlegen. Es gibt keine notwendige Option `adapter: inline` oder `adapter: p2c5v`.

## Was erkennt der Treiber?

Die gemessenen InLine 33380J POE01 und P2C5V stellen beide USB `0bda:8152`, Geräteversion `2000` und RTL-Revision `4C10` bereit. Der Treiber wählt die native USB-Konfiguration, prüft die Revision vor Registerzugriffen und liest die MAC des angeschlossenen Adapters. Ein Markenname lässt sich aus diesen gemeinsamen Kennungen nicht zuverlässig ableiten. Weitere, äußerlich gleiche Geräte mit anderen Chips oder Revisionen sind damit nicht automatisch freigegeben.

Bereits v0.1.0-rc.5 unterstützt beide Geräte beim Start mit derselben YAML. Der lokale Entwicklungskandidat härtet zusätzlich den Wechsel ab: MAC-Strings werden vollständig validiert, USB-Datenport und Steuerzugriffe bleiben an das tatsächlich geöffnete Gerät gebunden, alte Transfers werden vor erneutem Öffnen abgewartet und der Konfigurationszustand wird nach physischer USB-Trennung verworfen. Dieser Kandidat ist noch nicht als neue Version veröffentlicht oder am Board geprüft.

## Wechsel im bisherigen PoE-Aufbau

1. Den USB-C-Stecker des bisherigen Adapters vom Board trennen. Das Board geht dabei aus.
2. Den anderen Adapter an denselben geeigneten PoE-LAN-Anschluss anschließen.
3. Dessen USB-C-Stecker direkt mit dem Board verbinden.
4. Das Board startet dieselbe Firmware und erkennt den jetzt angeschlossenen Adapter. Kein erneutes Flashen nötig.

Diese Schritte sind ein späterer Hardwaretest; während der Softwarevorbereitung ist kein Umstecken erforderlich. Versorgungspins eines optionalen UART-Adapters bleiben bei PoE offen.

Ein unterbrechungsfreier Wechsel bei laufendem ESP32 ist ein anderer Test. Er setzt eine nach Board-Schaltplan geeignete unabhängige Versorgung ohne Rückspeisung voraus. Am getesteten Aufbau ist die einzige USB-C-Verbindung gleichzeitig Stromquelle. Keine zusätzliche Versorgung dafür improvisieren. Zwei gleichzeitig aktive Ethernetadapter werden nicht unterstützt.

## MAC, DHCP und Home Assistant

Ohne `mac_address` verwendet jeder Adapter seine eigene MAC. Dadurch können DHCP-Adresse und IPv6-Adressen wechseln. Die vorhandene ESPHome-Integration in Home Assistant gegebenenfalls auf die neue Adresse umstellen; mDNS kann bei funktionierender Namensauflösung helfen.

Optional lässt sich unter `ethernet` eine eigene, im LAN eindeutige lokale Unicast-MAC konfigurieren, etwa `mac_address: "02:12:34:56:78:90"`. Die Beispieladresse nicht auf mehreren Geräten verwenden. Diese MAC wird nach Erkennung bei jedem Adapter flüchtig programmiert und zurückgelesen; sie verändert kein EEPROM. Eine DHCP-Reservierung für die eigene MAC erleichtert eine gleichbleibende IPv4-Adresse. IPv6-Präfixe und tatsächliche DHCP-Zuteilung hängen weiterhin vom Netz ab.

## Noch offene Hardwareprüfung

- Start InLine → P2C5V → InLine mit derselben Firmware, jeweils DHCP, IPv4/IPv6, API, OTA und LLDP prüfen.
- Beide Richtungen zusätzlich mit fester MAC prüfen: Register-/Netif-MAC stimmen, keine alten IP-Sitzungen bleiben bestehen.
- Bei geeigneter unabhängiger Versorgung später USB-Abziehen/Anstecken unter laufendem Betrieb, auch bei Last und aus Suspend; keine alten Transfers an das Ersatzgerät, kein Watchdog oder Speicherverlust.
- Unbekannte USB-Geräte beziehungsweise abweichende Chiprevisionen dürfen keine RTL-Konfiguration erhalten.

Softwaretests ersetzen diese Hardwarefreigabe nicht. Bestehende Testgrenzen stehen in [TEST_STATUS.md](../TEST_STATUS.md).
