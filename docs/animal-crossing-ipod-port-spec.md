# Animal Crossing for iPod 6G: Feasibility and Port Specification

- Status: Phase 0 in progress; reference, asset packer, simulator shell, and
  native target overlay are working
- Target: Rockbox on iPod Classic 6G (S5L8702)
- Reference game: Animal Crossing, USA GameCube revision 0 (`GAFE01_00`)
- Preferred upstream:
  [flyngmt/ACGC-PC-Port](https://github.com/flyngmt/ACGC-PC-Port), audited
  at `70794ae7a2d274ad7edd1a3b48903755fda02d72` (v0.9.2)
- Related decomp: [ACreTeam/ac-decomp](https://github.com/ACreTeam/ac-decomp),
  audited at `09ca8e8b5b24e6ab44047ee980cf0088ad7ecb4c`
- Research date: 2026-07-30

## 1. Decision

Use the GameCube PC-port fork as the behavioral and data reference. The
measured Phase 0 gate rejects linking the complete original engine on iPod,
so the target implementation is an iPod-native demake that selectively ports
gameplay/data code and consumes assets prepared from the original disc.

Do not start with the Nintendo 64 Animal Forest decomp. The audited
[zeldaret/af](https://github.com/zeldaret/af) revision contains roughly 6,800
MIPS assembly inclusions. It rebuilds the original ROM but is not yet a
portable C codebase. Completing and translating that work would be a separate
multi-year decompilation project before an ARM port could begin.

The GameCube PC port is the only credible current base because it already:

- runs the original game logic as native 32-bit C/C++;
- replaces the GameCube OS, DVD, card, audio, and GX interfaces;
- loads legally user-supplied GameCube media;
- contains byte-swapping and save-card support for a little-endian host; and
- has a working reference renderer and audio path against which Rockbox can be
  tested.

The unmodified PC port does not fit the iPod memory budget, and its OpenGL
renderer cannot be used on Rockbox. The reference remains essential for rules,
calendar behavior, content interpretation, visual comparisons, and save
research, but its GameCube memory arenas and JSystem/GX runtime are not part of
the shipping target.

## 2. Success definition

The minimum useful port must:

1. boot from assets extracted from a user-owned `GAFE01_00` disc image;
2. create or load a town;
3. maintain the original 60 Hz simulation and calendar behavior;
4. render outdoor and indoor scenes legibly at 320x240 output;
5. support dialogue, inventory, shops, doors, and save-and-quit;
6. play music and sound effects without breaking Rockbox playback ownership;
7. survive repeated launch, save, exit, and relaunch cycles on real hardware;
8. never ship or commit Nintendo game data; and
9. leave at least 2 MiB of measured worst-case memory headroom.

The first acceptable performance target is 60 simulation ticks per second,
15 displayed frames per second, and uninterrupted audio. A 20-30 displayed
fps target is desirable, but it must not be achieved by changing game time,
physics, audio timing, or the real-time clock.

## 3. Known platform limits

The target is much smaller than a desktop and lacks both an FPU and a GPU.

| Resource | iPod 6G fact | Port consequence |
| --- | --- | --- |
| CPU | ARM926EJ-S, ARMv5TE, up to 216 MHz | Soft-float and software rasterization are primary risks |
| RAM | 64 MiB physical | Rockbox and firmware reservations leave less for a plugin |
| Shared audio/plugin region | 56,964,904 bytes (54.33 MiB) in the audited hardware map | Overlay, game arenas, framebuffer, stacks, and caches must all fit here |
| Normal plugin buffer | 3 MiB | Too small; the plugin must use an overlay loaded into the shared audio region |
| Codec buffer | 1 MiB | Not enough to change the overall strategy |
| Display | 320x240 RGB565 | Natural output is 320x240; 160x120 can be doubled for a performance mode |
| Accelerators | no GPU, no hardware FPU | OpenGL and floating-point-heavy raster paths must be replaced |

The exact shared-region addresses from the current iPod 6G link map are
`0x085a88d8` through `0x0bbfc000`. These values must be re-measured for every
release build instead of being hard-coded as permanent ABI.

The existing Super Mario 64 plugin proves the required loading technique:
a small `.rock` loader obtains the shared audio buffer, loads an `.ovl` image
at its upper end, and gives the remaining lower portion to the game allocator.
Animal Crossing should reuse that architecture and its safety checks.

## 4. Why the PC port does not fit unchanged

The desktop port reserves:

- 24 MiB of GameCube main memory;
- 16 MiB of emulated ARAM;
- approximately 8.46 MiB of statically allocated extracted game assets;
- a large renderer vertex buffer of about 6 MiB;
- code, read-only data, other BSS, stacks, framebuffers, depth storage, audio,
  texture metadata, and temporary disc/DOL/REL buffers.

The first three items alone total about 48.46 MiB. They exceed a safe iPod
budget before executable code and a renderer are counted. The desktop renderer
also places texture and buffer storage in GPU memory, which the desktop process
measurements do not charge to CPU RAM. On iPod all of it becomes ordinary RAM.

The desktop asset loader temporarily holds the full DOL and decompressed REL
while copying data into global arrays. That peak must be eliminated even if
the steady-state allocations are reduced.

Therefore "compile the PC port for ARM" is not a viable implementation plan.
Phase 0 must measure actual live sets and prove that the following changes are
enough:

- direct indexed reads from a prepared asset pack, without whole-image staging;
- smaller measured main-memory and ARAM allocations;
- bounded streaming vertex batches;
- a small explicitly capped texture cache;
- no OpenGL-side duplication; and
- an overlay whose complete linked span is included in the budget.

## 5. Hard Phase 0 go/no-go gate

Do not vendor the full upstream tree into the Rockbox repository until this
gate passes. The spike can live outside the repository or in a small,
throwaway worktree.

### 5.0 Measurements completed on 2026-07-30

The pinned reference was built as a native 32-bit Linux executable with a
private 32-bit SDL build. Linux compatibility fixes were needed for modern
glibc headers, case-sensitive include paths, executable image bounds, JSystem
allocator symbol visibility, and PC arena alignment.

The validated user disc boots the reference at 60 displayed frames per second,
accepts controller input, and renders K.K.'s opening sequence. This proves the
disc revision and upstream source are a usable behavioral baseline.

Current linked desktop sections are:

| Section | Bytes |
| --- | ---: |
| text + read-only data reported by `size` | 9,710,376 |
| initialized data | 3,166,546 |
| BSS | 22,761,852 |
| total ELF load/static span | 35,638,774 |

The desktop process reached approximately 168.6 MiB RSS and 129.8 MiB private
dirty memory in the opening sequence. This includes Mesa, SDL, driver
allocations, and desktop renderer duplication, so it is not an iPod budget.
It is evidence that the desktop process itself cannot be transplanted
unchanged.

The private asset pipeline has also been exercised against the supported disc:

| Pack measurement | Result |
| --- | ---: |
| DOL payload | 918,720 bytes |
| decompressed REL payload | 15,640,056 bytes |
| disc filesystem payloads | 10 files |
| total indexed entries | 12 |
| final pack | 43,094,651 bytes |

The pack is generated outside the repository and the iPod 6G simulator
validated every payload checksum through the Rockbox filesystem API.

The first native ARM overlay now links and occupies 10,092 bytes including
BSS. It is only the Phase 0 target shell and verifier, not the game engine.
Its load span is `0x0bbf9490` through `0x0bbfbc00`, below the audited shared
buffer ceiling. The simulator reports a 63,469 KiB arena; physical hardware
must supply the authoritative shared-memory measurement.

Instrumenting the reference heap during K.K.'s opening produced the decisive
native-engine result:

| Runtime pool | Peak or reservation |
| --- | ---: |
| system heap capacity | 25,152,880 bytes |
| system heap peak used | 25,114,376 bytes |
| system heap minimum free | 38,504 bytes |
| emulated ARAM high-water | 16,777,216 bytes |

The original main-memory arena therefore cannot be reduced meaningfully even
in the opening workload, and ARAM reaches its entire configured extent. These
two pools consume about 40 MiB before the 35.6 MiB desktop linked/static span,
software frame/depth buffers, texture cache, stacks, or Rockbox overhead.
Eliminating the roughly 6 MiB desktop vertex buffer and known static asset
duplication is not enough to close the gap.

**Phase 0 decision:** no-go for the complete decomp/PC-port engine on iPod 6G.
Proceed with the user-approved demake architecture. This is not permission to
substitute invented assets: art, audio, text, item/villager definitions, and
other copyrighted content must still come from the user's validated pack.
Original behavioral code may be selectively ported where it is small and does
not pull in the GameCube arena/JSystem dependency graph.

### 5.1 Measurements

Build the audited PC port with a user-owned USA revision 0 disc and add
instrumentation for:

- main heap allocation high-water mark and minimum free space;
- ARAM address high-water mark, live ranges, and DMA access locality;
- exact linked text, rodata, data, and BSS sizes;
- static asset array sizes and which scenes reference them;
- maximum vertices, triangles, draw calls, texture count, and texture live set;
- per-frame game-logic, audio-mixing, and rendering time;
- maximum temporary allocation during boot, town load, travel, and save;
- file reads and decompression working-set size; and
- audio production cost and underruns.

Capture at least these workloads:

1. boot through the title screen;
2. create a new town;
3. walk around a populated outdoor acre in rain or snow;
4. enter the player's house, museum, shop, and train station;
5. open inventory and dialogue;
6. fish or catch an insect;
7. save and reload;
8. travel using slot B; and
9. run across an hourly music transition.

### 5.2 Hardware spikes

Produce three deliberately incomplete iPod experiments:

1. a headless ARM overlay containing the game and platform layer, to obtain
   real ARM linked sizes;
2. a logic/audio benchmark on the physical iPod, to expose soft-float CPU cost;
3. a standalone captured-GX-trace player using the proposed software renderer.

The renderer spike must include a busy outdoor trace, alpha-tested foliage,
text/dialogue, fog, lighting, and at least one render-to-texture or indirect
texture effect if the captured game workload uses one.

### 5.3 Pass criteria

Proceed only when all of these are true:

- the full worst-case memory model is no more than 52 MiB;
- at least 2 MiB remains free after overlay, arenas, frame/depth buffers,
  caches, audio, stacks, and the largest measured temporary allocation;
- game logic plus audio completes within a 16.67 ms simulation interval on
  physical hardware, with enough time left to render at least every fourth
  tick;
- the trace renderer sustains at least 15 displayed fps in the representative
  outdoor scene;
- there are no audio underruns during a 30-minute headless run; and
- no essential game subsystem requires an unimplemented GameCube facility
  that invalidates the architecture.

If memory passes but CPU does not, profile before optimizing. If the combined
logic and audio workload alone cannot sustain 60 Hz after targeted fixed-point
and hot-loop work, stop the native port. Lowering the simulation rate would
make the clock, movement, audio, and scripted behavior unreliable.

## 6. Target architecture

```text
animalcrossing.rock (small loader)
        |
        +-- obtains shared audio buffer
        +-- loads animalcrossing.ovl at the top
        +-- gives the lower region to the game allocator
                         |
                         v
       iPod-native demake core + selected portable game rules
                         |
       +-----------------+------------------+
       |                 |                  |
 Rockbox save/RTC   2D scene renderer Rockbox audio
 and pack reader    + bounded sprites producer+mixer
       |                 |                  |
 assets.pack/save   RGB565 framebuffer PCM_MIXER_CHAN_PLAYBACK
```

Port original rules and data tables only when they have a bounded dependency
closure. The demake owns its compact world state, scene renderer, input model,
and persistence format. The audited reference is used to verify behavior; it
is not linked as an opaque compatibility layer.

Suggested repository layout after Phase 0:

```text
apps/plugins/animalcrossing/
    animalcrossing.c          small loader
    animalcrossing.make
    rockbox/
        rb_alloc.c
        rb_audio.c
        rb_card.c
        rb_disc.c
        rb_input.c
        rb_os.c
        rb_rtc.c
        rb_video.c
        rb_gx_*.c
    upstream/                 pinned, documented PC-port source
tools/
    animalcrossing_prepare_assets.py
docs/
    animal-crossing-ipod-port-spec.md
```

The precise vendoring mechanism must preserve the upstream commit and license
notices, and make future upstream synchronization reviewable.

## 7. Build and memory design

### 7.1 Overlay

Follow the SM64 loader/overlay pattern:

- tiny loader remains in the normal plugin buffer;
- loader calls `plugin_get_audio_buffer()` without calling `audio_stop()`;
- overlay is loaded at the aligned top of that region;
- allocator arena ends below the overlay with guard space;
- load addresses, lengths, and arithmetic are checked for overflow;
- the plugin refuses to start if the current Rockbox buffer is too small.

The build must emit a machine-readable memory report with section sizes,
overlay start/end, arena size, and configured caches. CI should reject a build
that exceeds the Phase 0 cap.

### 7.2 Target budget

This is a design target, not a claim that the current source meets it:

| Component | Initial cap |
| --- | ---: |
| Overlay text + rodata + initialized data | 5.5 MiB |
| Overlay BSS and unavoidable static assets | 8.5 MiB |
| GameCube main-memory arena | 18 MiB |
| ARAM or ARAM cache | 12 MiB |
| RGB565 framebuffer, depth, GX state, raster work | 1.0 MiB |
| Decoded texture cache | 1.0 MiB |
| Audio rings, thread stacks, logging, guards | 0.5 MiB |
| Temporary/load reserve and unclassified overhead | 2.0 MiB |
| Total target | 48.5 MiB |

Caps must be revised from measurements. They are enforced maximums, not pools
that every subsystem should eagerly allocate.

### 7.3 Main memory

The game's main-memory arena contains directly referenced pointers and cannot
be transparently paged. Instrument the desktop port and reduce its 24 MiB
reservation only if every required workload proves a smaller high-water mark.
Use guard regions and fail-fast allocation diagnostics during development.

### 7.4 ARAM

The desktop fork models 16 MiB of ARAM, while its default audio/graph
partitions account for most of that space. First determine which ranges are
actually live and how often they are accessed.

Preferred implementations, in order:

1. a reduced contiguous ARAM array if the live high-water fits;
2. a sparse page table with a 2-4 MiB RAM cache and file-backed dirty pages;
3. subsystem-specific streaming that removes unused graph/audio residency.

A file-backed implementation must use a writable work file separate from the
read-only asset pack. It must remain correct when pages are dirty, must avoid
writing every frame, and must tolerate an interrupted session without
corrupting game assets or saves.

## 8. Asset preparation and legal boundary

No Nintendo ROM, ISO, CISO, DOL, REL, textures, audio, extracted source data,
save files, generated asset arrays, or backup archives may be committed or
distributed with the repository.

The host preparation tool should:

1. accept an ISO, GCM, or supported CISO supplied by the user;
2. verify disc ID `GAFE01` and revision `00`;
3. validate known executable hashes where available;
4. extract only the indexed byte ranges required by the port;
5. perform any one-time decompression or endian conversion that reduces iPod
   CPU and temporary-memory cost;
6. create a versioned, checksummed `assets.pack` and index;
7. record the upstream port commit and pack-format version; and
8. never search for or download copyrighted game media.

Runtime layout:

```text
.rockbox/animalcrossing/
    assets.pack
    assets.index
    config.cfg
    save/
        slot-a/
        slot-b/
    cache/
```

All generated/runtime paths must be covered by repository ignore rules before
the tool is documented for use. The packer must write to a temporary file and
rename only after verification.

The runtime reader must fetch individual indexed ranges directly into their
final destinations. It must not reproduce the desktop behavior of loading the
entire DOL and decompressed REL at once.

## 9. Software GX renderer

This is the largest implementation task.

The PC port translates GameCube GX state to OpenGL 3.3 and shader-generated
TEV stages. Rockbox needs a CPU renderer that consumes the same high-level GX
calls. The embedded N64-derived display lists in Animal Crossing are already
translated to GX by the game/port, so they do not require a second complete
renderer.

### 9.1 Required pipeline

- model/view/projection transforms;
- viewport, scissor, clipping, culling, and depth test/write;
- streaming indexed triangles and quads;
- RGB565 color and a 16-bit depth buffer;
- vertex color, normals, basic lighting, and fog;
- alpha compare and supported blend modes;
- nearest texture sampling initially;
- GameCube tiled texture formats I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8,
  CI4/C4, CI8/C8, and CMPR;
- palette lookup for indexed textures;
- up to the three active TEV stages used by the audited port;
- the copy-to-texture/EFB behavior actually observed in captured traces; and
- indirect texture operations observed in required gameplay.

Do not allocate the PC port's 65,536-entry, roughly 6 MiB expanded vertex
buffer. Translate and rasterize bounded batches, initially 512 or 1,024
vertices, flushing on state change or capacity.

### 9.2 Numeric approach

Use fixed-point arithmetic in transform, edge, interpolation, depth, and TEV
hot paths where profiling proves it useful. Preserve sufficient precision to
avoid seams, unstable depth, and text corruption. Small cold-path matrix work
may remain soft-float until measurements justify conversion.

Start with perspective-correct texture coordinates. Affine mapping is
acceptable only as a temporary trace-player milestone because outdoor ground
and building geometry will expose distortion.

### 9.3 Texture memory

Keep source textures in their compact GameCube representation in the asset
pack or game memory. Decode into a capped RGB565/ARGB cache only when direct
sampling is too slow. Begin with a 1 MiB LRU cache and expose hits, misses,
evictions, bytes, and decode time in telemetry.

Never create unbounded decoded copies. Alpha-bearing formats may need an
RGB565-plus-alpha representation or ARGB1555/4444 intermediate selected per
texture.

### 9.4 Resolution and frame scheduling

The simulation, audio, and RTC must not depend on displayed frame rate.

- quality mode: render at 320x240;
- performance mode: render at 160x120 and scale 2x using the SM64-style path;
- run game logic at 60 Hz;
- render every second through fourth simulation tick based on a bounded,
  non-oscillating frame-skip policy;
- never skip audio production or advance the calendar from render count.

Because world and interface draws share the GX stream, a mixed-resolution
world with full-resolution UI is a later optimization and requires reliable
draw classification. It is not assumed by the initial plan.

### 9.5 Renderer verification

Add a host trace recorder to the OpenGL reference port and a replay tool for
the Rockbox renderer. Each trace contains GX state, vertex data, referenced
textures, and expected frame output without containing more copyrighted data
than the local user-generated test fixture requires.

Compare reference and software frames using pixel difference thresholds and
targeted screenshots. Do not commit traces or screenshots containing Nintendo
assets to the public repository.

## 10. Audio

Follow `docs/plugin-audio-lifecycle-steering.md`.

The upstream port produces 32 kHz stereo audio in software. The Rockbox backend
should retain that rate and feed `PCM_MIXER_CHAN_PLAYBACK`.

Recommended structure:

- one producer thread runs the existing per-frame game/DSP audio work;
- a small ring contains 8-16 blocks of 256 stereo frames;
- the Rockbox mixer callback only hands off completed blocks;
- callback code never allocates, performs file I/O, or invokes game logic;
- underrun/overrun counters are visible in the profiling log;
- volume remains under Rockbox control.

If a producer thread adds more scheduling overhead than it removes, a
main-loop producer is acceptable after measurement. In either design, audio
must remain decoupled from whether a video frame is drawn.

Lifecycle order is mandatory:

1. obtain the shared audio buffer through normal plugin ownership;
2. configure and start the playback mixer channel;
3. on exit, pause, USB, or error, prevent new callback work;
4. stop the producer and join its thread;
5. stop and drain the mixer channel;
6. restore any changed frequency or audio state; and
7. only then call `plugin_release_audio_buffer()`.

Never mutate the user's playlist. Validate Database music -> game, Files music
-> game, game -> Database music, game -> Files music, volume changes, pause,
rapid relaunch, and error exits. Include the upstream sound-effect failure
after returning from another town in the port's regression list.

## 11. Input mapping

Initial mapping:

| GameCube input | iPod input |
| --- | --- |
| Left stick | touch position around the wheel |
| A | Center |
| B | Play |
| X | Previous |
| Y | Next |
| Start | short Menu press, emitted on release |
| C-stick left/right | counter-clockwise/clockwise scroll events |
| L/R | Menu + Previous/Next |
| Z | Menu + Center |
| D-pad | Menu + Play toggles a temporary D-pad wheel mode |
| Rockbox pause/exit overlay | long Menu press |

Do not emit Start until Menu is released, so a long press cannot both pause the
port and press Start in the game. The Hold switch should pause safely and open
the Rockbox overlay rather than terminating in the middle of a card write.

The first-run help screen must explain controls. Configurable remapping is a
post-MVP feature. Game text entry should use the game's on-screen keyboard with
wheel/stick movement and A/B; desktop keyboard shortcuts are out of scope.

USB connection must pause the game, finish or abort any atomic save operation,
close files, stop audio in lifecycle order, release memory, and return
`PLUGIN_USB_CONNECTED`.

## 12. Save cards and real-time clock

Preserve upstream GCI/card compatibility where practical, with separate
slot-A and slot-B directories. Slot B matters because travel is part of the
base game and already exposes upstream compatibility defects.

Save requirements:

- all writes use a temporary file, flush, verify, then atomic rename;
- an interrupted write never replaces the last known-good card;
- pack/cache failures cannot modify card files;
- card format and byte-swapping are covered by host tests;
- low-space and read-only-volume errors are reported in-game or in a clear
  Rockbox error screen;
- no save or backup file is tracked by Git.

Avoid accumulating timestamped backup files. If recovery beyond atomic rename
is needed, keep at most one ignored previous-generation journal and rotate it
only after the new card validates.

Animal Crossing's calendar is gameplay state, not decoration. Use Rockbox wall
clock only for calendar/RTC queries and the monotonic tick counter for frame
timing. Convert local date/time to the GameCube epoch with tested leap-year,
month, and rollover logic. Daylight-saving or manual clock changes may change
the in-game wall clock but must never create a giant simulation delta.

## 13. Feature staging

### MVP

- logo, title screen, new town, and load;
- outdoor acres and essential interiors;
- player movement and camera;
- dialogue, inventory, item pickup/drop, doors, and shops;
- music, ambient sound, and sound effects;
- save-and-quit and RTC/calendar;
- performance log and safe exit.

### Compatibility phase

- seasons, weather, day/night, hourly music, and scheduled events;
- fishing, insects, fossils, museum, mail, patterns, and errands;
- train travel and slot B;
- island behavior that does not require external hardware;
- extended soak and save-card compatibility.

### Deferred

- built-in NES furniture games/FixNES;
- Game Boy Advance, e-Reader, and physical link-cable functions;
- PC-only enhancements;
- high-quality filtering and optional renderer effects;
- arbitrary non-USA disc revisions.

Deferred hardware features must fail gracefully and be documented. They are
not allowed to block ordinary town play or corrupt saves.

## 14. Milestones and exit criteria

### Phase 0: feasibility

Complete the measurements and spikes in Section 5. Exit only on a documented
go/no-go decision with hardware data.

### Phase 1: source and headless boot

- pin licenses and upstream commit;
- add overlay build and minimal Rockbox platform layer;
- prepare user assets without whole-image boot staging;
- reach the main game loop headlessly;
- prove memory guards and clean exit.

Exit: repeat 50 headless launches on hardware with no leak or crash.

### Phase 2: GX trace renderer

- implement state machine, texture decode, transforms, raster, depth, alpha,
  blend, TEV, lighting, and fog as required by traces;
- add 160x120 and 320x240 modes;
- establish reference-image tolerances and performance telemetry.

Exit: representative title, interior, outdoor, dialogue, and weather traces
meet correctness and 15 fps performance gates.

### Phase 3: interactive title and town

- connect live GX stream and input;
- boot to title;
- create a town and enter gameplay;
- make menus and text legible;
- handle Hold and clean quit.

Exit: a 30-minute outdoor/interior session without crash or memory growth.

### Phase 4: audio, save, and RTC

- integrate playback mixer lifecycle;
- implement card and atomic persistence;
- implement tested RTC conversion;
- validate hourly transitions and relaunch.

Exit: save/reload is byte-stable where expected, a one-hour audio soak has no
underruns, and all Rockbox playback-transition tests pass.

### Phase 5: compatibility and performance

- cover the compatibility feature list;
- fix scene-specific renderer gaps;
- optimize measured ARM hot paths;
- validate travel, weather, events, and long sessions.

Exit: the full test matrix passes and memory headroom remains at least 2 MiB.

### Phase 6: packaging

- document asset preparation and controls;
- add ignore rules before generated data is produced in-tree;
- add license/attribution and upstream sync notes;
- produce reproducible simulator and iPod builds;
- conduct a clean-room packaging check for copyrighted data.

Exit: a user with a valid supported disc can build, prepare assets, install,
play, save, exit, and resume using only documented steps.

## 15. Test matrix

### Functional

- title, new town, each player slot, save, reload, and erase flow;
- outdoor acres, house, shop, museum, station, inventory, dialogue, and mail;
- fishing, insects, item interactions, patterns, and text entry;
- day/night, each season, rain, snow, fog, and scheduled events;
- train/slot-B transitions and return;
- every supported texture format and observed TEV path.

### Lifecycle

- 50 repeated launch/exit cycles;
- exit from title, outdoor, interior, dialogue, inventory, and save screens;
- Hold and USB during ordinary play and during save;
- full disk, missing pack, bad checksum, truncated cache, and read-only storage;
- Database/Files playback transitions required by the audio steering document;
- pause, volume changes, rapid relaunch, and abnormal initialization failure.

### Endurance

- one-hour busy outdoor play;
- six-hour idle/day-transition soak;
- repeated indoor/outdoor and acre transitions;
- hourly music transition;
- repeated save/load and slot-B travel.

Run host builds with address and undefined-behavior sanitizers. Run simulator
tests for deterministic input and renderer replay, but treat physical iPod
results as authoritative for memory, scheduling, audio, and speed.

## 16. Telemetry

An opt-in development log at
`.rockbox/animalcrossing/animalcrossing_profile.log` should include:

- simulation, raster, and audio time per frame;
- displayed fps and skipped frames;
- vertices, triangles, draw calls, and state flushes;
- texture cache hits, misses, evictions, and decode time;
- main-memory minimum free and ARAM high-water/cache I/O;
- overlay and subsystem memory totals;
- audio underruns/overruns;
- asset reads, bytes, and latency; and
- current scene identifier sufficient for profiling.

Do not log save contents, player/town text, or host paths. Logs must be bounded
or explicitly started by a developer build so they cannot fill the iPod.

## 17. Principal risks

| Risk | Severity | Response |
| --- | --- | --- |
| Logic plus audio is too slow under ARM soft-float | Critical | Phase 0 hardware benchmark; convert measured hot paths; stop if 60 Hz logic is unattainable |
| Software GX renderer is too slow | Critical | Trace-first implementation, fixed-point hot paths, frame skip, 160x120 mode, bounded batches |
| Steady-state or load peak exceeds 54.33 MiB | Critical | Direct asset reads, measured arena shrink, sparse ARAM, strict linked budget |
| Renderer semantics are incomplete | High | Trace/replay against OpenGL reference; build features from observed workloads |
| Upstream pre-1.0 gameplay bugs | High | Pin revision, track inherited issues, maintain targeted regressions |
| Save corruption | High | Atomic validated writes, one-generation journal at most, power-loss tests |
| Audio callbacks outlive plugin memory | High | Required stop/join/drain/release order and repeated lifecycle tests |
| Text is unreadable at 160x120 | Medium | Keep 320x240 quality mode; investigate draw-classified UI only after correctness |
| Disc revision mismatch | Medium | Strict `GAFE01_00` verification initially |
| Copyrighted data enters Git/package | Critical | User-side packer, ignores, artifact scan, no committed traces/assets/saves/backups |

## 18. Known upstream compatibility work

The audited PC port is pre-1.0. Its open reports include sound effects stopping
after visiting another town, freezes around patterns/fish, a Porter softlock,
an invisible aerobics radio, timing/audio discrepancies, password
incompatibility, and incomplete NES/slot-B behavior. These are inherited risks,
not Rockbox-specific regressions.

Maintain a pinned upstream-issues document when implementation starts. Confirm
each report against the exact pinned revision before copying a fix, and keep
platform work separable from gameplay corrections.

## 19. Definition of done

The port is release-ready only when:

- every Phase 0 pass condition still holds in the release build;
- the MVP and agreed compatibility test matrix passes on physical iPod 6G;
- worst-case measured free memory is at least 2 MiB;
- simulation stays at 60 Hz and audio has no underruns in endurance tests;
- save/card recovery tests pass under interrupted writes;
- Rockbox audio ownership and playback-transition tests pass;
- install and asset preparation are reproducible from a clean checkout;
- generated assets, saves, caches, logs, and backups are ignored and absent
  from Git history and release packages; and
- licenses, upstream commits, unsupported hardware features, controls, and
  known defects are documented.

## 20. Immediate next action

Implement only the Phase 0 desktop instrumentation and hardware spikes.
Do not begin the full renderer or import the 90 MiB upstream source tree until
the memory and CPU gates have produced a defensible go decision.
