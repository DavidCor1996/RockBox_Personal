# InfoNES iPod 6G Performance and Color Spec

## Goal

Make NES emulation on the iPod 6G feel like stock NTSC NES hardware:

- Full-speed gameplay with no routine frameskip.
- Stable music pitch and clean sound without crackle, buffer underruns, or
  timing wobble.
- Correct controller feel for clickwheel hardware.
- Correct 2C02-style NTSC colors, including palette RAM mirroring, grayscale,
  and emphasis behavior.

The iPod 6G has more CPU and RAM headroom than older Rockbox targets. The
implementation should use that headroom aggressively when it improves timing or
audio quality. Do not optimize for the smallest possible memory footprint on
this target if a larger cache or buffer produces measurably better emulation.

## Reference Targets

Use NTSC as the first target. PAL can be added later as a separate mode.

- NTSC CPU clock: about 1.789773 MHz.
- NTSC frame timing: about 29780.5 CPU cycles per frame and 60.0988 Hz.
- Picture: 240 scanlines, with normal visible content usually treated as the
  NES 256x240 frame plus user/display overscan choices.
- Audio: five APU channels: two pulse, triangle, noise, and DMC. The APU frame
  counter runs independently of PPU NMI and drives envelopes, sweep, and length
  units.

Sources:

- NESdev cycle reference: https://www.nesdev.org/wiki/Cycle_reference_chart
- NESdev APU reference: https://www.nesdev.org/wiki/APU
- NESdev PPU palette reference: https://www.nesdev.org/wiki/PPU_palettes

## Current Problems

The current InfoNES port is playable, but not yet stock-quality:

- Simulator performance is better than iPod 6G hardware performance.
- Audio has been reported as choppy or poor on device.
- Earlier builds could overflow Rockbox's button queue in simulator.
- Current full-screen rendering is fast enough in sim, but hardware needs a
  measured frame-time breakdown.
- A static RGB palette swap did not fix the Super Mario Bros. sky color, which
  means the remaining color issue is likely PPU palette-index, PPUMASK, or
  palette RAM behavior, not just RGB constants.

## Non-Negotiable Acceptance Gates

### Runtime

On iPod 6G hardware:

- Super Mario Bros. runs for 10 minutes with no obvious slowdown.
- Gameplay cadence matches NTSC NES: target 60.0988 fps, not rounded 60.000
  unless the Rockbox tick/audio model forces a measured compromise.
- No emulator-driven frameskip in normal SMB gameplay.
- Worst-case test ROMs may use a bounded emergency frameskip mode only if audio
  remains stable and the skip count is logged.

### Audio

On iPod 6G hardware:

- Music pitch must not drift relative to video.
- No repeated crackle, underrun clicks, or buffer starvation.
- APU output should use the nonlinear NES mixer model or a close lookup-table
  approximation.
- DMC must be accounted for in timing and mixing, even if an interim fast mode
  can disable it for debugging.

### Color

- Palette RAM writes must mask values to 6 bits.
- Palette RAM mirrors must match NES behavior, especially `$3F00` / `$3F10`
  universal backdrop mirroring.
- PPUMASK grayscale must remap by palette index, not by desaturating RGB.
- PPUMASK emphasis bits must be implemented using precomputed color tables.
- Color `$0D` must not produce a brighter or unstable display output; map it to
  the chosen 2C02 reference behavior or a safe black-compatible value.
- The default RGB table should be generated from a documented 2C02 NTSC
  composite palette, not hand-adjusted by eye.

NESdev explicitly notes that no single composite RGB palette can match every
real TV setup. The project default should therefore be "2C02 NTSC composite,
SMPTE/Pally-derived, stable on iPod LCD", with optional user presets later.

### Controls

- Touch wheel should remain the D-pad on iPod targets with wheel position.
- Center/select should be usable as B/run.
- Play/bottom should be usable as A/jump.
- A+B, Start, and Select must be possible without awkward finger gymnastics.
- Touch direction should not accidentally change when pressing A/B.

## Measurement Harness

Do not tune by feel alone. Add an InfoNES profile log equivalent to the Rockboy
profile work.

Write summaries to:

`/.rockbox/infones/profile.log`

Each run should log:

- ROM name and mapper.
- Target, build hash, and whether CPU boost is active.
- Total frames, rendered frames, skipped frames.
- Effective fps x1000.
- Target fps x1000.
- Average and peak frame ticks.
- Average and peak CPU ticks.
- Average and peak PPU/render ticks.
- Average and peak scale/blit ticks.
- Average and peak APU ticks.
- PCM underruns.
- PCM queue-low events.
- PCM queue-full waits.
- Audio sample rate.
- Audio buffer size and queued buffer count.
- Palette writes and PPUMASK emphasis/grayscale changes.
- Mapper bank switches.
- Disk writes during emulation.

Use a compile-time or runtime profile flag so normal play does not pay for
profiling counters.

## Test Corpus

Use legally acquired ROMs only.

Minimum smoke corpus:

- Super Mario Bros. - baseline speed, music, scrolling, A+B control, sky color.
- The Legend of Zelda - SRAM, scrolling, sustained music.
- Tetris - simple visual load, music timing.
- Mike Tyson's Punch-Out!! or Punch-Out!! - sprite load and timing sensitivity.
- Mega Man 2 or Contra - action, scrolling, sprite pressure.
- Kirby's Adventure or Super Mario Bros. 3 - mapper/audio/DMC pressure.

Use one known-good emulator screenshot/audio capture per ROM as visual and
auditory reference. The reference emulator must use a documented 2C02 NTSC
palette and no shaders.

## Phase 1: Baseline and Instrumentation

Tasks:

- Add InfoNES profile counters.
- Add a simulator-first gate that autostarts a ROM and confirms the profile
  log is produced.
- Add an iPod hardware checklist for real clickwheel/audio testing.
- Capture current SMB on hardware for 2 minutes and log frame/audio status.
- Confirm whether iPod hardware is CPU-bound, APU-bound, render-bound, or
  blocked on PCM/disk/yield.

Acceptance:

- Profile log exists for sim and hardware.
- SMB hardware run identifies the top two bottlenecks.
- No optimization phase starts without before/after numbers.

## Phase 2: Use iPod 6G RAM Deliberately

The 64 MB model should not be treated like a tiny target. Prefer memory-backed
smoothness where it helps.

Candidates:

- Keep entire ROM in RAM. Avoid storage reads after launch except SRAM saves.
- Defer SRAM writes until exit, and avoid Rockbox idle storage callbacks during
  active emulation if they cause stalls.
- Increase PCM ring depth on iPod 6G. Start with 8 to 12 software buffers,
  then measure latency and underruns.
- Allocate a larger contiguous audio hardware-copy buffer if that reduces PCM
  callback pressure.
- Precompute 8 emphasis palettes x 64 NES colors into native `fb_data`.
- Precompute grayscale/remapped palette variants.
- Precompute 256-to-320 horizontal scale maps or expand tables for the 6G
  320x240 fast path.
- Cache decoded CHR tile rows in native pixel or palette-index form.
- Cache sprite row decode where CHR data is unchanged.
- Add mapper read/write lookup tables where InfoNES still uses branch-heavy
  memory paths.

Acceptance:

- Each RAM-backed cache must show a frame-time, underrun, or jitter improvement.
- Total memory use must be logged at plugin start.
- If memory allocation fails, the plugin must fall back cleanly or refuse launch
  with a clear message.

## Phase 3: CPU and Scheduling

Tasks:

- Keep CPU boosted for the whole emulation session on iPod 6G.
- Reduce yield/sleep frequency to the minimum needed for USB/system safety.
- Avoid `sleep(1)` in the tight frame pacing path; use yield or deadline waits
  only when ahead.
- Measure whether the current scanline loop should render every line or batch
  multiple scanlines before polling system events.
- Inline hot memory read/write paths in the 6502 core.
- Replace large branchy mapper paths with direct function pointers or maps for
  common mappers: NROM, MMC1, UNROM, CNROM, MMC3.
- Consider computed-goto or generated opcode dispatch only after profiling
  proves the C switch interpreter is the main bottleneck.

Acceptance:

- SMB average frame time stays below the NTSC frame budget with headroom.
- No button queue overflow in sim.
- USB/system exit still works.
- Hardware gameplay feels at least as smooth as simulator for SMB.

## Phase 4: Audio Quality

Current InfoNES pAPU is old and likely the biggest stock-quality risk. Treat
audio as a first-class subsystem, not a side effect of frame pacing.

Tasks:

- Confirm exact `pAPU_QUALITY` output rate and `samples_per_sync` against the
  60.0988 Hz NTSC cadence.
- Test 22.05 kHz, 44.1 kHz, and hardware-supported Rockbox rates on iPod 6G.
- Use larger PCM rings on iPod 6G before lowering sample rate.
- Implement queue-low and underrun counters.
- Make audio the master clock if video pacing causes audio starvation.
- Replace linear channel summing with an NES nonlinear mixer lookup table:
  - pulse table for pulse1 + pulse2.
  - TND table for triangle + noise + DMC.
- Verify frame counter behavior, especially games that write `$4017` once per
  frame, such as Super Mario Bros. and The Legend of Zelda.
- Confirm DMC sample playback timing and CPU steal behavior. If full DMC timing
  is too expensive, log it and make any fast-mode compromise explicit.

Acceptance:

- SMB overworld music has stable pitch and tempo for 5 minutes.
- Zelda title/overworld music has no repeating crackle.
- No PCM underruns during the SMB baseline run.
- If 44.1 kHz is too expensive, 22.05 kHz must still sound stable and clean.

## Phase 5: PPU Color Correctness

The SMB purple-sky symptom must be debugged at the PPU state level.

Tasks:

- Log palette RAM values for SMB after title load and during level 1-1.
- Dump `$3F00-$3F1F`, PPUMASK, and the first visible scanline backdrop color.
- Verify InfoNES does not ignore high palette bits incorrectly.
- Mask palette writes to `byData & 0x3f`.
- Fix palette mirrors for `$3F00/$3F10`, `$3F04/$3F14`,
  `$3F08/$3F18`, and `$3F0C/$3F1C`.
- Implement PPUMASK grayscale as index masking, generally `color &= 0x30`.
- Implement PPUMASK emphasis as a palette-table selector rather than per-pixel
  math.
- Generate a native 16-bit `fb_data nes_palette[8][64]` at startup:
  - 8 emphasis combinations.
  - 64 base colors.
  - optional grayscale remap.
- Validate backdrop behavior when background and sprites are transparent.

Acceptance:

- SMB 1-1 sky matches the reference emulator using the same documented palette.
- Tetris and Zelda colors match reference screenshots within expected RGB565
  quantization.
- No per-pixel color conversion remains in the hot render path.

## Phase 6: Renderer and Scaling

Tasks:

- Keep the direct framebuffer 256-to-320 fast path.
- Precompute horizontal expansion for 4 source pixels to 5 destination pixels.
- Consider rendering directly to 320x240 framebuffer for iPod 6G to avoid a
  separate native NES frame copy.
- If rendering direct-to-scaled is too invasive, keep current WorkFrame and
  optimize only the scale step.
- Add dirty-background fast paths only after correctness passes:
  - no scroll change,
  - no CHR dirty,
  - no sprite overlap,
  - unchanged palette.

Acceptance:

- Full-screen scaling cost is below 10 percent of frame budget on iPod 6G.
- Rendering direct-to-framebuffer must not break screenshot dumps or fallback
  targets.

## Phase 7: Compatibility Modes

Add modes only after the default path is measured.

Suggested modes:

- Accurate: default, no frameskip, correct color/audio behavior.
- Balanced: same audio/color, renderer caches enabled, emergency frameskip
  allowed if late.
- Fast: optional compatibility tradeoffs, clearly labeled, for heavy mappers or
  later experiments.

Do not make Fast the default just because it hides an audio or timing bug.

## Implementation Order

1. Add profiling and hardware log checklist.
2. Add larger iPod 6G PCM ring and underrun counters.
3. Fix palette RAM, PPUMASK grayscale, emphasis lookup, and SMB palette logs.
4. Add nonlinear APU mixer lookup.
5. Tune frame pacing against NTSC 60.0988 Hz.
6. Add RAM-backed PPU/CHR/render caches.
7. Add CPU interpreter hot-path optimizations if profiling still points there.
8. Build optional modes only after the default path is stable.

## Done Definition

This work is complete when:

- SMB on iPod 6G runs full speed with clean music and correct colors.
- The same build passes the simulator stability gate.
- At least five ROMs from the test corpus pass 5-minute hardware runs.
- Profile logs show no routine underruns, no default frameskip, and frame time
  below budget.
- The iPod artifact copied to the device matches the build artifact by hash.
- The implementation and logs are committed to git.

