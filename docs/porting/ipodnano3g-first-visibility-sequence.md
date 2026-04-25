# iPod Nano 3G First Visibility Sequence

Date: 2026-04-23
Status: LCD mode reduced; full cold-init chain **partially unresolved**

## Scope

Extract the earliest Apple-backed LCD bring-up chain that appears to be required
before a panel can respond visibly.

## Current Finding

The short awake sequence alone is not enough.

The no-output result from the `lcd-awake-n3g` test is consistent with the
decrypted WTF structure:

- `0x45bc` controller preamble is real
- command-mode selection is real
- `0x11`, delay, `0x13`, `0x29` is real
- but Apple also uses:
  - earlier LCD-entry callbacks before `0x45bc`
  - panel-group init helpers after `0x45bc`
  - larger panel-init tables for at least some panel groups

## Earliest Confirmed LCD Entry Path

The earliest confirmed caller into the direct LCD setup path is inside the
state machine at `0x220031b4`.

The LCD-related branch is:

1. `0x22003278`: `0x22003ce0(1)`
2. `0x22003284`: `0x22003ce0(4)`
3. `0x22003288`: `0x22003d14(0x22007338)`
4. `0x22003290`: `0x22003cc0()`
5. `0x22003294`: `0x2200455c()`

Evidence:

- `0x220031b4`
- `0x22003278..0x22003294`

Confidence:

- call order: High
- exact semantics of steps 1-4: Low-medium

## Runtime Callback Object at `0x22007398`

Steps 1-4 above are wrappers around a runtime callback-object pointer at
`0x22007398`.

Static-image status:

- `0x22007398 = 0x00000000`
- `0x2200739c = 0x00000000`
- `0x220073a0 = 0x00000000`
- `0x220073a4 = 0xad55ffff`
- `0x220073a8 = 0x000052aa`

No in-image write to `0x22007398` was found in the decrypted WTF body. The
pointer therefore appears to be provided by an earlier runtime phase outside
the static WTF body.

### Runtime Entry Evidence

The actual WTF reset entry is `0x22001420`.

Confirmed startup behavior from `0x22001420..0x220014c8`:

- local setup calls:
  - `0x22001124`
  - `0x220010f8`
  - `0x220010e4`
  - `0x22001394`
  - `0x220013fc`
- CPSR mode switches and stack setup:
  - IRQ stack from `0x2203fbfc`
  - SVC stack from `0x2203f7fc`
- image/data copy and zeroing:
  - copy range seeded from `0x22000000`
  - zero/init range extends through `0x22007810`
- handoff to `0x22002f20`
- then `svc 0x00123456` with:
  - `r0 = 24`
  - `r1 = 0x00020026`

Important negative finding:

- this entry path does **not** preserve incoming `r0`/`r1`/`r2`/`r3` into
  globals or service pointers before clobbering them
- there is therefore no evidence in the static WTF body that `0x22007398` is
  copied directly from bootrom-provided argument registers at entry

Additional runtime evidence:

- `0x220014f8` traps `svc 0x00123456`
- other WTF code also invokes the same service ABI, for example:
  - `0x22000484`
  - `0x22000640`
  - `0x220013ac`
  - `0x220014c4`

Current interpretation:

- `0x22007398` is more likely part of an external runtime/loader service layer
  than a plain pointer handed in once through entry registers
- the exact provider is still not proven:
  - bootrom runtime
  - DFU/runtime shim
  - loader-installed service object

Confidence:

- entrypoint `0x22001420`: High
- no direct register-to-global handoff for `0x22007398`: High
- external runtime/loader service interpretation: Medium

### Confirmed Slot Layout

- `0x22003ce0` calls object method at offset `+0x04`
- `0x22003d14` calls object method at offset `+0x08`
- `0x22003c98` calls object method at offset `+0x28`
- `0x22003cc0` calls object method at offset `+0x2c`

### Observed Argument Patterns

- slot `+0x04` receives mode values `1`, `3`, `4`
- slot `+0x08` receives pointer `0x22007338`
- slot `+0x28` receives pointer `0x22007338`
- slot `+0x2c` is called with no explicit arguments

### Wrapper ABI Reduction

Observed wrappers:

- `0x22003c98`
  - input: caller-provided `r0`
  - behavior:
    - if `0x22007398 == 0`, returns `0x11`
    - else loads `[table + 0x28]` and tail-calls it with the original `r0`
  - wrapper return value: callback return forwarded unchanged
- `0x22003cc0`
  - input: no explicit argument setup
  - behavior:
    - if `0x22007398 == 0`, returns `0x11`
    - else loads `[table + 0x2c]` and tail-calls it
  - wrapper return value: callback return forwarded unchanged
- `0x22003ce0`
  - input: caller-provided `r0`
  - behavior:
    - if `0x22007398 == 0`, returns `0x11`
    - else loads `[table + 0x04]`, calls it with the original `r0`, then
      forces wrapper return to `0`
  - wrapper return value: `0` on present table, callback result ignored
- `0x22003d14`
  - input: caller-provided `r0`
  - behavior:
    - if `0x22007398 == 0`, returns `0x11`
    - else loads `[table + 0x08]` and tail-calls it with the original `r0`
  - wrapper return value: callback return forwarded unchanged

Call-site reduction:

- all four wrappers are only referenced from the state machine around
  `0x220031b4`
- the LCD path does not branch on the returned values from:
  - `0x22003c98`
  - `0x22003cc0`
  - `0x22003ce0`
  - `0x22003d14`
- this means the minimum control-flow requirement is simply that the table
  exists and the callbacks return without faulting

### Classification

| Slot | Wrapper | Observed caller use | Classification | Confidence |
|---|---|---|---|---|
| `+0x04` | `0x22003ce0` | mode values `1`, `3`, `4` in the LCD state-machine path | mode/state transition hook | Medium |
| `+0x08` | `0x22003d14` | pointer `0x22007338` immediately before `0x22003cc0()` and `0x2200455c()` | context handoff / registration hook | Medium |
| `+0x28` | `0x22003c98` | pointer `0x22007338` at earlier LCD-entry points | setup/query/open hook | Medium |
| `+0x2c` | `0x22003cc0` | no-argument call immediately before `0x2200455c()` | finalize/commit/start hook | Low-medium |

`0x22007338` itself is a zeroed static block in the WTF body, so it looks more
like service-owned context storage than a direct MMIO descriptor.

This external callback service is the main remaining blocker for a fully raw
register-by-register cold-init payload.

### Bypass Risk By Slot

| Slot | Likely role | Bypass risk | Reason |
|---|---|---|---|
| `+0x04` | mode/state transition | unsafe to omit | called with staged values `1`, `3`, `4`; likely resource or subsystem state sequencing |
| `+0x08` | context registration / handoff | likely required | receives `0x22007338` immediately before the direct LCD path |
| `+0x28` | earlier setup/open/query | likely required | receives the same context pointer at earlier LCD entry points |
| `+0x2c` | finalize / commit / start | unknown, treat as required | last callback before `0x2200455c()` |

Current bypass decision:

- the later direct LCD path is **not** yet safe to treat as self-sufficient
- omitting the callbacks would still be speculative

### Minimal Stub Table Design

The smallest non-faulting table consistent with the wrappers is:

```c
struct n3g_runtime_service_stub {
    uint32_t reserved_00;          // +0x00
    uint32_t (*mode_hook)(uint32_t mode);   // +0x04
    uint32_t (*bind_ctx)(void *ctx);        // +0x08
    uint32_t reserved_0c[7];       // +0x0c .. +0x27
    uint32_t (*open_ctx)(void *ctx);        // +0x28
    uint32_t (*commit)(void);               // +0x2c
};
```

Evidence-backed stub behavior:

- `mode_hook`: return `0`
- `bind_ctx`: return `0`
- `open_ctx`: return `0`
- `commit`: return `0`

Why this is the minimum defensible stub:

- the wrappers only dereference these four offsets
- the LCD state path does not inspect their return values for branching
- `0x22007338` is not referenced later by the direct LCD code, so the pointer
  can remain as inert context storage for now

Limits:

- this only satisfies the observed control-flow ABI
- it does **not** prove that the missing runtime service has no hidden hardware
  side effects
- the table is therefore a useful reconstruction target, but not yet a proven
  substitute for the original runtime environment

## Directly Resolved LCD Steps

### Step 1: Resource/Gate Wrapper

`0x2200455c` begins by calling `0x2200428c`, which calls `0x22000664`.

`0x22000664` directly updates:

- `0x3c500048`
- `0x3c50004c`

Exact LCD-local call:

- `0x220042ac`: `0x22000664(r0=2, r1=0, r2=1)`
- because `r2 != 0`, this **clears** bit `1` in `0x3c500048`
- `0x220042b8` later restores the prior state using the saved return from
  `0x22003b18`

Interpretation:

- Apple is enabling a required precondition outside the LCD block before
  touching `0x38300000`.

Confidence: Medium

### Step 2: LCD Controller Preamble

From `0x220045bc`:

- `0x38300000 = 0x80000000`
- `0x38300000 = 0x80100db1`
- `0x38300088 = 0x01000000`
- `0x38300020 = 0x00000033`
- `0x3830007c = 0x00000804`

Confidence: High

### Step 3: Panel-Group Table Selection

`0x22004598` caches the selected panel-group table into `0x2200739c + 0x4`.

Table selection from `0x22004500`:

- selector `0` -> `0x2200730c`
- selector `1` -> `0x220072bc`
- selector `2` or `3` -> `0x220072e4`

Selector source:

- GPIO52 / GPIO53 through `0x22004438` and `0x22004f58`

Confidence: High

### Step 4: Resource/Gate Restore

`0x220042b8` restores the prior resource state through `0x22000664`.

Confidence: Medium

## Command-Mode Selection

The command-mode helper at `0x220042c8` selects between two low-bit patterns
using GPIO52 / GPIO53 straps:

- selector `0` / `1` -> `0x0c20`
- selector `2` / `3` -> `0x0da8`

After the Apple preamble value `0x80100db1`, the exact config writes become:

- `0x80000c21`
- `0x80000da9`

This matches the in-tree Rockbox controller constants:

- `LCD_MODE_P8  = 0x80000c20`
- `LCD_MODE_P18 = 0x80000da8`

Confidence: High

## Panel-Specific Init Dispatch

The larger panel init is dispatched through `0x220048bc`, not through `0x45bc`.

`0x220048bc`:

- reads the cached panel-group table from `0x2200739c + 0x4`
- calls the table’s init helper at offset `+0x8`

Helpers by panel group:

- selector `0` -> `0x22004968`
- selector `1` -> `0x22004a50`
- selector `2` / `3` -> `0x220049a8`

This is the strongest firmware-backed explanation for why the short awake-only
payload produced no visible output.

Confidence: High

Helper behavior:

- `0x22004968` handles selector `0`
  - mode `1` / `4`
  - 8-bit table family at `0x728b` / `0x729f`
- `0x22004a50` handles selector `1`
  - mode `1` / `4`
  - large 8-bit table family at `0x7018`
  - short awake table at `0x70d1`
- `0x220049a8` handles selector `2` / `3`
  - mode `1` / `4`
  - large 16-bit table family at `0x70dc`
  - includes an extra pre-table control pulse through `0x22004f88(0x36, ...)`

Confidence:

- helper-to-table mapping: High
- exact hardware meaning of the extra `0x22004f88(0x36, ...)` pulse:
  Medium

## Decoded Apple Panel Tables

### Group 0: Helper `0x22004968`

Mode `4` table:

- body offset `0x728b`
- length `0x14`
- decoded:
  - `CMD8 0x11`
  - `DELAY 0x78`
  - `CMD8 0x35, 0x00`
  - `CMD8 0x3a, 0x06`
  - `OP1`
  - `CMD8 0x13`
  - `CMD8 0x29`

Sleep table:

- body offset `0x729f`
- length `0x0a`
- decoded:
  - `CMD8 0x28`
  - `DELAY 0x32`
  - `CMD8 0x10`
  - `DELAY 0x32`

Confidence: High for table decode

### Group 1: Helper `0x22004a50`

Mode `4` table:

- body offset `0x70d1`
- length `0x0b`
- decoded:
  - `CMD8 0x11`
  - `DELAY 0x3c`
  - `CMD8 0x13`
  - `CMD8 0x29`

Mode `1` table:

- body offset `0x7018`
- length `0x0b9`
- decoded commands include:
  - `CMD8 0xb0` block, len `0x15`
  - `CMD8 0xb8 = 0xd8`
  - `CMD8 0xb1` block, len `0x1e`
  - `CMD8 0xd2 = 0x01`
  - gamma/programming blocks `0xe0`..`0xe5`
  - `CMD8 0x3a = 0x06`
  - `CMD8 0xc2 = 0x00`
  - `CMD8 0x35 = 0x00`
  - `OP1`
  - awake tail:
    - `CMD8 0x11`
    - `DELAY 0x3c`
    - `CMD8 0x13`
    - `CMD8 0x29`

This is the clearest Apple-backed example of a fuller LCD init sequence ending
in the short awake tail.

Confidence: High

### Group 2/3: Helper `0x220049a8`

Mode `4` table:

- body offset `0x70dc`
- length `0x179`
- decoded as a large 16-bit init sequence, including:
  - setup block `0x0010`..`0x0018`
  - multiple gamma/programming blocks `0x0300`..`0x032d`
  - `0x0400 = 0x001d`
  - `0x0401 = 0x0001`
  - power/display block:
    - `0x0100 = 0x17b0`
    - `0x0101 = 0x0220`
    - `0x0102 = 0x00bd`
    - `0x0103 = 0x1500`
    - `0x0105 = 0x0103`
    - `0x0106 = 0x0105`
  - final enable tail:
    - `0x0007 = 0x0021`
    - `OP1`
    - `0x0002 = 0x0500`
    - `0x0007 = 0x0031`
    - `0x0030 = 0x0007`
    - `DELAY 0x1e`
    - `0x0030 = 0x03ff`
    - `DELAY 0x3c`
    - `0x0007 = 0x0072`
    - `DELAY 0x96`
    - `0x0007 = 0x0173`

This group clearly needs much more than the short awake sequence.

Confidence: High

## Ordered Sequence Supported Today

1. Runtime callback stage:
   - `0x22003c98(0x22007338)` via object slot `+0x28`
   - `0x22003ce0(1)` via object slot `+0x04`
   - `0x22003ce0(4)` via object slot `+0x04`
   - `0x22003d14(0x22007338)` via object slot `+0x08`
   - `0x22003cc0()` via object slot `+0x2c`
   - Confidence: Medium for “required”, Low-medium for semantics
2. Resource/gate enable through `0x2200428c` -> `0x22000664`
   - direct writes in `0x3c500048/0x4c`
   - Confidence: Medium
3. LCD controller preamble at `0x220045bc`
   - Confidence: High
4. GPIO52/GPIO53 panel-group selection
   - Confidence: High
5. Panel-group init helper dispatch through `0x220048bc`
   - Confidence: High
6. Short awake tail only after the appropriate panel-group init
   - Confidence: High

## Earlier Platform Preconditions Now Confirmed

The first `lcd-fullinit-n3g` hardware run showed that the embedded Apple LCD
path can execute without producing visible output. That shifts the main blocker
earlier than `0x2200455c`: some platform state is still missing before the LCD
controller code becomes effective.

The best current Apple-backed prerequisite chain is:

1. PMU-side bring-up hook(s)
2. earlier clock/resource gates outside the LCD-local `0x2200428c` wrapper
3. GPIO / pin-configuration staging
4. only then the LCD-local gate wrapper, controller preamble, mode select, and
   panel tables

This section records only the steps with direct decrypted-firmware evidence.

### PMU / I2C Prerequisites

Confirmed Apple helpers:

- `0x22005420`: PMU read helper using slave `0x73`
- `0x22005474`: PMU write helper using slave `0x73`
- both use the same I2C path through `0x22005258`

Confirmed PMU register operations before the LCD state path:

| Function | PMU reg | Value / operation | Evidence | Risk |
|---|---:|---|---|---|
| `0x220054b0` | `0x1d` | write `0x0a` | `0x220054b8..0x220054cc` | Medium |
| `0x220054b0` | `0x1b` | write `0x01` when input nonzero, else `0x00` | `0x220054d8..0x220054f0` | Medium |
| `0x220054f8` | `0x43` | read-modify-write bit `0` | `0x22005500..0x22005538` | Medium |

Confirmed callers:

- `0x22003018` wraps `0x220054b0`
- `0x2200304c` wraps `0x220054f8`
- `0x22003078` calls both when input equals `1`
- `0x22003088` / `0x22003090` are then reached from the higher-level startup
  path

Current interpretation:

- PMU work is definitely part of the earlier platform sequence.
- It is not yet reduced to the minimal subset required specifically for first
  LCD visibility.
- No Apple-backed `LEDCTL (0x20)` backlight write has been found yet, so PMU
  evidence still supports “panel power / platform power preconditions” more
  strongly than “direct backlight enable.”

### Earlier Clock / Resource Gates

The LCD-local wrapper is not the only gate sequence. Earlier startup code calls
the same resource helper before reaching the LCD path.

Confirmed gate helper:

- `0x22000664`: read-modify-write helper for:
  - `0x3c500048`
  - `0x3c50004c`

Confirmed earlier-than-LCD calls:

| Call site | `r0` | `r1` | `r2` | Evidence | Risk |
|---|---:|---:|---:|---|---|
| `0x22001630` | `0x10000` | `0x0` | `1` | `0x22001624..0x22001630` | Medium |
| `0x22001650` | `0x10000` | `0x0` | `0` | `0x22001644..0x22001650` | Medium |
| `0x220018d8` | `0x400` | `0x0` | `1` | `0x220018cc..0x220018d8` | Medium |
| `0x220018e8` | `0x1` | `0x0` | `1` | `0x220018dc..0x220018e8` | Medium |
| `0x220018fc` | `0x0` | `0x2000` | `1` | `0x220018ec..0x220018fc` | Medium |
| `0x2200205c` | `0x2007df65` | `0x1ef49` | `0` | `0x22002050..0x2200205c` | Medium-high |
| `0x2200206c` | `0x06002082` | `0x1036` | `1` | `0x22002060..0x2200206c` | Medium-high |

Additional direct gate-like setup:

- `0x22003c18` is called at `0x22001f54` before the larger GPIO/config block
- `0x22001634..0x22001640` clears bits `0..2` at `0x3930003c`
- `0x220017e8..0x22001804` toggles bit `1` at `0x38400804` with Apple delay
  helpers between writes

Current interpretation:

- Some LCD prerequisite clocks/resources are probably enabled before the
  LCD-local gate wrapper ever runs.
- The low-mask `0x10000`, `0x400`, `0x1`, `0x2000` calls are stronger
  candidates for “minimal platform prerequisite” than the large mixed masks in
  `0x2200205c/0x2200206c`, but that reduction is not finished yet.

### GPIO / Pin Configuration Stage

There is also confirmed Apple GPIO preparation before the LCD path, but the
current evidence splits it into two different kinds of code:

- broader early platform clock/reset programming in `0x22001f4c`
- later GPIO state save/restore and GPIO command writes closer to the LCD path

Confirmed pieces:

- `0x22001f4c` performs a larger block of early platform setup before later LCD
  execution:
  - `0x22000958`
  - `0x22003890`
  - `0x220039fc`
  - `0x22003350`
- `0x22003130(0x2200791c)` snapshots the live `0x3cf00000` GPIO descriptor
  block into a RAM buffer
- `0x220030f0(0x2200791c)` restores that RAM-buffered descriptor block back to
  the `0x3cf00000` GPIO area
- `0x220061f4()` is a second pre-LCD helper in the state machine, but its
  strongest currently observed path is no longer clearly LCD-specific:
  - the LCD-adjacent state uses `0x220061f4(3)`
  - that path drops into `0x22005ef4` / `0x220060e0`
  - those routines operate on `0x3c200000`
  - and manipulate pins `72..75` via `0x22004f0c`

Relevant evidence:

- `0x22001f4c..0x22002070`
- `0x220030f0..0x220031ac`
- `0x220061f4..0x2200625c`
- `0x22004f0c..0x22004f54`
- `0x22005ef4..0x220060e0`
- earlier LCD mode-family selection still uses GPIO52/GPIO53 via:
  - `0x22004438`
  - `0x22004f58`

Current interpretation:

- `0x22001f4c` now looks more like early clock/reset programming than simple
  pinmux.
- `0x220030f0` / `0x22003130` are not defining a new LCD pin layout from
  literals; they are moving a 16-entry GPIO descriptor set between RAM
  (`0x2200791c`) and the live GPIO bank (`0x3cf00000`).
- `0x2200791c` is outside the decrypted WTF body and lies in the zero-padded
  work-buffer range used by the embedded payload, which explains why replaying
  the LCD-local code alone may still miss earlier GPIO state.
- That dependency is now stronger than before:
  - Apple state `4/5` calls `0x220030f0(0x2200791c)` before the LCD-local
    wrapper path
  - the only in-body producer of that buffer is the earlier state `1/2` call to
    `0x22003130(0x2200791c)`
  - so a standalone payload that tries to reproduce Apple's state `5` cold-init
    path either needs a valid synthesized descriptor buffer or must justify a
    state-machine bypass
- `0x220061f4` is still on the pre-LCD state path, but it is no longer a clean
  LCD candidate:
  - the actually observed LCD-adjacent call is `0x220061f4(3)`
  - that path reaches the `0x3c200000` peripheral region
  - the local triage notes classify `0x3c200000` as
    `WHEEL_OR_NAND_COLLISION`
  - so pins `72..75` should currently be treated as **unsafe / unrelated until
    proven otherwise**, not as a narrowed LCD-reset candidate

## Dependency Matrix

This matrix reduces the currently observed pre-LCD chain into concrete
operations, with omission status kept conservative.

| Operation | Firmware function | Register / address / value | Category | Evidence | Risk | Can omit? |
|---|---|---|---|---|---|---|
| PMU write reg `0x1d = 0x0a` | `0x220054b0` via `0x22003018` | PMU slave `0x73`, reg `0x1d`, value `0x0a` | PMU | High | Medium | Exclude for now; only observed on `0x22003078(1)`, not state `5/6` |
| PMU write reg `0x1b = 0x01/0x00` | `0x220054b0` via `0x22003018` | PMU slave `0x73`, reg `0x1b`, input-dependent | PMU | High | Medium | Exclude for now; only observed on `0x22003078(1)`, not state `5/6` |
| PMU reg `0x43` bit `0` RMW | `0x220054f8` via `0x2200304c` | PMU slave `0x73`, reg `0x43`, set/clear bit `0` | PMU / reset? | High | Medium | Exclude for now; only observed on `0x22003078(1)`, not state `5/6` |
| Early gate RMW | `0x22000664` at `0x22001630` | `0x3c500048 &= ~0x10000` | Clock / gate | High | Medium | Include; immediate startup path `0x2200160c` |
| Early gate RMW | `0x22000664` at `0x22001650` | `0x3c500048 |= 0x10000` | Clock / gate | High | Medium | Include; immediate startup path `0x2200160c` |
| Early gate RMW | `0x22000664` at `0x220018d8` | `0x3c500048 &= ~0x400` | Clock / gate | High | Medium | Exclude; appears in later `0x22001698` path, not immediate LCD startup |
| Early gate RMW | `0x22000664` at `0x220018e8` | `0x3c500048 &= ~0x1` | Clock / gate | High | Medium | Exclude; appears in later `0x22001698` path, not immediate LCD startup |
| Early gate RMW | `0x22000664` at `0x220018fc` | `0x3c50004c &= ~0x2000` | Clock / gate | High | Medium | Exclude; appears in later `0x22001698` path, not immediate LCD startup |
| Broad gate / mux RMW | `0x22000664` at `0x2200205c` | `0x3c500048 |= 0x2007df65`, `0x3c50004c |= 0x1ef49` | Clock / gate | High | Medium-high | Include at call level; direct `0x22001f4c` startup path, but helper meaning still broad |
| Broad gate / mux RMW | `0x22000664` at `0x2200206c` | `0x3c500048 &= ~0x06002082`, `0x3c50004c &= ~0x1036` | Clock / gate | High | Medium-high | Include at call level; direct `0x22001f4c` startup path, but helper meaning still broad |
| Early direct gate RMW | `0x22003c18(0)` via `0x22001f54` | clear `0x3c500000 bit 0x8000`, clear paired register bit `0x200` | Clock / reset | High | Medium | Include at call level; direct `0x22001f4c` startup path |
| Clock/reset bit clear | inline at `0x22001634..0x22001640` | `0x3930003c &= ~0x7` | Reset / mode | High | Medium | Include; immediate startup path `0x2200160c` |
| Toggle bit with delay | inline at `0x220017e8..0x22001804` | `0x38400804 bit1` set, delay, clear | Reset / pulse | High | Medium | Exclude; appears in later `0x22001698` path |
| Early platform config helper set | `0x22001f4c` | calls `0x22003c18`, `0x22000958`, `0x22003890`, `0x220039fc`, final `0x22003c18(1)` | Clock / reset / pin block | High | Medium-high | Include as the corrected full block; no longer depends on excluded PMU or sideband GPIO paths |
| GPIO snapshot | `0x22003130(0x2200791c)` | convert live `PCON/PDAT/PUNB/PUNC` for 16 groups into 8-byte RAM descriptors | GPIO state capture | High | Low | Include only if reproducing Apple state `1/2`; producer for later restore |
| GPIO restore | `0x220030f0(0x2200791c)` | restore 16 RAM descriptors back to live `PCON/PUNB/PUNC` at `0x3cf00000` | GPIO state restore | High | Low | Bypass-safe if `0x220061f4` is omitted; otherwise unresolved without valid buffer contents |
| Sideband GPIO/peripheral sequence | `0x220061f4(3)` -> `0x22005ef4` / `0x220060e0` | touches `0x3c200000` and configures pins `72..75` through `0x3cf00200` | Sideband / unknown | Medium-high | High | No for next LCD payload |
| Runtime callback stage | `0x22003c98`, `0x22003ce0`, `0x22003d14`, `0x22003cc0` | object at `0x22007398` | Service / orchestration | High | Low | No for Apple path |
| LCD-local gate wrapper | `0x2200428c` -> `0x22000664` | clear bit `1` in `0x3c500048`, then restore previous state | LCD-local gate | High | Medium | No for Apple path |
| LCD preamble and panel tables | `0x2200455c`, `0x220048bc`, tables | `0x38300000` block and related LCD paths | LCD-local | High | Medium | No |

## Proposed Minimal Sequence Today

This is the smallest ordered sequence that is currently defendable without
guessing.

1. Immediate startup gate work
   - corrected standalone candidate:
     - `0x22001f4c` full block
     - `0x22000664(0x10000, 0, 1)`
     - `0x3930003c &= ~0x7`
     - `0x22000664(0x10000, 0, 0)`
   - this keeps only the direct startup work and omits the later sideband
     `0x220061f4` / `0x2200791c` state-machine path
2. Any required GPIO / descriptor state
   - descriptor format is now explicit:
     - 16 entries
     - 8 bytes per entry
     - entry `+0x00..+0x03`: packed `PCON(group)` nibble config, with any
       live output nibble normalized from `1` to `0xE`/`0xF` using `PDAT(group)`
     - entry `+0x04`: `PUNB(group)` low byte
     - entry `+0x05`: `PUNC(group)` low byte
     - entry `+0x06..+0x07`: unused / untouched
   - Apple state `4/5` restores `0x2200791c` through `0x220030f0`
   - the only in-body producer is state `1/2` through `0x22003130`
   - the strongest current interpretation is that this is generic GPIO state
     preservation around the `0x220061f4` sideband helper, not an LCD-local
     input to `0x2200455c`
   - exact synthesized contents remain unresolved, but are unnecessary for the
     standalone candidate because the sideband helper is skipped
3. Runtime service table at `0x22007398`
4. LCD-local gate wrapper, controller preamble, command-mode select, and panel
   tables

Current candidate decision:

- **INCLUDE MINIMAL SUBSET**
- Rationale:
  - `0x22001f4c` is on the direct startup path before the LCD-local code
  - it no longer drags in the excluded PMU or `0x3c200000` sideband path
  - its call graph is now corrected and explicit
  - it directly overlaps later LCD-local gate state through
    `0x2200206c -> 0x22000664`, which clears bit `1` in `0x3c500048`, the same
    bit the LCD-local wrapper later enforces
  - no smaller raw-write subset is justified yet, so the minimal defensible
    subset is the corrected full `0x22001f4c` block plus the direct
    `0x22001630` / inline `0x3930003c` / `0x22001650` gates

## Provisional Dependency Chain Before Visible LCD Output

This is the strongest current ordering that is directly supported by firmware
evidence:

1. Immediate startup gate work via `0x2200160c`
   - concrete:
     - `0x220031b4(6)`
     - `0x22001f4c`
     - `0x22000664(0x10000, 0, 1)`
     - `0x3930003c &= ~0x7`
     - `0x22000664(0x10000, 0, 0)`
   - Confidence: High
2. Optional Apple state-machine pre-LCD transition
   - concrete:
     - state `4/5`:
       - `0x220061f4(3)`
       - `0x220030f0(0x2200791c)`
       - `0x22003078(4)`
     - only state `5` then continues into:
       - `0x22003ce0(1)`
       - `0x22003ce0(4)`
       - `0x22003d14(0x22007338)`
       - `0x22003cc0()`
       - `0x2200455c()`
   - Confidence: Medium
   - Standalone candidate decision: omitted
3. Runtime callback stage rooted at `0x22007398`
   - Confidence: Medium
4. LCD-local gate wrapper via `0x2200428c` -> `0x22000664`
   - Confidence: High
5. LCD controller preamble at `0x220045bc`
   - Confidence: High
6. GPIO52/GPIO53-based command-mode selection
   - Confidence: High
7. Panel helper dispatch and panel tables through `0x220048bc`
   - Confidence: High
8. Awake tail / panel-specific follow-on sequence
   - Confidence: High

This is still an analysis artifact, not a runnable “minimal write list.”
Steps 1-3 remain too broad to compress into a safe new payload without further
reduction.

## What Is Still Missing

- The exact constructor / population path for the runtime object pointer at
  `0x22007398`
- The concrete callback targets behind slots `+0x04`, `+0x08`, `+0x28`,
  `+0x2c`
- Whether the external service object is installed by bootrom, DFU runtime, or
  another loader stage before WTF startup
- The exact state-machine condition that chooses the longer init helper mode for
  the connected panel in Apple’s cold-init path
- The reduced minimal subset of the earlier `0x22001f4c` helper block that is
  genuinely required before `0x2200455c()` can make the panel respond visibly
- Whether any part of Apple state `4/5` must still be replayed once
  `0x220061f4` and its matching `0x2200791c` restore are intentionally omitted

## `0x22001f4c` Corrected Operation List

The corrected direct helper block is:

1. `0x22003c18(0)`
   - clear bit `0x8000` at `0x3c500000`
   - clear bit `0x200` at `0x39900000`
2. `0x22000958(0, 0, 1)`
   - mutate `0x3c500000` / `0x3c500004`
3. `0x22000958(4, 0, 1)`
   - mutate `0x3c500004`
4. `0x22000958(2, 0, 1)`
   - mutate `0x3c500004`
5. `0x22003890(1, 0xd8)`
   - program `0x3c500020` and `0x3c500044/0x40`
6. `0x22003890(2, 0xd8)`
   - program `0x3c500024` and `0x3c500044/0x40`
7. `0x22003890(3, 0xd8)`
   - program `0x3c500028` and `0x3c500044/0x40`
8. `0x22000958(4, 3, 4)`
   - mutate `0x3c500004`
9. `0x22000958(2, 3, 2)`
   - mutate `0x3c500004`
10. `0x22000958(0, 3, 1)`
   - mutate `0x3c500000` / `0x3c500004`
11. `0x22000958(5, 3, 0x12)`
   - mutate `0x3c500010`
   - program `0x38100000 + {0x08, 0x10, 0xf0}`
12. `0x22000958(8, 0, 4)`
   - write `8` to `0x38501000`
13. `0x220039fc(0, 1)`
   - no-op for this selector
14. `0x220039fc(5, 1)`
   - set bit `31` at `0x3c500010`
15. `0x220039fc(15, 0)`
   - clear bit `15` at `0x3c500008`
16. `0x220039fc(14, 0)`
   - clear bit `31` at `0x3c500008`
17. `0x220039fc(6, 0)`
   - clear bit `15` at `0x3c50000c`
18. `0x22003890(1, 0)`
   - clear enable/state bits for the `0x3c500020` path
19. `0x22003890(2, 0)`
   - clear enable/state bits for the `0x3c500024` path
20. `0x22000664(0x2007df65, 0x1ef49, 0)`
   - set broad masks in `0x3c500048` / `0x3c50004c`
21. `0x22000664(0x06002082, 0x1036, 1)`
   - clear broad masks in `0x3c500048` / `0x3c50004c`
   - includes clearing bit `1` in `0x3c500048`
22. `0x22003c18(1)`
   - set bit `0x8000` at `0x3c500000`
   - set bit `0x200` at `0x39900000`

This corrected list is the exact Apple-backed subset carried into the prepared,
unrun standalone payload.

## Safety Decision

- A no-MMIO service-table stub payload is safe to prepare host-side.
- Do not run a new “full LCD init” payload yet.
- Reason:
  - the direct LCD controller steps are real
  - the larger panel-init tables are real
  - but the callback service before `0x45bc` is still external to the static
    WTF body and not reduced to a raw, non-speculative register list
  - early WTF startup now suggests that service is provided by a wider runtime
    ABI rather than by a simple entry-register handoff, so a direct bypass
    remains unjustified

## Standalone-Payload Integration Boundary

There is an additional practical limit for a tiny standalone DFU payload:

- the Apple routines at `0x2200455c`, `0x220048bc`, and related addresses are
  offsets inside the decrypted WTF body, not immutable ROM entry points
- a new flat payload loaded at `0x22000000` does **not** automatically contain
  those routines

Therefore a standalone test payload has two distinct layers:

1. install a stub table at `0x22007398`
2. separately embed or transliterate the Apple LCD path that would consume it

The current reconstruction finishes step 1 and documents the control-flow ABI.
Step 2 still needs explicit code translation or a WTF-derived test image.

## Prepared Stub Bootstrap

Prepared but not run:

- [tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.S](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.S:1)
- `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.bin`
- `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.elf`
- `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.map`

Verified properties:

- entry point `0x22000000`
- text size `88` bytes
- only RAM write:
  - `[0x22007398] = 0x22000020`
- no MMIO writes
- callback table layout:
  - `+0x04 = 0x22000010`
  - `+0x08 = 0x22000010`
  - `+0x28 = 0x22000010`
  - `+0x2c = 0x22000010`

Purpose:

- bootstrap the observed service-table ABI only
- provide a concrete, safe artifact for later LCD-integration work

Non-goal:

- it does not call any LCD code
- it does not prove the no-op stubs are a semantically complete replacement for
  the original runtime service

## Prepared Embedded LCD Payload

Prepared but not run:

- [tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.S](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.S:1)
- [tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.lds](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.lds:1)
- `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.bin`
- `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.elf`
- `tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.map`

### Embed Strategy

Chosen strategy: **preserved-address embed**, not broad hand-translation.

Reason:

- the Apple LCD path has many PC-relative literals and absolute intra-image
  branches
- the payload still loads at `0x22000000`
- by placing the embedded Apple slices back at their original WTF VMAs, those
  references remain valid without guessing rewritten control flow

Included Apple-backed slices:

| Purpose | VMA in payload | Source file offset | Size |
|---|---:|---:|---:|
| gate/resource RMW helper | `0x22000664` | `0x664` | `0x38` |
| timer helpers | `0x22000828` | `0x828` | `0x0c8` |
| LCD/service/helper block | `0x22003b18` | `0x3b18` | `0x12c4` |
| LCD tables / panel records | `0x22007000` | `0x7000` | `0x0830` |

Locally supplied non-Apple code:

- bootstrap at `0x22000000`
- no-op `0x22002f04` stub replacing the original error-print helper
- service table object at `0x22000080`
- explicit zero padding from `0x22007830..0x22008000` for Apple work buffers

### Bootstrap Sequence

The handwritten offset-0 bootstrap performs only:

1. set `sp = 0x2200fff0`
2. install the minimal runtime service table at `0x22007398`
3. reseed panel selector cache:
   - `0x2200700c = 4`
4. clear panel helper cache:
   - `0x2200739c = 0`
   - `0x220073a0 = 0`
5. call the Apple wrappers:
   - `0x22003c98(0x22007338)`
   - `0x22003ce0(1)`
   - `0x22003ce0(4)`
   - `0x22003d14(0x22007338)`
   - `0x22003cc0()`
6. call the embedded Apple LCD path:
   - `0x2200455c()`
   - `0x220048bc(1, 0)`
   - `0x220048bc(4, 0)`
7. loop forever

Rationale for `mode 1` then `mode 4`:

- `mode 1` is the Apple full-init path for all three panel-group helpers
- `mode 4` is the Apple awake tail where the helper implements one
- for groups that do not implement `mode 4`, the helper returns immediately

This is a conservative Apple-backed superset for first-visibility work, not yet
proven as the exact single caller sequence used in all states.

### Relocation Hazards Handled

Handled by preserved-address placement:

- PC-relative literal loads in:
  - `0x220042c8`
  - `0x220045bc`
  - `0x22004624`
  - `0x220048bc`
  - `0x22004968`
  - `0x220049a8`
  - `0x22004a50`
  - `0x22004bf8`
- absolute intra-image branches between embedded functions
- panel record pointers at:
  - `0x220072bc`
  - `0x220072e4`
  - `0x2200730c`

Not handled by translation because it was unnecessary:

- no hand-rewritten panel-table interpreter
- no synthetic table records
- no guessed literal constants

### MMIO Inventory

The prepared payload may perform Apple-backed writes/reads in these regions:

| Region | Access type | Source functions | Risk |
|---|---|---|---|
| `0x3c500048`, `0x3c50004c` | read-modify-write | `0x22000664`, `0x2200428c`, `0x220042b8` | Medium |
| `0x38300000`, `0x38300020`, `0x3830007c`, `0x38300088` | write | `0x220045bc` | Medium |
| `0x3830001c` | poll/read | `0x220045f8`, `0x22004d74` | Low |
| `0x38300004`, `0x38300040` | write | `0x2200475c`, `0x22004778`, `0x22004794`, `0x22004d8c`, `0x22004da8` | Medium |
| `0x3cf000c4` | read | `0x22004438`, `0x22004f58` for GPIO52/53 selector | Low |
| `0x3cf00084` | read-modify-write | `0x22004f88` for GPIO36 pulse on group 2/3 path | Medium |
| `0x3820010c`, `0x3990010c` | write | `0x22003e20` / `0x22004480` `OP1` path | Medium-high |
| addresses from lookup table `0x22007560` | write | `0x22004bf8` via `OP1` path | High |

`0x22007560` resolves to Apple-backed MMIO targets including:

- `0x38300040`
- `0x3cb00010`
- `0x3cc00020`, `0x3cc00024`
- `0x3cc04020`, `0x3cc04024`
- `0x3cc08020`, `0x3cc08024`
- `0x3cc0c020`, `0x3cc0c024`
- `0x3ca00010`, `0x3ca00038`
- `0x3cd00010`, `0x3cd00038`
- `0x3d400010`, `0x3d400038`
- `0x38a00080`
- `0x3c300010`, `0x3c300020`
- `0x3ce00010`, `0x3ce00020`
- `0x3d200010`, `0x3d200020`

These are not guessed addresses; they come directly from the Apple lookup block
at body `0x7560`.

### Validation

Verified host-side:

- entry point `0x22000000`
- flat binary size `0x8000`
- embedded Apple slices remain at original VMAs
- work-buffer pad reaches `0x22008000`
- `0x22007398` service table is populated by bootstrap

### Safer Than Guessing Because

- all embedded hardware code comes from the decrypted Apple WTF body
- all panel records and tables come from the same body
- the only handwritten logic is:
  - stack setup
  - service-table install
  - cache re-init for Apple selector/helper state
  - top-level call ordering
- no synthetic MMIO bases or invented command values were added

## First Hardware Outcome

The embedded `lcd-fullinit-n3g.bin` payload was run once on real hardware.

Observed result:

- host-side DFU takeover occurred again:
  - pre-send `mks5lboot --dfuscan`: state `2`
  - post-send `mks5lboot --dfuscan`: `LIBUSB_ERROR_OTHER`
- visible screen result remained black:
  - no flicker
  - no backlight change
  - no Apple logo
  - no visible reset
- recovery remained intact:
  - manual reset worked
  - DFU re-entry returned to state `2`

Classification:

- `EXECUTION ONLY`

Implication:

- the widened Apple-backed LCD payload still did not reach a visible LCD
  milestone
- the remaining blocker is no longer payload linkage
- it is now either:
  - still-missing prerequisite PMU / clock / GPIO state outside the embedded
    LCD path, or
  - a panel/control path whose first visible effect is not yet being reached by
    this call order

## Cross-Check With Rockbox

Matches:

- command-mode families align with Rockbox `LCD_MODE_P8` / `LCD_MODE_P18`
- Rockbox already models multiple Nano 3G panel variants
- Apple uses much larger per-panel sequences for some groups, which explains why
  the shared awake-only payload was not enough

Caveat:

- Rockbox panel detection reads LCD ID later
- Apple’s early path uses GPIO straps first and runtime callbacks before the
  visible panel commands

## Minimal Power-Step Candidate

After the black-screen `lcd-pregate-fullinit-n3g.bin` run, the next narrowed
question was whether a **single** Apple-backed power or backlight step could be
added without reopening the broader PMU or sideband GPIO paths.

Candidate ranking from current evidence:

| Candidate | Evidence | Why not chosen / chosen |
|---|---|---|
| PMU reg `0x1b = 0x01/0x00` | Apple helper `0x220054b0`; in-tree `pmu_hdd_power(bool on)` writes `0x1b` | Excluded. The in-tree Nano 3G source already ties `0x1b` to HDD/power gating, not a first visibility signal. |
| PMU reg `0x43` bit `0` RMW | Apple helper `0x220054f8` | Excluded. The only observed Apple call is `0x2200304c(0)`, which **clears** bit `0`. Using the opposite polarity as an “enable” would be a guess. |
| GPIO pulse via `0x220061f4` | Apple code hits pins `72..75` through `0x22004f0c` | Excluded. The path still falls into the `0x3c200000` sideband block already treated as unsafe / non-LCD. |
| PMU reg `0x1d = 0x0a` | Apple helper `0x220054b0` at `0x220054b8..0x220054cc` | **Chosen**. It is the narrowest exact Apple PMU write that is not already tied to HDD semantics and does not require guessing a polarity. |

Selected single added action:

- PMU slave `0x73`
- register `0x1d`
- value `0x0a`

Current interpretation:

- likely regulator / panel-rail programming or adjacent platform analog state
- **not** proven backlight enable
- still safer than inventing `LEDCTL (0x20)` or forcing the unobserved
  `0x43 bit0 = 1` polarity

Risk:

- Medium
- narrower than replaying the full PMU branch
- still uses the Apple PMU I2C transport, so the payload necessarily touches:
  - `0x3c600000` I2C controller state
  - `0x3c7000b4` timer basis through Apple timeout helpers

Prepared host-side artifact, not run:

- `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.bin`
- entry point `0x22000000`
- relative to `lcd-pregate-fullinit-n3g`, it adds exactly one new hardware step
  before the existing pre-gate + LCD sequence:
  - `apple_05474(0x1d, 1, &0x0a)`

## Minimal Power-Step Candidate, Revision 2

After the first power-step candidate still failed to produce visible output,
the next bounded refinement was to add exactly one more Apple-backed PMU
operation without widening into broader PMU or GPIO behavior.

Selected added action:

- PMU slave `0x73`
- register `0x43`
- read-modify-write
- **clear bit `0`**

Why this is justified now:

- the behavior is explicitly present in Apple helper `0x220054f8`
- the only observed Apple call path uses argument `0`, which clears bit `0`
- this avoids inventing the opposite polarity or touching any new PMU register

Still excluded:

- reg `0x1b`
- reg `0x43` bit-set polarity
- `LEDCTL (0x20)`
- `0x220061f4` / `0x3c200000`

Prepared host-side artifact, not run:

- `tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.bin`
- entry point `0x22000000`

Delta relative to `lcd-powerstep-n3g`:

1. `apple_05474(0x1d, 1, &0x0a)`
2. **new:** `apple_054f8(0)` to clear PMU reg `0x43` bit `0`
3. existing pregate + LCD-local full init
4. loop forever

Risk:

- Medium
- still bounded to Apple-backed PMU/I2C helpers only
- touches no new PMU register beyond `0x43`
- no GPIO sideband path or guessed display reset line was introduced

## Visibility Root-Cause Isolation

After `lcd-fullinit`, `lcd-pregate-fullinit`, `lcd-powerstep`, and
`lcd-powerstep2` all reached DFU takeover without any visible LCD or backlight
response, the remaining firmware-backed candidates were reduced to:

1. PMU reg `0x1b`
2. sideband path `0x220061f4`

### PMU reg `0x1b`

Observed Apple behavior:

- only appears through `0x220054b0`
- `0x220054b0` always writes:
  - reg `0x1d = 0x0a`
  - then reg `0x1b = 0x01` or `0x00`
- the only direct caller in this early firmware path is:
  - `0x22003018`
- and the only observed state-machine use is:
  - `0x22003078(1)` -> `0x22003018(0)`

Cross-check against local source:

- `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
  defines:
  - `pmu_hdd_power(bool on) { pmu_write(0x1b, on ? 1 : 0); }`
- the same helper exists for iPod 6G and Nano 4G as HDD/storage power control

Conclusion:

- strongest classification: **unrelated / storage-oriented**
- it is not observed on the immediate LCD state path that reaches
  `0x2200455c`
- risk of treating it as a display rail is too high for a next-step payload

### Sideband path `0x220061f4`

Observed state-machine ordering:

- LCD-adjacent states `5/6` run:
  - `0x220061f4(3)`
  - `0x220030f0(0x2200791c)`
  - `0x22003078(4)`
- then state `5` alone continues into:
  - `0x22003ce0(1)`
  - `0x22003ce0(4)`
  - `0x22003d14(0x22007338)`
  - `0x22003cc0()`
  - `0x2200455c()`

So `0x220061f4(3)` is definitely on the same path immediately before the LCD
branch, unlike PMU `0x1b`.

What `0x220061f4(3)` actually does:

- dispatches to:
  - `0x22005ef4`
  - then `0x220060e0`
- `0x22005ef4`:
  - sends GPIO commands for pins `72`, `73`, `74`, `75` via `0x22004f0c`
  - all with op `2`
  - then performs multiple direct writes to `0x3c200000`
- `0x220060e0`:
  - polls and manipulates the same `0x3c200000` block
  - uses additional magic values `0x8000063a` / `0x8000062a`
  - stores sideband state into `0x22007788`

What remains missing:

- no firmware evidence tying pins `72..75` to LCD reset, panel enable, or
  backlight enable
- local Rockbox and bring-up notes still identify `0x3c200000` on S5L8702 as
  clickwheel, with historical NAND-address collision risk
- no safe reduced subset of `0x220061f4(3)` is justified from the current
  evidence alone

## Pin Ownership Verification For Pins 72..75

Cross-reference result from the decrypted WTF:

- all explicit uses of pins `72..75` are confined to the sideband family:
  - `0x22005ef4`
  - `0x220061f4`
- no LCD-local routine (`0x2200455c`, `0x220048bc`, panel-table helpers) refers
  to those pins directly

Exact state-machine ordering:

- state `5/6` path:
  - `0x220061f4(3)`
  - `0x220030f0(0x2200791c)`
  - `0x22003078(4)`
- state `5` then continues into:
  - `0x22003ce0(1)`
  - `0x22003ce0(4)`
  - `0x22003d14(0x22007338)`
  - `0x22003cc0()`
  - `0x2200455c()`

So the sideband path is immediately before LCD init by ordering, but its actual
pin behavior is not reset-like.

### Exact pin sequence

`0x22005ef4`:

- `0x22005f00`: `GPIO72 <- op2`
- `0x22005f08..0x22005f0c`: delay `1` via `0x22000884`
- `0x22005f18`: `GPIO73 <- op2`
- `0x22005f28`: `GPIO74 <- op2`
- `0x22005f38`: `GPIO75 <- op2`

`0x220061f4` mode variants:

- mode `0`: `0x22005ef4`, then `0x22006038`
- mode `1`: `GPIO74 <- output low`, delay `1`, then `GPIO72 <- output low`
- mode `2`: `GPIO74 <- op2`
- mode `3`: delay `25`, then `0x22005ef4`, then `0x220060e0`

Observed timing evidence:

- there is **no** low -> delay -> high pulse on the same pin in mode `3`
- mode `3` uses a delay before restoring pins `72..75` to `op2`, not before a
  release-high transition

### 0x3c200000 correlation

`0x22005ef4` and `0x220060e0` program the `0x3c200000` block directly:

- `0x3c200000`
- `0x3c200004`
- `0x3c200008`
- `0x3c20000c`
- `0x3c200010`
- `0x3c20001c`

And local S5L8702 mappings already identify:

- `WHEEL_BASE = 0x3c200000`
- `WHEEL00`, `WHEEL04`, `WHEEL08`, `WHEEL0C`, `WHEEL10`, `WHEELTX`

The sequence also matches a peripheral-command handshake better than panel
reset:

- `0x2200616c` clears and re-sets bit `0x200000` in `WHEEL00`
- writes `0x80000000 | (arg >> 1)` to `WHEELTX`
- sets bit `0` in `WHEEL04`
- polls `WHEEL0C` with timeout `0x5dc`

### Cross-check against GPIO defaults

Nano 3G `gpio_preinit()` defaults group `9` to:

- `gpio_data[9] = 0x22222222`

Pins `72..79` are GPIO group `9`, so `op2` is already the default alternate
function for that bank. The sideband path restoring pins `72..75` to `op2`
therefore fits “return wheel-side pins to peripheral mode” much better than
“pulse an LCD reset line”.

## Decision

- **NOT_LCD**

Why:

- PMU `0x1b` remains storage-oriented
- pins `72..75` are only seen in the `0x220061f4` sideband family
- the sideband family restores those pins to alternate-function `2`
- it then programs the confirmed `WHEEL_BASE` register block and polls wheel
  completion/status bits
- no reset-style low/high pulse or LCD-specific ownership evidence was found

Risk assessment:

- `PMU 0x1b`: Medium-high, but unrelated
- `0x220061f4` minimal subset: High and unjustified for LCD, because current
  evidence points at clickwheel-side peripheral setup

Prepared next payload:

- none

Current stop line:

- do not prepare `lcd-powerstep3-n3g.bin`
- the next safe visibility lead must come from a different evidence path than
  PMU `0x1b` or pins `72..75`

## Display-Power / Backlight Search Outside Rejected Paths

This pass explicitly searched for a remaining visibility dependency outside the
already rejected PMU `0x1b` and `0x220061f4` / `GPIO72..75` paths.

### PMU / backlight result

- No new Apple PMU display/backlight helper was found in the decrypted WTF.
- The only concrete PMU helpers still present are:
  - `0x220054b0`
    - writes reg `0x1d = 0x0a`
    - then writes reg `0x1b = 0x01/0x00`
  - `0x220054f8`
    - reads PMU reg `0x43`
    - modifies bit `0`
    - writes it back
- Cross-reference search for callers of:
  - `0x22005420`
  - `0x22005474`
  - `0x220054b0`
  - `0x220054f8`
  produced no additional display-adjacent PMU call sites.
- In particular, no explicit Apple-backed use of:
  - `LEDCTL (0x20)`
  - brightness/output regs such as `0x28` / `0x29`
  was found in this WTF body.

### GPIO / reset result

- No non-wheel GPIO low -> delay -> high pulse was found immediately before the
  LCD-local path.
- The only reset-like pulse still seen near the broader startup flow is:
  - `0x220017e8..0x22001804`
  - set bit `1` in `0x38400804`
  - delay `500` via `0x22000884`
  - clear bit `1` in `0x38400804`
- This pulse is not on the already tested immediate startup path
  `0x2200160c`; it lives in the later `0x22001698` path.

### Additional display-adjacent gate cluster

The same later `0x22001698` path also performs three narrow gate clears:

| Caller | Exact operation | Notes |
| --- | --- | --- |
| `0x220018d8` | `0x3c500048 &= ~0x400` | later path only |
| `0x220018e8` | `0x3c500048 &= ~0x1` | later path only |
| `0x220018fc` | `0x3c50004c &= ~0x2000` | later path only |

Those sit beside:

- the `0x38400804` bit-1 pulse
- runtime/service-object calls through the same broader bring-up path
- additional service-mediated work using IDs `19` and `33`

### Ordering and interpretation

- Immediate tested path:
  - `0x2200160c`
  - `0x220031b4(6)`
  - `0x22001f4c()`
  - `0x22000664(0x10000, 0, 1)`
  - `0x3930003c &= ~0x7`
  - `0x22000664(0x10000, 0, 0)`
- Later unresolved path:
  - `0x22001698`
  - `0x38400804` pulse
  - `0x220018d8`
  - `0x220018e8`
  - `0x220018fc`
  - multiple runtime/service-object calls

This makes the later path the strongest remaining non-PMU, non-wheel-side
visibility lead. But it does **not** reduce to a single independent action yet.
The pulse and the three gate clears are still coupled to the larger
`0x22001698` service-mediated flow, so isolating only one of them would still
be speculative.

### Candidate list

| Candidate | Subsystem | Evidence | Confidence | Risk | Safe as one new action? |
| --- | --- | --- | --- | --- | --- |
| `LEDCTL (0x20)` or brightness regs | PMU / backlight | Only Rockbox-local target code, no Apple WTF call site | Low | Medium | No |
| `0x38400804 bit1` pulse | Reset / enable | Exact Apple pulse with delay, but only in coupled `0x22001698` path | Medium | Medium | No |
| `0x3c500048 &= ~0x400` | Clock / gate | Exact Apple write in later path | Medium | Medium | No |
| `0x3c500048 &= ~0x1` | Clock / gate | Exact Apple write in later path | Medium | Medium | No |
| `0x3c50004c &= ~0x2000` | Clock / gate | Exact Apple write in later path | Medium | Medium | No |

### Decision

- **STILL_BLOCKED**

Reason:

- no explicit Apple backlight PMU path was found outside already tested
  registers `0x1d` and `0x43`
- no non-wheel LCD reset GPIO pulse was recovered
- the strongest remaining lead is a **coupled** later gate/reset cluster, not a
  single isolated action

Prepared artifact:

- none

## 2026-04-24 OSOS higher-phase evidence status update 11

- RetailOS decryption is still running and has now reached:
  - `7150080` bytes
  - `66.264%`
- No new visibility action is justified yet because the higher-phase OSOS
  plaintext is still incomplete.
- Current decision:
  - continue decryption
  - do not build or run any new hardware visibility payload

## 2026-04-23 status note

- First visible LCD/backlight sequence remains unresolved.
- Current effort is still OSOS decryption to obtain higher-level Apple-backed
  display/backlight evidence.
- Live decrypt progress has reached:
  - `3989760` bytes
  - `36.976%`
- No new hardware payload is justified or prepared in this checkpoint.

## 2026-04-23 status note update

- OSOS decrypt remains active and has reached:
  - `4165632` bytes
  - `38.605%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 2

- OSOS decrypt remains active and has reached:
  - `4364544` bytes
  - `40.449%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 3

- OSOS decrypt remains active and has reached:
  - `4504320` bytes
  - `41.744%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 4

- OSOS decrypt remains active and has reached:
  - `4675584` bytes
  - `43.332%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 5

- OSOS decrypt remains active and has reached:
  - `5009664` bytes
  - `46.428%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 6

- OSOS decrypt remains active and has reached:
  - `5207040` bytes
  - `48.257%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 7

- OSOS decrypt remains active and has reached:
  - `5379072` bytes
  - `49.851%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 8

- OSOS decrypt remains active and has reached:
  - `5532672` bytes
  - `51.275%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-23 status note update 9

- OSOS decrypt remains active and has reached:
  - `5675520` bytes
  - `52.599%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-24 status note update 10

- OSOS decrypt remains active and has reached:
  - `6023424` bytes
  - `55.823%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-24 status note update 11

- OSOS decrypt remains active and has reached:
  - `6187776` bytes
  - `57.346%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-24 status note update 12

- OSOS decrypt remains active and has reached:
  - `6398976` bytes
  - `59.303%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-24 status note update 13

- OSOS decrypt remains active and has reached:
  - `6646272` bytes
  - `61.595%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## 2026-04-24 status note update 14

- OSOS decrypt remains active and has reached:
  - `6800640` bytes
  - `63.026%`
- No new visibility payload is justified while higher-level firmware plaintext
  is still incomplete.

## OSOS decrypt exited checkpoint

- Latest observed recovery-backed OSOS state after the background decrypt
  stopped:
  - saved plaintext bytes: `3461328`
  - actual completion: `32.078%`
- Post-exit operational detail:
  - the decrypt process is no longer running
  - `mks5lboot --dfuscan` now returns `LIBUSB_ERROR_OTHER`
  - the next resume therefore needs another clean DFU re-entry

## OSOS decrypt live checkpoint 6

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `3347856`
  - actual completion: `31.027%`
- Most recent operational detail:
  - the decrypt process is still running in the background
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`, which matches the active
    decrypt still owning the DFU session

## OSOS decrypt live checkpoint 5

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `3243168`
  - actual completion: `30.056%`
- Most recent operational detail:
  - the decrypt process is still running in the background
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`, which matches the active
    decrypt still owning the DFU session

## OSOS decrypt live checkpoint 4

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `3138624`
  - actual completion: `29.088%`
- Most recent operational detail:
  - the decrypt process is still running in the background
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`, which matches the active
    decrypt still owning the DFU session

## OSOS decrypt live checkpoint 3

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2992032`
  - actual completion: `27.729%`
- Most recent operational detail:
  - the decrypt process is still running in the background
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`, which matches the active
    decrypt still owning the DFU session

## OSOS decrypt live checkpoint 2

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2923536`
  - actual completion: `27.094%`
- Most recent operational detail:
  - the decrypt process is still running in the background
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`, which matches the active
    decrypt still owning the DFU session

## OSOS decrypt checkpoint update 17

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2666640`
  - actual completion: `24.719%`
- Most recent operational detail:
  - the automatic loop resumed from clean DFU state `2`
  - one intermediate checkpoint remained usable in DFU state `9`, allowing one
    more bounded slice without a user prompt
  - after the newest checkpoint, host state returned to the stale-DFU pattern,
    so the next resume needs another clean DFU re-entry

## OSOS decrypt live checkpoint

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2794128`
  - actual completion: `25.895%`
- Most recent operational detail:
  - the decrypt process is still running in the background
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`, which matches the active
    decrypt still owning the DFU session

## OSOS decrypt checkpoint update 15

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2476224`
  - actual completion: `22.949%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - after checkpointing, the Nano remained usable in DFU state `9`, so the
    automatic loop continued without a user prompt

## OSOS decrypt checkpoint update 16

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2591376`
  - actual completion: `24.019%`
- Most recent operational detail:
  - this latest resume started from usable DFU state `9`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the automatic loop stopped and the next resume needs another clean DFU
    re-entry

## OSOS decrypt checkpoint update

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `1720752`
  - actual completion: `15.947%`
- This note is a checkpoint correction only; no new decrypt slice or hardware
  payload was run in the same step.
- Precondition failure for the next decrypt resume:
  - `mks5lboot --dfuscan` returned `LIBUSB_ERROR_OTHER`
  - so the Nano still needs another clean DFU re-entry before the next resume

## OSOS decrypt checkpoint update 2

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `1774224`
  - actual completion: `16.443%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the next resume may require another clean DFU re-entry

## OSOS decrypt checkpoint update 3

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `1828560`
  - actual completion: `16.941%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint update 4

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `1884288`
  - actual completion: `17.459%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the next resume may require another clean DFU re-entry

## OSOS decrypt checkpoint update 5

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `1935696`
  - actual completion: `17.938%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the next resume may require another clean DFU re-entry

## OSOS decrypt checkpoint update 6

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `1994448`
  - actual completion: `18.482%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the next resume may require another clean DFU re-entry

## OSOS decrypt checkpoint update 7

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2044896`
  - actual completion: `18.952%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint update 8

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2096064`
  - actual completion: `19.428%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint update 9

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2147184`
  - actual completion: `19.894%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the next resume may require another clean DFU re-entry

## OSOS decrypt checkpoint update 10

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2197776`
  - actual completion: `20.367%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the next resume may require another clean DFU re-entry

## OSOS decrypt checkpoint update 11

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2254272`
  - actual completion: `20.891%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint update 12

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2308800`
  - actual completion: `21.402%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, the Nano remained in clean DFU state `2`

## OSOS decrypt checkpoint update 13

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2365104`
  - actual completion: `21.918%`
- Most recent operational detail:
  - this latest resume started from clean DFU state `2`
  - after checkpointing, the Nano remained in clean DFU state `2`, so the
    automatic loop continued without a user prompt

## OSOS decrypt checkpoint update 14

- Latest observed recovery-backed OSOS state:
  - saved plaintext bytes: `2421456`
  - actual completion: `22.435%`
- Most recent operational detail:
  - this latest resume started from the prior clean DFU state `2`
  - repeated `libusb: interrupted [code -10]` messages still occurred during
    the long run, but the recovery buffer remained valid
  - after checkpointing, host state returned to the stale-DFU pattern again,
    so the automatic loop stopped and the next resume needs another clean DFU
    re-entry

## OSOS Decrypt Progress Update

Status:

- **OSOS_DECRYPT_STILL_IN_PROGRESS**

Latest checkpoints:

- checkpoint 3:
  - saved plaintext bytes: `165696`
  - actual completion: `1.536%`
- checkpoint 4:
  - saved plaintext bytes: `285648`
  - actual completion: `2.647%`

Operational notes:

- the latest resumable run continued successfully from DFU state `3`
- the recovery buffer remained valid despite repeated
  `libusb: interrupted [code -10]` messages during the long run
- after checkpointing, the device again returned to the stale host-side DFU
  condition:
  - `lsusb` still shows `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`

Meaning for first-visibility work:

- no completed higher-level Apple UI/display plaintext exists yet
- no additional visibility action is justified from OSOS evidence yet
- the correct next step is still to keep resuming the same recovery-backed
  decrypt until the decrypted OSOS image is complete

Latest checkpoint:

- checkpoint 5:
  - saved plaintext bytes: `350064`
  - actual completion: `3.244%`

Most recent operational detail:

- the latest resume started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, the Nano again returned to the host-visible but
  non-clean DFU state where `mks5lboot --dfuscan` fails with
  `LIBUSB_ERROR_OTHER`

Latest checkpoint:

- checkpoint 6:
  - saved plaintext bytes: `420240`
  - actual completion: `3.895%`

Meaning for first-visibility work remains unchanged:

- no completed higher-level Apple UI/display plaintext exists yet
- no additional visibility action is justified from OSOS evidence yet
- the correct next step is still to keep resuming the same recovery-backed
  decrypt until the decrypted OSOS image is complete

Latest checkpoint:

- checkpoint 16:
  - saved plaintext bytes: `1054848`
  - actual completion: `9.775%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, the Nano returned to clean DFU state `2`

Latest checkpoint:

- checkpoint 24:
  - saved plaintext bytes: `1643520`
  - actual completion: `15.235%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, host state returned to the stale-DFU pattern again,
  so the next resume may require another clean DFU re-entry

Latest checkpoint:

- checkpoint 19:
  - saved plaintext bytes: `1273968`
  - actual completion: `11.803%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, host state returned to the stale-DFU pattern again,
  so the next resume may require another clean DFU re-entry

Latest checkpoint:

- checkpoint 23:
  - saved plaintext bytes: `1574688`
  - actual completion: `14.595%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, the Nano returned to clean DFU state `2`

Latest checkpoint:

- checkpoint 22:
  - saved plaintext bytes: `1482672`
  - actual completion: `13.740%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, host state returned to the stale-DFU pattern again,
  so the next resume may require another clean DFU re-entry

Latest checkpoint:

- checkpoint 21:
  - saved plaintext bytes: `1414464`
  - actual completion: `13.107%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, host state returned to the stale-DFU pattern again,
  so the next resume may require another clean DFU re-entry

Latest checkpoint:

- checkpoint 20:
  - saved plaintext bytes: `1346016`
  - actual completion: `12.473%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, host state returned to the stale-DFU pattern again,
  so the next resume may require another clean DFU re-entry

Latest checkpoint:

- checkpoint 18:
  - saved plaintext bytes: `1202784`
  - actual completion: `11.148%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, host state returned to the stale-DFU pattern again,
  so the next resume may require another clean DFU re-entry

Latest checkpoint:

- checkpoint 17:
  - saved plaintext bytes: `1137024`
  - actual completion: `10.538%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, the Nano returned to clean DFU state `2`

Latest checkpoint:

- checkpoint 13:
  - saved plaintext bytes: `850560`
  - actual completion: `7.882%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, the Nano remained host-visible and still answered DFU,
  but in state `9` rather than clean state `2`

Latest checkpoint:

- checkpoint 14:
  - saved plaintext bytes: `921840`
  - actual completion: `8.547%`

Most recent operational detail:

- this latest resume succeeded directly from DFU state `9`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, the Nano returned to clean DFU state `2`

Latest checkpoint:

- checkpoint 15:
  - saved plaintext bytes: `990048`
  - actual completion: `9.176%`

Meaning for first-visibility work remains unchanged:

- no completed higher-level Apple UI/display plaintext exists yet
- no additional visibility action is justified from OSOS evidence yet
- the correct next step is still to keep resuming the same recovery-backed
  decrypt until the decrypted OSOS image is complete

Latest checkpoint:

- checkpoint 12:
  - saved plaintext bytes: `790512`
  - actual completion: `7.325%`

Meaning for first-visibility work remains unchanged:

- no completed higher-level Apple UI/display plaintext exists yet
- no additional visibility action is justified from OSOS evidence yet
- the correct next step is still to keep resuming the same recovery-backed
  decrypt until the decrypted OSOS image is complete

Latest checkpoint:

- checkpoint 11:
  - saved plaintext bytes: `730320`
  - actual completion: `6.763%`

Meaning for first-visibility work remains unchanged:

- no completed higher-level Apple UI/display plaintext exists yet
- no additional visibility action is justified from OSOS evidence yet
- the correct next step is still to keep resuming the same recovery-backed
  decrypt until the decrypted OSOS image is complete

Latest checkpoint:

- checkpoint 8:
  - saved plaintext bytes: `545472`
  - actual completion: `5.055%`

Most recent operational detail:

- this latest resume started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- unlike the prior several checkpoints, the Nano stayed in clean DFU state `2`
  after the stop

Latest checkpoint:

- checkpoint 9:
  - saved plaintext bytes: `605760`
  - actual completion: `5.610%`

Most recent operational detail:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages still occurred during the
  long run, but the recovery buffer remained valid
- after checkpointing, host state returned to the stale-DFU pattern again,
  so the next resume may require another clean DFU re-entry

Latest checkpoint:

- checkpoint 10:
  - saved plaintext bytes: `668688`
  - actual completion: `6.196%`

Meaning for first-visibility work remains unchanged:

- no completed higher-level Apple UI/display plaintext exists yet
- no additional visibility action is justified from OSOS evidence yet
- the correct next step is still to keep resuming the same recovery-backed
  decrypt until the decrypted OSOS image is complete

Latest checkpoint:

- checkpoint 7:
  - saved plaintext bytes: `483696`
  - actual completion: `4.481%`

Meaning for first-visibility work remains unchanged:

- no completed higher-level Apple UI/display plaintext exists yet
- no additional visibility action is justified from OSOS evidence yet
- the correct next step is still to keep resuming the same recovery-backed
  decrypt until the decrypted OSOS image is complete

## OSOS Decrypt Progress Checkpoint

Current decision:

- **OSOS_DECRYPT_STILL_IN_PROGRESS**

Latest checkpoint:

- recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
- saved plaintext: `126096` bytes
- real completion: `1.167%`

Implication:

- there is still no completed higher-level Apple plaintext to search for:
  - `LEDCTL (0x20)`
  - `0x28 / 0x29`
  - higher-level display sleep/wake
  - LCD-visible enable paths
- no new candidate payload is justified yet

## Fresh Display-Dependency Search Outside Rejected Paths

Outcome:

- **STILL_BLOCKED**

### What was searched

- decrypted WTF body for:
  - additional PMU display/backlight helpers
  - LCD-owned IRQ/gate setup
  - LCD wake/sleep callers
  - non-wheel GPIO reset/enable pulses
- additional firmware artifacts available locally:
  - `OSOS.fw`
  - `OSOS.fw.payload_0x800.bin`
  - `aupd.fw`
  - `aupd.fw.payload_0x800.bin`
- in-tree Nano 3G / 6G Rockbox code for:
  - backlight register conventions
  - LCD clock ownership
  - PMU/backlight register naming

### New evidence

1. No new Apple PMU display path was found in decrypted WTF.

   The only PMU helper/callsite family still visible is:

   - `0x22005420`: PMU read helper on slave `0x73`
   - `0x22005474`: PMU write helper on slave `0x73`
   - `0x220054b0`: write reg `0x1d = 0x0a`, then reg `0x1b = on/off`
   - `0x220054f8`: read-modify-write reg `0x43` bit `0`

   No Apple-backed use of:

   - `LEDCTL (0x20)`
   - `0x28`
   - `0x29`

   was recovered in the decrypted WTF body.

2. The extra `OSOS` / `aupd` artifacts are still not usable as Apple-backed
   display evidence in the current workspace.

   The available payload slices remain high-entropy and look encrypted:

   - `OSOS.fw.payload_0x800.bin`
   - `aupd.fw.payload_0x800.bin`

   So they do not currently provide a recoverable backlight or display-on path
   comparable to the decrypted WTF body.

3. The one surviving `#14` use in WTF is not a clean LCD IRQ path.

   `0x22002024` loads `r0 = 14` and calls `0x220039fc`, but `0x220039fc` is a
   generic selector/helper for the `0x3c500000` clock/power bank, not a direct
   VIC/LCD interrupt helper. It toggles bits in the `0x3c500000` register
   block according to an index table. So this is **not** a new, isolated
   `IRQ_LCD` candidate.

4. The in-tree Nano 3G code still points at backlight ownership, but not with
   Apple-backed proof.

   Nano 3G target code defines:

   - `D1671_REG_LEDCTL = 0x20`
   - `D1671_LEDCTL_ENABLE = 0x80`

   and its backlight driver uses that register directly.

   The debug screen also prints:

   - `pmu_read(0x29)` as backlight on/off
   - `pmu_read(0x28)` as brightness value

   Meanwhile iPod 6G backlight code uses:

   - `pmu_write(0x28, brightness)`
   - `pmu_write(0x29, on/off)`

   This is a useful hint that Nano 3G backlight ownership may live in the same
   PMU neighborhood, but it is **not Apple-backed** enough to justify a new
   hardware write by itself.

### Candidate table after the fresh search

| Candidate | Type | Evidence source | Confidence | Risk | Test now? |
| --- | --- | --- | --- | --- | --- |
| `LEDCTL (0x20)` enable | backlight / PMU | Nano 3G in-tree PMU/backlight code only | Medium | Medium | No |
| `0x28` / `0x29` PMU brightness/onoff | backlight / PMU | Nano 3G debug view + iPod 6G in-tree backlight code | Low-medium | Medium | No |
| `r0 = 14` path at `0x22002024 -> 0x220039fc` | LCD gate / IRQ-like helper | decrypted WTF only | Low | Medium | No |
| additional non-wheel GPIO reset pulse | panel reset GPIO | none recovered outside rejected pins | Low | Medium | No |

### Decision

- **STILL_BLOCKED**

Reason:

- decrypted WTF contains the LCD command path, but still no Apple-backed
  backlight/LEDCTL ownership path
- `OSOS` / `aupd` remain encrypted in the current workspace, so they do not
  yet contribute a usable Apple display-visibility sequence
- the surviving `#14` helper in WTF is generic `0x3c500000` bit programming,
  not a clean LCD-only IRQ/gate path
- no non-wheel GPIO reset/enable line was recovered

Prepared artifact:

- none

### Next safe step

- obtain a decrypted or traceable `OSOS` / display UI phase artifact, or
- recover a separate Apple PMU/backlight path from a firmware phase beyond WTF

Do **not** prepare `lcd-nextvisible-n3g.bin` on current evidence.

## OSOS-Focused Device-Assisted Decryption Review

Result so far:

- **OSOS_DECRYPTION_BLOCKED_WITH_REASON**

### Supported workflow status

Local `wInd3x` source does support device-assisted RetailOS decryption.

Important findings:

- `cmd_download.go` exposes:
  - `wInd3x download retailos <path>`
- `pkg/cache/cache.go` supports:
  - `PayloadKindRetailOSUpstream`
  - `PayloadKindRetailOSDecrypted`
- `getRetailOSDecrypted()`:
  - downloads/extracts `osos`
  - parses it as IMG1
  - passes `img1.Body` to the same `decrypt.Decrypt()` engine used for WTF
  - re-wraps the plaintext with `image.MakeUnsigned(...)`
- `README.md` documents the same workflow generically under CFW:
  - `download retailos`
  - `decrypt n7g-retailos.bin n7g-retailos.bin.dec`

So while the CLI help text for `decrypt` says “Decrypt DFU image”, the actual
implementation accepts IMG1 files in general, including RetailOS / `OSOS.fw`.

### Input-image verification

The local extracted firmware artifacts are valid IMG1 headers:

- `OSOS.fw`
  - magic `8702`
  - version `1.0`
  - format `3`
- `aupd.fw`
  - magic `8702`
  - version `1.0`
  - format `3`

That means the local `OSOS.fw` is a supported input type for the generic
device-assisted decrypt path.

### Attempted supported OSOS decrypt

Attempted command:

```bash
/tmp/wInd3x/wInd3x decrypt '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-decrypted.dfu
```

Observed:

- the command parsed the image as `kind="Nano 3G"`
- decryption started successfully
- upstream-style `libusb: interrupted` messages appeared during progress, which
  are expected noise per upstream docs
- throughput is extremely slow for full OSOS size (~10.8 MiB)

To make the workflow resumable, the command was restarted with:

```bash
/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu
```

### Exact blocker

The resumable attempt failed immediately with:

- `clean failed: ClrStatus: control: libusb: i/o error [code -1]`

Follow-up host checks showed:

- `lsusb` still sees `05ac:1223`
- `mks5lboot --dfuscan` fails with:
  - `Could not set USB configuration: LIBUSB_ERROR_OTHER`

So the present blocker is:

- **stale / non-clean DFU USB session after the first decrypt attempt**

not:

- unsupported image type
- unsupported tool path
- missing decrypt implementation

### Decision

- **OSOS_DECRYPTION_BLOCKED_WITH_REASON**

Reason:

- supported OSOS decrypt path exists and starts correctly
- but the current Nano session must be reset back into a clean DFU baseline
  before the long-running decrypt can continue reliably

Prepared artifact:

- none

Deferred next action once DFU is clean again:

- rerun the supported decrypt with recovery:

```bash
/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu
```

### Follow-up after clean DFU re-entry

After re-entering clean DFU, the resumable command was restarted successfully
and advanced without immediate USB errors.

Checkpoint recorded:

- recovery file: `/tmp/n3g-osos-work/n3g-osos.recovery`
- saved plaintext bytes: `72960`
- actual completion: `0.676%`

Important note:

- upstream `wInd3x` logs a field called `percent`, but for this long OSOS
  decrypt it is effectively reporting percentage points, not a `0.0..1.0`
  fraction
- the real progress must be computed from the recovery file size

This confirms the supported OSOS decrypt path is viable, but it is far too slow
to complete within a single short bring-up turn. No decrypted OSOS plaintext
artifact is available yet for display/backlight triage.

## Reduction of the `0x22001698` Gate / Reset Cluster

This pass tested whether the remaining later-path cluster could be treated as
one bounded Apple-backed pre-LCD step.

### Full direct MMIO footprint of `0x22001698`

The function does **not** reduce to only:

- `0x38400804` bit-1 pulse
- `0x3c500048 &= ~0x400`
- `0x3c500048 &= ~0x1`
- `0x3c50004c &= ~0x2000`

Direct MMIO/register effects observed in-order include:

1. `0x38100000 &= ~0x7`
   - `0x22001724..0x2200172c`
2. `0x38400804 |= 0x2`
   - `0x220017e8..0x220017f0`
3. delay `500`
   - `0x220017f4..0x220017f8`
4. `0x38400804 &= ~0x2`
   - `0x220017fc..0x22001804`
5. delay `10`
   - `0x22001808..0x2200180c`
6. `0x3c500048 &= ~0x400`
   - `0x220018cc..0x220018d8`
7. `0x3c500048 &= ~0x1`
   - `0x220018dc..0x220018e8`
8. `0x3c50004c &= ~0x2000`
   - `0x220018ec..0x220018fc`
9. `0x38e00014 = *0x22006b80`
   - `0x22001900..0x22001914`
10. `0x38e01014 = *0x22006b80`
   - `0x22001918..0x22001928`
11. set gate bits through `0x22003838`:
   - ID `39`
   - ID `40`
   - ID `19`
   - ID `33`
   - these become writes to `0x38e00010` / `0x38e01010`
12. restore saved state on exit:
   - `0x3c50004c = saved`
   - `0x3c500048 = saved`
   - `0x38100000 = saved`

### Runtime/service dependencies inside the cluster

`0x22001698` also depends on a larger runtime/service path before and around
those writes:

- `0x220024d8`
- `0x22001c70`
- service-object calls through the object at `0x22007784`
  - callback offsets used include `0x44`, `0x68`, `0x74`, `0x80`, `0x8c`, `0x90`
- `0x22006380`
- `0x22006430`
- `0x22006558`
- `0x22003b5c`
- `0x220032fc`
- `0x22003350`

So the cluster is not just a static reset/gate preamble; it is embedded in a
broader service-mediated bring-up path.

### Meaning of the `0x38400804` pulse

- all decrypted-WTF references to `0x38400804` still collapse to this one pulse
- there is no second confirmed LCD-local use to tie bit `1` directly to panel
  reset, controller reset, or backlight
- current classification:
  - **unknown reset/enable pulse**
  - not strong enough yet to call it LCD-specific

### Gate-ID interpretation

`0x220032fc` splits an ID into:

- bank = `id >> 5`
- bit = `id & 31`

`0x22003838` then sets a bit in:

- `0x38e00010` for bank `0`
- `0x38e01010` for bank `1`

That makes:

- ID `33` -> bank `1`, bit `1`

and this matches the in-tree S5L87xx mapping:

- `CLOCKGATE_CWHEEL = 33`

So the cluster is **not display-pure**; at least one of its gate IDs lines up
with clickwheel-side gating rather than LCD-specific gating.

### Cluster policy decision

- **STILL_BLOCKED**

Reason:

- the cluster touches broader state than the four initially isolated writes
- `0x38400804` pulse purpose is still unknown
- the service-mediated gate setup includes ID `33`, which matches
  `CLOCKGATE_CWHEEL`
- isolating only the pulse or only the three narrow clears would still be a
  speculative split
- replaying the full `0x22001698` path would pull in unresolved runtime/service
  dependencies well beyond a bounded display step

Prepared artifact:

- none

## Final Classification of `0x22001698`

This pass resolves the remaining ownership question for the path itself.

### Correction: the `0x38e00000` block is VIC, not clock control

The earlier “gate ID” interpretation was too optimistic. In-tree S5L87xx
headers define:

- `VIC_BASE = 0x38e00000`
- `VICINTENABLE(v) = VICBASE(v) + 0x10`
- `VICINTENCLEAR(v) = VICBASE(v) + 0x14`

and `crt0.S` resets the VIC using the same addresses:

- `0x38e00014`
- `0x38e01014`

So the writes in `0x22001698` are to the **interrupt controller**, not to a
display-clock gate block.

### Decoded IDs in `0x22001698`

`0x220032fc` splits IDs into:

- bank = `id >> 5`
- bit = `id & 31`

`0x22003838` then sets the corresponding bit in:

- `0x38e00010` = `VIC0INTENABLE`
- `0x38e01010` = `VIC1INTENABLE`

The IDs used in `0x22001698` therefore decode as IRQ lines:

| ID | Bank/bit | Register touched | Callsite | In-tree match | Likely subsystem |
| --- | --- | --- | --- | --- | --- |
| `19` | `0 / 19` | `VIC0INTENABLE` | `0x2200194c` | `IRQ_USB_FUNC` | USB / DFU |
| `33` | `1 / 1` | `VIC1INTENABLE` | `0x22001954` | `IRQ_EXT6` | external IRQ, not LCD |
| `39` | `1 / 7` | `VIC1INTENABLE` | `0x2200193c` | `IRQ_AES` | crypto engine |
| `40` | `1 / 8` | `VIC1INTENABLE` | `0x22001944` | `INT_IRQ40` / no display mapping | unknown, non-LCD |

Important negative evidence:

- LCD would be `IRQ_LCD = 14`
- no `14` appears in `0x22001698`

### `0x38100000` ownership

For S5L8702, in-tree headers define:

- `MIU_BASE = 0x38100000`
- `MIUCON = *(REG32_PTR_T)(MIU_BASE)`

So the `0x22001724..0x2200172c` write:

- `0x38100000 &= ~0x7`

targets the **memory interface unit configuration register**, not a
display-owned register block.

### `0x22007784` service dependency

The service area adjacent to the runtime object is now more explicit:

- `0x2200778c = 3`
- `0x22007790...` begins with repeated `"Uart$"` entries
- those entries point at:
  - `0x3cc00000`
  - `0x3cc04000`
  - `0x3cc08000`

`0x22006380`, `0x22006430`, and `0x22006558` walk and configure that service
table. This is consistent with a **generic peripheral/UART service path**, not
with display ownership.

### Final decision

- **EXCLUDE_0x22001698**

Reason:

- the path operates on VIC interrupt enable/clear registers, not on display
  clock gates
- its decoded IDs map to:
  - USB function
  - external IRQ
  - AES
  - unknown IRQ
- none of the IDs map to `IRQ_LCD`
- it also modifies MIU state at `0x38100000`
- it depends on a runtime service object whose nearby static tables are tagged
  `"Uart$"` and point at `0x3cc0xxxx` peripheral blocks

Prepared artifact:

- none
## 2026-04-24 OSOS-derived visibility sequence

OSOS decryption is complete, and the decrypted body at
`/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin` finally exposes a higher-level
Apple-backed display/backlight service path that was not visible in WTF.

### New OSOS evidence

- High-level UI/state strings exist in the decrypted body:
  - `HandleCycleBacklightSetting`
  - `HandleBacklightSelected`
  - `Backlight`
  - `Backlight On`
  - `Backlight Off`
  - `SetBacklight_AlwaysOff`
  - `SetBacklight_AlwaysOn`
  - `TCSlideshowLCD`
- The strongest body-visible display-on sequence is:
  - `0x220057d4`: `bl 0x2200374c`
  - `0x220057d8`: `bl 0x22004c5c`
  - `0x220057dc`: `bl 0x2200374c`
  - `0x220057e0`: `bl 0x22003774`
- Supporting wrappers reinforce the on/off interpretation:
  - `0x22005620`: service getter + `0x2200376c` + state write
  - `0x22005640`: service getter + `0x22003774` + state write

### Minimal OSOS-backed visibility candidate

The smallest new Apple-backed visibility step is now:

1. existing startup/pregate + LCD-local init path
2. `service = 0x2200374c()`
3. `0x22004c5c(service)`
4. `service = 0x2200374c()`
5. `0x22003774(service)`
6. loop

Why this is the current best candidate:

- it comes directly from decrypted OSOS, not guessed MMIO
- it is shorter and more targeted than replaying broad OSOS UI logic
- it adds one bounded higher-level display/backlight service step on top of the
  already-tested WTF LCD path

### Remaining limitation

The exact hardware-facing PMU/GPIO/MMIO writes are still hidden behind Apple
ROM/service imports:

- `0x080646b4`
- `0x08064790`
- `0x080db704`
- `0x080dbe58`

So this sequence is Apple-backed and concrete at the call level, but not yet
fully reduced to raw register writes.

### Prepared artifact

- `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.bin`
- `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.elf`
- `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.map`

Host-side validation:

- entrypoint `0x22000000`
- ELF `.text` size `0x40f0`
- payload prepared only, not executed

### 2026-04-24 single hardware run result

`lcd-osos-visible-n3g.bin` has now been exercised once on hardware.

Host-side result:

- clean pre-send DFU baseline: state `2`
- upload succeeded
- post-send USB still enumerated as `05ac:1223`
- post-send `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- recovery after manual reset was successful and restored DFU state `2`

Visual-result limitation:

- the intended 20-second screen observation was not captured reliably during
  this run

So this specific run is recorded as:

- **EXECUTION ONLY**
- note:
  - **no visual observation captured**

This means the OSOS-derived service sequence remains a plausible visibility
candidate, but this run does not provide evidence of visible LCD/backlight
success.
