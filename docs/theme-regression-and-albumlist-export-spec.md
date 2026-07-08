# Theme Regression Capture And Album-List Export Spec

## Purpose

Make visual/theme iteration repeatable without tying the workflow to one skin
name, and make album-list slideshow art refreshable outside a full sync.

## 1. Generic Theme Regression Capture

`tools/theme_regression_capture.sh` is the canonical simulator capture gate.
`tools/ipone_regression_capture.sh` remains as a compatibility wrapper.

Inputs:

- positional `build_dir`: simulator build directory, default
  `build-sim-video-5g`
- positional `out_dir`: screenshot output directory, default
  `docs/theme-regression-shots/<theme>`
- `THEME_CAPTURE_THEME`: theme id, default `iPone`
- `THEME_CAPTURE_START_SCREEN`: start screen, default `wps`
- `THEME_CAPTURE_NORMAL_TRACK`, `THEME_CAPTURE_LONG_TRACK`,
  `THEME_CAPTURE_NO_ART_TRACK`: track paths used for WPS states
- `THEME_CAPTURE_KEEP_ROOT=1`: preserve the temporary simulator root

Behavior:

- copies the simulator `.rockbox` tree into a temporary runtime root;
- overlays the current repo theme files from `themes/`, `wps/`, `backdrops/`,
  `icons/`, and font locations;
- launches `rockboxui --root <temp-root>` so captures do not mutate the source
  simdisk;
- validates active playback through a cropped pixel-delta check before
  lockscreen/volume scenarios;
- captures normal playback, menu mini-player, pause, lockscreen, volume,
  rapid lock/volume cycling, charging/docked, long-title, and no-art states.

## 5. Album-List Slide Export

Rockpod already exports album-list thumbnails, slides, and `index.tsv` during
sync. The missing helper is an explicit refresh command for visual work.

`rockpod/main.py generate-albumlist-art` builds the same `.rockbox/albumlist`
bundle from the local Rockpod database and artwork cache.

Inputs:

- `--config`: Rockpod config path
- `--db`: database path override
- `--mount`: optional mounted device/simulator root to deploy into
- `--out`: optional output root for the generated bundle
- `--force`: refresh cached thumbnail/slide renders
- `--synced-only`: include only tracks marked synced or having `device_path`
- `--limit`: cap the number of albums for quick visual tests
- `--json`: print machine-readable output

Output layout:

```text
<out>/.rockbox/albumlist/index.tsv
<out>/.rockbox/albumlist/thumbs/<album_id>.bmp
<out>/.rockbox/albumlist/slides/<album_id>.bmp
```

If `--mount` is supplied, the command copies the generated bundle into
`<mount>/.rockbox/albumlist`.

Acceptance:

- Running the generic capture for `THEME_CAPTURE_THEME=iPoneCustom` should
  create a separate screenshot set without modifying the base simdisk.
- Running `generate-albumlist-art --out /tmp/al --limit 5` should create an
  album-list manifest and matching thumbnail/slide BMPs for albums with
  available artwork.
- A normal Rockpod sync remains unchanged and continues using the existing
  sync-engine album-list export path.
