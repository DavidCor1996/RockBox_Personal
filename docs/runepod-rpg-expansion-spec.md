# RunePod RPG Expansion Spec

## Goal

Make `runepod.rock` feel like a small native click-wheel RPG on 320x240 iPods
without adding a ported engine, emulator core, playlist mutation, or plugin
audio dependency.

## First Slice

- Keep the existing top-down cursor/action model.
- Expand the explorable village map from one compact square into a broader
  valley with forest, mine, pond, ruins, and cave regions.
- Add more interactable targets while keeping the world deterministic and
  static enough for the iPod 6G plugin budget.
- Persist player state in `/.rockbox/rocks/games/runepod.save`.
- Generate and deploy all external art sheets required by the plugin:
  - `runepod/sprites/runepod_sprites.320x160x24.bmp`
  - `runepod/sprites/runepod_player_dirs.384x32x24.bmp`
  - `runepod/tiles/runepod_terrain_tiles.256x32x24.bmp`
- Keep RockPod responsible for installing the plugin asset folder beside
  `runepod.rock`.

## Save Data

The save file is a small versioned binary record with a magic value, version,
payload fields, and checksum. It stores:

- player/cursor/destination coordinates;
- facing direction and selected target;
- inventory, XP, health, enemy health, and quest stage;
- target cooldown ticks adjusted relative to the current tick on load.

If the file is missing, stale, truncated, or fails checksum validation, RunePod
starts a new game and keeps running.

## Second Slice

- Extend the first quest into a Druid cave trial:
  - Guide quest grants the charm.
  - Druid opens the cave trial after the player has the charm and enough field
    training.
  - Ratling combat is gated until the trial is active.
  - Clearing the trial consumes the charm, rewards coins/food, and advances the
    persistent quest stage.
- Make XP affect play:
  - woodcutting, mining, and fishing levels improve resource yields;
  - combat levels improve ratling damage;
  - the Levels screen explains the reward cadence.
- Show the current quest hint from the Inventory screen.

## Third Slice

- Add a city district east of the starter village with:
  - a market that sells food bundles;
  - a smith that converts ore, logs, and coins into ward charms;
  - an inn that restores health and saves;
  - a healer that restores health for coins.
- Add repeatable field mobs:
  - low-risk slimes for early coins;
  - bandits for higher coin rewards;
  - cave bats gated behind the ratling trial.
- Expand generated art:
  - the main sprite sheet gains city buildings, shop/service icons, and new
    enemy sprites;
  - the player sheet becomes a 12-frame directional walking strip with three
    frames each for south, east, north, and west.
- Keep old saves readable by migrating version 1 save data into version 2 and
  initializing the new targets without deleting inventory or quest progress.

## Fourth Slice

- Make the city read as a functional place with furniture/service targets:
  market counter, forge, anvil, inn bed, inn table, healing shrine, city well,
  and notice board.
- Move progression toward a slow cumulative XP curve inspired by RuneScape and
  Brighter Shores rather than a fast linear arcade curve.
- Add a fourth row to the generated sprite sheet for city furniture and service
  props.

## Fifth Slice

- Expand city outskirts with guard/bounty, bakery, trainer, training dummy,
  herb bed, bank chest, skeleton, and wolf targets.
- Add explicit action animations for every skill loop and supporting service:
  chopping, mining, fishing, cooking, crafting, combat, eating, herb picking,
  training, shrine/well effects, and trade/sorting coin flashes.
- Add a fifth generated sprite row for the new outskirts NPCs, mobs, buildings,
  and props.

## RockPod Integration

RockPod should expose RunePod as a custom game plugin and deploy all generated
assets under:

```text
/.rockbox/rocks/games/runepod/
```

Removing the plugin through RockPod should remove the deployed art assets but
leave `runepod.save` alone unless a future save-manager flow explicitly asks to
delete saves.

## Non-Goals

- No new engine ports.
- No background music or PCM work in this slice.
- No generated assets committed only in build output.
- No use of copyrighted game art.
