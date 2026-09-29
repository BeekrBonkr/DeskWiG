#include "WebServer.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>

#include "../app/ScreenManager.h"
#include "../layout/LayoutStore.h"
#include "../layout/LayoutData.h"
#include "../layout/LayoutTemplates.h"
#include "../layout/FontService.h"
#include "../layout/ImageService.h"
#include "../app/StatusLed.h"
#include "../net/WifiManager.h"
#include "../app/Builtins.h"
#include "../app/Log.h"
#include "../net/TimeService.h"
#include "../net/DataSource.h"
#include "Settings.h"
#include "Auth.h"
#include "Pages.h"
#include "LoginPage.h"
#include "EditorPage.h"
#include "DesignerPage.h"
#include "../app/Log.h"

extern ScreenManager screens;

static AsyncWebServer server(80);

// Routes use exact matching: the library default also matches any
// sub-path, so "/api/layouts" would swallow "/api/layouts/data".

static const char* FW_VERSION = "0.8.0";

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

// Session cookie set by the login page.
static bool sessionAuthed(AsyncWebServerRequest* req) {
  auto* c = req->getHeader("Cookie");
  if (!c) return false;
  char sid[AUTH_SID_LEN + 1];
  return authSidFromCookie(c->value(), sid, sizeof(sid)) && authSessionValid(sid);
}

// API key as a bearer token, for scripts and curl.
static bool bearerAuthed(AsyncWebServerRequest* req) {
  auto* h = req->getHeader("Authorization");
  if (!h) return false;
  const String& v = h->value();
  return v.startsWith("Bearer ") && authCheckKey(v.c_str() + 7);
}

// Hotspot requests are always trusted. Otherwise nothing is allowed until
// a username and password exist; after that a session or the key will do.
static bool isAuthed(AsyncWebServerRequest* req) {
  if (viaHotspot(req)) return true;
  if (!authConfigured()) return false;
  return sessionAuthed(req) || bearerAuthed(req);
}

// Returns true if the request is authorised. On failure it has already
// sent a 401, so callers just return.
static bool requireAuth(AsyncWebServerRequest* req) {
  if (isAuthed(req)) return true;
  sendError(req, 401, authConfigured() ? "unauthorized" : "create a username and password first");
  return false;
}

static void sendJsonWithCookie(AsyncWebServerRequest* req, int code, const JsonDocument& doc, const String& cookie) {
  String out;
  serializeJson(doc, out);
  AsyncWebServerResponse* r = req->beginResponse(code, "application/json", out);
  r->addHeader("Set-Cookie", cookie);
  req->send(r);
}

static String sessionCookie(const char* sid) {
  // A week of idle time, matching the session table. HttpOnly keeps it
  // away from page scripts; SameSite=Strict keeps other sites from riding it.
  return String("sid=") + sid + "; Path=/; Max-Age=604800; HttpOnly; SameSite=Strict";
}

static const char* CLEAR_COOKIE = "sid=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict";

static void fillWidgetList(JsonDocument& doc) {
  doc["active"] = screens.getActive();
  JsonArray list = doc["widgets"].to<JsonArray>();
  for (uint8_t i = 0; i < screens.getCount(); i++) {
    Widget* w = screens.get(i);
    const char* name = w->name();
    JsonObject o = list.add<JsonObject>();
    o["name"]    = (name && *name) ? name : "Unknown";
    o["key"]     = w->key();
    o["builtin"] = builtinExists(w->key());
  }
  builtinsHiddenToJson(doc["hidden"].to<JsonArray>());
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

  // Same revalidation for the stylesheet: a cached copy from the previous
  // firmware would otherwise leave new page elements unstyled for an hour.
  server.on(AsyncURIMatcher::exact("/style.css"), HTTP_GET, [](AsyncWebServerRequest* req) {
    char etag[48];
    snprintf(etag, sizeof(etag), "\"css-%s-%u\"", FW_VERSION, (unsigned)strlen(STYLE_CSS));
    if (req->hasHeader("If-None-Match") && req->getHeader("If-None-Match")->value() == etag) {
      AsyncWebServerResponse* r = req->beginResponse(304);
      r->addHeader("ETag", etag);
      req->send(r);
      return;
    }
    AsyncWebServerResponse* r = req->beginResponse(200, "text/css", (const uint8_t*)STYLE_CSS, strlen(STYLE_CSS));
    r->addHeader("Cache-Control", "no-cache");
    r->addHeader("ETag", etag);
    req->send(r);
  });

  server.on(AsyncURIMatcher::exact("/setup"), HTTP_GET, [](AsyncWebServerRequest* req) {
    // From flash, no copy: the pages are far larger than the free heap allows to duplicate.
    req->send(req->beginResponse(200, "text/html", (const uint8_t*)SETUP_HTML, strlen(SETUP_HTML)));
  });

  server.on(AsyncURIMatcher::exact("/widgets"), HTTP_GET, [](AsyncWebServerRequest* req) {
    // From flash, no copy: the pages are far larger than the free heap allows to duplicate.
    req->send(req->beginResponse(200, "text/html", (const uint8_t*)WIDGETS_HTML, strlen(WIDGETS_HTML)));
  });

  server.on(AsyncURIMatcher::exact("/designer.js"), HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200, "application/javascript", (const uint8_t*)DESIGNER_JS, strlen(DESIGNER_JS));
    r->addHeader("Cache-Control", "no-cache");
    req->send(r);
  });

  server.on(AsyncURIMatcher::exact("/editor"), HTTP_GET, [](AsyncWebServerRequest* req) {
    // From flash, no copy: the pages are far larger than the free heap allows to duplicate.
    req->send(req->beginResponse(200, "text/html", (const uint8_t*)EDITOR_HTML, strlen(EDITOR_HTML)));
  });

  server.on(AsyncURIMatcher::exact("/terminal"), HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(req->beginResponse(200, "text/html", (const uint8_t*)TERMINAL_HTML, strlen(TERMINAL_HTML)));
  });

  server.on(AsyncURIMatcher::exact("/login"), HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(req->beginResponse(200, "text/html", (const uint8_t*)LOGIN_HTML, strlen(LOGIN_HTML)));
  });

  server.on(AsyncURIMatcher::exact("/"), HTTP_GET, [](AsyncWebServerRequest* req) {
    req->redirect(viaHotspot(req) ? "/setup" : "/widgets");
  });
}

// =====================
// AUTH
// =====================
static void fillAuthState(AsyncWebServerRequest* req, JsonDocument& doc) {
  doc["configured"] = authConfigured();
  doc["loggedIn"]   = isAuthed(req);
  doc["hotspot"]    = viaHotspot(req);
  doc["user"]       = authConfigured() ? authUsername() : "";
  doc["lockedFor"]  = authLockedFor();
}

static void registerAuth() {
  // GET /api/auth -> whether an account exists and whether this browser is logged in.
  server.on(AsyncURIMatcher::exact("/api/auth"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    fillAuthState(req, doc);
    sendJson(req, 200, doc);
  });

  // POST /api/auth/login {"user","pass"} -> session cookie.
  auto* login = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/auth/login"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!authConfigured()) { sendError(req, 409, "no account yet: create one with the API key"); return; }
    uint32_t wait = authLockedFor();
    if (wait) { sendError(req, 429, "too many attempts, try again in a minute"); return; }
    const char* user = json["user"] | "";
    const char* pass = json["pass"] | "";
    if (!authCheckPassword(user, pass)) {
      authNoteFailure();
      sendError(req, 401, "wrong username or password");
      return;
    }
    authNoteSuccess();
    JsonDocument doc;
    doc["ok"] = true;
    doc["user"] = authUsername();
    sendJsonWithCookie(req, 200, doc, sessionCookie(authSessionCreate()));
  });
  login->setMethod(HTTP_POST);
  server.addHandler(login);

  // POST /api/auth/logout -> drops this browser's session.
  server.on(AsyncURIMatcher::exact("/api/auth/logout"), HTTP_POST, [](AsyncWebServerRequest* req) {
    auto* c = req->getHeader("Cookie");
    char sid[AUTH_SID_LEN + 1];
    if (c && authSidFromCookie(c->value(), sid, sizeof(sid))) authSessionDrop(sid);
    JsonDocument doc;
    doc["ok"] = true;
    sendJsonWithCookie(req, 200, doc, CLEAR_COOKIE);
  });

  // POST /api/auth/setup {"key","user","pass"} -> creates the account, or
  // replaces it when one exists (forgot password). The key on the device
  // screen is the proof. A reset also rotates the key and logs everyone out.
  auto* setup = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/auth/setup"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    uint32_t wait = authLockedFor();
    if (wait) { sendError(req, 429, "too many attempts, try again in a minute"); return; }
    const char* key = json["key"] | "";
    if (!viaHotspot(req) && !authCheckKey(key)) {
      authNoteFailure();
      sendError(req, 401, "wrong API key");
      return;
    }
    authNoteSuccess();
    bool reset = authConfigured();
    char err[96];
    if (!authSetAccount(json["user"] | "", json["pass"] | "", err, sizeof(err))) { sendError(req, 400, err); return; }
    if (reset) regenerateApiToken();
    authRevealKey(0);
    JsonDocument doc;
    doc["ok"] = true;
    doc["user"] = authUsername();
    doc["keyChanged"] = reset;
    sendJsonWithCookie(req, 200, doc, sessionCookie(authSessionCreate()));
  });
  setup->setMethod(HTTP_POST);
  server.addHandler(setup);

  // POST /api/auth/reveal -> shows the API key on the device screen for a
  // minute. Deliberately open: it only helps someone who can see the screen.
  server.on(AsyncURIMatcher::exact("/api/auth/reveal"), HTTP_POST, [](AsyncWebServerRequest* req) {
    authRevealKey(60000);
    JsonDocument doc;
    doc["ok"] = true;
    doc["seconds"] = 60;
    sendJson(req, 200, doc);
  });

  // GET /api/auth/key -> the API key, for scripts. Needs a login.
  server.on(AsyncURIMatcher::exact("/api/auth/key"), HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    doc["key"] = settings.apiToken;
    sendJson(req, 200, doc);
  });

  // PUT /api/auth/password {"current","pass"} -> change the password while logged in.
  auto* pw = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/auth/password"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    uint32_t wait = authLockedFor();
    if (wait) { sendError(req, 429, "too many attempts, try again in a minute"); return; }
    if (!viaHotspot(req) && !authCheckPassword(authUsername(), json["current"] | "")) {
      authNoteFailure();
      sendError(req, 401, "current password is wrong");
      return;
    }
    authNoteSuccess();
    const char* user = json["user"].isNull() ? authUsername() : json["user"].as<const char*>();
    char err[96];
    if (!authSetAccount(user, json["pass"] | "", err, sizeof(err))) { sendError(req, 400, err); return; }
    JsonDocument doc;
    doc["ok"] = true;
    doc["user"] = authUsername();
    sendJsonWithCookie(req, 200, doc, sessionCookie(authSessionCreate()));
  });
  pw->setMethod(HTTP_PUT);
  server.addHandler(pw);
}

static void registerStatus() {
  server.on(AsyncURIMatcher::exact("/api/status"), HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["firmware"]     = FW_VERSION;
    doc["uptimeMs"]     = millis();
    doc["freeHeap"]     = ESP.getFreeHeap();
    doc["freePsram"]    = ESP.getFreePsram();
    // Memory and storage, for the terminal page's resource strip. minHeap
    // is the low-water mark since boot: a small value there points at
    // what crashed the device even when the live number looks fine.
    JsonObject mem = doc["memory"].to<JsonObject>();
    mem["heapTotal"]  = ESP.getHeapSize();
    mem["heapFree"]   = ESP.getFreeHeap();
    mem["heapMin"]    = ESP.getMinFreeHeap();
    mem["heapBlock"]  = ESP.getMaxAllocHeap();
    mem["psramTotal"] = ESP.getPsramSize();
    mem["psramFree"]  = ESP.getFreePsram();
    mem["fsTotal"]    = LittleFS.totalBytes();
    mem["fsUsed"]     = LittleFS.usedBytes();
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

  // DELETE /api/widgets?key=<key>: a built-in is hidden (restorable), a
  // layout is deleted from the filesystem like DELETE /api/layouts.
  server.on(AsyncURIMatcher::exact("/api/widgets"), HTTP_DELETE, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("key")) { sendError(req, 400, "missing key"); return; }
    String key = req->getParam("key")->value();
    char err[96];
    bool ok = builtinExists(key.c_str()) ? builtinHide(key.c_str(), err, sizeof(err))
                                         : layoutDelete(key.c_str(), err, sizeof(err));
    if (!ok) { sendError(req, 400, err); return; }
    JsonDocument doc;
    fillWidgetList(doc);
    sendJson(req, 200, doc);
  });

  // PUT /api/widgets/order {"order":["key",...]}: keys listed come first in
  // that order, anything missing keeps its place after them.
  auto* order = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/widgets/order"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonArrayConst arr = json["order"].as<JsonArrayConst>();
    if (arr.isNull()) { sendError(req, 400, "missing order array"); return; }

    const char* keys[MAX_WIDGET_ORDER];
    uint8_t n = 0;
    for (JsonVariantConst v : arr) {
      const char* k = v | "";
      if (!*k || strlen(k) > WIDGET_KEY_LEN) { sendError(req, 400, "bad key"); return; }
      if (n >= MAX_WIDGET_ORDER) break;
      keys[n++] = k;
    }
    screens.applyOrder(keys, n);

    // Save the resulting full order, not just what was sent.
    settings.orderCount = 0;
    for (uint8_t i = 0; i < screens.getCount() && i < MAX_WIDGET_ORDER; i++) {
      strlcpy(settings.widgetOrder[settings.orderCount++], screens.get(i)->key(), sizeof(settings.widgetOrder[0]));
    }
    settings.activeWidget = screens.getActive();
    if (!saveSettings()) { sendError(req, 500, "failed to save settings"); return; }
    JsonDocument doc;
    fillWidgetList(doc);
    sendJson(req, 200, doc);
  });
  order->setMethod(HTTP_PUT);
  server.addHandler(order);

  server.on(AsyncURIMatcher::exact("/api/widgets/restore"), HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!req->hasParam("key", true)) { sendError(req, 400, "missing key"); return; }
    String key = req->getParam("key", true)->value();
    char err[96];
    if (!builtinRestore(key.c_str(), err, sizeof(err))) { sendError(req, 400, err); return; }
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

  // POST /api/sources/discover {"url","header":{"name","value"},"id"} ->
  // fetch the URL once and list its JSON paths. With an id and no header
  // value, the stored header of that source is used. Poll the GET for the result.
  auto* discover = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/api/sources/discover"), [](AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    if (wifiState != WifiState::CONNECTED) { sendError(req, 409, "not connected to WiFi"); return; }
    const char* url = json["url"] | "";
    const char* hName = json["header"]["name"] | "";
    const char* hValue = json["header"]["value"] | "";
    char storedValue[SOURCE_HDR_VALUE_LEN + 1] = "";
    if (hName[0] && !hValue[0] && !json["id"].isNull()) {
      sourcesLock();
      DataSource* s = sourceFind(json["id"] | "");
      if (s && !strcmp(s->headerName, hName)) strlcpy(storedValue, s->headerValue, sizeof(storedValue));
      sourcesUnlock();
      hValue = storedValue;
    }
    char err[96];
    if (!sourceDiscoverStart(url, hName, hValue, err, sizeof(err))) { sendError(req, 400, err); return; }
    JsonDocument doc;
    doc["ok"] = true;
    sendJson(req, 202, doc);
  });
  discover->setMethod(HTTP_POST);
  server.addHandler(discover);

  // GET /api/sources/discover -> state and, once done, the keys.
  server.on(AsyncURIMatcher::exact("/api/sources/discover"), HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    sourceDiscoverToJson(doc.to<JsonObject>());
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

static bool uploadAuthed(AsyncWebServerRequest* req) { return isAuthed(req); }

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
        if (!uploadAuthed(req)) { fontUploadFail(u, "unauthorized"); return; }

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
  imageDecodedToJson(doc["decoded"].to<JsonArray>());
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

// =====================
// SCREENSHOT
// =====================
lgfx::LGFX_Sprite* uiSprite();

// The screen as a 24-bit BMP, built into a PSRAM buffer on request.
static uint8_t* shotBuf = nullptr;
static size_t shotLen = 0;

static bool buildScreenshot() {
  lgfx::LGFX_Sprite* ui = uiSprite();
  if (!ui) return false;
  const int w = ui->width(), h = ui->height();
  const size_t rowBytes = ((size_t)w * 3 + 3) & ~3;
  const size_t total = 54 + rowBytes * h;
  if (!shotBuf) shotBuf = (uint8_t*)heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
  if (!shotBuf) return false;
  memset(shotBuf, 0, 54);
  uint8_t* b = shotBuf;
  b[0] = 'B'; b[1] = 'M';
  auto put32 = [&](size_t off, uint32_t v) { b[off] = v; b[off + 1] = v >> 8; b[off + 2] = v >> 16; b[off + 3] = v >> 24; };
  put32(2, total); put32(10, 54); put32(14, 40); put32(18, w); put32(22, h);
  b[26] = 1; b[28] = 24; put32(34, rowBytes * h);
  static lgfx::rgb888_t row[320];
  for (int y = 0; y < h; y++) {
    ui->readRect(0, y, w, 1, row);
    uint8_t* dst = shotBuf + 54 + (size_t)(h - 1 - y) * rowBytes;
    for (int x = 0; x < w; x++) { dst[x * 3] = row[x].b; dst[x * 3 + 1] = row[x].g; dst[x * 3 + 2] = row[x].r; }
  }
  shotLen = total;
  return true;
}

static void registerScreenshot() {
  server.on(AsyncURIMatcher::exact("/api/screenshot"), HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (!buildScreenshot()) { sendError(req, 500, "screenshot failed"); return; }
    AsyncWebServerResponse* r = req->beginResponse("image/bmp", shotLen, [](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
      if (index >= shotLen) return 0;
      size_t n = shotLen - index;
      if (n > maxLen) n = maxLen;
      memcpy(buf, shotBuf + index, n);
      return n;
    });
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });
}

// =====================
// OTA FIRMWARE UPDATE
// =====================
// POST /api/system/update  multipart upload of firmware.bin (the app image
// PlatformIO builds), written to the other OTA slot; the device reboots
// into it on success.
struct OtaUpload {
  bool ok;
  bool started;
  size_t written;
  char err[80];
};

static void registerUpdate() {
  server.on(AsyncURIMatcher::exact("/api/system/update"), HTTP_POST,
    [](AsyncWebServerRequest* req) {
      OtaUpload* u = (OtaUpload*)req->_tempObject;
      if (!u) { sendError(req, 400, "no file uploaded"); return; }
      bool ok = u->ok && u->started;
      char err[80];
      strlcpy(err, u->err, sizeof(err));
      size_t written = u->written;
      delete u;
      req->_tempObject = nullptr;
      if (!ok) {
        if (Update.isRunning()) Update.abort();
        sendError(req, !strcmp(err, "unauthorized") ? 401 : 400, err[0] ? err : "update failed");
        return;
      }
      if (!Update.end(true)) {
        sendError(req, 500, Update.errorString());
        return;
      }
      JsonDocument doc;
      doc["ok"] = true;
      doc["written"] = written;
      doc["rebooting"] = true;
      sendJson(req, 200, doc);
      Log.printf("[OTA] Update written (%u bytes), rebooting\n", (unsigned)written);
      schedule(PendingAction::REBOOT, 800);
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
      OtaUpload* u = (OtaUpload*)req->_tempObject;
      if (index == 0) {
        u = new OtaUpload();
        u->ok = true;
        u->started = false;
        u->written = 0;
        u->err[0] = '\0';
        req->_tempObject = u;
        if (!uploadAuthed(req)) { u->ok = false; strlcpy(u->err, "unauthorized", sizeof(u->err)); return; }
        // The app image starts with the ESP image magic byte.
        if (len < 1 || data[0] != 0xE9) { u->ok = false; strlcpy(u->err, "not an ESP32 app image (expected firmware.bin)", sizeof(u->err)); return; }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { u->ok = false; strlcpy(u->err, Update.errorString(), sizeof(u->err)); return; }
        u->started = true;
        Log.printf("[OTA] Receiving %s\n", filename.c_str());
      }
      if (!u || !u->ok || !u->started) return;
      if (len && Update.write(data, len) != len) {
        u->ok = false;
        strlcpy(u->err, Update.errorString(), sizeof(u->err));
        Update.abort();
        return;
      }
      u->written += len;
      (void)final;
    });
}

// GET /api/log?since=N -> the output written after byte N as text/plain,
// with X-Log-Seq giving the value to pass next time. since=0 (or one the
// buffer no longer holds) returns everything still kept.
static void registerLog() {
  server.on(AsyncURIMatcher::exact("/api/log"), HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    uint32_t since = req->hasParam("since") ? strtoul(req->getParam("since")->value().c_str(), nullptr, 10) : 0;
    const size_t CHUNK = 8192;
    char* buf = (char*)malloc(CHUNK);
    if (!buf) { sendError(req, 500, "out of memory"); return; }
    uint32_t next = 0;
    size_t n = Log.read(since, buf, CHUNK, &next);
    // The response is sent after this handler returns, so it takes a copy.
    String body;
    body.concat(buf, n);
    free(buf);
    AsyncWebServerResponse* res = req->beginResponse(200, "text/plain; charset=utf-8", body);
    res->addHeader("X-Log-Seq", String(next));
    res->addHeader("Cache-Control", "no-store");
    req->send(res);
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
  registerAuth();
  registerStatus();
  registerWidgets();
  registerLayouts();
  registerWifi();
  registerLog();
  registerConfig();
  registerSources();
  registerFonts();
  registerImages();
  registerScreenshot();
  registerUpdate();
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
  Log.println("[WEB] Server started");
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
  Log.println("[SYS] Restarting");
  delay(100);
  ESP.restart();
}
