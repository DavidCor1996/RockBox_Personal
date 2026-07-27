# Live TV Sync and DIRECTV Guide App — Specification

Status: implementation spec for this custom iPod tree (iPod Classic 6G/7G and
iPod Video 5G/5.5G, 320x240).

This feature adds a simulated live television experience:

* a **DIRECTV-style program guide** application on the iPod under
  *Extras -> Applications -> Live TV*;
* a **Live TV Sync** area in rockpod with two sections, *Shows* and
  *Commercials*;
* a **Live TV tab in the rockpod Store** for downloading new shows and ads;
* a **guide view inside rockpod** rendering the same schedule the iPod plays.

It is deliberately **separate** from Netflix and from the existing Video Sync
path. Live TV content lives in its own PC folders and its own device folder,
and never appears in the normal video list.

---

## 1. Product behaviour

1. The app opens directly into the program guide, with a channel already
   playing in the small video window in the corner (DIRECTV "picture in
   guide").
2. Highlighting a cell and pressing SELECT tunes that channel full screen.
3. Pressing MENU while watching returns to the guide, **with the channel
   still playing in the corner** — playback is never torn down.
4. Programming is driven by the wall clock. At 8:12 pm the channel is 12
   minutes into whatever the schedule says airs at 8:00 pm. The viewer may
   join mid-show or mid-commercial.
5. Each show is followed by a commercial break of **2–3 randomly chosen
   commercials**, drawn from that channel's ad pool.
6. Channels are changed with the click wheel (channel up/down) or from the
   guide.
7. Channel logos are optional bitmaps; when a logo is missing the guide falls
   back to the DIRECTV-correct text call sign (`503 HBOS`).
8. All video is MPEG-1/2 program stream (`.mpg`), decoded by the existing
   mpegplayer engine.

---

## 2. Architecture

### 2.1 Why the guide lives inside mpegplayer

Rockbox runs one plugin at a time; `rb->plugin_open()` tears down the caller.
A standalone `livetv.rock` that drew the guide therefore **could not** keep a
channel decoding in the corner — requirement 3 would be impossible.

This tree already solves the same problem twice:

* `apps/plugins/ipodtiktok.c` is a 94-line launcher; the feed logic lives in
  `mpegplayer.c` as *feed mode*.
* the YouTube "embedded" mode renders video into a 210x158 sub-rectangle at
  (4, 62) with app chrome drawn around it
  (`apps/plugins/mpegplayer/video_out_rockbox.c:564`).

Live TV follows the same precedent:

```
apps/plugins/livetv.c                       thin launcher (.rock in Applications)
   -> rb->plugin_open(VIEWERS_DIR "/mpegplayer.rock", "-livetv:/Videos/LiveTV")

apps/plugins/mpegplayer/livetv.[ch]         schedule model + guide renderer
apps/plugins/mpegplayer/mpegplayer.c        "-livetv:" mode hooks
```

Guide, tuning, banner and picture-in-guide are **states inside one plugin
instance**, not plugin transitions.

### 2.2 Video output

`vo_setup()` already supports an arbitrary scaled destination rectangle, and
`stream_vo_set_clip()` already exists. Live TV adds a third windowed mode:

| State | Video rect | Chrome |
|---|---|---|
| `LIVETV_WATCH` | full screen 320x240 | none (optional info banner) |
| `LIVETV_GUIDE` | **PIG** 80x60 at (236, 4) | full guide grid |

The guide is painted with ordinary `lcd_*` calls and committed with
`lcd_update_rect()` over regions that **exclude** the PIG rectangle. The video
thread continues to blit only the PIG rectangle. Nothing ever issues a full
`lcd_update()` while the guide is up, so the corner video never flickers.

### 2.3 Audio lifecycle

Live TV mode is a normal mpegplayer session. It inherits the existing
`stream_init()` / `stream_close()` / `stream_exit()` path unchanged and so
keeps the guarantees in `docs/plugin-audio-lifecycle-steering.md`:
`PCM_MIXER_CHAN_PLAYBACK`, sample-rate restore on exit, no direct
`audio_stop()`, no playlist mutation. Channel changes reuse
`stream_close()` + `stream_open()` exactly as feed mode's clip advance does,
so no new PCM ownership transitions are introduced.

---

## 3. Schedule model

### 3.1 The determinism requirement

Three components must independently agree on "what is channel 502 playing at
15:47": the rockpod guide, the iPod guide grid, and the iPod player. Any
disagreement destroys the illusion.

Therefore **no randomness at playback time**. Ad selection, ordering and
durations are resolved once, on the PC, during sync. The device performs a
pure lookup.

### 3.2 Weekly loop, not absolute timestamps

A finite list of absolute timestamps expires. Instead the generator emits a
**7-day rotation**: each slot carries a day index `0..6` and a start offset in
seconds from local midnight. Resolution on any platform is:

```
day    = (days_since_epoch_local) % 7
secs   = seconds since local midnight
slot   = last slot on (channel, day) with start <= secs
offset = secs - slot.start          /* how far into the file we join */
```

The schedule never expires, stays small, and gives the same answer on the PC
and on the iPod. Regeneration on each sync re-rolls the ad draw.

### 3.3 Files on the device

All under `/Videos/LiveTV/` (this subtree is excluded from the normal video
list and from Netflix browsing):

```
/Videos/LiveTV/channels.tsv          channel table
/Videos/LiveTV/guide.tsv             7-day schedule
/Videos/LiveTV/logos/<callsign>.bmp  optional 40x18 channel logos
/Videos/LiveTV/shows/...             show .mpg files
/Videos/LiveTV/ads/...               commercial .mpg files
/Videos/LiveTV/.livetv_state         last tuned channel
```

`channels.tsv` (tab separated, `#` comments allowed):

```
number  callsign  name                 category  logo
100     RTRO      Retro Classics       Series    logos/RTRO.bmp
200     GAME      Game Show Network    Variety
300     SPRT      Sports Time          Sports
```

`guide.tsv` (tab separated, sorted by channel then day then start):

```
chan  day  start  dur   kind  title                 rating  path
100   0    0      1740  S     ALF: A.L.F.           TV-PG   shows/alf/s01e01.mpg
100   0    1740   32    A     2000 Flushes          --      ads/2000/flushes.mpg
100   0    1772   31    A     Advil Cold & Sinus    --      ads/2000/advil.mpg
100   0    1803   1755  S     ALF: Strangers...     TV-PG   shows/alf/s01e02.mpg
```

* `kind` is `S` (show) or `A` (advertisement).
* `start`/`dur` are seconds; `start` is seconds after local midnight.
* Slots for a channel/day tile the full 86400 seconds with no gaps; the
  generator loops the show pool until the day is full.
* `path` is relative to `/Videos/LiveTV/`.

### 3.4 Device-side memory budget

Per `docs/ipodjs-ui-memory-animation-steering.md`, decoration must not shrink
playback memory. The schedule is held in fixed static arrays sized as follows,
and **only today and tomorrow are loaded**:

| Table | Cap | Bytes each | Total |
|---|---|---|---|
| channels | 24 | 64 | 1.5 KB |
| slots (2 days) | 2048 | 12 | 24 KB |
| title pool | — | — | 40 KB |
| path pool | — | — | 24 KB |
| **Total static** | | | **~90 KB** |

Nothing is allocated from `core_alloc()`; no tagcache or database access
occurs at any point.

---

## 4. Guide appearance

Colours were **sampled from the real DIRECTV receiver user guide artwork**
(page 15 of the DIRECTV HD & SD Standard Receivers user guide, rendered at
600 dpi and colour-picked), not chosen by eye:

The DIRECTV wordmark is the real one used by the 2004-2011 receivers
(Wikimedia Commons `File:DirecTV logo (2004-2011).svg`, public domain),
fetched once and rendered to a 64x22 BMP whose padding is the banner colour
so it disappears into the guide. When it is missing the guide draws the brand
as text instead. Channel logos are optional 40x18 BMPs padded with the
channel-column navy, falling back to the DIRECTV-correct call sign.

| Element | Sampled | `LCD_RGBPACK` |
|---|---|---|
| Banner gradient top | `#DCEEF9` | 220, 238, 249 |
| Banner gradient bottom / info strip | `#B7D6E9` | 183, 214, 233 |
| Info-strip text (navy) | `#10386B` | 16, 56, 107 |
| Description block | `#026FAF` | 2, 111, 175 |
| Time-header row / channel column | `#122549` | 18, 37, 73 |
| Grid row background | `#094871` | 9, 72, 113 |
| Grid separator line | `#0A2A50` | 10, 42, 80 |
| Selected cell | `#FEC425` | 254, 196, 37 |
| Selected cell text | `#10254A` | 16, 37, 74 |
| Hint bar | `#0F5689` | 15, 86, 137 |
| Dim / unavailable text | `#5C82A8` | 92, 130, 168 |
| Red hint dot | `#C22A18` | 194, 42, 24 |
| Green hint dot | `#2EA15C` | 46, 161, 92 |
| Yellow hint dot | `#F9C63C` | 249, 198, 60 |

### 4.1 Layout at 320x240

```
 y  0.. 25   banner: DIRECTV logo | program title | "guide" wordmark
 y 26.. 40   info strip: "Thu 12:01p" | "11:00p - 12:30p" | "PG-13"
 y 41.. 76   description block (3 lines, white on #026FAF)
             PIG video window 80x60 at (236,4), 1px white border,
             overlapping banner/strip/description exactly as the reference
 y 77.. 90   time header: "Thu 1/31 | 12:00p | 12:30p | 1:00p"
 y 91..222   grid: 6 rows x 22px
             channel column 58px (#122549), 3 half-hour columns of ~87px
 y223..239   hint bar: (red) -12 hrs   (green) +12 hrs   (yellow) Guide Options
```

### 4.2 DIRECTV features reproduced

* Grid of channel rows against half-hour time columns.
* Programs wider than one column span columns; programs that started before
  the window show a leading `<`; programs continuing past the window show a
  trailing `>` at the right edge.
* Selected cell in DIRECTV gold with navy text.
* Live info banner at the top with title, day/time, air window and rating.
* Multi-line program description block.
* Picture-in-guide live video window.
* One-line **mini guide** overlaid on full-screen video.
* **Guide Options** panel (yellow header, blue body) with
  *Sort programs by category*, *Jump to a date & time*, *Change favourites
  list*.
* Category filter / favourites list (All Channels vs. a favourites subset),
  shown at bottom-left exactly as the reference does.
* `-12 hrs` / `+12 hrs` time jumps.
* Channel entry by number and channel up/down.
* Guide "banner" row support (a promotional row between channels).
* Greyed-out rendering for channels with no content synced.

Everything is drawn from sampled colours and real text metrics — there is no
hand-drawn imitation artwork.

---

## 5. Controls (IPOD_4G_PAD)

**In the guide**

| Input | Action |
|---|---|
| Wheel / Up / Down | move up and down channels |
| Left / Right | move across the time grid (previous/next programme) |
| SELECT | tune the highlighted channel (goes full screen if it is airing now) |
| PLAY, long | Guide Options |
| MENU | leave Live TV |

**Watching full screen**

| Input | Action |
|---|---|
| MENU | back to the guide, channel keeps playing in the corner |
| Wheel | channel up / down with the DIRECTV info banner |
| SELECT | show the info banner |
| PLAY | mini guide |
| MENU, long | leave Live TV |

---

## 6. PC side (rockpod)

### 6.1 Source folders

| Folder | Purpose |
|---|---|
| `~/Videos/Live/` | shows (recursive; one sub-folder per series is fine) |
| `~/Videos/Live/ADS/` | commercials (recursive) |
| `~/.rockpod/livetv/staging/` | Store downloads, deleted after a verified sync |

Scanning rules:

* accept `.mp4 .m4v .mkv .avi .webm .mov .mpg .mpeg .ts`;
* skip `.part`, `.vtt`, `.txt`, `.srt`, `.jpg`, `.json` sidecars;
* skip Internet Archive duplicates: when both `X.mp4` and `X.ia.mp4` exist,
  keep one (`X.mp4`);
* `~/Videos/Live` is **excluded** from the ordinary video library scan so Live
  TV content never shows up in Video Sync or in the Netflix browser.

### 6.2 Live TV Sync panel

Two sections, exactly as requested:

* **Shows** — every discovered show, its channel assignment, duration and
  sync state.
* **Commercials** — every discovered ad, its channel pool assignment (or
  *All channels*), duration and sync state.

Plus channel management and a "Generate Schedule and Sync" action.

**Channels** can be added, renamed, renumbered and removed. Double-clicking a
row edits it. Shows and commercials are assigned to a channel from the Shows
and Commercials tabs (double-clicking assigns to the channel in the picker),
and can be unassigned again.

Two rules keep hand editing safe:

* **"Build Channels" is additive.** It only creates channels for series that
  have none, and only assigns shows that are not assigned anywhere. A channel
  is recognised by *what is assigned to it*, not by its name, so renaming one
  does not resurrect a duplicate on the next rebuild.
* **Channel numbers are unique**, enforced when a line-up is saved, loaded and
  synced. The device resolves a channel by number and takes the first match,
  so a duplicate would silently hide one channel's listings behind another's.
  Line-ups written before this rule are repaired on load.

### 6.3 Transcode

320x240 MPEG-2 program stream, 20 fps, MP2 audio at 44.1 kHz — the profile
mpegplayer expects. Durations come from `ffprobe` at scan time and are cached
so scheduling never re-probes.

**Framing fills the screen.** Live TV content is watched on a 4:3 screen, and
fitting a 16:9 rip inside it would letterbox the picture into a small band —
a 640x360 recording would occupy 320x180 of the 320x240 panel. A television
of this era centre-cut widescreen instead, so the encode scales until the
screen is covered and crops the overhang:

```
scale=iw*sar:ih,                                    square anamorphic pixels
scale=320:240:force_original_aspect_ratio=increase, cover the screen
crop=320:240,setsar=1,fps=20                        trim the overhang
```

The leading `scale=iw*sar:ih` matters for anamorphic 720x480 rips, which
would otherwise come out horizontally squashed.

The cache is keyed by `LIVETV_MPEG_PROFILE`, so changing the framing
re-encodes rather than silently reusing clips built the old way, and stale
profile directories are removed on the next sync.

### 6.4 Store Live TV tab

A Live TV tab in the Store searches and downloads period-appropriate shows and
commercials. Downloads land in `~/.rockpod/livetv/staging/`. After a sync that
verifies the device copy (size match), the staging file is deleted.

**The user's own `~/Videos/Live` library is never auto-deleted** — auto-delete
applies only to files the Store itself fetched into the staging directory.

### 6.5 Guide view in rockpod

The rockpod Live TV area renders the same `guide.tsv` in the same DIRECTV
palette, using a live clock, so the PC shows exactly what the iPod is playing.

---

## 7. Verification

* `tools/livetv_guide_sim_gate.py` — drives the simulator into the guide and
  checks the rendered screen against the sampled palette and layout.
* Python unit tests for schedule determinism: PC resolver and the C resolver
  logic must return the same slot for the same instant.
* Simulator run, then hardware builds for `ipod6g` and `ipodvideo`.
* Audio matrix from `docs/plugin-audio-lifecycle-steering.md`: Database music
  -> Live TV, Files music -> Live TV, Live TV -> Database music, rapid
  app/menu switching, volume, and menu exit.

---

## 8. Sources

* DIRECTV HD & SD Standard Receivers user guide (D12), pages 14-16 and 25 —
  program guide, guide banners, mini-guide, Guide Options.
  <https://content.abt.com/documents/23796/d12_manual.pdf>
* DirecTV Channel Guide EPG (2000-2005) reference template.
  <https://www.deviantart.com/blumaster2006/art/DirecTV-Channel-Guide-EPG-2000-2005-template-1220296652>
