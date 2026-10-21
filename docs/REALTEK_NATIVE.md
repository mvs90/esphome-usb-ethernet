# Nativer RTL8152-Pfad (experimentell)

Dieses Backend ist für das gemessene InLine-Exemplar 33380J POE01 mit
USB0bda:8152, bcdDevice2000 und maskierter TCR-Revision4C10 geschrieben.
Es ist keine pauschale Unterstützung aller RTL8152-/RTL8153-Revisionen.
Die Hardwareprüfung erfolgt am Waveshare ESP32-S3-LCD-Driver-Board ohne Display/Akku.

## Einbindung

Mit den lokalen externen Komponenten `[ethernet, logger, ethernet_info]` lässt sich
`ethernet.type: USB_RTL8152` statt `USB_ECM` wählen. ESPHome2026.9.1,
ESP-IDF5.5.5, Espressif USB1.6.0, usb_host_cdc_acm2.4.1, CDC3.1.0, ECM0.4.0 und
iot_eth1.1.0 sind gebunden. Der native Pfad installiert ausschließlich CDC-ACM;
CDC3.1.0 wird nur vom ECM-Pfad verwendet.
Die verwaltete USB-Komponente ersetzt die in IDF gebündelte Bibliothek. Es braucht
keinen veränderten ESPHome- oder IDF-Checkout. Die Ethernet-/Logger-Überlagerung ist
versionsabhängig und noch kein Bestandteil des offiziellen ESPHome-Projekts.

Genau eine Stelle installiert den Host. UART0 bleibt die Konsole; WLAN und eine
zweite `usb_host`-Konfiguration werden abgelehnt. Konfiguration1 liefert natives
Vendor-Bulk, Konfiguration2 bleibt der getrennt geprüfte ECM-Pfad.

## Datenpfad

Eigene Implementierung von8-Byte-TX- und24-Byte-RX-Deskriptoren, RX-FCS-Abtrennung,
Längen-/Grenzprüfung,8-Byte-RX-Recordausrichtung und Hardware-RX-Fehlerflags.
Optionale RX-/TX-Aggregation ist implementiert und am Gerät nachgewiesen.
`realtek.rx_aggregation` und `realtek.tx_aggregation` aktivieren sie (Standardfalse).
RX puffert Teilrecords über volle4096-Byte-Transfers; kurze Transfers beenden ein
Aggregat. TX richtet Records auf4Bytes aus, bis8192Bytes jeTransfer. Zähler
`rx_multi`/`tx_multi` belegen Transfers mit mehreren Records. Softwarechecksumprüfung bleibt
aktiv. Begrenzte TX-Warteschlange mit separater Aufgabe vermeidet USB-Warten im
TCP/IP-Thread. Bei Überlast werden Frames mit Fehler an den Stack zurückgewiesen.

Ethernet-Padding wird in der deklarierten Framelänge geführt. Trifft die gesamte
Übertragung genau eine64-Byte-USB-Grenze, wird ein zusätzliches Ethernet-Paddingbyte
angefügt. Dadurch entsteht ein kurzes USB-Endpaket. Für gepaddete Frames bleiben
Softwarechecksummen erhalten. Unbekannte Header, IPv4-Fragmente und IPv6-
Erweiterungsheader werden ebenfalls nicht in den experimentellen Offload gegeben.

TX-Checksum-Modi: 0 Software, 1 Nullseed-Gegenprobe, 2 Pseudoheaderseed.
`realtek.tx_checksum_offload: true` wählt den am Gerät geprüften Modus 2;
der Standard bleibt false. Laufzeitaktion: `ethernet.realtek.tx_checksum_offload`
mit `enabled: true` oder `false`. Modus1 ist nicht freigegeben. Hardwarechecksummen allein
bedeuten noch keine eingesparte CPU-Zeit, solange lwIP weiterhin selbst rechnet.
Der reguläre lwIP-Vertrag liefert bereits segmentierte Ethernetframes.
`realtek.tcp_segmentation: true` (Standardfalse) fasst streng kompatible
TCP-Segmente gleicher Verbindung erneut für Hardware-TSO zusammen. Laufzeitaktion:
`ethernet.realtek.tcp_segmentation` mit `enabled`. IPv4/IPv6-Hardwarepfade wurden
mit bytegenauem TCP-Echo bis524288Bytes und steigendenTSO-Zählern geprüft.
IP-/TCP-Header, Sequenz, IPv4-ID, MSS und Flags müssen passen; sonst bleibt
die normale Übertragung erhalten. IPv6-Payload-Länge wird für den TSO-Descriptor
auf0 gesetzt. Dies ist kein neuer GSO-Vertrag und belegt keine CPU-Ersparnis. Die zusätzliche
Testserie zeigt steigende TSO-Zähler ohne ausgehende TCP-Payload-Wiederholungen.
`tcp_repeat` beobachtet hierfür acht aktuelle TCP-Verbindungen vor Aggregation;
SYN setzt den Verlauf zurück. Der Wert ist eine Treiberdiagnose, kein RTL-Sensor
und keine vollständige globale TCP-MIB.
Kein EEPROM-/OTP-/eFuse- oder Firmwarepatch-Schreibpfad.

## IP-Konfiguration und Fehlerbehandlung

`ethernet.enable_ipv4` ist standardmäßig `true`. Für expliziten IPv6-only-Betrieb:

```yaml
ethernet:
  type: USB_RTL8152
  enable_ipv4: false
network:
  enable_ipv6: true
  min_ipv6_addr_count: 1
```

Die übrigen externen Komponenten und Boardoptionen entsprechen dem vollständigen
Beispiel. IPv6-only verbietet `manual_ip` und startet keinen DHCPv4-Client.
Im Dual-Stack-Betrieb bleibt der Netzwerkstatus bei entfallener IPv4 über eine
bevorzugte IPv6-Adresse verbunden; tatsächliche IPv4 und IPv6-Mindestanzahl
werden getrennt berücksichtigt. Feste `mac_address` gilt vor DHCP und SLAAC;
entfernte Option stellt die Deskriptor-Werksadresse wieder her, ohne EEPROM-Schreibzugriff.

Transiente USB-Fehler bei der Adapterkonfiguration lösen eine Wiederholung nach
500ms aus. Unbekannte Hardwareprofile bleiben abgelehnt. Nach einem Control-
Timeout gehört die Transferallokation weiterhin dem USB-Controller. Erst dessen
tatsächlicher Abschlusscallback erlaubt Wiederverwendung; niemals wird eine noch
laufende URB freigegeben. Ein dauerhaft blockierter Controller bleibt eine offene
Wiederherstellungsgrenze.

`Native Errors` unterscheidet TX-Warteschlange/USB, RX-Allokation, RX-Stack-
Ablehnung, Control-Timeout, spätere Control-Erholung und Konfigurationsretry.
Ein absichtlich deaktiviertes Netif kann empfangene Frames zurückweisen. Solche
Zählerdifferenzen sind vom RX-Parser-/Hardwarechecksumfehler zu unterscheiden.

Das native Beispiel aktiviert IPv4-/IPv6-Reassemblierung ausdrücklich im SDK.
Diese IP-Stack-Funktion ist von den RTL-Deskriptoren getrennt; fragmentierte
Frames behalten Softwarechecksummen. Große UDP-Datagramme sind kein Jumbo-
Ethernetmodus. Den jeweils bestandenen Hardwareumfang nennt TEST_STATUS.md.

## LED-Aktion

```yaml
- ethernet.realtek.leds:
    led0: 3
    led1: 8
    period: 1
    duty: 2
```

Bits pro LED:1 Link10,2 Link100,8 Aktivität;0 schaltet die ausgewählten Funktionen
aus. Kombinationen sind erlaubt, andere Bits werden abgelehnt. LEDSEL DD90 und
LEDFEATURE DD92 werden rückgelesen, obere Options-/Polaritätsbits bleiben erhalten.
Perioden0/1/2/3 entsprechen240/160/80ms bzw. geschwindigkeitsabhängig, Duty0/1/2/3
entspricht12,5/25/50/75%. Beide LEDs des InLine wurden über diese Modi optisch geprüft; der Nutzer
bestätigt die Zustandswechsel. Andere Adapterbeschaltungen brauchen eine eigene Prüfung. Es gibt keinen behaupteten beliebigen GPIO-Ausgang.

## Asynchroner USB-Sendepfad im neuen Kandidaten

Der neue Kandidat übergibt genau einen nativen OUT-Transfer an die öffentliche
USB-Host-API und behält dessen Puffer bis zum tatsächlichen Completion-Callback.
Eine Software-Wartefrist beendet oder überschreibt diesen Transfer nicht.
Wiederholte Ethernet-PAUSE-Frames dürfen die Übertragung länger blockieren;
die bestehende Queue mit32 Einträgen und5ms Zulassungsbudget begrenzt den
Rückstau. Queue-Abweisungen bei absichtlicher Sättigung sind von USB-Fehlern
getrennt. `usb_pending` und `usb_waits` zeigen laufende Übertragung und kurze
Software-Warteintervalle, keine automatisch abgebrochenen USB-Transfers.
Die neue Produktionsabnahme ist in TEST_STATUS.md getrennt ausgewiesen.

Vor USB-Suspend werden neue Sendepakete gesperrt und bereits eingereichte
OUT-Daten vollständig abgearbeitet. Dauert dies länger als10s, wird Suspend
verweigert; der vorhandene Transfer behält seinen Puffer, das Netz wird logisch
wieder freigegeben. `reason=drain-timeout`, `draining=0` und Schlafzeit0
unterscheiden diesen Fall von echtem Suspend/Resume. Die konfigurierte maximale
Schlafdauer beginnt erst nach dem Drain. Die Suspend-Aktion bestätigt zunächst
die Annahme des Auftrags; ihre tatsächliche Ausführung ist am Status und den
Suspend-/Resume-Zählern zu erkennen.

## USB-Suspend-Aktion

```yaml
- ethernet.realtek.usb_suspend:
    maximum_duration: 5s
    remote_wake: false
```

Experimentell, nur im nativen Backend, bei bestehender Verbindung und frühestens
65s nach Start. Dauer1s–24h;0s erlaubt unbefristetes Suspend nur mit
`remote_wake: true`. `ethernet.realtek.usb_resume` fordert manuelles Resume an. Der native Datenport und seine Transfers bleiben über globalen USB-Suspend
erhalten. CDC-ACM behandelt das Anhalten und Wiederaufnehmen des Pollings;
ein eigener Client hält das Gerätehandle und beobachtet Suspend/Resume. Der Timer ist
ein Wiederherstellungsweg, kein Nachweis für Remote-Wake. Ein positiver Wake-Test
muss ein Resume durch das Gerät vor Timerablauf und einen negativen Gegentest
belegen. Kein ESP32-Deep-Sleep und keine gemessene Leistungsersparnis.

Mit `remote_wake: true` wählt `wake_filters` die NIC-Auslöser als Bitmaske:
1 Magic,2 Link,4 Unicast,8 Broadcast,16 Multicast. Ohne Angabe gilt1. Der Wert0
ist als endliche Negativprobe zulässig; unbefristetes Suspend ohne Wakefilter
wird abgewiesen. Die Filter werden gesetzt und vor Suspend zurückgelesen.
Magic/Unicast/Broadcast/Multicast sind mit gezielten positiven und negativen
Proben im isolierten Test-VLAN praktisch geprüft: passende Klassen wecken nach
etwa1,29s; unpassende Klassen bzw. Maske0 bleiben biszum12,29s-Timer imSuspend.
Vor jeder Probe ist aktiver Empfang eines markierten Frames nachgewiesen,
Root-Suspend undDevice-Resume werden unabhängig überUART beobachtet. Externer Link-Wake wurde mit Switch-Port9/1Gbps20s→Auto
positiv geprüft; mit Maske0 bleibt derselbe Linkwechsel bis zum180s-Timer
imSuspend. Link-Wake meint LINK_ON/Linkwiederkehr, entsprechend WAKE_PHY
im referenzierten Linux-Treiber. In einem
normalen LAN können Broadcast-/Multicastfilter sofort durch Umgebungspakete
wecken. Solche frühen Wake-Ereignisse gelten nicht alsgezielter Positivnachweis. Falls der Root-Zustand
vor seiner250-ms-Abtastung bereits wieder aktiv ist, meldet die Diagnose
`reason=device-unsampled`, `suspended_ms=0` und erhöht `unsampled_resumes`.
Das ist keine gemessene Root-Suspenddauer.

Im Test können API und HA während Suspend erwartungsgemäß getrennt werden.
Mit der aktuellen Datenbasis wurden mehrere Timer-Rückkehrtests über 5 und
20 Sekunden ohne Neustart bestanden. Wiederholtes gültiges Magic-Wake kehrt
nach etwa4,1s mit `reason=device` zurück; ohne Paket und mit falscher Ziel-MAC
erst über den20-s-Timer. Unbefristetes Suspend wurde mit Magic-Wake sowie
manuellem Resume nach60s geprüft. NIC und der gemessene Adapterhub werden mit
DEVICE_REMOTE_WAKEUP aktiviert und über GET_STATUS bestätigt.
USB1.6.0 liefert noch kein verwendbares öffentliches Parent-Handle; deshalb
wird genau ein Hub05e3:0608/bcd8536 aus der Adressliste ausgewählt, keine feste
USB-Adresse vorausgesetzt. Andere Hubtopologien sind nicht freigegeben.
Frühere Fehler mit Schließen/Neuöffnen sind in RESULTS dokumentiert.

## Hardwarewerte

PHY-Modus, EEE-Advertisement/-Aushandlung, Wakefilter, MAC, Registerdiagnose und
13 Tally-Zähler stammen aus dem RTL. Für Temperatur und Stromaufnahme wurde keine
belegte Messschnittstelle dieser Revision gefunden. Es werden weder Fantasiewerte
noch ESP32-/Switch-Werte als RTL-Sensoren ausgegeben.

## Herkunft und Prüfbelege

Protokoll-/Registerfakten: Realtek RTL8152B-Datenblatt und
[Linux r8152 v6.17](https://github.com/torvalds/linux/blob/v6.17/drivers/net/usb/r8152.c).
Keine Linux-Implementierung kopiert. Neuer Runtimecode GPL-3.0-only, Espressif-
Abhängigkeiten behalten ihre eigenen Lizenzen. Exakte Builds, Fehler und erfolgreiche
Hardwaretests stehen in [RESULTS.md](../TEST_STATUS.md). Test-UDP-Echo gehört ausschließlich
zur lokalen Testfirmware und wird nicht als Produktionsdienst veröffentlicht.

## Sendewarteschlange bei Link-/USB-Wechseln

Jedes angenommene Paket trägt die Generation der aktuellen Verbindung.
USB-Trennung, PHY-Linkdown und Suspend wechseln diese Generation. Wartende
Pakete einer früheren Generation werden verworfen und nicht mit neuen Paketen
in einem Aggregat oder TSO-Transfer vermischt. Bereits eingereichte USB-Transfers
behalten ihren normalen Completion-/Ownership-Pfad.

`Native Errors` enthält zusätzlich `tx_discard` für angenommene, später ohne
USB-Versand verworfene Frames (Zustandswechsel oder Lock-Timeout) und `tx_stale`
als Teilmenge davon für ungültige Generationen. `tx_queue_drop` bleibt der Zähler
für fehlgeschlagene Allokation/Queue-Aufnahme. Diese Zähler dürfen während
absichtlich provoziertem Verbindungsabbruch steigen; im stationären Lasttest
werden ihre Differenzen separat geprüft. Die Fehlertextzeile bleibt auch bei
32-Bit-Maximalwerten einschließlich `config_retry` unter dem HA-Limit255Zeichen.


## Native FIFO/DMA und begrenzter Rückstau

Vor MAC-Freigabe setzt der native RTL8152B-Pfad USB_TX_AGG03,
USB_RX_BUF_TH7A120180 und USB_TX_DMA00000101 mit korrekten Byte-Lanes.
Alle drei Werte werden vollständig zurückgelesen; bei Abweichung bleibt der
MAC gestoppt. Reale Werte bleiben nach Suspend/Resume und logischer USB-
Neuerkennung erhalten. Diese Korrektur allein beweist keinen behobenen
seltenen UDP-Verlust; dazu läuft eine getrennte strenge Langzeitabnahme.

Die TX-Queue fasst32 dynamische Records. Bei voller Queue wartet der Producer
höchstens nominell5ms beziehungsweise mindestens einen FreeRTOS-Tick und gibt
bei weiterem Platzmangel einen Fehler zurück. Diagnose unterscheidet
`waited`, `wait_timeout` und `alloc_fail`; `peak_sampled` ist ein beobachtetes,
kein garantiert lückenloses Maximum. Speicherbedarf hängt von belegten Records
ab. Ein deterministischer Fault-Test füllt die Queue, bestätigt begrenzte
Timeouts und anschließende Erholung; er ist im Betriebsbuild nicht enthalten.

## Statischer Multicast-Hashfilter

```yaml
- ethernet.realtek.multicast:
    mode: FILTERED
    addresses: ["01:00:5e:01:02:03"]
    preserve_basic_groups: true
```

Nur im nativen Backend. `ALL` ist der sichere Standard nach Initialisierung
und Neuerkennung. `FILTERED` programmiert einen64-Bit-Hash mit Rücklesen;
Kollisionen sind möglich. Bis64 Multicast-MACs, kein Broadcast/Unicast.
`preserve_basic_groups` ergänzt All-Nodes, EUI-64-Solicited-Node sowie IGMP/mDNS.
Es gibt keine automatische dynamische IGMP/MLD-Mitgliedschaftsverfolgung.
Anwendungssockets müssen benötigten Gruppen weiterhin selbst beitreten.
Promiscuous kann den Filter umgehen. Tatsächliche Paketannahme wurde mit
ALL/FILTERED/NONE/ALL und unabhängig berechnetem Hash praktisch geprüft.
