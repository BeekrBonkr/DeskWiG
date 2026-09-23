#include "StatusLed.h"

#include <Adafruit_NeoPixel.h>
#include "Board.h"
#include "../web/Settings.h"

static Adafruit_NeoPixel led(1, Board::STATUS_LED, NEO_GRB + NEO_KHZ800);
static LedPattern pattern = LedPattern::OFF;
static bool enabled = true;

struct PatternSpec {
  uint8_t r, g, b;
  uint16_t periodMs;   // 0 = solid
};

static PatternSpec specFor(LedPattern p) {
  switch (p) {
    case LedPattern::AP_MODE:    return { 0,   0,   255, 1000 };
    case LedPattern::CONNECTING: return { 255, 120, 0,   0 };
    case LedPattern::NORMAL:     return { 0,   255, 0,   0 };
    case LedPattern::ALERT:      return { 255, 0,   0,   1000 };
    case LedPattern::RESETTING:  return { 255, 255, 255, 100 };
    default:                     return { 0,   0,   0,   0 };
  }
}

void ledBegin() {
  led.begin();
  led.setBrightness(5);
  led.clear();
  led.show();
}

void ledApplySettings() {
  enabled = settings.ledEnabled;
  led.setBrightness(settings.ledBrightness);
}

void ledSet(LedPattern p) {
  pattern = p;
}

void ledLoop() {
  static LedPattern lastPattern = LedPattern::OFF;
  static bool lastOn = false;
  static bool lastEnabled = true;

  PatternSpec s = specFor(pattern);
  bool on = true;
  if (s.periodMs) {
    on = (millis() % s.periodMs) < (s.periodMs / 2);
  }
  if (!enabled) on = false;

  // Only push to the strip when something changed; show() is slow.
  if (pattern == lastPattern && on == lastOn && enabled == lastEnabled) return;
  lastPattern = pattern;
  lastOn = on;
  lastEnabled = enabled;

  led.setPixelColor(0, on ? led.Color(s.r, s.g, s.b) : 0);
  led.show();
}
