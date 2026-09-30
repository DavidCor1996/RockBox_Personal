# 8-Bit Rebellion! — single-player iPod reconstruction specification

Research date: 2026-09-26. Status: feasibility and implementation proposal;
no original application binary inspected, no port implemented or benchmarked.
Target: this personal Rockbox tree, iPod Classic 6G/7G first, Video 5G/5.5G
second. All budgets and schedules below are provisional engineering estimates.

## Finding and surviving material

The game is **Linkin Park: 8-Bit Rebellion!**, developed with Artificial Life
for iPhone, iPod touch and iPad in 2010. Its official announcement explicitly
describes offline missions and shops, with later server synchronization.
Therefore a local campaign has a strong historical basis; reconstructing the
social service is unnecessary for the proposed version. [S1]

The advertised music includes original and 8-bit renditions of New Divide,
One Step Closer, In the End, Crawling, QWERTY, Hands Held High, Faint, and
No More Sorrow, with Blackbirds as a completion reward. These are recorded
game assets until inspection proves otherwise; “8-bit” does not establish
that there is an NES ROM, tracker module, MIDI file, or sound-chip program. [S1]

| Lead | Evidence checked | Use and limitation |
| --- | --- | --- |
| Official announcement [S1] | Story premise, music, offline missions/shops | Product scope, not a technical format specification |
| Spriters Resource [S2] | Five asset collections: maps, enemies/NPCs, inventory, miscellaneous, playable characters | Promising extraction reference; payload completeness and geometry not inspected |
| Jordan Firari scene viewer [S3] | Page responds with scene selection, scenery viewer, location names and dialogue | Reachable preservation project; gameplay completeness and source license not established |
| Author's preservation thread [S4] | Author reports movable player/NPCs/warps in 2021 and partial layout reverse engineering in September 2022 | Best lead for map format research; author claims are not independently validated |
| Layout viewer video [S5] | Linked by its author in the preservation thread | Reference lead; video playback not verified in this research |
| Application preservation thread [S6] | June 2024 post links a Google Drive reupload | Candidate IPA lead only; download availability, integrity, version and runnability unverified |
| Contemporary extraction report [S7] | Owners report finding music/video inside the IPA archive | Supports starting with asset inventory rather than executable analysis |

No verified original source release, complete playable browser remake, or
tested application package was established. A promised full playthrough in
the preservation thread is a lead, not proof of an accessible complete video.
Public availability of assets is not evidence of a reuse license. Keep the
new engine and importer separate from original media, and make imported game
data a local input rather than automatically bundling it in releases.

## Proposed product

Reconstruct a native C Rockbox game engine with offline campaign data.
Do not attempt iOS emulation or assume the ARM executable can run directly:
Rockbox does not provide the original application's platform environment.
Use asset/data recovery first and targeted executable analysis only where
behavior cannot be recovered from data or observation.

The eventual campaign should retain exploration, band-member dialogue,
missions, enemy combat, recovered tracks, shops, inventory and local saves.
Recover the actual district/scene graph before fixing its structure. Keep
avatar customization and apartment decoration for a later content milestone.
Replace account identity with a local profile; omit chat, friends, gifts,
leaderboards, Facebook/Twitter integration, news and server synchronization.
Convert any campaign-critical online gate to documented local progression.

First playable milestone: one verified room and an adjacent transition,
one player, one NPC, one enemy, one quest with a reward, one music loop,
pause, save/load and clean return to Rockbox. This demonstrates the difficult
parts before committing to the entire campaign.

## Reverse-engineering work packages

1. **Establish the reference build.** Obtain a usable original package from
   an existing copy or preservation lead. Record SHA-256, package version,
   bundle metadata, file list, executable architecture and encryption status.
   Work on copies. Compare versions only after one baseline is reproducible.
   Inspect files without executing the downloaded application on the host.
2. **Inventory assets.** Identify signatures rather than trusting extensions.
   Classify images, atlases, audio, text, layouts, scripts and unknown blobs.
   Detect any Apple-optimized PNG encoding before choosing a decoder. Create
   contact sheets and an audio manifest with sample rate, duration and loops.
   Record provenance and hashes for each input and converted output.
3. **Recover scene layout.** Study the existing viewer's public implementation
   if available and its reuse terms; otherwise use it as a visual reference.
   Determine coordinate origin, units, object records, scale, animation frames,
   draw order, collision, walkable paths, exits and interaction hotspots.
   A collection named “Maps” does not prove collision or quest data survives.
   Validate two different scenes before declaring a format understood.
4. **Recover campaign logic.** Search resources for dialogue, quest IDs,
   conditions, item definitions and rewards. Record an original offline run
   on a compatible reference environment if available. Capture before/after
   saves for accepting/completing quests, death, purchases and scene changes.
   Distinguish observations from inferred behavior in a quest matrix.
5. **Analyze gaps selectively.** If a suitable analyzable binary is available,
   use disassembly/decompilation to trace resource readers, conditions and
   save serialization. Establish executable accessibility before estimating
   this work. Do not make whole-program decompilation the critical path.
6. **Compile portable data.** Convert recovered content on the desktop into
   bounded, versioned packs. No iOS resource decoding on the iPod. Preserve
   original files alongside conversion manifests outside the source release.

Required research outputs: asset manifest, documented scene schema, annotated
two-scene comparison, quest dependency table, combat timing table, save-state
observations, and an explicit list of missing or approximated behavior.

## Rockbox implementation

Proposed new paths, not files created by this research:

- `apps/plugins/8bit_rebellion/`: entry point, pack reader, renderer, world,
  input, dialogue, quest state, combat, save and audio modules.
- `tools/8bit_rebellion/`: inventory, extraction, conversion, validation and
  reference comparison tools.
- Device data: `/.rockbox/rocks/games/8bit_rebellion.rock` and
  `/.rockbox/rocks/games/8bit_rebellion/` containing packs and local saves.

Integrate through the repository's plugin source/subdirectory/category and
build lists as required by the final layout. Aim to use the current plugin
API without firmware changes. Build against the matching personal firmware.

Use a fixed simulation step, initially 25 Hz, independent of drawing cadence;
target 25–30 rendered frames/s on Classic and at least 20 on Video, subject
to profiling. Yield between work units and bound catch-up after a slow read.
Use fixed-point movement, clipped sprite blits and preconverted RGB565 or
indexed sprites with masks. Stream scene chunks instead of retaining the
world. Do not assume the original uses a tile map: support positioned images
until inspection establishes the representation.

Both target configurations currently specify 320×240. Recompose the HUD and
wrap dialogue for this screen; choose scene scale after measuring assets and
use a scrolling camera where needed. Do not globally shrink the phone UI.

Proposed controls to test on hardware:

| Input | Behavior |
| --- | --- |
| Previous / Next held | Move left / right |
| Wheel | Select nearby destination, exit, target or menu item |
| Select | Confirm destination; talk/interact; attack selected enemy |
| Play | Pause/resume simulation and game-owned sound |
| Menu | Back from an overlay; from gameplay save and return to launcher |

If recovered maps need substantial movement in depth, wheel-selecting a
walkable destination plus Select starts bounded local pathfinding. Test that
against an alternative steering scheme before locking controls. Avoid making
Menu and Play directional controls that conflict with exit and pause. Make
inventory, quest log, sound and volume accessible from the pause overlay.

### Memory and data contracts

The inspected personal configurations reserve `0x2f0000` (2.9375 MiB) for
Classic plugins and `0x300000` (3 MiB) for Video plugins. This includes loaded
plugin code/static storage; `plugin_get_buffer()` returns the remainder.
Do not mistake total iPod RAM for available game memory.

Provisional Classic budget, including plugin code/static data:

| Component | Ceiling |
| --- | ---: |
| Loaded code, static data and stack allowance | 512 KiB |
| One optional RGB565 frame surface | 150 KiB |
| Visible scene chunks | 640 KiB |
| Sprite/animation cache | 512 KiB |
| Audio queue and working data | 256 KiB |
| World, quests, text, save and pack indexes | 256 KiB |
| Total | 2326 KiB |

This leaves roughly 682 KiB against the 3008 KiB Classic reservation, before
measured alignment/other costs. Confirm actual stack placement and linker
sizes; reject an oversized asset pack gracefully. Prefer existing framebuffer
access if it removes the optional surface. No large automatic arrays.

Pack header: magic, version, target dimensions, content hash and section
directory. Validate counts, offsets, lengths, integer arithmetic and IDs
before reading/allocating. Sections hold scenes, sprite metadata, dialogue,
items, finite quest conditions/actions, audio indexes and optional content.
Use stable IDs, not pointers, across scenes and saves. Enforce an instruction
budget if a small quest interpreter is needed.

Save header: format version, content identity, sequence and checksum. Save
profile, scene/position, inventory, quest flags, currency and unlocks. Use
two alternating complete save files and choose the newest valid compatible
one on load so a partial write cannot destroy the only good copy. Report
incompatible content and offer a new profile without silently wiping saves.

### Music and audio ownership

The 8-bit arrangements are a core acceptance requirement. Recover the actual
recordings and loop points; generating approximate chiptunes is a separate
creative scope. Initially convert locally to streamed 44.1 kHz 16-bit stereo
PCM to avoid adding a compressed decoder before the game works. This costs
about 10.1 MiB/minute of storage, not resident RAM; measure disk wakeups and
battery consumption. Add block ADPCM only if measurements justify it.

Use two modes:

- **Existing user music active or paused:** preserve playlist and playback
  state; default game music off. Fit graphics/state in the plugin buffer.
- **Game soundtrack selected:** explicitly hand over audio using the core
  ownership path. If taking shared audio memory, use
  `plugin_get_audio_buffer()` without a preceding `audio_stop()`. Stream and
  mix game music/effects into `PCM_MIXER_CHAN_PLAYBACK`. Never create or
  overwrite a user playlist to play game tracks.

Follow `docs/plugin-audio-lifecycle-steering.md` and the current mpegplayer
implementation before writing audio code. Fill queues in normal context;
callbacks only hand off ready samples, with no filesystem reads, allocation,
sleeping or decoding. Stop owned output, quiesce its callbacks, restore mixer
state and then release owned memory on every exit/error. Do not wait for
unrelated user music to stop in the preserve-user-music mode.

## Delivery gates and effort

Estimates are focused engineer-days after usable reference inputs exist,
not calendar promises. Unknown encrypted/missing data can expand them.

| Stage | Estimate | Exit criterion |
| --- | --- | --- |
| Reference and asset investigation | 2–5 days | Two scenes, usable music, identified quest evidence; explicit go/no-go |
| First playable room/quest | 5–10 days | Simulator and Classic run the complete small loop with save and sound |
| Campaign reconstruction | 10–25 days | All recovered campaign paths complete offline; missing behavior documented |
| Hardware refinement | 5–10 days | Performance, controls, audio transitions and persistence pass on both target families |

Planning range: roughly 4–10 full-time weeks for a faithful small campaign,
conditional on assets and quest evidence. Asset-only proof of concept could
arrive much sooner. If only images survive, re-scope as a reconstruction with
documented approximations rather than claiming original gameplay fidelity.

Validation must include pack corruption/truncation, interrupted saves,
quest re-entry and duplicate rewards, death/reload, all exits, scene boundary
collisions, and readable dialogue. Use simulator sanitizers for parsers and
game state. Measure frame times, peak memory and audio underruns on hardware.
Run fresh boot, Database/Files music into game, game back to Database/Files,
rapid switching, pause, volume, Menu and USB exit scenarios. Keep callbacks
and file handles stable over repeated launches and long play sessions.

Initial delivery is a plugin and its data for the personal runtime. Any
subsequent firmware deployment must follow repository deployment guidance;
the official-upstream Applications slot and its isolated runtime are not
destinations for this game.

The next concrete implementation step is the reference-package inventory
plus a two-scene extractor/viewer comparison. It resolves the largest risk
before investing in combat and campaign code.

## Sources

- [S1: Official Artificial Life / Linkin Park announcement](https://www.globenewswire.com/news-release/2010/03/30/417392/5164/en/LINKIN-PARK-Announces-the-Release-of-LINKIN-PARK-8-BIT-REBELLION-Mobile-Game-With-Artificial-Life-Inc.html)
- [S2: Spriters Resource game asset index](https://www.spriters-resource.com/mobile/linkinpark8bitrebellion/)
- [S3: Jordan Firari scene viewer](https://jordanfirari.com/8-bit-rebellion/)
- [S4: Author's preservation and layout research thread](https://lplive.net/forums/topic/6897-8-bit-rebellion/)
- [S5: Linked layout viewer demonstration](https://www.youtube.com/watch?v=7m5fg3J4eBc)
- [S6: Application preservation leads](https://lplive.net/forums/topic/13368-8-bit-rebellion/)
- [S7: Contemporary reports of extracting purchased application resources](https://lplive.net/forums/topic/4203-8-bit-rebellion-released/page/16/)

Local implementation evidence inspected: `CLAUDE.md`,
`docs/plugin-audio-lifecycle-steering.md`,
`firmware/export/config/ipod6g.h`, `firmware/export/config/ipodvideo.h`,
`apps/plugin.c`, `apps/plugin.h`, and plugin action mappings.
