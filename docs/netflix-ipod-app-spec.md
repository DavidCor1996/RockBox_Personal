# Netflix iPod App Spec

## Product Definition

Create a personal-use, offline Netflix library app for the iPod Classic 6G/7G
and iPod Video 5G/5.5G. The app browses and plays movies and TV episodes that
are already synced to the device. It is not a Netflix client, does not sign in,
does not stream, and does not download protected Netflix media.

The result should feel like a stock 2007 iPod application designed in
collaboration with disc-era Netflix:

- Apple iPod navigation, typography, status bar, click-wheel behavior, and
  transition grammar;
- the real 2001-2014 Netflix wordmark, not traced text or a substitute font;
- the white, gray, dark red, black, and gold-star visual language visible in
  Netflix's 2007 Browse, Queue, and Watch Now pages;
- real movie and TV posters, episode stills, ratings, runtimes, genres, years,
  season/episode numbers, and summaries from the user's local RockPod library;
- no hand-drawn placeholder covers, fake movie names, or generated key art.

The period reference is the January 2007 Watch Now interface, including its red
navigation treatment, white catalog surfaces, blue-gray utility chrome, title
rows, cover images, and star ratings. A surviving reference capture is linked
from [this history of the 2007 launch](https://www.elindependiente.com/economia/2018/01/20/el-dia-que-netflix-dinamito-el-sistema-audiovisual/).

## Scope And Non-Goals

The first complete release includes:

- a `Netflix` entry on the iPodJS home screen;
- a short branded launch screen;
- Watch Instantly, Movies, TV Shows, My Queue, and Search;
- full poster art on title and show detail screens;
- show -> season -> episode navigation;
- complete useful metadata on-device;
- playback through the existing format-aware Rockbox video dispatch;
- deterministic host-side metadata and art preparation in RockPod; and
- graceful empty, missing-art, unsupported-format, and corrupt-index states.

It does not include:

- Netflix authentication, APIs, streaming, recommendations, or DRM handling;
- scraping netflix.com;
- claims that a locally owned file is currently available from Netflix;
- network access from Rockbox;
- trailers, autoplay previews, or video decoding in list draw functions;
- background music or general navigation sounds (the authentic launch ident
  sound is the sole branded-audio exception);
- changes to the user's music playlist; or
- replacement of the existing generic `Videos` browser.

`Netflix` is a personal library skin and launcher. The UI should say `On this
iPod` in an About/Credits screen so it cannot be mistaken for an official or
connected Netflix product.

## Architecture Decision

Implement the catalog as a native iPodJS screen and keep playback in the
existing video players.

```text
RockPod library and metadata services
        |
        | atomic sync bundle
        v
/.rockbox/rockpod/netflix/
        |
        | bounded manifest parse + idle artwork service
        v
apps/gui/ipodjs_netflix.c
        |
        | selected local path
        v
filetype_load_plugin() -> mpegplayer.rock or openh264_player.rock
```

This split is required for three reasons:

1. The catalog needs iPodJS navigation state, native transitions, Hold/USB
   behavior, and cached art service points that already exist in the core UI.
2. A catalog plugin would unnecessarily take plugin memory while the user is
   only browsing.
3. MPEG and RVP playback already have format dispatch and the repository's
   required shared-audio-buffer lifecycle. Netflix must not fork those paths.

Suggested source ownership:

```text
apps/gui/ipodjs_netflix.c
apps/gui/ipodjs_netflix.h
rockpod/services/netflix_export.py
rockpod/tests/test_netflix_export.py
tools/ipodjs_netflix_sim_regression.sh
```

`apps/root_menu.c` should only register the menu item, enter the screen, and
route the selected file to the normal video dispatcher. Parsing, navigation,
and drawing belong in `ipodjs_netflix.c`, not in another large block inside
`root_menu.c`.

## Information Architecture

### Home

The top-level rows are fixed and appear in this order:

1. `Watch Instantly`
2. `Movies`
3. `TV Shows`
4. `My Queue`
5. `Search`

`Watch Instantly` is the period-authentic name for a local view. It contains,
in order:

- resumable titles, once a player progress contract exists;
- then unwatched items sorted by `date_added` descending; and
- a maximum of 64 rows in the first release.

If persistent progress is unavailable for the selected player, the first
release labels this view `Recently Added` internally but retains `Watch
Instantly` on screen. It must not invent progress bars.

`Movies` contains `video_kind=movie` only. `TV Shows` groups
`video_kind=show` rows by stable show id, then season number, then episode
number. `My Queue` contains only rows explicitly marked by the user in RockPod.
`Search` searches the loaded manifest's normalized title, show title, cast,
director, and genre fields without touching tagcache or opening media files.

### Movie flow

```text
Movies -> title list -> movie detail -> Play / Resume -> player -> detail
```

### TV flow

```text
TV Shows -> show list -> show detail -> season list -> episode list
          -> episode detail -> Play / Resume -> player -> episode detail
```

For a one-season show, Select on the show detail opens its episode list
directly. Specials are `Season 0` in the manifest and display as `Specials`.

### Return behavior

- Menu always unwinds exactly one Netflix hierarchy level.
- Menu on Netflix Home returns to iPodJS Home.
- Player Menu returns to the exact title/episode detail and selection state.
- A player error returns to detail with a concise error banner; it does not
  jump to Rockbox Home.
- Play/Pause from the Netflix catalog follows the normal iPodJS music shortcut
  and must not silently replace the user's current playlist.

## Visual System

### Shared iPod chrome

The application owns the area below the normal 20-pixel iPodJS status header.
It reuses the loaded iPodJS UI font, battery, Hold, and playback indicators.
It does not load a new font and does not reproduce the status bar in a branded
bitmap.

Base geometry for 320x240:

| Element | Geometry |
| --- | --- |
| iPod status header | `0,0 320x20` |
| application content | `0,20 320x220` |
| split-list left pane | `0,20 146x220` |
| split-list right pane | `147,20 173x220` |
| divider | `146,20 1x220` |
| standard list row | 36 px |
| compact season row | 28 px |
| footer/action strip | `0,214 320x26` when present |

The normal iPod header title is `Netflix`. A 2-pixel dark-red rule directly
under the header is the only brand change to Apple chrome.

### Period Netflix palette

Use measured colors from the selected real reference assets during asset
preparation. These are starting constants, not a license to redraw the logo:

| Role | Color |
| --- | --- |
| Netflix deep red | `#B20710` |
| brighter active red | `#D21F26` |
| page white | `#FFFFFF` |
| warm panel gray | `#F2F1EE` |
| divider gray | `#C7C5C0` |
| primary text | `#171717` |
| secondary text | `#66635F` |
| rating gold | `#E4A400` |
| selected iPod text | `#FFFFFF` |

Netflix red is an app accent. The selected list row retains the stock iPod
blue gradient so the application still reads as a 2007 Apple app. Red is used
for the wordmark, thin rules, section tabs, progress fill, and Play button.

### Launch screen

- Source: Netflix's official 2013 ident, which uses the same older wordmark
  family as this personal library skin.
- Duration: 3.3 seconds from the visual lead-in through the complete sound
  logo; any queued input cancels it.
- Visuals: twelve authentic 320x180 source frames at 10 fps, letterboxed on
  the 320x240 display, followed by a hold on the final red wordmark.
- Audio: the original 44.1 kHz stereo sound logo on
  `PCM_MIXER_CHAN_PLAYBACK`.
- Playback ownership: the selected viewer takes the shared plugin audio
  buffer once, runs the ident from that buffer, stops the intro channel, and
  then initializes the selected video. The catalog UI never allocates or
  owns an intro framebuffer.

The launch pack is an installed personal asset. Do not typeset `NETFLIX`
with an approximation and do not substitute the modern ribbon `N`.

### Home screen

Home uses the stock iPod split-pane composition:

- fixed navigation rows in the left 146-pixel pane;
- selected item uses the ordinary iPodJS blue gradient and arrow;
- the right pane uses a warm white background;
- `Watch Instantly`, `Movies`, `TV Shows`, and `My Queue` show one real poster
  at 112x168, centered, with a 1-pixel gray keyline and subtle fixed shadow;
- `Search` shows the existing real Apple search-field asset, not a new drawing;
- the 2007 Netflix wordmark appears at 88x24 above or below the poster when
  space allows.

The featured poster is deterministic: first resumable item, otherwise first
queued item, otherwise newest synced public title. There is no slideshow in
the first release.

### Title and show lists

Lists use 40-pixel rows containing a real 24x36 poster crop, title, and one
secondary metadata line.

- Movie secondary line: `2007  PG-13  1h 42m`.
- Show secondary line: `3 Seasons  42 Episodes`.
- Episode secondary line: `S02:E05  22 min`.
- Missing values collapse cleanly; adjacent separators are never left behind.
- A red 2-pixel progress mark is shown only when real saved progress is known.
- Unsupported files remain visible and use muted text plus `Can't Play`.

The selected row may show a 104x156 full poster in the right pane after the
idle artwork service has loaded it. Until then it shows a neutral gray poster
frame containing the title as normal text. This fallback is deliberately not
fake or hand-drawn cover art.

### Movie detail

Movie detail is a full-screen cached frame below the status bar:

- real poster at `8,27 96x144`;
- title at `112,28`, up to two lines;
- metadata line with year, content rating, runtime, and format;
- real or imported 0-5 star rating using a period-authentic star-strip asset;
- up to two genre lines;
- director and principal cast when available;
- synopsis in a scrollable text area beginning below the poster;
- a red `Play` or `Resume` pill and a gray `More` pill in the footer.

Select activates the focused footer action. Wheel moves between actions, then
scrolls the synopsis. Select-hold opens a text-only options menu with `Play
from Beginning`, `Add/Remove from My Queue`, `Video Info`, and `Credits`.

### Show and episode detail

Show detail uses a real series poster and displays:

- canonical show title;
- start year or year range;
- content rating and genres;
- season and episode counts present on the device;
- imported rating;
- summary; and
- `Episodes` and `My Queue` footer actions.

The season screen is a stock list. Each season row may use its real season
poster when RockPod has one, otherwise the real show poster. The episode list
uses a real 64x36 episode still when present and a show-poster crop otherwise.
Episode detail shows the episode still at 144x81, episode title, `Sxx:Eyy`, air
date/year, runtime, rating, and synopsis. It never substitutes an unrelated
frame extracted from another episode.

### Playback overlay

Playback remains owned by the selected viewer plugin. The catalog does not
draw over live video. A later player integration may add a `netflix` skin mode
with:

- the period wordmark at top-left;
- current title and episode identity;
- white elapsed/remaining time;
- a dark-red progress fill; and
- stock Play/Pause, wheel-volume, seek, Hold, USB, and Menu behavior.

That player mode is a separate acceptance slice. The catalog is complete when
it launches the existing player and reliably returns, even if the existing
player OSD is still used.

## Real Asset Contract

No AI-generated, hand-drawn, or title-simulating artwork is allowed in the
Netflix bundle. Assets fall into three classes.

### Brand assets

Use the actual 2001-2014 Netflix wordmark seen in the 2007 interface. Preserve
its proportions, letterforms, outline, and shadow. Store the original source
only in the user's RockPod cache and sync optimized Rockbox BMP derivatives.

The modern official Netflix brand page can be used to verify current color and
wordmark handling, but its current mark must not replace the selected period
mark. The modern page identifies `#E50914` and `#B20710` as Netflix reds:
[Netflix Brand Assets](https://brand.netflix.com/en/assets/logos/).

### Catalog art

Artwork priority is:

1. user-selected `artwork_path` in RockPod;
2. an adjacent real `poster.jpg`, movie-named image, season poster, or episode
   still;
3. the artwork already cached by RockPod's video metadata/artwork services;
4. an explicit online lookup initiated on the host by the user; and
5. a text-only neutral fallback.

TMDB is an optional host-side source. Its official documentation defines image
URLs from configuration, size, and returned file path, and its non-commercial
terms require attribution. When it supplies data or images, add the required
notice and approved TMDB mark to Netflix `Credits`; see
[TMDB image basics](https://developer.themoviedb.org/docs/image-basics) and
[TMDB API FAQ](https://developer.themoviedb.org/docs/faq).

Never perform an online lookup during device sync unless the user has enabled
online artwork lookup. Never fetch artwork on the iPod.

### Pre-rendered Rockbox assets

RockPod produces exact-size, 24-bit BMPs in the build cache, then syncs them to:

```text
/.rockbox/rockpod/netflix/assets/wordmark-2007.150x45x24.bmp
/.rockbox/rockpod/netflix/assets/wordmark-2007.88x24x24.bmp
/.rockbox/rockpod/netflix/assets/stars-2007.60x66x24.bmp
/.rockbox/rockpod/netflix/posters/list/<asset_id>.24x36x24.bmp
/.rockbox/rockpod/netflix/posters/detail/<asset_id>.104x156x24.bmp
/.rockbox/rockpod/netflix/posters/full/<asset_id>.112x168x24.bmp
/.rockbox/rockpod/netflix/stills/list/<asset_id>.64x36x24.bmp
/.rockbox/rockpod/netflix/stills/detail/<asset_id>.144x81x24.bmp
```

Host rendering uses a high-quality aspect-fill or aspect-fit operation selected
by role. Poster roles are aspect-fit against a white/gray matte and never crop
title text off the poster. Episode still roles are aspect-fill with a centered
crop. The exporter records source and output hashes so unchanged art is not
rewritten.

## On-Device Data Bundle

Do not stretch the existing 12-column `/.rockbox/videolist/index.tsv` until it
becomes ambiguous. Netflix uses a versioned bundle generated from the same
RockPod rows:

```text
/.rockbox/rockpod/netflix/
    library.tsv
    credits.tsv
    assets/
    posters/list/
    posters/detail/
    posters/full/
    stills/list/
    stills/detail/
    state/
        queue.tsv
        progress.tsv
```

`library.tsv` begins with:

```text
# rockpod netflix library v1
asset_id kind title sort_title device_path show_id show_title season episode year duration content_rating genre rating director cast plot_short plot_long date_added poster_list poster_detail poster_full still_list still_detail playable format locked
```

Rules:

- fields are tab-separated UTF-8 with tabs/newlines replaced by spaces;
- one physical line per playable media item;
- `kind` is `movie` or `show` for playable rows;
- `show_id` is stable across title spelling corrections;
- `season` and `episode` are base-10 integers or blank;
- `duration` is whole seconds;
- `rating` is `0` to `50`, representing 0.0 to 5.0 stars;
- cast is a comma-separated display string already bounded by RockPod;
- asset paths are relative to the Netflix bundle;
- `device_path` is absolute on-device and may not contain a tab/newline;
- `playable` reflects the synced output, not the host source extension;
- locked video rows are excluded completely in v1; and
- no parser field may exceed 1024 bytes after sanitization.

The parser loads a bounded index of at most 512 media rows. If a library is
larger, RockPod emits the first 512 according to user-visible sort rules and a
`truncated=1` header field; Netflix displays `Showing first 512 titles`.

`credits.tsv` records only sources actually used by the installed bundle. It
supports the required TMDB notice and a local `Personal, unofficial app` line.

### Queue and progress

RockPod is the authoritative source for queue membership in v1. Queue changes
made on-device are written atomically to `state/queue.tsv` as stable asset ids
and merged back by RockPod on the next sync.

Persistent progress is enabled only after both players expose the same small
contract:

```text
asset_id elapsed_seconds duration_seconds completed updated_at
```

The player writes through a temporary file plus rename after pause, clean exit,
and every 60 seconds at most. Completion means at least 95 percent watched;
completed titles leave Watch Instantly but retain their progress record.
Until `openh264_player` can report and restore progress consistently with
`mpegplayer`, the UI may expose resume only for formats that actually support
it and must not fabricate parity.

## RockPod Export

The exporter reuses the current video library, metadata, online artwork, video
thumbnail, and sync services. It should not introduce a second scanner.

For every synced video, RockPod must resolve or preserve:

- movie title or episode title;
- normalized `video_kind`;
- stable external/provider id when available;
- show title, season number, and episode number;
- year or episode air date;
- duration, target format, and playability;
- genre, content rating, imported star rating;
- director and bounded principal cast;
- short and long synopsis;
- real poster and optional episode still; and
- target device path after conversion.

The current RockPod database already stores most of the required identity and
summary fields. Add explicit `content_rating`, `video_rating`, `director`,
`cast_display`, `release_date`, and `date_added` export support where the
normalized source has them. Do not overload the music `rating` field without a
documented conversion.

Export order:

1. finish media conversion and determine final device paths;
2. resolve metadata using local values first;
3. resolve real artwork under the user's online-lookup preference;
4. render all fixed-size BMP derivatives in the host cache;
5. validate every manifest-relative path and every device media path;
6. write the bundle to a staging directory;
7. hash and sync only changed files;
8. atomically replace `library.tsv` last; and
9. remove obsolete Netflix-owned art only after the new manifest is installed.

If metadata lookup fails, retain the media row with local tags and filename
fallbacks. If real art is unavailable, use the neutral text fallback. A failed
online lookup is not a sync failure. A missing or invalid path for a row marked
playable is a hard validation failure for that row.

## Input Contract

| Input | Catalog behavior |
| --- | --- |
| Wheel | move selection; scroll long synopsis after action focus |
| Select | open selected hierarchy or activate focused action |
| Select hold | options menu on a title/episode |
| Menu | one level back; from Home return to iPodJS Home |
| Previous/Next | previous/next tab on detail; seek only inside player |
| Play/Pause | normal iPodJS music shortcut in catalog; pause in player |
| Hold | show normal iPodJS lock overlay and ignore catalog input |

Wheel movement cancels pending optional art work for the abandoned row. A
queued Menu or system event bypasses branded animation and is processed first.

## Memory, I/O, And Animation Constraints

All rules in `docs/ipodjs-ui-memory-animation-steering.md` apply.

The drawing API paints cached pixels and strings only. It may not call
`open()`, `stat()`, `read_bmp_file()`, directory enumeration, tagcache, image
resize, or manifest parsing. All such work occurs at screen entry or from a
bounded idle service point.

Target fixed memory budget for iPod 6G:

| Allocation | Maximum |
| --- | ---: |
| parsed row index and bounded string arena | 48 KiB |
| one detail/full poster slot | 38 KiB |
| four list-poster slots | 12 KiB |
| three episode-still slots, unioned with list posters | 14 KiB |
| wordmark and small UI assets | 12 KiB |
| state, search result ids, and scratch lines | 10 KiB |
| total new fixed workspace | 120 KiB maximum |

The list-poster and episode-still caches are a union because those screens
cannot be visible simultaneously. The exact compiled BSS delta must be
reported; 120 KiB is a ceiling, not a target.

Prohibited:

- `core_alloc(FRAMEBUFFER_SIZE)`;
- two-frame full-screen transition caches;
- decoding every visible row in one pass;
- storage access from a row draw callback;
- animation-driven image loads;
- a catalog action that stops, resumes, or shrinks music playback;
- new ordinary `font_load()` calls; and
- using the plugin audio buffer as catalog artwork memory.

At idle, service at most one missing bitmap after 8 ticks with an empty input
queue and a still-current row generation. On navigation, invalidate pending
work using a monotonically increasing screen/list generation. A failed decode
marks that asset unavailable for the current manifest generation.

Use the existing bounded iPodJS transition helper only when it can present a
cached destination. If its workspace is unavailable, draw the destination
directly. Animation position derives from elapsed ticks and input can end the
effect immediately.

## Playback Lifecycle

All rules in `docs/plugin-audio-lifecycle-steering.md` apply to player work.

Netflix catalog code does not call audio, PCM, mixer, playlist, or shared audio
buffer APIs. It launches the same viewer chosen for the final extension:

- `.mpg`, `.mpeg`, `.mpv`, `.m2v` -> `mpegplayer.rock`;
- `.rvp`, and playable raw H.264 where supported ->
  `openh264_player.rock`.

Any Netflix-specific player mode must continue to:

- acquire the shared buffer through `plugin_get_audio_buffer()` without a
  preceding `audio_stop()`;
- use `PCM_MIXER_CHAN_PLAYBACK` for long-form sound;
- stop callbacks before releasing plugin memory;
- restore mixer/sample-rate state on exit;
- leave Database and Files music usable without reboot; and
- preserve Menu, Play/Pause, volume, seek, Hold, USB, and shutdown behavior.

The catalog records launch intent and selection before invoking the viewer. It
must not retain pointers into relocatable buffers across `plugin_load()`.

## Error And Empty States

Errors use the standard iPod white panel and real text, never novelty art.

- Missing bundle: `No Netflix Library` / `Sync movies with RockPod`.
- Empty category: `No Movies on this iPod`, `No TV Shows on this iPod`, or
  `Your Queue is Empty`.
- Missing artwork: neutral poster frame plus the real title in text.
- Corrupt manifest: `Netflix Library Unavailable` plus `Resync with RockPod`.
- Missing media: `Movie Not Found` and preserve browser state.
- Unsupported media: `Can't Play This Format` and show the synced format.
- Player failure: `Playback Failed`; Menu/Select returns to detail.

The parser skips a malformed row, counts it, and keeps valid rows. It rejects
the entire manifest only for an unknown major version, missing header, unsafe
path, or resource limit violation.

## Build And Integration Plan

### Slice 1: deterministic bundle

- add the RockPod exporter and manifest validation;
- export real posters for a movie, show, season, and episode fixture;
- add atomic sync and obsolete-file cleanup scoped to the Netflix directory;
- add source/derivative hash tests; and
- do not change Rockbox UI yet.

Exit gate: a mounted-device or simulator bundle contains correct final media
paths, complete metadata, and only real or neutral-fallback art.

### Slice 2: catalog shell

- add `ipodjs_netflix.c/.h` and the iPodJS home item;
- implement launch, Home, Movies, TV Shows, seasons, episodes, My Queue, and
  Search;
- add the bounded parser and generation-based idle artwork cache; and
- reuse iPodJS status, Hold, font, search, and transition helpers.

Exit gate: all hierarchy and input tests pass during active music playback
without core memory or file-descriptor drift.

### Slice 3: detail and playback handoff

- implement movie, show, and episode details;
- launch through existing extension dispatch;
- restore exact browser/detail state after viewer exit; and
- add unsupported/missing/player-error states.

Exit gate: MPEG and RVP titles play and return, and both Database and Files
music still start with sound afterward.

### Slice 4: progress and branded player mode

- define a shared progress/report contract for both players;
- add Watch Instantly progress and Resume only where truthful;
- add the optional period Netflix OSD skin without forking decoder/audio
  lifecycle; and
- validate long video, pause/resume, seek, and segment transitions.

This slice must not block the first complete catalog release.

## Acceptance Matrix

### Data and art

- Movies and TV episodes are classified correctly from RockPod metadata.
- A show with Seasons 0, 1, and 2 groups correctly and sorts numerically.
- Every title detail uses its real poster or the neutral text fallback.
- Episode stills belong to the displayed episode.
- No generated, hand-drawn, or fake cover is present in fixtures or defaults.
- Long UTF-8 titles and summaries truncate or scroll without corrupting rows.
- Sync is idempotent and unchanged art is not rewritten.
- Removing one title removes only now-unreferenced Netflix-owned derivatives.

### UI and navigation

- All screens fit 320x240 with no clipped footer or status header.
- Ten full Home -> TV Show -> Season -> Episode -> Detail -> Home cycles pass.
- Twenty rapid Movies/TV Shows/My Queue switches pass.
- Menu unwinds one level and never jumps unexpectedly to Rockbox Home.
- Search finds movie title, show title, cast, director, and genre matches.
- Hold, USB, shutdown, and Play/Pause shortcuts remain observable.
- No Rockbox theme, old Netflix row, or stale art flashes during handoff.

### Resource stability

- Simulator and native iPod 6G builds succeed.
- Cached draw paths contain no file or decode calls.
- File-descriptor count returns to baseline after every hierarchy cycle.
- Core available/allocatable memory does not trend downward.
- Active music playlist identity and elapsed time do not regress while
  browsing Netflix.
- `rockbox.elf` BSS growth is reported and remains at or below the approved
  fixed budget.
- No UI allocation invokes playback's audio-buffer shrink callback.

### Playback

- fresh boot -> MPEG Netflix title has video and sound;
- fresh boot -> RVP Netflix title has video and sound;
- Database music -> Netflix video -> Database music works;
- Files music -> Netflix video -> Files music works;
- Play pauses/resumes, wheel changes volume, seek works, and Menu exits;
- rapid catalog/player/music switching does not freeze;
- long and segmented videos keep audio/video sync; and
- player exit restores the exact Netflix detail and selection.

Run the focused Netflix simulator gate plus
`tools/ipodjs_navigation_sim_regression.sh`. Hardware remains authoritative for
storage timing, codec wake, and memory pressure.

## Definition Of Done

The feature is done when a user can sync a mixed local movie/TV library in
RockPod, open Netflix from iPodJS Home, browse every item with real cover art
and useful metadata, play MPEG and RVP content, return to the same place, and
immediately resume ordinary music playback without a reboot or database error.

The finished app must look plausibly native to a 2007 iPod while unmistakably
using the real disc-era Netflix identity. Visual resemblance alone is not
enough: asset authenticity, metadata correctness, bounded memory, cached-only
drawing, and clean media lifecycle are release requirements.
