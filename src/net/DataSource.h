#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Data sources: named HTTP(S) URLs polled on an interval, with JSON paths
// picked out of the response. Every extracted field becomes a layout key,
// {api.<id>.<field>}, alongside {api.<id>.status|color|age|updated|error}.
//
// Like ping, a source is only fetched while a layout that references it is
// on screen (widgets call sourceTouch() from update()), or when a fetch is
// forced from the web UI. Fetches run in their own FreeRTOS task so a slow
// API never stalls the display; results are copied out under a mutex.
//
// HTTPS is encrypted but the server certificate is not verified.

constexpr uint8_t  MAX_SOURCES           = 6;
constexpr uint8_t  MAX_SOURCE_FIELDS     = 8;
constexpr uint8_t  SOURCE_ID_LEN         = 16;
constexpr uint16_t SOURCE_URL_LEN        = 511;   // long enough for Open-Meteo style query strings
constexpr uint8_t  SOURCE_FIELD_NAME_LEN = 16;
constexpr uint8_t  SOURCE_PATH_LEN       = 63;
constexpr uint8_t  SOURCE_VALUE_LEN      = 31;
constexpr uint8_t  SOURCE_HDR_NAME_LEN   = 31;
constexpr uint8_t  SOURCE_HDR_VALUE_LEN  = 127;
constexpr uint8_t  SOURCE_ERROR_LEN      = 47;
constexpr uint32_t SOURCE_MIN_INTERVAL_S = 10;
constexpr uint32_t SOURCE_DEFAULT_INTERVAL_S = 300;
constexpr size_t   SOURCE_BODY_MAX       = 65536;

enum class SourceState : uint8_t { IDLE, FETCHING, OK, ERROR };

struct SourceField {
  char name[SOURCE_FIELD_NAME_LEN + 1];
  char path[SOURCE_PATH_LEN + 1];   // dot path into the JSON, e.g. current.temperature_2m
                                    // or list[0].main.temp. Empty = the whole body as text.
  int8_t decimals;                  // round numbers to this many places; -1 = as received

  // runtime (not persisted)
  char value[SOURCE_VALUE_LEN + 1];
};

struct DataSource {
  char id[SOURCE_ID_LEN + 1];
  char url[SOURCE_URL_LEN + 1];
  uint32_t intervalS;
  char headerName[SOURCE_HDR_NAME_LEN + 1];     // optional request header, e.g. Authorization
  char headerValue[SOURCE_HDR_VALUE_LEN + 1];
  uint8_t fieldCount;
  SourceField fields[MAX_SOURCE_FIELDS];

  // runtime (not persisted)
  SourceState state;
  bool wanted;            // a widget showing this source is on screen
  bool force;             // fetch as soon as possible, ignoring the interval
  bool everOk;
  bool lastFailed;        // most recent attempt failed (values may be stale)
  int httpCode;
  uint32_t lastAttemptMs;
  uint32_t lastOkMs;
  time_t lastOkTime;
  char error[SOURCE_ERROR_LEN + 1];
};

// Creates the mutex and the fetch task. Call once after loadSettings().
void sourcesBegin();

// Lowercase letters, digits and dashes, 1-16 chars.
bool sourceValidId(const char* id);

// Looks a source up by id. Returns nullptr if there is none.
DataSource* sourceFind(const char* id);

// Marks a source as in use so it is (re)fetched on its interval.
void sourceTouch(const char* id, uint32_t now);

// Fetches a source as soon as the task is free, regardless of the interval.
void sourceFetchNow(DataSource& s);

// Resets the runtime state (values, status) of a source.
void sourceResetRuntime(DataSource& s);

// Fills a DataSource from its JSON description, validating every field.
// On failure returns false with a short reason in err and s untouched.
bool sourceFromJson(DataSource& s, JsonVariantConst v, char* err, size_t errLen);

// Writes the persisted part of a source to obj. With includeSecret the
// header value is written as is; otherwise only whether one is set.
void sourceToJson(const DataSource& s, JsonObject obj, bool includeSecret);

// Adds the runtime status (state, error, age, values) to obj.
void sourceStatusToJson(const DataSource& s, JsonObject obj);

const char* sourceStateName(SourceState s);

// Seconds since the last successful fetch, or UINT32_MAX if there was none.
uint32_t sourceAgeS(const DataSource& s);

// Resolves a layout key of the form <id>.<field> (the part after "api.").
// Returns false if the source or field is unknown.
bool sourceResolveKey(const char* rest, char* out, size_t outLen);

// Key discovery: fetch a URL once and list every JSON path in the reply
// with a sample value, so the setup page can offer them as fields. One
// discovery at a time; it runs on the fetch task like a normal fetch.
constexpr uint16_t DISCOVER_MAX_KEYS = 200;
bool sourceDiscoverStart(const char* url, const char* headerName, const char* headerValue, char* err, size_t errLen);
// state (idle|fetching|ok|error), url, error, json (bool), keys [{path, value}], truncated.
void sourceDiscoverToJson(JsonObject obj);

// Serialises settings.sources access. Anything that mutates settings.sources
// or reads runtime values from another task must hold this.
void sourcesLock();
void sourcesUnlock();
