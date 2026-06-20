# WPS Album Art First-Load Performance Spec

## Problem

When a song is first selected and the WPS opens, album art can appear late or
make the first playback screen feel stalled. The delay is most visible when
Rockbox must search several possible cover paths, decode embedded artwork, or
resize/decode a large JPEG before the WPS can draw the final cover.

This is a first-hit latency problem. Once the same folder art is already known
or buffered, later draws are usually faster.

## Preferred Outcome

Selecting a track should show the WPS immediately, with the correct album art
appearing without a visible stall. RockPod and Rockbox should cooperate so the
common path is cheap:

- RockPod syncs Rockbox-ready, exact-size cover assets for the active WPS.
- Rockbox finds the exact-size file early in its search order.
- Rockbox buffers the current and next likely album art before the user notices.
- If art is not ready, WPS draws a stable placeholder and swaps in art when
  decoding completes.

The user should not need to embed artwork in every audio file or manually resize
covers for Rockbox.

## Current Evidence

### Rockbox

- WPS album-art dimensions are claimed by the skin parser in
  `apps/gui/skin_engine/skin_parser.c`.
  - `%Cl(...)` becomes a `struct skin_albumart`.
  - `playback_claim_aa_slot()` registers the requested width/height.
- Playback loads album art in `apps/playback.c`.
  - `audio_finish_load_track()` calls `audio_load_albumart()`.
  - `audio_load_albumart()` searches external image files with
    `find_albumart()`, then may fall back to embedded JPEG/PNG artwork.
  - `load_album_art_from_path()` uses `bufopen(..., TYPE_BITMAP, ...)`.
  - There is a small `last_folder_aa_path` cache, but it only helps after a
    path has already been found and loaded.
- Album-art lookup is in `apps/recorder/albumart.c`.
  - `find_albumart()` first tries a size-specific file such as
    `cover.100x100.bmp`, then generic files such as `cover.jpg`.
  - With JPEG support, it probes `.jpeg`, `.jpg`, and `.bmp`.
  - If metadata from the audio file is incomplete, it may call
    `tagcache_fill_tags()` and search again with tagcache metadata.
- WPS drawing is in `apps/gui/skin_engine/skin_display.c`.
  - `draw_album_art()` draws only after `playback_current_aa_hid()` returns a
    valid buffered bitmap handle.

### RockPod

- RockPod already manages artwork in `rockpod/services/artwork_manager.py`.
  - `export_device_cover()` currently exports a device JPEG, normally up to
    `device_cover_art_size` pixels.
  - `export_rockbox_wps_cover()` exists in the current working tree and exports
    cached WPS-sized BMPs.
  - `export_album_list_thumbnail()` already exports BMP thumbnails for Rockbox
    album-list use.
- RockPod sync planning in `rockpod/services/sync_engine.py` already adds cover
  files to `plan.artwork_to_copy`.
  - The current device cover target is `Music/.../cover.jpg`.
  - The current working tree also plans WPS-sized cover targets named
    `Music/.../cover.<width>x<height>.bmp` when `export_wps_sized_covers` is
    enabled.
  - This helps Rockbox find external art, but still leaves Rockbox to decode a
    JPEG and resize it for the WPS slot if the exact-size BMP path is missing,
    stale, or not generated for the active skin slot.

## Current Implementation Audit

The current tree already contains part of the desired RockPod-side
implementation:

- `rockpod/services/rockbox_wps_art.py`
  - parses `%Cl(...)` tags from WPS text
  - selects sizes from RockPod profiles
  - limits generated sizes with `max_wps_cover_sizes_per_device`
- `ArtworkManager.export_rockbox_wps_cover()`
  - exports BMPs at exact WPS dimensions
  - supports `contain` and `cover` fit modes
  - caches by source hash, dimensions, and fit mode
- `SyncEngine._populate_artwork_sync_plan()`
  - reads `export_wps_sized_covers`
  - plans `cover.<width>x<height>.bmp` copies beside synced albums
  - avoids copying unchanged files by hashing existing device files

Remaining gaps found in the pre-implementation audit:

- Active skin detection needed to be device-truth-first. The implementation now
  parses connected-device `.rockbox/config.cfg` when available, reads the active
  `.wps` and `.sbs`, and falls back to managed profile sizes when the device is
  unavailable.
- SBS/miniplayer art needed to be covered. The helper now reads both WPS and SBS
  skin files before applying the per-device size cap.
- Fallback size data could drift from checked-in themes. The implementation
  parses profile skin files first and uses hard-coded fallbacks only when the
  theme files are unavailable.
- Rockbox's extension probe order checked `.jpeg` and `.jpg` before `.bmp` even
  for size-specific covers. Size-specific lookup now probes `.bmp` first while
  leaving generic cover lookup order unchanged.
- `apps/buffering.c` always reserved `JPEG_DECODE_OVERHEAD` for album-art bitmap
  loads when JPEG support was compiled in, even for external `.bmp` files. The
  external BMP path now skips that JPEG overhead reservation.
- Sync planning hashes existing cover files one by one. On large devices this
  can make planning artwork-heavy libraries slower than necessary.
- There is no explicit first-load timing instrumentation yet, so improvements
  need measurable before/after evidence.

## Implementation Status - 2026-06-20

Completed in the current working tree:

- WPS `%Cl(...)` size parsing is implemented in
  `rockpod/services/rockbox_wps_art.py`.
- RockPod config now includes `export_wps_sized_covers`,
  `wps_cover_fit_mode`, and `max_wps_cover_sizes_per_device`.
- `ArtworkManager.export_rockbox_wps_cover()` exports exact-size BMP covers,
  caches by source hash/dimensions/fit mode, and avoids rewriting unchanged
  outputs.
- `SyncEngine._populate_artwork_sync_plan()` plans
  `Music/.../cover.<width>x<height>.bmp` without resyncing unchanged audio.
- Connected-device `.rockbox/config.cfg` parsing discovers active WPS/SBS skins
  before falling back to managed profile skin files.
- Rockbox size-specific album-art lookup prefers `.bmp` before JPEG extensions.
- Rockbox buffering skips JPEG decode overhead for external `.bmp` album art.
- Tests cover parser behavior, BMP dimensions, no-rewrite behavior, and
  artwork-only sync planning, connected-device size discovery, BMP-first lookup,
  and external BMP overhead behavior.
- Validation passed:
  `pytest tests/test_online_artwork.py tests/test_rockbox_wps_art.py tests/test_album_art_first_load_source.py tests/test_rockboy_profile_instrumentation.py tests/test_rockbox_games.py -q`
  with 63 tests passed, `make -C build-sim-video-5g -j4`, and simulator gate
  `/tmp/rockbox-album-gameboy-final2-gate.txt` with 626 RockPod tests,
  53 WPS/SBS/FMS tests, and a passing simulator smoke run.

Deferred until measured evidence shows it is needed:

- Explicit first-load timing instrumentation.
- Resize scratch-space reduction when an exact-size external BMP is confirmed.
- Fully async WPS placeholder redraw.

## Risk Analysis

### What Is at Risk

- First WPS draw latency after selecting a track.
- Audio buffer pressure if large artwork competes with codec/audio buffering.
- Battery and disk use if Rockbox repeatedly probes missing art paths.
- UI polish: blank or late album art makes playback feel slower than it is.

### Why It Happens

First selection can hit several expensive steps in one path:

1. parse or retrieve track metadata
2. probe track-specific, album-specific, generic, parent, and `.rockbox/albumart`
   paths
3. probe multiple extensions
4. decode a JPEG/PNG/BMP
5. resize or crop to the WPS slot
6. allocate/buffer the bitmap before the WPS can draw it

The current RockPod `cover.jpg` export reduces embedded-art fallback, but it
does not avoid JPEG decode or guarantee the first search hit is the exact WPS
size.

### Constraints

- Rockbox targets have limited RAM; caching must be bounded.
- Playback must remain higher priority than artwork.
- Existing themes can request different album-art dimensions.
- Users may change WPS themes after sync.
- Some libraries have multiple albums in the same artist folder, so any
  generated cover name must not collide incorrectly.

## Performance Improvement Candidates

### Low Risk / Worth Doing

1. Complete RockPod WPS-sized BMP export validation.
   - The implementation mostly exists.
   - Add tests around active size discovery, generated BMP dimensions, sync
     planning, and no-rewrite behavior.
   - Risk is low because it only adds extra artwork files and keeps `cover.jpg`
     fallback.

2. Parse active device WPS/SBS files.
   - Prefer connected-device truth over RockPod profile guesses.
   - Read `.rockbox/config.cfg`, then parse active `.wps` and `.sbs` files.
   - Include fallback to profile parsing when the device is disconnected.
   - Risk is low if parsing failure simply returns existing profile/default
     sizes.

3. Fix fallback sizes and include known iPone-family SBS sizes.
   - Known iPone-style sizes in this tree include `138x138`, `128x128`, and
     `51x51` on 320x240 themes.
   - Keep the default max size count bounded, but make the selection explicit:
     main WPS art first, then visible miniplayer/lockscreen art.
   - Risk is low; excess generation can be controlled by config.

4. Avoid JPEG decode overhead allocation for external BMP album art.
   - In `apps/buffering.c`, skip `JPEG_DECODE_OVERHEAD` when `TYPE_BITMAP`
     source is an external `.bmp` and `embedded_albumart == NULL`.
   - This reduces buffer pressure for the exact-size BMP path.
   - Risk is low if JPEG/embedded paths keep existing overhead.

5. Prefer BMP extension for size-specific album-art lookup.
   - For non-empty size strings like `.138x138`, check `.bmp` before `.jpeg`
     and `.jpg`.
   - Generic no-size lookup can keep the current extension order.
   - Risk is low because all extensions remain supported; only the probe order
     changes for size-specific assets.

6. Add timing logs.
   - Log selected album-art source type, lookup time, decode time, dimensions,
     and cache hit/miss.
   - Risk is low behind debug/logf guards.

### Medium Risk / Worth Prototyping After Measurement

1. Add an artwork manifest for RockPod sync planning.
   - Store `rel_path -> output_hash` under `.rockbox/rockpod/`.
   - Use it to avoid hashing every existing cover on every plan.
   - Fall back to file hashing when the manifest is missing or stale.
   - Risk is medium because the manifest must stay consistent across manual
     device edits and partial sync failures.

2. Expand Rockbox decoded-art cache beyond `last_folder_aa_path`.
   - Use a tiny LRU keyed by art path plus dimensions.
   - Cache current, previous, and next likely album art.
   - Risk is medium because album-art handles share the playback buffer with
     audio/codecs.

3. Add negative lookup caching.
   - Cache "no art found" per directory/album/dim for a short window.
   - Avoid repeated failed probes across multiple tracks in no-art albums.
   - Risk is medium because stale negative entries can hide newly-added covers
     until invalidated.

### Higher Risk / Defer Unless Needed

1. Fully async WPS placeholder redraw.
   - Draw WPS immediately, then invalidate only the art viewport when ready.
   - This is the best UI model but touches playback/WPS timing and stale-art
     edge cases.

2. Centralized `.rockbox/albumart` deduplication as the primary path.
   - Could reduce duplicate cover files across multi-disc or split folders.
   - It is not a first-hit win with the current search order unless local
     folder covers are absent.

## Goals

- Make first WPS art display fast for the active iPod theme, especially iPone on
  320x240 targets.
- Avoid large JPEG decode on the Rockbox first-hit path when RockPod has already
  synced artwork.
- Preserve existing generic `cover.jpg` behavior for compatibility.
- Keep memory and file count bounded.
- Support current-track and next-track preloading without blocking audio.
- Add tests that measure path choice, generated asset dimensions, and WPS first
  load behavior.

## Non-Goals

- Do not remove support for embedded album art.
- Do not require a single hard-coded WPS album-art size for all themes.
- Do not make artwork loading synchronous with WPS drawing.
- Do not rewrite audio files just to change embedded artwork.
- Do not make device sync recopy all music solely to update artwork.

## Proposed RockPod Design

### 1. Detect Active Rockbox WPS Album-Art Sizes

RockPod should determine the album-art dimensions the active WPS needs:

- parse `.rockbox/config.cfg` for `wps:`
- parse `sbs:` as well as `wps:`
- read the selected `.wps` and `.sbs` files from the connected device when
  available
- parse `%Cl(x,y,w,h,...)` album-art load tags
- record the requested dimensions per target/profile

Fallbacks:

- if parsing fails, use profile defaults for known themes
- for iPone 320x240, use the active WPS slot dimensions from the checked-in
  theme
- keep `cover.jpg` export enabled for compatibility

Size priority:

1. active WPS main art
2. active SBS/miniplayer art visible during playback
3. lockscreen/notification art
4. profile fallback sizes

### 2. Export Exact-Size Rockbox BMP Covers

Use the existing artwork export method alongside `export_device_cover()`:

`export_rockbox_wps_cover(album_info, size=(w, h), force=False)`

It should:

- use the best album source RockPod already knows
- crop/fit consistently with WPS expectations
- output BMP, not JPEG, to avoid first-load JPEG decode cost
- name the file according to Rockbox's first-hit convention:
  `cover.<width>x<height>.bmp`
- optionally also export `<album>.<width>x<height>.bmp` only when it is safe and
  sanitized
- store source hash, output hash, dimensions, and fit mode in album metadata
- avoid rewriting if the source hash and output dimensions are unchanged

Destination:

- copy the exact-size BMP into each synced album folder:
  `Music/Artist/Album/cover.<width>x<height>.bmp`
- keep `Music/Artist/Album/cover.jpg` as the generic fallback when configured

This works with Rockbox's current `find_albumart()` order: size-specific
`cover.<width>x<height>.bmp` is checked before generic `cover.jpg`.

### 3. Add Sync Planning for WPS Covers

Extend `_populate_artwork_sync_plan()`:

- collect required WPS sizes for the connected device
- for each album target, export exact-size BMPs for those sizes
- compare output hashes against existing device files
- add only changed/missing BMPs to `plan.artwork_to_copy`
- include counts in sync dialog under album covers

Config:

- `export_wps_sized_covers`: default `true`
- `wps_cover_fit_mode`: default `contain`
- `max_wps_cover_sizes_per_device`: default `2`

The current working tree has these config keys plus targeted unit and simulator
coverage. Device-truth-first size discovery is implemented through
`.rockbox/config.cfg`; UI exposure remains optional follow-up work because the
managed RockPod sync path now has safe defaults.

### 4. Keep Existing Generic Cover Flow

Do not remove current `cover.jpg` export. It remains useful for:

- themes with unparsed or changed dimensions
- other Rockbox browsers/plugins
- manual file browsing
- devices not managed by RockPod

## Proposed Rockbox Design

### 1. Prefer Exact-Size Covers Already Supported

The first phase should rely on current behavior:

- keep `find_albumart()` search order intact
- ensure generated `cover.<width>x<height>.bmp` names match the exact WPS slot
- validate Rockbox picks the BMP before `cover.jpg`

This is the lowest-risk improvement because it mostly shifts work to RockPod.

### 2. Reduce BMP Load Overhead

Update `apps/buffering.c` so external `.bmp` album-art loads do not reserve
JPEG decode overhead.

Current behavior:

- all `TYPE_BITMAP` loads reserve output bitmap space
- all `TYPE_BITMAP` loads also reserve `JPEG_DECODE_OVERHEAD` when `HAVE_JPEG`
  is enabled
- this happens even for exact-size external BMPs

Desired behavior:

- embedded JPEG/PNG and external JPEG paths keep the existing overhead
- external BMP paths reserve only bitmap output space plus resize scratch space
- exact-size BMPs should avoid resize scratch space if `read_bmp_fd()` can
  confirm no resize is needed; this is optional and should be measured

### 3. Prefer BMP for Size-Specific Probes

Update `apps/recorder/albumart.c` so generated size-specific BMPs are reached
with fewer failed file checks:

- for size strings such as `.138x138`, probe `.bmp`, then `.jpeg`, then `.jpg`
- for generic covers, keep current behavior unless measurement shows otherwise

This keeps compatibility with existing JPEG covers while making RockPod's
generated BMP path the cheapest path.

### 4. Add Lightweight Album-Art Timing Logs

Behind a debug or logf build option, measure:

- path search time
- selected path
- decode/buffer time
- source type: exact BMP, generic image file, embedded artwork
- whether `last_folder_aa_path` cache hit

These logs make simulator and hardware comparison objective.

### 5. Optional Async Placeholder Flow

If first draw still blocks, change WPS behavior so art decode never blocks the
initial WPS paint:

- WPS draws immediately with placeholder or empty art frame
- album art load continues through playback/buffering
- when the handle becomes valid, WPS invalidates only the art viewport
- failures settle to a placeholder without repeated retries

This should be a second phase because it touches playback/WPS timing more
deeply.

### 6. Optional Small LRU Cache

Replace or extend `last_folder_aa_path` with a tiny LRU of decoded folder-art
handles:

- current album
- previous album
- next album

Rules:

- never evict audio or codec buffers just to keep art
- disable or shrink cache on low-memory targets
- cache by canonical art path plus dimensions
- flush on WPS dimension change or album-art mode change

## Implementation Plan

1. Finish RockPod tests for the current WPS-sized cover implementation.
   - `%Cl` parsing for iPone, SpringPod3, malformed skins, and multiple slots.
   - BMP format, exact dimensions, no unnecessary rewrite, and hash metadata.
   - Missing exact-size BMP is planned without recopying music.
   - Unchanged BMP is skipped.
2. Improve active device size discovery.
   - Parse connected `.rockbox/config.cfg`.
   - Parse both active `.wps` and `.sbs`.
   - Fall back to profile/default sizes.
3. Fix fallback size data and cap/priority rules.
   - Main WPS size first.
   - SBS/miniplayer size second.
   - Lock/notification size only if cap allows.
4. Add the low-risk Rockbox changes.
   - Skip JPEG overhead allocation for external BMPs.
   - Prefer `.bmp` for size-specific album-art probes.
   - Add debug timing logs as follow-up measured instrumentation.
5. Add a simulator fixture with an album that has both `cover.jpg` and
   `cover.<w>x<h>.bmp`.
   - Verify Rockbox chooses the exact-size BMP.
6. Measure before/after on iPod Video 5G simulator:
   - first track selection from stopped state
   - skip to next track same album
   - skip to next track different album
   - return to previous album
7. If planning time becomes noticeable on large libraries, add an artwork
   manifest to avoid hashing unchanged device covers.
8. If first WPS paint still blocks after exact-size BMP and Rockbox low-risk
   changes, prototype async placeholder redraw and/or the tiny LRU cache.

## Acceptance Criteria

- On a synced iPone/iPod Video 5G target, each album folder has:
  - `cover.jpg` when generic cover export is enabled
  - `cover.<wps_width>x<wps_height>.bmp` when WPS-sized cover export is enabled
- Rockbox first selects the exact-size BMP for WPS art.
- First WPS paint does not wait on embedded-art decode when an exact-size BMP is
  present.
- First selection with pre-staged BMP is measurably faster than JPEG-only
  `cover.jpg`.
- Exact-size BMP loads reserve less playback buffer memory than JPEG/generic
  image loads.
- Size-specific generated BMPs are found without probing JPEG extensions first.
- Repeating sync does not rewrite unchanged cover BMPs.
- Adding new music plans artwork-only copies for missing covers without
  resyncing unchanged audio.
- Playback remains uninterrupted when art is missing, oversized, corrupt, or the
  audio buffer is tight.
- Simulator screenshots show no stale previous-album art during track changes.

## Validation Matrix

| Scenario | Expected result |
| --- | --- |
| Album has exact-size BMP and generic JPG | exact-size BMP is loaded first |
| Album has only generic JPG | Rockbox falls back to current behavior |
| Album has embedded art only | playback works; art may arrive later |
| Theme changes WPS art dimensions | RockPod generates new size on next sync |
| Same album, next track | decoded art cache is reused |
| Different album, next track | next art is preloaded or placeholder swaps cleanly |
| Corrupt BMP | placeholder/no-art state appears; playback continues |
| Repeated sync | no unchanged artwork files are recopied |

## Open Questions

- Should RockPod parse every installed WPS or only the active WPS?
- Should exact-size covers use `contain` with padding or `cover` with center
  crop for the iPone design?
- Should exact-size BMPs live only beside music, or should RockPod also mirror
  them into `.rockbox/albumart/Artist-Album.<w>x<h>.bmp`?
- What is the minimum acceptable first-art display time on iPod Video 5G
  hardware?
