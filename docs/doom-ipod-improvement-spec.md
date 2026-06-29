# Doom iPod Improvement Spec

## Goal

Make Doom feel intentional on iPod Video 5G/5.5G and iPod Classic 6G/7G,
without regressing plugin exit, audio playback, or existing WAD compatibility.

The first milestone should improve the current `apps/plugins/doom` port. A
larger engine replacement can be evaluated later, but it should not block the
basic iPod usability fixes.

## Implemented iPod Flow

- Game Cover Flow shows Doom as a first-class game with cover art.
- Short Select on Doom launches `doom_play.rock`, which chains into Doom's
  direct-play mode.
- Long Select on Doom opens `doom.rock --setup`.
- Direct-play mode uses `/.rockbox/doom/launcher.cfg`.
- If no valid base WAD is saved yet, direct-play mode shows a one-time WAD
  picker, saves the selection, and launches.
- The setup menu persists the selected base WAD, addon index, demo index,
  sound state, Fast Video state, and iPod control preset.
- Doom's in-game menus use iPod-style controls: wheel scrolls, Select chooses,
  and Menu backs out.

## Current State

The Doom plugin is the Rockdoom/PrBoom-derived port in `apps/plugins/doom`.
It builds as `doom.rock` from `apps/plugins/doom/doom.make`.

Required runtime files live in:

```text
/.rockbox/doom/
```

The current iPod keymap is in `apps/plugins/doom/i_video.c` under
`IPOD_4G_PAD`, `IPOD_3G_PAD`, and `IPOD_1G2G_PAD`.

Current iPod gameplay mapping:

| Doom action | Current iPod input |
| --- | --- |
| Move forward | Menu |
| Turn left | Left |
| Turn right | Right |
| Fire | Play |
| Open/use | Menu |
| Weapon/select | Select |
| Strafe left/right | Scroll wheel events |
| In-game menu | Hold switch toggles Escape |

This works, but it has several problems:

- Forward and use are both mapped to Menu, which causes accidental opens and
  makes movement less predictable.
- The wheel only produces momentary strafe impulses through `ev_scroll`, not a
  held movement state.
- There is no iPod-specific control preset menu.
- `Set Keys` exposes generic "Key Record/Mode/Off/On" labels that do not match
  click-wheel iPods.
- Esc/menu depends on the hold switch, which is awkward and can physically lock
  the device.
- Sound uses direct `pcm_play_data()` in `apps/plugins/doom/i_sound.c`.

## Strategy

### Research Recommendation

Best path for iPod visuals and performance:

1. Improve the current Rockdoom/PrBoom-derived port first.
2. Update the bundled/recommended Freedoom IWAD as an asset update only.
3. Prototype `doomgeneric` only if the current renderer/audio path proves too
   hard to clean up.
4. Do not import a large modern desktop source port for Milestone 1.

Reasoning:

- The iPod display is 320x240. Modern source-port features such as 640x400
  rendering, widescreen, uncapped renderer interpolation, OpenGL, consoles,
  scripting, and broad mod standards add code and memory pressure without
  matching the target hardware.
- The current Rockdoom code already has a Rockbox plugin integration, WAD menu,
  save handling, software renderer, direct LCD output, and iPod-specific
  button path.
- The biggest iPod problems are controls, LCD copy/update cost, PCM lifecycle,
  and fit inside the plugin memory model. A new engine does not remove those
  problems; it mostly restarts the integration work.
- Freedoom is content, not an engine. Updating it can improve art, maps, sounds,
  and vanilla compatibility, but it will not change renderer speed or input
  feel.

Useful upstream references checked for this recommendation:

- Freedoom: `https://freedoom.github.io/`
- Chocolate Doom: `https://www.chocolate-doom.org/wiki/index.php/About`
- Crispy Doom: `https://github.com/fabiangreffrath/crispy-doom`
- DSDA-Doom: `https://github.com/kraflab/dsda-doom`
- doomgeneric: `https://github.com/ozkl/doomgeneric`

Engine decision matrix:

| Option | Best use | iPod fit | Recommendation |
| --- | --- | --- | --- |
| Current Rockdoom | Existing Rockbox plugin, Boom/PrBoom lineage, current WAD menu | Best fit; already integrated | Primary path |
| Update Freedoom | Better free IWAD content | Good, but content-only | Do it independently |
| doomgeneric | Minimal engine experiment | Possible; simple platform API, sound still hard | Prototype only if needed |
| Chocolate Doom | Accuracy/vanilla behavior | Portable but SDL/host assumptions need work; fewer visual wins | Not first |
| Crispy Doom | Higher-res and QoL visuals | Attractive features, but 640x400/widescreen do not fit 320x240 well | Mine ideas, do not import wholesale |
| DSDA-Doom | Advanced PrBoom+ successor, speedrunning/mod tooling | Too large and feature-heavy for this target | Avoid |
| GZDoom/Doomsday-class ports | Modern visuals | OpenGL/desktop assumptions | Avoid |

### Milestone 1: Improve The Existing Port

Keep the current engine and file layout. Make the iPod-specific integration
better in small, testable patches.

Scope:

- iPod-specific control presets.
- Better click-wheel input handling.
- iPod-accurate menu labels.
- Optional HUD/control overlay only while configuring controls.
- Safer sound lifecycle around direct PCM.
- Lightweight profiling hooks for real device testing.

This is the preferred first path because it has the lowest compatibility risk
and gives quick feedback on real hardware.

### Milestone 2: Evaluate A Better Engine

After Milestone 1, evaluate whether a newer compact Doom engine is worth
porting. Candidates must be GPL-compatible and realistic for Rockbox plugin
constraints.

Evaluation criteria:

- Builds as a Rockbox plugin without hosted OS assumptions.
- Supports software rendering at 320x200 or 320x240.
- Does not require floating point or large dynamic allocations.
- Can use Rockbox file APIs and plugin memory safely.
- Has a simple sound callback model that can be routed through Rockbox PCM or
  mixer APIs.
- Maintains Doom shareware, Doom, Ultimate Doom, Doom II, Freedoom, TNT, and
  Plutonia WAD selection.
- Does not require replacing user playlist state or core firmware behavior.

Do not start a replacement port until the current port has a measured baseline
for frame rate, audio stability, memory use, and control usability.

## Control Design

Add a control preset layer in `apps/plugins/doom/i_video.c` and expose it in
the Doom options menu in `apps/plugins/doom/rockdoom.c`.

Recommended presets:

| Preset | Intended feel | Mapping |
| --- | --- | --- |
| Classic | Similar to current behavior | Menu forward, Play fire, Left/Right turn, wheel strafe, Select weapon |
| Shooter | Best default for iPod | Play forward, Select fire, Left/Right turn, Menu use, wheel weapon/map or strafe mode |
| Strafe | Better combat movement | Left/Right strafe, wheel turn, Play forward, Select fire, Menu use |
| Menu-safe | Avoid hold switch dependency | Select+Menu Escape, Select+Play map, Play fire, Menu use |

Default recommendation: `Shooter`.

Rationale:

- Play is easier to hold continuously than Menu on click-wheel iPods.
- Select is the most deliberate single action and works well for fire.
- Menu is a good "use/open" input because it is reachable but less central than
  fire.
- Left/Right should remain turn by default because Doom without turn controls is
  not playable.

### Input Requirements

Implement input as named Doom actions, not only preprocessor button aliases.

Required actions:

- `doom_action_forward`
- `doom_action_back`
- `doom_action_turn_left`
- `doom_action_turn_right`
- `doom_action_strafe_left`
- `doom_action_strafe_right`
- `doom_action_fire`
- `doom_action_use`
- `doom_action_weapon`
- `doom_action_map`
- `doom_action_escape`

The iPod backend should translate these actions to Doom key events:

- Forward/back: `KEY_UPARROW`, `KEY_DOWNARROW`
- Turn: `KEY_LEFTARROW`, `KEY_RIGHTARROW`
- Fire: `KEY_RCTRL`
- Use: space
- Weapon: `w`
- Map: `KEY_TAB`
- Escape: `KEY_ESCAPE`

### Wheel Handling

Replace the current one-shot `ev_scroll` feel with a wheel accumulator that can
serve either of these modes per preset:

- Strafe impulse mode: existing behavior, but with tunable impulse strength.
- Turn impulse mode: wheel rotation adds bounded turn input for fine aiming.
- Weapon mode: wheel rotation cycles weapons, rate-limited to avoid skipping.

Requirements:

- Consume wheel events without starving normal button state.
- Rate-limit weapon/map actions to avoid repeated accidental toggles.
- Clamp accumulated wheel movement once per tic.
- Preserve current behavior as the `Classic` preset.

### Escape/Menu

Do not make the hold switch the only practical way to reach the in-game menu.

Add at least one chord:

- `Select + Menu`: Escape
- `Select + Play`: Automap
- Optional: long Menu press as Escape if it does not break use/open.

The hold switch behavior can remain as a fallback.

### Configuration UI

Update `Oset_keys()` in `apps/plugins/doom/rockdoom.c` for iPod targets:

- Show iPod labels: Menu, Play, Select, Left, Right, Wheel CW, Wheel CCW,
  Select+Menu, Select+Play.
- Add "Controls" preset to the Doom Options menu.
- Keep the existing generic key configuration for non-iPod targets.
- Save the selected preset through existing Doom defaults in `m_misc.c`.

## Audio Requirements

Any audio change must follow `docs/plugin-audio-lifecycle-steering.md`.

Current Doom sound uses direct PCM:

- `I_InitSound()` stops PCM, sets playback source, sets sample rate, allocates
  lookup/mix buffers, and precaches SFX.
- `I_SubmitSound()` calls `rb->pcm_play_data()`.
- `I_ShutdownSound()` stops PCM and restores `HW_SAMPR_DEFAULT`.
- Music functions are currently dummy stubs.

Milestone 1 audio scope:

- Keep sound effects only.
- Ensure `I_ShutdownSound()` clears active sound channels before memory can be
  released.
- Confirm exit from Doom restores normal Database and Files playback on iPod
  6G.
- Do not introduce background music until the PCM/mixer ownership model is
  explicit and tested.

Acceptance tests:

- Fresh boot -> Doom with sound.
- Database music -> Doom with sound -> Database music resumes with sound.
- Files music -> Doom with sound -> Files music resumes with sound.
- Doom exit during active SFX does not freeze.
- Rapid Doom launch/quit cycles do not mute later playback.

Future audio option:

- Consider routing Doom SFX through a plugin-owned mixer channel only if the
  plugin API supports a safe lifecycle for this use. Keep direct PCM if it is
  more reliable on the target after cleanup.

## Rendering And Performance

Keep the renderer software-only.

Performance tasks:

- Add optional profiling counters guarded by a compile-time flag or Doom option.
- Measure frame time, tic time, screen update time, and sound mix time.
- Avoid broad renderer rewrites until input and audio are stable.
- Check whether iPod 6G and iPod Video differ meaningfully in LCD update cost.

Possible optimizations after measurement:

- Avoid full-screen LCD updates when menus or status-only areas change.
- Review `R_DrawColumn`, `R_DrawSpan`, and `I_FinishUpdate` hot paths.
- Keep fixed-point math; do not introduce floating point.
- Keep memory allocations out of the tic/render loop.

## WAD And Asset UX

Improve user-facing failure modes:

- If `rockdoom.wad` is missing, show that exact filename.
- If no IWAD exists, list expected names:
  `doom1.wad`, `doom.wad`, `doomu.wad`, `doom2.wad`, `doomf.wad`,
  `plutonia.wad`, `tnt.wad`.
- Keep addons under `/.rockbox/doom/addons/`.
- Keep demos under `/.rockbox/doom/demos/`.

Do not bundle commercial WADs.

Freedoom update policy:

- Treat `doomf.wad` as an optional redistributable IWAD, separate from engine
  work.
- Prefer current stable Freedoom release assets after verifying that Rockdoom
  can load them and that savegames, menu text, and DeHackEd lumps behave.
- Keep the previous known-good `doomf.wad` available in a backup or release
  note until the new IWAD has been tested on real hardware.
- Do not use Freedoom version changes as a performance benchmark unless the
  map complexity is held constant; newer maps/assets can change workload.

## Implementation Plan

1. Add an iPod control preset enum and persistent setting.
2. Refactor `getkey()` in `apps/plugins/doom/i_video.c` to emit actions from
   the selected preset.
3. Add chord detection for Escape and Automap.
4. Replace the current wheel-only `ev_scroll` path with a small wheel
   accumulator.
5. Update `rockdoom.c` options and key labels for iPod targets.
6. Harden `i_sound.c` shutdown so active channels and PCM callbacks cannot
   reference stale plugin memory.
7. Build `doom.rock` for `ipodvideo` and `ipod6g`.
8. Test on simulator for menu/control regressions.
9. Test on real iPod hardware with the audio lifecycle matrix above.
10. Only then evaluate a replacement Doom engine.

## Non-Goals

- No commercial WAD distribution.
- No network multiplayer.
- No MIDI/music support in Milestone 1.
- No playlist mutation for plugin music.
- No firmware-wide button remapping.
- No renderer rewrite before profiling data exists.

## Acceptance Criteria

Milestone 1 is complete when:

- Doom launches on iPod Video and iPod Classic with existing WAD layout.
- The default preset is playable without using the hold switch.
- Fire, forward, turn, use, weapon, map, and menu are all reachable.
- Wheel behavior is predictable and does not skip multiple weapons or menus.
- Doom exits cleanly and normal Rockbox playback still works afterward.
- Existing non-iPod keymaps still build.
- The manual or in-plugin help reflects the iPod controls.
