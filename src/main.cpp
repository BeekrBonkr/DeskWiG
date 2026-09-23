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

// How long the connection-info screen (IP + API token) stays up after WiFi connects.
constexpr uint32_t INFO_SCREEN_MS = 30000;

static bool webStarted = false;
static uint32_t infoScreenUntil = 0;

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
// SYSTEM SCREENS
// =====================
static void drawApModeScreen() {
  ui.fillScreen(TFT_BLACK);
  ui.setTextColor(TFT_WHITE, TFT_BLACK);
  ui.setTextSize(1);
  ui.setCursor(10, 30);
  ui.println("Unable to connect to WiFi");
  ui.println("");
  ui.println("Connect to hotspot:");
  ui.println("ESP32-StatusPanel");
  ui.println("Password: configureme");
  ui.println("");
  ui.println("Open:");
  ui.println("http://192.168.4.1");
}

static void drawInfoScreen() {
  ui.fillScreen(TFT_BLACK);
  ui.setTextColor(TFT_WHITE, TFT_BLACK);

  ui.setTextSize(2);
  ui.setCursor(6, 12);
  ui.println("Connected");

  ui.setTextSize(1);
  ui.setCursor(6, 44);
  ui.println(WiFi.SSID());
  ui.println("");
  ui.println("Open in a browser:");
  ui.print("http://");
  ui.print(WiFi.localIP());
  ui.println("/widgets");
  ui.println("");
  ui.println("API token:");

  // 32 hex chars won't fit on one 28-column line; split in two.
  char half[17];
  strlcpy(half, settings.apiToken, sizeof(half));
  ui.println(half);
  strlcpy(half, settings.apiToken + 16, sizeof(half));
  ui.println(half);

  ui.setTextColor(0x39E7, TFT_BLACK);
  ui.setCursor(6, ui.height() - 14);
  uint32_t remaining = (infoScreenUntil - millis()) / 1000;
  ui.printf("Widgets start in %lus", (unsigned long)remaining);
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

  // Register widgets
  screens.add(&pingWidget);
  screens.add(&clockWidget);

  screens.setActive(settings.activeWidget);
  screens.begin();
}

// =====================
// LOOP
// =====================
void loop() {
  wifiLoop();

  // Start the web server once we have any network at all
  if (!webStarted && (wifiState == WifiState::CONNECTED || wifiState == WifiState::AP_MODE)) {
    startWebServer();
    webStarted = true;

    if (wifiState == WifiState::CONNECTED) {
      infoScreenUntil = millis() + INFO_SCREEN_MS;
      Serial.print("[WEB] STA IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.print("[WEB] AP IP: ");
      Serial.println(WiFi.softAPIP());
    }
  }

  updateStatusLed();

  if (wifiState == WifiState::AP_MODE) {
    drawApModeScreen();
    ui.pushSprite(0, 0);
    return;
  }

  if (wifiState == WifiState::CONNECTED && millis() < infoScreenUntil) {
    drawInfoScreen();
    ui.pushSprite(0, 0);
    return;
  }

  uint32_t now = millis();
  screens.update(now);
  screens.render(ui);
  ui.pushSprite(0, 0);
}
