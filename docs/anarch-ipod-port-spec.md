# Anarch Rockbox iPod Port Specification

## Decision

Port Anarch as a native Rockbox game plugin using the upstream C engine and
its real 3D software-rendered assets. This is substantially more feasible than
recreating Chao Garden: Anarch is already a small, portable, integer-only game
with embedded content, no heap requirement, and an explicit platform API.

The first supported target is iPod Classic 6G/7G (`ipod6g`) and its simulator.
Add iPod Video 5G after the physical 6G build is stable and a reduced rendering
profile has been measured on the slower CPU.

Initial scope:

- The complete upstream game, maps, textures, sound effects, and optional
  bytebeat music.
- 320x240 RGB565 output, with a 160x120 logical render mode available as the
  performance fallback.
- Click-wheel controls, haptics, saves, settings, clean USB exit, and a
  deterministic simulator gate.
- A silent playable milestone before mixer audio is enabled.

Non-goals for the first release are network play, replacement art, a level
editor, arbitrary mod loading, mouse-look parity, and changes to the Rockbox
plugin ABI.

## Upstream and Licensing Baseline

Import from the official [Anarch repository](https://gitlab.com/drummyfish/anarch)
at commit `6f90562161200682459e772f1dacb747f23c5f95`. Record the exact commit and
the imported-file list in `apps/plugins/anarch/UPSTREAM.md`.

Anarch is released under CC0 with a public-domain waiver. Preserve the upstream
license/waiver verbatim as `apps/plugins/anarch/LICENSE.upstream`. The engine's
maps, textures, palette, samples, and other bundled content may be imported
because upstream identifies them as original CC0 work. Do not mix unrelated
third-party assets into the import without a separate source and license entry.

The useful upstream properties are:

- C99, one engine compilation unit, integer/fixed-point rendering.
- No required operating-system, filesystem, floating-point, or dynamic-memory
  dependency.
- Embedded levels, 32x32 indexed textures, a 256-color palette, and seven small
  sound samples.
- Platform callbacks for pixels, time, sleep, keys, sound, music, events, and
  save/load.
- Compile-time controls for resolution, frame rate, raycasting subsampling,
  draw distance, fog, and dithering.

Keep upstream engine changes narrow. Platform behavior belongs in Rockbox
frontend files, not scattered through the imported game code.

## Repository Shape

```text
apps/plugins/anarch/
    SOURCES
    anarch.make
    anarch.c
    anarch_platform.c
    anarch_platform.h
    anarch_video.c
    anarch_input.c
    anarch_audio.c
    anarch_save.c
    anarch_menu.c
    LICENSE.upstream
    UPSTREAM.md
    upstream/
        game.h
        settings.h
        selected embedded content headers
```

`anarch.c` owns `plugin_start()`, startup ordering, the main loop, and one
cleanup path. The `upstream/` directory remains mechanically comparable with
the pinned revision. Build it through `apps/plugins/anarch/anarch.make` and add
the subdirectory to `apps/plugins/SUBDIRS` only for supported color targets.

The packaged plugin is `.rockbox/rocks/games/anarch.rock`. No external game
data is required.

## Upstream Platform Mapping

| Anarch callback | Rockbox implementation |
| --- | --- |
| `SFG_setPixel(x, y, color)` | Bounds-check, convert the embedded palette entry to RGB565, and write the current staging surface. |
| `SFG_getTimeMs()` | Derive monotonic milliseconds from `*rb->current_tick`; handle tick wrap with unsigned arithmetic. |
| `SFG_sleepMs(ms)` | Sleep only the remaining frame budget and continue polling for USB and exit events. |
| `SFG_keyPressed(key)` | Return the latched logical control state produced by the click-wheel adapter. |
| `SFG_getMouseOffset()` | Return zero initially; mouse-look remains disabled. |
| `SFG_setMusic(track)` | Change bytebeat/music state in the frontend mixer, never from the PCM callback. |
| `SFG_playSound(sound, volume)` | Enqueue a bounded sound command for the local game mixer. |
| `SFG_processEvent(type, data)` | Route vibration to the Rockbox haptic API and expose significant events to the debug trace. |
| `SFG_save(data)` / `SFG_load(data)` | Read or write the versioned save wrapper; never expose raw filesystem operations to the engine. |

Call `SFG_init()` once after video, input, and save state are ready. Call
`SFG_mainLoopBody()` from the plugin loop until it requests exit or the
frontend receives Menu/USB termination. Do not change Anarch's fixed-timestep
semantics to compensate for a slow renderer; select a sustainable fixed frame
rate instead.

## Rendering and Performance Plan

The bring-up profile is a 160x120 logical image presented at 2x nearest-neighbor
scale. `SFG_setPixel()` expands each logical pixel into a 2x2 RGB565 block in a
320x240 staging surface. This preserves the upstream assets and 3D renderer;
it is not hand-drawn substitution art.

Once the game is correct, profile these configurations on physical hardware:

| Profile | Logical resolution | Initial frame rate | Purpose |
| --- | ---: | ---: | --- |
| Quality | 320x240 | 25 fps | Preferred iPod 6G mode if the 95th-percentile frame meets budget. |
| Balanced | 160x120, 2x output | 30 fps | Default bring-up and likely iPod 5G mode. |
| Battery | 160x120, 2x output | 20 fps | Optional lower-load mode. |

Use one RGB565 staging surface, approximately 150 KiB, acquired from the
plugin-local buffer with `plugin_get_buffer()`. Do not take the shared audio
buffer merely to hold a framebuffer. Present through the normal LCD bitmap or
framebuffer API and measure render and LCD-copy time separately.

Quality mode ships only if all of the following pass during a five-minute
representative run:

- 95th-percentile frame time stays within the selected fixed-step budget.
- No sustained simulation slowdown or input queue growth occurs.
- No framebuffer guard, stack canary, or plugin-buffer bound is touched.
- Thermal behavior and battery draw are no worse than existing full-screen
  software-rendered games at the same CPU policy.

Expose resolution, FPS, raycasting subsample, fog/dither, and music as a small
settings menu. Keep settings within known-safe compile-time profiles rather
than accepting arbitrary engine constants.

## Click-Wheel Controls

Use absolute wheel position with eight zones and hysteresis. Do not treat raw
wheel velocity as a held direction. Preserve held-button state across repeat
events and clear all logical keys when the hold switch becomes active.

| iPod input | Game action |
| --- | --- |
| Wheel top / bottom | Move forward / backward |
| Wheel left / right | Turn left / right |
| Center | Fire / confirm |
| Play/Pause | Jump / cancel in menus |
| Previous | Strafe left |
| Next | Strafe right |
| Center + Previous | Previous weapon |
| Center + Next | Next weapon |
| Menu, short press | In-game pause menu |
| Menu, held press | Request exit, with save confirmation if state changed |

The pause menu provides Resume, Map, Options, Save, Restart Level, and Exit.
If simultaneous click-wheel button reporting proves unreliable on hardware,
move weapon selection into the pause menu instead of creating timing-sensitive
chords.

## Saves and Configuration

Store user data under `.rockbox/games/anarch/`:

```text
anarch.sav
anarch.cfg
```

Wrap the fixed-size upstream save payload in a frontend header containing a
four-byte magic, frontend format version, upstream commit identifier/hash,
payload length, and CRC32. Write to a sibling temporary file, verify the full
write and checksum, close it, then rename it over the previous save. A corrupt
or unknown file must produce a visible warning and start a new game without
overwriting the bad file until the user saves.

Configuration is versioned separately and contains only frontend choices. Use
Rockbox configuration helpers where practical. Never serialize pointers,
framebuffer contents, mixer state, or compiler-dependent structures.

## Audio Lifecycle

Follow `docs/plugin-audio-lifecycle-steering.md` for every audio change. Audio
lands after silent playability and is confined to `anarch_audio.c`.

Use a bounded plugin-owned PCM ring and Rockbox's mixer playback channel. Mix
Anarch's effects and optional bytebeat music into that one stream so callback
ownership remains simple. Allocate the ring from the plugin-local arena; the
small upstream samples remain read-only game data. Do not claim the shared
audio buffer unless measurement proves the local arena insufficient, and
never call `audio_stop()` as a precondition for buffer ownership.

The callback may only consume already prepared blocks and update atomic/ring
indices. It must not allocate, access files, change tracks, or invoke the game
engine. On pause or exit:

1. Stop generation and prevent new sound commands.
2. Stop/unregister the mixer channel.
3. Wait until no callback can reference plugin memory.
4. Restore any mixer frequency or global setting changed by the plugin.
5. Release or discard the plugin arena and return from `plugin_start()`.

Playlist state is never modified. Validate entry and exit from both Database
and Files with music playing, paused, and stopped, including rapid relaunches.

## Lifecycle, USB, and Haptics

Poll for `SYS_USB_CONNECTED` through the normal button/event path. On USB,
save if safe, perform the complete callback/audio teardown, restore LCD and
backlight state, and return `PLUGIN_USB_CONNECTED`.

Map upstream vibration events through the public Rockbox haptic API only when
global haptics are enabled. Rate-limit repeated weapon/damage events and stop
any active haptic effect on pause, hold-switch activation, USB, and exit.

Use a single cleanup function for partial initialization failures and normal
exit. Track initialized subsystems with explicit flags so cleanup order is
deterministic.

## Diagnostics and Verification

Simulator builds provide an opt-in deterministic mode with a fixed input
script, frame counter, render-time histogram, dropped/late-frame count, audio
underrun count, and final framebuffer hash. Debug logging must never include
save data and is compiled out of release builds.

Required gates:

- Build for the iPod 6G simulator and hardware target with no unresolved
  libc, libm, SDL, or operating-system symbols.
- Complete a scripted title-to-gameplay sequence for at least 1,800 frames.
- Exercise every click-wheel binding, hold-switch clearing, pause/resume, save,
  reload, corrupt-save recovery, and USB exit.
- Run framebuffer guard and arena high-water checks in simulator builds.
- Confirm silent and audio-enabled builds produce identical game-state hashes
  for the same deterministic input.
- On physical iPod 6G, complete five minutes in each enabled quality profile
  and record average/95th/worst frame time and audio underruns.
- On iPod 5G, enable the plugin only after the Balanced profile sustains its
  frame budget and clean audio lifecycle tests pass.

## Delivery Milestones

1. **Import gate:** pinned CC0 source, license ledger, build-only frontend, and
   host-side callback tests.
2. **Video gate:** deterministic title/menu rendering in the simulator at
   160x120 logical resolution, with golden framebuffer hashes.
3. **Playable-silent gate:** complete controls, pause, saves, haptics, USB,
   and a physical 6G gameplay test without audio.
4. **Audio gate:** sound effects, then optional music, with callback teardown
   and Database/Files regression coverage.
5. **Quality gate:** compare native and scaled rendering, choose the 6G default,
   and retain automatic fallback when startup memory checks fail.
6. **5G gate:** profile the slower target and enable only the profiles that
   satisfy the same correctness and lifecycle criteria.

The first implementation slice ends at milestone 3. It must not be delayed by
music, native-resolution optimization, or iPod 5G support.
