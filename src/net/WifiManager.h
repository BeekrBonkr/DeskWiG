#pragma once
#include <Arduino.h>

enum class WifiState {
  CONNECTING,
  CONNECTED,
  AP_MODE
};

extern WifiState wifiState;

void wifiBegin();
void wifiLoop();
const char* wifiStatusMessage();
