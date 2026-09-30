#pragma once
#include <cstddef>
#include <cstdint>

void displayUiSetup();
void displayUiLoop();
void displayUiShowResults(const uint8_t* payload, size_t length);
void displayUiSetCurrentSolve(uint32_t timeMs);
void displayUiSolveFinished();
