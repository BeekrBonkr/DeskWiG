#include "TimeService.h"

#include <WiFi.h>
#include <time.h>

#include "WifiManager.h"
#include "../web/Settings.h"
#include "../app/Log.h"

static const char* POOL_SERVER = "pool.ntp.org";
static const time_t MIN_VALID_TIME = 1600000000;

// SNTP keeps the pointer it is given rather than copying the string, so
// the server name has to live somewhere permanent.
static char serverBuf[64] = "";
static WifiState lastState = WifiState::AP_MODE;

const char* ntpSourceName(NtpSource s) {
  switch (s) {
    case NtpSource::ROUTER: return "router";
    case NtpSource::CUSTOM: return "custom";
    default:                return "pool";
  }
}

NtpSource ntpSourceFromName(const char* s) {
  if (s && !strcmp(s, "router")) return NtpSource::ROUTER;
  if (s && !strcmp(s, "custom")) return NtpSource::CUSTOM;
  return NtpSource::POOL;
}

static void pickServer() {
  switch (settings.ntpSource) {
    case NtpSource::ROUTER: {
      IPAddress gw = WiFi.gatewayIP();
      if (wifiState == WifiState::CONNECTED && gw != IPAddress(0, 0, 0, 0)) {
        strlcpy(serverBuf, gw.toString().c_str(), sizeof(serverBuf));
      } else {
        // Not connected yet: fall back to the pool so the clock still
        // syncs; timeLoop() re-applies once the gateway is known.
        strlcpy(serverBuf, POOL_SERVER, sizeof(serverBuf));
      }
      break;
    }
    case NtpSource::CUSTOM:
      strlcpy(serverBuf, settings.ntpServer[0] ? settings.ntpServer : POOL_SERVER, sizeof(serverBuf));
      break;
    default:
      strlcpy(serverBuf, POOL_SERVER, sizeof(serverBuf));
      break;
  }
}

void timeApply() {
  pickServer();
  configTzTime(settings.clockTz, serverBuf);
  Log.printf("[TIME] NTP %s (%s), TZ %s\n", serverBuf, ntpSourceName(settings.ntpSource), settings.clockTz);
}

void timeBegin() {
  lastState = wifiState;
  timeApply();
}

void timeLoop() {
  // Reconfigure on every (re)connect: the gateway may have changed, and
  // a fresh sntp_init() asks the server immediately instead of waiting
  // for the next poll interval.
  if (wifiState == WifiState::CONNECTED && lastState != WifiState::CONNECTED) {
    timeApply();
  }
  lastState = wifiState;
}

bool timeSynced() {
  return time(nullptr) >= MIN_VALID_TIME;
}

const char* timeServerInUse() {
  return serverBuf;
}
