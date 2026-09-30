#include <Arduino.h>

#include "gan_ble.h"
#include "solve_timer.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("GAN BLE scanner");
  solveTimerSetup();
  ganBleStartScan();
}

void loop() {
  solveTimerLoop();
  ganBleLoop();
  delay(5);
}
