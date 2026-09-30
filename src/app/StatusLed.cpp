#include "StatusLed.h"

#include <Adafruit_NeoPixel.h>
#include "Board.h"
#include "../web/Settings.h"

static Adafruit_NeoPixel led(1, Board::STATUS_LED, NEO_GRB + NEO_KHZ800);
static LedPattern pattern = LedPattern::OFF;
static bool enabled = true;
static LedSpec widgetSpec;
static bool haveWidgetSpec = false;

const char* ledModeName(LedMode m) {
  switch (m) {
    case LedMode::SOLID:   return "solid";
    case LedMode::BREATHE: return "breathe";
    case LedMode::BLINK:   return "blink";
    case LedMode::PULSE:   return "pulse";
    case LedMode::RAINBOW: return "rainbow";
    default:               return "off";
  }
}

bool ledModeFromName(const char* s, LedMode& out) {
  if (!s) return false;
  if (!strcmp(s, "off"))     { out = LedMode::OFF;     return true; }
  if (!strcmp(s, "solid"))   { out = LedMode::SOLID;   return true; }
  if (!strcmp(s, "breathe")) { out = LedMode::BREATHE; return true; }
  if (!strcmp(s, "blink"))   { out = LedMode::BLINK;   return true; }
  if (!strcmp(s, "pulse"))   { out = LedMode::PULSE;   return true; }
  if (!strcmp(s, "rainbow")) { out = LedMode::RAINBOW; return true; }
  return false;
}

void ledSetWidget(const LedSpec* spec) {
  haveWidgetSpec = spec != nullptr;
  if (spec) widgetSpec = *spec;
}

const LedSpec* ledWidgetSpec() { return haveWidgetSpec ? &widgetSpec : nullptr; }
LedPattern ledPattern() { return pattern; }

static void hsvToRgb(uint16_t h, uint8_t& r, uint8_t& g, uint8_t& b) {
  // h 0-359, full saturation and value
  uint8_t region = h / 60;
  uint8_t rem = (h % 60) * 255 / 60;
  uint8_t q = 255 - rem, t = rem;
  switch (region) {
    case 0:  r = 255; g = t;   b = 0;   break;
    case 1:  r = q;   g = 255; b = 0;   break;
    case 2:  r = 0;   g = 255; b = t;   break;
    case 3:  r = 0;   g = q;   b = 255; break;
    case 4:  r = t;   g = 0;   b = 255; break;
    default: r = 255; g = 0;   b = q;   break;
  }
}

// Color for the widget-controlled state at this instant.
static uint32_t widgetColor(uint32_t now) {
  if (!haveWidgetSpec || widgetSpec.mode == LedMode::OFF) return 0;
  const LedSpec& s = widgetSpec;
  uint16_t period = s.speedMs < 100 ? 100 : s.speedMs;
  uint32_t phase = now % period;
  uint8_t r = s.r, g = s.g, b = s.b;
  uint16_t level = 255;   // 0-255 modulation
  switch (s.mode) {
    case LedMode::SOLID: break;
    case LedMode::BLINK: level = phase < period / 2 ? 255 : 0; break;
    case LedMode::PULSE: level = phase < period / 8 ? 255 : 0; break;
    case LedMode::BREATHE: {
      // triangle wave eased into a smooth rise and fall, never fully off
      uint32_t half = period / 2;
      uint32_t t = phase < half ? phase : period - phase;
      uint32_t lin = t * 255 / (half ? half : 1);
      level = 20 + (lin * lin / 255) * 235 / 255;
      break;
    }
    case LedMode::RAINBOW: hsvToRgb((uint16_t)(phase * 360UL / period), r, g, b); break;
    default: break;
  }
  if (s.brightness >= 0) {
    uint16_t bl = s.brightness > 100 ? 100 : s.brightness;
    level = level * bl / 100;
  }
  return led.Color(r * level / 255, g * level / 255, b * level / 255);
}

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
  static uint32_t lastColor = 0xFFFFFFFF;
  static uint32_t lastPush = 0;
  uint32_t now = millis();
  if (now - lastPush < 20) return;   // 50 Hz is plenty for animations
  lastPush = now;

  uint32_t color;
  if (!enabled) {
    color = 0;
  } else if (pattern == LedPattern::NORMAL) {
    // Widget colors are already scaled by the spec's own brightness;
    // the device brightness setting applies on top via setBrightness().
    color = widgetColor(now);
  } else {
    PatternSpec s = specFor(pattern);
    bool on = true;
    if (s.periodMs) on = (now % s.periodMs) < (s.periodMs / 2);
    color = on ? led.Color(s.r, s.g, s.b) : 0;
  }

  // Only push to the strip when something changed; show() is slow.
  if (color == lastColor) return;
  lastColor = color;
  led.setPixelColor(0, color);
  led.show();
}
