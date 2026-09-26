#pragma once
#include <Arduino.h>

// A target is considered down after this many consecutive failures.
constexpr uint8_t PING_MAX_FAILS = 3;

// TCP connect latency in ms, or -1 on failure. Blocks up to `timeout`.
int tcpPing(const char* host, uint16_t port, uint32_t timeout);

// Runs one round of pings against settings.targets every pingIntervalMs.
// Any widget that shows ping data calls this from update() so the data
// is only refreshed while someone is looking at it.
void pingLoop(uint32_t now);
