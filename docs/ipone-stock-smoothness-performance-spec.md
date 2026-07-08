# iPone Stock Smoothness and Performance Spec

## Scope

This spec covers two related goals:

- Make the iPone full-art SBS right pane feel closer to stock iPod Classic cover-flow/sidebar art: continuous-looking panning, random album order, no right-pane flashing, no lock/charge/menu regressions.
- Identify Rockbox performance work in this repo that is worth doing without trading away battery life or breaking the 5G/6G theme split.

This is a research/spec document only. Implementation should happen in small phases with simulator video proof before hardware pushes.

## Findings

### The current full-art animation is cadence-limited

`apps/gui/statusbar-skinned.c` drives the right-pane slideshow from the SBS update loop:

- `IPONE_RIGHT_PANE_SLIDESHOW_UPDATE_DELAY` is `MAX(1, HZ / 20)`.
- When full-art mode is active, the whole SBS update loop is pulled toward that 20 fps cadence.
- The draw path calls `albumlist_draw_slideshow(&screens[screen], 160, 0, 160, 240)` and then `lcd_update_rect(160, 0, 160, 240)`.

The art update is already restricted to the right half of the LCD, which is good. The problem is that art animation and normal SBS skin refresh are coupled. Raising this cadence directly would make the whole statusbar skin do more work.

### The current motion model guarantees visible stepping

`apps/gui/albumlist_art.c` uses:

- `ALBUMLIST_SLIDESHOW_SIZE 260`
- `ALBUMLIST_SLIDESHOW_PERIOD (HZ * 20)`
- integer `src_x` / `src_y` crop coordinates passed to `bmp_part`

On a 160x240 right pane, a 260x260 source has about 100 horizontal pixels of pan range. Over a 20 second period, that is only 5 pixels per second. At 20 fps, the ideal motion is 0.25 pixels per frame, but the renderer can only crop at integer source pixels. That means several identical frames followed by a one-pixel jump. This matches the “jumpy” feel.

The fix should not start with a subpixel or bilinear renderer. On PP502x-era hardware, that is likely too expensive for both smoothness and battery. The first fix should make integer-pixel motion naturally advance often enough.

### The slideshow can still do disk work near the hot path

`albumlist_draw_slideshow()` caches the manifest count for 30 seconds, but the supporting helpers still scan files:

- `albumlist_count_manifest_entries()` opens and scans `/.rockbox/albumlist/index.tsv`.
- `albumlist_manifest_path_at()` opens and scans the same TSV to find a selected index.
- Album-list thumbnail lookup has a 16-entry album/artist cache, but misses can still call manifest scanning.

This is acceptable for a basic feature but not ideal for an always-visible animated pane.

### The device may still resize slideshow art

`albumlist_art.c` first looks for `/.rockbox/albumlist/slides/<album_id>.bmp`, then falls back to cover paths and calls `read_bmp_file(... FORMAT_RESIZE | FORMAT_KEEP_ASPECT | FORMAT_DITHER ...)`.

`rockpod/services/artwork_manager.py::export_album_list_thumbnail()` exports album-list thumbnails, but there is no matching right-pane slide export in the inspected path. If the slide files are missing, Rockbox may resize/dither larger art on device. That is the wrong side of the pipeline for a smooth idle animation.

### Existing Rockbox performance knobs should stay targeted

The repo already has useful general mechanisms:

- `dircache` defaults on for targets with enough RAM.
- `tagcache_ram` exists but defaults off.
- `storage mode` supports `auto,hdd,ssd`.
- `cpu_boost()` is used around expensive work across the tree.
- Kinetic list scrolling already has its own `HZ/25` reload cadence.

The right answer is to use these selectively. Always-on CPU boost or a global animation/performance mode would likely cost battery and could create audio/UI side effects.

### User-action CPU boost already exists

`firmware/drivers/button_queue.c` already implements a smart button boost on adjustable-frequency targets:

- Button events call `button_boost(true)`.
- The boost timeout is `BUTTON_UNBOOST_TMO HZ`, about one second.
- Repeated button activity extends the timeout.
- The queue loop calls `button_boost(false)` after the timeout passes.
- iPod Video and iPod 6G both define `HAVE_GUI_BOOST`, so they get the immediate button-event kick.

That means a new generic "boost while the user is doing things" layer would duplicate existing Rockbox behavior. The safer improvement is narrower: boost only around slideshow cache-miss work that is not already covered by the button path.

`cpu_boost()` itself is reference-counted in `firmware/system.c`, so short paired boost/unboost blocks are established and low correctness risk. The real risks are battery cost and missed unboost paths. `trigger_cpu_boost()` / `cancel_cpu_boost()` are per-thread wrappers in `firmware/kernel/thread.c`, but the slideshow code should use direct paired `cpu_boost(true/false)` only around a small synchronous block unless it moves loading to a worker thread.

## Goals

- No disk I/O in the per-frame right-pane draw path.
- No full SBS skin refresh solely to advance album-art pan.
- During active pan, minimize duplicate adjacent frames.
- Keep the existing 5G behavior working.
- Keep WPS, SBS lock screen, charge screen, and miniplayer mode from drawing over each other.
- Preserve battery-sensitive defaults: full-art remains an opt-in/right-pane setting, and miniplayer remains the cheaper mode.

## Recommended Implementation

### 1. Add a motion measurement gate

Create a simulator-side gate, either by extending `tools/sbs_miniplayer_capture.sh` or adding a companion profiler script.

The gate should:

- Record 8-12 seconds of SBS full-art right-pane video for 5G and 6G.
- Crop the right pane.
- Report duplicate-frame ratio during active pan.
- Report large frame gaps or flashes.
- Report apparent edge displacement jitter.
- Save results under `docs/sbs-miniplayer-shots/<case>/motion-report.txt`.

Acceptance target:

- No missing asset warnings.
- No right-pane black flash frames.
- Active-pan duplicate-frame ratio below 15%.
- No max frame gap above two intended update intervals in simulator capture.

### 2. Decouple art refresh from SBS skin refresh

In `apps/gui/statusbar-skinned.c`, add a separate right-pane slideshow schedule per screen.

The normal SBS skin update should keep its current cadence. When full-art mode is active and the art pane is due, draw only:

```c
albumlist_draw_slideshow(&screens[SCREEN_MAIN], 160, 0, 160, 240);
lcd_update_rect(160, 0, 160, 240);
```

Then test art-only cadence at `HZ / 30` or `3` ticks on `HZ == 100` targets. Do not raise the whole SBS update loop to 30 fps.

Guardrails:

- Do not draw in WPS.
- Do not draw on the SBS lock screen unless that mode explicitly owns the pane.
- Do not draw on charge screen.
- Do not draw when the iPone SBS is not active.

### 3. Replace the 20-second linear pan

Keep integer-pixel drawing, but change the motion to a stock-like segment:

- Pan a cover for about 6-8 seconds.
- Hold briefly or cross to the next cover.
- Use randomized horizontal and vertical pan directions.
- Make every active pan frame advance often enough to be visible.
- Use ease-in/ease-out only if it does not create long runs of duplicate source coordinates.

The important change is velocity. A 100-pixel range over 20 seconds at 20 fps repeats frames by design. A 100-pixel range over 6-8 seconds at 25-30 fps moves about 0.4-0.7 pixels/frame, which reduces duplicate frames without subpixel blending.

Implementation detail:

- Use fixed-point phase for timing.
- Quantize to integer crop coordinates only at the final `bmp_part` call.
- Carry the last crop coordinate and avoid generating long runs of identical coordinates during the active-pan segment.

### 4. Remove avoidable per-frame work

In `albumlist_draw_slideshow()`:

- Do not clear the pane with `fillrect()` when a valid full-pane art slot will cover it.
- Only clear when no slot is available or the draw dimensions leave exposed edges.
- Keep the current small left-edge shadow blend. It is only 18 pixels wide and fixes the visual integration with the menu.

This should reduce flashing risk and save some memory bandwidth.

### 5. Cache the album manifest in RAM

Replace repeated TSV scans with a bounded in-memory table for slideshow and album-list art lookup.

Recommended shape:

- Parse `/.rockbox/albumlist/index.tsv` once into a compact table of album IDs, thumb paths, cover dirs, album names, and artist names.
- Reload when file size/mtime changes, or on a conservative timed refresh.
- Use the table for manifest count and random index resolution.
- Use the table for album/artist thumbnail lookup before falling back to any slower path.
- Cap table memory and fail gracefully if the library is too large.

This removes file scanning from animation and visible list-row cache misses.

### 6. Export native slideshow slides from RockPod

Add a RockPod artwork export for right-pane slideshow assets:

- Output path: `.rockbox/albumlist/slides/<album_id>.bmp`
- Size: 260x260 native BMP for the current iPone right-pane source size.
- Fit: cover/center-crop rather than contain with letterboxing.
- Store source hash and output hash in album metadata.
- Re-export when source hash or target size changes.

Rockbox should then load already-sized BMPs and avoid resizing/dithering full-art slideshow images on device.

### 7. Increase small album thumbnail cache

`ALBUMLIST_BITMAP_CACHE` is currently 8. Increase it to 24 or 32 after checking memory on 5G.

Approximate cost for 40x40 RGB565 thumbnails is low compared with the slideshow buffers. This should reduce rereads while scrolling album lists, including compact mode where tiny art must remain visible.

### 8. Fix simulator/capture reliability

The capture/dev path is part of performance work because false missing-assets reports waste time.

Worth fixing:

- Make the SBS capture setup replace the destination theme directory before copying, or copy directory contents rather than nesting `wps/iPone` inside an existing `wps/iPone`.
- Add a minimal simdisk mode for capture runs so `/tmp` does not fill with full `.rockbox` roots.
- Emit missing-asset warnings into the capture report.

### 9. Add bounded slideshow CPU boost only for cache misses

After the motion gate exists, add profiling around `albumlist_load_slideshow_slot()` and `load_thumb_bitmap()`. If cache-miss decode time is a visible source of stalls, wrap only the synchronous load/decode block:

```c
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
cpu_boost(true);
#endif
rc = read_bmp_file(...);
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
cpu_boost(false);
#endif
```

Guardrails:

- Do not boost during every animation frame.
- Do not boost while the pane is idle and the current/next slots are already cached.
- Do not add a second generic user-action boost; the button queue already handles that.
- Pair every exit path, including read failures, with unboost.
- Prefer pre-rendered `.rockbox/albumlist/slides/<album_id>.bmp` so this path becomes rare.

This is low risk if scoped this way because it follows existing Rockbox patterns used by image scaling, database retrieval, file operations, and skin loading.

## Optional Performance Tweaks

These are worth considering, but should not be defaulted blindly:

- Short CPU boost only around slide decode/load cache misses, then immediately unboost.
- For flash-modded iPods, set `storage mode: ssd` through user/device profile after confirming hardware.
- For 64 MB 5G units, consider `tagcache_ram: quick` as an optional profile, not a hard default.
- Add debug counters around slideshow slot load time, manifest reload count, thumbnail cache hits/misses, and draw cadence.

## Not Recommended

- Do not globally force CPU boost for full-art mode.
- Do not add another generic user-action boost on top of `button_queue.c`.
- Do not raise the whole SBS skin loop to 30 fps.
- Do not implement bilinear/subpixel pan until integer-motion scheduling and asset pre-rendering are profiled.
- Do not force `tagcache_ram` for all devices.
- Do not change WPS layout while working on SBS smoothness.

## Acceptance Plan

Before pushing to hardware:

1. Build 5G and 6G simulator targets.
2. Run SBS full-art capture with no music playing.
3. Run SBS full-art capture while music is playing.
4. Run SBS lock screen with music playing.
5. Run WPS lock screen to confirm it is not affected.
6. Run album list full and compact modes to confirm art still appears.
7. Compare motion reports against the baseline.

Hardware push criteria:

- 5G and 6G simulator screenshots show correct layout.
- Motion report shows lower duplicate-frame ratio and no flashes.
- No missing fonts/assets in simulator.
- No menu drawing over SBS lock wallpaper.
- Album art works in full-art right pane and album list on 5G and 6G.

## Priority

Implement in this order:

1. Measurement gate.
2. Art-only refresh scheduling.
3. Faster pan segment with randomized direction.
4. Remove redundant per-frame clear.
5. RockPod pre-rendered slideshow slides.
6. Manifest RAM table.
7. Thumbnail cache increase.
8. Bounded cache-miss CPU boost if profiling shows decode stalls.
9. Optional storage/tagcache profile work.

The first four items should directly address the visible choppiness. Items five through eight reduce stalls and missing-art cases. Optional profile work can improve real-device feel, but only after confirming the iPod storage/RAM setup.
