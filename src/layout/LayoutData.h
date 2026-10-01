#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Template values available to JSON layouts as {key}.
//
//   time, time.sec, time.hour, time.min, time.second, time.ampm
//   date, date.day, date.md, date.dow, date.year, date.month, date.dom
//   wifi.ssid, wifi.ip, wifi.rssi, wifi.pct, wifi.bars, wifi.color
//   hostname, uptime, heap
//   ping.count
//   ping.<N|name>.name, .host, .ms, .status, .color, .bars, .trend
//   api.<source>.<field>, .status, .color, .age, .updated, .error
//   <series key>.min, .max, .avg, .first, .last, .delta, .count, .span
//     for keys sampled over time (see Series.h)
//
// Color keys resolve to a color role name (ok, warn, bad, dim, text).
//
// A brace body that is not a key is evaluated as arithmetic with keys as
// variables, e.g. {round(api.weather.temp * 9/5 + 32, 1)}; see LayoutExpr.h.

// Expands every {key} in tpl into out. Unknown keys become "--".
void layoutExpand(const char* tpl, char* out, size_t outLen);

// Resolves a single key. Returns false (and leaves out empty) if unknown.
bool layoutResolveKey(const char* key, char* out, size_t outLen);

// Fills obj with every available key and its current value.
void layoutFillData(JsonObject obj);
