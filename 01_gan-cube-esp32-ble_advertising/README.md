# GAN 356 i Carry 2 über BLE auslesen

Der ESP32 verbindet sich mit einem **GAN 356 i Carry 2**, liest Zug- und
Zustandsmeldungen und meldet Änderungen sowie den erneut erreichten
Startzustand:

```text
MOVE L serial=156
CUBE_SOLVED
```

Die Verbindung und GATT-Diagnose bleiben beim Start sichtbar; während des
Betriebs wird nur noch eine Zeile pro Zug ausgegeben.

## Aktueller Stand

Der ESP32 gibt Geräte aus, deren Name mit `GAN` beginnt:

- Gerätename
- BLE-Adresse
- Address Type
- RSSI
- Manufacturer Data
- Service-UUIDs aus dem Advertising

Nach dem Scan verbindet sich der ESP32 gezielt mit einem erkannten GAN-Cube und gibt
Services und Characteristics aus. Die BLE-Adresse wird nicht fest eingetragen,
weil der Cube eine Random Address verwendet, die sich ändern kann.

## BLE-Advertising

BLE-Geräte senden regelmäßig kurze, nicht verbundene Funkpakete. Diese heißen
Advertising und machen ein Gerät in der Umgebung sichtbar.

Der ESP32 kann diese Pakete empfangen, ohne sich zu verbinden. Dadurch lassen
sich Name, Adresse, RSSI, Manufacturer Data und angekündigte Service-UUIDs
auslesen.

Beim Scan werden unter anderem folgende Informationen erkannt:

```text
Name:           (erkannter GAN-Cube)
Address:        (zufällige BLE-Adresse)
Address Type:   random
RSSI:           (abhängig von der Entfernung)
Manufacturer:   (geräteabhängige Daten)
Service UUIDs:
  0x180a
```

`0x180A` ist der standardisierte Device-Information-Service. Der proprietäre
GAN-Service muss nicht im Advertising stehen und wird wahrscheinlich erst nach
der Verbindung bei der GATT-Service-Discovery sichtbar.

## GATT und Notifications

GATT ist das BLE-Protokoll für die strukturierte Kommunikation nach dem
Verbinden. Ein GATT-Service gruppiert zusammengehörige Daten; eine
Characteristic ist ein einzelner les-, schreib- oder abonnierbarer Datenkanal.

Der Cube verwendet hier:

- `8653000a-...`: proprietärer GAN-Gen3-Service
- `8653000c-...`: Command-Characteristic zum Schreiben
- `8653000b-...`: State-Characteristic für Notifications

Der ESP32 abonniert den State-Kanal. 16-Byte-Pakete enthalten Zugdaten,
19-Byte-Pakete den aktuellen Würfelzustand.

## Salt und Decodierung

Die GAN-Pakete sind nicht als Klartext übertragen. Aus den letzten sechs Bytes
der Manufacturer Data wird die Geräte-MAC abgeleitet. Für dieses Gerät ergibt
sich der Salt:

```text
Manufacturer Data: (geräteabhängige Daten)
AES-Salt:          (aus den Gerätedaten abgeleitet)
```

Der Salt wird auf die ersten sechs Bytes von AES-Key und IV addiert. Danach
werden die GAN-Gen3-Pakete mit AES-128-CBC entschlüsselt. Bei längeren Paketen
werden der erste und der letzte 16-Byte-Block in der vom Protokoll vorgegebenen
Reihenfolge verarbeitet.

Nach der Entschlüsselung beginnt ein Gen3-Paket mit `0x55`:

- Event `0x01`: Zug; Seriennummer, Seite und Richtung werden gelesen.
- Event `0x02`: Facelets-Zustand; Corner-/Edge-Permutation und Orientierung
  werden aus Bitfeldern gelesen und in Kociemba-Facelets umgewandelt.

Der Cube liefert für diesen Zustand eine stabile Geräte-Referenz, die nicht
der kanonischen Kociemba-Startpermutation entspricht. Deshalb wird beim Start
der erste empfangene Zustand als gelöste Referenz gespeichert. Wird derselbe
Zustand später wieder empfangen, erscheint `CUBE_SOLVED`.

## Solve-Timer

Der Taster an **GPIO4 (D2)** ist als Pulldown beschaltet: `LOW` bedeutet nicht
gedrückt, `HIGH` bedeutet gedrückt und startet eine neue Messung. Ein Tastendruck setzt
den Zugzähler und den Timer zurück. Die Zeit beginnt erst mit dem ersten
erkannten Cube-Zug. Beim Erreichen des gespeicherten gelösten Zustands wird
einmal ausgegeben:

```text
SOLVED moves=42 time_ms=12345
```

Ein weiterer Druck auf D2 setzt die nächste Messung zurück.

Für den aktuellen Scan muss keine UUID hinterlegt werden.

## Danksagung und Protokollreferenz

Die AES-Verarbeitung, die Gen3-Paketstruktur, Bitpositionen und das
Facelet-Mapping wurden anhand des Open-Source-Projekts
[`gan-web-bluetooth`](https://github.com/afedotov/gan-web-bluetooth) von
Afedotov. Das Projekt steht unter der MIT-Lizenz.

## Bauen und Flashen

```bash
pio run
pio run --target upload
pio device monitor -p /dev/ttyACM1 -b 115200
```

Für einen späteren Verbindungstest die Verbindung des Cubes in nRF Connect
trennen und nRF Connect möglichst schließen. Ein Cube kann je nach Firmware
nicht gleichzeitig eine Verbindung mit Smartphone und ESP32 halten.
