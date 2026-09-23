#pragma once
#include <LovyanGFX.hpp>

class Widget {
public:
  virtual ~Widget() = default;
  virtual void begin() = 0;
  virtual void update(uint32_t now) = 0;
  virtual void render(lgfx::LGFX_Sprite& ui) = 0;
  virtual const char* name() const = 0;
};
