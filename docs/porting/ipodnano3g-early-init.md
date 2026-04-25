# iPod Nano 3G Early System Initialization

Date: 2026-04-24
Scope: reduce the earliest shared Apple-backed startup required before any
display/audio/input-specific logic

## Goal

Find the smallest Apple-backed early-system initialization that plausibly
enables basic peripheral output on Nano 3G after DFU payload takeover.

This investigation is motivated by two failures with otherwise valid payload
execution:

- LCD/backlight paths executed but remained black
- piezo GPIO path executed but produced no audible output

That shifts the likely blocker earlier than subsystem-local code and toward
global system state.

## Reset Entry

Decrypted OSOS body entry:

- `0x22000000 -> b 0x22008808`

Reset landing:

- `0x22008808`
  - loads stack pointer
  - branches to `0x220039c4`

So the practical OSOS reset path begins at:

- `0x220039c4`

## Earliest OSOS Startup Chain

Visible early sequence from `0x220039c4`:

1. `0x2200881c`
   - relocation delta helper
2. `0x22004778`
   - relocation/copy helper
3. `0x220045b0`
   - service registration/helper setup
4. `0x220044c4`
   - CP15/cache/memory setup
5. multiple service calls through:
   - `0x22003594`
   - `0x2200359c`
   - `0x220035a4`
   - `0x220035ac`
   - `0x220035b4`
   - `0x220035bc`
   - `0x220035c4`
   - `0x220035cc`
   - `0x220035d4`
   - `0x220035dc`
6. `0x22003ac4`
7. `0x22003b28`

This is the true earliest startup chain, but much of it is still service-heavy.
For a bounded probe, the best reducible block is not the reset entry itself but
a smaller shared wrapper below it.

## Best Shared Early-Init Candidate

The strongest bounded common-init wrapper found in OSOS is:

- `0x22002770`

Why this one:

- it runs before any LCD-local or audio-local logic
- it is global/system-oriented
- it avoids the explicit PMU write branch present in `0x22000510`
- its direct MMIO footprint is visible enough to document

Exact wrapper:

```text
0x22002770: bl 0x22003414
0x22002778: bl 0x22003150
0x2200277c: bl 0x22003138
0x22002780: mov r0, #1
0x22002784: bl 0x22002420
0x22002788: bl 0x22002d78
0x2200278c: mov r0, #3
0x22002790: bl 0x22002420
0x22002794: bl 0x220030fc
0x22002798: pop ...
0x2200279c: b 0x22003110
```

## What `0x22002770` Does

### CP15 control changes

From the visible local helpers:

- `0x22003150`
  - clear control-register bit `0x4`
- `0x22003138`
  - clear control-register bit `0x1000`
- `0x220030fc`
  - set control-register bit `0x1000`
- `0x22003110`
  - set control-register bit `0x4`

These are system-level CPU state changes, not LCD or audio operations.

### MIU/global memory programming via `0x22002420`

`0x22002420` uses:

- base `0x38100000` (`MIU_BASE` / `MIUCON` on S5L8702)

Observed direct writes for mode `1` / `4` path:

- `0x38100000`
  - `(old & 0x7) | 0x1000 | 0x8`
- `0x38100008`
  - computed refresh value
- `0x38100010`
  - `0x001fb621`
- `0x38100200`
  - `0x00001845`
- `0x38100204`
  - `0x00001845`
- `0x38100210`
  - `0x00001800`
- `0x38100214`
  - `0x00001800`
- `0x38100220`
  - `0x00001845`
- `0x38100224`
  - `0x00001845`
- `0x38100230`
  - `0x00001885`
- `0x38100234`
  - `0x00001885`
- `0x38100014`
  - `25`
- `0x38100018`
  - `25`
- `0x3810001c`
  - `0x0790682b`
- `0x38100314`
  - clear bit `0x10`
- loop over 35 registers starting at `0x3810002c`
  - clear bit `0x01000000`
- `0x381001cc`
  - `0x540`
- `0x381001d4`
  - set bit `0x80`
- repeated command strobes through `0x38100004` / `0x3810000c`
- final `0x38100008 |= 0x61000`

Mode `3` path inside the same helper:

- clear bit `0x00100000` in `0x38100000`

This is broad memory-interface initialization, not subsystem-local pinmux.

### Clock/reset-style programming via `0x22002d78`

`0x22002d78` uses:

- `0x3c500000` (`CLK_BASE`)
- `0x39900000` (`DMA1_BASE` in local headers)

Direct writes observed:

- save:
  - `0x3c500000`
  - `0x3c500004`
  - `0x3c500008`
  - `0x3c50000c`
  - `0x3c500010`
  - `0x3c500014`
  - `0x3c500044`
  - `0x3c500060`
  - `0x3c500048`
  - `0x3c50004c`
  - `0x39900000`
- clear bits `0x600` in `0x39900000`
- write `0x3c500048 = 0xffffffe3`
- write `0x3c50004c = 0xffffeff7`
- write `0x3c500014 = 0x8000`
- write a repeated derived value into:
  - `0x3c500010`
  - `0x3c50000c`
  - `0x3c500008`
  - `0x3c500000`
- write `0x3c500044 = 0x100`
- set `0x3f00000` and bit `0` in `0x39900000`
- poll bit `0` in `0x39900000`
- issue cache/CP15 maintenance
- set bits `0x1000` and `0x2` in `0x39900000`
- poll bit `0x1000`
- clear bit `0` and clear `0x3f00000`
- restore the saved `0x3c5000xx` registers
- restore the saved `0x39900000`

This is the clearest shared global clock/reset-style block found so far.

## Remaining Service Dependencies

The reduced early-init candidate is not fully raw.

Opaque imports still used by `0x22002770` and its helpers:

| Veneer | Target | Role |
|---|---:|---|
| `0x22003414` | `0x08016234` | front-edge service call before CP15/MIU init |
| `0x220034bc` | `0x0801542c` | helper used by `0x22002420` while computing MIU timing |

These remain outside the decrypted OSOS body.

## Import Classification

### `0x22003414 -> 0x08016234`

Observed call pattern:

- called with no explicit argument setup in:
  - `0x22000510`
  - `0x22002770`
- return value is ignored at both call sites
- always appears as the first operation in the wrapper

Current classification:

- unknown front-edge service hook / init barrier

Why it matters:

- only local work before this call is the wrapper prologue
- so no meaningful early-init progress occurs unless this import returns

### `0x220034bc -> 0x0801542c`

Observed call pattern:

- takes arguments in `r0` / `r1`
- returns a value in `r0`
- result is immediately consumed as arithmetic/timing input
- repeated call sites use it like a pure helper:
  - `0x2200246c`
  - `0x22002120`
  - `0x22002ae0`
  - `0x22006248`
  - `0x220063a0`

Current classification:

- arithmetic / ratio / timing helper, not a direct MMIO or PMU service call

Why it matters:

- `0x22002420(1)` depends on it before continuing with MIU programming
- `0x22002420(3)` does not

## Split of `0x22002770`

The wrapper now reduces into:

1. local prologue
2. ROM-dependent front-edge call:
   - `0x22003414`
3. local CP15 state clear:
   - `0x22003150`
   - `0x22003138`
4. mixed local+ROM MIU phase:
   - `0x22002420(1)`
   - depends on `0x220034bc`
5. local clock/reset phase:
   - `0x22002d78`
6. local MIU follow-up:
   - `0x22002420(3)`
   - no visible `0x220034bc` dependency
7. local CP15 restore:
   - `0x220030fc`
   - `0x22003110`

## Minimal Subset Decision

### Fully raw subset

Blocked.

Reason:

- the best early common-init block still contains two unresolved ROM/runtime
  service imports
- therefore a fully raw “direct writes only” minimal sequence is not yet
  honestly justified

### Best bounded probe subset

Use the Apple-authored wrapper:

- `0x22002770`

Rationale:

- earliest bounded common-init candidate
- no LCD logic
- no audio logic
- no NAND or USB PHY logic
- narrower than the full reset chain at `0x220039c4`

## Prepared Probe

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`

Behavior:

1. enter at `0x22000000`
2. set a local stack
3. call embedded OSOS wrapper `0x22002770`
4. loop forever

Important caveat:

- this is a bounded **call-level** early-init probe, not a fully raw MMIO-only
  reduction
- its success still depends on the unresolved imported targets above

## Host-side Validation

Build command:

- `make -C tools/ipodnano3g/minimal_payload TARGET=system-init-probe-n3g LDSCRIPT=system-init-probe-n3g.lds`

Validated artifacts:

- `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`
- `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.elf`
- `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.map`

Observed properties:

- ELF entrypoint: `0x22000000`
- flat binary size: about `14 KiB`
- `_start` only:
  - sets `sp = 0x2200fff0`
  - calls `0x22002770`
  - loops forever
- embedded code slices are linked back at their original OSOS VMAs:
  - `0x22001f00`
  - `0x22002770`
  - `0x220030d4`
  - `0x220033f0`

## Observable Probe

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-observable-n3g.bin`
- `tools/ipodnano3g/minimal_payload/system-init-skiprom-n3g.bin`
- `tools/ipodnano3g/minimal_payload/system-init-1call-n3g.bin`

Behavior:

1. enter at `0x22000000`
2. set a local stack
3. call embedded OSOS wrapper `0x22002770`
4. run two software-only delay phases
5. branch to `0xdead0000` to force an observable reset/crash

Purpose:

- distinguish:
  - immediate crash inside early init
  - successful return from early init followed by delayed fault

No new hardware writes are added beyond the original early-init wrapper.

Host-side validation:

- build command:
  - `make -C tools/ipodnano3g/minimal_payload TARGET=system-init-observable-n3g LDSCRIPT=system-init-observable-n3g.lds`
- ELF entrypoint:
  - `0x22000000`
- flat binary size:
  - about `14 KiB`
- `_start` sequence:
  - set `sp = 0x2200fff0`
  - call `0x22002770`
  - call software delay
  - call software delay
  - branch to `0xdead0000`

Control variants:

- `system-init-skiprom-n3g.bin`
  - no ROM or OSOS init call
  - two software delays
  - branch to `0xdead0000`
- `system-init-1call-n3g.bin`
  - call only `0x22003414`
  - two software delays
  - branch to `0xdead0000`

Validation:

- `system-init-skiprom-n3g.bin`
  - size `40` bytes
  - `_start` only delays twice and faults
- `system-init-1call-n3g.bin`
  - size about `14 KiB`
  - `_start` calls only `0x22003414`, then delays twice and faults

## Risk

- Risk level: medium-high

Why:

- no LCD
- no audio
- no NAND
- no USB PHY
- but the path does touch:
  - `0x38100000` memory-interface state
  - `0x3c500000` clock/reset state
  - `0x39900000` global controller state
  - CP15 control bits

## Current Conclusion

- The strongest missing common dependency is early global system initialization,
  not a remaining LCD-only or piezo-only detail.
- The best current probe entry is `0x22002770`.
- `0x22003414` appears required just to enter the wrapper meaningfully.
- `0x220034bc` appears required for the first MIU-init phase but not for the
  later `mode 3` follow-up.
- A fully raw sequence is still blocked by unresolved ROM/runtime imports.
- A bounded Apple call-level early-init probe can still be prepared safely
  enough for later explicit approval.

## Decision

- **ROM_IMPORT_REQUIRED_FOR_INIT**

Reason:

- the first meaningful step in `0x22002770` is already the `0x22003414`
  import
- the first MIU-init phase inside the wrapper depends on `0x220034bc`
- so the wrapper cannot be reduced to a meaningful working early-init path
  without those ROM/runtime targets
