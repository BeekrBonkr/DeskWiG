#include "PingService.h"

#include <WiFiClient.h>
#include "../web/Settings.h"

constexpr uint32_t PING_TIMEOUT_MS  = 2000;
constexpr uint32_t SUPPRESS_MS      = 60000;
constexpr uint16_t FALLBACK_PORT    = 22;

int tcpPing(const char* host, uint16_t port, uint32_t timeout) {
  WiFiClient client;
  uint32_t start = millis();
  if (!client.connect(host, port, timeout)) return -1;
  int latency = millis() - start;
  client.stop();
  return latency;
}

static void process(PingTarget& t, uint32_t now) {
  if (t.host[0] == '\0') return;
  if (now < t.suppressUntil) return;

  uint16_t port = (t.port == 0) ? FALLBACK_PORT : t.port;
  int result = tcpPing(t.host, port, PING_TIMEOUT_MS);

  t.history[t.historyPos] = (result >= 0);
  t.historyPos = (t.historyPos + 1) % 8;

  if (result >= 0) {
    t.lastLatency = t.latency;
    t.latency = result;
    t.failCount = 0;
    t.suppressUntil = 0;
  } else {
    t.latency = -1;
    if (++t.failCount >= PING_MAX_FAILS) {
      t.suppressUntil = now + SUPPRESS_MS;
    }
  }
}

void pingLoop(uint32_t now) {
  static uint32_t lastPing = 0;
  if (now - lastPing < settings.pingIntervalMs) return;
  lastPing = now;

  for (uint8_t i = 0; i < settings.targetCount; i++) {
    process(settings.targets[i], now);
  }
}
