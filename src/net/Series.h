#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Series: layout keys sampled on an interval and kept in PSRAM, so a
// layout can draw how a value changed (the chart element) or read its
// minimum, maximum, average and change over the window ({key.min},
// {key.max}, {key.avg}, {key.first}, {key.last}, {key.delta},
// {key.count}, {key.span}).
//
// Each series names a key, how often to sample it and how long to keep
// samples for. A key from a data source keeps that source polling while
// the series exists, whatever is on screen; a ping key keeps pings
// running. Samples live in RAM only, so a reboot starts them over.

constexpr uint8_t  MAX_SERIES          = 12;
constexpr uint8_t  SERIES_KEY_LEN      = 47;
constexpr uint16_t SERIES_MAX_SAMPLES  = 720;
constexpr uint16_t SERIES_MIN_EVERY_S  = 5;
constexpr uint32_t SERIES_MAX_KEEP_S   = 7 * 86400;

struct SeriesCfg {
  char key[SERIES_KEY_LEN + 1];
  uint16_t everyS;
  uint32_t keepS;
};

struct SeriesStats {
  float min, max, avg, first, last;
  uint16_t count;
  uint32_t spanS;     // seconds between the oldest and newest sample counted
};

// Allocates buffers for settings.series. Call once after loadSettings().
void seriesBegin();

// Re-allocates after settings.series changed. Series whose key and size
// are unchanged keep their samples.
void seriesApply();

// Takes samples that are due and keeps the sources they need polling.
// Call from loop().
void seriesLoop(uint32_t now);

// Letters, digits, dots, dashes and underscores, 1-47 chars.
bool seriesValidKey(const char* key);

// Index into settings.series, or -1.
int seriesFind(const char* key);

// Fills a SeriesCfg from {"key","every","keep"}, validating each.
bool seriesFromJson(SeriesCfg& s, JsonVariantConst v, char* err, size_t errLen);

// Samples of series idx no older than windowS seconds (0 = all), oldest
// first. ageS receives how many seconds before now each was taken.
// Returns the number copied, at most max.
uint16_t seriesSamples(int idx, uint32_t windowS, float* v, uint32_t* ageS, uint16_t max);

// Statistics over the samples no older than windowS (0 = all). Returns
// false if the series does not exist; count is 0 when it has no samples.
bool seriesStats(int idx, uint32_t windowS, SeriesStats& out);

// Every series with its config and statistics. The one named by
// samplesFor (may be nullptr) also gets its samples as [ageS, value] pairs.
void seriesToJson(JsonArray arr, const char* samplesFor);
