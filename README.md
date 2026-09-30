# Rubik's Cube mit ESP32, BLE und LVGL

Dieses Repository enthält zwei Varianten eines Projekts rund um einen smarten
GAN-Rubik's-Cube:

- `01_gan-cube-esp32-ble_advertising` ist die schlanke BLE-Variante. Sie liest
  den Cube aus, erkennt Züge und erkennt, wenn derselbe Ausgangszustand wieder
  erreicht ist.
- `02_gan-cube-esp32-lvgl` erweitert das Projekt um eine grafische Oberfläche
  auf einem ESP32-Touchdisplay mit LVGL. Zusätzlich kann die Variante
  Lösungszeiten und Zugzahlen per MQTT weitergeben und Ergebnisse anzeigen.

Beide Projekte verwenden PlatformIO und kommunizieren mit einem GAN 356 i
Carry 2 über Bluetooth Low Energy.

Die Unterordner sind eigenständige PlatformIO-Projekte. Details zum jeweiligen
Aufbau, zur Verwendung und zum aktuellen Stand stehen in den zugehörigen
README-Dateien.

Die BLE-Auswertung basiert auf der Dokumentation und den Erkenntnissen aus dem
Projekt [`gan-web-bluetooth`](https://github.com/afedotov/gan-web-bluetooth).
