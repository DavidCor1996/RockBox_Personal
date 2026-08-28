# Club Penguin Offline Port Expansion Spec

## Goal

Expand the current **Club Penguin** Rockbox plugin from a real-asset island
viewer into a broader offline iPod-native port.

The port must stay:

- fully offline at runtime
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
  - displays a scaled island map and 58 authentic offline room scenes
  - draws an official paper-doll penguin with 12 directional walk frames
  - supports map-to-room transitions, bounded room movement, and return exits
  - opens the exact 16-page April 2012 Penguin Style catalog with all 186
    purchase variants: 14 colors, 12 backgrounds, 50 flags, and 110 authentic
    wearable paper-doll layers across every original clothing department
  - opens the preserved nine-page June 2008 Snow and Sports catalog from the
    Sport Shop; all 25 canonical item calls and five furniture calls use their
    official names and prices, with 12 authentic wearable paper-doll strips
  - opens the Stage's preserved six-page April 2012 Costume Trunk for Ruby and
    the Ruby; all 15 original purchase calls use their official item IDs,
    names, types, and prices, with 14 authentic 193-frame paper-doll SWFs
    converted into directional walking strips
  - opens the exact 15-page December 2011 Martial Artworks catalog from the
    authentic red book already present in the Ninja Hideout; all 20 original
    calls buy their 15 clothing items, four furniture items, or Dojo Igloo,
    with official prices, quantity limits, persistence, and equip behavior
  - composites equipped paper-doll strips without taking another framebuffer
    allocation
  - includes an `Oliver Tree Full Look` preset matching the *Alone in a
    Crowd* era, assembled exclusively from preserved Club Penguin items: The
    Part, Cat Eye Sunglasses, Pink Sled Coat, and Pink Canvas Shoes
  - streams room interactions from the bundled TSV on every transition, so
    the complete 121-interaction graph is not limited by a fixed global cache
  - contains no USB Internet, multiplayer, account, chat, payment, CDN, or
    other network path; all runtime reads are from the installed package
  - opens the exact 11-page February 2011 Adopt-a-Puffle book in the Pet Shop;
    its ten original Blue, Red, Pink, Black, Green, Yellow, Purple, White,
    Orange, and Brown calls each cost the official 800 local coins
  - opens the exact five-page March 2010 Pet Furniture book from the real
    yellow catalog at the Pet Shop's lower-right; all 27 original furniture
    calls use their official IDs and prices, including the untouched Puffle
    Condo, Red/Gray House, and Gray Bed secret overlays
  - packages all 12 preserved classic puffle colors from their official igloo
    timelines; Rainbow/Gold use location-specific offline quest completions
  - stores ownership plus food, rest, happiness, and cleanliness independently
    for every puffle; the active puffle can be changed in the adoption and
    care screens
  - exposes 14 preserved food icons with the exact effects from the archived
    `puffle_items.json`; Puffle O's remain the free staple, Apple retains its
    four-coin price, and the twelve 65000-sentinel foods are dig rewards
  - supports persistent walk-with-me state in every room using authentic
    color-specific directional timelines, plus color-specific preserved dig
    animations, rare-food inventory, Sleep, and Bath care effects
  - opens Play into the original normal and super toy for every puffle color;
    all 24 selectors use the official icon and color-specific frame-27/28
    igloo animation, exact archived care effects, and persistent super-toy
    ownership. The toy screen opens all six original tricks: Jump Forward,
    Jump Spin, Nuzzle, Roll, Speak, and Stand on Head; every trick uses its own
    preserved eight-keyframe timeline for all 12 colors (72 authentic SWFs)
  - exposes all 68 official puffle head-item records in an offline wardrobe;
    63 use their preserved care-wearable SWF, remain owned/equipped per puffle,
    and save atomically. The five metadata/icon records whose wearable SWF is
    absent from the verified mirrors stay unavailable and never deduct coins
    rather than receiving substitute art
  - applies the 63 preserved room-hat pairs to walk-with-me puffles in all
    four cardinal directions and both retained animation frames; the official
    rear layer is composited behind the puffle and the official front layer is
    composited above it once when a room loads
  - includes the preserved puffle Backyard as a distinct, fully offline room;
    all eight original location-specific backgrounds follow the equipped
    igloo location, the real loader igloo/info icons remain visible, and the
    hidden green `pet_area` collision clip is excluded exactly as it was by
    the original `BackyardBackgroundView`
  - places every owned non-walking puffle at deterministic points inside the
    recovered official backyard safe-zone ellipse, including its equipped
    real room-hat layers; selecting one opens care for that exact color, while
    the active walk-with-me puffle continues to follow the penguin
  - streams the official furniture metadata catalog from disk and exposes all
    1,382 archived furniture SWFs as individually packaged, real-art items in
    Edit Igloo without growing the framebuffer allocation
  - separates furniture by the official metadata types: 1,033 room objects,
    247 wall objects, 101 floor objects, and one puffle-care object; every
    category previews the complete preserved item symbol before purchase
  - supports 16 simultaneous furniture placements, movement, storage, and a
    versioned save format that migrates the original four Blue items without
    losing their positions
  - uses official furniture prices and maximum quantities; purchases deduct
    local coins and persist in a sparse ID-keyed inventory, while legacy
    placed items gain matching ownership automatically
  - composes My Place from 94 visible preserved building interiors, 246
    preserved flooring frames, 94 official floor masks, and eight preserved
    locations without loading a Flash runtime or using replacement art
  - exposes the complete 96-row building catalog (including the two official
    no-art removal/invisible states), 24-row flooring catalog, and eight-row
    location catalog with their official prices, local ownership, equip state,
    and atomic persistence
  - opens the exact 11-page February 2012 Igloo Upgrades book from My Place;
    all 28 original building/floor purchase calls use the archived page art,
    official names and prices, persistent ownership, and live equip state
  - opens the exact 14-page April 2012 Better Igloos catalog from Edit Igloo;
    all 120 original furniture purchase variants—including the 45 Create Your
    Furniture combinations—use the shared sparse inventory and exact archived
    catalog presentation
  - restores the April clearance page from the source SWF's own embedded
    seven-item sprite at its original root transform and depth order; no item,
    label, price, book chrome, or close-button pixel is drawn or substituted
  - opens Sound Studio from the original Night Club `mixmaster_mc` hotspot and
    uses the preserved title, five instruction pages, 40-button music board,
    Save Song prompt, Saved Tracks screen, and four verified 40-clip album
    SWFs
  - reproduces synchronized loop columns and one-shot buttons with decoded
    22,050 Hz mono audio, records up to eight three-minute tracks entirely
    offline, and replays or deletes those tracks from Saved Tracks
  - starts genuinely new profiles in the preserved room-112 Welcome Solo and
    advances through three English key states from the official
    `newplayerexperience.swf`; established saves migrate as already complete,
    and the final original map prompt exits into the offline island map
- `apps/root_menu.c`
  - has a `Club Penguin` launcher under Games
  - includes the Club Penguin cover directory in game cover scan paths
- installed asset package:
  - `/.rockbox/rocks/games/clubpenguin/world.bmp`
  - `/.rockbox/rocks/games/clubpenguin/player.bmp`
  - `/.rockbox/rocks/games/clubpenguin/covers/ClubPenguin.bmp`
  - `/.rockbox/rocks/games/clubpenguin/rooms/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/tutorial/welcome_{0,1,2}.bmp`
  - `/.rockbox/rocks/games/clubpenguin/backyard/{1,2,3,4,5,6,7,8}.bmp`
  - `/.rockbox/rocks/games/clubpenguin/avatar/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/data/*.tsv`, including the complete
    furniture/layer archives and exact April 2012 furniture, February 2012
    igloo, Penguin Style, Martial Artworks, Costume Trunk, Snow and Sports,
    and puffle tables
  - `/.rockbox/rocks/games/clubpenguin/shop/sport/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/shop/costume/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/shop/penguin_style/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/shop/ninja/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/adopt/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/furniture/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/{food,walk,dig,eat}/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/care/{background,icons}.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/tricks/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/toys/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/toys/icons/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/hats/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/puffles/hats/room/*/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/igloo/items/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/igloo/{buildings,flooring,locations,masks}/*`
  - `/.rockbox/rocks/games/clubpenguin/igloo/catalog/{furniture,upgrades}/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/soundstudio/*.bmp`
  - `/.rockbox/rocks/games/clubpenguin/soundstudio/*.cpsa`
  - Sound Studio source manifest in the same directory
- source assets used so far:
  - `despedite/clubpenguinfreeroam`
  - `nhaar/Waddle-Forever` vanilla/legacy preserved room SWFs
  - `icerink.solero.me` mirror of the original Club Penguin global room,
    puffle, and backyard media trees plus the preserved backyard client loader
  - `project-aether` login atlas was inspected and used earlier for cover work

Current room coverage:

- My Place (igloo)
- Backyard (all eight igloo-location variants)
- Town
- Plaza
- Dock
- Ski Village
- Dojo
- Cove
- Beach
- Snow Forts
- Forest
- Mine Shack and Mine
- Iceberg
- Gift Shop, Coffee Shop, Pet Shop, and Pizza Parlor
- Night Club and Dance Lounge
- Book Room, Boiler Room, and Underground Pool
- Ski Lodge, Lodge Attic, Sport Shop, and Ski Hill
- Lighthouse and Beacon
- Stadium, Cave Mine, Dojo Courtyard, and Stage
- Hidden Lake and Recycling Plant
- Ninja Hideout, Fire Dojo, Water Dojo, and Snow Dojo
- Everyday Phoning Facility, EPF Command Room, and VR Room
- Migrator Deck, Crow's Nest, Ship Hold, and Captain's Quarters
- Underwater
- Box Dimension, entered only through a placed Portal Box (furniture item 529)
- Cloud Forest and Puffle Wild
- Puffle Hotel lobby, spa, and roof
- Mall, School, Puffle Park, and Skatepark
- Welcome Solo, first-login-only as in the original game

Known constraints:

- the current plugin is a substantial native slice, not yet a literal full
  recreation of every room, catalog, puffle care item/species, activity,
  party state, or original room script
- room scenes are static preserved frames; original SWF animation and room
  scripts do not run on-device
- walkability currently uses one rectangular floor zone per room
- persistent coins, apparel inventory, puffle care, and igloo furniture saves
  are implemented
- building, flooring, and location ownership and the currently equipped My
  Place layers are persistent; flooring is clipped by the exact hidden
  `floor_area`/`room_area` geometry recovered from each official SWF
- Edit Igloo can browse all 1,382 preserved furniture records without loading
  the catalog into BSS; non-selected placed items are composited into the room
  one at a time and only the selected 64x80 item remains resident
- the authentic April 2012 furniture and February 2012 upgrade books are the
  normal shop fronts; the complete building/floor/location and 1,382-item
  archives remain available behind them for preserved-art coverage beyond the
  two matching catalog releases
- purchased/equipped apparel is visible in every supported walking direction;
  Play removes the selected wardrobe layer and Select buys/equips it
- Snow and Sports ownership, equipped head/body/feet/hand layers, selected
  background, and furniture quantities persist in the same atomic local save
  and ID-keyed furniture inventory used by the other offline shops
- Costume Trunk ownership and equipped head/body/feet/neck/hand layers persist
  in that same save; its Detective Background uses the shared background slot
- Martial Artworks clothing uses the same sparse ID-keyed inventory and
  equipped head/body/hand slots; its four exact furniture calls share Edit
  Igloo inventory and quantity limits, and Dojo Igloo shares persistent
  building ownership and immediately becomes the active My Place building
- the Gift Shop opens the exact April 2012 Penguin Style catalog. Its 186
  official purchases use an ID-keyed local clothing inventory; head, face,
  neck, body, hand, and feet layers, player color, background, and flag all
  persist. My Place keeps the full wardrobe and Oliver Tree preset as a
  separate customization entry
- no Flash VM or Phaser runtime is used by the plugin
- the runtime intentionally has no network or server fallback; a missing local
  asset is a package error
- preserved SWFs are rendered and converted on the host; the iPod never
  downloads or executes them

Measured 58-room, 121-interaction, 1,382-furniture, 120-item April 2012 Better
Igloos, 28-item February 2012 Igloo Upgrades, 20-item Martial Artworks,
10-call February 2011 Adopt-a-Puffle, 27-item March 2010 Pet Furniture,
96-building, 24-puffle-toy, 68-puffle-hat, Sound Studio, Costume Trunk, and
April 2012 Penguin Style offline build:

- the iPod 6G plugin ELF uses 78,564 bytes of text and 259,300 bytes of BSS,
  remaining 2,844 bytes below the 256 KiB ceiling; the iPod Video ELF uses
  80,908 bytes of text and the same 259,300-byte BSS
- the final plugin SHA-256 values are
  `3ab96ff34cf62fb01209cd2d3c8f064038950733e171869ec07eb6e8a594d060`
  for iPod 6G and
  `bc561ee859da99c845e224031535dbd828b79e9604b117a29be995a0d3f84a99`
  for iPod Video
- the simulator plugin SHA-256 is
  `72d16c41006d10c5f77333402d3f26aab7ef84d983c50dc538bfb8a030b21017`
- the direct simulator gate entered the preserved Ninja Hideout, loaded the
  unobscured Martial Artworks cover and page 2, parsed Ninja Outfit as page 2,
  ID 4034, body type, 1,000 coins, and quantity one, then verified a 5,000 to
  4,000 coin purchase and body-slot equip. The isolated fixture was restored
  afterward to SHA-256
  `4032a51d95f65ccd4a2aa50f708e559b363e38edbc411cfdd971b4b1012fbae3`;
  the production simulator save remains
  `a0f544f044539e94a057e9c167054bfab0865a2b2cc342579dfe9e2d5f089217`
- the direct simulator adoption gate loaded exact pages 1, 3, and 7, parsed
  both calls on page 3 at 800 coins, verified a 5,000-to-4,200 Blue adoption
  with ownership, active color, and all four care stats initialized to 70, and
  verified page 7 changes Orange to Brown. The isolated and production save
  hashes above were unchanged after restoration
- the direct simulator Pet Furniture gate loaded page 2's five visible calls,
  revealed its exact Puffle Condo secret (ID 220, 280 coins), switched page
  3's official secret between Red and Gray Houses, and verified that buying
  Gray House deducted 500 coins and added one shared igloo-inventory copy. It
  also verified the corrected adoption-sign and yellow-book hotspot geometry;
  both fixture files were restored to their original hashes
- both final ZIPs contain 3,024 files in each native and iPodJS Club Penguin
  root. The iPod 6G ZIP SHA-256 is
  `d8e9ae4ee0d7d3a577c54dc11371474d63b1f2b891d2c3ca34ee345af1c9654c`;
  the iPod Video ZIP SHA-256 is
  `236c209d7f68d7cd76a1e694f12166f835860b12f013334c33f833e916bf98d3`.
  Their self-contained installer hashes are
  `7733cc81e8e28b88e5a961b555b8b892091536d56dfcb2300e33736ca2ffefce`
  and
  `82f1a02f0019952efbbab461511646d36ad0ac9631b6e0a693de1167b0770f3a`
  respectively; each embedded payload matches its build ZIP byte-for-byte
- avatar layers reuse the room framebuffer only while rebuilding the 468x38
  player strip; the room is reloaded immediately afterward
- the on-device package contains 15 color strips and 199 official wearable
  item strips, including 12 Snow and Sports, 14 Costume Trunk, 15 Martial
  Artworks, and 110 April 2012 Penguin Style timelines
- the Costume Trunk package uses the exact `Apr2012Costume.swf` paired with the
  packaged April 25 Stage revision. Its six untouched page renders, 15-item
  catalog, 14 source paper SWFs, and every generated strip are hash-bound in a
  local manifest; no costume pixel is drawn or substituted by the port
- Penguin Style uses the exact archived `PenguinStyleApr2012.swf`, all 16
  untouched page renders, all 186 purchase calls, preserved item metadata,
  and all 110 source paper SWFs. Winter Threads composites its two official
  depth layers, and the Blue Water Bottle uses its original eight-direction
  pre-193-frame timeline; no clothing pixel is drawn or substituted
- Martial Artworks uses the exact archived
  `ENCataloguesNinjaDecember2011.swf` (SHA-256
  `d68ce06f035f861753c89ea1f58ba468bbbf03536573dfb52d3fa942b22ceeaf`),
  all 15 official book frames, and all 20 original purchase calls. Its 15
  source paper SWFs supply every cardinal walk phase; Ninja Outfit preserves
  sprites 194/221 in official depth order and Hand Gong preserves legacy
  sprites 25/43/61/79 and frames 1/4/8. No page or wearable pixel is drawn,
  inferred, or substituted
- Adopt-a-Puffle uses the exact archived `Feb2011Adopt.swf` (SHA-256
  `ed09fd8ac297f7edd7440d67b2f830b8c09e9695c677586e4f2cef7b79d92aef`),
  all 11 official book frames, and all ten original `buyPuffle` calls. Its page
  art is never covered by a native adoption panel; page, selection, coin, and
  ownership status stays in the preserved toolbar below the book
- Pet Furniture uses the exact archived `Mar2010Pets.swf` (SHA-256
  `323eb53571492549a0cc7702c23b50d39223972857181ab8aade289e778f1e3d`),
  all five official book frames, and all 27 original `buyFurniture` calls.
  Pages 2 through 4 reveal the source sprites 179, 216, and 268 at their
  original root transforms; no secret-card pixel is redrawn or substituted
- the puffle package contains 14 archived food icons, 12 eight-frame
  directional walk atlases, 12 eight-frame dig atlases, and 11 eight-frame
  eat atlases sampled from the official 60-frame timelines; all retain magenta
  transparency and recorded source/generated hashes. The pinned archive has
  no Gold Puffle eat SWF, so Gold deliberately keeps its authentic idle art
  instead of borrowing or inventing an eating animation.
- the care scene uses the preserved 760x480 `CareBackgroundAsset` scaled for
  the iPod and a six-frame atlas made from the embedded Food, Play, Sleep,
  Care, Walk, and Dig symbols; returning from food reloads the icon atlas and
  returning to the room restores the complete player strip
- the puffle-toy package contains 24 official icons and 24 color-specific
  eight-frame animation atlases: one normal and one super toy for each of the
  12 puffles. Compatibility is selected only from archived `reaction=2`
  records; normal toys keep their exact -2/-2/+30/-5 effects and super toys
  keep -5/-5/+40/-10 plus their original 200-coin one-time ownership
- the puffle-hat package contains 68 authentic catalog thumbnails and 63
  authentic front care layers, plus 126 original room SWFs packaged as 504
  two-frame directional front/back layer atlases. It deliberately marks
  Snowflake Helmet, Heavy Metal Hair, Jolly Roger Bandanna, The Big Bang, and
  Candy Cane Cap as icon-only because no corresponding wearable SWF survived
  in the audited Solero/Waddle sources; purchasing those five is disabled
- the backyard package contains eight 320x220 BMP3 derivatives from the eight
  preserved `content/global/backyard/<location>_backyard.swf` files. Its
  manifest records all source SWF, clean root-frame, official loader/icon, and
  generated hashes; the runtime selects the saved location ID and never calls
  a server or CDN
- the preserved-room package adds the official Crow's Nest, Underwater, Box
  Dimension, and Welcome Solo 760x480 root scenes as 320x220 BMP3 files. Only
  `triggers_mc`/`block_mc` placements hidden by the rooms' own ActionScript
  are removed; its manifest records the four original SWFs, clean frames,
  removed tag IDs, canonical start/exit coordinates, and generated hashes
- Welcome Solo has no ordinary room trigger in its preserved SWF. Its package
  therefore uses the original one-time entry semantics, composites only
  official root/subframes 3/79, 5/169, and 7/126 from the preserved first-login
  interface, and persists `welcome_complete`; it does not invent a map marker
  or reusable doorway
- the Migrator ladder follows the original direct `shipnest` join, and the
  Hidden Lake graph now matches its official trigger geometry: Forest at the
  upper-left passage, Cave Mine at the lower-left passage, and Underwater at
  the right-hand Moss-Key door. The free-membership offline profile treats
  archived item 7016 as active, so the canonical gated door remains usable
  without a server or an unavailable Puffle Rescue session
- a placed official Portal Box (furniture item 529) becomes a live My Place
  hotspot at that exact furniture placement and joins Box Dimension at its
  recovered `(575,145)` start. The preserved room's own portal returns to the
  map from its original trigger region; no permanent island doorway is added
- the igloo package contains 1,382 non-blank 64x80 BMP3 item derivatives; all
  1,382 source SWFs and generated files pass their recorded SHA-256 checks
- the My Place layer package contains 94 visible 320x220 building interiors,
  94 compact one-bit masks, 246 320x220 flooring frames, and eight 320x220
  locations; the two catalog states with no visible building are preserved as
  commands instead of being replaced by invented art
- the igloo-catalog package is pinned to
  `media/default/archives/Apr2012Furniture.swf` and
  `media/default/archives/February2012Igloo.swf`. It contains 14 and 11
  authentic full-page renders, 120 and 28 exact purchase rows, and recorded
  source/generated hashes. The furniture catalog's two hidden `secret` clips
  follow their original initial ActionScript visibility, while page 13 uses
  embedded sprites 853, 821, and 823 at their official transforms
- Martial Artworks opens from the real red book at the lower-right of the
  preserved Ninja Hideout frame; no map marker, replacement door, or invented
  shop icon is added
- room interactions, owned-item masks, equipped slots, puffle state, coins,
  and igloo layout are persistent
- Sound Studio packages four authentic albums and 160 original clips in an
  indexed streaming format. The verified archive has no Spooky album SWF, so
  Spooky remains visible but unavailable instead of using replacement audio

Remaining preserved-room audit for literal 1:1 coverage:

- additional era-specific agency, party, event, and temporary rooms after
  their preserved assets and canonical graph positions are verified
- the complete original minigame, quest, catalog, puffle, igloo, and room
  scripting surface

This list is an explicit non-completion record: passing the current build and
package gates does not mean the full historical online game is 1:1 yet.

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
/.rockbox/rocks/games/clubpenguin/avatar/
/.rockbox/rocks/games/clubpenguin/avatar/source.manifest
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

Status: implemented for all 12 map markers and a 57-room graph, including the
original one-way Portal Box entry to Box Dimension.
Preserved exterior-room exit arrows link the navigable island chain and its
interiors, including the Mine complex, elemental dojos, EPF rooms, and
Migrator. Town enters the Coffee Shop and Gift Shop; Plaza enters the Pet Shop,
Pizza Parlor, and Stage. Every packaged room is reachable from a map target and
has a validated return path.

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

Status: implemented for the current native slice, including the fixed update
loop, 320x220 packaged scenes, streamed interactions, persistent save state,
and Cart Surfer foundation. Hardware profiling and room-specific walk masks
remain ongoing as room coverage expands.

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

The map scene draws a dedicated 20x22 selector derived from the preserved
penguin art. It is a separately packed thirteenth player-sheet cell, not the
full-size room sprite. Its feet are anchored to the exact transformed hotspot
coordinate. `LEFT`, `RIGHT`, and wheel movement select the nearest destination
in that direction; `SELECT` enters it. Holding physical `LEFT` and `RIGHT`
together opens the map from any room or Cart Surfer state, with a release latch
that prevents repeated transitions from one hold.

The preserved map sheet is 2713x1823. `WORLD_ROWS` stores coordinates in that
actual source space and the packager transforms them to 320x220. The earlier
854x480 assumption was incorrect and placed selectors away from their visible
buildings.

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
from the current position. On click-wheel iPods, held `MENU`, `PLAY`, `LEFT`,
and `RIGHT` are sampled directly each update so movement does not inherit
Rockbox's initial button-repeat delay. Wheel detents add short vertical
movement impulses to the same queue.

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
room\tid\tx\ty\tradius\taction\ttarget\tlabel\tto_x\tto_y
mine\tcart_surfer\t246\t127\t28\tminigame\tcart_surfer\tPlay Cart Surfer\t0\t0
town\tto_dock\t8\t170\t24\troom\tdock\tGo to the Dock\t272\t170
```

`to_x` and `to_y` are optional destination spawn coordinates. Exterior warps
use them so entering through a preserved arrow places the penguin beside the
corresponding arrow in the destination room. A zero pair retains the room's
default spawn.

For the current real-asset package, `INTERACTION_ROWS` in
`tools/clubpenguin_package_assets.py` is the source of truth. Warp hotspots
are aligned to the preserved blue arrow art already present in each room SWF
capture. The runtime does not draw synthetic crosshair markers over room
warps or the preserved Cart Surfer sign; map selection markers remain because
the scaled island map has no movable penguin cursor.

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

Status: implemented. Penguin Style owns five departments, persistent ownership
masks, and one equipped slot per department. Official preserved paper-doll
layers are composited into all four walking directions; purchasing never
substitutes generated clothing art. The requested Oliver Tree full-look preset
is included as one preserved-art combination.

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

Status: the first complete local shop loop is implemented. Town's preserved
Gift Shop door enters the archived Gift Shop room, its preserved catalog prop
opens Penguin Style, and five original-art catalog departments expose 63
canonical head, body, feet, color, and face/full-look choices. Buying subtracts
coins with an underflow check, persists ownership atomically, and selecting an
owned item equips it without charging again.

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

### 5.1 iPod Shop Controls

- wheel or `PLAY`: move through items
- `LEFT` / `RIGHT`: change catalog department
- `SELECT`: purchase or equip
- physical `MENU`: return to the Gift Shop room
- `LEFT` + `RIGHT`: open the island map through the global map chord

The 497x617 archived mobile catalog pages are host-scaled to 177x220 and
placed unchanged on the left side of a 320x220 shop scene. The right side is a
native status panel for current coins, item name, ownership, and controls.
Item pictures and printed prices remain the preserved catalog pixels.

The extended `CLUBPENGUIN_SAVE_V1` remains backward compatible and adds:

```text
owned_lo=00000000
owned_hi=00000200
equipped_head=-1
equipped_body=-1
equipped_feet=-1
equipped_color=5
```

An older save receives the default owned Light Blue color. New purchases are
written at the same stable atomic-save boundaries as Cart Surfer rewards.

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

Status: the expanded native slice includes a three-second countdown, animated
preserved track patches, six preserved cart/penguin poses, four lives, bounded
speed stages, curves, jump hazards, airborne and grind tricks, repeat-trick
penalties, combo tracking, crash recovery, pause/abandon, results, retry, best
score/combo persistence, and one-time coin rewards.

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

The fixed update clock also drives presentation animation. The packager crops
four consecutive 80x24 rail patches from preserved `DefineSprite 201` frames
and places them in the same bounded atlas as six 40x40 cart poses from
preserved `DefineSprite 173`. Runtime compositing cycles those patches faster
at each bounded speed stage. It performs no filesystem access, decode, scale,
or allocation during a run.

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
- `PLAY`: crouch/grind trick while playing; return from title/results
- physical `MENU` while grounded: pause; after the button is released, a
  second `MENU` abandons the run and returns to the Mine
- `SELECT` while paused: resume
- results wheel selection: choose retry or return to the Mine

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

### 7.1 Sound Studio Offline Slice

The preserved `soundroom.swf` is intentionally a white loader which calls
`SHELL.startSoundStudio()`. The visible application is supplied by the
preserved Music client, not by invented room art. The package therefore uses:

- `Music.swf` plus `music_game.swf`, `music_mysonglist.swf`, and
  `music_widget.swf`
- the untouched title, the official empty Saved Tracks root frame, and the
  populated `all_saved_music_1` frame 1/subframe 2 row state
- the music board with only its prompt, start-screen, and instruction overlay
  instances hidden, matching the client state after Make Music is selected
- instruction character 495 frames 2 through 6 composited over that board
- the fully visible Save Song state from prompt character 392, frame 2,
  subframe 7, cropped to the original 760x480 stage
- album SWFs `pop`, `rock`, `dance`, and `dubstep`, each containing 40
  original MP3 clips

The Night Club entry uses the original `mixmaster_mc` placement at Flash
coordinate `(347.5,175)`, scaled to `(146,80)`. Buttons remain column-major:
clips 0 through 24 are synchronized loops, with one active loop per column,
and clips 25 through 39 are one-shots.

`CPSA_V1` is an offline stream container. Its 16-byte little-endian header is
followed by 40 offset/sample-count pairs and signed 16-bit mono PCM at 22,050
Hz. Audio is decoded from the archived MP3 tags during host packaging; no
sound is synthesized. Only the selected album is acquired through the shared
plugin audio buffer. Playback uses the Rockbox PCM mixer, restores the prior
mixer frequency and fade/latency settings when leaving, and never mutates the
user playlist.

Offline recordings use `CPTR_V1`: a 48-byte header followed by timestamped
64-bit board masks. Up to eight three-minute tracks are written under the
installed Sound Studio directory. The preserved Save Song prompt supports
editable 31-byte names; those names are stored in the CPTR header and shown
during Saved Tracks playback. The runtime uses the preserved empty popup when
no CPTR files exist and the preserved populated rows otherwise, paging all
eight slots four at a time. Only dynamic name, likes, and sharing placements
are removed from the archived row before local names are rendered. Tracks can
be replayed or deleted through that screen. Online sharing, likes, and
server-backed Shared Tracks are deliberately absent. The archived Spooky
selector has no verified album SWF and remains unavailable without a fake
substitute.

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
  --source /tmp/clubpenguinfreeroam \
  --out build-sim-ipod6g/simdisk/.rockbox/rocks/games/clubpenguin
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
  --backyard-export /tmp/cp-backyard-export \
  --archive-room-export /tmp/cp-archive-room-package \
  --sound-studio-export /tmp/cp-soundstudio/export \
  --cart-export /tmp/cart2006-export \
  --ui-export /tmp/cp-ui-export/2010 \
  --puffle-export /tmp/cp-puffle-export \
  --puffle-food-export /tmp/cp-puffle-food-export \
  --puffle-motion-export /tmp/cp-puffle-motion-export \
  --puffle-care-export /tmp/cp-puffle-care-export \
  --puffle-trick-export /tmp/cp-puffle-trick-export \
  --puffle-play-export /tmp/cp-puffle-play-export \
  --puffle-hat-export /tmp/cp-puffle-hat-export \
  --puffle-adopt-export /tmp/cp-adopt-catalog-audit \
  --pet-furniture-export /tmp/cp-pet-catalog-audit \
  --furniture-export /tmp/cp-furniture-export \
  --igloo-export /tmp/cp-igloo-export \
  --avatar-export /tmp/cp-avatar-export \
  --sport-export /tmp/cp-sport-catalog-export \
  --sport-avatar-export /tmp/cp-sport-avatar-export \
  --out assets/ipodjs/rockbox/clubpenguin
```

`room-frames` contains one Ruffle-rendered PNG per room ID. The importer crops
the actual 760x480 game canvas out of the 968x777 Ruffle window capture, scales
it to a full 320x220 24-bit BMP, and records the source SWF path, pinned
repository commit, source checksum, and generated checksum in
`source.manifest`.

`backyard-export` contains the preserved `play/v2/client/backyard.swf`, all
eight `play/v2/content/global/backyard/<location>_backyard.swf` files, clean
JPEXS root-frame PNGs, and the loader's exported `IglooIcon` and
`BackyardInfoIcon`. Clean means only the root `pet_area` placement has been
removed before rendering: the original client explicitly sets that collision
clip invisible after load. The packager scales the untouched visible room art
and places the two real interface symbols at their recovered ActionScript
coordinates; it does not paint over or redraw the debug ellipse.

`archive-room-export` contains the original preserved `shipnest.swf`,
`underwater.swf`, and `boxdimension.swf` plus clean 760x480 JPEXS root frames.
Clean means only the
root collision/control placements that each room's own ActionScript sets
invisible were removed: tags 291/294 for Crow's Nest, 142/148 for Underwater,
and 29/232 for Box Dimension. The visible pixels are scaled directly to BMP3
with no painting, inpainting, or substitute art.
`rooms/preserved.manifest` records every source, frame, visibility decision,
route coordinate, and generated hash.

`puffle-motion-export/root_svg/walk` and
`puffle-hat-export/root_svg/room` contain JPEXS root-frame SVG exports made
with `-sublength 8` for Flash root frames 9, 11, 13, and 15. The matching 12
walk SWFs live under `puffle-motion-export/source/walk`; all 126 room-hat SWFs
live under `puffle-hat-export/source/room`. The importer renders the same fixed
registered viewport for both sources, so their original external-loader
coordinates remain aligned instead of being independently trimmed or redrawn.

Furniture packaging consumes the pinned legacy furniture sprite SWFs plus the
official `furniture_items.json` metadata. It exports each SWF's complete
highest-level `DefineSprite` rather than the clipped 100x100 root stage. The 34
ActionScript-attached items that require a different preserved wrapper/frame
use explicit entries recorded in `FURNITURE_SPRITE_FRAMES`; no blank frame,
clipped root capture, or substitute art is accepted. The runtime streams
`data/furniture.tsv` and loads `igloo/items/<id>.bmp` on demand.

Igloo layer packaging follows the preserved legacy `igloo.swf` behavior:
collision clips such as `room_area`, `wall_area`, `trash_area`, `block_mc`,
`pet_area`, `triggers_mc`, and waypoints are removed from visible building
exports, while `background_mc` is retained. The official building
`floor_frame` chooses the corresponding frame from each flooring SWF, and the
building's hidden floor geometry is packed as a one-bit mask. This reproduces
the original client composition order locally: location, visible building,
then masked floor.

Snow and Sports packaging uses the preserved `sport_cpip.swf` root pages and
the exact `buyItem`/`buyFurniture` calls recovered from its buttons. Apparel
is only treated as a walking layer when its archived clothing SWF contains the
original 193-frame paper-doll timeline; balls, boards, rods, backgrounds, and
furniture remain their canonical special-action categories instead of being
misrepresented by fabricated wearable art.

Puffle food packaging reads the preserved care-icon SWFs and archived
`puffle_items.json` directly. The runtime therefore applies the original
food/rest/play/clean deltas instead of invented generic bonuses. Walk atlases
use root frames 9, 11, 13, and 15 from the original color SWFs for down, left,
up, and right; subframes 1 and 7 retain the source animation and the original
right-facing art rather than mirroring another direction. Their 20x20 external
loader stage is not used as a crop: JPEXS root-frame SVG transforms are
rendered through one fixed registered viewport shared with room hats. Dig
atlases sample eight frames across each color's complete preserved treasure
timeline. No idle sprite is presented as walking or digging art.

Puffle trick packaging consumes the 72 original standalone SWFs preserved by
the Solero Ice Rink mirror of `media1.clubpenguin.com`: six official commands
for Blue, Red, Pink, Black, Green, Purple, Yellow, White, Orange, Brown,
Rainbow, and Gold. JPEXS exports each locally bounded top-level timeline and
the importer samples eight evenly spaced source frames into a 400x50 atlas.
The runtime streams only the active color/trick atlas into the existing shared
player/minigame buffer, so the complete offline set adds no full-screen cache
or replacement animation.

Puffle toy packaging reads the archived care table in its original reaction
order and retains only reaction `2` (`likes`) for the active color. This yields
exactly one normal and one super toy for each of the 12 colors. The official
Waddle client identifies normal play as root frame 27 and super play as root
frame 28 in `RoomPuffle.as`; JPEXS exports the distinct color-specific sprite
timeline from those frames. Eight evenly spaced preserved frames are trimmed
and registered into each 400x50 device atlas, while the 24 original care-icon
SWFs become separate 40x40 selectors. The runtime streams the animation into
the existing player/minigame buffer and the icon into the existing hat buffer,
so no substitute drawing or additional animation framebuffer is introduced.

Puffle-hat packaging joins the official `type=head` metadata with the original
care-catalog icon SWFs and care-hat SWFs. JPEXS frame-SVG exports are rendered
around their external-loader registration point so negative vector coordinates
are not clipped; the device atlas then stores the complete authentic icon and
the authentic care `HatFront` layer. The 63 archived room wearables add one
official `_hat_back.swf` and `_hat_front.swf` apiece. Their eight-direction
walk timelines are mapped to the puffle's down/left/up/right root frames, with
subframes 1 and 7 retained in each layer. The runtime reuses the existing
80x40 care-hat buffer to stream one direction/layer at a time: back pixels fill
only transparent puffle pixels, then front pixels overwrite them. The finished
eight-frame puffle atlas remains in the existing puffle buffer, so correct
Flash z-order adds no BSS. `puffles/hats/source.manifest` records every
source/generated hash and the five verified missing wearable SWFs. Missing
wearables retain their real catalog icon but have no fabricated layer and no
purchase path.

The bottom 320x20 toolbar is real preserved interface art extracted from the
pinned 2010 interface SWF. It replaces Rockbox instruction text in the map,
rooms, and Cart Surfer. Unsupported chat and social buttons remain visual-only
until their offline actions are implemented.

The Game Cover Flow Flash system includes Club Penguin as a native plugin
entry alongside preserved SWF titles. Selecting it launches
`clubpenguin.rock` directly. Its 120x140 cover combines only the preserved
island-map source and archived official Club Penguin logo; both source paths
and checksums are recorded in `source.manifest`.

## Controls

iPod 5G/6G baseline:

- `MENU`: return to the previous screen; from a room, return to the island map
- `PLAY`: move down; in Cart Surfer, release to perform a grind trick
- `LEFT` / `RIGHT`: move horizontally
- `MENU` + `SELECT`: save and exit the plugin
- `SELECT`: interact / enter room / confirm
- on the map, directional buttons and wheel select destinations
- in rooms, wheel rotation is an additional vertical movement control
- in Puffle Care, the wheel chooses Food, Play, Sleep, Bath, Walk, or Dig;
  the food browser uses the wheel for food and `LEFT`/`RIGHT` for the active
  owned puffle; `SELECT` on the Play row opens Puffle Tricks, where the wheel
  chooses one of the six preserved commands, `LEFT`/`RIGHT` changes owned
  puffle color, and `SELECT` performs that color's original timeline. The
  physical `PLAY`
  button opens Puffle Hats; the wheel browses all 68 records,
  `LEFT`/`RIGHT` changes the active puffle, `SELECT` buys/equips, and `PLAY`
  removes the equipped hat without deleting ownership
- in Adopt-a-Puffle, `LEFT`/`RIGHT` turns all 11 original February 2011
  pages, the wheel selects only the two original calls on pages 3 through 7,
  and `SELECT` adopts or activates the selected puffle for the exact 800-coin
  price
- in Pet Furniture, `LEFT`/`RIGHT` turns all five March 2010 pages, the wheel
  selects only calls visible in the current authentic state, `SELECT` buys
  into the shared igloo inventory, and `PLAY` reveals or closes the source
  secret overlay on pages 2 through 4
- in Edit Igloo, `LEFT`/`RIGHT` changes official furniture type and the wheel
  browses while an item is stored; arrows move it while placed, `SELECT`
  places/stores (and can buy an unowned archive item), and `PLAY` opens the
  authentic April 2012 Better Igloos book. While an object is placed, `PLAY`
  retains buy/add-another-copy behavior
- in Better Igloos, `LEFT`/`RIGHT` turns all 14 authentic pages, the wheel
  selects exact page-specific purchases, `SELECT` buys, and `PLAY` returns to
  Edit Igloo
- in the authentic February 2012 Igloo Upgrades book, `LEFT`/`RIGHT` turns all
  11 pages, the wheel selects the exact building/floor purchase calls,
  `SELECT` buys/equips, and `PLAY` opens the complete preserved layer archive
- in the complete layer archive, `LEFT`/`RIGHT` changes building, floor, or
  location tab; wheel/arrows browse and `SELECT`/`PLAY` buys or equips the
  current layer. `MENU` returns to the authentic February 2012 book
- in Snow and Sports, `LEFT`/`RIGHT` turns the nine original catalog pages,
  the wheel selects purchases on that page, `SELECT` buys/equips, and `PLAY`
  removes the selected wearable or background
- in the Stage Costume Trunk, the same controls turn all six original April
  2012 pages, browse the exact page-specific purchase calls, buy/equip the
  selected item, and remove its wearable or background layer
- in Martial Artworks, `LEFT`/`RIGHT` turns all 15 December 2011 pages, the
  wheel selects only the original calls on the visible page, and `SELECT`
  buys/equips clothing, purchases the exact furniture quantity, or activates
  Dojo Igloo; `PLAY` removes a selected equipped clothing layer

All four apparel books draw their official 320x220 page derivative without a
runtime panel or paper-doll preview over it. Page/item/coin status is confined
to the preserved 20-pixel toolbar below the catalog image.

The quit chord is checked from the raw button state as well as the action map
so either press order is reliable. While the chord is held, the standalone
`MENU` action is suppressed so quitting cannot trigger an unwanted room/map
transition.

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
- the Stage trunk opens the exact April 2012 Costume Trunk and returns to the
  Stage without changing the saved room position

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

Rooms, Puffles, and Igloos:

- all 58 entries in `rooms.tsv` have preserved 320x220 room art and either a
  canonical route back to the island map or Welcome Solo's original one-time
  tutorial completion
- building doors link Town, Plaza, Ski Village, Beach, Snow Forts, Mine,
  Dojo, and their canonical interiors in both directions
- the Pet Shop opens all 11 authentic February 2011 Adopt-a-Puffle pages; the
  ten exact page-specific calls adopt or activate every preserved standard
  color for 800 local coins each
- the preserved upper-left Adopt-a-Puffle sign owns the adoption hotspot; the
  separate real yellow book at lower-right opens all five March 2010 Pet
  Furniture pages, all 27 calls, and all three official secret overlays
- Rainbow and Gold use preserved-art quest rewards at the Hotel Roof and Cave
  Mine rather than appearing incorrectly in the Pet Shop catalog
- each puffle's food, rest, happiness, cleanliness, ownership, active
  selection, and walk-with-me state survive restart independently
- the active puffle appears in My Place; when walking is enabled it follows the
  penguin in every room with authentic down/left/up/right art
- My Place and Backyard are linked in both directions. Backyard uses the
  selected igloo location's authentic scene, shows every owned puffle inside
  the original safe zone, retains real equipped room hats, and opens care for
  the selected puffle; all of this runs from installed local files
- the Migrator ladder links to the preserved Crow's Nest in both directions;
  Hidden Lake's three official trigger regions link to Forest, Cave Mine, and
  the preserved Underwater room, with item 7016 active for the serverless
  free-membership profile
- placing the authentic Portal Box furniture item creates the only entry to
  Box Dimension; its original room portal exits to the map
- the food browser uses 14 preserved icons and exact metadata effects; free
  Puffle O's, four-coin Apples, and twelve dig-only rare foods keep their
  archived availability semantics and saved quantities
- feeding plays the active color's preserved eating timeline for every color
  present in the pinned archive; no cross-color or hand-drawn fallback is used
- Play exposes the compatible official normal and super toy for every puffle
  color, with exact metadata effects, a one-time 200-coin super-toy purchase,
  saved ownership, and its own preserved color-specific animation; the toy
  screen exposes all six preserved trick timelines for every color
- Sleep and Bath apply the archived care effects, while Dig plays the active
  color's preserved treasure timeline and awards a saved rare food
- Puffle Hats exposes all 68 official head records; 63 preserved wearables can
  be bought, equipped, removed, and reloaded independently for all 12 puffles.
  The five records without an archived wearable retain their authentic icon,
  clearly report the missing SWF, and cannot deduct coins
- every available equipped puffle hat appears while walking in the official
  down/left/up/right art for both retained subframes; `_hat_back.swf` pixels
  remain behind the puffle and `_hat_front.swf` pixels remain in front
- Edit Igloo browses all 1,382 preserved furniture items, supports 16 placed
  objects, and saves each item ID and position; the legacy four-Blue-item save
  format migrates in place
- official prices, quantity limits, purchases, duplicate ownership, and
  duplicate placements are live; inventory bytes are keyed by stable item ID
  rather than catalog row order
- the April 2012 Better Igloos book displays all 14 authentic pages, including
  the exact embedded-art clearance page, and all 120 page-specific variants
  purchase into the same ID-keyed inventory used by Edit Igloo
- the February 2012 Igloo Upgrades book displays all 11 authentic pages; all
  28 building/floor calls buy, persist, and immediately set the active layer,
  including the official 20-coin floor-removal service
- the Ninja Hideout's existing red book opens all 15 authentic December 2011
  Martial Artworks pages; all 15 clothing, four furniture, and one building
  call buy and persist in the shared ID-keyed inventories, with all 15
  wearables visible in every cardinal walking direction
- the iPod 6G plugin BSS remains at or below 256 KiB
- the hardware plugin and source contain no USB Internet or multiplayer
  symbols, and the simulator starts with networking unavailable
- every recorded generated asset hash in `source.manifest` matches the
  installed file
- all 3,023 files recorded by the root manifest are present and hash-correct;
  each native and iPodJS package root contains the same 3,024 files including
  the manifest itself

Sound Studio:

- the original Night Club mixer hotspot opens the preserved title screen
- Instructions shows all five preserved instruction frames; Saved Tracks
  switches between the preserved empty popup and populated rows and pages all
  eight local slots; Make Music uses the preserved 40-button board
- Pop, Rock, Dance, and Dubstep load all 40 original clips, while Spooky
  clearly remains unavailable because no verified album SWF exists
- loop columns remain synchronized, one-shots interrupt prior one-shots, and
  switching albums does not retain audio from the prior album
- recording writes a valid `CPTR_V1` event stream, stops automatically after
  three minutes, opens the preserved naming prompt, and survives restart;
  custom names round-trip through Saved Tracks, which replays and deletes it
- entering and leaving restores mixer settings, releases the shared audio
  buffer, and neither stops nor replaces the user's playlist

Hardware:

- build `bin`
- deploy firmware to both required locations
- verify firmware checksums
- deploy `.rock` and assets
- verify plugin/asset checksums
- `sync`

## Remaining Implementation Order

Preserved Waddle Forever assets remain canonical where available. Continue in
small, buildable tranches:

1. Audit preserved agency, party, event, and temporary rooms by game era and
   add only those with verified source provenance and graph placement.
2. Recover and wire additional era-matched authentic catalogs beyond the
   completed February 2011 Adopt-a-Puffle, March 2010 Pet Furniture, April
   2012 Better Igloos, February 2012 Igloo Upgrades, and December 2011 Martial
   Artworks books, retaining exact purchase calls and archived presentation.
3. Add preserved-art native implementations of the remaining minigames and
   quests without adding server dependencies.
4. Reproduce important room animations and offline interaction scripts from
   preserved timelines within the iPod memory and frame-time budgets.
5. Repeat graph, manifest, hardware BSS, simulator startup, package integrity,
   and physical-device stability gates after every tranche.
