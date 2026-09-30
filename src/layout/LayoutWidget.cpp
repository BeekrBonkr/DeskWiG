#include "LayoutWidget.h"

#include <esp_heap_caps.h>
#include "LayoutData.h"
#include "LayoutExpr.h"
#include "FontService.h"
#include "../net/PingService.h"
#include "../net/DataSource.h"
#include "../net/Series.h"
#include <math.h>

static const uint16_t COLOR_BG     = 0x0000;
static const uint16_t COLOR_TEXT   = 0xFFFF;
static const uint16_t COLOR_DIM    = 0x39E7;
static const uint16_t COLOR_OK     = 0x07E0;
static const uint16_t COLOR_WARN   = 0xFD20;
static const uint16_t COLOR_BAD    = 0xF800;
static const uint16_t COLOR_ACCENT = 0x04FF;

static const int16_t SCREEN_W = 170;
static const int16_t SCREEN_H = 320;
static const int16_t CHAR_W = 6;
static const int16_t CHAR_H = 8;
// Largest bitmap scale: 40 x 8 px fills the 320 px screen height.
static const uint8_t BITMAP_MAX_SCALE = 40;
static const size_t TEXT_RUN = 96;

struct LayoutWidget::NamedStyle {
  char name[17];
  LayoutStyle st;
};

static bool roleColor(const char* name, uint16_t& out) {
  if (!strcmp(name, "bg"))     { out = COLOR_BG;     return true; }
  if (!strcmp(name, "text"))   { out = COLOR_TEXT;   return true; }
  if (!strcmp(name, "dim"))    { out = COLOR_DIM;    return true; }
  if (!strcmp(name, "ok"))     { out = COLOR_OK;     return true; }
  if (!strcmp(name, "warn"))   { out = COLOR_WARN;   return true; }
  if (!strcmp(name, "bad"))    { out = COLOR_BAD;    return true; }
  if (!strcmp(name, "accent")) { out = COLOR_ACCENT; return true; }

  if (name[0] == '#' && strlen(name) == 7) {
    char* end;
    long v = strtol(name + 1, &end, 16);
    if (*end == '\0') {
      out = lgfx::color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
      return true;
    }
  }
  // A number 0xRRGGBB, as rgb(), hsv() and mix() in an expression produce.
  if (name[0] >= '0' && name[0] <= '9') {
    char* end;
    unsigned long v = strtoul(name, &end, 10);
    if (*end == '\0' && v <= 0xFFFFFF) {
      out = lgfx::color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
      return true;
    }
  }
  return false;
}

static uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {   // t: 0 = a, 255 = b
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r = ar + (br - ar) * t / 255, g = ag + (bg - ag) * t / 255, bl = ab + (bb - ab) * t / 255;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Fills a (rounded) rectangle with a two-colour gradient, line by line.
static void fillGradient(lgfx::LGFX_Sprite& ui, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t c0, uint16_t c1, bool right) {
  if (w <= 0 || h <= 0) return;
  int16_t r = radius;
  if (r * 2 > w) r = w / 2;
  if (r * 2 > h) r = h / 2;
  int16_t len = right ? w : h;
  for (int16_t i = 0; i < len; i++) {
    uint8_t t = len > 1 ? (uint8_t)((int32_t)i * 255 / (len - 1)) : 0;
    uint16_t c = blend565(c0, c1, t);
    int16_t inset = 0;
    if (r > 0) {
      // Distance from this line to the nearer edge, inside the corner radius.
      int16_t d = i < r ? r - i : (i >= len - r ? i - (len - r) + 1 : 0);
      if (d > 0) {
        float dd = d - 0.5f;
        float in = r - sqrtf((float)r * r - dd * dd);
        inset = (int16_t)(in + 0.5f);
      }
    }
    int16_t span = (right ? h : w) - 2 * inset;
    if (span <= 0) continue;
    if (right) ui.drawFastVLine(x + i, y + inset, span, c);
    else       ui.drawFastHLine(x + inset, y + i, span, c);
  }
}

static void defaultStyle(LayoutStyle& st) {
  memset(&st, 0, sizeof(st));
  strlcpy(st.color, "text", sizeof(st.color));
  st.size = 1;
  st.align = ElAlign::STRETCH;
  st.justify = ElJustify::START;
}

// =====================
// LIFECYCLE
// =====================
LayoutWidget::LayoutWidget() {}

LayoutWidget::~LayoutWidget() {
  free(_n);
  free(_txt);
  free(_led);
  free(_bind);
}

bool LayoutWidget::alloc() {
  if (!_led) _led = (LedConfig*)heap_caps_malloc(sizeof(LedConfig), MALLOC_CAP_SPIRAM);
  if (!_led) _led = (LedConfig*)malloc(sizeof(LedConfig));
  if (!_bind) _bind = (LayoutBinding*)heap_caps_malloc(sizeof(LayoutBinding) * MAX_BINDINGS, MALLOC_CAP_SPIRAM);
  if (!_bind) _bind = (LayoutBinding*)malloc(sizeof(LayoutBinding) * MAX_BINDINGS);
  if (_n && _txt && _led && _bind) return true;
  size_t nBytes = sizeof(LayoutNode) * MAX_NODES;
  size_t tBytes = TEXT_BUF * MAX_NODES;
  if (!_n)   _n   = (LayoutNode*)heap_caps_malloc(nBytes, MALLOC_CAP_SPIRAM);
  if (!_n)   _n   = (LayoutNode*)malloc(nBytes);
  if (!_txt) _txt = (char (*)[TEXT_BUF])heap_caps_malloc(tBytes, MALLOC_CAP_SPIRAM);
  if (!_txt) _txt = (char (*)[TEXT_BUF])malloc(tBytes);
  return _n && _txt && _led && _bind;
}

void LayoutWidget::clear() {
  _loaded = false;
  _usesPing = false;
  _usesApi = false;
  _usesImages = false;
  _hasLed = false;
  _ledSpec = LedSpec();
  _count = 0;
  _bindCount = 0;
  _name[0] = '\0';
}

void LayoutWidget::setId(const char* id) {
  strlcpy(_id, id ? id : "", sizeof(_id));
}

// =====================
// PARSING
// =====================
static bool fail(char* err, size_t n, const char* path, const char* msg) {
  if (!path || !*path) strlcpy(err, msg, n);
  else if (!strncmp(path, "style", 5) || !strncmp(path, "led", 3) || !strcmp(path, "root")) snprintf(err, n, "%s: %s", path, msg);
  else snprintf(err, n, "element %s: %s", path, msg);
  return false;
}

static bool copyColor(char* dst, size_t n, const char* spec, const char* path, const char* what, char* err, size_t errLen) {
  if (strlen(spec) >= n) {
    char m[48];
    snprintf(m, sizeof(m), "%s too long", what);
    return fail(err, errLen, path, m);
  }
  if (*spec && !strchr(spec, '{')) {
    uint16_t tmp;
    if (!roleColor(spec, tmp)) {
      char m[64];
      snprintf(m, sizeof(m), "unknown %s (use a role name or #rrggbb)", what);
      return fail(err, errLen, path, m);
    }
  }
  strlcpy(dst, spec, n);
  return true;
}

static bool parseAlign(const char* s, ElAlign& out) {
  if (!strcmp(s, "left") || !strcmp(s, "start") || !strcmp(s, "top"))    { out = ElAlign::START;   return true; }
  if (!strcmp(s, "center"))                                               { out = ElAlign::CENTER;  return true; }
  if (!strcmp(s, "right") || !strcmp(s, "end") || !strcmp(s, "bottom"))  { out = ElAlign::END;     return true; }
  if (!strcmp(s, "stretch"))                                              { out = ElAlign::STRETCH; return true; }
  return false;
}

static bool parseJustify(const char* s, ElJustify& out) {
  if (!strcmp(s, "start"))   { out = ElJustify::START;   return true; }
  if (!strcmp(s, "center"))  { out = ElJustify::CENTER;  return true; }
  if (!strcmp(s, "end"))     { out = ElJustify::END;     return true; }
  if (!strcmp(s, "between")) { out = ElJustify::BETWEEN; return true; }
  return false;
}

static uint8_t clampU8(int v, int lo, int hi) {
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return (uint8_t)v;
}

// Remembers which live data a template refers to, so update() keeps it fed.
void LayoutWidget::noteKeys(const char* s) {
  if (strstr(s, "ping.")) _usesPing = true;
  if (strstr(s, "api."))  _usesApi = true;
}

// A number that may also be a "{template}". Numbers land in out at once;
// a template is recorded as a binding and out is 0 until the first frame.
bool LayoutWidget::bindNumber(JsonVariantConst v, uint8_t node, uint8_t field, double& out, const char* path, const char* what, char* err, size_t errLen) {
  if (v.is<const char*>()) {
    const char* t = v.as<const char*>();
    char m[80];
    if (!strchr(t, '{')) {
      snprintf(m, sizeof(m), "%s must be a number or a {template}", what);
      return fail(err, errLen, path, m);
    }
    if (node == NONE) {
      snprintf(m, sizeof(m), "%s cannot be a template in a named style", what);
      return fail(err, errLen, path, m);
    }
    if (strlen(t) >= sizeof(_bind[0].tpl)) {
      snprintf(m, sizeof(m), "%s template longer than %u characters", what, (unsigned)(sizeof(_bind[0].tpl) - 1));
      return fail(err, errLen, path, m);
    }
    if (_bindCount >= MAX_BINDINGS) {
      snprintf(err, errLen, "too many templated numbers (max %u)", MAX_BINDINGS);
      return false;
    }
    LayoutBinding& b = _bind[_bindCount++];
    b.node = node;
    b.field = field;
    strlcpy(b.tpl, t, sizeof(b.tpl));
    noteKeys(t);
    out = 0;
    return true;
  }
  if (!v.is<float>() && !v.is<int>() && !v.is<double>() && !v.is<long>()) {
    char m[80];
    snprintf(m, sizeof(m), "%s must be a number or a {template}", what);
    return fail(err, errLen, path, m);
  }
  out = v.as<double>();
  return true;
}

bool LayoutWidget::parseStyle(LayoutStyle& st, JsonObjectConst obj, uint8_t node, const char* path, char* err, size_t errLen) {
  double d;
  for (JsonPairConst kv : obj) {
    const char* k = kv.key().c_str();
    JsonVariantConst v = kv.value();
    if (!strcmp(k, "color")) {
      if (!copyColor(st.color, sizeof(st.color), v | "text", path, "color", err, errLen)) return false;
    } else if (!strcmp(k, "background") || !strcmp(k, "bg")) {
      if (!copyColor(st.bg, sizeof(st.bg), v | "", path, "background", err, errLen)) return false;
    } else if (!strcmp(k, "border")) {
      if (!copyColor(st.border, sizeof(st.border), v | "", path, "border", err, errLen)) return false;
      if (st.border[0] && st.borderW == 0) st.borderW = 1;
    } else if (!strcmp(k, "borderWidth")) {
      if (!bindNumber(v, node, BF_BORDERW, d, path, "borderWidth", err, errLen)) return false;
      st.borderW = clampU8((int)d, 0, 20);
    } else if (!strcmp(k, "radius")) {
      if (!bindNumber(v, node, BF_RADIUS, d, path, "radius", err, errLen)) return false;
      st.radius = clampU8((int)d, 0, 80);
    } else if (!strcmp(k, "padding")) {
      if (!bindNumber(v, node, BF_PAD, d, path, "padding", err, errLen)) return false;
      st.pad = clampU8((int)d, 0, 80);
    } else if (!strcmp(k, "gap")) {
      if (!bindNumber(v, node, BF_GAP, d, path, "gap", err, errLen)) return false;
      st.gap = clampU8((int)d, 0, 200);
    } else if (!strcmp(k, "thickness") || !strcmp(k, "width")) {
      if (!bindNumber(v, node, BF_THICK, d, path, "thickness", err, errLen)) return false;
      st.thick = clampU8((int)d, 1, 80);
    } else if (!strcmp(k, "size")) {
      if (!bindNumber(v, node, BF_SIZE, d, path, "size", err, errLen)) return false;
      int size = v.is<const char*>() ? 1 : (int)d;
      if (size < 1 || size > FONT_MAX_PX) return fail(err, errLen, path, "size must be 1-40 (bitmap font) or 6-160 (TrueType font)");
      st.size = size;
    } else if (!strcmp(k, "gradient")) {
      if (!copyColor(st.gradient, sizeof(st.gradient), v | "", path, "gradient", err, errLen)) return false;
    } else if (!strcmp(k, "gradientDir")) {
      const char* g = v | "down";
      if (!strcmp(g, "right") || !strcmp(g, "horizontal")) st.gradientRight = true;
      else if (!strcmp(g, "down") || !strcmp(g, "vertical")) st.gradientRight = false;
      else return fail(err, errLen, path, "gradientDir must be down or right");
    } else if (!strcmp(k, "kind")) {
      const char* g = v | "line";
      if (!strcmp(g, "line")) st.kind = ChartKind::LINE;
      else if (!strcmp(g, "area")) st.kind = ChartKind::AREA;
      else if (!strcmp(g, "bars")) st.kind = ChartKind::BARS;
      else if (!strcmp(g, "dots")) st.kind = ChartKind::DOTS;
      else return fail(err, errLen, path, "kind must be line, area, bars or dots");
    } else if (!strcmp(k, "min")) {
      if (!bindNumber(v, node, BF_MIN, d, path, "min", err, errLen)) return false;
      st.vmin = (float)d;
      st.hasMin = true;
    } else if (!strcmp(k, "max")) {
      if (!bindNumber(v, node, BF_MAX, d, path, "max", err, errLen)) return false;
      st.vmax = (float)d;
      st.hasMax = true;
    } else if (!strcmp(k, "font")) {
      const char* f = v | "";
      if (strlen(f) >= sizeof(st.font)) return fail(err, errLen, path, "font name too long");
      if (*f && !fontExists(f)) return fail(err, errLen, path, "unknown font (see the setup page for the list)");
      strlcpy(st.font, f, sizeof(st.font));
    } else if (!strcmp(k, "align")) {
      if (!parseAlign(v | "", st.align)) return fail(err, errLen, path, "align must be left/start, center, right/end or stretch");
    } else if (!strcmp(k, "justify")) {
      if (!parseJustify(v | "", st.justify)) return fail(err, errLen, path, "justify must be start, center, end or between");
    } else if (!strcmp(k, "direction")) {
      const char* d = v | "column";
      if (!strcmp(d, "row")) st.row = true;
      else if (!strcmp(d, "column")) st.row = false;
      else return fail(err, errLen, path, "direction must be column or row");
    } else if (!strcmp(k, "fill")) {
      st.fill = v | false;
    } else if (!strcmp(k, "position")) {
      const char* p = v | "";
      st.absolute = !strcmp(p, "absolute");
    } else if (!strcmp(k, "fit")) {
      const char* f = v | "contain";
      if (!strcmp(f, "contain")) st.fit = ImgFit::CONTAIN;
      else if (!strcmp(f, "cover")) st.fit = ImgFit::COVER;
      else if (!strcmp(f, "stretch")) st.fit = ImgFit::STRETCH;
      else return fail(err, errLen, path, "fit must be contain, cover or stretch");
    } else if (!strcmp(k, "image")) {
      const char* src = v | "";
      if (strlen(src) > IMAGE_SRC_LEN) return fail(err, errLen, path, "image source too long");
      if (*src && !imageSourceOk(src)) return fail(err, errLen, path, "unknown image (upload it on the setup page, or use an http(s) URL)");
      strlcpy(st.image, src, sizeof(st.image));
    } else {
      char m[64];
      snprintf(m, sizeof(m), "unknown style property \"%.20s\"", k);
      return fail(err, errLen, path, m);
    }
  }
  return true;
}

bool LayoutWidget::parseNode(JsonVariantConst v, uint8_t parent, uint8_t depth, const char* path,
                             const NamedStyle* styles, uint8_t styleCount, char* err, size_t errLen) {
  (void)parent;
  if (!v.is<JsonObjectConst>()) return fail(err, errLen, path, "must be an object");
  if (_count >= MAX_NODES) {
    snprintf(err, errLen, "too many elements (max %u including nested)", MAX_NODES - 1);
    return false;
  }
  if (depth > MAX_DEPTH) return fail(err, errLen, path, "nested too deep");

  uint8_t idx = _count++;
  {
    LayoutNode& e = _n[idx];
    memset(&e, 0, sizeof(e));
    e.firstChild = NONE;
    e.nextSibling = NONE;
    e.x = e.y = e.w = e.h = EL_AUTO;
    e.x2 = e.y2 = EL_AUTO;
    e.a0 = 0;
    e.a1 = 360;

    const char* type = v["type"] | "";
    if      (!strcmp(type, "text"))     e.type = ElType::TEXT;
    else if (!strcmp(type, "line"))     e.type = ElType::LINE;
    else if (!strcmp(type, "rect"))     e.type = ElType::RECT;
    else if (!strcmp(type, "bar"))      e.type = ElType::BAR;
    else if (!strcmp(type, "box"))      e.type = ElType::BOX;
    else if (!strcmp(type, "circle"))   e.type = ElType::CIRCLE;
    else if (!strcmp(type, "ellipse"))  e.type = ElType::ELLIPSE;
    else if (!strcmp(type, "arc"))      e.type = ElType::ARC;
    else if (!strcmp(type, "triangle")) e.type = ElType::TRIANGLE;
    else if (!strcmp(type, "polygon"))  e.type = ElType::POLYGON;
    else if (!strcmp(type, "image"))    e.type = ElType::IMAGE;
    else if (!strcmp(type, "chart"))    e.type = ElType::CHART;
    else return fail(err, errLen, path, "unknown type (text, line, rect, bar, box, circle, ellipse, arc, triangle, polygon, image, chart)");

    // ---- style: defaults, class, flat fields, style object ----
    defaultStyle(e.st);
    bool shape = e.type == ElType::CIRCLE || e.type == ElType::ELLIPSE || e.type == ElType::TRIANGLE || e.type == ElType::POLYGON;
    if (shape) e.st.fill = true;
    if (e.type == ElType::ARC) { e.st.thick = 8; strlcpy(e.st.bg, "dim", sizeof(e.st.bg)); }
    if (e.type == ElType::LINE) e.st.thick = 1;
    if (e.type == ElType::CHART) { e.st.thick = 2; strlcpy(e.st.color, "accent", sizeof(e.st.color)); }

    const char* cls = v["class"] | (v["style"].is<const char*>() ? (const char*)v["style"] : "");
    if (*cls) {
      bool found = false;
      for (uint8_t i = 0; i < styleCount; i++) {
        if (!strcmp(styles[i].name, cls)) {
          LayoutStyle merged = styles[i].st;
          merged.fill = styles[i].st.fill || e.st.fill;
          if (e.type == ElType::ARC && merged.thick == 0) merged.thick = 8;
          if (e.type == ElType::ARC && !merged.bg[0]) strlcpy(merged.bg, "dim", sizeof(merged.bg));
          if (e.type == ElType::LINE && merged.thick == 0) merged.thick = 1;
          if (e.type == ElType::CHART && merged.thick == 0) merged.thick = 2;
          e.st = merged;
          found = true;
          break;
        }
      }
      if (!found) return fail(err, errLen, path, "unknown class (define it in \"styles\")");
    }

    if (!v["color"].isNull()) {
      if (!copyColor(e.st.color, sizeof(e.st.color), v["color"] | "text", path, "color", err, errLen)) return false;
    }
    if (!v["size"].isNull()) {
      double d;
      if (!bindNumber(v["size"], idx, BF_SIZE, d, path, "size", err, errLen)) return false;
      int size = v["size"].is<const char*>() ? 1 : (int)d;
      if (size < 1 || size > FONT_MAX_PX) return fail(err, errLen, path, "size must be 1-40 (bitmap font) or 6-160 (TrueType font)");
      e.st.size = size;
    }
    if (!v["font"].isNull()) {
      const char* f = v["font"] | "";
      if (strlen(f) >= sizeof(e.st.font)) return fail(err, errLen, path, "font name too long");
      if (*f && !fontExists(f)) return fail(err, errLen, path, "unknown font (see the setup page for the list)");
      strlcpy(e.st.font, f, sizeof(e.st.font));
    }
    if (!v["align"].isNull()) {
      if (!parseAlign(v["align"] | "", e.st.align)) return fail(err, errLen, path, "align must be left, center or right");
    }
    if (!v["fill"].isNull()) e.st.fill = v["fill"] | false;

    if (v["style"].is<JsonObjectConst>()) {
      if (!parseStyle(e.st, v["style"].as<JsonObjectConst>(), idx, path, err, errLen)) return false;
    }
    if (e.type == ElType::TEXT) {
      if (!e.st.font[0] && e.st.size > BITMAP_MAX_SCALE) return fail(err, errLen, path, "size must be 1-40 with the bitmap font; set \"font\" for pixel sizes");
      if (e.st.font[0] && e.st.size < FONT_MIN_PX) return fail(err, errLen, path, "size must be at least 6 with a TrueType font");
    }

    // ---- geometry (numbers or templates) ----
    double d;
    if (!v["x"].isNull())  { if (!bindNumber(v["x"],  idx, BF_X,  d, path, "x",  err, errLen)) return false; e.x  = (int16_t)d; }
    if (!v["y"].isNull())  { if (!bindNumber(v["y"],  idx, BF_Y,  d, path, "y",  err, errLen)) return false; e.y  = (int16_t)d; }
    if (!v["w"].isNull())  { if (!bindNumber(v["w"],  idx, BF_W,  d, path, "w",  err, errLen)) return false; e.w  = (int16_t)d; }
    if (!v["h"].isNull())  { if (!bindNumber(v["h"],  idx, BF_H,  d, path, "h",  err, errLen)) return false; e.h  = (int16_t)d; }
    if (!v["r"].isNull())  { if (!bindNumber(v["r"],  idx, BF_R,  d, path, "r",  err, errLen)) return false; e.w = e.h = (int16_t)d * 2; }
    if (!v["x2"].isNull()) { if (!bindNumber(v["x2"], idx, BF_X2, d, path, "x2", err, errLen)) return false; e.x2 = (int16_t)d; }
    if (!v["y2"].isNull()) { if (!bindNumber(v["y2"], idx, BF_Y2, d, path, "y2", err, errLen)) return false; e.y2 = (int16_t)d; }
    e.hasXY = (!v["x"].isNull() && !v["y"].isNull()) || e.st.absolute;
    if (e.hasXY) {
      if (e.x == EL_AUTO) e.x = 0;
      if (e.y == EL_AUTO) e.y = 0;
    }

    if (e.type == ElType::ARC) {
      if (!v["start"].isNull()) { if (!bindNumber(v["start"], idx, BF_A0, d, path, "start", err, errLen)) return false; e.a0 = (int16_t)d; }
      if (!v["end"].isNull())   { if (!bindNumber(v["end"],   idx, BF_A1, d, path, "end",   err, errLen)) return false; e.a1 = (int16_t)d; }
    }

    if (e.type == ElType::CHART) {
      const char* key = v["series"] | "";
      if (!*key) return fail(err, errLen, path, "chart needs \"series\": a key sampled on the setup page under History");
      if (!seriesValidKey(key) || strlen(key) >= sizeof(e.text)) return fail(err, errLen, path, "\"series\" is not a valid key");
      strlcpy(e.text, key, sizeof(e.text));
      noteKeys(key);
      if (!v["window"].isNull()) {
        if (!bindNumber(v["window"], idx, BF_WINDOW, d, path, "window", err, errLen)) return false;
        e.window = d < 0 ? 0 : (uint32_t)d;
      }
    }

    if (e.type == ElType::TRIANGLE || e.type == ElType::POLYGON) {
      JsonArrayConst pts = v["points"].as<JsonArrayConst>();
      if (pts.isNull()) return fail(err, errLen, path, "\"points\" must be an array of [x,y] pairs");
      uint8_t n = 0;
      for (JsonVariantConst p : pts) {
        if (n >= 8) return fail(err, errLen, path, "too many points (max 8)");
        if (!p.is<JsonArrayConst>() || p.size() != 2) return fail(err, errLen, path, "each point must be [x,y]");
        if (!bindNumber(p[0], idx, BF_PT0 + n * 2,     d, path, "point x", err, errLen)) return false;
        e.pts[n * 2] = (int16_t)d;
        if (!bindNumber(p[1], idx, BF_PT0 + n * 2 + 1, d, path, "point y", err, errLen)) return false;
        e.pts[n * 2 + 1] = (int16_t)d;
        n++;
      }
      if (e.type == ElType::TRIANGLE && n != 3) return fail(err, errLen, path, "triangle needs exactly 3 points");
      if (n < 3) return fail(err, errLen, path, "polygon needs at least 3 points");
      e.npts = n;
    }

    // ---- image ----
    if (e.type == ElType::IMAGE) {
      const char* src = v["src"] | "";
      if (!*src) return fail(err, errLen, path, "image needs \"src\": an uploaded image name or an http(s) URL");
      if (strlen(src) > IMAGE_SRC_LEN) return fail(err, errLen, path, "image source too long");
      if (!imageSourceOk(src)) return fail(err, errLen, path, "unknown image (upload it on the setup page, or use an http(s) URL)");
      strlcpy(e.src, src, sizeof(e.src));
      int refresh = v["refresh"] | (int)IMAGE_DEFAULT_REFRESH_S;
      if (refresh < (int)IMAGE_MIN_REFRESH_S) refresh = IMAGE_MIN_REFRESH_S;
      if (refresh > 86400) refresh = 86400;
      e.refreshS = refresh;
    }

    // ---- text / value ----
    if (e.type != ElType::CHART) {
      const char* text = "";
      if (e.type == ElType::BAR || e.type == ElType::ARC) text = v["value"] | "0";
      else if (e.type == ElType::TEXT) text = v["text"] | "";
      if (strlen(text) >= sizeof(e.text)) {
        char m[64];
        snprintf(m, sizeof(m), "text longer than %u characters", (unsigned)(sizeof(e.text) - 1));
        return fail(err, errLen, path, m);
      }
      strlcpy(e.text, text, sizeof(e.text));
    }

    // Keys may sit inside an expression, so look for the prefix anywhere.
    const char* scan[5] = { e.text, e.st.color, e.st.bg, e.st.border, e.st.gradient };
    for (const char* s : scan) noteKeys(s);
    if (e.type == ElType::IMAGE || e.st.image[0]) _usesImages = true;
  }

  // ---- children ----
  JsonVariantConst children = v["children"];
  if (!children.isNull()) {
    if (_n[idx].type != ElType::BOX) return fail(err, errLen, path, "only a box can have children");
    if (!children.is<JsonArrayConst>()) return fail(err, errLen, path, "\"children\" must be an array");
    uint8_t last = NONE;
    int ci = 0;
    for (JsonVariantConst c : children.as<JsonArrayConst>()) {
      char cpath[24];
      snprintf(cpath, sizeof(cpath), "%s.%d", path, ci);
      uint8_t childIdx = _count;
      if (!parseNode(c, idx, depth + 1, cpath, styles, styleCount, err, errLen)) return false;
      if (last == NONE) _n[idx].firstChild = childIdx;
      else _n[last].nextSibling = childIdx;
      last = childIdx;
      ci++;
    }
  }
  return true;
}

bool LayoutWidget::load(JsonVariantConst doc, char* err, size_t errLen) {
  clear();
  if (errLen) err[0] = '\0';
  if (!alloc()) { strlcpy(err, "out of memory", errLen); return false; }

  if (!doc.is<JsonObjectConst>()) return fail(err, errLen, "", "layout must be a JSON object");

  const char* name = doc["name"] | "";
  strlcpy(_name, *name ? name : (_id[0] ? _id : "Layout"), sizeof(_name));

  JsonArrayConst elements = doc["elements"].as<JsonArrayConst>();
  if (elements.isNull()) return fail(err, errLen, "", "\"elements\" must be an array");

  NamedStyle* styles = (NamedStyle*)malloc(sizeof(NamedStyle) * MAX_STYLES);
  if (!styles) { strlcpy(err, "out of memory", errLen); return false; }
  uint8_t styleCount = 0;
  JsonVariantConst stylesV = doc["styles"];
  if (!stylesV.isNull()) {
    if (!stylesV.is<JsonObjectConst>()) { free(styles); return fail(err, errLen, "", "\"styles\" must be an object"); }
    for (JsonPairConst kv : stylesV.as<JsonObjectConst>()) {
      if (styleCount >= MAX_STYLES) { free(styles); snprintf(err, errLen, "too many styles (max %u)", MAX_STYLES); return false; }
      NamedStyle& ns = styles[styleCount];
      strlcpy(ns.name, kv.key().c_str(), sizeof(ns.name));
      defaultStyle(ns.st);
      char spath[32];
      snprintf(spath, sizeof(spath), "style \"%.16s\"", ns.name);
      if (!kv.value().is<JsonObjectConst>()) { free(styles); return fail(err, errLen, spath, "must be an object"); }
      if (!parseStyle(ns.st, kv.value().as<JsonObjectConst>(), NONE, spath, err, errLen)) { free(styles); return false; }
      styleCount++;
    }
  }

  // Implicit root box: the whole screen, styled by the top-level "style".
  LayoutNode& root = _n[0];
  memset(&root, 0, sizeof(root));
  root.type = ElType::BOX;
  root.x = root.y = 0;
  root.w = SCREEN_W;
  root.h = SCREEN_H;
  root.hasXY = true;
  root.firstChild = NONE;
  root.nextSibling = NONE;
  defaultStyle(root.st);
  _count = 1;
  if (doc["style"].is<JsonObjectConst>()) {
    if (!parseStyle(root.st, doc["style"].as<JsonObjectConst>(), 0, "root", err, errLen)) { free(styles); return false; }
  }

  uint8_t last = NONE;
  int i = 0;
  for (JsonVariantConst v : elements) {
    char path[8];
    snprintf(path, sizeof(path), "%d", i);
    uint8_t childIdx = _count;
    if (!parseNode(v, 0, 1, path, styles, styleCount, err, errLen)) { free(styles); clear(); return false; }
    if (last == NONE) _n[0].firstChild = childIdx;
    else _n[last].nextSibling = childIdx;
    last = childIdx;
    i++;
  }
  free(styles);

  if (!doc["led"].isNull() && !parseLed(doc["led"], err, errLen)) { clear(); return false; }

  _loaded = true;
  return true;
}

// =====================
// LED
// =====================
static bool roleRgb(const char* name, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (!strcmp(name, "ok"))     { r = 0;   g = 255; b = 0;   return true; }
  if (!strcmp(name, "warn"))   { r = 255; g = 120; b = 0;   return true; }
  if (!strcmp(name, "bad"))    { r = 255; g = 0;   b = 0;   return true; }
  if (!strcmp(name, "accent")) { r = 0;   g = 120; b = 255; return true; }
  if (!strcmp(name, "text"))   { r = 255; g = 255; b = 255; return true; }
  if (!strcmp(name, "dim"))    { r = 40;  g = 40;  b = 40;  return true; }
  if (!strcmp(name, "bg"))     { r = 0;   g = 0;   b = 0;   return true; }
  if (name[0] == '#' && strlen(name) == 7) {
    char* end;
    long v = strtol(name + 1, &end, 16);
    if (*end == '\0') { r = (v >> 16) & 0xFF; g = (v >> 8) & 0xFF; b = v & 0xFF; return true; }
  }
  if (name[0] >= '0' && name[0] <= '9') {
    char* end;
    unsigned long v = strtoul(name, &end, 10);
    if (*end == '\0' && v <= 0xFFFFFF) { r = (v >> 16) & 0xFF; g = (v >> 8) & 0xFF; b = v & 0xFF; return true; }
  }
  return false;
}

bool LayoutWidget::parseLedRule(LedRule& r, JsonObjectConst obj, const char* path, char* err, size_t errLen) {
  memset(&r, 0, sizeof(r));
  r.brightness = -1;
  for (JsonPairConst kv : obj) {
    const char* k = kv.key().c_str();
    JsonVariantConst v = kv.value();
    if (!strcmp(k, "when")) {
      const char* w = v | "";
      if (strlen(w) >= sizeof(r.when)) return fail(err, errLen, path, "\"when\" too long");
      strlcpy(r.when, w, sizeof(r.when));
    } else if (!strcmp(k, "key")) {
      const char* w = v | "";
      if (strlen(w) >= sizeof(r.key)) return fail(err, errLen, path, "\"key\" too long");
      strlcpy(r.key, w, sizeof(r.key));
    } else if (!strcmp(k, "is")) {
      const char* w = v | "";
      if (strlen(w) >= sizeof(r.is)) return fail(err, errLen, path, "\"is\" too long");
      strlcpy(r.is, w, sizeof(r.is));
    } else if (!strcmp(k, "color")) {
      if (!copyColor(r.color, sizeof(r.color), v | "", path, "color", err, errLen)) return false;
      r.hasColor = true;
    } else if (!strcmp(k, "mode")) {
      if (!ledModeFromName(v | "", r.mode)) return fail(err, errLen, path, "mode must be off, solid, breathe, blink, pulse or rainbow");
      r.hasMode = true;
    } else if (!strcmp(k, "speed")) {
      int sp = v | 2000;
      if (sp < 100) sp = 100;
      if (sp > 60000) sp = 60000;
      r.speed = sp;
      r.hasSpeed = true;
    } else if (!strcmp(k, "brightness")) {
      int b = v | 100;
      if (b < 0) b = 0;
      if (b > 100) b = 100;
      r.brightness = b;
      r.hasBrightness = true;
    } else if (!strcmp(k, "rules")) {
      // handled by parseLed
    } else {
      char m[64];
      snprintf(m, sizeof(m), "unknown led property \"%.20s\"", k);
      return fail(err, errLen, path, m);
    }
  }
  if (r.key[0] && !r.is[0]) return fail(err, errLen, path, "\"key\" needs \"is\"");
  return true;
}

bool LayoutWidget::parseLed(JsonVariantConst v, char* err, size_t errLen) {
  if (!v.is<JsonObjectConst>()) return fail(err, errLen, "", "\"led\" must be an object");
  memset(_led, 0, sizeof(*_led));
  if (!parseLedRule(_led->base, v.as<JsonObjectConst>(), "led", err, errLen)) return false;
  if (!_led->base.hasMode) { _led->base.mode = LedMode::SOLID; _led->base.hasMode = true; }
  if (!_led->base.hasSpeed) { _led->base.speed = 2000; _led->base.hasSpeed = true; }
  JsonVariantConst rules = v["rules"];
  if (!rules.isNull()) {
    if (!rules.is<JsonArrayConst>()) return fail(err, errLen, "led", "\"rules\" must be an array");
    int i = 0;
    for (JsonVariantConst rv : rules.as<JsonArrayConst>()) {
      if (_led->ruleCount >= 6) return fail(err, errLen, "led", "too many rules (max 6)");
      char path[24];
      snprintf(path, sizeof(path), "led rule %d", i);
      if (!rv.is<JsonObjectConst>()) return fail(err, errLen, path, "must be an object");
      LedRule& r = _led->rules[_led->ruleCount];
      if (!parseLedRule(r, rv.as<JsonObjectConst>(), path, err, errLen)) return false;
      if (!r.when[0] && !r.key[0]) return fail(err, errLen, path, "needs \"when\" or \"key\"/\"is\"");
      if (!r.hasColor && !r.hasMode && !r.hasSpeed && !r.hasBrightness) return fail(err, errLen, path, "sets nothing");
      _led->ruleCount++;
      i++;
    }
  }
  _hasLed = true;
  return true;
}

void LayoutWidget::updateLed() {
  const LedRule* pick = nullptr;
  for (uint8_t i = 0; i < _led->ruleCount && !pick; i++) {
    const LedRule& r = _led->rules[i];
    bool match = false;
    if (r.key[0]) {
      char val[48];
      match = layoutResolveKey(r.key, val, sizeof(val)) && strcasecmp(val, r.is) == 0;
    } else {
      double d;
      match = layoutEvalNumber(r.when, d) && d != 0;
    }
    if (match) pick = &r;
  }
  const LedRule& b = _led->base;
  const char* color = (pick && pick->hasColor) ? pick->color : b.color;
  LedMode mode = (pick && pick->hasMode) ? pick->mode : b.mode;
  uint16_t speed = (pick && pick->hasSpeed) ? pick->speed : b.speed;
  int16_t bright = (pick && pick->hasBrightness) ? pick->brightness : b.brightness;

  char buf[32];
  const char* name = color;
  if (strchr(color, '{')) { layoutExpand(color, buf, sizeof(buf)); name = buf; }
  uint8_t r, g, bl;
  if (!name[0] || !roleRgb(name, r, g, bl)) {
    r = g = bl = 0;
    if (mode != LedMode::RAINBOW) mode = LedMode::OFF;   // rainbow needs no colour
  }
  _ledSpec.mode = mode;
  _ledSpec.r = r;
  _ledSpec.g = g;
  _ledSpec.b = bl;
  _ledSpec.speedMs = speed;
  _ledSpec.brightness = bright;
}

bool LayoutWidget::ledSpec(LedSpec& out) {
  if (!_loaded || !_hasLed) return false;
  out = _ledSpec;
  return out.mode != LedMode::OFF;
}

// =====================
// RUNTIME
// =====================
// Tells every data source this layout references that it is on screen,
// so it gets fetched on its interval. Cheap, but no need to do it per frame.
void LayoutWidget::touchSources() {
  uint32_t now = millis();
  if (now - _lastTouch < 500 && _lastTouch != 0) return;
  _lastTouch = now;

  auto touchIn = [&](const char* f) {
      for (const char* p = strstr(f, "api."); p; p = strstr(p + 1, "api.")) {
        if (p > f) {
          char b = p[-1];
          bool boundary = b == '{' || b == ' ' || b == '(' || b == ',' || b == '+' || b == '-' || b == '*' || b == '/' || b == '%' || b == '^';
          if (!boundary) continue;
        }
        const char* start = p + 4;
        const char* end = strchr(start, '.');
        if (!end) continue;
        size_t len = end - start;
        if (len == 0 || len > SOURCE_ID_LEN) continue;
        char id[SOURCE_ID_LEN + 1];
        memcpy(id, start, len);
        id[len] = '\0';
        sourceTouch(id, now);
      }
  };
  for (uint8_t i = 1; i < _count; i++) {
    const char* fields[5] = { _n[i].text, _n[i].st.color, _n[i].st.bg, _n[i].st.border, _n[i].st.gradient };
    for (const char* f : fields) touchIn(f);
  }
  for (uint8_t b = 0; b < _bindCount; b++) touchIn(_bind[b].tpl);
}

void LayoutWidget::update(uint32_t now) {
  if (_usesPing) pingLoop(now);
  if (_usesApi) touchSources();
  if (_hasLed && (now - _lastLedEval >= 250 || _lastLedEval == 0)) { _lastLedEval = now; updateLed(); }
  if (_usesImages && now - _lastImageTouch >= 1000) {
    _lastImageTouch = now;
    for (uint8_t i = 1; i < _count; i++) {
      if (_n[i].type == ElType::IMAGE) imageTouch(_n[i].src, _n[i].refreshS);
      if (_n[i].st.image[0]) imageTouch(_n[i].st.image, IMAGE_DEFAULT_REFRESH_S);
    }
  }
}

uint16_t LayoutWidget::resolveColor(const char* spec, uint16_t fallback, bool* present) {
  if (present) *present = spec[0] != '\0';
  if (!spec[0]) return fallback;
  char buf[32];
  const char* name = spec;
  if (strchr(spec, '{')) {
    // A template that resolves to no known role (missing target, no data)
    // renders dim, matching how missing text values look.
    layoutExpand(spec, buf, sizeof(buf));
    name = buf;
    fallback = COLOR_DIM;
  }
  uint16_t c;
  return roleColor(name, c) ? c : fallback;
}

// =====================
// LAYOUT
// =====================
// Evaluates every templated number and writes it into its node. An
// unresolvable template counts as 0, like an unknown key in a bar's value.
void LayoutWidget::applyBindings() {
  char buf[32];
  for (uint8_t k = 0; k < _bindCount; k++) {
    const LayoutBinding& b = _bind[k];
    layoutExpand(b.tpl, buf, sizeof(buf));
    char* end;
    double d = strtod(buf, &end);
    if (end == buf || isnan(d) || isinf(d)) d = 0;
    LayoutNode& e = _n[b.node];
    long v = lround(d);
    if (v < INT16_MIN + 1) v = INT16_MIN + 1;
    if (v > INT16_MAX) v = INT16_MAX;
    switch (b.field) {
      case BF_X:  e.x = (int16_t)v; break;
      case BF_Y:  e.y = (int16_t)v; break;
      case BF_W:  e.w = (int16_t)v; break;
      case BF_H:  e.h = (int16_t)v; break;
      case BF_R:  e.w = e.h = (int16_t)(v * 2); break;
      case BF_X2: e.x2 = (int16_t)v; break;
      case BF_Y2: e.y2 = (int16_t)v; break;
      case BF_A0: e.a0 = (int16_t)v; break;
      case BF_A1: e.a1 = (int16_t)v; break;
      case BF_WINDOW: e.window = v < 0 ? 0 : (uint32_t)v; break;
      case BF_SIZE: e.st.size = clampU8((int)v, 1, e.st.font[0] ? FONT_MAX_PX : BITMAP_MAX_SCALE); break;
      case BF_THICK: e.st.thick = clampU8((int)v, 1, 80); break;
      case BF_RADIUS: e.st.radius = clampU8((int)v, 0, 80); break;
      case BF_BORDERW: e.st.borderW = clampU8((int)v, 0, 20); break;
      case BF_PAD: e.st.pad = clampU8((int)v, 0, 80); break;
      case BF_GAP: e.st.gap = clampU8((int)v, 0, 200); break;
      case BF_MIN: e.st.vmin = (float)d; break;
      case BF_MAX: e.st.vmax = (float)d; break;
      default:
        if (b.field >= BF_PT0 && b.field < BF_PT0 + 16) e.pts[b.field - BF_PT0] = (int16_t)v;
        break;
    }
  }
}

void LayoutWidget::expandAll() {
  applyBindings();
  for (uint8_t i = 1; i < _count; i++) {
    if (_n[i].type == ElType::TEXT || _n[i].type == ElType::BAR || _n[i].type == ElType::ARC) {
      layoutExpand(_n[i].text, _txt[i], TEXT_BUF);
    } else {
      _txt[i][0] = '\0';
    }
  }
}

// Width of a string in the bitmap font. Characters the bitmap font lacks
// (anything non-ASCII, e.g. emoji) come from the TrueType fallback at the
// same line height.
static int16_t bitmapTextW(const char* s, uint8_t size) {
  int16_t w = 0;
  const char* p = s;
  for (uint32_t cp = utf8Next(&p); cp; cp = utf8Next(&p)) {
    if (cp < 0x80) w += CHAR_W * size;
    else w += fontDrawFallbackGlyph(nullptr, cp, CHAR_H * size, 0, 0, 0);
  }
  return w;
}

static int16_t textW(const LayoutStyle& st, const char* s) {
  if (st.font[0]) return fontTextWidth(st.font, s, st.size);
  return bitmapTextW(s, st.size);
}

static int16_t textH(const LayoutStyle& st) {
  return st.font[0] ? st.size : CHAR_H * st.size;
}

// Bitmap-font text with TrueType fallback for characters it lacks.
// Drawn with a transparent background so text sits on box backgrounds
// and images; an element's own background is filled by the caller.
static void drawBitmapText(lgfx::LGFX_Sprite& ui, const char* s, int16_t x, int16_t y, uint8_t size, uint16_t color) {
  ui.setTextSize(size);
  ui.setTextColor(color);
  ui.setTextDatum(top_left);
  char run[TEXT_RUN];
  size_t rl = 0;
  int16_t pen = x;
  const char* p = s;
  for (;;) {
    const char* before = p;
    uint32_t cp = utf8Next(&p);
    if (cp && cp < 0x80 && rl + 1 < sizeof(run)) { run[rl++] = (char)cp; continue; }
    if (rl) { run[rl] = '\0'; ui.drawString(run, pen, y); pen += rl * CHAR_W * size; rl = 0; }
    if (!cp) break;
    if (cp < 0x80) { p = before; continue; }   // run buffer was full; retry this char
    pen += fontDrawFallbackGlyph(&ui, cp, CHAR_H * size, pen, y, color);
  }
}

// Intrinsic size into lw/lh. Explicit w/h win; EL_AUTO stays for a
// dimension the parent may stretch.
void LayoutWidget::measure(uint8_t i) {
  LayoutNode& e = _n[i];
  int16_t w = EL_AUTO, h = EL_AUTO;

  switch (e.type) {
    case ElType::TEXT:
      w = textW(e.st, _txt[i]);
      h = textH(e.st);
      break;
    case ElType::LINE:
      if (e.hasXY && e.x2 != EL_AUTO) {
        w = abs(e.x2 - e.x) + 1;
        h = (e.y2 == EL_AUTO ? 0 : abs(e.y2 - e.y)) + 1;
      } else {
        h = e.st.thick;
      }
      break;
    case ElType::BAR:
      h = 8;
      break;
    case ElType::CHART:
      h = 40;
      break;
    case ElType::RECT:
    case ElType::ELLIPSE:
      break;
    case ElType::IMAGE: {
      // Native size, or keep the aspect ratio when only one side is given.
      int16_t iw, ih;
      if (imageSize(e.src, iw, ih) && iw > 0 && ih > 0) {
        if (e.w != EL_AUTO && e.h == EL_AUTO) h = (int16_t)((int32_t)e.w * ih / iw);
        else if (e.h != EL_AUTO && e.w == EL_AUTO) w = (int16_t)((int32_t)e.h * iw / ih);
        else { w = iw; h = ih; }
      }
      break;
    }
    case ElType::CIRCLE:
    case ElType::ARC:
      if (e.w != EL_AUTO && e.h == EL_AUTO) h = e.w;
      if (e.h != EL_AUTO && e.w == EL_AUTO) w = e.h;
      break;
    case ElType::TRIANGLE:
    case ElType::POLYGON: {
      int16_t maxX = 0, maxY = 0;
      for (uint8_t k = 0; k < e.npts; k++) {
        if (e.pts[k * 2] > maxX) maxX = e.pts[k * 2];
        if (e.pts[k * 2 + 1] > maxY) maxY = e.pts[k * 2 + 1];
      }
      w = maxX + 1;
      h = maxY + 1;
      break;
    }
    case ElType::BOX: {
      int16_t main = 0, cross = 0;
      int n = 0;
      for (uint8_t c = e.firstChild; c != NONE; c = _n[c].nextSibling) {
        measure(c);
        if (_n[c].hasXY) continue;
        int16_t cw = _n[c].lw == EL_AUTO ? 0 : _n[c].lw;
        int16_t chh = _n[c].lh == EL_AUTO ? 0 : _n[c].lh;
        int16_t m = e.st.row ? cw : chh;
        int16_t x = e.st.row ? chh : cw;
        main += m;
        if (x > cross) cross = x;
        n++;
      }
      if (n > 1) main += e.st.gap * (n - 1);
      main += 2 * e.st.pad;
      cross += 2 * e.st.pad;
      w = e.st.row ? main : cross;
      h = e.st.row ? cross : main;
      break;
    }
  }

  e.lw = e.w != EL_AUTO ? e.w : w;
  e.lh = e.h != EL_AUTO ? e.h : h;
}

void LayoutWidget::place(uint8_t i, int16_t x, int16_t y, int16_t w, int16_t h) {
  LayoutNode& e = _n[i];
  e.lx = x;
  e.ly = y;
  e.lw = w;
  e.lh = h;
  if (e.type != ElType::BOX) return;

  int16_t cx = x + e.st.pad, cy = y + e.st.pad;
  int16_t cw = w - 2 * e.st.pad, ch = h - 2 * e.st.pad;
  if (cw < 0) cw = 0;
  if (ch < 0) ch = 0;
  int16_t mainAvail = e.st.row ? cw : ch;
  int16_t crossAvail = e.st.row ? ch : cw;

  // Pass 1: total of the flow children along the main axis.
  int16_t total = 0;
  int n = 0;
  for (uint8_t c = e.firstChild; c != NONE; c = _n[c].nextSibling) {
    LayoutNode& k = _n[c];
    if (k.hasXY) continue;
    int16_t m = e.st.row ? k.lw : k.lh;
    total += m == EL_AUTO ? 0 : m;
    n++;
  }
  int16_t gap = e.st.gap;
  if (n > 1) total += gap * (n - 1);
  int16_t freeSpace = mainAvail - total;
  int16_t offset = 0;
  if (freeSpace > 0) {
    if (e.st.justify == ElJustify::CENTER) offset = freeSpace / 2;
    else if (e.st.justify == ElJustify::END) offset = freeSpace;
    else if (e.st.justify == ElJustify::BETWEEN && n > 1) gap += freeSpace / (n - 1);
  }

  // Pass 2: position.
  int16_t cursor = offset;
  for (uint8_t c = e.firstChild; c != NONE; c = _n[c].nextSibling) {
    LayoutNode& k = _n[c];
    if (k.hasXY) {
      int16_t kw = k.lw == EL_AUTO ? 0 : k.lw;
      int16_t kh = k.lh == EL_AUTO ? 0 : k.lh;
      int16_t kx = cx + k.x;
      // Absolute text with no width keeps the old anchor semantics: x is
      // the left, centre or right edge depending on align.
      if (k.type == ElType::TEXT && k.w == EL_AUTO) {
        if (k.st.align == ElAlign::CENTER) kx -= kw / 2;
        else if (k.st.align == ElAlign::END) kx -= kw;
      }
      place(c, kx, cy + k.y, kw, kh);
      continue;
    }
    int16_t mainSize = e.st.row ? k.lw : k.lh;
    if (mainSize == EL_AUTO) mainSize = 0;
    int16_t crossSize = e.st.row ? k.lh : k.lw;
    bool stretch = e.st.align == ElAlign::STRETCH;
    bool crossExplicit = (e.st.row ? k.h : k.w) != EL_AUTO;
    if (crossSize == EL_AUTO) crossSize = stretch ? crossAvail : 0;
    else if (stretch && !crossExplicit &&
             (k.type == ElType::TEXT || k.type == ElType::BAR || k.type == ElType::BOX || k.type == ElType::LINE || k.type == ElType::CHART)) {
      crossSize = crossAvail;
    }
    int16_t crossOff = 0;
    if (e.st.align == ElAlign::CENTER) crossOff = (crossAvail - crossSize) / 2;
    else if (e.st.align == ElAlign::END) crossOff = crossAvail - crossSize;
    if (e.st.row) place(c, cx + cursor, cy + crossOff, mainSize, crossSize);
    else          place(c, cx + crossOff, cy + cursor, crossSize, mainSize);
    cursor += mainSize + gap;
  }
}

// =====================
// ANTIALIASING
// =====================
// The library draws smooth filled circles, rounded rectangles and wide
// lines natively. Everything else with a curve or a slope (arcs,
// ellipses, triangles, polygons, hollow rounded shapes) is drawn at 2x
// into a scratch sprite and downsampled with coverage as alpha.
static const uint16_t SS_KEY = 0x0821;     // "nothing drawn" in the scratch sprite
static lgfx::LGFX_Sprite* ssSprite = nullptr;
static int16_t ssW = 0, ssH = 0;           // scratch size in 1x pixels
static uint8_t* ssArgb = nullptr;
static size_t ssArgbSize = 0;

static lgfx::LGFX_Sprite* ssBegin(int16_t w, int16_t h) {
  if (w <= 0 || h <= 0 || w > SCREEN_W || h > SCREEN_H) return nullptr;
  if (!ssSprite || ssW < w || ssH < h) {
    int16_t nw = ssW > w ? ssW : w, nh = ssH > h ? ssH : h;
    if (ssSprite) { ssSprite->deleteSprite(); delete ssSprite; ssSprite = nullptr; }
    ssSprite = new lgfx::LGFX_Sprite();
    ssSprite->setPsram(true);
    ssSprite->setColorDepth(16);
    if (!ssSprite->createSprite(nw * 2, nh * 2)) { delete ssSprite; ssSprite = nullptr; ssW = ssH = 0; return nullptr; }
    ssW = nw;
    ssH = nh;
  }
  ssSprite->setClipRect(0, 0, w * 2, h * 2);
  ssSprite->fillRect(0, 0, w * 2, h * 2, SS_KEY);
  return ssSprite;
}

// Downsamples the 2x scratch into ui at (x, y): each output pixel gets the
// average colour of its drawn samples and their count as alpha.
static void ssEnd(lgfx::LGFX_Sprite& ui, int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!ssSprite) return;
  size_t need = (size_t)w * h * 4;
  if (need > ssArgbSize) {
    free(ssArgb);
    ssArgb = (uint8_t*)heap_caps_malloc(need, MALLOC_CAP_SPIRAM);
    if (!ssArgb) ssArgb = (uint8_t*)malloc(need);
    ssArgbSize = ssArgb ? need : 0;
    if (!ssArgb) return;
  }
  // Typed as rgb565_t: the uint16_t overload of readRect hands back
  // byte-swapped pixels, which flipped every colour and hid the key.
  static lgfx::rgb565_t row0[SCREEN_W * 2], row1[SCREEN_W * 2];
  lgfx::argb8888_t* out = (lgfx::argb8888_t*)ssArgb;
  for (int16_t oy = 0; oy < h; oy++) {
    ssSprite->readRect(0, oy * 2, w * 2, 1, row0);
    ssSprite->readRect(0, oy * 2 + 1, w * 2, 1, row1);
    for (int16_t ox = 0; ox < w; ox++) {
      uint16_t s[4] = { row0[ox * 2].raw, row0[ox * 2 + 1].raw, row1[ox * 2].raw, row1[ox * 2 + 1].raw };
      uint16_t r = 0, g = 0, b = 0;
      uint8_t n = 0;
      for (uint8_t k = 0; k < 4; k++) {
        if (s[k] == SS_KEY) continue;
        r += (s[k] >> 11) & 0x1F;
        g += (s[k] >> 5) & 0x3F;
        b += s[k] & 0x1F;
        n++;
      }
      if (n == 0) { out[oy * w + ox] = lgfx::argb8888_t(0, 0, 0, 0); continue; }
      uint8_t r8 = (r / n) * 255 / 31, g8 = (g / n) * 255 / 63, b8 = (b / n) * 255 / 31;
      out[oy * w + ox] = lgfx::argb8888_t(n * 64 - (n == 4 ? 1 : 0), r8, g8, b8);
    }
  }
  ui.pushAlphaImage(x, y, w, h, out);
}

// Hollow rounded rectangle with a smooth edge, via the supersampler.
static void smoothRoundRectOutline(lgfx::LGFX_Sprite& ui, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t bw, uint16_t color) {
  lgfx::LGFX_Sprite* sp = ssBegin(w, h);
  if (!sp) { ui.drawRoundRect(x, y, w, h, r, color); return; }
  sp->fillSmoothRoundRect(0, 0, w * 2, h * 2, r * 2, color);
  if (w > 2 * bw && h > 2 * bw) {
    int16_t ir = r > bw ? (r - bw) * 2 : 0;
    sp->fillSmoothRoundRect(bw * 2, bw * 2, (w - 2 * bw) * 2, (h - 2 * bw) * 2, ir, SS_KEY);
  }
  ssEnd(ui, x, y, w, h);
}

// =====================
// DRAWING
// =====================
static void fillPolygon(lgfx::LGFX_Sprite& ui, const int16_t* px, const int16_t* py, uint8_t n, uint16_t color) {
  int16_t minY = py[0], maxY = py[0];
  for (uint8_t i = 1; i < n; i++) { if (py[i] < minY) minY = py[i]; if (py[i] > maxY) maxY = py[i]; }
  for (int16_t y = minY; y <= maxY; y++) {
    int16_t xs[16];
    uint8_t cnt = 0;
    for (uint8_t i = 0, j = n - 1; i < n; j = i++) {
      int16_t y0 = py[j], y1 = py[i];
      if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
        int32_t x = px[j] + (int32_t)(y - y0) * (px[i] - px[j]) / (y1 - y0);
        if (cnt < 16) xs[cnt++] = (int16_t)x;
      }
    }
    for (uint8_t a = 1; a < cnt; a++) {
      int16_t t = xs[a];
      uint8_t b = a;
      while (b > 0 && xs[b - 1] > t) { xs[b] = xs[b - 1]; b--; }
      xs[b] = t;
    }
    for (uint8_t a = 0; a + 1 < cnt; a += 2) ui.drawFastHLine(xs[a], y, xs[a + 1] - xs[a] + 1, color);
  }
}

static void clipIntersect(int16_t& x, int16_t& y, int16_t& w, int16_t& h, int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  int16_t x1 = x > bx ? x : bx;
  int16_t y1 = y > by ? y : by;
  int16_t x2 = (x + w < bx + bw) ? x + w : bx + bw;
  int16_t y2 = (y + h < by + bh) ? y + h : by + bh;
  x = x1;
  y = y1;
  w = x2 > x1 ? x2 - x1 : 0;
  h = y2 > y1 ? y2 - y1 : 0;
}

void LayoutWidget::draw(lgfx::LGFX_Sprite& ui, uint8_t i, int16_t cx, int16_t cy, int16_t cw, int16_t ch) {
  LayoutNode& e = _n[i];
  int16_t x = e.lx, y = e.ly, w = e.lw, h = e.lh;
  if (w < 0) w = 0;
  if (h < 0) h = 0;

  bool hasBg, hasBorder, hasGrad;
  uint16_t color  = resolveColor(e.st.color, COLOR_TEXT);
  uint16_t bg     = resolveColor(e.st.bg, COLOR_BG, &hasBg);
  uint16_t border = resolveColor(e.st.border, COLOR_DIM, &hasBorder);
  uint16_t grad   = resolveColor(e.st.gradient, COLOR_BG, &hasGrad);
  uint8_t bw = hasBorder ? (e.st.borderW ? e.st.borderW : 1) : 0;

  switch (e.type) {
    case ElType::BOX:
    case ElType::RECT: {
      // rect keeps its old meaning: fill=true fills with colour, else outlines with colour.
      bool fillIt = hasBg || (e.type == ElType::RECT && e.st.fill);
      uint16_t fillColor = hasBg ? bg : color;
      bool outline = bw > 0 || (e.type == ElType::RECT && !e.st.fill && !hasBg);
      uint16_t outlineColor = bw > 0 ? border : color;
      uint8_t ow = bw > 0 ? bw : 1;
      // A gradient fades from the fill colour to style.gradient.
      auto fillArea = [&](int16_t fx, int16_t fy, int16_t fw, int16_t fh, int16_t fr) {
        if (hasGrad) fillGradient(ui, fx, fy, fw, fh, fr, fillColor, grad, e.st.gradientRight);
        else if (fr) ui.fillSmoothRoundRect(fx, fy, fw, fh, fr, fillColor);
        else ui.fillRect(fx, fy, fw, fh, fillColor);
      };
      if (hasGrad) fillIt = true;
      if (w > 0 && h > 0) {
        if (e.st.radius) {
          // Rounded: smooth fill; a border is a smooth outer fill with the
          // inner area refilled, or a supersampled hollow ring.
          if (outline && fillIt) {
            ui.fillSmoothRoundRect(x, y, w, h, e.st.radius, outlineColor);
            if (w > 2 * ow && h > 2 * ow) {
              int16_t ir = e.st.radius > ow ? e.st.radius - ow : 0;
              fillArea(x + ow, y + ow, w - 2 * ow, h - 2 * ow, ir);
            }
          } else if (fillIt) {
            fillArea(x, y, w, h, e.st.radius);
          } else if (outline) {
            smoothRoundRectOutline(ui, x, y, w, h, e.st.radius, ow, outlineColor);
          }
        } else {
          if (fillIt) fillArea(x, y, w, h, 0);
          if (outline) {
            for (uint8_t k = 0; k < ow && k * 2 < w && k * 2 < h; k++) ui.drawRect(x + k, y + k, w - 2 * k, h - 2 * k, outlineColor);
          }
        }
      }
      if (e.type == ElType::BOX) {
        // Children (and a background image) are clipped to the box.
        int16_t nx = x, ny = y, nw = w, nh = h;
        clipIntersect(nx, ny, nw, nh, cx, cy, cw, ch);
        ui.setClipRect(nx, ny, nw, nh);
        if (e.st.image[0]) imageDraw(ui, e.st.image, x, y, w, h, ImgFit::COVER, millis());
        for (uint8_t c = e.firstChild; c != NONE; c = _n[c].nextSibling) draw(ui, c, nx, ny, nw, nh);
        ui.setClipRect(cx, cy, cw, ch);
      }
      break;
    }
    case ElType::IMAGE: {
      if (hasBg && w > 0 && h > 0) ui.fillRect(x, y, w, h, bg);
      if (!imageDraw(ui, e.src, x, y, w, h, e.st.fit, millis()) && w > 8 && h > 8) {
        // Not available yet (URL still loading, or failed): a dim frame.
        ui.drawRect(x, y, w, h, COLOR_DIM);
      }
      break;
    }
    case ElType::TEXT: {
      const char* s = _txt[i];
      int16_t tw = textW(e.st, s);
      int16_t tx = x;
      if (w > tw) {
        if (e.st.align == ElAlign::CENTER) tx = x + (w - tw) / 2;
        else if (e.st.align == ElAlign::END) tx = x + w - tw;
      }
      if (hasBg && w > 0 && h > 0) ui.fillRect(x, y, w, h, bg);
      if (e.st.font[0]) fontDrawText(ui, e.st.font, s, e.st.size, tx, y, color);
      else drawBitmapText(ui, s, tx, y, e.st.size, color);
      break;
    }
    case ElType::LINE: {
      int16_t x1, y1;
      if (e.hasXY && e.x2 != EL_AUTO) {
        // Absolute line: x2/y2 are in the parent's content box like x/y.
        x1 = x + (e.x2 - e.x);
        y1 = y + (e.y2 == EL_AUTO ? 0 : e.y2 - e.y);
      } else {
        x1 = x + (w > 0 ? w - 1 : 0);
        y1 = y + (h > 0 ? h - 1 : 0);
        if (h <= e.st.thick) y1 = y;   // a flow "rule": horizontal
      }
      // Axis-aligned 1 px lines stay crisp; anything else is a smooth wide line.
      if (e.st.thick <= 1 && (x == x1 || y == y1)) ui.drawLine(x, y, x1, y1, color);
      else ui.drawWideLine(x, y, x1, y1, e.st.thick / 2.0f, color);
      break;
    }
    case ElType::BAR: {
      int v = atoi(_txt[i]);
      if (v < 0) v = 0;
      if (v > 100) v = 100;
      uint16_t track = hasBg ? bg : COLOR_DIM;
      if (w <= 0 || h <= 0) break;
      if (e.st.radius) smoothRoundRectOutline(ui, x, y, w, h, e.st.radius, 1, track);
      else             ui.drawRect(x, y, w, h, track);
      int inner = w > 2 ? w - 2 : 0;
      int fillW = inner * v / 100;
      if (fillW > 0 && h > 2) {
        int16_t fr = e.st.radius > 1 ? e.st.radius - 1 : 0;
        if (hasGrad)          fillGradient(ui, x + 1, y + 1, fillW, h - 2, fr, color, grad, e.st.gradientRight);
        else if (e.st.radius) ui.fillSmoothRoundRect(x + 1, y + 1, fillW, h - 2, fr, color);
        else                  ui.fillRect(x + 1, y + 1, fillW, h - 2, color);
      }
      break;
    }
    case ElType::CHART: {
      if (w <= 1 || h <= 1) break;
      if (hasBg) {
        if (e.st.radius) ui.fillSmoothRoundRect(x, y, w, h, e.st.radius, bg);
        else ui.fillRect(x, y, w, h, bg);
      }
      static float* sv = nullptr;
      static uint32_t* sa = nullptr;
      static int16_t* px = nullptr;
      static int16_t* py = nullptr;
      if (!sv) {
        sv = (float*)heap_caps_malloc(sizeof(float) * SERIES_MAX_SAMPLES, MALLOC_CAP_SPIRAM);
        sa = (uint32_t*)heap_caps_malloc(sizeof(uint32_t) * SERIES_MAX_SAMPLES, MALLOC_CAP_SPIRAM);
        px = (int16_t*)heap_caps_malloc(sizeof(int16_t) * SERIES_MAX_SAMPLES, MALLOC_CAP_SPIRAM);
        py = (int16_t*)heap_caps_malloc(sizeof(int16_t) * SERIES_MAX_SAMPLES, MALLOC_CAP_SPIRAM);
        if (!sv || !sa || !px || !py) { sv = nullptr; break; }
      }
      int idx = seriesFind(e.text);
      uint16_t n = idx >= 0 ? seriesSamples(idx, e.window, sv, sa, SERIES_MAX_SAMPLES) : 0;
      if (n == 0) {
        // Nothing sampled yet: a dim baseline shows where the chart is.
        ui.drawFastHLine(x, y + h - 1, w, COLOR_DIM);
        break;
      }
      // Time runs left to right across the window; without one the oldest
      // sample sits at the left edge.
      uint32_t span = e.window ? e.window : sa[0];
      if (span == 0) span = 1;
      float lo = e.st.hasMin ? e.st.vmin : sv[0], hi = e.st.hasMax ? e.st.vmax : sv[0];
      for (uint16_t i = 0; i < n; i++) {
        if (!e.st.hasMin && sv[i] < lo) lo = sv[i];
        if (!e.st.hasMax && sv[i] > hi) hi = sv[i];
      }
      if (!e.st.hasMin && !e.st.hasMax) {
        float pad = (hi - lo) * 0.05f;
        lo -= pad;
        hi += pad;
      }
      if (hi <= lo) { hi = lo + 1; lo -= 1; }
      int16_t bottom = y + h - 1;
      for (uint16_t i = 0; i < n; i++) {
        float tx = sa[i] > span ? 0 : (float)(span - sa[i]) / span;
        px[i] = x + (int16_t)lroundf(tx * (w - 1));
        float ty = (sv[i] - lo) / (hi - lo);
        if (ty < 0) ty = 0;
        if (ty > 1) ty = 1;
        py[i] = bottom - (int16_t)lroundf(ty * (h - 1));
      }
      float half = e.st.thick / 2.0f;
      switch (e.st.kind) {
        case ChartKind::AREA: {
          // The area is the line colour faded towards the background.
          uint16_t fillc = hasGrad ? grad : blend565(color, hasBg ? bg : COLOR_BG, 170);
          for (uint16_t i = 0; i + 1 < n; i++) {
            int16_t x0 = px[i], x1 = px[i + 1];
            if (x1 <= x0) { ui.drawFastVLine(x1, py[i + 1], bottom - py[i + 1] + 1, fillc); continue; }
            for (int16_t cx2 = x0; cx2 <= x1; cx2++) {
              int16_t yy = py[i] + (int32_t)(cx2 - x0) * (py[i + 1] - py[i]) / (x1 - x0);
              ui.drawFastVLine(cx2, yy, bottom - yy + 1, fillc);
            }
          }
          if (n == 1) ui.drawFastVLine(px[0], py[0], bottom - py[0] + 1, fillc);
        }
        // fall through: the line goes on top
        case ChartKind::LINE:
          if (n == 1) ui.fillSmoothCircle(px[0], py[0], half < 1 ? 1 : half, color);
          for (uint16_t i = 0; i + 1 < n; i++) {
            if (e.st.thick <= 1) ui.drawLine(px[i], py[i], px[i + 1], py[i + 1], color);
            else ui.drawWideLine(px[i], py[i], px[i + 1], py[i + 1], half, color);
          }
          break;
        case ChartKind::BARS: {
          // Bars stand on zero when it is in range, else on the bottom.
          int16_t base = bottom;
          if (lo < 0 && hi > 0) base = bottom - (int16_t)lroundf((0 - lo) / (hi - lo) * (h - 1));
          int16_t bwid = n > 0 ? (w / n) - 1 : 1;
          if (bwid < 1) bwid = 1;
          for (uint16_t i = 0; i < n; i++) {
            int16_t top = py[i] < base ? py[i] : base;
            int16_t bh = abs(py[i] - base) + 1;
            int16_t bx = px[i] - bwid / 2;
            if (bx < x) bx = x;
            if (bx + bwid > x + w) bwid = x + w - bx;
            if (hasGrad) fillGradient(ui, bx, top, bwid, bh, 0, color, grad, e.st.gradientRight);
            else ui.fillRect(bx, top, bwid, bh, color);
          }
          break;
        }
        case ChartKind::DOTS:
          for (uint16_t i = 0; i < n; i++) ui.fillSmoothCircle(px[i], py[i], half < 1 ? 1 : half, color);
          break;
      }
      break;
    }
    case ElType::CIRCLE:
    case ElType::ELLIPSE: {
      int16_t rx = w / 2, ry = h / 2;
      if (e.type == ElType::CIRCLE) { rx = ry = (w < h ? w : h) / 2; }
      int16_t mx = x + w / 2, my = y + h / 2;
      if (rx <= 0 || ry <= 0) break;
      bool fillIt = e.st.fill || hasBg;
      uint16_t fillColor = hasBg ? bg : color;
      uint16_t oc = bw > 0 ? border : color;
      uint8_t ow = bw > 0 ? bw : 1;
      bool hollow = bw > 0 || !fillIt;
      if (e.type == ElType::CIRCLE && fillIt && (!hollow || rx > ow)) {
        // Native smooth circle; a border is an outer disc with the inner disc refilled.
        if (hollow) { ui.fillSmoothCircle(mx, my, rx, oc); ui.fillSmoothCircle(mx, my, rx - ow, fillColor); }
        else ui.fillSmoothCircle(mx, my, rx, fillColor);
      } else {
        int16_t bx = mx - rx, by = my - ry, bwid = rx * 2 + 1, bhgt = ry * 2 + 1;
        lgfx::LGFX_Sprite* sp = ssBegin(bwid, bhgt);
        if (!sp) {
          if (fillIt) ui.fillEllipse(mx, my, rx, ry, fillColor);
          if (hollow) for (uint8_t k = 0; k < ow && k < rx && k < ry; k++) ui.drawEllipse(mx, my, rx - k, ry - k, oc);
          break;
        }
        int16_t cx2 = rx * 2 + 1, cy2 = ry * 2 + 1;
        if (fillIt && !hollow) sp->fillEllipse(cx2, cy2, rx * 2 + 1, ry * 2 + 1, fillColor);
        else {
          sp->fillEllipse(cx2, cy2, rx * 2 + 1, ry * 2 + 1, oc);
          if (rx > ow && ry > ow) sp->fillEllipse(cx2, cy2, (rx - ow) * 2 + 1, (ry - ow) * 2 + 1, fillIt ? fillColor : SS_KEY);
        }
        ssEnd(ui, bx, by, bwid, bhgt);
      }
      break;
    }
    case ElType::ARC: {
      int16_t r = (w < h ? w : h) / 2;
      int16_t mx = x + w / 2, my = y + h / 2;
      if (r <= 1) break;
      int16_t r0 = r - e.st.thick;
      if (r0 < 0) r0 = 0;
      int v = atoi(_txt[i]);
      if (v < 0) v = 0;
      if (v > 100) v = 100;
      // Our 0 degrees is the top, clockwise; the library's is 3 o'clock.
      float a0 = e.a0 - 90.0f, a1 = e.a1 - 90.0f;
      float av = a0 + (a1 - a0) * v / 100.0f;
      int16_t bx = mx - r, by = my - r, side = r * 2 + 1;
      lgfx::LGFX_Sprite* sp = ssBegin(side, side);
      if (!sp) {
        if (hasBg) ui.fillArc(mx, my, r0, r, a0, a1, bg);
        if (v > 0) ui.fillArc(mx, my, r0, r, a0, av, color);
        break;
      }
      int16_t c2 = r * 2 + 1;
      if (hasBg) sp->fillArc(c2, c2, r0 * 2, r * 2 + 1, a0, a1, bg);
      if (v > 0) sp->fillArc(c2, c2, r0 * 2, r * 2 + 1, a0, av, color);
      ssEnd(ui, bx, by, side, side);
      break;
    }
    case ElType::TRIANGLE:
    case ElType::POLYGON: {
      bool fillIt = e.st.fill || hasBg;
      uint16_t fillColor = hasBg ? bg : color;
      uint16_t oc = bw > 0 ? border : color;
      // Filled at 2x in the scratch sprite for smooth edges; outlines are wide lines.
      int16_t px[8], py[8];
      lgfx::LGFX_Sprite* sp = fillIt ? ssBegin(w, h) : nullptr;
      if (fillIt) {
        for (uint8_t k = 0; k < e.npts; k++) { px[k] = e.pts[k * 2] * 2; py[k] = e.pts[k * 2 + 1] * 2; }
        lgfx::LGFX_Sprite& t = sp ? *sp : ui;
        int16_t ox = sp ? 0 : x, oy = sp ? 0 : y;
        if (!sp) for (uint8_t k = 0; k < e.npts; k++) { px[k] = px[k] / 2 + ox; py[k] = py[k] / 2 + oy; }
        if (e.npts == 3) t.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], fillColor);
        else fillPolygon(t, px, py, e.npts, fillColor);
        if (sp) ssEnd(ui, x, y, w, h);
      }
      if (bw > 0 || !fillIt) {
        for (uint8_t k = 0; k < e.npts; k++) {
          uint8_t j = (k + 1) % e.npts;
          ui.drawWideLine(x + e.pts[k * 2], y + e.pts[k * 2 + 1], x + e.pts[j * 2], y + e.pts[j * 2 + 1], (bw > 1 ? bw : 1) / 2.0f, oc);
        }
      }
      break;
    }
  }
}

void LayoutWidget::render(lgfx::LGFX_Sprite& ui) {
  ui.fillScreen(COLOR_BG);
  ui.setTextDatum(top_left);
  ui.setTextSize(1);

  if (!_loaded || !_n) {
    ui.setTextColor(COLOR_BAD, COLOR_BG);
    ui.drawString("layout not loaded", 6, 12);
    return;
  }

  expandAll();
  measure(0);
  place(0, 0, 0, SCREEN_W, SCREEN_H);
  ui.setClipRect(0, 0, SCREEN_W, SCREEN_H);
  draw(ui, 0, 0, 0, SCREEN_W, SCREEN_H);
  ui.clearClipRect();

  ui.setTextDatum(top_left);
  ui.setTextSize(1);
}
