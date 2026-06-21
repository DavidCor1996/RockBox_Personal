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

Fixed during this pass:

- `POP AF` now masks unused low flag bits.
- `DAA` now uses explicit Game Boy flag math.
- `HALT` now idles even when IME is clear and wakes on enabled pending
  interrupts.
- `ADD SP,e8` and `LD HL,SP+e8` now use low-byte carry and half-carry flags.
- Simulator serial output now logs SB/SC test-ROM output.

Still failing:

- `blargg/mem_timing/mem_timing.gb`
  - Output: `01:01  02:01  03:01`
  - Result: `Failed 3 tests.`
  - Meaning: Rockboy still does not model exact per-instruction memory access
    cycle timing.

## Acceptance Before Claiming Frame Accuracy

Required:

- `cpu_instrs.gb` passes.
- `instr_timing.gb` passes.
- `mem_timing.gb` passes.
- LCD visual reference tests such as DMG/CGB acid tests match reference output.
- A small ROM corpus still boots in simulator after accuracy changes.

Current status: not frame-accurate yet.
