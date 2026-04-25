# iPod Nano 3G OSOS Reduced Display Sequence

Date: 2026-04-24
Source artifact: `/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin`
Base address: `0x22000000`

## Goal

Reduce the higher-level OSOS display/backlight sequence down to direct hardware
operations that can run in a standalone DFU payload without relying on OSOS
runtime services.

## Input Sequence

The previously extracted OSOS visibility candidate was:

1. `0x2200374c`
2. `0x22004c5c`
3. `0x2200374c`
4. `0x22003774`

That sequence executes and causes DFU takeover when embedded into a payload, but
it has not produced visible LCD/backlight output.

## Call Reduction

### `0x2200374c`

Body-visible form:

```armasm
0x2200374c: ldr pc, [pc, #-4]
0x22003750: .word 0x080db704
```

Meaning:

- pure veneer into Apple ROM/service code at `0x080db704`
- no body-visible PMU, GPIO, MMIO, delay, or gate writes

### `0x22004c5c`

Body-visible form:

```armasm
0x22004c5c: push {r4, lr}
0x22004c60: mov  r4, r0
0x22004c64: add  r0, r0, #0x44
0x22004c68: bl   0x2200367c
0x22004c6c: mov  r0, #0
0x22004c70: strb r0, [r4, #5]
0x22004c74: add  r0, r4, #0x44
0x22004c78: pop  {r4, lr}
0x22004c7c: b    0x22003684
```

Directly visible operations:

- clear byte `[service + 0x05]`

Indirect operations:

- `0x2200367c -> 0x080646b4`
- `0x22003684 -> 0x08064790`

Meaning:

- the only body-visible state change is a service-object field write
- the hardware-facing behavior remains inside Apple ROM/service imports

### `0x22003774`

Body-visible form:

```armasm
0x22003774: ldr pc, [pc, #-4]
0x22003778: .word 0x080dbe58
```

Meaning:

- pure veneer into Apple ROM/service code at `0x080dbe58`
- no body-visible PMU, GPIO, MMIO, delay, or gate writes

## Wrapper Analysis

### `0x22005640`

The compact OSOS “on” wrapper is:

```armasm
0x22005644: bl 0x2200374c
0x22005648: bl 0x22003774
0x2200564c: bl 0x220073b4
0x22005650: mov r1, #1
0x22005654: bl 0x22007610
```

This does not reduce the path to raw hardware writes.

#### `0x220073b4`

`0x220073b4` performs lazy/runtime service setup and then returns a runtime
object pointer. It still depends on imported ROM/service helpers:

- `0x2200368c`
- `0x22003694`
- `0x2200369c`

It also calls `0x22007d94`, but no direct display-specific MMIO is exposed in
the visible body before returning the object at `0x22014318`.

#### `0x22007610`

`0x22007610` is also not a raw hardware helper:

```armasm
0x22007610: ldr r0, [r0, #0x20]
0x22007614: b   0x220038dc
```

and `0x220038dc` is another ROM veneer:

- `0x220038dc -> 0x081838??` (Apple ROM/service target)

So even the wrapper path remains service-mediated.

## Runtime/Service Dependency Boundary

The OSOS visibility path remains blocked behind multiple opaque runtime/service
layers:

| Visible call | ROM / service target | Body-visible hardware ops? | Notes |
| --- | --- | --- | --- |
| `0x2200374c` | `0x080db704` | No | service getter |
| `0x2200367c` | `0x080646b4` | No | first `0x22004c5c` subcall |
| `0x22003684` | `0x08064790` | No | second `0x22004c5c` subcall |
| `0x22003774` | `0x080dbe58` | No | “on” helper |
| `0x2200368c` | `0x082a424c` | No | lazy service setup |
| `0x22003694` | `0x082a40f8` | No | lazy service setup |
| `0x2200369c` | `0x082a4268` | No | lazy service setup |
| `0x220038dc` | `0x081838??` | No | object-method dispatch via `0x22007610` |

The only direct body-visible write in the minimal OSOS candidate is:

- `0x22004c70: strb r0, [r4, #5]`

That is service-object state, not display hardware.

## Raw Hardware Sequence

### Result

No evidence-backed raw PMU/GPIO/MMIO sequence can be extracted from this OSOS
path using the decrypted body alone.

### Why

1. The display-on path is expressed as service/ROM calls, not inline hardware
   programming.
2. The body-visible helper `0x22004c5c` exposes only a service-object byte
   clear.
3. The surrounding wrappers (`0x22005620`, `0x22005640`, `0x220073b4`,
   `0x22007610`) add more service indirections rather than revealing direct
   registers.
4. No Apple-backed reduction to PMU register writes, GPIO toggles, LCD MMIO, or
   explicit delays is visible before the path disappears into ROM.

## Minimal Direct Sequence

None, at present.

The exact raw sequence remains blocked behind Apple ROM/service code.

## Validation Decision

- direct Apple-backed raw reduction: **blocked**
- standalone raw payload without guessed registers: **not justified**

## Risk Assessment

| Option | Risk | Reason |
| --- | --- | --- |
| Reuse OSOS service calls | Medium | Apple-backed but runtime-dependent and not visibly successful yet |
| Invent raw PMU/GPIO/MMIO writes from this path | High | hardware behavior is hidden behind ROM, so direct-write reconstruction would be guesswork |

## Output Decision

- **STILL_BLOCKED**

Reason:

- OSOS provides a valid higher-level visibility call chain, but not the raw
  hardware operations behind it.
- The display-facing MMIO remains encapsulated in Apple ROM/service helpers not
  present in the decrypted body.

## Payload Decision

`lcd-osos-raw-visible-n3g.bin` was **not** prepared.

Preparing it would require inventing direct hardware writes that are not
actually visible in the Apple-backed body code, which would violate the current
evidence bar.

## ROM / Service Target Investigation

Follow-up investigation focused on the imported service targets used by the OSOS
display path:

- `0x080646b4`
- `0x08064790`
- `0x080db704`
- `0x080dbe58`

### Region Ownership

Current best classification:

- `0x08000000` region: **DRAM image/runtime code space**
- `0x22000000` region: IRAM image/runtime code space
- bootrom mapping: **`0x00000000` and `0x20000000`**, per local `wInd3x`
  documentation

This means the service targets above are **not** currently best explained as
BootROM addresses. They are more consistent with:

- DRAM-mapped Apple runtime code
- a runtime-loaded service module
- or another higher-level service image placed in DRAM

They are not directly visible in the decrypted OSOS body itself.

### Existing Artifact Search Result

Searches across:

- decrypted WTF body
- decrypted OSOS body
- original IPSW-extracted files
- local `wInd3x` source
- local freemyipod/Rockbox notes and S5L8702 headers

did **not** recover concrete bodies for these four service targets.

What was confirmed:

- the decrypted OSOS body contains only veneers/callers for these targets
- the targets are not present as in-body code inside the decrypted WTF/OSOS
  payloads already available locally
- no local note or source file currently provides a pre-existing Nano 3G dump
  of the `0x080...` service region

### Read-Only Dump Support

Local `wInd3x` source and README do confirm a documented memory-read primitive:

- `wInd3x dump [offset] [size] [file]`

The README explicitly states that `wInd3x` supports memory reads from a running
bootrom, with an example dump from `0x20000000`.

That is enough to justify a **read-only dump plan** for the relevant `0x080...`
service windows, but it was **not executed** in this step.

### Prepared-Only Dump Plan

Candidate narrow dump ranges:

1. `0x08064000` size `0x2000`
   - covers `0x080646b4`
   - proposed output: `/tmp/n3g-romsvc-08064000.bin`
2. `0x080db000` size `0x2000`
   - covers `0x080db704` and `0x080dbe58`
   - proposed output: `/tmp/n3g-romsvc-080db000.bin`

Prepared commands, not run:

```bash
/tmp/wInd3x/wInd3x dump 0x08064000 0x2000 /tmp/n3g-romsvc-08064000.bin
/tmp/wInd3x/wInd3x dump 0x080db000 0x2000 /tmp/n3g-romsvc-080db000.bin
```

### Decision

- **ROM_SERVICE_DUMP_POSSIBLE**

Reason:

- the service targets are not recoverable from the currently available decrypted
  bodies alone
- a documented read-only dump primitive exists locally
- the next evidence-backed step is to dump the narrow DRAM-backed service
  windows and disassemble them, not to guess hardware writes

## First Runtime Dump Attempt

A first read-only attempt to dump the `0x080...` service window was made on
real hardware from clean DFU:

```bash
/tmp/wInd3x/wInd3x dump 0x08064000 0x2000 /tmp/n3g-romsvc-08064000.bin
```

Observed result:

- DFU baseline before dump:
  - `05ac:1223`
  - DFU state `2`
- `wInd3x dump` reached the device and logged:
  - `Dumping... offset=134627328`
- the first `dumpmem` trigger then failed with:
  - `bug trigger: USB timeout error`
- resulting file:
  - `/tmp/n3g-romsvc-08064000.bin`
  - size `0`
- post-attempt DFU state fell back into the usual stale host-visible condition:
  - `mks5lboot --dfuscan` -> `LIBUSB_ERROR_OTHER`

### Interpretation

This does **not** show that the `0x080...` region is unreadable in principle.
It does show that, on this Nano 3G DFU session, the current `wInd3x dump`
primitive timed out before returning even the first `0x40` bytes from the
runtime-service window.

The local `wInd3x` documentation only demonstrates dumping from running bootrom
space (`0x20000000`), so current evidence is now:

- read-only dump support exists
- runtime-service dump from `0x08064000` did **not** succeed on the first
  practical attempt

### Updated Decision

- **ROM_SERVICE_DUMP_FAILED_WITH_REASON**

Reason:

- first `0x080...` dump attempt timed out at the exploit trigger stage before
  any block data was returned

## Dump Capability Diagnostics

To determine whether `wInd3x dump` was failing generally or only against the
runtime/service region, three tiny `0x40`-byte probes were tested.

### Probe 1: BootROM mirror at `0x20000000`

Command:

```bash
/tmp/wInd3x/wInd3x dump 0x20000000 0x40 /tmp/n3g-dump-test-20000000.bin
```

Result:

- success
- output size: `64` bytes
- contents are ARM-like, not all `0x00` / not all `0xff`

Leading bytes:

```text
2e 00 00 ea 64 f0 9f e5 64 f0 9f e5 ...
```

### Probe 2: BootROM alias at `0x00000000`

Command:

```bash
/tmp/wInd3x/wInd3x dump 0x00000000 0x40 /tmp/n3g-dump-test-00000000.bin
```

Result:

- success
- output size: `64` bytes
- contents match the `0x20000000` dump

### Probe 3: runtime/service window at `0x08064000`

Command:

```bash
/tmp/wInd3x/wInd3x dump 0x08064000 0x40 /tmp/n3g-dump-test-08064000.bin
```

Result:

- failed immediately
- error:
  - `bug trigger: USB timeout error`
- output size: `0`
- post-failure DFU check:
  - `LIBUSB_ERROR_OTHER`

### Diagnostic Decision

- **DUMP_080_SMALL_FAILS**

Meaning:

- `wInd3x dump` works in this Nano 3G context for the documented bootrom
  address aliases
- the same primitive fails immediately on the DRAM-backed runtime/service
  window at `0x08064000`
- current evidence supports “bootrom-accessible only” much more strongly than
  “general Nano 3G memory dump”

## Alternate Primitive Investigation

Follow-up static analysis checked whether `wInd3x` already contains another
Nano 3G-readable path for DRAM-backed runtime/service code.

### What `dumpmem` actually is

`dumpmem.Trigger()` is not a higher runtime service call. It:

1. cleans DFU state
2. places a small payload in the DFU buffer
3. executes it through the standard Nano 3G bootrom bug trigger
4. uses `HandlerFooter(addr)` to return `0x40` bytes from the requested address

On Nano 3G, `HandlerFooter(addr)` specifically:

- loads `addr` into `r0`
- sets `r1 = 0x40`
- calls bootrom helper `0x2000aa40`
- returns via bootrom address `0x200048d4`

So the current dump primitive runs in **bootrom DFU RCE context**, not in a
later OSOS/runtime context.

### Alternate readers checked

The following `wInd3x` paths were inspected:

- `cmd_spew.go` / `readFrom()`
- CP14 / CP15 read helpers
- NAND read helpers
- NOR read helpers

Result:

- they all use the same underlying DFU exploit model:
  - payload in IRAM / DFU buffer
  - return via `HandlerFooter(...)`
- none provide a separate already-implemented reader for the `0x080...`
  runtime/service space

### Payload-assisted copy assessment

A future read-only path is architecturally plausible:

- execute a tiny payload in IRAM
- copy a small `0x080...` range into `0x22000100`
- return that IRAM buffer via `HandlerFooter(0x22000100)`

Why this is plausible:

- `cmd_spew` already uses `0x22000100` as a scratch/return buffer
- NAND/NOR helpers also return data through that same readable IRAM area

Why it is **not** yet promoted:

- current evidence still does not prove that the bootrom-DFU execution context
  can safely dereference `0x080...`
- the failing `0x08064000` dump strongly suggests that the current context does
  not have straightforward access to that DRAM-backed runtime/service window

### Static artifact recovery

No alternate static recovery path was found in this pass:

- no relocation/copy-table evidence was recovered that directly reconstructs the
  missing service bodies from the decrypted OSOS/WTF artifacts already on disk
- the `0x080...` targets still appear only as veneers/call destinations

### Decision

- **CURRENTLY_BLOCKED_WITH_REASON**

Reason:

- no separate runtime-space dump primitive was found
- current `wInd3x` readers all stay in bootrom DFU exploit context
- payload-assisted copy is conceptually possible but not yet justified without
  evidence that `0x080...` is accessible from that context
