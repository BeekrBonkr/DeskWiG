#pragma once

#include "Widget.h"

class ScreenManager {
public:
  void add(Widget* w);
  void begin();
  void update(uint32_t now);
  void render(lgfx::LGFX_Sprite& ui);

  void setActive(uint8_t idx);
  uint8_t getActive() const;
  uint8_t getCount() const;
  const char* getName(uint8_t idx) const;

private:
  Widget* widgets[8];
  uint8_t count = 0;
  uint8_t active = 0;
};
