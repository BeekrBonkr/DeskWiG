#pragma once
#include <Arduino.h>
#include "../net/PingTarget.h"

constexpr uint8_t API_TOKEN_LEN = 32;
constexpr uint8_t HOSTNAME_MAX  = 32;

struct Settings {
  // ---- Stored in NVS. Survives a filesystem wipe so the device stays reachable.
  char wifiSSID[32] = "";
  char wifiPass[64] = "";
  char apiToken[API_TOKEN_LEN + 1] = "";

  // ---- Stored in /config.json on LittleFS.
  char hostname[HOSTNAME_MAX + 1] = "deskwig";

  uint32_t pingIntervalMs = 10000;
  uint8_t activeWidget = 0;

  int32_t clockTzOffset = 0;
  int32_t clockDstOffset = 0;
  bool clock24h = true;

  bool ledEnabled = true;
  uint8_t ledBrightness = 5;

  uint8_t targetCount = 0;
  PingTarget targets[MAX_PING_TARGETS];
};

extern Settings settings;

// Mounts LittleFS, loads credentials + token from NVS, loads /config.json.
// Migrates settings from the pre-JSON NVS layout on first boot after upgrade.
void loadSettings();

// Writes /config.json atomically. Returns false on filesystem error.
bool saveSettings();

// Writes WiFi credentials to NVS.
void saveCredentials();

// Validates and stores a hostname (lowercase letters, digits, dashes).
// Returns false and leaves the setting untouched if invalid.
bool setHostname(const char* name);

// Erases the filesystem and NVS. Caller should restart afterwards.
void factoryReset();

void loadDefaultTargets();
void resetClockDefaults();
