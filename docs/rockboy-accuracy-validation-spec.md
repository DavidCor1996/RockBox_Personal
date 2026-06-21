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

Known failures from the expanded 2026-06-20 simulator pass:

- `blargg/oam_bug/oam_bug.gb` in forced-DMG mode:
  - Passing groups: `01`, `03`, `06`.
  - Failing groups: `02`, `04`, `05`, `07`, `08`.
  - Remaining cause: Rockboy still does not emulate the DMG OAM corruption bug
    caused by OAM accesses and 16-bit IDU operations during PPU mode 2.
- `blargg/dmg_sound/dmg_sound.gb`: groups `01` and `02` pass on-screen.
  Groups `03`-`12` still fail; the next known blocker is
  `rom_singles/03-trigger.gb`, which requires a stable APU frame-sequencer
  phase model for write-time length-enable/trigger extra clocks.
- `blargg/cgb_sound/cgb_sound.gb`: group `10` passed; the other groups failed
  on-screen.

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
- LCD visual reference tests such as DMG/CGB acid tests match reference output.
- A small ROM corpus still boots in simulator after accuracy changes.

Current status: not frame-accurate yet.
