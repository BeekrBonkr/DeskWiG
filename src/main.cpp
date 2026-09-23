#include <Arduino.h>
#include <WiFi.h>
#include <LovyanGFX.hpp>
#include <Adafruit_NeoPixel.h>

#include "app/ScreenManager.h"

#include "widgets/PingWidget.h"
#include "widgets/ClockWidget.h"

#include "web/Settings.h"
#include "web/WebServer.h"
#include "net/WifiManager.h"
bool webStarted = false;

// =====================
// STATUS LED
// =====================
constexpr uint8_t STATUS_LED_PIN = 48;
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

// =====================
// DISPLAY
// =====================
class LGFX_Display : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;
public:
  LGFX_Display() {
    auto b = _bus.config();
    b.spi_host = SPI2_HOST;
    b.pin_sclk = 12;
    b.pin_mosi = 11;
    b.pin_dc   = 9;
    _bus.config(b);
    _panel.setBus(&_bus);

    auto p = _panel.config();
    p.pin_cs = 10;
    p.pin_rst = 8;
    p.panel_width  = 170;
    p.panel_height = 320;
    p.offset_x = 35;
    p.rgb_order = false;
    _panel.config(p);

    setPanel(&_panel);
  }
};

LGFX_Display tft;
LGFX_Sprite ui(&tft);

// =====================
// WIDGETS / SCREENS
// =====================
ScreenManager screens;

PingWidget  pingWidget;
ClockWidget clockWidget;

// =====================
// STATUS LED UPDATE
// =====================
void updateStatusLed() {
  static uint32_t lastBlink = 0;
  static bool on = false;

  if (wifiState == WifiState::AP_MODE) {
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      on = !on;
      statusLed.setPixelColor(0, on ? statusLed.Color(0, 0, 255) : 0);
      statusLed.show();
    }
    return;
  }

  statusLed.setPixelColor(0, statusLed.Color(0, 255, 0));
  statusLed.show();
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);

  // Display init
  tft.init();
  tft.setColorDepth(16);
  tft.setRotation(0);
  tft.invertDisplay(true);

  ui.setColorDepth(16);
  ui.createSprite(tft.width(), tft.height());

  // LED
  statusLed.begin();
  statusLed.setBrightness(5);
  statusLed.clear();
  statusLed.show();

  // Settings + WiFi
  loadSettings();
  wifiBegin();

  // ✅ Register widgets FIRST
  screens.add(&pingWidget);
  screens.add(&clockWidget);

  screens.setActive(settings.activeWidget);
  screens.begin();

  // ✅ Start web server LAST
  startWebServer();

  Serial.println("[WEB] Server ready");
}

// =====================
// LOOP
// =====================
void loop() {
  wifiLoop();

  if (!webStarted) {
    if (wifiState == WifiState::CONNECTED || wifiState == WifiState::AP_MODE) {
      Serial.println("[WEB] Attempting to start web server...");
      startWebServer();
      webStarted = true;

      Serial.print("[WEB] WiFi mode: ");
      Serial.println((wifiState == WifiState::AP_MODE) ? "AP_MODE" : "STA_MODE");

      Serial.print("[WEB] STA IP: ");
      Serial.println(WiFi.localIP());

      Serial.print("[WEB] AP  IP: ");
      Serial.println(WiFi.softAPIP());

      Serial.println("[WEB] Web server started");
    }
  }

  updateStatusLed();

  // AP MODE SCREEN
  if (wifiState == WifiState::AP_MODE) {
    ui.fillScreen(TFT_BLACK);
    ui.setTextColor(TFT_WHITE, TFT_BLACK);
    ui.setCursor(10, 30);
    ui.println("Unable to connect to WiFi");
    ui.println("");
    ui.println("Connect to hotspot:");
    ui.println("ESP32-StatusPanel");
    ui.println("Password: configureme");
    ui.println("");
    ui.println("Open:");
    ui.println("http://192.168.4.1");
    ui.pushSprite(0, 0);
    return;
  }

  uint32_t now = millis();
  screens.update(now);
  screens.render(ui);
  ui.pushSprite(0, 0);
}
