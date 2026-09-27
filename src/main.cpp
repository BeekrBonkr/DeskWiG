#include <Arduino.h>
#include <WiFi.h>
#include <LovyanGFX.hpp>

#include "app/Board.h"
#include "app/ScreenManager.h"
#include "app/StatusLed.h"
#include "app/SystemScreens.h"

#include "widgets/PingWidget.h"
#include "widgets/ClockWidget.h"
#include "layout/LayoutStore.h"

#include "web/Settings.h"
#include "web/WebServer.h"
#include "net/WifiManager.h"
#include "net/TimeService.h"
#include "net/DataSource.h"
#include "layout/FontService.h"
#include "layout/ImageService.h"

// How long the connection-info screen (IP + API token) stays up after WiFi connects.
constexpr uint32_t INFO_SCREEN_MS = 30000;

static uint32_t infoScreenUntil = 0;

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
    b.pin_sclk = Board::TFT_SCLK;
    b.pin_mosi = Board::TFT_MOSI;
    b.pin_dc   = Board::TFT_DC;
    _bus.config(b);
    _panel.setBus(&_bus);

    auto p = _panel.config();
    p.pin_cs = Board::TFT_CS;
    p.pin_rst = Board::TFT_RST;
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
// RECOVERY JUMPER
// Returns true if the hotspot should be forced. Never returns if the
// jumper is held long enough for a factory reset.
// =====================
static bool checkRecoveryJumper() {
  pinMode(Board::RECOVERY_PIN, INPUT_PULLUP);
  delay(20);
  if (digitalRead(Board::RECOVERY_PIN) != LOW) return false;

  Serial.println("[SYS] Recovery jumper detected");
  ledSet(LedPattern::AP_MODE);
  uint32_t start = millis();

  while (digitalRead(Board::RECOVERY_PIN) == LOW) {
    uint32_t held = millis() - start;
    if (held >= Board::RESET_HOLD_MS) {
      ledSet(LedPattern::RESETTING);
      ledLoop();
      drawResettingScreen(ui);
      ui.pushSprite(0, 0);
      factoryReset();
      delay(1000);
      ESP.restart();
    }
    drawRecoveryScreen(ui, Board::RESET_HOLD_MS - held);
    ui.pushSprite(0, 0);
    ledLoop();
    delay(50);
  }

  Serial.println("[SYS] Jumper released, forcing setup hotspot");
  return true;
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setColorDepth(16);
  tft.setRotation(0);
  tft.invertDisplay(true);

  ui.setColorDepth(16);
  ui.createSprite(tft.width(), tft.height());

  ledBegin();

  bool forceAp = checkRecoveryJumper();

  loadSettings();
  fontsBegin();
  imagesBegin();
  ledApplySettings();
  wifiBegin(forceAp);
  timeBegin();
  sourcesBegin();

  screens.add(&pingWidget);
  screens.add(&clockWidget);
  layoutsBegin(screens);            // JSON widgets from /widgets/*.json
  screens.setActive(settings.activeWidget);
  screens.begin();

  // The async server binds fine before the network is up.
  startWebServer();
}

// =====================
// LOOP
// =====================
void loop() {
  uint32_t now = millis();

  wifiLoop();
  timeLoop();
  webLoop();

  // Show the info screen each time we (re)connect.
  static WifiState lastState = WifiState::AP_MODE;
  if (wifiState == WifiState::CONNECTED && lastState != WifiState::CONNECTED) {
    infoScreenUntil = now + INFO_SCREEN_MS;
  }
  lastState = wifiState;

  // LED: system states first, otherwise whatever the widget on screen asks for.
  if (wifiState == WifiState::CONNECTED) {
    LedSpec spec;
    Widget* w = screens.current();
    ledSetWidget(w && w->ledSpec(spec) ? &spec : nullptr);
    ledSet(LedPattern::NORMAL);
  } else if (wifiApActive()) {
    ledSet(LedPattern::AP_MODE);
  } else {
    ledSet(LedPattern::CONNECTING);
  }
  ledLoop();

  // Screen
  if (wifiState == WifiState::CONNECTED && (int32_t)(infoScreenUntil - now) > 0) {
    drawInfoScreen(ui, (infoScreenUntil - now) / 1000);
  } else if (wifiApActive()) {
    drawApScreen(ui, apReason, wifiState == WifiState::CONNECTING ? settings.wifiSSID : nullptr);
  } else if (wifiState == WifiState::CONNECTING && !wifiEverConnected()) {
    drawConnectingScreen(ui, settings.wifiSSID);
  } else {
    // Normal operation. Widgets keep rendering through brief reconnects.
    layoutsLoop(now);
    screens.update(now);
    screens.render(ui);
  }

  ui.pushSprite(0, 0);
}
