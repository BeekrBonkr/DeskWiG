#include "ScreenManager.h"

void ScreenManager::add(Widget* w) {
  if (!w || count >= MAX_WIDGETS) return;
  widgets[count++] = w;
}

bool ScreenManager::remove(Widget* w) {
  int idx = indexOf(w);
  if (idx < 0) return false;

  for (uint8_t i = idx; i + 1 < count; i++) widgets[i] = widgets[i + 1];
  count--;

  if (active > idx) active--;
  if (active >= count) active = count ? count - 1 : 0;
  if (overrideWidget == w) overrideWidget = nullptr;
  return true;
}

void ScreenManager::begin() {
  for (uint8_t i = 0; i < count; i++) {
    widgets[i]->begin();
  }
}

Widget* ScreenManager::current() const {
  if (overrideWidget) return overrideWidget;
  if (count == 0) return nullptr;
  return widgets[active];
}

void ScreenManager::update(uint32_t now) {
  Widget* w = current();
  if (w) w->update(now);
}

void ScreenManager::render(lgfx::LGFX_Sprite& ui) {
  Widget* w = current();
  if (w) w->render(ui);
  else ui.fillScreen(TFT_BLACK);
}

void ScreenManager::applyOrder(const char* const* keys, uint8_t n) {
  Widget* wasActive = count ? widgets[active] : nullptr;
  Widget* sorted[MAX_WIDGETS];
  bool placed[MAX_WIDGETS] = {};
  uint8_t out = 0;

  for (uint8_t k = 0; k < n; k++) {
    for (uint8_t i = 0; i < count; i++) {
      if (!placed[i] && !strcmp(widgets[i]->key(), keys[k])) {
        sorted[out++] = widgets[i];
        placed[i] = true;
        break;
      }
    }
  }
  for (uint8_t i = 0; i < count; i++) {
    if (!placed[i]) sorted[out++] = widgets[i];
  }
  for (uint8_t i = 0; i < count; i++) widgets[i] = sorted[i];

  int idx = indexOf(wasActive);
  active = idx >= 0 ? idx : 0;
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

Widget* ScreenManager::get(uint8_t idx) const {
  return idx < count ? widgets[idx] : nullptr;
}

int ScreenManager::indexOf(const Widget* w) const {
  for (uint8_t i = 0; i < count; i++) {
    if (widgets[i] == w) return i;
  }
  return -1;
}

void ScreenManager::setOverride(Widget* w) {
  overrideWidget = w;
}

void ScreenManager::clearOverride() {
  overrideWidget = nullptr;
}

Widget* ScreenManager::getOverride() const {
  return overrideWidget;
}
