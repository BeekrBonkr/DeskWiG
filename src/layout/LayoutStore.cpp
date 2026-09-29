#include "LayoutStore.h"

#include <LittleFS.h>
#include "LayoutTemplates.h"
#include "../web/Settings.h"
#include "../app/Log.h"

static const char* LAYOUT_DIR = "/widgets";
static const char* SEED_MARKER = "/widgets/.seeded";

static ScreenManager* screens = nullptr;
static LayoutWidget slots[MAX_LAYOUTS];
static LayoutWidget previewWidget;
static uint32_t previewUntil = 0;
static bool previewOn = false;

// =====================
// HELPERS
// =====================
bool layoutValidId(const char* id) {
  if (!id) return false;
  size_t len = strlen(id);
  if (len == 0 || len > LayoutWidget::ID_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = id[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    if (!ok) return false;
  }
  return id[0] != '-' && id[len - 1] != '-';
}

String layoutPath(const char* id) {
  return String(LAYOUT_DIR) + "/" + id + ".json";
}

uint8_t layoutCount() {
  uint8_t n = 0;
  for (auto& s : slots) if (s.isLoaded()) n++;
  return n;
}

uint8_t layoutFreeSlots() {
  return MAX_LAYOUTS - layoutCount();
}

LayoutWidget* layoutAt(uint8_t i) {
  uint8_t n = 0;
  for (auto& s : slots) {
    if (!s.isLoaded()) continue;
    if (n == i) return &s;
    n++;
  }
  return nullptr;
}

LayoutWidget* layoutFind(const char* id) {
  if (!id) return nullptr;
  for (auto& s : slots) {
    if (s.isLoaded() && strcmp(s.id(), id) == 0) return &s;
  }
  return nullptr;
}

static LayoutWidget* freeSlot() {
  for (auto& s : slots) if (!s.isLoaded()) return &s;
  return nullptr;
}

static bool parseFile(const String& path, JsonDocument& doc, char* err, size_t errLen) {
  File f = LittleFS.open(path, "r");
  if (!f) { strlcpy(err, "cannot open file", errLen); return false; }
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  if (e) { snprintf(err, errLen, "invalid JSON: %s", e.c_str()); return false; }
  return true;
}

// =====================
// BOOT
// =====================
// Copies the preload templates into /widgets once. Existing files are
// never overwritten, so a user's edits to a preloaded widget survive
// upgrades; deleting one is also permanent because of the marker file.
static void seedTemplates() {
  if (LittleFS.exists(SEED_MARKER)) return;

  for (uint8_t i = 0; i < LAYOUT_TEMPLATE_COUNT; i++) {
    const LayoutTemplate& t = LAYOUT_TEMPLATES[i];
    if (!t.preload) continue;
    String path = layoutPath(t.id);
    if (LittleFS.exists(path)) continue;
    File f = LittleFS.open(path, "w");
    if (!f) continue;
    f.print(t.json);
    f.close();
    Log.printf("[LAYOUT] Preloaded %s\n", t.id);
  }

  File m = LittleFS.open(SEED_MARKER, "w");
  if (m) { m.print("1"); m.close(); }
}

void layoutsBegin(ScreenManager& sm) {
  screens = &sm;

  if (!LittleFS.exists(LAYOUT_DIR)) LittleFS.mkdir(LAYOUT_DIR);
  seedTemplates();

  File dir = LittleFS.open(LAYOUT_DIR);
  if (!dir || !dir.isDirectory()) {
    Log.println("[LAYOUT] No widget directory");
    return;
  }

  char err[80];
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) continue;

    // "/widgets/<id>.json" -> "<id>"
    String path = f.path();
    f.close();
    int slash = path.lastIndexOf('/');
    String base = path.substring(slash + 1);
    if (base.startsWith(".") || !base.endsWith(".json")) continue;
    String id = base.substring(0, base.length() - 5);
    if (!layoutValidId(id.c_str())) {
      Log.printf("[LAYOUT] Skipping %s: bad id\n", path.c_str());
      continue;
    }

    LayoutWidget* slot = freeSlot();
    if (!slot) {
      Log.printf("[LAYOUT] Skipping %s: no free slot\n", path.c_str());
      continue;
    }

    JsonDocument doc;
    if (!parseFile(path, doc, err, sizeof(err))) {
      Log.printf("[LAYOUT] Skipping %s: %s\n", path.c_str(), err);
      continue;
    }
    slot->setId(id.c_str());
    if (!slot->load(doc.as<JsonVariantConst>(), err, sizeof(err))) {
      Log.printf("[LAYOUT] Skipping %s: %s\n", path.c_str(), err);
      continue;
    }
    screens->add(slot);
    Log.printf("[LAYOUT] Loaded %s (%s)\n", id.c_str(), slot->name());
  }
  dir.close();
}

// =====================
// SAVE / DELETE
// =====================
bool layoutSave(const char* id, JsonVariantConst doc, char* err, size_t errLen) {
  if (!layoutValidId(id)) {
    strlcpy(err, "invalid id: use lowercase letters, digits and dashes", errLen);
    return false;
  }

  LayoutWidget* existing = layoutFind(id);
  LayoutWidget* target = existing ? existing : freeSlot();
  if (!target) {
    snprintf(err, errLen, "no free widget slots (max %u)", MAX_LAYOUTS);
    return false;
  }
  if (!existing && screens && screens->getCount() >= ScreenManager::MAX_WIDGETS) {
    strlcpy(err, "screen manager is full", errLen);
    return false;
  }

  // Validate into a scratch widget first so a bad save never blanks a
  // widget that is currently on screen.
  LayoutWidget scratch;
  scratch.setId(id);
  if (!scratch.load(doc, err, errLen)) return false;

  String path = layoutPath(id);
  String tmp = path + ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) { strlcpy(err, "cannot write file", errLen); return false; }
  size_t written = serializeJsonPretty(doc, f);
  f.close();
  if (written == 0) {
    LittleFS.remove(tmp);
    strlcpy(err, "failed to write file", errLen);
    return false;
  }
  if (!LittleFS.rename(tmp, path)) {
    LittleFS.remove(path);
    if (!LittleFS.rename(tmp, path)) {
      strlcpy(err, "failed to move file into place", errLen);
      return false;
    }
  }

  target->setId(id);
  target->load(doc, err, errLen);
  if (!existing && screens) screens->add(target);
  return true;
}

bool layoutDelete(const char* id, char* err, size_t errLen) {
  LayoutWidget* w = layoutFind(id);
  if (!w) { strlcpy(err, "no such widget", errLen); return false; }

  if (screens) {
    uint8_t before = screens->getActive();
    screens->remove(w);
    if (screens->getActive() != before) {
      settings.activeWidget = screens->getActive();
      strlcpy(settings.activeWidgetName, screens->getName(settings.activeWidget), sizeof(settings.activeWidgetName));
      saveSettings();
    }
  }
  w->clear();
  LittleFS.remove(layoutPath(id));
  return true;
}

// =====================
// PREVIEW
// =====================
bool layoutPreview(JsonVariantConst doc, char* err, size_t errLen) {
  previewWidget.setId("preview");
  if (!previewWidget.load(doc, err, errLen)) {
    layoutPreviewStop();
    return false;
  }
  previewOn = true;
  previewUntil = millis() + LAYOUT_PREVIEW_MS;
  if (screens) screens->setOverride(&previewWidget);
  return true;
}

void layoutPreviewStop() {
  previewOn = false;
  if (screens && screens->getOverride() == &previewWidget) screens->clearOverride();
}

bool layoutPreviewActive() {
  return previewOn;
}

void layoutsLoop(uint32_t now) {
  if (previewOn && (int32_t)(now - previewUntil) >= 0) layoutPreviewStop();
}
