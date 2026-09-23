# ESP32 Desktop Widget

This project started as a Christmas gift for my dad. A small 3D-printed desktop display built around an ESP32-S3 and a 1.9" IPS screen. The firmware is a simple widget framework: each screen is a `Widget` class, and you pick which one is showing from a web page served by the device.

It currently ships with two widgets:

- **Ping / Server Status** – TCP-connects to a list of hosts on an interval and shows latency, an up/down sparkline, a trend arrow, WiFi signal strength, and the device IP. Targets are grouped into *Services* and *Servers*.
- **Clock** – NTP-synced clock with 12/24-hour display.

Adding your own widget is a matter of implementing four methods (see [Writing a widget](#writing-a-widget)).

## Hardware

| Part                                                | Link                                                                             |
| --------------------------------------------------- | -------------------------------------------------------------------------------- |
| ESP32-S3-DevKitC-1 (N16R8: 16 MB flash, 8 MB PSRAM) | https://a.co/d/0ay0gra1                                                          |
| 1.9" 170×320 IPS LCD, ST7789V2, SPI                 | https://a.co/d/0bqQSNMt                                                          |
| Printable enclosure (STL)                           | https://makerworld.com/en/models/2918927-esp-32-desktop-widget#profileId-3265758 |

The DevKitC's onboard WS2812 RGB LED (GPIO 48) is used as a status light.

### Wiring

| Display pin | ESP32-S3 GPIO                                       |
| ----------- | --------------------------------------------------- |
| SCLK        | 12                                                  |
| MOSI        | 11                                                  |
| CS          | 10                                                  |
| DC          | 9                                                   |
| RST         | 8                                                   |
| VCC         | 3V3                                                 |
| GND         | GND                                                 |
| BL          | 3V3 (or a spare GPIO if you want backlight control) |

Pins are set in the `LGFX_Display` class at the top of `src/main.cpp`.

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project. Open the folder in VS Code with the PlatformIO extension, or from the command line:

```bash
pio run -t upload
pio device monitor
```

Dependencies (LovyanGFX, Adafruit NeoPixel, ESP32Async/ESPAsyncWebServer, ESP32Async/AsyncTCP, ArduinoJson) are pulled in automatically from `platformio.ini`. The partition table (`default_16MB.csv`) gives two OTA app slots and a 3.4 MB LittleFS partition.

## First boot and WiFi setup

WiFi credentials and the API token are stored in the ESP32's NVS flash, not in the source. On first boot there are none, so the device starts its own access point and shows the details on screen:

|          |                     |
| -------- | ------------------- |
| SSID     | `ESP32-StatusPanel` |
| Password | `configureme`       |
| URL      | http://192.168.4.1  |

The status LED blinks blue in AP mode and turns solid green once connected to your network. If the saved network can't be reached within 15 seconds the device falls back to AP mode.

Once connected, the screen shows the device IP and the API token for 30 seconds before switching to the active widget. The token is generated on first boot and is required for any request that changes settings.

> **Note:** The web UI currently exposes the widget selector but not a WiFi form. To store credentials, either add a `/wifi` route to `src/web/WebServer.cpp`, or temporarily set `settings.wifiSSID` / `settings.wifiPass` in `setup()` and call `saveCredentials()` once, then remove the lines and reflash. The AP password is a plain default in `src/net/WifiManager.cpp`; change it if you like.

## Web interface

Once on your network, the device serves:

| Route | Auth | Purpose |
|-------|------|---------|
| `/` | no | Health check |
| `/widgets` | no | Page with a button per widget to switch the active screen |
| `GET /api/status` | no | Firmware version, uptime, heap, WiFi, active widget |
| `GET /api/widgets` | no | JSON list of widgets and the active index |
| `POST /api/widgets` (`index=N`) | yes | Switch the active widget; choice is persisted |

Authenticated routes need an `Authorization: Bearer <token>` header. The widget selector page has a field for the token and remembers it in the browser.

```bash
curl -X POST http://<device-ip>/api/widgets \
  -H "Authorization: Bearer <token>" \
  -d index=1
```

## Configuring ping targets

Defaults live in `loadDefaultTargets()` in `src/web/Settings.cpp`:

```cpp
setTarget(0, "Cloudflare", "1.1.1.1",      443, TargetType::SERVICE);
setTarget(1, "Google",     "8.8.8.8",      53,  TargetType::SERVICE);
setTarget(2, "Router",     "192.168.88.1", 0,   TargetType::SERVER);
```

Each target has a display name (15 chars max), host or IP, port, and a group. Port `0` falls back to 22 (SSH), which is a convenient liveness check for most Linux boxes. Up to 12 targets are supported. After three consecutive failures a target is marked down (blinking red) and paused for 60 seconds before retrying.

Everything except WiFi credentials and the token lives in `/config.json` on the LittleFS partition and looks like this:

```json
{
  "version": 1,
  "pingIntervalMs": 10000,
  "activeWidget": 0,
  "clock": { "tzOffset": 0, "dstOffset": 0, "24h": true },
  "targets": [
    { "name": "Cloudflare", "host": "1.1.1.1", "port": 443, "type": "service" },
    { "name": "Router", "host": "192.168.88.1", "port": 0, "type": "server" }
  ]
}
```

Writes go to a temp file and are renamed into place, so a power cut mid-save can't corrupt the config. Devices upgraded from the older firmware migrate their NVS settings into this file automatically on first boot.

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
  web/                Settings (NVS + LittleFS JSON), async web server + API
platformio.ini        board, partition table, library deps
```

## License

MIT – see [LICENSE](LICENSE).
