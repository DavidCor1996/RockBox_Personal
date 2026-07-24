# PicoDrive Genesis port for iPod 6G+

## Decision

Build an iPod 6G-first, cartridge-only PicoDrive plugin and integrate it with
RockPod Game Sync and Games Cover Flow. The first playable acceptance title is
the user's legally obtained `Sonic The Hedgehog 3 (USA).md`; that ROM remains
outside the repository, build tree, archives, and firmware packages.

This is technically feasible on the 216 MHz ARM926EJ-S iPod 6G. PicoDrive was
designed for ARM handhelds, its ARM build selects the Cyclone 68000 core and
DrZ80 by default, and it already contains ARMv4-compatible render, memory,
YM2612, and mixer paths. The first release must not include Sega CD, 32X, Sega
Pico, Master System, Game Gear, cheats, netplay, rewind, run-ahead, or ZIP
loading. Those features add code, memory pressure, unsupported input devices,
or media/licensing complexity without helping the first hardware gate.

There is a hard distribution constraint. The pinned PicoDrive core uses a
non-commercial MAME-style license, and DrZ80 is explicitly free only for
non-commercial use. CZ80 and the portable Musashi fallback carry similar
restrictions. This conflicts with treating the plugin as an ordinary
GPL-only Rockbox component. Until a source-by-source license review says
otherwise, this port is:

- opt-in and personal-fork-only;
- disabled from the default plugin build and `buildzip.pl` packages;
- excluded from public binaries, releases, and CI artifacts;
- built only after an explicit `PICODRIVE_NONCOMMERCIAL=1` acknowledgement;
- accompanied by the complete pinned and modified source whenever a binary is
  distributed under terms that permit that distribution.

The license gate is phase zero. It is not acceptable to merge a working binary
first and resolve the restriction later.

## Pinned upstream

Use the following immutable inputs:

| Component | Revision | Purpose |
| --- | --- | --- |
| `libretro/picodrive` | `f0d4a0118a9733a1f10bce5a4ac772c474f9300d` | PicoDrive core as audited 2026-07-14 |
| `irixxxx/cyclone68000` | `3ac7cf1bdeecb60e2414980e8dc72ff092f69769` | Generated ARM 68000 interpreter |

The Cyclone revision is the gitlink recorded by the pinned PicoDrive tree. Its
files are dual GPLv2/MAME licensed; select GPLv2 for Cyclone itself. That does
not relicense the rest of PicoDrive or DrZ80. Vendor only the cartridge core
and the exact generated Cyclone output required by the build. Keep upstream
`COPYING`, Cyclone's GPLv2 text, attribution, a file-level provenance manifest,
and the local patch series under `apps/plugins/picodrive/upstream/`.

Do not import the SDL, libretro, CD, 32X, Pico, SMS/GG, CHD, MP3, Ogg, virtual
keyboard, light-gun, mouse, or SH2 dynamic-recompiler frontends. The Rockbox
frontend calls the Pico core API directly.

## Product behavior

Games Cover Flow shows a `Sega Genesis / Mega Drive` console. Entering it lists
synced cartridge games. Selecting a title launches
`/.rockbox/rocks/games/picodrive.rock` with the ROM path as its plugin
parameter. Returning from the emulator restores the same game selection.

The first slice accepts uncompressed `.md`, `.gen`, `.bin`, and `.smd`
cartridges. A `.bin` must pass a Genesis header/reset-vector inspection before
RockPod or the plugin accepts it; merely having that extension is insufficient.
`.32x`, `.cue`, `.chd`, `.iso`, `.zip`, and `.7z` are rejected with a clear
message. The initial ROM size ceiling is 10 MiB and is checked before claiming
the shared buffer.

Target layout:

```text
/.rockbox/rocks/games/picodrive.rock
/.rockbox/games/genesis/roms/<game>.md
/.rockbox/games/genesis/saves/<safe-stem>-<crc32>.srm
/.rockbox/games/genesis/states/<safe-stem>-<crc32>.state0
/.rockbox/games/genesis/config/<safe-stem>-<crc32>.cfg
/.rockbox/games/genesis/games.tsv
/.rockbox/games/library/covers/genesis/<game-stem>.bmp
/.rockbox/games/library/covers/systems/genesis.bmp
```

Writes use a temporary file, `fsync`, and atomic rename. SRAM is flushed after
a dirty debounce, before reset/ROM change, and on every clean exit. A Hold
switch exit must save SRAM but must not silently create or overwrite a save
state. Save states are phase two; SRAM is required in phase one.

## Core and memory design

Create `apps/plugins/picodrive/` with a small Rockbox frontend, a narrow
platform shim, and the vendored cartridge-only upstream subset. Keep the core
API boundary visible so upstream updates can be audited rather than mixed into
frontend code.

Use Cyclone plus DrZ80 for the hardware build. Retain FAME/C or CZ80 only in a
host-side differential test build if the license gate permits it; do not link
multiple CPU cores into the iPod plugin. Generate `Cyclone.s` at import time or
check in the generated file with its generator revision and reproducibility
hash. Assemble it using the existing `arm-elf-eabi` toolchain and the upstream
ARMv4 configuration, then verify the result contains no ARMv6+ opcodes.

Call `plugin_get_audio_buffer()` exactly once after the user has selected a ROM
and before large allocation. Do not call `audio_stop()` first: claiming the
shared buffer owns that transition. Allocate one aligned arena in this order:

| Allocation | Sonic 3 target | Phase-one limit |
| --- | ---: | ---: |
| ROM | 2 MiB | 10 MiB |
| Core RAM, VRAM, Z80 RAM, SRAM and tables | measured, expected under 1 MiB | 2 MiB |
| 320x240 RGB565 framebuffer | 150 KiB | 150 KiB |
| Optional conversion/scratch framebuffer | 0-150 KiB | 150 KiB |
| PCM ring and concealment block | about 64 KiB | 96 KiB |
| State/snapshot scratch | none in phase one | 1 MiB in phase two |

All sizes are runtime-counted and logged in diagnostic builds. Allocation
failure returns cleanly before core initialization. ROM reads, cover decoding,
and save-state compression never occur from the PCM callback.

The plugin binary and small fixed data remain in the normal plugin buffer. ROM
and mutable emulation data live in the claimed shared buffer. The frontend
must not assume that a 3 MiB plugin buffer can hold Sonic 3 plus the emulator.

## Video and timing

Configure the upstream renderer with `PicoDrawSetOutFormat(PDF_RGB555, 0)`
without `USE_BGR555` or `USE_BGR565`; despite the historical enum name, that
configuration produces the RGB565 order used by this target. Point
`PicoDrawSetOutBuf()` at a 320-pixel-stride arena framebuffer.

- 320x224 output is copied one-to-one and vertically centered with eight black
  lines above and below.
- 256x224 output defaults to aspect-correct horizontal scaling into 320x224;
  a `Native 256` option centers it without scaling.
- 240-line modes are centered or clipped only according to the core's reported
  active area; never stretch 224 lines to 240 by default.
- NTSC and PAL timing follow the core's detected region. The frontend schedules
  approximately 59.92 Hz or 49.70 Hz and never changes emulated game speed to
  hide an overrun.

The default performance preset is `Auto frameskip`, maximum two consecutive
presentation skips, with emulation and audio continuing every frame through
`PicoIn.skipFrame`. Also expose `Off`, `1`, `2`, and `3`. Boost the CPU only
while a ROM is running and restore the previous boost state on every exit.

Do not add filters or overlays in phase one. Instrument frame execution,
render, RGB copy, PCM generation, wait time, skipped frames, and audio
underruns. The Sonic 3 hardware target is median frame time at or below the
region budget, 99th percentile no worse than 1.25x budget over a five-minute
run, and fewer than one audible underrun per ten minutes after warm-up.

## Click-wheel controls

Use wheel position as a four-way D-pad with diagonals; do not require hard
clicks for direction. The default three-button layout is:

| iPod control | Genesis action |
| --- | --- |
| Wheel touch position | D-pad, including diagonals |
| Select | B |
| Play/Pause | C |
| Previous/Left hard press | A |
| Menu short press | Start |
| Menu hold | Emulator menu |
| Hold switch engaged | Save SRAM, stop safely, return to Cover Flow |

For six-button games, the `Six Button` profile maps Select/Play/Previous to
A/B/C. Holding Next/Right changes those three buttons to X/Y/Z; Menu remains
Start. The emulator menu offers resume, reset with confirmation, control
profile, video mode, frameskip, sound, save state (phase two), load state
(phase two), and exit. Chord detection delays Menu/Start only long enough to
distinguish a hold and never emits Start when opening the menu.

Sonic 3 accepts A, B, or C for jump, so the default mapping is fully playable
without the six-button layer. Controls and menu actions must use the existing
Rockbox haptic preference rather than forcing vibration.

## Audio lifecycle

This port must follow `docs/plugin-audio-lifecycle-steering.md`.

Enable YM2612, SN76496 PSG, Z80, and stereo. Start with 32 kHz stereo signed
16-bit PCM; offer 22.05 kHz only as an economy option. PicoDrive writes one
frame of PCM through `PicoIn.sndOut`/`PicoIn.writeSound`; the callback enqueues
into a bounded single-producer/single-consumer ring. Rockbox playback uses
`PCM_MIXER_CHAN_PLAYBACK`, not a private codec path.

Startup order:

1. Save the previous mixer frequency and relevant plugin state.
2. Claim the shared buffer with `plugin_get_audio_buffer()`; do not pre-call
   `audio_stop()`.
3. Set the intended mixer frequency, stop stale playback-channel state, and
   initialize the core and ring.
4. Prime at least four frame blocks, then start the playback mixer channel with
   a short fade-in.

The PCM consumer performs no allocation, file I/O, logging, core calls, or UI
work. On underrun it supplies a bounded fade-to-zero concealment block and
increments a counter. Pausing stops or drains the playback channel without
discarding SRAM state.

Exit order is strict: stop core production, fade out, stop the playback mixer
channel, wait until no callback can reference the arena, save dirty SRAM,
destroy the core, restore the previous mixer rate/state and CPU boost, then
release/return from the plugin. Never mutate the user's playlist. Validate the
fresh-boot, music-to-emulator, emulator-to-Database, emulator-to-Files, rapid
switching, volume, pause, menu-exit, Hold-exit, and USB transition matrix on
physical hardware.

## RockPod Game Sync

Extend `rockpod/services/rockbox_games.py` as a system, not as an SMSGG alias:

- `GENESIS_ROM_EXTENSIONS = {".md", ".gen", ".bin", ".smd"}`;
- `GENESIS_ROM_TARGET_DIR = ".rockbox/games/genesis/roms"`;
- save/state/config target constants matching the layout above;
- `GENESIS_PLUGIN_PATH = ".rockbox/rocks/games/picodrive.rock"`;
- `SYSTEM_MANIFEST_RELATIVE_PATHS["genesis"] =
  ".rockbox/games/genesis/games.tsv"`;
- Genesis platform labels, scan roots, save inventories, backup/restore pairs,
  remove bundles, and launcher index routing;
- a Genesis header inspector returning internal title, product code, region,
  SRAM range, ROM size, CRC32, and validation status;
- a Libretro box-art base for `Sega - Mega Drive - Genesis`.

The library scanner accepts `.bin` only when inspection identifies a Genesis
cartridge. Sync preserves unrelated games and existing manifests. A ROM asset
is copied to the Genesis ROM directory, its real cover is rendered to a
140x124 RGB BMP in the central Genesis cover directory, and both the global
`games.tsv` and Genesis `games.tsv` are merged atomically. The global launcher
row uses the plugin as `rom_path` and the actual ROM as `plugin_param`.

RockPod save backup/restore handles `.srm` and phase-two `.state0` files by the
same safe-stem-plus-CRC identity used by the plugin. Removing a ROM must not
remove saves unless the user explicitly selects a separate save removal
action. Sync to a simulator and sync to a device use the same manifest schema.

Add service and UI tests for discovery, `.bin` rejection, sync, resync,
metadata preservation, cover conversion, manifest merge, remove, missing
source, simulator paths, save backup/restore, and two same-named ROMs with
different CRCs.

## Authentic Cover Flow artwork

No hand-drawn, generated, placeholder, or emulator-logo art satisfies the
acceptance gate for Sonic 3. The game tile must display authentic North
American retail box art. RockPod should resolve the owned ROM name
`Sonic The Hedgehog 3 (USA)` to the existing Libretro thumbnail named exactly
`Sonic The Hedgehog 3 (USA).png`, cache it outside the source tree, and convert
that image to the normal letterboxed 140x124 BMP during sync.

The search order is:

1. a same-basename user image beside the ROM (`.png`, `.jpg`, `.jpeg`, or
   `.webp`);
2. an already cached exact match;
3. the user-invoked Libretro thumbnail lookup;
4. system art only as a visible fallback.

There is no silent network fetch during device sync. Show the source URL and a
preview before the user accepts fetched artwork. Record URL, retrieval time,
source image SHA-256, and generated BMP SHA-256 in RockPod's local metadata.
Commercial box art is user-local personal media: do not commit it to
`assets/`, copy it into source releases, or include it in default firmware
ZIPs.

The Genesis system tile at
`/.rockbox/games/library/covers/systems/genesis.bmp` must likewise be derived
from an authentic Sega Genesis/Mega Drive wordmark or product photograph, with
its source recorded. It may be shipped only if its redistribution status is
reviewed and cleared; otherwise RockPod installs it as a user-local asset. A
small compiled `game_system_genesis.50x36x24.bmp`, if added, must be a resized
derivative of the same provenance-recorded authentic source—not a newly drawn
icon.

Add `genesis` to the launcher's built-in directories, supported extensions,
system list, missing-system message, cover fallback, save indicator, and plugin
dispatch. The launcher must prefer each row's real cover over the system tile.
The acceptance screenshot must visibly show the Sonic 3 retail box, title,
favorite/save state, and adjacent Cover Flow positions at 320x240.

## Owned Sonic 3 test fixture

The inspected local test input is:

```text
/home/david/Downloads/Sonic The Hedgehog 3 (USA).md
```

Observed properties on 2026-07-14:

| Property | Value |
| --- | --- |
| Size | 2,097,152 bytes |
| Console header | `SEGA GENESIS` |
| Domestic/international title | `SONIC THE HEDGEHOG 3` |
| Product code | `GM MK-1079 -00` |
| Header date | `(C)SEGA 1993.NOV` |
| Region | `U` / USA |
| SRAM declaration | `RA`, `0x200000-0x203fff` |

Do not hard-code or publish an allowlist that implies the project supplies this
ROM. A local test helper may take this path through an environment variable,
validate the properties above, and stage a copy only inside an ignored
simulator simdisk or explicitly selected mounted device. Logs include metadata
and hashes but never ROM bytes. Tests skip with a clear message when the file
is absent.

First-title functional gates:

- Sega and Sonic title screens render with correct color and no cropped HUD;
- a new game reaches Angel Island and accepts diagonal movement and jump;
- music, PSG effects, stereo panning, and volume changes are audible without
  sustained crackle;
- pause/Start and emulator-menu differentiation work;
- a game is saved, the plugin exits, music playback is started, the plugin is
  relaunched, and SRAM progress is recovered;
- Cover Flow shows authentic Sonic 3 retail box art, not system fallback art;
- the ROM and fetched source PNG are absent from `git status`, build ZIP file
  lists, and public artifact manifests.

## Delivery phases

### Phase 0: provenance and build proof

- Complete the file-level license matrix and keep PicoDrive opt-in.
- Import the pinned minimal source and reproduce Cyclone output.
- Build simulator and ARM overlay/plugin stubs with no ROM data.
- Add a source audit that fails if CD, 32X, libretro, SDL, or unapproved media
  decoder objects enter the link.

### Phase 1: Sonic 3 vertical slice

- Load uncompressed Genesis cartridges from the shared arena.
- Implement RGB565 video, three-button input, 32 kHz mixer audio, SRAM, clean
  exit, diagnostics, and Sonic 3 simulator tests.
- Add RockPod Genesis discovery/sync/save handling and both launcher manifests.
- Add user-local authentic box-art lookup/conversion and Genesis Cover Flow.

### Phase 2: compatibility and polish

- Add per-game configuration and one save-state slot.
- Add six-button input, PAL validation, `.smd` deinterleave tests, 256-wide
  display choices, CRC collision tests, and broader cartridge mapper/EEPROM
  coverage.
- Profile on iPod 6G hardware and selectively enable only audited ARM assembly
  paths that improve measured bottlenecks.

### Deferred

Sega CD, 32X, Pico, 8-bit Sega systems, compressed ROMs, lock-on cartridge
composition, SVP, cheats, rewind, run-ahead, multiplayer adapters, mouse/light
gun, and online services remain separate proposals. Sonic & Knuckles lock-on
support must not be inferred from Sonic 3 support.

## Completion gates

The port is complete for the requested slice only when all of the following
are true:

- the license/provenance gate permits the exact local use and packaging mode;
- clean simulator and iPod 6G builds succeed with the pinned source;
- RockPod sync/resync/remove and save backup/restore tests pass;
- Games Cover Flow launches the synced ROM and returns to the same selection;
- authentic user-local Sonic 3 box art is displayed and provenance recorded;
- the Sonic 3 functional, SRAM, timing, and ten-minute audio gates pass;
- the full physical audio transition matrix passes;
- teardown shows no callback-after-free, buffer leak, stale mixer rate, stuck
  CPU boost, playlist mutation, or save corruption;
- `git status` and package inspection prove the ROM and commercial cover source
  image were not added to the repository or default distribution.

## Research sources

- PicoDrive repository and pinned revision:
  <https://github.com/libretro/picodrive/tree/f0d4a0118a9733a1f10bce5a4ac772c474f9300d>
- PicoDrive license:
  <https://github.com/libretro/picodrive/blob/f0d4a0118a9733a1f10bce5a4ac772c474f9300d/COPYING>
- Cyclone pinned revision and dual-license files:
  <https://github.com/irixxxx/cyclone68000/tree/3ac7cf1bdeecb60e2414980e8dc72ff092f69769>
- Libretro Genesis box-art index:
  <https://thumbnails.libretro.com/Sega%20-%20Mega%20Drive%20-%20Genesis/Named_Boxarts/>
- Libretro Sonic 3 database entry:
  <https://db.libretro.com/Sega%20-%20Mega%20Drive%20-%20Genesis/Sonic%20The%20Hedgehog%203%20%28USA%29.html>
