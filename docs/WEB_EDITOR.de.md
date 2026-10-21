# Einrichtung nur im Browser: YAML einfügen und ESPHome Web flashen

Diese Anleitung braucht **keinen Git-Clone, kein Terminal, keine manuell kopierten Komponenten und keine `secrets.yaml`**. Du fügst den YAML-Inhalt unten direkt in den ESPHome-Webeditor ein. Die externen Komponenten lädt der Device Builder automatisch von den fest eingetragenen GitHub-Tags. Nur die vom Builder erzeugte Factory-BIN wird im Browser heruntergeladen und im Webflasher ausgewählt.

Das gleiche YAML funktioniert für die gemessenen **InLine33380J POE01- und P2C5V-Exemplare mit RTL8152-Revision4C10**. Ein Produktname allein garantiert die Chiprevision nicht. LLDP ist optional und hat ein [eigenes vollständiges Webeditor-Beispiel](https://github.com/mvs90/esphome-lldp/blob/main/docs/WEB_EDITOR.de.md).

## 1. Webeditor öffnen und Gerät anlegen

- **ESPHome Device Builder 2026.9.1** öffnen – beispielsweise über „ESPHome“ in Home Assistant oder die eigene ESPHome-Weboberfläche. Die Anleitung verwendet die geprüfte Kombination mit **ESP-IDF5.5.5**. `min_version` im YAML ist keine Zusage für alle neueren Builder-Versionen.
- „Neues Gerät / New device“ wählen, einen eindeutigen Namen vergeben und bei der Geräteauswahl **ESP32-S3** verwenden. Eine sofortige Installation überspringen; zunächst nur den Konfigurationseintrag anlegen. Assistentenbeschriftungen können je nach Sprache leicht abweichen.
- Beim neuen Gerät **Bearbeiten / Edit** öffnen. Falls dort bereits ein eigener API-Schlüssel unter `api.encryption.key` angezeigt wird, diesen für den nächsten Schritt merken. Vom Assistenten angelegte WLAN-Einträge werden durch das vollständige Ethernet-YAML ersetzt.

## 2. Vollständigen YAML-Inhalt einfügen

Den Inhalt des **neu angelegten** Geräts vollständig durch das YAML unten ersetzen. Dafür genügt Kopieren des Textes aus dem Codeblock; keine Beispieldatei herunterladen. Bei einem bereits vorhandenen eigenen Projekt stattdessen die Abschnitte sorgfältig zusammenführen.

Oben unter `substitutions` nur drei Angaben anpassen:

| Wert | Was eintragen? |
| --- | --- |
| `device_name` | Eindeutiger Gerätename mit Kleinbuchstaben, Ziffern und Bindestrichen, z. B. `flur-poe`. |
| `api_key` | Eigener 32-Byte-Base64-API-Schlüssel aus dem Assistenten. Alternativ auf der offiziellen [Native-API-Dokumentationsseite](https://esphome.io/components/api/#configuration-variables) den im Browser zufällig erzeugten Schlüssel mit **Copy/Kopieren** übernehmen. |
| `ota_password` | Eigenes langes OTA-Passwort. Es wird direkt im privaten Geräte-YAML gespeichert. |

Die Platzhalter müssen ersetzt werden. Einen API-Schlüssel nicht als beliebiges Wort erfinden und keinen Beispiel-/Testschlüssel wiederverwenden. Die Zugangsdaten bleiben in deiner privaten Builder-Konfiguration; sie gehören nicht in GitHub-Issues. Die Repository-Quellen enthalten keine Zugangsdaten.

**Boardbezug:** Die Flashgröße 8MB und UART-Pins43/44 passen zum getesteten **Waveshare ESP32-S3-LCD-Driver-Board**. Für andere Platinen sind diese Werte anhand ihrer Beschaltung zu prüfen; Touch-LCD-4 Rev.4.0 und ESP32-C6 Super Mini sind nicht austauschbar mit diesem Aufbau. Der externe USB-UART-Adapter ist für die normale Einrichtung und den ersten Flash nicht nötig.

```yaml
# Paste this complete file into the ESPHome Device Builder web editor.
# Replace all three values below. No secrets.yaml or local component copies needed.
substitutions:
  device_name: "usb-poe-web"
  api_key: "PASTE_YOUR_API_KEY_HERE"
  ota_password: "CHOOSE_YOUR_OWN_OTA_PASSWORD"

esphome:
  name: ${device_name}
  min_version: 2026.9.1

# Tested board: Waveshare ESP32-S3-LCD-Driver-Board, 8 MB flash.
esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  flash_size: 8MB
  toolchain: esp-idf
  framework:
    type: esp-idf
    version: 5.5.5
    sdkconfig_options:
      CONFIG_ESP_CONSOLE_UART_CUSTOM: "y"
      CONFIG_ESP_CONSOLE_UART_CUSTOM_NUM_0: "y"
      CONFIG_ESP_CONSOLE_UART_TX_GPIO: "43"
      CONFIG_ESP_CONSOLE_UART_RX_GPIO: "44"
      CONFIG_ESP_CONSOLE_UART_BAUDRATE: "115200"
      CONFIG_ESP_CONSOLE_SECONDARY_NONE: "y"
      CONFIG_USJ_ENABLE_USB_SERIAL_JTAG: "n"

external_components:
  - source: github://mvs90/esphome-usb-ethernet@v0.1.0-rc.5
    components: [ethernet, ethernet_info, logger]

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

network:
  enable_ipv6: true
  min_ipv6_addr_count: 1

# The native USB PHY is occupied by the Ethernet host; log via UART0.
logger:
  hardware_uart: UART0
  baud_rate: 115200
  level: INFO

api:
  reboot_timeout: 0s
  encryption:
    key: "${api_key}"

ota:
  - platform: esphome
    password: "${ota_password}"

text_sensor:
  - platform: ethernet_info
    ip_address:
      name: Ethernet IP
      address_1:
        name: Ethernet IPv6 Link Local
      address_2:
        name: Ethernet IPv6 Global
    mac_address:
      name: Ethernet MAC
```

## 3. Im Device Builder kompilieren

1. **Speichern / Save**, danach **Validieren / Validate** wählen, sofern angeboten.
2. **Installieren / Install → Manuell herunterladen / Manual download** öffnen.
3. Als Format ausdrücklich **Factory format / Factory-Image** wählen. Den Build abwarten und die vollständige Factory-BIN herunterladen, häufig `firmware.factory.bin` genannt. Manche Builder zeigen den Gerätenamen im Dateinamen.

Für die Erstinstallation mit ESPHome Web das **Factory-Image mit Bootloader und Partitionstabelle** verwenden, nicht die reine `firmware.ota.bin`. Es müssen keine Bootloader-Dateien zusammengesucht und keine Flash-Adressen manuell eingetragen werden. Die eigentlichen Komponenten lädt der Builder selbst von GitHub; Internetzugriff des Builders ist dafür erforderlich.

## 4. Erstflash mit ESPHome Web

1. **InLine/P2C5V vom USB-C-Anschluss des Boards abziehen.** Das Board mit einem USB-Datenkabel direkt an den Computer anschließen, an dem der Browser geöffnet ist. Es muss nicht am Home-Assistant-Server stecken.
2. In einem Desktopbrowser mit WebSerial, z. B. Chrome oder Edge, **[web.esphome.io](https://web.esphome.io/)** öffnen.
3. Falls der ESP32-S3 nicht im Downloadmodus erkannt wird: BOOT gedrückt halten, USB-Datenkabel einstecken, dann BOOT loslassen. Je nach Board gibt es alternativ eine BOOT/RESET-Tastenfolge.
4. **Verbinden / Connect** wählen und im Browser den **Port des ESP32-S3-Boards** auswählen – nicht den Port eines optionalen FT232R-UART-Adapters.
5. Nach dem Verbinden **Installieren / Install** und die zuvor heruntergeladene **eigene Factory-BIN** auswählen. Die eigene Datei flashen, keine generische „Für erste Verwendung vorbereiten / Prepare for first use“-Firmware.
6. Erfolgreiche Installation abwarten, Browserverbindung trennen und das USB-Datenkabel zwischen Board und Computer entfernen.

ESPHome Web ist hier nur der **USB-Flasher**: Es kompiliert das YAML nicht und lädt für diesen Ablauf keine vorgefertigte Firmware aus dem Repository. Auf dem Board ist anschließend die eigene, im Device Builder gebaute Konfiguration.

## 5. Auf PoE umstecken und Home Assistant verbinden

Jetzt verbinden: **aktiver PoE-Switch/Injector → Ethernetkabel → InLine/P2C5V → USB-C direkt ins Board**. Kein zusätzliches Computer-USB-Kabel am Board. Der ESP32-S3 ist jetzt USB-Host; der PoE-Adapter versorgt ihn und stellt Ethernet bereit.

Nach dem Boot die DHCP-Adresse im Router/Switch oder im Device Builder nachsehen. Home Assistant: **Einstellungen → Geräte & Dienste → Integration hinzufügen → ESPHome**, Geräte-IP und den zuvor eingesetzten API-Schlüssel eingeben. Die IP-/MAC-Sensoren erscheinen im Gerät. Für LLDP das [kombinierte Webeditor-YAML](https://github.com/mvs90/esphome-lldp/blob/main/docs/WEB_EDITOR.de.md) verwenden.

Weitere Updates direkt im Device Builder über **Installieren → Netzwerk / Wirelessly / OTA** ausführen. „Wirelessly“ bezeichnet je nach UI allgemein den Netzwerk-Upload – dieser läuft hier über **Ethernet**, ohne WLAN. Bei mDNS-Problemen die aktuelle Geräte-IP als Ziel verwenden. Für normale OTA-Updates ist kein USB-Umstecken mehr nötig.

## Häufige Stolperstellen

- **Ungültiger API-Schlüssel:** eigenen vollständigen Base64-Schlüssel einsetzen, Anführungszeichen beibehalten.
- **`secrets.yaml` fehlt:** dieses vollständige Webeditor-YAML verwenden; die anderen Beispiele mit `!secret` sind für einen dateibasierten Ablauf gedacht.
- **USB-Host nicht erkannt:** echten ESP32-S3, Datenkabel, passenden Port und Downloadmodus prüfen. Die Strom-/USB-Konfiguration nicht ungeprüft auf eine andere Platine übertragen.
- **Keine frühen USB-Logs im Betrieb:** die native USB-PHY wird für Ethernet verwendet. Optionaler Bootlog-UART am getesteten Board: GND→GND, GPIO43/TX→RX eines 3,3-V-Logikadapters; bei PoE bleiben alle UART-Versorgungspins offen.
- **Gerät nach Flash offline:** Computer-USB-Kabel abziehen und den PoE-Adapter direkt mit dem Board verbinden; DHCP und eigenen API-Schlüssel prüfen.

Die dokumentierten Hardware- und Langzeitgrenzen bleiben bestehen; siehe [TEST_STATUS.md](../TEST_STATUS.md). Dieser Browserablauf ist kein neuer Hardwaretest und ändert keine Freigabegrenze.
