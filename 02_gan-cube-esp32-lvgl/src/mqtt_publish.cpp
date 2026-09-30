#include "mqtt_publish.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <cstring>
#include "display_ui.h"
#if __has_include("credentials.h")
#include "credentials.h"
#else
static const char* wifiSsid = "CHANGE_ME";
static const char* wifiPassword = "CHANGE_ME";
static const char* mqttServer = "CHANGE_ME";
static const uint16_t mqttPort = 1883;
static const char* mqttUser = "CHANGE_ME";
static const char* mqttPassword = "CHANGE_ME";
static const char* mqttTopic = "cube/newRound";
#endif
namespace {
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
uint32_t nextMqttAttempt = 0;
}
void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (strcmp(topic, "cube/showToplist") == 0) displayUiShowResults(payload, length);
}

void mqttPublishSetup() { WiFi.mode(WIFI_STA); WiFi.begin(wifiSsid, wifiPassword); mqttClient.setServer(mqttServer, mqttPort); mqttClient.setCallback(mqttMessageCallback); Serial.println("WIFI_START"); }
void mqttPublishLoop() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqttClient.connected()) { if (millis() < nextMqttAttempt) return; nextMqttAttempt = millis() + 5000; String id = "gan-cube-" + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX); if (mqttClient.connect(id.c_str(), mqttUser, mqttPassword)) { mqttClient.subscribe("cube/showToplist"); Serial.println("MQTT_CONNECTED subscribed=cube/showToplist"); } else { Serial.print("MQTT_CONNECT_FAILED state="); Serial.println(mqttClient.state()); } return; }
  mqttClient.loop();
}
void mqttPublishSolved(uint32_t solvingTimeMs, uint32_t moves) {
  if (!mqttClient.connected()) { Serial.println("MQTT_PUBLISH_SKIPPED not_connected"); return; }
  char payload[96]; snprintf(payload, sizeof(payload), "{\"solvingtime\":%lu,\"moves\":%lu}", static_cast<unsigned long>(solvingTimeMs), static_cast<unsigned long>(moves));
  if (mqttClient.publish(mqttTopic, payload)) { Serial.print("MQTT_PUBLISHED payload="); Serial.println(payload); } else Serial.println("MQTT_PUBLISH_FAILED");
}

bool mqttPublishWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool mqttPublishMqttConnected() {
  return mqttClient.connected();
}
