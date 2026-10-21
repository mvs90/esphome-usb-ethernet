# Realtek-Diagnose: historischer lesender v5-Stand

Aktuell gibt es zusätzlich einen revisionsgebundenen Verwaltungspfad mit
flüchtigen Schreibzugriffen: [REALTEK_CONTROL.md](REALTEK_CONTROL.md).
Die folgenden Angaben dokumentieren ausdrücklich den früheren v5-Test.

Am 19. Oktober 2026 mit realtek-read-v5 am konkreten Adapter getestet. Eine
explizite Testschaltfläche liest zwei Registerblöcke. Keine automatische
periodische Abfrage. Ergebnis über verschlüsseltes API-Log gesichert:
`TCR=8008104F hw=4C10 PHY=8B000000`.
Die Kennung 4C10 entspricht im Linux-Referenzmapping RTL_VER_02.
PHY-Niederbyte 0x8B meldet Link, 100-Mbit-Modus und Vollduplex nach den dort
dokumentierten Bits; das ist keine gemessene ESP32-Nutzdatenrate.
Kein Diagnosefehler und kein Neustart während dieser Abfrage beobachtet.

Nur für das bereits gemessene USB-Gerät 0bda:8152/bcdDevice2000, nur bei bestehender
Verbindung. CDC kontrolliert die Control-Transfer-Synchronisierung. Aufruf nicht
während USB-Hotplug-/Versorgungstests; ein vollständiger Lifecycle-/Race-Nachweis
für zusätzliche Vendorabfragen steht noch aus.

USB-Request: device/vendor/IN (0xc0), Request 0x05, Index 0x0100 (PLA),
je vier Byte an 0xe610 (TCR-Block) und 0xe908 (PHY-Statusblock).
Hardwarekennung aus oberem Little-Endian-Halbwort des TCR-Blocks mit Maske 0x7cf0;
vorerst nur Rohdaten ausgeben. bcdDevice wird nicht als Siliziumrevision ausgegeben.

Es werden keine Register geschrieben, keine Offloads umgeschaltet, kein EEPROM
und keine eFuses gelesen oder verändert. Fehler bleiben Diagnosefehler und lösen
keinen Neustart aus. Der bewährte ECM-Datenpfad bleibt erhalten.

Die Implementierung verwendet die öffentliche Espressif-Control-Transfer-API und
einen neu geschriebenen kleinen Abfragepfad. Es wurde kein Linux-Treibercode
kopiert. Protokoll-/Registerreferenzen:

- [Realtek-USB-Requestkonstanten im Linux-Header](https://code.googlesource.com/linux/torvalds/linux/+/b20712e853305cbd04673f02b7e52ba5b12c11a9/include/linux/usb/r8152.h)
- [TCR-/PHY-Adressen und Hardwarekennung](https://github.com/torvalds/linux/blob/v6.17/drivers/net/usb/r8152.c)
- `iot_usbh_cdc` 3.1.0: `usbh_cdc_send_custom_request`, `usbh_cdc_get_dev_handle`;
  `iot_usbh_ecm` 0.4.0: `usb_ecm_get_cdc_port_handle`.


## PHY-Auflösung und rohe EEE-Werte

`read_realtek_link_capabilities()` liefert aktuellen PHY-Status, BMCR, BMSR
(zweimal gelesen wegen latch-low), lokale Advertisement und Partner-LPA.
`pause_valid` erfordert Link/Full-Duplex, aktivierte Autonegotiation und deren
Abschluss. `pause_rx`/`pause_tx` lösen die symmetrischen/asymmetrischen Bits nach
IEEE802.3 Annex28B auf; das sind ausgehandelte Fähigkeiten, keine Zählung
empfangener oder befolgter Pauseframes. Der getestete Partner kündigt ohne
zusätzliche Flow-Control-Konfiguration keine Pausefähigkeit an.

`EEE_cap` ist die rohe PCS-Fähigkeit (MMD3, Register20).
`EEE_wake_error_raw` ist ein roher Read des standardisierten PCS-Registers22;
FFFF kann fehlende Implementierung bedeuten. Ein gelesener Nullwert belegt
weder tatsächliche LPI-Eintritte noch Energieersparnis. Kein Temperatur- oder
Stromsensor wird daraus abgeleitet. Hardwareunterstützung und praktische
Funktionsprüfung bleiben von bloßem lesbarem Registerwert getrennt.
