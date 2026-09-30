#include "solve_timer.h"

#include <Arduino.h>

namespace {

constexpr uint8_t startButtonPin = 4;  // D2 on the XIAO ESP32-C3
constexpr uint32_t debounceMs = 35;

bool lastButtonReading = LOW;
bool stableButtonState = LOW;
uint32_t lastDebounceTime = 0;

bool runArmed = false;
bool timerRunning = false;
uint32_t startTime = 0;
uint32_t moveCount = 0;

void resetRun() {
  runArmed = true;
  timerRunning = false;
  startTime = 0;
  moveCount = 0;
  Serial.println("SOLVE_READY");
}

void checkButton() {
  const bool reading = digitalRead(startButtonPin);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastDebounceTime < debounceMs ||
      reading == stableButtonState) {
    return;
  }

  stableButtonState = reading;
  if (stableButtonState == HIGH) {
    resetRun();
  }
}

}  // namespace

void solveTimerSetup() {
  pinMode(startButtonPin, INPUT_PULLDOWN);
  stableButtonState = digitalRead(startButtonPin);
  lastButtonReading = stableButtonState;
  // Serial-only test mode: arm the first solve immediately after boot.
  resetRun();
}

void solveTimerLoop() {
  checkButton();
}

void solveTimerMove() {
  if (!runArmed || timerRunning) {
    if (timerRunning) ++moveCount;
    return;
  }

  runArmed = false;
  timerRunning = true;
  startTime = millis();
  moveCount = 1;
  Serial.println("TIMER_START move=1");
}

void solveTimerSolved() {
  if (!timerRunning) {
    return;
  }

  const uint32_t elapsedMs = millis() - startTime;
  Serial.print("SOLVED moves=");
  Serial.print(moveCount);
  Serial.print(" time_ms=");
  Serial.println(elapsedMs);
  timerRunning = false;
  // Arm the next solve automatically; the next MOVE starts a new timer.
  runArmed = true;
}
