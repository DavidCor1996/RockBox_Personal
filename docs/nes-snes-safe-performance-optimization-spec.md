# NES and SNES Safe Performance Optimization Specification

Status: implementation approved

Target: InfoNES and SNES Lite on the iPod 6G 320x240 RGB565 target, while
preserving portable-target behavior.

## Goals

- Reduce per-frame CPU work and LCD transfer volume without changing emulation
  timing, rendered game state, or default audio quality.
- Remove diagnostic overhead from normal InfoNES play while keeping the
  simulator/profile gate available on demand.
- Keep every change easy to disable, compare, and revert independently.

## Non-goals and safety constraints

- Do not enable frameskip by default.
- Do not lower the InfoNES sample rate or SNES audio quality.
- Do not add target-specific CPU, PPU, scaler, or SPC assembly. Previous SNES
  Lite hardware testing showed that renderer assembly can regress performance.
- Do not change mapper, CPU, PPU, APU, DSP, save-state, SRAM, or ROM semantics.
- Do not change plugin audio ownership or teardown ordering. InfoNES continues
  to stop PCM callbacks before releasing its audio buffer and restores the
  default PCM frequency and playback routing.
- Do not remove SNES special-chip sources in this pass. That is a size/link-time
  project and needs a compatibility inventory rather than a runtime-only test.

## Phase 1 implementation

### 1. SNES Lite native video fast path

For native 256x224 RGB565 video on a 320x240 display:

- Clear the black border only when entering native mode or when native geometry
  changes, instead of clearing all 76,800 pixels every frame.
- Copy each 256-pixel row with `memcpy` when the core and LCD formats are the
  same RGB565 layout; retain the scalar conversion fallback for other formats.
- Update only the native game rectangle after steady-state frames. Update the
  FPS overlay rectangle separately when it is enabled. The first native frame
  still performs a full update so cleared borders reach the panel.
- Preserve the existing fullscreen scaler and its full-screen update behavior.

Expected result: native mode removes the full-frame clear, replaces 57,344
per-pixel assignments with row copies, and reduces steady-state LCD transfer
from 320x240 to 256x224 pixels (about 25.3% fewer pixels).

### 2. InfoNES opt-in profiling

- Add `profile=1` to `.rockbox/infones/options.cfg` as the explicit switch for
  runtime profiling. The default is off.
- When profiling is off, skip histogram/counter updates, scale/sound timing
  reads, the 120-frame early log write, and the exit log write.
- Keep pacing and simulator termination independent from diagnostic logging.
- Make `tools/infones_sim_gate.py` write `profile=1`, preserving its current
  profile-log contract.

Expected result: normal play performs no profile-log disk I/O and avoids
diagnostic bookkeeping in frame, palette, PCM, and audio hot paths.

### 3. InfoNES batched audio-ring submission

- Fill the current stereo PCM ring slot in the largest safe chunk available.
- Call the ring submission/locking function only when a chunk fills a slot,
  rather than once for every generated sample.
- Preserve sample order, mixing, filtering, stereo duplication, startup water
  mark, underrun behavior, and full-ring drop behavior.

Expected result: a 735-sample 44.1 kHz APU callback normally performs one or
two capacity checks instead of 735 calls, without changing produced samples.

### 4. InfoNES incremental palette writes

- On ordinary palette writes, update only the affected `PalTable` entry.
- When the universal backdrop color changes, update the eight transparent
  backdrop entries.
- Retain a full 32-entry rebuild for PPUMASK grayscale/emphasis changes because
  those bits affect every cached color.

Expected result: common PPU palette writes fall from 32 color conversions to
one; universal-backdrop writes require eight. Palette mirroring remains intact.

### 5. Optional InfoNES native video mode

- Add `native_video=1` to `.rockbox/infones/options.cfg` as an opt-in setting.
- Keep fullscreen scaling as the default.
- In native mode, use the existing centered 256x240 bitmap path and partial LCD
  update, bypassing 256-to-320 horizontal scaling.

Expected result: native mode avoids the scaler and transfers 20% fewer LCD
pixels while leaving existing default presentation unchanged.

## Verification

The implementation is acceptable when all of the following pass:

1. InfoNES and SNES Lite compile for the iPod 6G simulator and hardware target.
2. Existing SNES Lite video self-tests pass for native and fullscreen modes.
3. The InfoNES simulator gate completes with sound both disabled and enabled,
   writes a profile only because the gate requests `profile=1`, reports the
   requested emulated-frame count, and reports zero configured frameskip.
4. A normal InfoNES launch without `profile=1` creates no new profile entry.
5. Visual checks show correct SNES black borders, no stale FPS overlay pixels,
   correct NES palette emphasis/grayscale, and identical native/fullscreen game
   content apart from scaling.
6. Audio transition tests cover playback active, paused, stopped, rapid exit,
   and USB/system interruption with no callback-after-release condition.

Hardware performance claims remain provisional until measured on an iPod 6G;
simulator timing is used for regression detection, not final speed claims.
