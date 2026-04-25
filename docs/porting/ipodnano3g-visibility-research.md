# iPod Nano 3G Visibility Research

## 2026-04-25 Reconciliation note

Current Nano 3G visibility/chainload results must be read with one correction:

- the active local `/tmp/wInd3x/pkg/cfw/defang_wtf.go` is now an experimental
  staged patch stack
- it is no longer just the earlier minimal Nano 3G defanger state described in
  some older notes

Active local Nano 3G patch categories now include:

- restored original WTF control-flow edges
- local readiness stub
- local loader callback stub
- local UART immediate-return bypass
- marker/probe stubs in free WTF body space

That means the current black-screen / WTF-stuck results reflect:

- a later experimental Nano 3G handoff branch

and should not be over-attributed to the earlier simpler `0x1990` / `0x19b8`
story alone.

MacPod note:

- the device being a **MacPod** instead of a **WinPod** does not explain the
  current visibility failures
- those failures are still occurring before RetailOS/UI/filesystem behavior is
  reached

Date: 2026-04-23
Scope: first safe execution visibility signal after confirmed DFU takeover

## Goal

Find the safest evidence-backed hardware action that could visibly confirm
payload execution on the Nano 3G without relying on guessed MMIO writes.

## Sources Reviewed

- `firmware/target/arm/s5l8702/ipodnano3g/backlight-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/pmu-target.h`
- `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/lcd-nano3g.c`
- `firmware/target/arm/s5l8702/lcd-s5l8702.c`
- `firmware/target/arm/s5l8702/ipodnano3g/piezo-nano3g.c`
- `bootloader/ipod-s5l87xx.c`
- `docs/porting/ipodnano3g-ghidra-triage.md`
- `tools/ghidra/Nano3GHwInitTriage.py`
- `/tmp/n3g-wtf-decrypted.body.bin`

## Local Firmware Artifact Status

- Decrypted Nano 3G WTF is available locally:
  - `/tmp/n3g-wtf-upstream.dfu`
  - `/tmp/n3g-wtf-decrypted.dfu`
  - `/tmp/n3g-wtf-decrypted.body.bin`
- Ghidra is not installed on this host, so triage was performed with:
  - `arm-elf-eabi-objdump`
  - `xxd`
  - source cross-checking

## Evidence Table

| Candidate | Subsystem | Address / function | Evidence | Confidence | Risk | Expected result | Recovery impact |
|---|---|---|---|---|---|---|---|
| Existing DFU liveness loss | RAM-only / external | `wInd3x run` + `mks5lboot --dfuscan` | repeated real-device tests in session log | High | Very low | host sees `05ac:1223`, DFU probe fails | None beyond manual reset |
| Piezo tone | Timer + GPIO + piezo | `piezo_tone()`, `piezo_seq()`, `PCON0`, `GPIOCMD`, `TACMD` | `piezo-nano3g.c`, `bootloader/ipod-s5l87xx.c` development beeps | Medium | Medium | audible beep | Low; manual reset |
| Backlight enable | PMU / I2C | `D1671_REG_LEDCTL = 0x20`, `D1671_LEDCTL_ENABLE = 0x80` | Rockbox Nano 3G PMU/backlight code; Apple WTF confirms PMU/I2C path but no explicit LEDCTL write found yet | Medium | Medium | backlight on | Low if correct; still blocked by missing Apple LEDCTL observation |
| LCD awake only | LCD controller + panel command set | preamble at `0x45bc`, selector at `0x4438` / `0x4f58`, awake table at `0x70d1` | decrypted Apple WTF + `lcd-nano3g.c` / `lcd-s5l8702.c` cross-check | High | Medium | panel wake or black-to-lit transition | Low-medium; payload prepared but not yet tested |
| LCD full init | LCD controller + panel | `lcd-s5l8702.c` + `lcd-nano3g.c` init sequences | in-tree target code only, many `TBC` comments in controller layer | Low-medium | High | valid framebuffer or text | Low-medium if correct, but broader hardware writes |
| PMU/LDO rail programming | PMU power | multiple `pmu_wr()` calls in `pmu_preinit()` | `pmu-nano3g.c` contains many `TBC` comments | Low | High | uncertain | Higher; avoid |

## Ranking By Safety

1. Existing DFU liveness loss
   - Safest confirmed execution-adjacent signal.
   - Not visible, but already evidence-backed.

2. Piezo tone
   - Best source-backed external confirmation path that does not depend on LCD
     or PMU.
   - Not visible, but likely lower risk than LCD writes.

3. Backlight enable
   - Still blocked by lack of an explicit Apple LEDCTL write in the decrypted
     WTF.

4. LCD awake only
   - Now reduced enough to prepare a first LCD payload without guessing the
     command-mode family.
   - Still medium risk because it is real LCD MMIO.

5. LCD full init
   - Broader than necessary for first visibility.

## Piezo Proof Path

The cleanest remaining physical proof channel is now the Nano 3G piezo path.

What is source-backed:

- `firmware/target/arm/s5l8702/ipodnano3g/piezo-nano3g.c`
- `bootloader/ipod-s5l87xx.c`
- close corroboration from the sibling S5L8702 targets:
  - `ipod6g/piezo-6g.c`
  - `ipodnano4g/piezo-nano4g.c`

What this path does not use:

- no codec or DAC path
- no PMU writes
- no NAND or storage
- no LCD or backlight
- no USB PHY work

### Exact Minimal Sequence

Bootloader `alive[]` sequence:

- period: `500` us
- duration: `100` ms
- gap: `0`

Bootloader-style `piezo_tone()` reduction for Nano 3G:

1. Read `PCON0` at `0x3cf00000`
2. Write `PCON0 = (PCON0 & ~0xff000000) | 0x53000000`
3. For about `100000` us total:
   - alternate `GPIOCMD` at `0x3cf00200` between:
     - `0x0000060e`
     - `0x0000060f`
   - wait `250` us between toggles using `USEC_TIMER` at `0x3c7000b4`
4. Read `PCON0` again
5. Write `PCON0 = (PCON0 & ~0xff000000) | 0xee000000`

Expected audible result:

- one short beep around `2 kHz`
- about `100 ms` long

### Apple Evidence Status

This path is not yet reduced from a clean Apple WTF/OSOS piezo routine in the
same way as the LCD path.

What was checked:

- decrypted WTF and OSOS were searched for:
  - `0x53000000`
  - `0xee000000`
  - `0x0060e`
  - `0x3cf00200`
  - `0x3cf00000`
  - `0x3c7000b4`

Result:

- no direct Apple piezo helper was isolated with enough confidence to claim a
  full Apple-backed raw call path
- however, the Nano 3G in-tree path is narrow, explicit, and matches sibling
  S5L8702 targets closely

### Risk

- Risk level: medium-low
- Why:
  - only three hardware regions are touched:
    - `0x3cf00000` (`PCON0`)
    - `0x3cf00200` (`GPIOCMD`)
    - `0x3c7000b4` (`USEC_TIMER`, read-only)
  - the path avoids the wider timer/PWM interrupt setup used by the full
    firmware piezo driver
  - the path still changes live GPIO state, so it is not zero-risk

### Prepared Artifact

Prepared only, not run:

- [tools/ipodnano3g/minimal_payload/piezo-beep-n3g.S](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/piezo-beep-n3g.S:1)
- `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.bin`
- `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.elf`
- `tools/ipodnano3g/minimal_payload/piezo-beep-n3g.map`

## Early System Init Shift

Both of the following now have confirmed execution but no external effect:

- LCD/backlight visibility payloads
- piezo proof payload

That makes a missing global prerequisite more likely than another
subsystem-specific detail.

Current best bounded early-init candidate from OSOS:

- `0x22002770`

Why it matters:

- it is earlier and more global than the LCD or piezo code paths
- it performs:
  - CP15 control changes
  - MIU programming at `0x38100000`
  - clock/reset-style programming at `0x3c500000`
  - global controller programming at `0x39900000`
- it avoids LCD and audio-local code directly

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`

Important caveat:

- this probe is Apple-backed at the call level, but not fully raw, because the
  wrapper still depends on unresolved OSOS imports:
  - `0x22003414 -> 0x08016234`
  - `0x220034bc -> 0x0801542c`

## Backlight Status

Evidence in favor:

- Nano 3G has a dedicated PMU LED register definition:
  - `D1671_REG_LEDCTL = 0x20`
  - `D1671_LEDCTL_ENABLE = 0x80`
- Decrypted Apple WTF confirms the PMU transport path:
  - I2C0 engine at `0x3c600000`
  - PMU slave `0x73`
  - PMU read/write helpers at `0x22005420` / `0x22005474`

Evidence still missing:

- An explicit Apple-backed `LEDCTL (0x20)` read/modify/write in the decrypted
  WTF.

Conclusion:

- Backlight remains second-choice.
- It is still not the first visible hardware write to test.

## LCD Status

Evidence in favor:

- Apple LCD controller preamble at `0x220045bc`
- Apple command-mode helper at `0x220042c8`
- Apple sequence interpreter at `0x22004624`
- Apple awake table at body offset `0x70d1`
- Apple ready wait at `0x22004d74`
- Apple delay helpers on timer counter `0x3c7000b4`

New finding:

- the short awake tail is only part of the Apple LCD path
- fuller Apple panel-init tables also exist at:
  - body `0x7018`
  - body `0x70dc`
  - body `0x728b`
- the remaining missing dependency is now earlier than the panel commands:
  - runtime callbacks through the object at `0x22007398`

### Mode Selection Reduction

The remaining LCD blocker is now reduced:

- `0x220042c8` calls `0x22004438`
- `0x22004438` reads a cached selector at `0x2200700c`
- if the cached value is `4`, it reads GPIO52 and GPIO53 using `0x22004f58`
- `0x22004f58` reads the GPIO bank data register on base `0x3cf00000`
- the selector is:
  - `selector = gpio52 | (gpio53 << 1)`

Selector grouping:

- selector `0` / `1` -> command-mode low bits `0x0c20`
- selector `2` / `3` -> command-mode low bits `0x0da8`

After the preamble value `0x80100db1`, the exact config writes become:

- `0x80000c21`
- `0x80000da9`

This means the early Apple mode choice depends on **GPIO straps**, not LCD ID
readback.

### Rockbox Cross-Check

Confirmed matches:

- `0x0c20` matches Rockbox `LCD_MODE_P8`
- `0x0da8` matches Rockbox `LCD_MODE_P18`
- Apple awake commands are `0x11`, delay, `0x13`, `0x29`
- Rockbox Nano 3G awake path already uses `0x11` and `0x29`

Caveat:

- Current Rockbox Nano 3G panel detection reads LCD ID later in bring-up.
- Apple’s early mode-family choice uses GPIO straps first.

Implication:

- For the first LCD visibility test, the Apple strap-based selector is the
  stronger evidence source.

## Prepared Candidate

Prepared only, not run:

- [tools/ipodnano3g/minimal_payload/lcd-awake-n3g.S](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-awake-n3g.S:1)

Payload behavior:

1. apply the Apple LCD preamble
2. read GPIO52 / GPIO53 exactly as Apple does
3. select the Apple-backed command-mode family at runtime
4. send the Apple awake sequence
5. loop forever

Explicitly not included:

- NAND
- PMU / backlight
- USB PHY
- audio
- storage
- framebuffer update or full LCD init

## Current Blocker

- The blocker is no longer “which command-mode family to use”.
- The blocker is that the full cold-init path still depends on unresolved
  runtime callbacks through `0x22007398` before `0x45bc`.
- Confirmed slot layout:
  - `+0x04`: mode/state transition hook
  - `+0x08`: context handoff hook receiving `0x22007338`
  - `+0x28`: earlier setup/query hook receiving `0x22007338`
  - `+0x2c`: final no-argument hook immediately before `0x2200455c()`
- No direct in-image write to `0x22007398` has been found in the decrypted WTF
  body, so the callback service appears to be provided externally.

## Runtime Service Findings

Primary WTF entry:

- `0x22001420`

Confirmed early startup behavior:

- calls local setup helpers before any high-level state-machine work:
  - `0x22001124`
  - `0x220010f8`
  - `0x220010e4`
  - `0x22001394`
  - `0x220013fc`
- switches CPU modes and installs separate IRQ/SVC stacks
- copies/zeros in-image state through `0x22007810`
- calls `0x22002f20`
- then issues `svc 0x00123456` with `r0 = 24`, `r1 = 0x00020026`

What this rules out:

- there is no observed early copy from incoming `r0`/`r1`/`r2`/`r3` into
  `0x22007398`
- the static WTF body therefore does not show `0x22007398` being populated by a
  simple entry-register handoff from bootrom

What it supports:

- the WTF uses a broader service ABI based around `svc 0x00123456`
- `0x22007398` is more likely an external runtime/loader-managed service object
  than a normal in-image static object

Classification:

- bootrom/runtime/loader-provided external service: Medium confidence
- direct bootrom register handoff into `0x22007398`: low confidence / not
  observed

## Callback Slot Assessment

| Slot | Wrapper | Observed arguments | Likely role | Omission risk |
|---|---|---|---|---|
| `+0x04` | `0x22003ce0` | `1`, `3`, `4` | mode/state transition | unsafe to omit |
| `+0x08` | `0x22003d14` | `0x22007338` | context registration/handoff | likely required |
| `+0x28` | `0x22003c98` | `0x22007338` | earlier setup/open/query | likely required |
| `+0x2c` | `0x22003cc0` | none explicit | finalize/commit/start | unknown, treat as required |

Bypass decision:

- the later direct LCD path cannot yet be called “safe to bypass”
- the remaining blocker is not the LCD command list anymore; it is the missing
  external service stage immediately before `0x2200455c()`

## Service Stub Reduction

New control-flow finding:

- the four service wrappers are only called from the LCD state path around
  `0x220031b4`
- none of their return values are used for later branching in that path

Minimum table required to satisfy the observed ABI:

- pointer written to `0x22007398`
- valid function pointers at:
  - `+0x04`
  - `+0x08`
  - `+0x28`
  - `+0x2c`

Minimum stub behavior:

- each callback returns `0`
- no MMIO
- no writes other than installing the table pointer in RAM

Interpretation:

- this is sufficient to avoid the wrappers’ built-in `0x11` fallback path
- it is **not** sufficient to prove that the original runtime service had no
  hidden hardware or resource-gating side effects

Practical integration limit:

- a tiny standalone DFU payload does not automatically contain the Apple WTF
  routines at `0x2200455c`, `0x220048bc`, and related addresses
- so the service-table stub can be prepared independently, but a runnable LCD
  payload still needs explicit translated/copied LCD code after the stub layer

Prepared artifact:

- [tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.S](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.S:1)
- built outputs:
  - `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.bin`
  - `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.elf`
  - `tools/ipodnano3g/minimal_payload/lcd-service-stub-n3g.map`

Verified behavior:

- writes only the service-table pointer at `0x22007398`
- installs four `return 0` callbacks at the observed offsets
- then loops forever

## Embedded LCD Payload Status

Prepared only, not run:

- [tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.S](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.S:1)
- [tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.lds](/home/david/Documents/RockBox_Personal-master/tools/ipodnano3g/minimal_payload/lcd-fullinit-n3g.lds:1)

Why embed won over full translation:

- the Apple LCD chain is dense with PC-relative literal pools and absolute
  intra-image branches
- the payload still loads at `0x22000000`
- preserving original VMAs is lower risk than hand-rewriting the interpreter,
  helper dispatch, panel records, and `OP1` side path

Embedded Apple slices:

- code:
  - body `0x664..0x69b`
  - body `0x828..0x8ef`
  - body `0x3b18..0x4ddb`
- data:
  - body `0x7000..0x782f`

Local additions:

- offset-0 bootstrap
- minimal `0x22002f04` no-op stub for the original error-print path
- explicit zero-filled work-buffer pad `0x22007830..0x22008000`

Current interpretation:

- this is the first self-contained LCD payload with no remaining dependency on
  unavailable WTF code outside the embedded slices
- the remaining uncertainty is semantic, not linkage:
  - whether the chosen top-level call order is the minimal visible sequence for
    all real panels

## Pre-LCD Prerequisite Update

The first full embedded LCD run stayed black even though DFU takeover was
confirmed again. That shifts the current blocker earlier than the LCD controller
and panel-command code itself.

Strongest Apple-backed preconditions now observed:

1. PMU transport and at least some PMU register programming occur before the
   LCD state path.
2. Earlier `0x22000664` resource/clock gate writes occur outside the
   LCD-local `0x2200428c` wrapper.
3. A wider pre-LCD state stage occurs before the LCD path, including:
   - early clock/reset helpers under `0x22001f4c`
   - GPIO state save/restore through `0x220030f0` / `0x22003130`
   - a sideband helper through `0x220061f4`, whose LCD relevance is now
     doubtful

### Concrete PMU Evidence

Confirmed Apple helpers:

- `0x22005420`: PMU read helper on slave `0x73`
- `0x22005474`: PMU write helper on slave `0x73`

Confirmed PMU operations:

- `0x220054b0`: write PMU reg `0x1d = 0x0a`
- `0x220054b0`: write PMU reg `0x1b = 0x01` or `0x00`
- `0x220054f8`: read-modify-write PMU reg `0x43` bit `0`

Call chain evidence:

- `0x22003018` -> `0x220054b0`
- `0x2200304c` -> `0x220054f8`
- `0x22003078` conditionally calls both

Interpretation:

- PMU state really is part of the earlier platform bring-up.
- The currently observed PMU writes sit behind `0x22003078(1)`, while the
  LCD state path that reaches `0x2200455c` goes through `0x220031b4(5)` /
  `0x220031b4(6)` and uses `0x22003078(4)`.
- So the currently observed PMU writes are best treated as **adjacent platform
  state**, not as part of the immediate Apple LCD startup branch.
- This is still not enough to justify a backlight or broader PMU test because
  no explicit Apple `LEDCTL (0x20)` write has been found.

### Concrete Earlier Gate / Clock Evidence

Confirmed earlier `0x22000664` calls:

- `0x22001630`: `r0=0x10000`, `r1=0`, `r2=1`
- `0x22001650`: `r0=0x10000`, `r1=0`, `r2=0`
- `0x220018d8`: `r0=0x400`, `r1=0`, `r2=1`
- `0x220018e8`: `r0=0x1`, `r1=0`, `r2=1`
- `0x220018fc`: `r0=0`, `r1=0x2000`, `r2=1`
- `0x2200205c`: `r0=0x2007df65`, `r1=0x1ef49`, `r2=0`
- `0x2200206c`: `r0=0x06002082`, `r1=0x1036`, `r2=1`

Additional early register evidence:

- `0x22001634..0x22001640`: clears bits `0..2` at `0x3930003c`
- `0x220017e8..0x22001804`: toggles bit `1` at `0x38400804` with delays

Interpretation:

- The LCD-local gate wrapper is not enough by itself.
- Some earlier platform resource state is probably required before the
  controller preamble can have visible effect.
- The startup-local versus later-path split is now clearer:
  - `0x22001630` / `0x22001650` are on the direct startup function
    `0x2200160c`, which `0x22002f20` calls before later state-machine work
  - `0x220018d8` / `0x220018e8` / `0x220018fc` sit in the later
    `0x22001698` path reached only through the tail branch at `0x22002fe4`
  - `0x2200205c` / `0x2200206c` are part of `0x22001f4c`, which *is* on the
    direct startup path
- `0x22000664` is now fully reduced:
  - `r2 == 0` sets bits
  - `r2 != 0` clears bits

### Concrete GPIO / Pinmux Evidence

Confirmed state-buffer path:

- `0x220030f0`: restores 16 packed GPIO descriptors into live GPIO group
  registers
- `0x22003130`: snapshots 16 live GPIO group registers into packed RAM
  descriptors and normalizes output nibbles using the live data bits

Important reduction:

- the RAM buffer used by those helpers is `0x2200791c`
- `0x2200791c` is **outside** the decrypted WTF body and lies in the zero-padded
  work area of the embedded payload
- descriptor format is now explicit:
  - 16 entries
  - 8 bytes per entry
  - `+0x00..+0x03`: packed `PCON(group)` nibble config
  - `+0x04`: `PUNB(group)` low byte
  - `+0x05`: `PUNC(group)` low byte
  - `+0x06..+0x07`: unused
- `0x22003130` reads from live GPIO registers:
  - `PCON(group)` at `0x3cf00000 + group*0x20 + 0x00`
  - `PDAT(group)` at `0x3cf00000 + group*0x20 + 0x04`
  - `PUNB(group)` at `0x3cf00000 + group*0x20 + 0x0c`
  - `PUNC(group)` at `0x3cf00000 + group*0x20 + 0x10`
- `0x220030f0` writes back only:
  - `PCON(group)` at `+0x00`
  - `PUNB(group)` at `+0x0c`
  - `PUNC(group)` at `+0x10`
- so these helpers are moving an existing GPIO-group state set around; they are
  not by themselves evidence of a fresh LCD pin definition being created from
  literals
- the dependency is now more concrete:
  - state `4/5` calls `0x220030f0(0x2200791c)` before the later LCD-local work
  - the only in-body producer of that buffer is state `1/2` calling
    `0x22003130(0x2200791c)`
  - so replaying Apple's full state machine would skip a real GPIO-state
    transfer, not just padding

Earlier configuration helpers:

- `0x22001f4c` calls:
  - `0x22000958`
  - `0x22003890`
  - `0x220039fc`
  - and brackets the sequence with `0x22003c18(0)` / `0x22003c18(1)`

Closer GPIO-command stage:

- the LCD-adjacent branch actually calls `0x220061f4(3)`
- that path falls into `0x22005ef4` / `0x220060e0`
- those routines:
  - manipulate pins `72..75` via `0x22004f0c` -> `0x3cf00200`
  - operate on the `0x3c200000` peripheral block
- local triage notes classify `0x3c200000` as
  `WHEEL_OR_NAND_COLLISION`

Interpretation:

- Apple is performing wider pre-LCD state work before the LCD code we embedded.
- `0x22001f4c` now looks more like early clock/reset configuration than pure
  GPIO pinmux.
- the corrected `0x22001f4c` call graph is narrower than earlier notes implied:
  - it does **not** call `0x22003350`
  - it does **not** depend on the excluded `0x220061f4` / `0x2200791c` path
  - it does touch:
    - `0x3c500000`
    - `0x3c500004`
    - `0x3c500008`
    - `0x3c50000c`
    - `0x3c500010`
    - `0x3c500020`
    - `0x3c500024`
    - `0x3c500028`
    - `0x3c500040`
    - `0x3c500044`
    - `0x3c500048`
    - `0x3c50004c`
    - `0x39900000`
    - `0x39300000`
    - `0x38100000`
    - `0x38501000`
- `0x220030f0` / `0x22003130` now look like a real state dependency around a
  RAM-backed descriptor set, not a cosmetic helper:
  - state `4/5` restores `0x2200791c`
  - only state `1/2` populates it in-body
- the most likely purpose is generic GPIO preservation around the sideband
  `0x220061f4` helper, not a hidden input to `0x2200455c`
- that makes the current best decision:
  - for a standalone LCD payload that intentionally omits `0x220061f4`,
    `0x2200791c` is currently **bypass-safe**
  - for any state-machine-faithful replay that keeps `0x220061f4`,
    `0x2200791c` remains required and unresolved
- `0x22001f4c` is no longer classified as bypass-safe:
  - it is on the direct startup path
  - it shares the later LCD-wrapper register family through
    `0x2200206c -> 0x22000664`, which clears bit `1` in `0x3c500048`
  - the safest current standalone candidate is therefore to include the full
    corrected `0x22001f4c` block, not to guess a smaller subset
- `0x220061f4` should currently be treated as a sideband pre-state helper, not
  as a clean LCD reset/power candidate.

## Recommended Next Step

If staying fully non-invasive:

1. keep the prepared `lcd-pregate-fullinit-n3g` payload unrun
2. treat the corrected full `0x22001f4c` block as the current minimal
   Apple-backed pre-LCD startup subset
3. continue reducing it only if later evidence proves some of its writes are
   unnecessary
4. keep `0x220061f4` and `0x2200791c` out of the standalone candidate unless a
   future test intentionally reintroduces the Apple sideband path

## Decision

- Current pre-LCD decision: **INCLUDE MINIMAL SUBSET**
- Included subset:
  - corrected full `0x22001f4c` block
  - direct startup gates from `0x22001630`, inline `0x3930003c &= ~0x7`,
    `0x22001650`
  - reduced runtime service table
  - embedded LCD-local full init
- Excluded:
  - PMU path
  - `0x220061f4` / `0x3c200000`
  - `0x2200791c` restore path
- Prepared artifact: `tools/ipodnano3g/minimal_payload/lcd-pregate-fullinit-n3g.bin`
- Run status: **not executed**

## Minimal Power-Step Revisit

The next visibility refinement pass re-opened only the narrowest candidate
power actions and kept the evidence bar strict.

What was re-checked:

- Apple-observed PMU writes:
  - `0x1d = 0x0a`
  - `0x1b = 0x01/0x00`
  - `0x43` bit `0` read-modify-write
- single-pin activity near the pre-LCD state path

Result:

- `0x1b` remains a poor first visibility candidate:
  - Rockbox Nano 3G already has `pmu_hdd_power(bool on)` writing `0x1b`
  - that makes it look more like generic storage / rail control than panel
    visibility
- `0x43` bit `0` also remains out:
  - the only Apple-observed polarity is `0x2200304c(0)`
  - that clears bit `0`
  - using `0x2200304c(1)` as an “enable” would be speculative
- no safe GPIO reset pulse was recovered:
  - the only clear single-pin activity remains under `0x220061f4`
  - that path still reaches `0x3c200000`
  - so it stays excluded

Chosen single-step candidate:

- PMU write on slave `0x73`
- register `0x1d`
- value `0x0a`

Why this one:

- exact Apple write in decrypted WTF
- one register, one byte
- narrower than replaying the `0x1d` + `0x1b` helper pair
- avoids guessed `0x43` polarity
- avoids non-Apple `LEDCTL (0x20)` injection

Prepared artifact, not run:

- `tools/ipodnano3g/minimal_payload/lcd-powerstep-n3g.bin`

Added step relative to `lcd-pregate-fullinit-n3g`:

1. service-table install and panel-state seed
2. **new:** `apple_05474(0x1d, 1, &0x0a)`
3. existing pre-gate startup subset
4. existing embedded LCD-local full init
5. loop forever

Risk assessment:

- Medium
- the single new PMU write is narrow, but the Apple PMU transport still drives
  the `0x3c600000` I2C controller and uses the `0x3c7000b4` timer basis for
  timeout polling
- still materially safer than adding unobserved PMU or GPIO behavior

## Minimal Power-Step Revisit, Revision 2

With the first `0x1d = 0x0a` power-step still not yielding visible output, the
next smallest Apple-backed refinement is to add one more PMU action that is
already present in the decrypted WTF.

Chosen addition:

- `apple_054f8(0)`
- effect: read PMU reg `0x43`, clear bit `0`, write it back

Why this one:

- exact Apple-backed helper behavior
- no new register beyond the already observed PMU set
- no guessed polarity; Apple only observed with argument `0`
- narrower than replaying the broader `0x22003078(1)` PMU branch

Still excluded:

- `apple_054f8(1)` or any bit-set polarity
- reg `0x1b`
- `LEDCTL (0x20)`
- any GPIO pulse from `0x220061f4`

Prepared artifact, not run:

- `tools/ipodnano3g/minimal_payload/lcd-powerstep2-n3g.bin`

Exact PMU sequence in the prepared payload:

1. write reg `0x1d = 0x0a`
2. read-modify-write reg `0x43`, clear bit `0`
3. existing pregate startup subset
4. existing embedded LCD-local full init
5. loop forever

Risk assessment:

- Medium
- still bounded to Apple PMU/I2C transport and the same timer-backed timeout
  helpers already used by the first power-step payload
- no unrelated PMU, GPIO, USB, storage, or audio path added

## Root-Cause Isolation Result

Two remaining candidates were re-traced:

1. PMU reg `0x1b`
2. sideband path `0x220061f4`

### PMU reg `0x1b`

Evidence:

- only observed here through `0x220054b0`
- only called on the early PMU branch:
  - `0x22003078(1)` -> `0x22003018(0)` -> `0x220054b0`
- local Nano 3G source maps `0x1b` to:
  - `pmu_hdd_power(bool on)`

Assessment:

- best classification: **unrelated / storage-oriented**
- not on the immediate LCD branch

### Sideband `0x220061f4`

Evidence:

- is on the immediate LCD-adjacent branch:
  - states `5/6` run `0x220061f4(3)` before the LCD call chain
- mode `3` dispatches to:
  - `0x22005ef4`
  - `0x220060e0`
- these routines:
  - configure pins `72..75` via `0x22004f0c`
  - perform direct programming on `0x3c200000`
  - store additional sideband state under `0x22007788`

Assessment:

- ordering argues it may be required
- ownership evidence does **not** yet prove it is LCD-related rather than a
  clickwheel / sideband precondition
- no safe minimal subset is justified yet

## Pin Ownership Verification Result

Pins `72..75` now reduce further than “still ambiguous”.

What mode `3` actually does:

- `0x22006260`: delay `25`
- `0x22006268`: call `0x22005ef4`
- `0x22006270`: tail-call `0x220060e0`

`0x22005ef4` exact GPIO sequence:

- `GPIO72 <- op2`
- delay `1`
- `GPIO73 <- op2`
- `GPIO74 <- op2`
- `GPIO75 <- op2`

`0x220060e0` / `0x2200616c` exact peripheral behavior:

- operate on `0x3c200000`
- clear then set bit `0x200000` in `0x3c200000`
- write command payload to `0x3c20001c`
- set bit `0` in `0x3c200004`
- poll `0x3c20000c` with timeout `0x5dc`

Cross-check:

- `0x3c200000` is already identified locally as `WHEEL_BASE`
- Nano 3G GPIO defaults set group `9` to `0x22222222`
- pins `72..75` are group `9`, so `op2` matches the wheel-side alternate
  function default

Result:

- **NOT_LCD**

Reason:

- the sideband path looks like clickwheel-peripheral restoration/command
  sequencing, not panel reset or panel power enable
- no low -> delay -> high reset pulse was found on the same pin
- no LCD-local routine directly references these pins

Prepared artifact:

- none

## OSOS higher-phase decrypt exited status

- Higher-phase search is still blocked on incomplete plaintext, but the most
  recent completed decrypt span ended at:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `3461328` bytes
  - real completion = `32.078%`
- Post-exit USB state:
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`
  - so the next safe step is another clean DFU re-entry before resuming

## OSOS higher-phase decrypt live status 6

- Higher-phase search is still blocked on incomplete plaintext, but the live
  recovery-backed decrypt has now reached:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `3347856` bytes
  - real completion = `31.027%`
- Current USB state:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - this still matches an active decrypt holding the DFU session open

## OSOS higher-phase decrypt live status 5

- Higher-phase search is still blocked on incomplete plaintext, but the live
  recovery-backed decrypt has now reached:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `3243168` bytes
  - real completion = `30.056%`
- Current USB state:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - this still matches an active decrypt holding the DFU session open

## OSOS higher-phase decrypt live status 4

- Higher-phase search is still blocked on incomplete plaintext, but the live
  recovery-backed decrypt has now reached:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `3138624` bytes
  - real completion = `29.088%`
- Current USB state:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - this still matches an active decrypt holding the DFU session open

## OSOS higher-phase decrypt live status 3

- Higher-phase search is still blocked on incomplete plaintext, but the live
  recovery-backed decrypt has now reached:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2992032` bytes
  - real completion = `27.729%`
- Current USB state:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - this still matches an active decrypt holding the DFU session open

## OSOS higher-phase decrypt live status 2

- Higher-phase search is still blocked on incomplete plaintext, but the live
  recovery-backed decrypt has now reached:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2923536` bytes
  - real completion = `27.094%`
- Current USB state:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - this matches an active decrypt still holding the DFU session open

## OSOS higher-phase decrypt status update 17

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2666640` bytes
  - real completion = `24.719%`
- The latest automatic loop advanced through another clean-DFU slice and one
  usable-DFU-state-`9` slice before returning to the stale host-visible
  pattern:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt live status

- Higher-phase search is still blocked on incomplete plaintext, but the live
  recovery-backed decrypt has now reached:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2794128` bytes
  - real completion = `25.895%`
- Current USB state:
  - `mks5lboot --dfuscan` returns `LIBUSB_ERROR_BUSY`
  - this matches an active decrypt still holding the DFU session open

## OSOS higher-phase decrypt status update 15

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2476224` bytes
  - real completion = `22.949%`
- The first automatic resume slice ended with DFU still usable in state `9`,
  so the automatic loop continued without user intervention.

## OSOS higher-phase decrypt status update 16

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2591376` bytes
  - real completion = `24.019%`
- The second automatic resume slice started from usable DFU state `9`, then
  returned DFU to the stale host-visible pattern after checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `1720752` bytes
  - real completion = `15.947%`
- The latest pre-resume check did not start a new decrypt slice because the
  Nano was only host-visible, not cleanly usable in DFU:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update 2

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `1774224` bytes
  - real completion = `16.443%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and then returned to the stale host-visible DFU pattern after
  checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update 3

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `1828560` bytes
  - real completion = `16.941%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and returned to clean DFU state `2` after checkpointing.
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains resuming the same recovery file from the newer
    checkpoint

## OSOS higher-phase decrypt status update 4

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `1884288` bytes
  - real completion = `17.459%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and then returned to the stale host-visible DFU pattern after
  checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update 5

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `1935696` bytes
  - real completion = `17.938%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and then returned to the stale host-visible DFU pattern after
  checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update 6

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `1994448` bytes
  - real completion = `18.482%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and then returned to the stale host-visible DFU pattern after
  checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update 7

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2044896` bytes
  - real completion = `18.952%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and returned to clean DFU state `2` after checkpointing.
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains resuming the same recovery file from the newer
    checkpoint

## OSOS higher-phase decrypt status update 8

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2096064` bytes
  - real completion = `19.428%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and returned to clean DFU state `2` after checkpointing.
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains resuming the same recovery file from the newer
    checkpoint

## OSOS higher-phase decrypt status update 9

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2147184` bytes
  - real completion = `19.894%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and then returned to the stale host-visible DFU pattern after
  checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update 10

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2197776` bytes
  - real completion = `20.367%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and then returned to the stale host-visible DFU pattern after
  checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS higher-phase decrypt status update 11

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2254272` bytes
  - real completion = `20.891%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and returned to clean DFU state `2` after checkpointing.
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains resuming the same recovery file from the newer
    checkpoint

## OSOS higher-phase decrypt status update 12

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2308800` bytes
  - real completion = `21.402%`
- The latest bounded resume started from clean DFU state `2`, advanced
  normally, and returned to clean DFU state `2` after checkpointing.
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains resuming the same recovery file from the newer
    checkpoint

## OSOS higher-phase decrypt status update 13

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2365104` bytes
  - real completion = `21.918%`
- The first automatic resume slice ended with DFU still usable in state `2`,
  so the decryption loop was able to continue without user intervention.

## OSOS higher-phase decrypt status update 14

- The higher-phase search is still blocked on incomplete plaintext, but the
  current recovery-backed decrypt checkpoint is now:
  - `/tmp/n3g-osos-work/n3g-osos.recovery` = `2421456` bytes
  - real completion = `22.435%`
- The second automatic resume slice returned DFU to the stale host-visible
  pattern after checkpointing:
  - `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- Impact on visibility search remains unchanged:
  - no new OSOS-derived display/backlight candidate can be promoted yet
  - next safe step remains restoring clean DFU and resuming the same recovery
    file

## OSOS Decrypt Progress Update

Result:

- **OSOS_DECRYPT_STILL_IN_PROGRESS**

New checkpoints:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `165696` bytes
  - real completion = `1.536%`
- `/tmp/n3g-osos-work/n3g-osos.recovery` = `285648` bytes
  - real completion = `2.647%`

Notes:

- the resumable decrypt continues to make forward progress against local
  `OSOS.fw`
- the most recent resume succeeded even from DFU state `3`
- repeated `libusb: interrupted [code -10]` messages occurred during the long
  run but did not corrupt the saved recovery state
- after stopping, the Nano again fell into the familiar stale host-visible DFU
  condition where `lsusb` still shows `05ac:1223` but `mks5lboot --dfuscan`
  fails with `LIBUSB_ERROR_OTHER`

Impact on visibility search:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `350064` bytes
  - real completion = `3.244%`

Operational note:

- this resume started from clean DFU state `2`
- the decrypt continued making forward progress despite repeated
  `libusb: interrupted [code -10]` messages
- after checkpointing, host state again fell back to the standard stale-DFU
  pattern where `mks5lboot --dfuscan` returns `LIBUSB_ERROR_OTHER`

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `420240` bytes
  - real completion = `3.895%`

Updated impact on visibility search:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `483696` bytes
  - real completion = `4.481%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1643520` bytes
  - real completion = `15.235%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1574688` bytes
  - real completion = `14.595%`

Operational note:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1482672` bytes
  - real completion = `13.740%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1414464` bytes
  - real completion = `13.107%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1346016` bytes
  - real completion = `12.473%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1054848` bytes
  - real completion = `9.775%`

Operational note:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1273968` bytes
  - real completion = `11.803%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1202784` bytes
  - real completion = `11.148%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `1137024` bytes
  - real completion = `10.538%`

Operational note:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `850560` bytes
  - real completion = `7.882%`

Operational note:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano remained host-visible and still answered DFU,
  but in state `9` rather than clean state `2`

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `921840` bytes
  - real completion = `8.547%`

Operational note:

- this latest resume succeeded directly from DFU state `9`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, the Nano returned to clean DFU state `2`

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `990048` bytes
  - real completion = `9.176%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `790512` bytes
  - real completion = `7.325%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `730320` bytes
  - real completion = `6.763%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `545472` bytes
  - real completion = `5.055%`

Operational note:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- unlike several earlier checkpoints, the Nano remained in clean DFU state `2`
  after the stop

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `605760` bytes
  - real completion = `5.610%`

Operational note:

- this latest resume also started from clean DFU state `2`
- repeated `libusb: interrupted [code -10]` messages continued during the run
  but did not stop progress
- after checkpointing, host state returned to the stale-DFU pattern again,
  with `mks5lboot --dfuscan` failing until the Nano is re-entered into clean
  DFU

Latest decrypt checkpoint:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `668688` bytes
  - real completion = `6.196%`

Updated impact on visibility search remains unchanged:

- no new higher-level display/backlight candidate can be promoted yet because
  decrypted `OSOS` plaintext is still incomplete
- the next safe step remains continuing the same recovery-backed decrypt until
  `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu` is complete

## OSOS Decrypt Progress Checkpoint

Result:

- **OSOS_DECRYPT_STILL_IN_PROGRESS**

Latest saved state:

- `/tmp/n3g-osos-work/n3g-osos.recovery`
- `126096` bytes plaintext
- `1.167%` complete

Interpretation:

- supported OSOS decryption is progressing correctly
- no completed decrypted OSOS artifact exists yet
- therefore no higher-level display/backlight triage was started in this pass

## OSOS Device-Assisted Decryption Status

Current result:

- **OSOS_DECRYPTION_BLOCKED_WITH_REASON**

### What was proven

- `wInd3x` does support device-assisted RetailOS decryption internally:
  - `download retailos`
  - `PayloadKindRetailOSUpstream`
  - `PayloadKindRetailOSDecrypted`
  - `getRetailOSDecrypted()`
- the generic `decrypt` path operates on parsed IMG1 bodies, not only WTF/DFU
  payloads
- local `OSOS.fw` is a valid `87021.0` IMG1 with format `3`, so it is a
  supported input to that path

### Attempt status

The supported direct command was attempted against local `OSOS.fw`:

```bash
/tmp/wInd3x/wInd3x decrypt '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-decrypted.dfu
```

This started correctly and made early progress, confirming the image type and
tooling path are valid.

To preserve future progress, a resumable form was then attempted:

```bash
/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu
```

It failed immediately with:

- `clean failed: ClrStatus: control: libusb: i/o error [code -1]`

Host follow-up:

- `lsusb` still shows `05ac:1223`
- `mks5lboot --dfuscan` fails with `Could not set USB configuration:
  LIBUSB_ERROR_OTHER`

### Interpretation

The blocker is a stale USB / DFU session after the first decrypt attempt, not
an unsupported OSOS image type and not a missing decryption implementation.

No decrypted OSOS plaintext is available yet, so no new higher-phase
display/backlight candidate was extracted in this pass.

### Follow-up progress

After a fresh DFU re-entry, the resumable command:

```bash
/tmp/wInd3x/wInd3x decrypt -r /tmp/n3g-osos-work/n3g-osos.recovery '/home/david/Documents/rockbox3g nano/nano3g_firmware/analysis/binaries/OSOS.fw' /tmp/n3g-osos-work/n3g-osos-decrypted.dfu
```

did run normally.

Saved progress before stopping:

- `/tmp/n3g-osos-work/n3g-osos.recovery` = `72960` bytes
- real completion = `0.676%`

So OSOS decryption is not blocked by format or missing support anymore; it is
blocked operationally by runtime length. The current evidence bar still does not
allow a new visibility payload because no decrypted higher-phase plaintext is
available yet.

## Fresh Display-Dependency Search Outside Rejected Paths

Result:

- **STILL_BLOCKED**

### Additional artifacts checked

- `analysis/binaries/OSOS.fw`
- `analysis/binaries/OSOS.fw.payload_0x800.bin`
- `analysis/binaries/aupd.fw`
- `analysis/binaries/aupd.fw.payload_0x800.bin`

These remain high-entropy / effectively encrypted in the current workspace, so
they do not yet expose a usable Apple display or backlight routine.

### WTF-only PMU result remains unchanged

The decrypted WTF still exposes only:

- `0x22005420`: PMU read helper (`slave 0x73`)
- `0x22005474`: PMU write helper (`slave 0x73`)
- `0x220054b0`: reg `0x1d = 0x0a`, then reg `0x1b = on/off`
- `0x220054f8`: reg `0x43` bit `0` RMW

No Apple-backed PMU helper using:

- `0x20`
- `0x28`
- `0x29`

was recovered in decrypted WTF.

### Backlight-related non-Apple anchors

Local Nano 3G source still says:

- `D1671_REG_LEDCTL = 0x20`
- `D1671_LEDCTL_ENABLE = 0x80`

and `backlight-nano3g.c` uses that register directly.

Local debug code says:

- `pmu_read(0x29)` = backlight on/off
- `pmu_read(0x28)` = brightness value

iPod 6G backlight code uses:

- `pmu_write(0x28, brightness)`
- `pmu_write(0x29, on/off)`

This is useful circumstantial evidence that Nano 3G display visibility likely
still depends on a PMU backlight path, but it is not Apple-backed enough to
promote to the next hardware write.

### LCD-owned IRQ / gate result

The only fresh `#14` use found in decrypted WTF is:

- `0x22002024: mov r0, #14`
- call into `0x220039fc`

But `0x220039fc` is a generic helper that toggles bits in the `0x3c500000`
clock/power bank by selector index. It is not a direct VIC / `IRQ_LCD`
configuration helper, so this is not a new LCD-only dependency candidate.

### Decision

- **STILL_BLOCKED**

Reason:

- no Apple-backed `LEDCTL` / backlight write was recovered
- no decrypted higher-level firmware phase is available yet beyond WTF
- no new panel-reset GPIO outside the rejected wheel-side pins was recovered
- no isolated LCD-only IRQ/gate precondition was found

Prepared artifact:

- none

## Remaining Visibility Search Outside Rejected Paths

This pass searched for the next visibility dependency outside:

- PMU `0x1b`
- `0x220061f4`
- `GPIO72..75`
- `0x3c200000`

### What was checked

- PMU helper cross-references:
  - `0x22005420`
  - `0x22005474`
  - `0x220054b0`
  - `0x220054f8`
- LCD state-machine callers:
  - `0x220031b4`
  - `0x22004540`
  - `0x2200455c`
  - `0x220048bc`
- earlier startup paths:
  - `0x2200160c`
  - `0x22001698`
- clock/gate writes around:
  - `0x3c500048`
  - `0x3c50004c`
  - `0x3930003c`
  - `0x38400804`

### Findings

1. No new Apple-backed PMU display/backlight call site was found.
   - The decrypted WTF still only exposes:
     - `0x220054b0`: reg `0x1d = 0x0a`, then reg `0x1b = on/off`
     - `0x220054f8`: reg `0x43` bit `0` RMW
   - No caller using:
     - `LEDCTL (0x20)`
     - brightness/output regs like `0x28` / `0x29`
     was recovered from this WTF body.

2. No non-wheel GPIO reset pulse was found near the LCD-local path.
   - The only remaining reset-like pulse is:
     - `0x220017e8..0x22001804`
     - set bit `1` in `0x38400804`
     - delay `500`
     - clear bit `1`

3. The strongest remaining lead is a later coupled gate/reset cluster in
   `0x22001698`.
   - exact narrow writes:
     - `0x220018d8`: `0x3c500048 &= ~0x400`
     - `0x220018e8`: `0x3c500048 &= ~0x1`
     - `0x220018fc`: `0x3c50004c &= ~0x2000`
   - this cluster also contains:
     - the `0x38400804` pulse
     - service/object mediated calls using IDs `19` and `33`

### Interpretation

This does **not** yet justify a one-action payload.

The remaining cluster is promising because it sits outside the rejected wheel
and PMU-storage paths, but it is still coupled to the larger later startup path
and cannot yet be reduced to a single evidence-backed “display enable” write.

### Decision

- **STILL_BLOCKED**

### Best remaining lead

- later `0x22001698` gate/reset cluster:
  - `0x38400804` bit-1 pulse
  - `0x3c500048 &= ~0x400`
  - `0x3c500048 &= ~0x1`
  - `0x3c50004c &= ~0x2000`

Prepared artifact:

- none

## Reduction of the Later `0x22001698` Cluster

Result:

- **STILL_BLOCKED**

### Why the cluster is not bounded enough yet

The later path does not consist only of the four previously isolated writes.
It also performs:

- `0x38100000 &= ~0x7`
- `0x38400804` bit-1 pulse with delay `500`
- delay `10`
- three narrow gate clears in `0x3c500048/0x3c50004c`
- writes to:
  - `0x38e00014`
  - `0x38e01014`
- gate-bit enables through:
  - `0x38e00010`
  - `0x38e01010`

and it is wrapped in a service-mediated bring-up flow using:

- `0x220024d8`
- `0x22001c70`
- `0x22006380`
- `0x22006430`
- `0x22006558`
- `0x22003b5c`
- `0x22003350`
- multiple callbacks through the runtime object at `0x22007784`

### Gate-ID decoding

`0x220032fc` splits gate IDs into bank/bit fields, and `0x22003838` sets the
corresponding bit in `0x38e00010` or `0x38e01010`.

Important consequence:

- ID `33` decodes to bank `1`, bit `1`
- that matches in-tree `CLOCKGATE_CWHEEL = 33`

So the cluster contains at least one gate enable that is not display-specific.

### `0x38400804` pulse

- only one confirmed decrypted-WTF use remains
- no second LCD-local cross-reference ties bit `1` directly to panel reset or
  LCD controller reset
- treat as:
  - **unknown peripheral reset/enable pulse**

### Decision

- do **not** prepare `lcd-clustergate-fullinit-n3g.bin`
- do **not** split out a single pulse/gate clear as the next test

Prepared artifact:

- none

## Reclassification of `0x22001698`

Result:

- **EXCLUDE_0x22001698**

### Why

The decisive correction is that `0x38e00000` is the VIC block, not a clock
control block:

- `0x38e00010` = `VIC0INTENABLE`
- `0x38e00014` = `VIC0INTENCLEAR`
- `0x38e01010` = `VIC1INTENABLE`
- `0x38e01014` = `VIC1INTENCLEAR`

So the IDs in this path are IRQ IDs, not display-clock gate IDs.

Decoded IDs:

- `19` -> `IRQ_USB_FUNC`
- `33` -> `IRQ_EXT6`
- `39` -> `IRQ_AES`
- `40` -> unknown IRQ in bank 1, not LCD

Negative evidence:

- `IRQ_LCD = 14`
- `0x22001698` never touches IRQ `14`

The path also writes:

- `0x38100000 &= ~0x7`

and `0x38100000` is `MIUCON` on S5L8702, i.e. memory-interface state, not a
display register.

### Runtime service interpretation

The service object region near `0x22007784` is also non-display:

- table count at `0x2200778c` is `3`
- entries at `0x22007790...` are tagged `"Uart$"`
- their blocks live at `0x3cc00000`, `0x3cc04000`, `0x3cc08000`

This makes the whole `0x22001698` path look like broader
peripheral/interrupt/service bring-up rather than display wake.

Prepared artifact:

- none

## 2026-04-23 OSOS decrypt live progress

- Higher-level display/backlight triage is still blocked on OSOS plaintext.
- The active device-assisted decrypt is still running.
- Latest observed progress from the live decrypt session:
  - `ix=3989760`
  - `36.976%`
- No new Apple-backed visibility candidate has been extracted in this step
  because decryption has not completed yet.

## 2026-04-23 OSOS decrypt live progress update

- Decrypt remains active.
- Latest observed progress from the live session:
  - `4165632` bytes
  - `38.605%`
- No new higher-level display/backlight evidence can be claimed until plaintext
  completion.

## 2026-04-23 OSOS decrypt live progress update 2

- Decrypt remains active.
- Latest observed progress from the live session:
  - `4364544` bytes
  - `40.449%`
- No new higher-level display/backlight evidence can be extracted yet because
  OSOS plaintext is still incomplete.

## 2026-04-23 OSOS decrypt live progress update 3

- Decrypt remains active.
- Latest observed progress from the live session:
  - `4504320` bytes
  - `41.744%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-23 OSOS decrypt live progress update 4

- Decrypt remains active.
- Latest observed progress from the live session:
  - `4675584` bytes
  - `43.332%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-23 OSOS decrypt live progress update 5

- Decrypt remains active.
- Latest observed progress from the live session:
  - `5009664` bytes
  - `46.428%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-23 OSOS decrypt live progress update 6

- Decrypt remains active.
- Latest observed progress from the live session:
  - `5207040` bytes
  - `48.257%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-23 OSOS decrypt live progress update 7

- Decrypt remains active.
- Latest observed progress from the live session:
  - `5379072` bytes
  - `49.851%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-23 OSOS decrypt live progress update 8

- Decrypt remains active.
- Latest observed progress from the live session:
  - `5532672` bytes
  - `51.275%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-23 OSOS decrypt live progress update 9

- Decrypt remains active.
- Latest observed progress from the live session:
  - `5675520` bytes
  - `52.599%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-24 OSOS decrypt live progress update 10

- Decrypt remains active.
- Latest observed progress from the live session:
  - `6023424` bytes
  - `55.823%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-24 OSOS decrypt live progress update 11

- Decrypt remains active.
- Latest observed progress from the live session:
  - `6187776` bytes
  - `57.346%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-24 OSOS decrypt live progress update 12

- Decrypt remains active.
- Latest observed progress from the live session:
  - `6398976` bytes
  - `59.303%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-24 OSOS decrypt live progress update 13

- Decrypt remains active.
- Latest observed progress from the live session:
  - `6646272` bytes
  - `61.595%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-24 OSOS decrypt live progress update 14

- Decrypt remains active.
- Latest observed progress from the live session:
  - `6800640` bytes
  - `63.026%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.

## 2026-04-24 OSOS decrypt live progress update 15

- Decrypt remains active.
- Latest observed progress from the live session:
  - `7150080` bytes
  - `66.264%`
- No higher-level display/backlight evidence is ready to extract yet because
  OSOS plaintext is still incomplete.
## 2026-04-24 OSOS display/backlight triage result

OSOS decryption completed successfully and produced:

- `/tmp/n3g-osos-work/n3g-osos-decrypted.dfu`
- `/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin`

The decrypted OSOS body adds stronger display/backlight evidence than WTF.

### Confirmed OSOS service veneers

The following veneers branch into fixed Apple ROM/service code:

- `0x2200367c -> 0x080646b4`
- `0x22003684 -> 0x08064790`
- `0x2200374c -> 0x080db704`
- `0x2200376c -> 0x080dbd8c`
- `0x22003774 -> 0x080dbe58`

### Strongest higher-level display-on sequence

The clearest OSOS body-visible candidate is:

1. `0x220057d4`: `bl 0x2200374c`
2. `0x220057d8`: `bl 0x22004c5c`
3. `0x220057dc`: `bl 0x2200374c`
4. `0x220057e0`: `bl 0x22003774`

`0x22004c5c` itself is also visible in-body:

- add `0x44` to the service pointer
- call `0x2200367c`
- clear byte `[service + 0x05]`
- add `0x44` again
- tail-call `0x22003684`

This is the first higher-level Apple-backed “make it visible” path recovered
after the lower-level WTF LCD init proved insufficient on hardware.

### Negative result

Even OSOS does not expose raw `LEDCTL (0x20)` or direct PMU `0x28/0x29`
backlight writes in plain body code here. The hardware-facing writes of the
candidate remain encapsulated inside the imported ROM/service helpers listed
above.

### Decision

- **OSOS_DECRYPT_COMPLETE_CANDIDATE_FOUND**
- chosen candidate class:
  - **USE_OTHER_SINGLE_ACTION**
- chosen action:
  - OSOS display/backlight service sequence
    - `0x2200374c`
    - `0x22004c5c`
    - `0x2200374c`
    - `0x22003774`

### Prepared-only payload

Prepared but not run:

- `tools/ipodnano3g/minimal_payload/lcd-osos-visible-n3g.bin`

Supporting analysis doc:

- `docs/porting/ipodnano3g-osos-display-sequence.md`

## 2026-04-24 OSOS raw-reduction result

Follow-up reduction of the OSOS visibility calls did **not** expose a direct
standalone PMU/GPIO/MMIO sequence.

What was confirmed:

- `0x2200374c` is only a veneer to `0x080db704`
- `0x22003774` is only a veneer to `0x080dbe58`
- `0x22004c5c` is the only body-visible helper in the minimal candidate, but it
  only:
  - calls `0x2200367c -> 0x080646b4`
  - clears byte `[service + 0x05]`
  - tail-calls `0x22003684 -> 0x08064790`
- the compact wrapper `0x22005640` adds more service indirections through:
  - `0x220073b4`
  - `0x22007610`
  and those still reduce to imported ROM/service code rather than direct
  display hardware writes

Decision:

- **STILL_BLOCKED**

Consequence:

- `lcd-osos-raw-visible-n3g.bin` was not prepared
- no evidence-backed raw display-enable sequence exists yet from OSOS body
  analysis alone

Supporting reduction note:

- `docs/porting/ipodnano3g-osos-reduced-display.md`

## 2026-04-24 ROM/service target investigation

The blocked OSOS visibility path was traced one step further by classifying the
service targets it calls:

- `0x080646b4`
- `0x08064790`
- `0x080db704`
- `0x080dbe58`

### Region ownership

Local S5L8702 headers and Nano 3G analysis notes agree on:

- `DRAM_ORIG = 0x08000000`
- `IRAM_ORIG = 0x22000000`

Local `wInd3x` documentation separately states that bootrom is mapped at:

- `0x00000000`
- `0x20000000`

So the current best interpretation is:

- the `0x080...` display-service targets are **DRAM-backed runtime code**, not
  direct BootROM addresses

### Existing artifact search

Searches across:

- decrypted WTF body
- decrypted OSOS body
- original IPSW-extracted files
- `aupd`
- `rsrc`
- local `wInd3x` source
- freemyipod/Rockbox notes

did **not** recover concrete code bodies for the four service targets above.

The decrypted OSOS body still exposes only:

- veneers into `0x080...`
- callers that manipulate service-object state
- no direct display/backlight MMIO behind those calls

### Dump support

`wInd3x` documents and implements a read-only memory dump primitive:

- `wInd3x dump [offset] [size] [file]`

This is sufficient to justify a **prepared-only** dump plan for the narrow
service windows that contain the missing display/backlight targets.

Prepared commands, not run:

```bash
/tmp/wInd3x/wInd3x dump 0x08064000 0x2000 /tmp/n3g-romsvc-08064000.bin
/tmp/wInd3x/wInd3x dump 0x080db000 0x2000 /tmp/n3g-romsvc-080db000.bin
```

### Decision

- **ROM_SERVICE_DUMP_POSSIBLE**

Reason:

- the target code is not present in currently available plaintext artifacts
- the addresses appear to live in DRAM runtime space
- a documented read-only extraction path exists locally
- no dump was executed yet, so raw display/backlight MMIO remains unresolved

## 2026-04-24 Runtime/service dump attempt

One real-device read-only dump attempt was made against the first narrow
runtime-service window:

```bash
/tmp/wInd3x/wInd3x dump 0x08064000 0x2000 /tmp/n3g-romsvc-08064000.bin
```

Result:

- clean pre-dump DFU state was confirmed (`05ac:1223`, DFU state `2`)
- `wInd3x dump` reached the device and started at `0x08064000`
- the very first `dumpmem` trigger failed with:
  - `bug trigger: USB timeout error`
- the output file remained zero bytes
- post-attempt DFU degraded to the usual stale host-visible state:
  - `mks5lboot --dfuscan` -> `LIBUSB_ERROR_OTHER`

This means the next blocker is no longer “does a dump path exist?” but:

- can the current Nano 3G `dumpmem` primitive actually read DRAM-backed
  runtime/service code in this DFU context, or is it effectively limited to the
  bootrom-oriented use documented in `wInd3x`?

Updated status:

- **ROM_SERVICE_DUMP_FAILED_WITH_REASON**

## 2026-04-24 Dump capability diagnostics

Three tiny `0x40`-byte probes were used to classify Nano 3G `wInd3x dump`
behavior.

Successful probes:

- `0x20000000 -> /tmp/n3g-dump-test-20000000.bin`
- `0x00000000 -> /tmp/n3g-dump-test-00000000.bin`

Both returned:

- size `64`
- identical contents
- non-zero, ARM-like instruction bytes

Example prefix:

```text
2e 00 00 ea 64 f0 9f e5 64 f0 9f e5 ...
```

Failed probe:

- `0x08064000 -> /tmp/n3g-dump-test-08064000.bin`

Failure mode:

- first trigger immediately returned:
  - `bug trigger: USB timeout error`
- output file remained size `0`
- DFU then degraded to:
  - `LIBUSB_ERROR_OTHER`

Decision:

- **DUMP_080_SMALL_FAILS**

Interpretation:

- the dump primitive is not generally broken
- it works for bootrom-accessible aliases (`0x00000000` / `0x20000000`)
- it does not currently work for the DRAM-backed runtime/service window
  `0x08064000` in the tested Nano 3G DFU context

## 2026-04-24 Alternate DRAM/runtime read investigation

Static inspection of local `wInd3x` paths shows that the existing Nano 3G
memory readers all share the same basic execution model:

- payload executes in DFU exploit context
- payload lives in IRAM / DFU buffer
- result comes back through `HandlerFooter(...)`

This applies to:

- `dumpmem.Trigger()`
- `cmd_spew.go` `readFrom()`
- CP14 / CP15 read helpers
- NAND helpers
- NOR helpers

Important Nano 3G detail:

- `HandlerFooter(addr)` on Nano 3G calls bootrom helper `0x2000aa40`
- return path uses bootrom address `0x200048d4`

So the current reader family is fundamentally **bootrom-DFU-context RCE**, not
later OSOS/runtime-context inspection.

### Consequences

- no distinct alternate dump primitive was found in local `wInd3x`
- no static artifact yet reconstructs the missing `0x080...` service bodies
- a payload-assisted copy into `0x22000100` is conceptually possible because
  that IRAM scratch/return buffer is already used by `cmd_spew` and NAND/NOR
  helpers
- but that idea is still blocked by the same core unknown:
  - whether the current execution context can dereference `0x080...` safely at
    all

### Decision

- **CURRENTLY_BLOCKED_WITH_REASON**

Reason:

- no alternate runtime-space reader was found
- current readable paths remain bootrom-oriented
- payload-assisted copy is not yet evidence-backed enough to promote

## 2026-04-24 Minimal `0x080...` runtime-read probe

A smallest-possible read-only probe payload was prepared to test whether a
normal Nano 3G payload executing at `0x22000000` can dereference the
runtime/service window directly.

Prepared artifacts:

- `tools/ipodnano3g/minimal_payload/probe-080-read-n3g.bin`
- `tools/ipodnano3g/minimal_payload/probe-080-read-n3g.elf`
- `tools/ipodnano3g/minimal_payload/probe-080-read-n3g.map`

Verified host-side properties:

- entrypoint: `0x22000000`
- flat binary size: `52` bytes
- behavior:
  - write `0x11111111` to `0x22000100`
  - read one word from `0x08064000`
  - store result to `0x22000104`
  - write `0x22222222` to `0x22000108`
  - loop forever

Relevant disassembly:

```text
22000000: ldr r0, =0x22000100
22000004: ldr r1, =0x11111111
22000008: str r1, [r0]
2200000c: ldr r2, =0x08064000
22000010: ldr r3, [r2]
22000014: str r3, [r0, #4]
22000018: ldr r1, =0x22222222
2200001c: str r1, [r0, #8]
22000020: b   0x22000020
```

### Result-readback assessment

There is **no current existing command** that can honestly be documented as a
working post-takeover readback path for `0x22000100`.

Why:

- after `wInd3x run`, successful payload execution consistently breaks normal
  DFU responsiveness
- the established post-send host signal is:
  - USB still visible as `05ac:1223`
  - `mks5lboot --dfuscan` fails with `LIBUSB_ERROR_OTHER`
- existing `wInd3x dump` / `readFrom` style readers depend on live bootrom DFU
  handling, which is exactly what the looping payload takes over

So while `0x22000100` is a structurally correct scratch/return buffer, the
probe result is **not currently retrievable** with the existing tool path after
payload takeover.

### Test-plan status

A one-run plan is easy to define:

1. clean DFU
2. `wInd3x run probe-080-read-n3g.bin`
3. try to read back `0x22000100`
4. recover DFU

But step 3 has no evidence-backed working command today, so the plan is blocked
before execution.

### Decision

- **PROBE_PREPARED_BUT_NO_RESULT_READBACK**

## 2026-04-24 Payload result-output strategy search

The next question was whether a standalone Nano 3G payload has any minimal way
to report a small result back to the host or user after takeover.

### Host-visible return to DFU

Current answer: not evidence-backed.

What was checked:

- `wInd3x` DFU / haxed-DFU implementation
- Nano 3G `HandlerFooter(...)`
- `run` / `SendImage(...)` path
- existing `spew`, `dump`, NAND, and NOR helpers

What that shows:

- `wInd3x dump`, `spew`, and related readers only work while the bootrom DFU
  request handlers are still alive
- after a successful standalone image run, the established host signature is:
  - USB may still enumerate as `05ac:1223`
  - `mks5lboot --dfuscan` falls to `LIBUSB_ERROR_OTHER`
- no existing local command re-attaches a post-payload read primitive after that
  takeover

Conclusion:

- no evidence-backed “return to clean DFU and keep scratch RAM readable” path
  exists yet for standalone payloads

### Minimal USB output

Current answer: not available in the present context.

What exists:

- `wInd3x` bootrom exploit code can still use USB control handling while
  executing the DFU exploit payload itself
- haxed-DFU changes the USB product string during exploit-time setup

What does not exist:

- a known tiny reusable USB/status routine for standalone Nano 3G IMG1 payloads
- a documented way to keep DFU upload/readback alive after payload takeover
- a safe already-reversed mini USB-debug path from OSOS/WTF that avoids USB PHY
  reinitialization

Conclusion:

- no evidence-backed standalone USB output channel is currently available

### Physical signal options

Source-backed option still on record:

- piezo tone / beep

Why it is not promoted here:

- it requires timer/GPIO programming, not a no-op host return path
- the current task explicitly excludes preparing an audio-based proof payload

Other simple physical channels checked:

- hold-switch state effects: not an output channel
- reset timing: not robust enough to encode a trustworthy result
- LCD/backlight: explicitly out of scope for this task

### Overall Decision

- **NO_SAFE_OUTPUT_CHANNEL_FOUND**

Meaning:

- no host-readable return path survives standalone payload takeover
- no minimal USB output path is currently evidence-backed
- the only source-backed physical signal option is piezo, which is outside the
  present no-audio constraint

## 2026-04-24 Early shared OSOS init result

The investigation shifted upward from subsystem-local LCD/piezo work to common
startup state and identified one bounded OSOS wrapper as the best current
shared-init candidate:

- `0x22002770`

That wrapper visibly performs:

- CP15 control-bit clear/restore through:
  - `0x22003150`
  - `0x22003138`
  - `0x220030fc`
  - `0x22003110`
- MIU/global memory programming through `0x22002420`
- clock/reset-style programming through `0x22002d78`

Direct shared hardware state touched includes:

- `0x38100000`
- `0x3c500000`
- `0x39900000`

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-probe-n3g.bin`

Host-side validation:

- entrypoint `0x22000000`
- flat binary size about `14 KiB`
- `_start` only sets stack, calls `0x22002770`, then loops

Still unresolved:

- `0x22003414 -> 0x08016234`
- `0x220034bc -> 0x0801542c`

So the current result is a bounded Apple call-level early-init probe, not a
fully raw MMIO-only reduction.

### Controlled run status

One run of `system-init-probe-n3g.bin` is now host-confirmed:

- clean pre-run DFU state `2`
- upload succeeded
- post-send `lsusb` still showed `05ac:1223`
- post-send `dfuscan` fell to `LIBUSB_ERROR_OTHER`

But:

- user later clarified there was no visible or audible behavior for that run

So the current classification is only:

- **SAFE_EXECUTION_ONLY**

### Import reduction

`0x22002770` now splits into:

1. wrapper prologue
2. `0x22003414`
3. local CP15 clear helpers
4. `0x22002420(1)` which depends on `0x220034bc`
5. local `0x22002d78`
6. local `0x22002420(3)`
7. local CP15 restore helpers

Current classifications:

- `0x22003414 -> 0x08016234`
  - front-edge init barrier/hook
  - likely required for any meaningful progress into the wrapper
- `0x220034bc -> 0x0801542c`
  - arithmetic/timing helper
  - required for the first MIU phase, not obviously for the later mode-3 phase

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-observable-n3g.bin`

That observable variant preserves the same bounded early-init wrapper and then:

- delays in software
- branches to `0xdead0000`

This is intended to make later testing visually classifiable without adding new
hardware writes.

Host-side validation:

- entrypoint `0x22000000`
- flat binary size about `14 KiB`
- staged behavior:
  - early-init wrapper
  - software delay
  - software delay
  - deliberate invalid branch to `0xdead0000`

Prepared comparison controls:

- `tools/ipodnano3g/minimal_payload/system-init-skiprom-n3g.bin`
- `tools/ipodnano3g/minimal_payload/system-init-1call-n3g.bin`

Purpose:

- compare no-ROM baseline vs first-ROM-call-only vs full early-init wrapper
- determine whether the missing observable behavior is caused by:
  - ROM import blocking
  - second-import/full-path blocking
  - or a generally non-visible fault path

## 2026-04-24 Observable output redesign

Fault-based observability is now treated as failed on Nano 3G:

- `0xdead0000` style probes did not produce a visible Apple logo, reset, or
  other user-facing signal
- the skip-ROM, one-call, and full early-init fault probes therefore do not
  provide a usable comparison channel

### Chosen replacement: watchdog-timed reboot

The narrowest safe replacement found is the in-tree S5L8702 reboot path:

- `system_reboot()` in `firmware/target/arm/s5l8702/system-s5l8702.c`
- implementation:
  - switch to SVC mode
  - write `0x00100000` to `WDT_BASE`
  - wait forever for reset

Relevant register basis:

- `WDT_BASE = 0x3c800000`
- source:
  - `firmware/export/s5l87xx.h`
  - `firmware/target/arm/s5l8702/system-s5l8702.c`

Why this is better than the invalid-branch fault:

- it is a real target reboot path already used by the Nano 3G / S5L8702 code
- it replaces undefined crash behavior with one narrow watchdog write
- no LCD, PMU, GPIO, USB PHY, NAND, or audio writes are added

### Timing-probe design

Prepared only, not run:

- `tools/ipodnano3g/minimal_payload/system-init-timingprobe-n3g.bin`

Behavior:

1. set a local stack
2. call bounded early-init wrapper `0x22002770`
3. short software delay
4. long software delay
5. write `0x00100000` to `0x3c800000`
6. loop forever waiting for watchdog reset

### Why there is no repeated reset loop

With the present DFU payload model, the payload is not persistent across a
reboot. So a true repeating short/long reset pattern is not available without a
boot-stage installer or other persistence mechanism, which is outside the
current constraints.

The realistic observable signal is therefore:

- one known delay pattern
- followed by a watchdog reboot

### Host-side validation

- entrypoint `0x22000000`
- flat binary size about `14 KiB`
- `_start` sequence:
  - `0x22002770`
  - short delay
  - long delay
  - `str r0, [0x3c800000]` with `r0 = 0x00100000`
  - infinite wait loop

### Controlled run result

One run of `system-init-timingprobe-n3g.bin` is now recorded:

- clean pre-run DFU state `2`
- upload succeeded
- post-send `lsusb` still showed `05ac:1223`
- post-send `dfuscan` fell to `LIBUSB_ERROR_OTHER`
- user-visible result over 30 seconds:
  - black / no change
  - no delayed Apple logo
  - no immediate reboot
  - no flicker / click / beep
  - no visible USB disconnect/reconnect

Classification:

- **WATCHDOG_NO_VISIBLE_RESET**

Meaning:

- replacing the invalid fault with the in-tree watchdog reboot path still did
  not create a usable physical observation channel in the tested standalone
  payload context

## 2026-04-24 Execution-context shift strategy

The current `wInd3x run` path is confirmed to be the wrong execution context for
late display/backlight work.

### Why `run` is too early

Per local `wInd3x` documentation and source:

- `run` does:
  - BootROM DFU
  - haxed DFU exploit
  - defanged WTF upload
  - second DFU image upload
- this remains in the recovery-stage path:
  - `BootROM -> WTF, defanged (DFU) -> payload`

That explains why our standalone payloads execute but still lack the full Apple
runtime environment expected by OSOS display and other higher-level services.

### Later-stage path already present in wInd3x

`wInd3x` already defines a later chainload flow under CFW:

- `BootROM -> WTF, defanged (DFU) -> Modified RetailOS/U-Boot/... (DFU)`

Evidence:

- `/tmp/wInd3x/README.md`
- `/tmp/wInd3x/cmd/wInd3x/cmd_cfw.go`
- `/tmp/wInd3x/web/src/components.ts`

This is the first path in the local tooling that intentionally boots into a
full RetailOS context rather than a bare standalone DFU payload.

### Injection mechanism inside later RetailOS

`wInd3x` also already contains EFI patch infrastructure:

- `pkg/cfw/cfw.go`
  - `VisitVolume`
  - `VisitPE32InFile`
  - `PatchAt`
  - `ReplaceExact`

That means the intended injection model is not “run raw code after DFU” but:

1. decrypt RetailOS
2. patch one PE32 section within its EFI firmware volume
3. chainload the modified RetailOS through defanged WTF
4. let Apple perform its own global/system/display bring-up
5. execute the hook inside that already-initialized environment

### Role clarification

- WTF:
  - early recovery-stage loader / bridge
  - useful for defanged handoff and second-stage image acceptance
- OSOS / RetailOS:
  - full Apple runtime
  - the first plausible place where display/audio/input services are already
    initialized enough for higher-level hooks to be meaningful

### Best current strategy

- **CHAINLOAD_METHOD**

More specifically:

- use defanged WTF only as a bridge
- modify RetailOS, not the raw DFU payload
- patch or hook a post-init RetailOS PE32/DXE target instead of replacing the
  whole execution context with a tiny payload at `0x22000000`

### Practical implication

The next meaningful step is no longer another raw DFU payload. It is to locate
one RetailOS PE32/DXE module and one post-init function boundary suitable for a
small hook, then generate a customized RetailOS image via the existing `wInd3x`
CFW/EFI patch machinery.

## 2026-04-24 Nano 3G RetailOS hook-target reduction

The initial EFI assumption for Nano 3G RetailOS was wrong.

### RetailOS structure

Current evidence says Nano 3G OSOS is a **raw ARM monolith**, not an EFI
firmware volume:

- Nano 3G WTF defanging in local `wInd3x` already uses raw body patching
  (`defangRaw`) rather than EFI visitors.
- Parsing decrypted Nano 3G OSOS as an EFI volume at body offset `0` fails with
  an invalid volume header.
- `_FVH` signatures were not recovered from the decrypted OSOS body.

So `cfw run` remains the correct execution-context shift, but the patching model
for Nano 3G RetailOS should be treated as **raw IMG1 body patching**, not
PE32/DXE module patching.

### Lowest-risk first post-init proof point

The safest initialized-runtime target found so far is the USB/DiskMode
connected-screen string cluster:

- owner strings:
  - `controller.SwitchToConnected1`
  - `controller.SwitchToDisconnected1`
  - `controller.SwitchToLoading1`
  - `DiskMode_ScreenLayout_Connected`
  - `DiskMode_OKToDisconnect_String`
  - `RemoteUI_Ok_To_Disconnect_String`
- visible body strings:
  - `0x817980`: `Connected`
  - `0x8179b8`: `Do not disconnect.`
  - `0x8179f0`: `OK to Disconnect`

This cluster is preferred over a direct code hook because:

- it should only be reached inside initialized RetailOS
- it is naturally relevant while USB remains attached during `cfw run`
- it avoids new MMIO
- it avoids first-attempt branch-hook instability

### Prepared chainload proof image

Prepared only, not run:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`

Applied raw body substitutions:

- `0x817980`: `Connected` -> `CFW mode!`
- `0x8179b8`: `Do not disconnect.` -> `CFW runtime ready!`
- `0x8179f0`: `OK to Disconnect` -> `CFW booted      `

Helper used:

- `/tmp/wInd3x/cmd/patch_n3g_connected_ui.go`

### Higher-risk follow-up code targets

Still available for later review if the visible-string proof works:

- `0x22005620`
- `0x22005640`
- `0x22005660`

These sit in the `TCRemoteUI` / display-service cluster and are better treated
as second-stage raw hook candidates rather than first-stage proof points.

### Current decision

- **HOOK_TARGET_FOUND_PATCH_PREPARED**

With the Nano 3G-specific caveat that the prepared proof is a **raw RetailOS
resource patch**, not a PE32/DXE hook.

## 2026-04-24 First Nano 3G `cfw run` outcome

The first controlled chainload test of the prepared RetailOS visible-string
artifact did **not** produce a post-init UI proof.

Tested artifact:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible.dfu`

Observed result:

- screen remained black for the full 60-second observation window
- no patched `CFW ...` strings appeared
- no original connected/disk text appeared
- no normal Apple UI appeared

Host-side result:

- the device left plain DFU
- USB enumerated as `05ac:1242` (`WTF mode`)
- it did not progress into a visible RetailOS runtime state
- `mks5lboot --dfuscan` stayed busy until the host `wInd3x` process was
  terminated
- recovery to clean DFU state `2` succeeded after manual reset

Implication:

- the current blocker has moved again:
  - no longer “find a visible RetailOS proof string”
  - now “understand why Nano 3G `cfw run` does not visibly escape WTF mode”

So the prepared UI patch was safe and valid, but it was not sufficient to prove
or reach initialized RetailOS UI execution on this device.

## 2026-04-24 Nano 3G WTF handoff blocker

The current evidence points away from “bad patched RetailOS” and toward an
incomplete Nano 3G WTF defang.

### Header comparison

Stock Nano 3G OSOS:

- format `3`
- entrypoint `0x0`
- body length `0xa4a570`

Decrypted/repacted RetailOS used by `cfw run`:

- format `2`
- entrypoint `0x0`
- body length `0xa4a570`

Patched visible-string RetailOS:

- format `2`
- entrypoint `0x0`
- body length `0xa4a570`

So the prepared artifact is structurally consistent with local `wInd3x`
RetailOS decryption output. The visible-string patch did not introduce a new
wrapper anomaly beyond the existing Nano 3G decrypted-repack format change.

### Remaining unpatched WTF path

Nano 3G defanged WTF currently only applies one functional patch:

- body offset `0x1990`
- intended effect: skip one signature-related branch

But the subsequent block remains intact:

- runtime `0x22001998..0x220019dc`
- calls callback `[service + 0x74]`
- then checks:
  - `0x38c00040`
  - `0x38c0000c`

That looks like a second-stage validator/decrypt completion gate.

### Why this matters

Later-device defangers in local `wInd3x` are broader:

- patch header signature checks
- patch data signature checks
- patch AES/decrypt behavior when needed

Nano 3G lacks an equivalent second-stage bypass, yet stock OSOS is still a
format-`3` encrypted image. That makes the handoff path much more likely to
stall in WTF after the transfer succeeds.

### Current best diagnosis

- **WTF_HANDOFF_BLOCKED_BY_CHECK**

The most probable immediate blocker is the still-live Nano 3G WTF
validator/decrypt path after `0x22001990`, not the RetailOS resource patch.

## 2026-04-24 Second-stage Nano 3G WTF handoff patch

Implemented in local `wInd3x` source:

- `/tmp/wInd3x/pkg/cfw/defang_wtf.go`

Reasoning:

- keep the callback/setup path at `0x22001998..0x220019b4`
- only neutralize the remaining branch into the failure block
- preserve earlier transfer/setup logic as much as possible

Exact new patch:

- body offset `0x19b8`
- original bytes:
  - `08 00 00 0a`
- replacement bytes:
  - `19 00 00 ea`

Meaning:

- original:
  - `beq 0x220019e0`
- patched:
  - `b 0x22001a24`

So the callback still executes, but the immediate failure leg is replaced with a
branch to the existing success path.

Build status:

- rebuilt successfully into:
  - `/tmp/wInd3x/wInd3x`

Important operational note:

- the previously cached defanged WTF:
  - `/home/david/.local/share/wInd3x/n3g-wtf-defanged.bin`
  was generated before this patch
- it must be invalidated before the next `cfw run`, or the new handoff patch
  will not be used

## 2026-04-24 Nano 3G WTF handoff: additional checks beyond `0x19b8`

Full tracing from `0x22001998` forward shows that landing on `0x22001a24`
does **not** mean WTF has handed off to RetailOS. After that point it still:

- calls two more `[service + 0x8c]` callbacks
- runs `0x22006558`, which contains a polling loop on
  `[selected UART base + 0x18] & 0x200`
- only reaches the execute trampoline if:
  - `r4 == 0`
  - `r5 & 0x10` is set
  - control reaches `0x220024b8`

Important pre-success checks remain callback-driven too:

- `[service + 0x6c]` must return nonzero or the path exits with `r4 = 23`
- `[service + 0x74]` is called with:
  - `r0 = 0x08000000`
  - `r1 = 0x08000800`
  - `r2 = 2`
  and originally must also return nonzero

So the current best diagnosis is:

- **CALLBACK_DEPENDENCY_REQUIRED**

freemyipod's public docs reinforce this. WTF is a real second-stage bootloader
that still performs IMG1 verification/decryption before booting the next stage,
and upstream `wInd3x` still marks Nano 3G `CFW` as `soon`, not supported.

Practical consequence:

- do not keep adding blind outer branch-skips
- the next correct fix is to resolve and patch/stub the callback targets behind
  `[service + 0x6c]`, `[service + 0x74]`, and likely `[service + 0x8c]`, or
  the state they initialize

## 2026-04-24 WTF service callback table origin

The handoff callback source is now narrowed down:

- `r9` is loaded from WTF literal `0x22007784`
- `0x22007784` contains `0x20000020`

So the critical handoff callbacks come from a BootROM-resident object/table
starting at `0x20000020`, not from the local WTF runtime service registry.

Relevant callback slots:

- `[service + 0x6c]` -> BootROM word `0x2000008c`
- `[service + 0x74]` -> BootROM word `0x20000094`
- `[service + 0x8c]` -> BootROM word `0x200000ac`

Related paired/helper slots used in the same path:

- `[service + 0x80]` -> `0x200000a0`
- `[service + 0x90]` -> `0x200000b0`

This separates two mechanisms clearly:

1. local WTF registry at `0x220073ec`
   - managed by `0x220053a8`, `0x220053e0`, `0x22003508`
   - holds local entries such as `Uart$` at `0x22007790+`
2. BootROM callback table at `0x20000020`
   - drives the actual handoff callbacks used by `0x22001698`

Current classification refinement:

- **CALLBACK_TABLE_BOOTROM_KNOWN**

The table base and slot offsets are now known, but the actual function-pointer
contents at `0x2000008c`, `0x20000094`, and `0x200000ac` are not present in the
currently saved tiny BootROM dump. So the exact callback targets still need
BootROM-word recovery before a correct Nano 3G WTF fix can be specified.

## 2026-04-24 BootROM callback bodies and Nano 3G IMG1 mismatch

BootROM callback bodies are now recovered:

- `0x200036c8` <- `[service + 0x6c]`
- `0x200006dc` <- `[service + 0x74]`
- `0x2000106c` <- `[service + 0x8c]`
- `0x2000132c` <- `[service + 0x90]`

Key result:

- `0x2000106c` / `0x2000132c` are just setter/getter helpers for per-ID BootROM
  tables
- the real handoff loader is `0x200006dc`

In the exact WTF handoff mode we use:

- arguments:
  - `r0 = 0x08000000`
  - `r1 = 0x08000800`
  - `r2 = 2`

`0x200006dc` takes the `r2 = 2` branch and explicitly accepts only image types:

- `3`
- `4`

Any other type returns `0`.

This matches the current observed mismatch:

- stock Nano 3G `OSOS.fw`:
  - format `3`
- decrypted/patched Nano 3G RetailOS from local `wInd3x`:
  - format `2`

So the current best diagnosis is stronger than just "callbacks matter":

- **the BootROM loader callback is rejecting our format-2 Nano 3G RetailOS
  wrapper before handoff completes**

Implication:

- the next correct fix is not another WTF branch patch
- it is to make Nano 3G `cfw run` produce a wrapper that `0x200006dc` accepts
  in mode `2`, most likely format `4` with the expected footer/signature layout

## 2026-04-24 Prepared Nano 3G format-4 chainload image

Applied the wrapper fix in local `wInd3x`:

- `/tmp/wInd3x/pkg/image/image.go`

Nano 3G `MakeUnsigned(...)` now emits:

- magic `8702`
- version `1.0`
- format `4`
- signature area `0x80`
- certificate area `0x300`

instead of the old Nano 3G special case:

- format `2`
- no signature area
- no certificate area

Rebuilt:

- `/tmp/wInd3x/wInd3x`

Regenerated visible-string RetailOS artifact:

- `/tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Verified header:

- format `4`
- entry `0`
- body `0xa4a570`
- data `0xa4a8f0`
- cert offset `0xa4a5f0`
- cert length `0x300`

This is the first Nano 3G chainload image prepared to match the recovered
BootROM loader callback's accepted type set (`3` or `4`).

## 2026-04-24 Format-4 Nano 3G chainload test

Tested once:

- `/tmp/wInd3x/wInd3x cfw run /tmp/n3g-osos-work/n3g-osos-cfw-visible-format4.dfu`

Observed:

- pre-run device state was normal DFU:
  - `05ac:1223`
  - DFU state `2`
- after handoff attempt, the device immediately presented as WTF mode:
  - `05ac:1242`
- host-side scan in that state returned:
  - `LIBUSB_ERROR_BUSY`
- no visible RetailOS behavior occurred:
  - no Apple logo
  - no patched strings
  - no original connected UI

Conclusion:

- correcting the Nano 3G RetailOS wrapper from format `2` to format `4`
  removed one proven blocker but did not complete the handoff
- the remaining failure is now later in the WTF runtime path, after the
  BootROM image-format acceptance point

Current classification:

- **CHAINLOAD_WTF_STUCK**

## 2026-04-24 Nano 3G format-4 handoff still rejected

Verified after the failed chainload test:

- cached defanged WTF really includes both Nano 3G raw patches
  - `0x1990` first-stage branch bypass
  - `0x19b8` second-stage handoff branch bypass
- the actual chainload image is really the regenerated format-`4` wrapper
  - magic `8702`
  - version `1.0`
  - format `4`
  - entry `0`
  - body `0xa4a570`
  - cert offset `0xa4a5f0`
  - cert length `0x300`

So the remaining failure is not:

- patch not applied
- wrong RetailOS image being sent

Recovered BootROM callback tracing now shows the next concrete gate.

In `0x200006dc`, once the type-`4` image passes the early type/layout checks,
the callback reaches:

- `0x200008e4: bl 0x200055f0`
- `0x200008e8: cmp r0, #1`
- `0x200008ec: bne 0x2000095c`

That is a later verification stage inside the type-`4` path. It happens after
format acceptance, and failure returns `0` from the callback.

Local Nano 3G `MakeUnsigned(...)` still fills the footer with placeholders:

- signature area:
  - `0x80` bytes of `'S'`
- certificate area:
  - `0x300` bytes of `'C'`

Therefore the best current explanation is:

- the format-`4` wrapper is now structurally accepted
- but the placeholder signature/certificate data is rejected by the later
  verification call at `0x200055f0`

Current classification:

- **FORMAT4_STILL_REJECTED**

## 2026-04-24 Nano 3G loader callback replacement prepared

Preferred fix direction is now concrete.

Instead of redirecting the full service pointer at `0x22007784`, the minimal
change is to patch only the one loader callback invocation in WTF:

- body offset `0x19ac`
- original:
  - `33 ff 2f e1`
  - `blx r3`
- replacement:
  - `c6 14 00 eb`
  - `bl 0x22006ccc`

Reason:

- only `[service + 0x74]` is the currently proven handoff blocker
- other service slots should stay on their BootROM implementations
- a direct callsite replacement preserves all other callbacks untouched

Local stub placement:

- body offset `0x6ccc`
- runtime `0x22006ccc`
- size `300` bytes
- fits entirely inside a confirmed zeroed free run:
  - start `0x6cca`
  - length `0x30a`

Stub behavior:

- if `r2 != 2`, tail-call original `0x200006dc`
- if `r2 == 2`, replay the BootROM type-`4` pre-verification loader work:
  - `0x200005dc`
  - type/layout checks
  - `0x20002574`
  - `0x2000273c`
  - `0x200020f8`
- skip only the later cert/signature verification gate:
  - `0x200008e4: bl 0x200055f0`
  - `0x200008e8: cmp r0, #1`
  - `0x200008ec: bne 0x2000095c`
- replay the original success metadata writes
- return `1`

Implemented in local `wInd3x`:

- `/tmp/wInd3x/pkg/cfw/defang_wtf.go`

Rebuilt:

- `/tmp/wInd3x/wInd3x`

Current decision:

- **LOADER_CALLBACK_STUB_PLAN_READY**

## 2026-04-24 Visibility status after local loader-stub failure analysis

The current blocker is no longer a visibility-path question.

Offline analysis of the first Nano 3G WTF-local loader stub showed that the
branch patch itself was valid, but the replacement callback did not reproduce
enough BootROM side effects to let WTF advance into RetailOS.

Important result:

- callsite patch at `0x220019ac` is correct
- failure class for the first local stub is:
  - **MISSING_SIDE_EFFECT**

Why this matters for visibility work:

- the device is still getting trapped before any RetailOS-visible proof point
  can execute
- that means more connected-screen string work, LCD proof work, or post-init
  UI substitution is not the next bottleneck
- the next justified action remains fixing the WTF -> RetailOS handoff itself

Specific side effects that were missing from the first local stub:

- `r9` preservation
- BootROM preflight helpers:
  - `0x20001ef0`
  - `0x20001fe0`
- original post-callback status handling at `0x19b8`
- fuller replay of the BootROM success-side callback path

Current visibility implication:

- visible proof points in RetailOS remain valid as later goals
- but they are still downstream of the current handoff failure

Revised decision:

- **VISIBILITY_BLOCKED_BY_WTF_HANDOFF**

Current safe boundary:

- revised local `wInd3x` stub has been rebuilt and verified offline
- no new device-side `cfw run` was performed during this analysis step

## 2026-04-24 Visibility impact after revised local-stub hardware test

One hardware test was performed with the revised cached Nano 3G defanged WTF.

Observed result:

- host left DFU and re-enumerated as:
  - `05ac:1242`
- no RetailOS-visible proof point appeared
- no Apple logo or connected-screen text appeared

Meaning:

- the revised local stub still fails before any RetailOS visibility target
  becomes relevant
- the visibility plan itself is unchanged
- the handoff remains the active bottleneck

Updated decision:

- **VISIBILITY_STILL_BLOCKED_BY_WTF_HANDOFF**

## 2026-04-24 Visibility result from cache-aware DRAM execution probe

The stronger DRAM probe still produced no visible signal.

Probe summary:

- WTF copied a tiny redirect stub into `0x08000800`
- cleaned dcache
- invalidated icache
- loaded `pc = 0x08000800`
- the DRAM stub should immediately jump back into an IRAM marker stub that
  calls `0x22002138(12)` and loops

Observed result:

- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock UI
- no visible reset loop
- host-side device remained in the same DFU-resident late-handoff state

Meaning:

- the absence of visible output is not explained by only missing cache
  maintenance after the DRAM write
- visibility is still blocked by a deeper entry-environment or execution-state
  issue than the current handoff patches have reproduced

Updated decision:

- **PROBE_OUTPUT_INVISIBLE**

## 2026-04-24 Alternate execution probes prepared

The earlier DRAM probe result remains visibility-negative, but because it lacked
cache maintenance it was not a decisive DRAM-execution test.

Prepared stronger probe:

- write known 8-byte redirect stub into destination
- clean dcache
- invalidate icache
- jump to destination

Prepared destinations:

- `0x08000800`
- `0x08000000`
- `0x08001000`
- `0x22008000`

Status:

- verified with raw-body dumper only
- no hardware run performed yet

## 2026-04-24 - direct PC entry patch prepared

Visibility work remains blocked upstream, but the current handoff experiment is
now a direct non-returning transfer into RetailOS entry.

Verified bytes:

- `0x22001b14 = 67 02 00 1b`
- `0x220024c8 = a7 14 00 ea`
- `0x2200776c = 04 f0 1f e5 00 08 00 08`

This preserves the wrapper setup but replaces the final `blx r4` with a local
stub that loads `pc = 0x08000800` directly. No hardware run has been performed
with this patch yet.

## 2026-04-24 - direct PC entry run result

Even a direct non-returning `pc` load to `0x08000800` produced no visible
progress:

- device remained in the same DFU-resident host state
- screen stayed black
- no connected-screen proof text appeared

This keeps the visibility work blocked by a failure at or immediately after the
entry-context transition, not by the wrapper entry or `blx r4` semantics alone.

## 2026-04-24 execution-context fix prepared

The next visibility-unblocking test should not change OSOS or add another
marker. The prepared handoff patch instead changes the execution context before
entry:

- preserves wrapper entry and `0x22002138(0)`
- replaces final `blx r4` with a local context stub
- disables IRQ/FIQ
- disables MMU and caches
- invalidates/drains caches
- then loads `pc = 0x08000800`

No hardware run has been performed with this context fix yet.

## 2026-04-24 execution-context fix run result

The execution-context cleanup test also produced no visible progress:

- device remained in the same DFU-resident host state
- screen remained blank
- no connected-screen proof text appeared

This keeps visibility blocked by a still-unidentified failure at or immediately
after the final handoff, even after matching the solved S5L87xx handoff pattern
more closely.

## 2026-04-24 DRAM execution probe prepared

Because the loaded OSOS body at `0x08000800` is a relocation trampoline rather
than a final UI/runtime body, visibility is now blocked by a likely
entry-environment dependency.

A cleaner diagnostic is prepared:

- write a tiny known stub into `0x08000800` from WTF
- jump there directly
- have that DRAM stub immediately redirect into a known IRAM marker stub

No hardware run has been performed with this DRAM probe yet.

## 2026-04-24 Visibility interpretation after late-gap branch/poll analysis

The `[service + 0x68]` bypass result still leaves visibility blocked upstream of
any OSOS-visible proof point. The next unresolved late-gap logic in WTF is:

- `0x220019c8`
  - branch to later success-side handling when `0x38c00040 & 3 == 0`
- `0x220019cc..0x220019d4`
  - poll loop waiting for `0x38c0000c & 1`

So the best next visibility-relevant split is not another OSOS patch. It is a
WTF-side reachability proof at `0x220019c8` while keeping the earlier
`0x22001938` bypass in place.

Prepared marker:

- `0x220019c8`
  - `15 00 00 0a` -> `41 17 00 ea`
  - target:
    - `0x220076d4`
  - marker behavior:
    - `0x22002138(11)`
    - loop forever

Updated decision:

- **LATE_GAP_MARKER_PREPARED**

## 2026-04-24 Visibility impact after `0x220019c8` late-gap marker

The late-gap marker at `0x220019c8` does not produce visible OSOS/runtime
progress, but it does prove reachability of the branch gate before the
`0x220019cc..0x220019d4` poll loop.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- the unresolved blocker is later than `0x220019c8`
- the next visibility-relevant split is the poll loop itself at
  `0x220019cc..0x220019d4`

Updated decision:

- **MARKER_19C8_REACHED**

## 2026-04-24 Visibility interpretation for the prepared `0x220019d4` bypass

The next visibility-relevant WTF-side experiment is the narrowest possible
poll-loop reduction:

- leave the AES/status sample at `0x220019cc` / `0x220019d0`
- remove only the `0x220019d4` back-edge

This avoids another blind OSOS-side patch and answers whether the remaining
black-screen handoff failure is just the stuck wait on `0x38c0000c & 1`.

Prepared bytes:

- `0x220019d4`
  - `fc ff ff 0a` -> `00 00 a0 e1`

Updated decision:

- **POLL_LOOP_BYPASS_PREPARED**

## 2026-04-24 Visibility impact after `0x220019d4` poll-loop bypass

The narrow poll-loop bypass at `0x220019d4` still does not produce any
RetailOS-visible proof point.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`
- screen stayed black:
  - no Apple logo
  - no backlight
  - no flicker
  - no patched text
  - no stock UI

Meaning:

- the late-gap poll wait on `0x38c0000c & 1` is not the only remaining upstream
  blocker
- visibility work remains blocked by deeper WTF-side state/control issues even
  after the back-edge is removed

Updated decision:

- **CHAINLOAD_BLACKSCREEN**

## 2026-04-24 Visibility interpretation for the prepared `0x220019d8` post-poll marker

With both the poll-loop back-edge removal and the completion-condition
emulation ending in the same black-screen result, the next visibility-relevant
split is whether control reaches the first post-poll tail instruction at
`0x220019d8`.

Prepared marker:

- `0x220019d8`
  - `00 00 50 e3` -> `3d 17 00 ea`
  - target:
    - `0x220076d4`
  - marker behavior:
    - `0x22002138(12)`
    - loop forever

Updated decision:

- **POST_POLL_MARKER_PREPARED**

## 2026-04-24 Visibility interpretation after `0x220019dc` final-tail marker

The final-tail marker at `0x220019dc` proves reachability of the conditional
branch after the late AES/status region, but it still does not produce any
visible OSOS/runtime progress on its own.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- the blocker is later than `0x220019dc`
- the next visibility-relevant split is the landing block at `0x22001a24`

Updated decision:

- **MARKER_19DC_REACHED**

## 2026-04-24 Visibility interpretation after `0x22001a24` landing-block marker

The landing-block marker at `0x22001a24` proves reachability of the post-poll
success landing, but it still does not produce visible OSOS/runtime progress on
its own.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- the blocker is later than `0x22001a24`
- the next visibility-relevant split is now further into the tail toward the
  previously unreached `0x22001ae8`

Updated decision:

- **MARKER_1A24_REACHED**

## 2026-04-24 Visibility interpretation after `0x22001a28` post-landing marker

The post-landing marker at `0x22001a28` proves reachability of the first call
after the success landing, but it still does not produce any visible
OSOS/runtime progress on its own.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- the blocker is later than `0x22001a28`
- the next useful split is further down the tail between `0x22001a28` and the
  still-failed `0x22001ae8`

Updated decision:

- **MARKER_1A28_REACHED**

## 2026-04-24 Visibility interpretation after `0x22001a4c` deeper tail marker

The deeper tail marker at `0x22001a4c` proves reachability further into the
post-landing linear path, but it still does not produce any visible
OSOS/runtime progress on its own.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- the blocker is later than `0x22001a4c`
- the next useful split remains deeper in the linear tail toward the still
  failed `0x22001ae8`

Updated decision:

- **MARKER_1A4C_REACHED**

## 2026-04-24 Visibility interpretation after `0x220019d8` post-poll marker

The post-poll marker at `0x220019d8` proves reachability just beyond the
late-gap AES/status region, but it still does not produce any visible OSOS
progress on its own.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- the blocker is later than `0x220019d8`
- the next visibility-relevant split is the following tail branch at
  `0x220019dc`

Updated decision:

- **MARKER_19D8_REACHED**

## 2026-04-24 Visibility interpretation for prepared AES/status completion emulation

The `0x220019d4` loop removal alone still left the device in
`CHAINLOAD_BLACKSCREEN`, so the next WTF-side test should satisfy the same late
gap status condition instead of merely skipping it.

Prepared change:

- `0x220019d0`
  - `01 00 11 e3` -> `01 10 b0 e3`
  - force the ready/completion condition to pass while preserving the rest of
    the branch flow

Updated decision:

- **STATUS_FLAG_PATCH_READY**

## 2026-04-24 Visibility impact after `0x220019d0` AES/status completion emulation

The late-gap completion emulation at `0x220019d0` still does not produce any
RetailOS-visible proof point.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- post-run USB state remained:
  - `05ac:1223`
  - DFU mode visible
- post-run `mks5lboot --dfuscan` returned:
  - `LIBUSB_ERROR_OTHER`
- screen stayed black:
  - no Apple logo
  - no backlight
  - no flicker
  - no patched text
  - no stock UI

Meaning:

- satisfying the single late-gap completion condition is not enough by itself
- visibility remains blocked by deeper WTF-side or transfer-state issues

Updated decision:

- **CHAINLOAD_BLACKSCREEN**

## 2026-04-24 Visibility meaning of the final-gap reduction

The failed `0x22001ae8` tail marker does not make the final execute gate the
next target. Static reduction of the current defanged build shows that the
first unresolved call in the gap is still:

- `0x22001938: blx [service + 0x68]`

Meaning:

- visibility remains blocked by an upstream WTF callback before the late tail
- the new lowest-risk proof/fix is to bypass the ignored-return callback at
  `0x22001938`
- only if that still fails should the later status loop at:
  - `0x220019cc..0x220019d4`
  become the next blocker candidate

Updated decision:

- **RUNTIME_SIGNAL_BLOCKED**

## 2026-04-24 Visibility impact after `[service + 0x68]` bypass hardware test

The local no-op bypass for the first unresolved callback at `0x22001938` still
did not produce any visible proof of OSOS execution.

Observed result:

- host remained in the DFU-resident post-upload state:
  - `05ac:1223`
  - `LIBUSB_ERROR_OTHER` on `dfuscan`
- screen stayed fully black for 90 seconds
- no Apple logo, backlight, flicker, patched text, stock UI, or reset loop

Meaning:

- bypassing `[service + 0x68]` is not sufficient by itself to produce visible
  boot progress
- the unresolved failure remains upstream of any display proof point
- later unresolved state inside the `0x22001938..0x22001ae8` band still
  matters, even with this callback removed

Updated decision:

- **CHAINLOAD_BLACKSCREEN**

## 2026-04-24 Visibility impact after 0x22001938 marker hardware retest

The new `0x22001938` forward marker still does not produce any RetailOS-visible
proof, but it does preserve the reached-marker host signature.

Observed result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- the device stayed visible on USB as:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- visibility work is still blocked by the same upstream WTF handoff failure
- however the unresolved handoff region is now known to be later than:
  - `0x22001938`
- no new display/audio/runtime conclusion should be drawn from this run beyond
  that continued upstream block

Updated decision:

- **VISIBILITY_STILL_BLOCKED_BY_WTF_HANDOFF**

## 2026-04-24 Visibility meaning of the reached `0x22001768` marker

The new middle-path marker at `0x22001768` produced the same shifted host-side
end state as the reached `0x22001758` marker:

- `wInd3x` timed out waiting for WTF
- the Nano did not disappear from USB
- `lsusb` still showed:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- control reaches at least as far as:
  - `0x22001768`
- so the unresolved blocker is later than:
  - `0x2200152c`
- the next useful visibility split is now:
  - `0x2200177c`
  - then `0x22001788`

Updated decision:

- **MARKER_1768_REACHED**

## 2026-04-24 Visibility meaning of the reached `0x2200177c` marker

The new middle-path marker at `0x2200177c` produced the same shifted host-side
end state as the already reached markers at `0x22001758` and `0x22001768`:

- `wInd3x` timed out waiting for WTF
- the Nano did not disappear from USB
- `lsusb` still showed:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- control reaches at least as far as:
  - `0x2200177c`
- so the unresolved blocker is later than:
  - `0x22006380`
- the next useful visibility split is now:
  - `0x22001788`

Updated decision:

- **MARKER_177C_REACHED**

## 2026-04-24 Visibility meaning of the reached `0x22001788` marker

The new middle-path marker at `0x22001788` produced the same shifted host-side
end state as the already reached markers at `0x22001758`, `0x22001768`, and
`0x2200177c`:

- `wInd3x` timed out waiting for WTF
- the Nano did not disappear from USB
- `lsusb` still showed:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- control reaches at least as far as:
  - `0x22001788`
- so the unresolved blocker is later than:
  - `0x22006430`
- the full currently tested middle-chain block is reached on hardware

Updated decision:

- **MARKER_1788_REACHED**

## 2026-04-24 Visibility meaning of the reached `0x22001798` marker

The new post-`0x1788` marker at `0x22001798` produced the same shifted
host-side end state as the already reached earlier markers:

- `wInd3x` timed out waiting for WTF
- the Nano did not disappear from USB
- `lsusb` still showed:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- control reaches at least as far as:
  - `0x22001798`
- so the unresolved blocker is later than this first post-`0x1788` call
- the next useful visibility split is now:
  - `0x220017a0`
  - then `0x2200181c`
  - then `0x2200182c`
  - then `0x22001938`

Updated decision:

- **MARKER_1798_REACHED**

## 2026-04-24 Visibility meaning of the aborted `0x220017a0` attempt

There is no new visibility conclusion from this attempt because the run never
started from a clean usable DFU state.

Observed host behavior:

- `lsusb` still reported `05ac:1223`
- `mks5lboot --dfuscan` failed with `LIBUSB_ERROR_OTHER`
- `wInd3x cfw run` failed before exploit execution with a USB string-descriptor
  I/O error

So this attempt does not distinguish `MARKER_17A0_REACHED` from
`MARKER_17A0_NOT_REACHED`. Nano 2G remains only a comparison reference for
expected clean handoff behavior, not a byte-level equivalence.

## 2026-04-24 Visibility meaning of the reached `0x220017a0` marker

The retried `0x220017a0` run produced the same shifted host-side end state as
the already reached earlier markers:

- `wInd3x` timed out waiting for WTF
- the Nano did not disappear from USB
- `lsusb` still showed:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- control reaches at least as far as:
  - `0x220017a0`
- the Nano 2G / S5L8701 comparison remains only a reference for expected clean
  handoff behavior, not a byte-level equivalence
- the unresolved blocker is later than:
  - `0x22003b5c`

Updated decision:

- **MARKER_17A0_REACHED**

## 2026-04-24 Visibility meaning of the reached `0x2200181c` marker

The `0x2200181c` run produced the same shifted host-side end state as the
already reached earlier markers:

- `wInd3x` timed out waiting for WTF
- the Nano did not disappear from USB
- `lsusb` still showed:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- control reaches at least as far as:
  - `0x2200181c`
- the unresolved blocker is later than:
  - `0x220032fc`
- the next useful visibility split is now:
  - `0x2200182c`
  - then `0x22001938`

Updated decision:

- **MARKER_181C_REACHED**

## 2026-04-24 Visibility meaning of the reached `0x2200182c` marker

The `0x2200182c` run produced the same shifted host-side end state as the
already reached earlier markers:

- `wInd3x` timed out waiting for WTF
- the Nano did not disappear from USB
- `lsusb` still showed:
  - `05ac:1223`
- `mks5lboot --dfuscan` failed with:
  - `LIBUSB_ERROR_OTHER`

Meaning:

- control reaches at least as far as:
  - `0x2200182c`
- the unresolved blocker is later than the `[state + 0x90]` callback dispatch
- the next useful visibility split is now:
  - `0x22001938`
  - versus the already failed tail marker at `0x22001ae8`

Updated decision:

- **MARKER_182C_REACHED**

## 2026-04-24 Post-handoff reachability map for OSOS idle/scheduler discovery

Recovered reachability:

- entry body still lands in the early Apple startup path rooted at:
  - `0x220039c4`
- that path explicitly performs init work and then falls through:
  - `0x22003aa4: b 0x22003af8`

Recovered stable loop:

- `0x22003af8: nop`
- `0x22003afc: nop`
- `0x22003b00: nop`
- `0x22003b04: nop`
- `0x22003b08: mcr p15, 0, r0, c7, c0, 4`
- `0x22003b1c: b 0x22003af8`

Meaning:

- `0x22003af8` is the first stable post-init loop recovered directly from the
  startup trace
- it is a stronger proof point than the earlier visibility hooks because it
  does not depend on LCD or audio becoming active

Recovered recurring callback setup immediately before that loop:

- `0x22003ac4`
  - arms a callback/timer path through:
    - `0x220035f4`
    - `0x220035fc`
- literal callback target:
  - `0x22003af4`
  - `b 0x22001d48`
- `0x22001d48` increments counters/state and looks timer/tick-like, but it is
  not needed for the first proof image because the idle shell at `0x22003af8`
  is already unconditional

Prepared proof mechanism:

- do not rely on LCD or audio
- branch from `0x22003af8` to a local stub that uses the already-known watchdog
  reboot path:
  - `WDT_BASE = 0x3c800000`
  - write:
    - `0x00100000`

Prepared verification:

- `0x22003af8`
  - `00 00 a0 e1` -> `3c 14 00 ea`
  - now disassembles as:
    - `b 0x22008bf0`
- `0x22008bf0`
  - local stub:
    - `mov r1, #0x3c800000`
    - `mov r0, #0x00100000`
    - `str r0, [r1]`
    - `b .`

Meaning:

- if the Nano reaches the first stable OSOS loop, the new image should convert
  that hidden state into a controlled reboot signature
- if the device does not reset, the failure is earlier than the recovered idle
  shell

Updated decision:

- **HOOK_POINT_FOUND**

## 2026-04-24 Visibility meaning of failed scheduler probe

The scheduler probe converted the first recovered post-init loop head into a
watchdog-reset test:

- `0x22003af8`
  - `00 00 a0 e1` -> `3c 14 00 ea`
  - branch to:
    - `0x22008bf0`
- `0x22008bf0`
  - writes:
    - `0x00100000`
  - to:
    - `0x3c800000`

Observed hardware result:

- host side still escaped the old WTF-stuck pattern and disappeared from USB
- device side stayed black for the full window
- no immediate reboot
- no delayed reboot
- no reboot loop

Meaning:

- the recovered loop-head probe did not yield an observable reset signature
- this leaves two main interpretations:
  - the post-handoff execution path does not actually reach `0x22003af8`
  - the watchdog write that is valid in earlier reboot research is not effective
    in this OSOS runtime context

So the scheduler-probe result is informative but not decisive. It does not yet
prove `OSOS_ENTRY_NOT_REACHED`; it only proves that this particular
loop-to-watchdog observability path did not produce a hardware-visible signal.

Updated decision:

- **SCHEDULER_PROBE_NOT_TRIGGERED**

## 2026-04-24 Structural proof redesign after failed scheduler probe

The scheduler watchdog probe failed to produce a reset signature, so the next
proof should not rely on display, audio, or watchdog side effects.

Corrected entry-point distinction:

- `0x22000800` is normal body code inside the image
- the one-time startup shell is:
  - `0x22008808`
    - stack setup
  - `0x22008814`
    - branch into:
      - `0x220039c4`

This makes `0x22008814` the correct first structural cutoff for an entry probe,
not `0x22000800`.

Repeated/runtime candidates recovered during the same pass:

- `0x22003ac4`
  - arms a recurring callback path through imported services
  - literal callback target:
    - `0x22003af4`
    - `b 0x22001d48`
- `0x22001d48`
  - updates counters/state and still looks timer/tick-like
- `0x22003b28`
  - enters RTXC-style service dispatch via:
    - `0x2200360c`

Prepared structural entry probe:

- patch:
  - `0x22008814`
  - `6a ec ff ea` -> `fe ff ff ea`
- effect:
  - self-loop immediately before the full startup body at `0x220039c4`

Why this is the preferred first probe:

- it answers the narrow question:
  - does OSOS entry execute at all?
- it avoids watchdog assumptions
- it avoids LCD/audio dependence
- it does not add raw USB PHY writes

Prepared image:

- `/tmp/n3g-osos-work/n3g-osos-entry-loop-probe.dfu`

Verification:

- `0x22008814` now disassembles as:
  - `eafffffe  b 0x22008814`

Limitation:

- unchanged host behavior would still be ambiguous, because USB disappearance
  may occur before this probe point
- but any changed host-visible behavior would strongly support entry execution

Updated decision:

- **OSOS_ENTRY_PROBE_PREPARED**

## 2026-04-24 Visibility meaning of unchanged OSOS entry-loop probe

The structural entry probe replaced:

- `0x22008814`
  - `b 0x220039c4`

with:

- `0x22008814`
  - `b 0x22008814`

Observed host result:

- exploit completed
- defanged WTF upload completed
- `wInd3x` timed out waiting for WTF
- the Nano disappeared from USB entirely
- no `05ac:1242`
- no DFU device after the run

Meaning:

- host behavior was unchanged versus the current post-WTF baseline
- strongest operational classification is:
  - `ENTRY_LOOP_NOT_REACHED`
- but the probe remains inconclusive as a proof of non-execution, because USB
  teardown may happen before `0x22008814`

Updated decision:

- **ENTRY_LOOP_NOT_REACHED**

## 2026-04-24 Corrected meaning of the WTF execute path

The chainload execute path had to be re-anchored to the correct binary:

- WTF body:
  - `/tmp/n3g-wtf-decrypted.body.bin`
- not the OSOS body

Correct execute path:

- `0x22002fe4`
  - enters `0x22001698` with:
    - `r0 = 0x1d`
    - `r1 = 0x08000000`
    - `r2 = 0x00f80000`
- after readiness and post-loader service work:
  - `0x22001b04: cmp r4, #0`
  - `0x22001b0c: tst r5, #16`
  - `0x22001b10: addne r0, r6, r7`
  - `0x22001b14: blne 0x220024b8`
- execute wrapper:
  - `0x220024bc: mov r4, r0`
  - `0x220024c4: bl 0x22002138`
  - `0x220024c8: blx r4`

So the exact intended OSOS transfer is:

- `0x220024c8: blx r4`

with target:

- `r4 = 0x08000800`

State at the transfer is not the suspicious part:

- ARM state to even address
- no Thumb mismatch
- valid caller and callee stack frames

Current strongest failure class:

- **EXECUTE_NEVER_CALLED**

Meaning:

- the missing proof is still before the actual WTF -> OSOS jump
- unchanged host behavior with the OSOS entry-loop probe does not override the
  earlier failed pre-execute marker; it only confirms that later OSOS-side
  probes are not proving reachability

Updated decision:

- **EXECUTE_PATH_MAPPED**

## 2026-04-24 Final-gate condition reduction

In the current defanged WTF build:

- readiness callback at `0x2200197c` is already replaced by the local stub at
  `0x22006e00`
- loader callback at `0x220019ac` is already replaced by the local stub at
  `0x22006ccc`
- UART helper at `0x22006558` is already patched to immediate return

That changes the meaning of the final execute analysis:

- the unresolved problem is no longer the `0x22006558` UART poll
- the last unresolved tail before `0x22001b04` is now:
  - `0x22001ae8: bl 0x22002724`

Static final-gate conditions remain satisfied:

- `r4` is set to `0` at `0x22001a24`
- no direct writes to `r4` occur before `0x22001b04`
- `r5 = 0x1d`, so `r5 & 0x10` is set before `0x22001b0c`

So the execute gate itself is not the active unknown condition. Reachability of
the last unresolved tail is.

Prepared visibility marker:

- `0x22001ae8`
  - `0d 03 00 eb` -> `94 14 00 ea`
  - branch to:
    - `0x22006d40`
- marker stub at `0x22006d40`:
  - `mov r0, #4`
  - `blx 0x22002138`
  - loop forever

Updated decision:

- **FINAL_GATE_MARKER_PREPARED**

## 2026-04-24 Visibility meaning of the failed `0x22001ae8` final-tail marker

The current defanged WTF build already:

- replaces the readiness callback with a local success stub
- bypasses the UART helper at `0x22006558`

So the marker at:

- `0x22001ae8`

was specifically meant to prove reachability of the last unresolved call before
the execute gate.

Observed result:

- host behavior remained identical to the current post-WTF baseline
- no new mode/state change attributable to `0x22002138(4)`
- no `05ac:1242`
- no DFU device afterward

Meaning:

- strongest current classification:
  - `MARKER_1AE8_NOT_REACHED`
- therefore the blocker is earlier than:
  - `0x22001ae8`

Updated decision:

- **MARKER_1AE8_NOT_REACHED**

## 2026-04-24 Visibility meaning of the new middle-path split

The final-tail marker at `0x22001ae8` was not reached, so the next useful split
must move into the unresolved middle of `0x22001698`.

Prepared active checkpoint:

- `0x22001758`
  - original:
    - `blx [service + 0x80]`
  - replacement:
    - `b 0x22006f00`
- marker stub:
  - `0x22006f00`
  - `mov r0, #2`
  - `blx 0x22002138`
  - loop forever

Prepared inactive follow-ups:

- `0x22001768 -> 0x22006f20`
- `0x2200177c -> 0x22006f40`
- `0x22001788 -> 0x22006f60`

Meaning:

- if the earliest middle-path callback is reached, WTF should switch to marker
  state `2` and stall there
- if not, the blocker is even earlier than the current middle-path cluster

Updated decision:

- **MARKER_1758_PREPARED**

## 2026-04-24 Visibility meaning of the reached `0x22001758` marker

The active marker at:

- `0x22001758`

was designed to call:

- `0x22002138(2)`

and then loop forever.

Observed host result:

- previous baseline:
  - Nano disappeared from USB entirely after handoff
- marker run:
  - Nano remained visible as:
    - `05ac:1223`
    - DFU mode

Meaning:

- this is the first clear middle-path reachability proof
- strongest current classification:
  - `MARKER_1758_REACHED`

So the unresolved blocker is later than:

- `0x22001758: blx [service + 0x80]`

Updated decision:

- **MARKER_1758_REACHED**

## 2026-04-24 Later runtime hook map after black-screen startup tests

The current Nano 3G evidence no longer supports spending more cycles on the
early `0x22005640` visibility wrapper by itself.

Mapped early OSOS reachability:

- entry:
  - `0x22008814 -> 0x220039c4`
- startup helpers:
  - `0x22004778`
  - `0x220045b0`
  - `0x22003594`
  - `0x220044c4`
  - `0x220045ac`
  - `0x22003ac4`
  - `0x22003b28`
- early startup hook already tested:
  - `0x22003a9c`
  - original:
    - `bl 0x22003d98`
- compact display/visibility cluster:
  - `0x22005620`
  - `0x22005640`
  - `0x22005660`
  - connected branch:
    - `0x220057d0`

Interpretation:

- those startup/display wrappers are real and reachable early in OSOS startup
- but the hardware result remained:
  - black screen
  - no backlight
  - no flicker
- so they are not sufficient proof of later Apple runtime execution

Later runtime/audio candidates recovered:

- strings:
  - `Beep`
  - `PlayTone`
  - `DiskMode_ScreenLayout_Connected`
  - `RemoteUI_Ok_To_Disconnect_String`
- object/helper path:
  - `0x2219d048`
  - `0x2219d1e8`
  - `0x2219d410`
  - `0x22276c98`
- later controller/runtime path:
  - `0x220ffb00..0x22100324`

Most useful later hook:

- `0x22100318`
  - later than the failed startup hooks
  - already sits inside a controller/runtime handler that passes through the
    Apple-owned audio path

Prepared proof hook:

- hook site:
  - runtime `0x22100318`
  - body offset `0x100318`
  - original:
    - `91 fb ff eb`
    - `bl 0x220ff164`
  - replacement:
    - `2c 22 fc eb`
    - `bl 0x22008bd0`
- local stub:
  - runtime `0x22008bd0`
  - body offset `0x8bd0`
  - disassembly:
    - `push {r0, r1, lr}`
    - `bl 0x220ff164`
    - `ldr r0, [sp]`
    - `ldr r0, [r0, #0xc8]`
    - `add r1, sp, #12`
    - `bl 0x2219d410`
    - `pop {r0, r1, pc}`

Meaning:

- preserve the original later-runtime call
- then invoke the Apple-owned `Beep` manager with the same caller frame/state
  that the connected/runtime handler already built
- this gives a non-display proof option without inventing new raw hardware
  writes

Prepared artifacts:

- helper:
  - `/tmp/wInd3x/cmd/patch_n3g_later_beep_signal.go`
- patched IMG1:
  - `/tmp/n3g-osos-work/n3g-osos-cfw-later-beep-n3g.dfu`
- patched body:
  - `/tmp/n3g-osos-work/lcd-osos-later-beep-n3g.bin`

Updated decision:

- **LATER_HOOK_PATCH_READY**

## 2026-04-24 Visibility result from later Apple-owned beep proof hook

The later-runtime Apple `Beep` hook changed nothing observable on-device.

Observed behavior:

- black screen for the full run
- no Apple logo
- no backlight
- no flicker
- no patched text
- no stock text
- no audible beep/tone/click
- no visible reset loop

Host side still showed:

- defanged WTF upload completed
- no re-enumeration as:
  - `05ac:1242`
- USB disappeared entirely after upload

Meaning:

- the later hook is not sufficient as a runtime proof signal
- either the hooked path is still not reached on the successful chainload path,
  or the Apple-owned audio/display service it invokes is also nonfunctional in
  the current runtime state

Updated decision:

- **RUNTIME_SIGNAL_BLOCKED**

## 2026-04-24 Early OSOS backlight-enable path prepared

After the readiness-stub handoff fix, Nano 3G host behavior changed from stable
WTF re-enumeration to "USB disappears entirely", but the screen still remained
black. That means the useful next signal is an earlier Apple display/backlight
wrapper, not another later connected-screen string patch by itself.

Backlight/display extraction result:

- no direct PMU `LEDCTL` / `0x28` / `0x29` writes were recovered as a simple
  body-visible backlight function in decrypted OSOS
- the strongest compact Apple-owned callable path remains:
  - `0x22005640`
    - `bl 0x2200374c`
    - `bl 0x22003774`
    - `bl 0x220073b4`
    - `mov r1, #1`
    - `bl 0x22007610`
- interpretation:
  - compact "visibility on" wrapper through Apple runtime services

Earlier startup hook selected:

- startup sequence around:
  - `0x22003a98: mov r0, r4`
  - `0x22003a9c: bl 0x22003d98`
- the live patch point is:
  - runtime `0x22003a9c`
  - body offset `0x3a9c`
- replacement:
  - original `bd 00 00 eb`
  - patched `43 14 00 eb`
  - effect: `bl 0x22008bb0`

Local stub:

- runtime `0x22008bb0`
- body offset `0x8bb0`
- bytes:
  - `04 e0 2d e5`
  - `77 ec ff eb`
  - `a0 f2 ff eb`
  - `04 f0 9d e4`
- behavior:
  - `push {lr}`
  - `bl 0x22003d98`
  - `bl 0x22005640`
  - `pop {pc}`

Why this is the current low-risk patch:

- preserves the original Apple startup call first
- then invokes the compact Apple visibility-on wrapper early enough that a dark
  display/backlight path may still be recoverable
- introduces no new raw MMIO or PMU guesses

Prepared outputs:

- `/tmp/wInd3x/cmd/patch_n3g_backlight_signal.go`
- `/tmp/n3g-osos-work/n3g-osos-cfw-backlight-n3g.dfu`
- `/tmp/n3g-osos-work/lcd-osos-backlight-n3g.bin`

Updated decision:

- **PATCH_READY**

## 2026-04-24 Early startup visibility wrapper hardware result

The early startup hook that redirects `0x22003a9c` into a local stub calling
`0x22005640` did not produce any visible change on Nano 3G hardware.

Observed screen result:

- full black screen for 90 seconds
- no Apple logo
- no backlight
- no flicker
- no connected-screen text, patched or stock

Observed host result:

- the device still left the old stuck-WTF pattern
- `wInd3x` timed out waiting for WTF re-enumeration
- the Nano disappeared from USB entirely

Meaning:

- the compact Apple "visibility on" wrapper `0x22005640` is not sufficient by
  itself, even when forced early in startup
- the remaining blocker is likely below this wrapper boundary:
  - missing display power/gate state
  - missing earlier panel/LCD init
  - missing backlight path not covered by the visibility wrapper

Updated decision:

- **RUNTIME_BLACKSCREEN**

## 2026-04-24 Visibility impact of BootROM readiness callback trace

The readiness callback at `0x200036c8` is now identified as a visibility
blocker before the loader setup path.

Visibility-relevant findings:

- `0x200036c8` does not just return a flag
- it can loop forever while polling BootROM-managed state:
  - `[state + 0x2c]`
  - `[state + 0x738 + 0x37]`
  - `[state + 0x04]`
- on return, it always performs two scratch-global updates:
  - `*0x2203fff8 = (*0x2203fff8)->next`
  - `*0x2203fffc = (*0x2203fffc + 0x2000)->0x720`

That means the visibility problem is not “WTF ignores RetailOS entrypoint.”
The handoff is still blocked earlier by BootROM readiness state.

Prepared local visibility-unblocker:

- replace the callback callsite at:
  - `0x2200197c`
- with a local Nano 3G stub at:
  - `0x22006e00`
- stub effect:
  - replay the two observed scratch-global updates
  - return `r0 = 1`
  - avoid the non-returning wait loop

Why this is the narrowest justified bypass:

- it does not touch NAND, storage, USB PHY, LCD, PMU, or unrelated MMIO
- it keeps the post-call WTF flow intact at:
  - `0x22001980`
- it reproduces the only directly observed persistent state changes from the
  callback itself

Updated decision:

- **VISIBILITY_READY_WITH_READINESS_STUB**

## 2026-04-24 Visibility impact of the first readiness-stub hardware run

The first Nano 3G hardware run with the readiness stub did not fall back to the
old visible WTF-stuck signature.

Host-visible difference from earlier runs:

- previous failing signature:
  - re-enumerate as `05ac:1242`
  - remain in WTF mode
- readiness-stub run:
  - `wInd3x` timed out waiting for WTF mode
  - no `05ac:1242` appeared
  - no DFU device appeared either
  - the Nano disappeared from USB entirely until manual recovery

Visibility interpretation:

- this is the first run where the old handoff blocker appears to be cleared
- that is not yet proof of patched or unpatched RetailOS UI on screen
- but it is stronger than the previous visibility state because the host no
  longer sees the stale WTF handoff failure

Updated visibility decision:

- **VISIBILITY_ADVANCED_PAST_WTF_BLOCK**

## 2026-04-24 Visibility impact after upstream path isolation before `0x22001b14`

The failed pre-execute marker at `0x22001b14` moves the useful visibility split
earlier.

Current path interpretation:

- if control reaches `0x22001a24`, the remaining path to `0x22001b14` is
  almost linear under the currently prepared patches:
  - `r4 = 0`
  - `0x22006558` returns immediately from the UART bypass patch
  - `r5 & 0x10` is already set on the active `cfw run` path
- therefore the lack of any `0x1b14` marker effect strongly suggests:
  - `0x22001a24` is not reached

The next visibility split is now staged at three earlier checkpoints:

- Marker A:
  - runtime `0x22006d40`
  - effect:
    - call `0x22002138(2)`
    - loop forever
  - intended branch site:
    - `0x220019bc`

- Marker B:
  - runtime `0x22006d60`
  - effect:
    - call `0x22002138(3)`
    - loop forever
  - intended branch site:
    - `0x22001a24`

- Marker C:
  - runtime `0x22006d80`
  - effect:
    - call `0x22002138(4)`
    - loop forever
  - intended branch site:
    - `0x22001b04`

Chosen next single hardware test:

- branch from `0x22001a24` to Marker B

Why this split is best:

- it is the earliest clean post-callback success landing
- if reached, the path forward to the already-tested `0x1b14` checkpoint should
  be nearly linear
- if not reached, the blocker is definitely earlier, most likely in:
  - `0x220019bc..0x220019dc`
  - or another upstream failure leg before `0x22001a24`

Prepared state only:

- local defanger now uses:
  - `0x1a24 -> cd 14 00 ea`
- the old `0x1b14` proof marker is removed:
  - restored to `67 02 00 1b`
- cached Nano 3G defanged WTF was cleared again
- no hardware run performed yet

Updated decision:

- **VISIBILITY_SPLIT_MOVED_UPSTREAM_TO_0x1a24**

## 2026-04-24 Visibility impact after hardware test of marker at `0x22001a24`

The earlier post-callback marker did not produce any new visible or host-visible
state.

Observed host result:

- pre-run device state:
  - `05ac:1223`
  - DFU state `2`
- post-run USB state:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan`:
  - `LIBUSB_ERROR_BUSY`

Meaning:

- the prepared branch from `0x22001a24` to Marker B did not create evidence of a
  new reached checkpoint
- the active failing path still appears to stop before the post-callback
  success landing

Updated visibility interpretation:

- the next useful visibility split must move earlier than `0x22001a24`
- the highest-value remaining region is now:
  - `0x220019bc..0x220019dc`

Updated decision:

- **VISIBILITY_BLOCKED_BEFORE_0x1a24**

## 2026-04-24 Visibility impact after hardware test of marker at `0x220019b0`

The earliest prepared post-loader marker still did not create a new visible or
host-visible state.

Observed host result:

- pre-run state:
  - `05ac:1223`
  - DFU state `2`
- post-run USB state:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan`:
  - `LIBUSB_ERROR_BUSY`
- after manual recovery:
  - `05ac:1223`
  - DFU state `2`

Meaning:

- the branch at `0x220019b0` did not produce evidence of a reached marker
- the active failing path appears to diverge or stall before the post-loader
  compare at `0x220019b0`

Updated visibility interpretation:

- useful proof points must move earlier than `0x220019b0`
- the next likely target region is now before the callback result handling,
  rather than later in `0x22001998..0x220019dc`

Updated decision:

- **VISIBILITY_BLOCKED_BEFORE_0x19b0**

## 2026-04-24 Visibility impact after preparing loader-callback precall marker

The next proof point is now moved to the loader-callback callsite itself.

Active prepared split:

- `0x220019ac`
  - original:
    - `bl 0x22006ccc`
  - active replacement:
    - `b 0x22006d40`

Meaning:

- if the next hardware run shows any marker-attributable change, execution
  reaches the callsite before entering the callback/stub path
- if the next hardware run still shows the same WTF behavior, the divergence is
  even earlier than the callback callsite

Prepared but inactive follow-up:

- local loader stub entry:
  - `0x22006ccc`
  - would branch to marker B at:
    - `0x22006d60`
  - not active in the current build

Updated decision:

- **VISIBILITY_SPLIT_MOVED_TO_PRECALL**

## 2026-04-24 Visibility impact after hardware test of pre-call marker at `0x220019ac`

The pre-call marker at the loader-callback callsite still did not create a new
visible or host-visible state.

Observed host result:

- pre-run state:
  - `05ac:1223`
  - DFU state `2`
- post-run USB state:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan`:
  - `LIBUSB_ERROR_BUSY`
- after manual recovery:
  - `05ac:1223`
  - DFU state `2`

Meaning:

- the branch at `0x220019ac` did not produce evidence of a reached marker
- the active failing path appears to diverge or stall before the loader-callback
  callsite

Updated visibility interpretation:

- useful proof points must move earlier than `0x220019ac`
- the next likely target region is now before the callback callsite setup at:
  - `0x22001998..0x220019a8`

Updated decision:

- **VISIBILITY_BLOCKED_BEFORE_0x19ac**

## 2026-04-24 Visibility impact after preparing pre-loader setup marker

The next proof point is now moved into the setup block immediately before the
loader-callback callsite.

Active prepared split:

- `0x22001998`
  - original:
    - `ldr r0, [r9]`
  - active replacement:
    - `b 0x22006d40`

Meaning:

- if the next hardware run shows a marker-attributable change, execution
  reaches the start of the setup block
- if the next hardware run still shows the same WTF path, the divergence is
  earlier than `0x22001998`

Prepared but inactive follow-ups:

- `0x220019a0 -> 0x22006d60`
- `0x220019a8 -> 0x22006d80`

Updated decision:

- **VISIBILITY_SPLIT_MOVED_TO_0x1998**

## 2026-04-24 Visibility impact after hardware test of pre-loader marker at `0x22001998`

The earliest prepared pre-loader marker still did not create a new visible or
host-visible state.

Observed host result:

- pre-run state:
  - `05ac:1223`
  - DFU state `2`
- post-run USB state:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan`:
  - `LIBUSB_ERROR_BUSY`
- after manual recovery:
  - `05ac:1223`
  - DFU state `2`

Meaning:

- the branch at `0x22001998` did not produce evidence of a reached marker
- the active failing path appears to diverge or stall before the pre-loader
  setup block

Updated visibility interpretation:

- useful proof points must move earlier than `0x22001998`
- the next likely target region is now before this block, likely in the
  predecessor path into `0x22001998`

Updated decision:

- **VISIBILITY_BLOCKED_BEFORE_0x1998**

## 2026-04-24 Visibility impact after predecessor-path analysis before `0x22001998`

The predecessor map explains why the old `0x22001998` marker was not useful in
the current defanged build.

Key result:

- `0x22001998` should be reached only by falling through:
  - `0x2200198c`
  - `0x22001990`
  - `0x22001994`
- but the old `0x1990/0x1994` bypass patch forced the path away from that edge

Updated live split:

- active marker now moves to:
  - `0x22001994`
- prepared follow-ups remain earlier at:
  - `0x2200198c`
  - `0x22001988`

Meaning:

- the next hardware run can finally answer whether the path reaches the real
  immediate predecessor of the loader setup block
- if `0x22001994` still does not show a marker-attributable effect, the
  divergence is earlier than the `r5 & 0x8` gate itself

Updated decision:

- **VISIBILITY_SPLIT_MOVED_TO_0x1994**

## 2026-04-24 Visibility impact after hardware test of predecessor marker at `0x22001994`

The real predecessor-edge marker still did not create a new visible or
host-visible state.

Observed host result:

- pre-run state:
  - `05ac:1223`
  - DFU state `2`
- post-run USB state:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan`:
  - `LIBUSB_ERROR_BUSY`
- after manual recovery:
  - `05ac:1223`
  - DFU state `2`

Meaning:

- the branch at `0x22001994` did not produce evidence of a reached marker
- the active failing path appears to diverge or stall before the predecessor
  edge into the loader setup block

Updated visibility interpretation:

- useful proof points must move earlier than `0x22001994`
- the next likely target region is now around:
  - `0x22001988`
  - or the callback-result path feeding it

Updated decision:

- **VISIBILITY_BLOCKED_BEFORE_0x1994**

## 2026-04-24 Visibility impact after preparing callback-result marker at `0x22001988`

The next proof point now moves into the callback-result failure branch.

Active prepared split:

- `0x22001988`
  - original:
    - `beq 0x22001a28`
  - active replacement:
    - `b 0x22006d80`

Meaning:

- if the next hardware run shows a marker-attributable change, execution
  reaches the callback-result decision block
- if the next hardware run still shows the same WTF path, the divergence is
  earlier than the callback-result branch itself

Prepared but inactive follow-up:

- `0x2200198c -> 0x22006d60`

Updated decision:

- **VISIBILITY_SPLIT_MOVED_TO_0x1988**

## 2026-04-24 Visibility impact after hardware test of callback-result marker at `0x22001988`

The callback-result branch marker still did not create a new visible or
host-visible state.

Observed host result:

- pre-run state:
  - `05ac:1223`
  - DFU state `2`
- post-run USB state:
  - `05ac:1242`
  - WTF mode
- post-run `mks5lboot --dfuscan`:
  - `LIBUSB_ERROR_BUSY`
- after manual recovery:
  - `05ac:1223`
  - DFU state `2`

Meaning:

- the branch at `0x22001988` did not produce evidence of a reached marker
- the active failing path appears to diverge or stall before the callback-result
  decision block

Updated visibility interpretation:

- useful proof points must move earlier than `0x22001988`
- the next likely target region is now:
  - the `[service + 0x6c]` callback itself at `0x2200197c`
  - or the preceding setup around `0x22001974..0x22001980`

Updated decision:

- **VISIBILITY_BLOCKED_BEFORE_0x1988**

## 2026-04-24 Visibility impact after preparing callback pre-call marker at `0x2200197c`

The next proof point now isolates the readiness callback callsite itself.

Active prepared split:

- `0x2200197c`
  - original:
    - `blx r0`
  - active replacement:
    - `b 0x22006d40`

Prepared but inactive follow-up:

- `0x22001980`
  - original:
    - `cmp r0, #0`
  - prepared replacement:
    - `b 0x22006d60`

Meaning:

- if the next hardware run shows a marker-attributable change, execution reaches
  the callback callsite
- if not, the divergence is earlier than the callback call itself
- if the pre-call marker is reached, the next clean split becomes the post-call
  marker at `0x22001980` to determine whether the callback returns

Updated decision:

- **VISIBILITY_SPLIT_MOVED_TO_CALLBACK_PRECALL**

## 2026-04-24 Visibility impact after hardware test of callback pre-call marker at `0x2200197c`

This run differed from the earlier “not reached” marker tests.

Observed host result:

- pre-run state:
  - `05ac:1223`
  - DFU state `2`
- after firmware upload, the host log stopped emitting the repeated
  `libusb: interrupted` spam that characterized the earlier no-marker runs
- USB still re-enumerated as:
  - `05ac:1242`
  - WTF mode
- after manual recovery:
  - `05ac:1223`
  - DFU state `2`

Meaning:

- there is now a material host-side behavioral change at the callback pre-call
  split
- this is the first evidence that execution likely reaches the `blx r0`
  readiness callback site
- the remaining ambiguity is now whether the callback would return on the
  original path

Updated visibility interpretation:

- the next useful split is the prepared post-call marker at:
  - `0x22001980`

Updated decision:

- **VISIBILITY_SUGGESTS_PRECALL_REACHED**

## 2026-04-24 Visibility impact after preparing callback post-call marker at `0x22001980`

The next proof point now isolates whether the readiness callback returns.

Active prepared split:

- `0x22001980`
  - original:
    - `cmp r0, #0`
  - active replacement:
    - `b 0x22006d60`

Meaning:

- if the next hardware run shows a marker-attributable change, the readiness
  callback returns to WTF
- if the next hardware run falls back to the previous quiet-WTF behavior with
  no marker effect, the callback is entered but does not return

Updated decision:

- **VISIBILITY_SPLIT_MOVED_TO_CALLBACK_POSTCALL**

## 2026-04-24 Visibility impact after hardware test of callback post-call marker at `0x22001980`

The post-call split resolves the callback question.

Observed host result:

- pre-run state:
  - `05ac:1223`
  - DFU state `2`
- after firmware upload, the host returned to the older repeated
  `libusb: interrupted` spam pattern
- USB re-enumerated as:
  - `05ac:1242`
  - WTF mode
- after manual recovery:
  - `05ac:1223`
  - DFU state `2`

Meaning:

- compared against the earlier pre-call run, which had gone quiet after upload,
  the post-call split did not show evidence of returning past `blx r0`
- strongest current interpretation:
  - the readiness callback is entered
  - the readiness callback does not return to WTF

Updated decision:

- **VISIBILITY_SHOWS_CALLBACK_ENTERED_NO_RETURN**

## 2026-04-24 Visibility impact after UART stall reduction

The post-loader blocker is now reduced further:

- the wait in `0x22006558` is a UART TX-FIFO drain
- selected block on the active path is:
  - `0x3cc00000`
- wait register is:
  - `0x3cc00018`
- wait bit is:
  - `0x200`

Cross-check against freemyipod's Nano UART test shows:

- `UFSTAT` is at `0x3cc00018`
- `while (UFSTAT & BIT(9))` is used before each transmit byte
- `UTXH` transmit register is at `0x3cc00020`

So this is a serial-output stall, not a RetailOS entrypoint or storage
precondition.

Visibility implication:

- if the earlier post-loader path reaches `0x22001a24`, then suppressing
  `0x22006558`'s unbounded TX wait should allow control to continue toward:
  - `0x220024b8`
  - `0x08000800`

Local Nano 3G defanged WTF is now prepared with:

- `0x22006558`
  - original: `f8 40 2d e9 02 70 a0 e1`
  - patched: `00 00 a0 e3 1e ff 2f e1`

Updated decision:

- **VISIBILITY_PATH_PREPARED_PAST_UART_STALL**

## 2026-04-24 Visibility impact after UART-bypass hardware retest

The prepared UART bypass patch did not change the host-visible handoff result.

Observed result:

- patched defanged WTF uploaded
- firmware upload started
- device still re-enumerated as:
  - `05ac:1242`
- no host-side evidence of leaving WTF mode appeared

Meaning:

- the visibility path is still blocked before any RetailOS-visible proof point
- suppressing the UART transmit helper alone was not enough to advance the
  handoff

Updated decision:

- **VISIBILITY_STILL_BLOCKED_BEFORE_RETAILOS**

## 2026-04-24 Visibility impact after execute-gate verification

The execute wrapper itself is now understood well enough that visibility is no
longer blocked by entrypoint ambiguity.

Verified:

- the tested WTF artifact really contained the UART-bypass patch
- `0x220024b8` takes the target directly from `r0`
- the Nano 3G path passes:
  - `0x08000800`
- the current OSOS payload has:
  - `Header.Entrypoint = 0`
- and valid code is present at runtime body start:
  - `0x08000800`

So the remaining visibility ambiguity is purely runtime:

- either WTF never reaches the execute gate at `0x22001b14`
- or it reaches it and the OSOS body immediately returns/crashes before any
  visible proof point

A proof-only pre-execute marker is prepared:

- patch `0x22001b14` from:
  - `blne 0x220024b8`
- to:
  - `blne 0x22006d40`
- where `0x22006d40` calls:
  - `0x22002138(3)`
  and then loops forever

That should let the next hardware test distinguish:

- execute gate not reached
- execute gate reached

without guessing new MMIO.

Updated decision:

- **VISIBILITY_RUNTIME_PROOF_PATCH_READY**

## 2026-04-24 Visibility impact after pre-execute marker integration

The next Nano 3G run is now set up to answer one narrow question only:

- does WTF reach the execute-gate branch at `0x22001b14`?

Prepared marker behavior:

- patch `0x22001b14` from:
  - `blne 0x220024b8`
- to:
  - `blne 0x22006d40`
- marker stub at `0x22006d40` calls:
  - `0x22002138(3)`
- then loops forever

Meaning for visibility:

- if the marker path is reached, the next hardware run should show a changed
  pre-RetailOS outcome instead of the same opaque WTF stall
- if there is no behavioral change, the execute gate is still not being hit

The RetailOS image itself is unchanged, so this run isolates the execute-gate
question without introducing a new OSOS variable.

Updated decision:

- **VISIBILITY_EXECUTE_GATE_PROOF_READY**

## 2026-04-24 Visibility impact after pre-execute marker hardware test

The proof marker did not produce any host-visible behavioral change.

Observed result:

- patched marker WTF uploaded
- firmware upload started
- device still re-enumerated as:
  - `05ac:1242`
- post-run scan still showed:
  - `LIBUSB_ERROR_BUSY`

Meaning:

- there is no positive sign that control reached the marker at `0x22006d40`
- so visibility is still blocked before the execute wrapper / RetailOS body

Updated decision:

- **VISIBILITY_BLOCKED_BEFORE_EXECUTE_GATE**

## 2026-04-24 Visibility impact after post-loader WTF tracing

The post-loader path is now narrow enough that visibility is blocked by one
specific pre-execute service phase rather than by a generic “more callback
state”.

Key corrections:

- `[service + 0x8c]` is only BootROM setter `0x2000106c`
- `[service + 0x44]` is a temporary pointer-slot override, not a callback
- the active `cfw run` path enters `0x22001698` with flags:
  - `0x1d`
  - so both `r5 & 0x8` and `r5 & 0x10` are already set

Meaning:

- once the post-loader path reaches `0x22001a24`, the execute gate is already
  armed
- the return value from `0x22006558` is ignored
- `r4` is not modified again before the execute test

So the only remaining pre-execute blocker in that post-loader region is the
non-returning UART wait inside:

- `0x220065d0..0x220065fc`

polling:

- `[selected_uart_base + 0x18] & 0x200`

If that bit never clears, WTF never reaches:

- `0x220024b8`
- RetailOS body entry `0x08000800`

Updated decision:

- **VISIBILITY_BLOCKED_BY_UART_SERVICE_STALL**

## 2026-04-24 Visibility impact after callback state reconstruction

The new callback-state reconstruction shifts the likely blocker again:

- the old local stub already replayed the visible metadata writes
  - entrypoint
  - version bytes
- but it skipped `0x200055f0` completely

That matters because the immediate post-callback WTF gate is not the metadata
bytes. It is:

- `0x38c00040`
- `0x38c0000c`

and those are in the AES block on S5L8702.

So the best current visibility interpretation is:

- RetailOS-visible proof points are still downstream
- the remaining missing state is most likely helper/MMIO side effects preserved
  by the original BootROM callback path
- the minimal next fix is to let BootROM `0x200006dc` and `0x200055f0` run,
  then patch only the final success-side metadata/return path

Updated decision:

- **CALLBACK_STATE_REQUIRED_PATCH_READY**

## 2026-04-24 Visibility impact after narrowed callback-state hardware retest

The narrower patch that lets BootROM run `0x200006dc` and `0x200055f0` still
does not reach any RetailOS-visible proof point.

Observed result:

- firmware upload began from WTF
- the device re-enumerated as:
  - `05ac:1242`
- no Apple logo appeared
- no connected-screen proof text appeared

Meaning:

- the visibility work remains blocked by the same upstream handoff failure
- preserving the original verification-side helper/MMIO path was necessary to
  test, but not sufficient to get past WTF mode

Updated decision:

- **VISIBILITY_STILL_BLOCKED_BY_WTF_HANDOFF**
