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
  - `export_album_list_thumbnail()` already exports BMP thumbnails for Rockbox
    album-list use.
- RockPod sync planning in `rockpod/services/sync_engine.py` already adds cover
  files to `plan.artwork_to_copy`.
  - The current device cover target is `Music/.../cover.jpg`.
  - This helps Rockbox find external art, but still leaves Rockbox to decode a
    JPEG and resize it for the WPS slot on first load.

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
- read the selected `.wps` file
- parse `%Cl(x,y,w,h,...)` album-art load tags
- record the requested dimensions per target/profile

Fallbacks:

- if parsing fails, use profile defaults for known themes
- for iPone 320x240, use the active WPS slot dimensions from the checked-in
  theme
- keep `cover.jpg` export enabled for compatibility

### 2. Export Exact-Size Rockbox BMP Covers

Add an artwork export method alongside `export_device_cover()`:

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

### 2. Add Lightweight Album-Art Timing Logs

Behind a debug or logf build option, measure:

- path search time
- selected path
- decode/buffer time
- source type: exact BMP, generic image file, embedded artwork
- whether `last_folder_aa_path` cache hit

These logs make simulator and hardware comparison objective.

### 3. Optional Async Placeholder Flow

If first draw still blocks, change WPS behavior so art decode never blocks the
initial WPS paint:

- WPS draws immediately with placeholder or empty art frame
- album art load continues through playback/buffering
- when the handle becomes valid, WPS invalidates only the art viewport
- failures settle to a placeholder without repeated retries

This should be a second phase because it touches playback/WPS timing more
deeply.

### 4. Optional Small LRU Cache

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

1. Add WPS album-art size parser in RockPod.
   - Unit test `%Cl` parsing for iPone and malformed skins.
2. Add `export_rockbox_wps_cover()` to `ArtworkManager`.
   - Unit test BMP format, exact dimensions, no unnecessary rewrite, and hash
     metadata.
3. Extend `SyncEngine._populate_artwork_sync_plan()`.
   - Unit test missing exact-size BMP is planned without recopying music.
   - Unit test unchanged BMP is skipped.
4. Add a simulator fixture with an album that has both `cover.jpg` and
   `cover.<w>x<h>.bmp`.
   - Verify Rockbox chooses the exact-size BMP.
5. Add Rockbox timing/log instrumentation around `find_albumart()` and
   `bufopen(... TYPE_BITMAP ...)`.
6. Measure before/after on iPod Video 5G simulator:
   - first track selection from stopped state
   - skip to next track same album
   - skip to next track different album
   - return to previous album
7. If exact-size BMP staging is not enough, implement async placeholder redraw
   and/or the tiny LRU cache.

## Acceptance Criteria

- On a synced iPone/iPod Video 5G target, each album folder has:
  - `cover.jpg` when generic cover export is enabled
  - `cover.<wps_width>x<wps_height>.bmp` when WPS-sized cover export is enabled
- Rockbox first selects the exact-size BMP for WPS art.
- First WPS paint does not wait on embedded-art decode when an exact-size BMP is
  present.
- First selection with pre-staged BMP is measurably faster than JPEG-only
  `cover.jpg`.
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
