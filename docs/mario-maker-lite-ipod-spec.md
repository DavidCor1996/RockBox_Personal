# Mario Maker Lite for iPod

Status: implemented; simulator-qualified; private-content and physical-hardware
qualification require user-supplied source files and an iPod  
Primary target: iPod Classic 6G/7G (`ipod6g`)  
Secondary target: iPod Video 5G/5.5G (`ipodvideo`) after hardware qualification  
Desktop authoring host: Rockpod on Linux and macOS

## Implementation snapshot

The repository now contains the portable 60 Hz C runtime, all three rulesets,
the native Rockbox plugin, v4 directional metasprite private-art format,
RPML v3 16-bit terrain and per-object art references, a paged Rockpod creator
and shared-core preview, transactional export/sync, project covers,
per-project controls and saves, Steam/Classic launch integration, an Open
Surge `.lev` subset importer, three editable test levels, deterministic trace
fixtures, and host/simulator qualification tools.

Simulator qualification covers direct Mario, Zelda, and Sonic launches plus
the complete Steam and Classic paths. It exercises Hold pause, Menu pause,
the controls screen, resume, save creation, corrupt-save quarantine, backup
recovery, framebuffer output, deadlines, and click-wheel mappings. Native
scripted runs complete every test level; the Zelda dungeon also crosses a
reciprocal room link. The diagnostic simulator art is deliberately synthetic
and non-shipping.

Authentic commercial art is never fetched or committed. Final visual and
behavior parity is therefore a local qualification step: the user imports a
verified supported game image and its exact extracted cells/effects, then
captures private reference traces. Physical iPod performance, playback-state,
USB, storage-full, and long-duration gates likewise remain device tests rather
than claims made from the simulator.

## Product decision

Build a Rockpod-authored, native Rockbox game named **Mario Maker Lite** with
three deliberately separate creation styles:

- **Mario** — a side-scrolling platformer modeled on *Super Mario World*;
- **Zelda** — a top-down action/adventure ruleset modeled on
  *The Legend of Zelda: A Link to the Past*; and
- **Sonic** — a high-speed 360-degree platformer modeled on the classic
  Mega Drive games, with verified Sonic 2 recipe support and a built-in
  Sonic 3 US extractor.

Rockpod is the creator. The iPod is the player. Version 1 does not attempt to
place or edit level objects on the click wheel.

Every exported project is an individual installed game. It appears in the
existing iPodJS Steam library, carries its own user-changeable cover, and is
grouped under a generated `Maker Lite` console filter. The Classic Games
appearance gets a matching `Maker Lite` entry.

The Mario, Zelda, and Sonic reference kits must use authentic, locally
imported source-game pixels and audio. No AI-generated, hand-drawn, traced, or
look-alike art may stand in for missing commercial source assets. Nintendo and
Sega assets are not committed to this repository or included in public
Rockbox/Rockpod packages. Rockpod prepares a private asset kit from files
supplied by the user and syncs that private kit to the user's device.

Rockpod also includes `zelda-neon-nook-v1`, a clearly labeled original asset
kit for a cozy cyberpunk life-sim project built on the Zelda/top-down ruleset.
It has its own original-generated provenance and is never presented as
authenticated Nintendo or Sega material.

### Neon Nook gameplay slice

The one-click Neon Nook installer creates an open-ended 80x56-tile city and a
dedicated 16x14 player-owned apartment using the RPML v3 `life_sim` level
flag. It deliberately has no goal, completion condition, scripted story, or
room event chain. The playable loop is:

- begin at home with ten basic furnishings that can be picked up, repositioned,
  sold directly, or carried to the shop for resale;
- collect city salvage and sell one or all pieces at the shop;
- take repeatable jobs from more than fifty roaming NPC residents for credits;
- pay the current home debt, then build or upgrade through three house tiers;
- cycle the house facade style independently of its tier;
- buy three car upgrades, enter or exit the parked car, and drive faster at
  each tier;
- retain credits, debt, house tier/style, car tier/state, player/car position,
  furniture positions and held state, and collected objects in the
  fixed-size per-project save.

The dedicated house, shop, car, NPC, furniture, and non-interactive decoration
entity kinds are valid only as authored entities; the life-sim behavior flag
is currently restricted to the Zelda/top-down ruleset. All menus are native
click-wheel gameplay UI and use the existing fixed-buffer renderer and
non-owning effect playback path.

## Experience goals

The complete path should feel like one product:

1. Open `Games > Maker Lite` in Rockpod.
2. Import one or more supported, personally supplied source games.
3. Choose `Mario`, `Zelda`, or `Sonic`.
4. Build a level in a modern drag-and-drop editor.
5. Press Play to test with the same engine used on the iPod.
6. Choose or replace the project's cover art.
7. Press `Sync to iPod`.
8. On iPod, open Games in Steam appearance, hold Select to choose the
   `Maker Lite` console, open the project's cover, and press Play.

## Creator expansion pass

The expanded creator keeps Mario, Zelda, and Sonic as the three native
physics rulesets, while exposing multiple game types within each ruleset.
Game types are starter layouts made only from ordinary RPML terrain, entities,
paths, events, and metadata; they do not introduce a second desktop-only
runtime.

| Ruleset | Game types |
| --- | --- |
| Mario | Classic Course, Obstacle Course, Checkpoint Sprint |
| Zelda | Overworld Adventure, Key & Door Dungeon, Switch Puzzle |
| Sonic | High-Speed Race, Ring Challenge, Spring Stunt Course |

Every compatible kit also offers **Maker Brawl Arena**, a one-player versus
CPU platform-fighting mode with two active characters. Its separate gameplay,
roster, private guest-asset boundary, controls, and qualification contract are
defined in [`maker-brawl-ipod-spec.md`](maker-brawl-ipod-spec.md).

The New Project flow asks for both a verified kit and a game type. Templates
place useful starter terrain and gameplay objects, bind available catalog art
by entity kind, and remap starter terrain to the kit's verified solid brush.
The result remains fully editable and compiles through RPML v3.

The mouse-focused editing pass adds four terrain brush shapes:

- freehand, with continuous interpolation between pointer samples;
- a straight line brush;
- a filled rectangle brush; and
- bounded four-way flood fill.

`Alt`-click samples a verified terrain or object part back into the palette.
Objects can still be dragged directly, `Ctrl`/`Cmd`-drag copies them,
`Ctrl`/`Cmd`+`D` duplicates the selection, arrow keys nudge it by one pixel,
and `Shift`+arrow nudges by one tile. `B`, `L`, `R`, and `F` select the four
brush shapes. Every shape stroke, fill, move, and copy is one undoable project
mutation.

This pass deliberately expands the use of authenticated kit catalogs rather
than bundling substitute commercial assets. Catalogs may contain up to 1,024
parts, keep their kit-defined categories, and retain searchable, pinned,
recent, and paged views.

The acceptance standard is not merely that Mario, Link, or Sonic is visible.
Each character must have a separate ruleset, animation graph, collision
behavior, camera, and control preset based on its reference game.

## Scope

### Version 1 includes

- a Qt/PySide6 Rockpod creator with project browser, palette, canvas,
  inspector, undo/redo, test mode, validation, cover editor, and device sync;
- an iTunes/stock-Apple three-pane hierarchy with restrained unified chrome,
  searchable 72-part pages, category filtering, pinned parts, and recents;
- three fixed, non-interchangeable rulesets;
- one authentic visual/audio kit per ruleset;
- one optional original Neon Nook visual/audio kit for the Zelda/top-down
  ruleset;
- declarative tiles, objects, paths, triggers, rooms, and goals;
- desktop preview through the same portable C simulation core as the iPod;
- one native `maker_lite.rock` plugin;
- auto-save, checkpoints, completion records, and per-project settings;
- dynamic registration in both Steam and Classic game appearances;
- custom cover import, crop/contain, preview, and replacement;
- simulator gates and iPod 6G hardware performance qualification.

### Version 1 does not include

- arbitrary scripting on the iPod;
- cross-franchise levels or characters;
- a public level-sharing service;
- online asset or level downloads on the iPod;
- a complete reimplementation of every object in the three source games;
- ROM emulation;
- runtime extraction of ROM data on the iPod;
- the full Open Surge engine on Rockbox;
- an iPod-side level editor;
- generated substitute art when an authentic asset is unavailable.

## Reference styles

Version 1 pins exactly one reference revision per style. The importer must use
an allowlist of verified hashes; filenames are hints and never proof.

| Style | Reference | Native presentation | Initial playable character |
| --- | --- | --- | --- |
| Mario | *Super Mario World*, SNES | 256x224 at 1:1, centered on 320x240 | Mario |
| Zelda | *A Link to the Past*, SNES | 256x224 at 1:1, centered on 320x240 | Link |
| Sonic | *Sonic the Hedgehog 3* (US built-in) or pinned *Sonic 2*, Genesis | 320x224 at 1:1 plus a 16-pixel system band | Sonic |

Rockpod may later add other revisions or styles through new, explicitly
versioned extractors. It must not accept an unknown ROM and guess offsets.

All gameplay art is integer-positioned and nearest-neighbor only. Mario and
Zelda retain their native 256-pixel camera width; the 32-pixel side gutters
may show cached, non-animated project trim but must never stretch the playfield.

## Authenticity contract

### Visuals

The local import pipeline extracts and converts:

- character frames, palettes, and animation timing;
- terrain and object tiles;
- enemies, collectibles, effects, and HUD glyphs;
- palette cycles and simple frame sequences; and
- title marks needed inside the private project library.

The extractor preserves original pixels. Allowed conversion operations are
palette expansion, channel conversion, atlas packing, transparent-index
mapping, lossless cropping, and exact integer positioning. Smoothing,
redrawing, AI inpainting, vector tracing, invented missing frames, and style
transfer are prohibited.

If a required animation is unavailable in the selected reference, the object
is unavailable in that kit. The creator shows `Not available in this style`
instead of fabricating artwork.

### Character feel

The three players do not inherit from one generic platform-controller preset:

- Mario has separate walk/run acceleration, skidding, crouch, variable jump,
  stomp, swim, climb, carry, power state, and goal behavior.
- Link has eight-way top-down movement, facing locks during attacks, sword
  timing, knockback, shield state, item use, pushing, pits, doors, and
  room-transition behavior.
- Sonic has ground inertia, air acceleration, rolling, spin dash, braking,
  slope projection, wall/ceiling attachment, springs, rings, damage scatter,
  checkpoints, and goal behavior.

The source of truth is a set of clean-room behavior traces captured while
playing the user's verified reference revision. A trace contains input state,
character state, position, velocity, angle, animation ID, camera position, and
collision contacts; it contains no image or audio data. Golden tests replay the
same input into Maker Lite and compare at each 60 Hz simulation tick.

Open Surge values may bootstrap Sonic tuning but are not an authenticity
oracle. Final Sonic acceptance is against traces captured from the exact
hash-verified Sonic 2 or Sonic 3 revision selected for that kit.

## Asset and rights boundary

Personal use does not by itself grant permission to fetch or redistribute
commercial game assets. The implementation therefore follows this repository's
existing personal-ROM and personal-Apple-asset pattern:

- Rockpod imports only a local file explicitly selected by the user.
- The importer displays the detected title, region, revision, and hash before
  extraction.
- Original ROMs remain outside the repository and are never copied to the
  device for Maker Lite.
- Extracted packs live in a gitignored Rockpod private-data directory.
- Build scripts, test fixtures, source releases, and `rockbox.zip` contain no
  Nintendo or Sega data.
- Rockpod backups clearly label derived private packs and do not upload them.
- An export-leak gate rejects source ROM signatures, full ROM hashes, and
  unapproved private asset paths from distributable packages.
- The device receives only the bounded assets needed by installed projects.

The default project cover comes from the corresponding source-game cover
already selected or cached in the user's Rockpod game library. `Change Cover`
accepts a user-selected PNG, JPEG, WebP, or BMP and produces the exact Rockbox
BMP derivative. No fake cover is generated. A project without a readable cover
can be played from Rockpod but is not advertised in the Steam library.

Suggested private host layout:

```text
<rockpod-data>/maker_lite/
    sources.json
    kits/
        smw-us-<hash-prefix>/
        alttp-us-<hash-prefix>/
        sonic2-world-<hash-prefix>/
    projects/
        <project-uuid>/
            project.json
            level.json
            cover-source.<ext>
            autosave/
    exports/
        <project-uuid>/
```

Suggested device layout:

```text
/.rockbox/games/maker_lite/
    kits/
        <kit-id>/
            kit.mlk
            art.mla
            audio.mla
            provenance.tsv
    projects/
        <project-id>/
            game.mlp
            cover.144x108x24.bmp
            metadata.tsv
    saves/
        <project-id>.sav
    settings.cfg
    projects.tsv
/.rockbox/rocks/games/maker_lite.rock
/.rockbox/rocks/games/maker_lite/games.tsv
```

`projects.tsv` is the complete Classic-browser index and contains every
successfully synchronized project, including projects without covers or with
`Show in Steam` disabled. The 11-column `games.tsv` is the cover-gated Steam
catalog only.

`provenance.tsv` records the source digest, supported revision ID, extractor
version, output digests, and conversion operations. It does not contain the
ROM path from the host.

The concrete revision allowlist, local extraction-recipe schema, native
SNES/Genesis 4bpp decoders, and prepared-bundle compatibility contract are in
[`maker-lite-authentic-assets.md`](maker-lite-authentic-assets.md).

## Rockpod creator

### Navigation

Add `Maker Lite` beneath the existing Games area, next to Game Sync. Its
landing page has:

- `Projects`;
- `Asset Kits`;
- `New Project`;
- `Import Project`;
- `Sync Status`; and
- `Open Saves`.

The creator uses the existing Rockpod visual system and Qt controls, but its
canvas is purpose-built. It must remain usable at 1280x720 and scale cleanly
on HiDPI displays.

### Editor layout

```text
┌ Projects / scenes ┬──────────── Level canvas ────────────┬ Inspector ┐
│ project cover     │ toolbar: select paint erase path     │ object    │
│ levels / rooms    │ grid, rulers, camera and safe frame  │ rules     │
│ checkpoints       │ authentic imported art only          │ links     │
├───────────────────┴──────────────────────────────────────┴───────────┤
│ Object palette / search | validation | performance | Test on iPod   │
└──────────────────────────────────────────────────────────────────────┘
```

Core interactions:

- authentic terrain and object cards drag directly from the searchable palette
  onto the canvas;
- large authentic kits are split into stable 72-part pages with previous/next
  controls and a visible `Page X of Y · N parts` count; search, category,
  `Pinned`, and `Recent` views reset and clamp pagination predictably;
- a semi-transparent source-art ghost and green/red grid outline follow the
  pointer, and a valid drop places exactly one grid-snapped asset;
- dropped assets use copy semantics, remain in the palette for repeated use,
  and create the same undoable operation as click placement;
- placed characters and objects can be grabbed and repositioned directly
  without first switching to a separate move mode;
- left click places or selects;
- drag paints tiles or moves a selection;
- right click erases or opens the context menu;
- wheel zooms; middle drag pans;
- `Space` pans temporarily;
- `Ctrl/Cmd+Z` and `Ctrl/Cmd+Shift+Z` undo/redo;
- `P` enters test mode at the selected spawn;
- `Esc` returns from test mode without losing edits.

### Course Maker interaction reference

The desktop interaction follows Nintendo's documented Course Maker behavior,
adapted to a mouse:

- Nintendo's 3DS manual says the palette contains placeable elements, can
  switch between sets, and can be rearranged for easier access to frequently
  used elements. Rockpod therefore uses kit-defined categories plus searchable
  `Pinned` and `Recent` sets.
- The same manual documents direct placement, moving Mario and placed
  elements, an eraser mode, undo, trial play, selecting multiple elements,
  copying, and dragging elements onto tracks. Rockpod keeps placement, direct
  object movement, Ctrl/Cmd-drag copy, eraser strokes, undo, test mode, and
  path tools in the main canvas rather than hiding them in dialogs.
- Nintendo's current Maker guidance explicitly documents pinning a favorite
  item to the parts palette. Rockpod persists pinned authentic part IDs in the
  project metadata.

Primary references:

- [Nintendo 3DS Super Mario Maker electronic manual](https://www.nintendo.com/eu/media/downloads/games_8/emanuals/nintendo_3ds_2/supermariomakerfornintendo3ds/ElectronicManual_Nintendo3DS_SuperMarioMakerForNintendo3DS_EN.pdf)
- [Nintendo: How to build cool courses](https://play.nintendo.com/news-tips/tips-tricks/super-mario-maker-2-tips-tricks/)
- [Official Super Mario Maker 2 Make page](https://supermariomaker.nintendo.com/make/)

Paint and erase motion is previewed locally and committed as one bounded
stroke on mouse release. It must not deep-copy the project or rebuild the full
scene for every raw mouse-move event. Palette/hover ghosts reuse their
`QGraphicsItem` instances and zoom remains anchored beneath the pointer.
Grid cells skipped between high-speed pointer samples are filled with integer
line interpolation, so fast mouse strokes stay continuous without waiting for
the event stream. Direct object dragging updates one cached ghost and commits
one move/copy operation on release.

The canvas shows the exact native camera frame and iPod crop/gutters. A
toggleable input overlay visualizes click-wheel sectors and physical buttons
during test mode.

### Ruleset-specific tools

Mario provides:

- ground/ledge/pipe tile brushes;
- question, breakable, note, and turn blocks;
- coins, power-ups, doors/pipes, checkpoints, enemies, and goal;
- autoscroll and water regions; and
- moving-platform paths.

Zelda provides:

- screen/room authoring on a 16x16 grid;
- floor, wall, ledge, water, pit, stair, and doorway brushes;
- locked doors, keys, switches, push blocks, pots, chests, enemies, and exit;
- room-to-room links with reciprocal-link validation; and
- item requirement and simple event wiring.

Sonic provides:

- solid and one-way terrain;
- sampled slope/loop collision paths with visible normals;
- rings, monitors, springs, spikes, enemies, checkpoints, and goal;
- moving paths and camera bounds; and
- a heat map for speed traps, blind jumps, and collision discontinuities.

### Declarative events

Version 1 uses a small event graph rather than code:

```text
trigger -> condition -> action
```

Supported conditions are bounded comparisons such as `has_key`,
`rings >= N`, `switch_on`, `enemy_group_clear`, and `entered_region`.
Supported actions include `open_door`, `toggle_block_group`,
`spawn_group`, `play_effect`, `set_checkpoint`, and `complete_level`.

The editor rejects cycles without a delay or one-shot edge. The iPod runtime
has fixed arrays for events and never allocates nodes while playing.

### Validation

Export is blocked by:

- no player spawn;
- no reachable completion goal;
- missing or mismatched asset kit;
- unknown object IDs;
- object/entity count over budget;
- invalid room links;
- collision path gaps over the ruleset tolerance;
- a project path escaping its root;
- corrupt source or output hashes; or
- missing cover when `Show in Steam` is enabled.

Warnings, which do not block export, include:

- a section exceeding the measured frame budget;
- too many simultaneous audio voices;
- a camera trap or spawn over solid terrain;
- an unreachable optional collectible;
- a ruleset action with no comfortable default click-wheel mapping; and
- use of a feature not yet qualified on iPod Video 5G.

### Preview parity

Do not write separate Python physics for the editor. Put deterministic world
simulation, collision, rulesets, event evaluation, and pack decoding in
portable C under `lib/maker_lite/`. Build it:

- into `maker_lite.rock` through the Rockbox plugin build;
- as a small host shared library loaded by Rockpod; and
- into headless trace and fuzz tests.

Rockpod may render the host core with Qt, but Qt is not allowed inside the
simulation. Preview and device use the same fixed timestep, fixed-point math,
object limits, pack decoder, and deterministic random generator.

## Open Surge research and use

The evaluated upstream baseline is Open Surge 0.6.1.3, released in January
2026. Upstream describes it as a C-based retro engine inspired by 16-bit Sonic,
using Allegro, SurgeScript, and PhysicsFS. It provides:

- human-readable `.lev` scene files;
- a built-in level editor;
- bricks and scriptable entities;
- character files with movement multipliers and animation mappings;
- sprite sheets plus `.spr` animation descriptions and override folders;
- 360-degree platform physics suitable for slopes and loops;
- mod loading, multiple players, and custom mechanics through SurgeScript; and
- desktop and Android targets.

Official references:

- [Open Surge repository and build dependencies](https://github.com/alemart/opensurge)
- [Open Surge 0.6.1.3 release notes](https://github.com/alemart/opensurge/releases/tag/v0.6.1.3)
- [Open Surge level specification](https://wiki.opensurge2d.org/Level_specification)
- [Open Surge level editor tutorial](https://wiki.opensurge2d.org/How_to_make_a_level)
- [Open Surge character format](https://wiki.opensurge2d.org/Characters)
- [Open Surge sprite format and overrides](https://wiki.opensurge2d.org/Sprites)
- [SurgeScript level API](https://docs.opensurge2d.org/engine/level/)

### Why the full engine is not the iPod runtime

Open Surge's supported platform layer assumes Allegro 5 plus its display,
event, timer, image, font, and audio facilities; current builds also require
SurgeScript and PhysicsFS. Rockbox has none of those host abstractions.
Supplying them would mean creating and maintaining a substantial Allegro 5
backend before any Maker Lite work.

The iPod 6G target has a 216 MHz S5L8702, no usable graphics accelerator for
this plugin, a 320x240 RGB565 LCD, an 8 KiB native main-thread stack, and a
3 MiB plugin buffer. The iPod Video target is weaker. Open Surge's Android
performance work and optional OpenGL ES path do not predict bare-metal
Rockbox performance. A full port would also bring a general scripting VM and
asset system that Maker Lite v1 intentionally does not need.

Open Surge is GPLv3. A private experimental plugin can comply with that
license, and Rockbox source files commonly permit GPLv2-or-later, but a copied
or modified engine would still require explicit source/license packaging.
Using concepts and a clean import bridge avoids making the complete Open Surge
codebase a permanent Rockbox dependency.

### What to reuse

Reuse these ideas:

- brick/entity separation;
- readable source projects compiled to a runtime pack;
- explicit sprite animation metadata;
- character-specific tuning;
- override packs rather than edited base assets;
- slope/loop collision visualization;
- grid and entity palettes; and
- per-level author/license metadata.

Open Surge is not an authenticity source. Its shipped Surge artwork and
character are not substitutes for Sonic 2/3 pixels or movement. A Maker Lite
Sonic kit must still come from a verified owned Sonic 2 or Sonic 3 revision.
The useful boundary is the documented brick/entity split, explicit sprite and
collision metadata, and import of reviewable `.lev` layouts.

Provide an optional, host-only **Open Surge subset importer** for existing
Sonic layouts:

- input: one `.lev`, with an optional reviewed brick-ID collision mapping at
  the service boundary;
- accepted: `name`, `author`, `spawn_point`, exact-grid static `brick`
  commands, and allowlisted `entity` commands;
- rejected with line-number diagnostics: scripts, unknown entities, arbitrary
  setup objects, non-grid bricks, guessed brick collision, shaders, and
  unsupported behaviors;
- output: editable Maker Lite project data, never a direct device pack.

If `zone.lev` is accompanied by `zone.maker-lite-bricks.json`, Rockpod loads
its reviewed collision mapping:

```json
{
  "format_version": 1,
  "brick_collision": {
    "1": "solid",
    "2": ["slope_up", "loop"],
    "3": []
  }
}
```

IDs must be canonical decimal values from 0 through 255 and collisions must
come from Maker Lite's fixed allowlist. Without this sidecar, bricks remain
decorative and produce diagnostics; the importer never guesses collision from
appearance.

Do not promise round-trip export to Open Surge in version 1. Do not use its
editor as Rockpod's UI, and do not use Open Surge preview as the final parity
test.

## Runtime architecture

### Modules

```text
maker_lite.rock
    frontend/       plugin entry, menus, save/exit, LCD and input
    core/           fixed tick, entities, events, camera, deterministic RNG
    render/         tile spans, sprite batches, palette effects, HUD
    audio/          bounded PCM effects and mixer lifecycle
    rules/mario/    Mario state machine and objects
    rules/zelda/    Link state machine, rooms, items and objects
    rules/sonic/    Sonic inertia, angles, paths and objects
    pack/           validated kit/project readers
```

Only one ruleset is initialized for a launch. Shared code covers file
validation, tile drawing, broad-phase queries, audio transport, input
normalization, menus, and saves; it must not flatten character behavior into a
common lowest denominator.

### Simulation and rendering

- Simulation runs at a fixed 60 Hz using signed fixed-point arithmetic.
- Input is sampled every simulation tick and edge events are retained until
  consumed.
- Rendering targets 30 frames per second on iPod 6G while simulation remains
  60 Hz. A measured 60 fps render mode may be enabled later.
- Rendering may skip presentation, never simulation.
- Animation position derives from simulation ticks, not delivered LCD frames.
- The renderer draws directly into the existing LCD framebuffer or bounded
  strips. It does not allocate a second full-screen framebuffer.
- The validated pack and its bounded tile map stay resident in the plugin
  arena; there is no gameplay-time map I/O.
- Assets are atlas-packed in host order for sequential RGB565 copies.
- No file open, asset decode, allocation, or map chunk load occurs inside a
  sprite/tile draw callback.
- Storage and validation finish before play begins; every loop still yields
  immediately to input, Hold, USB, or exit.

### Initial hard limits

| Resource | Limit |
| --- | ---: |
| total resident arena from `plugin_get_buffer()` | normal 3 MiB plugin buffer |
| authentic effect slots | 6, each at most one second |
| tile/sprite atlas working set | 768 KiB |
| decoded map neighborhood | 128 KiB |
| live entities | 192 |
| event graph nodes | 256 |
| simultaneous effects | 1 mixer side-channel voice |
| Mario/Sonic map size | 512x64 16-pixel tiles |
| Zelda rooms | 64 |
| Zelda room size | one native camera frame |
| native function stack warning | 2 KiB |
| total native main-thread stack | 8 KiB |

These are maximum design inputs, not permission to fill every budget
simultaneously. The build gate records plugin text/data/BSS, remaining plugin
buffer, peak arena use, maximum live entities, and worst frame time.

The runtime must fit in the normal 3 MiB plugin buffer and should not call
`plugin_get_audio_buffer()`. If implementation measurements prove that
impossible, the design must be reviewed against
`docs/plugin-audio-lifecycle-steering.md` before taking playback memory.

### Pack format

Source projects are JSON for diffs and recovery. Device packs are little-endian
binary and start with:

```text
magic = "RPML"
format_version
ruleset_id
ruleset_version
kit_id
project_id
flags
native_view_width
native_view_height
section_directory_offset
section_count
file_size
payload_crc32
```

Sections have a type, offset, compressed size, decoded size, element count,
and CRC32. Unknown required sections reject the pack; unknown optional
sections are skipped. Counts and offsets are validated before allocation.
No string from a pack becomes an unchecked filesystem path.

The implemented RPML v3 keeps the fixed 128-byte validated header used by the
portable core. Its tile section stores little-endian 16-bit atlas cells; its
collision section stores one byte per map cell; and each 18-byte entity stores
kind, flags, position, four behavior parameters, and an independent 16-bit
render cell. This permits many authentic enemies and decorations to share a
behavior kind without sharing artwork. Readers retain compatibility with
RPML v1 and v2. Kit atlases remain shared across projects so ten levels do not
duplicate the same Mario, Link, or Sonic sheets.

### Saves

Save files are private to `project_id` and contain:

- pack digest;
- ruleset/save schema version;
- last checkpoint or room;
- completion and best-time state;
- collected project-local flags; and
- settings overrides.

Write to a sibling temporary file, close, validate by reopening, move the
existing valid save to `.previous`, and then rename the verified replacement.
An interrupted commit restores `.previous`; an incompatible or corrupt current
save is quarantined as `.corrupt` and a valid previous save is recovered in
the same launch.

## Click-wheel controls

The physical click wheel is treated as a touched radial pad, not merely two
scroll events. `wheel_status()` is normalized into eight sectors with
hysteresis. Touching a sector holds a direction; leaving the wheel releases
it. Physical click buttons remain independent actions. A pressed physical
button suppresses the underlying touch sector for that tick so pressing Menu
does not also move up.

### Default presets

| Input | Mario | Zelda | Sonic |
| --- | --- | --- | --- |
| wheel touch | left/right; down crouches | eight-way movement | left/right; down crouches/rolls |
| Select | jump / confirm | sword / confirm | jump / confirm |
| Play/Pause | run/fire / secondary | equipped item | crouch/roll modifier; with Select charges spin |
| Previous | camera look left / reserve item | cycle item left / shield modifier | look left |
| Next | camera look right / reserve item | cycle item right / shield modifier | look right |
| wheel rotation | reserve-item selection | quick item selection | optional camera glance |
| Menu short | pause menu | inventory/pause | pause menu |
| Menu hold | save and exit to library | save and exit to library | save and exit to library |
| Hold switch | pause, lock overlay, resume when unlocked | same | same |

The pause menu includes Controls and shows an actual click-wheel diagram.
Rockpod can choose `Stock`, `Left-handed`, or `Custom` per project. Custom
bindings are stored outside the project pack so replacing a level does not
overwrite the user's device preference.

New physical action presses use the existing short haptic helper when enabled.
Continuous wheel movement does not buzz. USB connection saves if safe, stops
audio, and exits through the normal Rockbox system path.

## Audio lifecycle

Rockpod accepts user-supplied authentic effects as uncompressed signed 16-bit
stereo PCM at 44.1 or 48 kHz. Each of the six bounded effects is at most one
second, the complete payload is loaded before play, and no decode or allocation
occurs during a frame. Version 1 does not install a project-music channel.

Required behavior:

- never create, clear, or replace the user's playlist;
- if Rockbox music is active at launch, keep it active and suppress all project
  audio;
- if Rockbox music is not active, play authentic effects only through the
  bounded `PCM_MIXER_CHAN_BEEP` side channel;
- install callbacks only after their rings and voices are initialized;
- pause/stop channels and clear callbacks before replacing a level or kit;
- on exit, stop channels, wait until callbacks cannot reference plugin memory,
  restore mixer frequency/source state, and then free the arena;
- leave both Database and Files playback able to start immediately after exit.

Background music remains out of scope until it passes the lifecycle and CPU
gates. Do not substitute a sound-alike track.

## Steam and game-library integration

Rockpod writes two compatible 11-column launcher manifests:

```text
/.rockbox/games/maker_lite/projects.tsv
/.rockbox/rocks/games/maker_lite/games.tsv
```

Each row uses:

- title: project title;
- launch path: `/.rockbox/rocks/games/maker_lite.rock`;
- cover path: the project's 144x108 BMP;
- favorite and save hint;
- year, genre, publisher/author, developer/creator, and description; and
- plugin parameter: absolute device path to `game.mlp`.

Extend `ipodjs_steam_load_library()` to read this third manifest. Map `.mlp`
parameters to platform `Maker Lite`, producing the requested console filter.
The existing Steam entry checks remain authoritative: the plugin, parameter,
and cover must all exist or the project does not appear.

`games.tsv` contains only projects that opt into Steam and have a readable
cover. `projects.tsv` contains every synchronized project; its cover field may
be empty. Classic Games adds `Maker Lite` and reads this complete browser
index, falling back to the legacy Steam manifest for older synchronized
devices. The browser index is not a duplicate project database: both
manifests point at the same `game.mlp` payloads. Sync commits project and kit
data first, then `projects.tsv`, then the Steam catalog, so neither launcher
can advertise a partially staged project.

Cover behavior in Rockpod:

- `Change Cover` opens a local file picker;
- crop, contain, and background color are explicit user choices;
- preview shows the exact 144x108 Steam result and any source cropping;
- saving a new cover changes only the art asset and manifest digest;
- sync atomically replaces the cover;
- `Restore Source Cover` returns to the source-game cover selected during kit
  import; and
- no-cover projects stay out of Steam rather than showing invented art.

## Implemented repository surface

Rockbox:

```text
apps/plugins/maker_lite/
apps/plugins/SOURCES
apps/plugins/CATEGORIES
apps/root_menu.c
lib/maker_lite/
```

Rockpod:

```text
rockpod/services/maker_lite_assets.py
rockpod/services/maker_lite_projects.py
rockpod/services/maker_lite_export.py
rockpod/services/maker_lite_neon_nook.py
rockpod/services/maker_lite_runtime.py
rockpod/services/maker_lite_open_surge.py
rockpod/services/maker_lite_settings.py
rockpod/services/maker_lite_trace.py
rockpod/ui/maker_lite_creator.py
rockpod/ui/maker_lite_canvas.py
rockpod/ui/maker_lite_cover_dialog.py
rockpod/tests/test_maker_lite_*.py
```

Tools and documentation:

```text
tools/maker_lite_host_gate.py
tools/maker_lite_pack_inspect.py
tools/maker_lite_sim_gate.py
tools/maker_lite_launcher_sim_gate.py
tools/maker_lite_neon_nook_sim_gate.py
tools/maker_lite_trace_gate.py
tools/maker_lite_package_leak_gate.py
tools/build_maker_lite_neon_nook_pack.py
assets/maker_lite/neon_nook/
docs/maker-lite-authentic-assets.md
docs/mario-maker-lite-ipod-spec.md
testdata/maker_lite/
```

Commercial private kit/cache directories and extracted derivatives are
gitignored. The original Neon Nook sources and reproducible bounded pack are
checked in. There is no checked-in `assets/maker_lite/nintendo` or
`assets/maker_lite/sega` directory.

## Delivery milestones

### M0 — feasibility harness

- portable C core runs a rectangle player and tile collision headlessly;
- same trace result on host, simulator, and ARM build;
- direct RGB565 tile/sprite prototype sustains 30 fps on iPod 6G;
- fixed arena and stack audit pass;
- launch/exit leaves active and stopped music states healthy.

This milestone is a hard gate. Do not build the full creator until native frame
and audio lifecycle measurements are credible.

### M1 — private kits and Mario vertical slice

- built-in, hash-gated SMW importer with dynamic Mario frames and a broad
  Map16 source library;
- authentic Mario, ground, coin, one enemy, block, and goal;
- Rockpod canvas, undo/redo, validation, test, export, cover, and sync;
- Maker Lite Steam console registration;
- one five-minute Mario test level completes on hardware.

### M2 — Mario Lite

- complete version-1 Mario object list;
- checkpoints, saves, water, moving platforms, power states, and audio;
- parity traces and stress maps pass.

### M3 — Zelda

- built-in, hash-gated ALttP extractor for Link DMA frames, named starter
  objects, multiple background sets, and a paged sprite-source library;
- rooms, transitions, sword, item, keys/doors, switches, push blocks, enemies,
  completion, and save schema;
- Zelda-specific creator tools and control tutorial.

### M4 — Sonic

- verified Sonic 2 recipe importer and built-in Sonic 3 US extractor;
- original-size Sonic 3 mappings/DPLCs and v4 metasprite playback;
- authentic draggable ring, spikes, spring, Rhinobot, starpost, monitor, goal,
  Angel Island terrain, and decoded source-stream parts;
- fixed-point path collision, slopes/loops, roll/spin, rings, springs,
  checkpoint, damage, goal, and camera;
- optional Open Surge subset importer;
- worst-case speed/collision stress map passes on hardware.

### M5 — iPod Video qualification

- profile 5G CPU, storage, audio, and LCD separately;
- lower only render cadence or resident asset window where required;
- never alter physics tick rate, character constants, or authentic pixels to
  claim support;
- mark 5G unsupported until the full hardware gate passes.

## Test gates

### Host and Rockpod

- known-good and known-bad ROM hash detection;
- bounded folder scans match owned sources by content, never filenames;
- suite installation pairs each source with exactly one complete recipe;
- `@source` native-tile recipes decode the verified image without copying it;
- deterministic extraction and output hashes;
- no source ROM copied into exports;
- undo/redo round trips every editor operation;
- corrupt JSON recovery from autosave;
- pack encode/decode/property fuzzing;
- malicious count, offset, CRC, path, and decompression inputs reject safely;
- custom cover replacement changes no gameplay data;
- preview and headless traces are bit-identical;
- Open Surge subset import produces useful diagnostics for unsupported input.

### Simulator

- launch each ruleset from its Steam cover and Classic entry;
- 30-minute scripted play per ruleset;
- repeated level restart, checkpoint, pause, Hold, resume, exit, and relaunch;
- missing kit, missing cover, corrupt pack, old save, and full save directory;
- active playback remains the same playlist/track when project music is
  suppressed;
- Database -> Maker Lite -> Database and Files -> Maker Lite -> Files;
- rapid Games/Steam/project/Menu navigation without memory or descriptor
  growth;
- no asset I/O or allocation from draw functions;
- `tools/ipodjs_navigation_sim_regression.sh` still passes.

### iPod 6G hardware

- p95 presented frame <= 33.3 ms and no simulation backlog over two ticks;
- input-to-visible-action <= 80 ms;
- stable frame pacing through a maximum-speed Sonic loop;
- no stack warning over 2 KiB and no 8 KiB stack overflow;
- fixed plugin arena returns to its exact baseline after every level;
- 100 launch/play/exit cycles without freeze or descriptor leak;
- Hold always pauses and never becomes directional input;
- USB exit is clean;
- saves survive forced restart and a nearly full volume;
- game audio, volume, pause, and exit pass the complete plugin audio lifecycle
  matrix;
- Database and Files playback both work immediately after every ruleset;
- device database/tagcache checksums remain unchanged by targeted plugin/data
  deployment.

### Asset authenticity

- every displayed gameplay atlas entry resolves to a provenance row;
- authenticated commercial-kit pixels match the extracted source after
  allowed palette conversion;
- no AI/generated/placeholder path can fill a missing commercial source asset;
- missing commercial assets remove an object from the palette rather than
  synthesizing it;
- bundled original kits declare `source.type=original-generated`, remain
  visibly distinct from authenticated commercial kits, and pass the same
  bounded-format, checksum, catalog, and per-cell provenance validation;
- release/package leak gate finds no commercial ROM or private kit.

## Definition of done

Mario Maker Lite is done when a user can import personally supplied reference
games into Rockpod, build and test distinct Mario, Zelda, and Sonic projects
with authentic local assets and faithful character rules, assign or replace
each project's cover, sync them safely, browse them under the `Maker Lite`
console in the existing Steam-style iPod library, and play them with
click-wheel-first controls on real iPod 6G hardware without regressions to
music playback, playlists, database data, memory, or input responsiveness.
