# Game & Watch Rockbox Port Spec

## Summary

Implement a Game & Watch system in the existing game library as a real playable
plugin, with RockPod sync support and controls that feel like stock click-wheel
iPod games.

Recommended emulator base: `libretro/gw-libretro`.

Rationale:

- It is small, zlib-licensed C code and therefore fits Rockbox plugin
  constraints better than MAME.
- Upstream describes the core as a libretro core for Game & Watch simulators.
- Upstream changelog says the core became pure C in version 1.4.0.
- It already has menu and button abstraction work intended to avoid weird
  mappings.
- It uses Lua game packages converted from MADrigal simulators, so it is a
  better first port than a full Sharp SM5xx/MAME hardware port.

Important caveat: `gw-libretro` is simulator based, not a cycle-accurate
original-ROM hardware emulator. Treat the game packages/assets as user-supplied
content unless their redistribution terms are explicitly cleared.

MAME is the accuracy-first reference, but not the first implementation target:
its Game & Watch support lives in the large C++ handheld/MAME device framework
with artwork/layout dependencies. Porting that cleanly to this C Rockbox tree
would be a separate emulator project, not a quick plugin.

## Current Local State

Game & Watch already exists as launcher scaffolding:

- `apps/plugins/rockboy_launcher.c` defines `GWATCH_ROM_DIR`.
- The default systems list includes `gwatch`, but with an empty plugin path.
- No `gwatch` extension list is configured, so normal scanning finds no files.
- `apps/plugins/bitmaps/native/game_system_gwatch.50x36x24.bmp` already exists.

RockPod already has the correct sync surface:

- `rockpod/services/rockbox_games.py` handles game discovery, cover staging,
  launcher index generation, system manifests, save backup/restore, and
  simulator/device targets.
- `rockpod/ui/game_manager.py` is the game sync panel.
- Existing system-manifest handling covers `nes` and `smsgg`; Game & Watch
  should extend the same path instead of adding a standalone sync script.

## Target User Experience

In Rockbox:

- Game Library shows `Game & Watch` as a normal system.
- Entering it lists synced games with covers and metadata.
- Selecting a game opens `gwatch.rock` with the selected package path.
- Menu returns to the library, preserving the previous launcher state.
- Play/Pause pauses and resumes.
- Volume keys retain normal Rockbox volume behavior.
- Exit returns cleanly to the launcher and does not break later music playback.

In RockPod:

- The Games panel discovers Game & Watch packages from the configured game
  library.
- Sync copies packages, covers, and manifests to the mounted iPod or bound
  simulator.
- Dry-run, remove, cover optimization, metadata display, and simulator launch
  work the same way as other game systems.

## File Layout

Device and simulator target paths:

```text
/.rockbox/rocks/games/gwatch.rock
/.rockbox/games/gwatch/roms/<game>.mgw
/.rockbox/games/gwatch/covers/<game>.bmp
/.rockbox/games/gwatch/games.tsv
/.rockbox/games/gwatch/saves/<game>.state
/.rockbox/games/gwatch/config.cfg
```

Use `.mgw` as the canonical synced package extension. A package is a zip-style
or directory-equivalent bundle containing the converted Lua game script,
background/sprite assets, optional audio samples, and metadata. RockPod may also
accept `.gw`, `.gwz`, and `.zip` at import time, but should normalize the device
copy to `.mgw` once the package structure validates.

`games.tsv` format should reuse the existing system manifest shape:

```text
id<TAB>title<TAB>file<TAB>cover<TAB>favorite<TAB>last_played<TAB>haptic_profile
```

Optional future columns can be appended without breaking the current parser:

```text
publisher<TAB>developer<TAB>year<TAB>series<TAB>controls
```

## Firmware Implementation

Add a new plugin subtree:

```text
apps/plugins/gwatch/
    SOURCES
    gwatch.make
    LICENSE.upstream
    README.rockbox.md
    gwatch.c
    gwatch_core.c/.h
    gwatch_lua_platform.c/.h
    gwatch_package.c/.h
    gwatch_video.c/.h
    gwatch_input.c/.h
    gwatch_audio.c/.h
    gwatch_menu.c/.h
    upstream/
```

Build integration:

- Add `apps/plugins/gwatch` to `apps/plugins/SUBDIRS`.
- Add `gwatch` to `apps/plugins/CATEGORIES` as `games`.
- Add viewer entries to `apps/plugins/viewers.config`:

```text
mgw,games/gwatch,6
gw,games/gwatch,6
gwz,games/gwatch,6
```

- Add `apps/plugins/gwatch/gwatch.make` following the `smsgg` and `arduboy`
  plugin patterns.

Launcher integration:

- Define `GWATCH_PLUGIN_PATH` as `PLUGIN_GAMES_DIR "/gwatch.rock"`.
- Give the default `gwatch` system this plugin path.
- Add `.mgw,.gw,.gwz` in `set_system_extensions()`.
- Keep the existing Game & Watch cover fallback.
- Update setup text to reference `.rockbox/games/gwatch/roms/`.
- Avoid Rockboy fallback launch for Game & Watch entries by always carrying a
  plugin path or package plugin entry.

Core porting:

- Import only the portable `gw-libretro` core pieces and preserve upstream
  license notices.
- Replace libretro callbacks with a small Rockbox host interface:
  - `gwatch_core_load(path)`
  - `gwatch_core_reset()`
  - `gwatch_core_run_frame(input, audio_out, video_out)`
  - `gwatch_core_save_state(path)`
  - `gwatch_core_load_state(path)`
  - `gwatch_core_unload()`
- Use Rockbox file APIs through a package loader. Do not depend on libc file
  paths inside upstream code.
- Allocate from the plugin audio buffer or plugin heap once at startup, using a
  bump allocator like `smsgg_platform.c`.
- Disable dynamic allocation after game load unless upstream Lua absolutely
  needs it; if Lua needs allocation, route it through a fixed arena with clear
  out-of-memory handling.

Rendering:

- Render to an internal 320x240 native framebuffer for iPod Video/Classic.
- Composite LCD background, active segments, labels, and menu overlays once per
  frame.
- Cache decoded package bitmaps after load. Do not decode images during the
  gameplay loop.
- Provide view modes:
  - `Fit`: preserve package aspect inside 320x240.
  - `Zoom`: crop to gameplay LCD area when useful.
  - `Device`: show the full handheld art.
- Default to `Fit` for full game speed.

Timing and performance:

- Use fixed-step emulation driven by Rockbox ticks.
- Target full upstream game speed with no frame skipping on iPod Video 5G and
  iPod Classic 6G/7G.
- Prefer one simulation step per rendered frame for these games; if a package
  asks for higher-frequency timers, run multiple cheap core ticks before one
  LCD draw.
- Add a debug FPS overlay behind a menu option, not enabled by default.
- Acceptance target: average frame loop time below 12 ms on iPod Video 5G for
  representative single-screen and dual-screen games, leaving headroom for input
  and audio.

Audio:

- Game & Watch sound is short beeps and effects, not long-form media.
- Implement audio after silent video/input is stable.
- If using mixer output, use a short-effect-safe channel and follow
  `docs/plugin-audio-lifecycle-steering.md`.
- Do not mutate user playlists.
- On exit, stop callbacks, drain/stop the plugin audio path, then release any
  shared audio buffer only after callbacks can no longer touch plugin memory.
- Provide a `Sound: On/Off` setting. Default can be `Off` until hardware audio
  lifecycle testing is complete.

## Controls

Design goal: feel like stock iPod games, not like a desktop emulator.

Global:

```text
Menu               In-game menu / back
Long Menu          Exit to launcher
Play/Pause         Pause / resume
Select             Confirm / primary action
Left/Right         Left/right gameplay buttons
Wheel rotate       Alternate left/right for games that benefit from wheel input
Select + Left      Game A from title/menu
Select + Right     Game B from title/menu
Select + Play      Time/clock when supported
```

Menu overlay:

```text
Wheel              Move selection
Select             Activate
Menu               Back
Play/Pause         Pause / resume
```

Per-game control profiles:

- Package metadata may declare logical buttons:
  `left,right,up,down,action,game_a,game_b,time,alarm`.
- Default mapping should be generated from that metadata.
- Store overrides in `/.rockbox/games/gwatch/config.cfg`.
- Never require more than one awkward combo for normal gameplay. Game A/B/Time
  can live in the menu because they are mode buttons, not constant action
  buttons.

Haptics:

- Optional short haptic ticks on button presses, following `smsgg_haptics`.
- Keep haptics disabled by default until device testing confirms no input lag.

## RockPod Sync Implementation

Extend `rockpod/services/rockbox_games.py`:

- Add extensions:

```python
GWATCH_PACKAGE_EXTENSIONS = {".mgw", ".gw", ".gwz", ".zip"}
GWATCH_ROM_TARGET_DIR = ".rockbox/games/gwatch/roms"
GWATCH_COVER_TARGET_DIR = ".rockbox/games/gwatch/covers"
GWATCH_SAVE_TARGET_DIR = ".rockbox/games/gwatch/saves"
GWATCH_PLUGIN_PATH = ".rockbox/rocks/games/gwatch.rock"
```

- Include `.mgw`, `.gw`, `.gwz` in discovery.
- Treat `.zip` as importable only if package validation confirms it is a Game &
  Watch package.
- Add `gwatch` to `SYSTEM_MANIFEST_RELATIVE_PATHS`.
- Add package extensions to `SYSTEM_MANIFEST_EXTENSIONS`.
- Route Game & Watch entries like SMS/GG:
  - launcher index `rom_path` points to `/.rockbox/rocks/games/gwatch.rock`;
  - `plugin_param` points to the synced package under
    `/.rockbox/games/gwatch/roms/`;
  - `games.tsv` lists the package file directly.
- Stage covers to `/.rockbox/games/gwatch/covers/<stem>.bmp`.
- Back up and restore save states from `/.rockbox/games/gwatch/saves/`.
- Add performance thresholds tuned for packages, not ROM size. Warn on packages
  above 8 MiB and critical above 16 MiB for iPod 5G until measured.

Package validation:

- Required: one manifest file, one Lua/script payload, and declared title.
- Required metadata:

```json
{
  "system": "gwatch",
  "title": "Fire",
  "id": "fire",
  "version": 1,
  "entry": "main.lua",
  "controls": ["left", "right", "game_a", "game_b", "time"]
}
```

- Reject packages with absolute paths, parent-directory paths, or files outside
  the package root.
- Reject unexpectedly huge files before copying to FAT32 targets.

UI changes:

- Rename "Choose ROM Folder" copy to "Choose Game Folder" or leave as-is if the
  broader Games panel wording is out of scope.
- Show platform labels: Game Boy, NES, Sega, Game & Watch.
- Let Fetch Cover use sidecar covers first. Do not rely on Libretro thumbnail
  lookup for Game & Watch unless a reliable named source is added.

Tests:

- Discovery includes `.mgw` and excludes invalid `.zip`.
- Sync copies package, cover, launcher index, and `gwatch/games.tsv`.
- Remove deletes package and cover and prunes both manifests.
- Missing local package remains removable from device/simulator.
- Save backup/restore handles `.state`.
- Simulator target paths mirror device target paths.

## Legal and Distribution Policy

- Ship the emulator plugin source and binary.
- Do not ship MADrigal-derived game packages in the firmware zip unless
  redistribution terms are explicitly documented and compatible with this repo.
- RockPod may sync user-provided packages from a local folder.
- Include upstream `gw-libretro` zlib license in
  `apps/plugins/gwatch/LICENSE.upstream`.
- Keep a `README.rockbox.md` that explains where users place legally obtained
  packages.

## Validation Plan

Firmware:

- `make -C build-sim rocks` or the equivalent configured simulator build.
- Launch a `.mgw` package from the file browser.
- Launch the same package from Game Library.
- Verify Menu returns to Game Library.
- Verify Play/Pause pauses timers and audio.
- Verify all mapped controls register once per click and wheel detents do not
  over-repeat.
- Verify no frame drops on iPod Video 5G simulator before device testing.

RockPod:

- `./rockpod/.venv/bin/python -m pytest rockpod/tests/test_rockbox_games.py -q`
- Add targeted tests for Game & Watch package discovery and manifests.
- Dry-run sync to simulator and inspect generated paths.
- Remove synced packages and verify unrelated game files remain.

Device:

- Fresh boot -> Game & Watch -> exit -> Database music starts.
- Database music -> Game & Watch -> exit -> Database music still starts.
- Files music -> Game & Watch -> exit -> Files music still starts.
- Rapid launcher/game/menu transitions do not freeze.
- Sound enabled test only after silent mode is stable.

## Implementation Phases

1. Launcher and RockPod metadata
   - Wire `gwatch.rock` path, extensions, setup text, and RockPod manifest
     generation.
   - Add tests proving sync and launcher manifests are correct.

2. Silent playable plugin
   - Port `gw-libretro` enough to load one known-good package, render video, and
     handle stock iPod controls.
   - No audio yet.

3. Package compatibility
   - Support multiple representative package layouts.
   - Add package validator and clearer error screens.

4. Saves and settings
   - Save/load state, per-game control profile, view mode, and sound toggle.

5. Audio and haptics
   - Add beep/effect playback with plugin audio lifecycle tests.
   - Add optional haptics after input timing is verified.

6. Performance polish
   - Cache all render assets.
   - Add profiling overlay.
   - Tune scaling and input repeat for iPod Video 5G and iPod Classic 6G/7G.

