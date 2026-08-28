# A Link to the Past native iPod port specification

## Product boundary

This is a native Rockbox port of `snesrev/zelda3`, not a SNES emulator. The
ported C game logic, optimized PPU renderer, and original SPC/DSP music path run
directly as an iPod plugin. The upstream code is pinned to commit
`fbbb3f967a51fafe642e6140d0753979e73b4090` and retains its MIT license.

Nintendo-owned levels, dialogue, graphics, and audio are not stored in this
repository or build output. The installer accepts only the clean US ROM with
SHA-256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`
and locally produces `zelda3_assets.dat`. The original ROM is not needed at
runtime. Cover art must also be supplied by the owner; the installer only
resizes and letterboxes it to 144x108. No substitute or hand-drawn assets are
generated.

## Target and presentation

- Initial hardware target: iPod Classic 6G/7G, 320x240 color LCD.
- Game viewport: the complete stock 256x224 image expands to 320x240 using a
  fixed nearest-neighbor 5:4 horizontal and 15:14 vertical mapping. This fills
  the LCD and corrects the SNES non-square pixel presentation to 4:3 without
  cropping, filtering, redrawing, or replacing any artwork.
- No widescreen scene extension, shader, texture filtering, replacement sprite,
  or enhanced map art is enabled.
- The game appears in the iPodJS Steam library under Super Nintendo only when
  the native loader, extracted asset file, and authentic cover are installed.

## Controls

| SNES control | iPod control |
| --- | --- |
| D-pad | Click-wheel up/down/left/right |
| B (sword/cancel) | Center |
| A (action/confirm) | Play/Pause |
| Y (equipped item) | Previous/left |
| X | Next/right |
| Start | Menu |
| Select | Menu + Center or Menu + Play/Pause |
| L | Menu + Previous |
| R | Menu + Next |
| Safe exit | Hold switch, or hold Menu |

Menu chords suppress their unmodified button, so opening Select or shoulder
inputs does not also trigger an action button.

## Runtime and performance contract

- The approximately 1.2 MiB engine is a Rockbox overlay loaded by a small game
  launcher.
- Dynamic state and extracted assets use the plugin audio buffer through TLSF;
  no playback buffer is resized or owned by decorative UI code.
- CPU boost is active only while the game is running.
- Rendering is fixed at the original 60 Hz cadence and combines direct
  32-bit-to-native framebuffer conversion with the fixed-ratio fullscreen
  expansion in one pass.
- Audio is generated from the original SPC data at 32 kHz stereo in exact
  533/534-frame increments and queued through the primary playback mixer.
- Optional MSU-1/Opus audio is disabled and omitted from the binary.

## Saving and recovery

The stock three-slot SRAM is loaded from
`/.rockbox/zelda3/saves/sram.dat`. Every in-game save and orderly exit rotates
the previous file to `sram.bak` before writing all 8192 bytes. Existing saves
are never removed by installation.

## Installation

Build `zelda3.rock` and `zelda3.ovl`, then run:

```sh
python3 tools/zelda3_install_assets.py \
  --rom /path/to/zelda3.sfc \
  --cover /path/to/authentic-art-or-screenshot.png \
  --target /path/to/ipod
```

The resulting runtime layout is:

```text
/.rockbox/rocks/games/zelda3.rock
/.rockbox/rocks/games/zelda3.ovl
/.rockbox/zelda3/zelda3_assets.dat
/.rockbox/zelda3/cover.bmp
/.rockbox/zelda3/saves/sram.dat
/.rockbox/zelda3/saves/sram.bak
```

## Qualification gates

The asset-free control and native-engine gates are reproducible:

```sh
tools/zelda3_native_port_gate.py --build-dir build-hw-ipod6g
tools/zelda3_sim_gate.py --build-dir build-sim-ipod6g --controls-only
```

The first requires every upstream gameplay/PPU/DSP object and representative
title-to-ending symbols in the ARM overlay, rejects unresolved symbols and the
optional original-CPU comparison emulator, and confirms no ROM-derived media
is bundled. The second executes the iPod mapping and requires all twelve SNES
control bits (`0xfff`).

Sustained gameplay timing must use a locally extracted authentic archive:

```sh
tools/zelda3_sim_gate.py --build-dir build-sim-ipod6g \
  --assets /path/to/zelda3_assets.dat --frames 3600
```

This runs 60 seconds with video and original SPC audio enabled. It requires
every frame to render, at least 57 fps over the run, and no more than one
percent single-frame overruns. Simulator timing qualifies the runtime loop and
instrumentation; a final full-speed claim for PortalPlayer hardware requires a
sustained run on the physical iPod and inspection of
`/.rockbox/zelda3/zelda3.log`.

1. iPod 6G cross-build produces both loader and overlay without undefined
   symbols.
2. Wrong-region, modified, or headered ROMs are rejected before extraction.
3. Missing asset data produces a clear error and never creates a save.
4. Steam omits incomplete installs and launches the native loader for complete
   installs.
5. New game, all three save slots, save-and-quit, restart/load, backup recovery,
   death/reload, dungeon transitions, Mode 7 map, ending, hold-switch exit, and
   USB exit are exercised on hardware.
6. A 30-minute hardware trace records frame overruns, audio underruns, and
   dropped audio blocks from `/.rockbox/zelda3/zelda3.log`.
