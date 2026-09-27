#include "LayoutWidget.h"

#include <esp_heap_caps.h>
#include "LayoutData.h"
#include "FontService.h"
#include "../net/PingService.h"
#include "../net/DataSource.h"

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
  return false;
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
}

bool LayoutWidget::alloc() {
  if (_n && _txt) return true;
  size_t nBytes = sizeof(LayoutNode) * MAX_NODES;
  size_t tBytes = TEXT_BUF * MAX_NODES;
  if (!_n)   _n   = (LayoutNode*)heap_caps_malloc(nBytes, MALLOC_CAP_SPIRAM);
  if (!_n)   _n   = (LayoutNode*)malloc(nBytes);
  if (!_txt) _txt = (char (*)[TEXT_BUF])heap_caps_malloc(tBytes, MALLOC_CAP_SPIRAM);
  if (!_txt) _txt = (char (*)[TEXT_BUF])malloc(tBytes);
  return _n && _txt;
}

void LayoutWidget::clear() {
  _loaded = false;
  _usesPing = false;
  _usesApi = false;
  _count = 0;
  _name[0] = '\0';
}

void LayoutWidget::setId(const char* id) {
  strlcpy(_id, id ? id : "", sizeof(_id));
}

// =====================
// PARSING
// =====================
static bool fail(char* err, size_t n, const char* path, const char* msg) {
  if (path && *path) snprintf(err, n, "element %s: %s", path, msg);
  else strlcpy(err, msg, n);
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

bool LayoutWidget::parseStyle(LayoutStyle& st, JsonObjectConst obj, const char* path, char* err, size_t errLen) {
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
      st.borderW = clampU8(v | 1, 0, 20);
    } else if (!strcmp(k, "radius")) {
      st.radius = clampU8(v | 0, 0, 80);
    } else if (!strcmp(k, "padding")) {
      st.pad = clampU8(v | 0, 0, 80);
    } else if (!strcmp(k, "gap")) {
      st.gap = clampU8(v | 0, 0, 200);
    } else if (!strcmp(k, "thickness") || !strcmp(k, "width")) {
      st.thick = clampU8(v | 1, 1, 80);
    } else if (!strcmp(k, "size")) {
      int size = v | 1;
      if (size < 1 || size > FONT_MAX_PX) return fail(err, errLen, path, "size must be 1-8 (bitmap font) or 6-160 (TrueType font)");
      st.size = size;
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
    else return fail(err, errLen, path, "unknown type (text, line, rect, bar, box, circle, ellipse, arc, triangle, polygon)");

    // ---- style: defaults, class, flat fields, style object ----
    defaultStyle(e.st);
    bool shape = e.type == ElType::CIRCLE || e.type == ElType::ELLIPSE || e.type == ElType::TRIANGLE || e.type == ElType::POLYGON;
    if (shape) e.st.fill = true;
    if (e.type == ElType::ARC) { e.st.thick = 8; strlcpy(e.st.bg, "dim", sizeof(e.st.bg)); }
    if (e.type == ElType::LINE) e.st.thick = 1;

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
      int size = v["size"] | 1;
      if (size < 1 || size > FONT_MAX_PX) return fail(err, errLen, path, "size must be 1-8 (bitmap font) or 6-160 (TrueType font)");
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
      if (!parseStyle(e.st, v["style"].as<JsonObjectConst>(), path, err, errLen)) return false;
    }
    if (e.type == ElType::TEXT) {
      if (!e.st.font[0] && e.st.size > 8) return fail(err, errLen, path, "size must be 1-8 with the bitmap font; set \"font\" for pixel sizes");
      if (e.st.font[0] && e.st.size < FONT_MIN_PX) return fail(err, errLen, path, "size must be at least 6 with a TrueType font");
    }

    // ---- geometry ----
    if (!v["x"].isNull()) e.x = v["x"] | 0;
    if (!v["y"].isNull()) e.y = v["y"] | 0;
    if (!v["w"].isNull()) e.w = v["w"] | 0;
    if (!v["h"].isNull()) e.h = v["h"] | 0;
    if (!v["r"].isNull()) { int r = v["r"] | 0; e.w = e.h = r * 2; }
    if (!v["x2"].isNull()) e.x2 = v["x2"] | 0;
    if (!v["y2"].isNull()) e.y2 = v["y2"] | 0;
    e.hasXY = (e.x != EL_AUTO && e.y != EL_AUTO) || e.st.absolute;
    if (e.hasXY) {
      if (e.x == EL_AUTO) e.x = 0;
      if (e.y == EL_AUTO) e.y = 0;
    }

    if (e.type == ElType::ARC) {
      e.a0 = v["start"] | 0;
      e.a1 = v["end"] | 360;
    }

    if (e.type == ElType::TRIANGLE || e.type == ElType::POLYGON) {
      JsonArrayConst pts = v["points"].as<JsonArrayConst>();
      if (pts.isNull()) return fail(err, errLen, path, "\"points\" must be an array of [x,y] pairs");
      uint8_t n = 0;
      for (JsonVariantConst p : pts) {
        if (n >= 8) return fail(err, errLen, path, "too many points (max 8)");
        if (!p.is<JsonArrayConst>() || p.size() != 2) return fail(err, errLen, path, "each point must be [x,y]");
        e.pts[n * 2]     = p[0] | 0;
        e.pts[n * 2 + 1] = p[1] | 0;
        n++;
      }
      if (e.type == ElType::TRIANGLE && n != 3) return fail(err, errLen, path, "triangle needs exactly 3 points");
      if (n < 3) return fail(err, errLen, path, "polygon needs at least 3 points");
      e.npts = n;
    }

    // ---- text / value ----
    const char* text = "";
    if (e.type == ElType::BAR || e.type == ElType::ARC) text = v["value"] | "0";
    else if (e.type == ElType::TEXT) text = v["text"] | "";
    if (strlen(text) >= sizeof(e.text)) {
      char m[64];
      snprintf(m, sizeof(m), "text longer than %u characters", (unsigned)(sizeof(e.text) - 1));
      return fail(err, errLen, path, m);
    }
    strlcpy(e.text, text, sizeof(e.text));

    // Keys may sit inside an expression, so look for the prefix anywhere.
    const char* scan[4] = { e.text, e.st.color, e.st.bg, e.st.border };
    for (const char* s : scan) {
      if (strstr(s, "ping.")) _usesPing = true;
      if (strstr(s, "api."))  _usesApi = true;
    }
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
      if (!parseStyle(ns.st, kv.value().as<JsonObjectConst>(), spath, err, errLen)) { free(styles); return false; }
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
    if (!parseStyle(root.st, doc["style"].as<JsonObjectConst>(), "root", err, errLen)) { free(styles); return false; }
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

  _loaded = true;
  return true;
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

  for (uint8_t i = 1; i < _count; i++) {
    const char* fields[4] = { _n[i].text, _n[i].st.color, _n[i].st.bg, _n[i].st.border };
    for (const char* f : fields) {
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
    }
  }
}

void LayoutWidget::update(uint32_t now) {
  if (_usesPing) pingLoop(now);
  if (_usesApi) touchSources();
}

uint16_t LayoutWidget::resolveColor(const char* spec, uint16_t fallback, bool* present) {
  if (present) *present = spec[0] != '\0';
  if (!spec[0]) return fallback;
  char buf[24];
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
void LayoutWidget::expandAll() {
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
static void drawBitmapText(lgfx::LGFX_Sprite& ui, const char* s, int16_t x, int16_t y, uint8_t size, uint16_t color, uint16_t bg) {
  ui.setTextSize(size);
  ui.setTextColor(color, bg);
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
    case ElType::RECT:
    case ElType::ELLIPSE:
      break;
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
             (k.type == ElType::TEXT || k.type == ElType::BAR || k.type == ElType::BOX || k.type == ElType::LINE)) {
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

  bool hasBg, hasBorder;
  uint16_t color  = resolveColor(e.st.color, COLOR_TEXT);
  uint16_t bg     = resolveColor(e.st.bg, COLOR_BG, &hasBg);
  uint16_t border = resolveColor(e.st.border, COLOR_DIM, &hasBorder);
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
      if (fillIt && w > 0 && h > 0) {
        if (e.st.radius) ui.fillRoundRect(x, y, w, h, e.st.radius, fillColor);
        else             ui.fillRect(x, y, w, h, fillColor);
      }
      if (outline) {
        for (uint8_t k = 0; k < ow && k * 2 < w && k * 2 < h; k++) {
          int16_t r = e.st.radius > k ? e.st.radius - k : 0;
          if (r) ui.drawRoundRect(x + k, y + k, w - 2 * k, h - 2 * k, r, outlineColor);
          else   ui.drawRect(x + k, y + k, w - 2 * k, h - 2 * k, outlineColor);
        }
      }
      if (e.type == ElType::BOX) {
        // Children are clipped to the box.
        int16_t nx = x, ny = y, nw = w, nh = h;
        clipIntersect(nx, ny, nw, nh, cx, cy, cw, ch);
        ui.setClipRect(nx, ny, nw, nh);
        for (uint8_t c = e.firstChild; c != NONE; c = _n[c].nextSibling) draw(ui, c, nx, ny, nw, nh);
        ui.setClipRect(cx, cy, cw, ch);
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
      else drawBitmapText(ui, s, tx, y, e.st.size, color, hasBg ? bg : COLOR_BG);
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
      if (e.st.thick > 1) ui.drawWideLine(x, y, x1, y1, e.st.thick / 2.0f, color);
      else ui.drawLine(x, y, x1, y1, color);
      break;
    }
    case ElType::BAR: {
      int v = atoi(_txt[i]);
      if (v < 0) v = 0;
      if (v > 100) v = 100;
      uint16_t track = hasBg ? bg : COLOR_DIM;
      if (w <= 0 || h <= 0) break;
      if (e.st.radius) ui.drawRoundRect(x, y, w, h, e.st.radius, track);
      else             ui.drawRect(x, y, w, h, track);
      int inner = w > 2 ? w - 2 : 0;
      int fillW = inner * v / 100;
      if (fillW > 0 && h > 2) {
        if (e.st.radius) ui.fillRoundRect(x + 1, y + 1, fillW, h - 2, e.st.radius > 1 ? e.st.radius - 1 : 0, color);
        else             ui.fillRect(x + 1, y + 1, fillW, h - 2, color);
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
      if (fillIt) ui.fillEllipse(mx, my, rx, ry, fillColor);
      if (bw > 0 || !fillIt) {
        uint16_t oc = bw > 0 ? border : color;
        uint8_t ow = bw > 0 ? bw : 1;
        for (uint8_t k = 0; k < ow && k < rx && k < ry; k++) ui.drawEllipse(mx, my, rx - k, ry - k, oc);
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
      if (hasBg) ui.fillArc(mx, my, r0, r, a0, a1, bg);
      float av = a0 + (a1 - a0) * v / 100.0f;
      if (v > 0) ui.fillArc(mx, my, r0, r, a0, av, color);
      break;
    }
    case ElType::TRIANGLE:
    case ElType::POLYGON: {
      int16_t px[8], py[8];
      for (uint8_t k = 0; k < e.npts; k++) { px[k] = x + e.pts[k * 2]; py[k] = y + e.pts[k * 2 + 1]; }
      bool fillIt = e.st.fill || hasBg;
      uint16_t fillColor = hasBg ? bg : color;
      if (fillIt) {
        if (e.npts == 3) ui.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], fillColor);
        else fillPolygon(ui, px, py, e.npts, fillColor);
      }
      if (bw > 0 || !fillIt) {
        uint16_t oc = bw > 0 ? border : color;
        for (uint8_t k = 0; k < e.npts; k++) {
          uint8_t j = (k + 1) % e.npts;
          if (bw > 1) ui.drawWideLine(px[k], py[k], px[j], py[j], bw / 2.0f, oc);
          else ui.drawLine(px[k], py[k], px[j], py[j], oc);
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
