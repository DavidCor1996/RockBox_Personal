# iPone Full-Art Right Pane Spec

## Scope

This applies to the `ipone right pane: full art` SBS mode on iPod Video/5G and
iPod 6G targets. It must not change the `miniplayer` right-pane mode.

## Behavior

- The right pane is filled by album art for the full height and width of the
  pane.
- A narrow menu-side shadow is drawn over the left edge of the art so the menu
  still reads as the foreground pane.
- Album order is randomized per runtime using a shuffled modular walk through
  the albumlist manifest. It must not simply follow manifest/alphabetical order.
- Art pans continuously for the whole slide period. Horizontal pans are most
  common, with periodic vertical pans for variety.
- Panning uses a constant-speed fixed-point position, not an ease-in/ease-out
  curve, so the art does not slow to a stop and jump at cycle edges.

## Performance

- The current slide and next slide are cached in two bitmap slots.
- The next slide is prefetched before the current cycle ends.
- Resized BMP loads must allocate `ALBUMLIST_SCALED_BYTES`, which wraps
  `BM_SCALED_SIZE` and adds scaler scratch headroom. Plain `BM_SIZE` is too
  small, and `BM_SCALED_SIZE` alone was still short on 5G full-art paths.
- The 5G symptom for undersized resize buffers is missing art in full-screen
  right-pane/full album-list modes while compact album-list art still works.
  Compact can pass because it decodes into a smaller target.
- The SBS update delay remains capped only while the full-art pane is active,
  so miniplayer and non-iPone SBS screens keep their normal refresh behavior.

## Implementation

- `apps/gui/albumlist_art.c`
  - Uses a two-slot slideshow cache keyed by manifest index.
  - Uses a per-runtime modular shuffled manifest walk for album order.
  - Uses fixed-point linear pan position over a 20-second slide period.
  - Alternates reverse direction by cycle and uses occasional vertical pan.
  - Draws a 10px menu-side shadow over the left edge of full-art panes.
  - Uses scaled BMP buffers with extra scratch for both album-list thumbnails
    and right-pane slideshow art.
- `apps/gui/statusbar-skinned.c`
  - Keeps the faster SBS repaint cadence scoped to active full-art right-pane
    mode.
- `tools/sbs_miniplayer_capture.sh`
  - Can record `slideshow-glide.mp4` in addition to screenshot crops.
- `tools/ipone_album_list_capture.sh`
  - Can run against 5G/6G simulator build directories and seed config when the
    simdisk lacks one.

## Verification

- Run static source checks for the setting, full/compact album list paths,
  random slideshow order, shadow draw, two-slot cache, and scaled BMP buffers.
- Build and run 5G and 6G simulators.
- Capture screenshots for the full-art SBS pane and fullscreen album list.
- Capture a short simulator recording of the full-art SBS pane to inspect pan
  smoothness.

## Current Proof

- `./rockpod/.venv/bin/python -m pytest rockpod/tests/test_album_list_layout_source.py -q`
  passed.
- 5G simulator build: `make -C build-sim-video-5g all` passed.
- 6G simulator build: `make -C build-sim-ipod6g all` passed.
- 5G full-art SBS recording:
  `docs/sbs-miniplayer-shots/ipod5g-rightpane-full-idle/slideshow-glide.mp4`
  at 640x480, 120 frames, 6 seconds.
- 6G full-art SBS recording:
  `docs/sbs-miniplayer-shots/ipod6g-rightpane-full-idle/slideshow-glide.mp4`
  at 640x480, 120 frames, 6 seconds.
- 5G full album-list screenshot:
  `docs/album-list-layout-shots/full-5g/03-albums-list.png`.
- 6G full album-list screenshot:
  `docs/album-list-layout-shots/full-6g/03-albums-list.png`.
- Mounted 5G deploy validation found:
  - 239 albumlist thumbnail references, 0 missing.
  - 84 iPone WPS/SBS/FMS bitmap references, 0 missing.
  - Active config/theme font references, 0 missing.
