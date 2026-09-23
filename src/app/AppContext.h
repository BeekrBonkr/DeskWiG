#pragma once
#include <WiFi.h>

struct AppContext {
  uint32_t now;
  bool wifiConnected;
};
