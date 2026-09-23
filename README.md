# ESP32 Desktop Widget

A small 3D-printed desktop display built around an ESP32-S3 and a 1.9" IPS screen. The firmware is a simple widget framework: each screen is a `Widget` class, and you pick which one is showing from a web page served by the device.

It currently ships with two widgets:

- **Ping / Server Status** – TCP-connects to a list of hosts on an interval and shows latency, an up/down sparkline, a trend arrow, WiFi signal strength, and the device IP. Targets are grouped into *Services* and *Servers*.
- **Clock** – NTP-synced clock with 12/24-hour display.

Adding your own widget is a matter of implementing four methods (see [Writing a widget](#writing-a-widget)).

## Hardware

| Part | Link |
|------|------|
| ESP32-S3-DevKitC-1 (N16R8: 16 MB flash, 8 MB PSRAM) | https://a.co/d/0ay0gra1 |
| 1.9" 170×320 IPS LCD, ST7789V2, SPI | https://a.co/d/0bqQSNMt |
| Printable enclosure (STL) | https://makerworld.com/en/models/2918927-esp-32-desktop-widget#profileId-3265758 |

The DevKitC's onboard WS2812 RGB LED (GPIO 48) is used as a status light.

### Wiring

| Display pin | ESP32-S3 GPIO |
|-------------|---------------|
| SCLK | 12 |
| MOSI | 11 |
| CS | 10 |
| DC | 9 |
| RST | 8 |
| VCC | 3V3 |
| GND | GND |
| BL | 3V3 (or a spare GPIO if you want backlight control) |

Pins are set in the `LGFX_Display` class at the top of `src/main.cpp`.

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project. Open the folder in VS Code with the PlatformIO extension, or from the command line:

```bash
pio run -t upload
pio device monitor
```

Dependencies (LovyanGFX, Adafruit NeoPixel, ESPAsyncWebServer, AsyncTCP) are pulled in automatically from `platformio.ini`.

## First boot and WiFi setup

WiFi credentials are stored in the ESP32's NVS flash, not in the source. On first boot there are none, so the device starts its own access point and shows the details on screen:

| | |
|--|--|
| SSID | `ESP32-StatusPanel` |
| Password | `configureme` |
| URL | http://192.168.4.1 |

The status LED blinks blue in AP mode and turns solid green once connected to your network. If the saved network can't be reached within 15 seconds the device falls back to AP mode.

> **Note:** The web UI currently exposes the widget selector but not a WiFi form. To store credentials, either add a `/wifi` route to `src/web/WebServer.cpp`, or temporarily set `settings.wifiSSID` / `settings.wifiPass` in `setup()` and call `saveSettings()` once, then remove the lines and reflash. The AP password is a plain default in `src/net/WifiManager.cpp`; change it if you like.

## Web interface

Once on your network, the device serves:

| Route | Purpose |
|-------|---------|
| `/` | Health check |
| `/widgets` | Page with a button per widget to switch the active screen |
| `GET /api/widgets` | JSON list of widgets and the active index |
| `POST /api/widgets` (`index=N`) | Switch the active widget; choice is persisted |

## Configuring ping targets

Defaults live in `loadDefaultTargets()` in `src/web/Settings.cpp`:

```cpp
setTarget(0, "Cloudflare", "1.1.1.1",      443, TargetType::SERVICE);
setTarget(1, "Google",     "8.8.8.8",      53,  TargetType::SERVICE);
setTarget(2, "Router",     "192.168.88.1", 0,   TargetType::SERVER);
```

Each target has a display name (15 chars max), host or IP, port, and a group. Port `0` falls back to 22 (SSH), which is a convenient liveness check for most Linux boxes. Up to 12 targets are supported. After three consecutive failures a target is marked down (blinking red) and paused for 60 seconds before retrying.

The ping interval, timezone offset, and 12/24-hour setting are also persisted in NVS; see `Settings.h`.

## Writing a widget

Subclass `Widget` (`src/app/Widget.h`):

```cpp
class MyWidget : public Widget {
public:
  const char* name() const override { return "My Widget"; }
  void begin() override {}                       // called once at boot
  void update(uint32_t now) override {}          // called every loop
  void render(lgfx::LGFX_Sprite& ui) override {  // draw into the sprite
    ui.fillScreen(TFT_BLACK);
    ui.drawString("Hello", 10, 10);
  }
};
```

Then register it in `setup()` in `src/main.cpp`:

```cpp
MyWidget myWidget;
// ...
screens.add(&myWidget);
```

It will appear in the web selector automatically. The `ScreenManager` holds up to 8 widgets. Rendering is double-buffered through a full-screen `LGFX_Sprite`, so widgets just draw and don't need to worry about flicker.

## Project layout

```
src/
  main.cpp            display + LED init, boot sequence, main loop
  app/                Widget interface and ScreenManager
  widgets/            PingWidget, ClockWidget
  net/                WiFi manager (STA/AP fallback), TCP ping
  web/                Settings (NVS persistence), async web server
platformio.ini        board, flash/PSRAM config, library deps
```

`src/User_Setup.h` is a leftover TFT_eSPI config and isn't used by the LovyanGFX build; it's kept as a reference for the display's pinout and BGR order.

## License

MIT – see [LICENSE](LICENSE).
