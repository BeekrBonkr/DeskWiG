#include "ImageService.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <AnimatedGIF.h>
#include <esp_heap_caps.h>

#include "../net/WifiManager.h"
#include "../app/Log.h"

static const char* IMG_DIR = "/img";
static const uint16_t TRANSP = 0x0821;          // sprite colour treated as "nothing drawn"
static const uint32_t CONNECT_TIMEOUT_MS = 5000;
static const uint32_t READ_TIMEOUT_MS = 10000;
static const uint32_t RETRY_AFTER_ERROR_S = 60;

static void* psAlloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
  return p ? p : malloc(n);
}

static SemaphoreHandle_t lock = nullptr;
static void take() { if (lock) xSemaphoreTake(lock, portMAX_DELAY); }
static void give() { if (lock) xSemaphoreGive(lock); }

// =====================
// FILE TABLE
// =====================
struct ImageFile {
  char name[IMAGE_NAME_LEN + 1];
  char ext[5];
  size_t size;
};
static ImageFile files[MAX_IMAGE_FILES];
static uint8_t fileCount = 0;

bool imageValidName(const char* name) {
  if (!name) return false;
  size_t len = strlen(name);
  if (len == 0 || len > IMAGE_NAME_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return name[0] != '-' && name[len - 1] != '-';
}

static int fileFind(const char* name) {
  for (uint8_t i = 0; i < fileCount; i++) if (!strcmp(files[i].name, name)) return i;
  return -1;
}

const char* imageExt(const char* name) {
  int i = fileFind(name);
  return i < 0 ? "" : files[i].ext;
}

String imagePath(const char* name) {
  int i = fileFind(name);
  if (i < 0) return "";
  return String(IMG_DIR) + "/" + name + "." + files[i].ext;
}

static bool isUrl(const char* src) {
  return !strncmp(src, "http://", 7) || !strncmp(src, "https://", 8);
}

bool imageSourceOk(const char* src) {
  if (!src || !*src) return false;
  if (isUrl(src)) return strlen(src) <= IMAGE_SRC_LEN;
  return fileFind(src) >= 0;
}

ImgType imageSniff(const uint8_t* d, size_t len) {
  if (len >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return ImgType::PNG;
  if (len >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return ImgType::JPG;
  if (len >= 6 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F' && d[3] == '8') return ImgType::GIF;
  return ImgType::UNKNOWN;
}

static void scanLocked() {
  fileCount = 0;
  if (!LittleFS.exists(IMG_DIR)) LittleFS.mkdir(IMG_DIR);
  File dir = LittleFS.open(IMG_DIR);
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) { f.close(); continue; }
    String path = f.path();
    size_t size = f.size();
    f.close();
    int slash = path.lastIndexOf('/');
    String base = path.substring(slash + 1);
    int dot = base.lastIndexOf('.');
    if (dot <= 0) continue;
    String name = base.substring(0, dot), ext = base.substring(dot + 1);
    if (ext != "png" && ext != "jpg" && ext != "gif") continue;
    if (!imageValidName(name.c_str()) || fileFind(name.c_str()) >= 0) continue;
    if (fileCount >= MAX_IMAGE_FILES) break;
    ImageFile& e = files[fileCount++];
    strlcpy(e.name, name.c_str(), sizeof(e.name));
    strlcpy(e.ext, ext.c_str(), sizeof(e.ext));
    e.size = size;
  }
  dir.close();
}

void imagesToJson(JsonArray arr) {
  for (uint8_t i = 0; i < fileCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = files[i].name;
    o["type"] = files[i].ext;
    o["size"] = files[i].size;
  }
}

// =====================
// RAW DATA (files and URLs in PSRAM)
// =====================
enum class RawState : uint8_t { NONE, WAIT, FETCHING, OK, ERROR };

struct RawImage {
  bool used;
  char src[IMAGE_SRC_LEN + 1];
  bool remote;
  uint8_t* data;
  size_t size;
  ImgType type;
  int16_t w, h;
  uint32_t lastUsed;
  uint32_t generation;       // bumped when data is replaced, invalidates decodes
  // remote
  RawState state;
  bool wanted, force;
  uint16_t refreshS;
  uint32_t lastAttemptMs;
  char error[48];
};
static RawImage raws[IMAGE_RAW_SLOTS];
static uint32_t generationCounter = 1;

static bool headerSize(ImgType t, const uint8_t* d, size_t len, int16_t& w, int16_t& h) {
  if (t == ImgType::PNG && len >= 24) {
    // Widths beyond 32767 are not for this screen anyway; the low 16 bits are enough.
    w = (int16_t)(((uint32_t)d[18] << 8) | d[19]);
    h = (int16_t)(((uint32_t)d[22] << 8) | d[23]);
    return w > 0 && h > 0;
  }
  if (t == ImgType::GIF && len >= 10) {
    w = d[6] | (d[7] << 8);
    h = d[8] | (d[9] << 8);
    return w > 0 && h > 0;
  }
  if (t == ImgType::JPG) {
    size_t i = 2;
    while (i + 9 < len) {
      if (d[i] != 0xFF) { i++; continue; }
      uint8_t m = d[i + 1];
      if (m == 0xFF) { i++; continue; }
      if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01) { i += 2; continue; }
      uint16_t seg = (d[i + 2] << 8) | d[i + 3];
      if ((m >= 0xC0 && m <= 0xC3) || (m >= 0xC5 && m <= 0xC7) || (m >= 0xC9 && m <= 0xCB) || (m >= 0xCD && m <= 0xCF)) {
        h = (d[i + 5] << 8) | d[i + 6];
        w = (d[i + 7] << 8) | d[i + 8];
        return w > 0 && h > 0;
      }
      i += 2 + seg;
    }
  }
  return false;
}

static RawImage* rawFind(const char* src) {
  for (auto& r : raws) if (r.used && !strcmp(r.src, src)) return &r;
  return nullptr;
}

static void rawFree(RawImage& r) {
  if (r.data) free(r.data);
  memset(&r, 0, sizeof(r));
}

static RawImage* rawAlloc(const char* src) {
  RawImage* victim = nullptr;
  for (auto& r : raws) {
    if (!r.used) { victim = &r; break; }
    if (!victim || r.lastUsed < victim->lastUsed) victim = &r;
  }
  if (victim->used) rawFree(*victim);
  memset(victim, 0, sizeof(*victim));
  victim->used = true;
  strlcpy(victim->src, src, sizeof(victim->src));
  victim->remote = isUrl(src);
  victim->generation = generationCounter++;
  victim->refreshS = IMAGE_DEFAULT_REFRESH_S;
  return victim;
}

static void rawSetData(RawImage& r, uint8_t* data, size_t size) {
  if (r.data) free(r.data);
  r.data = data;
  r.size = size;
  r.type = imageSniff(data, size);
  r.w = r.h = 0;
  headerSize(r.type, data, size, r.w, r.h);
  r.generation = generationCounter++;
}

// Loads a stored file into the raw table (caller holds the lock).
static RawImage* rawLoadFile(const char* name) {
  String path = imagePath(name);
  if (!path.length()) return nullptr;
  File f = LittleFS.open(path, "r");
  if (!f) return nullptr;
  size_t size = f.size();
  if (size == 0 || size > IMAGE_MAX_FILE) { f.close(); return nullptr; }
  uint8_t* buf = (uint8_t*)psAlloc(size);
  if (!buf) { f.close(); return nullptr; }
  size_t got = f.read(buf, size);
  f.close();
  if (got != size) { free(buf); return nullptr; }
  RawImage* r = rawAlloc(name);
  rawSetData(*r, buf, size);
  r->state = RawState::OK;
  return r;
}

// Finds a source, loading a file on first use. Remote sources are created
// in WAIT state and filled by the fetch task.
static RawImage* rawGet(const char* src, uint32_t now) {
  RawImage* r = rawFind(src);
  if (!r) {
    if (isUrl(src)) {
      r = rawAlloc(src);
      r->state = RawState::WAIT;
    } else {
      r = rawLoadFile(src);
    }
  }
  if (r) r->lastUsed = now ? now : 1;
  return r;
}

// =====================
// DECODED CACHE
// =====================
struct Decoded {
  bool used;
  RawImage* raw;
  uint32_t generation;
  int16_t w, h;
  ImgFit fit;
  lgfx::LGFX_Sprite* sprite;
  uint32_t lastUsed;
  // gif
  AnimatedGIF* gif;
  uint32_t frameDue;
  int16_t drawX, drawY, drawW, drawH;   // where the scaled canvas lands inside the sprite
  int16_t prevX, prevY, prevW, prevH;   // last frame rect, for disposal
  uint8_t prevDisposal;
  int16_t* xmap;                        // for each output column, the source column
  uint32_t lastUs, avgUs;               // decode time of the last frame and a running average
};
static Decoded decoded[IMAGE_DECODED];

static void decodedFree(Decoded& d) {
  if (d.gif) { d.gif->close(); delete d.gif; }
  if (d.sprite) { d.sprite->deleteSprite(); delete d.sprite; }
  free(d.xmap);
  memset(&d, 0, sizeof(d));
}

// Sprites store 16-bit pixels byte-swapped (big-endian), so the GIF palette
// is requested in that order and lines are written straight into the buffer.
static const uint16_t TRANSP_SWAPPED = (uint16_t)((TRANSP << 8) | (TRANSP >> 8));

static void fitRect(ImgFit fit, int16_t iw, int16_t ih, int16_t w, int16_t h,
                    int16_t& dx, int16_t& dy, int16_t& dw, int16_t& dh) {
  float sx = (float)w / iw, sy = (float)h / ih;
  if (fit == ImgFit::STRETCH) { dx = 0; dy = 0; dw = w; dh = h; return; }
  float s = fit == ImgFit::CONTAIN ? (sx < sy ? sx : sy) : (sx > sy ? sx : sy);
  dw = (int16_t)lroundf(iw * s);
  dh = (int16_t)lroundf(ih * s);
  if (dw < 1) dw = 1;
  if (dh < 1) dh = 1;
  dx = (w - dw) / 2;
  dy = (h - dh) / 2;
}

// GIF line callback: scales the frame line and writes it into the sprite
// buffer directly. Canvas pixels map to output columns through xmap.
static void gifDraw(GIFDRAW* p) {
  Decoded* d = (Decoded*)p->pUser;
  if (!d || !d->sprite || !d->gif || !d->xmap) return;
  int chh = d->gif->getCanvasHeight();
  if (chh <= 0) return;

  int srcY = p->iY + p->y;
  int y0 = d->drawY + (int)((int32_t)srcY * d->drawH / chh);
  int y1 = d->drawY + (int)((int32_t)(srcY + 1) * d->drawH / chh);
  if (y0 < 0) y0 = 0;
  if (y1 > d->h) y1 = d->h;
  if (y1 <= y0) return;

  uint16_t* buf = (uint16_t*)d->sprite->getBuffer();
  const int16_t* xmap = d->xmap;
  const uint16_t* pal = p->pPalette;
  const uint8_t* px = p->pPixels;
  int fx0 = p->iX, fx1 = p->iX + p->iWidth;

  // Output columns whose source column falls inside this frame.
  int ox0 = d->drawX, ox1 = d->drawX + d->drawW;
  if (ox0 < 0) ox0 = 0;
  if (ox1 > d->w) ox1 = d->w;

  uint16_t* row = buf + (size_t)y0 * d->w;
  for (int ox = ox0; ox < ox1; ox++) {
    int sx = xmap[ox - d->drawX];
    if (sx < fx0 || sx >= fx1) continue;
    uint8_t idx = px[sx - fx0];
    if (p->ucHasTransparency && idx == p->ucTransparent) continue;
    uint16_t c = pal[idx];
    if (c == TRANSP_SWAPPED) c ^= 0x0100;   // keep the key colour for "nothing drawn"
    row[ox] = c;
  }
  // Rows sharing this source line are copies of the first.
  for (int y = y0 + 1; y < y1; y++) memcpy(buf + (size_t)y * d->w + ox0, row + ox0, (ox1 - ox0) * 2);
  d->prevDisposal = p->ucDisposalMethod;
}

static Decoded* decodeGet(RawImage* r, int16_t w, int16_t h, ImgFit fit, uint32_t now) {
  Decoded* hit = nullptr;
  Decoded* victim = nullptr;
  for (auto& d : decoded) {
    if (d.used && d.raw == r && d.generation == r->generation && d.w == w && d.h == h && d.fit == fit) { hit = &d; break; }
    if (!d.used) { if (!victim || victim->used) victim = &d; }
    else if (!victim || (victim->used && d.lastUsed < victim->lastUsed)) victim = &d;
  }
  if (hit) { hit->lastUsed = now; return hit; }
  if (!r->data || r->w <= 0 || r->h <= 0 || w <= 0 || h <= 0) return nullptr;

  if (victim->used) decodedFree(*victim);
  Decoded& d = *victim;
  memset(&d, 0, sizeof(d));
  d.sprite = new lgfx::LGFX_Sprite();
  d.sprite->setPsram(true);
  d.sprite->setColorDepth(16);
  if (!d.sprite->createSprite(w, h)) { delete d.sprite; d.sprite = nullptr; return nullptr; }
  d.sprite->fillSprite(TRANSP);
  d.used = true;
  d.raw = r;
  d.generation = r->generation;
  d.w = w;
  d.h = h;
  d.fit = fit;
  d.lastUsed = now;
  fitRect(fit, r->w, r->h, w, h, d.drawX, d.drawY, d.drawW, d.drawH);
  float sx = (float)d.drawW / r->w, sy = (float)d.drawH / r->h;

  bool ok = true;
  switch (r->type) {
    case ImgType::PNG: ok = d.sprite->drawPng(r->data, r->size, d.drawX, d.drawY, 0, 0, 0, 0, sx, sy); break;
    case ImgType::JPG: ok = d.sprite->drawJpg(r->data, r->size, d.drawX, d.drawY, 0, 0, 0, 0, sx, sy); break;
    case ImgType::GIF: {
      d.gif = new AnimatedGIF();
      d.gif->begin(GIF_PALETTE_RGB565_BE);
      if (!d.gif->open(r->data, r->size, gifDraw)) { ok = false; break; }
      int cw = d.gif->getCanvasWidth();
      d.xmap = (int16_t*)psAlloc(sizeof(int16_t) * (d.drawW > 0 ? d.drawW : 1));
      if (!d.xmap || cw <= 0) { ok = false; break; }
      for (int ox = 0; ox < d.drawW; ox++) d.xmap[ox] = (int16_t)((int32_t)ox * cw / d.drawW);
      d.frameDue = 0;
      break;
    }
    default: ok = false;
  }
  if (!ok) {
    Log.printf("[IMG] %s: decode failed\n", r->src);
    decodedFree(d);
    return nullptr;
  }
  return &d;
}

static void gifStep(Decoded& d, uint32_t now) {
  if (!d.gif || (int32_t)(now - d.frameDue) < 0) return;
  if (d.prevDisposal == 2 && d.prevW > 0) d.sprite->fillRect(d.prevX, d.prevY, d.prevW, d.prevH, TRANSP);
  int delay = 100;
  uint32_t t0 = micros();
  int rc = d.gif->playFrame(false, &delay, &d);
  d.lastUs = micros() - t0;
  d.avgUs = d.avgUs ? (d.avgUs * 7 + d.lastUs) / 8 : d.lastUs;
  if (rc < 0) { d.frameDue = now + 1000; return; }
  if (rc == 0) d.gif->reset();
  if (delay < 20) delay = 20;
  d.frameDue = now + delay;
  // Remember this frame's rect for disposal; AnimatedGIF exposes it via the last draw.
  d.prevX = d.drawX; d.prevY = d.drawY; d.prevW = d.drawW; d.prevH = d.drawH;
}

// =====================
// PUBLIC DRAWING
// =====================
bool imageSize(const char* src, int16_t& w, int16_t& h) {
  take();
  RawImage* r = rawGet(src, millis());
  bool ok = r && r->data && r->w > 0 && r->h > 0;
  if (ok) { w = r->w; h = r->h; }
  give();
  return ok;
}

bool imageDraw(lgfx::LGFX_Sprite& ui, const char* src, int16_t x, int16_t y, int16_t w, int16_t h,
               ImgFit fit, uint32_t now) {
  if (w <= 0 || h <= 0) return false;
  take();
  RawImage* r = rawGet(src, now);
  if (!r || !r->data) { give(); return false; }
  Decoded* d = decodeGet(r, w, h, fit, now);
  if (!d) { give(); return false; }
  if (d->gif) gifStep(*d, now);
  d->sprite->pushSprite(&ui, x, y, TRANSP);
  give();
  return true;
}

void imageTouch(const char* src, uint16_t refreshS) {
  if (!isUrl(src)) return;
  take();
  RawImage* r = rawGet(src, millis());
  if (r) {
    r->wanted = true;
    if (refreshS < IMAGE_MIN_REFRESH_S) refreshS = IMAGE_MIN_REFRESH_S;
    r->refreshS = refreshS;
  }
  give();
}

void imageDecodedToJson(JsonArray arr) {
  take();
  for (auto& d : decoded) {
    if (!d.used || !d.raw) continue;
    JsonObject o = arr.add<JsonObject>();
    o["src"] = d.raw->src;
    o["w"] = d.w;
    o["h"] = d.h;
    if (d.gif) {
      o["gif"] = true;
      o["frameMs"] = d.lastUs / 1000.0;
      o["avgFrameMs"] = d.avgUs / 1000.0;
    }
  }
  give();
}

void imageRemotesToJson(JsonArray arr) {
  take();
  for (auto& r : raws) {
    if (!r.used || !r.remote) continue;
    JsonObject o = arr.add<JsonObject>();
    o["src"] = r.src;
    const char* st = "wait";
    switch (r.state) {
      case RawState::OK:       st = "ok"; break;
      case RawState::ERROR:    st = "error"; break;
      case RawState::FETCHING: st = "fetching"; break;
      default: break;
    }
    o["state"] = st;
    if (r.error[0]) o["error"] = r.error;
    if (r.data) { o["size"] = r.size; o["w"] = r.w; o["h"] = r.h; }
    o["refreshS"] = r.refreshS;
    if (r.lastAttemptMs) o["ageS"] = (millis() - r.lastAttemptMs) / 1000;
  }
  give();
}

// =====================
// FILE MANAGEMENT
// =====================
bool imageDelete(const char* name, char* err, size_t errLen) {
  take();
  String path = imagePath(name);
  if (!path.length()) { give(); strlcpy(err, "no such image", errLen); return false; }
  LittleFS.remove(path);
  RawImage* r = rawFind(name);
  if (r) {
    for (auto& d : decoded) if (d.used && d.raw == r) decodedFree(d);
    rawFree(*r);
  }
  scanLocked();
  give();
  return true;
}

void imagesRescan() {
  take();
  scanLocked();
  // A re-uploaded file with the same name must be reloaded.
  for (auto& r : raws) {
    if (r.used && !r.remote) {
      for (auto& d : decoded) if (d.used && d.raw == &r) decodedFree(d);
      rawFree(r);
    }
  }
  give();
}

// =====================
// REMOTE FETCH TASK
// =====================
class BufferStream : public Stream {
public:
  BufferStream(uint8_t* buf, size_t cap) : _buf(buf), _cap(cap) {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    if (_len + n > _cap) { overflow = true; n = _cap - _len; }
    memcpy(_buf + _len, b, n);
    _len += n;
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t length() const { return _len; }
  bool overflow = false;
private:
  uint8_t* _buf;
  size_t _cap;
  size_t _len = 0;
};

static bool nextJob(char* url, size_t urlLen, uint32_t now) {
  bool found = false;
  take();
  for (auto& r : raws) {
    if (!r.used || !r.remote || r.state == RawState::FETCHING) continue;
    bool due = r.force;
    if (!due && r.wanted) {
      if (r.lastAttemptMs == 0) due = true;
      else {
        uint32_t waitS = r.state == RawState::ERROR ? RETRY_AFTER_ERROR_S : r.refreshS;
        due = (now - r.lastAttemptMs) >= waitS * 1000UL;
      }
    }
    if (!due) continue;
    strlcpy(url, r.src, urlLen);
    r.state = RawState::FETCHING;
    r.lastAttemptMs = now ? now : 1;
    r.wanted = false;
    r.force = false;
    found = true;
    break;
  }
  give();
  return found;
}

static void fetchTask(void*) {
  static char url[IMAGE_SRC_LEN + 1];
  for (;;) {
    if (wifiState != WifiState::CONNECTED || !nextJob(url, sizeof(url), millis())) {
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    char error[48] = "";
    uint8_t* buf = nullptr;
    size_t len = 0;
    {
      bool https = !strncmp(url, "https://", 8);
      WiFiClientSecure secure;
      WiFiClient plain;
      if (https) secure.setInsecure();
      WiFiClient& client = https ? static_cast<WiFiClient&>(secure) : plain;
      HTTPClient http;
      http.setConnectTimeout(CONNECT_TIMEOUT_MS);
      http.setTimeout(READ_TIMEOUT_MS);
      http.setReuse(false);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.setUserAgent("DeskWiG/0.7");
      if (!http.begin(client, url)) {
        strlcpy(error, "bad url", sizeof(error));
      } else {
        http.addHeader("Accept", "image/png, image/jpeg, image/gif;q=0.9, */*;q=0.5");
        int code = http.GET();
        if (code <= 0) snprintf(error, sizeof(error), "%s", HTTPClient::errorToString(code).c_str());
        else if (code < 200 || code >= 300) snprintf(error, sizeof(error), "HTTP %d", code);
        else if (http.getSize() > (int)IMAGE_MAX_FILE) snprintf(error, sizeof(error), "image larger than %uK", (unsigned)(IMAGE_MAX_FILE / 1024));
        else {
          buf = (uint8_t*)psAlloc(IMAGE_MAX_FILE);
          if (!buf) strlcpy(error, "out of memory", sizeof(error));
          else {
            BufferStream bs(buf, IMAGE_MAX_FILE);
            int written = http.writeToStream(&bs);
            if (bs.overflow) snprintf(error, sizeof(error), "image larger than %uK", (unsigned)(IMAGE_MAX_FILE / 1024));
            else if (written < 0 && bs.length() == 0) strlcpy(error, "read failed", sizeof(error));
            else {
              len = bs.length();
              if (imageSniff(buf, len) == ImgType::UNKNOWN) strlcpy(error, "not a PNG, JPEG or GIF", sizeof(error));
            }
          }
        }
        http.end();
      }
    }

    take();
    RawImage* r = rawFind(url);
    if (r && r->state == RawState::FETCHING) {
      if (!error[0] && buf) {
        // Shrink to fit so a small image does not pin 512K of PSRAM.
        uint8_t* fit = (uint8_t*)psAlloc(len);
        if (fit) { memcpy(fit, buf, len); free(buf); buf = fit; }
        for (auto& d : decoded) if (d.used && d.raw == r) decodedFree(d);
        rawSetData(*r, buf, len);
        buf = nullptr;
        r->state = RawState::OK;
        r->error[0] = '\0';
      } else {
        r->state = RawState::ERROR;
        strlcpy(r->error, error[0] ? error : "failed", sizeof(r->error));
      }
    }
    give();
    if (buf) free(buf);
    Log.printf("[IMG] %s: %s%s\n", url, error[0] ? "error - " : "ok", error);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void imagesBegin() {
  if (lock) return;
  lock = xSemaphoreCreateMutex();
  take();
  scanLocked();
  give();
  xTaskCreatePinnedToCore(fetchTask, "images", 12288, nullptr, 1, nullptr, 0);
  Log.printf("[IMG] %u image(s)\n", fileCount);
}
