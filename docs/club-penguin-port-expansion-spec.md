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
  - displays a scaled island map
  - draws an offline penguin sprite
  - supports movement and room hotspot interaction
- `apps/root_menu.c`
  - has a `Club Penguin` launcher under Games
  - includes the Club Penguin cover directory in game cover scan paths
- installed asset package:
  - `/.rockbox/rocks/games/clubpenguin/world.bmp`
  - `/.rockbox/rocks/games/clubpenguin/player.bmp`
  - `/.rockbox/rocks/games/clubpenguin/covers/ClubPenguin.bmp`
- source assets used so far:
  - `despedite/clubpenguinfreeroam`
  - `project-aether` login atlas was inspected and used earlier for cover work

Known constraints:

- the current plugin is a first slice, not a full room engine
- no igloo room has been imported yet
- no persistent inventory, coins, catalog, furniture, or clothing system exists
- no Flash VM or Phaser runtime is used by the plugin
- the hardware build can build core firmware with `make -C build-hw-ipod6g bin`
  while the full `make` still fails in unrelated plugin trees

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
id	title	bmp	start_x	start_y
town	Town	rooms/town.bmp	160	150
player_home	My Place	rooms/player_home.bmp	160	150
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

Candidate minigames:

- simple fishing loop
- bean counter style catching game
- sled-style timing game
- dance floor rhythm-lite interaction

Implementation options:

- keep minigames inside `clubpenguin.rock` if small
- split into separate `.rock` files only if memory/code size becomes painful

Validation:

- minigame returns to previous room
- coins are awarded locally
- no audio lifecycle changes unless explicitly implemented and tested

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

## Controls

iPod 5G/6G baseline:

- `MENU`: exit plugin or back out of current modal
- `SELECT`: interact / enter room / confirm
- `PLAY`: optional pause or open local action menu
- `UP/DOWN/LEFT/RIGHT`: move player or menu cursor
- wheel scroll: menu navigation where available

Screen layout:

- world/room view uses top `LCD_HEIGHT - status_h`
- bottom strip shows current room or interaction text
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

Initial memory shape:

```text
world/map background: 320 * 220 * sizeof(fb_data)
current room background: 320 * 220 * sizeof(fb_data)
player sprite: small static buffer
metadata: fixed arrays parsed from TSV
```

If memory becomes tight:

- map and room can share the same background buffer
- reload world map when exiting a room
- keep only metadata resident

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

Hardware:

- build `bin`
- deploy firmware to both required locations
- verify firmware checksums
- deploy `.rock` and assets
- verify plugin/asset checksums
- `sync`

## Open Questions

- Which preserved asset source should be canonical for room backgrounds?
- Should the first room be a real original igloo asset, a player-home room from
  a later client, or a room extracted from another preserved HTML5 remake?
- Should wardrobe/catalog be added before more rooms, or should room coverage
  come first?
- Should minigames live inside the same plugin or as separate plugins launched
  from room hotspots?

Recommended next pass:

1. build `tools/clubpenguin_package_assets.py`
2. move current hard-coded hotspots into `data/world.tsv`
3. import first `rooms/player_home.bmp`
4. implement room state and map-to-room transition
