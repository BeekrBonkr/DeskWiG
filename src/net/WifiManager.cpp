#include "WifiManager.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include "../web/Settings.h"

WifiState wifiState = WifiState::CONNECTING;
ApReason apReason = ApReason::NONE;

static const char* AP_SSID = "ESP32-Widget-Setup";
static const char* AP_PASS = "configureme";

static const uint32_t CONNECT_TIMEOUT_MS   = 15000;  // first connect after boot / join
static const uint32_t RECONNECT_TIMEOUT_MS = 60000;  // after a drop; avoids flapping to AP
static const uint32_t AP_RETRY_MS          = 60000;  // retry saved network while hotspot is up
static const uint32_t AP_GRACE_MS          = 20000;  // keep hotspot up after joining via portal

static DNSServer dns;
static bool apActive = false;
static bool everConnected = false;
static bool mdnsStarted = false;
static uint32_t connectStart = 0;
static uint32_t lastApRetry = 0;
static uint32_t apDropAt = 0;

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
    Serial.printf("[WIFI] Hotspot up: %s  http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
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
  Serial.println("[WIFI] Hotspot stopped");
}

static void startConnect() {
  WiFi.setHostname(settings.hostname);
  if (!apActive) WiFi.mode(WIFI_STA);
  WiFi.begin(settings.wifiSSID, settings.wifiPass);
  connectStart = millis();
  wifiState = WifiState::CONNECTING;
  Serial.printf("[WIFI] Connecting to %s\n", settings.wifiSSID);
}

static void startMdns() {
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
  if (MDNS.begin(settings.hostname)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    Serial.printf("[WIFI] mDNS: http://%s.local\n", settings.hostname);
  } else {
    Serial.println("[WIFI] mDNS failed to start");
  }
}

static void onConnected() {
  everConnected = true;
  wifiState = WifiState::CONNECTED;
  apReason = ApReason::NONE;
  Serial.printf("[WIFI] Connected, IP %s\n", WiFi.localIP().toString().c_str());
  startMdns();
  if (apActive) apDropAt = millis() + AP_GRACE_MS;
}

// =====================
// PUBLIC API
// =====================
void wifiBegin(bool forceAp) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
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
      uint32_t timeout = everConnected ? RECONNECT_TIMEOUT_MS : CONNECT_TIMEOUT_MS;
      if (now - connectStart > timeout) {
        Serial.println("[WIFI] Connect timed out");
        startAp(ApReason::CONNECT_FAILED);
      }
      break;
    }

    case WifiState::CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Connection lost, reconnecting");
        connectStart = now;
        wifiState = WifiState::CONNECTING;
        break;
      }
      if (apActive && (int32_t)(now - apDropAt) >= 0) stopAp();
      break;

    case WifiState::AP_MODE:
      // Periodically retry the saved network so a router reboot heals itself.
      if (apReason != ApReason::FORCED && hasCredentials() && now - lastApRetry > AP_RETRY_MS) {
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
  WiFi.disconnect(false);
  startConnect();
}

void wifiForget() {
  settings.wifiSSID[0] = '\0';
  settings.wifiPass[0] = '\0';
  saveCredentials();
  WiFi.disconnect(false);
  everConnected = false;
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
