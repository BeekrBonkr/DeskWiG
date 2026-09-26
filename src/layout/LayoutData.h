#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Template values available to JSON layouts as {key}.
//
//   time, time.sec, time.hour, time.min, time.ampm
//   date, date.day, date.md, date.dow, date.year
//   wifi.ssid, wifi.ip, wifi.rssi, wifi.pct, wifi.bars, wifi.color
//   hostname, uptime, heap
//   ping.count
//   ping.<N|name>.name, .host, .ms, .status, .color, .bars, .trend
//
// Colour keys resolve to a colour role name (ok, warn, bad, dim, text).

// Expands every {key} in tpl into out. Unknown keys become "--".
void layoutExpand(const char* tpl, char* out, size_t outLen);

// Resolves a single key. Returns false (and leaves out empty) if unknown.
bool layoutResolveKey(const char* key, char* out, size_t outLen);

// Fills obj with every available key and its current value.
void layoutFillData(JsonObject obj);
