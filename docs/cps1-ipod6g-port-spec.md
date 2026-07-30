# CPS1 arcade emulator for iPod Classic 6G

## Product contract

The port is a native Rockbox plugin for the 64 MiB iPod Classic 6G/7G. It
launches MAME/FBA-style `.zip` archives without unpacking their ROM chips and
appears as a dedicated `CPS1 Arcade` console in the iPodJS Steam library.

The device layout is:

```
/.rockbox/rocks/games/cps1.rock
/.rockbox/rocks/games/cps1/games.tsv
/.rockbox/games/cps1/roms/*.zip
/.rockbox/games/cps1/install-report.json
/.rockbox/games/library/covers/cps1/*.bmp
```

The supplied `CapcomCPS1ByGhostware.zip` contains 137 nested archives. The
pinned FBA2x driver revision recognizes 116 of them. Those archives represent
31 parent games plus regional clones. The installer copies every nested
archive so parent dependencies remain intact, but publishes one clean Steam
entry per parent game. Twenty-one unsupported bootleg/hack archives are kept
on device and listed in the install report; they are not advertised as
playable.

## Core and licensing

Use the CPS1-only subset of DigitalLumberjack PiFBA/FBA2x revision
`419faa7c3967560f6fa149f2f78b5aedca5b123a`. Its ROM definitions match the
supplied archive by filename and CRC, and its GP2X path is optimized for an
ARM920T-class CPU with the Cyclone 68000 core. The iPod S5L8702 is an ARM926EJ-S
and can execute that ARMv4 assembly.

PiFBA carries GPLv2 at repository level, while the inherited Final Burn Alpha
material has additional non-commercial/source-disclosure restrictions.
Consequently this port is opt-in only:

```
make CPS1_NONCOMMERCIAL=1
```

It must not enter a default public Rockbox binary package. The source import
must retain the complete upstream license, individual component notices, the
pinned revision, a file manifest, and local modifications. ROM images are
never part of source or firmware artifacts. A private device staging package
may contain user-supplied ROMs.

## Runtime architecture

The plugin claims the shared audio buffer with `plugin_get_audio_buffer()` and
uses it as a monotonic arena. The transfer intentionally stops music; the
plugin does not call `audio_stop()` first and does not alter the playlist.
Allocation failure is fatal and reports the requested and remaining byte
counts.

Cyclone runs the 68000. The compatible FBA Z80 implementation runs the sound
CPU. CPS1, CPS1 QSound, YM2151, MSM6295, and the required driver modules are
linked; CPS2, Neo Geo, Cave, and other arcade systems are excluded.

ROM loading searches the selected archive first, followed by its declared
parent archive in the same directory. Members are matched by CRC and then by
name. Deflate uses the repository's small zlib-compatible inflate path.
Temporary ROM decode buffers come from the arena and are released by arena
markers after driver initialization.

## Video

CPS1's normal 384x224 framebuffer is rendered directly as RGB565. The display
path scales horizontally to 320 pixels with a fixed-point nearest-neighbor
table and centers the 224 active lines vertically in the 320x240 LCD. It does
not allocate a second 320x240 framebuffer. Vertical games rotate and
letterbox into 149x240.

The hardware default renders one of every three emulated frames. CPU, input,
and audio continue on every frame, so render skipping does not slow the
arcade clock or pitch. The pause menu exposes render skip 0 through 4 for
per-game tuning. The direct iPod framebuffer path specializes the common
384-to-320 conversion with aligned packed stores and updates only the active
LCD rectangle. Input descriptor pointers are resolved once at startup rather
than searched by name on every emulated frame. A fractional tick accumulator
paces the 59.637 Hz arcade clock without rounding it to the 100 Hz system
tick.

## Audio lifecycle

The core produces signed stereo PCM at 22050 Hz by default. Six fixed
512-frame ring blocks feed `PCM_MIXER_CHAN_PLAYBACK`, starting after two
blocks are ready. The larger blocks reduce mixer locking and callback churn
without increasing the ring's memory footprint. The callback only swaps
ready blocks; it never allocates, logs, opens files, or calls emulator code.

Exit order is mandatory:

1. stop and detach `PCM_MIXER_CHAN_PLAYBACK`;
2. wait until no callback can reference the ring;
3. shut down the driver and core;
4. restore the previous mixer frequency and channel amplitude;
5. release the shared audio buffer.

Database music, Files music, plugin exit to Database, rapid switching,
pause/resume, and volume changes are acceptance tests.

## Click-wheel controls

The wheel's four directional zones are the arcade stick and preserve
diagonals. Controls are described on first launch and in the in-game menu.

| CPS1 input | iPod control |
|---|---|
| Up/down/left/right | wheel zones |
| Button 1 / light punch | Select |
| Button 2 / medium punch | Play/Pause |
| Button 3 / heavy punch | Previous |
| Button 4 / light kick | Next |
| Button 5 / medium kick | Menu + Previous |
| Button 6 / heavy kick | Menu + Next |
| Player 1 Start | pause menu `Start game`, or Menu + Select |
| Coin 1 | pause menu `Insert coin`, or Menu + Play/Pause |
| Emulator menu | short press Menu |
| Emergency exit | hold Menu for two seconds, or engage Hold |

For two-button games, Select is Attack and Play/Pause is Jump. Previous and
Next remain available for game-specific buttons. The pause menu exposes
explicit coin/start actions, render-skip selection, resume, reset, control
help, and exit. Save states are deferred until their large memory and storage
costs can be measured on hardware.

## Steam/Cover Flow contract

The installer writes the existing eleven-column launcher manifest. Each entry
uses `cps1.rock` as the plugin, its parent archive as the plugin parameter,
`CPS1 Arcade` as the platform inferred from `.zip`, and real game artwork.
The Steam loader reads the manifest once on entry. Cover decode remains an
idle-only cache operation and no CPS1 change owns or shrinks playback memory.

Artwork is sourced from Libretro's MAME `Named_Boxarts` collection and is
staged as bounded RGB565-compatible BMP files. A missing cover excludes that
title from Steam, matching the existing no-placeholder library contract, but
does not remove the ROM.

## Performance and acceptance gates

The CPS1 refresh target is 59.637 Hz, a 16.768 ms frame budget. Instrument
separate counters for 68000, Z80/audio, drawing, scaling, and sleep. Report
one-percent-low speed, audio underruns, rendered frames, and arena high-water
mark after a five-minute run.

Required representatives:

- `sf2.zip`: six-button input and common raster load;
- `ffight.zip`: sprites and two-button controls;
- `1941.zip`: vertical rotation;
- `punisher.zip`: QSound and larger ROM set;
- `slammast.zip`: high memory requirement;
- `varth.zip`: sustained scrolling and vertical mode.

Simulator gates validate ROM lookup, CRCs, parent fallback, core
initialization, both standard and QSound audio paths, manifest parsing, and
rendering bounds. Physical iPod 6G is authoritative for controls and speed.
The runtime writes aggregate emulation, audio-submit, and display tick counts
to `/.rockbox/logs/cps1.log` only during cleanup, avoiding storage I/O in the
frame loop.
Final performance acceptance means no audio underruns after warm-up and at
least 98% emulated speed at stock maximum CPU frequency for each
representative, with adaptive frameskip no worse than one.

The first hardware-validation package is ready to deploy when all
representatives pass the simulator gate, both firmware copies are present in
the package, the private ROM report has zero missing parent archives or
covers, and the database-preservation deploy preflight succeeds. The
full-speed claim remains provisional until the representative set is timed
on the physical iPod.
