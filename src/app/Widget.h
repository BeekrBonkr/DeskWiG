#pragma once
#include <LovyanGFX.hpp>
#include "StatusLed.h"

class Widget {
public:
  virtual ~Widget() = default;
  virtual void begin() = 0;
  virtual void update(uint32_t now) = 0;
  virtual void render(lgfx::LGFX_Sprite& ui) = 0;
  virtual const char* name() const = 0;

  // What the status LED should show while this widget is on screen.
  // Return false to leave it off.
  virtual bool ledSpec(LedSpec& out) { (void)out; return false; }
};
