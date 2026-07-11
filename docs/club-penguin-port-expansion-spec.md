# Club Penguin Offline Port Expansion Spec

## Goal

Expand the current **Club Penguin** Rockbox plugin from a real-asset island
viewer into a broader offline iPod-native port.

The port should stay:

- offline-first
- real-assets-only
- playable on iPod Classic 6G/7G hardware
- available from Games and Game Cover Flow
- free-membership by default
- independent from any live server, chat backend, account backend, or payment
  system

This is not a browser port. The current iPod target cannot reasonably host the
full Flash/HTML5 client stack plus network services. The expansion should use
host-side import tools to prepare assets and a small native Rockbox runtime to
display rooms, move a local penguin, and run simple local interactions.

## Current State

Implemented pieces:

- `apps/plugins/clubpenguin.c`
  - loads real BMP assets from `/.rockbox/rocks/games/clubpenguin/`
  - displays a scaled island map and 12 authentic offline room scenes
  - draws an offline penguin sprite
  - supports map-to-room transitions, bounded room movement, and return exits
- `apps/root_menu.c`
  - has a `Club Penguin` launcher under Games
  - includes the Club Penguin cover directory in game cover scan paths
- installed asset package:
  - `/.rockbox/rocks/games/clubpenguin/world.bmp`
  - `/.rockbox/rocks/games/clubpenguin/player.bmp`
  - `/.rockbox/rocks/games/clubpenguin/covers/ClubPenguin.bmp`
  - `/.rockbox/rocks/games/clubpenguin/rooms/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/data/{world,rooms}.tsv`
- source assets used so far:
  - `despedite/clubpenguinfreeroam`
  - `nhaar/Waddle-Forever` vanilla/legacy preserved room SWFs
  - `project-aether` login atlas was inspected and used earlier for cover work

Current room coverage:

- My Place (igloo)
- Town
- Plaza
- Dock
- Ski Village
- Dojo
- Cove
- Beach
- Snow Forts
- Forest
- Mine Shack
- Iceberg

Known constraints:

- the current plugin is a first slice, not a full room engine
- room scenes are static preserved frames; original SWF animation and room
  scripts do not run on-device
- walkability currently uses one rectangular floor zone per room
- no persistent inventory, coins, catalog, furniture, or clothing system exists
- no Flash VM or Phaser runtime is used by the plugin
- the hardware build can build core firmware with `make -C build-hw-ipod6g bin`
  while the full `make` still fails in unrelated plugin trees

Measured baseline for the next pass:

- the current iPod 6G plugin ELF has 62,180 bytes of text and 891,056 bytes
  of BSS
- the 854x480 map backing store accounts for most of BSS even though the
  visible playfield is only 320x220
- the player is composited with a per-pixel loop that changes foreground
  color for every visible sprite pixel
- the whole 320x240 display is redrawn at roughly 12 Hz, including while the
  player and UI are idle
- room-local interaction data and save data are not implemented yet

## Source Asset Policy

All visible game art must come from preserved Club Penguin-style sources or
explicitly imported local archive material.

Acceptable source classes:

- local Flashpoint package assets, when available
- checked-out public repos containing preserved Club Penguin client/media assets
- user-provided local SWF/PNG/JPG/WebP assets
- generated intermediate BMPs derived directly from the above

Not acceptable for the main game art:

- placeholder geometric drawings pretending to be rooms
- AI-generated replacement rooms
- invented furniture/player art for canonical areas
- online fetches at runtime

Generated assets are acceptable for:

- cover-flow thumbnails
- cropped/scaled/palettized derivatives
- debug overlays
- non-game UI framing that does not masquerade as original content

Every import should leave a small manifest near the assets:

```text
/.rockbox/rocks/games/clubpenguin/source.manifest
```

Suggested fields:

```text
CLUBPENGUIN_ASSET_MANIFEST_V1
source_repo=despedite/clubpenguinfreeroam
source_commit=<sha>
source_path=images/sprite-sheet0.png
generated=world.bmp
sha256=<hash>
```

## Target Package Layout

Use one package root under Games:

```text
/.rockbox/rocks/games/clubpenguin.rock
/.rockbox/rocks/games/clubpenguin/
/.rockbox/rocks/games/clubpenguin/covers/ClubPenguin.bmp
/.rockbox/rocks/games/clubpenguin/world.bmp
/.rockbox/rocks/games/clubpenguin/player.bmp
/.rockbox/rocks/games/clubpenguin/rooms/
/.rockbox/rocks/games/clubpenguin/sprites/
/.rockbox/rocks/games/clubpenguin/data/
/.rockbox/rocks/games/clubpenguin/save.dat
/.rockbox/rocks/games/clubpenguin/source.manifest
```

Room files:

```text
rooms/town.bmp
rooms/plaza.bmp
rooms/coffee.bmp
rooms/nightclub.bmp
rooms/petshop.bmp
rooms/player_home.bmp
rooms/dock.bmp
rooms/ski_village.bmp
rooms/dojo.bmp
rooms/cove.bmp
rooms/beach.bmp
rooms/snow_forts.bmp
rooms/forest.bmp
rooms/mine.bmp
rooms/iceberg.bmp
```

Data files:

```text
data/world.tsv
data/rooms.tsv
data/warps.tsv
data/items.tsv
data/dialogue.tsv
data/catalog.tsv
```

Keep data line-oriented and TSV-based. Avoid JSON in firmware code unless a
small parser is already linked for this plugin.

## Runtime Architecture

Split the port into four native systems.

### 1. Shell / Launcher

Responsibilities:

- load package metadata
- validate required assets
- show title/loading errors
- initialize save state
- route between island map and rooms

This should remain in `apps/plugins/clubpenguin.c` until it becomes too large.
When it grows, split into:

```text
apps/plugins/clubpenguin.c
apps/plugins/clubpenguin/cp_assets.c
apps/plugins/clubpenguin/cp_world.c
apps/plugins/clubpenguin/cp_room.c
apps/plugins/clubpenguin/cp_save.c
```

### 2. Asset Loader

Responsibilities:

- load fixed-size BMPs with `read_bmp_file`
- support `FORMAT_NATIVE | FORMAT_DITHER`
- avoid alpha unless buffer sizes include Rockbox alpha side storage
- expose hard errors when required assets are absent

Rule:

- canonical room backgrounds should be pre-scaled to 320x220 or 320x240
- runtime scaling should be avoided on iPod hardware

### 3. Room Engine

Responsibilities:

- display room background
- draw local penguin
- move player through walkable zones
- handle exit triggers
- handle local hotspots
- return to island map

Each room needs metadata:

```text
id	title	bmp	start_x	start_y	walk_left	walk_top	walk_right	walk_bottom
town	Town	rooms/town.bmp	160	170	25	120	295	198
player_home	My Place	rooms/player_home.bmp	160	170	45	120	275	198
```

Warp metadata:

```text
from	x	y	radius	to	to_x	to_y	label
map	91	65	25	player_home	160	150	My Place
town	42	188	18	map	91	65	Map
town	145	80	20	coffee	160	150	Coffee Shop
```

Walkable zones should start simple:

- rectangular zones per room
- optional blocked rectangles for furniture/buildings
- later: polygon zones if needed

### 4. Offline Progression

Responsibilities:

- free membership flag always true
- local coins
- local unlocked clothing/furniture
- selected color
- current room
- basic inventory

Save format:

```text
CLUBPENGUIN_SAVE_V1
membership=free
coins=500
color=blue
room=map
x=91
y=65
items=blue_color,red_ballcap
furniture=basic_igloo,chair_1,table_1
```

No account, password, email, moderation, chat server, or payment fields should
exist.

## Expansion Phases

## Phase 1: Stabilize Current Slice

Target outcome:

- current island map loads reliably on simulator and hardware
- no `Club Penguin assets missing` false positives
- cover appears in Game Cover Flow
- Games menu launches directly

Tasks:

- keep `player.bmp` as 24-bit unless alpha storage is correctly handled
- add `source.manifest`
- add `data/world.tsv` for current map hotspots
- move hotspot definitions out of C into TSV
- improve missing-asset splash to say exactly which file failed
- add a small `tools/clubpenguin_package_assets.py`

Validation:

- simulator smoke run starts
- hardware `.rock` starts from Games
- cover visible in Game Cover Flow
- `SELECT` on My Place reports offline room entry
- exit returns cleanly to Rockbox

## Phase 2: First Real Room

Status: complete for the current native slice.

Target outcome:

- selecting My Place enters a real room background
- player can walk inside room
- `MENU` or room exit returns to island

Preferred room:

- use a real preserved room/igloo-style asset if available
- name it `player_home` in code/data, not `igloo`, because this package will
  eventually include the whole game

Tasks:

- locate or import a preserved room background
- pre-scale to `rooms/player_home.bmp`
- add room loader and state enum
- add room/player coordinates
- add rectangular walkable area
- add warp back to map

Validation:

- start from Games
- enter My Place
- move in all directions
- blocked boundaries work
- return to map

## Phase 3: Core Island Rooms

Status: core map-to-room coverage complete for all 12 current map markers.
Interior-to-interior door warps and room-local interaction hotspots remain.

Target outcome:

- island map can enter several preserved rooms
- room transitions are local and fast

Recommended first set:

- Town
- Plaza
- Coffee Shop
- Night Club
- Pet Shop
- Dock
- Cove
- Ski Village
- Dojo
- My Place

Tasks:

- define `rooms.tsv`
- define `warps.tsv`
- import room BMPs
- add room-local hotspot text
- add current-room save/load
- add fallback if a room asset is missing

Validation:

- every map hotspot either enters a room or gives a clear missing-room message
- every room has a way back to map
- memory remains below plugin limit
- no dynamic allocation churn while moving

## Phase 3.5: Improvement And Optimization Pass

This is the next implementation pass. Complete its renderer and data-model
work before expanding room count or adding Cart Surfer content.

Target outcome:

- map and rooms feel immediate on iPod Classic hardware
- only room scenes contain a walking penguin; the island map behaves as a
  destination selector
- room-local exits and actions are data-driven
- the runtime has enough memory and frame-time headroom for a native minigame
- simulator builds expose repeatable performance counters without changing a
  release build's UI

### 3.5.1 Map And Memory Shape

Change the device package from the current 854x480 scrolling map to a
host-prepared 320x220 map. Preserve the high-resolution image only as an
import source.

The importer must:

1. scale and letterbox the source map to exactly 320x220
2. transform hotspot centers and radii into screen coordinates
3. reject hotspots outside the visible image bounds
4. emit the transformed values in `data/world.tsv`
5. record source and generated checksums in `source.manifest`

The map scene should draw a highlighted destination marker or cursor, not the
room walking sprite. `LEFT`, `RIGHT`, and wheel movement select the nearest
destination in that direction; `SELECT` enters it. This makes map navigation
distinct from room movement and removes the misleading appearance that the
penguin is walking across the island illustration.

Memory acceptance gate:

- replace the 854x480 scene allocation with a 320x220 scene allocation plus
  only the BMP decoder scratch space actually required by the packaged files
- keep map and room backgrounds in the same buffer and reload on transition
- target plugin BSS at or below 256 KiB on the iPod 6G build
- perform no allocation, asset decode, or filesystem access in a movement or
  minigame frame

### 3.5.2 Renderer And Input Loop

Replace `cp_draw_player()`'s per-pixel color-key loop with
`lcd_bitmap_transparent_part()`. The importer must continue to use the
Rockbox transparent color key and must validate all sprite frame dimensions.
Clip source and destination rectangles before calling the bitmap function.

Use a 25 Hz fixed update clock for room animation and Cart Surfer. Input may
be polled every tick, but rendering is required only when one of these becomes
dirty:

- scene or camera changes
- player position or animation frame changes
- selected hotspot changes
- message visibility changes
- status values change

Room movement uses a bounded target queue rather than applying the full input
distance in the button event handler. Each fixed update advances at most two
pixels per axis toward that target, and queued travel is capped at 12 pixels
from the current position. On click-wheel iPods, held `LEFT` and `RIGHT` are
sampled directly each update so movement does not inherit Rockbox's initial
button-repeat delay. Wheel detents add short vertical movement impulses to the
same queue.

Start with a full 320x240 redraw when dirty. Add background restoration and
`lcd_update_rect()` only if hardware timing shows a full dirty frame misses
the 40 ms budget; do not add fragile dirty-rectangle complexity solely on
simulator results.

Add a compile-time debug HUD, disabled in packaged builds, showing:

- update count and rendered-frame count
- worst and rolling-average update/render time
- scene loads and failed loads
- current state, room, and interaction ID

Renderer acceptance gates on iPod 6G hardware:

- 25 updates per second during continuous movement
- no visible input backlog after releasing the wheel or direction buttons
- 95 percent of dirty frames complete within 40 ms
- idle rooms render only for an animation or UI-state change
- 15 minutes of map/room transitions produce no crash or memory growth

### 3.5.3 Room Interactions And Transitions

Add `data/interactions.tsv` rather than adding special cases to
`clubpenguin.c`:

```text
room\tid\tx\ty\tradius\taction\ttarget\tlabel
mine\tcart_surfer\t246\t127\t28\tminigame\tcart_surfer\tPlay Cart Surfer
town\tcoffee_door\t160\t105\t24\troom\tcoffee\tEnter Coffee Shop
```

Supported Phase 3.5 actions are:

- `room`: load another room and its spawn point
- `map`: return to the island selector
- `minigame`: launch a registered native minigame
- `message`: show local interaction text

Coordinates shown above are provisional transformed coordinates. The import
tool, not a hand-maintained table, must derive the final Cart Surfer hotspot
from the preserved room trigger (`spawn` 585,280 in its 760x480 source).

Validate the whole interaction graph at package time:

- every room and minigame target exists
- every hotspot lies inside its room and has a positive radius
- every room has a path back to the map
- duplicate IDs in one room are rejected
- a missing optional interaction is disabled with a clear status message;
  missing required data fails package validation

### 3.5.4 Save And Recovery Foundation

Introduce `CLUBPENGUIN_SAVE_V1` before awarding Cart Surfer coins. Save only
at stable boundaries: room transition, completed minigame, settings change,
and clean plugin exit.

Required fields for this pass:

```text
CLUBPENGUIN_SAVE_V1
coins=0
room=map
x=160
y=170
cart_best_score=0
cart_best_combo=0
```

Write a temporary file, close it, then atomically rename it over `save.dat`.
Clamp parsed numeric values, ignore unknown keys for forward compatibility,
and fall back to defaults on an invalid header or truncated file. A failed
save must not discard the last valid save.

## Phase 4: Player Customization

Target outcome:

- user can change penguin color and a small set of clothing items
- all membership-gated choices are available offline

Tasks:

- import real penguin sprite sheet variants if available
- add `items.tsv`
- add local inventory
- add simple wardrobe UI
- save selected item/color

Scope limits:

- do not implement multiplayer avatar layering yet
- do not implement online item IDs unless needed for asset provenance

Validation:

- selected color persists after plugin restart
- wardrobe uses real assets
- missing clothing asset does not crash plugin

## Phase 5: Catalogs And Coins

Target outcome:

- offline catalog browsing
- all member items purchasable locally
- coins are local only

Tasks:

- add `catalog.tsv`
- add catalog UI
- add buy/unlock logic
- add local coin rewards for interactions
- add debug/free grant option if desired

Rules:

- membership is always free
- no payment UI
- no server calls
- no ads

Validation:

- item purchase persists
- no negative coin underflow
- catalog exits cleanly

## Phase 6: Minigame Hooks

Target outcome:

- room hotspots can launch small native minigames or standalone plugin modules

Cart Surfer is the first required minigame. Fishing, Bean Counters, sled
racing, and dance-floor interactions remain later candidates.

## Phase 6A: Cart Surfer Native Vertical Slice

Cart Surfer will be a native fixed-step game, not an on-device Flash player.
Use preserved SWFs as the behavior and art reference, then convert only the
needed assets on the host.

Canonical source order:

1. `media/default/fix/CartSurfer2006.swf` for the first gameplay baseline
2. `media/default/slegacy/media/play/v2/games/mine/CartSurfer.swf` for later
   art and feature comparison
3. `media/default/svanilla/media/play/v2/games/mine/CartSurfer.swf` only when
   its behavior intentionally supersedes the legacy version

Pinned source facts from the Waddle Forever checkout:

```text
CartSurfer2006.swf
Flash version: 6
size: approximately 76 KiB
sha256: fd30e04c8de51fbfd970841fac9e761d0f9c41481f77b3bee74524a42fe188f9

slegacy CartSurfer.swf
Flash version: 9
size: approximately 778 KiB
sha256: a5356ec154d99c7648550634309edf05b18597ea7ef75199ccc2d46b56b2a506

svanilla CartSurfer.swf
Flash version: 9
size: approximately 777 KiB
sha256: d9609be70c7cc85ad6c4bd1d6b4f456a33978f98924e1289cb52909a7a4a838c
```

The package manifest must also record the pinned Waddle Forever commit
`bcf7e9d4d4f7619710492448d532f4e7eb1e5caa`. If the checkout, file hashes, or
extracted frame inventory changes, regenerate and visually review the Cart
Surfer package rather than silently accepting it.

### 6A.1 Package Layout And Import

Add:

```text
clubpenguin/minigames/cart_surfer/
clubpenguin/minigames/cart_surfer/title.bmp
clubpenguin/minigames/cart_surfer/tunnel.bmp
clubpenguin/minigames/cart_surfer/track.bmp
clubpenguin/minigames/cart_surfer/cart.bmp
clubpenguin/minigames/cart_surfer/obstacles.bmp
clubpenguin/minigames/cart_surfer/source.manifest
clubpenguin/data/cart_surfer.tsv
clubpenguin/ui/toolbar.bmp
clubpenguin/ui/source.manifest
```

Exact sprite-sheet division may change after extraction, but runtime files
must remain screen-sized backgrounds or bounded fixed-size sheets. The import
tool must:

- extract or render frames from the pinned SWF without redrawing canonical art
- crop transparent margins and normalize the Rockbox color key
- pack animation frames into rows no wider than the loader's validated limit
- emit frame rectangles, origins, and collision bounds in
  `data/cart_surfer.tsv`
- fail if a declared frame is outside its sheet or two IDs collide
- create a contact sheet for review but exclude it from the device package

No SWF, ActionScript VM, PNG decoder, or runtime scaling belongs in the iPod
package.

### 6A.2 Runtime States And Ownership

Keep the vertical slice inside `clubpenguin.rock` so entry, save, and return
behavior share one owner. Split implementation into focused files when the
first slice begins:

```text
apps/plugins/clubpenguin.c
apps/plugins/clubpenguin/cp_cart_surfer.c
apps/plugins/clubpenguin/cp_cart_surfer.h
```

Required states:

```text
CART_TITLE -> CART_COUNTDOWN -> CART_PLAYING
CART_PLAYING -> CART_CRASH -> CART_PLAYING
CART_PLAYING -> CART_RESULTS -> previous room
```

The caller owns the previous room and save state. The minigame owns only its
loaded art, deterministic run state, score, lives, and frame clock. Exiting
from the title or pause screen returns to the Mine without awarding coins.
Finishing a run commits the reward once and then returns to the Mine.

### 6A.3 Playable Mechanics

The first playable slice must include:

- forward motion through a deterministic sequence of straight track, curves,
  jumpable obstacles, and hazards
- speed that rises in bounded steps and never depends on render rate
- left/right balance during curves
- jump, airborne trick, landing, crash, recovery, score, and the original four
  starting lives
- diminishing value for repeating the same trick, encouraging varied combos
- an end-of-track results screen with score, best score, and earned coins
- the original offline reward rule of `floor(score / 10)` coins, applied with
  overflow-safe arithmetic

Do not guess parity-critical constants. Before implementation, extract the
2006 ActionScript values for speed stages, trick scores, duplicate-trick
penalties, curve timing, lives, and reward conversion into a checked-in
reference table. Native constants and simulator tests must cite that table.

Use integer or fixed-point math only in the frame loop. Track segments should
be authored as a compact deterministic table, not generated with a heap-based
object list. Collision boxes should be deliberately forgiving and separately
defined from visible sprite bounds.

### 6A.4 iPod Controls

Controls should preserve the intent of Cart Surfer while fitting the click
wheel:

- `LEFT` / `RIGHT`: balance and steer through curves; modify an airborne trick
- `SELECT`: jump, confirm, or start
- wheel clockwise / counter-clockwise: rotate the airborne cart and select
  title/result choices when grounded
- `PLAY`: crouch/grind trick while playing; pause/resume otherwise
- `MENU`: open pause/back confirmation; a second confirmation abandons the
  run and returns to the Mine

Input is sampled into a per-tick edge/held state. Never perform two menu
transitions from one held button, and clear held inputs after countdown,
unpause, and crash recovery.

### 6A.5 Performance, Determinism, And Tests

Cart Surfer targets 25 updates and rendered frames per second on iPod 6G. If
hardware cannot sustain that after blit and asset optimization, the fallback
is 20 Hz with identical fixed-step physics; gameplay speed must never vary
with measured render time.

Acceptance gates:

- enter from the Cart Surfer hotspot in the Mine and return to that room
- complete and intentionally fail a full run without a crash or leaked state
- sustained play meets the 40 ms frame budget at 25 Hz, or the documented
  50 ms fallback budget at 20 Hz
- identical seed and scripted input produce identical segment order, score,
  lives, and results in simulator tests
- every trick can be triggered and every obstacle can be cleared
- repeated trick scoring and coin conversion match extracted reference data
- coin reward is committed exactly once, including after save failure/retry
- corrupt or missing Cart Surfer assets disable the hotspot with a useful
  message while normal rooms remain playable
- 30 consecutive runs do not grow memory or degrade transition time

Cart Surfer MVP is deliberately silent. Audio is a separate follow-up and may
begin only after the lifecycle matrix in
`docs/plugin-audio-lifecycle-steering.md` is completed. It must not stop or
replace the user's playlist.

## Phase 7: Audio

Target outcome:

- optional room ambience or short UI sounds

Do not start here. Audio touches plugin lifecycle risk.

Before editing audio code, follow:

```text
docs/plugin-audio-lifecycle-steering.md
```

Hard rules:

- do not mutate user playlist
- do not call `audio_stop()` directly before stealing shared audio buffer
- use mixer path if streaming audio
- test Database music -> plugin and plugin -> Database music

## Import Tool

Add:

```text
tools/clubpenguin_package_assets.py
```

Responsibilities:

1. Accept one or more source roots.
2. Discover supported source layouts:
   - `despedite/clubpenguinfreeroam`
   - `project-aether`
   - local Flashpoint media directories
   - user-provided image folders
3. Convert/crop/scale known assets with `ffmpeg`.
4. Write package tree under a selected output root.
5. Generate `source.manifest`.
6. Validate required files.

Example:

```bash
python3 tools/clubpenguin_package_assets.py \
  --freeroam /tmp/clubpenguinfreeroam \
  --output build-sim-ipod6g/simdisk/.rockbox/rocks/games/clubpenguin
```

Required output for Phase 1:

```text
world.bmp
player.bmp
covers/ClubPenguin.bmp
data/world.tsv
source.manifest
```

Required output for Phase 2:

```text
rooms/player_home.bmp
data/rooms.tsv
data/warps.tsv
```

Required output for Phase 3.5:

```text
world.bmp at exactly 320x220
data/world.tsv with transformed screen coordinates
data/interactions.tsv
```

Required output for Phase 6A:

```text
minigames/cart_surfer/*.bmp
minigames/cart_surfer/source.manifest
data/cart_surfer.tsv
```

Current import invocation:

```bash
python3 tools/clubpenguin_package_assets.py \
  --source /tmp/clubpenguinfreeroam \
  --room-frames /tmp/clubpenguin-room-captures \
  --waddle-source /tmp/waddle-forever \
  --cart-export /tmp/cart2006-export \
  --ui-export /tmp/cp-ui-export/2010 \
  --out assets/ipodjs/rockbox/clubpenguin
```

`room-frames` contains one Ruffle-rendered PNG per room ID. The importer crops
the actual 760x480 game canvas out of the 968x777 Ruffle window capture, scales
it to a full 320x220 24-bit BMP, and records the source SWF path, pinned
repository commit, source checksum, and generated checksum in
`source.manifest`.

The bottom 320x20 toolbar is real preserved interface art extracted from the
pinned 2010 interface SWF. It replaces Rockbox instruction text in the map,
rooms, and Cart Surfer. Unsupported chat and social buttons remain visual-only
until their offline actions are implemented.

## Controls

iPod 5G/6G baseline:

- `MENU`: back out of a room or minigame; on the island map it does nothing
- `MENU` + `SELECT`: save and exit the plugin
- `SELECT`: interact / enter room / confirm
- `PLAY`: optional pause or open local action menu
- on the map, `LEFT/RIGHT` and wheel select destinations
- in rooms, `LEFT/RIGHT` move horizontally and wheel scroll moves vertically

The quit chord is checked from the raw button state as well as the action map
so either press order is reliable. A lone `MENU` action is delayed briefly,
allowing the chord to take priority without accidentally navigating backward.

Screen layout:

- world/room view uses top `LCD_HEIGHT - status_h`
- bottom strip is the preserved blue Club Penguin toolbar with no Rockbox text
- no keyboard/chat entry by default

## Performance And Memory

Target hardware:

- iPod Classic 6G/7G
- 320x240 LCD
- native color BMPs

Rules:

- pre-scale assets on host
- load one room background at a time
- avoid keeping all rooms resident
- avoid heap fragmentation
- prefer static buffers for current room/player
- avoid PNG/JPEG decode in plugin runtime

Next-pass memory shape:

```text
shared map/current-room background: 320 * 220 * sizeof(fb_data)
player and active-minigame sprite sheets: bounded static buffers
metadata: fixed arrays parsed from TSV
```

Hard requirements:

- map and room share the same background buffer
- reload world map when exiting a room
- keep only metadata resident
- release or reuse room-only sprite storage before Cart Surfer loads
- keep BSS at or below the Phase 3.5 target and record `size` output in the
  validation notes

## Build Strategy

Core firmware deploy:

```bash
make -C build-hw-ipod6g -j4 bin
```

Full plugin build currently has unrelated blockers in other plugin trees. Do
not use full `make` as the gate for this port until those are fixed.

Club Penguin hardware plugin can be built through normal plugin rules once the
generated build graph includes it. If blocked, the known working manual link
shape is:

```bash
arm-elf-eabi-gcc \
  -mcpu=arm926ej-s -nostdlib -ffreestanding \
  -Wl,--gc-sections \
  -Wl,-Map,build-hw-ipod6g/apps/plugins/clubpenguin.map \
  -Tbuild-hw-ipod6g/apps/plugins/plugin.link \
  -o build-hw-ipod6g/apps/plugins/clubpenguin.elf \
  build-hw-ipod6g/apps/plugins/plugin_crt0.o \
  build-hw-ipod6g/apps/plugins/clubpenguin.o \
  build-hw-ipod6g/apps/plugins/libplugin.a \
  build-hw-ipod6g/apps/plugins/bitmaps/libpluginbitmaps.a \
  build-hw-ipod6g/lib/libsetjmp.a \
  build-hw-ipod6g/lib/libfixedpoint.a \
  -lgcc

arm-elf-eabi-objcopy -O binary \
  build-hw-ipod6g/apps/plugins/clubpenguin.elf \
  build-hw-ipod6g/apps/plugins/clubpenguin.rock
```

Deploy rule:

```bash
cp build-hw-ipod6g/rockbox.ipod "/run/media/david/DAVID_S IPO/rockbox.ipod"
cp build-hw-ipod6g/rockbox.ipod "/run/media/david/DAVID_S IPO/.rockbox/rockbox.ipod"
sha256sum build-hw-ipod6g/rockbox.ipod \
  "/run/media/david/DAVID_S IPO/rockbox.ipod" \
  "/run/media/david/DAVID_S IPO/.rockbox/rockbox.ipod"
sync
```

## Validation Checklist

Phase 1:

- `clubpenguin.rock` exists in `.rockbox/rocks/games`
- `world.bmp`, `player.bmp`, and cover BMP exist under package root
- firmware strings contain:
  - `Club Penguin`
  - `clubpenguin.rock`
  - `clubpenguin/covers`
- Game Cover Flow sees the cover
- Games menu launches the plugin
- no `Club Penguin assets missing` error with packaged assets

Phase 2:

- My Place enters `player_home`
- room loads real background
- movement is bounded
- exit returns to map
- current room persists if save is enabled

Phase 3:

- all room entries in `rooms.tsv` load or fail with clear message
- all warps have valid targets
- repeated room switching does not leak or corrupt display

Phase 3.5:

- device map is 320x220 and all transformed markers remain selectable
- map uses a selector, while penguin walking remains inside rooms
- player rendering uses a transparent bitmap blit
- interactions load from TSV and the Mine exposes Cart Surfer only when its
  package validates
- iPod 6G ELF BSS and frame timings meet the documented gates
- invalid and truncated saves recover without destroying the last valid save

Phase 6A:

- Cart Surfer launches from and returns to the Mine
- a complete run, four-life failure, pause/abandon, and asset failure all
  follow their specified state transitions
- deterministic simulator replay, score, coin, repeated-run, and hardware
  frame-budget gates pass
- audio and the user's current playlist remain untouched

Hardware:

- build `bin`
- deploy firmware to both required locations
- verify firmware checksums
- deploy `.rock` and assets
- verify plugin/asset checksums
- `sync`

## Implementation Order

The earlier open choices are resolved for this pass: preserved Waddle Forever
assets remain canonical where available, room coverage precedes wardrobe and
catalog work, and the first Cart Surfer slice stays inside the main plugin.

Execute in this order, keeping each numbered item buildable:

1. Add timing counters and capture current simulator and iPod 6G baselines.
2. Generate the 320x220 map and transformed hotspot table; change the map to a
   directional destination selector.
3. Replace per-pixel player drawing with a clipped transparent bitmap blit and
   render only dirty frames.
4. Add `interactions.tsv`, graph validation, room-to-room actions, and the Mine
   Cart Surfer registration point.
5. Add atomic `save.dat` handling and simulator corruption/round-trip tests.
6. Extract the pinned 2006 Cart Surfer behavior constants and art inventory;
   check in the reference table, manifest data, and review contact sheet.
7. Implement Cart Surfer title, countdown, deterministic track, controls,
   tricks, collision, lives, scoring, results, and return-to-Mine flow.
8. Run simulator replay tests, hardware performance/stability loops, package
   validation, and the complete firmware/plugin deployment checklist.

Do not begin wardrobe, catalogs, extra minigames, or audio until items 1-8
pass. Those features would otherwise obscure regressions in the shared room,
save, renderer, and minigame foundations.
