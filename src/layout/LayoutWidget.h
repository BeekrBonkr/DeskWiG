#pragma once

#include <ArduinoJson.h>
#include "../app/Widget.h"
#include "ImageService.h"

// A widget described by a JSON document instead of C++.
//
// {
//   "name": "Weather",
//   "styles": { "label": { "color": "dim" } },
//   "style": { "direction": "column", "gap": 8, "padding": 6 },
//   "elements": [
//     {"type":"text","class":"label","text":"WEATHER"},
//     {"type":"text","text":"{api.weather.temp}","style":{"size":6,"align":"center"}},
//     {"type":"box","style":{"direction":"row","gap":6,"align":"center"},
//      "children":[
//        {"type":"circle","w":10,"color":"{ping.0.color}"},
//        {"type":"text","text":"{ping.0.name}"}
//      ]},
//     {"type":"arc","w":80,"value":"{wifi.pct}","style":{"thickness":8}},
//     {"type":"text","x":6,"y":304,"text":"absolute"}
//   ]
// }
//
// Elements are laid out like a column of HTML blocks: a box stacks its
// children along its direction with a gap, and the cross axis defaults to
// stretch. An element with both x and y is positioned absolutely inside
// its parent's content box instead, which is how pre-existing layouts
// (every element with x/y at the top level) keep rendering unchanged.
//
// Text uses the 6x8 bitmap font scaled by size, or a TrueType font when
// "font" names one ("sans", "bold", "emoji" or an uploaded font), in which
// case size is the line height in pixels. Emoji and other characters the
// bitmap font lacks are drawn from the emoji font.
//
// Style properties can be given as flat fields on the element (color,
// size, align, fill, font), in a "style" object, or in a named entry of the
// top-level "styles" map referenced with "class". Later ones win:
// class, then flat fields, then the style object.
//
// Types: text, line, rect, bar, box, circle, ellipse, arc, triangle, polygon,
// image, chart. An image element shows an uploaded image by name or an
// http(s) URL ("src"), scaled by style.fit; a box can have a background
// image (style.image) drawn to cover it. Animated GIFs play. A chart draws
// a sampled key ("series", see Series.h) over its last "window" seconds
// as a line, area, bars or dots (style.kind), scaled between style.min
// and style.max or to the data.
//
// Colors are role names (bg, text, dim, ok, warn, bad, accent), "#rrggbb",
// or a template that resolves to a role name or to a number 0xRRGGBB, e.g.
// "{ping.0.color}" or "{if(api.w.temp > 30, rgb(255,80,0), rgb(0,120,255))}".
// A box, rect or bar with style.gradient fades from its background (or
// color) to that second color, top to bottom or left to right.
//
// Any number (x, y, w, h, r, x2, y2, start, end, points, window, size,
// thickness, radius, borderWidth, padding, gap, min, max) can instead be
// a "{template}" that is evaluated every frame, so positions and shapes
// can follow live values: {"type":"circle","x":"{api.iss.lon + 90}","y":20,"r":4}.

enum class ElType : uint8_t { TEXT, LINE, RECT, BAR, BOX, CIRCLE, ELLIPSE, ARC, TRIANGLE, POLYGON, IMAGE, CHART };
enum class ChartKind : uint8_t { LINE, AREA, BARS, DOTS };
enum class ElAlign : uint8_t { START, CENTER, END, STRETCH };
enum class ElJustify : uint8_t { START, CENTER, END, BETWEEN };

constexpr int16_t EL_AUTO = INT16_MIN;

// Room for a color expression such as {mix(rgb(0,0,255), rgb(255,0,0), api.w.temp / 40)}.
constexpr uint8_t COLOR_LEN = 64;

struct LayoutStyle {
  char font[24];       // TrueType font name; "" = built-in 6x8 bitmap font
  char color[COLOR_LEN];   // text color, fill color (fill=true) or outline color
  char bg[COLOR_LEN];      // background (box, rect, shapes) or arc track; "" = none
  char border[COLOR_LEN];  // border color; "" = none
  uint8_t size;        // bitmap text scale 1-40, or pixel line height 6-160 with a font
  uint8_t borderW;     // border width in px
  uint8_t radius;      // corner radius (box, rect)
  uint8_t pad;         // box padding
  uint8_t gap;         // box gap between children
  uint8_t thick;       // arc ring thickness, line width
  ElAlign align;       // text alignment, or cross-axis alignment of a box's children
  ElJustify justify;   // main-axis distribution of a box's children
  bool row;            // box direction: row instead of column
  bool fill;           // shapes: fill with color instead of outline
  bool absolute;       // forced absolute positioning
  ImgFit fit;          // image scaling: contain (default), cover, stretch
  char image[IMAGE_SRC_LEN + 1];   // background image for a box (name or URL)
  char gradient[COLOR_LEN];   // second color of a gradient fill; "" = flat
  bool gradientRight;  // gradient runs left to right instead of top to bottom
  ChartKind kind;      // chart: line (default), area, bars, dots
  float vmin, vmax;    // chart value range; used when hasMin/hasMax
  bool hasMin, hasMax;
};

struct LayoutNode {
  ElType type;
  int16_t x, y, w, h;          // EL_AUTO when not given
  int16_t x2, y2;              // line end (absolute lines)
  int16_t a0, a1;              // arc start/end angles, 0 = top, clockwise
  uint8_t npts;                // triangle / polygon
  int16_t pts[16];
  LayoutStyle st;
  char text[64];               // text template, or bar/arc value template
  char src[IMAGE_SRC_LEN + 1]; // image source (name or URL)
  uint16_t refreshS;           // URL image refresh interval
  uint32_t window;             // chart: seconds of history shown, 0 = all
  uint8_t firstChild;          // 0xFF = none
  uint8_t nextSibling;         // 0xFF = none
  bool hasXY;

  // computed each render
  int16_t lx, ly, lw, lh;
};

// Status LED control, from the layout's top-level "led" object:
//
//   "led": {
//     "color": "{ping.0.color}", "mode": "breathe", "speed": 3000, "brightness": 50,
//     "rules": [
//       {"when": "ping.0.ms > 100", "color": "warn", "mode": "blink", "speed": 500},
//       {"key": "ping.0.status", "is": "down", "color": "bad", "mode": "blink", "speed": 200}
//     ]
//   }
//
// The first matching rule wins ("when" is an expression that is true when
// non-zero, "key"/"is" compares a key's text); otherwise the top-level
// color and mode apply. A rule only overrides the fields it sets. Color
// is a role name, #rrggbb or a template. No "led" means the LED stays off.
struct LedRule {
  char when[64];
  char key[40];
  char is[24];
  char color[COLOR_LEN];
  LedMode mode;
  uint16_t speed;
  int16_t brightness;
  bool hasColor, hasMode, hasSpeed, hasBrightness;
};

struct LedConfig {
  LedRule base;
  LedRule rules[6];
  uint8_t ruleCount;
};

// A number given as a template: evaluated each frame and written into
// the node's field before layout. Points use BF_PT0 + index.
enum BindField : uint8_t {
  BF_X, BF_Y, BF_W, BF_H, BF_R, BF_X2, BF_Y2, BF_A0, BF_A1, BF_WINDOW,
  BF_SIZE, BF_THICK, BF_RADIUS, BF_BORDERW, BF_PAD, BF_GAP, BF_MIN, BF_MAX,
  BF_PT0
};

struct LayoutBinding {
  uint8_t node;
  uint8_t field;
  char tpl[56];
};

class LayoutWidget : public Widget {
public:
  static constexpr uint8_t MAX_NODES = 64;     // including the implicit root
  static constexpr uint8_t MAX_DEPTH = 6;
  static constexpr uint8_t MAX_STYLES = 8;
  static constexpr uint8_t MAX_BINDINGS = 40;
  static constexpr uint8_t ID_LEN = 24;
  static constexpr uint8_t NAME_LEN = 32;
  static constexpr uint8_t NONE = 0xFF;
  static constexpr size_t TEXT_BUF = 96;

  LayoutWidget();
  ~LayoutWidget();

  // Parses a layout document. On failure the widget is left empty and
  // err holds a short reason.
  bool load(JsonVariantConst doc, char* err, size_t errLen);
  void clear();

  bool isLoaded() const { return _loaded; }
  bool usesPing() const { return _usesPing; }
  bool usesApi() const { return _usesApi; }
  const char* id() const { return _id; }
  void setId(const char* id);

  void begin() override {}
  void update(uint32_t now) override;
  void render(lgfx::LGFX_Sprite& ui) override;
  const char* name() const override { return _name; }
  const char* key() const override { return _id; }
  bool ledSpec(LedSpec& out) override;

private:
  struct NamedStyle;
  bool parseLed(JsonVariantConst v, char* err, size_t errLen);
  bool parseLedRule(LedRule& r, JsonObjectConst obj, const char* path, char* err, size_t errLen);
  void updateLed();
  bool alloc();
  bool parseNode(JsonVariantConst v, uint8_t parent, uint8_t depth, const char* path,
                 const NamedStyle* styles, uint8_t styleCount, char* err, size_t errLen);
  bool parseStyle(LayoutStyle& st, JsonObjectConst obj, uint8_t node, const char* path, char* err, size_t errLen);
  bool bindNumber(JsonVariantConst v, uint8_t node, uint8_t field, double& out, const char* path, const char* what, char* err, size_t errLen);
  void noteKeys(const char* s);
  void touchSources();

  void applyBindings();
  void expandAll();
  void measure(uint8_t i);
  void place(uint8_t i, int16_t x, int16_t y, int16_t w, int16_t h);
  void draw(lgfx::LGFX_Sprite& ui, uint8_t i, int16_t cx, int16_t cy, int16_t cw, int16_t ch);
  uint16_t resolveColor(const char* spec, uint16_t fallback, bool* present = nullptr);

  char _id[ID_LEN + 1] = "";
  char _name[NAME_LEN + 1] = "";
  bool _loaded = false;
  bool _usesPing = false;
  bool _usesApi = false;
  bool _usesImages = false;
  uint32_t _lastTouch = 0;
  uint32_t _lastImageTouch = 0;
  uint8_t _count = 0;
  LayoutNode* _n = nullptr;            // MAX_NODES, in PSRAM when available
  char (*_txt)[TEXT_BUF] = nullptr;    // expanded text per node
  LedConfig* _led = nullptr;           // in PSRAM; nullptr = LED off
  LayoutBinding* _bind = nullptr;      // MAX_BINDINGS, in PSRAM
  uint8_t _bindCount = 0;
  bool _hasLed = false;
  LedSpec _ledSpec;
  uint32_t _lastLedEval = 0;
};
