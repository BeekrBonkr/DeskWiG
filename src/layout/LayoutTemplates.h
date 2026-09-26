#pragma once
#include <Arduino.h>

// Ready-made layouts. Every template is offered in the editor's picker;
// the ones marked preload are written to /widgets on first boot so the
// device has something to show besides the two native widgets.

struct LayoutTemplate {
  const char* id;
  const char* name;
  bool preload;
  const char* json;   // the layout document, as stored on disk
};

extern const LayoutTemplate LAYOUT_TEMPLATES[];
extern const uint8_t LAYOUT_TEMPLATE_COUNT;
