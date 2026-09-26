#pragma once

#include <ArduinoJson.h>
#include "LayoutWidget.h"
#include "../app/ScreenManager.h"

// Owns the JSON layout widgets stored at /widgets/<id>.json on LittleFS,
// registers them with the ScreenManager, and drives the editor preview.

constexpr uint8_t MAX_LAYOUTS = 8;
constexpr uint32_t LAYOUT_PREVIEW_MS = 60000;

// Loads every /widgets/*.json and adds the valid ones to the screen manager.
void layoutsBegin(ScreenManager& sm);

// Expires the editor preview. Call from the main loop.
void layoutsLoop(uint32_t now);

uint8_t layoutCount();
LayoutWidget* layoutAt(uint8_t i);
LayoutWidget* layoutFind(const char* id);
uint8_t layoutFreeSlots();

// Lowercase letters, digits and dashes, 1-24 chars.
bool layoutValidId(const char* id);

// Path of the layout file for an id (no existence check).
String layoutPath(const char* id);

// Validates, writes the file and (re)loads the widget in place.
// New ids take a free slot and are appended to the screen manager.
bool layoutSave(const char* id, JsonVariantConst doc, char* err, size_t errLen);

// Removes the widget from the screen manager and deletes the file.
bool layoutDelete(const char* id, char* err, size_t errLen);

// Shows a layout on the device without saving it, for LAYOUT_PREVIEW_MS.
bool layoutPreview(JsonVariantConst doc, char* err, size_t errLen);
void layoutPreviewStop();
bool layoutPreviewActive();
