#include "Builtins.h"

#include "../widgets/PingWidget.h"
#include "../widgets/ClockWidget.h"
#include "../web/Settings.h"
#include "Log.h"

static PingWidget  pingWidget;
static ClockWidget clockWidget;
static Widget* const BUILTINS[] = { &pingWidget, &clockWidget };
static constexpr uint8_t BUILTIN_COUNT = sizeof(BUILTINS) / sizeof(BUILTINS[0]);
static_assert(BUILTIN_COUNT <= MAX_HIDDEN_WIDGETS, "raise MAX_HIDDEN_WIDGETS");

static ScreenManager* screens = nullptr;

static Widget* find(const char* key) {
  if (!key) return nullptr;
  for (Widget* w : BUILTINS) if (!strcmp(w->key(), key)) return w;
  return nullptr;
}

static int hiddenIndex(const char* key) {
  for (uint8_t i = 0; i < settings.hiddenCount; i++) {
    if (!strcmp(settings.hiddenWidgets[i], key)) return i;
  }
  return -1;
}

// Keep the saved active-widget choice pointing at what is on screen
// after the list changed underneath it.
static void persistActive(uint8_t before) {
  if (!screens || screens->getActive() == before) return;
  settings.activeWidget = screens->getActive();
  strlcpy(settings.activeWidgetName, screens->getName(settings.activeWidget), sizeof(settings.activeWidgetName));
}

void builtinsBegin(ScreenManager& sm) {
  screens = &sm;
  for (Widget* w : BUILTINS) {
    if (hiddenIndex(w->key()) < 0) sm.add(w);
    else Log.printf("[SYS] Built-in widget %s is hidden\n", w->key());
  }
}

bool builtinExists(const char* key) { return find(key) != nullptr; }
bool builtinHidden(const char* key) { return find(key) && hiddenIndex(key) >= 0; }

bool builtinHide(const char* key, char* err, size_t errLen) {
  Widget* w = find(key);
  if (!w) { strlcpy(err, "no such built-in widget", errLen); return false; }
  if (hiddenIndex(key) >= 0) { strlcpy(err, "already deleted", errLen); return false; }
  if (settings.hiddenCount >= MAX_HIDDEN_WIDGETS) { strlcpy(err, "hidden list full", errLen); return false; }

  strlcpy(settings.hiddenWidgets[settings.hiddenCount++], key, sizeof(settings.hiddenWidgets[0]));
  if (screens) {
    uint8_t before = screens->getActive();
    screens->remove(w);
    persistActive(before);
  }
  if (!saveSettings()) { strlcpy(err, "failed to save settings", errLen); return false; }
  return true;
}

bool builtinRestore(const char* key, char* err, size_t errLen) {
  Widget* w = find(key);
  if (!w) { strlcpy(err, "no such built-in widget", errLen); return false; }
  int idx = hiddenIndex(key);
  if (idx < 0) { strlcpy(err, "not deleted", errLen); return false; }
  if (screens && screens->getCount() >= ScreenManager::MAX_WIDGETS) {
    strlcpy(err, "widget list is full", errLen);
    return false;
  }

  for (uint8_t i = idx; i + 1 < settings.hiddenCount; i++) {
    strlcpy(settings.hiddenWidgets[i], settings.hiddenWidgets[i + 1], sizeof(settings.hiddenWidgets[0]));
  }
  settings.hiddenCount--;
  if (screens) screens->add(w);
  if (!saveSettings()) { strlcpy(err, "failed to save settings", errLen); return false; }
  return true;
}

void builtinsHiddenToJson(JsonArray arr) {
  for (uint8_t i = 0; i < settings.hiddenCount; i++) arr.add(settings.hiddenWidgets[i]);
}
