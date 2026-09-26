#include "LayoutWidget.h"

#include "LayoutData.h"
#include "../net/PingService.h"

static const uint16_t COLOR_BG     = 0x0000;
static const uint16_t COLOR_TEXT   = 0xFFFF;
static const uint16_t COLOR_DIM    = 0x39E7;
static const uint16_t COLOR_OK     = 0x07E0;
static const uint16_t COLOR_WARN   = 0xFD20;
static const uint16_t COLOR_BAD    = 0xF800;
static const uint16_t COLOR_ACCENT = 0x04FF;

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

// =====================
// PARSING
// =====================
void LayoutWidget::clear() {
  _loaded = false;
  _usesPing = false;
  _count = 0;
  _name[0] = '\0';
}

void LayoutWidget::setId(const char* id) {
  strlcpy(_id, id ? id : "", sizeof(_id));
}

static bool fail(char* err, size_t n, const char* msg, int idx = -1) {
  if (idx >= 0) snprintf(err, n, "element %d: %s", idx, msg);
  else strlcpy(err, msg, n);
  return false;
}

bool LayoutWidget::load(JsonVariantConst doc, char* err, size_t errLen) {
  clear();
  if (errLen) err[0] = '\0';

  if (!doc.is<JsonObjectConst>()) return fail(err, errLen, "layout must be a JSON object");

  const char* name = doc["name"] | "";
  strlcpy(_name, *name ? name : (_id[0] ? _id : "Layout"), sizeof(_name));

  JsonArrayConst elements = doc["elements"].as<JsonArrayConst>();
  if (elements.isNull()) return fail(err, errLen, "\"elements\" must be an array");
  if (elements.size() > MAX_ELEMENTS) {
    snprintf(err, errLen, "too many elements (max %u)", MAX_ELEMENTS);
    return false;
  }

  int idx = 0;
  for (JsonVariantConst v : elements) {
    if (!v.is<JsonObjectConst>()) return fail(err, errLen, "must be an object", idx);
    LayoutElement& e = _el[_count];
    memset(&e, 0, sizeof(e));

    const char* type = v["type"] | "";
    if      (!strcmp(type, "text")) e.type = ElType::TEXT;
    else if (!strcmp(type, "line")) e.type = ElType::LINE;
    else if (!strcmp(type, "rect")) e.type = ElType::RECT;
    else if (!strcmp(type, "bar"))  e.type = ElType::BAR;
    else return fail(err, errLen, "unknown type (use text, line, rect or bar)", idx);

    e.x  = v["x"]  | 0;
    e.y  = v["y"]  | 0;
    e.x2 = v["x2"] | e.x;
    e.y2 = v["y2"] | e.y;
    e.w  = v["w"]  | 0;
    e.h  = v["h"]  | 0;
    e.fill = v["fill"] | false;

    int size = v["size"] | 1;
    if (size < 1 || size > 8) return fail(err, errLen, "size must be 1-8", idx);
    e.size = size;

    const char* align = v["align"] | "left";
    if      (!strcmp(align, "left"))   e.align = ElAlign::LEFT;
    else if (!strcmp(align, "center")) e.align = ElAlign::CENTER;
    else if (!strcmp(align, "right"))  e.align = ElAlign::RIGHT;
    else return fail(err, errLen, "align must be left, center or right", idx);

    const char* color = v["color"] | "text";
    if (strlen(color) >= sizeof(e.color)) return fail(err, errLen, "color too long", idx);
    strlcpy(e.color, color, sizeof(e.color));
    if (!strchr(color, '{')) {
      uint16_t tmp;
      if (!roleColor(color, tmp)) return fail(err, errLen, "unknown color (use a role name or #rrggbb)", idx);
    }

    const char* text = (e.type == ElType::BAR) ? (v["value"] | "0") : (v["text"] | "");
    if (strlen(text) >= sizeof(e.text)) {
      snprintf(err, errLen, "element %d: text longer than %u characters", idx, (unsigned)(sizeof(e.text) - 1));
      return false;
    }
    strlcpy(e.text, text, sizeof(e.text));

    if (strstr(e.text, "{ping.") || strstr(e.color, "{ping.")) _usesPing = true;

    _count++;
    idx++;
  }

  _loaded = true;
  return true;
}

// =====================
// RUNTIME
// =====================
void LayoutWidget::update(uint32_t now) {
  if (_usesPing) pingLoop(now);
}

uint16_t LayoutWidget::resolveColor(const char* spec, uint16_t fallback) {
  char buf[24];
  const char* name = spec;
  if (strchr(spec, '{')) {
    layoutExpand(spec, buf, sizeof(buf));
    name = buf;
  }
  uint16_t c;
  return roleColor(name, c) ? c : fallback;
}

void LayoutWidget::render(lgfx::LGFX_Sprite& ui) {
  ui.fillScreen(COLOR_BG);
  ui.setTextDatum(top_left);
  ui.setTextSize(1);

  if (!_loaded) {
    ui.setTextColor(COLOR_BAD, COLOR_BG);
    ui.drawString("layout not loaded", 6, 12);
    return;
  }

  char buf[96];
  for (uint8_t i = 0; i < _count; i++) {
    const LayoutElement& e = _el[i];
    uint16_t color = resolveColor(e.color, COLOR_TEXT);

    switch (e.type) {
      case ElType::TEXT: {
        layoutExpand(e.text, buf, sizeof(buf));
        ui.setTextSize(e.size);
        ui.setTextColor(color, COLOR_BG);
        ui.setTextDatum(e.align == ElAlign::CENTER ? top_center :
                        e.align == ElAlign::RIGHT  ? top_right : top_left);
        ui.drawString(buf, e.x, e.y);
        break;
      }
      case ElType::LINE:
        ui.drawLine(e.x, e.y, e.x2, e.y2, color);
        break;
      case ElType::RECT:
        if (e.fill) ui.fillRect(e.x, e.y, e.w, e.h, color);
        else        ui.drawRect(e.x, e.y, e.w, e.h, color);
        break;
      case ElType::BAR: {
        layoutExpand(e.text, buf, sizeof(buf));
        int v = atoi(buf);
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        ui.drawRect(e.x, e.y, e.w, e.h, COLOR_DIM);
        int inner = e.w > 2 ? e.w - 2 : 0;
        int fillW = inner * v / 100;
        if (fillW > 0 && e.h > 2) ui.fillRect(e.x + 1, e.y + 1, fillW, e.h - 2, color);
        break;
      }
    }
  }

  ui.setTextDatum(top_left);
  ui.setTextSize(1);
}
