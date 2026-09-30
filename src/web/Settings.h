#pragma once
#include <Arduino.h>
#include "../net/PingTarget.h"
#include "../net/TimeService.h"
#include "../net/DataSource.h"
#include "../net/Series.h"

// Short enough to type from the device screen; brute force is blunted by
// the login lockout in Auth.cpp.
constexpr uint8_t API_TOKEN_LEN = 8;
constexpr uint8_t HOSTNAME_MAX  = 32;
constexpr uint8_t TZ_MAX        = 63;
constexpr uint8_t NTP_HOST_MAX  = 63;
constexpr uint8_t MAX_HIDDEN_WIDGETS = 4;
constexpr uint8_t MAX_WIDGET_ORDER = 16;    // matches ScreenManager::MAX_WIDGETS
constexpr uint8_t WIDGET_KEY_LEN = 24;

struct Settings {
  // ---- Stored in NVS. Survives a filesystem wipe so the device stays reachable.
  char wifiSSID[32] = "";
  char wifiPass[64] = "";
  char apiToken[API_TOKEN_LEN + 1] = "";

  // ---- Stored in /config.json on LittleFS.
  char hostname[HOSTNAME_MAX + 1] = "deskwig";

  uint32_t pingIntervalMs = 10000;
  uint8_t activeWidget = 0;           // index, kept for older configs
  char activeWidgetName[33] = "";     // preferred: widget list order changes as layouts come and go

  // Built-in widgets the user has deleted, by key ("Ping", "Clock").
  uint8_t hiddenCount = 0;
  char hiddenWidgets[MAX_HIDDEN_WIDGETS][16];

  // Widget keys in the order the encoder steps through them. Widgets not
  // listed (new layouts) follow in load order.
  uint8_t orderCount = 0;
  char widgetOrder[MAX_WIDGET_ORDER][WIDGET_KEY_LEN + 1];

  // POSIX TZ string, e.g. "EST5EDT,M3.2.0,M11.1.0". "UTC0" = no offset.
  char clockTz[TZ_MAX + 1] = "UTC0";
  bool clock24h = true;
  NtpSource ntpSource = NtpSource::POOL;
  char ntpServer[NTP_HOST_MAX + 1] = "pool.ntp.org";

  bool ledEnabled = true;
  uint8_t ledBrightness = 5;

  uint8_t targetCount = 0;
  PingTarget targets[MAX_PING_TARGETS];

  uint8_t sourceCount = 0;
  DataSource sources[MAX_SOURCES];

  // Keys sampled over time for charts and {key.min}-style statistics.
  uint8_t seriesCount = 0;
  SeriesCfg series[MAX_SERIES];
};

extern Settings settings;

// Mounts LittleFS, loads credentials + token from NVS, loads /config.json.
// Migrates settings from the pre-JSON NVS layout on first boot after upgrade.
void loadSettings();

// Writes /config.json atomically. Returns false on filesystem error.
bool saveSettings();

// Writes WiFi credentials to NVS.
void saveCredentials();

// Replaces the API key with a fresh random one and stores it in NVS.
void regenerateApiToken();

// Validates and stores a hostname (lowercase letters, digits, dashes).
// Returns false and leaves the setting untouched if invalid.
bool setHostname(const char* name);

// Erases the filesystem and NVS. Caller should restart afterwards.
void factoryReset();

void loadDefaultTargets();
void resetClockDefaults();

// Validates and stores a POSIX TZ string. Returns false if it is empty,
// too long, or contains characters a TZ string never uses.
bool setClockTz(const char* tz);

// Validates and stores the custom NTP host (hostname or IP).
bool setNtpServer(const char* host);
