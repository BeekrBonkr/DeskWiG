#pragma once
#include <Arduino.h>

constexpr uint8_t MAX_PING_TARGETS = 12;

enum class TargetType : uint8_t {
  SERVICE,
  SERVER
};

struct PingTarget {
  char name[16];
  char host[64];
  uint16_t port;
  TargetType type;

  // runtime (not persisted)
  int latency = -1;
  int lastLatency = -1;
  uint8_t failCount = 0;
  uint32_t suppressUntil = 0;
  uint8_t historyPos = 0;
  bool history[8] = {};
};
