#pragma once
#include <Arduino.h>

// SNTP + timezone. The clock is UTC from NTP; settings.clockTz (a POSIX TZ
// string such as "EST5EDT,M3.2.0,M11.1.0") turns it into local time, with
// daylight saving handled by the C library.
//
// NTP source options:
//   POOL    pool.ntp.org on the internet (default)
//   ROUTER  the WiFi gateway, for routers that run an NTP server or for
//           networks without internet access
//   CUSTOM  settings.ntpServer

enum class NtpSource : uint8_t { POOL, ROUTER, CUSTOM };

void timeBegin();

// Re-applies timezone and NTP server from settings. Safe to call any time;
// also called automatically when WiFi connects, because the gateway
// address isn't known before that.
void timeApply();

void timeLoop();

bool timeSynced();                 // true once NTP has set the clock
const char* timeServerInUse();     // host or IP currently configured for SNTP
const char* ntpSourceName(NtpSource s);
NtpSource ntpSourceFromName(const char* s);
