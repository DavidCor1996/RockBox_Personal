# ScummVM Rockbox Port Specification

This document defines the first supported ScummVM shape for this iPod-focused
Rockbox tree. It is intentionally narrower than upstream ScummVM: the first
goal is a reliable plugin backend for 320x240 iPods, not a full desktop
launcher.

## Goals

- Launch games from small `.scummvm` descriptor files in the file browser.
- Keep all game data outside the firmware tree and outside the plugin binary.
- Preserve user music and playlist state unless the ScummVM backend actually
  needs primary game audio.
- Support an incremental engine allowlist, starting with low-resolution 2D
  engines that fit 320x240 and clickwheel controls.
- Keep the upstream ScummVM import isolated under `apps/plugins/scummvm/` so
  local Rockbox backend work is reviewable.

## Non-Goals

- Do not import every upstream engine at once.
- Do not provide a desktop-style ScummVM GUI launcher in the first slice.
- Do not emulate DOS, Windows, or PC hardware.
- Do not mutate playlists or use the Rockbox audio engine as a music launcher.

## Launch Descriptor

Descriptor files use the `.scummvm` extension and simple `key=value` lines:

```text
gameid=monkey
engine=scumm
path=/ScummVM/monkey
savepath=/.rockbox/scummvm/saves/monkey
```

Supported keys in the first slice:

- `gameid`: ScummVM target id or a local id for saves/config.
- `engine`: engine family name, for example `scumm`, `sky`, or `queen`.
- `path`: game data directory.
- `savepath`: optional save directory. If omitted, the plugin uses
  `/.rockbox/scummvm/saves`.

Blank lines and lines beginning with `#` are ignored. Unknown keys are ignored
so descriptors can grow without breaking older builds.
If `path` is omitted, the launcher defaults to `/ScummVM/<gameid>` so a mounted
iPod can keep games directly under a top-level ScummVM folder.

Engine data shared by ScummVM engines belongs under
`/.rockbox/scummvm/engine-data`. The Sky engine requires `sky.cpt`; the loader
also accepts it in the game data directory for compatibility with upstream
ScummVM layouts. Without `sky.cpt`, the current Rockbox bridge can still render
the intro resources from `sky.dsk`, but the section bootstrap and full game loop
cannot execute.

## Backend Plan

The current plugin entry point parses and validates descriptors, then hands the
result to `scummvm_backend_run()`. That function owns the Rockbox-facing event
loop, input mapping, framebuffer presentation, and the narrow C/C++ bridge into
the imported ScummVM 1.9.0 sources.

Expected backend responsibilities:

- Provide filesystem, timer, input, graphics, save, and audio implementations
  using the Rockbox plugin API.
- Prefer 320x200 and 320x240 engines first; scale smaller modes with integer
  or letterboxed scaling.
- Use a fixed input map with an in-game menu for verb/action modifiers.
- Store saves under `/.rockbox/scummvm/saves`.
- Avoid dynamic plugin loading from upstream ScummVM.

## Candidate Engines

Initial candidates should be small and 2D:

- `sky` for Beneath a Steel Sky.
- `queen` for Flight of the Amazon Queen.
- early `scumm` targets at 320x200.

NiBiRu is a later, explicit experiment. Current upstream ScummVM identifies
it as an unstable AGDS 2.511 target, but this Rockbox port's GPLv2-compatible
ScummVM 1.9.0 import predates that engine. The local port therefore starts
with bounded native ADB/GRP readers and will adapt only the required AGDS
runtime behavior after validating a user-owned release. Original artwork,
sound, video, and game data remain outside the source tree.

Avoid engines with high-resolution, 3D, heavy video, or large scripting/runtime
dependencies until the backend is stable.

## Current Engine Bridge

The active import is ScummVM 1.9.0, kept under
`apps/plugins/scummvm/upstream-1.9.0/`. This vintage is GPLv2-compatible with
Rockbox and builds without requiring a C++11 runtime.

Implemented bridge pieces:

- `sky`: validates `sky.dnr`/`sky.dsk`, loads real resource entries, and uses
  the imported `Sky::RncDecoder` to validate and unpack RNC-compressed
  resources. It also loads and renders the first upstream static intro screens
  through the Rockbox framebuffer: `60110`/`60111`, `60112`/`60113`, and
  `60114`/`60115`. The bridge also has a minimal Sky sequence player for the
  skip/run encoded intro animation resources and starts the floppy intro
  sequence files `60082` through `60086` after the static screens. After the
  intro resource path completes, the bridge validates the section-0 bootstrap
  resources used before the first playable scene: the fixed item resources and
  all 70 grid files (`60000` through `60069`). It also parses and validates
  `sky.cpt`, including the compact data-list table, source/ascii sections,
  dlinc aliases, diff block, and save-id table. The native runtime layer now
  initializes the core Sky script variables, enters section 0, and scans the
  active logic list from the retained compact table each frame. It also loads
  Sky script modules from `sky.dsk`, decodes the core script opcodes including
  compact-field access and switch tables, and steps all active compacts in the
  retained logic list. The native mcode layer now covers cache-fast,
  cache-chip, draw-screen, subroutine setup, get-to routing, stand/turn setup,
  arriving/leaving counters, alternate scripts, kill-id, stop flags, sync and
  request messages, sprite draw-state toggles, pause/wait-sync states,
  simple/module/frame animation setup and stepping, mega-set changes, item
  movement, chooser-list state, random/person/coordinate/list/place queries,
  custom Joey state fetches, palette changes, text-name changes, mini-load,
  cache flush, interact/request/menu state, reset blocks, face-id direction
  calculation, and section changes. A native Sky text decoder now reuses the
  upstream Huffman text tables and `sky.dsk` text resources for speech,
  chooser, and text-module events. The runtime now renders decoded strings
  through Sky's bitmap font resource into generated Sky-format sprite buffers,
  attaches them to the upstream text compact range, and uses those compacts for
  speech subtitles, text-module output, and chooser entries. Chooser clicks
  route through the normal mouse-detectable compact path back to Sky's
  `THE_CHOSEN_ONE` script variable. The Rockbox backend now sends cursor and
  click state into the Sky runtime, which performs mouse-list compact
  hit-testing and chooser state updates.
  Draw-screen loads the current Sky screen resource and renders it through the
  retained compact palette into the Rockbox video surface; the render path now
  follows Sky draw-list variables, chained draw-list compacts, and sorted
  sprite ordering before falling back to logic-list sprites during bootstrap.
  It composites active Sky sprites from real item resources using the upstream
  frame layout. Runtime state is autosaved under the descriptor save path as
  script variables plus the mutable compact entries listed by `sky.cpt`. The
  compact scheduler also advances alternate-script, chooser, pause, wait-sync,
  turning, and animation states so those scripts can resume. Autoroute now
  loads the real Sky walk-grid resources, builds compact route buffers with the
  upstream grid conversion and flood-fill route shape, and consumes those route
  commands during AR animation. Dynamic grid mutation now follows the upstream
  lifecycle by removing grid-plotted actors before logic, plotting them back
  after logic, clearing one-frame syncs, and honoring plot/remove grid mcodes.
  AR animation now performs compact collision checks at tile boundaries, waits
  for previously-blocking actors to clear, and restarts route scripts when a
  non-routing actor blocks the current path. When another routing actor blocks
  the current path, it now runs the actor's mini-bump script against the active
  compact, matching the upstream collision hook. Route playback also consumes
  the actor megaset's directional animation lists for walk frame selection and
  per-frame movement deltas, falling back to generic route movement only when a
  compact lacks animation data; route direction changes now use megaset turn
  tables before returning to route playback. The sprite compositor also applies
  Sky's layer-grid vertical masks by re-blitting active layer tile blocks over
  background and sorted sprites for walk-behind scene geometry. The Sky runtime
  now owns a PCM mixer-backed audio path on `PCM_MIXER_CHAN_PLAYBACK`: it loads
  section sound resources, converts unsigned 8-bit Sky SFX and CD speech data
  into a plugin-owned 44.1 kHz stereo buffer, maps effect requests through
  Sky's upstream room/flag SFX table including delayed starts, starts
  speech/effect mcodes and animation-stream `LF_START_FX` hooks through that
  path, streams optional ScummVM-style Sky digital music files from the game
  directory when supplied as 8-48 kHz PCM WAV files, mixes music under
  speech/effects through the same playback callback, and stops/restores the
  mixer frequency on reset without taking the shared audio buffer or touching
  playlists.
- `queen`: validates `queen.1`/`queen.1c`, recognizes known retail/demo file
  sizes, recognizes rebuilt `QTBL` resource headers, and now verifies that a
  usable Queen resource index exists before marking the engine ready. Rebuilt
  resources expose the embedded table directly, English DOS floppy uses the
  upstream built-in resource count, and other original releases require
  `queen.tbl` beside the game data. Indexed Queen releases also verify
  `QUEEN.JAS` presence and compare DOS JAS version bytes against the expected
  upstream version string. The native Queen loader can now resolve and read
  named resources through rebuilt, external, and the built-in English DOS
  floppy JAS table entries; prepare scans the upstream `QUEEN.JAS` layout
  after the 20-byte skip, records section offsets and the room, object, item,
  command, furniture, actor, and animation counts with bounds checks, reads
  the entry object to derive Queen's initial room, and resolves the initial
  room's upstream object range. It also scans the initial room objects and
  classifies visible/hidden/static/animated/person/paste-down rows using the
  referenced `GraphicData` entries. Fixed-width JAS row readers are wired for
  object, item, graphic, walk-off, object-description, furniture, actor, and
  graphic-animation tables; prepare validates sample reads from each present
  section before marking Queen ready. Queen command subtable offsets and row
  readers now cover command-list, area, object, inventory, and game-state
  command records, also validated during prepare. `QUEEN2.JAS` text metadata
  now follows upstream string-region offset derivation and validates the text
  file has enough lines for descriptions, object names, room names, verbs,
  Joe responses, actor animation names, actor names, and actor files. The
  native loader can now stream a specific text line from `QUEEN2.JAS`; prepare
  validates that by reading the initial room name and first named room object.
  Native grid readers now expose Queen room area records and object boxes from
  the JAS grid section; prepare validates the initial room grid header, first
  area when present, and first room-object box.
- Both engines render through the Rockbox framebuffer shell. Sky now has a
  native script, scene, input, save, and SFX path; Queen remains at resource
  fixed-width JAS row loading, command table loading, and `QUEEN2.JAS`
  metadata/line loading, and grid area/object-box loading through the initial
  room summary until its native loop is ported.

## Audio Lifecycle

If the backend has no audio, it must not touch playback. If game audio is
enabled:

- call `plugin_get_audio_buffer()` only when the backend needs the shared audio
  buffer, and do not call `audio_stop()` before it;
- use `PCM_MIXER_CHAN_PLAYBACK` for primary game audio;
- stop the mixer channel and clear callbacks before releasing plugin-owned
  memory;
- restore sample rate and mixer state on exit;
- never modify or replace the user's active playlist.

These rules are mandatory for this tree and come from
`docs/plugin-audio-lifecycle-steering.md`.

## Implementation Phases

1. Descriptor launcher and build/file-browser integration. Done.
2. Rockbox backend shell with graphics, input, save paths, and no audio. Done.
3. Engine allowlist and data probing for `sky` and `queen`. Done.
4. C++ bridge for importing ScummVM 1.9.0 engine classes. Done.
5. Imported Sky decompressor compiled and exercised through the Rockbox Sky
   resource loader. Done.
6. Queen resource-file validation through the bridge. Done.
7. Sky startup screen rendering from real game resources. Done.
8. Timed Sky static intro screen sequence from real game resources. Done.
9. Minimal Sky intro sequence player for skip/run animation resources. Done.
10. Sky section-0 fixed item and grid resource bootstrap. Done.
11. Sky engine-data probing for `sky.cpt`. Done.
12. Sky compact-table loader and bootstrap validation. Done.
13. Native Sky runtime state and active logic-list scanning. Done.
14. Sky script-module loading and first-mcode decoder. Done.
15. Initial Sky mcode execution for cache and screen draw. Done.
16. Sky active-list scheduler, compact offset mapping, script branch opcodes,
    and broad state-mutating mcode coverage. Done.
17. Sky Rockbox input delivery, compact hit-testing, menu/request state, and
    visible sprite compositing. Done.
18. Sky draw-list based layering and autosave snapshots. Done.
19. Basic Sky autoroute, alternate-script, chooser, and stopped-state stepping.
    Done.
20. Sky Huffman text decoding and Rockbox overlay for speech, chooser, and
    text-module events. Done.
21. Sky walk-grid backed autoroute buffer generation and route playback. Done.
22. Sky dynamic grid plot/remove and actor grid occupancy lifecycle. Done.
23. Sky AR actor collision waits and route-script restart handling. Done.
24. Sky route animation-list frame and movement playback. Done.
25. Sky AR turn-table animation playback. Done.
26. Sky layer-grid vertical masking and walk-behind compositing. Done.
27. Sky moving-actor collision mini-bump script execution. Done.
28. Sky generated in-scene text sprites for speech, chooser, and text modules.
    Done.
29. PCM mixer-backed Sky speech/effect audio path using the required lifecycle.
    Done.
30. Upstream Sky SFX room/flag mapping and delayed starts. Done.
31. Optional Sky digital music WAV streaming and software mixing with
    speech/effects. Done.
32. Remaining Sky audio parity: native AdLib/GM/MT-32 sequencing.
33. Queen native resource-index readiness checks. Done.
34. Queen native `QUEEN.JAS` presence/version validation. Done.
35. Queen indexed named-resource read path. Done.
36. Queen native `QUEEN.JAS` layout/count scanner. Done.
37. Queen native JAS section offsets and entry-room object parse. Done.
38. Queen native room object-range reader. Done.
39. Queen native initial-room object summary scan. Done.
40. Queen native fixed-width JAS row readers. Done.
41. Queen native command table row readers. Done.
42. Queen native `QUEEN2.JAS` text-region metadata validation. Done.
43. Queen native `QUEEN2.JAS` line reader and initial text lookup. Done.
44. Queen native grid area and object-box readers. Done.
45. Queen native room furniture/actor summaries and command-match batch
    readers. Done.
46. Queen native command-batch operation classification, reference validation,
    and upstream command-object workaround. Done.
47. Queen native current-room backdrop, bank, and optional dynalum resource
    probes. Done.
48. Queen native PCX backdrop metadata and palette-marker validation. Done.
49. Queen native PCX backdrop RLE decode and framebuffer render bootstrap.
    Done.
50. Queen native current-room object-box hit testing and hover/click status.
    Done.
51. Queen native click default-verb command lookup, game-state condition/set,
    object mutation, and inventory mutation cache. Done.
52. Queen native current-room grid-area cache and command-area on/off
    mutation. Done.
53. Queen native runtime autosave/restore for object, item, inventory,
    game-state, and current-room area caches. Done.
54. Queen native DOS BBK room-bank parser and static furniture/object sprite
    compositing over the PCX backdrop. Done.
55. Queen native animated room/furniture BOB frame cycling from DOS BBK
    banks. Done.
56. Queen native animated BOB playback timing from per-graphic speed fields.
    Done.
57. Queen native current-room actor standing-frame rendering from room or
    actor BBK banks with game-state gating. Done.
58. Queen native room paste-down layering and Y-sorted live BOB draw order.
    Done.
59. Queen native runtime current-room state, room asset/bank/grid reload,
    room-aware autosave, and entry-object room transitions. Done.
60. Queen native default-action `WALK_TO` fallback, command image-order
    mutation, and open/close/move object-state mutation. Done.
61. Queen native linked entry-object open/close state mirroring for paired
    doors/exits. Done.
62. Queen native look-at object-description selection, persistent
    last-seen tracking, and text status surfacing. Done.
63. Queen native `WALK_TO` closed-exit handling and source walk-off target
    lookup for entry-object room transitions. Done.
64. Queen native command game-state `speak_value` surfacing for failed
    conditions and successful state-set responses. Done.
65. Queen native Joe position/facing state for `WALK_TO`, no-command exits,
    entry-object room arrival, and autosave persistence. Done.
66. Queen native wrong-action Joe-response text for default verbs without
    matching command rows. Done.
67. Queen native standing-Joe sprite rendering from `JOE_B.BBK`, including
    facing, horizontal flip, Y-sorted room layering, and `WALK_TO` redraws.
    Done.
68. Queen native visible Joe `WALK_TO` interpolation with `JOE_A.BBK`
    walking-frame playback and clean autosave restore to standing state. Done.
69. Queen native four-slot inventory state, command add/delete slot refresh,
    autosave persistence, and Rockbox backend inventory overlay. Done.
70. Queen native inventory slot hover/click hit testing with item-name status
    and native item description text surfacing. Done.
71. Queen native command `special_section` handling for journal pending state
    and Joe clothes/dress/underwear game-state plus matching Joe BBK bank
    selection. Done.
72. Queen native `.CUT`/`.DOG` resource verification from object-description
    text, including native script-header parsing, encoded talk-string decoding,
    cutaway entry/bank/talk-file preview, initial dialog line and first-level
    option discovery, native cutaway face-command application, cutaway
    room-fade object normalization to Joe, first cutaway Joe placement/move
    execution, first cutaway `fromObject` object copy/on
    handling, current-room cutaway person-list visibility, current-room
    non-Joe person-record placement/move effects, first cutaway temporary
    room/person-list loading, cutaway tail
    game-state/object/final Joe position handling, first cutaway object
    sentence decoding for Joe/text/credit records, inline speak-control token
    cleanup with simple Joe face/move control application,
    persistent dialog talked-to and option-selection state, Rockbox dialog
    message/option overlay surfacing,
    click handling for first-level dialog choices with immediate NPC reply and
    repeat-NPC reply selection, child-option surfacing, exit-only dialog
    auto-finish, dialog return-value level routing and game-state application,
    `.DOG` post-dialog inventory and conditional cutaway triggers,
    conservative multi-object cutaway record scanning with animation-packet
    frame/flip/position/move/off-state effects with autosave persistence,
    cutaway tail current-room area
    toggles, native object-level cutaway special-move state effects for Joe clothing,
    fight/Frank/robot/Azura/guard/mannequin/puzzle cases, first-click default
    action dispatch, backdrop-click Joe walk targets, upstream-style
    multi-match command row scanning, special walk-area Joe responses and
    cutaway triggers for the upstream handled Queen rooms, explicit Queen verb
    cycling from clickwheel/cancel controls with save/restore and overlay
    status, paged four-slot inventory browsing from the inventory panel,
    inventory item selection overlay and `USE`/`GIVE` item-on-object command
    dispatch, cutaway `.DOG` talk-file handoff with bounded chained
    `.CUT`/`.DOG` follow-up execution, and
    metadata surfacing for missing/corrupt resource failures. Done.
73. Full Queen scene/script loop.
74. Engine allowlist expansion and per-game input profiles.
75. AGDS ADB/GRP bounded index validation and encrypted member lookup. Done.
76. AGDS `main` object header validation and direct BMP/PCX archive streaming
    with 1024x768-to-320x240 decode-time scaling. Done with synthetic simulator
    coverage and the owned retail startup logo gate.
77. NiBiRu AGDS 2.509 process VM (opcode base 2217), screen composition, pointer regions,
    animation, audio, and save/load. Retail selector placement and room 1864
    layer composition are done. The first room's bounded process tree, globals,
    suspension, animation phases, and sample load/restart/stop events now run.
    The VM also parses retail polygon regions, executes mouse-area enter/leave
    objects and embedded look/use handlers, supports cloned object code, and
    preserves globals through screen changes. The authentic New Game handler
    now runs `1009.1067` and `107a` before opcode 79 transitions to `1864`;
    the simulator gate asserts that trace and rejects pending-opcode failures.
    Its owned first-scene mesh/animation/texture/camera set is interpolated at
    24 fps and rasterized to a sparse RGB565 stream as a draw-stage
    optimization. Martin, chair, and handset transforms all come from the
    authored tracks while AGDS remains in control of game state and timing.
    Broader room handlers, full inventory,
    save/load, and broader opcode coverage remain.

## Test Matrix

For every phase:

- launch a valid `.scummvm` descriptor from Files;
- launch with missing descriptor keys;
- launch with a missing game data directory;
- exit back to the file browser;
- Database music -> ScummVM launcher -> Database music still works;
- Files music -> ScummVM launcher -> Files music still works.

For audio phases, also run the full matrix in
`docs/plugin-audio-lifecycle-steering.md`.
