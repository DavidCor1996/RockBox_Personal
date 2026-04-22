# PocketCatch Overworld Plan

## Goal

Build the first playable walking screen for PocketCatch:

- choose a boy or girl player avatar
- walk a large stitched city built from FireRed town assets
- collide with visible wild Pokemon to enter the catch encounter

This is an overworld only. Do not add battles, interiors, items, gyms, trainers, or story scripting in this phase.

## Design Direction

PocketCatch should feel like Pokemon Go translated to an iPod:

- one continuous city screen with camera follow
- wheel-centric walking instead of D-pad RPG movement
- visible roaming wild Pokemon on the map
- immediate transition into the existing catch encounter on contact

## Asset Plan

Use personal-use extracted FireRed assets behind the asset provider layer.

Required player assets:

- `player_boy_idle_0.bmp`
- `player_boy_walk_0.bmp`
- `player_boy_walk_1.bmp`
- `player_girl_idle_0.bmp`
- `player_girl_walk_0.bmp`
- `player_girl_walk_1.bmp`

Required world assets:

- stitched tile chunks derived from Pallet Town, Viridian City, Pewter City, and route grass edges
- simplified roaming Pokemon encounter sprites

## Runtime Modules

Add these modules without coupling them to ROM filenames:

- `pc_world_state.*`
- `pc_world_render.*`
- `pc_world_input.*`
- `pc_world_assets.*`

Core structs:

- world mode / camera / avatar selection
- player position, facing, and walk animation
- city chunk metadata and collision mask
- roaming spawn positions and species ids
- transition request into encounter

## Input Model

- wheel motion sets travel direction and step intent
- continuous wheel movement keeps the player moving
- center confirms avatar selection or future interactions
- menu exits

## Implementation Order

1. Avatar asset provider
   Load boy and girl overworld sprites through the same asset abstraction used by the encounter screen.

2. Single stitched city
   Build one larger-than-screen city playfield using FireRed town art and route edges. Start with a hand-authored composite map rather than a full map importer.

3. Camera-follow walking
   Keep the player near screen center while the city scrolls. Use smooth 8-direction movement if it feels good on the wheel; otherwise use 4-direction with easing.

4. Visible wild Pokemon
   Spawn a few creatures in grass or sidewalk hotspots. Roaming can be simple idle drift.

5. Encounter transition
   On overlap with a spawn, freeze world motion and enter the existing catch screen with that species.

## Constraints

- keep memory conservative by chunking the world background rather than loading a full oversized bitmap
- prefer precomposed 320x240 or quadrant-friendly chunks over a general-purpose tile engine for the first pass
- keep deterministic frame timing similar to the encounter screen

## First Milestone Output

The first overworld milestone is complete when:

- the player can choose boy or girl
- the player can walk a stitched city
- roaming Pokemon are visible
- touching one launches the catch encounter
