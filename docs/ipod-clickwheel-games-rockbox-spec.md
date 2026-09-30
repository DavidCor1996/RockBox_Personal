# iPod Click Wheel Games Rockbox Compatibility-Layer Spec

## Document Status

- Status: research-backed implementation specification
- Initial target: `ipod6g` only
- Working plugin name: `ipodgames`
- Primary hardware: iPod Classic 6G/6.5G/7G, S5L8702, 64 MB RAM
- Initial game target: one user-owned 2D game with a decrypted Classic eApp
- Long-term corpus: the 54 preserved iPod Click Wheel games

This document specifies a Rockbox compatibility layer for native Apple iPod
Click Wheel game executables. It is not a proposal to emulate the complete
Apple retailOS firmware and it is not a source-level port of each game.

Normative terms such as **must**, **must not**, **should**, and **may** describe
requirements for an implementation derived from this specification.

## Implementation Checkpoint (2026-07-17)

The repository now contains an executable compatibility prototype:

- safe `.ipg` inspection, normalized import, artwork extraction, and atomic
  Game Cover Flow index generation under system ID `ipodgames`;
- an `ipodgames.rock` catalog and direct `.igame` launch path that enters the
  runtime;
- target-side eApp header and complete framework-table validation;
- an ARM interpreter, import traps/HLE, asset I/O, RGB565 software renderer,
  texture storage, timing, and click-wheel event translation;
- rooted per-game save-file open/write/close/read behavior with traversal
  rejection and a deterministic round-trip probe;
- a saturating 64-object software mixer for the target's 11,025 Hz mono
  signed-16-bit WAV resources, including per-voice gain, pan, pitch, callback
  quiescence, exact play/pause/resume/stop and repeat-count behavior, global
  volume bridging, and mixer-frequency restore;
- simulator gates for Game Cover Flow, direct loading, deterministic game
  execution, the full name-entry wheel, main-menu Volume, and gameplay
  pause/Continue;
- clean simulator and ARM iPod 6G builds plus ten passing host-tool tests.

A locally supplied, user-owned Ms. PAC-MAN Classic eApp (build 2805293) now
passes the simulator gate at 816,760 bytes, load base `0x18000000`, six
framework tables, and 277 imports. Its commercial executable and assets remain
outside version control. The deterministic gate executes the official startup,
loads 23 texture atlases and all 20 audio resources, completes name entry,
enters the main and play menus, crosses the tutorial, renders Stage 1, accepts
positional click-wheel input, and consumes pellets. The current 1,000-frame
plus 129-frame input run completes 1,128 presented frames, 126,121 observed
draw calls, 25,370,190 guest instructions, six official sound starts, 48,640
natural PCM frames, and a verified save round trip without an ARM fault. An
extended 1,800-frame run reaches 14 official sound starts, four
simultaneous stock voices, and 148,992 output frames including the isolated
512-frame mixer probe and newly recovered repeating sounds. The save gate now
calls Ms. PAC-MAN's own persistence routine
and verifies its exact 76-byte `save/ms_pac_man.dat` record in addition to an
independent 16-byte filesystem round trip.

RetailOS's actual 5G click-wheel normalizer was recovered from OSOS at runtime
address `0x100e95a4`. The required conversion is
`((119 - (rockbox_raw % 96)) * 8 / 3) & 0xff`; it includes both an offset and a
polarity reversal. A linear `0..95` to `0..255` expansion is incorrect: on the
physical wheel it maps right into the game's down sector and reverses menu
scrolling. The simulator now validates all 96 converted coordinates and uses
the stock joystick artwork as an image oracle: normalized positions `0`, `64`,
`128`, and `192` must visibly point right, up, left, and down. A separate
frame-pair oracle requires Ms. Pac-Man's yellow sprite centroid to change maze
position during live gameplay. The stock name-entry path still verifies that
the complete alphabet and confirmation states remain reachable.

Contemporary reviews confirm that stock gameplay uses touch position, not
clicks: the player touches or brushes the wheel in the desired one of four
directions and the miniature on-screen joystick reports that direction. This
matches the recovered input code and is the acceptance behavior for the port.

The first direction-correct physical run exposed a separate performance
failure: 862 presented frames took 6,375 Rockbox ticks (63.75 seconds at
`HZ=100`), an effective 13.5 fps, and every measured frame missed its 60 Hz
deadline. A first batching/render pass improved a later sample to only 16.6 fps:
932 frames in 5,607 ticks while the CPU was already boosted to 216 MHz. That
cadence explains slow option activation and actors appearing to teleport even
though the guest reports a fixed 16 ms frame delta. Frame skipping, altered
guest time, and visual-only interpolation are not acceptable parity fixes.

The target runtime batches ARM dispatch between framework traps, keeps its
instruction counter in a register during each batch, inlines guest DRAM
loads/stores and ARM barrel shifts, and uses an exact 1:1 RGBA5551 texture blit
path. In the simulator, the blit path covers 1,245 of 1,481 quads and 5,606,038
pixels during the 119-frame startup sample. The 16.6 fps log also proved the
fast blitter covered 113,268 quads and 34,247,354 pixels, so the next target
build removes Rockbox's size optimization from this throughput-bound plugin
and forces the bank-aware register lookup into the ARM decoder. Hardware
disassembly must contain no out-of-line `reg` calls; simulator behavior must
remain unchanged. Physical acceptance still requires a fresh runtime log
demonstrating smooth menu response and continuous actors at the stock cadence.

Two additional deterministic paths cover stock UI branches rather than
reconstructed substitutes. The options path highlights the original Volume
row, enters its edit state, moves the original slider, and observes Audio
ordinal 53 set the normalized system volume from 255 to 239. The pause path
opens the original `RESUME GAME / VOLUME / OPTIONS / ABANDON GAME / SAVE &
EXIT` menu during Stage 1, observes two pause and two resume calls, selects
Resume Game, and returns to the live maze.

The observed OpenGLES subset now includes clear color/clear, texture bind and
RGBA5551 upload, vertex/color/texture-coordinate arrays, fixed render-pipeline
selection, quads, triangle strips, and presentation. Ms. PAC-MAN uploads
textures with type `0x8034`, which is
`GL_UNSIGNED_SHORT_5_5_5_1`: five bits each of red, green, and blue followed
by one coverage bit. Treating the low nibble as RGBA4444 alpha corrupts the
blue channel and causes incorrect menu and maze rendering.

This is not yet a complete release-quality port. The current simulator and ARM
builds must still pass the physical iPod 6G performance, controls, persistent
save/relaunch, audio-transition, and teardown matrix. The post-audio/save/
renderer hardware package has been deployed with both firmware copies and the
plugin checksummed, while preserving and revalidating the mounted tag database.
The later audio-semantics build was deployed plugin-only with SHA-256
`bdb677f68716b962f5f032d58a047412bda991f0ac6cc8a96df905e018d08584`;
all 11 readable database files and `tagcache_autoupdate: on` were revalidated
before sync and unmount.
Physical controls, performance, save/relaunch, and audio-transition testing
remain before calling Ms. PAC-MAN fully ported rather than simulator-playable.

The first physical build exposed severe missed-frame behavior: menu animations
ran slowly and live gameplay appeared to teleport between widely separated
positions. The renderer was performing two signed integer divisions for every
sampled sprite pixel. The S5L8702 ARM core has no hardware integer divide, so
those operations became millions of software helper calls per second. The
device renderer now uses an exact incremental source-coordinate accumulator,
reducing division to two setup operations per quad while preserving the
pre-system-bridge 1,000-frame framebuffer checksum
`0x974505f4bb6a6f9f`. Per-pixel texture provenance accounting and guest-PC
tracing are also compiled only into simulator builds. Interactive device runs
record elapsed ticks, total and maximum frame-execution ticks, late frames,
maximum lateness, and deadline rebases in
`/.rockbox/ipodgames/ipodgames-runtime.log`.

The retail system bridge now supplies the real clock, 12/24-hour preference,
battery level, and reversible screen brightness. This fixes the visibly wrong
`17:35`/empty-orange-battery status bar; the deterministic stock-reference
fixture now renders `5:35PM` with a green battery and has checksum
`0xcd7a17cb60ba22bf`. During that work, the ARM interpreter was found to omit
the carry-out of rotated data-processing immediate operands. The game's
optimized `strcmp` uses `TST` followed by `RRX`, so the missing carry made
equal strings compare unequal and could select the wrong menu branches. The
interpreter now implements the ARM carry rule, and an exact `"12"` setting
comparison returns equal.

### Vortex bring-up checkpoint (2026-07-18)

For the later 30 Hz timing, music streaming, and outstanding full-game
qualification work, see [Vortex retail compatibility](vortex-retail-parity.md).
The deployment and measurements below describe the July checkpoint.

The full paid Vortex 1.0 package, not the bundled demo, is imported as game
GUID `12345`, build 2563290. Its Platform ID 1 executable is a decrypted
414,600-byte eApp at load base `0x18000000`, with seven framework tables and
429 import slots. Vortex appears as its own entry in Game Cover Flow under the
`iPod Games` system; neither its commercial executable nor its assets are
tracked by the repository.

The runtime has a game-neutral simulator mode that omits only the Ms. PAC-MAN-
specific save, wheel-sweep, and multivoice probes. Vortex passes this gate for
120 event frames with no ARM fault, 14,398 rasterized primitives, 120
presentations, and a nonblack framebuffer. The same build still passes the
existing 120-frame Ms. PAC-MAN gate, including its exact save and mixer probes,
and all ten host-tool unit tests pass.

Vortex exposed and now exercises shared ABI behavior not required by the first
game:

- OpenGLES-generated texture names rather than implicit texture zero;
- unsigned-byte RGBA8888 and alpha-only uploads, RGB565, and RGBA4444 in
  addition to RGBA5551;
- a third fixed-point vertex-color attribute, per-program uniform color,
  framebuffer texture copies, multiple texture units, and arbitrary triangle,
  triangle-strip, triangle-fan, and quad batches;
- the previously unclassified Metadata framework;
- a two-stage AsyncFileIO open/read completion path for the packaged
  `Localization/en.lproj/text.strings` asset; and
- a synchronous follow-up request required to advance the stock loader.

The guest lifecycle and input mappings are now recovered. Event type 1 is the
Menu/quit control and type 2 is the center button; the click wheel uses a
touch-down-relative normalized byte. A deterministic path crosses the title,
enters the first-run name editor, selects `A`, rotates through the stock Done,
Backspace, Space, and alphabet cells, commits the profile, opens the circular
main menu, selects New Game, completes the tunnel transition, fires during
live play, rotates the paddle smoothly around the arena, opens the six-item
pause carousel, selects its door, and reaches the guest's terminal state.

Vortex's effects are 27,000 Hz mono signed-16-bit PCM rather than Ms. PAC-MAN's
11,025 Hz resources. The mixer now selects the correct per-game output rate;
the extended gameplay gate reaches 15 official sound starts and produces
134,751 PCM frames. Selecting the pause-menu door invokes the stock save path:
four writes create `stats`, localized `en/stats`, `options`, and an 18,988-byte
`quicka` save, totaling 21,356 bytes with zero failures. The host recognizes
Vortex state 6 plus completed writes as a clean exit, avoiding the prior
post-save frozen frame.

The hardware plugin and complete user-owned Vortex directory are deployed to
the mounted iPod without unmounting it. The deployed plugin SHA-256 is
`0958d5d5f0edf442ebcc4001e04a108b6042a53284f8354d38ac849498265f20`,
and the deployed executable matches the simulator-tested source at
`6b89c1f3d8ce439a6352d9b577c82a2da4a094df31baac031d840b5b547bf06b`.

Two fidelity gaps remain before describing Vortex as stock-identical. The
software renderer does not yet reproduce every two-sampler transition/mask
shader and depth/compositing detail of the cylindrical scene, so some tunnel
backgrounds and menu layering differ from retailOS. The three 44.1 kHz stereo
AAC/M4A music streams are registered by the guest but are not decoded by the
plugin; PCM gameplay effects work. Both issues must be resolved under the
plugin audio-lifecycle and shared-buffer constraints before a 1:1 claim.

## Executive Decision

Build a Wine-like binary compatibility layer that loads a decrypted Apple
`eapp` executable, resolves its imported Apple framework tables to Rockbox
implementations, and executes the game's native ARM code directly.

Do not pursue these alternatives as the primary design:

- Full retailOS hardware emulation inside Rockbox.
- Static source reconstruction of all 54 proprietary games.
- A permanently patched stock firmware as the final user experience.
- Bundling decrypted games or Apple framework code with Rockbox.

The compatibility-layer approach is selected because inspected game code is
native ARM and reaches operating-system services through explicit framework
import tables. The iPod Classic and Rockbox use the same processor and display
that the Classic version of each game was built for.

## Goals

### Prototype goals

1. Parse and validate decrypted `eapp` executables on a host computer.
2. Recover the Classic retailOS eApp loader and framework registry well enough
   to identify framework UUIDs, ordinals, signatures, and loader behavior.
3. Load one decrypted Classic executable on iPod 6G without booting stock
   firmware.
4. Resolve framework calls to instrumented Rockbox stubs.
5. Reach the game's entry point and report the first unsupported call safely.
6. Display the first correct frame of a simple 2D game.
7. Support clickwheel input and persistent save data for that game.
8. Add correct audio lifecycle behavior after silent gameplay is stable.

### Product goals

1. Launch compatible user-owned games from Rockbox as `.ipg` packages or from
   an imported game directory.
2. Present imported titles in Game Cover Flow under a first-class console named
   `iPod Games`, using artwork extracted from the user's package.
3. Preserve the original game code, assets, controls, saves, timing, and aspect
   ratio as closely as practical.
4. Require no normal boot into stock firmware after a game has been imported.
5. Keep the open-source loader legally and technically separate from game
   executables, authorization data, and Apple firmware.
6. Recover cleanly so Database and Files music playback work after game exit.
7. Allow compatibility to grow one framework call and one game at a time.

## Non-Goals

The first implementation will not:

- Emulate the PortalPlayer-based iPod Video hardware.
- Generalize Nano-only Platform ID 1 binaries beyond the observed
  Classic-compatible package selected for the first target.
- Support Nano-only screen sizes or accelerometers.
- Reimplement the complete OpenGL ES 1.x API before a game requires it.
- Preserve active Apple FairPlay authorization at Rockbox runtime.
- Replace or mutate the user's Rockbox playlist.
- Ship Apple retailOS, `IC-Info.sidb`, account keys, decrypted executables, game
  artwork, music, or other copyrighted game data.
- Promise compatibility with all 54 titles from the first release.
- Attempt fault isolation equivalent to an operating-system process boundary.

## Confirmed Research Baseline

### Hardware and Rockbox

The initial target provides:

- Samsung S5L8702 with an ARM926EJ-S CPU.
- 64 MB DRAM starting at physical address `0x08000000`.
- 216 MHz maximum configured CPU frequency.
- 320x240 RGB565 LCD.
- Absolute clickwheel position and normal Rockbox button events.
- A 3 MiB normal plugin buffer.
- Access to the larger shared audio buffer through
  `plugin_get_audio_buffer()`.
- PCM, mixer, filesystem, timer, cache, and LCD services through the Rockbox
  plugin API.

Relevant source files:

- `firmware/export/config/ipod6g.h`
- `firmware/export/s5l87xx.h`
- `firmware/target/arm/s5l8702/system-s5l8702.c`
- `apps/plugin.h`
- `apps/plugin.c`
- `docs/plugin-audio-lifecycle-steering.md`

### Preservation project

The iPod Click Wheel Games Preservation Project provides a complete game
corpus and an offline iTunes authorization workflow. Its game executables are
still encrypted. The project works because iTunes installs account keys into a
device-specific authorization database; it is not a decrypted or source-level
archive.

Consequences:

- The preservation repository is a valid package and asset corpus.
- Files copied directly from that repository are not executable by Rockbox.
- `IC-Info.sidb` is sensitive, device-specific input and must never be checked
  into this repository or written into logs.
- A decrypted Classic-generation eApp remains a hard prerequisite for native
  execution research.

### Inspected game formats

Direct inspection established the following:

- `.ipg` is a ZIP-compatible package.
- `Manifest.plist` lists assets, executable paths, sizes, platform IDs, DRM,
  verification requirements, build IDs, game name, version, and GUID.
- A package may contain separate Platform ID 1 and later-generation binaries.
- Later Platform IDs may share the same executable.
- Encrypted executables are high-entropy data and do not expose the `eapp`
  header.
- `.sinf` files use nested atom-like records including scheme, user, key, and
  IV information, but do not expose a plaintext content key by themselves.
- `Manifest.plist.p7b` is a PKCS#7 signature over package metadata.

A historical decrypted Platform ID 1 Tetris executable established that:

- The executable begins with `eapp`.
- It contains native little-endian ARM code.
- It is linked into the `0x18000000` virtual-address region.
- Framework imports use ARM jump stubs followed by loader-filled function
  pointer slots.
- Imports are identified by framework name, a 16-byte identifier, and ordinal.
- The sample imports `OpenGLES`, `AsyncFileIO`, `Audio`, `InputEvents`,
  `Metadata`, `miscTBD`, and `Settings`.
- The OpenGLES table exposes 179 slots, although a game may call only a small
  subset of them.

The Tetris binary is useful for format-tool development but is not proof of the
Classic ABI because it is a Platform ID 1 iPod Video build.

### Current installed Classic corpus

At research time the connected authenticated Classic contained later-platform
executables for:

- Peggle
- Ms. PAC-MAN
- Real Soccer 2009
- Asphalt 4
- Cake Mania 3

The later-platform executables are approximately 574-851 KiB. These are
appropriate candidates for Classic loader research. Ms. PAC-MAN is the
preferred first gameplay target because it is 2D and avoids making 3D
performance part of the initial proof.

## Current retailOS Reverse-Engineering State

Public retailOS documentation describes a monolithic RTXC 3.2/Pixo system, an
eApp framework segment, native game execution, and a self-relocating firmware
layout. The most detailed public memory-layout example is for Nano 5G rather
than a completed Classic 6G decompilation.

The local Classic 1.1.2 `osos` extraction is still encrypted and cannot yet be
used as trustworthy decompiler input. No complete public Classic Ghidra project
or reconstructed game SDK is assumed to exist.

Current `wInd3x` documentation reports Classic 6G support for tethered boot-ROM
execution, IMG1 decryption, and memory dumping. Classic custom-firmware launch
support is experimental. The research workflow must therefore treat Classic
retailOS decryption as feasible while treating live patched-retailOS execution
as an item to prove.

## Legal, Privacy, and Distribution Boundary

The implementation must be usable with copies supplied by their owner. The
Rockbox project must not distribute game content or authentication secrets.

Permitted repository content:

- eApp and `.ipg` parsers.
- Import, extraction, validation, and analysis tools.
- Framework signatures and ordinal mappings expressed as independently
  derived facts.
- Clean-room framework implementations.
- Synthetic fixtures and executables built specifically for testing.
- Hashes and metadata needed to identify compatible user-supplied versions.

Content that must not enter the repository:

- Decrypted commercial eApp binaries.
- `.ipg` packages containing commercial game assets.
- Apple retailOS images or extracted Apple framework code.
- `IC-Info.sidb`, account credentials, device authorization keys, FairPlay
  content keys, or device-unique secrets.
- Decompiled Apple or game code copied substantially into the implementation.

Logs must redact account identifiers, serial numbers, authorization records,
content keys, and absolute host paths unless a developer explicitly enables a
local diagnostic mode.

The final loader should accept user-supplied data and should produce a local
compatibility manifest containing hashes, not secrets.

## Proposed User Experience

### Imported game layout

The initial runtime should use a normalized imported layout rather than parse
and decrypt arbitrary `.ipg` packages inside the plugin:

```text
/.rockbox/ipodgames/
    games/
        <guid>/
            game.igame
            executable.eapp
            cover.jpg
            assets/
    saves/
        <guid>/
            data/
            stats/
    logs/
        ipodgames.log
    compatibility.db
/.rockbox/games/ipodgames/
    games.tsv
```

`game.igame` is a versioned, line-oriented local launch record. It currently
uses the `IPODGAMES/1` header so the target plugin can parse it without adding a
general TOML implementation. It should include:

- game GUID, name, and version;
- original executable build ID and platform ID;
- SHA-256 of the encrypted and decrypted executable when available;
- framework identifiers and required ordinal set;
- expected load base and entry point;
- compatibility profile name;
- renderer and audio feature flags;
- asset-root mapping;
- save compatibility version;
- package filename and package hash for provenance;
- the local executable and cover-art filenames.

The importer must rebuild `/.rockbox/games/ipodgames/games.tsv` atomically.
That index is the contract with Game Cover Flow: system ID `ipodgames`, display
name `iPod Games`, `.igame` launch path, and original package artwork path. The
launcher must pass the selected `.igame` path to `ipodgames.rock`. Generic
directory scanning is only a recovery fallback because every imported file is
named `game.igame` and therefore cannot supply a useful title by filename.

### Launch flow

1. The user selects the `iPod Games` console in Game Cover Flow and then an
   imported title. Directly opening an associated metadata file remains a
   supported fallback.
2. The plugin validates the local metadata and executable hash.
3. It reports known compatibility status and required experimental features.
4. It acquires memory and initializes framework services.
5. It maps and relocates the eApp.
6. It resolves imports or installs diagnostic traps for unsupported ordinals.
7. It enters the game through the recovered loader ABI.
8. MENU behavior follows the game when safe; a documented emergency chord
   always requests termination.
9. Exit stops all callbacks, restores hardware and mixer state, unmaps game
   memory, releases buffers, flushes saves, and returns to the prior menu.

### Unsupported game behavior

An unsupported game must fail with a useful report rather than a generic
plugin crash. The report should show:

- game/build/hash;
- validation or loader stage;
- framework identifier and ordinal if applicable;
- last eApp program counter;
- requested memory and available memory;
- renderer/audio feature that was missing;
- log path.

## Repository Shape

The proposed implementation layout is:

```text
apps/plugins/ipodgames/
    ipodgames.c
    ipodgames.make
    SOURCES
    browser.c
    browser.h
    runtime.c
    runtime.h
    eapp/
        eapp_format.h
        eapp_parse.c
        eapp_load.c
        eapp_mmu.c
        eapp_trampoline.S
    frameworks/
        framework.c
        framework.h
        fw_asyncfileio.c
        fw_audio.c
        fw_inputevents.c
        fw_metadata.c
        fw_misc.c
        fw_opengles.c
        fw_settings.c
        generated_ordinals.h
    renderer/
        gles_state.c
        gles_texture.c
        gles_raster.c
        gles_present.c
    platform/
        rb_audio.c
        rb_files.c
        rb_input.c
        rb_log.c
        rb_memory.c
        rb_time.c
tools/ipodgames/
    ipg_inspect.py
    ipg_import.py
    eapp_inspect.py
    framework_db.py
    retailos_to_elf.py
    data/
        frameworks.json
docs/ipod-clickwheel-games-rockbox-spec.md
```

The exact split may change after the first executable is recovered. Format
parsing, framework knowledge, platform glue, and software rendering must remain
separable so they can be tested independently.

## Host Research and Import Tools

### `ipg_inspect.py`

Must operate read-only and report:

- ZIP/package validity;
- game name, GUID, version, and build IDs;
- executable list, platform IDs, size, DRM, and verification flags;
- asset counts and aggregate sizes;
- manifest and signature presence;
- executable entropy/magic without printing encrypted contents;
- SHA-256 hashes.

It must not parse or print private authorization databases.

### `eapp_inspect.py`

Must initially support the known decrypted sample and evolve with the Classic
format. It should report:

- header fields with raw offsets;
- proposed load base, image size, entry point, and segment permissions;
- relocation records;
- framework name, identifier, table width, and slot addresses;
- ARM/Thumb code ranges;
- every absolute pointer outside the proposed image;
- malformed ranges and integer overflows;
- a machine-readable import manifest.

Unknown fields must retain their raw values. The tool must not silently assign
meaning to a field before that meaning is confirmed across multiple binaries.

### `retailos_to_elf.py`

This is a research tool, not part of the shipped plugin. It should:

- accept a locally decrypted retailOS image;
- model known SRAM, DRAM, framework, and BSS segments;
- support firmware-version-specific layout descriptors;
- emit an ELF suitable for Ghidra analysis;
- preserve source offsets in section names or a sidecar map;
- identify framework strings and probable registration tables;
- never embed the source firmware in generated repository fixtures.

### `ipg_import.py`

The first importer may require a separately obtained decrypted executable. It
must:

- accept the owner's `.ipg` package and decrypted matching eApp;
- verify manifest sizes and hashes where applicable;
- select the Classic-compatible platform build;
- reject a mismatched plaintext executable;
- extract only required assets;
- normalize paths and reject traversal entries;
- write the normalized game layout;
- generate local hashes and compatibility metadata;
- avoid copying `.sinf`, `IC-Info.sidb`, or account data into the game folder.

Offline decryption may be added later as a separate optional stage after its
inputs and secret-handling requirements are understood. It must not be a
prerequisite for proving the runtime.

## eApp Loader Requirements

### Parser safety

The parser must validate all data before making executable mappings:

- checked addition and multiplication for every offset and count;
- file ranges contained within the executable;
- maximum segment, relocation, framework, and import counts;
- alignment of words, branch stubs, tables, and load segments;
- non-overlapping writable runtime regions unless overlap is explicitly part
  of the recovered format;
- entry point contained within an executable segment;
- import pointer slots contained within writable game memory;
- known ARM instruction form for any import stub that the loader patches;
- no path or asset reference escaping the imported game root.

Invalid input must be rejected before calling game code.

### Memory acquisition

The runtime must use a single arena where practical. On hardware:

1. Record only the playback and mixer state that can be restored correctly.
2. Call `plugin_get_audio_buffer()` directly when the large arena is needed.
3. Do not call `audio_stop()` immediately before acquiring that buffer.
4. Divide the arena into loader image, game heap, framework state, textures,
   framebuffers, decoded audio, and log storage.
5. Fail before mapping if the complete minimum budget cannot be reserved.

Initial target budget:

| Region | Initial budget |
| --- | ---: |
| eApp code/data/BSS | 4 MiB |
| game heap | 8 MiB |
| framework state and I/O | 2 MiB |
| textures and renderer | 16 MiB |
| two RGB565 framebuffers | 300 KiB |
| decoded/mixed audio | 1 MiB |
| file cache and logs | 1 MiB |
| reserve/fragmentation | 4 MiB |

These are ceilings for the first arena planner, not guaranteed allocations.
Actual per-game budgets must come from measurement.

### Virtual addressing

The historical sample uses addresses in the `0x18000000` region while Rockbox
DRAM begins at `0x08000000`.

The loader should support two strategies behind one interface:

1. Apply complete eApp relocation metadata to an arbitrary arena address.
2. On `ipod6g`, map a temporary virtual alias at the expected eApp base.

Relocation is preferred when the recovered format proves complete. A temporary
MMU alias is the fallback because the S5L8702 target already uses an ARM section
translation table.

MMU alias requirements:

- use only an otherwise unused virtual range;
- map only the minimum whole 1 MiB sections;
- save every replaced translation-table entry;
- map to arena pages owned by the plugin;
- set executable code read-only where practical and data writable;
- clean data cache and invalidate instruction cache before entry;
- invalidate TLBs after map and restore operations;
- restore all mappings before releasing the backing arena;
- never leave callbacks executing through an alias during teardown.

The implementation must verify on hardware that `0x18000000` is not used by
Rockbox or target peripherals before enabling this strategy.

### ABI and entry trampoline

Initial execution support is ARM state only. The trampoline must:

- preserve the Rockbox callee-saved register set;
- create a dedicated aligned game stack inside the arena;
- install the recovered eApp startup arguments exactly;
- preserve the platform's soft-float ABI;
- record the current game PC around framework transitions when practical;
- return through a controlled termination trampoline;
- reject unsupported Thumb entry points until explicitly implemented.

The exact startup structure is a research deliverable. It must not be guessed
from the Platform ID 1 sample alone.

### Relocations

The loader must preserve relocation type and source information. Supported
types will be added only after confirmation. At minimum it must distinguish:

- absolute data pointers;
- code pointers;
- import table pointers;
- ARM branch relocations;
- base-relative offsets;
- values that are already runtime virtual addresses.

Unknown relocation types are fatal in normal mode. Diagnostic mode may report
them but must not execute the image.

## Framework Compatibility Architecture

### Framework identity

A framework implementation is selected by both name and its 16-byte identifier
or version value. Name alone is insufficient.

The framework database must record:

- framework name;
- identifier bytes;
- source retailOS/device/firmware version used for research;
- table width;
- ordinal number;
- recovered symbolic name;
- calling convention and typed signature;
- confidence level;
- implementation status;
- games known to call it;
- behavioral notes and test evidence.

Confidence values should be `unknown`, `inferred`, `traced`, or `confirmed`.

### Import resolution

For each import slot the loader must install one of:

- a confirmed compatibility wrapper;
- a diagnostic trap;
- an explicit per-game compatibility override.

It must never install a generic zero-return stub silently. Returning an
incorrect value can corrupt game memory far away from the original call and
make reverse engineering less reliable.

A diagnostic trap should log:

- framework and identifier;
- ordinal;
- r0-r3;
- a bounded stack-word snapshot;
- link register and probable caller PC;
- game/build identifier;
- frame number and timestamp.

After logging, the default behavior is controlled termination. A developer may
mark a particular ordinal as safe to return a chosen value after documenting
why.

### Ordinal discovery

Ordinal semantics should be recovered through several independent methods:

1. Locate framework tables and registrations in decrypted Classic retailOS.
2. Trace framework resolution or calls under an instrumented, authenticated
   stock loader.
3. Compare call sites across multiple decrypted eApps.
4. Match OpenGL ES behavior against the published OpenGL ES 1.x API.
5. Confirm candidate signatures by argument shape and observed side effects.

Copied Apple function bodies are not an acceptable implementation shortcut.

## Framework Requirements

### `InputEvents`

The input framework must map the recovered Apple event structure to:

- MENU, SELECT, PLAY, LEFT, and RIGHT buttons;
- clickwheel forward/back events;
- absolute wheel position when requested;
- button press, release, repeat, and hold semantics;
- hold-switch state if exposed to games;
- monotonic event timestamps.

The runtime must reserve an emergency-exit chord that cannot be disabled by a
game. The proposed chord is MENU+SELECT held for two seconds, subject to
hardware testing against the bootloader reset chord.

Input delivery must not busy-loop. It should cooperate with Rockbox ticks and
yield when the game has no work.

### `AsyncFileIO`

All game-visible paths must be virtualized.

| Apple-style purpose | Rockbox location | Access |
| --- | --- | --- |
| packaged assets | game `assets/` | read-only |
| game data | `saves/<guid>/data/` | read/write |
| game statistics | `saves/<guid>/stats/` | read/write |
| temporary files | per-launch arena or temp directory | read/write |

Requirements:

- canonicalize separators and reject `..` traversal;
- enforce a bounded path length;
- emulate expected case behavior on FAT32;
- keep package data read-only;
- use atomic replace for save files where possible;
- flush save data on normal termination;
- bound outstanding operations and buffer sizes;
- deliver completions according to recovered asynchronous ordering.

The first implementation may execute host filesystem operations synchronously,
but completion callbacks must be queued and delivered later if the Apple API is
observably asynchronous.

### `Metadata`

Implement only confirmed queries. Likely responsibilities to investigate
include:

- display dimensions and pixel format;
- device family and platform version;
- language, region, and text direction;
- battery and charging state;
- time and timezone;
- game package metadata;
- music-library or now-playing metadata.

Device and account identifiers must be anonymized or unavailable unless a game
provably requires a stable value. A local per-install pseudonymous identifier
is preferred over exposing the hardware serial number.

### `Settings`

Settings must be stored per game under the save root. Global Rockbox settings
must not be mutated merely to satisfy a game. Screen brightness and volume may
be changed only through normal Rockbox services and must be restored on exit
when the game-specific API requires temporary control.

### `miscTBD`

This framework must remain split into ordinal-specific wrappers. Do not create
one untyped miscellaneous service. Expected functions may include allocation,
time, scheduling, logging, strings, or application lifecycle, but each mapping
must be evidenced independently.

### `OpenGLES`

The initial renderer is a clean-room software OpenGL ES 1.x subset targeting a
320x240 RGB565 framebuffer.

For Ms. PAC-MAN build 2805293, runtime tracing observes OpenGLES ordinals 4,
12, 13, 36, 37, 40, 99, 105, 137, 149, 152, 153, 157, 159, and 164. The
implemented raster path covers the state and drawing behavior that affects its
frames. Ordinals whose effects are not required by the observed 2D output
remain instrumented compatibility no-ops rather than guessed APIs.

First candidate feature set:

- viewport and scissor;
- clear color and buffer clear;
- model-view and projection matrix stacks;
- fixed-point and floating-point matrix entry points as required;
- vertex, color, and texture-coordinate arrays;
- triangles, triangle strips, and triangle fans;
- RGB565, RGBA4444, RGBA5551, and RGBA8888 texture upload as required;
- nearest and bilinear texture sampling;
- texture wrap/clamp;
- alpha test and standard source-alpha blending;
- orthographic projection;
- framebuffer presentation and `glGetError` behavior.

Deferred until observed:

- lighting;
- fog;
- depth buffering;
- mipmaps;
- compressed textures;
- point sprites;
- uncommon blend equations;
- readback;
- extensions.

Renderer rules:

- implement state behavior, not merely matching function names;
- use fixed-point inner loops where that materially improves ARM926 speed;
- clip all primitives before writing pixels;
- validate texture sizes and total texture memory;
- use dirty regions when practical;
- provide 15, 20, and 30 fps caps;
- yield between frames;
- record frame time, triangle count, texture uploads, and overdraw diagnostics;
- make visual-error toggles available only in developer builds.

The existing SM64 software renderer may provide reusable Rockbox-facing
framebuffer and rasterization ideas, but its game-specific rendering API is not
itself an OpenGL ES implementation.

### `Audio`

Audio follows stable silent gameplay. For Ms. PAC-MAN, the recovered API uses
opaque sound objects backed by decoded PCM: data pointer, byte size, sample
rate, channels, sample width, gain, pan, pitch, play, stop, and playing-status
ordinals. Static call-site tracing identifies Audio ordinal 13 as unsigned
`0..32767` gain, ordinal 14 as signed pan with zero centered, ordinal 15 as a
per-mille pitch/rate scale with 1000 normal, and ordinal 2 as play. Direct
RetailOS implementation recovery further establishes ordinal 3 as pause,
ordinal 4 as resume, ordinal 5 as stop, ordinal 16 as repeat count (`0` means
indefinite, `1` means once, and higher values decrement), ordinal 23 as the
PCM-data getter, and ordinal 39 as the playing-state test. Static ordinals 51,
52, and 53 are current normalized system volume, constant maximum 255, and
clamped system-volume setter respectively. Rockbox maps those normalized
values linearly across the target's real `SOUND_VOLUME` minimum and maximum.
All 20 observed resources are 11,025 Hz mono signed 16-bit little-endian WAV.
The compatibility layer mixes all active sound objects into stereo playback
blocks with signed saturation. Starting one handle restarts that sound object
without stopping distinct handles. The deterministic probe advances two
voices through one shared block; representative Stage 1 execution naturally
reaches four simultaneous voices, proving overlap is required stock behavior.

Required lifecycle:

1. If the game owns primary foreground audio, acquire the shared audio buffer
   through `plugin_get_audio_buffer()` without a preceding `audio_stop()`.
2. Use `PCM_MIXER_CHAN_PLAYBACK` for a game-owned mixed music/audio stream.
3. Use side channels only for short effects layered over deliberately retained
   user music.
4. Set the mixer frequency before starting output.
5. Stop stale channel state before installing callbacks.
6. Ensure the iPod 6G codec/MCLK wake path is respected.
7. Never replace the user's playlist to play a game's soundtrack.
8. Before seek, stream replacement, or buffer reuse, stop or pause callbacks
   that reference the old memory.
9. On exit, stop channels and direct PCM, wait until playback clears, remove
   callbacks, restore frequency/source state, and only then release shared
   memory.

If user music is intentionally allowed to continue, the game must either use a
safe short-effect channel or run without its background music. This policy must
be selected per compatibility profile, not inferred unpredictably at runtime.

Compressed M4A/AAC assets should use existing Rockbox codec facilities where
the plugin architecture permits. Otherwise the importer may predecode owned
assets into a documented local format. Predecoded output is local derived data
and must not be distributed.

## Runtime Scheduling and Time

The game must not take permanent control of the Rockbox thread without yielding.
The compatibility layer should expose:

- monotonic milliseconds or ticks based on Rockbox time;
- bounded sleeps/yields;
- a queued event pump;
- per-frame callback delivery;
- asynchronous file and audio completion queues;
- deterministic shutdown requests.

Timing compatibility profiles may adjust a game's expected tick frequency only
after measurement. Timing hacks must be keyed by executable hash.

## Saves and Compatibility

Save data belongs outside the read-only package tree. Requirements:

- one directory per game GUID;
- distinguish normal data from statistics when the original framework does;
- retain original filenames after safe normalization;
- write a save-format metadata file with executable version and compatibility
  profile;
- never erase a newer save automatically;
- provide import/export tools for the owner's stock `GameData_RW` and
  `GameStats_WO` directories;
- back up an existing save before any migration;
- test interrupted-write behavior.

Cross-compatibility with stock saves is a goal only after the file semantics
are verified. The loader must not claim compatibility based solely on matching
filenames.

## Diagnostics

### Runtime log

The default log path is:

```text
/.rockbox/ipodgames/logs/ipodgames.log
```

Use a bounded ring buffer and append a concise session record at shutdown.
Required fields:

- loader version;
- target and Rockbox version;
- game GUID, version, build, platform, and executable hash;
- load base, entry point, image size, arena use, and free memory;
- frameworks and ordinal coverage;
- last unsupported import;
- renderer frame statistics;
- input queue high-water mark;
- audio channel, frequency, buffer, and teardown state;
- exit reason.

Do not log authorization records, keys, account names, serial numbers, or game
asset contents.

### Research trace

Developer builds may enable a bounded binary framework-call trace. It must be
off by default because logging every graphics call would alter timing and wear
storage. A trace decoder should run on the host.

## Performance Requirements

Prototype minimums for a selected 2D game:

- launch to first framework trap in under 5 seconds;
- first rendered frame in under 10 seconds after successful initialization;
- at least 15 fps during representative gameplay;
- input-to-visible-response latency below 150 ms;
- no unbounded allocation after game entry;
- no frame write outside the 320x240 framebuffer;
- no callback referencing released plugin memory;
- save flush completes or reports failure before normal exit.

Product targets:

- 20-30 fps for 2D titles;
- stable audio without underruns during representative gameplay;
- clean exit and immediate working Database/Files playback;
- no permanent MMU, clock, codec, LCD, or playlist state changes.

Performance failure for a complex 3D game does not invalidate the architecture
if the selected 2D reference title meets the prototype gate.

## Security and Stability Model

An eApp is native privileged ARM code. Rockbox plugins do not provide a strong
process sandbox, so malformed or malicious executables can corrupt memory or
the filesystem despite parser validation.

Mitigations:

- accept only explicitly selected local files;
- hash every executable and display unknown-build status;
- validate all loader-controlled structures;
- expose only virtualized filesystem paths through framework wrappers;
- keep framework argument validation strict;
- use guard patterns around arena regions;
- check guards at every framework boundary in developer builds;
- prefer read-only MMU permissions for code if supported by the target setup;
- require an explicit developer option to run unknown hashes;
- start execution tests with synthetic fixtures, then known owned games.

The documentation must state clearly that the runtime is not a security
sandbox.

## Development Phases and Gates

### Phase 0: reproducible corpus inventory

Deliverables:

- read-only `.ipg` inspection tool;
- inventory of all preservation packages by GUID/build/platform/hash;
- inventory of installed owned Classic test games;
- encrypted package and authorization backups stored outside the repository;
- no plaintext commercial executable committed anywhere.

Gate 0 passes when the same inventory can be regenerated and package parsing is
covered by malformed synthetic fixtures.

### Phase 1: Classic retailOS analysis

Deliverables:

- locally decrypted, hashed Classic retailOS image;
- firmware-specific ELF/segment map;
- identified eApp loader path;
- identified framework registry and table widths;
- startup structure and load/relocation algorithm notes;
- initial ordinal database with confidence levels.

Gate 1 passes when at least one framework table and the loader's import-fixup
loop are independently confirmed in the Classic firmware.

### Phase 2: decrypted Classic eApp

Preferred methods, in order:

1. Instrument an authenticated stock loader and dump the plaintext image after
   decryption and relocation.
2. Recover the content-key unwrap and executable decryption path for local use.
3. Patch a temporary research retailOS to export one owned plaintext image.

Persistent firmware modification is not required or preferred. Any device
experiment must begin with verified backups and a documented recovery path.

Gate 2 passes when one Classic executable:

- has stable `eapp` structure;
- can be matched cryptographically to its encrypted package/build;
- yields a complete segment/import/relocation report;
- is retained only as private local research data.

This is the primary go/no-go gate.

### Phase 3: host loader and synthetic fixture

Deliverables:

- hardened eApp parser;
- framework database generator;
- synthetic ARM eApp fixture with known imports and relocations;
- relocation and import resolution tests;
- arena planner tests;
- deterministic malformed-file rejection.

Gate 3 passes when the synthetic image and the owned Classic image both parse
without format guesses that affect execution.

### Phase 4: hardware entry and framework traps

Deliverables:

- minimal `ipodgames.rock`;
- MMU alias or complete relocation support;
- game stack and entry/return trampoline;
- diagnostic import traps;
- reliable normal and emergency exit before enabling graphics/audio.

Gate 4 passes when the owned game repeatedly reaches the same first framework
calls on hardware and exits without rebooting or damaging subsequent Rockbox
operation.

### Phase 5: silent 2D gameplay

Deliverables:

- required lifecycle, settings, time, metadata, filesystem, and input calls;
- minimum OpenGL ES subset;
- first frame, menu flow, clickwheel input, and save support;
- per-frame performance instrumentation.

Gate 5 passes when a selected 2D game is playable for 15 minutes, can save and
reload, exits cleanly, and averages at least 15 fps.

### Phase 6: audio

Deliverables:

- recovered audio ordinals and signatures;
- decoded or streamed game audio;
- volume and pause behavior;
- complete mixer/buffer teardown;
- audio transition test results.

Gate 6 passes only after the mandatory real-device matrix succeeds:

- fresh boot -> game with sound;
- Database music -> game with sound;
- Files music -> game with sound;
- game -> Database music starts with sound;
- game -> Files music starts with sound;
- rapid game/menu/music switching does not freeze;
- volume changes do not create static or permanent mute;
- pause/resume works;
- MENU and emergency exit return to the expected menu.

### Phase 7: second and third games

Add one additional 2D game and one 3D game. Per-game hacks must be isolated in
hash-keyed profiles. Shared discoveries must be promoted into framework-level
behavior.

Gate 7 determines whether broad compatibility is economical. A full catalog
effort should proceed only if most new work is ordinal implementation rather
than invasive per-game binary patching.

## Test Strategy

### Host tests

- truncated package and executable at every structural boundary;
- oversized counts and arithmetic overflow;
- overlapping segments;
- invalid entry point and load base;
- malformed import stubs and pointer slots;
- duplicate/unknown framework identifiers;
- unknown relocation types;
- ZIP traversal and duplicate normalized paths;
- save-path traversal;
- deterministic metadata generation;
- framework database schema validation.

### Simulator tests

The Rockbox simulator cannot execute an ARM eApp natively. It should still test:

- browser and metadata UI;
- package/imported-layout discovery;
- arena planning;
- framework state machines with synthetic host callbacks;
- input translation;
- filesystem virtualization;
- software renderer using generated draw streams;
- save migration and diagnostics.

Do not add a full ARM emulator merely to make the simulator execute games.

### Hardware tests

- repeated launch/exit cycles;
- emergency exit at startup and during rendering/audio;
- hold switch and every clickwheel control;
- low-memory rejection;
- corrupt executable rejection;
- missing/corrupt asset behavior;
- save during normal and near-power-loss conditions;
- LCD sleep/backlight transitions if games request them;
- long gameplay thermal and battery behavior;
- all audio transitions required by
  `docs/plugin-audio-lifecycle-steering.md`.

### Regression artifacts

Keep only redistributable artifacts:

- synthetic eApps;
- parser JSON reports with hashes and secrets removed;
- generated renderer command streams;
- screenshots that do not contain undistributable assets unless kept outside
  the repository;
- timing and memory summaries;
- test scripts and expected exit codes.

## Device Safety and Deployment

Research that enters DFU, decrypts firmware, or reads memory must be documented
as tethered and non-persistent where that is true. Do not claim Classic CFW
support is stable until verified.

The runtime should ultimately be plugin-only. If implementation requires a
Rockbox core or plugin API change, normal hardware deployment rules apply.

For any iPod 6G firmware deploy:

- build the correct `ipod6g` target;
- copy `rockbox.ipod` to both the volume root and `.rockbox/rockbox.ipod`;
- verify both device checksums match the local build;
- use `tools/deploy_ipod6g_preserve_database.sh` for a full package deploy;
- preserve and verify all tagcache/database files;
- keep `tagcache_autoupdate` enabled;
- `sync` before eject.

Game import tooling must never replace the mounted `.rockbox` directory
wholesale.

## Ranked Risks

| Risk | Severity | Mitigation / decision point |
| --- | --- | --- |
| No reproducible decrypted Classic eApp | Critical | Gate 2; stop runtime work if unresolved |
| Framework ordinal meanings remain unknown | Critical | retailOS table recovery plus dynamic tracing |
| eApp relocation/startup differs from 5G sample | High | require Classic binary before loader execution |
| OpenGL ES software rendering too slow | High | prove one 2D title first; implement observed subset |
| Game directly accesses retailOS or MMIO | High | trace external pointers/calls; reject incompatible titles |
| Native game crash corrupts Rockbox | High | strict validation, known hashes, synthetic fixtures |
| Audio teardown breaks later playback | High | mandatory lifecycle and hardware transition matrix |
| Per-game behavior dominates shared ABI | Medium-high | stop broad rollout after Gate 7 if hacks proliferate |
| Save behavior is not stock-compatible | Medium | version saves and preserve backups |
| Shared audio arena conflicts with callbacks | High | acquire/release ordering and callback quiescence |
| MMU alias conflicts with Rockbox mappings | High | runtime map audit, save/restore TTB entries, TLB tests |
| 3D titles exceed ARM926 performance | Medium-high | classify as unsupported or reduced-quality individually |
| Legal redistribution mistakes | High | keep games, firmware, plaintext, and secrets external |

## Stop Conditions

Pause or terminate the project path if any of these is established:

- Classic eApps execute substantial game logic through direct retailOS/MMIO
  calls rather than framework imports.
- No owned Classic eApp can be recovered reproducibly.
- The loader requires redistributing Apple code or secret material.
- A simple 2D game's observed OpenGL ES subset cannot reach 15 fps after basic
  profiling and reasonable fixed-point optimization.
- Reliable MMU/relocation teardown cannot be achieved without destabilizing
  normal Rockbox operation.

Failure of an individual game, especially a 3D title, is not a project-wide
stop condition.

## Open Questions

1. Which Platform ID is selected on each Classic and Nano revision, even when
   multiple IDs reference the same binary?
2. Is the Classic eApp header identical to the decrypted Platform ID 1 sample?
3. What relocation types and startup arguments does the Classic loader use?
4. Is the game heap supplied by retailOS or described in the eApp image?
5. Are framework identifiers stable across Classic firmware revisions?
6. Which OpenGL ES calls are actually used by Ms. PAC-MAN?
7. Does any game access PowerVR or display MMIO directly?
8. Does `Audio` accept compressed assets, decoded buffers, or both?
9. Can existing Rockbox codecs be driven safely from this plugin runtime?
10. Are stock save files portable between authenticated devices and Rockbox?
11. Can an authenticated Classic loader be instrumented without persistent
    firmware modification?
12. Can the local FairPlay unwrap be implemented without coupling the runtime
    to sensitive authorization data?
13. Can executable code pages be made read-only with the current S5L8702 MMU
    layout while leaving import pointer slots writable?
14. Which emergency-exit chord is reliable without colliding with reset or game
    controls?

## Initial Work Order

Implementation should begin in this order:

1. Create `ipg_inspect.py` and inventory the preservation corpus.
2. Update to and validate the current official `wInd3x` Classic path.
3. Decrypt the owned Classic retailOS image without modifying device storage.
4. Implement the firmware-specific ELF mapper.
5. Find the eApp loader, framework registry, and import fixup loop.
6. Recover one owned Classic eApp, preferably Ms. PAC-MAN.
7. Freeze the Classic eApp structure and ABI findings in synthetic fixtures.
8. Implement the hardened host parser and framework database.
9. Build the hardware loader through the first diagnostic framework trap.
10. Add lifecycle, input, filesystem, metadata, and settings calls.
11. Implement the observed 2D OpenGL ES subset.
12. Add saves and complete silent-game Gate 5.
13. Implement audio under the mandatory lifecycle matrix.
14. Add Peggle as the second 2D title and Asphalt 4 as the first 3D feasibility
    test.

Keep full-catalog import UX and artwork handling minimal before Gate 5, except
for the required `iPod Games` Game Cover Flow index and package-cover extraction.
Do not begin complete OpenGL ES coverage before observing the first target's
actual call set.

## External Research References

- Preservation project:
  https://github.com/Olsro/ipodclickwheelgamespreservationproject
- Freemyipod retailOS documentation:
  https://freemyipod.org/wiki/RetailOS
- Freemyipod Classic 6G hardware page:
  https://freemyipod.org/wiki/Classic_6G
- Current wInd3x repository:
  https://github.com/freemyipod/wInd3x
- Historical iPod game reverse-engineering notes:
  https://github.com/Xlinka/iPodReverseEngineering
- Clicky hardware emulator, useful only as older PortalPlayer reference:
  https://github.com/daniel5151/clicky
- Macworld's contemporary Ms. PAC-MAN control description:
  https://www.macworld.com/article/185102/mspacman-2.html
- Pocket Gamer's contemporary click-wheel gameplay review:
  https://www.pocketgamer.com/ms-pac-man/review/

## Prototype Completion Definition

The prototype is complete when all of the following are true:

1. A privately held, user-owned Classic executable is reproducibly imported.
2. Its eApp structure, startup ABI, imports, and relocations are documented.
3. `ipodgames.rock` loads it on iPod 6G and executes native game code.
4. Unsupported framework calls fail through a named ordinal trap.
5. The selected 2D game renders correctly at 15 fps or better.
6. Clickwheel and buttons support complete basic gameplay.
7. Save data survives exit and relaunch.
8. Audio works without corrupting later Database or Files playback.
9. Normal and emergency exit restore MMU, LCD, input, clock, mixer, PCM, and
   shared-buffer state.
10. No commercial executable, asset, Apple firmware, or authorization secret is
    present in the repository or release package.
