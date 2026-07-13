# SNES Lite port status

SNES Lite is an experimental iPod Classic 6G+ plugin in RockBox_Personal. The
implemented first milestone uses the Snes9x2002 C fallback, not its platform
assembly, and keeps the third-party core beneath `apps/plugins/snes_lite/`.
See the plugin README and `LICENSE.notes` before redistributing any build.

## Implemented

- Hardware and simulator plugin build integration, including zip packaging.
- Rockbox file loading for raw `.sfc` and `.smc` ROMs (6 MiB plus copier
  header limit), arena allocation from the shared plugin/audio buffer, and
  explicit memory diagnostics.
- Fullscreen 320x240 or centered 256x224 RGB565 output, frameskip auto/off/1-4,
  optional FPS display, CPU boost during gameplay, six click-wheel controller
  profiles, an iPodJS-styled emulator menu, clean reset/quit, and performance
  presets.
- SRAM load/save under `/.rockbox/saves/snes/` on launch, explicit menu save,
  and exit.
- Cartridge header inspection and clean rejection of SuperFX, SA-1, S-DD1,
  and C4 games.
- RockPod `.sfc`/`.smc` scanning, SHA-256 and SNES header metadata,
  compatibility status, cover preparation, ROM/index/config/plugin sync,
  save backup/restore, cache invalidation, and existing list/cover-flow UI.
- Existing iPod JS Games launcher integration through its console manifests:
  `Super Nintendo` appears as a console, reads resized covers lazily, shows
  save status, and invokes SNES Lite with the selected ROM path.

## Audio and per-game controls

Audio uses `PCM_MIXER_CHAN_PLAYBACK` and follows
`docs/plugin-audio-lifecycle-steering.md`: the plugin claims the shared buffer
once, stops its callback before release, restores the previous mixer frequency,
and does not mutate the user's playlist. The core uses the actual mixer rate
(32 kHz normally, 22.05 kHz Economy), while its real NTSC/PAL refresh rate
drives frontend pacing. A 16-block queue, short-block underrun concealment, and
occupancy-driven frameskip reduce crackle and drift.

RockPod classifies SNES titles/genres as platformer, RPG, action, fighting,
racing, or sports. Each sync emits a matching same-basename file beneath
`/.rockbox/config/snes_lite/`; the plugin overlays it on global defaults before
core/audio startup. ZIP loading, save states, arbitrary per-button remapping,
custom cover lookup, special chips, MSU-1, rewind, cheats, shaders, netplay,
and full compatibility are deferred.

## RockPod workflow

Add user-owned ROMs to a configured game library directory. RockPod scans them
as `Super Nintendo`, uses a same-basename local image when supplied, and never
downloads ROMs. In Games, filter to Super Nintendo, select desired titles, and
review the sync plan. The SNES bundle installs the plugin/config/launcher,
selected ROMs, generated covers and `games.tsv`, preserves unselected media,
and never automatically deletes SRAM. Removal is a separate explicit action.

Use mock-device mode before a physical sync. For diagnostics, inspect the sync
plan and `/.rockbox/logs/snes_lite.log`.

## Validation

Run:

```sh
cd rockpod
./.venv/bin/python -m pytest tests/test_rockbox_games.py -q
cd ..
SDL_VIDEODRIVER=dummy python3 tools/snes_lite_sim_gate.py \
  --build-dir build-sim-ipod6g --rom /path/to/test.sfc --run --frames 180
```

Hardware acceptance still requires an iPod session: boot two ordinary games,
verify controls/rendering, persist SRAM in a saving game, and confirm an
unsupported-chip image fails cleanly. Simulator success alone is not evidence
of real-time playability or good hardware audio. The Database/Files/plugin
transition matrix in the audio lifecycle document remains required after each
physical audio update.
