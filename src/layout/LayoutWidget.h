#pragma once

#include <ArduinoJson.h>
#include "../app/Widget.h"

// A widget described by a JSON document instead of C++:
//
// {
//   "name": "Big Clock",
//   "elements": [
//     {"type":"text","x":85,"y":110,"size":4,"align":"center","color":"text","text":"{time}"},
//     {"type":"line","x":6,"y":296,"x2":164,"y2":296,"color":"dim"},
//     {"type":"rect","x":0,"y":0,"w":170,"h":28,"color":"dim","fill":false},
//     {"type":"bar","x":6,"y":230,"w":158,"h":8,"color":"ok","value":"{wifi.pct}"}
//   ]
// }
//
// Colours are role names (bg, text, dim, ok, warn, bad, accent), "#rrggbb",
// or a template that resolves to a role name, e.g. "{ping.0.color}".
// Text and value fields are templates; see LayoutData.h for the keys.

enum class ElType : uint8_t { TEXT, LINE, RECT, BAR };
enum class ElAlign : uint8_t { LEFT, CENTER, RIGHT };

struct LayoutElement {
  ElType type;
  ElAlign align;
  uint8_t size;
  bool fill;
  int16_t x, y;
  int16_t x2, y2;     // line end point
  int16_t w, h;       // rect / bar size
  char color[24];
  char text[64];      // text template, or bar value template
};

class LayoutWidget : public Widget {
public:
  static constexpr uint8_t MAX_ELEMENTS = 32;
  static constexpr uint8_t ID_LEN = 24;
  static constexpr uint8_t NAME_LEN = 32;

  // Parses a layout document. On failure the widget is left empty and
  // err holds a short reason.
  bool load(JsonVariantConst doc, char* err, size_t errLen);
  void clear();

  bool isLoaded() const { return _loaded; }
  bool usesPing() const { return _usesPing; }
  const char* id() const { return _id; }
  void setId(const char* id);

  void begin() override {}
  void update(uint32_t now) override;
  void render(lgfx::LGFX_Sprite& ui) override;
  const char* name() const override { return _name; }

private:
  uint16_t resolveColor(const char* spec, uint16_t fallback);

  char _id[ID_LEN + 1] = "";
  char _name[NAME_LEN + 1] = "";
  bool _loaded = false;
  bool _usesPing = false;
  uint8_t _count = 0;
  LayoutElement _el[MAX_ELEMENTS];
};
