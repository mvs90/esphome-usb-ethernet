# Hardware-Robustheitstests

Aktueller Aufbau: InLine33380J POE01 am USW Pro XG24PoE, Port9;
frühere Link-/Kaltstartserie am USW Flex2.5G8PoE.
Waveshare ESP32-S3-LCD-Driver-Board, kein Akku, kein Display.
UART: Board GND → Adapter GND, GPIO43/TXD0 → RX; alle Versorgungspins offen.

## Reihenfolge, immer mit laufender UART-Aufzeichnung

1. **Softwarezyklen:** Nichts umstecken. Codex betätigt die Testschaltflächen
   per verschlüsselter API. Netzwerk wird nach fünf Sekunden automatisch aktiviert.
2. **Verkehrssperre und echter PHY-Linkverlust unterscheiden:** Keine Stecker
   ziehen. Ursprüngliche UniFi-Portoptionen merken.
   - Portzustand deaktivieren und PoE unverändert lassen, nach 20 Sekunden wieder
     aktivieren: Im getesteten Aufbau sperrt dies Verkehr, PHY bleibt auf 100/Full/up.
   - Für echten PHY-Ausfall Portzustand aktiv und PoE unverändert lassen,
     Link Speed kurz auf 1 Gbit/s stellen, nach 20 Sekunden wieder auf Auto.
     Der 10/100-InLine hat dann vorübergehend keinen passenden Linkmodus.
     Tatsächlich getestet: ESPHome disconnected, Board-Uptime läuft weiter,
     nach Auto DHCP/API wieder da. Nur den InLine-Port bearbeiten.
   - Falls PoE wider Erwarten ausfällt, als Stromunterbrechung einordnen.
     Keine Änderungen an VLAN, Uplink oder anderen Ports nötig.
3. **PoE-Kaltstart:** InLine bleibt am PoE-Ethernet. Nur seinen USB-C-Stecker aus
   dem Testboard ziehen, fünf Sekunden warten, wieder einstecken; BOOT nicht drücken.
   UART bleibt unverändert. Auf gemessene API-Wiederkehr warten, dann wiederholen.
4. **Vollständiger Adapter-Kaltstart:** USB-C am Board lassen; nur RJ45 am InLine
   zehn Sekunden ziehen, wieder verbinden. Adapter und Board booten gemeinsam.
5. **USB-Hotplug ohne Boardreset:** Im aktuellen Aufbau nicht möglich: USB-C ist
   zugleich die einzige Stromquelle. Keine UART-Versorgung improvisieren. Dafür
   erst unabhängige, anhand des Schaltplans geprüfte Boardversorgung vorbereiten.
6. **OTA-Abbruch:** Erst nach erfolgreicher Wiederherstellung/Boot-Serie. Upload
   gezielt vor Commit abbrechen, alte Revision und anschließendes vollständiges OTA
   prüfen. Kein willkürliches Steckerziehen während unbekannter Commitphase.

Codex koordiniert die Hardwaretests einzeln, damit Softwaretests und Umstecken
nicht ineinandergreifen. Ein vorbereiteter Ablauf ist kein bestandenes Ergebnis.

[UniFi-Portoptionen](https://help.ui.com/hc/en-us/articles/33402927617047-UniFi-Switch-Settings)
führen Portzustand und PoE separat; der konkrete Erhalt der Versorgung wird bei
unserem Test anhand Bootlog und Uptime festgestellt.

## Ergänzende native Prüfungen

Softwaregesteuerter Rootport-Stop/Start mit fünf Sekunden Abstand prüft
USB-Neu-Enumeration ohne Änderung der PoE-Versorgung. Drei Zyklen bestanden.
Vor unterbrechenden Tests mindestens65s nach OTA-Boot warten, damit das Image
bestätigt ist. Kein Ersatz für physischen Hotplug bei unabhängiger Versorgung.
Timer-/Magic-Wake, Bulkaggregation und TCP/UDP-Offload brauchen keine Kabeländerung.

## Erweiterte Fehlerfallserie

Die Profile `usb-fault-test.yaml` und `usb-no-dhcp-start-test.yaml` sind ausschließlich
Testfirmware. Ihre Echoports, CPU/RAM-Last und Fehlerknöpfe gehören nicht in das
öffentliche Betriebsbeispiel. Die Kontrollschnittstelle bleibt verschlüsselt.

- DHCP: Nur ausgehende Clientpakete dieses Netif werden zeitlich begrenzt
  verworfen. T1/T2/T0 werden lokal verkürzt; der tatsächliche ESPHome-SDK-Takt ist
  `CONFIG_LWIP_DHCP_COARSE_TIMER_SECS=1`. Die normalen lwIP-Timer laufen weiter.
  IPv6 kontrolliert den Betrieb während Renew/Rebind/Lease-Ablauf. Ein NAK mit
  passender beziehungsweise falscher Transaktions-ID wird ausschließlich in den
  lokalen Empfangsstack eingespeist. Dies prüft lwIP und die Statusintegration,
  keinen vom Ethernetkabel empfangenen NAK.
- DNS: Nur DNS-Ausgaben dieses Netif werden blockiert, danach wird der Filter
  entfernt. Negative und positive Antwortkontrollen getrennt dokumentieren.
- USB-Control: Ein Testflag beendet das Warten nach echter Submit-Operation;
  ausschließlich der echte USB-Callback gibt die Transferallokation wieder frei.
  Der Test simuliert den Wartetimeout, keinen elektrisch festhängenden Controller.
- OTA: Falscher Schlüssel und Verbindungsabbruch nach fest begrenzter Bytezahl
  vor Commit. Anschließend API/Uptime und ein vollständiges Update prüfen.
- UDP-Fragmente: Datagramme oberhalb MTU in beiden Richtungen bytegenau prüfen;
  Reassemblierung ist explizit aktiviert. IPv6-Quellfragmentierung wird über die
  Socketoption zur minimalen IPv6-MTU angefordert.
- Last: Unveränderte Firmware und Verbindungen über den gesamten Messzeitraum;
  Fehlerzählerdifferenzen statt alter kumulierter Zählerstände beurteilen.
  Künstliche CPU-/RAM-Konkurrenz ist kein Displaytest.

Das umfangreiche Echo-Testprofil benötigt mehr Socketkapazität als das bereinigte
Betriebsbeispiel: UDP-/TCP-Echodienste, angenommene TCP-Verbindungen, API und OTA
teilen denselben lwIP-Socketpool. Für den gemeinsamen Lastlauf ist
`CONFIG_LWIP_MAX_SOCKETS=16` explizit gesetzt. Ein erster Anlauf mit zehn Sockets
wies die zweite API-Anmeldung zurück; ohne die beiden Echo-TCP-Verbindungen
funktionierten beide API-Anmeldungen sofort. Dies als Testkonfigurationsgrenze
von einem Stillstand des RTL-Datenpfads unterscheiden.

Vor jedem unterbrechenden Test mindestens65s seit dem OTA-Boot warten. Ein
bestandener Build, lokaler Parsertest und bestandener Hardwaretest bleiben
getrennte Nachweise. Laufende Langzeittests erst nach Ablauf als bestanden werten.
