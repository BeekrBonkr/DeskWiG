#pragma once

#include "../app/Widget.h"
#include <time.h>

// Time itself comes from TimeService (NTP + timezone); this only draws it.
class ClockWidget : public Widget {
public:
  const char* name() const override { return "Clock"; }

  void begin() override {}
  void update(uint32_t now) override;
  void render(lgfx::LGFX_Sprite& ui) override;

private:
  tm timeinfo{};
};
