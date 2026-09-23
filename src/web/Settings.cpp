#include "Settings.h"
#include <Preferences.h>

Settings settings;
static Preferences prefs;

void resetClockDefaults() {
  settings.clockTzOffset = 0;
  settings.clockDstOffset = 0;
  settings.clock24h = true;
}

static void setTarget(uint8_t i, const char* name, const char* host, uint16_t port, TargetType type) {
  PingTarget& t = settings.targets[i];
  memset(&t, 0, sizeof(PingTarget));
  strlcpy(t.name, name, sizeof(t.name));
  strlcpy(t.host, host, sizeof(t.host));
  t.port = port;
  t.type = type;
  t.latency = -1;
  t.lastLatency = -1;
}

void loadDefaultTargets() {
  settings.targetCount = 3;
  setTarget(0, "Cloudflare", "1.1.1.1", 443, TargetType::SERVICE);
  setTarget(1, "Google", "8.8.8.8", 53, TargetType::SERVICE);
  setTarget(2, "Router", "192.168.88.1", 0, TargetType::SERVER);
}

void loadSettings() {
  prefs.begin("config", true);

  prefs.getString("ssid", settings.wifiSSID, sizeof(settings.wifiSSID));
  prefs.getString("pass", settings.wifiPass, sizeof(settings.wifiPass));

  settings.pingIntervalMs = prefs.getUInt("pingMs", 10000);
  settings.activeWidget = prefs.getUChar("widget", 0);

  settings.clockTzOffset = prefs.getInt("tz", 0);
  settings.clockDstOffset = prefs.getInt("dst", 0);
  settings.clock24h = prefs.getBool("24h", true);

  settings.targetCount = prefs.getUChar("tcount", 0);
  if (settings.targetCount == 0 || settings.targetCount > MAX_PING_TARGETS) {
    loadDefaultTargets();
  } else {
    for (uint8_t i = 0; i < settings.targetCount; i++) {
      prefs.getBytes(String("t" + String(i)).c_str(), &settings.targets[i], sizeof(PingTarget));
    }
  }

  prefs.end();
}

void saveSettings() {
  prefs.begin("config", false);

  prefs.putString("ssid", settings.wifiSSID);
  prefs.putString("pass", settings.wifiPass);
  prefs.putUInt("pingMs", settings.pingIntervalMs);
  prefs.putUChar("widget", settings.activeWidget);

  prefs.putInt("tz", settings.clockTzOffset);
  prefs.putInt("dst", settings.clockDstOffset);
  prefs.putBool("24h", settings.clock24h);

  prefs.putUChar("tcount", settings.targetCount);
  for (uint8_t i = 0; i < settings.targetCount; i++) {
    prefs.putBytes(String("t" + String(i)).c_str(), &settings.targets[i], sizeof(PingTarget));
  }

  prefs.end();
}
