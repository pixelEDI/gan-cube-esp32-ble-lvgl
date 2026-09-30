#pragma once
#include <cstdint>
void mqttPublishSetup();
void mqttPublishLoop();
void mqttPublishSolved(uint32_t solvingTimeMs, uint32_t moves);
bool mqttPublishWifiConnected();
bool mqttPublishMqttConnected();
