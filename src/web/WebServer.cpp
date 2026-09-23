#include "WebServer.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "../app/ScreenManager.h"
#include "Settings.h"

extern ScreenManager screens;

static AsyncWebServer server(80);

static const char* FW_VERSION = "0.2.0";

// =====================
// HTML: Widget Selector
// =====================
static const char WIDGETS_HTML[] = R"rawliteral(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Widget Selector</title>
<style>
  body { font-family: system-ui, sans-serif; background: #111; color: #eee; margin: 16px; }
  button { display: block; width: 100%; margin: 8px 0; padding: 12px; font-size: 16px;
           background: #222; color: #eee; border: 1px solid #444; border-radius: 6px; }
  button.active { border-color: #0f0; }
  input { width: 100%; padding: 10px; font-size: 14px; background: #222; color: #eee;
          border: 1px solid #444; border-radius: 6px; box-sizing: border-box; }
  label { font-size: 12px; color: #999; }
  #msg { color: #f66; min-height: 1.2em; }
</style>
</head>
<body>
<h2>Widget Selector</h2>
<label for="token">API token (shown on the device screen after boot)</label>
<input id="token" placeholder="paste token">
<p id="msg"></p>
<div id="list"></div>

<script>
const tokenEl = document.getElementById('token');
const msgEl = document.getElementById('msg');
try { tokenEl.value = localStorage.getItem('apiToken') || ''; } catch (e) {}
tokenEl.addEventListener('change', () => {
  try { localStorage.setItem('apiToken', tokenEl.value.trim()); } catch (e) {}
});

async function load() {
  msgEl.textContent = '';
  try {
    const r = await fetch('/api/widgets');
    const j = await r.json();
    const list = document.getElementById('list');
    list.innerHTML = '';
    j.widgets.forEach((w, i) => {
      const b = document.createElement('button');
      b.textContent = w + (i === j.active ? '  (active)' : '');
      if (i === j.active) b.className = 'active';
      b.onclick = () => activate(i);
      list.appendChild(b);
    });
  } catch (e) {
    msgEl.textContent = 'Failed to load widgets';
  }
}

async function activate(i) {
  const r = await fetch('/api/widgets', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
      'Authorization': 'Bearer ' + tokenEl.value.trim()
    },
    body: 'index=' + i
  });
  if (r.status === 401) {
    msgEl.textContent = 'Unauthorized. Check the API token.';
    return;
  }
  load();
}

load();
</script>
</body>
</html>
)rawliteral";

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

// Returns true if the request carries a valid bearer token.
// On failure it has already sent a 401, so callers just return.
static bool requireAuth(AsyncWebServerRequest* req) {
  auto* h = req->getHeader("Authorization");
  if (h) {
    const String& v = h->value();
    if (v.startsWith("Bearer ") && v.substring(7) == settings.apiToken) return true;
  }
  sendError(req, 401, "unauthorized");
  return false;
}

static void sendWidgetList(AsyncWebServerRequest* req) {
  JsonDocument doc;
  doc["active"] = screens.getActive();
  JsonArray list = doc["widgets"].to<JsonArray>();
  for (uint8_t i = 0; i < screens.getCount(); i++) {
    const char* name = screens.getName(i);
    list.add((name && *name) ? name : "Unknown");
  }
  sendJson(req, 200, doc);
}

// =====================
// SERVER START
// =====================
void startWebServer() {
  static bool started = false;
  if (started) return;
  started = true;

  Serial.println("[WEB] Registering routes");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "ESP32 Desktop Widget OK");
  });

  server.on("/widgets", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", WIDGETS_HTML);
  });

  // ---- Status (no auth; read-only) ----
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["firmware"]     = FW_VERSION;
    doc["uptimeMs"]     = millis();
    doc["freeHeap"]     = ESP.getFreeHeap();
    doc["freePsram"]    = ESP.getFreePsram();
    doc["apMode"]       = (WiFi.getMode() & WIFI_AP) != 0;
    doc["ssid"]         = WiFi.SSID();
    doc["rssi"]         = WiFi.RSSI();
    doc["ip"]           = WiFi.localIP().toString();
    doc["activeWidget"] = screens.getActive();
    doc["widgetCount"]  = screens.getCount();
    sendJson(req, 200, doc);
  });

  // ---- Widgets ----
  server.on("/api/widgets", HTTP_GET, [](AsyncWebServerRequest* req) {
    sendWidgetList(req);
  });

  server.on("/api/widgets", HTTP_POST, [](AsyncWebServerRequest* req) {
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

    Serial.printf("[WEB] Switching widget to index %d\n", idx);
    screens.setActive(idx);
    settings.activeWidget = idx;
    if (!saveSettings()) {
      sendError(req, 500, "failed to save settings");
      return;
    }

    sendWidgetList(req);
  });

  server.onNotFound([](AsyncWebServerRequest* req) {
    sendError(req, 404, "not found");
  });

  server.begin();
  Serial.println("[WEB] Server started");
}
