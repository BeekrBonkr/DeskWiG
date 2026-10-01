# Example widgets

Fifty-six layouts that show what DeskWiG layouts can do. They are not preloaded on the device: open `http://deskwig.local/editor`, paste one into the editor, and press **Save**, or push it from a shell:

```bash
curl -X PUT "http://deskwig.local/api/layouts?id=big-clock" \
  -H "Authorization: Bearer <key>" -H "Content-Type: application/json" \
  --data @examples/widgets/big-clock.json
```

Every layout renders on its own, but the ones marked with a data source show `--` until that source exists. The sources are listed below with the exact fields the layouts expect; add them under **Data sources** on `http://deskwig.local/setup` (paste the URL, press **Discover keys** and tap the paths) or with `PUT /api/sources?id=<id>`. Field names must match exactly.

| File | Shows off | Needs |
| --- | --- | --- |
| `big-clock.json` | Hero card whose sky shade follows the hour (`hsv()`), a day arc with a marker riding it (templated `start`/`end`), gradient day bar | |
| `flip-clock.json` | 104 px digits in gradient tiles with a seam and notches, named styles | |
| `night-clock.json` | Dim custom hex colors, a minute ring with the hour as a marker on it | |
| `world-clock.json` | Math on `time.hour` with `%` for other time zones; each row's shade and sun or moon dot come from that zone's hour | |
| `day-progress.json` | Day ring, 24 hour cells colored by `hsv()` and `if()`, gradient hour bar, minutes-left arithmetic | |
| `calendar-card.json` | Tear-off calendar built from gradient cards, day bar | |
| `weather-card.json` | Hero gradient that follows the temperature, a marker on a gradient scale (templated `x`), stat cards with gradient bars, `.updated` and `.color` status keys | `weather` |
| `weather-rings.json` | Three concentric ring gauges with a color legend | `weather` |
| `weather-fahrenheit.json` | Unit conversion with `round()`, a thermometer whose column grows with the temperature (templated `y` and `h`), both scales beside it | `weather` |
| `air-quality.json` | Zoned gauge made of arc segments with a marker, number colored by `hsv()`, `min()` | `aqi` |
| `sunrise-sunset.json` | ISO timestamps cut down to HH:MM by clipping fixed-pitch text in a box, UV bar | `sun` |
| `crypto-ticker.json` | Card and arrow that turn green or red with `if()`, a triangle with templated points, area chart of the price | `btc`, sampled `api.btc.usd` |
| `github-repo.json` | Emoji stat tiles, two-line wrap of a long description, ratio bars from division | `repo` |
| `iss-tracker.json` | World map drawn from polygons, crosshair and marker placed from latitude and longitude, fast-refresh source | `iss` |
| `home-assistant.json` | Thermostat dial with a comfort band, marker and number colored by the value, chart of the sensor, a source with an `Authorization` header | `ha`, sampled `api.ha.state` |
| `server-dots.json` | Up to six targets; rows for targets that are not configured fold away with a templated `h`, latency bar per row | ping targets |
| `latency-bars.json` | Bars scaled with `min()` whose color slides from green to red with the latency, trend arrows, history | ping targets |
| `latency-gauges.json` | Partial arcs (`start`/`end`) as gauges, one large and two small | ping targets |
| `network-panel.json` | WiFi gauge, signal bars lit by `if()`, marker on a gradient scale, ping by target name | ping target named `Router` |
| `system-monitor.json` | Card grid from one named style, chart of free heap, uptime | sampled `heap` |
| `shapes-showcase.json` | Circle, ellipse, rounded rect, triangle, polygon, arc, gradients, and shapes that move with the minute and the signal | |
| `styles-demo.json` | Classes, inline style precedence, `justify`, `align`, pills, gradients in both directions | |
| `emoji-board.json` | Emoji in every font, color and size on gradient tiles | `weather` (one line) |
| `typography.json` | Bitmap sizes next to sans and bold pixel sizes, a text whose `size` is a template | |
| `photo-frame.json` | URL image as a full-screen background with a clock overlay | internet |
| **Finance** | | |
| `stock-ticker.json` | One stock: tinted price card, change pill, chart of the day, day-range bar, LED on big moves | `stock`, sampled `api.stock.last` |
| `stock-board.json` | Four stocks with a bar that grows left or right of center with the change (templated `x` and `w`), raw Finnhub field names | `nvda` `tsm` `sgdm` `vti` |
| `crypto-board.json` | Four coins from one CoinGecko call as tiles shaded by the size and sign of the 24 h move (`hsv()` with `if()` and `abs()`) | `coins` |
| `fear-greed.json` | Five-zone arc gauge (`start`/`end` past 360) with a marker, number and label colored by the value | `fng` |
| `mempool-fees.json` | A plain-text source (empty path) for the block height, fee tiers as gradient bars, chart of the next-block fee | `fees` `tip`, sampled `api.fees.fast` |
| `forex-rates.json` | Currency tiles with symbols on colored disks, reverse conversions with `/` | `fx` |
| `power-price.json` | Spot electricity price colored by `hsv()`, now and next hour as columns with templated heights, bar chart of the day, LED by price band | `power`, sampled `api.power.price` |
| **Space and science** | | |
| `astronauts.json` | Array items by index (`people.0.name`), a dot per person lit with `if()`, shapes clipped by their card | `astros` |
| `next-launch.json` | Nested paths three levels deep, date and time cut out of one ISO string, long names on two lines, `key`/`is` LED rules on a string | `launch` |
| `earthquakes.json` | GeoJSON feed: rings whose radius follows the latest magnitude (templated `r`), marker on a magnitude scale | `quakes` |
| `space-weather.json` | Object keys that look like numbers (`0.G.Scale`), three level meters lit by `if()`, a card that changes color and caption on an aurora watch | `swpc` |
| `space-dashboard.json` | Three sources on one screen: ISS on a small map, crew dots, near-Earth objects | `iss` `astros` `neo` |
| **Weather** | | |
| `weather-hourly.json` | Current conditions: hero card whose gradient follows the temperature (`hsv()`), condition tiles lit by `if()` on rain, snow and `is_day`, gradient rain-chance bar, stat cards | `wx` |
| `weather-6h.json` | Next six hours as six full-width rows: per-hour icon picked by moving the other icons out of view with a templated `x`, a fill as wide as the chance of rain (templated `w`), temperature colored by `hsv()` | `w6` |
| `rain-next-hours.json` | Six hourly columns from one array (`hourly.x.0` to `.5`) with templated heights, shade from `mix()`, `max()` | `rain` |
| `forecast-3day.json` | Three days, each with a range bar placed from its low and high (templated `x` and `w`), rain chance shaded by `mix()` | `fc` `weather` |
| `barometer.json` | Zoned dial over a 950–1050 hPa window with a marker (`clamp()`), area chart of the last day, `.delta` | `baro`, sampled `api.baro.pressure` |
| `surf-report.json` | Marine API: swell drawn as polygons whose crests rise with the wave height, compass ring with a direction marker, `^` for wave energy | `surf` `weather` |
| `wind-meter.json` | Compass ring with a direction marker, speed gauge, Beaufort cells from km/h with `^` (power) | `wind` |
| **Over time** | | |
| `stock-chart.json` | Area chart of a sampled price, `.delta`/`.min`/`.max`/`.span` keys, colors from `rgb()` and `if()`, templated `min`/`max` on a bar chart | `nvda`, sampled `api.nvda.c` |
| `temp-history.json` | Line color from `hsv()`, area chart over 12 h, the day's range as a rect placed from `.min` and `.max`, gradient bar | `weather`, sampled `api.weather.temp` |
| `latency-history.json` | Dot chart of ping latency, area chart of signal with fixed range, a rect whose width follows `wifi.pct` | ping targets, sampled `ping.0.ms` and `wifi.rssi` |
| `daylight-arc.json` | Half-ring sun path from unix timestamps with the sun as a marker on it, sun or moon swapped with a templated `x`, time to sunset | `sun2` |
| **Home and tech** | | |
| `pihole.json` | Blocked-percentage ring, a split bar with a templated `w`, `.status` in a rule | `pihole` |
| `3d-printer.json` | OctoPrint progress ring, a print that grows on the bed with the nozzle above it (templated `y` and `h`), temperature bars, an `X-Api-Key` header | `printer` |
| `steam-players.json` | Live player count scaled with `/ 1000` and a `k` suffix, area chart of the day with `.max` | `steam`, sampled `api.steam.players` |
| `package-stats.json` | npm and PyPI weekly downloads in millions and thousands, yesterday against the daily average | `npm` `pypi` |
| `github-user.json` | Profile card with an avatar disk, three stat columns, repo card | `gh` `repo` |
| `f1-next-race.json` | Checkered strip from rects, deeply nested Ergast paths, date and start time cut out of their strings | `f1` |
| `next-holiday.json` | Top-level array (`0.name`, `1.name`), MM-DD cut out of ISO dates at three sizes | `holiday` |
| **No API** | | |
| `binary-clock.json` | Hour, minute and second bits as rounded cells: `floor(time.second / 16) % 2` inside `if()` picks the lit or unlit color | |
| `binary-date-clock.json` | The binary clock plus a second grid for day, month and two-digit year (`date.year % 100`) | |

## Data sources used

Each block below can be pasted as is: open **Data sources** on the setup page, press **Paste data sources as text**, paste one or more blocks and press **Import**. Replace the coordinates, symbols and tokens with your own. Field names must match exactly.

**weather** (Open-Meteo, no key)

```
id:       weather
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&current=temperature_2m,relative_humidity_2m,wind_speed_10m,apparent_temperature
interval: 600
fields:   temp = current.temperature_2m (decimals 0)
          humidity = current.relative_humidity_2m
          wind = current.wind_speed_10m (decimals 0)
          feels = current.apparent_temperature (decimals 0)
```

**aqi** (Open-Meteo air quality, no key)

```
id:       aqi
url:      https://air-quality-api.open-meteo.com/v1/air-quality?latitude=51.48&longitude=0.00&current=european_aqi,pm2_5,pm10
interval: 900
fields:   aqi = current.european_aqi
          pm25 = current.pm2_5
          pm10 = current.pm10
```

**sun** (Open-Meteo daily, no key)

```
id:       sun
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&daily=sunrise,sunset,uv_index_max&timezone=auto&forecast_days=1
interval: 3600
fields:   sunrise = daily.sunrise.0
          sunset = daily.sunset.0
          uv = daily.uv_index_max.0 (decimals 0)
```

The sunrise strings look like `2026-09-26T06:45`; the layout shows them as is.

**btc** (CoinGecko, no key)

```
id:       btc
url:      https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum&vs_currencies=usd&include_24hr_change=true&include_market_cap=true
interval: 120
fields:   usd = bitcoin.usd (decimals 0)
          change = bitcoin.usd_24h_change
          eth = ethereum.usd (decimals 0)
          cap = bitcoin.usd_market_cap
```

**repo** (GitHub, no key for public repos)

```
id:       repo
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
id:       iss
url:      http://api.open-notify.org/iss-now.json
interval: 30
fields:   lat = iss_position.latitude
          lon = iss_position.longitude
```

**stock** (Finnhub, free API key from finnhub.io)

```
id:       stock
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

Quotes update during market hours; outside them the values are the last close. `stock-board.json` uses four sources named after their tickers, **nvda**, **tsm**, **sgdm** and **vti**, each the same URL with its own `symbol=` and the field names exactly as **Discover keys** generates them (`c`, `d`, `dp`, `h`, `l`, `o`, `pc`, `t`); the layout reads `c` and `dp`. For other stocks, name the source after the ticker and change the label and the `api.<id>` keys in that row. No key at all? Yahoo's chart endpoint works without one: `https://query1.finance.yahoo.com/v8/finance/chart/AAPL?range=1d&interval=1d` with `price = chart.result.0.meta.regularMarketPrice` and `prev = chart.result.0.meta.chartPreviousClose`.

**coins** (CoinGecko, no key)

```
id:       coins
url:      https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum,solana,dogecoin&vs_currencies=usd&include_24hr_change=true
interval: 120
fields:   btc = bitcoin.usd            btcc = bitcoin.usd_24h_change
          eth = ethereum.usd           ethc = ethereum.usd_24h_change
          sol = solana.usd             solc = solana.usd_24h_change
          doge = dogecoin.usd          dogec = dogecoin.usd_24h_change
```

**fng** (alternative.me crypto Fear & Greed index, no key)

```
id:       fng
url:      https://api.alternative.me/fng/
interval: 1800
fields:   value = data.0.value
          label = data.0.value_classification
```

**fees** and **tip** (mempool.space, no key)

```
id:       fees
url:      https://mempool.space/api/v1/fees/recommended
interval: 120
fields:   fast = fastestFee
          half = halfHourFee
          hour = hourFee
          eco = economyFee

id:       tip
url:      https://mempool.space/api/blocks/tip/height
interval: 120
fields:   height = (path left empty)
```

The `tip` reply is a bare number, so the field's path is left empty and the whole body becomes the value. Name that field `height`; the layout reads `{api.tip.height}`.

**fx** (Frankfurter, ECB reference rates, no key)

```
id:       fx
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
id:       power
url:      https://api.awattar.de/v1/marketdata
interval: 900
fields:   price = data.0.marketprice
          next = data.1.marketprice
```

Prices are EUR/MWh; the layout divides by 10 for ct/kWh. Any hourly price API with a similar shape works, e.g. Nord Pool or ENTSO-E mirrors.

**astros** (Open Notify, plain http, no key)

```
id:       astros
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
id:       launch
url:      https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=1&mode=list
interval: 900
fields:   name = results.0.name
          net = results.0.net
          state = results.0.status.abbrev
          provider = results.0.launch_service_provider.name
          pad = results.0.pad.location.name
```

The launch status goes in a field called `state`, not `status`: `api.<id>.status`, `.color`, `.age`, `.updated` and `.error` always mean the source's own fetch state, so a field with one of those names is never read.

**quakes** (USGS, magnitude 4.5+ in the last day, no key)

```
id:       quakes
url:      https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson
interval: 600
fields:   count = metadata.count
          mag = features.0.properties.mag
          place = features.0.properties.place
```

**swpc** (NOAA Space Weather Prediction Center, no key)

```
id:       swpc
url:      https://services.swpc.noaa.gov/products/noaa-scales.json
interval: 900
fields:   g = 0.G.Scale         gtext = 0.G.Text
          s = 0.S.Scale         stext = 0.S.Text
          r = 0.R.Scale         rtext = 0.R.Text
```

The object's keys are `"-1"`, `"0"`, `"1"`... for yesterday, now and the forecast; `0.G.Scale` reads the current geomagnetic scale.

**neo** (NASA near-Earth objects for today; `DEMO_KEY` works, a free key at api.nasa.gov lifts the rate limit)

```
id:       neo
url:      https://api.nasa.gov/neo/rest/v1/feed/today?detailed=false&api_key=DEMO_KEY
interval: 3600
fields:   count = element_count
```

**wx** (Open-Meteo hourly in imperial units, no key). `forecast_hours` makes every hourly array start at the current hour, so index `.0` is now. The field names below are exactly what **Discover keys** produces when you tap the `hourly_units` entries first and then the `hourly` values: the second tap of a name gets a `2`, and names are cut to 16 characters. Tap only the value paths and rename them to these, or tap both sets and use them as they come.

```
id:       wx
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,precipitation_probability,rain,showers,snowfall,snow_depth,is_day,weather_code&timezone=auto&wind_speed_unit=mph&temperature_unit=fahrenheit&precipitation_unit=inch&forecast_hours=6
interval: 600
fields:   temperature_2m2 = hourly.temperature_2m.0
          apparent_tempe2 = hourly.apparent_temperature.0
          relative_humid2 = hourly.relative_humidity_2m.0
          precipitation2 = hourly.precipitation.0
          precipitation_2 = hourly.precipitation_probability.0
          rain2 = hourly.rain.0
          showers2 = hourly.showers.0
          snowfall2 = hourly.snowfall.0
          is_day2 = hourly.is_day.0
```

The tiles light from the amounts: sun or moon (by `is_day`) when nothing is falling, rain when `rain + showers` is above zero, snow when `snowfall` is. Temperatures are rounded in the layout with `round()`, so the fields can keep their decimals.

**w6** (Open-Meteo hourly forecast for the next six hours, no key). `forecast_hours=6` makes every hourly array hold the current hour and the five after it. The layout expects short names, so type them in place of the ones Discover suggests: `t` temperature, `r` chance of rain, `c` WMO weather code, `d` is_day, each with the hour index.

```
id:       w6
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&hourly=temperature_2m,precipitation_probability,weather_code,is_day&forecast_hours=6&timezone=auto&temperature_unit=fahrenheit
interval: 600
fields:   t0 = hourly.temperature_2m.0
          t1 = hourly.temperature_2m.1
          t2 = hourly.temperature_2m.2
          t3 = hourly.temperature_2m.3
          t4 = hourly.temperature_2m.4
          t5 = hourly.temperature_2m.5
          r0 = hourly.precipitation_probability.0
          r1 = hourly.precipitation_probability.1
          r2 = hourly.precipitation_probability.2
          r3 = hourly.precipitation_probability.3
          r4 = hourly.precipitation_probability.4
          r5 = hourly.precipitation_probability.5
          c0 = hourly.weather_code.0
          c1 = hourly.weather_code.1
          c2 = hourly.weather_code.2
          c3 = hourly.weather_code.3
          c4 = hourly.weather_code.4
          c5 = hourly.weather_code.5
          d0 = hourly.is_day.0
          d1 = hourly.is_day.1
          d2 = hourly.is_day.2
          d3 = hourly.is_day.3
          d4 = hourly.is_day.4
          d5 = hourly.is_day.5
```

Weather codes: 0-2 clear or partly cloudy (sun by day, moon at night), 3-48 overcast and fog, 51-67 and 80-99 rain, drizzle, showers and storms, 71-77 snow. Put your own coordinates in the URL.

**rain** (Open-Meteo hourly, no key)

```
id:       rain
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&hourly=precipitation_probability&forecast_hours=6&timezone=auto
interval: 900
fields:   p0 = hourly.precipitation_probability.0
          p1 = hourly.precipitation_probability.1
          p2 = hourly.precipitation_probability.2
          p3 = hourly.precipitation_probability.3
          p4 = hourly.precipitation_probability.4
          p5 = hourly.precipitation_probability.5
```

**fc** (Open-Meteo daily, no key)

```
id:       fc
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=auto&forecast_days=3
interval: 1800
fields:   hi0 = daily.temperature_2m_max.0 (decimals 0)    lo0 = daily.temperature_2m_min.0 (decimals 0)    rain0 = daily.precipitation_probability_max.0
          hi1, lo1, rain1 and hi2, lo2, rain2 the same with .1 and .2
```

**baro** (Open-Meteo, no key)

```
id:       baro
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&current=pressure_msl,cloud_cover,uv_index
interval: 600
fields:   pressure = current.pressure_msl
          cloud = current.cloud_cover
          uv = current.uv_index
```

**surf** (Open-Meteo marine, no key; pick a coastal point)

```
id:       surf
url:      https://marine-api.open-meteo.com/v1/marine?latitude=39.60&longitude=-9.07&current=wave_height,wave_period,wave_direction,swell_wave_height
interval: 900
fields:   wave = current.wave_height
          period = current.wave_period
          dir = current.wave_direction
          swell = current.swell_wave_height
```

**wind** (Open-Meteo, no key)

```
id:       wind
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&current=wind_speed_10m,wind_gusts_10m,wind_direction_10m
interval: 300
fields:   speed = current.wind_speed_10m
          gust = current.wind_gusts_10m
          dir = current.wind_direction_10m
```

**sun2** (Open-Meteo with unix timestamps, no key)

```
id:       sun2
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&longitude=0.00&daily=sunrise,sunset&current=is_day&timeformat=unixtime&timezone=auto&forecast_days=1
interval: 300
fields:   t = current.time
          r = daily.sunrise.0
          s = daily.sunset.0
          day = current.is_day
```

Name this source `sun` on the device (the layout reads `api.sun.*`); it is listed as `sun2` here only to keep it apart from the `sun` source used by `sunrise-sunset.json`, which reads different fields. Unix timestamps keep every value numeric, which is what lets the arc do `(now - sunrise) / (sunset - sunrise)`.

**pihole** (Pi-hole v5 web API, no key for the summary)

```
id:       pihole
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
id:       printer
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
id:       steam
url:      https://api.steampowered.com/ISteamUserStats/GetNumberOfCurrentPlayers/v1/?appid=730
interval: 300
fields:   players = response.player_count
```

**npm** and **pypi** (no key)

```
id:       npm
url:      https://api.npmjs.org/downloads/point/last-week/esbuild
interval: 3600
fields:   downloads = downloads
          pkg = package

id:       pypi
url:      https://pypistats.org/api/packages/platformio/recent
interval: 3600
fields:   day = data.last_day
          week = data.last_week
          pkg = package
```

**gh** (GitHub user profile, no key; pairs with **repo** above)

```
id:       gh
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
id:       f1
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
id:       holiday
url:      https://date.nager.at/api/v3/NextPublicHolidays/US
interval: 21600
fields:   name0 = 0.name    date0 = 0.date
          name1 = 1.name    date1 = 1.date
          name2 = 2.name    date2 = 2.date
```

**ha** (Home Assistant, long-lived access token in a header)

```
id:       ha
url:      http://homeassistant.local:8123/api/states/sensor.living_room_temperature
interval: 60
header:   Authorization = Bearer <long-lived token>
fields:   state = state
          unit = attributes.unit_of_measurement
          name = attributes.friendly_name
```

## Sampled keys

The charts draw keys that are recorded under **History** on the setup page (or with `PUT /api/series?key=...`). A chart shows a dim baseline until the first sample lands, and the `.min`, `.max`, `.avg` and `.delta` values next to it show `--`. A device keeps up to 12 sampled keys, which is exactly this list:

| Key | Every | Keep | Used by |
| --- | --- | --- | --- |
| `api.nvda.c` | 120 s | 14400 s | `stock-chart.json` |
| `api.stock.last` | 120 s | 14400 s | `stock-ticker.json` |
| `api.btc.usd` | 300 s | 86400 s | `crypto-ticker.json` |
| `api.fees.fast` | 120 s | 14400 s | `mempool-fees.json` |
| `api.power.price` | 900 s | 86400 s | `power-price.json` |
| `api.weather.temp` | 300 s | 43200 s | `temp-history.json` |
| `api.baro.pressure` | 600 s | 86400 s | `barometer.json` |
| `api.ha.state` | 60 s | 21600 s | `home-assistant.json` |
| `api.steam.players` | 300 s | 86400 s | `steam-players.json` |
| `ping.0.ms` | 10 s | 1800 s | `latency-history.json` |
| `wifi.rssi` | 10 s | 1800 s | `latency-history.json` |
| `heap` | 60 s | 3600 s | `system-monitor.json` |

## LED behavior

Every example carries a `led` block, so the RGB LED does something sensible while it is on screen: clocks breathe softly and go dark at night, the ping and network layouts show the worst target's color and blink red when something is down, weather layouts warn on heat, frost, wind or bad air, the crypto ticker flashes green or red on big moves, and the showcase layouts cycle through a rainbow. Delete the block to keep the LED off, or edit the `rules` to taste.

## Notes

- The screen is 170 × 320. Text longer than the width is clipped by its box, so keep labels short or drop the font size. Values from a source can be up to 31 characters; the layouts that show long ones (`next-launch`, `earthquakes`, `f1-next-race`, `github-repo`, `3d-printer`) put the same bitmap text in two boxes, the second slid left by one line's width, so it reads as two lines.
- The same trick cuts a part out of a fixed-format string: `sunrise-sunset`, `next-launch`, `f1-next-race` and `next-holiday` show only the `HH:MM` or `MM-DD` of an ISO timestamp by sliding the text left inside a box just wide enough for those characters. It relies on the bitmap font, where every character is 6 × `size` pixels wide.
- Layouts that calculate with `time.hour` (`big-clock`, `world-clock`, `day-progress`, `calendar-card`, `night-clock`, `photo-frame`) expect the 24-hour clock setting; in 12-hour mode `time.hour` runs 1 to 12.
- `world-clock.json` hard-codes hour offsets from US Eastern; edit the four numbers in the `(time.hour+N)%24` expressions for your zone.
- `photo-frame.json` points at a placeholder PNG on httpbin.org. Put your own image URL there, or upload an image on the setup page and use its name.
- Every file passes the same validation the editor and the device run, so they load as they are.
