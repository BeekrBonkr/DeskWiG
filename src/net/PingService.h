#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PingTarget.h"

// A target is considered down after this many consecutive failures.
constexpr uint8_t PING_MAX_FAILS = 3;
constexpr uint32_t PING_MIN_INTERVAL_MS = 1000;

// TCP connect latency in ms, or -1 on failure. Blocks up to `timeout`.
int tcpPing(const char* host, uint16_t port, uint32_t timeout);

// Runs one round of pings against settings.targets every pingIntervalMs.
// Any widget that shows ping data calls this from update() so the data
// is only refreshed while someone is looking at it.
void pingLoop(uint32_t now);

// Asks for a round as soon as the main loop gets to it, whatever is on
// screen and however recently the last one ran. Used after the targets
// change so the new ones do not sit at "wait" until a ping widget shows.
void pingRequestRound();
bool pingRoundPending();

// Live state as the layout keys show it: ok/wait/down, a color role,
// the last 8 results as "||.|||||" (9 bytes with the terminator) and
// "^", "v", ">" or " " for the latency trend.
const char* pingStatus(const PingTarget& t);
const char* pingColor(const PingTarget& t);
void        pingBars(const PingTarget& t, char* out, size_t n);
const char* pingTrend(const PingTarget& t);

const char* pingTypeName(TargetType t);
TargetType  pingTypeFromName(const char* s);

// Fills one slot from its parts. No validation: the config loader uses
// it for whatever was saved, and the JSON setter after checking.
void pingSetTarget(uint8_t i, const char* name, const char* host, uint16_t port, TargetType type);

// Replaces settings.targets with the array of {name, host, port, type}
// objects, validating every entry first; on failure returns false with a
// short reason in err and the targets untouched. Targets that keep their
// host and port keep their latency and history too.
bool pingTargetsFromJson(JsonArrayConst arr, char* err, size_t errLen);

// Appends every target to arr as it is saved, plus its live state (ms,
// status, color, bars, trend) when withStatus is set.
void pingTargetsToJson(JsonArray arr, bool withStatus);
