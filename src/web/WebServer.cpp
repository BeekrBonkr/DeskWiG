#include "WebServer.h"

#include <ESPAsyncWebServer.h>
#include "../app/ScreenManager.h"
#include "Settings.h"

extern ScreenManager screens;

AsyncWebServer server(80);

// =====================
// HTML: Widget Selector
// =====================
static const char WIDGETS_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<body>
<h2>Widget Selector</h2>
<ul id="list"></ul>

<script>
async function load() {
  try {
    const r = await fetch('/api/widgets');
    const j = await r.json();

    const ul = document.getElementById('list');
    ul.innerHTML = '';

    j.widgets.forEach((w, i) => {
      const b = document.createElement('button');
      b.textContent = w + (i === j.active ? ' (active)' : '');
      b.onclick = () =>
        fetch('/api/widgets', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: 'index=' + i
        }).then(load);

      ul.appendChild(b);
      ul.appendChild(document.createElement('br'));
    });
  } catch (e) {
    document.body.innerHTML += '<p style="color:red">Failed to load widgets</p>';
  }
}

load();
</script>
</body>
</html>
)rawliteral";

// =====================
// SERVER START
// =====================
void startWebServer() {
  static bool started = false;
  if (started) {
    Serial.println("[WEB] startWebServer() called again — ignored");
    return;
  }
  started = true;

  Serial.println("[WEB] Registering routes");

  // Root
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "ESP32 Status Panel OK");
  });

  // Widget selector page
  server.on("/widgets", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", WIDGETS_HTML);
  });

  // =====================
  // API: widgets (GET)
  // =====================
  server.on("/api/widgets", HTTP_GET, [](AsyncWebServerRequest* req) {
    uint8_t count = screens.getCount();

    Serial.print("[WEB] GET /api/widgets count=");
    Serial.println(count);

    String json = "{";
    json += "\"active\":" + String(screens.getActive()) + ",";
    json += "\"widgets\":[";

    for (uint8_t i = 0; i < count; i++) {
      const char* name = screens.getName(i);
      if (!name || !*name) name = "Unknown";

      json += "\"";
      json += name;
      json += "\"";

      if (i + 1 < count) json += ",";
    }

    json += "]}";

    req->send(200, "application/json", json);
  });

  // =====================
  // API: widgets (POST)
  // =====================
  server.on(
    "/api/widgets",
    HTTP_POST,
    [](AsyncWebServerRequest* req) {
      // required but unused
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      String body;
      for (size_t i = 0; i < len; i++) body += (char)data[i];

      Serial.print("[WEB] POST /api/widgets body: ");
      Serial.println(body);

      int pos = body.indexOf("index=");
      if (pos >= 0) {
        uint8_t idx = body.substring(pos + 6).toInt();

        Serial.print("[WEB] Switching widget to index ");
        Serial.println(idx);

        screens.setActive(idx);
        settings.activeWidget = idx;
        saveSettings();
      }

      req->send(200, "text/plain", "OK");
    }
  );

  // 404
  server.onNotFound([](AsyncWebServerRequest* req) {
    Serial.print("[WEB] 404: ");
    Serial.println(req->url());
    req->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("[WEB] Server started");
}
