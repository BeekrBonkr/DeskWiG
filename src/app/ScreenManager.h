#pragma once

#include "Widget.h"

class ScreenManager {
public:
  static constexpr uint8_t MAX_WIDGETS = 16;

  void add(Widget* w);
  // Removes a widget. The active index is clamped so something is always showing.
  bool remove(Widget* w);

  void begin();
  void update(uint32_t now);
  void render(lgfx::LGFX_Sprite& ui);

  void setActive(uint8_t idx);
  uint8_t getActive() const;
  uint8_t getCount() const;
  const char* getName(uint8_t idx) const;
  Widget* get(uint8_t idx) const;
  int indexOf(const Widget* w) const;

  // Temporary screen shown instead of the active widget (editor preview).
  // The override is not part of the widget list and is never persisted.
  void setOverride(Widget* w);
  void clearOverride();
  Widget* getOverride() const;

  // The widget on screen: the override if set, else the active one.
  Widget* current() const;

private:

  Widget* widgets[MAX_WIDGETS];
  Widget* overrideWidget = nullptr;
  uint8_t count = 0;
  uint8_t active = 0;
};
