# nightcity

`nightcity` is a compact Rockbox narrative RPG built for the iPod Video / 5G clickwheel target and simulator.

## Design notes

- Original cyberpunk-inspired setting with an original plot, factions, characters, and endings.
- Built as a small data-driven narrative engine:
  - story nodes and choices live in `apps/plugins/nightcity/story.c`
  - engine/save flow lives in `apps/plugins/nightcity/engine.c`
  - UI and input handling live in `apps/plugins/nightcity/ui.c`
  - encounters live in `apps/plugins/nightcity/encounter.c`
- Current presentation assets are Rockbox-native and code-driven:
  - animated scene panels
  - speaker glyphs
  - ambient skyline / clinic / relay / convoy motion
  - glitch transitions between story nodes
- Prioritizes stable text-heavy play over graphics-heavy ambition.
- Uses Rockbox plugin actions so clickwheel hardware and the SDL simulator share the same control logic.

## Controls

- Scroll wheel / `Up` and `Down`: move through options
- Center / `Return` or keypad `5`: confirm, advance, choose
- Left / `Left` or keypad `4`: open stats and inventory
- Menu / `Backspace`, `Insert`, `Escape`, or keypad `.`: pause / back

Simulator keyboard references come from `uisimulator/buttonmap/ipod.c`.

## Build

From the existing simulator build directory:

```bash
make -C build-sim-video-5g -j4
make -C build-sim-video-5g install
```

For a broader rebuild:

```bash
make -C build-sim-video-5g fullinstall
```

## Run In Simulator

Launch the iPod Video / 5G simulator:

```bash
build-sim-video-5g/rockboxui --zoom 2 --nobackground --root build-sim-video-5g/simdisk
```

Then browse to:

- `Plugins`
- `Games`
- `nightcity`

## Save / Continue

- The plugin writes a checkpoint save to `/.rockbox/rocks/games/nightcity.sav`
- A save is written whenever a new story node is entered
- `Continue` from the title screen resumes from the last checkpoint
- Encounter checkpoints restart from the beginning of the encounter node

## Testing notes

Initial expected checks:

- plugin builds cleanly in `build-sim-video-5g`
- plugin launches from the simulator without immediate crash
- title menu renders
- animated scene panels and transitions render in the story UI
- lifepath selection works
- dialogue pages and choice menus respond to wheel navigation
- encounters resolve without invalid state
- return-to-title and continue both work with the checkpoint save

## Follow-up recommendations

- Add a richer optional mid-game encounter branch
- Add a compact mission log panel if the story grows
- Add tiny UI sounds only if simulator and device testing show no instability
- Consider a small manual entry once the story structure settles
