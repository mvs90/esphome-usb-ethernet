# Treiberumfang für RTL8152-Revision4C10

Am Waveshare ESP32-S3-LCD-Driver-Board sind InLine33380J POE01 und das
identifizierte P2C5V-Exemplar geprüft. P2C5V nutzt denselben nativen Backendpfad
mit zusätzlicher Erkennung der einzelnen direkt angeschlossenen NIC für USB-
Suspend/Wake. Sauberes v101 und drei echte PoE-Kaltstarts sind bestätigt.
Seine Langzeitabnahme enthält die ausdrücklich akzeptierte externe Mac-Link-
Ausnahme; keine verlustfreie P2C5V-Stunde. Siehe [Adapter](ADAPTERS.md) und
[Prüfstatus](../TEST_STATUS.md). Die Matrix beschreibt vorhandene
Implementierung; historische InLine-Einzelprüfungen gelten nicht automatisch
als zusätzliche P2C5V-Abnahme.

Ziel ist ein vollständig nutzbarer ESPHome-Netzwerkpfad für das tatsächlich
identifizierte Exemplar. USB-CDC-ECM verwendet Espressifs Implementierung.
Das native RTL8152-Backend besitzt einen eigenen revisionsgebundenen Daten-
und Kontrollpfad. Seine neue asynchrone TX-/Suspend-Abnahme läuft getrennt.
Ein vollständiger Vendortreiber wird durch erfolgreiche ECM-Tests nicht belegt.

## Funktionsmatrix

| Funktion | Implementierung / Abnahme |
| --- | --- |
| PoE-Erkennung, 5-V-Wandlung, Überspannungsschutz | Adapterhardware; keine ESPHome-Softwarefunktion. Elektrische Grenzwerte nicht vermessen. |
| Adapterhub und Full-Speed-USB | ESP-IDF-USB-Host, genau eine Initialisierung; am Driver-Board getestet. |
| Auswahl ECM-Konfiguration | Expliziter gemessener Quirk 0bda:8152 / bcdDevice 2000 / zwei Konfigurationen → 2. Andere Revisionen nicht automatisch unterstützt. |
| Ethernet RX/TX, MAC lesen, ECM-Linkmeldungen | Espressif ECM 0.4.0 + CDC 3.1.0 + iot_eth 1.1.0. |
| DHCP/IPv4, API, OTA, HA | Implementiert und am realen Gerät getestet. |
| ESPHome-Verbindungsstatus und IP-/DNS-Listener | Implementiert; Adress-/DNS-Vergleich ergänzt. Echter DHCP-Adresswechsel separat prüfen. |
| Statische IPv4/DNS | static-v2 nach DNS2-Korrektur per API erreichbar; weitere Tests im aktuellen Prüfstatus. |
| Interface enable/disable | Logische IP-Schnittstelle; kein Abschalten des PHY oder der PoE-Stromversorgung. Softwarezyklen separat protokolliert. |
| IPv6 | Dual Stack/SLAAC, bevorzugte Adressen, Mindestanzahl und Listener; Link-local/globale API und IPv6-OTA praktisch getestet. IPv6-only mit fester MAC und ohne DHCPv4 nativ bestanden (v54); statische IPv6 nicht freigegeben. |
| Feste MAC | Flüchtiges Hardware-IDR und esp_netif vor DHCP/SLAAC, Unicast-Validierung und Rücklesen; statisch und mit DHCP getestet. Werksadresse bei entfernter Option wiederherstellen. Keine EEPROM-Programmierung. |
| Realtek-Revisions-/PHY-Rohdaten | Lesender, expliziter Diagnoseaufruf am konkreten Exemplar getestet; Hardwarekennung 4C10, PHY 8B000000. Keine periodische Linkkorrektur. |
| PHY/MDIO | Revisionsgebundener OCP-/MDIO-Pfad. 10/100-Vollduplex mit Autonegotiation und festem Modus bei passend konfiguriertem Switch bestanden; verschlüsselte IPv4-/IPv6-API während des Festmodus geprüft. Halbduplexregister und Rückstellung geprüft, Halbduplex-Datenverkehr mangels passender Switchoption nicht qualifiziert. |
| EEE / Pause | Konfiguration/Advertisement implementiert und rückgelesen. EEE am USW Pro XG 24 PoE ausgehandelt und OFF/ON/OFF/ON mit Rohregisterbeobachtung geprüft. Reale PAUSE-Frames am Switch gezählt; Empfangsbremswirkung mit zwei Negativkontrollen bestätigt. Reine asymmetrische Gegenstelle und unabhängige physische LPI-Zeit-/Energiemessung fehlen. |
| Wake-on-LAN | Fünf Filterarten gesetzt/rückgelesen; Magic/Unicast/Broadcast/Multicast über USB-Remote-Wake mit isolierten positiven/negativen Gegenproben bestanden; externer Link-Wake mit Gegenprobe am realen Switch bestätigt. Kein ESP32-Deep-Sleep. |
| Multicast / Promiscuous | ECM-All-Multicast, Promiscuous schaltbar; nativ statischer64-Bit-Hashfilter mit Rücklesen und realer ALL/FILTERED/NONE-Paketprobe. Keine dynamische IGMP/MLD-Mitgliedschaftsverfolgung. |
| Hardwarezähler | 13 Realtek-Tallyzähler lesbar; Werte können ESP32-Neustarts überleben. |
| Nativer Vendormodus | Neues Backend `USB_RTL8152`, Konfiguration1, eigene TX-/RX-Deskriptoren. API/IPv6/MTU zunächst bestanden; Grenzlängenfehler korrigiert und80/80 Antworten bestanden. Logische USB-Neuanmeldungen, DHCP-/DNS-Ausfälle, OTA-Abbrüche und UDP-Überlast mit Erholung bestanden; 61min API unter Last bestanden; strikte UDP-Langläufe nach2756s und1453s fehlgeschlagen. Aktueller Verlust im Mac-WLAN-Capture ausgehend vorhanden, vor Board-RX nicht beobachtet; zwei kabelgebundene Läufe durch spontane Mac-Linkausfälle unterbrochen. Nach Mac-Kabeltausch eine Antwort nach USB-TX nicht angekommen; 550s-Kontrolle bestanden, Ursache offen. TX-Modus0-Vergleich nach2479s mit einer fehlenden Antwort nach USB-TX gescheitert; voller Mac-PCAP bestätigt fehlende Antwort bei0 Kernel-Drops. Vergleich ohne TX-Aggregation bestand die Datenstunde, scheiterte als Gesamtabnahme an108 Queue-Abweisungen. Neu: Queue32/begrenzter Rückstau und explizite native FIFO/DMA-Initialisierung hardwaregeprüft; neue strenge Vollmodus-Stunde3600,39s/2083786752B bestanden, API3700s und drei Lastphasen ohne neue Fehler; Idleheap-64B. Kein PCAP dieses Laufs, keine isolierte Ursachenzusage. |
| Hardwarechecksummen | TX-Deskriptoren für IPv4/IPv6 TCP/UDP mit Pseudoheaderseed praktisch geprüft; Padding/Fragmente/Erweiterungsheader behalten Softwarepfad. RX-Fehlerflags ausgewertet, Softwareprüfung bleibt aktiv. |
| TSO / Aggregation | Optionale RX-/TX-Aggregation tatsächlich nachgewiesen, Last Fenster4 verlustfrei; Fenster8 nach Nachbarauflösung verlustfrei, kalte IPv6-Achterbursts verlieren vor USB-RX Pakete, warme Bursts vollständig; UDP-Timeouts in zwei Mischlastläufen werden separat diagnostiziert. Ein weiterer kalter IPv6-Antwortburst verlor fünf Pakete nach App-sendto und vor USB-TX (SDK-Nachbarqueue3), warme Kontrolle8/8. Konservative TCP-Rekoaleszierung für IPv4/IPv6-Hardware-TSO bytegenau praktisch geprüft. Kein GSO-Vertrag mit lwIP. |
| USB-Suspend / Remote-Wake | Espressif USB1.6.0/CDC-ACM2.4.1; mehrfach Timer-, manuelles und Magic-Packet-Resume ohne Neustart bestanden, einschließlich unbefristetem Suspend. Kein ESP32-Deep-Sleep. |
| RTL-LEDs | Selektor und Blinkparameter flüchtig les-/schreibbar, Register-Rücklesen und optische Abnahme beider LEDs bestanden. |
| RTL-Temperatur / Stromaufnahme | Keine belegte Sensor-/Registerschnittstelle. Keine erfundenen Werte und nach Nutzerwunsch keine Ersatztelemetrie. |
| USB-Billboard/USB-C-PD | Eigenständige Adapterfunktion; kein USB-C-PD-Treiber implementiert. |
| Display/Touch, andere S3-Platinen | Eigenständige spätere Integrationsabnahme. |

## Weg zu einem vollständigen Adaptertreiber

1. ECM-Datenpfad robust abnehmen: Link-/Strom-/USB-Lebenszyklus, DHCP/DNS,
   statische Konfiguration, Last, OTA-Unterbrechung und Wiederherstellung.
2. Tatsächliche Siliziumrevision über dokumentierte Realtek-Zugriffe ermitteln.
   `bcdDevice` ist dafür kein ausreichender Beleg.
3. Unterstützte Realtek-Funktionen anhand Chiprevision und Primärquellen abgrenzen.
   Linux `r8152` zeigt einen wesentlich größeren Verwaltungs-/PHY-/ethtool-Pfad,
   dessen Übernahme nicht mit dem ECM-Protokoll gleichgesetzt werden darf.
4. Für zusätzliche Funktionen zuerst einen isolierten, lesenden Diagnosepfad
   vorbereiten. Revisionsgebundene flüchtige Registerzugriffe sind inzwischen implementiert.
   EEPROM-/eFuse-Programmierung bleibt ausgeschlossen.
5. Vendormodus nur implementieren, wenn benötigte Funktionen nicht über ECM
   erreichbar sind. Datenframing, Initialisierung, Revisionen und Lizenzen müssen
   dann separat behandelt werden. Hardwarefunktionen werden dadurch nicht Software.

Referenzen: [Espressif ECM](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_ecm.html),
[Linux r8152 v6.17](https://github.com/torvalds/linux/blob/v6.17/drivers/net/usb/r8152.c).

Der öffentliche Vorabstand muss seine Grenzen sichtbar benennen. Eine
Vollständigkeitszusage gibt es erst nach Implementierung und Abnahme des oben
festgelegten Umfangs. Bis dahin kein stabiles 1.0-Release.

Aktuelle Optionen, Aktionen, Grenzen und Protokollherkunft: [REALTEK_CONTROL.md](REALTEK_CONTROL.md).
