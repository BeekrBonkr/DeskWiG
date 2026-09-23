# DeskWiG

DeskWiG (Desk Widget) started as a Christmas gift for my dad. A small 3D-printed desktop display built around an ESP32-S3 and a 1.9" IPS screen. The firmware is a simple widget framework: each screen is a `Widget` class, and you pick which one is showing from a web page served by the device.

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

The DevKitC's onboard WS2812 RGB LED (GPIO 48) is used as a status light. GPIO 4 is the recovery jumper (see [Recovery](#recovery-jumper)).

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

All pins are defined in `src/app/Board.h`.

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project. Open the folder in VS Code with the PlatformIO extension, or from the command line:

```bash
pio run -t upload
pio device monitor
```

Dependencies (LovyanGFX, Adafruit NeoPixel, ESP32Async/ESPAsyncWebServer, ESP32Async/AsyncTCP, ArduinoJson) are pulled in automatically from `platformio.ini`. The partition table (`default_16MB.csv`) gives two OTA app slots and a 3.4 MB LittleFS partition.

## First boot and WiFi setup

WiFi credentials and the API token are stored in the ESP32's NVS flash, not in the source. On first boot there are none, so the device starts a setup hotspot and shows the details on screen:

|          |                      |
| -------- | -------------------- |
| SSID     | `DeskWiG-Setup`    |
| Password | `configureme`        |
| URL      | http://192.168.4.1   |

1. Join the hotspot from a phone or laptop. A setup page should open automatically (captive portal). If it doesn't, browse to the URL above.
2. Tap **Scan for networks**, pick yours, enter the password, and tap **Join**.
3. The page reports when the device has connected and shows its new address. The hotspot stays up for 20 seconds after connecting so you can read it, then turns off.
4. The device screen shows the address and the API token for 30 seconds, then switches to the active widget.

The device is reachable at `http://deskwig.local` (mDNS) or its IP. The name is changeable on the setup page.

If the saved network can't be reached within 15 seconds the hotspot comes back, and the device keeps retrying the saved network once a minute so it recovers by itself after a router reboot. Requests made over the hotspot don't need the API token, since anyone standing next to the device with the hotspot password is already trusted for setup.

### Status LED

| LED               | Meaning                                  |
| ----------------- | ---------------------------------------- |
| Blue, blinking    | Setup hotspot active                     |
| Amber, solid      | Connecting to WiFi                       |
| Green, solid, dim | Connected, running normally              |
| White, fast blink | Factory reset in progress                |
| Off               | Disabled in config (`led.enabled=false`) |

Brightness defaults to 5 out of 255 and is adjustable via `/api/config`.

### Recovery jumper

Bridge GPIO 4 to GND and press reset:

- **Release within 8 seconds:** the device skips the saved network and starts the setup hotspot. Use this if you moved it to a new network or mistyped a password.
- **Keep it bridged for 8 seconds:** the screen counts down, then the device erases all settings (LittleFS and NVS) and restarts into setup mode. A new API token is generated.

A tactile switch or two exposed pads on the enclosure work equally well.

## Web interface

Once on your network, the device serves:

| Route                           | Auth | Purpose                                                        |
| ------------------------------- | ---- | -------------------------------------------------------------- |
| `/setup`                        | no   | WiFi scan/join, device name, forget network                    |
| `/widgets`                      | no   | Page with a button per widget to switch the active screen      |
| `GET /api/status`               | no   | Firmware, uptime, heap, WiFi state, active widget              |
| `GET /api/wifi`                 | no   | Connection state, SSID, IP, hostname, hotspot state            |
| `GET /api/wifi/scan`            | no   | Starts a scan; poll until `status` is `done`                   |
| `POST /api/wifi/join`           | yes  | JSON `{"ssid","pass"}`. Saves and connects, hotspot stays up   |
| `POST /api/wifi/forget`         | yes  | Clears credentials, returns to hotspot                         |
| `GET /api/config`               | no   | Hostname, ping interval, clock, LED settings                   |
| `PUT /api/config`               | yes  | JSON with any subset of the above; saved immediately           |
| `GET /api/widgets`              | no   | JSON list of widgets and the active index                      |
| `POST /api/widgets` (`index=N`) | yes  | Switch the active widget; choice is persisted                  |
| `POST /api/system/reboot`       | yes  | Restart                                                        |
| `POST /api/system/reset`        | yes  | Factory reset and restart                                      |

Authenticated routes need an `Authorization: Bearer <token>` header, except when the request comes in over the setup hotspot. Both pages have a field for the token and remember it in the browser.

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
  "hostname": "deskwig",
  "pingIntervalMs": 10000,
  "activeWidget": 0,
  "clock": { "tzOffset": 0, "dstOffset": 0, "24h": true },
  "led": { "enabled": true, "brightness": 5 },
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
  main.cpp            display init, recovery jumper, boot sequence, main loop
  app/                Board pins, Widget interface, ScreenManager, status LED, system screens
  widgets/            PingWidget, ClockWidget
  net/                WiFi manager (STA/hotspot/captive portal/mDNS), TCP ping
  web/                Settings (NVS + LittleFS JSON), async web server, API, HTML pages
platformio.ini        board, partition table, library deps
```

## License

MIT – see [LICENSE](LICENSE).
