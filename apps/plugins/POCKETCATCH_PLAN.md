# PocketCatch Plan (iPod 5G Rockbox Plugin)

Updated: 2026-04-08

## Vision

Build a polished, modern-feeling monster-catching game for iPod 5G in Rockbox, inspired by Pokémon Go catch flow and classic iPod click-wheel gameplay.

## Core Design Pillars

- Authentic click-wheel-first controls (no touch-screen assumptions)
- Fast, satisfying catch loop
- Lightweight visuals that still feel stylish and modern
- Simulator-first development and validation

## MVP Scope (Phase 1)

Focus only on the catch scene:
- Spawn a wild creature in a single encounter screen
- Spin ball by rotating wheel
- Auto-throw when wheel input stops (release timeout)
- Evaluate hit, shake sequence, catch/fail result
- Retry loop with score/streak tracking

Out of scope for MVP:
- Overworld map walking
- Inventory systems beyond 1-2 basic ball types
- Multiplayer/network features

## Input & Controls (iPod Style)

Primary controls:
- Wheel clockwise/counterclockwise: build spin + curve
- Wheel idle timeout (`~120-180ms`): release/throw trigger
- `SELECT`: confirm / quick throw fallback
- `LEFT` / `RIGHT`: switch ball type (later phase)
- `MENU`: pause/exit

Implementation notes:
- Use `BUTTON_SCROLL_FWD` / `BUTTON_SCROLL_BACK`
- Scrollwheel has no natural release event, so release is inferred by timeout
- Track recent wheel velocity and directional bias for throw physics

## Catch Mechanics Spec (Initial)

Throw parameters:
- `power`: from recent spin velocity + total spin amount
- `curve`: from clockwise vs counterclockwise bias
- `accuracy`: penalize too little/too much spin

Catch result flow:
1. Ball thrown
2. Hit/miss check against moving target ring
3. On hit: 1-3 shake sequence
4. Resolve as catch or breakout

## Technical Architecture

Plugin modules (planned split):
- `pocketcatch.c` (entry point, main loop)
- `pc_input.*` (wheel/button sampling)
- `pc_physics.*` (throw trajectories and hit checks)
- `pc_state.*` (scene/catch state machine)
- `pc_render.*` (draw background, sprites, UI)
- `pc_assets.*` (external pack loading + fallbacks)

## Asset Strategy

Primary policy:
- Ship with original/permissive placeholder assets
- Support external packs for custom user content

Related spec:
- `POCKETCATCH_ASSET_PACK_SPEC.md`

Potential reference sources:
- `pret/pokeemerald`
- `pret/pokefirered`
- `pret/pokeruby`
- `pret/pokecrystal`

Legal note:
- Do not bundle copyrighted commercial assets in public plugin builds.

## Visual Direction (Modern on Rockbox)

- Layered/parallax background with restrained palette
- Smooth throw trail + impact spark effects
- Minimal, readable HUD with strong contrast
- Consistent icon style and subtle animation timing

## Milestones

### Milestone 0: Scaffold
- Create plugin skeleton and update loop
- Add on-screen debug telemetry (spin, curve, power)

### Milestone 1: Input Prototype
- Read wheel events reliably on iPod/simulator
- Implement spin accumulation and release timeout throw

### Milestone 2: Physics + Hit Detection
- Add throw trajectory + curve effect
- Add moving target ring and hit evaluation

### Milestone 3: Catch State Machine
- Throw -> impact -> shake -> result states
- Add basic success/fail probabilities

### Milestone 4: Visual Polish
- Add modern HUD, particles, transitions
- Tune frame pacing and responsiveness

### Milestone 5: Asset Pack Integration
 - Load external pack from `PLUGIN_GAMES_DIR`
 - Add robust fallback to compiled placeholders

## Validation Plan

- Simulator-first testing for all gameplay iterations
- Per-milestone checks:
  - Input latency and wheel sensitivity
  - Deterministic throw outcomes under fixed seed
  - FPS/memory stability on target iPod profile

## Status Update
- MVP skeleton implemented: pocketcatch.c added and built into plugin suite. This provides a minimal plugin_start path that shows a splash and exits.
- Next steps: flesh out input sampling (wheel/buttons), basic catch loop scaffolding, and asset pack hooks per MVP scope.

## Immediate Next Actions

1. Create `pocketcatch.c` scaffold + debug HUD
2. Implement wheel-to-throw prototype with placeholder circles
3. Hook first external assets using the pack spec
