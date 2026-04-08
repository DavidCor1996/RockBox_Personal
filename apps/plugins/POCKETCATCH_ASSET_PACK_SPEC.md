# PocketCatch Asset Pack Spec (v0.1)

Updated: 2026-04-08

This document defines the external asset pack format for the planned `pocketcatch` Rockbox plugin.

Goal: allow two safe workflows:
- Original/custom art packs (recommended for distribution)
- User-local import packs derived from classic game references (for personal use)

## Runtime location

PocketCatch should load external files from `PLUGIN_GAMES_DIR`.

Recommended install location on device/simdisk:
- `.rockbox/rocks/games/pocketcatch/`

Simulator mirrors (if your build tree requires both):
- `build-sim-video/simdisk/.rockbox/rocks/games/pocketcatch/`
- `build-sim-video/simdisk/rocks/games/pocketcatch/`

## Pack folder layout

Required layout:

```text
pocketcatch/
  pack.json
  sprites/
    creatures/
      creature_001_idle_0.bmp
      creature_001_idle_1.bmp
      creature_001_hit_0.bmp
      creature_001_catch_0.bmp
    balls/
      ball_default_idle_0.bmp
      ball_default_spin_0.bmp
      ball_default_spin_1.bmp
      ball_default_throw_0.bmp
    ui/
      ring_outer_0.bmp
      ring_inner_0.bmp
      spark_0.bmp
      hud_panel_0.bmp
      icon_ball_0.bmp
  backgrounds/
    scene_day_layer0.bmp
    scene_day_layer1.bmp
    scene_day_layer2.bmp
  audio/
    sfx_throw.wav
    sfx_hit.wav
    sfx_catch_start.wav
    sfx_catch_success.wav
    sfx_catch_fail.wav
  fonts/
    ui_8x8.fnt
```

## Required files

Minimum required to boot without fallback art:
- `pack.json`
- `sprites/creatures/creature_001_idle_0.bmp`
- `sprites/balls/ball_default_idle_0.bmp`
- `backgrounds/scene_day_layer0.bmp`
- `sprites/ui/hud_panel_0.bmp`

If optional files are missing, plugin should fall back to compiled placeholders.

## `pack.json` schema (v0.1)

```json
{
  "pack_name": "PocketCatch Classic",
  "version": "0.1.0",
  "target": "rockbox-pocketcatch",
  "author": "YourName",
  "screen_mode": "auto",
  "creatures": [
    {
      "id": 1,
      "name": "Sproutle",
      "base_capture_rate": 0.42,
      "sprite_prefix": "creature_001",
      "anim": {
        "idle_frames": 2,
        "hit_frames": 1,
        "catch_frames": 1
      }
    }
  ],
  "balls": [
    {
      "id": "default",
      "name": "Classic Ball",
      "curve_bonus": 0.10,
      "catch_bonus": 1.00,
      "sprite_prefix": "ball_default"
    }
  ],
  "scene": {
    "bg_prefix": "scene_day",
    "ring_radius_px": 18,
    "target_y_px": 42
  }
}
```

## Image format rules

Use BMP for easiest Rockbox loading compatibility.

Recommended constraints:
- Creature sprite frame: `48x48` max
- Ball sprite frame: `20x20` max
- UI element sprite: `32x32` max (except panels)
- Background layers: match LCD size where possible
- Color depth: `24-bit BMP` accepted; keep palettes limited for style consistency

Fallback sizes for lower-memory targets:
- Creature sprite frame: `32x32`
- Ball sprite frame: `16x16`

## Animation naming rules

Creature frames:
- `<prefix>_idle_<n>.bmp`
- `<prefix>_hit_<n>.bmp`
- `<prefix>_catch_<n>.bmp`

Ball frames:
- `<prefix>_idle_<n>.bmp`
- `<prefix>_spin_<n>.bmp`
- `<prefix>_throw_<n>.bmp`

UI frames:
- `<prefix>_<n>.bmp`

Frame index starts at `0` and must be contiguous.

## Audio format rules

Primary target:
- PCM WAV, mono, `22050 Hz`, `16-bit`

If decode/CPU cost is too high later, move to short pre-decoded clips or plugin beep fallback.

## Runtime behavior contract

- Plugin first attempts external pack load.
- If any required file is missing, plugin logs warning and uses compiled fallback for that asset class.
- Missing optional animation frames reduce to frame `0`.
- Invalid `pack.json` falls back to built-in default pack.

## Legal and distribution policy

- Public plugin builds should ship only original or permissive-license assets.
- Any pack derived from commercial game assets should be user-local and not bundled in repo/releases.
- Keep extraction/conversion tooling separate from the main plugin distribution path.

## Suggested conversion pipeline (for personal-use imports)

1. Obtain source sprites/audio from user-owned reference data.
2. Convert indexed/tiled graphics to flat BMP sequence.
3. Normalize dimensions to PocketCatch limits.
4. Generate consistent frame names by prefix.
5. Emit `pack.json` metadata.
6. Copy pack to `.rockbox/rocks/games/pocketcatch/`.

## v0.2 planned extensions

- Multiple scenes with weighted random encounter tables
- Per-creature throw hitboxes in `pack.json`
- Palette swap variants (day/night/shiny)
- Localization strings in `lang/en.txt`
- Optional packed archive (`.pcpack`) for single-file installs
