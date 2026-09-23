#include "Settings.h"

#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_random.h>

Settings settings;

static const char* NVS_NS         = "config";
static const char* CONFIG_PATH    = "/config.json";
static const char* CONFIG_TMP     = "/config.json.tmp";
static const uint8_t CONFIG_VERSION = 1;
static const uint32_t MIN_PING_INTERVAL_MS = 1000;

// =====================
// DEFAULTS
// =====================
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

static const char* typeName(TargetType t) {
  return t == TargetType::SERVER ? "server" : "service";
}

static TargetType typeFromName(const char* s) {
  return (s && strcmp(s, "server") == 0) ? TargetType::SERVER : TargetType::SERVICE;
}

// =====================
// NVS: credentials + API token
// =====================
static void generateToken() {
  static const char hex[] = "0123456789abcdef";
  for (uint8_t i = 0; i < API_TOKEN_LEN; i += 8) {
    uint32_t r = esp_random();
    for (uint8_t j = 0; j < 8; j++) {
      settings.apiToken[i + j] = hex[r & 0xF];
      r >>= 4;
    }
  }
  settings.apiToken[API_TOKEN_LEN] = '\0';
}

static void loadNvs() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  prefs.getString("ssid", settings.wifiSSID, sizeof(settings.wifiSSID));
  prefs.getString("pass", settings.wifiPass, sizeof(settings.wifiPass));
  prefs.getString("token", settings.apiToken, sizeof(settings.apiToken));
  prefs.end();

  if (strlen(settings.apiToken) != API_TOKEN_LEN) {
    generateToken();
    prefs.begin(NVS_NS, false);
    prefs.putString("token", settings.apiToken);
    prefs.end();
    Serial.println("[CFG] Generated new API token");
  }
}

void saveCredentials() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.putString("ssid", settings.wifiSSID);
  prefs.putString("pass", settings.wifiPass);
  prefs.end();
}

// =====================
// LEGACY MIGRATION
// Pre-JSON firmware stored everything in NVS, including raw PingTarget bytes.
// Read it once, then delete the old keys so this never runs again.
// =====================
static bool migrateLegacyNvs() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);

  bool hasLegacy = prefs.isKey("tcount") || prefs.isKey("pingMs");
  if (hasLegacy) {
    settings.pingIntervalMs = prefs.getUInt("pingMs", 10000);
    settings.activeWidget   = prefs.getUChar("widget", 0);
    settings.clockTzOffset  = prefs.getInt("tz", 0);
    settings.clockDstOffset = prefs.getInt("dst", 0);
    settings.clock24h       = prefs.getBool("24h", true);

    uint8_t count = prefs.getUChar("tcount", 0);
    if (count == 0 || count > MAX_PING_TARGETS) {
      loadDefaultTargets();
    } else {
      settings.targetCount = count;
      for (uint8_t i = 0; i < count; i++) {
        PingTarget raw{};
        String key = "t" + String(i);
        if (prefs.getBytes(key.c_str(), &raw, sizeof(PingTarget)) == sizeof(PingTarget)) {
          setTarget(i, raw.name, raw.host, raw.port, raw.type);
        }
      }
    }

    prefs.remove("pingMs");
    prefs.remove("widget");
    prefs.remove("tz");
    prefs.remove("dst");
    prefs.remove("24h");
    prefs.remove("tcount");
    for (uint8_t i = 0; i < MAX_PING_TARGETS; i++) {
      String key = "t" + String(i);
      if (prefs.isKey(key.c_str())) prefs.remove(key.c_str());
    }
  }

  prefs.end();
  return hasLegacy;
}

// =====================
// JSON CONFIG
// =====================
static bool loadConfigFile() {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[CFG] %s parse failed: %s\n", CONFIG_PATH, err.c_str());
    return false;
  }

  if (!setHostname(doc["hostname"] | "widget")) strlcpy(settings.hostname, "widget", sizeof(settings.hostname));

  settings.pingIntervalMs = doc["pingIntervalMs"] | 10000u;
  if (settings.pingIntervalMs < MIN_PING_INTERVAL_MS) settings.pingIntervalMs = MIN_PING_INTERVAL_MS;
  settings.activeWidget = doc["activeWidget"] | 0;

  JsonVariantConst clock = doc["clock"];
  settings.clockTzOffset  = clock["tzOffset"]  | 0;
  settings.clockDstOffset = clock["dstOffset"] | 0;
  settings.clock24h       = clock["24h"]       | true;

  JsonVariantConst led = doc["led"];
  settings.ledEnabled    = led["enabled"]    | true;
  settings.ledBrightness = led["brightness"] | 5;

  settings.targetCount = 0;
  for (JsonVariantConst t : doc["targets"].as<JsonArrayConst>()) {
    if (settings.targetCount >= MAX_PING_TARGETS) break;
    setTarget(
      settings.targetCount++,
      t["name"] | "",
      t["host"] | "",
      (uint16_t)(t["port"] | 0),
      typeFromName(t["type"] | "service")
    );
  }
  if (settings.targetCount == 0) loadDefaultTargets();

  return true;
}

bool saveSettings() {
  JsonDocument doc;
  doc["version"]        = CONFIG_VERSION;
  doc["hostname"]       = settings.hostname;
  doc["pingIntervalMs"] = settings.pingIntervalMs;
  doc["activeWidget"]   = settings.activeWidget;

  JsonObject clock = doc["clock"].to<JsonObject>();
  clock["tzOffset"]  = settings.clockTzOffset;
  clock["dstOffset"] = settings.clockDstOffset;
  clock["24h"]       = settings.clock24h;

  JsonObject led = doc["led"].to<JsonObject>();
  led["enabled"]    = settings.ledEnabled;
  led["brightness"] = settings.ledBrightness;

  JsonArray targets = doc["targets"].to<JsonArray>();
  for (uint8_t i = 0; i < settings.targetCount; i++) {
    const PingTarget& src = settings.targets[i];
    JsonObject t = targets.add<JsonObject>();
    t["name"] = src.name;
    t["host"] = src.host;
    t["port"] = src.port;
    t["type"] = typeName(src.type);
  }

  // Write to a temp file, then rename over the real one so a power loss
  // mid-write can't leave a half-written config.
  File f = LittleFS.open(CONFIG_TMP, "w");
  if (!f) {
    Serial.println("[CFG] Failed to open temp config for writing");
    return false;
  }
  size_t written = serializeJsonPretty(doc, f);
  f.close();

  if (written == 0) {
    LittleFS.remove(CONFIG_TMP);
    Serial.println("[CFG] Failed to serialize config");
    return false;
  }

  if (!LittleFS.rename(CONFIG_TMP, CONFIG_PATH)) {
    // Some VFS builds refuse to rename over an existing file.
    LittleFS.remove(CONFIG_PATH);
    if (!LittleFS.rename(CONFIG_TMP, CONFIG_PATH)) {
      Serial.println("[CFG] Failed to move config into place");
      return false;
    }
  }

  return true;
}

// =====================
// HOSTNAME
// =====================
bool setHostname(const char* name) {
  if (!name) return false;
  size_t len = strlen(name);
  if (len == 0 || len > HOSTNAME_MAX) return false;

  char clean[HOSTNAME_MAX + 1];
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    if (!ok) return false;
    clean[i] = c;
  }
  clean[len] = '\0';
  if (clean[0] == '-' || clean[len - 1] == '-') return false;

  strlcpy(settings.hostname, clean, sizeof(settings.hostname));
  return true;
}

// =====================
// FACTORY RESET
// =====================
void factoryReset() {
  Serial.println("[CFG] Factory reset: erasing LittleFS and NVS");
  LittleFS.end();
  LittleFS.format();

  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();
}

// =====================
// ENTRY POINT
// =====================
void loadSettings() {
  loadNvs();

  if (!LittleFS.begin(true)) {
    Serial.println("[CFG] LittleFS mount failed, running on defaults");
    loadDefaultTargets();
    return;
  }

  if (loadConfigFile()) {
    Serial.printf("[CFG] Loaded %s\n", CONFIG_PATH);
    return;
  }

  if (migrateLegacyNvs()) {
    Serial.println("[CFG] Migrated settings from legacy NVS layout");
  } else {
    loadDefaultTargets();
    Serial.println("[CFG] No config found, using defaults");
  }

  saveSettings();
}
