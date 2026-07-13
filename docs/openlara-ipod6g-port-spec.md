# OpenLara iPod 6G Port Specification

## Implemented milestone (2026-07-13)

The first playable milestone described below is implemented. Both the 64-bit
iPod simulator and ARM iPod 6G builds produce `openlara.rock`; the simulator
gate has loaded and rendered Caves for 180 frames with audio enabled and has
also passed the no-title/no-music fallback. Games cover flow registers the
engine as PlayStation and launches one `Tomb Raider` `.olr` descriptor.

The milestone remains intentionally limited to the fixed engine's upstream
title, Lara's Home, Caves, and City of Vilcabamba table. The supplied US Rev-6
PlayStation disc was validated as Mode-2/2352 and contains the complete PSX
level set, but upstream's fixed packer accepts PC `.PHD`, not retail `.PSX`.
Full-campaign PSX conversion therefore remains deferred exactly as described
in the decision and later-level audit sections below.

## Decision

Port OpenLara as an `ipod6g`-only Rockbox C++ plugin, starting from
OpenLara's `src/fixed` engine and portable GBA renderer. Do not begin with the
desktop OpenGL/SDL engine.

This is a viable proof-of-concept port, but it is not yet reasonable to promise
the complete Tomb Raider campaign. The fixed engine is a deliberately reduced
branch: its current level table enables the title, Lara's Home, Caves, and City
of Vilcabamba, while later levels are commented out. The first go/no-go gate is
therefore a playable Caves build at acceptable speed on a physical iPod.

Initial scope:

- iPod Classic 6G/7G (`ipod6g`), 64 MB RAM, 320x240 RGB565 display.
- User-supplied Tomb Raider I PC data converted on the host to OpenLara's
  fixed-engine `.PKD` format.
- Fixed-point software rendering with no OpenGL, SDL, or floating-point game
  path.
- Title screen, Lara's Home, and Caves for the first playable milestone.
- Save/settings support, click-wheel controls, clean USB exit, and simulator
  automation.
- Sound effects after silent playability is proven; music follows only after
  the Rockbox PCM lifecycle is stable.

Deferred until the first physical performance gate passes:

- City of Vilcabamba and additional levels.
- Full-campaign compatibility.
- FMV playback, cutscenes, CD audio parity, and expansion content.
- TR2/TR3 content, multiplayer, high-resolution rendering, OpenGL effects, and
  desktop OpenLara feature parity.

## Research Baseline

Pin the first import to official OpenLara commit
`8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea` (2026-07-08). OpenLara is
BSD-2-Clause licensed. Preserve its `LICENSE` as
`apps/plugins/openlara/LICENSE.upstream` and record every imported file and
local patch in `apps/plugins/openlara/UPSTREAM.md`.

The relevant upstream evidence is:

- `src/fixed/` is a separate fixed-point engine using a 14-bit fixed-point
  representation and packed level data.
- `src/platform/gba/` runs that engine at 240x160 with an indexed software
  renderer and 10,512 Hz 8-bit audio.
- `src/platform/32x/` runs the fixed engine at 320x224.
- `src/platform/tns/` builds the fixed engine for ARMv5TE at 320x240 and reuses
  the portable GBA renderer. This is the closest CPU/display precedent for the
  Rockbox port, although its sound and save hooks are unfinished.
- The fixed engine uses placement `new` for in-place item construction and has
  no general-purpose heap dependency in its core. Dynamic allocation in the
  existing platform frontends is primarily for loading level data.
- The GBA packer converts user-owned TR1 PC `.PHD` levels into `.PKD`. The
  official source snapshot contains representative packed sizes of 313,700
  bytes for the title, 1,465,004 for Lara's Home, 2,356,992 for Caves, and
  2,616,756 for City of Vilcabamba. `TRACKS.AD4` is about 3.0 MB.

The important limitation is equally explicit: `src/fixed/common.cpp` currently
enables only four TR1 entries, and `src/fixed/game.h` contains alpha-era level
progression handling. Full-game support is a later engineering project, not an
assumption of the initial port.

Official upstream references:

- [OpenLara repository](https://github.com/XProger/OpenLara)
- [Pinned source revision](https://github.com/XProger/OpenLara/tree/8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea)
- [BSD-2-Clause license](https://github.com/XProger/OpenLara/blob/8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea/LICENSE)
- [Fixed engine configuration](https://github.com/XProger/OpenLara/blob/8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea/src/fixed/common.h)
- [Fixed level table](https://github.com/XProger/OpenLara/blob/8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea/src/fixed/common.cpp)
- [GBA platform and packer](https://github.com/XProger/OpenLara/tree/8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea/src/platform/gba)
- [ARMv5TE TI-Nspire platform](https://github.com/XProger/OpenLara/tree/8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea/src/platform/tns)

## Local Implementations To Reuse

Use these local plugins as implementation references rather than creating a
new frontend architecture:

| Concern | Primary local reference | Pattern to reuse |
| --- | --- | --- |
| C++ plugin build | `apps/plugins/flashplayer/flashplayer.make`, `apps/plugins/scummvm/scummvm.make`, `apps/plugins/cxxprobe.cpp` | `PLUGIN_CXXFLAGS`, no exceptions/RTTI, `extern "C" plugin_start`, localized C++ rules |
| Large runtime arena | `apps/plugins/snes_lite/snes_lite.c`, `snes_lite_frontend.c`, `apps/plugins/smsgg/smsgg_platform.c` | Claim the shared audio buffer once, align it, use bounded arena allocation, fail before partial startup |
| RGB565 framebuffer | `apps/plugins/snes_lite/snes_lite_video.c`, `apps/plugins/smsgg/smsgg_video.c` | Direct framebuffer access, explicit conversion/scaling, simulator frame dumps, renderer self-test |
| Click-wheel controls | `apps/plugins/snes_lite/snes_lite_input.c`, `apps/plugins/smsgg/smsgg_input.c` | Absolute wheel zones, held-button chords, hold-switch protection, USB detection |
| Saves/config | `apps/plugins/snes_lite/snes_lite_saves.c`, `snes_lite_config.c`, `apps/plugins/smsgg/smsgg_state.c` | Versioned files, complete-write checks, per-game directories, preserve saves during removal |
| PCM output | `apps/plugins/snes_lite/snes_lite_audio.c`, `apps/plugins/mpegplayer/pcm_output.c` | Playback mixer channel, queued blocks, callback-safe stop, previous frequency restoration |
| Plugin lifecycle | `apps/plugins/smsgg/smsgg.c`, `apps/plugins/snes_lite/snes_lite.c` | Single cleanup path, save before core unload, audio shutdown before arena release, USB status propagation |
| RockPod deployment | `rockpod/services/rockbox_games.py`, `tools/snes_lite_rockpod_gate.py` | Scoped sync plans, mock device first, generated manifests, checksum verification, separate removal action |
| Simulator automation | `tools/snes_lite_sim_gate.py` | Isolated simdisk, Open Plugin launch record, fixed frame count, deterministic log and frame capture |
| Haptics | `apps/plugins/smsgg/smsgg_haptics.c` | Respect global haptic state and use the plugin API instead of target-private hardware calls |

Do not use the dormant SDL or Quake ports as lifecycle references. Their age
and direct platform assumptions make them useful only for isolated algorithms.

## Repository Shape

The port should land in an isolated subtree:

```text
apps/plugins/openlara/
    SOURCES
    openlara.make
    openlara.cpp
    openlara.h
    openlara_platform.cpp
    openlara_video.cpp
    openlara_input.cpp
    openlara_files.cpp
    openlara_save.cpp
    openlara_audio.cpp
    openlara_menu.cpp
    LICENSE.upstream
    UPSTREAM.md
    fixed/
        selected upstream fixed-engine files
    renderer/
        portable renderer derived from platform/gba/render.iwram.cpp
```

Host-only conversion belongs outside firmware code:

```text
tools/openlara_pack/
    README.md
    manifest.json
    selected upstream packer sources
    rockpod wrapper/tests
```

Do not import upstream platform assets, sample levels, title screens, audio, or
other Tomb Raider data into the Rockbox tree. The repository and firmware ZIP
must remain engine-only.

## Build Contract

Add `openlara` to `apps/plugins/SUBDIRS` only when `PLUGIN_CXX_AVAILABLE` and
the target is `IPOD_6G` or the iPod 6G simulator. The first port is intentionally
not enabled on every Rockbox target.

Compile the fixed core and frontend with the existing plugin C++ toolchain:

```text
$(PLUGIN_CXXFLAGS)
-std=gnu++03
-O3
-fno-exceptions
-fno-rtti
-fno-threadsafe-statics
-fno-use-cxa-atexit
-fno-strict-aliasing
-fomit-frame-pointer
```

Add `-flto` on hardware only after a non-LTO build is stable and map-file size
is recorded. Do not import an independent C++ standard library. Wrap the
Rockbox API in a small platform layer and use Rockbox string, memory, file, time,
LCD, button, mixer, and logging calls.

Introduce an upstream platform define such as `__ROCKBOX__` in the imported
`fixed/common.h` compatibility patch. Its initial configuration is:

```text
USE_DIV_TABLE
MODE13
FRAME_WIDTH  320
FRAME_HEIGHT 240
USE_FMT      LVL_FMT_PKD
SND_CHANNELS 6
```

Do not enable `USE_ASM` initially. The GBA ARM assembly assumes a 240-pixel
framebuffer and GBA memory/DMA registers. First make the portable C renderer
correct, profile it, then add narrowly verified ARMv6 routines if needed.

Build gates:

- `openlara.rock` fits inside the 3 MiB `ipod6g` plugin region with at least
  128 KiB of `plugin_get_buffer()` headroom.
- No unresolved C++ runtime, libc, libm, SDL, or OpenGL symbol remains.
- Both `build-sim-ipod6g` and `build-hw-ipod6g` produce the plugin.
- `make zip` contains only the engine/plugin, license, and permitted default
  configuration; no game data.

## Runtime Memory Plan

The 3 MiB plugin region holds executable code, read-only tables, and fixed
global state. Level/media data comes from the shared audio buffer.

Call `plugin_get_audio_buffer()` exactly once during startup and let it perform
the playback ownership transfer. Never call `audio_stop()` first. On this
64 MB `ipod6g` build the shared region is substantially larger than the 3 MiB
plugin image region, but the plugin must inspect the returned size and never
assume a constant address or size.

Use a 16-byte-aligned arena with marks:

```text
arena base
  persistent frontend state
  320x240 indexed render buffer                 75 KiB
  256-entry RGB565 palette                     512 B
  optional RGB565 staging buffer               150 KiB
  face/vertex/order-table renderer work        measured at build time
  audio block ring                             <= 64 KiB initially
  current level .PKD                           file size, 16-byte aligned
  optional TRACKS.AD4                          file size, or streamed later
  temporary loading/checksum workspace
arena end
```

The initial minimum accepted shared arena is 8 MiB. Log the actual size, every
partition, high-water mark, level size, and remaining bytes. Fail with a clear
message before `gameInit()` if the arena is too small.

Use one-level-at-a-time loading. On level transition:

1. Stop OpenLara's logical sounds for the old level.
2. Stop or pause the Rockbox audio queue so no callback references level data.
3. Run fixed-engine level teardown.
4. Rewind to the level arena mark.
5. Read and validate the complete `.PKD` into aligned arena memory.
6. Initialize the new level and its sound offsets.
7. Reset and resume the audio queue.

Do not use pointers returned by movable core allocations across `yield()` or
disk I/O. Do not place live callback buffers in memory that can be rewound.

## Data And Legal Boundary

The user must supply a legally obtained Tomb Raider I PC data directory.
RockPod may inspect and convert it locally; it must not download, bundle, or
redistribute game data.

Recommended on-device layout:

```text
/.rockbox/games/openlara/
    manifest.txt
    levels/
        TITLE.PKD
        GYM.PKD
        LEVEL1.PKD
    screens/
        TITLE.SCR
    audio/
        TRACKS.AD4             # optional until music milestone
    saves/
        slot0.sav
    openlara.cfg
```

The host packer wrapper must:

- accept a user-selected TR1 installation directory;
- identify required source `.PHD` files case-insensitively;
- convert only locally available files;
- never fall back to downloading missing data;
- write to a staging directory, not directly to a mounted iPod;
- record source basename, source SHA-256, packed SHA-256, packed size, format
  version, packer commit, and enabled level ID in `manifest.txt`;
- validate every embedded pointer/offset, count, and file boundary before sync;
- produce deterministic output from identical input;
- reject partial or incompatible conversions with a per-file explanation.

The upstream packer currently assumes a development data layout and a broad
level set. Refactor its entry point into a per-level conversion API instead of
copying its current `main()` unchanged. The conversion tool may be modern host
C++; none of its desktop image or filesystem dependencies belong in the plugin.

## Rendering Design

Use the fixed engine's indexed software renderer, initially at 320x240. It
already supports the target logical resolution through the TI-Nspire path, but
its renderer must be separated from GBA DMA and VRAM assumptions.

Rockbox backend requirements:

- Define `fb` as a 320x240 byte index buffer, not a fake GBA VRAM address.
- Replace `dmaCopy`/`dmaFill` with bounded Rockbox memory operations.
- Preserve OpenLara's 256-entry palette and lightmap behavior.
- Convert index rows to native RGB565 directly into the main framebuffer.
- Convert only dirty/full rows generated for the current frame; avoid a second
  full copy when profiling proves direct conversion is safe.
- Update the LCD once per presented frame.
- Clear and redraw the whole plugin viewport so no Rockbox theme or previous
  framebuffer can flash through during launch, pause, USB, or exit.

Performance modes:

| Mode | Internal resolution | Presentation | Purpose |
| --- | --- | --- | --- |
| Native | 320x240 | 1:1 RGB565 | Quality target and final default if fast enough |
| Fast | 240x160 | centered or integer-aware 320x213 with black bars | First fallback; matches GBA-tested geometry |
| Diagnostic | 160x120 | 2x nearest-neighbor | Profiling only, not the promised final experience |

Do not silently distort the aspect ratio. The default may become Fast only if
the native mode cannot sustain the acceptance floor on hardware.

Frame timing uses a monotonic fixed-step accumulator. Cap catch-up at the
upstream `MAX_UPDATE_FRAMES`, never speed the simulation up to compensate for
dropped draws, and separate update FPS from presented FPS in diagnostics.

Initial performance gates on physical 6G:

- Caves gameplay median update rate at least 20 Hz for five minutes.
- 1% low update rate at least 15 Hz.
- No input sample is held longer than 100 ms because of rendering or disk I/O.
- No visible palette tearing or previous-screen flash.
- If 320x240 misses the gate, 240x160 must pass before the port proceeds.

These are go/no-go prototype targets, not claims about current performance.

## Click-Wheel Controls

The fixed GBA control model maps unusually well to the iPod. Use it directly
rather than inventing an on-screen virtual controller:

| iPod input | OpenLara key | Game action |
| --- | --- | --- |
| Wheel touch at top/right/bottom/left | `IK_UP/RIGHT/DOWN/LEFT` | Run/turn/backstep |
| Wheel diagonal zones | paired direction bits | Diagonal input where the core accepts it |
| Centre | `IK_A` | Action |
| Play/Pause | `IK_B` | Jump |
| Previous | `IK_L` | Modifier |
| Next | `IK_R` | Walk; with Previous, Look |
| Previous + Centre | `IK_L + IK_A` | Draw/holster weapon; Action while weapon is busy |
| Previous + Play | `IK_L + IK_B` | Roll |
| Menu short release | `IK_SELECT` pulse | Inventory/back |
| Menu hold for 700 ms | frontend only | Open plugin menu; do not also send `IK_SELECT` |
| Hold switch newly engaged | frontend only | Pause, autosave if valid, then quit to Games |

Use eight wheel zones with hysteresis and a dead state when the wheel is not
touched. Direction comes from absolute `wheel_status()`, not scroll events.
Sample held state every update, but edge-trigger Menu, inventory, save, and
frontend commands. If launched while the hold switch is already engaged, do
not immediately exit; arm exit only after it is released once, following SNES
Lite.

The plugin menu contains Resume, Save Game, Load Game, Display Mode, Sound,
Haptics, Show FPS, Restart Level, Controls, and Quit to Games. Pause audio and
drop CPU boost while the menu is visible. Clear queued input before resuming.

`SYS_USB_CONNECTED` must stop the game through the same cleanup path and return
`PLUGIN_USB_CONNECTED`. It must never wait for another button event after USB
is detected.

## Save And Configuration Contract

Use versioned, checksum-protected files. Never write directly over the last
known-good save:

1. Write `slot0.tmp` completely.
2. Close and reopen it for size/header/checksum validation.
3. Rename the valid file to `slot0.sav`, preserving the old save until the new
   file is known good.

Store at minimum:

- fixed-engine save version;
- upstream import commit;
- packed level manifest hash;
- level ID and data size;
- payload CRC32;
- timestamp for UI display only.

Reject incompatible saves without mutating them. Saving is disabled during a
level load, title transition, cutscene transition, or while player state is not
initialized.

Configuration is stored in `/.rockbox/games/openlara/openlara.cfg` and includes
display mode, frameskip policy, sound, music, haptics, haptic strength, gamma,
FPS overlay, and Menu-hold duration. The global Rockbox haptic quick setting is
authoritative: plugin haptics run only when both the plugin option and
`rb->haptic_feedback_enabled()` are true.

Map upstream `osJoyVibrate()` to rate-limited `rb->haptic_feedback()` pulses;
never access target haptic hardware directly.

## Audio Lifecycle

Audio implementation must follow `docs/plugin-audio-lifecycle-steering.md`.
The safe milestone order is:

1. Silent renderer/gameplay.
2. Six-channel sound effects at 22,050 Hz, mixed to signed 16-bit stereo.
3. Music from `TRACKS.AD4` only after sound-effect transitions are stable.

Do not copy the GBA 8-bit DMA frontend. Reuse its sample/ADPCM decoding logic,
but write into a Rockbox block queue modeled on SNES Lite and MPEGPlayer.

Required startup sequence:

1. Capture only the mixer frequency/state that the plugin can restore.
2. Call `plugin_get_audio_buffer()`; do not pre-call `audio_stop()`.
3. Partition the arena and initialize the core while PCM callbacks are absent.
4. Stop stale `PCM_MIXER_CHAN_PLAYBACK` state.
5. Set the requested mixer frequency and playback source.
6. Fill a startup watermark, set unity amplitude, and start
   `PCM_MIXER_CHAN_PLAYBACK`.

Required shutdown sequence:

1. Mark the audio producer stopped so no new blocks are submitted.
2. Under the PCM lock, stop `PCM_MIXER_CHAN_PLAYBACK` and remove callback
   reachability into plugin memory.
3. Clear queue state and wait briefly for direct PCM to be idle if applicable.
4. Undo low-latency/fade state and restore the previous mixer frequency and
   source.
5. Tear down the OpenLara core and level data.
6. Call `plugin_release_audio_buffer()` last.

The plugin must never create, clear, or replace the user's playlist. Plugin
music is local PCM only. Volume uses the normal Rockbox volume path and should
not implement a second hardware volume controller.

## Lifecycle And Failure Handling

Use a single state machine and one cleanup label:

```text
entered
  -> display claimed/cleared
  -> shared arena claimed
  -> manifest and assets validated
  -> renderer initialized
  -> level loaded
  -> core running
  -> audio running (optional)
  -> paused/menu
  -> audio quiesced
  -> save/core teardown
  -> arena released
  -> Rockbox UI restored
```

Every partial startup failure enters cleanup with explicit flags for arena,
level, renderer, audio, CPU boost, backlight, font, and log ownership. Cleanup
must be safe when any subset was initialized.

Create `/.rockbox/logs/openlara.log` with:

- upstream commit and local build ID;
- plugin and shared arena sizes;
- selected level, asset hashes, and load time;
- renderer mode, update FPS, present FPS, render time, and LCD time;
- input zone and missed-update counters when diagnostic input is enabled;
- audio requested/actual rate, queue depth, underruns, and dropped blocks;
- save/load result;
- USB and exit reason;
- cleanup flags and final plugin status.

Log errors; do not log every frame in normal mode.

## RockPod Integration

Treat OpenLara as a native game, not a console ROM type. Add a Tomb Raider I
data source workflow to RockPod only after the standalone converter passes its
tests.

RockPod flow:

1. User selects a local Tomb Raider I PC installation directory.
2. RockPod reports detected/missing source files and their hashes.
3. User explicitly starts local conversion.
4. RockPod stages `.PKD`, screen, optional audio, manifest, and cover files.
5. RockPod shows a scoped simulator or device sync diff.
6. Mock-device/simulator apply runs first.
7. Physical sync copies only changed OpenLara assets and plugin files.
8. Removal is explicit and preserves `saves/` unless the user separately asks
   to remove saves.

The Games launcher entry should be `Tomb Raider I` under Native Games and invoke
`/.rockbox/rocks/games/openlara.rock`. Use a user-supplied cover or a neutral
engine-generated fallback; do not scrape or embed copyrighted box art by
default.

Add RockPod tests for case-insensitive source discovery, missing files,
deterministic packing, malformed pack rejection, sync idempotence, mock-device
paths, checksum verification, separate save removal, and preservation of
unrelated files.

## Milestones And Acceptance Gates

### M0: Imported core compiles

- Import pinned fixed-engine sources, license, and patch ledger.
- Build a C++ `openlara.rock` for simulator and hardware.
- Initialize/tear down the shared arena 100 times in simulator.
- Render a deterministic palette/test polygon and dump a PPM.
- No game data is required or included.

Gate: hardware and simulator builds pass; plugin image/headroom contract passes.

### M1: Packed data validation

- Build the host packer wrapper for `TITLE`, `GYM`, and `LEVEL1`.
- Add strict `.PKD` bounds validation before pointer fixups.
- Load/unload each converted level repeatedly without rendering.
- Add known-bad truncation, count-overflow, and offset-overflow fixtures.

Gate: AddressSanitizer/UndefinedBehaviorSanitizer host validation passes and
identical inputs produce identical outputs.

### M2: Visible static Caves frame

- Initialize the portable renderer.
- Render Caves at 320x240 and 240x160.
- Add deterministic simulator frame capture and pixel checksum.
- Verify no old Rockbox theme/frame flashes on launch, pause, USB, or exit.

Gate: reference screenshots are visually correct and conversion self-tests pass.

### M3: Playable silent Caves

- Implement the click-wheel mapping, inventory, plugin menu, save/load, USB
  handling, pacing, CPU boost, and diagnostics.
- Run a 10-minute scripted simulator path and a five-minute physical session.

Gate: the physical performance floor is met with no crash, stuck input, save
corruption, or UI bleed. If neither 320x240 nor 240x160 passes, stop the port
before audio work.

### M4: Sound effects

- Add signed 16-bit stereo SFX through the playback mixer channel.
- Exercise pause, save/load, restart, level transition, USB, and rapid exit.
- Complete the required Database/Files/plugin audio transition matrix.

Gate: no underrun burst, freeze, muted post-plugin playback, playlist mutation,
or callback-after-free occurs on physical hardware.

### M5: Title, Lara's Home, progression, music

- Add the title screen and Lara's Home.
- Validate City of Vilcabamba separately before enabling progression.
- Add optional music and its transition tests.
- Add RockPod conversion/sync UI and Native Games launcher entry.

Gate: three consecutive cold-boot runs can start, save, load, transition,
return to Games, and resume Database/Files music.

### M6: Campaign expansion investigation

- Audit every commented fixed-engine level and unsupported object/enemy path.
- Convert and validate one later level at a time.
- Maintain a compatibility table with blockers and physical performance.

Gate: enable no level by default until its geometry, objects, enemies, secrets,
save/load, audio, and transition behavior all pass. Full-campaign support is a
separate deliverable.

## Regression Matrix

Simulator checks:

- cold launch with no assets gives one clear error and clean exit;
- corrupt manifest, truncated `.PKD`, invalid offsets, and wrong save version
  fail without a crash;
- 100 load/unload cycles leave the arena high-water mark unchanged;
- deterministic fixed-frame capture matches its checksum;
- menu/inventory distinction and every wheel chord are scripted;
- USB event exits without waiting;
- silent and audio-enabled cleanup logs show the correct order;
- full Rockbox simulator remains responsive after plugin exit.

Physical iPod 6G checks:

- fresh boot -> OpenLara -> Games;
- Database music -> OpenLara -> Database music with working sound;
- Files music -> OpenLara -> Files music with working sound;
- rapid Games/OpenLara/Database switching ten times;
- Menu inventory tap versus Menu plugin-menu hold;
- hold-switch exit, including launch while already locked;
- save, reboot, load, and corrupt-save preservation;
- USB connection during gameplay, inventory, load, save, and audio startup;
- five-minute native and Fast-mode performance captures;
- volume changes during SFX/music without static or permanent mute;
- no old theme, clock, wallpaper, or framebuffer flash at any transition.

Do not deploy an OpenLara build to physical hardware until M0-M3 simulator
gates pass. Audio builds additionally require the lifecycle regression matrix.

## First Implementation Slice

The first coding pass should be deliberately small:

1. Add the pinned license/source ledger and C++ build skeleton.
2. Add `__ROCKBOX__` fixed-core configuration and compile only the fixed core,
   portable renderer, and silent platform stubs.
3. Claim/release the shared arena with diagnostics and a single cleanup path.
4. Render a palette/test frame to RGB565 in simulator.
5. Build the host per-level packer and validator for a user-supplied `LEVEL1.PHD`.
6. Load Caves and render one deterministic frame.

Do not start with audio, RockPod UI, the full campaign, or ARM assembly. Those
features depend on proving that the fixed engine, packed asset path, and portable
renderer are correct and fast enough on the iPod first.
