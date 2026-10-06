#include "PingService.h"

#include <WiFiClient.h>
#include "../web/Settings.h"

constexpr uint32_t PING_TIMEOUT_MS  = 2000;
constexpr uint32_t SUPPRESS_MS      = 60000;
constexpr uint16_t FALLBACK_PORT    = 22;

static bool roundRequested = false;

int tcpPing(const char* host, uint16_t port, uint32_t timeout) {
  WiFiClient client;
  uint32_t start = millis();
  if (!client.connect(host, port, timeout)) return -1;
  int latency = millis() - start;
  client.stop();
  return latency;
}

static void process(PingTarget& t, uint32_t now) {
  if (t.host[0] == '\0') return;
  if (now < t.suppressUntil) return;

  uint16_t port = (t.port == 0) ? FALLBACK_PORT : t.port;
  int result = tcpPing(t.host, port, PING_TIMEOUT_MS);

  t.history[t.historyPos] = (result >= 0);
  t.historyPos = (t.historyPos + 1) % 8;

  if (result >= 0) {
    t.lastLatency = t.latency;
    t.latency = result;
    t.failCount = 0;
    t.suppressUntil = 0;
  } else {
    t.latency = -1;
    if (++t.failCount >= PING_MAX_FAILS) {
      t.suppressUntil = now + SUPPRESS_MS;
    }
  }
}

void pingLoop(uint32_t now) {
  static uint32_t lastPing = 0;
  bool forced = roundRequested;
  if (!forced && now - lastPing < settings.pingIntervalMs) return;
  lastPing = now;
  roundRequested = false;

  for (uint8_t i = 0; i < settings.targetCount; i++) {
    // A requested round probes everything, including targets paused after
    // repeated failures: the user may just have fixed what was wrong.
    if (forced) settings.targets[i].suppressUntil = 0;
    process(settings.targets[i], now);
  }
}

void pingRequestRound() { roundRequested = true; }
bool pingRoundPending() { return roundRequested; }

// =====================
// STATE AS TEXT
// =====================
const char* pingStatus(const PingTarget& t) {
  if (t.latency >= 0) return "ok";
  return t.failCount >= PING_MAX_FAILS ? "down" : "wait";
}

const char* pingColor(const PingTarget& t) {
  if (t.latency < 0) return t.failCount >= PING_MAX_FAILS ? "bad" : "dim";
  return t.latency < 50 ? "ok" : "warn";
}

void pingBars(const PingTarget& t, char* out, size_t n) {
  char buf[9];
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t idx = (t.historyPos + i) % 8;
    buf[i] = t.history[idx] ? '|' : '.';
  }
  buf[8] = '\0';
  strlcpy(out, buf, n);
}

const char* pingTrend(const PingTarget& t) {
  if (t.lastLatency < 0 || t.latency < 0) return " ";
  return t.latency < t.lastLatency ? "^" : (t.latency > t.lastLatency ? "v" : ">");
}

// =====================
// CONFIG
// =====================
const char* pingTypeName(TargetType t) {
  return t == TargetType::SERVER ? "server" : "service";
}

TargetType pingTypeFromName(const char* s) {
  return (s && strcmp(s, "server") == 0) ? TargetType::SERVER : TargetType::SERVICE;
}

void pingSetTarget(uint8_t i, const char* name, const char* host, uint16_t port, TargetType type) {
  PingTarget& t = settings.targets[i];
  memset(&t, 0, sizeof(PingTarget));
  strlcpy(t.name, name, sizeof(t.name));
  strlcpy(t.host, host, sizeof(t.host));
  t.port = port;
  t.type = type;
  t.latency = -1;
  t.lastLatency = -1;
}

// Printable ASCII, not blank at either end, and not all digits: a layout
// reads a target as ping.<N> or ping.<name>, so "3" would be ambiguous.
static bool validName(const char* name) {
  size_t len = strlen(name);
  if (len == 0 || len >= sizeof(PingTarget::name)) return false;
  if (name[0] == ' ' || name[len - 1] == ' ') return false;
  bool digitsOnly = true;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = name[i];
    if (c < ' ' || c >= 127) return false;
    if (c < '0' || c > '9') digitsOnly = false;
  }
  return !digitsOnly;
}

// A hostname or IPv4 address.
static bool validHost(const char* host) {
  size_t len = strlen(host);
  if (len == 0 || len >= sizeof(PingTarget::host)) return false;
  for (size_t i = 0; i < len; i++) {
    char c = host[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
    if (!ok) return false;
  }
  return host[0] != '.' && host[0] != '-' && host[len - 1] != '.' && host[len - 1] != '-';
}

bool pingTargetsFromJson(JsonArrayConst arr, char* err, size_t errLen) {
  if (errLen) err[0] = '\0';
  if (arr.isNull()) { strlcpy(err, "targets must be an array", errLen); return false; }
  if (arr.size() > MAX_PING_TARGETS) { snprintf(err, errLen, "too many targets (max %u)", MAX_PING_TARGETS); return false; }

  static PingTarget tmp[MAX_PING_TARGETS];
  uint8_t count = 0;
  for (JsonVariantConst v : arr) {
    const char* name = v["name"] | "";
    const char* host = v["host"] | "";
    const char* type = v["type"] | "service";
    int port = v["port"] | 0;
    if (!validName(name)) { snprintf(err, errLen, "target %u: name must be 1-15 characters and not just digits", count); return false; }
    if (!validHost(host)) { snprintf(err, errLen, "target %u: host must be a hostname or IP address", count); return false; }
    if (port < 0 || port > 65535) { snprintf(err, errLen, "target %u: port must be 0-65535", count); return false; }
    if (strcmp(type, "service") && strcmp(type, "server")) { snprintf(err, errLen, "target %u: type must be service or server", count); return false; }
    for (uint8_t j = 0; j < count; j++) {
      if (!strcasecmp(tmp[j].name, name)) { snprintf(err, errLen, "target %u: duplicate name", count); return false; }
    }

    PingTarget& t = tmp[count++];
    memset(&t, 0, sizeof(PingTarget));
    strlcpy(t.name, name, sizeof(t.name));
    strlcpy(t.host, host, sizeof(t.host));
    t.port = (uint16_t)port;
    t.type = pingTypeFromName(type);
    t.latency = -1;
    t.lastLatency = -1;

    // Same host and port as before: carry the measurements over so the
    // screen does not blank for a rename or a regrouping.
    for (uint8_t j = 0; j < settings.targetCount; j++) {
      const PingTarget& old = settings.targets[j];
      if (old.port == t.port && !strcmp(old.host, t.host)) {
        t.latency       = old.latency;
        t.lastLatency   = old.lastLatency;
        t.failCount     = old.failCount;
        t.suppressUntil = old.suppressUntil;
        t.historyPos    = old.historyPos;
        memcpy(t.history, old.history, sizeof(t.history));
        break;
      }
    }
  }

  memcpy(settings.targets, tmp, sizeof(PingTarget) * count);
  settings.targetCount = count;
  return true;
}

void pingTargetsToJson(JsonArray arr, bool withStatus) {
  for (uint8_t i = 0; i < settings.targetCount; i++) {
    const PingTarget& t = settings.targets[i];
    JsonObject o = arr.add<JsonObject>();
    o["name"] = t.name;
    o["host"] = t.host;
    o["port"] = t.port;
    o["type"] = pingTypeName(t.type);
    if (!withStatus) continue;
    if (t.latency >= 0) o["ms"] = t.latency;
    o["status"] = pingStatus(t);
    o["color"]  = pingColor(t);
    char bars[9];
    pingBars(t, bars, sizeof(bars));
    o["bars"]  = bars;
    o["trend"] = pingTrend(t);
  }
}
