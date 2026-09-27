#include "WebServer.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "../app/ScreenManager.h"
#include "../layout/LayoutStore.h"
#include "../layout/LayoutData.h"
#include "../layout/LayoutTemplates.h"
#include "../layout/FontService.h"
#include "../layout/ImageService.h"
#include "../app/StatusLed.h"
#include "../net/WifiManager.h"
#include "../net/TimeService.h"
#include "../net/DataSource.h"
#include "Settings.h"
#include "Pages.h"
#include "EditorPage.h"
#include "DesignerPage.h"

extern ScreenManager screens;

static AsyncWebServer server(80);

// Routes use exact matching: the library default also matches any
// sub-path, so "/api/layouts" would swallow "/api/layouts/data".

static const char* FW_VERSION = "0.7.0";

// Deferred actions. Restarting from inside an async handler is unsafe,
// so handlers set these and webLoop() acts on them.
enum class PendingAction : uint8_t { NONE, REBOOT, FACTORY_RESET };
static PendingAction pendingAction = PendingAction::NONE;
static uint32_t pendingAt = 0;

// =====================
// HELPERS
// =====================
static void sendJson(AsyncWebServerRequest* req, int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  req->send(code, "application/json", out);
}

static void sendError(AsyncWebServerRequest* req, int code, const char* message) {
  JsonDocument doc;
  doc["error"] = message;
  sendJson(req, code, doc);
}

static void sendOk(AsyncWebServerRequest* req) {
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(req, 200, doc);
}

// True when the request arrived over the setup hotspot. Anyone standing
// next to the device with the hotspot password is trusted for setup.
static bool viaHotspot(AsyncWebServerRequest* req) {
  if (!wifiApActive() || !req->client()) return false;
  return req->client()->localIP() == WiFi.softAPIP();
}

// Returns true if the request is authorised. On failure it has already
// sent a 401, so callers just return.
static bool requireAuth(AsyncWebServerRequest* req) {
  if (viaHotspot(req)) return true;

  auto* h = req->getHeader("Authorization");
  if (h) {
    const String& v = h->value();
    if (v.startsWith("Bearer ") && v.substring(7) == settings.apiToken) return true;
  }
  sendError(req, 401, "unauthorized");
  return false;
}

static void fillWidgetList(JsonDocument& doc) {
  doc["active"] = screens.getActive();
  JsonArray list = doc["widgets"].to<JsonArray>();
  for (uint8_t i = 0; i < screens.getCount(); i++) {
    const char* name = screens.getName(i);
    list.add((name && *name) ? name : "Unknown");
  }
}

static void fillWifiStatus(JsonObject doc) {
  doc["state"]    = wifiStateName();
  doc["apReason"] = wifiApReasonName();
  doc["apActive"] = wifiApActive();
  doc["ssid"]     = settings.wifiSSID;
  doc["hostname"] = settings.hostname;
  doc["ip"]       = (wifiState == WifiState::CONNECTED) ? WiFi.localIP().toString() : String("");
  doc["apIp"]     = wifiApActive() ? WiFi.softAPIP().toString() : String("");
  doc["rssi"]     = (wifiState == WifiState::CONNECTED) ? WiFi.RSSI() : 0;
}

static void fillConfig(JsonDocument& doc) {
  doc["hostname"]       = settings.hostname;
  doc["pingIntervalMs"] = settings.pingIntervalMs;
  doc["activeWidget"]   = settings.activeWidget;
  JsonObject clock = doc["clock"].to<JsonObject>();
  clock["tz"]        = settings.clockTz;
  clock["24h"]       = settings.clock24h;
  clock["ntpSource"] = ntpSourceName(settings.ntpSource);
  clock["ntpServer"] = settings.ntpServer;
  JsonObject led = doc["led"].to<JsonObject>();
  led["enabled"]    = settings.ledEnabled;
  led["brightness"] = settings.ledBrightness;
}

static void fillTimeStatus(JsonObject doc) {
  doc["synced"] = timeSynced();
  doc["server"] = timeServerInUse();
  doc["source"] = ntpSourceName(settings.ntpSource);
  doc["tz"]     = settings.clockTz;
  doc["gateway"] = (wifiState == WifiState::CONNECTED) ? WiFi.gatewayIP().toString() : String("");
  char buf[32] = "";
  if (timeSynced()) {
    time_t t = time(nullptr);
    tm lt;
    localtime_r(&t, &lt);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
  }
  doc["local"] = buf;
}

static void schedule(PendingAction a, uint32_t delayMs) {
  pendingAction = a;
  pendingAt = millis() + delayMs;
}

// =====================
// ROUTES
// =====================
// CodeMirror bundle for the editor, built by tools/editor and embedded gzipped.
extern const uint8_t cm_js_gz_start[] asm("_binary_web_cm_js_gz_start");
extern const uint8_t cm_js_gz_end[]   asm("_binary_web_cm_js_gz_end");

static void registerPages() {
  server.on(AsyncURIMatcher::exact("/cm.js"), HTTP_GET, [](AsyncWebServerRequest* req) {
    // Revalidated on every load with an ETag so a new firmware's bundle is
    // never shadowed by a cached one; a match costs a tiny 304.
    size_t len = cm_js_gz_end - cm_js_gz_start;
    char etag[32];
    snprintf(etag, sizeof(etag), "\"cm-%u\"", (unsigned)len);
    if (req->hasHeader("If-None-Match") && req->getHeader("If-None-Match")->value() == etag) {
      AsyncWebServerResponse* r = req->beginResponse(304);
      r->addHeader("ETag", etag);
      req->send(r);
      return;
    }
    AsyncWebServerResponse* r = req->beginResponse(200, "application/javascript", cm_js_gz_start, len);
    r->addHeader("Content-Encoding", "gzip");
    r->addHeader("Cache-Control", "no-cache");
    r->addHeader("ETag", etag);
    req->send(r);
  });

  server.on(AsyncURIMatcher::exact("/style.css"), HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200, "text/css", STYLE_CSS);
    r->addHeader("Cache-Control", "max-age=3600");
    req->send(r);
  });

  server.on(AsyncURIMatcher::exact("/setup"), HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", SETUP_HTML);
  });

  server.on(AsyncURIMatcher::exact("/widgets"), HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", WIDGETS_HTML);
  });

  server.on(AsyncURIMatcher::exact("/designer.js"), HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200, "application/javascript", (const uint8_t*)DESIGNER_JS, strlen(DESIGNER_JS));
    r->addHeader("Cache-Control", "no-cache");
    req->send(r);
  });

  server.on(AsyncURIMatcher::exact("/editor"), HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", EDITOR_HTML);
  });

  server.on(AsyncURIMatcher::exact("/"), HTTP_GET, [](AsyncWebServerRequest* req) {
    req->redirect(viaHotspot(req) ? "/setup" : "/widgets");
  });
}

static void registerStatus() {
  server.on(AsyncURIMatcher::exact("/api/status"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["firmware"]     = FW_VERSION;
    doc["uptimeMs"]     = millis();
    doc["freeHeap"]     = ESP.getFreeHeap();
    doc["freePsram"]    = ESP.getFreePsram();
    doc["activeWidget"] = screens.getActive();
    doc["widgetCount"]  = screens.getCount();
    fillWifiStatus(doc["wifi"].to<JsonObject>());
    fillTimeStatus(doc["time"].to<JsonObject>());
    JsonObject led = doc["led"].to<JsonObject>();
    led["enabled"] = settings.ledEnabled;
    const LedSpec* ls = ledPattern() == LedPattern::NORMAL ? ledWidgetSpec() : nullptr;
    if (ls) {
      led["mode"] = ledModeName(ls->mode);
      char c[8];
      snprintf(c, sizeof(c), "#%02x%02x%02x", ls->r, ls->g, ls->b);
      led["color"] = c;
      led["speedMs"] = ls->speedMs;
      led["brightness"] = ls->brightness;
    } else {
      led["mode"] = ledPattern() == LedPattern::NORMAL ? "off" : "system";
    }
    sendJson(req, 200, doc);
  });
}

static void registerWidgets() {
  server.on(AsyncURIMatcher::exact("/api/widgets"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    fillWidgetList(doc);
    sendJson(req, 200, doc);
  });

  server.on(AsyncURIMatcher::exact("/api/widgets"), HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("index", true)) {
      sendError(req, 400, "missing index");
      return;
    }
    int idx = req->getParam("index", true)->value().toInt();
    if (idx < 0 || idx >= screens.getCount()) {
      sendError(req, 400, "index out of range");
      return;
    }
    screens.setActive(idx);
    settings.activeWidget = idx;
    strlcpy(settings.activeWidgetName, screens.getName(idx), sizeof(settings.activeWidgetName));
    if (!saveSettings()) {
      sendError(req, 500, "failed to save settings");
      return;
    }
    JsonDocument doc;
    fillWidgetList(doc);
    sendJson(req, 200, doc);
  });
}

static void fillLayoutList(JsonDocument& doc) {
  JsonArray list = doc["layouts"].to<JsonArray>();
  for (uint8_t i = 0; i < layoutCount(); i++) {
    LayoutWidget* w = layoutAt(i);
    JsonObject o = list.add<JsonObject>();
    o["id"]    = w->id();
    o["name"]  = w->name();
    o["index"] = screens.indexOf(w);
  }
  doc["free"]    = layoutFreeSlots();
  doc["max"]     = MAX_LAYOUTS;
  doc["preview"] = layoutPreviewActive();
}

static void registerLayouts() {
  // GET /api/layouts           -> list
  // GET /api/layouts?id=<id>   -> the stored JSON file
  server.on(AsyncURIMatcher::exact("/api/layouts"), HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("id")) {
      String id = req->getParam("id")->value();
      if (!layoutValidId(id.c_str()) || !layoutFind(id.c_str())) {
        sendError(req, 404, "no such widget");
        return;
      }
      req->send(LittleFS, layoutPath(id.c_str()), "application/json");
      return;
    }
    JsonDocument doc;
    fillLayoutList(doc);
    sendJson(req, 200, doc);
  });

  auto* put = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/layouts"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("id")) {
      sendError(req, 400, "missing id");
      return;
    }
    String id = req->getParam("id")->value();
    char err[96];
    if (!layoutSave(id.c_str(), json.as<JsonVariantConst>(), err, sizeof(err))) {
      sendError(req, 400, err);
      return;
    }
    JsonDocument doc;
    fillLayoutList(doc);
    doc["saved"] = id;
    sendJson(req, 200, doc);
  });
  put->setMethod(HTTP_PUT);
  put->setMaxContentLength(16384);
  server.addHandler(put);

  server.on(AsyncURIMatcher::exact("/api/layouts"), HTTP_DELETE, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("id")) {
      sendError(req, 400, "missing id");
      return;
    }
    String id = req->getParam("id")->value();
    char err[96];
    if (!layoutDelete(id.c_str(), err, sizeof(err))) {
      sendError(req, 404, err);
      return;
    }
    JsonDocument doc;
    fillLayoutList(doc);
    doc["active"] = screens.getActive();
    sendJson(req, 200, doc);
  });

  // Show a layout on the device without saving it.
  auto* preview = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/layouts/preview"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    char err[96];
    if (!layoutPreview(json.as<JsonVariantConst>(), err, sizeof(err))) {
      sendError(req, 400, err);
      return;
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["expiresMs"] = LAYOUT_PREVIEW_MS;
    sendJson(req, 200, doc);
  });
  preview->setMethod(HTTP_POST);
  preview->setMaxContentLength(16384);
  server.addHandler(preview);

  server.on(AsyncURIMatcher::exact("/api/layouts/preview"), HTTP_DELETE, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    layoutPreviewStop();
    sendOk(req);
  });

  // Built-in templates for the editor's picker. Assembled by hand so the
  // stored JSON text is passed through without re-serialising it.
  server.on(AsyncURIMatcher::exact("/api/layouts/templates"), HTTP_GET, [](AsyncWebServerRequest* req) {
    String out;
    out.reserve(12288);
    out += "[";
    for (uint8_t i = 0; i < LAYOUT_TEMPLATE_COUNT; i++) {
      const LayoutTemplate& t = LAYOUT_TEMPLATES[i];
      if (i) out += ",";
      out += "{\"id\":\"";
      out += t.id;
      out += "\",\"name\":\"";
      out += t.name;
      out += "\",\"preload\":";
      out += t.preload ? "true" : "false";
      out += ",\"layout\":";
      out += t.json;
      out += "}";
    }
    out += "]";
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", out);
    r->addHeader("Cache-Control", "max-age=3600");
    req->send(r);
  });

  // Every template key with its current value, for the editor's preview.
  server.on(AsyncURIMatcher::exact("/api/layouts/data"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    layoutFillData(doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });
}

static void registerWifi() {
  server.on(AsyncURIMatcher::exact("/api/wifi"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    fillWifiStatus(doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });

  // Async scan: first call starts it, later calls return results when ready.
  server.on(AsyncURIMatcher::exact("/api/wifi/scan"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    int16_t n = WiFi.scanComplete();

    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true, false);
      doc["status"] = "scanning";
    } else if (n == WIFI_SCAN_RUNNING) {
      doc["status"] = "scanning";
    } else {
      doc["status"] = "done";
      JsonArray list = doc["networks"].to<JsonArray>();

      // Sort by signal, drop duplicates and hidden networks.
      int16_t order[64];
      int16_t count = n > 64 ? 64 : n;
      for (int16_t i = 0; i < count; i++) order[i] = i;
      for (int16_t i = 1; i < count; i++) {
        int16_t v = order[i];
        int16_t j = i - 1;
        while (j >= 0 && WiFi.RSSI(order[j]) < WiFi.RSSI(v)) { order[j + 1] = order[j]; j--; }
        order[j + 1] = v;
      }
      for (int16_t k = 0; k < count; k++) {
        int16_t i = order[k];
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        bool dup = false;
        for (JsonObject e : list) {
          if (ssid == (const char*)e["ssid"]) { dup = true; break; }
        }
        if (dup) continue;
        JsonObject e = list.add<JsonObject>();
        e["ssid"]   = ssid;
        e["rssi"]   = WiFi.RSSI(i);
        e["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
    }
    sendJson(req, 200, doc);
  });

  auto* join = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/wifi/join"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    const char* ssid = json["ssid"] | "";
    const char* pass = json["pass"] | "";
    if (!*ssid || strlen(ssid) >= sizeof(settings.wifiSSID)) {
      sendError(req, 400, "invalid ssid");
      return;
    }
    if (strlen(pass) >= sizeof(settings.wifiPass)) {
      sendError(req, 400, "password too long");
      return;
    }
    wifiJoin(ssid, pass);
    JsonDocument doc;
    fillWifiStatus(doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });
  join->setMethod(HTTP_POST);
  server.addHandler(join);

  server.on(AsyncURIMatcher::exact("/api/wifi/forget"), HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    wifiForget();
    sendOk(req);
  });
}

static void registerConfig() {
  server.on(AsyncURIMatcher::exact("/api/config"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    fillConfig(doc);
    sendJson(req, 200, doc);
  });

  auto* put = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/config"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;

    bool hostnameChanged = false;
    if (!json["hostname"].isNull()) {
      const char* h = json["hostname"];
      if (strcmp(h, settings.hostname) != 0) {
        if (!setHostname(h)) {
          sendError(req, 400, "invalid hostname: use letters, digits and dashes");
          return;
        }
        hostnameChanged = true;
      }
    }
    if (!json["pingIntervalMs"].isNull()) {
      uint32_t v = json["pingIntervalMs"];
      settings.pingIntervalMs = v < 1000 ? 1000 : v;
    }
    JsonVariant clock = json["clock"];
    bool clockChanged = false;
    if (!clock.isNull()) {
      if (!clock["tz"].isNull()) {
        if (!setClockTz(clock["tz"])) {
          sendError(req, 400, "invalid tz: use a POSIX TZ string like EST5EDT,M3.2.0,M11.1.0");
          return;
        }
        clockChanged = true;
      }
      if (!clock["ntpSource"].isNull()) {
        settings.ntpSource = ntpSourceFromName(clock["ntpSource"]);
        clockChanged = true;
      }
      if (!clock["ntpServer"].isNull()) {
        if (!setNtpServer(clock["ntpServer"])) {
          sendError(req, 400, "invalid ntpServer: use a hostname or IP address");
          return;
        }
        clockChanged = true;
      }
      if (!clock["24h"].isNull()) settings.clock24h = clock["24h"];
    }
    JsonVariant led = json["led"];
    if (!led.isNull()) {
      if (!led["enabled"].isNull())    settings.ledEnabled    = led["enabled"];
      if (!led["brightness"].isNull()) settings.ledBrightness = led["brightness"];
      ledApplySettings();
    }

    if (!saveSettings()) {
      sendError(req, 500, "failed to save settings");
      return;
    }
    if (hostnameChanged) wifiApplyHostname();
    if (clockChanged) timeApply();

    JsonDocument doc;
    fillConfig(doc);
    sendJson(req, 200, doc);
  });
  put->setMethod(HTTP_PUT);
  server.addHandler(put);
}

// =====================
// DATA SOURCES
// =====================
static void fillSourceList(JsonDocument& doc) {
  JsonArray arr = doc["sources"].to<JsonArray>();
  sourcesLock();
  for (uint8_t i = 0; i < settings.sourceCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    sourceToJson(settings.sources[i], o, false);
    sourceStatusToJson(settings.sources[i], o);
  }
  sourcesUnlock();
  doc["free"] = MAX_SOURCES - settings.sourceCount;
  doc["max"]  = MAX_SOURCES;
}

static void registerSources() {
  // GET /api/sources -> every source with its config (header value
  // redacted), fetch state and current values.
  server.on(AsyncURIMatcher::exact("/api/sources"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    fillSourceList(doc);
    sendJson(req, 200, doc);
  });

  // PUT /api/sources?id=<id> -> create or replace one source. An omitted
  // or empty header value keeps the stored one, so the UI can re-save a
  // source without knowing the secret.
  auto* put = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/sources"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("id")) {
      sendError(req, 400, "missing id");
      return;
    }
    String id = req->getParam("id")->value();
    json["id"] = id;

    static DataSource parsed;
    char err[96];
    sourcesLock();
    bool ok = sourceFromJson(parsed, json.as<JsonVariantConst>(), err, sizeof(err));
    if (ok) {
      DataSource* existing = sourceFind(parsed.id);
      if (existing) {
        if (parsed.headerName[0] && !parsed.headerValue[0] && !strcmp(existing->headerName, parsed.headerName)) {
          strlcpy(parsed.headerValue, existing->headerValue, sizeof(parsed.headerValue));
        }
        *existing = parsed;
        sourceFetchNow(*existing);
      } else if (settings.sourceCount < MAX_SOURCES) {
        settings.sources[settings.sourceCount] = parsed;
        sourceFetchNow(settings.sources[settings.sourceCount]);
        settings.sourceCount++;
      } else {
        snprintf(err, sizeof(err), "no free slots (max %u sources)", MAX_SOURCES);
        ok = false;
      }
    }
    sourcesUnlock();

    if (!ok) {
      sendError(req, 400, err);
      return;
    }
    if (!saveSettings()) {
      sendError(req, 500, "failed to save settings");
      return;
    }
    JsonDocument doc;
    fillSourceList(doc);
    doc["saved"] = id;
    sendJson(req, 200, doc);
  });
  put->setMethod(HTTP_PUT);
  put->setMaxContentLength(4096);
  server.addHandler(put);

  server.on(AsyncURIMatcher::exact("/api/sources"), HTTP_DELETE, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("id")) {
      sendError(req, 400, "missing id");
      return;
    }
    String id = req->getParam("id")->value();
    sourcesLock();
    DataSource* s = sourceFind(id.c_str());
    if (s) {
      uint8_t idx = s - settings.sources;
      for (uint8_t i = idx; i + 1 < settings.sourceCount; i++) settings.sources[i] = settings.sources[i + 1];
      settings.sourceCount--;
    }
    sourcesUnlock();
    if (!s) {
      sendError(req, 404, "no such source");
      return;
    }
    if (!saveSettings()) {
      sendError(req, 500, "failed to save settings");
      return;
    }
    JsonDocument doc;
    fillSourceList(doc);
    sendJson(req, 200, doc);
  });

  // POST /api/sources/test?id=<id> -> fetch now. The result shows up in
  // GET /api/sources a moment later.
  server.on(AsyncURIMatcher::exact("/api/sources/test"), HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("id")) {
      sendError(req, 400, "missing id");
      return;
    }
    String id = req->getParam("id")->value();
    sourcesLock();
    DataSource* s = sourceFind(id.c_str());
    if (s) sourceFetchNow(*s);
    sourcesUnlock();
    if (!s) {
      sendError(req, 404, "no such source");
      return;
    }
    if (wifiState != WifiState::CONNECTED) {
      sendError(req, 409, "not connected to WiFi");
      return;
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["queued"] = id;
    sendJson(req, 202, doc);
  });
}

// =====================
// FONTS
// =====================
static void fillFontList(JsonDocument& doc) {
  fontsToJson(doc["fonts"].to<JsonArray>());
  doc["max"] = MAX_FONTS;
  doc["fsFree"] = LittleFS.totalBytes() - LittleFS.usedBytes();
  doc["fsTotal"] = LittleFS.totalBytes();
}

// Per-request state for a file upload (fonts and images).
struct FontUpload {
  File file;
  String name;
  String tmp;
  size_t written;
  size_t maxSize;
  bool ok;
  char err[80];
};
typedef FontUpload FileUpload;

static void fontUploadFail(FontUpload* u, const char* msg) {
  if (u->ok) strlcpy(u->err, msg, sizeof(u->err));
  u->ok = false;
  if (u->file) { u->file.close(); }
  if (u->tmp.length()) { LittleFS.remove(u->tmp); u->tmp = ""; }
}

static bool uploadAuthed(AsyncWebServerRequest* req) {
  if (!req->hasHeader("Authorization")) return false;
  String v = req->getHeader("Authorization")->value();
  return v.startsWith("Bearer ") && v.substring(7) == settings.apiToken;
}

static void registerFonts() {
  server.on(AsyncURIMatcher::exact("/api/fonts"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    fillFontList(doc);
    sendJson(req, 200, doc);
  });

  // POST /api/fonts?name=<name>  multipart upload of a .ttf file.
  server.on(AsyncURIMatcher::exact("/api/fonts"), HTTP_POST,
    [](AsyncWebServerRequest* req) {
      FontUpload* u = (FontUpload*)req->_tempObject;
      if (!u) { sendError(req, 400, "no file uploaded"); return; }
      bool ok = u->ok;
      char err[80];
      strlcpy(err, u->err, sizeof(err));
      String name = u->name, tmp = u->tmp;
      if (u->file) u->file.close();
      delete u;
      req->_tempObject = nullptr;

      if (!ok) { sendError(req, err[0] ? 400 : 500, err[0] ? err : "upload failed"); return; }
      if (!fontValidateFile(tmp.c_str())) {
        LittleFS.remove(tmp);
        sendError(req, 400, "not a usable TrueType (.ttf) file");
        return;
      }
      String path = fontPath(name.c_str());
      LittleFS.remove(path);
      if (!LittleFS.rename(tmp, path)) {
        LittleFS.remove(tmp);
        sendError(req, 500, "failed to store font");
        return;
      }
      fontsRescan();
      JsonDocument doc;
      fillFontList(doc);
      doc["saved"] = name;
      sendJson(req, 200, doc);
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
      FontUpload* u = (FontUpload*)req->_tempObject;
      if (index == 0) {
        u = new FontUpload();
        u->written = 0;
        u->ok = true;
        u->err[0] = '\0';
        req->_tempObject = u;

        // Auth is checked here rather than with requireAuth() because a
        // response cannot be sent mid-upload; the final handler reports it.
        bool authed = false;
        if (req->hasHeader("Authorization")) {
          String v = req->getHeader("Authorization")->value();
          authed = v.startsWith("Bearer ") && v.substring(7) == settings.apiToken;
        }
        if (!authed) { fontUploadFail(u, "unauthorized"); return; }

        String name = req->hasParam("name") ? req->getParam("name")->value() : filename;
        if (name.endsWith(".ttf")) name = name.substring(0, name.length() - 4);
        name.toLowerCase();
        if (!fontValidName(name.c_str())) { fontUploadFail(u, "invalid name: use 1-23 lowercase letters, digits and dashes"); return; }
        int existing = fontFind(name.c_str());
        if (existing >= 0 && fontBuiltinData(name.c_str(), nullptr)) { fontUploadFail(u, "that name is a built-in font"); return; }
        if (existing < 0) {
          JsonDocument tmpDoc;
          JsonArray arr = tmpDoc.to<JsonArray>();
          fontsToJson(arr);
          if (arr.size() >= MAX_FONTS) { fontUploadFail(u, "no free font slots"); return; }
        }
        size_t total = req->contentLength();
        if (total > FONT_MAX_FILE + 4096) { fontUploadFail(u, "font larger than 2 MB"); return; }
        size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (total + 8192 > freeBytes) { fontUploadFail(u, "not enough space on the filesystem"); return; }
        if (!LittleFS.exists("/fonts")) LittleFS.mkdir("/fonts");
        u->name = name;
        u->tmp = fontPath(name.c_str()) + ".tmp";
        u->file = LittleFS.open(u->tmp, "w");
        if (!u->file) { fontUploadFail(u, "cannot write file"); return; }
      }
      if (!u || !u->ok) return;
      if (len) {
        if (u->written + len > FONT_MAX_FILE) { fontUploadFail(u, "font larger than 2 MB"); return; }
        if (u->file.write(data, len) != len) { fontUploadFail(u, "write failed (filesystem full?)"); return; }
        u->written += len;
      }
      if (final) u->file.close();
    });

  server.on(AsyncURIMatcher::exact("/api/fonts"), HTTP_DELETE, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("name")) { sendError(req, 400, "missing name"); return; }
    String name = req->getParam("name")->value();
    char err[80];
    if (!fontDelete(name.c_str(), err, sizeof(err))) { sendError(req, 400, err); return; }
    JsonDocument doc;
    fillFontList(doc);
    sendJson(req, 200, doc);
  });

  // The font files themselves, so the editor's preview can use them.
  server.on(AsyncURIMatcher::prefix("/fonts/"), HTTP_GET, [](AsyncWebServerRequest* req) {
    String url = req->url();
    String base = url.substring(7);
    if (!base.endsWith(".ttf")) { sendError(req, 404, "not found"); return; }
    String name = base.substring(0, base.length() - 4);
    if (!fontValidName(name.c_str())) { sendError(req, 404, "not found"); return; }
    size_t len = 0;
    const uint8_t* data = fontBuiltinData(name.c_str(), &len);
    AsyncWebServerResponse* r;
    if (data) {
      r = req->beginResponse(200, "font/ttf", data, len);
    } else if (fontExists(name.c_str())) {
      r = req->beginResponse(LittleFS, fontPath(name.c_str()), "font/ttf");
    } else {
      sendError(req, 404, "not found");
      return;
    }
    r->addHeader("Cache-Control", "max-age=86400");
    req->send(r);
  });
}

// =====================
// IMAGES
// =====================
static void fillImageList(JsonDocument& doc) {
  imagesToJson(doc["images"].to<JsonArray>());
  imageRemotesToJson(doc["remote"].to<JsonArray>());
  doc["max"] = MAX_IMAGE_FILES;
  doc["fsFree"] = LittleFS.totalBytes() - LittleFS.usedBytes();
  doc["fsTotal"] = LittleFS.totalBytes();
}

static void registerImages() {
  server.on(AsyncURIMatcher::exact("/api/images"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    fillImageList(doc);
    sendJson(req, 200, doc);
  });

  // POST /api/images?name=<name>  multipart upload of a PNG, JPEG or GIF.
  server.on(AsyncURIMatcher::exact("/api/images"), HTTP_POST,
    [](AsyncWebServerRequest* req) {
      FileUpload* u = (FileUpload*)req->_tempObject;
      if (!u) { sendError(req, 400, "no file uploaded"); return; }
      bool ok = u->ok;
      char err[80];
      strlcpy(err, u->err, sizeof(err));
      String name = u->name, tmp = u->tmp;
      if (u->file) u->file.close();
      delete u;
      req->_tempObject = nullptr;
      if (!ok) { sendError(req, !strcmp(err, "unauthorized") ? 401 : 400, err[0] ? err : "upload failed"); return; }

      // Type from the file's own bytes, not the name the browser sent.
      File f = LittleFS.open(tmp, "r");
      uint8_t head[32];
      size_t n = f ? f.read(head, sizeof(head)) : 0;
      if (f) f.close();
      ImgType t = imageSniff(head, n);
      const char* ext = t == ImgType::PNG ? "png" : t == ImgType::JPG ? "jpg" : t == ImgType::GIF ? "gif" : nullptr;
      if (!ext) { LittleFS.remove(tmp); sendError(req, 400, "not a PNG, JPEG or GIF file"); return; }

      // Replace any existing file of that name whatever its type.
      String old = imagePath(name.c_str());
      if (old.length()) LittleFS.remove(old);
      String path = String("/img/") + name + "." + ext;
      if (!LittleFS.rename(tmp, path)) {
        LittleFS.remove(tmp);
        sendError(req, 500, "failed to store image");
        return;
      }
      imagesRescan();
      JsonDocument doc;
      fillImageList(doc);
      doc["saved"] = name;
      sendJson(req, 200, doc);
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
      FileUpload* u = (FileUpload*)req->_tempObject;
      if (index == 0) {
        u = new FileUpload();
        u->written = 0;
        u->maxSize = IMAGE_MAX_FILE;
        u->ok = true;
        u->err[0] = '\0';
        req->_tempObject = u;
        if (!uploadAuthed(req)) { fontUploadFail(u, "unauthorized"); return; }

        String name = req->hasParam("name") ? req->getParam("name")->value() : filename;
        int dot = name.lastIndexOf('.');
        if (dot > 0) name = name.substring(0, dot);
        name.toLowerCase();
        if (!imageValidName(name.c_str())) { fontUploadFail(u, "invalid name: use 1-23 lowercase letters, digits and dashes"); return; }
        if (!imagePath(name.c_str()).length()) {
          JsonDocument tmpDoc;
          JsonArray arr = tmpDoc.to<JsonArray>();
          imagesToJson(arr);
          if (arr.size() >= MAX_IMAGE_FILES) { fontUploadFail(u, "no free image slots"); return; }
        }
        size_t total = req->contentLength();
        if (total > IMAGE_MAX_FILE + 4096) { fontUploadFail(u, "image larger than 512 KB"); return; }
        size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (total + 8192 > freeBytes) { fontUploadFail(u, "not enough space on the filesystem"); return; }
        if (!LittleFS.exists("/img")) LittleFS.mkdir("/img");
        u->name = name;
        u->tmp = String("/img/") + name + ".tmp";
        u->file = LittleFS.open(u->tmp, "w");
        if (!u->file) { fontUploadFail(u, "cannot write file"); return; }
      }
      if (!u || !u->ok) return;
      if (len) {
        if (u->written + len > IMAGE_MAX_FILE) { fontUploadFail(u, "image larger than 512 KB"); return; }
        if (u->file.write(data, len) != len) { fontUploadFail(u, "write failed (filesystem full?)"); return; }
        u->written += len;
      }
      if (final) u->file.close();
    });

  server.on(AsyncURIMatcher::exact("/api/images"), HTTP_DELETE, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("name")) { sendError(req, 400, "missing name"); return; }
    String name = req->getParam("name")->value();
    char err[80];
    if (!imageDelete(name.c_str(), err, sizeof(err))) { sendError(req, 404, err); return; }
    JsonDocument doc;
    fillImageList(doc);
    sendJson(req, 200, doc);
  });

  // GET /img/<name> or /img/<name>.<ext>: the stored file, for the editor's preview.
  server.on(AsyncURIMatcher::prefix("/img/"), HTTP_GET, [](AsyncWebServerRequest* req) {
    String name = req->url().substring(5);
    int dot = name.lastIndexOf('.');
    if (dot > 0) name = name.substring(0, dot);
    String path = imagePath(name.c_str());
    if (!path.length()) { sendError(req, 404, "not found"); return; }
    const char* ext = imageExt(name.c_str());
    const char* type = !strcmp(ext, "png") ? "image/png" : !strcmp(ext, "jpg") ? "image/jpeg" : "image/gif";
    AsyncWebServerResponse* r = req->beginResponse(LittleFS, path, type);
    r->addHeader("Cache-Control", "no-cache");
    req->send(r);
  });
}

static void registerSystem() {
  server.on(AsyncURIMatcher::exact("/api/system/reboot"), HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    schedule(PendingAction::REBOOT, 500);
    sendOk(req);
  });

  server.on(AsyncURIMatcher::exact("/api/system/reset"), HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    schedule(PendingAction::FACTORY_RESET, 500);
    sendOk(req);
  });
}

// =====================
// SERVER START
// =====================
void startWebServer() {
  static bool started = false;
  if (started) return;
  started = true;

  registerPages();
  registerStatus();
  registerWidgets();
  registerLayouts();
  registerWifi();
  registerConfig();
  registerSources();
  registerFonts();
  registerImages();
  registerSystem();

  // Captive portal: any unknown URL requested over the hotspot lands on /setup.
  // Phones probe URLs like /generate_204 and /hotspot-detect.html; a redirect
  // is what makes them pop the "sign in to network" sheet.
  server.onNotFound([](AsyncWebServerRequest* req) {
    if (viaHotspot(req)) {
      String url = "http://" + WiFi.softAPIP().toString() + "/setup";
      req->redirect(url);
      return;
    }
    sendError(req, 404, "not found");
  });

  server.begin();
  Serial.println("[WEB] Server started");
}

void webLoop() {
  if (pendingAction == PendingAction::NONE) return;
  if ((int32_t)(millis() - pendingAt) < 0) return;

  PendingAction a = pendingAction;
  pendingAction = PendingAction::NONE;

  if (a == PendingAction::FACTORY_RESET) {
    ledSet(LedPattern::RESETTING);
    ledLoop();
    factoryReset();
  }
  Serial.println("[SYS] Restarting");
  delay(100);
  ESP.restart();
}
