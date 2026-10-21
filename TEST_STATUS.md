# Public repository layout

The Ethernet repository contains 17 runtime sanitizer suites. The three LLDP
suites and implementation are published separately as mvs90/esphome-lldp.
Runtime source bytes are unchanged by the split. Public YAML examples are
minimal configurations derived from the tested board profile; CI validates
the pinned build. Historical laboratory qualification below refers to the
measured adapters, not to every product batch.

# Prüfstatus 0.1.0-rc.5 – P2C5V am ESP32-S3

Lokaler Quellkandidat vom 21. Oktober 2026, nicht veröffentlicht. Zusätzlich
zum InLine wird das gemessene P2C5V-Exemplar (Nutzerreferenz ASIN B09M2N3NHX,
USB 0bda:8152/bcd2000, maskierte Realtek-Revision 4C10) am Waveshare
ESP32-S3-LCD-Driver-Board ohne Display/Akku unterstützt. Die direkte USB-
Topologie benötigt keinen vorgeschalteten Hub-Wake-Schritt; unbekannte oder
mehrdeutige Hubtopologien werden weiterhin abgelehnt.

| P2C5V-Prüfung | Befund |
| --- | --- |
| Basisbetrieb / Integration | PoE, native USB-Initialisierung, DHCP, IPv4/IPv6, verschlüsselte API/OTA und bestehende Home-Assistant-Anbindung bestanden. |
| Bulk / Offload | Kurzer bytegenauer UDP/TCP4/6-Test, IPv4-/IPv6-TSO, Grenzgrößen, fünf Control-Timeout-Erholungen und TX-Sitzungsgrenze bestanden. |
| PHY / EEE | Auto sowie passend gepaarte feste 10/100-Mbit/s-Vollduplexverbindungen, Rückstellung und EEE-OFF/ON-Beobachtung bestanden. Physische LPI-Zeit/Energie bleibt ungeprüft. |
| Wake | Sieben gezielte Paket-Wake-Positiv-/Negativfälle und externer Link-Wake mit Negativkontrolle bestanden. Erster vorzeitiger Unicast-Wake nicht reproduziert; Ursache offen. |
| Pause | Vier Modi mit Switch-Flow-OFF/ON, unabhängig gezählte RTL-Pauseframes, Empfangsbremswirkung mit zwei Negativkontrollen, drei Timer-Suspend-Zyklen und begrenzte Suspendverweigerung bei langem USB-OUT bestanden. |
| Langzeitdaten | Keine vollständige verlustfreie Datenstunde bestätigt. en7: 3083,39s, 2784 vollständige Zyklen, 1727773696 Duplexbyte bis zu einem belegten Mac-Linkausfall; keine neuen Treiber-/RTL-Fehler. |
| Abnahmekriterium | Nutzer akzeptiert die belegten Mac-Linkunterbrechungen als externe Einschränkung und erlaubt die Treiberabnahme ohne weitere Kabel-/Stundenwiederholung, da keine zusätzlichen Treiber-/Hardwarefehler gemessen wurden. Rohberichte bleiben FAIL; kein vollständiger Idle-Nachlastvergleich. |
| Sauberes Betriebsimage | v101: hashgebundene verschlüsselte OTA, bestätigter Boot ab65s, API4/LLA/GUA, aktuelle HA-Entitäten und90s ohne neue Fehler bestanden. |
| Echte PoE-Kaltstarts | Drei UART-POWERONs mit erfolgreicher nativer Initialisierung; API4/LLA/GUA jeweils nach65,725/66,471/66,883s bestätigt. Anschließende unabhängige Bootprüfung bestanden; alle Fehlerzähler0. Weitere90s API/Heap/Queue ohne neue Treiber-/RTL-Fehler bestanden; Fehlerzähler absolut0. |

Im IPv4-Spiegeltest war die am Mac fehlende Antwort am Board-Switchport
vorhanden, am normalen Mac-Anschluss nicht. Im en7-Gegenlauf sind zwei
Mac-Linkunterbrechungen einschließlich IP-Entfernung protokolliert; die zweite
fällt mit drei bereits am Board fehlenden IPv6-Anfragen zusammen. Die Ursache
dieser Host-Linkabbrüche ist nicht eindeutig bestimmt. Es gibt keinen daraus
abgeleiteten, bewiesenen RTL-Firmwaredefekt, aber auch keine unbeschränkte
Dropfreiheitszusage. Wechsel der Gegenstellenkonfiguration erzeugten zuvor
kumulativ vier RTL-TX-Fehler; im stationären Lauf kein weiterer Anstieg.

rc.5-Export: 20 ASAN/UBSAN-Suiten bestanden; P2C5V-Nativbeispiel mit
synthetischen Zugangsdaten im vorhandenen ESPHome-Container vollständig
gebaut, niemals geflasht. Initialer Nullschlüssel wurde vor dem Build
abgelehnt; gültiger synthetischer Schlüssel und Wiederholung bestanden.
Die unveränderten Ethernet-/LLDP-Schemafälle aus rc.4 wurden nicht erneut
ausgeführt. Runtimequellen im Export sind bytegleich zum Hauptbaum.

Halbduplex-Datenverkehr, reine asymmetrische Pause, unabhängige physische
LPI-Messung, andere Revisionen und Platinen benötigen separate Abnahme.
Temperatur-/Stromsensoren dieser RTL-Revision sind nicht belegt. Die folgende
rc.4-Historie gilt für den InLine; dessen bestandene Stunde wird nicht auf den
P2C5V übertragen.

---

# Prüfstatus 0.1.0-rc.4 – neue TX-/Suspend-Abnahme

Lokaler Kandidat vom20.Oktober2026, nicht veröffentlicht. Die neue Datenstunde
ist auf dem beschriebenen Aufbau bestanden; das bereinigte v97-Image bestand
OTA, Boot/API und90s Nachbeobachtung. Auch die Rückstellung der temporären
Switchwerte und der abschließende echte v97-PoE-Kaltstart sind bestätigt.
Weitere90s ohne neue Fehler; rx_stack_drop2 aus dem Anlauf blieb unverändert,
alle anderen nativen und RTL-Hardwarefehlerzähler stehen auf0.
Keine pauschale Freigabe anderer Chips oder Platinen. Geprüftes Exemplar: InLine33380J POE01,
RTL8152B4C10 am Waveshare ESP32-S3-LCD-Driver-Board ohne Display/Akku.

Der alte synchrone Sendepfad scheiterte reproduzierbar an wiederholten
PAUSE-Frames. Ein bloß verlängertes Timeout war nur eine Diagnose. Der neue
Pfad behält genau einen asynchronen USB-OUT-Transfer bis zum echten Callback
und wartet vor Suspend auf dessen Ende. Nach10s erfolglosem Drain wird
Suspend begrenzt verweigert, ohne USB-Puffer freizugeben oder zu überschreiben.

| Neue Prüfung | Ergebnis |
| --- | --- |
|8s PAUSE ohne Suspend, mit zwei Negativkontrollen|v93 bestanden, Daten und API kehren ohne Neustart zurück.|
|8s PAUSE mit aktivem USB-Suspend|v95 drei Fälle bestanden, darunter zwei Negativkontrollen. Je etwa5,13s echter Suspend, Datenrückkehr, keine neuen USB-/Controlfehler.|
|18s PAUSE, Suspend-Drain überschreitet10s|Suspend korrekt verweigert, keine zusätzliche Schlafphase, Daten danach wieder vorhanden; separate frische100 UDP-Echos pro Macanschluss bestanden. Ursprünglicher Bericht INCONCLUSIVE durch zu spätes Auswertefenster, unverändert erhalten. Kein längerer ursprünglicher Nachlauf gemessen.|
|Alte Sendepakete über Suspendgrenze|v95 bestanden: alte Queueeinträge verworfen, frische Echos nach Resume.|
|USB-Steuerzeitüberschreitung|v95 fünf absichtliche Fehler und fünf Erholungen, ohne Neustart bestanden.|
|10/100Full Autonegotiation|v95 je50 exakte IPv4-/IPv6-UDP-Echos und zehn IPv6-API-Abfragen während des Modus; UART-Carrier und Auto-Rückstellung bestanden.|
|Neue Daten-Dauerprüfung|v95:600,95s/560 vollständige Zyklen/347504640Byte UDP/TCP4/6 bytegenau bestanden; API720s und vergleichbarer Idleheap203396→203402B, keine neuen Treiber-/RTL-Fehler. Neue Stunde nach HA2026.10.2-Update bestanden:3600,07s/3253Zyklen/2018629632Byte exakt; alte Stundenabnahmen wurden nicht übernommen.|
|Bereinigtes v96-Betriebsimage|Verschlüsseltes OTA, Boot ab65s und IPv4-/LLA-/GUA-API bestanden. Netzwerkzyklus, gemessener5,125s-USB-Suspend und Neustart mit API-/LLDP-Erholung sowie90s stationäre Nachbeobachtung ohne neue Fehler bestanden. Echter v96-PoE-Kaltstart mit UART-POWERON, neuer Uptime, Dual-Stack-API und90s fehlerfreier Nachbeobachtung bestanden. Neue Stunde nach Update auf HA2026.10.2 bestanden; ESPHome2026.9.1 ist weiterhin aktueller Stable.|
|Reale PAUSE-Frames|Am Switch unabhängig per SNMP gezählt; RTL-TX und Empfangsbremswirkung mit Negativkontrollen praktisch bestätigt.|
|Halbduplex|Registersteuerung geprüft, echte Halbduplex-Gegenstelle fehlt. UniFi bietet am Port nur Full.|
|Reine asymmetrische Pause|Aktueller UniFi-Partner bietet symmetrische Pause; reine asymmetrische Richtungen nicht hardwarequalifiziert.|
|EEE-LPI|EEE-Aushandlung und reproduzierbare OFF/ON-Rohregisterbeobachtung bestätigt; physische LPI-Zeit/Energieeinsparung weiterhin nicht unabhängig qualifiziert.|

Für Halbduplex zuerst den vorhandenen RTL8153-Zusatzadapter unter Linux mit
ethtool auf tatsächlich unterstützte Modi prüfen. Für einen direkten Test
braucht der InLine weiterhin eine passende aktive IEEE802.3af/at-PoE-Einspeisung.
Keine Speisung über UART. Unabhängige LPI-Abnahme benötigt belegte Zähler einer
100BASE-TX-EEE-Gegenstelle oder entsprechende Messtechnik. Temperatur-/Stromsensoren
dieser RTL-Revision sind nicht belegt und werden nicht ersetzt oder erfunden.

Exportprüfung rc.4:20 ASAN/UBSAN-Suiten,53 Ethernet-Schemafälle und30 LLDP-Schemafälle bestanden. Beide tatsächlichen Exportbeispiele bauen mit synthetischen Schlüsseln; diese Images werden niemals geflasht. Hauptbaum, bereinigter Build und Export besitzen bytegleiche Runtimequellen.

Der neue Stundenlauf umfasst416384 exakteUDP-Anfragen und ebenso viele
Antworten, zwei persistenteTCP-Ströme und3700,01s verschlüsselteDual-Stack-API.
Keine neuen Treiber-/RTL-Fehler;3790s stabiler Macpfad ohne aufgezeichneten
Linkverlust. Zusätzliche CPU-/RAM-Stressphasen wurden in diesem neuen Lauf
nicht zugeschaltet. Hostweite UDP-Statistiken enthalten auch anderen Verkehr
und sind kein einzelner Testfluss; daraus wird keine globale Dropfreiheit
aller Macanwendungen abgeleitet.

Der ursprüngliche API-Bericht hat als einzigen Gesamtfehler ein unvollständiges
Nachlast-Messfenster: Der Monitor setzte dessen Beginn auf das verspätete
Erkennen des Datenendes. Die unverändert gespeicherten Zustände enthalten zwei
Messpunkte18,6/28,6s nach dem tatsächlichen Ende, mit denselben durchgehend
verbundenen API-Clients. Separate Neuauswertung: Idlemedian204404→204268B,
Delta-136B. Keine Rohdaten umgeschrieben; künftige Prüfläufe nutzen den
expliziten Endzeitstempel. Das ist eine Korrektur der Prüfauswertung, kein
zusätzlicher Firmware- oder Hardwareerfolg.

v97: Lokale kalibrierte LLDP-Portzuordnung im Betriebsprofil aktiviert,
physischer Port9 bei unverändertem rohem Port-ID-Feld bestätigt. Vorhandenes
HA-Gerät mit38 aktuellen Entitäten nach dem HA2026.10.2-Update geprüft.
Die native Transportlogik ist zum Stunden-Prüfstand quellgleich; Echo- und
Prüfhooks sind im bereinigten Betriebsimage ausgeschlossen. Das aktualisierte
native Exportbeispiel baut mit synthetischer Kalibrierung und Schlüsseln.

Die nachfolgende rc.3-/rc.2-Historie beschreibt frühere Softwarestände.
Insbesondere ihre Stundenabnahme ersetzt keine Abnahme des neuen USB-Sendepfads.

---

# Prüfstatus 0.1.0-rc.3

Lokaler Produktionskandidat vom 20. Oktober 2026, nicht veröffentlicht.
Geprüft werden InLine 33380J POE01 mit RTL8152-Revision 4C10 und das Waveshare
ESP32-S3-LCD-Driver-Board ohne Display oder Akku. Andere Chiprevisionen und
Boards benötigen eigene Abnahmen.

| Prüfung | Ergebnis |
| --- | --- |
| LLDP empfangen und tatsächlich am Switch senden | Bestanden; unabhängiger Spiegelport-Mitschnitt mit vier ESP-Ankündigungen, TTL 120, etwa 30 Sekunden Abstand und keinen Kernel-Drops. |
| UniFi-Portzuordnung | An den physischen Ports 7, 9, 20 und 21 unabhängig bestätigt. Explizite Kalibrierung erforderlich; Original-Port-ID bleibt erhalten. |
| Betriebsimage mit UniFi-Fix | v84: verschlüsseltes OTA, Bootbestätigung ab 65 Sekunden, IPv4-/IPv6-API, 38 HA-Entitäten und 180 Sekunden stationäre Nachbeobachtung bestanden. Nach den abschließenden Paarproben erneut OTA/Boot/API/HA und 90 Sekunden ohne neue Treiber- oder Hardwarefehler bestanden. |
| Gezielte Paket-Wakefilter | Sieben positive und negative Fälle im isolierten VLAN bestanden: Magic, Unicast, Broadcast und Multicast. |
| Externer Link-Wake | Am tatsächlichen Switch bestanden: ohne Filter Timer-Resume nach 180,236 Sekunden, mit Linkfilter Device-Resume nach dem Portwechsel. |
| PHY-Autonegotiation | 10 und 100 Mbit/s Vollduplex sowie automatische Rückstellung bestanden. |
| Erzwungene PHY-Modi | Feste 10/100 Mbit/s Vollduplex mit passend eingestelltem Switch bestanden; je zehn verschlüsselte API-Abfragen über IPv4 und IPv6 während des Festmodus. Registersteuerung und Rückstellung für alle vier Modi geprüft. |
| Halbduplex-Gegenstelle | UniFi bietet am Testport nur Vollduplex an. Halbduplex-Datenverkehr bleibt ungeprüft. |
| Tatsächliche Pauseframes und EEE-LPI | Noch nicht abgenommen. EEE-Aushandlung, Umschaltungen und Registerwerte sind geprüft; sie belegen keine LPI-Energieeinsparung. |

Die Paket-Wake-Proben bestätigen jeweils zuerst den aktiven Empfang eines
markierten Frames. UART erfasst Root-Suspend und Device-Resume unabhängig von
der währenddessen geschlossenen API. Unpassende Klassen bleiben etwa
12,29 Sekunden bis zum Timer im Suspend; passende Klassen wecken nach etwa
1,29 Sekunden ab Suspendbeginn. Kein Neustart, Empfangsfilter und Mac-DHCP
werden wiederhergestellt. Die Zeiten enthalten die geplante Wartezeit bis zur
Paketgabe und sind keine isolierte Hardware-Wake-Latenz.

Ein erster Prüfhelferlauf wertete einen alten Sensorwert aus und brach ab.
Die Rohdaten bleiben erhalten. Der korrigierte Lauf wartet auf aktualisierte
Resume-Zähler. Auch die 78,349 Sekunden des externen Link-Wake-Falls enthalten
die Bedienzeit bis zum Portwechsel und stellen keine Wake-Latenz dar.

Historische rc.3-Exportprüfung: 19 ASan/UBSan-Suiten, 53 Ethernet-Konfigurationsfälle und
30 LLDP-Konfigurationsfälle bestanden. Native- und ECM-Beispiel bauen mit
synthetischen Zugangsdaten. Diese Build-Images werden niemals geflasht.

Die folgenden früheren Prüfstände bleiben als Historie einschließlich ihrer
Fehler erhalten. Die neue gezielte Wake-Abnahme ersetzt die frühere Aussage,
dass Broadcast-/Multicast-Auslösung noch nicht isoliert nachgewiesen sei.

# Prüfstatus 0.1.0-rc.2 – Produktionskandidat in Qualifizierung

20. Oktober 2026, lokaler Quellkandidat, nicht veröffentlicht. Keine pauschale
Vollständigkeitszusage für die RTL-Produktfamilie. InLine33380J POE01,
RTL8152-Revision4C10 am Waveshare ESP32-S3-LCD-Driver-Board ohne Display/Akku.
PoE am USW Pro XG24PoE; UART separat, Versorgungspins offen.

## Ergänzung rc.2: PHY-Anlauf und LLDP

- TX-Anlauffehler durch vorzeitigen IP-Start während PHY-Neuaushandlung in v78
  reproduziert; v79/v80/v81 geben IP erst nach frischem Carrier frei.
- v81: sieben EEE-/Pause-Umschaltungen inklusive exakter Wiederherstellung
  bestanden; Link/API/LLDP erholen sich ohne Neustart/neue RTL-Hardwarefehler.
- LLDP-RX am echten Switch und Identitätsabgleich, eigene Laufzeittexte,
  Deaktivierung35s und Wiederaktivierung bestanden. Port-/Modelltexte stehen
  bereits so im Switch-Frame; unabhängig mit tcpdump bestätigt.
- RX-/TX-only, deaktivierter Start, Retry, TTL/Shutdown, UTF8 und20000 fehlerhafte
  LLDP-Frames unter ASan/UBSan geprüft.21 zusätzliche Schemafälle bestanden.
- v81 LLDP-Datenregression600,08s/558Zyklen/346263552B bytegenau über UDP/TCP4/6
  bestanden. API660s, keine neuen Treiber-/RTL-Fehler. Kein PCAP dieses Laufs.
- v82-Betriebsimage ohne Testdienste installiert: verschlüsseltes OTA, Boot>=65s,
  API4/LLA/GUA und35HA-Entitäten bestanden. Zwölf LLDP-Entitäten und
  eigener Systemname über HA geändert und wiederhergestellt. Netzwerkzyklus,
  5s USB-Suspend und Neustart mit frischem LLDP-Nachbarn bestanden. Kein neuer
  TX-Fehler; missed+13 über die absichtlichen Unterbrechungen getrennt dokumentiert.
- v82-PoE-Kaltstart mit UART-POWERON, neuer Uptime, API4/LLA/GUA und HA
  bestanden;180s Nachbeobachtung ohne neue Fehler. Hardwaretxerr/rxerr/missed0.
- LLDP-Wire-Sendenachweis bestanden:120s auf en7, vier ESP- und vier Switch-
  Frames, TTL120, korrekte Identität, drei Intervalle30,002–30,005s,0Kernel-Drops.
  Pauseframe-Wirkung,
  isolierte BC/MC-Wake-Auslösung und Halbduplex bleiben ohne Abnahme.

## Neuester Stand: v76 integriert, neue Vollmodus-Abnahme vorbereitet

- v70 ohne TX-Aggregation: Daten3600,84s/2044071936B bytegenau bestanden,
  voller IPv4-Test-PCAP210816 Anfragen/Antworten,0Kernel-Drops. Gemeinsame
  Abnahme trotzdem fehlgeschlagen:108 Queue-Abweisungen und Heap-Gate unter
  aktiver Last. Nach Lastende Heap wieder220016B, kein belegter Speicherleck.
- Neue Queue32 mit begrenztem Rückstau: Kurzlast ohne Aufnahmefehler bestanden;
  deterministischer Vollqueue-Faulttest mit erwarteten Timeouts und Erholung.
- Native FIFO/DMA vor MAC-Freigabe explizit03/7A120180/00000101; Rücklesen
  und erneute Initialisierung nach logischer USB-Neuerkennung praktisch geprüft.
- Statischer Multicast-Hashfilter auf Hardware geprüft. Wählbare Wakefilter:
  Magic/Link/Unicast mit negativen Gegenproben; Broadcast/Multicast sofortiges
  Umgebungs-Wake, deshalb keine isolierte positive Zielprobe bestanden.
- Resume-Telemetrie unterscheidet gemessenen Root-Suspend von sehr schnellem
  Device-Resume ohne Rootsample. Timer5s, EEEoff/on und Sessiongrenze bestanden.
- Integrierte und exportierte16 Sanitizer-Suiten und53 Schemafälle bestanden;
  beide exportierten Beispielprofile bauen mit synthetischen Secrets.
- Neue Vollmodus-Stunde v76 bestanden:3600,39s/3358Zyklen/2083786752Byte
  bytegenau über IPv4/IPv6 UDP/TCP. API3700s, drei5min CPU-/48KiB-Lastphasen,
  keine neuen Treiber-/Hardwarefehler. Tatsächliche Offload-/Aggregations-/TSO-
  Zähler steigen. Vergleichbarer Idleheap215780→215716B (Delta-64B).
  Mac en24/WLANoff3790s ohne aufgezeichneten Linkverlust/Routenfehler.
  Kein administrativer PCAP in diesem Lauf; Ursache alter Verluste nicht durch
  einen isolierten Einzelfix nachgewiesen. Alte Fehler bleiben erhalten.
- Betriebsprofilv77 ohne Echo/Fehlerinjektion/Stress gebaut,49Quelldateien
  bytegleich zur integrierten Quelle. Verschlüsseltes OTA, Boot>=65s und API4/LLA/GUA bestanden. HA verwendet den vorhandenen Eintrag:15Sensoren/7Buttons verfügbar. Nutzerbestätigter PoE-Kaltstart und180s Nachbeobachtung bestanden. Nichts veröffentlicht.

Ältere Abschnitte unten sind historische Prüfungen und behalten ihre Fehler.

## Aktueller nativer Datenpfad

- Vendor-Konfiguration1, echte eigene TX-/RX-Descriptors; API/DHCP/IPv4/IPv6/OTA
  über den InLine ohne WLAN praktisch geprüft.
- Grenzgrößen einschließlich64-Byte-USB-Grenzen und MTU1500:80/80 Antworten.
- TCP-/UDP-Checksum-Offload IPv4/IPv6 mit Pseudoheaderseed bestanden; Padding und
  nicht unterstützte Header behalten Softwarepfad. Nullseed-Negativkontrolle zeigt
  Fehler bei UDP6 und ist nicht als Betriebsmodus vorgesehen.
- RX-/TX-Aggregation durch Transfers mit mehreren Records tatsächlich belegt,
  keine Parser-/Hardwarechecksum-Fehler in den bestandenen Serien.
- UDP-Last Fenster4:3000 Pakete verlustfrei, etwa8,4/8,2Mbit/s beidseitige Payload.
  Fenster8 bei warmem IPv6-Nachbarn:6000 Pakete/16,8MB verlustfrei. Erster kalter
  IPv6-Achterburst verliert fünf Pakete; Treiberfehlerzähler bleiben0. NDP-Anlauf
  als Ursache plausibel, Ort ohne Mitschnitt noch nicht bewiesen.
- Hardware-TSO durch steigende IPv4-/IPv6-Zähler und zehn bytegenaue TCP-Echos
  mit8192 bis524288Bytes bestätigt. Zusätzliche Serie mit beobachtendem TCP-
  Sequenzverlauf: keine ausgehende Payload-Wiederholung, TSO-Zähler30/32.
  Kein neuer lwIP-GSO-Vertrag/CPU-Gewinn behauptet.
- USB-Suspend mit erhaltenem Datenport: mehrere5-/20-s-Timerzyklen ohne Neustart.
  Drei gültige Magic-Proben nach etwa4,1s; ohne Paket/falsche MAC erst Timer.
  Unbefristetes Suspend mit manuellem Resume nach60s und Magic-Wake bestanden.
  NIC-/Hub-Remote-Wake wird gesetzt/rückgelesen. Kein ESP32-Deep-Sleep-Nachweis.
- LED-Register geprüft, Nutzer bestätigt Statuswechsel beider LEDs über Off/Link/
  Activity. Keine belegten RTL-Temperatur-/Stromsensoren, keine Ersatzwerte.
- Sanitizerprüfungen beider Kontrolltransporte,100000 fehlerhafter Frames,
  20000 Aggregatstreams und50000 TSO-Eingaben bestanden; zusätzliche Transfer-/
  Zeitüberlauf- und10000 Testmarker-Grenzfuzzfälle bestanden;36 Schemafälle bestanden.

## Finale native Lebenszyklusabnahme

- Drei logische USB-Neuanmeldungen bei erhaltener Versorgung ohne Neustart
  bestanden. Früher Start im unbestätigten OTA-Image löste korrekt Rollback aus;
  der gültige Wiederholungstest wartete mindestens65s.
- Bereinigtes natives Betriebsimage: verschlüsselte IPv4-/IPv6-API (Link-local
  und global),21 HA-Entitäten mit14 verfügbaren Sensoren, drei Interfacezyklen,
  zwei Software-Neustarts, API-Sicherheitsprüfungen, IPv6-OTA und5-s-Timersuspend
  bestanden (v35). v37 ergänzt den in v36 geprüften beobachtenden TCP-Zähler;
  IPv6-OTA,5-s-Suspend/Resume sowie finale IPv4-/IPv6-API-/HA-Abnahme bestanden.
  Vier Hardware-Kaltstarts bestanden: drei Board-Kaltstarts und ein vollständiger
  Adapter-/Board-Kaltstart. Jeweils POWERON, native Initialisierung, DHCP und
  verschlüsselte API ohne Absturz. Danach IPv4-/IPv6-API und HA erneut geprüft.

## Zuvor separat im ECM-Pfad bestanden

Dual Stack/SLAAC mit globaler/link-lokaler verschlüsselter API und IPv6-OTA;
statische IPv4/DNS; feste flüchtige MAC mit DHCP/statischer IPv4, Neustarts und
Werks-MAC-Wiederherstellung; A-/AAAA-DNS-Anfragen;14 PHY/EEE/Pause/Wake/Filter-
Operationen; EEE tatsächlich ausgehandelt sowie an/aus/an gegengeprüft.
Software-/PoE-Neustarts, Linkverlust bei erhaltener Versorgung und gestörte OTA-
Übertragung mit Wiederherstellung sind im früheren ECM-Prüfstand dokumentiert.
Diese Ergebnisse gelten nicht automatisch als erneute Abnahme jeder nativen Variante.

## Ergänzende Langzeit-/Fehlerfallserie

- 30min unverändertes natives v37-Betriebsimage: verschlüsselte API über IPv4
  und globales IPv6 stabil, keine Neustarts. Erstes/letztes Heap-Median286532B,
  Minimum284640B.100 zusätzliche saubere/abrupte API-Anmeldungen bestanden.
- v48: DHCP-T1/ACK, T2/Rebind, Lease-Ablauf mit tatsächlichem IPv4-Verlust,
  automatische Wiederherstellung nach210s gerätebegrenzter DHCP-Sendesperre.
  IPv6-API blieb verfügbar. Passender lokaler DHCP-NAK entfernt Lease; falsche
  XID wird ignoriert. DNS positive Kontrolle, Ausfall und Wiederherstellung.
  Kein LAN-DHCP-Server geändert; NAK nur im eigenen Stack injiziert.
- Sechs simulierte Control-Wartetimeouts nach echter USB-Submit-Operation,
  Freigabe ausschließlich nach echtem Callback, sechs Erholungen; ein
  Konfigurationsretry ohne Neustart. Dauerhaft blockierter Controller nicht bewiesen.
- 200 bytegenaue UDP-Echos1473–4096B über IPv4/IPv6 mit expliziter Reassemblierung.
- Gezielte UDP-Überlast50000Pakete:4665 Antworten, keine beschädigt/doppelt;
  danach600 normale UDP-Echos verlustfrei und API verfügbar. Keine Behauptung
  verlustfreier Überlastübertragung.
- Falscher OTA-Schlüssel sowie Abbrüche nach4096/16384Clientbytes vor Commit
  abgewiesen; alte Firmware ohne Neustart erreichbar. Danach vollständiges OTA
  erfolgreich. Während eines Flash-Abbruchs einmal TX-Timeout, danach erholt.
- v50: Fünf frische IPv6-Nachbarn, kalt jeweils nur letztes Paket bereits am
  USB-RX; warm alle8 Pakete im USB-RX und in der Anwendung. Verlust vor RX-Parser
  eingegrenzt; macOS-NDP-Puffer plausibel, kein Kabelmitschnitt zur finalen
  Trennung Mac/Adaptereingang. Drei logische USB-Zyklen und TSO-Regression bestanden.
- Timer-/Magic-/Negativ-/falscheMAC-/unbefristete Wake-Proben erneut bestanden.
  Manuelle60-s-Probe nach Bereinigung überschneidender Test-Rettungsfristen
  mit59904ms bestanden. Erster überlappender40-s-Lauf separat failed.
- Zusätzlicher Codebefund: Nicht geplante Retry-Deadline0 konnte nach24,86Tagen
  Wiederverbindungen verhindern. Korrigiert; lokale Grenzwerttests für24,9-/
 49,7-Tage-Grenzen bestanden. Kein tatsächlich mehrwöchiger Lauf behauptet.
- Aktuelle Sanitizersuite umfasst zusätzlich Transferbesitz und Zeitüberlauf;
 36Schemafälle bestanden. Native Bootprofile bestanden: Start mit90-s-DHCP-Sperre und IPv6-API davor;
  IPv6-only mit festerMAC und DHCPINIT0; festeMAC mit DHCP; statischeIPv4
  mit beiden DNS-Einträgen und DHCPSTOPPED2. Sensorverschiebung ohneIPv4
  gefunden/korrigiert und im v54-Profil explizit gegengeprüft.
- Erster Mischlastanlauf mit zehn lwIP-Sockets unvollständig: zweite API
  zurückgewiesen. A/B-Gegenprobe und16-Socket-Testprofil v55 beheben die
  Kapazitätsgrenze der zusätzlichen Echo-/Beobachtungsdienste. Darin100 weitere
  saubere/abrupte API-Anmeldungen unter Last bestanden; beide Dauer-APIs stabil.
  Dieser erste v55-Datenlauf endete nach262s/147MB mit einem einzelnen
  ungeklärten UDP-Timeout, obwohl Treiber-/Hardwarefehlerzähler unverändert
  und beide APIs stabil blieben. Kein bestandener Stundenlauf.
- Acht gezielte UDP-Checksumme-null/FFFF-Grenzfälle bestanden. Instrumentierte
  Wiederholung v56 endete nach2755,88s/2495Zyklen/1548257280B ebenfalls mit
  IPv4-UDP-Timeout: vier neue Pakete ohne Echo. Keine neue Burstkennung am
  USB-RX/App-RX; Fragmentpakete sind vom Markerparser ausgenommen. Treiber-/
  Hardwarefehlerdeltas0, APIs weiter verbunden. Kein bestandener Stundenlauf
  und ohne Kabelmitschnitt keine abschließende Verlustursache. Eine zehnminütige
  Gegenprobe mit verbundenen UDP-Sockets besteht600,84s/542Zyklen/336334848B. Der61min-API-Lauf ist
  bestanden: beideFamilien unterbrechungsfrei, keineNeustarts, Heapminimum152952B,
  eingeschwungenerMedian216476B→final215510B (−966B).

Zusätzliche gezielte Paketdiagnose am20.Oktober:570,63s/453Zyklen und
281106432B bytegenau bestanden, beideAPIs580s stabil. Der aufIPv4-TestUDP/ARP
begrenzte Mac-Mitschnitt enthält451vollständigeZyklen mit28864Anfragen und
28864passendenAntworten, keineDuplikate/fehlendenAntworten imerfasstenFenster.
Dieletzten2Datenzyklen liegen nachCaptureende undsindnurvomClient geprüft.
Der frühere UDP-Verlust ist damit weder erklärt noch behoben.

## Noch offen / Grenzen

- Physischer USB-Hotplug bei unabhängig versorgtem Board benötigt einen geeigneten
  Aufbau. Logischer Rootport-Neustart ist ein eigener, davon getrennter Test.
- Strikter instrumentierter UDP-Stundenlauf fehlgeschlagen; Verlustursache offen.
  Ein weiterer kalter IPv6-Antwortburst zeigt8App-sendto-Erfolge, aber nur die
  letzten3amUSB-TX/Mac; warmerBurst8/8. Passt zurSDK-Nachbarwarteschlange3,
  erklärt den separatenIPv4-Verlust nicht.
- Native Kontrollserie v61: alle14Operationen erfolgreich, Wake-/Promisc-/EEE-/
  Pause-Rücklesewerte undAutoneg10/100full bestehen. Carrier-down-Registerzugriff
  undAuto-Restore amBoard bestanden. Gesamtfall failed, da erzwungenes10/100half
  trotz korrekterBMCR-Bits keinenCarrier amaktuellenSwitch ergibt. Keine
  Halbduplex-Hardwareabnahme behauptet; automatischeAuto-Rückkehr bestätigt.
- NativeIPv4-onlyv62 bestanden, einschließlichnegativerIPv6API-Kontrolle.
  Feste MAC und Magic-Wake über IPv4 sowie IPv6 bestanden; alte Werks-MAC
  weckt nicht vorzeitig (Timer19996ms), gültige neue MAC weckt nach3844/3853ms.
  Bereinigtes v63 bestätigt: IPv6-OTA, Dual-Stack-API, HA mit14Sensoren und
 7Buttons sowie5-s-Timersuspend/Resume (5124ms, ohneNeustart) bestanden.
  PHY-Wiederholung an Port9 bestanden: Ausfall und automatischeRückkehr,
  PHY100/full/EEE, IPv6API und kontinuierlicheUptime ohneReset nachgewiesen.
  Abschließender vollständiger Adapter-/Board-Kaltstart desv63-Betriebsimages
  bestanden: UARTPOWERON, erneuteUSB-Erkennung, DHCP, IPv4-/IPv6-API undHA.
  NachKaltstart alleNativeErrors0.
- Selektiver Multicastfilter, tatsächliche Pause-Frames/LPI-
  Stromersparnis, weitere Wakefilter-Auslöser und echte Displaylast offen.
- Control-Timeout quarantiniert noch laufende URB; keine behauptete automatische
  Wiederherstellung eines dauerhaft blockierten Controllers.
- Keine EEPROM-/OTP-/eFuse-Programmierung, Firmwarepatch-Uploads, USB-C-PD-Steuerung
  oder Unterstützung ungetesteter RTL8152-/RTL8153-Revisionen.

Beide alpha.4-Exportbeispiele nach Sensorslot- und Carrier-Guardkorrektur mit
synthetischen Nichtnullwerten kompiliert;36 Schemafälle gegen den Export und
exportierte Sanitizersuite bestanden. Der ECM-Pfad mit
USB1.6.0 hat nur Buildabnahme; frühere ECM-Hardwareabnahme ist separat gebunden.

BUILD_INFO.json bindet Versionsstände, Quellen und konkrete Prüfrevisionen.

## Produktionskandidat mit abgesicherter TX-Warteschlange

Neue Generation pro Sendepaket verhindert die Übernahme wartender Pakete aus
einer alten USB-/Link-Sitzung. Zustands-/Lock-Verwerfungen werden mit tx_discard
und dessen Teilmenge tx_stale gezählt. Hardware-Grenzprobe v65 bestanden: zwei
alte Antworten nach Suspend/Resume nicht gesendet, zwei frische danach bytegenau;
9stale/9discard, kein USB-Fehler. Absichtliche8s-Sperre füllt die kleine Queue und
erzeugt weitere dokumentierte Verwerfungen, kein verlustfreier Überlasttest.

Control-Recovery6/6 undKonfigretry1 sowieMagicWake4098ms erneut bestanden.
IsolierteTSO-Serie10bytegenaueIPv4-/IPv6-Echos bis524288B bestanden, TSO57/59,
keine zusätzlichenTCP-Wiederholungen. Erste irrtümlich überlappte Probe scheiterte
an veraltetem Statistikwert; Helper wartet jetzt eine frische Nachmessung ab.

3600s-Datenlauf erneut failed nach1453,03s/1381Zyklen/856971264B: IPv4-
Sequenz4 eines64er-Bursts fehlt bereits vorUSB-RX. MacARP meldete vorweiterer
Diagnose keinenEintrag. API-Dauerlauf3660s abgeschlossen und bestanden, einschließlich drei CPU/RAM-Lastphasen; keine neuen Treiber-/Hardwarefehler. Capture bestätigt den ausgehenden Frame
mit korrekter Ziel-MAC, aber keine Antwort. Der Mac verwendet WLAN; damit bleibt
der Verlust zwischen Mac-Capture und Board-RX unlokalisiert. Kernel-Dropstatistik
fehlt nach erzwungenem Captureende. Kabelgebundene Gegenprobe ist vorbereitet. Direkte60s-Recoveryprobe ohneNeustart bestand48Zyklen/
29786112B. KeineStundenfreigabe undkeineUrsachenzusage. Das bereinigte Betriebsimagev66 enthält
dieselben Treiberquellen, keine Testdienste; gebaut, noch nicht geflasht.
Der konkrete Betriebsumfang steht in docs/PRODUCTION.md.

Separate Recovery-Datenprobe ohne Neustart:1800,99s/1584Zyklen/982941696B
bytegenau bestanden. Der ursprüngliche Datenfehler bleibt erhalten. Beide
Messungen verwendeten die unveränderten eingefrorenen Produktionsquellen.
Die kabelgebundene Mac-Gegenprobe steht weiter aus; keine Produktionsfreigabe.

## Kabelgebundene Gegenprobe (noch laufend)

Erster Lauf nach531,73s abgebrochen: macOS meldete bei IPv6 „No route to host“.
Das lokale configd-Protokoll belegt einen0,207s-Linkverlust am zusätzlichen
Mac-Ethernetanschluss mit anschließender Adressneukonfiguration. Der Nutzer
bestätigt unveränderte Verkabelung. Das Testboard lief weiter, neue IPv6-API-
Verbindungen funktionierten ohne Reset. Dieser Abbruch ist deshalb kein
Beleg desselben früheren RTL-/UDP-Fehlermusters; er bleibt separat dokumentiert.

Eine Wiederholung mit unverändertem Treiber läuft. Zusätzlich zum Datenverkehr
werden dauerhafte IPv4-/link-lokale IPv6-API und frische globale IPv6-Verbindungen
geprüft. Ein direkter Host-Linkereignisbeobachter ergänzt die Routenabfragen.
Noch keine bestandene kabelgebundene Stunde und keine Produktionsfreigabe.

Auch die Wiederholung wurde durch einen zweiten direkt protokollierten
Mac-Linkausfall unterbrochen (Daten814,30s/745Zyklen/462484480B). Keine neue
Gerätefehlerzählung. Beide kabelgebundenen Datenläufe bleiben fehlgeschlagen;
eine gültige Stundenabnahme erfordert zunächst einen stabilen Mac-Netzpfad.
Der Nutzer wurde zum alleinigen Mac-LAN-Kabelvergleich aufgefordert.

## Ergänzung nach Mac-Kabeltausch: weiterhin keine strenge Stundenfreigabe

- WLAN aus, Mac en24 verkabelt. Nach Austausch ausschließlich seines LAN-Kabels
  ein IPv4-Antwortverlust nach93,72s: Board zeigt alle64 Marker bis USB-TX.
  Kein aufgezeichneter Mac-Linkflap.550,50s anschließende Vollpaketkontrolle
  bestanden, Ursache damit nicht behoben.
- Isolierter TX-Modus0-Lauf (normale Softwarechecksummen, Aggregation/TSO/EEE
  weiter aktiv) nach2479,22s/1426010112B fehlgeschlagen: E00008FA/57 fehlt.
  Voll-PCAP bestätigt gültige Anfrage und fehlende Antwort am Mac; alle64
  Boardmarker bis erfolgreichem USB-TX,0 Kernel-Drops, keine neuen Fehlerzähler.
  TSO ist weiterhin Hardwarechecksumfunktion; kein Alles-Software-Test.
- Daraus keine Behauptung eines behobenen Offloadfehlers. Neue Diagnose ohne
  TX-Aggregation läuft noch, zeigt aber bereits Queue-Abweisungen und ist
  deshalb unabhängig vom späteren Datenergebnis keine strenge Produktionsabnahme.
- Wake-/Multicast-Auswahl, lesende FIFO/DMA/EEE-Snapshots, TX-Integritätsmessung
  und begrenzte Queue-Rückstau-Behandlung werden isoliert vorbereitet. Ihre
  Softwarebuilds sind kein Nachweis von Hardwarefunktionen und gehören noch
  nicht zu diesem exportierten Quellstand. Es wurde nichts veröffentlicht.


## Abschließender v77-PoE-Kaltstart

Nutzerbestätigung plus neue API-Laufzeit41,474s (vorher>997s), richtige Revision/
MAC, Boot>=65s und API4/LLA/GUA bestanden. HA15Sensoren/7Buttons verfügbar.
180s Nachbeobachtung ohne neue Treiber-/Hardwarefehler, Aggregation/TSO aktiv.
Der frühere API-Beobachter war bereits abgelaufen; UART-Prozess ebenfalls beendet,
daher kein neuer UART-POWERON-/Resetursachenbeleg. Nicht als solcher behauptet.
Hardware-Tallytxerr beim ersten Lesen1, während180s konstant; Ursache nicht
isoliert. Andere Hardwarefehlerbaselines0 und alle Fehlerdifferenzen0.
Spezialfälle EEE-LPI/Pause-Framewirkung/erzwungener Halbduplex sowie isoliertes
Broadcast-/Multicast-Wake bleiben offen. Kein stabiles1.0-Release.

## Adapter replacement candidate (not released)

Both measured adapters already share one native YAML. Local reconnect changes add strict factory-MAC parsing, per-device data/control binding and pending-transfer fences. No boards were connected for this change. Cold replacement in both directions, fixed-MAC replacement and separately powered live USB replacement remain unqualified. See [the test plan](docs/ADAPTER_SWITCHING.de.md).
