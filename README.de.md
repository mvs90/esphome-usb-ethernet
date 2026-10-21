# USB-Ethernet und PoE für ESPHome am ESP32-S3

**Ohne Dateien zu kopieren oder Terminal:** [YAML im Webeditor einfügen und mit ESPHome Web erstflashen](docs/WEB_EDITOR.de.md). Vollständige Vorlage ohne `secrets.yaml` und mit automatisch geladenen GitHub-Komponenten.

[English](README.md) · [Vollständige Beispiele](examples) · [Prüfstatus](TEST_STATUS.md)

Eine USB-C-Verbindung liefert dem ESP32-S3 **PoE-Strom und Ethernet** für die native ESPHome-API, Home Assistant und OTA – ohne WLAN. Die externe Komponente überlagert gezielt die ESPHome-Komponenten `ethernet`, `ethernet_info` und `logger`.

## Welches Beispiel brauche ich?

- **InLine 33380J POE01:** [inline.yaml](examples/inline.yaml).
- **P2C5V**, geprüftes Exemplar zur ASIN B09M2N3NHX: [p2c5v.yaml](examples/p2c5v.yaml).
- Beide verwenden `type: USB_RTL8152`. Gemessen wurden `0bda:8152`, bcdDevice `2000`, maskierte Chiprevision `4C10`. Ein Produktname allein garantiert diese Hardware nicht.

Getestet ist das **Waveshare ESP32-S3-LCD-Driver-Board mit 8 MB Flash, ohne Display/Akku**. Das Touch-LCD-4 Rev.4.0 ist eine andere Platine und braucht eine eigene Prüfung. ESP32-C6 Super Mini eignet sich nicht für diesen nativen USB-Host-Aufbau.

## Einrichtung in sechs Schritten

1. **ESPHome 2026.9.1** verwenden. Die Beispiele binden ESP-IDF **5.5.5** und den Repository-Tag **v0.1.0-rc.5** fest ein. Andere Versionen sind nicht automatisch kompatibel.
2. Das passende Beispiel in den eigenen ESPHome-Konfigurationsordner kopieren. Daneben `secrets.yaml` mit eigenem `api_encryption_key` und `ota_password` anlegen; Vorlage: [secrets.example.yaml](examples/secrets.example.yaml). Einen API-Schlüssel erzeugt `openssl rand -base64 32`.
3. Einen eindeutigen Gerätenamen einstellen. Mit `esphome config inline.yaml` prüfen und mit `esphome compile inline.yaml` bauen; bei P2C5V den entsprechenden Dateinamen verwenden.
4. Zum ersten Flash den PoE-Adapter vom Board trennen, das Board direkt per USB-Datenkabel am Computer anschließen und den eindeutig zugeordneten Downloadport verwenden: `esphome upload inline.yaml --device <BOARD_PORT>`. Falls erforderlich BOOT/Downloadmodus benutzen.
5. USB-Kabel zum Computer entfernen. Dann **PoE-Port → Ethernetkabel → InLine/P2C5V → USB-C direkt ans Board** verbinden. Das Board erhält dabei Strom und Netzwerk über den Adapter.
6. DHCP-Adresse ermitteln und das Gerät in Home Assistant unter **Einstellungen → Geräte & Dienste → Integration hinzufügen → ESPHome** einbinden. Eigenen API-Schlüssel eingeben. Weitere Firmwareupdates funktionieren über `esphome upload inline.yaml --device <GERÄTE_IP>`.

Für ein bestehendes Projekt den `external_components`-/`ethernet`-Block aus [README.md](README.md#add-it-to-an-existing-project) und die erforderliche UART-/Frameworkkonfiguration des vollständigen Beispiels übernehmen. Vorhandene YAML-Blöcke zusammenführen; keine zweite USB-Hostinitialisierung anlegen. Diese getestete Konfiguration verwendet kein WLAN und keine USB-Seriell-Konsole. Ein ESPHome-/IDF-Fork ist nicht nötig.

## Diagnose und optionale Funktionen

Für den normalen Betrieb reichen API und OTA; ein separater UART-Adapter ist nicht nötig. Für frühe Bootlogs am getesteten Board: UART0 mit 115200 Baud, GPIO43/TX → RX eines **3,3-V-Logikadapters**, GND → GND. Bei PoE alle Versorgungspins des UART-Adapters offen lassen.

[diagnostics.yaml](examples/diagnostics.yaml) kann als lokales ESPHome-Paket ergänzt werden und zeigt Status und Fehlerzähler. Der Standardaufbau nutzt DHCP und die MAC des jeweiligen Adapters. Statische IP oder eine flüchtige feste MAC sind im [Einrichtungsleitfaden](docs/SETUP.md) beschrieben.

**LLDP ist optional und separat:** [mvs90/esphome-lldp](https://github.com/mvs90/esphome-lldp). Es zeigt Switch-/Nachbardaten in Home Assistant und erlaubt eigene System-/Portbeschreibungen. Die LLDP-Komponente benötigt derzeit die Layer-2-Schnittstelle dieses Ethernet-Overlays.

## Was ist tatsächlich freigegeben?

Die gemessenen InLine-/P2C5V-Exemplare sind am genannten Board geprüft, einschließlich API/OTA, Offload, Wake- und Pausefunktionen. Das saubere P2C5V-v101 bestand drei echte PoE-Kaltstarts und die abschließende Fehlerzählerprüfung.

**Für P2C5V ist keine vollständige verlustfreie Datenstunde bestätigt.** Belegte Mac-Linkabbrüche wurden vom Betreiber ausdrücklich als externe Ausnahme akzeptiert; die ursprünglichen FAIL-Ergebnisse bleiben erhalten. Andere Chips/Platinen, Halbduplexverkehr, rein asymmetrische Pause und eine unabhängige physische LPI-Messung benötigen eigene Prüfungen. Es ist keine Temperatur-/Strommessschnittstelle dieser RTL-Revision belegt. Der USB-Full-Speed-Bus begrenzt den Durchsatz; 100-Mbit-Link ist nicht 100-Mbit-Nutzdatenrate.

Details: [TEST_STATUS.md](TEST_STATUS.md). Lizenz: C/C++-Runtime GPL-3.0-only, Python MIT, vorhandene ESPHome-Hinweise bleiben erhalten; siehe [LICENSE](LICENSE).
