#pragma once
#include <Arduino.h>

// Status LED. System states (hotspot, connecting, resetting) own the LED;
// in normal operation the widget on screen does, through a LedSpec.
enum class LedPattern : uint8_t {
  OFF,
  AP_MODE,     // blue, 1 Hz blink
  CONNECTING,  // amber, solid
  NORMAL,      // controlled by the active widget (off if it sets nothing)
  ALERT,       // red, 1 Hz blink
  RESETTING    // white, fast blink
};

enum class LedMode : uint8_t { OFF, SOLID, BREATHE, BLINK, PULSE, RAINBOW };

struct LedSpec {
  LedMode mode = LedMode::OFF;
  uint8_t r = 0, g = 0, b = 0;
  uint16_t speedMs = 2000;     // one cycle of breathe/blink/pulse/rainbow
  int16_t brightness = -1;     // 0-100, or -1 for the device setting
};

const char* ledModeName(LedMode m);
bool ledModeFromName(const char* s, LedMode& out);

void ledBegin();
void ledApplySettings();       // re-read brightness/enabled from settings
void ledSet(LedPattern p);
void ledSetWidget(const LedSpec* spec);   // what NORMAL shows; nullptr = off
const LedSpec* ledWidgetSpec();           // current widget spec, or nullptr
LedPattern ledPattern();
void ledLoop();
