# Rockboy Accuracy Validation Spec

## Goal

Rockboy should not be described as Game Boy frame-accurate until it passes
repeatable CPU, timing, memory, LCD, and visual reference tests in the
simulator.

## Harness

- `tools/rockboy_accuracy_gate.py` prepares an isolated simulator disk,
  direct-starts a supplied accuracy ROM, and validates Rockboy serial output.
- `tools/rockboy_accuracy_gate.py --force-dmg` patches only the staged copy of
  a ROM so CGB-compatible DMG test ROMs run in DMG mode.
- Simulator-only `ROCKBOY_SERIAL_LOG=1` captures Game Boy link-port bytes to
  `/.rockbox/rockboy/serial.log`.
- `tools/rockboy_accuracy_gate.py --run` launches the iPod Video simulator,
  waits for the expected serial text, and stops the simulator process after the
  ROM emits a verdict.
- Simulator-only `ROCKBOX_SIM_PLUGIN` and `ROCKBOX_SIM_PLUGIN_PARAM` can be
  used by the gates to launch Rockboy directly with a staged ROM path. This
  avoids fragile manual navigation and keeps hardware builds unchanged.
- Simulator-only `ROCKBOY_ACCURACY_FAST=1` is used by `--run` to skip frame
  pacing and LCD line refresh work during serial-only accuracy gates. A black
  simulator window is expected in this mode; normal simulator and iPod runs do
  not use this flag.
- Test ROM binaries are not committed. Use an external checkout such as
  `/tmp/gb-test-roms`.

## Current Results

Passed on 2026-06-20:

- `blargg/cpu_instrs/cpu_instrs.gb`: `Passed all tests`
- `blargg/instr_timing/instr_timing.gb`: `Passed`
- `blargg/mem_timing/mem_timing.gb`: `Passed all tests`
- `blargg/mem_timing-2/mem_timing.gb`: on-screen `Passed`
  - This ROM did not emit a serial log in the simulator run, so the result was
    verified by simulator screenshot instead of `serial.log`.
- `blargg/interrupt_time/interrupt_time.gb`: on-screen `Passed`
  - The final rows report `0D`, matching the expected interrupt-entry timing.
- `blargg/halt_bug.gb`: on-screen `Passed`
  - The test verifies the DMG HALT PC increment bug and IF unused-bit readback.
- `blargg/oam_bug/rom_singles/1-lcd_sync.gb`: on-screen `Passed`
  - The staged ROM must be run with `--force-dmg` because the original header
    is CGB-compatible and Rockboy otherwise starts it in CGB mode.
- `blargg/dmg_sound/rom_singles/01-registers.gb`: on-screen `Passed`
  - Sound register reads now return the DMG fixed read-one bits, APU power-off
    clears NR10-NR51, powered-off register writes are ignored, and wave RAM
    remains readable/writable.
- `blargg/dmg_sound/rom_singles/02-len ctr.gb`: on-screen `Passed`
  - Trigger writes now preserve nonzero length counters, zero length reloads to
    the channel maximum, length counters only clock while length enable is set,
    disabled channels can still clock length, and DAC-off writes immediately
    clear channel status.

Passed on 2026-06-21:

- `blargg/dmg_sound/rom_singles/03-trigger.gb`: on-screen `Passed`
  - Fifth-register length-enable writes now use a frame-sequencer phase model
    for DMG extra clocks, and trigger writes still perform length reload and
    DAC-gated side effects while preserving nonzero counters.
- `blargg/dmg_sound/rom_singles/07-len sweep period sync.gb`: on-screen
  `Passed`
  - Length and sweep clocks now run from the CPU-driven APU frame sequencer
    rather than audio-mixer sample cadence.
- `blargg/dmg_sound/rom_singles/08-len ctr during power.gb`: on-screen
  `Passed`
  - DMG APU power-off now freezes internal length counters, and powered-off
    length-register writes update the hidden counters without making normal
    powered-off register writes visible.
- `blargg/dmg_sound/rom_singles/09-wave read while on.gb`: on-screen
  `Passed`
- `blargg/dmg_sound/rom_singles/10-wave trigger while on.gb`: on-screen
  `Passed`
- `blargg/dmg_sound/rom_singles/11-regs after power.gb`: on-screen
  `Passed`
- `blargg/dmg_sound/rom_singles/12-wave write while on.gb`: on-screen
  `Passed`
- `blargg/dmg_sound/dmg_sound.gb`: on-screen `Passed`
  - Final aggregate screen showed `01:ok` through `12:ok` followed by
    `Passed`.
- `blargg/oam_bug/oam_bug.gb` in forced-DMG mode: serial `Passed`
  - Individual groups `1-lcd_sync`, `2-causes`, `3-non_causes`,
    `4-scanline_timing`, `5-timing_bug`, `6-timing_no_bug`, and
    `8-instr_effect` were also validated with the simulator gate.
  - `7-timing_effect` is diagnostic-heavy as an individual ROM, but the
    aggregate OAM ROM passes with the same implementation.
- `blargg/cgb_sound/cgb_sound.gb`: serial `Passed`
  - The previously failing `08-len ctr during power`,
    `09-wave read while on`, `11-regs after power`, and `12-wave` singles now
    pass in the simulator.
- Automated gate rerun:
  - `cpu_instrs.gb`: serial `Passed all tests`
  - `instr_timing.gb`: serial `Passed`
  - `mem_timing.gb`: serial `Passed all tests`
  - `dmg_sound.gb`: serial `Passed`
  - `oam_bug.gb` with `--force-dmg`: serial `Passed`
  - `cgb_sound.gb`: serial `Passed`
- LCD/gameplay smoke:
  - `cpu_instrs.gb` frame 120 dump-only LCD capture completed and produced a
    gate-validated non-blank 160x144 PPM frame.
  - `Pokemon - Red Version (USA, Europe) (SGB Enhanced).gb` frame 300
    dump-only LCD capture completed after scripted START/A input and produced
    a gate-validated non-blank 160x144 PPM frame.
- Performance cadence:
  - Normal Rockboy gameplay now paces frames from the original Game Boy clock:
    4,194,304 Hz / 70,224 cycles per frame = 59.7275 fps.
  - `tools/rockboy_profile_gate.py --run --validate-speed` validates
    `target_fps_x1000=59728`, average paced frame ticks, and skipped-frame
    ratio from `profile.log`.
  - `Pokemon - Red Version (USA, Europe) (SGB Enhanced).gb` passed the
    simulator speed gate with sound on and frameskip forced off:
    `total_frames=900`, `rendered_frames=900`, `skipped_frames=0`,
    `effective_fps_x1000=59094`, `target_fps_x1000=59728`.

Fixed during this pass:

- `POP AF` now masks unused low flag bits.
- `DAA` now uses explicit Game Boy flag math.
- `HALT` now idles even when IME is clear and wakes on enabled pending
  interrupts.
- DMG `HALT` with `IME=0` and pending `IE & IF` now suppresses the next opcode
  fetch PC increment, matching the HALT bug.
- Interrupt entry now charges five machine cycles before the interrupt handler
  instruction stream continues.
- IF register reads now return bits 5-7 set while writes remain masked to the
  implemented low interrupt bits.
- `ADD SP,e8` and `LD HL,SP+e8` now use low-byte carry and half-carry flags.
- Simulator serial output now logs SB/SC test-ROM output.
- LCD enable now advances the first visible scanline at the boundary expected
  by `oam_bug/rom_singles/1-lcd_sync.gb`.
- DMG sound register masks, APU power-off semantics, wave RAM write/read
  behavior, length counter reload/preserve behavior, length-enable clocking,
  and DAC-gated channel status now match Blargg's first two DMG sound groups.
- DMG sound frame-sequencer length/sweep timing, fifth-register extra clocks,
  DMG power-off length counter freezing, active wave-RAM read/write windows,
  and DMG wave retrigger corruption now pass Blargg's full `dmg_sound.gb`
  aggregate suite in the iPod Video simulator.
- The DMG OAM corruption bug is now modeled for mode-2 OAM accesses, stack
  push/pop ordering, and HL auto-increment/decrement 16-bit IDU effects. The
  forced-DMG `oam_bug.gb` aggregate now passes in the iPod Video simulator.
- CGB APU power-off/power-on behavior now resets hidden length counters,
  ignores powered-off register writes, preserves wave RAM, exposes channel-3's
  current wave byte while playing, targets current-byte wave writes, and uses a
  CGB-specific wave restart phase. The full `cgb_sound.gb` aggregate now
  passes in the iPod Video simulator.
- Frame pacing no longer targets rounded 60 fps. The normal gameplay loop uses
  the hardware CPU/frame-cycle ratio, while serial-only fast accuracy gates
  still opt out of pacing with `ROCKBOY_ACCURACY_FAST=1`.
- The profile speed gate stages only the selected ROM into a minimal simulator
  disk and forces `frameskip=0`/`maxskip=0`, so passing performance evidence is
  based on rendered frames rather than hidden skip recovery.

Resolved failures from the expanded 2026-06-20 simulator pass:

- `blargg/oam_bug/oam_bug.gb` in forced-DMG mode now reports `Passed`.
- `blargg/cgb_sound/cgb_sound.gb` now reports `Passed`.

Memory timing work completed:

- `blargg/mem_timing/mem_timing.gb`
  - Initial output: `01:01  02:01  03:01`
  - Initial result: `Failed 3 tests.`
  - Cause: Rockboy advanced timers once per whole instruction, after the
    instruction completed, so TIMA was not observable at the instruction's
    individual read/write cycles.
  - Fix approach: the CPU interpreter now opens an instruction-local timing
    context after opcode fetch, and `readb`/`writeb`/`readw`/`writew` advance
    one emulated memory cycle while that context is active.
  - Final output: `01:ok  02:ok  03:ok`
  - Final result: `Passed all tests`

## Acceptance Before Claiming Frame Accuracy

Required:

- `cpu_instrs.gb` passes.
- `instr_timing.gb` passes.
- `mem_timing.gb` passes.
- `oam_bug.gb` passes in forced-DMG mode.
- `dmg_sound.gb` and `cgb_sound.gb` pass.
- LCD visual reference tests such as DMG/CGB acid tests match reference output
  once external reference images are supplied.
- A small ROM corpus still boots in simulator after accuracy changes.
- Profile speed gates show normal gameplay cadence within tolerance and do not
  rely on sustained frameskip to keep up.

Current status: serial CPU/timing/memory/OAM/APU gates pass and LCD dump-only
smokes are automated with nonblank-frame validation. Do not claim frame
accuracy yet until true LCD reference images are supplied and matched
pixel-for-pixel.
