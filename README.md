# DeskWiG

DeskWiG (Desk Widget) started as a Christmas gift for my dad. A small 3D-printed desktop display built around an ESP32-S3 and a 1.9" IPS screen. The firmware is a simple widget framework: each screen is a `Widget` class, and you pick which one is showing from a web page served by the device.

It currently ships with two widgets (either can be deleted from the widgets page if you only want your own, and restored from the same place):

- **Ping / Server Status** – TCP-connects to a list of hosts on an interval and shows latency, an up/down sparkline, a trend arrow, WiFi signal strength, and the device IP. Targets are grouped into *Services* and *Servers*.
- **Clock** – NTP-synced clock with 12/24-hour display. Time can come from the internet, from your router, or from any NTP server you name; timezones use POSIX TZ strings so daylight saving is automatic.

You can also build your own screens without a toolchain: [JSON layout widgets](#json-layout-widgets) are written in the browser, previewed live, and stored on the device. They can show live values from any web API through [data sources](#data-sources): weather, stock prices, a Home Assistant sensor, anything that answers HTTP with JSON or text. Native widgets in C++ are still a matter of implementing four methods (see [Writing a widget](#writing-a-widget)).

## Hardware

| Part                                                | Link                                                                             |
| --------------------------------------------------- | -------------------------------------------------------------------------------- |
| ESP32-S3-DevKitC-1 (N16R8: 16 MB flash, 8 MB PSRAM) | https://a.co/d/0ay0gra1                                                          |
| 1.9" 170×320 IPS LCD, ST7789V2, SPI                 | https://a.co/d/0bqQSNMt                                                          |
| Printable enclosure (STL)                           | https://makerworld.com/en/models/2918927-esp-32-desktop-widget#profileId-3265758 |
| Rotary encoder with push button (KY-040 module), optional | any                                                                        |

The DevKitC's onboard WS2812 RGB LED (GPIO 48) is used as a status light. GPIO 4 is the recovery jumper (see [Recovery](#recovery-jumper)). GPIO 5, 6 and 7 take an optional rotary encoder (see [Rotary encoder](#rotary-encoder)).

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

| Encoder pin | ESP32-S3 GPIO |
| ----------- | ------------- |
| CLK (A)     | 5             |
| DT (B)      | 6             |
| SW          | 7             |
| +           | 3V3           |
| GND         | GND           |

All pins are defined in `src/app/Board.h`.

### Rotary encoder

A KY-040 style encoder (or a bare encoder plus a button) turns the knob into a widget selector, no phone needed. The knob steps through widgets in the order shown on `/widgets`, where rows can be dragged to rearrange them:

- **Turn** goes to the next or previous widget, in the order the Widgets page lists them, wrapping at the ends. A banner with the widget's name and position appears for a moment, and the choice is saved so it survives a reboot. Turning also ends an editor preview.
- **Click** shows the connection screen (address and API key) for 15 seconds; click again to dismiss it.

Wire CLK, DT and SW to GPIO 5, 6 and 7, the module's `+` to 3V3 and `GND` to ground. The firmware enables the internal pull-ups, so a bare encoder works with its common pins to ground and no resistors; the KY-040's own pull-ups are fine alongside. Nothing needs configuring: with no encoder connected the pins simply stay high.

If clockwise goes the wrong way, swap CLK and DT or set `ENC_REVERSE` in `src/app/Board.h`. If one click of the knob skips a widget or needs two clicks, set `ENC_STEPS_PER_DETENT` to 2 or 4 to match your encoder. Rotation is decoded in interrupts with a state table, so bounce doesn't register as extra steps.

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project. Open the folder in VS Code with the PlatformIO extension, or from the command line:

```bash
pio run -t upload
pio device monitor
```

Once the device is on your network you can update it without a cable: build with `pio run`, then upload `.pio/build/esp32-s3-devkitc-1/firmware.bin` under **Firmware** on `http://deskwig.local/setup`, or from a shell:

```bash
curl -X POST http://deskwig.local/api/system/update \
  -H "Authorization: Bearer <key>" \
  -F "file=@.pio/build/esp32-s3-devkitc-1/firmware.bin"
```

The image goes into the spare OTA app slot and the device reboots into it; settings, layouts, fonts and images are kept.

Dependencies (LovyanGFX, Adafruit NeoPixel, ESP32Async/ESPAsyncWebServer, ESP32Async/AsyncTCP, ArduinoJson) are pulled in automatically from `platformio.ini`. The partition table (`default_16MB.csv`) gives two OTA app slots and a 3.4 MB LittleFS partition.

## First boot and WiFi setup

WiFi credentials, the API key and the login account are stored in the ESP32's NVS flash, not in the source. On first boot there are none, so the device starts a setup hotspot and shows the details on screen:

|          |                      |
| -------- | -------------------- |
| SSID     | `DeskWiG-Setup`    |
| Password | `configureme`        |
| URL      | http://192.168.4.1   |

1. Join the hotspot from a phone or laptop. A setup page should open automatically (captive portal). If it doesn't, browse to the URL above.
2. Tap **Scan for networks**, pick yours, enter the password, and tap **Join**.
3. The page reports when the device has connected and shows its new address. The hotspot stays up for 20 seconds after connecting so you can read it, then turns off.
4. The device screen shows the address and an 8-character API key for 30 seconds, then switches to the active widget.
5. Open the address in a browser. The login page asks for that key once, then for a username and password of your choice. Nothing on the device can be changed until the account exists.

The device is reachable at `http://deskwig.local` (mDNS) or its IP. The name is changeable on the setup page.

If the saved network can't be reached within 15 seconds the hotspot comes back, and the device keeps retrying the saved network once a minute so it recovers by itself after a router reboot. Requests made over the hotspot don't need a login, since anyone standing next to the device with the hotspot password is already trusted for setup.

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
- **Keep it bridged for 8 seconds:** the screen counts down, then the device erases all settings (LittleFS and NVS) and restarts into setup mode. A new API key is generated and the login account is gone.

A tactile switch or two exposed pads on the enclosure work equally well.

## Web interface

Once on your network, the device serves:

| Route                           | Auth | Purpose                                                        |
| ------------------------------- | ---- | -------------------------------------------------------------- |
| `/login`                        | no   | Create the account, log in, or reset a forgotten password      |
| `/setup`                        | no   | WiFi scan/join, device name, clock, data sources, account      |
| `/widgets`                      | no   | Switch the active screen, drag to reorder, delete or restore widgets |
| `/editor`                       | no   | In-browser editor for JSON layout widgets                      |
| `GET /api/auth`                 | no   | Whether an account exists and whether this browser is logged in |
| `POST /api/auth/setup`          | key  | JSON `{"key","user","pass"}`. Creates the account, or replaces it and rotates the key |
| `POST /api/auth/login`          | no   | JSON `{"user","pass"}`. Sets the session cookie                |
| `POST /api/auth/logout`         | no   | Drops the session                                              |
| `POST /api/auth/reveal`         | no   | Shows the API key on the device screen for 60 s                |
| `GET /api/auth/key`             | yes  | The API key, for scripts                                       |
| `PUT /api/auth/password`        | yes  | JSON `{"current","pass"}`. Changes the password               |
| `GET /api/status`               | no   | Firmware, uptime, heap, WiFi state, active widget, clock sync  |
| `GET /api/wifi`                 | no   | Connection state, SSID, IP, hostname, hotspot state            |
| `GET /api/wifi/scan`            | no   | Starts a scan; poll until `status` is `done`                   |
| `POST /api/wifi/join`           | yes  | JSON `{"ssid","pass"}`. Saves and connects, hotspot stays up   |
| `POST /api/wifi/forget`         | yes  | Clears credentials, returns to hotspot                         |
| `GET /api/config`               | no   | Hostname, ping interval, clock, LED settings                   |
| `PUT /api/config`               | yes  | JSON with any subset of the above; saved immediately           |
| `GET /api/fonts`                | no   | Fonts on the device with size and whether built in             |
| `POST /api/fonts?name=<name>`   | yes  | Multipart upload of a `.ttf` (field `file`), up to 2 MB       |
| `DELETE /api/fonts?name=<name>` | yes  | Remove an uploaded font                                        |
| `GET /fonts/<name>.ttf`         | no   | The font file, used by the editor's preview                    |
| `GET /api/images`               | no   | Uploaded images with type and size                             |
| `POST /api/images?name=<name>`  | yes  | Multipart upload of a PNG, JPEG or GIF (field `file`), up to 512 KB |
| `DELETE /api/images?name=<name>`| yes  | Remove an image                                                |
| `GET /img/<name>`               | no   | The image file, used by the editor's preview                   |
| `GET /api/sources`              | no   | Data sources with fetch state and current values (header value redacted) |
| `PUT /api/sources?id=<id>`      | yes  | Create or replace a data source (see [Data sources](#data-sources)) |
| `DELETE /api/sources?id=<id>`   | yes  | Remove a data source                                           |
| `POST /api/sources/test?id=<id>`| yes  | Fetch it now; poll `GET /api/sources` for the result           |
| `POST /api/sources/discover`    | yes  | JSON `{"url","header":{"name","value"}}`. Fetch once and list every JSON path; poll the GET |
| `GET /api/sources/discover`     | yes  | State of the last discovery and its paths with sample values   |
| `GET /api/widgets`              | no   | JSON list of widgets (`name`, `key`, `builtin`), the active index and deleted built-ins |
| `POST /api/widgets` (`index=N`) | yes  | Switch the active widget; choice is persisted                  |
| `DELETE /api/widgets?key=<key>` | yes  | Remove a widget: a built-in is hidden, a layout is deleted     |
| `PUT /api/widgets/order`        | yes  | JSON `{"order":["key",...]}`. Order for the list and the encoder knob; persisted |
| `POST /api/widgets/restore` (`key=K`) | yes | Bring back a deleted built-in widget                       |
| `GET /api/layouts`              | no   | List of layout widgets (`id`, `name`, `index`) and free slots  |
| `GET /api/layouts?id=<id>`      | no   | The stored layout JSON                                         |
| `PUT /api/layouts?id=<id>`      | yes  | Create or replace a layout; validated, saved, applied at once  |
| `DELETE /api/layouts?id=<id>`   | yes  | Remove a layout widget                                         |
| `POST /api/layouts/preview`     | yes  | Show a layout on the device for 60 s without saving it         |
| `DELETE /api/layouts/preview`   | yes  | End the preview early                                          |
| `GET /api/layouts/data`         | no   | Every template key with its current value                      |
| `GET /api/layouts/templates`    | no   | Built-in templates with their layout JSON                      |
| `POST /api/system/update`       | yes  | Multipart upload of `firmware.bin` (field `file`); writes the spare OTA slot and reboots |
| `POST /api/system/reboot`       | yes  | Restart                                                        |
| `POST /api/system/reset`        | yes  | Factory reset and restart                                      |

Authenticated routes accept either the session cookie the login page sets or an `Authorization: Bearer <key>` header, except when the request comes in over the setup hotspot. Neither works until a username and password have been created. The key is 8 characters from the device screen; the setup page shows it again to a logged-in user. Five wrong passwords or keys in a row lock logins for a minute.

Forgot the password? The **Forgot your password?** link on the login page puts the key on the device screen for a minute. Enter it with a new username and password; the key is replaced afterwards, so update any scripts that use it.

```bash
curl -X POST http://<device-ip>/api/widgets \
  -H "Authorization: Bearer <key>" \
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

Everything except WiFi credentials, the key and the account lives in `/config.json` on the LittleFS partition and looks like this:

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
  -H "Authorization: Bearer <key>" \
  -H "Content-Type: application/json" \
  -d '{"clock":{"tz":"EST5EDT,M3.2.0,M11.1.0","ntpSource":"router"}}'
```

## Data sources

A data source is a URL the device polls and picks values out of. Each value becomes a layout key, so a screen can show the temperature outside, a stock price, a Home Assistant sensor, or the number of open issues on a repo. Add them under **Data sources** on `http://deskwig.local/setup`: enter the URL, press **Discover keys** and the device fetches it and lists every value it finds, so a tap adds the field with its path filled in. Or push one with the API:

```bash
curl -X PUT "http://<device-ip>/api/sources?id=weather" \
  -H "Authorization: Bearer <key>" \
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

Fifty-one more example layouts live in [`examples/widgets/`](examples/widgets/), from stock tickers, crypto boards and exchange rates to launches, earthquakes, space weather, surf and rain forecasts, Pi-hole and OctoPrint, each showing a different feature and a matching LED behaviour, with the data sources they need documented alongside. They are not preloaded; paste one into the editor or push it with the API.

Eight layouts are preloaded on first boot so the device is useful out of the box: Big Clock, Stacked Clock, Ping Board, Status Lights, Latency Hero, Latency Meters, Dashboard and Network. Edit or delete them like any other widget; they are only written once, so your changes stick. The editor's template picker also offers Blank, Night Clock, Date Card, Server Rack, Signal Meter, Weather and Cards. Templates live in `src/layout/LayoutTemplates.cpp`, and adding one there makes it appear in the picker and, if marked `preload`, on new devices.

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

The screen is 170 × 320 with a black background. Text uses the 6 × 8 pixel built-in font scaled by `size`, so size 1 fits 28 columns, size 2 fits 14 and size 4 fits 7. Up to 63 elements per layout, nested up to 6 deep.

### Elements

| Type       | Fields                                  | Notes                                                                       |
| ---------- | --------------------------------------- | --------------------------------------------------------------------------- |
| `text`     | `text`, `w` `h` optional                | Sizes itself to the text unless `w`/`h` are given                           |
| `line`     | `w` `h`, or `x2` `y2` when absolute     | In flow, a line with only `h: 1` is a full-width rule                       |
| `rect`     | `w` `h` `fill`                          | `fill: true` fills with `color`, otherwise outlines. `style.radius` rounds it |
| `bar`      | `w` `h` `value`                         | Horizontal progress bar; `value` is 0–100 after expansion                   |
| `box`      | `children`                              | A container; see [Flow layout](#flow-layout)                                |
| `circle`   | `w` or `r`                              | Filled with `color`; `style.border` outlines                                |
| `ellipse`  | `w` `h`                                 |                                                                             |
| `arc`      | `w`, `value`, `start`, `end`            | Ring gauge: filled from `start` to `end` degrees (0 = top, clockwise) in proportion to `value` 0–100. `style.thickness` sets the ring width, `style.background` the track (default dim) |
| `triangle` | `points: [[x,y],[x,y],[x,y]]`           | Points are relative to the element's own top-left                           |
| `polygon`  | `points: [[x,y], ...]`                  | 3 to 8 points                                                               |
| `image`    | `src`, `w` `h`, `refresh`               | An uploaded image by name or an http(s) URL; see [Images](#images)          |

### Flow layout

Elements are laid out like blocks in HTML: the screen is a column, and each element takes the next slot. A `box` groups children and stacks them along its `direction` (`column` or `row`) with a `gap`, inside its `padding`. Children fill the cross axis by default (`align: stretch`), so a text element in a column is as wide as the box and its `align` positions the text within it. Nothing overlaps when a value changes length.

```json
{
  "name": "Cards",
  "style": {"direction":"column","gap":8,"padding":6},
  "styles": {
    "label": {"color":"dim"},
    "card":  {"background":"#101820","border":"#2a3a4a","radius":8,"padding":8,"gap":4}
  },
  "elements": [
    {"type":"box","class":"card","children":[
      {"type":"text","class":"label","text":"TIME"},
      {"type":"text","text":"{time}","style":{"size":4,"align":"center"}}
    ]},
    {"type":"box","class":"card","style":{"direction":"row","align":"center","gap":10},"children":[
      {"type":"arc","w":56,"value":"{wifi.pct}","color":"{wifi.color}"},
      {"type":"text","text":"{wifi.rssi} dBm","size":2}
    ]}
  ]
}
```

An element with both `x` and `y` is positioned absolutely inside its parent's content box instead of flowing, which is how every layout written before this model still renders unchanged. Absolute text without `w` keeps its old anchor semantics: `x` is the left, centre or right edge according to `align`.

Box properties, all in `style`:

| Property    | Values                                  | Meaning                                                              |
| ----------- | --------------------------------------- | -------------------------------------------------------------------- |
| `direction` | `column` (default), `row`               | Main axis                                                            |
| `gap`       | px                                      | Space between children                                               |
| `padding`   | px                                      | Space inside the box edge                                            |
| `align`     | `stretch` (default), `start`, `center`, `end` | Cross-axis placement of children                               |
| `justify`   | `start` (default), `center`, `end`, `between` | Main-axis distribution when children do not fill the box       |

The top-level `style` applies to the implicit root box, so `{"style":{"direction":"column","gap":8,"padding":6}}` is how a layout gets margins and spacing.

### Fonts

Text uses the built-in 6 × 8 bitmap font scaled by `size` (1–40) unless `font` names a TrueType font, in which case `size` is the line height in pixels (6–160):

```json
{"type":"text","text":"{time}","font":"bold","size":48,"align":"center"}
{"type":"text","text":"☀ {api.weather.temp}°","font":"sans","size":20}
```

Three fonts are built into the firmware: **sans** and **bold** (Inter, Latin subset) and **emoji** (Noto Emoji, monochrome, about 1,300 common emoji). Upload more `.ttf` files (up to 2 MB each, 12 fonts total) under **Fonts** on the setup page or with `POST /api/fonts?name=<name>` as a multipart form with a `file` field. Uploaded fonts live in `/fonts` on the filesystem and are loaded into PSRAM the first time a layout uses them.

Any character a font lacks falls back to the emoji font and then to sans, so emoji work in every font, including the bitmap one, and render in the element's colour. Glyphs are rasterised on the device with [stb_truetype](https://github.com/nothings/stb) into a cache in PSRAM. That library does no bounds checking on the font file, so only upload fonts you trust. The editor loads the same font files from the device, so the preview matches. Built-in fonts are subset with `tools/make_fonts.py`; regenerate them from the full fonts if you want a different character set.

### Images

Upload PNG, JPEG or animated GIF files (up to 512 KB each, 24 in total) under **Images** on the setup page or with `POST /api/images?name=<name>` as a multipart form with a `file` field. The screen is 170 × 320, so resize images first. Then:

```json
{"type":"image","src":"sun","w":48}
{"type":"image","src":"https://example.com/radar.png","w":158,"h":120,"refresh":300,"style":{"fit":"cover"}}
{"type":"box","style":{"image":"sky"},"children":[ ... ]}
```

An image element sizes itself to the picture, or keeps the aspect ratio when only `w` or `h` is given. `style.fit` is `contain` (default), `cover` or `stretch`. A box's `style.image` is drawn to cover the box behind its children, which is how a layout gets a picture background. Animated GIFs play at their own frame rate.

`src` can be an http(s) URL. Like data sources, a URL image is downloaded only while a layout showing it is on screen, then every `refresh` seconds (default 600, minimum 30), so a weather radar or a camera still stays current without hammering the server. HTTPS is encrypted but the certificate is not verified. Until the first download finishes the element shows a dim frame.

Images are decoded once per size into sprites cached in PSRAM, so drawing them each frame is cheap; a full-screen background costs about 108 KB of PSRAM. The editor's preview loads stored images from the device and URLs directly (a GIF shows its first frame there).

### Status LED

Each layout controls the RGB status LED while it is on screen through a top-level `led` object. A layout without one leaves the LED off. System states (setup hotspot, connecting, resetting) still take over, and the device-wide LED enable and brightness on the setup page still apply on top.

```json
"led": {
  "color": "{ping.0.color}", "mode": "breathe", "speed": 3000, "brightness": 50,
  "rules": [
    {"when": "ping.0.ms > 100", "color": "warn", "mode": "blink", "speed": 500},
    {"key": "ping.0.status", "is": "down", "color": "bad", "mode": "blink", "speed": 200},
    {"when": "time.hour >= 22 || time.hour < 7", "mode": "off"}
  ]
}
```

| Field        | Meaning                                                                                       |
| ------------ | --------------------------------------------------------------------------------------------- |
| `color`      | A role name, `#rrggbb`, or a template such as `{api.weather.color}`                            |
| `mode`       | `solid` (default), `breathe`, `blink`, `pulse`, `rainbow` or `off`                             |
| `speed`      | Milliseconds per cycle of the animation (100–60000, default 2000)                             |
| `brightness` | 0–100 for this widget, scaled by the device brightness setting                                |
| `rules`      | Up to 6 situations, checked in order; the first that matches overrides only the fields it sets |

A rule matches with `when`, an expression that counts as true when non-zero (any key, comparisons, `&&`, `\|\|`), or with `key` and `is`, which compares a key's text case-insensitively, which is how string states like `down`, `stale` or `error` are tested. Rules are re-evaluated four times a second.

### Styles

Style properties can be flat fields on the element (`color`, `size`, `align`, `fill`), a `style` object, or a named entry in the top-level `styles` map applied with `class`. Later ones win: class, then flat fields, then the `style` object.

| Property                    | Applies to                | Meaning                                                 |
| --------------------------- | ------------------------- | ------------------------------------------------------- |
| `color`                     | all                       | Text, fill or outline colour                            |
| `background` (or `bg`)      | box, rect, text, shapes, bar, arc | Fill behind the element; the track for bar and arc |
| `border`, `borderWidth`     | box, rect, shapes         | Outline colour and width                                |
| `radius`                    | box, rect, bar            | Corner radius                                           |
| `font`                      | text                      | TrueType font name; empty for the bitmap font           |
| `size`                      | text                      | Bitmap scale 1–8, or pixel line height 6–160 with a font |
| `align`                     | text, box                 | Text alignment, or a box's cross-axis alignment         |
| `fill`                      | rect, shapes              | Fill with `color`                                       |
| `thickness` (or `width`)    | arc, line                 | Ring width or line width                                |
| `position`                  | any                       | `absolute` positions at `x`/`y` even if one is missing  |
| `fit`                       | image                     | `contain`, `cover` or `stretch`                         |
| `image`                     | box                       | Background image (name or URL), drawn to cover the box  |

`color`, `background` and `border` take a role name (`bg`, `text`, `dim`, `ok`, `warn`, `bad`, `accent`), a `#rrggbb` hex value, or a template that resolves to a role name such as `{ping.0.color}`. That is how a layout changes colour with the data without needing conditionals. A colour template that can't be resolved (for example a ping target that isn't configured) renders dim.

### Keys

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

### Math in braces

A brace that is not a plain key is evaluated as arithmetic, with keys as variables. That is how a layout converts units or scales a value for a bar:

```json
{"type":"text","text":"{round(api.weather.temp * 9/5 + 32, 1)} F"}
{"type":"bar","value":"{min(ping.0.ms / 2, 100)}"}
{"type":"text","text":"{ping.router.ms - ping.gateway.ms} ms"}
```

| Syntax                                        | Notes                                                                            |
| --------------------------------------------- | -------------------------------------------------------------------------------- |
| `+ - * / % ^ ( )`                             | Usual precedence; `^` is power                                                   |
| `round(x, n)`                                 | Round to `n` decimals (0–6) and print exactly that many. `round(x)` is to a whole number |
| `abs(x)`, `floor(x)`, `ceil(x)`, `sqrt(x)`    |                                                                                  |
| `min(a, b, ...)`, `max(a, b, ...)`            | Up to four arguments                                                             |
| `clamp(x, lo, hi)`                            |                                                                                  |
| `< > <= >= == !=`                             | Comparisons give `1` or `0`                                                      |
| `&&`, `\|\|`, `!`                              | Logic on those                                                                   |
| `if(cond, a, b)`                              | `a` when `cond` is non-zero, else `b`                                            |

Values are read as numbers, and a leading number is enough, so `{api.weather.age}` reading `12s` gives 12. Without `round`, whole numbers print without decimals and anything else with up to two. Any unknown key, non-numeric value or division by zero makes the whole brace `--`, the same as an unknown key. Put spaces around a minus after a key that contains dashes (`{api.my-source.temp - 3}`), since `my-source` is read as one name first.

The editor page has a **Design** panel next to the code: click an element on the preview or in the element tree to select it, edit its properties in the inspector (type, text, font, colours with a picker, size, alignment, fill, radius, borders, flow settings, image, points, arc angles), drag fixed-position elements on the preview, and drag rows in the tree to reorder them or drop them into a box. The toolbar adds, duplicates, moves and deletes elements, and the widget's own entry edits the name, root layout and the LED rules. Every change is written into the JSON, and typing in the JSON updates the tree, so both views always agree and undo covers both.

The code side is a real code editor: JSON highlighting, bracket matching and auto-closing, folding, search and replace (Ctrl+F), undo and redo that work anywhere on the page (Ctrl+Z, Ctrl+Y, also for changes made in the Design panel), a Format button, Ctrl+S to save, and validation errors underlined on the exact element, style or LED rule they refer to. It is CodeMirror, bundled by `tools/editor` and served gzipped from the firmware at `/cm.js`, so it works with no internet. It lists every key with its current value and inserts it at the cursor when tapped. The preview is drawn in the browser with the same font and colours as the device, so what you see is what you get. Layouts can also be pushed from a script:

```bash
curl -X PUT "http://<device-ip>/api/layouts?id=clock" \
  -H "Authorization: Bearer <key>" \
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
  layout/             JSON layout widgets: parser/renderer, template keys, expressions, fonts, images, file store + preview, built-in templates
examples/widgets/     25 example layouts with a README of the data sources they use
tools/editor/         npm project that builds web/cm.js.gz, the CodeMirror bundle for the editor
web/                  cm.js.gz, embedded in the firmware
fonts/                TrueType subsets embedded in the firmware (sans, bold, emoji), built by tools/make_fonts.py
lib/stb/              stb_truetype (public domain) font rasteriser
  net/                WiFi manager (STA/hotspot/captive portal/mDNS), TCP ping, NTP/timezone, data sources (HTTP fetch task)
  web/                Settings (NVS + LittleFS JSON), async web server, API, HTML pages, editor
platformio.ini        board, partition table, library deps
```

## License

MIT – see [LICENSE](LICENSE).
