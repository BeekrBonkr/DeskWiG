#pragma once
#include <ArduinoJson.h>
#include "ScreenManager.h"

// The native widgets compiled into the firmware (Ping, Clock). Each can be
// removed from the widget list like a layout; the choice is saved in
// config.json so it survives a reboot, and can be undone from /widgets.

// Registers every built-in that is not hidden. Call before layoutsBegin().
void builtinsBegin(ScreenManager& sm);

// True if key names a built-in widget (hidden or not).
bool builtinExists(const char* key);
bool builtinHidden(const char* key);

// Remove from / return to the screen manager and save. Return false and
// fill err if key is not a built-in or is already in that state.
bool builtinHide(const char* key, char* err, size_t errLen);
bool builtinRestore(const char* key, char* err, size_t errLen);

// Names of the hidden built-ins.
void builtinsHiddenToJson(JsonArray arr);
