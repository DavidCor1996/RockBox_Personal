# RunePod Game Plugin Spec

Updated: 2026-06-27

## Goal

Build `runepod`, an original small-scale fantasy life-and-combat RPG for the
iPod Classic/6G Rockbox target. The game should evoke the readable, low-poly,
tile-based feeling of older browser/MMO RPGs without using copyrighted
RuneScape assets, names, maps, quests, UI, music, models, or data.

The design target is not "desktop MMO on an iPod." It is a click-wheel-native
single-player game with compact zones, skills, resource gathering, light
questing, simple combat, and an interaction model that feels like it belongs on
stock iPod OS.

## Target Hardware

Primary target:

- Device: Apple iPod Classic/6G (`IPOD_6G`)
- Screen: 320x240 RGB565 color LCD
- Input: `BUTTON_SELECT`, `BUTTON_MENU`, `BUTTON_LEFT`, `BUTTON_RIGHT`,
  `BUTTON_PLAY`, `BUTTON_SCROLL_FWD`, `BUTTON_SCROLL_BACK`
- Plugin memory: 3 MiB (`PLUGIN_BUFFER_SIZE 0x300000`)
- Storage: disk-backed, so runtime asset loads should be batched and cached

Initial build gate:

- Compile only for color LCD targets at 320px width or wider.
- Optimize and validate for iPod 6G first. Wider portability can come later
  after the control model and memory budget are proven.

## Design Pillars

- Click-wheel first: every major action must work through wheel scrolling,
  center select, menu back, and left/right paging.
- Small world, dense interactions: a handful of handmade zones beat a large,
  empty map.
- Readable 2.5D presentation: top-down/isometric-style tiles and sprites,
  not real-time 3D.
- Skill loops over grind volume: mining, woodcutting, fishing, cooking,
  crafting, and combat should each be short, legible loops.
- Original art with consistent generated style: generated assets should share
  camera angle, palette, outlines, lighting, and scale.
- Simulator-first iteration: controls, frame pacing, map streaming, and asset
  memory must be verified before real-device deployment.

## Product Shape

RunePod is a single-player, offline RPG built around a village hub and nearby
resource/combat areas.

Core loop:

1. Choose an activity from the hub or nearby wilderness.
2. Walk to an object, NPC, enemy, or resource node.
3. Select a contextual action.
4. Gain XP, items, coins, or quest progress.
5. Return to a shop, bank chest, workbench, or quest NPC.

MVP content should include:

- One village hub.
- One forest/resource zone.
- One mine/resource zone.
- One small combat cave.
- Three NPCs: guide, shopkeeper, quest giver.
- Six skills: combat, mining, woodcutting, fishing, cooking, crafting.
- One starter quest that touches at least three skills.
- One simple equipment track: tool tier 1, weapon tier 1, armor tier 1.

## Controls

The controls should feel like iPod OS list navigation adapted to a game world,
not like a desktop cursor awkwardly mapped to a wheel.

### World Controls

- Wheel: move the world cursor vertically through the visible play area.
- Wheel repeat/fast spin: move the world cursor vertically faster.
- `LEFT` / `RIGHT`: move the world cursor horizontally.
- `LEFT` / `RIGHT` repeat: move the world cursor horizontally faster.
- `SELECT`: if the cursor is over an interactable, walk to it and perform its
  default action; otherwise walk to the cursor position.
- `SELECT` hold: if the cursor is over an interactable, open its action menu;
  otherwise walk to the cursor position.
- `MENU`: exit RunePod from world view.
- `PLAY`: open inventory/status. Early MVP uses this as the native-feeling
  quick status button; later builds may add a tab inside status for repeat
  skilling or run/walk.

### Menu Controls

- Wheel: scroll list.
- `SELECT`: choose row.
- `MENU`: close the current menu/status screen back to world.
- `LEFT` / `RIGHT`: switch tabs.
- `PLAY`: no close behavior inside menu/status screens; ignore releases so a
  normal Play press from world cannot immediately close the status UI.

Inventory/status tabs:

- Inventory opens first from `PLAY`.
- `SELECT` or `RIGHT` switches inventory to Levels.
- Levels shows combat, mining, woodcutting, fishing, cooking, and crafting as
  compact rows with level number and current XP progress toward the next level.
- `SELECT` or `RIGHT` switches Levels to Map.
- Map shows the expanded village area, the current viewport, the player, the
  cursor, and interactable targets.
- `SELECT` or `RIGHT` switches Map to Inventory.
- `LEFT` moves backward through Map, Levels, and Inventory.
- `MENU` closes any status tab back to world.

### Why This Model

Old-school RuneScape is point-and-click, but an iPod click wheel is strongest
at controlled incremental movement and list scrolling. RunePod should preserve
the "point at a thing, choose an action, watch the avatar do it" rhythm while
replacing the mouse pointer with a native world cursor and context list. The
world must not feel like a menu of targets; target lists belong only inside
menus.

## Interaction Model

Every interactable exposes a sorted action list:

- NPC: Talk, Trade, Quest, Examine.
- Resource node: Gather, Prospect/Inspect, Examine.
- Enemy: Attack, Examine.
- Ground anchor: Walk here.
- Item on ground: Take, Examine.
- Exit: Enter/Leave.

Cursor-first world model:

- The cursor is an explicit screen-space focus marker clamped inside the world
  bounds and rendered through the current camera.
- The cursor selects an interactable only when it is within the configured
  cursor radius; otherwise the selected action is `Walk here`.
- The player avatar walks to cursor destinations or interactable anchors.
- The selected target ring is a cursor affordance, not a list index.
- Menus may still use wheel-scrolled rows; world play should not require
  cycling through every visible object.
- The world may be larger than the screen. The camera follows the cursor and
  clamps to map bounds.

Default action rules:

- If the target has an active quest step, default to the quest action.
- If the target is a resource and the player has the required tool, default to
  Gather.
- If the target is hostile, default to Attack.
- If no higher-priority action exists, default to Walk here or Talk.

Focus sorting:

- Prefer visible targets within a 5-tile radius.
- Keep the previous target selected if it remains valid.
- Sort by category, then screen-space angle from the avatar, then distance.
- Wheel acceleration may skip within the same category before moving to the
  next category.

## Visual Direction

RunePod should use original generated assets with a deliberately consistent
"small old 3D fantasy game rendered into sprites" look:

- Low-poly-like silhouettes rendered as 2D sprites.
- Slightly chunky proportions for readability at 320x240.
- Three-quarter top-down camera, fixed across all characters and props.
- Hand-painted texture feel, limited palette, visible edges, no photorealism.
- Warm daylight for the village, cooler cave palette, muted forest greens.
- UI should mimic stock iPod OS restraint: simple panels, clean lists,
  highlighted rows, compact icons, and no dense MMO HUD.

Do not use actual RuneScape visual assets, names, icons, models, sounds, map
layouts, skill icons, NPC names, or item art. Asset prompts and filenames should
describe generic fantasy objects, not proprietary references.

## Rendering Strategy

Use a 2.5D tile/sprite engine:

- Logical map: square grid, 16x16 logical tiles.
- Display style: diamond-ish or angled top-down illusion through sprite art and
  painter's-order sorting, not real 3D projection.
- Camera: centered on player, clamped to map bounds.
- Draw order: ground tiles, decals, low props, actors sorted by `y`, overlays,
  UI.
- Frame target: stable 30 fps on simulator and device; animation can run at
  10-15 fps.
- Coordinates: fixed-point integer world positions, no floats in the hot path.

Recommended viewport layout:

- Full screen world view.
- Top 18px status strip for HP, coins, selected skill/action, or zone name.
- Bottom 24-34px contextual action strip only when target/action changes.
- Menus are full-screen iPod-like lists instead of nested overlay cards.

## Asset Pipeline

Source assets should be generated or authored into a normalized pack before
being converted to Rockbox-friendly BMP/header data.

Recommended source layout:

```text
assets/runepod/source/
  prompts/
  sprites/
  tiles/
  ui/
  audio/
```

Runtime/install layout:

```text
.rockbox/rocks/games/runepod/
  pack.json
  maps/
    village.rmap
    forest.rmap
    mine.rmap
    cave.rmap
  sprites/
    actor_player.bmp
    npc_guide.bmp
    npc_shopkeeper.bmp
    enemy_ratling.bmp
  tiles/
    village_tiles.bmp
    forest_tiles.bmp
    mine_tiles.bmp
    cave_tiles.bmp
  ui/
    icons.bmp
  audio/
    click.wav
    gather.wav
    hit.wav
    level.wav
  save/
    runepod.sav
```

Compiled fallback assets:

- Include a tiny built-in pack so the plugin boots without external files.
- Fallback pack can be one tilesheet, one player sprite, one NPC, one enemy,
  and generated UI icons.

Asset budgets for MVP:

- Tilesheets: four sheets, each up to 256x256 RGB565-equivalent source.
- Actor sprites: 24x32 or 32x32 frames, 4 directions, 2-4 walk frames.
- Props/resources: 16x16 to 32x32 sprites, usually static.
- UI icons: 12x12 or 16x16.
- Audio: optional short mono WAV clips for effects. User-provided music must not
  be streamed by creating or replacing a Rockbox playlist from the plugin.

Generated asset style contract:

- Same camera angle for all sprites.
- Transparent key color reserved and documented.
- Actor feet align to a common origin.
- Each sheet has a generated manifest with frame rectangles and logical anchor.
- Prompts, seed/reference notes, and post-processing settings are stored beside
  the generated sources for reproducibility.
- Source sheets use `#ff00ff` as the chroma key unless the manifest explicitly
  records a different key.
- Runtime BMP sheets must use exact `#ff00ff` transparent pixels after crop and
  downscale. Any fuzzy generated magenta edge must be removed before packing so
  sprites do not show purple boxes or halos in Rockbox.
- Runtime drawing must pass `STRIDE(SCREEN_MAIN, sheet_width, sheet_height)` to
  `lcd_bitmap*_part()` so the same sheet works on horizontal and vertical
  framebuffer targets.
- Generated source images are not runtime assets until they have been cropped,
  downscaled, keyed, packed, and screenshot-tested in the iPod 6G simulator.

Generated source asset v1:

- Source: `assets/runepod/source/reference/runepod_style_sheet_v1.png`
- Runtime sheet: `assets/runepod/runtime/sprites/runepod_sprites.320x64x24.bmp`
- Directional/player source:
  `assets/runepod/source/reference/runepod_directional_terrain_sheet_v1.png`
- Directional player sheet:
  `assets/runepod/runtime/sprites/runepod_player_dirs.128x32x24.bmp`
- Terrain tile sheet:
  `assets/runepod/runtime/tiles/runepod_terrain_tiles.256x32x24.bmp`
- Terrain v2 source:
  `assets/runepod/source/reference/runepod_terrain_tiles_v2.png`
- Prompt: `assets/runepod/source/prompts/sprite_style_sheet_v1.prompt.txt`
- Directional/terrain prompt:
  `assets/runepod/source/prompts/directional_terrain_sheet_v1.prompt.txt`
- Terrain v2 prompt:
  `assets/runepod/source/prompts/terrain_tiles_v2.prompt.txt`
- Manifest: `assets/runepod/source/manifest_v1.json`
- Status: first runtime sprite sheet extracted and loaded by the plugin. The
  sheet is 320x64 BMP3 with 32x32 cells and exact `#ff00ff` transparency. It
  is a first-pass static sheet; directional animation frames and richer tile
  sheets remain later asset-pipeline work.
- Terrain v2 status: `runepod_terrain_tiles.256x32x24.bmp` is regenerated from
  `runepod_terrain_tiles_v2.png` with a roof-edge/foundation village tile. The
  runtime terrain SHA-256 is
  `e001b1b97cf9405c0e25966a8a081eac88335e26496dc926bb4cf8e064d22316`.

Runtime music v1:

- Disabled. RunePod must not create a Rockbox playlist, start bundled music, or
  leave a resume playlist behind. Plugin-owned background music previously used
  `Harmony.mp3`, but that asset is intentionally removed because it could become
  the global current playlist and resume when Play was pressed after leaving the
  plugin.
- Provenance: previous user-provided track removed from the runtime asset set.

Expanded map v1:

- Runtime world size: 640x420.
- Screen viewport: 320x188 between the top status strip and bottom action
  strip.
- The camera follows the cursor, not a hidden target list.
- The village includes generated terrain tiles for grass, dark grass, path,
  wood, stone, water, village roof/foundation, and cave ground.
- Village tile placement uses distinct building footprints with wood interiors
  and stone plaza/path tiles; it must not repeat a full house tile into a pile
  of houses.
- Player facing uses generated south/east/north/west 32x32 sprites and updates
  toward the dominant walking axis.

Animation v1:

- Do not spend memory on extra frames until the extraction pipeline is stable.
- Use low-cost procedural animation on top of the first static sheet:
  - player bob while walking,
  - selected NPC/enemy idle bob,
  - fire flicker overlay,
  - pond shimmer lines,
  - selected target pulse,
  - chopping axe swing with leaf chips,
  - mining pick swing with impact sparks,
  - fishing rod/line/bobber with water ripple,
  - cooking flame/smoke burst,
  - crafting hammer swing with bench sparks,
  - combat strike/slash with enemy hit flash,
  - eating/healing marker near the player.
- Animation must be subtle at 320x240 and must not move actor foot anchors more
  than one pixel unless a real walk cycle is available.
- Animation code must keep the primitive fallback path working when the runtime
  BMP is missing.
- Successful actions resolve immediately, then play a short timed animation
  while the bottom action strip reports the reward. Movement clears any active
  action animation.

## Data Formats

Keep runtime parsing small. Prefer compact binary formats generated by host
tools, plus JSON only for authoring metadata.

Suggested runtime map format (`.rmap`):

- Header: magic, version, width, height, tile size, layer counts.
- Tile layers: ground, overlay, collision flags.
- Object table: type id, x, y, script id, respawn/radius fields.
- NPC table: npc id, x, y, facing, dialogue id.
- Spawn table: enemy id, region rectangle, max count.

Suggested save file:

- Header with version and checksum.
- Player position and zone.
- Inventory slots.
- Equipment slots.
- Skill XP.
- Quest flags.
- Object/resource cooldown state.

Do not rely on text JSON parsing in the plugin for hot-path gameplay. Host
tools can emit generated C headers for default data and `.rmap`/`.sav` files
for device data.

## Gameplay Systems

### Movement

- Tap `SELECT` on a highlighted destination to pathfind there.
- Small A* over the current loaded map only.
- Cancel path by selecting a new target or pressing `MENU`.
- Use short path lengths and simple collision; if blocked, walk as far as
  possible and show a brief status message.

### Skills

Each skill should be a deterministic timed interaction:

- Mining: select rock, play timed swing loop, chance to receive ore.
- Woodcutting: select tree, timed chop loop, receive logs, tree cooldown.
- Fishing: select fishing spot, timed wait, receive fish.
- Cooking: select fire/range, consume raw food, chance to cook or burn.
- Crafting: select workbench/anvil, choose recipe list, consume inputs.
- Combat: select enemy, avatar closes distance, attack ticks resolve.

XP should use small integer tables with visible level-up feedback. Keep the
MVP level cap low, such as level 10, so balancing is manageable.

### Combat

Combat should be readable, slow, and menu-light:

- Select enemy to attack.
- Player and enemy exchange attacks on fixed ticks.
- `SELECT` hold on enemy opens combat actions: Attack, Use Food, Flee, Examine.
- `PLAY` can eat the first available food during combat.
- Enemy AI is simple: idle, approach, attack, return to spawn.

MVP enemies:

- Ratling: melee, low HP.
- Cave imp: melee, slightly higher hit chance.
- Moss guard: slow, high HP, used as optional stronger target.

### Inventory And Economy

- Inventory: 20 slots, stackable resources, non-stackable tools/equipment.
- Bank chest in village: larger fixed storage table.
- Shopkeeper: buy/sell tier-1 tools, food, and basic gear.
- Coins: single integer currency.

### Questing

Use a tiny quest flag system, not a full scripting VM in MVP.

Starter quest example:

1. Talk to guide.
2. Chop 3 logs.
3. Mine 2 copper ore.
4. Cook 1 fish.
5. Return to guide for coins, XP, and access to cave.

Dialogue should be short and list-driven, with each screen fitting the iPod
display comfortably.

## Technical Architecture

Planned file split:

```text
apps/plugins/runepod.c
apps/plugins/runepod/
  rp_assets.c / rp_assets.h
  rp_audio.c / rp_audio.h
  rp_combat.c / rp_combat.h
  rp_data.c / rp_data.h
  rp_input.c / rp_input.h
  rp_inventory.c / rp_inventory.h
  rp_map.c / rp_map.h
  rp_path.c / rp_path.h
  rp_render.c / rp_render.h
  rp_save.c / rp_save.h
  rp_skill.c / rp_skill.h
  rp_ui.c / rp_ui.h
```

Build integration:

- Add `runepod.c` to `apps/plugins/SOURCES` behind
  `HAVE_LCD_COLOR && LCD_WIDTH >= 320`.
- Add `runepod,games` to `apps/plugins/CATEGORIES`.
- Add generated bitmap headers under `apps/plugins/bitmaps/native/` only for
  fallback assets.
- External generated asset packs live under `PLUGIN_GAMES_DIR "/runepod"`.
- Disk-backed plugin music is disabled. If background audio returns later, it
  must use plugin-local PCM/mixer output and must not mutate the user's playlist
  or Rockbox resume state.

Runtime states:

- Boot/load assets.
- Title/continue screen.
- World.
- Action menu.
- Inventory.
- Skills.
- Quest/dialogue.
- Shop/bank.
- Pause/options.

## Memory Budget

Target a conservative runtime footprint inside the 3 MiB plugin buffer:

- Code and static data: <= 700 KiB.
- Frame/background buffers: avoid full duplicate 320x240 buffers unless a
  measured renderer needs one.
- Active tilesheets/sprites: <= 900 KiB.
- Map/object/NPC data: <= 300 KiB.
- Pathfinding scratch, UI strings, inventory, save staging: <= 200 KiB.
- Audio clips/cache: <= 200 KiB for MVP.
- Background music: disabled unless implemented later through plugin-local
  PCM/mixer output that does not mutate Rockbox playlist or resume state.
- Free headroom: >= 500 KiB.

Implementation rules:

- Load one zone's active asset set at a time.
- Cache only neighboring zone metadata, not all zone art.
- Prefer sheets over many small files to reduce disk churn.
- Keep decoded images in plugin-owned buffers with explicit sizes.
- Avoid malloc-heavy designs; use fixed arrays and visible limits.

## Current Implementation Status

Status as of 2026-06-27:

- `apps/plugins/runepod.c` exists as a single-file prototype.
- `apps/plugins/SOURCES` builds `runepod.c` for
  `HAVE_LCD_COLOR && LCD_WIDTH >= 320`.
- `apps/plugins/CATEGORIES` registers `runepod,games`.
- The first scene is a procedural/placeholder village with:
  - title screen,
  - world view,
  - click-wheel vertical cursor movement,
  - left/right horizontal cursor movement,
  - camera follows the cursor over the expanded map,
  - `SELECT` default actions,
  - `SELECT` hold action menu,
  - `PLAY` inventory screen,
  - dialogue/examine panel,
  - basic movement-to-target,
  - resource interactions,
  - starter quest flags,
  - shop food purchase,
  - simple ratling combat,
  - XP and inventory counters.
- Native iPod 6G plugin build passed and produced a valid Rockbox plugin:
  `build-hw-ipod6g/apps/plugins/runepod.rock`.
- A hardware-ready copy is staged at:
  `.rockbox/rocks/games/runepod.rock`.
- The simulator build passed and `runepod.rock` linked in the iPod 6G
  simulator build.

Important binary distinction:

- Hardware/device copy must come from `build-hw-ipod6g/apps/plugins/runepod.rock`
  or the staged `.rockbox/rocks/games/runepod.rock`.
- Simulator `.rock` files are ELF shared objects and will be rejected by native
  Rockbox as "not a plugin".
- Device deployment scripts must never copy `build-sim-*/simdisk/.../runepod.rock`
  to a real iPod.

Current known gaps:

- The prototype is still single-file and should be split only after the first
  control/content slice is proven on device.
- Save/load is not implemented.
- The map is not data-driven yet.
- Placeholder rendering uses primitives rather than generated sprite sheets.
- Simulator autostart validation is currently less reliable than normal
  simulator boot/build validation; device launch must be checked manually after
  copying the native plugin.

## Milestones

### Milestone 0: Skeleton

- Add `runepod.rock` scaffold.
- Boot to title, then a blank world view.
- Add iPod 6G control telemetry overlay in simulator.
- Validate build for iPod 6G simulator.

Status:

- Complete enough for prototype. The current title/world loop is already beyond
  a blank scaffold.
- Remaining improvement: add a small in-game debug/status overlay controlled by
  a compile-time `RP_DEBUG_OVERLAY` flag instead of relying on simulator-only
  smoke logging.

### Milestone 1: World Navigation Prototype

- Draw one small generated/placeholder village map.
- Implement avatar movement, focus target cycling, and default action.
- Add collision, camera clamp, and simple pathfinding.
- Acceptance: player can select a destination/resource/NPC without cursor drift
  or confusing wheel behavior.

Status:

- Partially complete. Cursor movement, camera follow, default actions, status
  tabs, map view, and direct walk-to-cursor/target are implemented.
- Not complete: collision grid and pathfinding are still placeholders.

Next acceptance details:

- Wheel movement moves the world cursor vertically by one normal step when no
  repeat flag is present.
- Wheel repeat moves the world cursor vertically by the configured repeat step.
- `LEFT`/`RIGHT` moves the world cursor horizontally.
- `SELECT` on open ground walks toward the cursor.
- `SELECT` on a far target under the cursor walks toward it and runs the
  default action when close.
- `MENU` exits from title/world and backs out from menus.
- `PLAY` opens inventory/status without changing world cursor selection.
- `MENU` closes inventory/status; `PLAY` does not close it.

Pathfinding plan:

- Use a fixed `RP_MAP_W x RP_MAP_H` collision grid for the village.
- Keep path node arrays fixed-size, such as 32x24 cells at 10px or 16px
  granularity.
- A* may be overkill for the first village; start with direct axis-biased
  stepping plus collision rejection, then add A* only when multi-zone collision
  needs it.
- If no path is found within the fixed node budget, move to the nearest
  reachable adjacent cell and show a status message.

### Milestone 2: Asset Style Slice

- Generate first coherent art set: player, one NPC, one tree, one rock, one
  enemy, one village tilesheet.
- Build conversion tool that emits BMP sheets plus manifest.
- Add compiled fallback assets and optional external pack loading.
- Acceptance: screenshot reads as one consistent game, not mixed placeholder
  art.

Asset slice v1:

- `actor_player`: 4 directions, idle + 2 walk frames, 24x32 or 32x32.
- `npc_villager`: 4 directions, idle only for first pass.
- `npc_shopkeeper`: 1 direction, idle only.
- `enemy_ratling`: 4 directions, idle + attack frame.
- `resource_oak`: static healthy, stump/cooldown variant.
- `resource_copper`: static healthy, depleted/cooldown variant.
- `resource_pond`: 2-frame water shimmer.
- `prop_fire`: 2-frame flame.
- `prop_workbench`: static.
- `ui_icons`: HP, coins, logs, ore, fish, food, combat.

First generated prompt contract:

- "Original low-poly-inspired fantasy RPG sprite, three-quarter top-down camera,
  compact silhouette, readable at 32px, hand-painted texture, muted natural
  palette, dark readable outline, transparent background."
- Do not include "RuneScape", "OSRS", known NPC names, known item names, or
  exact skill icon references in prompts or filenames.

Conversion rules:

- Keep source PNGs in an authoring folder.
- Convert final runtime assets to BMP sheets for Rockbox loading.
- For compiled fallback assets, convert small sheets to native bitmap headers
  under `apps/plugins/bitmaps/native/`.
- Use one transparent key color for all sprite sheets and record it in the
  manifest.

Proposed first external pack:

```text
.rockbox/rocks/games/runepod/
  pack.json
  sprites/actors.bmp
  sprites/resources.bmp
  sprites/props.bmp
  tiles/village_tiles.bmp
  ui/icons.bmp
```

Do not build the external pack loader until the fallback sprite sheet and
manifest format are stable. Early asset iteration can be compiled or directly
loaded from known BMP paths.

### Milestone 3: Skill Loop Slice

- Implement woodcutting, mining, fishing, cooking, XP, inventory, and level-up.
- Add resource cooldowns and simple messages.
- Acceptance: starter gathering loop works for 10 minutes without leaks,
  crashes, or input stalls.

Status:

- Basic one-click skill rewards exist in prototype form.
- Cooldowns exist as per-target tick timers.
- XP exists as counters only, without levels or level-up messaging.

Next skill model:

- XP tables are fixed integer thresholds:
  - level 1: 0 XP,
  - level 2: 10 XP,
  - level 3: 25 XP,
  - level 4: 45 XP,
  - level 5: 70 XP,
  - level 6: 105 XP,
  - level 7: 150 XP,
  - level 8: 210 XP,
  - level 9: 285 XP,
  - level 10: 380 XP.
- Resource actions take visible time:
  - mining: 3 ticks,
  - woodcutting: 3 ticks,
  - fishing: 4 ticks,
  - cooking: 2 ticks,
  - crafting: immediate menu action.
- Timed actions must be cancellable by selecting a new target or pressing
  `MENU`.
- Each action should display a compact progress indicator in the bottom strip.

Skilling data table fields:

- target kind,
- required tool kind,
- required level,
- action ticks,
- success chance,
- item reward,
- XP reward,
- cooldown ticks.

For MVP, success chance can be deterministic until the UI loop is tuned. Add
randomness only after save/load and tests are stable.

### Milestone 4: NPCs, Shop, Quest

- Add guide dialogue, shopkeeper, bank chest, and starter quest.
- Add save/load with version/checksum.
- Acceptance: quest can be completed, saved, reloaded, and completed state
  persists.

Status:

- Guide and shopkeeper behavior exist as hardcoded actions.
- Starter quest can be advanced and completed in memory.
- No save/load or bank chest yet.

Dialogue requirements:

- Dialogue text wraps to two or three short lines per screen.
- Wheel scrolls choices when choices exist.
- `SELECT` advances.
- `MENU` closes.
- No dialogue screen should require reading while holding a button.

Starter quest state machine:

```text
0: not started
1: accepted, needs 3 logs, 2 ore, 1 cooked fish
2: complete, cave unlocked
```

Save file v1:

- Path: `PLUGIN_GAMES_DATA_DIR "/runepod.sav"`.
- Magic: `RPOD`.
- Version: `1`.
- Fields:
  - player x/y,
  - selected target,
  - inventory,
  - XP counters,
  - HP,
  - quest stage,
  - enemy HP,
  - resource cooldown remaining in coarse seconds.
- Trailer checksum: simple CRC32 if available through Rockbox helpers, otherwise
  additive checksum for v1 with a TODO to replace.

Save behavior:

- Autosave on clean plugin exit.
- Manual save from pause/options once pause menu exists.
- If save is corrupt or version-mismatched, boot with defaults and show a short
  message.

### Milestone 5: Combat Cave

- Add cave zone, enemy spawns, melee combat, food use, drops, and respawn.
- Tune combat for click-wheel latency.
- Acceptance: player can enter cave, fight, flee/eat, receive drops, and return
  to village.

Combat v1 details:

- Combat remains target-and-watch, not twitch input.
- Attack tick interval: 1.2 seconds for player, 1.5 seconds for ratling.
- Hit formula for MVP:
  - player hit: `1 + weapon_bonus + combat_level / 3`,
  - ratling hit: `0 or 1`,
  - no floating point.
- Food restores 4 HP and is consumed by `PLAY` or Use Food.
- Flee returns player to cave entrance if not already in village.
- Drops:
  - ratling: 1-2 coins,
  - cave imp: 2-4 coins, chance of charm,
  - moss guard: 5 coins, guaranteed charm in MVP.

Cave unlock:

- Cave entrance is visible from the start but locked until quest stage 2.
- Selecting locked cave entrance defaults to "Inspect" and shows guide hint.

Enemy state:

- active/inactive,
- spawn point,
- current HP,
- respawn tick,
- aggro radius,
- target actor id.

### Milestone 6: Polish And Device Gate

- Add compact iPod-style menus, icon pass, sounds, pacing fixes, and options.
- Run simulator smoke tests and real-device deployment checks.
- Acceptance: stable 30 fps target in normal scenes, readable UI, no excessive
  disk activity during walking/combat.

Device deployment checklist:

- Build native target with `make -C build-hw-ipod6g -j8`.
- Confirm native plugin header starts with `RocK`:
  `xxd -g1 -l 4 build-hw-ipod6g/apps/plugins/runepod.rock`.
- Copy native plugin only:
  `build-hw-ipod6g/apps/plugins/runepod.rock` to
  `.rockbox/rocks/games/runepod.rock`.
- Never deploy `build-sim-*` plugin files to device.
- Launch from Plugins -> Games -> RunePod.
- Verify title screen, world screen, inventory, action menu, and exit.
- Verify the plugin returns to Rockbox cleanly after `MENU` exit.

Simulator validation gap:

- The iPod 6G simulator build and timed smoke can validate build health and
  simulator boot stability.
- Direct simulator autostart for this plugin may need a dedicated gate script
  rather than the current generic start-screen approach.
- Until that exists, final acceptance of controls requires manual simulator or
  physical-device interaction.

## Validation Plan

Simulator checks:

- Build `ipod6g` simulator.
- Launch `runepod.rock` directly from a clean simdisk.
- Exercise world navigation, menus, save/load, and one full quest path.
- Capture screenshots for village, forest, mine, cave, inventory, and dialogue.

Performance checks:

- Log frame time buckets in debug builds.
- Track max asset memory, loaded zone, active actors, path nodes, and disk load
  count.
- Fail debug gate if normal world frame pacing drops below target for sustained
  movement.

Input checks:

- Wheel slow scroll moves the cursor vertically by a normal step.
- Wheel repeat moves the cursor vertically by the repeat step.
- `LEFT`/`RIGHT` move the cursor horizontally.
- `PLAY` opens status from world and does not immediately close it on release.
- `MENU` closes status screens and exits RunePod from world.
- No required action depends on simultaneous button chords.

Device checks:

- Copy plugin and asset pack to the iPod 6G.
- Confirm boot, save/load, zone transition, and backlight behavior.
- Confirm disk does not spin repeatedly during steady-state walking.

## Testing Strategy

RunePod testing must cover four separate risks:

- Build correctness: the plugin compiles and links for simulator and native
  iPod 6G.
- Plugin binary correctness: the file copied to device is a native Rockbox
  plugin, not a simulator ELF.
- Runtime stability: Rockbox simulator and device stay alive while the plugin
  is launched, played, exited, and relaunched.
- Gameplay correctness: controls, state changes, quest progress, inventory,
  save/load, and zone transitions behave as designed.

Testing is not complete if only the simulator boots. Testing is complete for a
slice only when its automated checks pass and its manual checklist has been run
on either the iPod 6G simulator with visible interaction or the physical iPod
6G.

### Automated Build Tests

Run after every code change:

```sh
make -C build-sim-ipod6g -j8
make -C build-hw-ipod6g -j8
make -C build-sim-video-5g -j8
```

Acceptance:

- All three commands exit 0.
- `LD runepod.rock` appears in the relevant build when `runepod.c` changed.
- Native output exists at `build-hw-ipod6g/apps/plugins/runepod.rock`.
- Simulator output exists at `build-sim-ipod6g/apps/plugins/runepod.rock`.
- Runtime sprites and terrain assets exist in the simulator simdisk before
  launch testing. There is intentionally no bundled Runepod music asset.

### Native Plugin Header Test

Run after every native build and before every device deploy:

```sh
xxd -g1 -l 4 build-hw-ipod6g/apps/plugins/runepod.rock
xxd -g1 -l 4 .rockbox/rocks/games/runepod.rock
```

Expected native header:

```text
4b 63 6f 52
```

Failure cases:

- `7f 45 4c 46` means ELF. That is a simulator file and will show
  "not a plugin" on device.
- Missing staged file means deploy packaging is incomplete.
- Different first four bytes mean stop and rebuild before deploying.

### Runtime Asset Hash Test

Run before every device deploy:

```sh
sha256sum assets/runepod/runtime/sprites/runepod_sprites.320x64x24.bmp
sha256sum assets/runepod/runtime/sprites/runepod_player_dirs.128x32x24.bmp
sha256sum assets/runepod/runtime/tiles/runepod_terrain_tiles.256x32x24.bmp
```

Acceptance:

- Hashes match `assets/runepod/source/manifest_v1.json`.
- Device copies under `.rockbox/rocks/games/runepod/` match the workspace
  runtime assets after push.

### Simulator Smoke Gate

Run after every build-affecting change:

```sh
tools/simulator_first_gate.sh \
  --target ipod6g \
  --skip-build \
  --smoke \
  --timeout 5 \
  --allow-mounted-ipod \
  --evidence-file /tmp/runepod-ipod6g-gate.txt
```

Acceptance:

- Gate exits 0.
- Evidence file is written.
- Smoke result says the simulator stayed alive for the requested timeout.

Current limitation:

- This generic smoke proves simulator boot stability with the current simdisk.
  It does not yet prove RunePod direct-launch gameplay. A dedicated RunePod
  launch gate is required before this can be counted as automated gameplay
  validation.

### RunePod Direct-Launch Gate

Required before MVP completion.

Goal: launch RunePod directly in an isolated simulator simdisk and prove the
plugin reached its main loop.

Required behavior:

- Copy the freshly built simulator `runepod.rock` into an isolated simdisk.
- Configure the isolated simdisk to start RunePod.
- Run `rockboxui` for a bounded timeout.
- Produce an evidence artifact that RunePod reached:
  - `plugin_start`,
  - first title render,
  - first world render after synthetic or manual `SELECT`,
  - clean exit when synthetic or manual `MENU`/Exit is issued.

Implementation options:

- Add a RunePod-specific gate script under `tools/runepod_sim_gate.sh`.
- Or extend `tools/simulator_first_gate.sh` with a `--plugin` argument that
  writes a correct `plugin.dat` entry for the current build's generated
  language ids.
- Do not rely on hardcoded language enum values; read
  `build-sim-ipod6g/apps/lang/lang-enum.txt`.

Acceptance:

- The gate fails if no RunePod evidence artifact is produced.
- The gate fails if the simulator process exits before the expected timeout
  unless it exits due to a scripted clean RunePod exit.
- The gate prints the exact plugin path used.

### Manual Simulator Test Script

Run before deploying to physical iPod when direct-launch automation is not yet
available.

Setup:

- Build simulator: `make -C build-sim-ipod6g -j8`.
- Install simulator runtime: `make -C build-sim-ipod6g install`.
- Launch `build-sim-ipod6g/rockboxui`.
- Navigate to Plugins -> Games -> RunePod.

Checklist:

- Title screen appears and text is readable.
- `SELECT` enters the village.
- Wheel slow scroll moves the cursor vertically.
- `LEFT`/`RIGHT` moves the cursor horizontally.
- `SELECT` on Oak walks to Oak and chops.
- `SELECT` on Copper walks to Copper and mines.
- `SELECT` on Pond catches raw fish.
- `SELECT` on Fire cooks if raw fish exists.
- `PLAY` opens inventory.
- `MENU` closes inventory.
- `PLAY` release does not immediately close inventory after opening it.
- `SELECT` hold on Ratling opens action menu.
- Ratling combat changes HP/enemy HP and can reward coins.
- Guide starts quest, reports missing items, and completes quest after required
  resources.
- Shop buys food if coins are available.
- Exit path returns to Rockbox without hang or visual corruption.

Manual simulator acceptance:

- No crash during a 10-minute loop through gathering, inventory, guide, shop,
  and combat.
- No text overlaps in title, bottom strip, action menu, inventory, or dialogue.
- No required action depends on simultaneous button chords.

### Physical iPod 6G Test Script

Run before marking any gameplay slice complete.

Deploy:

- Build native: `make -C build-hw-ipod6g -j8`.
- Verify native header with `xxd`.
- Copy `build-hw-ipod6g/apps/plugins/runepod.rock` to
  `.rockbox/rocks/games/runepod.rock` on the iPod.
- Do not copy any `build-sim-*` `.rock` file to device.

Checklist:

- RunePod appears under Plugins -> Games.
- Launch does not show "not a plugin".
- Title appears within 2 seconds.
- Backlight stays on during active play.
- Wheel selection feels predictable.
- `MENU` backs out or exits according to the current UI state.
- Device returns to Rockbox after exit.
- Relaunch works without rebooting Rockbox.
- Battery/disk behavior is acceptable: no repeated disk spin-up during steady
  village play once assets are loaded.

Physical acceptance:

- 10 minutes of play without crash.
- Exit and relaunch three times.
- Complete the starter quest once save/load exists.
- Power-cycle after save/load and verify persisted state.

### Slice Acceptance Matrix

| Slice | Automated Build | Simulator Smoke | Manual Sim | Physical iPod | Extra Gate |
| --- | --- | --- | --- | --- | --- |
| A: Prototype hardening | required | required | required | required | native header |
| B: Save/load | required | required | required | required | corrupt-save test |
| C: Data-driven village | required | required | required | optional | object table diff review |
| D: First sprite sheet | required | required | required | required | screenshot review |
| E: Multi-zone transition | required | required | required | required | zone transition loop |
| MVP | required | required | required | required | direct-launch gate |

### Save/Load Tests

Required once Slice B starts:

- New game creates default state.
- Autosave on clean exit writes `runepod.sav`.
- Relaunch loads inventory, XP, HP, quest stage, player position, and selected
  target.
- Corrupt save starts a new game and displays a warning.
- Save version mismatch starts a new game and displays a warning.
- Partial write should not destroy the last valid save. Use a temp file plus
  rename for final write.

Manual save/load scenario:

1. Start new game.
2. Talk to Guide.
3. Chop 1 log, mine 1 ore, catch 1 fish.
4. Exit RunePod.
5. Relaunch.
6. Verify quest stage, inventory, and position persisted.
7. Finish quest.
8. Exit and relaunch.
9. Verify quest remains complete.

### Input Regression Tests

Run manually after every input change:

- Pressing `SELECT` once never opens the hold menu.
- Holding `SELECT` opens the action menu once, not repeatedly.
- Releasing `SELECT` after a hold does not also execute the default action.
- Wheel movement in world moves the cursor vertically and does not cycle a
  hidden target list.
- `LEFT` / `RIGHT` in world move the cursor horizontally and clamp at viewport
  edges.
- `SELECT` on open ground walks to the cursor.
- `SELECT` on an object under the cursor walks to and uses that object.
- `MENU` in action menu returns to world, not Rockbox.
- Exiting RunePod restores Rockbox click-wheel menu scrolling.
- `MENU` from pause confirms exit before leaving.
- `PLAY` quick action is disabled or predictable during dialogue.

### Rendering Tests

Run after every UI or sprite change:

- No text overlaps with the top strip, bottom strip, menu rows, or dialogue box.
- Selected target ring remains visible against all terrain.
- Player sprite anchor remains at feet, not sprite center.
- Actor/resource sprites draw in stable order.
- Runtime sprites do not show magenta/purple boxes or halos around their cells.
- Runtime sprite blits use Rockbox stride macros, not raw sheet width.
- Procedural animations are subtle and do not break target selection boxes or
  actor foot anchors.
- Inventory and Levels screens fit in 320x240 without row/text overlap.
- Color contrast remains readable on iPod 6G LCD.
- The first viewport gives a clear signal that this is RunePod, not a generic
  debug test screen.

Run after every generated asset source change:

- `file` and image metadata report the expected PNG/BMP dimensions and channel
  layout.
- The prompt and manifest are present beside the source image.
- The manifest records the chroma key, provenance constraints, included
  concepts, and runtime conversion status.
- Visual inspection confirms no text, watermark, proprietary layout, copied
  icon, or non-original asset reference is present.
- Converted runtime sheets are verified in simulator screenshots before they
  replace the current primitive fallback rendering.

### Performance Tests

MVP target:

- 30 fps target for village/forest/mine/cave normal play.
- No sustained frame stalls during target cycling.
- Zone transition may show a loading message but should not blank silently.
- No disk reads during steady-state walking after a zone is loaded.

Debug counters to add:

- frame count,
- slow frame count,
- max frame ticks,
- loaded zone id,
- active actor count,
- active target count,
- path nodes visited,
- asset bytes loaded,
- save write result.

Acceptance:

- In a 10-minute manual run, no visible long stall during normal village play.
- Any measured frame pacing issue must include the debug counters in the report.

### Test Evidence Format

Every substantial RunePod change should report:

- native build command/result,
- simulator build command/result,
- simulator smoke command/result,
- native plugin header bytes,
- staged plugin path,
- manual simulator/device checks run,
- known warnings or gaps.

Example:

```text
RunePod test evidence:
- Native build: make -C build-hw-ipod6g -j8 -> passed
- Simulator build: make -C build-sim-ipod6g -j8 -> passed
- Simulator smoke: tools/simulator_first_gate.sh ... -> passed
- Native header: 4b 63 6f 52
- Staged plugin: .rockbox/rocks/games/runepod.rock
- Manual device check: not run, iPod not mounted
- Known gap: direct RunePod simulator autostart gate not implemented
```

## Next Engineering Slices

### Slice A: Clean Prototype Hardening

Goal: make the current single-file prototype reliable enough to play on device
for five minutes.

Tasks:

- Remove or gate simulator-only smoke logging behind a build flag.
- Add a pause/options screen instead of immediate world `MENU` quit.
- Add explicit "Exit RunePod" confirmation from pause menu.
- Add top-strip debug build tag, such as `RP-PROT-1`, for screenshots.
- Fix any clipped text in title, bottom strip, action menu, and inventory.
- Add native deploy helper or documented copy step that uses only the hardware
  `.rock` file.

Acceptance:

- Native `runepod.rock` launches on iPod 6G without "not a plugin".
- Title, world, action menu, inventory, dialogue, and exit all work.
- No action requires more than one simultaneous button chord.

### Slice B: Save/Load

Goal: make the prototype worth playing across launches.

Tasks:

- Implement `rp_save_write`.
- Implement `rp_save_read`.
- Add save version and checksum.
- Autosave on clean exit.
- Load before title screen and show "Continue" if save exists.
- Add a "New Game" option that resets state.

Acceptance:

- Complete part of the starter quest, exit, relaunch, and verify state persists.
- Corrupt save file does not crash plugin.
- Version mismatch starts a clean game.

### Slice C: Data-Driven Village

Goal: stop hardcoding all world objects in C.

Tasks:

- Define a compact `struct rp_target_def` table for village objects.
- Split object state from object definition.
- Add collision rectangles.
- Add zone id and zone-local object ids.
- Move prototype village table to `rp_data.c` once file split begins.

Acceptance:

- Adding a new tree or rock requires editing only data tables.
- Cooldowns and object state remain separate from static map definitions.

### Slice D: First Sprite Sheet

Goal: replace primitive actor/resource drawings with one coherent fallback
sprite sheet.

Tasks:

- Generate or author first original source sprites.
- Convert to one BMP sheet.
- Add native bitmap header for fallback.
- Draw sprites with transparent-key handling.
- Preserve primitive fallback behind a debug flag until sprite rendering is
  stable.

Acceptance:

- Screenshot looks like a deliberate small RPG, not only debug geometry.
- All sprites share the same camera angle and scale.

### Slice E: First Multi-Zone Transition

Goal: prove the village can lead to a second area.

Tasks:

- Add forest zone.
- Add village gate target.
- Add zone transition state and player spawn points.
- Keep both zones in compiled data for now.
- Add forest-only trees and fishing spot.

Acceptance:

- Player can move village -> forest -> village.
- Inventory and quest state persist across zone transition.
- No full-screen blanking longer than one frame unless an intentional loading
  message is shown.

## File Split Trigger

Keep `runepod.c` single-file until Slice A and B are complete. Split after
save/load proves the state model.

First split should be mechanical:

- `rp_data.*`: static target definitions, XP tables, item names.
- `rp_render.*`: drawing helpers and views.
- `rp_input.*`: event-to-command mapping.
- `rp_state.*`: game state transitions and action execution.
- `rp_save.*`: save/load.

Do not split into asset, combat, skill, and quest modules until those systems
have enough code to justify separate files.

## Build And Deploy Notes

Build commands:

```sh
make -C build-sim-ipod6g -j8
make -C build-hw-ipod6g -j8
```

Native deploy source:

```text
build-hw-ipod6g/apps/plugins/runepod.rock
```

Staged native deploy destination:

```text
.rockbox/rocks/games/runepod.rock
```

Sanity checks:

```sh
xxd -g1 -l 4 build-hw-ipod6g/apps/plugins/runepod.rock
xxd -g1 -l 4 .rockbox/rocks/games/runepod.rock
```

Both native outputs must start with:

```text
4b 63 6f 52
```

That byte sequence is `RocK` in little-endian storage and confirms the file is a
native Rockbox plugin container. Simulator ELF files begin with `7f 45 4c 46`
and must not be copied to the iPod.

## Open Questions

- Should the camera be pure top-down or stronger isometric? Top-down is easier
  to read and implement; isometric better evokes the reference genre.
- Should combat be fully automatic once started, or should `SELECT` be used for
  timed attacks? Automatic is more native to the target and safer for MVP.
- Should external packs be required for the full art set, or should the build
  eventually compile the complete original asset set? External packs keep the
  plugin smaller and iteration faster.
- Should the first public version target only iPod 6G, or also iPod Video/5G?
  The 5G has the same 320x240 shape but a smaller plugin budget, so it should
  be a later compatibility pass.

## Immediate Next Actions

1. Verify the staged native `.rockbox/rocks/games/runepod.rock` on device.
2. Add pause/options and an explicit Exit command so `MENU` behavior matches
   iPod OS expectations.
3. Add save/load v1 for inventory, XP, HP, quest stage, and position.
4. Generate the first original fallback sprite sheet and replace primitive
   actor/resource drawings.
5. Add a dedicated RunePod simulator/direct-launch gate or document the manual
   simulator launch sequence once it is reliable.
