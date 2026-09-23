#pragma once
#include <Arduino.h>

// Status LED patterns. See PLANNING §8.1 / README for meanings.
enum class LedPattern : uint8_t {
  OFF,
  AP_MODE,     // blue, 1 Hz blink
  CONNECTING,  // amber, solid
  NORMAL,      // green, solid, dim
  ALERT,       // red, 1 Hz blink
  RESETTING    // white, fast blink
};

void ledBegin();
void ledApplySettings();       // re-read brightness/enabled from settings
void ledSet(LedPattern p);
void ledLoop();
