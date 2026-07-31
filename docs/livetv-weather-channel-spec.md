# Live TV Weather Channel — Specification

Status: implementation spec for this custom iPod tree (iPod Classic 6G/7G and
iPod Video 5G/5.5G, 320x240). Extends `docs/livetv-directv-guide-spec.md` and
reuses the forecast pipeline from `docs/rockpod-weather-app-spec.md`.

## 1. Product behaviour

A "Weather" channel appears in the Live TV guide, styled after the local
weather channels of the 1990s-2000s (WeatherSTAR "Local on the 8s" segments,
Weatherscan): continuous instrumental music under a loop of rotating forecast
panels, with ordinary commercial breaks like any other channel.

1. The channel **only exists when a forecast has been synced**. If RockPod
   has never produced `forecast.tsv`, the channel is not written to
   `channels.tsv`/`guide.tsv` at all — it does not appear in the guide grid,
   channel-up/down skips over it (there is nothing to skip; it is simply
   absent), exactly as an un-synced channel behaves today.
2. Tuning it plays continuous music (the user's own local tracks) while the
   screen cycles through several forecast panels — current conditions,
   today's forecast, the extended outlook, and an almanac panel — each shown
   for several seconds before advancing, the way a real Local on the 8s loop
   rotates slides.
3. If the channel was synced in the past but the forecast has since expired
   (device clock has moved past every forecast date) or the file is missing
   at tune time, the panels show a plain "Forecast unavailable — sync with
   RockPod" state instead of stale or blank data. Freshness is never hidden.
4. Commercial breaks happen the same way they do on every other channel —
   drawn from the user's synced ad pool at the normal cadence — nothing
   channel-specific is added for ads.

## 2. Why this is *not* a new slot kind

The obvious-looking design — a new `LIVETV_KIND_WEATHER` slot that skips
`stream_open()` and renders data instead of decoding video — was considered
and rejected. `livetv_tune()`/`livetv_advance()` and all three of their call
sites in `mpegplayer.c` (launch, `stream_open`, the `VIDEO_NEXT`/failure-retry
path) assume a resolved slot always produces a playable file; special-casing
that in three places, including the failure-retry loop that already exists to
route around unplayable channels, is exactly the kind of change
`docs/plugin-audio-lifecycle-steering.md` asks to be justified against
`pcm_output.c`/`stream_mgr.c`/`plugin.c` first, and it buys nothing that the
alternative below does not already provide.

**Instead, the Weather channel is an ordinary channel** whose "shows" are
real MPEG-2 program-stream files, generated once per sync like any other Live
TV content:

* the video track is a small original generated pattern (a slow colour
  cycle sampled from the Weatherscan-era blue palette — nothing copied from
  a reference image, just as the DIRECTV guide's colours were sampled, not
  screenshotted);
* the audio track is the user's own local music, concatenated and looped to
  fill a fixed block length.

This means `livetv_tune`, `livetv_slot_path`, `stream_open`/`stream_close`,
the audio lifecycle guarantees, the failure-retry path, and
`LiveTvScheduler._build_day`/`write_guide_tsv` on the PC side are **all
reused completely unmodified**. The only genuinely new code is:

* a small on-device forecast reader + panel renderer, using the whole
  screen (see section 5.3 on why there is no video box to dodge);
* a PC-side generator that turns music, presenter images, and interstitial
  videos into hour-long carriers and a channel-builder that wires them into
  the existing lineup/scheduler.

The carrier alternates flat panel backing with full-screen presenter and
report inserts on the 120-second clock in section 5.7. The guide's
picture-in-guide is unaffected and continues to show the decoded carrier.

## 3. Slot budget

The device holds `LIVETV_MAX_SLOTS` (2048) slots for *today and tomorrow,
across every channel*. One block per song, per programme, would blow this
budget immediately — two of the reference music files are ~15 seconds long,
which at normal ad-break density would generate over a thousand slots a day
for this channel alone.

Instead the channel uses **fixed 1-hour blocks**: 24 shows/day, each an
hour-long bumper file (music looped/concatenated to fill the hour, one of a
small rotating set of variants so consecutive hours don't sound identical).
With the lineup's normal ad-break settings (2-3 ads per break, one break per
show) that is at most:

```
24 shows/day + 24 breaks/day * 3 ads = 96 slots/day
```

×2 days (today + tomorrow) = **~192 slots**, under 10% of the total budget,
leaving headroom for every other channel. `LiveTvSync.sync()` already warns
if the two-day total exceeds `LIVETV_MAX_SLOTS` (`max_two_day_slot_count`),
so no new budget check is required — the existing one covers this channel
too.

## 4. PC side (rockpod)

### 4.1 Gating

The channel is (re)built only when a forecast has actually been produced:

```
<config.cache_dir>/weather/forecast.tsv
```

exists (this is `build_weather_bundle()`'s host-side cache path in
`rockpod/services/weather.py`). If it is missing, any previously-built
Weather channel/media are left in place but excluded from the next sync's
`shows` list — same drop-and-warn behaviour `LiveTvSync.sync()` already
applies to any channel whose files didn't make it to the device — rather
than crashing or leaving a broken listing.

### 4.2 Music source

New config key, mirroring the `weather_*` keys already in
`docs/rockpod-weather-app-spec.md`:

* `livetv_weather_music_dir` — default `~/Documents/Weather Songs`.

Every `.mp3`/`.m4a`/`.flac`/`.wav`/`.ogg` file directly inside it is a
candidate track. No internet fetch, no bundled music — the channel simply
does not build if the folder is empty, the same way Live TV itself does
nothing if `~/Videos/Live` is empty.

### 4.3 Bumper generation

A small new function generates `LIVETV_WEATHER_VARIANTS` (3) bumper files,
each `LIVETV_WEATHER_BLOCK_SECONDS` (3600) long, cached under
`~/.rockpod/livetv/cache/` next to ordinary converted clips (same cache
directory, same pruning). For each variant:

* **video**: `ffmpeg -f lavfi -i "color=..."` generating a slow, original
  colour-cycle pattern at the `LIVETV_MPEG_PROFILE` resolution/rate — not
  derived from any reference image;
* **audio**: the user's tracks concatenated (via ffmpeg `concat` demuxer)
  and looped (`-stream_loop`) to exactly fill the block, offset per variant
  so the three variants don't all start on the same track;
* muxed to the same MPEG-2 program stream profile as every other Live TV
  file, through the same `_mpeg_command`-style ffmpeg invocation already
  used by `LiveTvSync.ensure_mpeg()`.

Each variant becomes one `LiveTvMedia(kind="show", series="Weather",
duration=3600, path=<cache file>, title="Local Forecast", rating="TV-G",
description="Continuous local weather.")`.

### 4.4 Channel wiring

A small helper, additive like `LiveTvLineup.autobuild()`:

* find the channel by a fixed marker (e.g. `category == "Weather"`), never by
  name, so a user rename survives rebuilds exactly like other channels;
* create it if absent (`lineup.next_free_number()`, callsign `WX`);
* set `channel.shows` to the three bumper media keys;
* select ordinary ad media under
  `~/Videos/Live/ADS/Weather/<snow|summer|general>` for `channel.ads`, using
  the same condition/season priority as report inserts. This makes commercial
  breaks weather-specific without changing slot kinds or playback. A missing
  seasonal pool falls back first to `general`, then to the normal shared ad
  pool;
* set `channel.logo` to a user-supplied source image path, if configured,
  converted to a device BMP by the **existing, already-generic**
  `LiveTvSync.install_logos()` — no new logo code is needed. As with the
  DIRECTV wordmark, no logo is fetched by RockPod itself and nothing is
  committed to git; a channel with no configured logo source simply falls
  back to the text call sign like any other channel.

### 4.5 Integration points

Hooked in exactly where `shows`/`ads` are gathered, before
`lineup.autobuild()`/`sync.sync()`:

* `rockpod/scripts/livetv_sync_cli.py` (after `library.scan()`);
* `rockpod/ui/main_window.py`, the scan/autobuild path (~line 7806-7937) and
  the sync trigger (~line 8218-8337).

## 5. Device side (mpegplayer / livetv_guide.c)

### 5.1 Recognising the channel

`struct livetv_channel` already has a free-text `category` field. The weather
render path activates when the *current* channel's category is `"Weather"` —
no new slot `kind`, no `guide.tsv` format change.

### 5.2 Forecast reader

A new, deliberately small reader (not a copy of `weather.c`'s ~80 KB of
static state, which combined with Live TV's own ~90 KB of static tables would
need a BSS audit per `docs/ipodjs-ui-memory-animation-steering.md`): 7 daily
rows and up to 48 hourly rows (today + tomorrow, matching the "only today and
tomorrow" discipline the guide itself already follows), narrow fixed-width
fields sized to what the four panels below actually print. Budget: well under
5 KB, versus weather.c's ~80 KB, because this reader carries no detail-screen
formatting logic, no icon bitmap cache, no background bitmap — just numbers
and short strings for a handful of on-screen fields.

Read from the **existing** device path, no new sync step:

```
/.rockbox/rockpod/weather/forecast.tsv
```

Loaded once, at the moment the Weather channel is tuned (not per frame, not
per row — same discipline `livetv_channel_logo()` already uses for bitmaps).

### 5.3 Panel phases suppress carrier blits

The carrier always keeps its ordinary full-screen video rectangle and decode
lifecycle. During native-data phases, `vo_draw_frame()` sees
`mpegplayer_livetv_weather_hidden` and skips only the framebuffer blit, so a
fresh decoded carrier frame cannot overwrite the panel. Presenter and report
phases clear that flag and immediately draw the latest frame. No display-mode
reconfiguration, stream reopen, seek, PCM change, or buffer ownership change
occurs at a phase boundary.

This is deliberately separate from the guide's picture-in-guide
(`mpegplayer_livetv_pig`), which is untouched: browsing away from the
Weather channel into the guide still shows it live and small in the
corner, exactly like any other channel, because that box means something
there - "this is the channel you tuned away from." Watching the channel
itself has no such thing to show.

### 5.4 Panels

Cycle every 8 seconds (homage to "Local on the 8s"), driven from the
existing Live TV idle tick in `mpegplayer.c`'s `BUTTON_NONE` handler — no new
timer/thread. Each panel is one `lcd_*` full-repaint via `lcd_update_rect()`
calls, using the whole screen (section 5.3):

1. **Current Conditions** — big condition icon + current temp, condition
   text, today's high/low, precipitation chance.
2. **Today's Forecast** — four time-of-day columns (from the hourly rows
   nearest morning/afternoon/evening/night) each with icon, temp, and a
   simple bar, echoing the Weatherscan "TODAY'S FORECAST" strip.
3. **Extended Outlook** — the 7-day list (day, condition, high/low, precip),
   the same data `weather.c`'s overview screen already shows.
4. **Almanac** — sunrise/sunset for the selected day (all data already in
   `forecast.tsv`; no fabricated fields).

Each slide has a 70-pixel animated **weather wall** above its stable data card,
styled after the glossy blue virtual sets and moving forecast backdrops used by
local cable weather channels in the early 2000s. The wall is selected from the
current hourly condition (falling back to today's daily condition):

The upper scene band uses an early-2000s digital-cable glass treatment:
condition-toned depth gradients, a restrained technical grid, chrome rules,
and a fully-painted medium-blue receiver ribbon carrying `LOCAL WEATHER`,
`WX 102 - LIVE`, the current condition, and the exact current Celsius value.
A subtle two-line data pulse travels along the chrome baseline. It replaces
the earlier full-height wipe and near-black title slab, so no animation frame
can resemble a blank black bar. The band contains no procedurally drawn sun,
cloud, rain, snow, moon, or fog shapes. Missed pulse frames are skipped rather
than replayed.

An animation refresh repaints only the 320x70 scene band; it does not redraw
the data card or invoke the icon loader. The renderer has no file access,
decode, allocation, full-screen cache, timer, thread, PCM/mixer call, shared
audio-buffer call, or playlist interaction. Its persistent state is four
machine words (scene epoch, next animation tick, panel index, next panel tick),
so it adds no framebuffer-sized BSS cost and cannot ask playback to shrink.

Condition icons reuse the same dimensional CGI atlas as the standalone weather
app. `tools/generate_weather_icons.py` crops and downsamples the consistent
production source at
`rockpod/assets/weather/source/broadcast-weather-atlas-v2.png` into 40px and
64px colour-keyed BMPs under `rockpod/assets/weather/icons/`. The result uses
beveled, studio-lit broadcast symbols rather than emoji, line art, or
procedurally drawn weather shapes. They are read from the same device path
`weather.c` already reads
(`WEATHER_ICON_DIR`). `LiveTvSync.sync()` deploys them there whenever the
Weather channel is in the line-up. A fresh install with no icon pack yet on
the device falls back to a plain soft circle, never a hand-drawn shape.

A one-line ticker at the bottom of every panel carries the location name and
a short status string (`Updated by RockPod` / `Stale` / `Expired forecast`),
ported from `weather.c`'s `weather_status()` logic — freshness must stay
visible on every panel, per the release-blocker rule in
`docs/rockpod-weather-app-spec.md` ("never hide stale data").

**Regional/Travel Cities is intentionally omitted** — `forecast.tsv` only
ever holds one location, and fabricating a second city's data to fill out
the classic rotation would be showing invented numbers as if they were real.

### 5.5 Redrawing the chrome

Because the panels are hidden-video chrome rather than a video that
continuously repaints itself, every place the screen could otherwise be
left blank on this channel needs an explicit repaint:

* `livetv_overlay_hide()` (SELECT/PLAY on any channel) repaints the info
  banner/mini-guide strip's own region afterwards via `stream_draw_frame()`;
  on the Weather channel that only covers the video (hidden, so a no-op),
  so it also calls `livetv_weather_draw()` to repaint the chrome the strip
  had drawn over.
* `livetv_guide_session()`'s "return to watching" path (MENU → guide →
  SELECT/back) already clears the screen to black before resuming; it
  calls `livetv_weather_draw()` there too.
* Plain channel up/down (`MPEG_RW`/`MPEG_FF`) reopens the stream without
  going through either of the above, and without setting
  `livetv_banner_pending` - originally the only trigger checked at the top
  of `button_loop()`. Nothing there repainted the chrome, so the screen
  stayed on whatever `stream_open()` had left (typically black) until the
  next 8-second rotation tick. Fixed by also checking
  `mpegplayer_livetv_weather_hidden` at that same point, unconditionally.

### 5.6 Empty/stale states

At tune time, if `forecast.tsv` cannot be read or parses to zero rows, show
a single static panel — "Forecast unavailable, sync with RockPod" — instead
of entering the rotation, mirroring `weather.c`'s `draw_empty()`. This is the
in-session counterpart to §4.1's build-time gating: a channel that *was*
synced once but has gone stale must still say so rather than looping garbage.

### 5.7 Broadcast flow and video inserts

Forecast shows use a repeating 120-second carrier clock. The mpegplayer
audio-master stream timestamp, rather than UI-entry time or a separate wall
clock, selects the phase. This keeps decoded speech and the report picture
locked across stream-open and seek latency, while tuning in midway through an
hour still lands on the content the carrier is currently showing:

| Clock type | Presentation |
| --- | --- |
| Panel clock | Every available forecast/presenter board, with short radar holds and one-second fade, wipe, smooth-push, or slide transitions |
| Report clock, opening | Forecast panels through second 48 (or earlier only when a longer complete report needs the room) |
| Report clock, insert | The complete source clip, with video and speech beginning on the same decoded timestamp |
| Report clock, return | Three-second forecast-continuation card, then the changing panel clock for the remainder |

The host builds this sequence from clean, text-free artwork in
`~/Documents/Weather Channel/presenter-backplates` (falling back to
`presenter` for older installations) and condition-specific videos below
`interstitials/general` or `interstitials/snow`. Before ffmpeg builds a
carrier, RockPod reads the same cached `forecast.tsv` that it will send to
the iPod and renders current conditions, location, high/low, precipitation,
wind, and the extended outlook onto each backplate. All displayed
temperatures are converted to Celsius and wind speeds to km/h even if the
source bundle uses imperial units. The rendered copies live only in the
host-side Live TV state directory; the clean originals are never modified.
This prevents generated example numbers, fictional map labels, or stale
values from reaching the broadcast.

The hourly condition nearest the sync computer's local clock selects the snow
directory when its code contains snow, sleet, ice, or freezing. Snow routing
always takes priority. Otherwise June through August select
`interstitials/summer`, allowing a summer anchor read to replace the generic
studio cut; the other months use `interstitials/general`. A missing or empty
seasonal folder falls back to `general`. A changed forecast, backplate, or
selected insert invalidates the rendered still and carrier caches on the next
sync.

The hour is not thirty back-to-back presenter reports. Three clocks spaced
twenty minutes apart carry the condition-matched forecast reports, five carry
distinct news breaks, one
near the half hour carries viewer comments when available, and the remaining
twenty-one clocks are the changing local-data panel service with music. This
gives the channel a believable forecast/music cadence without repeating the
same presenter every two minutes. Condition-matched clips lead the pool;
when that pool is thin, byte-distinct Carissa Codel anchor and field segments
from the season-appropriate general library fill the rotation to at least
three clips. Exact duplicate files are ignored. Clips without an audio stream
or shorter than 45 seconds are omitted until they are recut as a complete,
natural report.

If no condition-matched report clears that floor, those three clocks remain
changing local-data panel service. News and viewer-comment breaks continue
normally; a thin forecast pool does not disable the rest of the broadcast
layer or force a short or mismatched weather clip on air.

The panel service is not a presenter slideshow. RockPod interleaves the
forecast-aware presenter backplates with presenter-free next-24-hours,
seven-day, precipitation-timeline, and wind-outlook boards. Every plotted
value comes from the synced hourly/daily rows. When online map refresh
succeeds, four current radar frames are rendered from an OpenStreetMap base
and RainViewer radar tiles. Those frames run as a compact twelve-second radar
animation with one-second dissolves; ordinary boards rotate through
restrained fade, wipe-left, smooth-left, and slide-up broadcast transitions.
The radar board prints both providers and the radar observation time. If map
refresh fails, cached radar may remain available, but the forecast boards
continue without inventing map data.

News inserts are a separate format break. Complete clips placed in
`interstitials/news` must be 24–300 seconds, are labelled `NEWS BREAK`, and
are placed at five spaced opportunities per hour. A source longer than 112
seconds receives one continuous 240- or 360-second carrier clock; the hour
advances by the matching number of 120-second blocks, so the report is never
split or interrupted. News never replaces or pretends to supply the current
forecast: the long-form condition-matched report rules above remain
unchanged, music is side-chain ducked just before speech and recovers after
the exact end of the clip. The source video fills the upper 320×176 content
window while a host-baked 64-pixel ribbon shows the actual synced location,
Celsius temperature, condition, precipitation, wind, and high/low using the
dimensional broadcast icon atlas. This is the same size and information
hierarchy as the commercial ribbon, not an unrelated pasted graphic.

An optional `interstitials/news/clips.json` manifest selects `lower-third` or
`sidebar` per clip. The alternate sidebar retains that exact bottom ribbon,
shrinks the source without stretching into a 216×176 program window, and
uses the remaining 104-pixel right column for the next three real forecast
hours: time, temperature, precipitation probability, wind direction, and
wind speed. Conditional weather-news rules use the same `overlay_style`
field. Missing or invalid values fall back to `lower-third`; general news
without a manifest alternates both layouts in stable filename order.

Weather-news reports such as tornado coverage remain in the conditional
manifest, not the general news folder. A `weather_overlay` rule marks them
for the same live-data ribbon, while the existing condition, precipitation,
temperature, and month checks still decide whether sync activates them.
Tornado reports use the `thunder` condition gate and cannot appear in clear,
ordinary rain, or winter rotations.

The decoded report uses its measured full duration and ends at its source
sentence boundary. It is never padded with a frozen presenter or arbitrarily
cut to a nominal report length. A brief forecast-aware continuation slate
bridges directly back to the changing panels. The report also receives a
small original
`WX 102` channel bug
labelled `RECORDED REPORT`. This keeps the station identity continuous while
making it unambiguous that an archival presenter clip is not the source of
the current Moncton observations shown on the surrounding live-data panels.
No end fade is applied to report speech. Forecast reports longer than 112
seconds are rejected instead of being shortened. At that ceiling the host
uses a four-second station lead-in and a four-second return, preserving the
entire source inside the 120-second clock rather than trimming its final
sentence.

The carrier is a single continuously decoded full-screen timeline, including
the rendered forecast panels. No native overlay is allowed to cover the
decoded picture, so report speech cannot begin under a blue framebuffer.
Changing phase does not reconfigure, stop, reopen, seek, or take ownership of
audio.

Weather commercials use only the channel-specific
`Live/ADS/Weather/<season>` and `Live/ADS/Weather/general` pools. Seasonal
safety spots and general local-cable-style advertising can coexist; unrelated
shared-channel ads never leak into the Weather schedule. During an ad, the
decoder is clipped above a 64-pixel broadcast lower-third showing a
dimensional condition icon from the synced weather artwork plus the current
location, temperature, condition, precipitation, wind, and high/low. The
commercial audio/video is otherwise untouched. The lower-third is restored after
volume, information-banner, and guide overlays without reopening, seeking, or
changing the audio path. On hardware, the opaque strip is composited into the
same YUV presentation buffer as the decoded frame. It does not depend on an
ordinary framebuffer update while the iPod LCD is in YUV mode, which would
leave the deliberately clipped bottom band black on native targets.

The final host mix does not rely on repeated-file concat timestamps for
audio. RockPod creates one normalized music bed and loops it by decoded sample
count; the 30 clock audio tracks are concatenated as explicit filter inputs.
The acceptance check requires the last audio packet, not merely the container
duration, to reach the end of the 3600-second carrier.

While the Weather channel is active, the green volume strip uses an
opaque Weather-navy base rather than copying pixels from the decoded carrier.
This prevents a presenter or previous carrier frame from appearing
behind the volume meter; the panel is redrawn when the strip closes.

## 6. Assets

Consistent with how the DIRECTV wordmark is handled today (fetched once from
a source tagged public domain, converted, never committed to git, text
fallback when absent):

* **No trademarked logo, jingle, commercial, presenter photograph, or web
  video is committed by this feature.** Personal presenter and interstitial
  files live outside the repository. Their source manifest is copied beside
  them and must be reviewed by the user before redistribution.
* **Presenter values are deterministic data overlays, not generated text.**
  Image generation may be used only to make a clean personal backplate.
  RockPod draws the exact forecast values afterward, during host-side sync.
* **Music** is entirely the user's own local library
  (`livetv_weather_music_dir`) — nothing is downloaded.
* **Weather commercial pool** uses intact, attributed personal-source videos
  under `~/Videos/Live/ADS/Weather/<season>`. Official public-domain NWS/NOAA
  PSAs are preferred. RockPod does not download or commit them; it only scans
  and schedules files the user has placed there. These station-specific ads
  are excluded from every ordinary channel's fallback pool; if no matching
  Weather pool exists, the forecast runs without an ad break instead of
  borrowing an unrelated commercial.
* **Channel logo** uses the same generic, already-existing `channel.logo` →
  `install_logos()` path every other channel uses. If the user points it at
  a real, currently-trademarked logo file on their own disk for personal,
  non-distributed use, that is the same generic per-channel logo mechanism
  already in the tree, converted to the device only — it is never written
  into the git repository or the release zip pipeline.

## 7. Testing

* Simulator: tune the Weather channel with no forecast synced (channel must
  not appear at all), then with a synced forecast (panels rotate correctly,
  ticker shows freshness), then with an expired forecast (empty/stale state).
* Verify SELECT/PLAY overlays and MENU→guide→SELECT all leave the corner
  video rect intact on this channel.
* Verify no full `lcd_update()` call fires while the corner video is
  decoding (flicker check, same as the guide's own verification).
* Check `mpegplayer.rock`'s BSS size delta before a hardware build, per
  `docs/ipodjs-ui-memory-animation-steering.md`.
* Audio matrix from `docs/plugin-audio-lifecycle-steering.md`: Database
  music → Weather channel, Weather channel → Database music, rapid channel
  switching, volume, menu exit.
