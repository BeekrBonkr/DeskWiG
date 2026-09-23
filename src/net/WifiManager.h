#pragma once
#include <Arduino.h>

enum class WifiState : uint8_t {
  CONNECTING,
  CONNECTED,
  AP_MODE
};

enum class ApReason : uint8_t {
  NONE,
  NO_CREDENTIALS,
  CONNECT_FAILED,
  FORCED
};

extern WifiState wifiState;
extern ApReason apReason;

// forceAp: skip the saved network and start the setup hotspot immediately.
void wifiBegin(bool forceAp = false);
void wifiLoop();

// Save credentials and attempt to join. Safe to call from the hotspot:
// the AP stays up until the connection succeeds (plus a grace period).
void wifiJoin(const char* ssid, const char* pass);

// Clear credentials and return to the setup hotspot.
void wifiForget();

// Restart mDNS after the hostname setting changes.
void wifiApplyHostname();

bool wifiApActive();           // hotspot is up (may be alongside STA)
bool wifiEverConnected();      // connected at least once since boot
const char* wifiApSsid();
const char* wifiApPassword();
const char* wifiStateName();   // "connecting" | "connected" | "ap"
const char* wifiApReasonName();
