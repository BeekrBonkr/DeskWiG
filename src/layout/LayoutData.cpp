#include "LayoutData.h"

#include <WiFi.h>
#include <time.h>

#include "../web/Settings.h"
#include "../net/PingService.h"
#include "../net/WifiManager.h"
#include "../net/DataSource.h"

static const char* DAY_SHORT[]   = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* DAY_LONG[]    = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char* MONTH_SHORT[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Anything before this is "the clock hasn't synced yet".
static const time_t MIN_VALID_TIME = 1600000000;

static bool localTime(tm& out) {
  time_t t;
  time(&t);
  if (t < MIN_VALID_TIME) return false;
  localtime_r(&t, &out);
  return true;
}

static bool wifiUp() {
  return wifiState == WifiState::CONNECTED;
}

static int wifiRssi() {
  return wifiUp() ? WiFi.RSSI() : -100;
}

// =====================
// TIME
// =====================
static bool resolveTime(const char* field, char* out, size_t n) {
  tm t;
  bool ok = localTime(t);

  if (!*field) {
    if (!ok) { strlcpy(out, "--:--", n); return true; }
    int h = t.tm_hour;
    if (!settings.clock24h) { h %= 12; if (h == 0) h = 12; }
    snprintf(out, n, "%02d:%02d", h, t.tm_min);
    return true;
  }
  if (!strcmp(field, "sec")) {
    if (!ok) { strlcpy(out, "--:--:--", n); return true; }
    int h = t.tm_hour;
    if (!settings.clock24h) { h %= 12; if (h == 0) h = 12; }
    snprintf(out, n, "%02d:%02d:%02d", h, t.tm_min, t.tm_sec);
    return true;
  }
  if (!strcmp(field, "hour")) {
    if (!ok) { strlcpy(out, "--", n); return true; }
    int h = t.tm_hour;
    if (!settings.clock24h) { h %= 12; if (h == 0) h = 12; }
    snprintf(out, n, "%02d", h);
    return true;
  }
  if (!strcmp(field, "min")) {
    if (!ok) { strlcpy(out, "--", n); return true; }
    snprintf(out, n, "%02d", t.tm_min);
    return true;
  }
  if (!strcmp(field, "ampm")) {
    if (!ok || settings.clock24h) { out[0] = '\0'; return true; }
    strlcpy(out, t.tm_hour >= 12 ? "PM" : "AM", n);
    return true;
  }
  return false;
}

static bool resolveDate(const char* field, char* out, size_t n) {
  tm t;
  bool ok = localTime(t);

  if (!*field) {
    if (!ok) { strlcpy(out, "----------", n); return true; }
    snprintf(out, n, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return true;
  }
  if (!strcmp(field, "day")) {
    strlcpy(out, ok ? DAY_SHORT[t.tm_wday] : "---", n);
    return true;
  }
  if (!strcmp(field, "dow")) {
    strlcpy(out, ok ? DAY_LONG[t.tm_wday] : "---", n);
    return true;
  }
  if (!strcmp(field, "year")) {
    if (!ok) { strlcpy(out, "----", n); return true; }
    snprintf(out, n, "%04d", t.tm_year + 1900);
    return true;
  }
  if (!strcmp(field, "md")) {
    if (!ok) { strlcpy(out, "--- --", n); return true; }
    snprintf(out, n, "%s %d", MONTH_SHORT[t.tm_mon], t.tm_mday);
    return true;
  }
  return false;
}

// =====================
// WIFI / SYSTEM
// =====================
static bool resolveWifi(const char* field, char* out, size_t n) {
  int rssi = wifiRssi();

  if (!strcmp(field, "ssid")) {
    strlcpy(out, wifiUp() ? WiFi.SSID().c_str() : "offline", n);
    return true;
  }
  if (!strcmp(field, "ip")) {
    strlcpy(out, wifiUp() ? WiFi.localIP().toString().c_str() : "--", n);
    return true;
  }
  if (!strcmp(field, "rssi")) {
    if (!wifiUp()) { strlcpy(out, "--", n); return true; }
    snprintf(out, n, "%d", rssi);
    return true;
  }
  if (!strcmp(field, "pct")) {
    int pct = wifiUp() ? (rssi + 90) * 2 : 0;   // -90 dBm -> 0, -40 dBm -> 100
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    snprintf(out, n, "%d", pct);
    return true;
  }
  if (!strcmp(field, "bars")) {
    const char* bars = "....";
    if (wifiUp()) {
      if (rssi > -55)      bars = "||||";
      else if (rssi > -65) bars = "|||.";
      else if (rssi > -75) bars = "||..";
      else if (rssi > -85) bars = "|...";
    }
    strlcpy(out, bars, n);
    return true;
  }
  if (!strcmp(field, "color")) {
    const char* c = "dim";
    if (wifiUp()) c = rssi > -65 ? "ok" : (rssi > -85 ? "warn" : "bad");
    strlcpy(out, c, n);
    return true;
  }
  return false;
}

static void resolveUptime(char* out, size_t n) {
  uint32_t s = millis() / 1000;
  uint32_t d = s / 86400; s %= 86400;
  uint32_t h = s / 3600;  s %= 3600;
  uint32_t m = s / 60;
  if (d > 0) snprintf(out, n, "%ud %uh", (unsigned)d, (unsigned)h);
  else       snprintf(out, n, "%uh %02um", (unsigned)h, (unsigned)m);
}

// =====================
// PING
// =====================
static PingTarget* findTarget(const char* sel, size_t len) {
  if (len == 0) return nullptr;

  bool numeric = true;
  for (size_t i = 0; i < len; i++) {
    if (sel[i] < '0' || sel[i] > '9') { numeric = false; break; }
  }
  if (numeric) {
    int idx = atoi(String(sel, len).c_str());
    return (idx >= 0 && idx < settings.targetCount) ? &settings.targets[idx] : nullptr;
  }
  for (uint8_t i = 0; i < settings.targetCount; i++) {
    if (strlen(settings.targets[i].name) == len &&
        strncasecmp(settings.targets[i].name, sel, len) == 0) {
      return &settings.targets[i];
    }
  }
  return nullptr;
}

static const char* pingColor(const PingTarget& t) {
  if (t.latency < 0) return t.failCount >= PING_MAX_FAILS ? "bad" : "dim";
  return t.latency < 50 ? "ok" : "warn";
}

static bool resolvePing(const char* rest, char* out, size_t n) {
  if (!strcmp(rest, "count")) {
    snprintf(out, n, "%u", settings.targetCount);
    return true;
  }

  const char* dot = strchr(rest, '.');
  if (!dot) return false;
  PingTarget* t = findTarget(rest, dot - rest);
  if (!t) return false;
  const char* field = dot + 1;

  if (!strcmp(field, "name")) { strlcpy(out, t->name, n); return true; }
  if (!strcmp(field, "host")) { strlcpy(out, t->host, n); return true; }
  if (!strcmp(field, "ms")) {
    if (t->latency < 0) strlcpy(out, "--", n);
    else snprintf(out, n, "%d", t->latency);
    return true;
  }
  if (!strcmp(field, "status")) {
    const char* s = "ok";
    if (t->latency < 0) s = t->failCount >= PING_MAX_FAILS ? "down" : "wait";
    strlcpy(out, s, n);
    return true;
  }
  if (!strcmp(field, "color")) { strlcpy(out, pingColor(*t), n); return true; }
  if (!strcmp(field, "bars")) {
    char buf[9];
    for (uint8_t i = 0; i < 8; i++) {
      uint8_t idx = (t->historyPos + i) % 8;
      buf[i] = t->history[idx] ? '|' : '.';
    }
    buf[8] = '\0';
    strlcpy(out, buf, n);
    return true;
  }
  if (!strcmp(field, "trend")) {
    const char* a = " ";
    if (t->lastLatency >= 0 && t->latency >= 0) {
      a = t->latency < t->lastLatency ? "^" : (t->latency > t->lastLatency ? "v" : ">");
    }
    strlcpy(out, a, n);
    return true;
  }
  return false;
}

// =====================
// PUBLIC
// =====================
bool layoutResolveKey(const char* key, char* out, size_t n) {
  if (n == 0) return false;
  out[0] = '\0';

  if (!strncmp(key, "time", 4) && (key[4] == '\0' || key[4] == '.')) return resolveTime(key[4] ? key + 5 : "", out, n);
  if (!strncmp(key, "date", 4) && (key[4] == '\0' || key[4] == '.')) return resolveDate(key[4] ? key + 5 : "", out, n);
  if (!strncmp(key, "wifi.", 5)) return resolveWifi(key + 5, out, n);
  if (!strncmp(key, "ping.", 5)) return resolvePing(key + 5, out, n);
  if (!strncmp(key, "api.", 4))  return sourceResolveKey(key + 4, out, n);

  if (!strcmp(key, "hostname")) { strlcpy(out, settings.hostname, n); return true; }
  if (!strcmp(key, "uptime"))   { resolveUptime(out, n); return true; }
  if (!strcmp(key, "heap"))     { snprintf(out, n, "%uk", (unsigned)(ESP.getFreeHeap() / 1024)); return true; }
  return false;
}

void layoutExpand(const char* tpl, char* out, size_t outLen) {
  size_t o = 0;
  char key[40];
  char val[64];

  while (*tpl && o + 1 < outLen) {
    if (*tpl == '{') {
      const char* end = strchr(tpl + 1, '}');
      size_t klen = end ? (size_t)(end - tpl - 1) : 0;
      if (end && klen > 0 && klen < sizeof(key)) {
        memcpy(key, tpl + 1, klen);
        key[klen] = '\0';
        if (!layoutResolveKey(key, val, sizeof(val))) strlcpy(val, "--", sizeof(val));
        for (const char* v = val; *v && o + 1 < outLen; v++) out[o++] = *v;
        tpl = end + 1;
        continue;
      }
    }
    out[o++] = *tpl++;
  }
  out[o] = '\0';
}

void layoutFillData(JsonObject obj) {
  static const char* STATIC_KEYS[] = {
    "time", "time.sec", "time.hour", "time.min", "time.ampm", "date", "date.day", "date.md", "date.dow", "date.year",
    "wifi.ssid", "wifi.ip", "wifi.rssi", "wifi.pct", "wifi.bars", "wifi.color",
    "hostname", "uptime", "heap", "ping.count"
  };
  static const char* PING_FIELDS[] = { "name", "host", "ms", "status", "color", "bars", "trend" };

  char val[64];
  for (const char* k : STATIC_KEYS) {
    if (layoutResolveKey(k, val, sizeof(val))) obj[k] = val;
  }
  for (uint8_t i = 0; i < settings.targetCount; i++) {
    for (const char* f : PING_FIELDS) {
      char key[32];
      snprintf(key, sizeof(key), "ping.%u.%s", i, f);
      if (layoutResolveKey(key, val, sizeof(val))) obj[key] = val;
    }
  }

  static const char* SOURCE_FIELDS[] = { "status", "color", "age", "updated" };
  for (uint8_t i = 0; i < settings.sourceCount; i++) {
    const DataSource& s = settings.sources[i];
    char key[48];
    for (uint8_t f = 0; f < s.fieldCount; f++) {
      snprintf(key, sizeof(key), "api.%s.%s", s.id, s.fields[f].name);
      if (layoutResolveKey(key, val, sizeof(val))) obj[key] = val;
    }
    for (const char* f : SOURCE_FIELDS) {
      snprintf(key, sizeof(key), "api.%s.%s", s.id, f);
      if (layoutResolveKey(key, val, sizeof(val))) obj[key] = val;
    }
  }
}
