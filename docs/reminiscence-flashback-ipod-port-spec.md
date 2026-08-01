# REminiscence / Flashback Rockbox iPod Port Specification

## Decision

Port REminiscence as a native Rockbox C++ game plugin. The first supported
target is iPod Classic 6G/7G (`ipod6g`) and its simulator. Start with original
DOS floppy or DOS CD Flashback data, pixel-perfect 256x224 graphics, complete
gameplay and cutscenes, save states, and sound effects.

This is a technically favorable port. REminiscence already separates the game
from its platform through `SystemStub`; its renderer is indexed, integer based,
and smaller than the iPod display; and the engine has no OpenGL requirement.
The main work is replacing SDL/libc services, providing a bounded allocator,
and fitting Flashback's controls to the click wheel.

The first release includes:

- iPod Classic 6G/7G and the matching 64-bit simulator;
- user-supplied DOS floppy and DOS CD data;
- the complete upstream game loop, menus, protection screen, polygon
  cutscenes, passwords, checkpoints, and manual save states;
- 256x224 output centered in the 320x240 RGB565 LCD;
- click-wheel controls, hold-switch safety, USB exit, and a Rockbox pause
  menu;
- sound effects after a silent gameplay gate passes;
- deterministic simulator and physical-device lifecycle tests.

Deferred until the initial port is stable:

- Amiga, Macintosh, PC-98, Sega CD voice, and demo-data variants;
- DOS PRF/AdLib or MT-32 music, Ogg music, and external sound fonts;
- rewind history, widescreen room extension, filters, and arbitrary scalers;
- iPod Video 5G enablement;
- cheats, replacement assets, and modifications to the Rockbox plugin ABI.

## Upstream Baseline And License Gate

Use Gregory Montoir's archived official
[REminiscence repository](https://github.com/cyxx/REminiscence) and pin the
first import to release 0.5.6, commit
[`947a4dce261aacc4f6ec0c2c90cb9d7e37712c7b`](https://github.com/cyxx/REminiscence/commit/947a4dce261aacc4f6ec0c2c90cb9d7e37712c7b)
(2026-07-22). Do not track a moving fork during bring-up.

REminiscence has historically been distributed as GPL-3.0-or-later, and the
[Libretro documentation](https://docs.libretro.com/library/reminiscence/)
identifies its core as GPLv3. However, the archived 0.5.6 GitHub snapshot has
copyright headers but no top-level `LICENSE`/`COPYING` file and no complete
license grant in those headers. This ambiguity is a hard import gate:

1. Obtain an authoritative GPL-3.0-or-later notice for 0.5.6 from the author,
   an official release archive, or source history.
2. Record that evidence in `apps/plugins/reminiscence/UPSTREAM.md`.
3. Preserve the complete applicable license as
   `apps/plugins/reminiscence/LICENSE.upstream`.
4. Confirm the release package's source and notice obligations before shipping
   `reminiscence.rock`.

Engineering prototypes may be kept out of distributable ZIPs until that gate
passes. Do not infer redistribution permission solely from a public GitHub
archive.

Flashback game data, voices, music, logos, and manuals remain proprietary.
None may be committed, generated into C arrays, or included in a Rockbox ZIP.
The plugin must work only with data copied by the user from a legally obtained
release.

## Why The Port Is Feasible

The pinned source has several properties that suit the iPod:

- `SystemStub` is the single video, input, timing, and audio platform boundary.
- Standard DOS gameplay is 256x224 with an 8-bit indexed framebuffer and a
  256-entry palette.
- The pinned standalone engine updates gameplay at 30 Hz, but renders small
  dirty regions rather than a modern full-screen 3D scene.
- The engine mixes four 8-bit sound-effect channels into signed 16-bit stereo.
- The default desktop output rate is 22,050 Hz and 0.5.6 makes it selectable.
- Save-state support and data-version detection already exist in the engine.
- The complete upstream source is about 1.2 MiB, although `staticres.cpp`
  contributes substantial read-only data and final `.rock` size must be
  measured.

The primary risks are:

- more than 200 upstream allocation/free sites, especially in resource
  loading;
- reliance on stdio, filesystem enumeration, time, assertions, and C++
  allocation operators;
- callback-safe audio shutdown when resource buffers are allocator-owned;
- controls requiring four directions plus action, use-item, draw/holster, and
  inventory/skip;
- license evidence for the final archived upstream snapshot.

None of these requires emulating a new CPU or replacing the renderer.

## Local References

Use current local ports as patterns, not the upstream SDL backend:

| Concern | Local reference | Reuse |
| --- | --- | --- |
| C++ plugin build | `apps/plugins/openlara/`, `apps/plugins/scummvm/` | `PLUGIN_CXXFLAGS`, no exceptions/RTTI, `extern "C" plugin_start` |
| Dynamic allocator | `apps/plugins/puzzles/rbmalloc.c` | TLSF over an explicitly bounded arena |
| Indexed game frontend | `apps/plugins/xworld/` | palette conversion, timing, Flashback-adjacent UI behavior |
| Click wheel | `apps/plugins/openlara/openlara.cpp` | absolute wheel zones, hysteresis, held-state polling |
| Plugin lifecycle | `apps/plugins/snes_lite/`, `apps/plugins/smsgg/` | one cleanup path, USB propagation, save-before-unload |
| PCM output | `apps/plugins/mpegplayer/pcm_output.c` | playback mixer channel, queue ownership, frequency restoration |
| Simulator automation | `tools/openlara_sim_gate.py` | isolated simdisk, scripted launch, log/frame validation |

Do not link SDL into the plugin or treat dormant desktop/SDL ports as audio
lifecycle references.

## Repository Shape

Keep the import isolated and mechanically comparable with upstream:

```text
apps/plugins/reminiscence/
    SOURCES
    reminiscence.make
    reminiscence.cpp
    reminiscence.h
    rockbox_compat.h
    systemstub_rockbox.cpp
    reminiscence_video.cpp
    reminiscence_input.cpp
    reminiscence_audio.cpp
    reminiscence_files.cpp
    reminiscence_alloc.cpp
    reminiscence_save.cpp
    reminiscence_menu.cpp
    LICENSE.upstream
    UPSTREAM.md
    upstream/
        selected 0.5.6 engine sources and headers
```

Frontend files own Rockbox policy. Imported files receive only narrow,
documented portability changes. `UPSTREAM.md` lists the commit, every imported
file, every omitted optional backend, and each local patch.

The installed layout is:

```text
/.rockbox/rocks/games/reminiscence.rock
/.rockbox/games/reminiscence/data/       user-owned game files
/.rockbox/games/reminiscence/saves/      save states
/.rockbox/games/reminiscence/reminiscence.cfg
/.rockbox/logs/reminiscence.log          opt-in diagnostic log
```

Keep data and saves outside the plugin directory so plugin upgrades never
replace user content.

## Build Contract

Add the plugin to `apps/plugins/SUBDIRS` only when all initial conditions hold:

```text
PLUGIN_CXX_AVAILABLE == yes
HAVE_LCD_COLOR
LCD_WIDTH == 320
LCD_HEIGHT == 240
MEMORYSIZE >= 32
target is IPOD_6G or its simulator
```

Use the repository C++ plugin flags and add only port-local options:

```text
-std=gnu++03
-fno-exceptions
-fno-rtti
-fno-threadsafe-statics
-fno-use-cxa-atexit
-fno-strict-aliasing
```

Begin at `-O2`. Enable `-O3`, LTO, or ARM-specific routines only after a
correct non-LTO simulator and hardware build has map and performance data.
Do not import a C++ standard library, SDL, libmodplug, zlib, libogg, libvorbis,
FluidSynth, or MT-32 emulation.

`rockbox_compat.h` maps the small required libc surface to Rockbox APIs and
port helpers. It must not globally hide unsafe functions without auditing call
semantics. Replace command-line parsing with typed frontend configuration.

Build gates:

- simulator and ARM hardware builds both produce `reminiscence.rock`;
- the plugin fits the 3 MiB iPod 6G plugin region with at least 128 KiB
  headroom after code, read-only data, BSS, and frontend stack are counted;
- the link has no unresolved libc, libm, SDL, threading, dynamic-loader, or
  host filesystem symbol;
- `make zip` includes engine code and notices but no Flashback data;
- all imported source and license files are represented in the source
  distribution.

## Upstream Platform Mapping

Implement a `SystemStub_Rockbox` with the following behavior:

| `SystemStub` operation | Rockbox implementation |
| --- | --- |
| `init` / `destroy` | Initialize frontend state and unwind through the single plugin cleanup path. |
| `setScreenSize` | Accept 256x224 for the DOS milestone; reject unsupported high-resolution modes before allocation. |
| palette access | Keep 256 RGB888 entries plus a cached RGB565 conversion table. |
| `copyRect` | Clip and copy indexed dirty rectangles into the 256x224 staging surface. |
| `copyRectRgb24` | Convert bounded cutscene/title RGB data to the staging surface; never allocate per call. |
| zoom/widescreen calls | Disable in milestone one and log unexpected calls in debug builds. |
| `updateScreen` | Convert dirty indexed pixels to centered RGB565 output and perform one LCD update. |
| `processEvents` | Poll held buttons, wheel position, Menu timing, hold switch, and USB. |
| `sleep` | Sleep only the remaining interval while continuing periodic exit/USB polls. |
| `getTimeStamp` | Convert `*rb->current_tick` to monotonic milliseconds with unsigned wrap handling. |
| `startAudio` / `stopAudio` | Attach/detach a bounded block queue to `PCM_MIXER_CHAN_PLAYBACK`. |
| `lockAudio` / `unlockAudio` | Protect mixer state with the smallest Rockbox-safe critical section; never block the PCM callback. |

Remove desktop-only screenshots from the first plugin. A debug frame dump may
be implemented in the frontend using a preallocated buffer and only while
audio is stopped.

## Data Detection And Compatibility

Milestone one recognizes upstream's DOS probes, including `LEVEL1.MAP`,
`INTRO.SEQ`, and supported DOS `.ABA` layouts, but the acceptance test uses a
full retail DOS data set rather than a demo.

File handling requirements:

- resolve data files case-insensitively without rescanning the directory on
  every open;
- build a bounded filename index once at startup and reject duplicate names
  that differ only by case;
- reject path traversal and absolute paths originating in resource names;
- check every open, seek, length, read, allocation, and decompression bound;
- distinguish missing data, unsupported edition, corrupt archive, and out of
  memory in the user-visible error;
- never write inside `data/`.

The plugin accepts either its own launcher entry or a path within a valid data
directory. Normalize both to the same data-root selection. Configuration and
saves remain under the fixed Rockbox path, not beside a user-selected file.

First-run validation checks a small manifest of required DOS resources and
reports missing filenames. It does not copy, download, convert, or patch game
data.

## Memory And Allocation Plan

REminiscence cannot use Rockbox's host `malloc` assumptions directly. At
startup, after validating the data root, claim the shared audio buffer exactly
once with `plugin_get_audio_buffer()` and let that call stop core playback and
transfer ownership. Never call `audio_stop()` first.

Create one 16-byte-aligned TLSF pool over the returned memory. Route upstream
`malloc`, `calloc`, `realloc`, `free`, scalar `new/delete`, and array
`new[]/delete[]` to that pool. The allocator wrapper must:

- detect overflow in `count * size`;
- return aligned storage suitable for all imported types;
- keep current, peak, largest-live, and failure counters in debug builds;
- poison freed blocks and use arena guards in the simulator;
- make allocation failure recoverable at the frontend boundary instead of
  continuing with a null dereference;
- never be reset or released while audio callbacks can reference resources.

Preallocate frontend-owned surfaces and PCM blocks before entering the game:

```text
256x224 indexed staging surface                 56 KiB
256-entry RGB565 palette                       512 B
optional 320x240 RGB565 staging surface        150 KiB
PCM queue, four stereo blocks at 22,050 Hz     measured/bounded
filename index and frontend state              bounded by manifest cap
remaining pool                                 engine/resources/save work
```

Prefer direct centered conversion into the LCD framebuffer if its format and
stride are safe on both simulator and hardware; otherwise retain the RGB565
surface. Do not keep both after measurement proves one unnecessary.

Record pool size and high-water usage for title, every level transition,
cutscenes, save/load, death/restart, and final cleanup. Release the shared
audio buffer only after the channel is stopped and callback quiescence is
confirmed.

## Video And Timing

The default display is pixel-perfect:

```text
source       256 x 224
destination  256 x 224
offset        32 x   8
border       black or current overscan palette color
```

No smoothing is required. Preserve the engine's palette changes, dirty-block
semantics, fade, shake offset, protection screen, menus, and polygon
cutscenes. Clear the complete LCD on mode transitions so stale Rockbox pixels
cannot appear around the game image.

Keep the pinned engine's original 30 Hz simulation timing. Rendering may
coalesce dirty LCD updates, but gameplay state must not run faster or slower
based on render cost. If a frame is late, do not build a backlog of sleep or
input events. Measure simulation, indexed-to-RGB565 conversion, LCD transfer,
and file I/O separately.

Macintosh's 512x448 mode is deferred because it requires downscaling and a
larger working set. Widescreen extensions and blur/mirror modes are also
deferred; they add memory and do not improve first-release correctness.

Physical 6G gates for a representative five-minute section:

- at least 29.5 simulated updates per second averaged over each 10-second
  window;
- 99th-percentile input-to-sample delay below 60 ms;
- no persistent late-frame queue, palette tearing, or border corruption;
- no allocator growth across ten room transitions or five death/restarts;
- CPU boost is enabled only during gameplay/load work and released in menus.

## Click-Wheel Controls

Use the wheel as an absolute directional pad. Eight zones allow diagonals and
hysteresis prevents edge chatter. Direction becomes neutral immediately when
the wheel is no longer touched. Do not derive held direction from scroll
events.

| iPod input | REminiscence field | Flashback action |
| --- | --- | --- |
| Wheel top/right/bottom/left | direction mask | Up/right/down/left |
| Wheel diagonal zones | paired direction bits | Diagonal movement/action |
| Centre | `shift` | Talk, use, run, shoot, confirm |
| Play/Pause | `enter` | Use current inventory object |
| Previous | `space` | Draw/holster weapon |
| Next | `backspace` | Inventory; skip cutscene |
| Menu short release | frontend only | Open Rockbox pause menu |
| Menu hold for 700 ms | frontend only | Offer save, then quit to Games |
| Hold switch newly engaged | frontend only | Pause and clear every logical key |

The field mapping above matches the pinned engine's desktop key contract.
Confirm each action in gameplay, the title menu, inventory, protection entry,
and cutscenes.

Held buttons are sampled each update. Menu, draw/holster, inventory, save,
load, and skip are edge-triggered. Clear the Rockbox event queue before
resuming from a frontend menu. If a physical build cannot report a required
button combination reliably, expose the action in the pause menu rather than
adding timing-sensitive chords.

The pause menu contains Resume, Save State, Load State, Restart Checkpoint,
Sound, Music, Controls, Diagnostics, and Quit to Games.

## Saves And Configuration

Keep upstream's game-state payload but wrap it in a frontend header containing:

- four-byte Rockbox port magic and format version;
- pinned upstream commit identifier;
- detected data edition and language;
- upstream state slot and payload size;
- payload CRC32;
- timestamp for display only.

Use at least five user slots plus the engine's in-game checkpoint. Never write
directly over the last known-good file:

1. Write a sibling temporary file completely.
2. Flush/close it and reopen it.
3. Validate header, exact length, and CRC32.
4. Rename the validated file over the selected slot.

On any error, preserve the old save. A corrupt or incompatible save is not
deleted or overwritten until the user explicitly saves to that slot.

`reminiscence.cfg` is versioned and contains data path, detected language,
sound, music backend, volume offset, Menu-hold duration, autosave policy, and
diagnostic overlay. It contains no pointers, allocator state, framebuffer,
PCM indices, or compiler-dependent engine structures.

Autosave only at a stable checkpoint or paused main-loop boundary. Never save
from a PCM callback, during level/resource loading, while a cutscene decoder
owns temporary buffers, or during USB teardown after a filesystem error.

## Audio Lifecycle

All implementation must follow `docs/plugin-audio-lifecycle-steering.md`.
Audio lands only after silent gameplay, saves, and cleanup are stable.

Milestone one audio is the engine's four-channel sound-effect mixer at 22,050
Hz signed 16-bit stereo. Generate into a bounded queue of preallocated blocks
and play it on `PCM_MIXER_CHAN_PLAYBACK`. The PCM callback only returns the
next ready block and updates queue indices. It must never allocate, load a
file, decode a resource, take a lock that can sleep, or call the game engine.

The main/game thread fills blocks by calling the upstream mixer. Protect
channel state with short critical sections matching Rockbox's mixer rules.
Track underruns and fill silence without reusing a block still owned by PCM.

Music is staged:

1. silent game;
2. native Flashback sound effects;
3. upstream's built-in MOD player with user-supplied compatible music;
4. DOS PRF/AdLib only if a bounded Rockbox-native synthesizer passes size and
   CPU gates;
5. Ogg only if an existing Rockbox decoder can be integrated without unsafe
   nested codec/plugin ownership.

Do not import libmodplug, FluidSynth, MT-32 emulation, or `stb_vorbis` for the
first release. Never create or replace the user's playlist to play game music.

Startup and teardown order is mandatory:

1. Capture only mixer/sample-rate state that can actually be restored.
2. Claim the shared audio buffer without a preceding `audio_stop()`.
3. Initialize allocator, resources, then PCM blocks.
4. Stop stale playback-channel state, set 22,050 Hz, and start queued output.
5. On pause/exit, prevent new blocks and stop logical sounds.
6. Stop `PCM_MIXER_CHAN_PLAYBACK` and clear its callback/data ownership.
7. Wait until no callback can reference plugin memory.
8. Restore mixer frequency/state and undo any playback fade.
9. Destroy engine/resource objects and TLSF metadata.
10. Release the shared audio buffer, then restore LCD/backlight/CPU state.

Database and Files playback must both start with audible output after exit.

## Lifecycle, Errors, And USB

`plugin_start()` uses one cleanup path with explicit initialized flags. Normal
quit, Menu quit, hold-switch quit, missing data, allocation failure, audio
failure, engine error, and USB all pass through it.

On `SYS_USB_CONNECTED`, stop accepting input, autosave only if the state and
filesystem are safe, perform full audio and memory teardown, restore display
state, and return `PLUGIN_USB_CONNECTED`. Never wait for another input event
after USB detection.

Replace upstream abort/assert behavior at external-data boundaries with a
controlled error carrying subsystem, filename, operation, offset/size, and
error code. Internal impossible-state assertions may remain in simulator debug
builds but must not become silent memory corruption in release builds.

The release log is off by default. Diagnostic mode writes a capped log with
edition detection, pool size/high-water, level transitions, frame timing,
audio underruns, save results, and cleanup stages. It must not log save payloads
or proprietary resource contents.

## Packaging And Games UI

Register `reminiscence,games` in `apps/plugins/CATEGORIES` only when the plugin
build exists. Add a neutral engine cover or a user-supplied cover through the
existing native-game cover pipeline; do not ship extracted Flashback art.

The Rockbox ZIP includes:

- `reminiscence.rock`;
- upstream license/notice material after the license gate passes;
- a short setup README listing supported data editions and directory layout;
- no game data, music, voices, screenshots, or cover art from Flashback.

Device sync copies only the plugin and engine-owned support files. It preserves
`data/`, `saves/`, configuration, and unknown user files. Removal of the engine
must be a separate explicit action from removal of user data.

## Verification Gates

### Static And Build

- Verify the pinned import and license ledger.
- Build clean simulator and ARM 6G plugins with no unresolved host symbols.
- Record `.rock`, text, rodata, data, BSS, and remaining plugin-buffer sizes.
- Scan the ZIP and source archive for prohibited Flashback data signatures and
  extensions.
- Run allocator overflow, alignment, double-free, out-of-memory, and guard
  tests in the simulator.

### Simulator

- Detect a supported DOS data set and reject incomplete/corrupt variants.
- Script title to level one and run at least 3,000 deterministic updates.
- Hash selected indexed frames before RGB565 conversion.
- Exercise all controls, diagonals, release-to-neutral, Menu timing, hold
  switch, pause/resume, protection entry, and cutscene skip.
- Save/load every slot, load a corrupt/truncated save, and verify old-save
  preservation after an injected short write.
- Loop level/room transitions, deaths, restarts, and title returns while
  checking allocator high-water and file-descriptor counts.
- Run silent and audio-enabled paths with identical scripted game-state hashes.
- Inject PCM underruns, partial startup failures, and USB at each initialization
  stage; every case must leave no live callback or leaked shared buffer.

### Physical iPod 6G

- fresh boot -> Flashback with sound -> Games;
- Database music -> Flashback -> Database music with working sound;
- Files music -> Flashback -> Files music with working sound;
- paused music -> Flashback -> resume the prior track successfully;
- Flashback sound -> Database and Files playback without reboot;
- ten rapid Games/Flashback/Database transitions without freeze or mute;
- volume changes, pause/resume, save/load, death/restart, and cutscene skip;
- hold-switch pause and clean USB exit;
- at least 30 minutes across multiple rooms with no audio underrun burst,
  memory growth, or filesystem leak;
- verify the 30 Hz timing and input latency gates in a five-minute profile.

Physical deployment is outside a specification-only change. When a firmware
build is eventually deployed, update and checksum both `rockbox.ipod` locations
as required by repository guidance. A full iPod 6G package deploy must use
`tools/deploy_ipod6g_preserve_database.sh`.

## Delivery Milestones

1. **License/import gate:** authoritative GPL notice, pinned 0.5.6 ledger,
   selected engine sources, and a build-only C++ frontend.
2. **Platform gate:** bounded allocator, filesystem index, timing, input, and
   256x224 RGB565 output with deterministic frame hashes.
3. **Playable-silent gate:** full DOS level-one gameplay, menus, cutscenes,
   saves, hold/USB behavior, and physical 6G timing profile.
4. **SFX gate:** 22,050 Hz playback-channel audio and the complete
   Database/Files lifecycle matrix.
5. **Completeness gate:** full DOS campaign transitions, protection,
   checkpoints, ending/credits, long-run memory checks, and packaging.
6. **Music gate:** built-in MOD backend first; PRF/AdLib only after separate
   memory, CPU, licensing, and lifecycle approval.
7. **5G gate:** profile iPod Video 5G and enable it only if timing, memory, and
   audio lifecycle criteria match the 6G release.

The first implementation slice ends at milestone 3. Audio and music must not
delay proof that gameplay, controls, resources, saving, and teardown are
correct.

## Acceptance Criteria

The first distributable release is accepted when:

- the upstream license gate is resolved and notices are packaged correctly;
- a user-owned supported DOS release completes the full game without patched
  proprietary data;
- output is pixel-correct and centered at 256x224 with stable 30 Hz gameplay;
- every required game action is reliable on physical click-wheel hardware;
- save states are transactional, versioned, and recover safely from corruption
  and write failure;
- the plugin has bounded allocation and no growth across the long-run tests;
- SFX audio has no recurring underruns and all callbacks are quiescent before
  memory release;
- Database and Files playback both work after every tested exit path;
- USB, hold switch, Menu quit, missing data, and partial initialization all
  return through clean teardown;
- the binary/source packages contain no proprietary Flashback assets.

Until all criteria pass, label the port experimental and keep it out of default
release packages.
