#include "Series.h"

#include <esp_heap_caps.h>
#include "../web/Settings.h"
#include "../layout/LayoutData.h"
#include "DataSource.h"
#include "PingService.h"
#include "../app/Log.h"

struct SeriesBuf {
  char key[SERIES_KEY_LEN + 1];
  float* v;
  uint32_t* t;            // seconds since boot when the sample was taken
  uint16_t cap, count, head;   // head = next slot to write
  uint32_t lastSampleMs;
  uint32_t lastTouchMs;
};
static SeriesBuf bufs[MAX_SERIES];
static SemaphoreHandle_t lock = nullptr;

static void take() { if (lock) xSemaphoreTake(lock, portMAX_DELAY); }
static void give() { if (lock) xSemaphoreGive(lock); }

static void* psAlloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
  return p ? p : malloc(n);
}

static uint32_t nowS() { return millis() / 1000; }

static uint16_t capFor(const SeriesCfg& c) {
  uint32_t n = c.everyS ? c.keepS / c.everyS : 0;
  if (n < 2) n = 2;
  if (n > SERIES_MAX_SAMPLES) n = SERIES_MAX_SAMPLES;
  return (uint16_t)n;
}

static void bufFree(SeriesBuf& b) {
  free(b.v);
  free(b.t);
  memset(&b, 0, sizeof(b));
}

static bool bufAlloc(SeriesBuf& b, const SeriesCfg& c) {
  memset(&b, 0, sizeof(b));
  strlcpy(b.key, c.key, sizeof(b.key));
  b.cap = capFor(c);
  b.v = (float*)psAlloc(sizeof(float) * b.cap);
  b.t = (uint32_t*)psAlloc(sizeof(uint32_t) * b.cap);
  if (!b.v || !b.t) { bufFree(b); return false; }
  return true;
}

bool seriesValidKey(const char* key) {
  if (!key) return false;
  size_t len = strlen(key);
  if (len == 0 || len > SERIES_KEY_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = key[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')) return false;
  }
  return key[0] != '.' && key[len - 1] != '.';
}

int seriesFind(const char* key) {
  if (!key) return -1;
  for (uint8_t i = 0; i < settings.seriesCount; i++) {
    if (!strcmp(settings.series[i].key, key)) return i;
  }
  return -1;
}

bool seriesFromJson(SeriesCfg& s, JsonVariantConst v, char* err, size_t errLen) {
  const char* key = v["key"] | "";
  if (!seriesValidKey(key)) { strlcpy(err, "key must be 1-47 letters, digits, dots, dashes or underscores", errLen); return false; }
  long every = v["every"] | 60L;
  long keep  = v["keep"]  | 3600L;
  if (every < SERIES_MIN_EVERY_S) every = SERIES_MIN_EVERY_S;
  if (every > 86400) every = 86400;
  if (keep < every * 2) keep = every * 2;
  if (keep > (long)SERIES_MAX_KEEP_S) keep = SERIES_MAX_KEEP_S;
  if (keep / every > SERIES_MAX_SAMPLES) {
    snprintf(err, errLen, "keep / every is more than %u samples: sample less often or keep less", SERIES_MAX_SAMPLES);
    return false;
  }
  memset(&s, 0, sizeof(s));
  strlcpy(s.key, key, sizeof(s.key));
  s.everyS = (uint16_t)every;
  s.keepS = (uint32_t)keep;
  return true;
}

void seriesBegin() {
  if (!lock) lock = xSemaphoreCreateMutex();
  seriesApply();
}

void seriesApply() {
  take();
  SeriesBuf old[MAX_SERIES];
  memcpy(old, bufs, sizeof(bufs));
  memset(bufs, 0, sizeof(bufs));
  for (uint8_t i = 0; i < settings.seriesCount; i++) {
    const SeriesCfg& c = settings.series[i];
    bool kept = false;
    for (auto& o : old) {
      if (o.v && !strcmp(o.key, c.key) && o.cap == capFor(c)) {
        bufs[i] = o;
        memset(&o, 0, sizeof(o));
        kept = true;
        break;
      }
    }
    if (!kept && !bufAlloc(bufs[i], c)) Log.printf("[SERIES] No memory for %s\n", c.key);
  }
  for (auto& o : old) if (o.v) bufFree(o);
  give();
}

static void push(SeriesBuf& b, float v, uint32_t t) {
  b.v[b.head] = v;
  b.t[b.head] = t;
  b.head = (b.head + 1) % b.cap;
  if (b.count < b.cap) b.count++;
}

void seriesLoop(uint32_t now) {
  for (uint8_t i = 0; i < settings.seriesCount; i++) {
    SeriesBuf& b = bufs[i];
    if (!b.v) continue;
    const SeriesCfg& c = settings.series[i];

    // Keep whatever feeds the key alive, as a widget on screen would.
    if (now - b.lastTouchMs >= 1000 || b.lastTouchMs == 0) {
      b.lastTouchMs = now;
      if (!strncmp(c.key, "api.", 4)) {
        const char* end = strchr(c.key + 4, '.');
        size_t len = end ? (size_t)(end - c.key - 4) : strlen(c.key + 4);
        if (len > 0 && len <= SOURCE_ID_LEN) {
          char id[SOURCE_ID_LEN + 1];
          memcpy(id, c.key + 4, len);
          id[len] = '\0';
          sourceTouch(id, now);
        }
      }
    }
    if (!strncmp(c.key, "ping.", 5)) pingLoop(now);

    if (b.lastSampleMs != 0 && now - b.lastSampleMs < (uint32_t)c.everyS * 1000) continue;
    char val[32];
    double d = 0;
    bool ok = false;
    if (layoutResolveKey(c.key, val, sizeof(val))) {
      char* end;
      d = strtod(val, &end);
      ok = end != val;
    }
    if (!ok) {
      // Nothing to record yet (source not fetched, target waiting): look again in a second.
      b.lastSampleMs = now - (uint32_t)c.everyS * 1000 + 1000;
      continue;
    }
    b.lastSampleMs = now;
    take();
    push(b, (float)d, nowS());
    give();
  }
}

// Index of the j-th oldest sample.
static inline uint16_t slot(const SeriesBuf& b, uint16_t j) {
  return (uint16_t)((b.head + b.cap - b.count + j) % b.cap);
}

uint16_t seriesSamples(int idx, uint32_t windowS, float* v, uint32_t* ageS, uint16_t max) {
  if (idx < 0 || idx >= settings.seriesCount || !bufs[idx].v) return 0;
  uint16_t n = 0;
  uint32_t t0 = nowS();
  take();
  const SeriesBuf& b = bufs[idx];
  for (uint16_t j = 0; j < b.count && n < max; j++) {
    uint16_t k = slot(b, j);
    uint32_t age = t0 - b.t[k];
    if (windowS && age > windowS) continue;
    v[n] = b.v[k];
    ageS[n] = age;
    n++;
  }
  give();
  return n;
}

bool seriesStats(int idx, uint32_t windowS, SeriesStats& out) {
  memset(&out, 0, sizeof(out));
  if (idx < 0 || idx >= settings.seriesCount || !bufs[idx].v) return false;
  uint32_t t0 = nowS();
  double sum = 0;
  uint32_t oldest = 0, newest = 0;
  take();
  const SeriesBuf& b = bufs[idx];
  for (uint16_t j = 0; j < b.count; j++) {
    uint16_t k = slot(b, j);
    uint32_t age = t0 - b.t[k];
    if (windowS && age > windowS) continue;
    float x = b.v[k];
    if (out.count == 0) { out.min = out.max = out.first = x; oldest = b.t[k]; }
    if (x < out.min) out.min = x;
    if (x > out.max) out.max = x;
    out.last = x;
    newest = b.t[k];
    sum += x;
    out.count++;
  }
  give();
  if (out.count) {
    out.avg = (float)(sum / out.count);
    out.spanS = newest - oldest;
  }
  return true;
}

void seriesToJson(JsonArray arr, const char* samplesFor) {
  for (uint8_t i = 0; i < settings.seriesCount; i++) {
    const SeriesCfg& c = settings.series[i];
    JsonObject o = arr.add<JsonObject>();
    o["key"]   = c.key;
    o["every"] = c.everyS;
    o["keep"]  = c.keepS;
    o["cap"]   = capFor(c);
    SeriesStats st;
    seriesStats(i, 0, st);
    o["count"] = st.count;
    if (st.count) {
      o["span"] = st.spanS;
      o["min"]  = st.min;
      o["max"]  = st.max;
      o["avg"]  = st.avg;
      o["last"] = st.last;
    }
    if (samplesFor && !strcmp(samplesFor, c.key) && bufs[i].v) {
      JsonArray s = o["samples"].to<JsonArray>();
      uint32_t t0 = nowS();
      take();
      const SeriesBuf& b = bufs[i];
      for (uint16_t j = 0; j < b.count; j++) {
        uint16_t k = slot(b, j);
        JsonArray pair = s.add<JsonArray>();
        pair.add(t0 - b.t[k]);
        pair.add(b.v[k]);
      }
      give();
    }
  }
}
