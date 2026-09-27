# Example widgets

Twenty-five layouts that show what DeskWiG layouts can do. They are not preloaded on the device: open `http://deskwig.local/editor`, paste one into the editor, and press **Save**, or push it from a shell:

```bash
curl -X PUT "http://deskwig.local/api/layouts?id=big-clock" \
  -H "Authorization: Bearer <token>" -H "Content-Type: application/json" \
  --data @examples/widgets/big-clock.json
```

Every layout renders on its own, but the ones marked with a data source show `--` until that source exists. The sources are listed below with the exact fields the layouts expect; add them under **Data sources** on `http://deskwig.local/setup` or with `PUT /api/sources?id=<id>`.

| File | Shows off | Needs |
| --- | --- | --- |
| `big-clock.json` | TrueType bold clock, flow layout with `justify: center` | |
| `flip-clock.json` | 104 px digits in rounded tiles, named style | |
| `night-clock.json` | Custom hex colours for a dim bedside clock | |
| `world-clock.json` | Math on `time.hour` with `%` for other time zones | |
| `day-progress.json` | Bars driven by expressions, minutes-left arithmetic | |
| `calendar-card.json` | Cards with background and border, big date | |
| `weather-card.json` | Emoji, data source keys, `.age` and `.color` status keys | `weather` |
| `weather-rings.json` | Ring gauges with centred labels (absolute text inside a box) | `weather` |
| `weather-fahrenheit.json` | Unit conversion with `round()` | `weather` |
| `air-quality.json` | Value-coloured bar using a source's `.color`, `min()` | `aqi` |
| `sunrise-sunset.json` | Rows of emoji and text, string fields from an API | `sun` |
| `crypto-ticker.json` | Prices, percent change, large-number math | `btc` |
| `github-repo.json` | Three stat rows with `justify: between` and emoji | `repo` |
| `iss-tracker.json` | Fast-refresh source, `round(x, 2)` | `iss` |
| `home-assistant.json` | A source with an `Authorization` header | `ha` |
| `server-dots.json` | Ping status circles, nested rows, target count | ping targets |
| `latency-bars.json` | Bars scaled with `min()`, trend arrows, history | ping targets |
| `latency-gauges.json` | Partial arcs (`start`/`end`) as gauges | ping targets |
| `network-panel.json` | WiFi ring, key/value rows, ping by target name | ping target named `Router` |
| `system-monitor.json` | Card grid from one named style, heap and uptime | |
| `shapes-showcase.json` | Circle, ellipse, rounded rect, triangle, polygon, arc | |
| `styles-demo.json` | Classes, inline style precedence, `justify`, `align`, pills | |
| `emoji-board.json` | Emoji in every font and colour | `weather` (one line) |
| `typography.json` | Bitmap sizes next to sans and bold pixel sizes | |
| `photo-frame.json` | URL image as a full-screen background with an overlay | internet |

## Data sources used

Replace the coordinates, symbols and tokens with your own. Field names must match exactly.

**weather** (Open-Meteo, no key)

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&current=temperature_2m,relative_humidity_2m,wind_speed_10m,apparent_temperature
interval: 600
fields:   temp = current.temperature_2m (decimals 0)
          humidity = current.relative_humidity_2m
          wind = current.wind_speed_10m (decimals 0)
          feels = current.apparent_temperature (decimals 0)
```

**aqi** (Open-Meteo air quality, no key)

```
url:      https://air-quality-api.open-meteo.com/v1/air-quality?latitude=42.36&longitude=-71.06&current=european_aqi,pm2_5,pm10
interval: 900
fields:   aqi = current.european_aqi
          pm25 = current.pm2_5
          pm10 = current.pm10
```

**sun** (Open-Meteo daily, no key)

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&daily=sunrise,sunset,uv_index_max&timezone=auto&forecast_days=1
interval: 3600
fields:   sunrise = daily.sunrise.0
          sunset = daily.sunset.0
          uv = daily.uv_index_max.0 (decimals 0)
```

The sunrise strings look like `2026-09-26T06:45`; the layout shows them as is.

**btc** (CoinGecko, no key)

```
url:      https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum&vs_currencies=usd&include_24hr_change=true&include_market_cap=true
interval: 120
fields:   usd = bitcoin.usd (decimals 0)
          change = bitcoin.usd_24h_change
          eth = ethereum.usd (decimals 0)
          cap = bitcoin.usd_market_cap
```

**repo** (GitHub, no key for public repos)

```
url:      https://api.github.com/repos/BeekrBonkr/DeskWiG
interval: 600
fields:   name = name
          desc = description
          stars = stargazers_count
          forks = forks_count
          issues = open_issues_count
```

**iss** (Open Notify, plain http)

```
url:      http://api.open-notify.org/iss-now.json
interval: 30
fields:   lat = iss_position.latitude
          lon = iss_position.longitude
```

**ha** (Home Assistant, long-lived access token in a header)

```
url:      http://homeassistant.local:8123/api/states/sensor.living_room_temperature
interval: 60
header:   Authorization = Bearer <long-lived token>
fields:   state = state
          unit = attributes.unit_of_measurement
          name = attributes.friendly_name
```

## LED behaviour

Every example carries a `led` block, so the RGB LED does something sensible while it is on screen: clocks breathe softly and go dark at night, the ping and network layouts show the worst target's colour and blink red when something is down, weather layouts warn on heat, frost, wind or bad air, the crypto ticker flashes green or red on big moves, and the showcase layouts cycle through a rainbow. Delete the block to keep the LED off, or edit the `rules` to taste.

## Notes

- The screen is 170 × 320. Text longer than the width is clipped by its box, so keep labels short or drop the font size.
- `world-clock.json` hard-codes hour offsets from US Eastern; edit the numbers in the four `{(time.hour + N) % 24}` expressions for your zone.
- `photo-frame.json` points at a placeholder PNG on httpbin.org. Put your own image URL there, or upload an image on the setup page and use its name.
- Every file passes the same validation the editor and the device run, so they load as they are.
