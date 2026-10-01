#include "Menu.h"

#include <WiFi.h>
#include <time.h>
#include "Version.h"
#include "StatusLed.h"
#include "SystemScreens.h"
#include "Log.h"
#include "../web/Settings.h"
#include "../net/WifiManager.h"
#include "../net/TimeService.h"

void backlightSet(uint8_t pct);   // main.cpp, owns the display

// =====================
// LOOK
// =====================
static const uint16_t C_TEXT  = TFT_WHITE;
static const uint16_t C_LABEL = 0xC618;   // light grey, unselected rows
static const uint16_t C_DIM   = 0x8410;
static const uint16_t C_SEL   = 0x3186;   // highlight bar
static const uint16_t C_BOX   = 0x18C3;   // unselected button box
static const uint16_t C_OK    = 0x07E0;
static const uint16_t C_WARN  = 0xFD20;
static const uint16_t C_BAD   = 0xF800;

static const int MARGIN   = 6;
static const int TITLE_Y  = 12;
static const int LIST_Y   = 44;
static const int FOOTER_H = 18;
static const int ROW_PLAIN = 22;   // label only
static const int ROW_VALUE = 34;   // label plus a small value line
static const int ROW_EDIT  = 42;   // value being changed, shown large

// Built-in font: 6 px per character at size 1, 12 px at size 2. 170 px wide.
static const int VALUE_CHARS = 27;
static const int TEXT_COLS   = 13;

constexpr uint32_t IDLE_CLOSE_MS = 45000;

// =====================
// MENU DATA
// =====================
enum class ItemType : uint8_t { BACK, SUBMENU, ACTION, TOGGLE, NUMBER, CHOICE, TEXT, INFO };

struct Page;
struct Item {
  const char* label;
  ItemType type;
  const Page* sub;                        // SUBMENU
  void (*action)();                       // ACTION
  int (*get)();                           // TOGGLE / NUMBER / CHOICE
  void (*set)(int v, bool commit);        // called while turning (commit=false) and on click (true)
  int min, max, step;                     // NUMBER
  const char* unit;                       // NUMBER suffix
  const char* const* choices;             // CHOICE labels
  int nChoices;
  void (*fmt)(char* buf, size_t n);       // INFO value; TEXT current value
  const char* charset;                    // TEXT: characters offered
  const char* hint;                       // TEXT: one line under the title
  uint8_t textMax;                        // TEXT: length limit
  bool (*textSet)(const char* s);         // TEXT: store; false = rejected
};

struct Page {
  const char* title;
  const Item* items;
  uint8_t count;
};

static Item mk(const char* label, ItemType t) {
  Item i = {};
  i.label = label;
  i.type = t;
  i.unit = "";
  i.step = 1;
  return i;
}
static Item itemBack() { return mk("< Back", ItemType::BACK); }
static Item itemSub(const char* l, const Page* p) { Item i = mk(l, ItemType::SUBMENU); i.sub = p; return i; }
static Item itemAction(const char* l, void (*fn)()) { Item i = mk(l, ItemType::ACTION); i.action = fn; return i; }
static Item itemInfo(const char* l, void (*fmt)(char*, size_t)) { Item i = mk(l, ItemType::INFO); i.fmt = fmt; return i; }
static Item itemToggle(const char* l, int (*get)(), void (*set)(int, bool)) {
  Item i = mk(l, ItemType::TOGGLE); i.get = get; i.set = set; return i;
}
static Item itemNumber(const char* l, int (*get)(), void (*set)(int, bool), int min, int max, int step, const char* unit) {
  Item i = mk(l, ItemType::NUMBER); i.get = get; i.set = set; i.min = min; i.max = max; i.step = step; i.unit = unit; return i;
}
static Item itemChoice(const char* l, int (*get)(), void (*set)(int, bool), const char* const* choices, int n) {
  Item i = mk(l, ItemType::CHOICE); i.get = get; i.set = set; i.choices = choices; i.nChoices = n; return i;
}
static Item itemText(const char* l, void (*fmt)(char*, size_t), const char* charset, const char* hint, uint8_t max, bool (*textSet)(const char*)) {
  Item i = mk(l, ItemType::TEXT); i.fmt = fmt; i.charset = charset; i.hint = hint; i.textMax = max; i.textSet = textSet; return i;
}

// Character sets for the text editor. Hostnames and NTP hosts get only
// what they can contain, so nothing typed is ever rejected for a character.
static const char CHARS_HOST[] = "abcdefghijklmnopqrstuvwxyz0123456789-";
static const char CHARS_NTP[]  = "abcdefghijklmnopqrstuvwxyz0123456789-.";
static const char CHARS_FULL[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

// =====================
// STATE
// =====================
enum class Mode : uint8_t { LIST, EDIT, TEXT, SCAN, CONFIRM, INFO };

struct Frame { const Page* page; uint8_t cursor; uint8_t scroll; };
static Frame stack[6];
static uint8_t depth = 0;
static Mode mode = Mode::LIST;
static bool active = false;
static uint32_t lastInput = 0;
static lgfx::LGFX_Sprite* uiRef = nullptr;   // for the restart and reset screens

// EDIT
static int editVal = 0, editOrig = 0;

// TEXT
static char textBuf[65];
static uint8_t textLen = 0, textMax = 0;
static const char* textCharset = CHARS_FULL;
static int textSel = 0;
static const char* textTitle = "";
static char textHint[40];
static bool (*textDone)(const char*) = nullptr;
static Mode textReturn = Mode::LIST;
static bool textRejected = false;
static const Item* textItem = nullptr;

// CONFIRM
static const char* confirmTitle = "";
static const char* confirmLine1 = "";
static const char* confirmLine2 = "";
static void (*confirmFn)() = nullptr;
static uint8_t confirmCursor = 0;

// SCAN
struct Net { char ssid[33]; int8_t rssi; bool secure; };
static Net nets[16];
static uint8_t netCount = 0;
static bool scanning = false;
static uint8_t scanCursor = 0, scanScroll = 0;
static char joinSsid[33];

static Frame& top() { return stack[depth ? depth - 1 : 0]; }
static void push(const Page* p) {
  if (depth < sizeof(stack) / sizeof(stack[0])) { stack[depth].page = p; stack[depth].cursor = 0; stack[depth].scroll = 0; depth++; }
}
static void pop() {
  if (depth > 1) depth--;
  else menuClose();
}

static void confirm(const char* title, const char* l1, const char* l2, void (*fn)()) {
  confirmTitle = title; confirmLine1 = l1; confirmLine2 = l2; confirmFn = fn;
  confirmCursor = 0;                      // lands on "No"
  mode = Mode::CONFIRM;
}

// =====================
// VALUES
// =====================
static int  getBacklight() { return settings.displayBrightness; }
static void setBacklight(int v, bool commit) {
  setDisplayBrightness(v);
  backlightSet(settings.displayBrightness);
  if (commit) saveSettings();
}

static int  getLedOn() { return settings.ledEnabled; }
static void setLedOn(int v, bool commit) { settings.ledEnabled = v; ledApplySettings(); if (commit) saveSettings(); }
static int  getLedBright() { return settings.ledBrightness; }
static void setLedBright(int v, bool commit) { settings.ledBrightness = v; ledApplySettings(); if (commit) saveSettings(); }

static int  get24h() { return settings.clock24h; }
static void set24h(int v, bool commit) { settings.clock24h = v; if (commit) saveSettings(); }

// Same list as the setup page. The last entry stands for a TZ string that
// is not in the list (set from the browser) and changes nothing if chosen.
static const char* const ZONE_NAMES[] = {
  "UTC", "US Eastern", "US Central", "US Mountain", "US Arizona", "US Pacific", "US Alaska", "US Hawaii",
  "Canada Atlantic", "Brazil (Sao Paulo)", "UK / Ireland", "Central Europe", "Eastern Europe", "Moscow",
  "India", "China / Singapore", "Japan / Korea", "Australia East", "Australia West", "New Zealand", "Custom",
};
static const char* const ZONE_TZ[] = {
  "UTC0", "EST5EDT,M3.2.0,M11.1.0", "CST6CDT,M3.2.0,M11.1.0", "MST7MDT,M3.2.0,M11.1.0", "MST7", "PST8PDT,M3.2.0,M11.1.0",
  "AKST9AKDT,M3.2.0,M11.1.0", "HST10", "AST4ADT,M3.2.0,M11.1.0", "<-03>3", "GMT0BST,M3.5.0/1,M10.5.0",
  "CET-1CEST,M3.5.0,M10.5.0/3", "EET-2EEST,M3.5.0/3,M10.5.0/4", "MSK-3", "IST-5:30", "CST-8", "JST-9",
  "AEST-10AEDT,M10.1.0,M4.1.0/3", "AWST-8", "NZST-12NZDT,M9.5.0,M4.1.0/3",
};
static const int N_ZONES = sizeof(ZONE_TZ) / sizeof(ZONE_TZ[0]);

static int getZone() {
  for (int i = 0; i < N_ZONES; i++) if (!strcmp(settings.clockTz, ZONE_TZ[i])) return i;
  return N_ZONES;
}
static void setZone(int v, bool commit) {
  if (!commit || v >= N_ZONES) return;
  setClockTz(ZONE_TZ[v]);
  timeApply();
  saveSettings();
}

static const char* const NTP_NAMES[] = { "Internet", "Router", "Custom server" };
static int  getNtp() { return (int)settings.ntpSource; }
static void setNtp(int v, bool commit) {
  if (!commit) return;
  settings.ntpSource = (NtpSource)v;
  timeApply();
  saveSettings();
}
static void fmtNtp(char* b, size_t n) { strlcpy(b, settings.ntpServer, n); }
static bool setNtpText(const char* s) {
  if (!setNtpServer(s)) return false;
  timeApply();
  saveSettings();
  return true;
}

static int  getPing() { return settings.pingIntervalMs / 1000; }
static void setPing(int v, bool commit) {
  if (!commit) return;
  settings.pingIntervalMs = (uint32_t)v * 1000;
  saveSettings();
}

static void fmtHostname(char* b, size_t n) { strlcpy(b, settings.hostname, n); }
static bool setHostnameText(const char* s) {
  if (!setHostname(s)) return false;
  wifiApplyHostname();
  saveSettings();
  return true;
}

static void fmtTime(char* b, size_t n) {
  if (!timeSynced()) { strlcpy(b, "waiting for time sync", n); return; }
  time_t t = time(nullptr);
  tm lt;
  localtime_r(&t, &lt);
  strftime(b, n, settings.clock24h ? "%H:%M:%S" : "%I:%M:%S %p", &lt);
}
static void fmtSsid(char* b, size_t n) { strlcpy(b, settings.wifiSSID[0] ? settings.wifiSSID : "(none saved)", n); }
static void fmtIp(char* b, size_t n) {
  if (wifiState == WifiState::CONNECTED) strlcpy(b, WiFi.localIP().toString().c_str(), n);
  else if (wifiApActive()) snprintf(b, n, "hotspot %s", WiFi.softAPIP().toString().c_str());
  else strlcpy(b, "connecting...", n);
}
static void fmtSignal(char* b, size_t n) {
  if (wifiState != WifiState::CONNECTED) { strlcpy(b, "-", n); return; }
  int r = WiFi.RSSI();
  const char* q = r > -55 ? "excellent" : r > -65 ? "good" : r > -75 ? "fair" : "weak";
  snprintf(b, n, "%d dBm, %s", r, q);
}
static void fmtFirmware(char* b, size_t n) { strlcpy(b, FW_VERSION, n); }
static void fmtUptime(char* b, size_t n) {
  uint32_t s = millis() / 1000;
  snprintf(b, n, "%lud %luh %lum", (unsigned long)(s / 86400), (unsigned long)((s / 3600) % 24), (unsigned long)((s / 60) % 60));
}
static void fmtMemory(char* b, size_t n) {
  snprintf(b, n, "%u KB free, %u KB PSRAM", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024));
}

// =====================
// ACTIONS
// =====================
static void showConnection() { mode = Mode::INFO; }

static void scanStart() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true, false);
  scanning = true;
  netCount = 0;
  scanCursor = scanScroll = 0;
}
static void joinNetwork() { scanStart(); mode = Mode::SCAN; }

static void doForget() { wifiForget(); menuClose(); }
static void forgetNetwork() { confirm("Forget WiFi?", "The device goes back to", "the setup hotspot.", doForget); }

static void splash(const char* title, const char* text) {
  if (!uiRef) return;
  uiRef->fillScreen(TFT_BLACK);
  uiRef->setTextDatum(top_left);
  uiRef->setTextSize(2);
  uiRef->setTextColor(C_WARN, TFT_BLACK);
  uiRef->drawString(title, MARGIN, TITLE_Y);
  uiRef->setTextSize(1);
  uiRef->setTextColor(C_DIM, TFT_BLACK);
  uiRef->drawString(text, MARGIN, LIST_Y);
  uiRef->pushSprite(0, 0);
}
static void doRestart() {
  splash("Restarting", "Back in a few seconds.");
  Log.println("[SYS] Restart from the menu");
  delay(200);
  ESP.restart();
}
static void restartDevice() { confirm("Restart?", "Settings are kept.", "", doRestart); }

static void doReset() {
  ledSet(LedPattern::RESETTING);
  ledLoop();
  if (uiRef) { drawResettingScreen(*uiRef); uiRef->pushSprite(0, 0); }
  Log.println("[SYS] Factory reset from the menu");
  factoryReset();
  delay(500);
  ESP.restart();
}
static void factoryResetDevice() { confirm("Erase all?", "Removes WiFi, account,", "widgets and settings.", doReset); }

// =====================
// PAGES
// =====================
static Item clockItems[] = {
  itemBack(),
  itemInfo("Time", fmtTime),
  itemToggle("24-hour", get24h, set24h),
  itemChoice("Timezone", getZone, setZone, ZONE_NAMES, N_ZONES + 1),
  itemChoice("Time from", getNtp, setNtp, NTP_NAMES, 3),
  itemText("NTP server", fmtNtp, CHARS_NTP, "Used with 'Custom server'", NTP_HOST_MAX, setNtpText),
};
static const Page clockPage = { "Clock", clockItems, sizeof(clockItems) / sizeof(clockItems[0]) };

static Item ledItems[] = {
  itemBack(),
  itemToggle("Enabled", getLedOn, setLedOn),
  itemNumber("Brightness", getLedBright, setLedBright, 5, 255, 5, ""),
};
static const Page ledPage = { "Status LED", ledItems, sizeof(ledItems) / sizeof(ledItems[0]) };

static Item wifiItems[] = {
  itemBack(),
  itemInfo("Network", fmtSsid),
  itemInfo("Address", fmtIp),
  itemInfo("Signal", fmtSignal),
  itemAction("Join network", joinNetwork),
  itemAction("Forget", forgetNetwork),
};
static const Page wifiPage = { "WiFi", wifiItems, sizeof(wifiItems) / sizeof(wifiItems[0]) };

static Item deviceItems[] = {
  itemBack(),
  itemAction("Connection", showConnection),
  itemText("Name", fmtHostname, CHARS_HOST, "Address is <name>.local", HOSTNAME_MAX, setHostnameText),
  itemNumber("Ping every", getPing, setPing, 1, 60, 1, " s"),
  itemInfo("Firmware", fmtFirmware),
  itemInfo("Uptime", fmtUptime),
  itemInfo("Memory", fmtMemory),
  itemAction("Restart", restartDevice),
  itemAction("Factory reset", factoryResetDevice),
};
static const Page devicePage = { "Device", deviceItems, sizeof(deviceItems) / sizeof(deviceItems[0]) };

static Item mainItems[] = {
  itemNumber("Brightness", getBacklight, setBacklight, 1, 100, 1, "%"),
  itemSub("Clock", &clockPage),
  itemSub("Status LED", &ledPage),
  itemSub("WiFi", &wifiPage),
  itemSub("Device", &devicePage),
  itemAction("Exit", menuClose),
};
static const Page mainPage = { "Settings", mainItems, sizeof(mainItems) / sizeof(mainItems[0]) };

// =====================
// TEXT EDITOR
// =====================
static void textOpen(const char* title, const char* hint, const char* initial, const char* charset, uint8_t max,
                     bool (*done)(const char*), Mode ret) {
  textTitle = title;
  strlcpy(textHint, hint ? hint : "", sizeof(textHint));
  strlcpy(textBuf, initial ? initial : "", sizeof(textBuf));
  textLen = strlen(textBuf);
  textCharset = charset;
  textMax = max < sizeof(textBuf) - 1 ? max : sizeof(textBuf) - 1;
  textSel = 0;
  textDone = done;
  textReturn = ret;
  textRejected = false;
  mode = Mode::TEXT;
}

static bool itemTextDone(const char* s) { return textItem && textItem->textSet && textItem->textSet(s); }

static bool joinDone(const char* pass) {
  Log.printf("[MENU] Joining %s\n", joinSsid);
  wifiJoin(joinSsid, pass);
  menuClose();
  return true;
}

// =====================
// INPUT
// =====================
static void activate(const Item& it) {
  switch (it.type) {
    case ItemType::BACK:    pop(); break;
    case ItemType::SUBMENU: push(it.sub); break;
    case ItemType::ACTION:  if (it.action) it.action(); break;
    case ItemType::TOGGLE:  it.set(!it.get(), true); break;
    case ItemType::NUMBER:
    case ItemType::CHOICE:
      editOrig = editVal = it.get();
      mode = Mode::EDIT;
      break;
    case ItemType::TEXT: {
      char cur[65];
      it.fmt(cur, sizeof(cur));
      textItem = &it;
      textOpen(it.label, it.hint, cur, it.charset, it.textMax, itemTextDone, Mode::LIST);
      break;
    }
    case ItemType::INFO: break;
  }
}

static void inputList(const EncoderEvent& ev) {
  Frame& f = top();
  int n = f.page->count;
  if (ev.steps) {
    int c = (int)f.cursor + ev.steps;
    if (c < 0) c = 0;
    if (c > n - 1) c = n - 1;
    f.cursor = c;
  }
  if (ev.button == EncoderButton::CLICK) activate(f.page->items[f.cursor]);
  else if (ev.button == EncoderButton::LONG_PRESS) pop();
}

static void inputEdit(const EncoderEvent& ev) {
  const Item& it = top().page->items[top().cursor];
  if (ev.steps) {
    if (it.type == ItemType::CHOICE) {
      int n = it.nChoices;
      editVal = ((editVal + ev.steps) % n + n) % n;
    } else {
      editVal += ev.steps * it.step;
      if (editVal < it.min) editVal = it.min;
      if (editVal > it.max) editVal = it.max;
    }
    it.set(editVal, false);
  }
  if (ev.button == EncoderButton::CLICK) { it.set(editVal, true); mode = Mode::LIST; }
  else if (ev.button == EncoderButton::LONG_PRESS) { it.set(editOrig, false); mode = Mode::LIST; }
}

static void inputText(const EncoderEvent& ev) {
  int nChars = strlen(textCharset);
  int total = nChars + 3;             // + Delete, Cancel, OK
  if (ev.steps) {
    textSel = ((textSel + ev.steps) % total + total) % total;
    textRejected = false;
  }
  if (ev.button == EncoderButton::CLICK) {
    if (textSel < nChars) {
      if (textLen < textMax) { textBuf[textLen++] = textCharset[textSel]; textBuf[textLen] = '\0'; }
    } else if (textSel == nChars) {           // Delete
      if (textLen) textBuf[--textLen] = '\0';
    } else if (textSel == nChars + 1) {       // Cancel
      mode = textReturn;
    } else {                                  // OK
      if (textDone && textDone(textBuf)) { if (active) mode = textReturn; }
      else textRejected = true;
    }
  } else if (ev.button == EncoderButton::LONG_PRESS) {
    mode = textReturn;
  }
}

static void scanPoll() {
  if (!scanning) return;
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scanning = false;
  if (n < 0) return;
  // Strongest first, no duplicates or hidden networks, 16 at most.
  netCount = 0;
  for (int16_t i = 0; i < n && netCount < 16; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    bool dup = false;
    for (uint8_t k = 0; k < netCount; k++) if (ssid == nets[k].ssid) { dup = true; break; }
    if (dup) continue;
    Net e;
    strlcpy(e.ssid, ssid.c_str(), sizeof(e.ssid));
    e.rssi = WiFi.RSSI(i);
    e.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    uint8_t pos = netCount;
    while (pos > 0 && nets[pos - 1].rssi < e.rssi) { nets[pos] = nets[pos - 1]; pos--; }
    nets[pos] = e;
    netCount++;
  }
  WiFi.scanDelete();
}

static void inputScan(const EncoderEvent& ev) {
  int n = netCount + 2;                       // Back, networks, Scan again
  if (ev.steps) {
    int c = (int)scanCursor + ev.steps;
    if (c < 0) c = 0;
    if (c > n - 1) c = n - 1;
    scanCursor = c;
  }
  if (ev.button == EncoderButton::CLICK) {
    if (scanCursor == 0) mode = Mode::LIST;
    else if (scanCursor == n - 1) scanStart();
    else if (!scanning) {
      const Net& e = nets[scanCursor - 1];
      strlcpy(joinSsid, e.ssid, sizeof(joinSsid));
      if (e.secure) {
        char hint[40];
        snprintf(hint, sizeof(hint), "for %s", e.ssid);
        textOpen("Password", hint, "", CHARS_FULL, 63, joinDone, Mode::SCAN);
      } else {
        joinDone("");
      }
    }
  } else if (ev.button == EncoderButton::LONG_PRESS) {
    mode = Mode::LIST;
  }
}

static void inputConfirm(const EncoderEvent& ev) {
  if (ev.steps) confirmCursor = (ev.steps > 0) ? 1 : 0;
  if (ev.button == EncoderButton::CLICK) {
    if (confirmCursor == 1 && confirmFn) { mode = Mode::LIST; confirmFn(); }
    else mode = Mode::LIST;
  } else if (ev.button == EncoderButton::LONG_PRESS) {
    mode = Mode::LIST;
  }
}

static void inputInfo(const EncoderEvent& ev) {
  if (ev.button != EncoderButton::NONE) mode = Mode::LIST;
}

// =====================
// DRAWING
// =====================
static void header(lgfx::LGFX_Sprite& ui, const char* title, uint16_t color = C_TEXT) {
  ui.fillScreen(TFT_BLACK);
  ui.setTextDatum(top_left);
  ui.setTextSize(2);
  ui.setTextColor(color, TFT_BLACK);
  ui.drawString(title, MARGIN, TITLE_Y);
  ui.drawFastHLine(MARGIN, LIST_Y - 8, ui.width() - 2 * MARGIN, C_BOX);
  ui.setTextSize(1);
}

static void footer(lgfx::LGFX_Sprite& ui, const char* text) {
  ui.setTextDatum(top_left);
  ui.setTextSize(1);
  ui.setTextColor(C_DIM, TFT_BLACK);
  ui.drawString(text, MARGIN, ui.height() - 14);
}

static void bodyLine(lgfx::LGFX_Sprite& ui, int& y, const char* text, uint16_t color = C_TEXT, int size = 1) {
  ui.setTextDatum(top_left);
  ui.setTextSize(size);
  ui.setTextColor(color, TFT_BLACK);
  ui.drawString(text, MARGIN, y);
  y += size == 2 ? 20 : 11;
}

struct Row {
  const char* label;
  const char* right;          // short text at the right edge, size 2
  char value[VALUE_CHARS + 1];
  bool hasValue;
  bool editing;
};
typedef void (*RowFn)(int i, Row& r);

static int rowHeight(const Row& r) { return r.editing ? ROW_EDIT : r.hasValue ? ROW_VALUE : ROW_PLAIN; }

// A scrolling list of rows with the cursor row highlighted. scroll is the
// first visible row and is moved as needed to keep the cursor on screen.
static void drawList(lgfx::LGFX_Sprite& ui, int count, int cursor, uint8_t& scroll, RowFn fn) {
  const int avail = ui.height() - LIST_Y - FOOTER_H;
  if (cursor < scroll) scroll = cursor;
  for (;;) {
    int h = 0;
    bool fits = true;
    for (int i = scroll; i <= cursor; i++) {
      Row r = {};
      fn(i, r);
      h += rowHeight(r);
      if (h > avail) { fits = false; break; }
    }
    if (fits || scroll >= cursor) break;
    scroll++;
  }

  int y = LIST_Y;
  int i = scroll;
  for (; i < count; i++) {
    Row r = {};
    fn(i, r);
    int h = rowHeight(r);
    if (y + h > LIST_Y + avail) break;
    bool sel = (i == cursor);
    uint16_t bg = sel ? C_SEL : TFT_BLACK;
    if (sel) ui.fillRoundRect(2, y, ui.width() - 4, h - 2, 4, C_SEL);
    ui.setTextDatum(top_left);
    ui.setTextSize(2);
    ui.setTextColor(sel ? C_TEXT : C_LABEL, bg);
    ui.drawString(r.label, MARGIN + 4, y + 3);
    if (r.right) {
      ui.setTextDatum(top_right);
      ui.drawString(r.right, ui.width() - MARGIN - 4, y + 3);
      ui.setTextDatum(top_left);
    }
    if (r.hasValue) {
      ui.setTextSize(r.editing ? 2 : 1);
      ui.setTextColor(r.editing ? C_OK : (sel ? C_TEXT : C_DIM), bg);
      ui.drawString(r.value, MARGIN + 4, y + 22);
    }
    y += h;
  }

  // More above or below than fits.
  ui.setTextSize(1);
  ui.setTextColor(C_DIM, TFT_BLACK);
  ui.setTextDatum(top_right);
  if (scroll > 0) ui.drawString("^", ui.width() - MARGIN, LIST_Y - 7);
  if (i < count) ui.drawString("v", ui.width() - MARGIN, ui.height() - FOOTER_H - 10);
  ui.setTextDatum(top_left);
}

static void pageRow(int i, Row& r) {
  const Item& it = top().page->items[i];
  r.label = it.label;
  r.editing = (mode == Mode::EDIT && i == (int)top().cursor);
  switch (it.type) {
    case ItemType::SUBMENU:
      r.right = ">";
      break;
    case ItemType::TOGGLE:
      r.right = it.get() ? "On" : "Off";
      break;
    case ItemType::NUMBER: {
      int v = r.editing ? editVal : it.get();
      snprintf(r.value, sizeof(r.value), "%s%d%s%s", r.editing ? "< " : "", v, it.unit, r.editing ? " >" : "");
      r.hasValue = true;
      break;
    }
    case ItemType::CHOICE: {
      int v = r.editing ? editVal : it.get();
      if (v < 0 || v >= it.nChoices) v = 0;
      snprintf(r.value, sizeof(r.value), "%s%s%s", r.editing ? "< " : "", it.choices[v], r.editing ? " >" : "");
      r.hasValue = true;
      break;
    }
    case ItemType::TEXT:
    case ItemType::INFO:
      it.fmt(r.value, sizeof(r.value));
      r.hasValue = true;
      break;
    default:
      break;
  }
}

static void renderList(lgfx::LGFX_Sprite& ui) {
  Frame& f = top();
  header(ui, f.page->title);
  drawList(ui, f.page->count, f.cursor, f.scroll, pageRow);
  if (mode == Mode::EDIT) footer(ui, "Turn to change, click to set");
  else footer(ui, depth > 1 ? "Click opens, hold goes back" : "Click opens, hold exits");
}

static void renderText(lgfx::LGFX_Sprite& ui) {
  header(ui, textTitle);
  int y = LIST_Y - 2;
  if (textHint[0]) bodyLine(ui, y, textHint, C_DIM);
  y += 4;

  // The text so far, wrapped, with a cursor after the last character.
  ui.setTextSize(2);
  ui.setTextColor(C_TEXT, TFT_BLACK);
  char line[TEXT_COLS + 2];
  int lines = textLen / TEXT_COLS + 1;        // the last line carries the cursor
  for (int l = 0; l < lines && y < 160; l++) {
    int p = l * TEXT_COLS;
    int n = textLen - p;
    if (n > TEXT_COLS) n = TEXT_COLS;
    memcpy(line, textBuf + p, n);
    if (l == lines - 1) line[n++] = '_';
    line[n] = '\0';
    ui.drawString(line, MARGIN, y);
    y += 18;
  }

  // Character strip: the selected one in the middle, five either side.
  const int nChars = strlen(textCharset);
  const int total = nChars + 3;
  const int cells = 11, cellW = 14, stripY = 178;
  const int x0 = (ui.width() - cells * cellW) / 2;
  for (int k = 0; k < cells; k++) {
    int idx = ((textSel - 5 + k) % total + total) % total;
    if (idx >= nChars) continue;                 // action slots stay blank in the strip
    int x = x0 + k * cellW;
    bool s = (k == 5);
    if (s) ui.fillRoundRect(x, stripY, cellW, 22, 3, C_SEL);
    char c = textCharset[idx];
    ui.setTextSize(2);
    ui.setTextColor(s ? C_TEXT : C_DIM, s ? C_SEL : TFT_BLACK);
    if (c == ' ') ui.drawFastHLine(x + 2, stripY + 17, cellW - 4, s ? C_TEXT : C_DIM);
    else { char str[2] = { c, 0 }; ui.drawString(str, x + 1, stripY + 4); }
  }

  // Delete / Cancel / OK below the strip; reached by turning past the characters.
  static const char* const ACTS[3] = { "Delete", "Cancel", "OK" };
  const int boxY = 212, boxW = 50, boxH = 22;
  for (int a = 0; a < 3; a++) {
    int x = MARGIN + a * (boxW + 4);
    bool s = (textSel == nChars + a);
    ui.fillRoundRect(x, boxY, boxW, boxH, 4, s ? C_SEL : C_BOX);
    ui.setTextSize(1);
    ui.setTextDatum(middle_center);
    ui.setTextColor(s ? C_TEXT : C_DIM, s ? C_SEL : C_BOX);
    ui.drawString(ACTS[a], x + boxW / 2, boxY + boxH / 2);
  }
  ui.setTextDatum(top_left);

  int yy = 246;
  bodyLine(ui, yy, "Turn to pick a character,", C_DIM);
  bodyLine(ui, yy, "click to add it. Turn back", C_DIM);
  bodyLine(ui, yy, "past 'a' for Delete and OK.", C_DIM);
  footer(ui, textRejected ? "Not accepted, try again" : "Click adds, hold cancels");
}

static void scanRow(int i, Row& r) {
  if (i == 0) { r.label = "< Back"; return; }
  if (i == (int)netCount + 1) { r.label = "Scan again"; return; }
  const Net& e = nets[i - 1];
  r.label = e.ssid;
  snprintf(r.value, sizeof(r.value), "%d dBm, %s", e.rssi, e.secure ? "password" : "open");
  r.hasValue = true;
}

static void renderScan(lgfx::LGFX_Sprite& ui) {
  header(ui, "Join network");
  if (scanning) {
    int y = LIST_Y;
    static const char spin[4] = { '|', '/', '-', '\\' };
    char buf[24];
    snprintf(buf, sizeof(buf), "%c Scanning...", spin[(millis() / 150) & 3]);
    bodyLine(ui, y, buf, C_DIM);
    footer(ui, "Hold to go back");
    return;
  }
  drawList(ui, netCount + 2, scanCursor, scanScroll, scanRow);
  footer(ui, netCount ? "Click joins, hold goes back" : "No networks found");
}

static void confirmRow(int i, Row& r) { r.label = i ? "Yes" : "No"; }

static void renderConfirm(lgfx::LGFX_Sprite& ui) {
  header(ui, confirmTitle, C_WARN);
  int y = LIST_Y;
  if (confirmLine1[0]) bodyLine(ui, y, confirmLine1, C_DIM);
  if (confirmLine2[0]) bodyLine(ui, y, confirmLine2, C_DIM);
  y += 6;
  for (int i = 0; i < 2; i++) {
    bool sel = (i == confirmCursor);
    if (sel) ui.fillRoundRect(2, y, ui.width() - 4, ROW_PLAIN - 2, 4, C_SEL);
    ui.setTextSize(2);
    ui.setTextColor(sel ? (i ? C_BAD : C_TEXT) : C_LABEL, sel ? C_SEL : TFT_BLACK);
    ui.drawString(i ? "Yes" : "No", MARGIN + 4, y + 3);
    y += ROW_PLAIN;
  }
  footer(ui, "Click to choose, hold for no");
}

static void renderInfo(lgfx::LGFX_Sprite& ui) {
  header(ui, "Connection", wifiState == WifiState::CONNECTED ? C_OK : C_WARN);
  int y = LIST_Y;
  char buf[48];
  if (wifiState == WifiState::CONNECTED) {
    bodyLine(ui, y, "Network:", C_DIM);
    bodyLine(ui, y, settings.wifiSSID);
    y += 6;
    bodyLine(ui, y, "Open in a browser:", C_DIM);
    snprintf(buf, sizeof(buf), "http://%s.local", settings.hostname);
    bodyLine(ui, y, buf);
    snprintf(buf, sizeof(buf), "http://%s", WiFi.localIP().toString().c_str());
    bodyLine(ui, y, buf);
    y += 6;
    bodyLine(ui, y, "API key:", C_DIM);
    bodyLine(ui, y, settings.apiToken, C_TEXT, 2);
    y += 4;
    bodyLine(ui, y, "The login page asks for it", C_DIM);
    bodyLine(ui, y, "once to create the account,", C_DIM);
    bodyLine(ui, y, "or to reset the password.", C_DIM);
  } else if (wifiApActive()) {
    bodyLine(ui, y, "Not on a network.", C_WARN);
    bodyLine(ui, y, "The setup hotspot is on:", C_DIM);
    y += 6;
    bodyLine(ui, y, wifiApSsid(), C_TEXT, 2);
    snprintf(buf, sizeof(buf), "password: %s", wifiApPassword());
    bodyLine(ui, y, buf);
    snprintf(buf, sizeof(buf), "http://%s", WiFi.softAPIP().toString().c_str());
    bodyLine(ui, y, buf);
    y += 6;
    bodyLine(ui, y, "Or pick WiFi > Join network", C_DIM);
    bodyLine(ui, y, "in this menu.", C_DIM);
  } else {
    bodyLine(ui, y, "Connecting to:", C_DIM);
    bodyLine(ui, y, settings.wifiSSID);
  }
  footer(ui, "Click to go back");
}

// =====================
// PUBLIC
// =====================
void menuOpen(uint32_t now) {
  active = true;
  depth = 0;
  push(&mainPage);
  mode = Mode::LIST;
  lastInput = now;
}

void menuClose() {
  active = false;
  mode = Mode::LIST;
  depth = 0;
}

bool menuActive() { return active; }

void menuInput(const EncoderEvent& ev, uint32_t now) {
  if (!active) return;
  if (ev.steps || ev.button != EncoderButton::NONE) lastInput = now;
  else if ((int32_t)(now - lastInput) > (int32_t)IDLE_CLOSE_MS) {
    // Walked away: drop any half-done change and go back to the widget.
    if (mode == Mode::EDIT) {
      const Item& it = top().page->items[top().cursor];
      it.set(editOrig, false);
    }
    menuClose();
    return;
  }
  switch (mode) {
    case Mode::LIST:    inputList(ev); break;
    case Mode::EDIT:    inputEdit(ev); break;
    case Mode::TEXT:    inputText(ev); break;
    case Mode::SCAN:    inputScan(ev); break;
    case Mode::CONFIRM: inputConfirm(ev); break;
    case Mode::INFO:    inputInfo(ev); break;
  }
}

void menuRender(lgfx::LGFX_Sprite& ui, uint32_t now) {
  (void)now;
  uiRef = &ui;
  if (!active || depth == 0) return;
  switch (mode) {
    case Mode::LIST:
    case Mode::EDIT:    renderList(ui); break;
    case Mode::TEXT:    renderText(ui); break;
    case Mode::SCAN:    scanPoll(); renderScan(ui); break;
    case Mode::CONFIRM: renderConfirm(ui); break;
    case Mode::INFO:    renderInfo(ui); break;
  }
}
