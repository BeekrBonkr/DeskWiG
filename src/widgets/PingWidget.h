#pragma once

#include "../app/Widget.h"
#include "../net/PingTarget.h"
#include <LovyanGFX.hpp>

class PingWidget : public Widget {
public:
  void begin() override;
  void update(uint32_t now) override;
  void render(lgfx::LGFX_Sprite& ui) override;
  const char* name() const override { return "Ping"; }

  // Solid LED in the color of the worst target: red if any is down,
  // amber if any is waiting or slow, green otherwise.
  bool ledSpec(LedSpec& out) override;

private:
  void drawRow(lgfx::LGFX_Sprite& ui, int y, PingTarget& t);
  void drawSparkline(lgfx::LGFX_Sprite& ui, int x, int y, const PingTarget& t);
  void drawWifiStrength(lgfx::LGFX_Sprite& ui, int y);
  void drawIPAddress(lgfx::LGFX_Sprite& ui);

  uint16_t latencyColor(int ms);
  char trendArrow(const PingTarget& t);
};
