# IPv6 und Realtek-Verwaltung

Zielgerät: gemessenes InLine 33380J POE01, USB 0bda:8152/bcd2000,
Hardwarekennung (TCR & 7cf0) 4c10. Schreibzugriffe werden erst nach Prüfung
aller dieser Merkmale freigegeben. Andere ECM-Geräte behalten den generischen
Datenpfad, erhalten aber keine Realtek-Schreibzugriffe.

## Konfiguration

```yaml
network:
  enable_ipv6: true
  min_ipv6_addr_count: 1
ethernet:
  type: USB_ECM
  # Optional: flüchtige Hardware-MAC, vor DHCP und IPv6 gesetzt.
  mac_address: "02:00:00:00:00:01"
  realtek:
    speed: AUTO       # AUTO, "10", "100"
    duplex: FULL      # HALF nur bei fest vorgegebener Geschwindigkeit
    eee: false       # optional; ohne Angabe vorhandene Einstellung behalten
    wake_filters: 0  # optional, Bitmaske 0..31
```

Für einen normalen Betrieb ist keine `realtek:`-Sektion erforderlich. Bei AUTO
werden alle vier 10/100-Halb-/Vollduplexmodi angeboten. Erzwungenes Vollduplex
verlangt eine passend konfigurierte Gegenstelle; sonst droht Duplex-Mismatch
oder es entsteht kein Carrier. Im Test waren feste 10/100 Mbit/s Vollduplex
mit ebenfalls fest auf dieselbe Geschwindigkeit gesetztem Switch erfolgreich,
einschließlich verschlüsselter IPv4-/IPv6-API während des Festmodus.
`min_ipv6_addr_count` zählt bevorzugte Adressen einschließlich Link Local.
Die Verbindungsfreigabe berücksichtigt IPv4 und bevorzugte IPv6-Adressen
getrennt. Mit passender IPv6-Mindestanzahl bleibt die Schnittstelle auch bei
entfallener IPv4 über IPv6 verbunden. `enable_ipv4: false` schaltet DHCPv4 aus
und verlangt aktiviertes IPv6; IPv6-only mit fester MAC wurde nativ geprüft.
Eine zusätzliche statische IPv6-YAML-Schnittstelle ist nicht implementiert.

Die feste MAC ist eine MAC im aktiven Realtek-Adressregister und in esp_netif;
keine reine Änderung der Anzeige. DHCP und IPv6 beginnen erst danach. Ohne
`mac_address` stellt der Treiber auch nach einem OTA-Neustart die Adresse aus dem
ECM-Deskriptor wieder her. EEPROM und eFuses werden nicht beschrieben.

## Laufende Steuerung

Diese neu implementierten Aktionen sind Bestandteil der externen Komponente,
keine Behauptung über bereits upstream vorhandene ESPHome-Optionen:

```yaml
# Innerhalb eines on_press/then:
- ethernet.realtek.link:
    speed: AUTO
    advertisement: 480  # 0x1e0: 10H=0x20,10F=0x40,100H=0x80,100F=0x100
- ethernet.realtek.eee:
    enabled: false
- ethernet.realtek.pause:
    rx: true
    tx: true
- ethernet.realtek.wake:
    filters: 0
- ethernet.realtek.promiscuous:
    enabled: false
```

PHY-/EEE-/Pause-Änderungen können die Verbindung kurz unterbrechen. Zeitlich
begrenzte Prüfungen laufen als Script auf dem Board weiter, auch wenn die API
während der Neuverhandlung ausfällt. EEE-/Pause-Laufzeitaktionen starten die
Auto-Neuverhandlung; einen gewünschten Festmodus danach ausdrücklich erneut
setzen. Ein Interface-disable ist kein PHY-powerdown.

Wake-Bits: 1 Magic Packet, 2 Link, 4 Unicast, 8 Broadcast, 16 Multicast.
Die Wake-Aktion allein programmiert und prüft die NIC-Filter. USB-Suspend mit
`remote_wake: true` verwendet die eigene optionale Bitmaske `wake_filters`;
ohne Angabe gilt1 (Magic). Vor dem Suspend wird sie programmiert und
zurückgelesen, unabhängig von zuvor gesetzten Wake-Aktionen. Mit
`remote_wake: false` sind NIC-Auslöser deaktiviert. `wake_filters: 0` ist nur
bei endlichem Suspend zulässig; unbefristetes Suspend braucht einen Auslöser.
Magic, Unicast, Broadcast und Multicast sind mit gezielten positiven und
negativen Proben im isolierten Testnetz bestätigt; externe Link-Wake-Prüfung
und verbleibende Grenzen stehen in [REALTEK_NATIVE.md](REALTEK_NATIVE.md).
Das ist USB-Remote-Wake, kein ESP32-Deep-Sleep.

EEE kann nur aktiv werden, wenn beide Linkpartner es unterstützen. Am ursprünglichen USW Flex 2.5G 8 PoE bot der getestete Port keine
EEE-Fähigkeit an. Nach Wechsel auf einen USW Pro XG 24 PoE wurden Peer 000E
und gemeinsame 100BASE-TX-EEE-Fähigkeit 0002 bei 100Mbit/s Vollduplex gemessen.
Die Diagnose verlangt für EEE_negotiated auch aktivierte und abgeschlossene
Autonegotiation. Das belegt Aushandlung, keine gemessene LPI-Stromersparnis.
Pause steuert die symmetrische/asymmetrische Advertisement; ein Test tatsächlicher
Pause-Frames unter Überlast ist davon getrennt.

## Diagnose und Datenpfad

`read_realtek_diagnostics()` liefert Hardwarekennung, PHY-Link/Geschwindigkeit/
Duplex, Hardware-MAC, BMCR/BMSR, lokale/Peer-Advertisement, EEE- und Wake-Register
sowie RCR. `read_realtek_counters()` liest die 13 Hardwarezähler für Pakete,
Fehler, verpasste Pakete, Kollisionen und Empfangsklassen. Diese Hardwarezähler
können ESP32-Neustarts überleben: nur Differenzen innerhalb eines passenden
Messfensters auswerten, keine historischen Fehler dem letzten Test zuordnen.
Diagnosebeispiel fragt alle 30 s ab; dies ersetzt nicht die ECM-Linkereignisse.

Bei `USB_ECM` bleibt ECM der RX/TX-Datenpfad. Der tatsächliche ECM-Funktionsdeskriptor meldet
keine CDC-Statistiken und keine selektiven Multicastfilter. IPv6/mDNS erhalten
Multicast über den All-Multicast-Paketfilter. Promiscuous ist explizit schaltbar;
die Annahme fremdadressierter Frames muss separat mit geeigneten Frames geprüft
werden. Der Realtek-RCR-Rücklesewert hilft bei dieser Abnahme.

Offload-/TSO-/Aggregationsdeskriptoren werden ausschließlich im separaten
`USB_RTL8152`-Backend verwendet und dort praktisch geprüft. lwIP rechnet weiterhin
Softwarechecksummen; unterstützte TX-Pakete erhalten zusätzlich Hardwareoffload.
Details und offene Fälle stehen in REALTEK_NATIVE.md. Kein Firmwarepatch-Pfad.
USB Full Speed begrenzt diesen S3-Aufbau.
USB-C-PD/PoE-Wandlung sind eigenständige Hardwarefunktionen des Adapters.

## Lebensdauer und Herkunft

Alle Verwaltungsvorgänge laufen im ESPHome-Mainloop. ECM nutzt die CDC-
Serialisierung, der native Backendpfad einen eigenen begrenzten Control-Transfer. OCP-Zugriffe sichern/restaurieren die Registerbank, MMD setzt
den Adressmodus zurück. MAC-/Wake-Konfigurationsfreigabe wird auch bei Fehlern
zurückgesetzt. Unabhängiger USB-Hotplug während eines Vendorzugriffs ist noch
nicht abgenommen; CDC 3.1.0 bietet hier keine durch diesen Treiber belegte
Port-Lebensdauer-Garantie. Keine Hotplug-Sicherheitszusage daraus ableiten.

Neu geschriebene Implementierung, kein kopierter Linux-Code. Register- und
Protokollfakten wurden mit dem versionsgebundenen
[Realtek/Linux r8152 v6.17](https://github.com/torvalds/linux/blob/v6.17/drivers/net/usb/r8152.c)
abgeglichen. Die Referenz steht unter GPL-2.0-only, diese ESPHome-Erweiterung unter
GPL-3.0-only; Linux-Implementierung wird nicht mitgeliefert. ESP-NETIF-API siehe
[Espressif 5.5.2](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/network/esp_netif_programming.html).

Die externe ethernet_info-Erweiterung publiziert die MAC ereignisgesteuert nach
USB-Enumeration und leert entfernte IPv6-Adressen. Mitgeladene Komponenten:
`[ethernet, logger, ethernet_info]`. Das Beispiel verwendet den normalen
`ethernet_info.mac_address`-Sensor, keinen MAC-Template-Workaround.
