#pragma once
#include <Arduino.h>
#include "../net/PingTarget.h"

struct Settings {
  char wifiSSID[32] = "";
  char wifiPass[64] = "";

  uint32_t pingIntervalMs = 10000;
  uint8_t activeWidget = 0;

  // clock
  int32_t clockTzOffset = 0;
  int32_t clockDstOffset = 0;
  bool clock24h = true;

  uint8_t targetCount = 0;
  PingTarget targets[MAX_PING_TARGETS];
};

extern Settings settings;

void loadSettings();
void saveSettings();
void loadDefaultTargets();
void resetClockDefaults();
