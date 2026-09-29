#include <Arduino.h>
#include <WiFi.h>
#include <LovyanGFX.hpp>

#include "app/Board.h"
#include "app/ScreenManager.h"
#include "app/StatusLed.h"
#include "app/SystemScreens.h"
#include "app/Encoder.h"

#include "app/Builtins.h"
#include "layout/LayoutStore.h"

#include "web/Settings.h"
#include "web/WebServer.h"
#include "web/Auth.h"
#include "net/WifiManager.h"
#include "net/TimeService.h"
#include "net/DataSource.h"
#include "layout/FontService.h"
#include "layout/ImageService.h"
#include "app/Log.h"

// How long the connection-info screen (IP + API key) stays up after WiFi connects.
constexpr uint32_t INFO_SCREEN_MS = 30000;

static uint32_t infoScreenUntil = 0;

// Rotary encoder: a brief name banner after a turn, and the widget choice
// saved a moment after the knob stops so a fast spin is one flash write.
constexpr uint32_t TOAST_MS = 1500;
constexpr uint32_t SAVE_DELAY_MS = 2000;
constexpr uint32_t INFO_BY_BUTTON_MS = 15000;
static uint32_t toastUntil = 0;
static uint32_t saveAt = 0;
static bool savePending = false;

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

// =====================
// RECOVERY JUMPER
// Returns true if the hotspot should be forced. Never returns if the
// jumper is held long enough for a factory reset.
// =====================
static bool checkRecoveryJumper() {
  pinMode(Board::RECOVERY_PIN, INPUT_PULLUP);
  delay(20);
  if (digitalRead(Board::RECOVERY_PIN) != LOW) return false;

  Log.println("[SYS] Recovery jumper detected");
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

  Log.println("[SYS] Jumper released, forcing setup hotspot");
  return true;
}

// =====================
// ROTARY ENCODER
// =====================
static void switchWidget(int8_t steps, uint32_t now) {
  uint8_t n = screens.getCount();
  if (!n) return;
  int idx = ((int)screens.getActive() + steps) % (int)n;
  if (idx < 0) idx += n;
  layoutPreviewStop();                    // the knob wins over an editor preview
  infoScreenUntil = now;                  // and over the info screen
  screens.setActive(idx);
  settings.activeWidget = idx;
  strlcpy(settings.activeWidgetName, screens.getName(idx), sizeof(settings.activeWidgetName));
  saveAt = now + SAVE_DELAY_MS;
  savePending = true;
  toastUntil = now + TOAST_MS;
}

// Name banner over the bottom of the widget after a turn.
static void drawToast() {
  const int h = 40, y = ui.height() - h - 8;
  ui.fillRoundRect(8, y, ui.width() - 16, h, 8, 0x2104);
  ui.drawRoundRect(8, y, ui.width() - 16, h, 8, 0x4208);
  char idx[12];
  snprintf(idx, sizeof(idx), "%u / %u", screens.getActive() + 1, screens.getCount());
  ui.setTextDatum(top_right);
  ui.setTextSize(1);
  ui.setTextColor(0x8410, 0x2104);
  ui.drawString(idx, ui.width() - 14, y + 5);
  char name[13];
  strlcpy(name, screens.getName(screens.getActive()), sizeof(name));   // 12 chars fit at size 2
  ui.setTextDatum(bottom_left);
  ui.setTextSize(2);
  ui.setTextColor(TFT_WHITE, 0x2104);
  ui.drawString(name, 16, y + h - 5);
  ui.setTextDatum(top_left);
  ui.setTextSize(1);
}

static void encoderStep(uint32_t now) {
  EncoderEvent ev = encoderLoop(now);
  if (ev.steps) switchWidget(ev.steps, now);
  if (ev.button == EncoderButton::CLICK) {
    // Toggle the connection-info screen (address and API key).
    bool showing = (int32_t)(infoScreenUntil - now) > 0;
    infoScreenUntil = showing ? now : now + INFO_BY_BUTTON_MS;
  }
  if (savePending && (int32_t)(now - saveAt) >= 0) {
    savePending = false;
    saveSettings();
  }
}

// =====================
// SETUP
// =====================
// The composed screen, for the screenshot endpoint.
lgfx::LGFX_Sprite* uiSprite() { return &ui; }

void setup() {
  Log.begin(115200);

  tft.init();
  tft.setColorDepth(16);
  tft.setRotation(0);
  tft.invertDisplay(true);

  ui.setColorDepth(16);
  ui.createSprite(tft.width(), tft.height());

  ledBegin();
  encoderBegin();

  bool forceAp = checkRecoveryJumper();

  loadSettings();
  authBegin();
  fontsBegin();
  imagesBegin();
  ledApplySettings();
  wifiBegin(forceAp);
  timeBegin();
  sourcesBegin();

  builtinsBegin(screens);           // Ping and Clock, unless deleted
  layoutsBegin(screens);            // JSON widgets from /widgets/*.json
  {
    const char* keys[MAX_WIDGET_ORDER];
    for (uint8_t i = 0; i < settings.orderCount; i++) keys[i] = settings.widgetOrder[i];
    screens.applyOrder(keys, settings.orderCount);
  }
  // Restore the active widget by name; the index is only a fallback for
  // configs written before names were stored.
  screens.setActive(settings.activeWidget);
  if (settings.activeWidgetName[0]) {
    for (uint8_t i = 0; i < screens.getCount(); i++) {
      if (!strcmp(screens.getName(i), settings.activeWidgetName)) { screens.setActive(i); break; }
    }
  }
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
  encoderStep(now);

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
  if (authKeyScreenActive()) {
    drawKeyScreen(ui, authKeyScreenSecondsLeft());
  } else if (wifiState == WifiState::CONNECTED && (int32_t)(infoScreenUntil - now) > 0) {
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
    if ((int32_t)(toastUntil - now) > 0) drawToast();
  }

  ui.pushSprite(0, 0);
}
