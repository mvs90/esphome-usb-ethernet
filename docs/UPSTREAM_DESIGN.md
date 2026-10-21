# Vorbereitung einer späteren ESPHome-Änderung

Dies ist ein lokaler Entwurf, kein eingereichter Vorschlag und keine Zusage der
Maintainer. Das externe Paket funktioniert ohne einen ESPHome-/ESP-IDF-Fork.

## Beobachtete Änderungen und Grund

- USB-Host darf nur einmal installiert werden. ECM/CDC muss vor der Enumeration
  registriert sein; die Probe startet den Host mit unversorgtem Root-Port und
  aktiviert den Port nach Installation des Netzwerkpfads.
- Der InLine benötigt seinen gemessenen Konfigurationsquirk. Ein pauschaler
  Wechsel aller USB-Geräte auf Konfiguration 2 wäre falsch.
- Netzwerkhelfer erwarten `ethernet::global_eth_component` und dessen Status-/
  Adressschnittstelle. `iot_eth_handle_t` darf nicht als `esp_eth_handle_t`
  ausgegeben werden. Es braucht einen eigenständigen Backendpfad.
- Logger 2026.9.1 fordert optional eine sekundäre USB-Serial-JTAG-Konsole an,
  obwohl UART0 gewählt ist. Auf dem S3 kollidiert dies mit dem Host-PHY.
  Die externe Loggerkopie umgeht diese Fähigkeitserkennung bei USB_ECM.
- IP-/DNS-Listener müssen auch Änderungen bei unverändertem Verbindungsstatus
  sehen. ECM-MAC ist erst nach USB-Enumeration verfügbar, nicht einmalig bei Setup.
- ECM 0.4.0 setzt Driver-start/stop auf NULL. Interface-Wiederaktivierung muss
  deshalb den erhaltenen Link für die Netif-Glue erneut melden. Das ist keine
  Implementierung eines PHY-Powerdown.

## Mögliche getrennte Beiträge

1. Allgemeine Logger-/USB-PHY-Eigentümerschaft, mit Schema-/Codegen-Nachweis.
2. Host-Startkoordination und gerätespezifische Konfigurationswahl, unter Erhalt
   vorhandener USB-Clients und ihrer Task-/Transferverwaltung.
3. USB-ECM-Backend im existierenden Ethernet-Baustein, ohne RMII-/SPI-Pfade zu
   ersetzen. Nur getestete Optionen im Schema anbieten.
4. Asynchrones MAC-Publishing, Link-/IP-/DNS-Ereignisse und dokumentierte
   Hardware-/Lifecycle-Tests. Erst nach diesen Nachweisen breitere Freigabe.

Ein vollständiger Realtek-Vendortreiber wäre ein eigenes Vorhaben mit
Revisionsidentifikation, Protokoll-/Lizenzprüfung und Verwaltungspfad. Er ist
keine Voraussetzung, um den funktionierenden ECM-Pfad extern zu veröffentlichen.

Quellen: [External Components](https://esphome.io/components/external_components/),
[Beitragsrichtlinien](https://developers.esphome.io/contributing/code/),
lokal auditierter offizieller ESPHome-Tag 2026.9.1,
Commit 0b2b16e664ecfc0add37f3435db1c2140b8a687e.

Die externe ethernet_info-Erweiterung publiziert die MAC ereignisgesteuert nach
USB-Enumeration und leert entfernte IPv6-Adressen. Mitgeladene Komponenten:
`[ethernet, logger, ethernet_info]`. Das Beispiel verwendet den normalen
`ethernet_info.mac_address`-Sensor, keinen MAC-Template-Workaround.

## Nativer Backendstand alpha.3

`USB_RTL8152` ergänzt einen eigenen revisionsgebundenen Daten-/Kontrollpfad.
Espressif USB1.6.0 und CDC-ACM2.4.1 werden als versionierte Komponenten geladen;
ihr offenes Dateninterface überlebt Suspend/Resume. Ein zusätzlicher Client
verwaltet begrenzte Control-Transfers und Remote-Wake von NIC/Adapterhub.
TX-Queue, IPv4/IPv6-Checksum-Offload, Paketaggregation und konservative TCP-
Rekoaleszierung für Hardware-TSO benötigen keinen geänderten lwIP-GSO-Vertrag.

Für Veröffentlichung als externe Komponente reicht das Quellpaket. Für Aufnahme
in ESPHome wären reguläre Änderungen am Ethernet-Backend, Logger-/Hostkoordination
und ethernet_info sinnvoll. Die versionsgebundene komplette Komponentenüberlagerung
ist eine Vorablieferung, keine langfristige Upstream-Architektur. Kein ESPHome-
„Kernel“-Fork und keine lokale Änderung an ESP-IDF sind erforderlich.


## Optionale LLDP-Komponente

LLDP ist als eigener Protokollbaustein implementiert. Das externe Ethernet-
Overlay stellt begrenzte Layer2-Listener und Sendefunktionen bereit. Eine spätere
Upstream-Aufnahme sollte diese Schnittstelle unabhängig vom RTL-Treiber prüfen
und andere Ethernet-Backends gesondert anbinden. LLDP ist kein Anlass für einen
ESP-IDF-/ESPHome-Kernelfork. Der aktuelle vollständige Komponentenoverlay bleibt
an ESPHome2026.9.1 gebunden und ist noch keine allgemeine Upstream-L2-API.
