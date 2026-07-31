# Magazines for Rockbox: iPod bookshelf and page-turn reader

- Status: implementation specification
- Primary targets: iPod Classic 6G/7G (`ipod6g`) and iPod Video
  5G/5.5G (`ipodvideo`)
- Display: 320x240, 16-bit colour
- Entry point: `Extras` -> `Magazines`
- Implementation: native Rockbox C plugin
- Initial test issue: [WWF Divas 2002 on Internet
  Archive](https://archive.org/details/wwf-divas-2002-c/)

The implemented progress scrubber, persistent bookmarks, preparation profiles,
margin cropping, and 720x960 Fine Text contract are specified in
[`magazines-reader-spec.md`](magazines-reader-spec.md). Where the original
480x640-only limits below conflict with that extension, the extension is
authoritative.

## 1. Product outcome

Magazines is an offline magazine library and reader made specifically for the
iPod click wheel. Its library should recall the original wooden iBooks
bookshelf without copying Apple artwork: covers sit on two warm wooden shelves,
the selected cover lifts forward, and selecting it opens the issue with a short
cover-to-page transition.

In the reader, clockwise and counter-clockwise wheel motion turns pages with a
complete animated paper flip. The effect is part of the required experience,
not an optional mock-up. The reader also needs a useful zoom mode because a
portrait magazine page is not legible when fitted to a 320x240 display.

The first release is deliberately offline. A host preparation tool converts a
PDF or an Internet Archive item into bounded, iPod-ready page images. The iPod
does not parse PDF, EPUB, JP2, or ZIP files and does not download from the
network.

## 2. Non-negotiable requirements

1. `Magazines` appears as a normal item inside the existing `Extras` menu.
2. The library is a 3-column by 2-row cover shelf with an iPodJS-compatible
   header, focus treatment, and click-wheel navigation.
3. Selecting a cover opens the last-read spread, or the front cover for an
   unread issue. The front and back covers are single pages; interior leaves
   are composed as open two-page spreads (`2-3`, `4-5`, and so on).
4. In fitted-page mode, clockwise wheel motion turns forward and
   counter-clockwise motion turns backward.
5. Every normal page change uses the paper-turn animation. A direct cut is
   permitted only for a documented fail-closed condition such as a corrupt
   destination page, USB connection, shutdown, or insufficient bounded
   workspace.
6. Select toggles between fitted-page and readable zoom mode.
7. The current page and reading position persist per issue.
8. Existing music playback, playlist identity, elapsed time, PCM state, codec
   state, and the tagcache remain untouched.
9. The plugin never calls `plugin_get_audio_buffer()`, `audio_stop()`,
   `plugin_release_audio_buffer()`, a PCM API, or a mixer API.
10. Draw and animation functions perform no file I/O, directory scan, image
    decode, or allocation.
11. Menu returns from the reader to the shelf, then to categories, then to
    `Extras`.
12. USB, Hold, shutdown, and system events remain observable throughout loads
    and animations.

## 3. Test-source facts and implications

The Internet Archive metadata endpoint for
`wwf-divas-2002-c` currently describes:

- title: `WWF Divas 2002`;
- creator: `World Wrestling Entertainment`;
- year: `2002`;
- 130 scanned leaves at 300 PPI;
- an original 43,543,849-byte text PDF with MD5
  `b34036e0fc44a4e295a90d1303498f03`;
- a 57,441,045-byte JP2 page bundle;
- an 87,259,808-byte EPUB;
- mostly portrait source pages around 2,450x3,205 pixels, plus a few landscape
  leaves near the end.

The source record is available from the [Internet Archive metadata
API](https://archive.org/metadata/wwf-divas-2002-c). These facts make direct
on-device PDF/EPUB/JP2 support a poor first target. The acceptance fixture is a
prepared derivative containing baseline JPEG pages no larger than 480x640.

The source magazine and prepared pages are local test data. They must not be
committed to this repository or shipped in a Rockbox package. The preparation
tool records source attribution but does not imply that Internet Archive
availability grants redistribution rights.

### 3.1 RockPod Magazine Sync

RockPod exposes `Magazine Sync` in its Rockbox sidebar. `Upload PDF…` imports
one or more user-selected PDFs into RockPod's private magazine library.
Byte-identical PDFs already in that library are skipped, regardless of their
source filename, so selecting the same upload again does not create a numbered
duplicate.
`Prepare Selected` runs the host converter in the background and reports its
progress without blocking the desktop UI. `Sync Selected` atomically installs
only the prepared cover, manifest, and JPEG pages into `/Magazines` on the
selected iPod (or its bound simulator) and rebuilds `catalog.mgi`.

The panel also creates, assigns, renames, and deletes magazine categories and
bulk locks selected magazines. Locking uses the same four-digit device PIN as
Netflix/Locked Videos. These fields survive re-preparation and are written into
the issue manifest during sync.

The original PDF remains on the computer. Removing an issue from the target
does not delete that local PDF or its prepared cache, so it can be synced
again. RockPod validates the PDF signature, issue identifier, prepared first
and final pages, target mount, and catalog before presenting an issue as ready.

## 4. User experience

### 4.1 Shelf

The shelf is full-screen and does not show the normal Rockbox status bar.
It uses a 24-pixel iPodJS-style header and two shelves:

```text
+------------------------------------------------------+
| Magazines                        playing       battery|
+------------------------------------------------------+
|      [cover]          [cover]          [cover]        |
|        title            title            title         |
|======================================================|
|      [cover]          [cover]          [cover]        |
|        title            title            title         |
|======================================================|
+------------------------------------------------------+
```

The illustration is structural, not pixel-exact. Required 320x240 geometry:

| Element | Geometry |
| --- | --- |
| Header | `x=0, y=0, w=320, h=24` |
| Upper shelf cell band | `y=24..130` |
| Lower shelf cell band | `y=131..239` |
| Cover target | at most `58x78` pixels |
| Cover column centres | `x=56, 160, 264` |
| Shelf lip | 10-12 pixels at the bottom of each band |
| Selected lift | 3 pixels upward, 1-pixel light keyline, soft dark shadow |

The shelf background and lips are drawn procedurally with fixed colour ramps.
No Apple texture or icon asset is copied. The active iPodJS accent colour may
be reused for the selection keyline, while the wood remains a warm,
low-saturation brown so colourful covers dominate.

Only cached covers are painted. Missing covers use a generated paper rectangle
with the first two lines of the title. Cover loading occurs one file at a time
from the idle service point.

Shelf order defaults to most recently opened, then title. Unread issues with no
timestamp follow read issues in title order. Page state is shown by a thin
progress rule below the selected cover; `100%` issues receive a small
procedurally drawn check badge.

### 4.2 Opening an issue

Select initiates this sequence:

1. Stop cover prefetch and close any cover file.
2. Read the selected manifest and saved position.
3. Decode the current fitted page and its immediate previous/next neighbours.
4. Draw the current page completely.
5. If the cached cover and bounded compositor are available, animate the cover
   expanding into the page for 160-220 ms.
6. Enter fitted-page mode and schedule the far neighbour for idle prefetch.

Steps 2 and 3 occur on a dedicated loading screen, never inside a shelf draw or
transition callback. Menu during the load cancels and returns to the shelf.

### 4.3 Fitted-page reader

The front cover is centred alone on a charcoal background. Interior leaves are
shown as a two-page open magazine with a four-pixel gutter; the final source
page is centred alone as the back cover. An odd unpaired interior leaf remains
single rather than being paired with the back cover. Landscape pages retain
their fitted aspect ratio and are not silently rotated.

Reader chrome is hidden by default. Pressing Play shows, for 1.5 seconds:

- issue title at the top;
- `Page N of M`;
- a bottom progress rule;
- a small `Select: Zoom` hint on the first two uses only.

The chrome is composited from cached state and does not cause a page reload.

### 4.4 Readable zoom

On an interior spread, Left decodes the left leaf and Right decodes the right
leaf at up to 400x480 into the fixed zoom slot. Select defaults to the left
leaf. A front or back cover always decodes as one centred page. Only one source
page occupies the zoom slot, keeping text clearer than zooming both leaves at
once. A small translucent position map appears briefly after every pan.

Zoom-mode controls are:

| Input | Result |
| --- | --- |
| Wheel clockwise/counter-clockwise | Pan down/up within the chosen page |
| Right/Left | Pan right/left within the chosen page |
| Select | Return to fitted-page mode |
| Play | Toggle reader chrome and position map |
| Menu | Return to fitted mode; a second Menu returns to shelf |

Pan movement is 24 pixels for a wheel action and 64 pixels for a repeat action,
clamped to the page. Page turns do not occur while zoomed, preventing an
accidental turn while reading a column. The fitted-page cache remains valid
while zoomed.

MVP has one readable zoom size. Additional zoom levels, OCR reflow, search,
annotations, and text selection are post-MVP.

### 4.5 Resume and completion

The plugin saves position after the page has been stable for two seconds, when
returning to the shelf, and on normal plugin exit. It must not write on every
wheel event.

Opening an issue resumes its saved leaf. Finishing the last page marks the issue
complete but leaves it on the last page. Turning forward from the last page
performs a short resistant edge curl and settles back without wrapping. The
first page behaves symmetrically in reverse.

## 5. Click-wheel control map

### Shelf

| Input | Action |
| --- | --- |
| Wheel clockwise | Next issue |
| Wheel counter-clockwise | Previous issue |
| Select | Open selected issue |
| Right | Move one shelf cell forward |
| Left | Move one shelf cell backward |
| Play | Toggle `Recent` / `Title` sort |
| Menu | Return to `Extras` |

Selection wraps between shelf rows but not from the final issue to the first.
Moving beyond a six-item page scrolls the shelf by one row.

### Fitted reader

| Input | Action |
| --- | --- |
| Wheel clockwise | Animated next spread |
| Wheel counter-clockwise | Animated previous spread |
| Right | Zoom the right page (or the single cover) |
| Left | Zoom the left page (or the single cover) |
| Select | Zoom the left page, or the single cover |
| Play | Toggle temporary reader chrome |
| Menu | Return to shelf |

One discrete action starts one turn. Repeats may queue at most one additional
turn in the same direction. Opposite input while a turn is active settles to
the closest logical page and then starts the requested direction. The plugin
never accumulates an unbounded wheel queue.

## 6. Page-turn effect

### 6.1 Visual model

The turn is a single portrait sheet curling around a moving vertical fold:

- forward: the right edge lifts and travels left;
- backward: the effect is mirrored;
- the destination page is visible underneath;
- the turning page narrows in perspective;
- the back of the sheet uses a warm, desaturated reflection of the source;
- a soft shadow follows the fold and peaks near the middle of the turn;
- the first and final frame exactly match the stable source and destination
  pages.

This is a fixed-point strip compositor, not a general 3D engine. Divide the
source page into 24 vertical strips. For each delivered frame:

1. derive normalized progress from elapsed ticks;
2. apply a cubic ease-in/ease-out lookup table;
3. paint the cached destination page into the composition buffer;
4. map source strips around the moving fold using a fixed-point cosine lookup;
5. mirror applicable strips after the fold passes 90 degrees;
6. apply an 8-level shade lookup to the turning sheet;
7. paint a 1- to 10-pixel alpha-approximated fold shadow;
8. blit the completed composition buffer to the LCD.

The implementation may approximate alpha through RGB565 colour blending.
There is no per-frame allocation and no floating-point work.

### 6.2 Timing

- nominal duration on iPod 6G: 200 ms;
- nominal duration on iPod Video 5G: 240 ms;
- target delivered frames: 8-10;
- frame position is always based on elapsed ticks;
- delayed frames are skipped rather than extending the turn;
- the loop sleeps or yields until the next deadline and never busy-waits.

Every frame polls input and system events. USB or shutdown cancels visual work,
settles to a complete logical page, and follows the ordinary system path.
Hold settles the current turn and suppresses further navigation until released.

If the destination fitted page is not cached, the reader shows the stable
current page and loads it before beginning the effect. It must never begin a
turn with a missing destination and then perform I/O from the compositor.

### 6.3 Fail-closed behaviour

If the composition slot is unavailable or a render invariant fails:

- show the fully decoded destination page directly;
- update the logical page exactly once;
- record a diagnostic counter in simulator/debug builds;
- continue reading without taking other memory.

A failed image decode keeps the reader on the current page and shows
`Page image unavailable` in cached chrome. It does not substitute a half-drawn
frame.

## 7. On-device content layout

The default library root is:

```text
/Magazines/
    catalog.mgi
    wwf-divas-2002/
        issue.mgi
        cover.jpg
        pages/
            0001.jpg
            0002.jpg
            ...
            0130.jpg
```

`catalog.mgi` is generated by the host preparation tool and contains one issue
directory per line. Blank lines and lines beginning with `#` are ignored. Paths
are relative to `/Magazines`, use `/`, and must not contain `..`.

If `catalog.mgi` is missing, the plugin may rebuild it by enumerating one
directory level on an explicit `Building Library` screen. It writes a temporary
file and renames it only after the complete scan. The shelf render path never
falls back to directory enumeration.

An issue manifest is UTF-8, line-oriented `key=value`:

```ini
schema=1
id=wwf-divas-2002
title=WWF Divas 2002
creator=World Wrestling Entertainment
year=2002
page_count=130
cover=cover.jpg
page_pattern=pages/%04d.jpg
source_kind=internet_archive
source_id=wwf-divas-2002-c
source_url=https://archive.org/details/wwf-divas-2002-c/
source_md5=b34036e0fc44a4e295a90d1303498f03
prepared_width=480
prepared_height=640
```

Parser limits:

- 128 issues in MVP;
- 4 KiB maximum manifest size;
- 95 displayed bytes for title and 95 for creator, truncated on UTF-8
  boundaries;
- 9,999 pages maximum;
- paths must remain within the issue directory;
- `schema`, `id`, `title`, `page_count`, and `page_pattern` are required;
- unknown keys are ignored for forward compatibility.

Baseline JPEG is the only MVP page format. Progressive JPEG, CMYK JPEG, PDF,
EPUB, JP2, PNG, animated image formats, and arbitrary image directories are
rejected with a useful preparation hint.

## 8. Host preparation tool

Implement `tools/magazine_prepare.py` as the supported ingestion path. It
accepts either a local PDF or an Internet Archive identifier:

```text
tools/magazine_prepare.py \
    --archive-id wwf-divas-2002-c \
    --output /path/to/IPOD/Magazines/wwf-divas-2002
```

```text
tools/magazine_prepare.py \
    --pdf ~/Downloads/issue.pdf \
    --title "Issue title" \
    --output /path/to/IPOD/Magazines/issue-title
```

For an Archive identifier the tool:

1. fetches `https://archive.org/metadata/<identifier>`;
2. selects an original `Text PDF` or ordinary PDF, never an EPUB derivative;
3. displays title, creator, page count, source size, and destination before
   downloading;
4. streams the PDF to a temporary directory;
5. verifies the source MD5 when supplied by the metadata;
6. renders each leaf, respecting its PDF rotation and crop box;
7. scales within 480x640 without upscaling, using a high-quality host filter;
8. flattens transparency onto warm white;
9. writes baseline RGB JPEG, quality 86, 4:2:0, with metadata stripped;
10. makes `cover.jpg` from the first access leaf;
11. writes `issue.mgi` and updates the library `catalog.mgi` atomically;
12. validates every output image and reports total size.

The implementation may use `pdftoppm`, MuPDF, or ImageMagick as an external
renderer, but it must detect the chosen dependency before download and give an
actionable error if missing. The Python tool itself must not require a large
third-party Python package merely to parse command-line arguments or metadata.

Temporary output stays outside the destination issue directory. An interrupted
run cannot leave a half-valid issue on the shelf. `--force` is required to
replace an existing issue, and replacement first creates a complete sibling
directory before a recoverable rename.

The test fixture must additionally verify:

- identifier and source MD5 match the values in section 3;
- `page_count=130`;
- portrait and landscape leaves all fit within 480x640;
- generated filenames are `0001.jpg` through `0130.jpg`;
- the first and final images decode through Rockbox's JPEG loader;
- prepared content is excluded by `.gitignore` and remains untracked.

## 9. Plugin architecture

Create a focused plugin rather than extending `imageviewer` or putting the
reader into `apps/root_menu.c`.

Proposed paths:

```text
apps/plugins/magazines/
    magazines.c
    magazine_library.c
    magazine_library.h
    magazine_reader.c
    magazine_reader.h
    magazine_render.c
    magazine_render.h
    magazine_state.c
    magazine_state.h
    SOURCES
    magazines.make
tools/
    magazine_prepare.py
    test_magazine_prepare.py
```

If the build system makes a single `apps/plugins/magazines.c` loader plus an
overlay more natural, that split is acceptable. The behavioural and memory
boundaries remain the same.

Responsibilities:

| Module | Responsibility |
| --- | --- |
| `magazines.c` | plugin entry, theme/backlight lifecycle, top-level state machine |
| `magazine_library` | catalog/manifest parsing, issue sort, cover service |
| `magazine_reader` | page state, input, cache rotation, resume |
| `magazine_render` | cached shelf draw, page draw, fixed-point turn compositor |
| `magazine_state` | bounded settings and atomic per-issue progress |

The state machine is explicit:

```text
SHELF -> ISSUE_LOADING -> READER_FIT -> ZOOM_LOADING -> READER_ZOOM
  ^          |               |                              |
  +----------+---------------+------------------------------+
```

`TURNING_FORWARD` and `TURNING_BACKWARD` are substates of `READER_FIT`.
File/decode service functions are legal only in `ISSUE_LOADING`,
`ZOOM_LOADING`, or the stable-state idle service point. Render functions accept
already decoded bitmaps and scalar state only.

## 10. Memory and resource budget

Both primary targets define a 3 MiB plugin buffer. The plugin calls
`plugin_get_buffer()` once and divides the returned region itself. It never
falls back to the audio buffer.

Maximum runtime data budget:

| Region | Maximum bytes | Notes |
| --- | ---: | --- |
| Three fitted page bitmaps | 447,456 | `3 * 316 * 236 * 2`; includes landscape |
| Turn frame / fit-decode staging union | 280,224 | max of screen frame or fit pixels plus decode tail |
| Zoom bitmap plus decode tail | 745,472 | `480 * 640 * 2 + 128 KiB` |
| Six shelf cover bitmaps | 65,536 | active only on shelf; may union with zoom |
| Catalog and manifest records | 65,536 | 128 bounded records and strings |
| State, strip tables, paths, stack margin | 98,304 | no large automatic arrays |
| Total bounded data ceiling | 1,702,528 | before plugin code/static image |

The shelf cover cache and zoom slot have non-overlapping lifetimes and should
share a union if that keeps the allocator simpler. At least 1 MiB of the 3 MiB
plugin region must remain unused after code, static data, and all active slots
are accounted for. If the measured plugin binary makes that impossible, reduce
cover caching before reducing playback safety or taking the audio buffer.

Additional resource rules:

- maximum one content file descriptor open at a time;
- every opened file and directory closes in the same service operation;
- no stack object larger than 4 KiB;
- no `core_alloc()` or `buflib_alloc()` use;
- no font load; reuse the active UI font or built-in system font;
- no storage operation while a page turn is active;
- no tagcache query anywhere in the plugin.

Native `rockbox.elf` and `magazines.rock` size reports must record text, data,
BSS, maximum plugin-buffer use, and stack-use output before hardware testing.

## 11. Decode and cache lifecycle

All source pages are at most 480x640 baseline JPEG. Rockbox's existing
`read_jpeg_file()` API decodes them into the fixed slots:

- fitted decode requests `FORMAT_NATIVE | FORMAT_RESIZE |
  FORMAT_KEEP_ASPECT` with the fitted page bounds, writes into the shared
  staging region, and copies the completed pixels into the selected cache slot;
- zoom decode requests native format at no more than the prepared dimensions
  and uses the contiguous decode tail following the zoom bitmap;
- decode failure clears the slot before returning;
- the render loop sees only a `ready` slot with page number and generation.

The fitted cache contains `previous`, `current`, and `next`. After a forward
turn:

1. rotate `current` into `previous`;
2. rotate `next` into `current`;
3. mark the old `previous` slot as the new missing `next`;
4. draw the stable current page;
5. when the input queue is empty and the page has settled for 150 ms, decode
   the new next page into that slot.

Reverse turns mirror the operation. Every slot has an issue generation and
page number. Leaving an issue increments the generation so an abandoned load
can never become a visible page.

Loading functions poll for system events between bounded read/decode stages.
If the core JPEG API cannot be interrupted within its decode call, prepared
dimensions remain capped at 480x640 and the measured worst decode time becomes
an acceptance gate.

## 12. Persistence

Store app state in:

```text
/.rockbox/rocks/apps/magazines.cfg
```

The bounded line-oriented file contains:

```ini
schema=1
sort=recent
last_issue=wwf-divas-2002
issue.wwf-divas-2002.page=37
issue.wwf-divas-2002.completed=0
issue.wwf-divas-2002.opened=1785360000
```

Only IDs present in the current catalog are retained when the file is compacted.
Keep at most 128 issue records. Write to `magazines.cfg.tmp`, flush/close, and
rename over the prior config. If a write fails, continue the session and leave
the last valid file intact.

Zoom pan position is session-only in MVP. Page number, completion, selected
issue, and sort order persist.

## 13. Rockbox integration

### Build

- add the magazine plugin sources under
  `HAVE_LCD_COLOR && LCD_WIDTH >= 320 && LCD_HEIGHT >= 240`;
- add `magazines,apps` to `apps/plugins/CATEGORIES`;
- ensure packaging installs `magazines.rock` at
  `/.rockbox/rocks/apps/magazines.rock`;
- do not change the plugin API unless an existing required function is
  genuinely absent.

### Extras menu

In `apps/root_menu.c`:

1. add `launch_magazines_plugin()` using
   `PLUGIN_APPS_DIR "/magazines.rock"`;
2. add a `MENUITEM_FUNCTION` labelled `Magazines`;
3. insert it in `applications_menu` after `Offline Web` and before
   `Pocket Sky`;
4. use `Icon_Folder` until an original magazine icon is added to the normal
   icon system;
5. on a missing plugin, show
   `Magazines plugin missing` and return to `Extras`.

This does not add Magazines to the iPodJS home screen or the configurable root
menu. Its required location is inside `Extras`.

### Plugin lifecycle

On entry:

- save the current viewport/theme/backdrop and backlight policy;
- request only `plugin_get_buffer()`;
- turn off any backdrop used by the previous screen;
- leave audio and playlists untouched.

On exit:

- close the active content descriptor;
- commit pending reading state if possible;
- restore backlight policy, viewport, theme, backdrop, colours, and font;
- return the correct status for USB or normal exit.

The reader is silent. Adding page sounds is explicitly post-MVP and would
require a separate review under `docs/plugin-audio-lifecycle-steering.md`.

## 14. Error and empty states

Required messages:

| Condition | User-visible result |
| --- | --- |
| `/Magazines` missing | Empty shelf, `Add magazines with magazine_prepare.py` |
| Empty catalog | Empty shelf, same preparation hint |
| Catalog invalid | `Library index is damaged` with `Select: Rebuild` |
| Issue manifest invalid | Disabled cover placeholder, `Issue unavailable` |
| Page missing/corrupt | Stay on current page, `Page image unavailable` |
| Plugin buffer below requirement | `Not enough plugin memory`; return to Extras |
| USB connected | Settle state, save if safe, return `PLUGIN_USB_CONNECTED` |
| Source path too long | Skip issue and record one diagnostic |

One bad issue must not prevent other issues from loading. Rebuild never deletes
an issue directory.

## 15. Performance targets

Measured on both simulator and physical hardware:

- shelf first paint from a valid catalog: at most 250 ms, placeholders allowed;
- selected cover visible: at most 500 ms after first paint;
- fitted current page visible after selection: at most 750 ms on 6G and
  1,200 ms on 5G;
- cached page turn input-to-first-frame: at most 50 ms;
- turn duration: within 20% of target;
- wheel input after turn settlement: processed within 50 ms;
- no file descriptor growth after 100 issue open/close cycles;
- no plugin-buffer high-water growth after 500 page turns;
- no playback stop, codec reload, elapsed-time reset, or playlist change;
- no partial, stale, or wrong-issue frame ever presented.

Visual smoothness is judged on hardware. The simulator verifies monotonic
progress and complete endpoint frames but is not authoritative for LCD cadence.

## 16. Verification plan

### Host tool tests

`tools/test_magazine_prepare.py` must cover:

- metadata selection chooses the original PDF;
- MD5 success and mismatch;
- 130-page test metadata;
- portrait and landscape resizing;
- zero-padded file order;
- baseline/RGB output enforcement;
- interrupted preparation leaves no visible issue;
- path traversal and unsafe identifier rejection;
- atomic catalog update;
- `--force` replacement safety.

Tests use synthetic PDFs/images and a saved minimal metadata response. They do
not download or commit the magazine.

### Simulator gate

Add `tools/magazines_sim_regression.sh` or an equivalent focused gate that:

1. installs a generated synthetic library with at least eight issues and
   twelve pages per issue;
2. verifies shelf pagination and both sort orders;
3. opens/resumes every issue;
4. performs 500 alternating wheel page turns;
5. enters, pans, and exits zoom on portrait and landscape pages;
6. injects rapid Menu, wheel, Select, Hold, and USB actions;
7. verifies turn traces are monotonic and begin/end on exact stable frames;
8. verifies render traces contain no open, read, directory, decode, or
   allocation operation;
9. compares file descriptor and plugin-buffer high-water baselines;
10. runs with active Database playback and active Files playback.

Also run `tools/ipodjs_navigation_sim_regression.sh` because the menu entry and
return path participate in the iPodJS navigation experience.

### Build gate

- simulator build succeeds;
- native `ipod6g` build succeeds;
- native `ipodvideo` build succeeds;
- compiler warnings remain clean under the repository flags;
- stack-use and binary-size reports meet section 10;
- prepared fixture is absent from `git status`.

### Physical iPod gate

Run on both iPod 6G and iPod Video 5G:

1. Fresh boot -> Extras -> Magazines -> test issue.
2. Turn from page 1 through page 20 in both directions.
3. Jump by repeated wheel input to the final page and test both hard edges.
4. Zoom and pan dense text, a full-page photo, and a landscape leaf.
5. Exit to shelf, Extras, Music, and Files; reopen and verify resume.
6. Start a Database track, repeat 100 turns, and confirm uninterrupted
   playback, unchanged playlist, and increasing elapsed time.
7. Repeat from a Files-started track.
8. Rapidly alternate Magazines, Music, and Extras twenty times.
9. Engage Hold during load and during a turn.
10. Connect USB during shelf, decode, turn, and zoom.
11. Confirm Database and Files playback start normally after plugin exit.
12. Reboot and verify saved page, completion, sort, and selected issue.

Hardware logs must include page-cache generations, decode times, turn start/end
ticks, dropped frames, file descriptor baseline, plugin-buffer high-water, and
audio status at plugin entry/exit. Debug logging must not be enabled in release
builds.

## 17. Implementation phases

### Phase 1: ingestion and static reader

- host preparation tool and tests;
- manifest/catalog parser;
- Extras integration;
- shelf with placeholders and cached covers;
- static fitted page display;
- persistence;
- playback-safe plugin lifecycle.

Exit gate: the test issue prepares locally, appears on the shelf, opens all 130
pages, and resumes without affecting playback.

### Phase 2: full page turn

- three-slot fitted cache;
- fixed-point strip compositor;
- forward/reverse wheel integration;
- edge resistance;
- timing and trace instrumentation;
- rapid-input and system-event handling.

Exit gate: every ordinary fitted page change has the required complete effect
and the simulator/hardware resource baselines remain flat.

### Phase 3: readable zoom and polish

- bounded 400x480 zoom decode;
- wheel/left/right pan;
- position map and reader chrome;
- cover-to-page launch;
- recent/title sort and progress markers;
- final 5G cadence tuning.

Exit gate: the complete product and physical-device matrices pass.

## 18. Explicitly out of MVP

- on-device network download or Internet Archive browsing;
- direct PDF, EPUB, CBZ, CBR, ZIP, or JP2 parsing;
- OCR, text reflow, search, dictionary, annotation, and clipping;
- pinch-like continuous zoom or arbitrary rotation;
- page-turn sound;
- DRM or storefront support;
- syncing reading position to another device;
- background preparation on the iPod;
- use of playback memory to make a larger or smoother effect.

## 19. Definition of done

Magazines is done when a locally prepared copy of the 130-leaf test issue:

- appears in an original iPod-sized wooden bookshelf under `Extras`;
- opens and resumes correctly;
- turns every normal page through a complete wheel-driven paper effect;
- provides legible zoom and pan;
- survives rapid input, Hold, USB, corrupt pages, and repeated navigation;
- stays within the declared fixed plugin-buffer and stack budgets;
- leaves music, playlists, tagcache, file descriptors, themes, and backlight
  behaviour in the same valid state in which it found them;
- passes simulator, both native builds, and the physical 6G/5G gates without
  committing or redistributing the test magazine.
