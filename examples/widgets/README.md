# Example widgets

Fifty-two layouts that show what DeskWiG layouts can do. They are not preloaded on the device: open `http://deskwig.local/editor`, paste one into the editor, and press **Save**, or push it from a shell:

```bash
curl -X PUT "http://deskwig.local/api/layouts?id=big-clock" \
  -H "Authorization: Bearer <key>" -H "Content-Type: application/json" \
  --data @examples/widgets/big-clock.json
```

Every layout renders on its own, but the ones marked with a data source show `--` until that source exists. The sources are listed below with the exact fields the layouts expect; add them under **Data sources** on `http://deskwig.local/setup` (paste the URL, press **Discover keys** and tap the paths) or with `PUT /api/sources?id=<id>`. Field names must match exactly.

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
| **Finance** | | |
| `stock-ticker.json` | One stock: price, change, day-range bar, LED on big moves | `stock` |
| `stock-board.json` | Three stocks with a centred change bar under each | `stock1` `stock2` `stock3` |
| `crypto-board.json` | Four coins from one CoinGecko call, 24h change bar | `coins` |
| `fear-greed.json` | Three-quarter arc gauge (`start`/`end` past 360) with the label inside | `fng` |
| `mempool-fees.json` | A plain-text source (empty path) for the block height, fee tiers | `fees` `tip` |
| `forex-rates.json` | Exchange rates and a reverse conversion with `/` | `fx` |
| `power-price.json` | Spot electricity price in ct/kWh, LED by price band | `power` |
| **Space and science** | | |
| `astronauts.json` | Array items by index (`people.0.name`), who is in orbit | `astros` |
| `next-launch.json` | Nested paths three levels deep, `key`/`is` LED rules on a status string | `launch` |
| `earthquakes.json` | GeoJSON feed: count plus the latest magnitude and place | `quakes` |
| `space-weather.json` | Object keys that look like numbers (`0.G.Scale`), aurora alert LED | `swpc` |
| `space-dashboard.json` | Three sources on one screen with emoji icons in rows | `iss` `astros` `neo` |
| **Weather** | | |
| `weather-hourly.json` | Condition tiles lit by `if()` on the weather code and `is_day`, hourly fields at index 0 thanks to `forecast_hours`, rain chance rows, stat cards | `wx` |
| `rain-next-hours.json` | Six hourly bars from one array (`hourly.x.0` to `.5`), `max()` | `rain` |
| `forecast-3day.json` | Daily highs, lows and rain chance for three days | `fc` `weather` |
| `barometer.json` | Gauge over a 950–1050 hPa window with `clamp()` | `baro` |
| `surf-report.json` | Marine API: wave height, period and direction | `surf` `weather` |
| `wind-meter.json` | Beaufort number from km/h with `^` (power) | `wind` |
| `daylight-arc.json` | Half-ring sun arc from unix timestamps, minutes to sunset | `sun2` |
| **Home and tech** | | |
| `pihole.json` | Pi-hole blocked-percentage gauge, `.status` in a rule | `pihole` |
| `3d-printer.json` | OctoPrint progress ring with an `X-Api-Key` header | `printer` |
| `steam-players.json` | Live player count scaled with `/ 1000` and a `k` suffix | `steam` |
| `package-stats.json` | npm and PyPI weekly downloads in millions and thousands | `npm` `pypi` |
| `github-user.json` | Three stat columns from a user profile plus repo stars | `gh` `repo` |
| `f1-next-race.json` | Deeply nested Ergast paths, next race and start time | `f1` |
| `next-holiday.json` | Top-level array (`0.name`, `1.name`), next three days off | `holiday` |
| **No API** | | |
| `binary-clock.json` | Bits as square bars: `floor(time.hour / 16) % 2 * 100` fills or empties a cell | |

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

**stock** (Finnhub, free API key from finnhub.io)

```
url:      https://finnhub.io/api/v1/quote?symbol=AAPL&token=<your key>
interval: 120
fields:   last = c
          change = d
          pct = dp
          hi = h
          lo = l
          open = o
          prev = pc
```

Quotes update during market hours; outside them the values are the last close. **stock1**, **stock2** and **stock3** for `stock-board.json` are the same source three times with `symbol=AAPL`, `MSFT` and `NVDA` and the fields `price = c` and `pct = dp`. Edit the labels in the layout to match your symbols. No key at all? Yahoo's chart endpoint works without one: `https://query1.finance.yahoo.com/v8/finance/chart/AAPL?range=1d&interval=1d` with `price = chart.result.0.meta.regularMarketPrice` and `prev = chart.result.0.meta.chartPreviousClose`.

**coins** (CoinGecko, no key)

```
url:      https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum,solana,dogecoin&vs_currencies=usd&include_24hr_change=true
interval: 120
fields:   btc = bitcoin.usd            btcc = bitcoin.usd_24h_change
          eth = ethereum.usd           ethc = ethereum.usd_24h_change
          sol = solana.usd             solc = solana.usd_24h_change
          doge = dogecoin.usd          dogec = dogecoin.usd_24h_change
```

**fng** (alternative.me crypto Fear & Greed index, no key)

```
url:      https://api.alternative.me/fng/
interval: 1800
fields:   value = data.0.value
          label = data.0.value_classification
```

**fees** and **tip** (mempool.space, no key)

```
url:      https://mempool.space/api/v1/fees/recommended
interval: 120
fields:   fast = fastestFee
          half = halfHourFee
          hour = hourFee
          eco = economyFee

url:      https://mempool.space/api/blocks/tip/height
interval: 120
fields:   height = (path left empty)
```

The `tip` reply is a bare number, so the field's path is left empty and the whole body becomes the value. Name that field `height`; the layout reads `{api.tip.height}`.

**fx** (Frankfurter, ECB reference rates, no key)

```
url:      https://api.frankfurter.app/latest?from=USD&to=EUR,GBP,JPY,CHF
interval: 3600
fields:   eur = rates.EUR
          gbp = rates.GBP
          jpy = rates.JPY
          chf = rates.CHF
          date = date
```

**power** (aWATTar day-ahead spot price, Germany; use awattar.at for Austria, no key)

```
url:      https://api.awattar.de/v1/marketdata
interval: 900
fields:   price = data.0.marketprice
          next = data.1.marketprice
```

Prices are EUR/MWh; the layout divides by 10 for ct/kWh. Any hourly price API with a similar shape works, e.g. Nord Pool or ENTSO-E mirrors.

**astros** (Open Notify, plain http, no key)

```
url:      http://api.open-notify.org/astros.json
interval: 3600
fields:   count = number
          p1 = people.0.name
          p2 = people.1.name
          p3 = people.2.name
          p4 = people.3.name
          p5 = people.4.name
```

**launch** (Launch Library 2, no key, 15 requests per hour)

```
url:      https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=1&mode=list
interval: 900
fields:   name = results.0.name
          net = results.0.net
          status = results.0.status.abbrev
          provider = results.0.launch_service_provider.name
          pad = results.0.pad.location.name
```

**quakes** (USGS, magnitude 4.5+ in the last day, no key)

```
url:      https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson
interval: 600
fields:   count = metadata.count
          mag = features.0.properties.mag
          place = features.0.properties.place
```

**swpc** (NOAA Space Weather Prediction Center, no key)

```
url:      https://services.swpc.noaa.gov/products/noaa-scales.json
interval: 900
fields:   g = 0.G.Scale         gtext = 0.G.Text
          s = 0.S.Scale         stext = 0.S.Text
          r = 0.R.Scale         rtext = 0.R.Text
```

The object's keys are `"-1"`, `"0"`, `"1"`... for yesterday, now and the forecast; `0.G.Scale` reads the current geomagnetic scale.

**neo** (NASA near-Earth objects for today; `DEMO_KEY` works, a free key at api.nasa.gov lifts the rate limit)

```
url:      https://api.nasa.gov/neo/rest/v1/feed/today?detailed=false&api_key=DEMO_KEY
interval: 3600
fields:   count = element_count
```

**wx** (Open-Meteo hourly in imperial units, no key). `forecast_hours` makes every hourly array start at the current hour, so index `.0` is now and `.3` is three hours ahead. Twelve fields, so raise nothing: a source holds up to 24.

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,precipitation_probability,rain,showers,snowfall,snow_depth,is_day,weather_code&timezone=auto&wind_speed_unit=mph&temperature_unit=fahrenheit&precipitation_unit=inch&forecast_hours=6
interval: 600
fields:   temp = hourly.temperature_2m.0 (decimals 0)
          feels = hourly.apparent_temperature.0 (decimals 0)
          humidity = hourly.relative_humidity_2m.0
          precip = hourly.precipitation.0 (decimals 2)
          snow = hourly.snowfall.0
          day = hourly.is_day.0
          code = hourly.weather_code.0
          p0 = hourly.precipitation_probability.0
          p1 = hourly.precipitation_probability.1
          p2 = hourly.precipitation_probability.2
          p3 = hourly.precipitation_probability.3
```

The tiles read the WMO `weather_code`: 0–1 clear (sun by day, moon at night), 2–48 cloud and fog, 51–94 rain or snow depending on `snowfall`, 95+ thunderstorm.

**rain** (Open-Meteo hourly, no key)

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&hourly=precipitation_probability&forecast_hours=6&timezone=auto
interval: 900
fields:   p0 = hourly.precipitation_probability.0
          p1 = hourly.precipitation_probability.1
          ... up to p5 = hourly.precipitation_probability.5
```

**fc** (Open-Meteo daily, no key)

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=auto&forecast_days=3
interval: 1800
fields:   hi0 = daily.temperature_2m_max.0 (decimals 0)    lo0 = daily.temperature_2m_min.0 (decimals 0)    rain0 = daily.precipitation_probability_max.0
          hi1, lo1, rain1 and hi2, lo2, rain2 the same with .1 and .2
```

**baro** (Open-Meteo, no key)

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&current=pressure_msl,cloud_cover,uv_index
interval: 600
fields:   pressure = current.pressure_msl
          cloud = current.cloud_cover
          uv = current.uv_index
```

**surf** (Open-Meteo marine, no key; pick a coastal point)

```
url:      https://marine-api.open-meteo.com/v1/marine?latitude=33.66&longitude=-118.0&current=wave_height,wave_period,wave_direction,swell_wave_height
interval: 900
fields:   wave = current.wave_height
          period = current.wave_period
          dir = current.wave_direction
          swell = current.swell_wave_height
```

**wind** (Open-Meteo, no key)

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&current=wind_speed_10m,wind_gusts_10m,wind_direction_10m
interval: 300
fields:   speed = current.wind_speed_10m
          gust = current.wind_gusts_10m
          dir = current.wind_direction_10m
```

**sun2** (Open-Meteo with unix timestamps, no key)

```
url:      https://api.open-meteo.com/v1/forecast?latitude=42.36&longitude=-71.06&daily=sunrise,sunset&current=is_day&timeformat=unixtime&timezone=auto&forecast_days=1
interval: 300
fields:   t = current.time
          r = daily.sunrise.0
          s = daily.sunset.0
          day = current.is_day
```

Name this source `sun` on the device (the layout reads `api.sun.*`); it is listed as `sun2` here only to keep it apart from the `sun` source used by `sunrise-sunset.json`, which reads different fields. Unix timestamps keep every value numeric, which is what lets the arc do `(now - sunrise) / (sunset - sunrise)`.

**pihole** (Pi-hole v5 web API, no key for the summary)

```
url:      http://pi.hole/admin/api.php?summaryRaw
interval: 60
fields:   pct = ads_percentage_today
          queries = dns_queries_today
          blocked = ads_blocked_today
          domains = domains_being_blocked
```

Pi-hole v6 moved to `/api/stats/summary` behind a session; use v5 or a password-less v6 install.

**printer** (OctoPrint, API key from its settings page)

```
url:      http://octopi.local/api/job
interval: 30
header:   X-Api-Key = <your OctoPrint key>
fields:   state = state
          progress = progress.completion
          left = progress.printTimeLeft
          elapsed = progress.printTime
          file = job.file.name
```

The nozzle and bed temperatures come from `/api/printer` (`temperature.tool0.actual`, `temperature.bed.actual`); add a second source named `printer` fields `tool` and `bed` there, or drop that row.

**steam** (Steam Web API, no key for player counts; 730 is Counter-Strike 2)

```
url:      https://api.steampowered.com/ISteamUserStats/GetNumberOfCurrentPlayers/v1/?appid=730
interval: 300
fields:   players = response.player_count
```

**npm** and **pypi** (no key)

```
url:      https://api.npmjs.org/downloads/point/last-week/esbuild
interval: 3600
fields:   downloads = downloads
          pkg = package

url:      https://pypistats.org/api/packages/platformio/recent
interval: 3600
fields:   day = data.last_day
          week = data.last_week
          pkg = package
```

**gh** (GitHub user profile, no key; pairs with **repo** above)

```
url:      https://api.github.com/users/BeekrBonkr
interval: 900
fields:   name = name
          login = login
          repos = public_repos
          followers = followers
          following = following
```

**f1** (Ergast-compatible Jolpica API, no key)

```
url:      https://api.jolpi.ca/ergast/f1/current/next.json
interval: 3600
fields:   round = MRData.RaceTable.round
          race = MRData.RaceTable.Races.0.raceName
          circuit = MRData.RaceTable.Races.0.Circuit.circuitName
          date = MRData.RaceTable.Races.0.date
          time = MRData.RaceTable.Races.0.time
```

**holiday** (Nager.Date public holidays, no key; change `US` to your country code)

```
url:      https://date.nager.at/api/v3/NextPublicHolidays/US
interval: 21600
fields:   name0 = 0.name    date0 = 0.date
          name1 = 1.name    date1 = 1.date
          name2 = 2.name    date2 = 2.date
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
