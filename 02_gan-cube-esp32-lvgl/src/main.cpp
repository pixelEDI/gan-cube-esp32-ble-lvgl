#include <Arduino.h>

#include "gan_ble.h"
#include "solve_timer.h"
#include "display_ui.h"
#include "mqtt_publish.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("GAN BLE scanner");
  displayUiSetup();
  mqttPublishSetup();
  ganBleStartScan();
}

void loop() {
  mqttPublishLoop();
  displayUiLoop();
  ganBleLoop();
  delay(5);
}
