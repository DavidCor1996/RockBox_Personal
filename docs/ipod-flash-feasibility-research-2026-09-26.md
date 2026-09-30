# iPod Flash and Stick RPG feasibility research

Research date: 2026-09-26. Scope: standalone execution of original SWF games on Rockbox iPod Classic 6G/7G, with comparison to iPod Video 5G/5.5G.

**Conclusion:** Faithful Stick RPG Complete is a plausible engineering target, but this tree does not establish that it works correctly or at acceptable speed on hardware. A curated collection of older AVM1 games is plausible. Broad, faithful, performant support for arbitrary Flash content is not a realistic commitment on these iPods. There is no ready-made engine replacement that removes both compatibility and hardware constraints.

This was a source audit, an independent parse of the bundled SWF, inspection of an existing simulator log and test scripts, and primary-source engine research. No firmware changes, deployment, new runtime test, or hardware benchmark was performed. Historical log output is not a measurement of the current build. The user's report of broken gameplay is not contradicted by startup or save-file smoke checks.

## What the actual game requires

Inspected asset: `tools/ios/RockPodLink/Resources/SimulatorInstall/.rockbox/flash/stickrpg/stickrpg.swf`.

SHA-256: `3bdfaaba4f980be19575e90bc925ee83a2999f4712085e576681c70b69b85605`.

| Property | Independently measured result |
|---|---|
| Container | CWS, SWF version 6 |
| File size | 2,540,508 bytes |
| Inflated size, including header | 3,054,794 bytes; matches declared length |
| Stage | 550 × 400 |
| Authored rate | 35 frames/second |
| Root timeline | 162 frames |
| Nested sprite definitions | 289 |
| DoAction blocks | 309 |
| DefineButton2 | 263 |
| Static text / editable text definitions | 432 / 300 |
| DefineSound | 33: 26 MP3, 7 ADPCM |
| DoABC / embedded video tags | None |

These are recursive tag counts, not counts of concurrently visible or active objects. Sound-related tags in the existing runtime log total 340; that does not mean 340 separate sound assets. Parsing confirms format and workload, not correct execution of the ActionScript or availability of external resources.

This game needs AVM1, the older ActionScript execution model. It does not need AVM2/ActionScript 3 or a video decoder for its embedded content. It does need correct object lookup, nested timelines, event ordering, buttons, keyboard/mouse input, dynamic text, saved objects and sound. Its simple-looking artwork should not be mistaken for a simple program.

## What is already implemented

The current implementation is significantly beyond the early bring-up description in `docs/flash-cxx-plugin-port-spec.md`. It includes GameSWF, a software renderer, image decode integration, timeline execution, input mapping, text adaptation, shared-object persistence and cache files. Rebuilding an initial SWF parser or converting the engine from C++ to C would repeat work without addressing the primary problems.

Useful source locations:

- `apps/plugins/flashplayer/flashplayer.cpp`: renderer, game adaptation, loading, persistence, input and playback loop.
- `apps/plugins/flashplayer/ipod_engine.cpp`: target gate, memory ownership, CPU boost and frame clock.
- `apps/plugins/flashplayer/gameswf/`: imported runtime with local changes.
- `tools/stickrpg_authentic_menu_sim_gate.py` and `tools/stickrpg_save_sim_gate.py`: narrow simulator checks.
- `rockpod/tests/test_flashplayer_stickrpg_hardware_policy.py`: source-text assertions about selected policies and implementation patterns.

The last category is not a behavioral Flash conformance suite. A passing assertion that particular source text exists cannot establish correct gameplay.

## Why the present implementation cannot be considered faithful

The following are direct source findings. They explain concrete limitations; they do not identify the first failing opcode in a reproduced session.

1. **Gameplay state is overwritten by the host.** `stabilize_stickrpg_spawn()` at approximately line 4461 sets cash to 100, day to 1 and karma to 0, and refills health or substitutes default health. The normal gameplay-entry path invokes it around line 4943, even with priming and the gameplay shortcut disabled. It also hides specific display objects. The comment associates the workaround with fast-forwarded startup, but its normal-entry call is broader.

2. **Save restoration has a particularly strong risk.** A fresh session resets the stabilization flag. If Continue restores a character and reaches the ordinary gameplay frame, the same stabilization path can overwrite restored values. The reload gate around lines 170–183 only waits for cursor and shared-object-load markers and checks for load errors. It does not compare the restored character's cash, day, karma, health, inventory or location against the saved values. This is a source-supported failure mechanism that needs a behavioral reproduction, not a claim that such a reproduction was performed here.

3. **World behavior is partially recreated outside ActionScript.** `sync_stickrpg_world()` around line 4515 positions seven named map clips using hard-coded offsets and visibility thresholds. It also forces both cars to off-screen coordinates every eligible update because of a documented transform problem. This directly changes gameplay and prevents traffic behavior from matching the original.

4. **Animation and clock behavior are repaired externally.** `sync_stickrpg_clock()` addresses a particular sprite by depth, character ID and frame count, seeks it and stops it. `prepare_stickrpg_person()` around line 3912 forces the walking frame range when the runtime wanders into other artwork. These are evidence of unresolved timeline semantics, rather than general Flash compatibility.

5. **Audio is intentionally absent.** `SilentSoundHandler` around line 1472 accepts sound operations but does not decode or play anything. Adding a volume setting cannot fix this. Sound completion and timing semantics also need consideration when implementing a real backend.

6. **Rendering has fidelity limits.** The player sets the curve error tolerance to 24.0 where GameSWF's source default is 1.0, drops triangles beyond a 32,000-per-frame budget, and has a separate mesh-generation budget. Its mask backend has one buffer and resets it for each submission; nested-mask behavior needs explicit testing. Antialiasing enable requests are ignored. None of these observations alone proves which artifact the user has seen.

7. **Text is a constrained adaptation.** The Stick RPG bitmap-text bridge has fixed span/length limits and replaces non-ASCII printable characters with `?`. The character-name helper writes a fixed name. This is not complete editable text/font support for arbitrary games.

8. **Host integration is game-specific.** The movie URL callback returns a fixed Stick RPG URL. The code installs a `doneIntro` callback to emulate a missing original container callback. Recreating an actual container contract can be legitimate, but that claim should be verified against the original wrapper or reference execution. It does not justify arbitrary changes to game state, and the fixed URL is unsuitable as a universal Flash hosting policy.

Some old shortcuts are disabled by default on hardware. It would be inaccurate to say all debug hacks run on every launch. The findings above distinguish that from the normal gameplay path, which still performs several interventions.

## Hardware and resource feasibility

The local Classic build targets ARM926EJ-S and 64 MiB RAM. Its S5L8702 maximum CPU frequency is 216 MHz. Sources: `build-hw-ipod6g/Makefile`, `firmware/export/config/ipod6g.h`, and `firmware/target/arm/s5l8702/system-target.h`. The adjacent 266 MHz definition is for S5L8720, not evidence that this Classic target runs at 266 MHz.

At 35 authored frames/second the entire frame budget is about 28.6 ms, or 6.17 million CPU cycles at 216 MHz. ActionScript, timeline advancement, drawing, sound and Rockbox scheduling all share that budget. That arithmetic is a budget, not a measured performance result. The existing Flash renderer is CPU-based; this port does not provide a general GPU rendering backend.

One 320×240 RGB565 framebuffer costs 153,600 bytes. A one-byte-per-pixel mask adds 76,800 bytes. Those surfaces are affordable; object graphs, meshes, fonts, decoded images and allocation overhead can be much larger. A 2.54 MB SWF does not imply a 2.54 MB working set.

The archived log at `tools/ios/RockPodLink/Resources/SimulatorInstall/.rockbox/flash/flashplayer.log` reports a roughly 60,336 KiB heap after retaining the inflated movie, falling to 18,777 KiB free at root readiness. That is about 40.6 MiB of heap consumption before later reclamation. It later reports roughly 31 MiB free in gameplay. Its addresses and autorun markers identify host simulator evidence, despite its model label saying iPod6g. Object sizes, allocator behavior and available memory differ on 32-bit hardware; do not treat these figures as device measurements.

The same log reaches a gameplay frame and flushes saved data with no reported engine errors. This proves only that a previous instrumented run reached those milestones. It contains spawn stabilization and does not prove faithful state, full gameplay, visual quality or speed.

The plugin takes the shared audio buffer, so simultaneous user-music playback is not a supported assumption. Any future audio implementation must follow the repository's audio-lifecycle guidance, own its buffers correctly and restore normal music playback on exit.

iPod Video is a separate, less favorable target: the PP target maximum is 80 MHz, and the current `ipod_engine_get_profile()` enables native support only for `IPOD_6G`. A built 5G plugin file is not proof that the plugin accepts or runs on that hardware. Establish Classic compatibility and resource use first; memory variants and the slower processor require separate validation for Video models.

## Engine alternatives

| Approach | Assessment for this project |
|---|---|
| Repair and constrain current GameSWF | Lowest integration cost. Plausible for Stick RPG and a selected AVM1 catalog. Requires real semantic fixes and regression tests; unsuitable as a promise of universal compatibility. |
| Port Ruffle core | Strong candidate for desktop reference behavior. Native iPod use requires substantial runtime/platform and rendering work, followed by actual RAM and CPU measurements. Not a drop-in replacement. |
| Port Gnash | Useful AVM1 reference and test source, with embedded history and software renderers. Still incomplete; its POSIX/C++ and backend dependencies require work in bare-metal Rockbox. No demonstrated advantage for this game without a comparison. |
| Port Lightspark | Current project supports all ActionScript language versions, but remains incomplete and documents desktop dependencies including OpenGL, SDL2 and FFmpeg libraries. Large adaptation effort, with no evidence it will fit the iPod budget. |
| Adobe Flash Lite | Historical evidence that constrained-device Flash is possible. Adobe documented mobile ActionScript 2 support. That does not supply a usable Rockbox runtime or establish compatibility with arbitrary desktop SWFs. |
| Translate or remake individual games | Potentially much faster for one title. Dynamic ActionScript prevents a simple universal SWF-to-C conversion. A remake is a different deliverable from running the original SWF. |
| Render on a computer and stream to iPod | Moves the Flash workload off-device. Potentially useful in a connected system, but does not fulfill standalone iPod Flash support. |

GameSWF's author explicitly describes incomplete ActionScript compatibility and a design originally aimed at game UIs using hardware graphics APIs. Its historic speed claims cannot be transferred to this software-rendered port. The local fork has evolved, so the author's old feature list is context rather than an exact inventory of this tree. [GameSWF project](https://tulrich.com/geekstuff/gameswf.html), [Marmalade port](https://github.com/marmalade/gameswf).

Ruffle currently reports AVM1 language/API coverage of 99%/82%, and AVM2 coverage of 90%/82%. These are project-wide implementation indicators, not percentages of games guaranteed to work. Its render-backend interface permits a custom implementation, so lack of a GPU is not a logical impossibility. However, its source uses Rust standard-library facilities and a substantial dependency graph; simply compiling it for ARM is not a Rockbox port. The inspected core features do not expose a simple AVM1-only switch. [Compatibility](https://ruffle.rs/compatibility), [core dependencies/features](https://raw.githubusercontent.com/ruffle-rs/ruffle/master/core/Cargo.toml), [render interface](https://raw.githubusercontent.com/ruffle-rs/ruffle/master/render/src/backend.rs).

Gnash's manual describes primarily SWF 7 support with incomplete later coverage, POSIX/C++ platforms and selectable rendering/media backends. Its old documentation is historical evidence, not a current compatibility measurement. [Gnash manual](https://www.gnu.org/software/gnash/manual/gnashref.html).

Lightspark's current README explicitly supports all ActionScript language versions but also acknowledges that many applications remain unsupported. It lists LLVM JIT as optional and disabled by default; it would be incorrect to reject it on a claim that LLVM is mandatory. [Lightspark README](https://raw.githubusercontent.com/lightspark/lightspark/master/README.md).

Adobe's release notes describe Flash Lite 2's XML, device-video and ActionScript 2 features. This supports a limited feasibility precedent, not a full desktop-Flash guarantee. [Adobe release notes](https://www.adobe.com/support/documentation/en/flash/fl8/releasenotes.html).

## Recommended development decision

Do not expand the promise to “full Flash.” Define the immediate deliverable as **faithful Stick RPG Complete, from the verified original SWF, on iPod Classic**, followed by an explicitly tested AVM1 catalog.

The next work should establish correctness before further cosmetic or performance patches:

1. Pin the exact SWF hash and identify the original wrapper, FlashVars and required resources. Run the same content through desktop Ruffle; where behavior disagrees, compare with a trusted original Flash reference if available. Do not assume Ruffle is infallible or all Stick RPG variants are interchangeable.
2. Make a test configuration that disables gameplay state overrides independently of host callbacks and input adaptation. Record the first divergence in variables, display-list ownership and timeline events. Existing prime/shortcut flags do not disable every workaround.
3. Extract small behavioral SWFs for the failing cases: target/environment restoration, case rules by SWF version, frame seeking, removal/recreation of clips, action ordering, object aliases and button events. Fix the shared runtime semantics and remove each corresponding game-specific override only after comparison passes.
4. Add a real Continue test: change cash/day/health/karma and inventory, save, exit fully, reload and compare all values and location. Exercise saving from different scenes. A load marker or a sufficiently large save file is insufficient.
5. Validate menus, name entry, movement in all directions, collisions, scene changes, shops/jobs, sleeping, traffic damage, death and repeated save/load. Compare screenshots and state at fixed milestones. Run with sanitizers on the desktop where practical.
6. Measure real Classic hardware: cold/warm load time, peak heap, fragmentation, per-frame time spent in scripts/timelines/rendering and input latency. Then optimize the measured bottleneck. Maintain logical game timing when reducing drawing frequency; reducing both together changes the game.
7. Retain and strengthen cache precomputation with source-hash/version validation. Consider static-background caches, tighter visibility rejection and fixed-point raster hot paths. Do not globally replace ActionScript numeric semantics with integers. Do not meet a frame budget by silently dropping visible content.
8. Add MP3/ADPCM event audio and relevant sound semantics once the visual/game-state path is stable, then test the required Rockbox playback transitions. Repeat performance measurements with sound enabled.

If semantic repairs become too extensive, conduct a bounded Ruffle-core experiment: one small AVM1 file, custom/null rendering first, then a minimal software renderer and the exact Stick RPG file. Record binary size, peak memory and update/draw time before choosing a wholesale engine migration. A successful build alone is not the decision gate.

Acceptance should require a sustained hardware play session through meaningful game progression and save/reload, without host-imposed stat resets, hidden traffic or game-state repairs. No defensible completion date or promised frame rate follows from the present evidence.

**Feasibility judgment:** Original Stick RPG is plausible but unproven; selected older Flash games are plausible with per-title limits; arbitrary full Flash compatibility at playable speed is not a realistic target for this hardware. The present failure is not evidence that Flash is categorically impossible on an iPod. It is evidence that this partial runtime has been made to advance through the game without yet reproducing all of its behavior.
