#include "WifiManager.h"
#include <WiFi.h>
#include "../web/Settings.h"

WifiState wifiState = WifiState::CONNECTING;

static uint32_t connectStart = 0;
static const uint32_t WIFI_TIMEOUT_MS = 15000;

static const char* AP_SSID = "ESP32-StatusPanel";
static const char* AP_PASS = "configureme";

void wifiBegin() {
  if (settings.wifiSSID[0] == '\0') {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    wifiState = WifiState::AP_MODE;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.wifiSSID, settings.wifiPass);
  connectStart = millis();
  wifiState = WifiState::CONNECTING;
}

void wifiLoop() {
  if (wifiState == WifiState::CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = WifiState::CONNECTED;
      return;
    }

    if (millis() - connectStart > WIFI_TIMEOUT_MS) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, AP_PASS);
      wifiState = WifiState::AP_MODE;
    }
  }
}

const char* wifiStatusMessage() {
  switch (wifiState) {
    case WifiState::CONNECTED: return "WiFi connected";
    case WifiState::CONNECTING: return "Connecting to WiFi...";
    case WifiState::AP_MODE: return "Unable to connect to WiFi";
  }
  return "";
}
