#include "ScreenManager.h"

void ScreenManager::add(Widget* w) {
  if (count < 8) {
    widgets[count++] = w;
  }
}

void ScreenManager::begin() {
  for (uint8_t i = 0; i < count; i++) {
    widgets[i]->begin();
  }
}

void ScreenManager::update(uint32_t now) {
  widgets[active]->update(now);
}

void ScreenManager::render(lgfx::LGFX_Sprite& ui) {
  widgets[active]->render(ui);
}

void ScreenManager::setActive(uint8_t idx) {
  if (idx < count) active = idx;
}

uint8_t ScreenManager::getActive() const {
  return active;
}

uint8_t ScreenManager::getCount() const {
  return count;
}

const char* ScreenManager::getName(uint8_t idx) const {
  if (idx >= count) return "";
  return widgets[idx]->name();
}
