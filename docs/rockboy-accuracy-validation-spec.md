# Rockboy Accuracy Validation Spec

## Goal

Rockboy should not be described as Game Boy frame-accurate until it passes
repeatable CPU, timing, memory, LCD, and visual reference tests in the
simulator.

## Harness

- `tools/rockboy_accuracy_gate.py` prepares an isolated simulator disk,
  direct-starts a supplied accuracy ROM, and validates Rockboy serial output.
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

Fixed during this pass:

- `POP AF` now masks unused low flag bits.
- `DAA` now uses explicit Game Boy flag math.
- `HALT` now idles even when IME is clear and wakes on enabled pending
  interrupts.
- `ADD SP,e8` and `LD HL,SP+e8` now use low-byte carry and half-carry flags.
- Simulator serial output now logs SB/SC test-ROM output.

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
