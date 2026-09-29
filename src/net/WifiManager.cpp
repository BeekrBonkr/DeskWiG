#include "WifiManager.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include "../web/Settings.h"
#include "../app/Log.h"

WifiState wifiState = WifiState::CONNECTING;
ApReason apReason = ApReason::NONE;

static const char* AP_SSID = "DeskWiG-Setup";
static const char* AP_PASS = "configureme";

static const uint32_t CONNECT_TIMEOUT_MS   = 15000;  // first connect after boot / join
static const uint32_t RECONNECT_TIMEOUT_MS = 60000;  // after a drop; avoids flapping to AP
static const uint32_t AP_RETRY_MS          = 60000;  // retry saved network while hotspot is up
static const uint32_t AP_RETRY_MAX_MS      = 480000; // backoff ceiling for repeated failures
static const uint32_t AP_GRACE_MS          = 20000;  // keep hotspot up after joining via portal

static DNSServer dns;
static bool apActive = false;
static bool everConnected = false;
static bool mdnsStarted = false;
static uint32_t connectStart = 0;
static uint32_t lastApRetry = 0;
static uint32_t apDropAt = 0;
static uint8_t  connectFailures = 0;   // consecutive failed attempts; drives retry backoff

static bool hasCredentials() {
  return settings.wifiSSID[0] != '\0';
}

// =====================
// INTERNAL TRANSITIONS
// =====================
static void startAp(ApReason reason) {
  if (!apActive) {
    // AP_STA so we can scan and retry the saved network without dropping the hotspot.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", WiFi.softAPIP());
    apActive = true;
    Log.printf("[WIFI] Hotspot up: %s  http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  }
  apReason = reason;
  wifiState = WifiState::AP_MODE;
  lastApRetry = millis();
}

static void stopAp() {
  if (!apActive) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  Log.println("[WIFI] Hotspot stopped");
}

static void startConnect() {
  WiFi.setHostname(settings.hostname);
  if (!apActive) WiFi.mode(WIFI_STA);
  // The core retries indefinitely on its own while this is set; stopConnect()
  // clears it once we give up so the STA radio goes quiet.
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.wifiSSID, settings.wifiPass);
  connectStart = millis();
  wifiState = WifiState::CONNECTING;
  Log.printf("[WIFI] Connecting to %s\n", settings.wifiSSID);
}

// Stop the station trying. Without this the core keeps reconnecting to a
// wrong password / missing SSID forever, and the constant scanning and
// channel hopping makes the hotspot unreachable even though it is "up".
static void stopConnect() {
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
}

// Retry interval while the hotspot is up: 1, 2, 4, 8 min for consecutive
// failures, so bad credentials don't disrupt the hotspot every minute.
static uint32_t apRetryInterval() {
  uint8_t shift = connectFailures > 1 ? connectFailures - 1 : 0;
  if (shift > 3) shift = 3;
  uint32_t ms = AP_RETRY_MS << shift;
  return ms > AP_RETRY_MAX_MS ? AP_RETRY_MAX_MS : ms;
}

static void startMdns() {
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
  if (MDNS.begin(settings.hostname)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    Log.printf("[WIFI] mDNS: http://%s.local\n", settings.hostname);
  } else {
    Log.println("[WIFI] mDNS failed to start");
  }
}

static void onConnected() {
  everConnected = true;
  connectFailures = 0;
  wifiState = WifiState::CONNECTED;
  apReason = ApReason::NONE;
  Log.printf("[WIFI] Connected, IP %s\n", WiFi.localIP().toString().c_str());
  startMdns();
  if (apActive) apDropAt = millis() + AP_GRACE_MS;
}

// =====================
// PUBLIC API
// =====================
void wifiBegin(bool forceAp) {
  WiFi.persistent(false);
  WiFi.setHostname(settings.hostname);

  if (forceAp) {
    startAp(ApReason::FORCED);
    return;
  }
  if (!hasCredentials()) {
    startAp(ApReason::NO_CREDENTIALS);
    return;
  }
  startConnect();
}

void wifiLoop() {
  uint32_t now = millis();
  if (apActive) dns.processNextRequest();

  switch (wifiState) {
    case WifiState::CONNECTING: {
      if (WiFi.status() == WL_CONNECTED) {
        onConnected();
        break;
      }
      // A drop from a working connection gets a long grace period so a
      // router reboot doesn't flap us to the hotspot. Retries made while
      // the hotspot is already up fail fast to keep it usable.
      uint32_t timeout = (everConnected && !apActive) ? RECONNECT_TIMEOUT_MS : CONNECT_TIMEOUT_MS;
      if (now - connectStart > timeout) {
        if (connectFailures < 255) connectFailures++;
        Log.printf("[WIFI] Connect timed out (status %d, %u failures)\n", (int)WiFi.status(), connectFailures);
        stopConnect();
        startAp(ApReason::CONNECT_FAILED);
      }
      break;
    }

    case WifiState::CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WIFI] Connection lost, reconnecting");
        connectStart = now;
        wifiState = WifiState::CONNECTING;
        break;
      }
      if (apActive && (int32_t)(now - apDropAt) >= 0) stopAp();
      break;

    case WifiState::AP_MODE:
      // Periodically retry the saved network so a router reboot heals itself.
      if (apReason != ApReason::FORCED && hasCredentials() && now - lastApRetry > apRetryInterval()) {
        lastApRetry = now;
        startConnect();
      }
      break;
  }
}

void wifiJoin(const char* ssid, const char* pass) {
  strlcpy(settings.wifiSSID, ssid, sizeof(settings.wifiSSID));
  strlcpy(settings.wifiPass, pass, sizeof(settings.wifiPass));
  saveCredentials();

  // A wrong password should fail fast (15 s) and land back on the hotspot.
  everConnected = false;
  connectFailures = 0;
  stopConnect();
  startConnect();
}

void wifiForget() {
  settings.wifiSSID[0] = '\0';
  settings.wifiPass[0] = '\0';
  saveCredentials();
  stopConnect();
  everConnected = false;
  connectFailures = 0;
  startAp(ApReason::NO_CREDENTIALS);
}

void wifiApplyHostname() {
  WiFi.setHostname(settings.hostname);
  if (wifiState == WifiState::CONNECTED) startMdns();
}

bool wifiApActive()      { return apActive; }
bool wifiEverConnected() { return everConnected; }
const char* wifiApSsid()     { return AP_SSID; }
const char* wifiApPassword() { return AP_PASS; }

const char* wifiStateName() {
  switch (wifiState) {
    case WifiState::CONNECTING: return "connecting";
    case WifiState::CONNECTED:  return "connected";
    case WifiState::AP_MODE:    return "ap";
  }
  return "";
}

const char* wifiApReasonName() {
  switch (apReason) {
    case ApReason::NO_CREDENTIALS: return "no_credentials";
    case ApReason::CONNECT_FAILED: return "connect_failed";
    case ApReason::FORCED:         return "forced";
    default:                       return "none";
  }
}
