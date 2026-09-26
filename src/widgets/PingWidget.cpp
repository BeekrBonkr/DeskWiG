#include "PingWidget.h"

#include <WiFi.h>

#include "../web/Settings.h"
#include "../net/PingService.h"

// =====================
// CONFIG
// =====================
constexpr uint32_t SPINNER_FRAME_MS   = 120;

// =====================
// UI CONSTANTS
// =====================
constexpr int TOP_PADDING = 12;
constexpr int MARGIN_L    = 6;
constexpr int MARGIN_R    = 6;
constexpr int LINE_H      = 14;

static const uint16_t COLOR_BG     = TFT_BLACK;
static const uint16_t COLOR_TEXT   = TFT_WHITE;
static const uint16_t COLOR_DIM    = 0x39E7;
static const uint16_t COLOR_GREEN  = 0x07E0;
static const uint16_t COLOR_ORANGE = 0xFD20;
static const uint16_t COLOR_RED    = 0xF800;

// =====================
// SPINNER
// =====================
static uint32_t lastSpinnerFrame = 0;
static uint8_t spinnerIndex = 0;
static const char spinnerChars[4] = { '|', '/', '-', '\\' };

static void drawSeparator(lgfx::LGFX_Sprite& ui, int y) {
  ui.drawFastHLine(
    MARGIN_L,
    y,
    ui.width() - (MARGIN_L + MARGIN_R),
    COLOR_DIM
  );
}

// =====================
// LIFECYCLE
// =====================
void PingWidget::begin() {}

void PingWidget::update(uint32_t now) {
  // spinner animation
  if (now - lastSpinnerFrame >= SPINNER_FRAME_MS) {
    lastSpinnerFrame = now;
    spinnerIndex = (spinnerIndex + 1) & 3;
  }

  pingLoop(now);
}

void PingWidget::render(lgfx::LGFX_Sprite& ui) {
  ui.fillScreen(COLOR_BG);
  ui.setTextSize(1);

  int y = TOP_PADDING;

  // header: SSID + RSSI bars
  ui.setTextColor(COLOR_TEXT, COLOR_BG);
  ui.setCursor(MARGIN_L, y);
  ui.print("  ");
  ui.print(WiFi.SSID());
  drawWifiStrength(ui, y);

  y += LINE_H;
  drawSeparator(ui, y);
  y += LINE_H;

  // SERVICES
  ui.setTextColor(COLOR_DIM, COLOR_BG);
  ui.setCursor(MARGIN_L, y);
  ui.print("SERVICES");
  y += LINE_H;

  for (uint8_t i = 0; i < settings.targetCount; i++) {
    if (settings.targets[i].type == TargetType::SERVICE) {
      drawRow(ui, y, settings.targets[i]);
      y += LINE_H;
    }
  }

  drawSeparator(ui, y);
  y += LINE_H;

  // SERVERS
  ui.setCursor(MARGIN_L, y);
  ui.print("SERVERS");
  y += LINE_H;

  for (uint8_t i = 0; i < settings.targetCount; i++) {
    if (settings.targets[i].type == TargetType::SERVER) {
      drawRow(ui, y, settings.targets[i]);
      y += LINE_H;
    }
  }

  drawIPAddress(ui);
}

// =====================
// UI HELPERS
// =====================
uint16_t PingWidget::latencyColor(int ms) {
  if (ms < 0) return COLOR_DIM;
  if (ms < 50) return COLOR_GREEN;
  return COLOR_ORANGE;
}

char PingWidget::trendArrow(const PingTarget& t) {
  if (t.lastLatency < 0 || t.latency < 0) return ' ';
  if (t.latency < t.lastLatency) return '^';
  if (t.latency > t.lastLatency) return 'v';
  return '>';
}

void PingWidget::drawSparkline(lgfx::LGFX_Sprite& ui, int x, int y, const PingTarget& t) {
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t idx = (t.historyPos + i) % 8;
    ui.setTextColor(t.history[idx] ? COLOR_GREEN : COLOR_DIM, COLOR_BG);
    ui.setCursor(x + i * 6, y);
    ui.print(t.history[idx] ? '|' : '.');
  }
}

void PingWidget::drawRow(lgfx::LGFX_Sprite& ui, int y, PingTarget& t) {
  const int sparkX = MARGIN_L;
  const int nameX  = sparkX + (8 * 6) + 6;
  const int valueW = ui.textWidth("0000ms >");
  const int valueX = ui.width() - MARGIN_R - valueW;

  drawSparkline(ui, sparkX, y, t);

  bool blink = (t.latency < 0 && t.failCount >= PING_MAX_FAILS);
  bool blinkOn = ((millis() / 500) % 2) == 0;

  ui.setTextColor((blink && blinkOn) ? COLOR_RED : COLOR_TEXT, COLOR_BG);
  ui.setCursor(nameX, y);
  ui.printf("%-12s", t.name);

  ui.setCursor(valueX, y);

  if (t.latency < 0) {
    if (t.failCount < PING_MAX_FAILS) {
      ui.printf("  %c   ", spinnerChars[spinnerIndex]);
    } else {
      ui.setTextColor(COLOR_RED, COLOR_BG);
      ui.print("  ######");
    }
    return;
  }

  ui.setTextColor(latencyColor(t.latency), COLOR_BG);
  ui.printf("%4dms ", t.latency);

  ui.setTextColor(COLOR_TEXT, COLOR_BG);
  ui.print(trendArrow(t));
}

void PingWidget::drawWifiStrength(lgfx::LGFX_Sprite& ui, int y) {
  int rssi = WiFi.RSSI();
  const char* bars = ".... ";
  uint16_t color = COLOR_DIM;

  if (rssi > -55)      { bars = "|||| "; color = COLOR_GREEN; }
  else if (rssi > -65) { bars = "|||. "; color = COLOR_GREEN; }
  else if (rssi > -75) { bars = "||.. "; color = COLOR_ORANGE; }
  else if (rssi > -85) { bars = "|... "; color = COLOR_ORANGE; }

  int x = ui.width() - MARGIN_R - ui.textWidth(bars);
  ui.setTextColor(color, COLOR_BG);
  ui.setCursor(x, y);
  ui.print(bars);
}

void PingWidget::drawIPAddress(lgfx::LGFX_Sprite& ui) {
  IPAddress ip = WiFi.localIP();
  ui.setTextColor(COLOR_DIM, COLOR_BG);
  ui.setCursor(MARGIN_L, ui.height() - LINE_H - 2);
  ui.printf("IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}
