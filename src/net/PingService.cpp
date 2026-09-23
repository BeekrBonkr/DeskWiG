#include "PingService.h"
#include <Arduino.h>

int tcpPing(const char* host, uint16_t port, uint32_t timeout) {
  WiFiClient client;
  uint32_t start = millis();
  if (!client.connect(host, port, timeout)) return -1;
  int latency = millis() - start;
  client.stop();
  return latency;
}
