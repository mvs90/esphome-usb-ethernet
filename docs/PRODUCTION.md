> Aktueller rc.5-Stand vom21.10.2026: P2C5V mit direkter USB-Topologie,
> sauberes v101 und drei UART-/API-bestätigte PoE-Kaltstarts ergänzt.
> Die P2C5V-Langzeitabnahme enthält eine vom Nutzer akzeptierte externe
> Mac-Link-Ausnahme; keine vollständige verlustfreie Datenstunde bestätigt.
> Weitere Hardwaregrenzen siehe TEST_STATUS.md. Die folgende rc.4-Historie
> bezieht sich auf den InLine und wird nicht auf P2C5V übertragen.

> Aktueller rc.4-Stand vom20.10.2026: Neue native Datenstunde nach Update auf
> HA2026.10.2 bestanden:3600,07s/2018629632Byte exakter UDP/TCP4/6-Verkehr.
> API3700s fehlerfrei; die ursprüngliche Nachlastfenster-Auswertung war zu spät
> angesetzt. Separate Neuauswertung unveränderter Zustände: Idleheap-136B.
> V97-Betriebsimage: verschlüsseltes OTA, Boot/API,90s ohne neue Fehler und38
> aktuelle HA-Entitäten inklusive kalibriertem physischem Port9 bestätigt.
> Originale UniFi-Einstellungen und finaler v97-PoE-Kaltstart sind bestätigt:
> Boot67,303s, API4/LLA/GUA und weitere90s ohne neue Fehler. rx_stack_drop2
> aus dem Anlauf unverändert; alle übrigen nativen/RTL-Fehlerzähler0.
> Andere Chips/Boards, Halbduplex,
> reine asymmetrische Pause und unabhängige physische LPI-Messung sind damit
> nicht pauschal freigegeben. Frühere Messungen unten bleiben als Historie.

# Produktiver Betrieb des RTL8152 am ESP32-S3

Dieser Produktionskandidat ist auf die gemessenen InLine33380J-POE01- und
P2C5V-Exemplare mit RTL8152-Revision4C10 am Waveshare ESP32-S3-LCD-Driver-Board
begrenzt. Der Stand der laufenden
Abnahme steht in `TEST_STATUS.md`. Eine beliebige andere RTL-Revision oder ein
anderes ESP32-Board ist damit nicht freigegeben.

## Aktueller Abnahmestand

Der integrierte Treiberstand bestand eine vollständige Stunde bytegenauen
IPv4/IPv6-UDP/TCP-Verkehr mit allen Offload-/Aggregationsoptionen, drei
CPU-/RAM-Lastphasen und3700s verschlüsselter API. Keine neuen Treiber-/RTL-
Fehler, vergleichbarer Idleheap-64B. Host3790s stabil, WLAN aus. Kein PCAP
in diesem Lauf; daraus keine isolierte Ursachenzusage für frühere Verluste.

Die bereinigte Betriebsfirmwarev84 mit optionalem LLDP und kalibrierbarer
UniFi-Portzuordnung ist per verschlüsseltem OTA geprüft. Boot>=65s,
IPv4-/link-lokale/globaleIPv6-API und38HA-Entitäten
(27Sensoren/7Buttons/3Textfelder/1Schalter) sind geprüft. Fünfzehn LLDP-Entitäten
und eine über HA ausgeführte, anschließend wiederhergestellte eigene Textänderung
sind bestätigt. Die neue Datenregressionv81 bestand600,08s/558Zyklen/346263552B
bytegenau bei aktivem LLDP und allen Offload-/Aggregationsfunktionen; API660s.
Der vorherige TX-Anlauffehler wurde reproduziert und durch frische PHY-Prüfung
vor IP-Freigabe vermieden. Sieben laufende EEE-/Pause-Umschaltungen bestanden
mit stabilen Hardwarefehlerzählern. Der neue v82-PoE-Kaltstart ist mit UART-POWERON, Uptime-Rücksetzung,
API/HA und180s stationärer Beobachtung bestanden. Hardware-TX-/RX-/Missed-Zähler
sind0, ohne neue Fehlerdifferenzen. Der LLDP-Wire-Sendenachweis ist am
Switch-Spiegelport bestanden: vier korrekte Ankündigungen, TTL120 und30s-Intervall.
Zusätzlich wurden im isolierten VLAN sieben positive/negative Paket-Wake-
Proben bestanden: Magic, Unicast, Broadcast und Multicast lösen gezieltResume
aus, unpassende Klassen bleiben bisTimer imSuspend. Externer Link-Wake
ist am tatsächlichen Switch mit Negativkontrolle bestätigt. Die vier
Ankündigungsproben10/100Half/Full setzen die Register korrekt und stellen Auto
wieder her; Halbduplex-Autonegotiation hat am aktuellen Partner keinen gemeinsamen
Modus. Feste 10/100 Mbit/s Vollduplex sind mit passend fest eingestelltem
Switch und verschlüsselter IPv4-/IPv6-API während des Festmodus bestätigt.
Kein stabiles1.0-Release und keine Freigabe anderer Chips/Platinen.

## Betriebskonfiguration

`examples/usb-rtl8152.yaml` enthält das Betriebsprofil. Es verwendet Ethernet-Auto-
Negotiation, DHCP, IPv4/IPv6, verschlüsselte native API und passwortgeschütztes OTA.
WLAN ist nicht konfiguriert. Checksum-Offload, Aggregation und TSO sind für das
geprüfte Exemplar eingeschaltet; die konkrete Software- und Quellbindung steht
in `BUILD_INFO.json`.

- ESPHome2026.9.1 und ESP-IDF5.5.5 verwenden. Andere Versionen separat prüfen.
- Eigene Secrets aus der Beispieldatei erstellen. Keine Beispielschlüssel verwenden.
- Genau eine USB-Host-Initialisierung. Kein weiterer `usb_host` und kein Logger
  auf USB_SERIAL_JTAG: dieselben USB-Pins werden für den Adapter gebraucht.
- Die externen Komponenten `ethernet`, `logger` und `ethernet_info` gemeinsam
  übernehmen; für LLDP zusätzlich `lldp`. Die offizielle ESPHome-/IDF-Installation
  wird nicht verändert. LLDP-Konfiguration und Grenzen stehen in `LLDP.md`.
- Der InLine versorgt das Board über PoE/USB-C. Ein optionaler UART-Adapter
  bekommt nur GND und RX; seine Versorgungsausgänge bleiben offen.
- Keine automatische USB-Suspend-Aktion für normalen Dauerbetrieb einrichten.
  Die vorhandene Diagnoseaktion ist eine bewusst ausgelöste Netzunterbrechung.
- Testprofile mit Echo-Server, Fehlerinjektionen, künstlicher CPU/RAM-Last oder
  `USE_RTL_FAULT_INJECTION` gehören nicht in die Betriebsfirmware.

Bei mehreren Geräten jeweils eine eindeutige MAC verwenden. Eine explizite
`mac_address` bleibt flüchtig im Adapter; EEPROM oder eFuses werden nicht geändert.
Die Werks-MAC wird ohne diese Option vor DHCP/SLAAC wiederhergestellt. Eine
Umstellung der MAC kann DHCP-Adresse und IPv6-Adressen ändern.

## Überwachung und Wiederherstellung

Nach Firmwarewechsel mindestens65s erfolgreichen Betrieb abwarten. API, IP-Adresse,
MAC, freien internen Heap und größten freien Block prüfen. Verbindungsverlust in
Home Assistant allein unterscheidet keinen LAN-Ausfall von einem Gerätefehler.

`Native Errors` ist eine Diagnosezeile mit kumulativen32-Bit-Zählern:

- `tx_queue_drop`: Speicher-/Queue-Aufnahme fehlgeschlagen.
- `tx_usb_error`: ein USB-Sendeversuch schlug fehl.
- `tx_discard`: angenommene Frames wurden später ohne USB-Versand verworfen.
- `tx_stale`: Teilmenge davon mit ungültiger Verbindungsgeneration.
- `rx_alloc_drop` und `rx_stack_drop`: RX-Speicherfehler beziehungsweise Ablehnung
  durch den IP-Stack.
- `control_timeout`/`control_recovered`: Management-Wartezeit überschritten und
  anschließend tatsächliche Completion erhalten.
- `config_retry`: Adapterkonfiguration wurde erneut versucht.

Im stationären Betrieb die Differenz der Zähler betrachten. Bei absichtlichem
Linkverlust, Suspend oder Überlast können Verwerfungen auftreten. `tx_stale` wird
nicht zusätzlich zu `tx_discard` als unabhängiger Verlust addiert. Nach Neustart
beginnen Softwarezähler neu; Hardware-Tallyzähler können einen Board-Neustart
überleben. Zählerüberlauf ist kein Nachweis eines Neustarts.

Linkverlust wird automatisch erkannt und die IP-Verbindung nach Rückkehr erneut
hergestellt. Wartende Sendepakete einer alten Verbindung werden verworfen statt
in die neue Sitzung übernommen. USB-Control-Transfers werden bei einem Timeout
nicht unsicher freigegeben. Ein dauerhaft blockierter Controller ist damit noch
nicht automatisch repariert; als Wiederherstellungsweg bleibt ein vollständiger
PoE-Neustart des Adapters und Boards über den Switch oder dessen RJ45-Verbindung.

Für Wartung die funktionierende vorherige OTA-Datei und passende ELF-Symbole
aufbewahren. Der USB-Downloadmodus ist der unabhängige Flashweg. Ein unbestätigtes
OTA-Image kann zurückrollen; ein bestandener Flash bestätigt noch keinen
Netzwerk- oder Kaltstarttest.

## Grenzen der Freigabe

Für normalen Betrieb Auto-Negotiation verwenden. Erzwungenes10/100-Halbduplex hat
am geprüften Switch keinen Carrier ergeben und ist nicht freigegeben. Weitere
Wake-Filter sind nicht durch echte Auslöser vollständig abgenommen. Selektive
Multicastfilter, gemessene Pauseframes/LPI-Ersparnis, Displaylast und physischer
USB-Hotplug bei unabhängig versorgtem Board bleiben separate Aufgaben.

Mehrere strikte UDP-Langläufe verloren Pakete. Beim früheren WLAN-Lauf war ein
Paket im Mac-Mitschnitt vorhanden, vor Board-RX aber nicht beobachtet. Zwei
kabelgebundene Wiederholungen wurden durch kurze, unbeabsichtigte Linkausfälle
des zusätzlichen Mac-Ethernetanschlusses unterbrochen. Nach Tausch allein dessen
LAN-Kabels verlor ein weiterer Lauf eine IPv4-Antwort nach erfolgreichem USB-TX;
die physische Zustellung ist damit nicht bewiesen. Eine anschließende 550-s-
Vollpaketkontrolle bestand, ohne die frühere Verlustursache zu klären.
Auch der kontrollierte TX-Modus0-Vergleich verlor nach2479s eine IPv4-Antwort:
Anfrage im vollständigen Mitschnitt gültig, Boardmarker bis erfolgreichem USB-TX,
Antwort am Mac nicht erfasst;0 Kernel-Drops. Aggregation und TSO waren aktiv,
TSO nutzt weiterhin Hardwarechecksummen für seine Segmente. Damit erklärt der
normale TX-Checksum-Offload allein den Verlust nicht. Der neue Vergleich ohne
TX-Aggregation läuft diagnostisch weiter; bereits beobachtete Queue-Abweisungen
verhindern unabhängig vom späteren Datenergebnis eine strenge Produktionsabnahme. Kürzere bestandene Proben sind kein belegter Fix. Die aktuellen
Stundentests und deren Ergebnisse werden getrennt in `TEST_STATUS.md` ausgewiesen.
UDP garantiert keine Zustellung; ein fehlendes Echo allein lokalisiert den Fehler
nicht zum RTL-Treiber. Für API und OTA werden auch bytegenaue TCP-Übertragungen und
Wiederverbindungen separat geprüft.

Keine RTL-Temperatur oder Stromaufnahme wird als Sensor angeboten: Für dieses
Exemplar ist keine belegte Schnittstelle vorhanden. Das spätere Touch-LCD-4Rev4
braucht seine eigene Board-/Pin-/Versorgungs- und Integrationsprüfung.
