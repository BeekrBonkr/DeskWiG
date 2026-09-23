#pragma once

#include "../app/Widget.h"
#include <time.h>

class ClockWidget : public Widget {
public:
  const char* name() const override { return "Clock"; }

  void begin() override;
  void update(uint32_t now) override;
  void render(lgfx::LGFX_Sprite& ui) override;

private:
  time_t lastSync = 0;
  tm timeinfo{};
  void syncTime();
};
