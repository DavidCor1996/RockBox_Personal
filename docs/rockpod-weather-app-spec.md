# RockPod Weather App Spec

## Goal

Add a small offline weather app to Rockbox for 320x240 iPods. RockPod fetches
and normalizes a 7 day forecast during device sync, then Rockbox displays the
last synced forecast without needing network access on-device.

The first version should be useful when the device is used offline for a day or
two, while making stale data obvious instead of pretending it is live weather.

## First Slice

- Add `weather.rock` as an application plugin.
- Add RockPod settings for one forecast location and units.
- During a normal RockPod sync, fetch a 7 day daily forecast on the host.
- Write the forecast bundle under:

```text
/.rockbox/rockpod/weather/
```

- The Rockbox plugin only reads the generated files. It does not fetch weather,
  mutate settings, touch playback, or require the Rockbox database.
- The plugin supports iPod Classic 6G/7G and iPod Video 5G/5.5G layouts first.

## Forecast Bundle

Use a simple, bounded text format so the plugin can parse it without a JSON
parser dependency. The canonical file is:

```text
/.rockbox/rockpod/weather/forecast.tsv
```

Header row:

```text
rockpod_weather_v1	location_name	latitude	longitude	timezone	generated_at_utc	valid_from_local	units
```

Daily rows:

```text
date	condition_code	condition_text	temp_min	temp_max	precip_probability	wind_speed	wind_direction	sunrise	sunset	source
```

Rules:

- exactly 1 header row and up to 7 daily rows;
- dates use `YYYY-MM-DD`;
- times use local `HH:MM`;
- `generated_at_utc` uses `YYYY-MM-DDTHH:MM:SSZ`;
- temperatures are integers after host-side rounding;
- precipitation probability is `0` to `100`, or blank if unavailable;
- unknown numeric fields are blank, not magic sentinels;
- tabs and newlines inside text fields are replaced with spaces by RockPod;
- UTF-8 is allowed, but the plugin must render replacement text if a glyph is
  missing from the active font.

Optional companion file:

```text
/.rockbox/rockpod/weather/manifest.json
```

The manifest is for RockPod ownership and troubleshooting only. It records the
provider, provider response timestamp, request location, selected units, output
hash, and any warnings. `weather.rock` must not require it.

## Condition Codes

RockPod normalizes provider-specific weather values into a small Rockbox-facing
code set:

- `clear`
- `partly_cloudy`
- `cloudy`
- `fog`
- `drizzle`
- `rain`
- `snow`
- `sleet`
- `thunderstorm`
- `wind`
- `unknown`

The TSV also includes provider text for more detail, but the plugin should rely
on the normalized code for icons and fallback display. Later slices can add
weather-specific bitmap icons under:

```text
/.rockbox/rockpod/weather/icons/
```

The first slice can use text glyphs or small compiled monochrome/color bitmaps
to avoid asset deployment complexity.

## RockPod Integration

Add a `weather` service module responsible for:

- loading effective device/user weather settings;
- geocoding a saved place name, or using saved latitude/longitude directly;
- fetching daily forecast data from a provider adapter;
- normalizing units and condition codes;
- writing `forecast.tsv` atomically into a local cache;
- adding the generated forecast bundle to the sync plan.

Suggested config keys:

- `weather_enabled`: default `false`;
- `weather_location_name`: display name, for example `Moncton, NB`;
- `weather_latitude`;
- `weather_longitude`;
- `weather_units`: `metric`, `imperial`, or `auto`;
- `weather_update_on_sync`: default `true`;
- `weather_provider`: provider id, default first built-in provider;
- `weather_cache_max_age_minutes`: default `60`;
- `weather_sync_stale_cache`: default `true`.

Sync behavior:

- If weather is disabled, RockPod leaves existing on-device weather files alone
  in the first slice.
- If enabled and the network fetch succeeds, RockPod copies the new bundle.
- If enabled and the network fetch fails, RockPod may sync the most recent host
  cache when `weather_sync_stale_cache` is true, but the manifest and plugin UI
  must mark it stale.
- Repeated syncs should not rewrite unchanged forecast files.
- Forecast copy operations should appear in the sync summary as weather data,
  not album artwork or media.

RockPod should also expose a manual refresh command for testing:

```text
rockpod/main.py refresh-weather --config <path> --out <root> --json
```

When `--out` is supplied, the command writes:

```text
<out>/.rockbox/rockpod/weather/forecast.tsv
<out>/.rockbox/rockpod/weather/manifest.json
```

## Provider Strategy

Use a narrow provider adapter interface so changing providers does not affect
the Rockbox plugin or sync engine:

```text
fetch_daily_forecast(location, units) -> WeatherForecast
```

The normalized model should contain:

- location display name;
- coordinates;
- timezone;
- generated timestamp;
- source/provider id;
- 7 daily forecast records.

Provider-specific concerns stay in RockPod:

- request URLs and API keys;
- rate limits;
- geocoding;
- provider weather-code mapping;
- timezone handling;
- retries and timeout policy;
- cache validation.

No API key or provider response should be written to the Rockbox device unless
it is needed for diagnostics and safe to expose.

## Plugin UI

Default launch opens the 7 day overview.

Overview screen:

- compact header with location and freshness;
- current/first forecast day highlighted;
- seven rows with day name, condition, high/low, and precipitation chance;
- stale marker when the forecast is older than the configured freshness window;
- clear empty state if no forecast file exists.

Detail screen for a selected day:

- date and condition text;
- high and low;
- precipitation probability;
- wind speed and direction;
- sunrise and sunset;
- source and generated time in a small footer.

Controls:

- wheel: move through days or scroll detail text;
- select: open day detail;
- menu: back or exit from overview;
- left/right: previous/next day where available;
- play/pause: toggle units display only if both unit values are present in a
  later bundle version. First slice can leave this unused.

The plugin should use Rockbox list and text drawing APIs. It should not allocate
memory proportional to anything larger than the fixed 7 day file.

## Staleness And Clock Handling

Weather is only as trustworthy as both the host sync time and the iPod clock.
The plugin should display freshness conservatively:

- If `generated_at_utc` is missing or unparsable, show `Forecast date unknown`.
- If the Rockbox clock is unavailable, show `Synced forecast` plus the local
  valid-from date instead of an age.
- If the forecast is more than 36 hours old, mark it `Stale`.
- If all forecast dates are before the device date, show `Expired forecast`.
- Never hide stale data; make the status visible and continue showing the rows.

RockPod should write local dates from the provider timezone, not the host
timezone, so travel and remote-location forecasts remain consistent.

## Build Integration

Rockbox:

- add `apps/plugins/weather.c`;
- add `weather.c` to `apps/plugins/SOURCES`;
- add `weather,apps` to `apps/plugins/CATEGORIES`;
- use only plugin API file, list, button, LCD, font, and time helpers;
- do not call PCM, mixer, playlist, tagcache write, or shared audio buffer APIs.

RockPod:

- add `rockpod/services/weather.py`;
- add unit tests under `rockpod/tests/test_weather.py`;
- wire the generated files into `SyncPlan` with a distinct weather operation
  list or a generalized generated-assets list;
- add UI settings in the device/settings area rather than burying them in sync
  internals;
- add the manual `refresh-weather` command in `rockpod/main.py`.

## Testing

### RockPod Unit Tests

- provider response with seven complete days writes valid `forecast.tsv`;
- provider response with fewer days writes available rows and a warning;
- unknown provider condition maps to `unknown`;
- metric and imperial units are normalized consistently;
- tabs/newlines in provider text are sanitized;
- failed network fetch uses cached forecast only when configured;
- stale cache is marked stale in the manifest;
- repeated unchanged refresh keeps the same output hash;
- unsafe output paths are rejected.

### Simulator Gate

- Build `weather.rock` in iPod 6G and iPod Video simulators.
- Open with no forecast file and verify a clear empty state.
- Open with a normal 7 day file and navigate overview/detail.
- Open with truncated, malformed, empty, oversized, and wrong-version files.
- Verify long location names and condition text do not overlap controls.
- Verify missing optional fields render as `--` or a similarly compact fallback.
- Verify stale and expired forecasts are visibly labeled.
- Run repeated open/back/detail/exit cycles under AddressSanitizer where
  available.

### Hardware Gate

- Test on iPod Classic 6G/7G and iPod Video 5G/5.5G.
- Sync a fresh forecast with RockPod, eject safely, boot Rockbox, and open
  `weather.rock`.
- Verify display readability on the default theme and the custom iPone themes.
- Verify navigation remains responsive while music is playing.
- Exit the plugin and confirm Database and Files playback still work.
- Power-cycle after sync and verify the forecast still opens.

### Release Blockers

- Any crash, reboot, memory corruption, or unbounded parser read.
- Any plugin path that writes to playback, playlist, tagcache, PCM, mixer, or
  shared audio buffer state.
- Any network dependency from Rockbox itself.
- Any sync failure that blocks normal music sync when weather is optional.
- Any UI state where stale or expired forecasts look current.

## Later Slices

- Multiple saved locations.
- Hourly forecast for the next 12 to 24 hours.
- Weather icons deployed as Rockbox-ready BMP assets.
- Lockscreen or status-bar weather summary generated into theme assets.
- Calendar-aware weather notes for travel dates.
- Manual on-host refresh without a full media sync.
- Provider selection UI with optional API-key storage.

## September 2026 visual and sync refresh

The plugin uses an open blue gradient, embedded Helvetica glyphs, dimensional
keyed bitmap weather symbols, seven daily rows, and a six-hour details strip.
There are no opaque forecast-row or text backgrounds. Select toggles details;
the wheel selects a day. Hero artwork drifts on an elapsed-tick clock, with
condition-specific rain, snow and clear-sky shimmer. Rendering is capped below
8 updates per second and action processing takes priority over animation.

Icon decoding runs once at entry into fixed plugin BSS (93,440 bytes on RGB565).
Draw functions perform no file reads or allocations. The full-screen background
cache is removed; forecast parsing streams through a 512-byte line buffer.
No audio buffer, mixer, playlist or core allocation API is used. Missing artwork
shows a neutral placeholder instead of a crude substitute weather drawing.
The embedded glyph source is generated from the repository Helvetica BDFs with
`tools/convbdf`, preserving the source font attribution.

Weather sync rejects cached forecasts from another location or unit setting,
and tolerates truncated daily arrays. Simulator and iPod 6G weather plugin
builds pass; the weather Python suite passes 12 tests. Simulator captures verify
transparent artwork, overview/details layout and changes between animation
frames. Native plugin BSS is 147,896 bytes; the draw function's ARM local stack
reservation is 116 bytes (plus saved registers and callees). Physical-device
playback and navigation stress testing has not been performed for this pass.
