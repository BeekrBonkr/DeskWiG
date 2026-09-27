#include "DataSource.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <esp_heap_caps.h>

#include "WifiManager.h"
#include "../web/Settings.h"

static const uint32_t CONNECT_TIMEOUT_MS = 5000;
static const uint32_t READ_TIMEOUT_MS    = 8000;
static const uint32_t RETRY_AFTER_ERROR_S = 30;
static const uint32_t TASK_STACK         = 12288;

static SemaphoreHandle_t lock = nullptr;

void sourcesLock()   { if (lock) xSemaphoreTake(lock, portMAX_DELAY); }
void sourcesUnlock() { if (lock) xSemaphoreGive(lock); }

// =====================
// LOOKUP / STATE
// =====================
bool sourceValidId(const char* id) {
  if (!id) return false;
  size_t len = strlen(id);
  if (len == 0 || len > SOURCE_ID_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

DataSource* sourceFind(const char* id) {
  if (!id) return nullptr;
  for (uint8_t i = 0; i < settings.sourceCount; i++) {
    if (!strcmp(settings.sources[i].id, id)) return &settings.sources[i];
  }
  return nullptr;
}

void sourceResetRuntime(DataSource& s) {
  s.state = SourceState::IDLE;
  s.wanted = false;
  s.force = false;
  s.everOk = false;
  s.lastFailed = false;
  s.httpCode = 0;
  s.lastAttemptMs = 0;
  s.lastOkMs = 0;
  s.lastOkTime = 0;
  s.error[0] = '\0';
  for (uint8_t i = 0; i < MAX_SOURCE_FIELDS; i++) strlcpy(s.fields[i].value, "--", sizeof(s.fields[i].value));
}

void sourceTouch(const char* id, uint32_t now) {
  (void)now;
  sourcesLock();
  DataSource* s = sourceFind(id);
  if (s) s->wanted = true;
  sourcesUnlock();
}

void sourceFetchNow(DataSource& s) {
  s.wanted = true;
  s.force = true;
}

const char* sourceStateName(SourceState s) {
  switch (s) {
    case SourceState::FETCHING: return "fetching";
    case SourceState::OK:       return "ok";
    case SourceState::ERROR:    return "error";
    default:                    return "idle";
  }
}

uint32_t sourceAgeS(const DataSource& s) {
  if (!s.everOk) return UINT32_MAX;
  return (millis() - s.lastOkMs) / 1000;
}

// =====================
// JSON (CONFIG + API)
// =====================
static bool validUrl(const char* url) {
  size_t len = strlen(url);
  if (len < 8 || len > SOURCE_URL_LEN) return false;
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return false;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = url[i];
    if (c <= ' ' || c >= 127) return false;
  }
  return true;
}

static bool validFieldName(const char* name) {
  size_t len = strlen(name);
  if (len == 0 || len > SOURCE_FIELD_NAME_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  // Reserved for the built-in status keys.
  return strcmp(name, "status") && strcmp(name, "color") && strcmp(name, "age") &&
         strcmp(name, "updated") && strcmp(name, "error");
}

static bool validPath(const char* path) {
  size_t len = strlen(path);
  if (len > SOURCE_PATH_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = path[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == '_' || c == '-' || c == '.' || c == '[' || c == ']')) return false;
  }
  return true;
}

static bool validHeaderName(const char* name) {
  size_t len = strlen(name);
  if (len > SOURCE_HDR_NAME_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}

static bool validHeaderValue(const char* value) {
  size_t len = strlen(value);
  if (len > SOURCE_HDR_VALUE_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = value[i];
    if (c < ' ' || c >= 127) return false;
  }
  return true;
}

bool sourceFromJson(DataSource& s, JsonVariantConst v, char* err, size_t errLen) {
  if (errLen) err[0] = '\0';
  if (!v.is<JsonObjectConst>()) { strlcpy(err, "source must be a JSON object", errLen); return false; }

  const char* id = v["id"] | "";
  if (!sourceValidId(id)) { strlcpy(err, "invalid id: use 1-16 lowercase letters, digits and dashes", errLen); return false; }

  const char* url = v["url"] | "";
  if (!validUrl(url)) { strlcpy(err, "invalid url: must start with http:// or https:// (max 191 chars)", errLen); return false; }

  const char* hName  = v["header"]["name"]  | "";
  const char* hValue = v["header"]["value"] | "";
  if (!validHeaderName(hName))   { strlcpy(err, "invalid header name", errLen); return false; }
  if (!validHeaderValue(hValue)) { strlcpy(err, "invalid header value", errLen); return false; }
  if (hName[0] == '\0' && hValue[0] != '\0') { strlcpy(err, "header value given without a name", errLen); return false; }

  JsonArrayConst fields = v["fields"].as<JsonArrayConst>();
  if (fields.isNull() || fields.size() == 0) { strlcpy(err, "add at least one field", errLen); return false; }
  if (fields.size() > MAX_SOURCE_FIELDS) { snprintf(err, errLen, "too many fields (max %u)", MAX_SOURCE_FIELDS); return false; }

  DataSource tmp;
  memset(&tmp, 0, sizeof(tmp));
  strlcpy(tmp.id, id, sizeof(tmp.id));
  strlcpy(tmp.url, url, sizeof(tmp.url));
  strlcpy(tmp.headerName, hName, sizeof(tmp.headerName));
  strlcpy(tmp.headerValue, hValue, sizeof(tmp.headerValue));

  uint32_t interval = v["intervalS"] | SOURCE_DEFAULT_INTERVAL_S;
  tmp.intervalS = interval < SOURCE_MIN_INTERVAL_S ? SOURCE_MIN_INTERVAL_S : interval;

  int idx = 0;
  for (JsonVariantConst f : fields) {
    const char* name = f["name"] | "";
    const char* path = f["path"] | "";
    if (!validFieldName(name)) {
      snprintf(err, errLen, "field %d: name must be 1-16 letters, digits or _ (not a status key)", idx);
      return false;
    }
    if (!validPath(path)) { snprintf(err, errLen, "field %d: invalid path", idx); return false; }
    for (int j = 0; j < idx; j++) {
      if (!strcmp(tmp.fields[j].name, name)) { snprintf(err, errLen, "field %d: duplicate name", idx); return false; }
    }
    SourceField& sf = tmp.fields[idx];
    strlcpy(sf.name, name, sizeof(sf.name));
    strlcpy(sf.path, path, sizeof(sf.path));
    int dec = f["decimals"] | -1;
    sf.decimals = (dec < 0) ? -1 : (dec > 6 ? 6 : dec);
    idx++;
  }
  tmp.fieldCount = idx;

  sourceResetRuntime(tmp);
  s = tmp;
  return true;
}

void sourceToJson(const DataSource& s, JsonObject obj, bool includeSecret) {
  obj["id"]        = s.id;
  obj["url"]       = s.url;
  obj["intervalS"] = s.intervalS;
  if (s.headerName[0]) {
    JsonObject h = obj["header"].to<JsonObject>();
    h["name"] = s.headerName;
    if (includeSecret) h["value"] = s.headerValue;
    else               h["set"]   = s.headerValue[0] != '\0';
  }
  JsonArray fields = obj["fields"].to<JsonArray>();
  for (uint8_t i = 0; i < s.fieldCount; i++) {
    JsonObject f = fields.add<JsonObject>();
    f["name"] = s.fields[i].name;
    f["path"] = s.fields[i].path;
    if (s.fields[i].decimals >= 0) f["decimals"] = s.fields[i].decimals;
  }
}

void sourceStatusToJson(const DataSource& s, JsonObject obj) {
  obj["state"]    = sourceStateName(s.state);
  obj["error"]    = s.error;
  obj["httpCode"] = s.httpCode;
  obj["everOk"]   = s.everOk;
  obj["stale"]    = s.everOk && s.lastFailed;
  uint32_t age = sourceAgeS(s);
  if (age != UINT32_MAX) obj["ageS"] = age;
  JsonObject values = obj["values"].to<JsonObject>();
  for (uint8_t i = 0; i < s.fieldCount; i++) values[s.fields[i].name] = s.fields[i].value;
}

// =====================
// LAYOUT KEYS
// =====================
static void formatAge(uint32_t age, char* out, size_t n) {
  if (age == UINT32_MAX)  strlcpy(out, "--", n);
  else if (age < 60)      snprintf(out, n, "%us", (unsigned)age);
  else if (age < 3600)    snprintf(out, n, "%um", (unsigned)(age / 60));
  else if (age < 86400)   snprintf(out, n, "%uh", (unsigned)(age / 3600));
  else                    snprintf(out, n, "%ud", (unsigned)(age / 86400));
}

bool sourceResolveKey(const char* rest, char* out, size_t n) {
  const char* dot = strchr(rest, '.');
  if (!dot) return false;
  size_t idLen = dot - rest;
  if (idLen == 0 || idLen > SOURCE_ID_LEN) return false;
  char id[SOURCE_ID_LEN + 1];
  memcpy(id, rest, idLen);
  id[idLen] = '\0';
  const char* field = dot + 1;

  sourcesLock();
  DataSource* s = sourceFind(id);
  if (!s) { sourcesUnlock(); return false; }

  bool found = true;
  uint32_t age = sourceAgeS(*s);
  bool stale = s->everOk && (s->lastFailed || age > s->intervalS * 3);

  if (!strcmp(field, "status")) {
    strlcpy(out, s->everOk ? (stale ? "stale" : "ok") : (s->state == SourceState::ERROR ? "error" : "wait"), n);
  } else if (!strcmp(field, "color")) {
    strlcpy(out, s->everOk ? (stale ? "warn" : "ok") : (s->state == SourceState::ERROR ? "bad" : "dim"), n);
  } else if (!strcmp(field, "age")) {
    formatAge(age, out, n);
  } else if (!strcmp(field, "updated")) {
    if (s->lastOkTime > 0) {
      tm lt;
      localtime_r(&s->lastOkTime, &lt);
      int h = lt.tm_hour;
      if (!settings.clock24h) { h %= 12; if (h == 0) h = 12; }
      snprintf(out, n, "%02d:%02d", h, lt.tm_min);
    } else {
      strlcpy(out, "--:--", n);
    }
  } else if (!strcmp(field, "error")) {
    strlcpy(out, s->error, n);
  } else {
    found = false;
    for (uint8_t i = 0; i < s->fieldCount; i++) {
      if (!strcmp(s->fields[i].name, field)) {
        strlcpy(out, s->fields[i].value, n);
        found = true;
        break;
      }
    }
  }
  sourcesUnlock();
  return found;
}

// =====================
// PATHS
// =====================
// "list[0].main.temp" -> segments "list", "0", "main", "temp".
static int splitPath(const char* path, char* buf, size_t bufLen, const char* segs[], int maxSegs) {
  strlcpy(buf, path, bufLen);
  int n = 0;
  char* p = buf;
  while (*p && n < maxSegs) {
    while (*p == '.' || *p == '[' || *p == ']') p++;
    if (!*p) break;
    segs[n++] = p;
    while (*p && *p != '.' && *p != '[' && *p != ']') p++;
    if (*p) *p++ = '\0';
  }
  return n;
}

static bool isIndex(const char* seg) {
  if (!*seg) return false;
  for (const char* c = seg; *c; c++) if (*c < '0' || *c > '9') return false;
  return true;
}

// Adds a path to an ArduinoJson filter document. Array indexes become
// element 0 of a filter array, which ArduinoJson applies to every element.
static void addFilterPath(JsonDocument& filter, const char* path) {
  char buf[SOURCE_PATH_LEN + 1];
  const char* segs[16];
  int n = splitPath(path, buf, sizeof(buf), segs, 16);
  if (n == 0) return;

  JsonVariant node = filter.as<JsonVariant>();
  for (int i = 0; i < n; i++) {
    if (node.is<bool>()) return;              // a shorter path already keeps this whole subtree
    bool last = (i + 1 == n);
    if (isIndex(segs[i])) {
      JsonArray arr = node.is<JsonArray>() ? node.as<JsonArray>() : node.to<JsonArray>();
      if (arr.size() == 0) arr.add<JsonVariant>();
      if (last) { if (arr[0].isNull()) arr[0].set(true); return; }
      node = arr[0];
    } else {
      JsonObject obj = node.is<JsonObject>() ? node.as<JsonObject>() : node.to<JsonObject>();
      if (last) { if (obj[segs[i]].isNull()) obj[segs[i]].set(true); return; }
      if (obj[segs[i]].isNull()) obj[segs[i]].to<JsonVariant>();
      node = obj[segs[i]];
    }
  }
}

static JsonVariantConst navigate(JsonVariantConst root, const char* path) {
  char buf[SOURCE_PATH_LEN + 1];
  const char* segs[16];
  int n = splitPath(path, buf, sizeof(buf), segs, 16);
  JsonVariantConst node = root;
  for (int i = 0; i < n && !node.isNull(); i++) {
    if (node.is<JsonArrayConst>() && isIndex(segs[i])) node = node[atoi(segs[i])];
    else node = node[segs[i]];
  }
  return node;
}

static void trimCopy(const char* in, size_t inLen, char* out, size_t n) {
  while (inLen && (in[0] == ' ' || in[0] == '\n' || in[0] == '\r' || in[0] == '\t')) { in++; inLen--; }
  while (inLen && (in[inLen - 1] == ' ' || in[inLen - 1] == '\n' || in[inLen - 1] == '\r' || in[inLen - 1] == '\t')) inLen--;
  size_t len = inLen < n - 1 ? inLen : n - 1;
  memcpy(out, in, len);
  out[len] = '\0';
}

static void formatValue(JsonVariantConst v, int8_t decimals, char* out, size_t n) {
  if (v.isNull()) { strlcpy(out, "--", n); return; }
  if (v.is<const char*>()) { trimCopy(v.as<const char*>(), strlen(v.as<const char*>()), out, n); return; }
  if (v.is<bool>()) { strlcpy(out, v.as<bool>() ? "true" : "false", n); return; }
  if (decimals >= 0 && v.is<double>()) { snprintf(out, n, "%.*f", decimals, v.as<double>()); return; }
  serializeJson(v, out, n);
}

// =====================
// FETCH
// =====================
// Collects a response body into a fixed buffer; HTTPClient handles
// chunked encoding for us when we go through writeToStream().
class BufferStream : public Stream {
public:
  BufferStream(uint8_t* buf, size_t cap) : _buf(buf), _cap(cap) {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    if (_len + n > _cap) { overflow = true; n = _cap - _len; }
    memcpy(_buf + _len, b, n);
    _len += n;
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t length() const { return _len; }
  bool overflow = false;
private:
  uint8_t* _buf;
  size_t _cap;
  size_t _len = 0;
};

// A snapshot of the config the task works from, so the lock is not held
// during the network round trip.
struct FetchJob {
  bool discover;                    // list keys instead of extracting fields
  char id[SOURCE_ID_LEN + 1];
  char url[SOURCE_URL_LEN + 1];
  char headerName[SOURCE_HDR_NAME_LEN + 1];
  char headerValue[SOURCE_HDR_VALUE_LEN + 1];
  uint8_t fieldCount;
  struct { char name[SOURCE_FIELD_NAME_LEN + 1]; char path[SOURCE_PATH_LEN + 1]; int8_t decimals; } fields[MAX_SOURCE_FIELDS];
};

struct FetchResult {
  bool ok;
  int httpCode;
  char error[SOURCE_ERROR_LEN + 1];
  char values[MAX_SOURCE_FIELDS][SOURCE_VALUE_LEN + 1];
};

static void extract(const FetchJob& job, const uint8_t* body, size_t len, FetchResult& r) {
  bool needJson = false;
  JsonDocument filter;
  for (uint8_t i = 0; i < job.fieldCount; i++) {
    if (job.fields[i].path[0]) { addFilterPath(filter, job.fields[i].path); needJson = true; }
  }

  JsonDocument doc;
  if (needJson) {
    DeserializationError err = deserializeJson(doc, (const char*)body, len,
                                               DeserializationOption::Filter(filter),
                                               DeserializationOption::NestingLimit(20));
    if (err) {
      snprintf(r.error, sizeof(r.error), "not JSON: %s", err.c_str());
      r.ok = false;
      return;
    }
  }

  for (uint8_t i = 0; i < job.fieldCount; i++) {
    if (!job.fields[i].path[0]) trimCopy((const char*)body, len, r.values[i], sizeof(r.values[i]));
    else formatValue(navigate(doc.as<JsonVariantConst>(), job.fields[i].path), job.fields[i].decimals, r.values[i], sizeof(r.values[i]));
  }
  r.ok = true;
}

// =====================
// KEY DISCOVERY
// =====================
// Large replies are parsed whole, so the document lives in PSRAM.
struct PsramAllocator : ArduinoJson::Allocator {
  void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); }
  void deallocate(void* p) override { heap_caps_free(p); }
  void* reallocate(void* p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM); }
};
static PsramAllocator psramAlloc;

static struct {
  bool pending;
  SourceState state = SourceState::IDLE;
  char url[SOURCE_URL_LEN + 1];
  char headerName[SOURCE_HDR_NAME_LEN + 1];
  char headerValue[SOURCE_HDR_VALUE_LEN + 1];
  char error[SOURCE_ERROR_LEN + 1];
  bool isJson;
  bool truncated;
  String keys;            // serialised [{path, value}, ...]
} disc;
static String discScratch;  // built on the fetch task, swapped in under the lock

// Walks the document depth first. Arrays contribute only their first
// element: the path uses [0], which is what a field would be written as.
static void flattenInto(JsonVariantConst v, char* path, size_t pathLen, JsonArray out, uint16_t& count, bool& truncated) {
  if (count >= DISCOVER_MAX_KEYS) { truncated = true; return; }
  size_t used = strlen(path);
  if (v.is<JsonObjectConst>()) {
    for (JsonPairConst kv : v.as<JsonObjectConst>()) {
      const char* k = kv.key().c_str();
      if (used + strlen(k) + 2 > pathLen) continue;
      snprintf(path + used, pathLen - used, "%s%s", used ? "." : "", k);
      flattenInto(kv.value(), path, pathLen, out, count, truncated);
      path[used] = '\0';
      if (count >= DISCOVER_MAX_KEYS) return;
    }
  } else if (v.is<JsonArrayConst>()) {
    JsonArrayConst arr = v.as<JsonArrayConst>();
    if (arr.size() == 0 || used + 4 > pathLen) return;
    snprintf(path + used, pathLen - used, "[0]");
    flattenInto(arr[0], path, pathLen, out, count, truncated);
    path[used] = '\0';
  } else {
    if (!used) return;  // a bare scalar body has no path; the whole-body field covers it
    JsonObject o = out.add<JsonObject>();
    o["path"] = path;
    char val[SOURCE_VALUE_LEN + 1];
    formatValue(v, -1, val, sizeof(val));
    o["value"] = val;
    count++;
  }
}

static void discoverFrom(const uint8_t* body, size_t len, FetchResult& r) {
  JsonDocument doc(&psramAlloc);
  DeserializationError err = deserializeJson(doc, (const char*)body, len, DeserializationOption::NestingLimit(20));
  JsonDocument out(&psramAlloc);
  out["json"] = !err;
  bool truncated = false;
  if (err) {
    char sample[SOURCE_VALUE_LEN + 1];
    trimCopy((const char*)body, len, sample, sizeof(sample));
    out["text"] = sample;
  } else {
    JsonArray keys = out["keys"].to<JsonArray>();
    char path[SOURCE_PATH_LEN + 1] = "";
    uint16_t count = 0;
    flattenInto(doc.as<JsonVariantConst>(), path, sizeof(path), keys, count, truncated);
  }
  out["truncated"] = truncated;
  discScratch = "";
  serializeJson(out, discScratch);
  r.ok = true;
}

bool sourceDiscoverStart(const char* url, const char* headerName, const char* headerValue, char* err, size_t errLen) {
  if (!validUrl(url)) { strlcpy(err, "invalid url: must start with http:// or https:// (max 191 chars)", errLen); return false; }
  if (!validHeaderName(headerName) || !validHeaderValue(headerValue)) { strlcpy(err, "invalid header", errLen); return false; }
  sourcesLock();
  if (disc.state == SourceState::FETCHING || disc.pending) {
    sourcesUnlock();
    strlcpy(err, "a discovery is already running", errLen);
    return false;
  }
  strlcpy(disc.url, url, sizeof(disc.url));
  strlcpy(disc.headerName, headerName, sizeof(disc.headerName));
  strlcpy(disc.headerValue, headerValue, sizeof(disc.headerValue));
  disc.error[0] = '\0';
  disc.keys = "";
  disc.pending = true;
  disc.state = SourceState::FETCHING;
  sourcesUnlock();
  return true;
}

void sourceDiscoverToJson(JsonObject obj) {
  sourcesLock();
  obj["state"] = sourceStateName(disc.state);
  obj["url"] = disc.url;
  if (disc.state == SourceState::ERROR) obj["error"] = disc.error;
  if (disc.state == SourceState::OK && disc.keys.length()) {
    // Already serialised; splice it in as raw JSON.
    obj["result"] = serialized(disc.keys);  // String overload copies, so the lock can go
  }
  sourcesUnlock();
}

static void doFetch(const FetchJob& job, FetchResult& r) {
  memset(&r, 0, sizeof(r));
  for (uint8_t i = 0; i < MAX_SOURCE_FIELDS; i++) strlcpy(r.values[i], "--", sizeof(r.values[i]));

  bool https = strncmp(job.url, "https://", 8) == 0;
  WiFiClientSecure secure;
  WiFiClient plain;
  if (https) secure.setInsecure();
  WiFiClient& client = https ? static_cast<WiFiClient&>(secure) : plain;

  HTTPClient http;
  http.setConnectTimeout(CONNECT_TIMEOUT_MS);
  http.setTimeout(READ_TIMEOUT_MS);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("DeskWiG/0.6");

  if (!http.begin(client, job.url)) {
    strlcpy(r.error, "bad url", sizeof(r.error));
    return;
  }
  http.addHeader("Accept", "application/json, text/plain;q=0.8, */*;q=0.5");
  if (job.headerName[0]) http.addHeader(job.headerName, job.headerValue);

  int code = http.GET();
  r.httpCode = code;
  if (code <= 0) {
    snprintf(r.error, sizeof(r.error), "%s", HTTPClient::errorToString(code).c_str());
    http.end();
    return;
  }
  if (code < 200 || code >= 300) {
    snprintf(r.error, sizeof(r.error), "HTTP %d", code);
    http.end();
    return;
  }
  if (http.getSize() > (int)SOURCE_BODY_MAX) {
    snprintf(r.error, sizeof(r.error), "response too large (max %uK)", (unsigned)(SOURCE_BODY_MAX / 1024));
    http.end();
    return;
  }

  size_t cap = SOURCE_BODY_MAX;
  uint8_t* buf = (uint8_t*)ps_malloc(cap);
  if (!buf) { cap = 16384; buf = (uint8_t*)malloc(cap); }
  if (!buf) {
    strlcpy(r.error, "out of memory", sizeof(r.error));
    http.end();
    return;
  }

  BufferStream bs(buf, cap);
  int written = http.writeToStream(&bs);
  http.end();

  if (bs.overflow) {
    snprintf(r.error, sizeof(r.error), "response too large (max %uK)", (unsigned)(cap / 1024));
  } else if (written < 0 && bs.length() == 0) {
    snprintf(r.error, sizeof(r.error), "read failed: %s", HTTPClient::errorToString(written).c_str());
  } else if (job.discover) {
    discoverFrom(buf, bs.length(), r);
  } else {
    extract(job, buf, bs.length(), r);
  }
  free(buf);
}

// Picks the next source that needs fetching and snapshots it into job.
static bool nextJob(FetchJob& job, uint32_t now) {
  bool found = false;
  sourcesLock();
  if (disc.pending) {
    memset(&job, 0, sizeof(job));
    job.discover = true;
    strlcpy(job.url, disc.url, sizeof(job.url));
    strlcpy(job.headerName, disc.headerName, sizeof(job.headerName));
    strlcpy(job.headerValue, disc.headerValue, sizeof(job.headerValue));
    disc.pending = false;
    sourcesUnlock();
    return true;
  }
  for (uint8_t i = 0; i < settings.sourceCount && !found; i++) {
    DataSource& s = settings.sources[i];
    if (s.state == SourceState::FETCHING) continue;
    bool due = s.force;
    if (!due && s.wanted) {
      if (s.lastAttemptMs == 0 && !s.everOk && s.state != SourceState::ERROR) due = true;
      else {
        uint32_t waitS = s.state == SourceState::ERROR
          ? (s.intervalS < RETRY_AFTER_ERROR_S ? s.intervalS : RETRY_AFTER_ERROR_S)
          : s.intervalS;
        due = (now - s.lastAttemptMs) >= waitS * 1000UL;
      }
    }
    if (!due) continue;

    memset(&job, 0, sizeof(job));
    strlcpy(job.id, s.id, sizeof(job.id));
    strlcpy(job.url, s.url, sizeof(job.url));
    strlcpy(job.headerName, s.headerName, sizeof(job.headerName));
    strlcpy(job.headerValue, s.headerValue, sizeof(job.headerValue));
    job.fieldCount = s.fieldCount;
    for (uint8_t f = 0; f < s.fieldCount; f++) {
      strlcpy(job.fields[f].name, s.fields[f].name, sizeof(job.fields[f].name));
      strlcpy(job.fields[f].path, s.fields[f].path, sizeof(job.fields[f].path));
      job.fields[f].decimals = s.fields[f].decimals;
    }
    s.state = SourceState::FETCHING;
    s.lastAttemptMs = now ? now : 1;
    s.wanted = false;
    s.force = false;
    found = true;
  }
  sourcesUnlock();
  return found;
}

static void applyResult(const FetchJob& job, const FetchResult& r) {
  if (job.discover) {
    sourcesLock();
    if (r.ok) { disc.state = SourceState::OK; disc.keys = discScratch; }
    else { disc.state = SourceState::ERROR; strlcpy(disc.error, r.error, sizeof(disc.error)); }
    sourcesUnlock();
    discScratch = "";
    Serial.printf("[SRC] discover: %s%s%s\n", r.ok ? "ok" : "error", r.ok ? "" : " - ", r.ok ? "" : r.error);
    return;
  }
  sourcesLock();
  DataSource* s = sourceFind(job.id);
  if (s && s->state == SourceState::FETCHING) {
    s->httpCode = r.httpCode;
    if (r.ok) {
      s->state = SourceState::OK;
      s->everOk = true;
      s->lastFailed = false;
      s->lastOkMs = millis();
      time(&s->lastOkTime);
      s->error[0] = '\0';
      // Match values by name in case the field list changed mid-fetch.
      for (uint8_t i = 0; i < s->fieldCount; i++) {
        for (uint8_t j = 0; j < job.fieldCount; j++) {
          if (!strcmp(s->fields[i].name, job.fields[j].name)) {
            strlcpy(s->fields[i].value, r.values[j], sizeof(s->fields[i].value));
            break;
          }
        }
      }
    } else {
      s->state = SourceState::ERROR;
      s->lastFailed = true;
      strlcpy(s->error, r.error, sizeof(s->error));
    }
  }
  sourcesUnlock();
  Serial.printf("[SRC] %s: %s%s%s\n", job.id, r.ok ? "ok" : "error", r.ok ? "" : " - ", r.ok ? "" : r.error);
}

static void fetchTask(void*) {
  static FetchJob job;
  static FetchResult result;
  for (;;) {
    if (wifiState != WifiState::CONNECTED || !nextJob(job, millis())) {
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }
    doFetch(job, result);
    applyResult(job, result);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void sourcesBegin() {
  if (lock) return;
  lock = xSemaphoreCreateMutex();
  for (uint8_t i = 0; i < settings.sourceCount; i++) sourceResetRuntime(settings.sources[i]);
  xTaskCreatePinnedToCore(fetchTask, "sources", TASK_STACK, nullptr, 1, nullptr, 0);
  Serial.printf("[SRC] %u data source(s)\n", settings.sourceCount);
}
