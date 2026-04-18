# nightcity

`nightcity` is a compact Rockbox narrative RPG built for the iPod Video / 5G clickwheel target and simulator.

## Design notes

- Original cyberpunk-inspired setting with an original plot, factions, characters, and endings.
- Built as a small data-driven narrative engine:
  - story nodes and choices live in `apps/plugins/nightcity/story.c`
  - engine/save flow lives in `apps/plugins/nightcity/engine.c`
  - UI and input handling live in `apps/plugins/nightcity/ui.c`
  - encounters live in `apps/plugins/nightcity/encounter.c`
- Current presentation assets are Rockbox-native and compact:
  - compiled bitmap title card, scene cards, and portrait strip for color 320x240 targets
  - added Nyra portrait art and an afterglow rooftop scene card
  - dedicated threat cards for encounters, a deck module strip, and ending banner art
  - code-drawn fallback panels and glyphs for lower-capability builds
  - full-width animated scene banners for story beats, with ambient skyline / clinic / relay / convoy motion
  - a dedicated afterglow/full-moon animation theme for Nyra's early and rooftop scenes
  - a full-screen animated scene intro when a run enters story content
  - an auto-shuffled background radio pass from the active Rockbox playlist when story scenes begin
  - optional "radio bleed" overlays that surface the currently playing shuffled track in-scene
  - small original synth/beep stings for the title screen and afterglow scenes when no music is already playing
  - glitch transitions between story nodes
- Prioritizes stable text-heavy play over graphics-heavy ambition.
- Uses Rockbox plugin actions so clickwheel hardware and the SDL simulator share the same control logic.

## Controls

- Scroll wheel / `Up` and `Down`: move through options
- Center / `Return` or keypad `5`: confirm, advance, choose
- Left / `Left` or keypad `4`: open stats and inventory
- Menu / `Backspace`, `Insert`, `Escape`, or keypad `.`: pause / back
- New game flow: choose lifepath, then choose gender

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

Current verified checks:

- `nightcity.rock` links successfully for the iPod Video / 5G simulator target after the gender/Nyra/audio pass
- updated build was copied into `build-sim-video-5g/simdisk/.rockbox/rocks/games/nightcity.rock`
- basic launch was previously confirmed in the simulator

Recommended manual regression pass after asset changes:

- verify the title card renders cleanly at 320x240
- verify animated story banners change with scene context and do not overwrite dialogue text
- verify the initial full-screen story intro plays once at run start and returns cleanly to the scene UI
- verify the new gender picker appears after lifepath selection and saves correctly
- verify Nyra's `Afterhours` intro scene appears before loadout and the femme-only romance option gates correctly
- verify the rooftop/broadcast scene uses the full-moon afterglow animation theme
- verify portrait strip alignment for each speaker slot
- verify long choice labels wrap cleanly without clipping the selector
- verify encounter threat cards and status chips render without overlapping logs or actions
- verify deck screen cyberware strip and stat meters stay readable on simulator and device
- verify each ending banner maps to the intended ending text
- verify background radio shuffles the active playlist once at story start without breaking playback
- verify optional radio-bleed overlays read current playback metadata without disturbing music playback
- verify title/afterglow stings only play when Rockbox is not already playing music
- verify glitch transitions still return cleanly to dialogue choice input
- verify stats/inventory and encounter screens still recover correctly after backing out

## Follow-up recommendations

- Add a richer optional mid-game encounter branch
- Add a compact mission log panel if the story grows
- Add tiny UI sounds only if simulator and device testing show no instability
- Consider a small manual entry once the story structure settles
