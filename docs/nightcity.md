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
  - compiled bitmap title card, scene cards, and a larger cast/profile portrait atlas for color 320x240 targets
  - visible cast portraits for Vesper, Juno, Mira, Rook, Sable, Kade, hostile speakers, and Nyra
  - three creator-driven protagonist profile portraits that appear in story scenes
  - added Nyra portrait art and an afterglow rooftop scene card
  - dedicated threat cards for encounters, a deck module strip, and ending banner art
  - code-drawn fallback panels and glyphs for lower-capability builds
  - full-width animated scene banners for story beats, with ambient skyline / clinic / relay / convoy motion
  - a dedicated afterglow/full-moon animation theme for Nyra's early and rooftop scenes
  - a full-screen animated scene intro when a run enters story content, now with a larger animated speaker portrait
  - an auto-shuffled background radio pass from the active Rockbox playlist when story scenes begin
  - optional "radio bleed" overlays that surface the currently playing shuffled track in-scene
  - small original synth/beep stings for the title screen and afterglow scenes when no music is already playing
  - glitch transitions between story nodes
- Story expansion now includes recurring cast payoffs instead of one-off introductions:
  - Juno returns in a new `Dead Channel` interlude after the Sable reveal
  - Nyra's femme-only path now leads into a dedicated `Rooftop Afterglow` scene instead of ending at a single choice line
  - Mira and Rook now have route-specific relationship choices during the clinic and convoy branches
  - ending cards surface relationship-specific tag lines for Nyra, Mira, Rook, and Juno
- Prioritizes stable text-heavy play over graphics-heavy ambition.
- Uses Rockbox plugin actions so clickwheel hardware and the SDL simulator share the same control logic.

## Controls

- Scroll wheel / `Up` and `Down`: move through options
- Center / `Return` or keypad `5`: confirm, advance, choose
- Left / `Left` or keypad `4`: open stats and inventory
- Menu / `Backspace`, `Insert`, `Escape`, or keypad `.`: pause / back
- New game flow: choose lifepath, then choose gender, then choose an operator profile

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

- `nightcity.rock` links successfully for both the iPod Video / 5G simulator and hardware targets after the cast/romance expansion pass
- updated build was copied into `build-sim-video-5g/simdisk/.rockbox/rocks/games/nightcity.rock`
- simulator startup was re-verified with `build-sim-video-5g/rockboxui --zoom 1 --nobackground --root simdisk`
- updated hardware build was copied to `/.rockbox/rocks/games/nightcity.rock` on the mounted iPod
- confirm handling now uses select-release only inside `nightcity` to avoid clickwheel double-advance

Recommended manual regression pass after asset changes:

- verify the title card renders cleanly at 320x240
- verify animated story banners change with scene context and do not overwrite dialogue text
- verify the initial full-screen story intro plays once at run start and returns cleanly to the scene UI
- verify the new gender picker appears after lifepath selection and saves correctly
- verify the new operator profile picker shows the portrait preview and applies the intended stat bonus
- verify Vesper uses the chosen creator portrait during player-spoken scenes
- verify Juno's `Dead Channel` scene appears after the Sable conversation and before the route crossroads
- verify Nyra's `Afterhours` intro scene appears before loadout and the femme-only romance option gates correctly
- verify the rooftop/broadcast path uses the full-moon afterglow animation theme and reaches the dedicated `Rooftop Afterglow` follow-up scene
- verify Mira and Rook spark routes unlock their new clinic/convoy relationship choices
- verify large cast portraits stay aligned in both the regular story view and the cinematic scene intro
- verify long choice labels wrap cleanly without clipping the selector
- verify a single center-button press only advances once on clickwheel hardware and in the simulator
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
- Add more route-specific scene art or animated overlays for Mira/Rook/Juno to match Nyra's stronger cinematic treatment
- Add a compact mission log panel if the story grows
- Add tiny UI sounds only if simulator and device testing show no instability
- Consider a small manual entry once the story structure settles
