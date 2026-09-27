#include "SystemScreens.h"

#include <WiFi.h>
#include "../web/Settings.h"

static const uint16_t C_TEXT = TFT_WHITE;
static const uint16_t C_DIM  = 0x39E7;
static const uint16_t C_OK   = 0x07E0;
static const uint16_t C_WARN = 0xFD20;
static const uint16_t C_BAD  = 0xF800;

static const int MARGIN = 6;
static const int TITLE_Y = 12;
static const int BODY_Y = 44;

static void title(lgfx::LGFX_Sprite& ui, const char* text, uint16_t color = C_TEXT) {
  ui.fillScreen(TFT_BLACK);
  ui.setTextDatum(top_left);
  ui.setTextSize(2);
  ui.setTextColor(color, TFT_BLACK);
  ui.setCursor(MARGIN, TITLE_Y);
  ui.println(text);
  ui.setTextSize(1);
  ui.setTextColor(C_TEXT, TFT_BLACK);
  ui.setCursor(MARGIN, BODY_Y);
}

static void footer(lgfx::LGFX_Sprite& ui, const char* text) {
  ui.setTextSize(1);
  ui.setTextColor(C_DIM, TFT_BLACK);
  ui.setCursor(MARGIN, ui.height() - 14);
  ui.print(text);
}

static void line(lgfx::LGFX_Sprite& ui, const char* text, uint16_t color = C_TEXT) {
  ui.setTextColor(color, TFT_BLACK);
  ui.setCursor(MARGIN, ui.getCursorY());
  ui.println(text);
}

static void blank(lgfx::LGFX_Sprite& ui) {
  ui.println("");
}

// The API key at double size so it can be read from across a desk.
static void keyLine(lgfx::LGFX_Sprite& ui) {
  ui.setTextSize(2);
  ui.setTextColor(C_TEXT, TFT_BLACK);
  ui.setCursor(MARGIN, ui.getCursorY());
  ui.println(settings.apiToken);
  ui.setTextSize(1);
}

// =====================
// SETUP HOTSPOT
// =====================
void drawApScreen(lgfx::LGFX_Sprite& ui, ApReason reason, const char* connectingTo) {
  title(ui, "Setup");

  switch (reason) {
    case ApReason::NO_CREDENTIALS:
      line(ui, "No WiFi network saved.", C_DIM);
      break;
    case ApReason::CONNECT_FAILED:
      line(ui, "Couldn't connect to:", C_WARN);
      line(ui, settings.wifiSSID);
      break;
    case ApReason::FORCED:
      line(ui, "Recovery jumper set.", C_WARN);
      break;
    default:
      break;
  }
  blank(ui);

  line(ui, "1. Join this WiFi:", C_DIM);
  ui.setTextSize(2);
  line(ui, wifiApSsid());
  ui.setTextSize(1);
  ui.print("   password: ");
  ui.println(wifiApPassword());
  blank(ui);

  line(ui, "2. A setup page should", C_DIM);
  line(ui, "   open. If not, go to:", C_DIM);
  ui.print("   http://");
  ui.println(WiFi.softAPIP());

  if (connectingTo && *connectingTo) {
    blank(ui);
    ui.setTextColor(C_WARN, TFT_BLACK);
    ui.print("Trying ");
    ui.print(connectingTo);
    ui.println("...");
  }

  footer(ui, "Setup hotspot");
}

// =====================
// CONNECTING (first boot only)
// =====================
void drawConnectingScreen(lgfx::LGFX_Sprite& ui, const char* ssid) {
  title(ui, "Connecting");
  line(ui, "Joining WiFi network:", C_DIM);
  line(ui, ssid);
  blank(ui);

  static const char spin[4] = { '|', '/', '-', '\\' };
  ui.setTextColor(C_DIM, TFT_BLACK);
  ui.print(spin[(millis() / 150) & 3]);
  ui.println(" please wait");

  footer(ui, "Setup hotspot starts after 15s");
}

// =====================
// CONNECTION INFO
// =====================
void drawInfoScreen(lgfx::LGFX_Sprite& ui, uint32_t secondsLeft) {
  title(ui, "Connected", C_OK);

  line(ui, settings.wifiSSID);
  blank(ui);

  line(ui, "Open in a browser:", C_DIM);
  ui.print("http://");
  ui.print(settings.hostname);
  ui.println(".local");
  ui.print("http://");
  ui.println(WiFi.localIP());
  blank(ui);

  line(ui, "API key:", C_DIM);
  keyLine(ui);
  blank(ui);
  line(ui, "Enter it on the login page", C_DIM);
  line(ui, "to create your account.", C_DIM);

  char buf[32];
  snprintf(buf, sizeof(buf), "Widgets start in %lus", (unsigned long)secondsLeft);
  footer(ui, buf);
}

// =====================
// API KEY (forgot password)
// =====================
void drawKeyScreen(lgfx::LGFX_Sprite& ui, uint32_t secondsLeft) {
  title(ui, "API key", C_WARN);

  line(ui, "Requested from the login", C_DIM);
  line(ui, "page to reset the password.", C_DIM);
  blank(ui);
  keyLine(ui);
  blank(ui);
  line(ui, "Enter it in the browser", C_DIM);
  line(ui, "with a new username and", C_DIM);
  line(ui, "password. The key changes", C_DIM);
  line(ui, "after the reset.", C_DIM);

  char buf[32];
  snprintf(buf, sizeof(buf), "Screen clears in %lus", (unsigned long)secondsLeft);
  footer(ui, buf);
}

// =====================
// RECOVERY
// =====================
void drawRecoveryScreen(lgfx::LGFX_Sprite& ui, uint32_t msUntilReset) {
  title(ui, "Recovery", C_WARN);

  line(ui, "Release now to start", C_DIM);
  line(ui, "the setup hotspot.", C_DIM);
  blank(ui);
  line(ui, "Keep holding to erase", C_BAD);
  line(ui, "all settings.", C_BAD);
  blank(ui);

  ui.setTextSize(4);
  ui.setTextColor(C_BAD, TFT_BLACK);
  char buf[8];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)((msUntilReset + 999) / 1000));
  ui.setTextDatum(middle_center);
  ui.drawString(buf, ui.width() / 2, ui.height() / 2 + 30);
  ui.setTextDatum(top_left);
  ui.setTextSize(1);

  footer(ui, "Factory reset countdown");
}

void drawResettingScreen(lgfx::LGFX_Sprite& ui) {
  title(ui, "Resetting", C_BAD);
  line(ui, "Erasing all settings...", C_DIM);
  blank(ui);
  line(ui, "The device will restart", C_DIM);
  line(ui, "into setup mode.", C_DIM);
  footer(ui, "Do not power off");
}
