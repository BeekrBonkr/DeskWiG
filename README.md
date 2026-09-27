# DeskWiG

DeskWiG (Desk Widget) started as a Christmas gift for my dad. A small 3D-printed desktop display built around an ESP32-S3 and a 1.9" IPS screen. The firmware is a simple widget framework: each screen is a `Widget` class, and you pick which one is showing from a web page served by the device.

It currently ships with two widgets:

- **Ping / Server Status** – TCP-connects to a list of hosts on an interval and shows latency, an up/down sparkline, a trend arrow, WiFi signal strength, and the device IP. Targets are grouped into *Services* and *Servers*.
- **Clock** – NTP-synced clock with 12/24-hour display. Time can come from the internet, from your router, or from any NTP server you name; timezones use POSIX TZ strings so daylight saving is automatic.

You can also build your own screens without a toolchain: [JSON layout widgets](#json-layout-widgets) are written in the browser, previewed live, and stored on the device. They can show live values from any web API through [data sources](#data-sources): weather, stock prices, a Home Assistant sensor, anything that answers HTTP with JSON or text. Native widgets in C++ are still a matter of implementing four methods (see [Writing a widget](#writing-a-widget)).

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
| `/setup`                        | no   | WiFi scan/join, device name, clock, data sources               |
| `/widgets`                      | no   | Page with a button per widget to switch the active screen      |
| `/editor`                       | no   | In-browser editor for JSON layout widgets                      |
| `GET /api/status`               | no   | Firmware, uptime, heap, WiFi state, active widget, clock sync  |
| `GET /api/wifi`                 | no   | Connection state, SSID, IP, hostname, hotspot state            |
| `GET /api/wifi/scan`            | no   | Starts a scan; poll until `status` is `done`                   |
| `POST /api/wifi/join`           | yes  | JSON `{"ssid","pass"}`. Saves and connects, hotspot stays up   |
| `POST /api/wifi/forget`         | yes  | Clears credentials, returns to hotspot                         |
| `GET /api/config`               | no   | Hostname, ping interval, clock, LED settings                   |
| `PUT /api/config`               | yes  | JSON with any subset of the above; saved immediately           |
| `GET /api/sources`              | no   | Data sources with fetch state and current values (header value redacted) |
| `PUT /api/sources?id=<id>`      | yes  | Create or replace a data source (see [Data sources](#data-sources)) |
| `DELETE /api/sources?id=<id>`   | yes  | Remove a data source                                           |
| `POST /api/sources/test?id=<id>`| yes  | Fetch it now; poll `GET /api/sources` for the result           |
| `GET /api/widgets`              | no   | JSON list of widgets and the active index                      |
| `POST /api/widgets` (`index=N`) | yes  | Switch the active widget; choice is persisted                  |
| `GET /api/layouts`              | no   | List of layout widgets (`id`, `name`, `index`) and free slots  |
| `GET /api/layouts?id=<id>`      | no   | The stored layout JSON                                         |
| `PUT /api/layouts?id=<id>`      | yes  | Create or replace a layout; validated, saved, applied at once  |
| `DELETE /api/layouts?id=<id>`   | yes  | Remove a layout widget                                         |
| `POST /api/layouts/preview`     | yes  | Show a layout on the device for 60 s without saving it         |
| `DELETE /api/layouts/preview`   | yes  | End the preview early                                          |
| `GET /api/layouts/data`         | no   | Every template key with its current value                      |
| `GET /api/layouts/templates`    | no   | Built-in templates with their layout JSON                      |
| `POST /api/system/reboot`       | yes  | Restart                                                        |
| `POST /api/system/reset`        | yes  | Factory reset and restart                                      |

Authenticated routes need an `Authorization: Bearer <token>` header, except when the request comes in over the setup hotspot. Every page has a field for the token and remembers it in the browser.

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
  "clock": { "tz": "EST5EDT,M3.2.0,M11.1.0", "24h": true, "ntpSource": "router", "ntpServer": "pool.ntp.org" },
  "led": { "enabled": true, "brightness": 5 },
  "sources": [
    { "id": "weather", "url": "https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&current=temperature_2m,relative_humidity_2m,wind_speed_10m",
      "intervalS": 600,
      "fields": [ { "name": "temp", "path": "current.temperature_2m", "decimals": 0 },
                  { "name": "humidity", "path": "current.relative_humidity_2m" },
                  { "name": "wind", "path": "current.wind_speed_10m", "decimals": 0 } ] }
  ],
  "targets": [
    { "name": "Cloudflare", "host": "1.1.1.1", "port": 443, "type": "service" },
    { "name": "Router", "host": "192.168.88.1", "port": 0, "type": "server" }
  ]
}
```

Writes go to a temp file and are renamed into place, so a power cut mid-save can't corrupt the config. Devices upgraded from the older firmware migrate their NVS settings into this file automatically on first boot.

## Clock and timezone

NTP always delivers UTC. The device turns that into local time with a POSIX TZ string stored as `clock.tz`, which also encodes the daylight-saving rule, so the clock stays right all year. The setup page has a dropdown of common zones and a custom field; a few examples:

| Zone           | TZ string                    |
| -------------- | ---------------------------- |
| UTC            | `UTC0`                       |
| US Eastern     | `EST5EDT,M3.2.0,M11.1.0`     |
| US Pacific     | `PST8PDT,M3.2.0,M11.1.0`     |
| UK             | `GMT0BST,M3.5.0/1,M10.5.0`   |
| Central Europe | `CET-1CEST,M3.5.0,M10.5.0/3` |
| India          | `IST-5:30`                   |
| Japan          | `JST-9`                      |

Where the time comes from is `clock.ntpSource`:

| Value    | Server used                                                                                     |
| -------- | ----------------------------------------------------------------------------------------------- |
| `pool`   | `pool.ntp.org` (default)                                                                        |
| `router` | The WiFi gateway address. Most routers answer NTP, and it keeps the clock working with no internet |
| `custom` | `clock.ntpServer`, a hostname or IP                                                             |

Changes apply immediately, no reboot needed, and the setup page shows the device's current local time and which server it is using. `GET /api/status` reports the same under `time`. Configs from firmware 0.4 and earlier that used `tzOffset` are converted to a fixed-offset TZ string on first boot.

```bash
curl -X PUT http://<device-ip>/api/config \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"clock":{"tz":"EST5EDT,M3.2.0,M11.1.0","ntpSource":"router"}}'
```

## Data sources

A data source is a URL the device polls and picks values out of. Each value becomes a layout key, so a screen can show the temperature outside, a stock price, a Home Assistant sensor, or the number of open issues on a repo. Add them under **Data sources** on `http://deskwig.local/setup`, or push one with the API:

```bash
curl -X PUT "http://<device-ip>/api/sources?id=weather" \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "url": "https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&current=temperature_2m,relative_humidity_2m,wind_speed_10m",
    "intervalS": 600,
    "fields": [
      { "name": "temp",     "path": "current.temperature_2m", "decimals": 0 },
      { "name": "humidity", "path": "current.relative_humidity_2m" },
      { "name": "wind",     "path": "current.wind_speed_10m", "decimals": 0 }
    ]
  }'
```

| Field            | Meaning                                                                                              |
| ---------------- | ---------------------------------------------------------------------------------------------------- |
| `id`             | Name used in layouts: 1–16 lowercase letters, digits or dashes. Up to 6 sources                      |
| `url`            | `http://` or `https://`, up to 191 characters. Put API keys that go in the query string here          |
| `intervalS`      | Seconds between fetches, minimum 10, default 300                                                     |
| `header`         | Optional `{ "name": "Authorization", "value": "Bearer ..." }` for APIs that want a key in a header    |
| `fields`         | Up to 8 of `{ "name", "path", "decimals" }`. `path` is a dot path into the JSON: `current.temp`, `items[0].price`, `data.0.value`. An empty path takes the whole response as text, for endpoints that answer with a bare value. `decimals` rounds numbers |

A layout then uses `{api.<id>.<field>}`, for example `{api.weather.temp}`. Every source also provides:

| Key                    | Value                                                                                   |
| ---------------------- | --------------------------------------------------------------------------------------- |
| `api.<id>.status`      | `ok`, `stale` (last fetch failed or is overdue, old values still shown), `error`, `wait` |
| `api.<id>.color`       | `ok`, `warn`, `bad` or `dim` for the same states, so a colour can track the fetch state  |
| `api.<id>.age`         | Time since the last successful fetch: `12s`, `5m`, `2h`                                 |
| `api.<id>.updated`     | Clock time of the last successful fetch                                                 |
| `api.<id>.error`       | The last error, e.g. `HTTP 401` or `not JSON: InvalidInput`                             |

Values are strings of up to 31 characters; nested objects and arrays are serialised and truncated. Like pings, a source is only fetched while a widget that references it is on screen, so an API quota is not spent on screens nobody is looking at. Saving a source or pressing **Test** on the setup page fetches it immediately and shows the extracted values. Fetches run in a background task, so a slow API never stalls the display, and the response is parsed with a filter that keeps only the requested paths, so large API responses cost little memory. Responses over 64 KB are rejected.

HTTPS connections are encrypted but the server certificate is **not** verified. Header values are stored in `/config.json` and never returned by the API; `GET /api/config` omits sources entirely. The editor's template picker includes a **Weather** layout built for the Open-Meteo source above.

## JSON layout widgets

A layout widget is a JSON file that lists what to draw. No compiler, no flashing: open `http://deskwig.local/editor`, pick a template, edit the text, and watch the preview. "Show on device" puts it on the real screen for 60 seconds, "Save" stores it at `/widgets/<id>.json` on the device and adds it to the widget list. Up to 12 layouts can be stored.

Eight layouts are preloaded on first boot so the device is useful out of the box: Big Clock, Stacked Clock, Ping Board, Status Lights, Latency Hero, Latency Meters, Dashboard and Network. Edit or delete them like any other widget; they are only written once, so your changes stick. The editor's template picker also offers Blank, Night Clock, Date Card, Server Rack and Signal Meter. Templates live in `src/layout/LayoutTemplates.cpp`, and adding one there makes it appear in the picker and, if marked `preload`, on new devices.

```json
{
  "name": "Big Clock",
  "elements": [
    {"type":"text","x":85,"y":96,"size":4,"align":"center","color":"text","text":"{time}"},
    {"type":"text","x":85,"y":176,"size":2,"align":"center","color":"text","text":"{date.day} {date.md}"},
    {"type":"line","x":6,"y":296,"x2":164,"y2":296,"color":"dim"},
    {"type":"text","x":6,"y":304,"size":1,"color":"dim","text":"{wifi.ssid}"},
    {"type":"text","x":164,"y":304,"size":1,"align":"right","color":"{wifi.color}","text":"{wifi.bars}"}
  ]
}
```

The screen is 170 × 320 with a black background. Text uses the 6 × 8 pixel built-in font scaled by `size`, so size 1 fits 28 columns, size 2 fits 14 and size 4 fits 7. Up to 32 elements per layout.

| Type   | Fields                                   | Notes                                                        |
| ------ | ---------------------------------------- | ------------------------------------------------------------ |
| `text` | `x y size align color text`              | `align` is `left` (default), `center` or `right`, relative to `x` |
| `line` | `x y x2 y2 color`                        |                                                              |
| `rect` | `x y w h color fill`                     | `fill` defaults to `false` (outline)                         |
| `bar`  | `x y w h color value`                    | Horizontal progress bar; `value` is 0–100 after expansion    |

`color` is a role name (`bg`, `text`, `dim`, `ok`, `warn`, `bad`, `accent`), a `#rrggbb` hex value, or a template that resolves to a role name such as `{ping.0.color}`. That is how a layout changes colour with the data without needing conditionals. A colour template that can't be resolved (for example a ping target that isn't configured) renders dim.

`text` and `value` are templates. Any `{key}` is replaced with a live value; unknown keys render as `--`.

| Key                                            | Value                                                       |
| ---------------------------------------------- | ----------------------------------------------------------- |
| `time`, `time.sec`, `time.ampm`                | `09:41`, `09:41:07`, `AM` (empty in 24-hour mode)           |
| `time.hour`, `time.min`                        | `09`, `41`, for stacked clocks                              |
| `date`, `date.day`, `date.md`, `date.dow`      | `2026-09-26`, `Sat`, `Sep 26`, `Saturday`                   |
| `date.year`                                    | `2026`                                                      |
| `wifi.ssid`, `wifi.ip`, `wifi.rssi`            | Network name, IP address, signal in dBm                     |
| `wifi.pct`, `wifi.bars`, `wifi.color`          | Signal as 0–100, `\|\|\|.`, and `ok`/`warn`/`bad`/`dim`     |
| `hostname`, `uptime`, `heap`                   | Device name, `3d 4h`, free heap in KB                       |
| `ping.count`                                   | Number of configured targets                                |
| `ping.N.name`, `ping.N.host`                   | Target N (0-based) as configured                            |
| `ping.N.ms`, `ping.N.status`, `ping.N.color`   | Latency or `--`; `ok`/`wait`/`down`; colour role            |
| `ping.N.bars`, `ping.N.trend`                  | Last 8 results as `\|\|.\|\|\|\|\|`; `^`, `v` or `>`            |

| `api.<id>.<field>`                             | A value from a [data source](#data-sources)                |
| `api.<id>.status`, `.color`, `.age`, `.updated`| Fetch state of that source                                  |

`N` can also be the target name, case-insensitive: `{ping.router.ms}`. Pings only run while a widget that shows ping data is on screen, and the same goes for data sources.

The editor page lists every key with its current value and inserts it at the cursor when tapped. The preview is drawn in the browser with the same font and colours as the device, so what you see is what you get. Layouts can also be pushed from a script:

```bash
curl -X PUT "http://<device-ip>/api/layouts?id=clock" \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  --data @clock.json
```

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

It will appear in the web selector automatically. The `ScreenManager` holds up to 16 widgets (native plus layouts). Rendering is double-buffered through a full-screen `LGFX_Sprite`, so widgets just draw and don't need to worry about flicker.

## Project layout

```
src/
  main.cpp            display init, recovery jumper, boot sequence, main loop
  app/                Board pins, Widget interface, ScreenManager, status LED, system screens
  widgets/            PingWidget, ClockWidget
  layout/             JSON layout widgets: parser/renderer, template keys, file store + preview, built-in templates
  net/                WiFi manager (STA/hotspot/captive portal/mDNS), TCP ping, NTP/timezone, data sources (HTTP fetch task)
  web/                Settings (NVS + LittleFS JSON), async web server, API, HTML pages, editor
platformio.ini        board, partition table, library deps
```

## License

MIT – see [LICENSE](LICENSE).
