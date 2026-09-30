#include "solve_timer.h"

#include <Arduino.h>

namespace {

bool runArmed = false;
bool timerRunning = false;
uint32_t startTime = 0;
uint32_t finishedTime = 0;
uint32_t moveCount = 0;

}  // namespace

void solveTimerReset() {
  runArmed = true;
  timerRunning = false;
  startTime = 0;
  finishedTime = 0;
  moveCount = 0;
  Serial.println("SOLVE_READY");
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
  finishedTime = elapsedMs;
  Serial.print("SOLVED moves=");
  Serial.print(moveCount);
  Serial.print(" time_ms=");
  Serial.println(elapsedMs);
  timerRunning = false;
}

uint32_t solveTimerElapsedMs() {
  if (timerRunning) return millis() - startTime;
  return finishedTime;
}

uint32_t solveTimerMoveCount() { return moveCount; }
