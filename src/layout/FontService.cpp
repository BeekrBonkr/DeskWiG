#include "FontService.h"

#include <LittleFS.h>
#include <esp_heap_caps.h>

static void* psAlloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
  return p ? p : malloc(n);
}

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u) ((void)(u), psAlloc(x))
#define STBTT_free(x, u)   ((void)(u), free(x))
#define STBTT_assert(x)    ((void)0)
#include "stb_truetype.h"

// Built into the firmware (platformio.ini board_build.embed_files).
extern const uint8_t sans_ttf_start[]  asm("_binary_fonts_sans_ttf_start");
extern const uint8_t sans_ttf_end[]    asm("_binary_fonts_sans_ttf_end");
extern const uint8_t bold_ttf_start[]  asm("_binary_fonts_bold_ttf_start");
extern const uint8_t bold_ttf_end[]    asm("_binary_fonts_bold_ttf_end");
extern const uint8_t emoji_ttf_start[] asm("_binary_fonts_emoji_ttf_start");
extern const uint8_t emoji_ttf_end[]   asm("_binary_fonts_emoji_ttf_end");

static const char* FONT_DIR = "/fonts";

struct Font {
  char name[FONT_NAME_LEN + 1];
  const uint8_t* data;     // flash (built in) or PSRAM (uploaded, once loaded)
  size_t size;
  bool builtin;
  bool loaded;             // stbtt_fontinfo initialised
  bool broken;             // failed to load; don't retry every frame
  stbtt_fontinfo info;
  int ascent, descent, lineGap;
};

static Font fontTable[MAX_FONTS];
static uint8_t fontCount = 0;
static size_t loadedBytes = 0;
static int idxSans = -1, idxEmoji = -1;
static SemaphoreHandle_t lock = nullptr;

static void take() { if (lock) xSemaphoreTake(lock, portMAX_DELAY); }
static void give() { if (lock) xSemaphoreGive(lock); }

// =====================
// GLYPH CACHE
// =====================
struct Glyph {
  bool used;
  uint8_t font;
  uint16_t px;
  uint32_t cp;
  int16_t w, h, xoff, yoff, adv;
  uint8_t* alpha;          // w*h bytes, PSRAM; may be null for blank glyphs
};

static const uint16_t CACHE_SIZE = 512;   // power of two
static Glyph* cache = nullptr;
static uint8_t* scratch = nullptr;        // ARGB conversion buffer
static size_t scratchSize = 0;

static void cacheFlush() {
  if (!cache) return;
  for (uint16_t i = 0; i < CACHE_SIZE; i++) {
    if (cache[i].used && cache[i].alpha) free(cache[i].alpha);
    cache[i].used = false;
    cache[i].alpha = nullptr;
  }
}

static uint32_t hashKey(uint8_t font, uint16_t px, uint32_t cp) {
  return (cp * 2654435761u) ^ ((uint32_t)px << 7) ^ font;
}

// =====================
// FONT TABLE
// =====================
bool fontValidName(const char* name) {
  if (!name) return false;
  size_t len = strlen(name);
  if (len == 0 || len > FONT_NAME_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return name[0] != '-' && name[len - 1] != '-';
}

String fontPath(const char* name) {
  return String(FONT_DIR) + "/" + name + ".ttf";
}

int fontFind(const char* name) {
  if (!name || !*name) return -1;
  for (uint8_t i = 0; i < fontCount; i++) if (!strcmp(fontTable[i].name, name)) return i;
  return -1;
}

bool fontExists(const char* name) {
  return fontFind(name) >= 0;
}

static bool initFont(Font& f) {
  int off = stbtt_GetFontOffsetForIndex(f.data, 0);
  if (off < 0 || !stbtt_InitFont(&f.info, f.data, off)) {
    f.broken = true;
    Serial.printf("[FONT] %s: not a usable TrueType file\n", f.name);
    return false;
  }
  stbtt_GetFontVMetrics(&f.info, &f.ascent, &f.descent, &f.lineGap);
  f.loaded = true;
  return true;
}

// Uploaded fonts are read into PSRAM the first time they are used.
static bool ensureLoaded(Font& f) {
  if (f.loaded) return true;
  if (f.broken) return false;
  if (f.builtin) return initFont(f);

  File file = LittleFS.open(fontPath(f.name), "r");
  if (!file) { f.broken = true; return false; }
  size_t size = file.size();
  if (size == 0 || size > FONT_MAX_FILE || loadedBytes + size > FONT_MAX_LOADED) {
    file.close();
    f.broken = true;
    Serial.printf("[FONT] %s: too large to load (%u bytes)\n", f.name, (unsigned)size);
    return false;
  }
  uint8_t* buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!buf) { file.close(); f.broken = true; return false; }
  size_t got = file.read(buf, size);
  file.close();
  if (got != size) { free(buf); f.broken = true; return false; }
  f.data = buf;
  f.size = size;
  loadedBytes += size;
  if (!initFont(f)) return false;
  Serial.printf("[FONT] Loaded %s (%u bytes)\n", f.name, (unsigned)size);
  return true;
}

static void addBuiltin(const char* name, const uint8_t* start, const uint8_t* end) {
  Font& f = fontTable[fontCount++];
  memset(&f, 0, sizeof(f));
  strlcpy(f.name, name, sizeof(f.name));
  f.data = start;
  f.size = end - start;
  f.builtin = true;
}

static void scanLocked() {
  // Drop uploaded entries, keep built-ins.
  for (uint8_t i = 0; i < fontCount; i++) {
    if (!fontTable[i].builtin && fontTable[i].loaded) { free((void*)fontTable[i].data); }
  }
  loadedBytes = 0;
  uint8_t n = 0;
  for (uint8_t i = 0; i < fontCount; i++) if (fontTable[i].builtin) fontTable[n++] = fontTable[i];
  fontCount = n;
  cacheFlush();

  if (!LittleFS.exists(FONT_DIR)) LittleFS.mkdir(FONT_DIR);
  File dir = LittleFS.open(FONT_DIR);
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) { f.close(); continue; }
    String path = f.path();
    size_t size = f.size();
    f.close();
    int slash = path.lastIndexOf('/');
    String base = path.substring(slash + 1);
    if (!base.endsWith(".ttf")) continue;
    String name = base.substring(0, base.length() - 4);
    if (!fontValidName(name.c_str()) || fontFind(name.c_str()) >= 0) continue;
    if (fontCount >= MAX_FONTS) break;
    Font& e = fontTable[fontCount++];
    memset(&e, 0, sizeof(e));
    strlcpy(e.name, name.c_str(), sizeof(e.name));
    e.size = size;
  }
  dir.close();
}

void fontsBegin() {
  if (!lock) lock = xSemaphoreCreateMutex();
  if (!cache) {
    cache = (Glyph*)psAlloc(sizeof(Glyph) * CACHE_SIZE);
    if (cache) memset(cache, 0, sizeof(Glyph) * CACHE_SIZE);
  }
  take();
  fontCount = 0;
  addBuiltin("sans",  sans_ttf_start,  sans_ttf_end);
  addBuiltin("bold",  bold_ttf_start,  bold_ttf_end);
  addBuiltin("emoji", emoji_ttf_start, emoji_ttf_end);
  idxSans = 0;
  idxEmoji = 2;
  scanLocked();
  give();
  Serial.printf("[FONT] %u font(s)\n", fontCount);
}

void fontsRescan() {
  take();
  scanLocked();
  give();
}

bool fontValidate(const uint8_t* data, size_t len) {
  if (!data || len < 12) return false;
  int off = stbtt_GetFontOffsetForIndex(data, 0);
  if (off < 0 || (size_t)off >= len) return false;
  stbtt_fontinfo info;
  if (!stbtt_InitFont(&info, data, off)) return false;
  int a, d, g;
  stbtt_GetFontVMetrics(&info, &a, &d, &g);
  return a != d;
}

bool fontValidateFile(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t size = f.size();
  if (size < 12 || size > FONT_MAX_FILE) { f.close(); return false; }
  uint8_t* buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!buf) { f.close(); return false; }
  size_t got = f.read(buf, size);
  f.close();
  bool ok = got == size && fontValidate(buf, size);
  free(buf);
  return ok;
}

bool fontDelete(const char* name, char* err, size_t errLen) {
  int i = fontFind(name);
  if (i < 0) { strlcpy(err, "no such font", errLen); return false; }
  if (fontTable[i].builtin) { strlcpy(err, "built-in fonts cannot be deleted", errLen); return false; }
  take();
  LittleFS.remove(fontPath(name));
  scanLocked();
  give();
  return true;
}

const uint8_t* fontBuiltinData(const char* name, size_t* len) {
  int i = fontFind(name);
  if (i < 0 || !fontTable[i].builtin) return nullptr;
  if (len) *len = fontTable[i].size;
  return fontTable[i].data;
}

void fontsToJson(JsonArray arr) {
  for (uint8_t i = 0; i < fontCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = fontTable[i].name;
    o["size"] = fontTable[i].size;
    o["builtin"] = fontTable[i].builtin;
  }
}

// =====================
// TEXT
// =====================
uint32_t utf8Next(const char** s) {
  const uint8_t* p = (const uint8_t*)*s;
  if (!*p) return 0;
  uint32_t cp;
  int extra;
  if (*p < 0x80)        { cp = *p; extra = 0; }
  else if (*p < 0xC0)   { cp = '?'; extra = 0; }
  else if (*p < 0xE0)   { cp = *p & 0x1F; extra = 1; }
  else if (*p < 0xF0)   { cp = *p & 0x0F; extra = 2; }
  else                  { cp = *p & 0x07; extra = 3; }
  p++;
  for (int i = 0; i < extra; i++) {
    if ((*p & 0xC0) != 0x80) { cp = '?'; break; }
    cp = (cp << 6) | (*p & 0x3F);
    p++;
  }
  *s = (const char*)p;
  return cp;
}

static bool invisible(uint32_t cp) {
  return cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D || cp == 0x200B;
}

// Finds or rasterises a glyph. Returns nullptr if the font lacks it.
static Glyph* getGlyph(uint8_t fi, uint16_t px, uint32_t cp) {
  Font& f = fontTable[fi];
  if (!ensureLoaded(f)) return nullptr;
  if (!cache) return nullptr;

  uint32_t h = hashKey(fi, px, cp);
  uint16_t slot = h & (CACHE_SIZE - 1);
  int16_t empty = -1;
  for (uint16_t probe = 0; probe < 32; probe++) {
    Glyph& g = cache[(slot + probe) & (CACHE_SIZE - 1)];
    if (!g.used) { if (empty < 0) empty = (slot + probe) & (CACHE_SIZE - 1); break; }
    if (g.font == fi && g.px == px && g.cp == cp) return &g;
  }
  if (empty < 0) { cacheFlush(); empty = slot; }

  int gi = stbtt_FindGlyphIndex(&f.info, cp);
  if (gi == 0) return nullptr;

  float scale = stbtt_ScaleForPixelHeight(&f.info, px);
  int adv, lsb;
  stbtt_GetGlyphHMetrics(&f.info, gi, &adv, &lsb);
  int w = 0, hh = 0, xoff = 0, yoff = 0;
  uint8_t* bmp = stbtt_GetGlyphBitmap(&f.info, scale, scale, gi, &w, &hh, &xoff, &yoff);

  Glyph& g = cache[empty];
  g.used = true;
  g.font = fi;
  g.px = px;
  g.cp = cp;
  g.w = w;
  g.h = hh;
  g.xoff = xoff;
  g.yoff = yoff;
  g.adv = (int16_t)lroundf(adv * scale);
  g.alpha = (w > 0 && hh > 0) ? bmp : nullptr;
  if (!g.alpha && bmp) STBTT_free(bmp, nullptr);
  return &g;
}

// Glyph from the font, then emoji, then sans.
static Glyph* resolveGlyph(int fi, uint16_t px, uint32_t cp) {
  Glyph* g = nullptr;
  if (fi >= 0) g = getGlyph(fi, px, cp);
  if (!g && idxEmoji >= 0 && idxEmoji != fi) g = getGlyph(idxEmoji, px, cp);
  if (!g && idxSans >= 0 && idxSans != fi) g = getGlyph(idxSans, px, cp);
  if (!g && cp != '?') g = resolveGlyph(fi, px, '?');
  return g;
}

static void blitGlyph(lgfx::LGFX_Sprite& ui, const Glyph& g, int16_t x, int16_t y, uint16_t color565) {
  if (!g.alpha || g.w <= 0 || g.h <= 0) return;
  size_t need = (size_t)g.w * g.h * 4;
  if (need > scratchSize) {
    free(scratch);
    scratch = (uint8_t*)psAlloc(need);
    scratchSize = scratch ? need : 0;
    if (!scratch) return;
  }
  uint8_t r = ((color565 >> 11) & 0x1F) * 255 / 31;
  uint8_t gg = ((color565 >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color565 & 0x1F) * 255 / 31;
  lgfx::argb8888_t* px = (lgfx::argb8888_t*)scratch;
  size_t n = (size_t)g.w * g.h;
  for (size_t i = 0; i < n; i++) px[i] = lgfx::argb8888_t(g.alpha[i], r, gg, b);
  ui.pushAlphaImage(x, y, g.w, g.h, px);
}

static int16_t ascentPx(int fi, uint16_t px) {
  if (fi < 0) return px;
  Font& f = fontTable[fi];
  if (!f.loaded) return px;
  float scale = stbtt_ScaleForPixelHeight(&f.info, px);
  return (int16_t)lroundf(f.ascent * scale);
}

static int16_t layoutText(lgfx::LGFX_Sprite* ui, int fi, const char* utf8, uint16_t px,
                          int16_t x, int16_t y, uint16_t color565) {
  if (px < FONT_MIN_PX) px = FONT_MIN_PX;
  if (px > FONT_MAX_PX) px = FONT_MAX_PX;
  if (fi >= 0 && !ensureLoaded(fontTable[fi])) fi = idxSans;
  if (fi >= 0 && !ensureLoaded(fontTable[fi])) return 0;
  int16_t baseline = y + ascentPx(fi, px);
  int16_t pen = 0;
  const char* s = utf8;
  for (uint32_t cp = utf8Next(&s); cp; cp = utf8Next(&s)) {
    if (invisible(cp)) continue;
    Glyph* g = resolveGlyph(fi, px, cp);
    if (!g) continue;
    if (ui) blitGlyph(*ui, *g, x + pen + g->xoff, baseline + g->yoff, color565);
    pen += g->adv;
  }
  return pen;
}

int16_t fontTextWidth(const char* font, const char* utf8, uint16_t px) {
  int fi = fontFind(font);
  if (fi < 0) fi = idxSans;
  take();
  int16_t w = layoutText(nullptr, fi, utf8, px, 0, 0, 0);
  give();
  return w;
}

void fontDrawText(lgfx::LGFX_Sprite& ui, const char* font, const char* utf8, uint16_t px,
                  int16_t x, int16_t y, uint16_t color565) {
  int fi = fontFind(font);
  if (fi < 0) fi = idxSans;
  take();
  layoutText(&ui, fi, utf8, px, x, y, color565);
  give();
}

int16_t fontDrawFallbackGlyph(lgfx::LGFX_Sprite* ui, uint32_t cp, uint16_t px,
                              int16_t x, int16_t y, uint16_t color565) {
  if (invisible(cp)) return 0;
  if (px < FONT_MIN_PX) px = FONT_MIN_PX;
  if (px > FONT_MAX_PX) px = FONT_MAX_PX;
  take();
  Glyph* g = resolveGlyph(idxEmoji, px, cp);
  int16_t adv = 0;
  if (g) {
    if (ui) {
      int fi = g->font;
      blitGlyph(*ui, *g, x + g->xoff, y + ascentPx(fi, px) + g->yoff, color565);
    }
    adv = g->adv;
  }
  give();
  return adv;
}
