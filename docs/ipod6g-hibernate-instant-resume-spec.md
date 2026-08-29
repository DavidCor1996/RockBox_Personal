# iPod Classic 6G/7G Hibernate and Instant Resume Specification

## Status

This is a research and implementation specification, not a claim that full
resume is already safe. The retained-RAM and controlled retained-payload gates
now pass on a real iPod Classic. The controlled CPU-context code also completes
its bounded round trip after a forced reset, but the ABI-5 R3 hardware attempt
remained black and did not respond to Menu or USB before that reset. R3 then
misclassified the forced reset as a wake because `pmu_is_hibernated()` proves
only that GPIO3 is low and the PMU does not report a cold boot. It does not
prove that the PMU ever entered Standby. ABI-6 Stage 3A-R4 added a PMU-retained
`ENTRY_STALLED` result and captured complete early PMU status. Its first
hardware result proves that the CPU stopped, SDRAM and the controlled context
survived, and USB insertion reached the PMU as an EXTON2 rising edge, but the
PMU did not automatically wake the SoC. ABI-7 Stage 3A-R5 attempted to correct
only the EXTON2 wake-edge mode and added exact pre-entry readback. It failed
closed before arming because its raw-polled live-context write did not change
OOCMODE. Stage 3A-R6 retains ABI 7 and performs that live preflight write
through Rockbox's normal serialized PMU driver instead. Repeated retention,
full kernel continuation, driver resume, and fault-injection gates remain
experimental and incomplete.

The target is the existing `IPOD_6G` Rockbox target. Rockbox uses that target
for both the 6th- and 7th-generation Classic hardware.

### Implementation status

The Stage 1 implementation is now present in the personal tree:

- the application and bootloader linker scripts protect the fixed 64 KiB
  range `0x0bfec000` through `0x0bffbfff`;
- `hibernate-6g.c` implements the retained PCF `MEMBYTE0..7` ownership token,
  ordered publication, CRC-8 validation, the fixed SDRAM control record,
  CRC-32 validation, the 48 KiB IRAM shadow, and a 12 KiB deterministic
  retention pattern;
- the bootloader has an early Rockbox ownership decision and Stage 1
  validation/fallback path;
- the application-side Stage 1 request, preparation, I2C preflight, and arming
  operations are separate, so no ordinary power-off can accidentally publish
  ownership;
- an enabled bootloader publishes a CRC-protected capability record on cold
  Rockbox boots, and the application refuses to prepare or arm without that
  matching resume ABI handshake;
- the hidden `Test retained standby` debug action is only compiled when Stage
  1 is enabled. It creates a 30-second, one-shot request and then uses the
  normal `sys_poweroff()` pipeline so audio, storage, LCD, and other hardware
  are shut down before the target-specific handoff;
- before arming, the application verifies the final polling I2C implementation
  by writing the current `OOCWAKE` value unchanged and reading it back through
  the normal PMU driver;
- the final routine disables IRQ/FIQ and both caches, enters the exact
  RetailOS MIU mode, and performs the GPIO3 and `OOCSHDWN = 2` writes using
  bounded polling code in IRAM;
- the linked experimental image has been audited, not merely the C source:
  the entry routine and every post-self-refresh call target are in IRAM, and
  the active main stack is in IRAM;
- the bootloader gate is compile-time disabled by default through
  `IPOD6G_HIBERNATE_STAGE1=0`.

The normal and Stage 1 application builds and the normal and Stage 1 bootloader
builds all pass. The first deliberately armed real-hardware Stage 1 cycle also
passes. Stage 1 validates retained SDRAM and then performs a normal Rockbox
load; it does **not** jump back into the old kernel or UI. Production settings,
a kernel resume jump, and automatic hibernation remain out of scope until the
later gates pass.

The compile-time-gated Stage 2 implementation is now present and passes its
local build and linked-image audit. It advances the resume path by executing
one 256-byte retained application payload on a dedicated 2 KiB retained DRAM
stack, verifying its return cookie and observed stack pointer, and then using
the same normal Rockbox cold-load fallback as Stage 1. Its first real-hardware
cycle passes all Stage 2 checks. It is still not an instant UI resume and is
not enabled in a normal build.

The compile-time-gated Stage 3A implementation is also present. It saves the
Rockbox system-mode context, re-snapshots IRAM after capture, verifies an exact
translation-table CRC and app/bootloader build-version fingerprint, restores
IRAM, and performs a bounded context/stack round trip through an IRAM1
bootloader frame. Normal, Stage 2, and Stage 3 app/bootloader builds pass, the
linked retained code contains no unapproved external direct branch, and the
Stage 3 dependency guard rejects an incomplete configuration. Its first
hardware attempt did not visibly return. ABI-4 Stage 3A-R2 added an isolated
runtime directory, explicit build identity, broader test-only PMU wake enables,
and raw handoff breadcrumbs. Its first hardware run exposed a second evidence
loss path: ordinary capability publication replaced the attempt with `ready`
once no PMU token remained. ABI-5 Stage 3A-R3 keeps valid `CAPABLE`, `PASSED`,
and `FAILED` records and converts every valid nonterminal record to a durable
failure instead of replacing it. One R3 hardware run proved that persistence
and the controlled context handoff after a forced reset, but it did not enter
or wake from PMU Standby. ABI-6 R4 adds a fail-closed Standby-entry proof before
any further context test. Its hardware run proved Standby entry and isolated
the remaining USB failure to EXTON2's wake-edge mode. ABI-7 R5 captured the
live preflight configuration but failed closed when its raw-polled write left
OOCMODE unchanged. R6 keeps ABI 7 and changes only that app-side write
transport to the normal PMU driver already proven by the immediately preceding
OOCWAKE write.

### First real-hardware retention result — 2026-08-28

The first controlled cycle passed on the personal iPod Classic using the
matching experimental dual-boot bootloader and the isolated Rolo application:

- the normal personal RockPod firmware booted successfully after the
  bootloader update;
- before arming, the application reported `State: ready`, `Seq:0`,
  `attempts:0`, `Phase:0`, and `failure:0`;
- the one-shot action entered retained standby and woke into the normal
  Rockbox cold-load path as designed for Stage 1;
- returning through Rolo reported `State: PASSED` and `failure:0`;
- `PASSED` is only committed after one validation attempt reaches
  `IPOD6G_HIBERNATE_PHASE_DATA_VERIFIED` (`Phase:6`), which means both the
  48 KiB IRAM-shadow CRC and deterministic 12 KiB SDRAM-probe CRC matched.

This result proves the bounded PMU/I2C entry path, Rockbox ownership token,
MIU self-refresh transition, early bootloader claim, retained-memory
validation, and safe normal-load fallback for one cycle. That Stage 1 result
does not prove controlled execution from retained SDRAM by itself; the
separate Stage 2 gate below now proves that handoff. It does not prove
transparent kernel/UI resume, peripheral restoration, long-duration
retention, or repeat reliability.

### First real-hardware controlled-payload result — 2026-08-28

The first Stage 2 cycle passed on the same personal iPod Classic using the
matching ABI-2 dual-boot bootloader and isolated Rolo application:

- preflight reported ready with `mode:0`, `attempts:0`, `Phase:0`, and
  `failure:0`;
- the iPod entered retained standby, woke, executed the retained payload, and
  then cold-loaded the normal personal RockPod firmware;
- returning through Rolo reported `State:PASSED`, `mode:2`, `attempts:1`,
  `Phase:8`, and `failure:0`;
- expected cookie, observed cookie, and return value all reported `48365032`;
- the observed SP was inside the required retained-stack interval
  `0x0bfec800` through `0x0bfecfff`.

This proves one complete boot-ROM/bootloader-to-retained-application-code
handoff, including payload bounds and CRC validation, the IRAM1 assembly
wrapper, the dedicated DRAM stack, payload execution and return, post-return
stack/cookie checks, PMU ownership clearing, and safe normal-load fallback. It
does not yet prove restoration of a suspended Rockbox CPU/kernel context,
scheduler continuation, peripheral restoration, repeat reliability, or the
production instant-resume latency target.

### First real-hardware controlled-context attempt — 2026-08-28

The first Stage 3A attempt was inconclusive and is recorded as a failure, not
as evidence that the context path works:

- the isolated application armed the test and the iPod entered retained
  standby;
- neither pressing Menu nor inserting a cable produced a visible Rockbox
  screen, and a forced reset eventually returned to the normal personal
  firmware;
- the archived application had accidentally been configured with
  `ROCKBOX_DIR="/.rockbox"` even though its manifest claimed `/.rbtv`, so Rolo
  reused the normal theme and settings and was visually indistinguishable from
  the personal firmware; and
- after a forced reset, the ABI-3 bootloader cleared the stale ownership token
  and published a fresh capability record. That destroyed the last context
  phase and made it impossible to tell whether the PMU had failed to wake or
  the bootloader had woken and then hung during the handoff.

Stage 3A-R2 uses ABI 4 so it cannot arm against the earlier bootloader. Its
application is compiled against `/.rbtv`, displays an unavoidable three-second
`HIBERNATE TEST BUILD / Stage 3A-R2 / ABI 4 / Runtime: /.rbtv` splash, and
labels its debug screen the same way. The explicitly armed test uses PMU wake
mask `0xc7` (ONKEY, EXTON1, EXTON2, dedicated USB insert, and adapter insert),
requires exact readback before arming, and records OOC status plus PMU INT1 and
INT2 status on wake. Ordinary Rockbox power-off keeps its established wake mask
unchanged.

Five raw breadcrumbs identify the last boundary crossed: bootloader context
call, application continuation entry, application return dispatch, IRAM1
return-stub entry, and return to the bootloader caller. If a handoff hangs, a
subsequent forced reset converts the stale `RESUMING` token into a CRC-protected
failure record while preserving that breadcrumb. This makes the next attempt
diagnostic rather than another blind repetition.

### Stage 3A-R2 evidence-persistence result — 2026-08-28

The R2 isolation gate behaved correctly: before the ABI-4 bootloader was
installed, Rolo displayed `Runtime: /.rbtv` and refused to arm with
`Matching ABI-4 bootloader required`. The verified dual-boot installer was then
sent in Apple DFU state 2, and the iPod re-enumerated normally. With the
matching pair installed, the debug action became ready and one attempt was
armed.

After the subsequent reboot and manual Rolo launch, every diagnostic field had
returned to the original `ready` capability state. This is not a Stage 3 pass
or a meaningful handoff result. Static review identified the deterministic
cause: `ipod6g_hibernate_stage1_publish_capability()` cleared the entire valid
SDRAM record whenever a later boot no longer had a Rockbox-owned PMU token.
That included terminal results and intermediate records that could otherwise
identify the failed boundary.

Stage 3A-R3 increments all Stage 3 protocol versions to ABI 5. On cold boot it:

- leaves a valid `CAPABLE`, `PASSED`, or `FAILED` record unchanged;
- converts any other valid record into a durable failure, preserving the
  context metadata and raw breadcrumb; and
- publishes a new `CAPABLE` record only when no compatible valid record exists.

Preparing the next attempt remains the explicit acknowledgement that clears
the previous result. Thus viewing, Rolo-loading, or cold-booting cannot erase
the evidence before it is recorded.

### Stage 3A-R3 forced-reset result — 2026-08-28

One isolated ABI-5 R3 attempt was armed. The display shut off, but the iPod
remained black: Menu did not wake it and USB insertion did not wake or enumerate
it. A forced reset was required. After the normal personal firmware booted,
Rolo displayed `State:PASSED`, `mode:3`, `attempts:1`, `phase:12`, and
`failure:0`, with matching translation-table CRCs and the expected context
cookie.

That screen proves that the forced-reset bootloader restored IRAM, entered the
saved Rockbox mode and stack, ran the bounded retained continuation, returned
through the IRAM1 stub, and preserved the result. It is **not** a Standby/wake
pass. Static review found the false-positive path: the final entry deliberately
left GPIO3 low, and `pmu_is_hibernated()` interprets GPIO3 low plus no cold-boot
bit as a hibernated system. A forced reset therefore followed the same ABI-5
resume path even if the CPU had merely continued spinning after
`OOCSHDWN = 2`.

Stage 3A-R4 increments the Stage 3 token, record, and resume ABI to 6. After
issuing `OOCSHDWN`, it waits 250 ms in IRAM. A real Standby transition cannot
return to that instruction; if execution continues, R4 writes a CRC-protected
`ENTRY_STALLED` state into PMU-retained bytes. On the next forced reset the
bootloader records `failure:16` (`STANDBY_NOT_ENTERED`) and cold-boots without
executing retained context. R4 also captures these pre-`pmu_preinit` groups:

- `PMU`: `OOCSHDWN`, `OOCWAKE`, `OOCMODE`, and `OOCSTAT`;
- `IRQ`: `INT1` through `INT4`; and
- `PWR`: `INT5`, PCF50635 `INT6`, `GPIO3CFG`, and `OOCCTL`.

Only an automatic return caused by a real wake source, with an ABI-6 `ARMED`
token that was never converted to `ENTRY_STALLED`, may enter the context
validation path. Do not arm ABI 5 again.

### Stage 3A-R4 Standby-entry and USB-wake result - 2026-08-29

One matching ABI-6 R4 controlled-context attempt was armed. USB was inserted
only after the display had gone black. The iPod did not wake automatically;
Menu+Select was required to force-reset it. Rolo then reported a complete
`PASSED`, mode 3, attempt 1, phase 12, failure 0 context result with matching
TTB CRCs, the expected `48365033` context cookie, and the `boot returned`
breadcrumb.

This is not an automatic-wake pass. It does prove real Standby entry: R4's
IRAM tail did not survive long enough to publish `ENTRY_STALLED`, so the PMU
stopped the CPU before the 250 ms deadline. It also reconfirms SDRAM retention,
IRAM restoration, controlled context entry, and the bounded return path after
the forced reset.

The early snapshot was:

```text
wake:001c80ed
PMU:ed58c700 IRQ:00401c80
PWR:27606100
```

Decoded before `pmu_preinit()` could clear the latches, `INT2=1c` includes
`EXTON2R=10`, while `OOCSTAT=ed` has EXTON2 high. The dedicated `USBINS` bit in
INT1 was not set. On this iPod, the inserted cable therefore reached the PMU
through EXTON2. `OOCWAKE=c7` had EXTON2 enabled, but the captured
`OOCMODE=58` selected EXTON2 mode `10`.

The NXP PCF50633 manual defines EXTON mode `10` as wake on falling edge while
a rising edge only starts the eight-second timer. Mode `01` is immediate wake
on a rising edge. The manual also states that OOCMODE resets only in NoPower,
not on entry to Standby. This exactly explains an EXTON2 rising interrupt with
no automatic wake. Source: [NXP PCF50633 User Manual, Tables 9 and 12 and
section 8.1.6.5](https://www.freecalypso.org/pub/GSM/GTA02/PCF50633UM_6.pdf).

Stage 3A-R5 increments the token, record, and resume ABI to 7. Immediately
before arming, it:

- reads the contiguous `OOCWAKE` through `OOCSTAT` register block;
- retains `WAKE | MODE<<8 | OOCCTL<<16 | OOCSTAT<<24` as `ENT` before data;
- changes only OOCMODE bits 3:2 from their current value to `01`, preserving
  all EXTON1, EXTON3, and ONKEY mode bits;
- writes the unchanged `OOCWAKE` and corrected `OOCMODE` through the audited
  polling I2C path; and
- reads the register block again, retains the after value, and refuses to arm
  unless both the wake mask and corrected mode match exactly.

The debug screen displays `ENT:<before>/<after>`. A successful R5 preflight
derived from the R4 snapshot would change only the mode byte, for example
`ed2758c7` to `ed2754c7`. The post-reset `PMU` field remains useful, but `ENT`
is the authoritative proof of what was configured immediately before Standby
in case the boot ROM changes PMU state during a forced reset.

### Stage 3A-R5 fail-closed preflight result - 2026-08-29

The ABI-7 mismatch gate passed, the matching dual-boot bootloader was
installed, and exactly one R5 attempt was requested. The screen powered down,
but inserting USB performed an ordinary cold boot. The durable record then
reported:

```text
State:FAILED mode:3
Attempts:0 phase:1 fail:6
TTB:599fd0c7/00000000
Cookie:48865033/00000000
ENT:e927e3c7/e927e3c7
```

`phase:1`, `fail:6`, and `attempts:0` prove that preparation completed but the
I2C preflight failed before the ownership token was armed. No retained context
was entered, so the subsequent USB boot is not a hibernate wake result.

The `ENT` values are authoritative: OOCWAKE was `c7`, OOCMODE was `e3`,
OOCCTL was `27`, and OOCSTAT was `e9`; the after-read was identical. The
preceding call to `pmu_set_wake_condition()` had successfully changed and read
back OOCWAKE through Rockbox's normal mutex-protected I2C/PMU driver. The R5
raw-polled preflight then reported no effective OOCMODE change. This isolates
the failure to use of the final-entry raw writer while the normal kernel and
I2C driver were still active, not to PMU register permissions or the desired
EXTON2 value.

Stage 3A-R6 therefore keeps the record, token, and resume ABI at 7 and changes
only the app-side OOCMODE write to `pmu_write()`. It still records before and
after evidence and refuses to arm unless OOCWAKE remains unchanged and
OOCMODE reads back with EXTON2 mode `01`. The raw IRAM writer used after
caches and interrupts are disabled is unchanged; R4 already proved that path
can write GPIO3CFG and OOCSHDWN and enter Standby. Because the ABI and
bootloader path are unchanged, R6 requires only a replacement isolated Rolo
core, not another NOR/DFU installation.

## Desired User Experience

When the user chooses Hibernate:

1. Rockbox flushes storage and quiesces hardware.
2. The iPod enters the same retained-SDRAM standby class used by RetailOS.
3. A button, cable, dock, or optional RTC alarm wakes the iPod.
4. Rockbox returns to the previous screen and application state without loading
   `rockbox.ipod` from storage.

The production target is visible UI in roughly 0.5 to 0.8 seconds after wake.
Audio may take slightly longer to restart. The first implementation is allowed
to resume previously playing audio in a paused state.

This project does **not** initially attempt a storage-backed 64 MiB hibernation
image. That would be slower, would add substantial writes, and would introduce
filesystem failure modes that retained SDRAM avoids.

## Feasibility Summary

| Capability | Assessment | Reason |
| --- | --- | --- |
| Prove SDRAM retention | High | RetailOS uses it; Rockbox already detects the PMU state and has a self-refresh exit path. |
| Resume a controlled Rockbox test payload | Medium-high | The bootloader runs from IRAM while SDRAM is retained and can restore IRAM0 before jumping. |
| Resume the Rockbox UI and kernel | Medium | RAM state survives, but every power-lost peripheral needs a resume hook and one-time initialization must be bypassed. |
| Resume audio transparently mid-track | Medium-low for the first version | Logical playback state is retained, but DMA, codec, PCM, and storage hardware state is not. |
| Resume arbitrary plugins immediately | Medium-low | Plugin code and data are retained, but plugins can own hardware or be blocked inside non-resumable driver calls. |
| Storage-backed hibernate | Technically possible, not recommended | It is not instant and creates large writes and additional corruption risks. |

Overall: a safe retained-RAM hibernate is realistic. A production-quality
transparent resume is a multi-stage driver project, not a single PMU register
write.

## What RetailOS Actually Does

The analyzed image is the decrypted iPod Classic 2.0.4 OSOS image. Addresses
below are expressed as offsets in the decrypted ARM payload, after its 0x800
byte IMG1 header. This avoids mixing IMG1 file offsets with the address at
which the NOR loader places the payload. Earlier research notes did mix those
two coordinate systems and consequently labeled the same instructions 0x800
bytes too high; the instruction bytes and register deductions were unaffected.

### Confirmed locations

| Payload offset | Finding | Confidence |
| --- | --- | --- |
| `0x15175c` | `CanHibernate` machine-capability key | High |
| `0x265760` | Deep-sleep statistics text | High |
| `0x2667e0` | `Enter Deep Sleep` usage-event label | High |
| `0x2667f4` | `Exit Deep Sleep` usage-event label | High |
| `0x2ad1ac` | `PCFPowerMgr` class/component name | High |
| `0x000364` | Low-level PCF state writer | High |
| `0x00053c` | Dedicated retained-standby shutdown stub | High |
| `0x00244c` | MIU/SDRAM mode routine | High |
| `0x00318c` | Disable I-cache while leaving the MMU enabled | High |
| `0x0031a4` | Disable D-cache while leaving the MMU enabled | High |
| `0x2bca38` | Separate MIU low-three-bit helper | High |

The strings prove that the feature and its policy exist, but the `Enter Deep
Sleep` and `Exit Deep Sleep` strings are event-log labels, not the sleep
implementation.

The stripped high-level policy call chain has not yet been given reliable
semantic names. It reaches hardware through indirect C++/component calls around
`PCFPowerMgr`, so naming a particular high-level function “hibernate” from raw
direct branches would be guesswork. The low-level PMU and MIU mechanisms below
are directly identified; the wake half is independently identified as ONB.

### Low-level PMU writer at `0x08000b64`

The routine prepares a two-byte PCF I2C transaction to device address `0x73`:

- input mode 0 writes register `0x0c = 1`
- input mode 1 writes register `0x0c = 2`
- input mode 2 writes register `0x16 = 0`

Register `0x0c` is `OOCSHDWN`; bit 0 requests transition to Standby. Register
`0x16` is `GPIO3CFG`. Rockbox's existing bootloader independently documents
that RetailOS drives GPIO3 low when SDRAM contains a hibernated image.

The meaning of PCF50635-specific `OOCSHDWN` bit 1 is not documented by the
public PCF50633 manual. It is nevertheless no longer an inferred value: the
dedicated RetailOS retained-standby stub explicitly selects writer mode 1,
which writes `OOCSHDWN = 2`. Ordinary shutdown selects writer mode 0 and writes
`OOCSHDWN = 1`. The Stage 1 retention test must therefore use the observed
Apple value 2; using Rockbox's ordinary value 1 would test a different PMU
state.

### MIU routines

The stock routine at payload offset `0x244c` programs the same S5L8702 MIU
constants used by Rockbox, including:

- controller base `0x38100000`
- `MIUSDPARA = 0x1fb621`
- mode registers `0x33` and `0x8040`
- the same bank/timing values now present in `miu_preinit()`

The separate helper at payload offset `0x2bca38` changes the low three bits of
`MIUCON`:

| Helper input | Resulting low bits |
| --- | --- |
| 0 or 1 | 0 |
| 2 | 5 |
| 3 | 3 |
| 4 | 1 |

That helper is not called by the dedicated deep-sleep stub. The actual retained
standby path calls mode 1 of the routine at payload offset `0x244c`. Its complete
mode-1 action is:

```
MIUCON = (MIUCON & ~0x0f000000) | 0x0a100000;
MIU_REG(0x14) = 1;
```

RetailOS then waits 10 ms, drives PCF GPIO3 low, waits approximately 100 ms,
and writes `OOCSHDWN = 2`. Immediately before the MIU transition it disables
D-cache and I-cache, but leaves the MMU enabled. Rockbox's wake side already
uses `MIUCON = 0x11` before restoring the normal controller configuration.

Two calls in the stock stub pass through a component/service veneer table and
cannot be assigned semantics from the static image alone. They make no directly
visible MIU or PMU register writes. Stage 1 therefore reproduces only the
directly observed and independently checkable hardware sequence, keeps the
entry stub in IRAM, and treats retained-data validation after wake as the gate
for any later full-resume work.

### The important split: OSOS enters, ONB restores

The complete wake implementation is not in OSOS.

`bootloader/ipod-s5l87xx.c` documents and implements the actual boot flow:

1. The S5L8702 boot ROM loads the Rockbox bootloader from NOR into IRAM.
2. The bootloader checks `pmu_is_hibernated()` while SDRAM is still in
   self-refresh.
3. For a RetailOS snapshot, it loads the approximately 128 KiB Original NOR
   Boot image (ONB) into IRAM0.
4. ONB exits retention and restores the pre-hibernation RetailOS state.

The current Rockbox bootloader deliberately calls `launch_onb(1)` whenever the
PMU reports a hibernated system. A Rockbox resume implementation must branch
before that call and only when a Rockbox-owned token is valid.

Obtaining a read-only dump of the device's ONB remains valuable for comparing
clock, cache, and peripheral restore order. It is not required before the
retention-only probe because Rockbox already contains a self-refresh exit path.

## Existing Rockbox Support

### PMU and wake support

`firmware/target/arm/s5l8702/ipod6g/pmu-6g.c` already provides:

- `pmu_set_wake_condition()`
- `pmu_enter_standby()`
- `pmu_is_hibernated()` in the bootloader
- PMU initialization that keeps SDRAM rails configured
- wake support for ONKEY, external inputs, RTC, USB, and adapter insertion

`pmu_is_hibernated()` identifies the RetailOS convention: GPIO3 low and no
`COLDBOOT` indication.

The PMU has eight general-purpose retained bytes at registers `0x67` through
`0x6e`. They are declared in `firmware/export/pcf5063x.h` and are currently
unused by Rockbox. The NXP PCF50633 user manual explicitly says this memory can
support sleep states and is retained until the PMU enters NoPower, even when
only backup power remains. These bytes solve the early ownership problem:

- RetailOS hibernate + no Rockbox token: run ONB exactly as today.
- Rockbox hibernate + valid Rockbox token: run the Rockbox resume gate.
- Cold boot or invalid Rockbox record: load Rockbox normally.

Reference: [NXP PCF50633 User Manual, Rev. 06](https://www.freecalypso.org/pub/GSM/GTA02/PCF50633UM_6.pdf), section 8.16.

### SDRAM restoration

`firmware/target/arm/s5l8702/system-s5l8702.c` already has
`miu_preinit(bool selfrefreshing)`. When `selfrefreshing` is true it first writes
`MIUCON = 0x11`, skips destructive SDRAM mode-register programming, and then
restores normal refresh/controller configuration.

`system_preinit()` already:

1. initializes clocks, GPIO, and I2C,
2. reads `pmu_is_hibernated()`,
3. initializes the PMU,
4. calls `miu_preinit(hibernated)`.

That is a strong starting point for the bootloader-side retention probe.

### Memory map and volatile IRAM

The 6G target has 64 MiB of SDRAM:

- SDRAM: `0x08000000` through `0x0bffffff`
- translation table: final 16 KiB at `0x0bffc000`
- physical IRAM0: `0x22000000` through `0x2201ffff`
- physical IRAM1: `0x22020000` through `0x2203ffff`

The application linker uses only 48 KiB of IRAM0 for exception vectors, IRAM
code/data, IRAM BSS, and the main/IRQ/FIQ stacks. IRAM is not assumed to survive
PMU Standby. Before sleep, Rockbox must copy the complete used 48 KiB IRAM
window into a reserved SDRAM shadow. On wake the bootloader is executing from
IRAM1, so it can restore IRAM0 without overwriting itself.

### Thread state

Non-running Rockbox threads already have `r4-r11`, `sp`, and `lr` stored in
their retained `struct regs`. Only the thread performing the final hibernate
transition needs an explicit setjmp-like save record. The resume trampoline
must additionally preserve CPSR and any CP15 state not rebuilt by the
bootloader.

The bootloader must not call `kernel_init()` during resume. Existing thread
queues, mutexes, stacks, and scheduler state are the state being resumed.

## Required Architecture

### 1. Reserve retained SDRAM explicitly

Reserve a fixed block immediately below the translation table by changing the
6G application linker layout. Do not place an ad-hoc structure inside the
current plugin or codec buffer.

Initial reservation:

- 64 KiB total
- 48 KiB IRAM shadow
- one 4 KiB control/CPU-context page
- remaining space for checksums, diagnostics, and alignment

Suggested fixed range:

- `0x0bfec000` through `0x0bffbfff`
- existing TTB remains at `0x0bffc000`

The exact address must be asserted by both linker scripts at build time. The
application's usable plugin-buffer end must move down by the reservation size.

### 2. Use a two-level ownership record

#### PMU retained token

Use all eight `MEMBYTE` registers:

| Byte | Meaning |
| --- | --- |
| 0-3 | magic `RBH6` |
| 4 | protocol version |
| 5 | state: armed, resuming, or failed |
| 6 | format/build ABI byte |
| 7 | CRC-8 over bytes 0-6 |

Write bytes 4-7 first and the four-byte magic last. A partially armed token
must not claim a RetailOS snapshot. Clear the magic only after GPIO3 has been
returned high or after the system is known not to be hibernated.

#### SDRAM control record

The reserved control page should contain:

- 64-bit magic and format version
- target/model number
- record length and state
- matching PMU protocol/build ABI
- monotonic attempt sequence
- Rockbox build fingerprint
- saved resume PC, SP, LR, CPSR, and `r4-r11`
- required CP15/MMU state
- IRAM shadow address, size, and CRC32
- control-record CRC32
- sentinel/checksum description for the retention probe
- requested wake mask and observed wake reason
- last completed entry and resume phases
- failure code and boot-attempt count

The PMU token establishes ownership before SDRAM is touched. The SDRAM record
then establishes whether the retained Rockbox image is compatible and intact.

### 3. Add an early bootloader resume gate

The decision must occur in `bootloader/ipod-s5l87xx.c` after minimal I2C setup
and before the current unconditional hibernation call to `launch_onb(1)`.

Decision table:

| PMU hibernated | Rockbox token | Action |
| --- | --- | --- |
| No | any stale token | clear token when safe; normal boot |
| Yes | absent | launch ONB exactly as current code does |
| Yes | valid and armed | change token to resuming; enter Rockbox resume gate |
| Yes | Rockbox-owned but malformed/retrying | never launch ONB; force safe Rockbox cold boot |

The gate then:

1. runs the non-destructive hibernated form of `system_preinit()`,
2. returns GPIO3 high as part of PMU initialization,
3. exits SDRAM self-refresh,
4. rebuilds the Rockbox-compatible MMU mapping without clearing SDRAM,
5. validates the SDRAM control record and required checksums,
6. restores the 48 KiB IRAM shadow into IRAM0,
7. invalidates stale instruction/TLB state as required,
8. branches to the retained Rockbox resume trampoline.

It must not run `bss_init()`, load `rockbox.ipod`, or initialize the kernel.

If validation fails after Rockbox ownership was established, GPIO3 must first
be high, the PMU token must be cleared, and the bootloader must perform a normal
Rockbox cold boot. It must not hand a Rockbox memory image to ONB.

### 4. Add an IRAM-only final entry stub

After MIU enters self-refresh, executing code or reading a stack in SDRAM is no
longer safe. The final entry routine and everything it calls must execute from
IRAM and use an IRAM stack.

Implemented Stage 1 final sequence:

1. use the normal shutdown coordinator to quiesce playback, storage, display,
   video output, and other active hardware;
2. prepare and checksum the SDRAM record, copy the used 48 KiB IRAM window,
   and fill/checksum the deterministic 12 KiB probe region;
3. preflight the IRAM polling I2C primitive while SDRAM is still available;
4. publish the PMU ownership token and commit the complete data cache;
5. disable IRQ and FIQ, clean/discard both caches, and leave the MMU enabled;
6. from IRAM, program RetailOS MIU mode 1 and drain the write buffer;
7. wait 10 ms, drive PMU GPIO3 low, and wait approximately 100 ms;
8. write the RetailOS retained-standby value `OOCSHDWN = 2` through the
   IRAM-safe polling I2C primitive;
9. loop in IRAM if the PMU does not remove power.

The generic PMU driver and normal I2C stack are not used after step 5 because
their code, data, locks, or stack may live in SDRAM. If publishing GPIO3 fails,
Stage 1 requests ordinary standby rather than claiming an unmarked retained
image. Every polling wait is bounded, so a wedged I2C controller cannot trap
the code before the final fallback request.

### 5. Resume through a controlled trampoline

The retained trampoline should behave like `setjmp`/`longjmp`:

- initial save returns 0 and proceeds into hibernate,
- bootloader restoration returns 1 at the same controlled call site,
- no arbitrary interrupted instruction is resumed.

After returning with 1, Rockbox runs hardware-only resume hooks and only then
reenables interrupts and other threads.

## Driver Quiesce and Resume Contract

Retained memory does not imply retained hardware state. A production resume
needs explicit pre-sleep and post-wake phases.

### Mandatory before sleep

- reject hibernate during recording
- complete `system_flush()` and `storage_flush()`
- wait for ATA activity to stop and place storage in its safe sleep state
- stop PCM and DMA
- close or suspend codec hardware
- disable composite output cleanly
- suspend LCD/backlight hardware
- quiesce USB and accessory transactions
- stop timers that can update retained state during the final transition
- mask VIC/EINT sources

No hibernate attempt may continue after a storage flush error or timeout.

### Mandatory after wake

Hardware-only resume functions must be split from one-time initialization:

- clocks and timers
- GPIO, VIC, EINT, and DMA
- PMU interrupt masks and cached input state, without creating another PMU
  thread or queue
- ATA controller and storage media state
- click wheel and hold switch
- LCD and backlight
- codec and PCM
- USB/accessory state
- composite output, but only if the setting and dock state require it

Only after those hooks succeed may the scheduler continue normally.

### Time behavior

The first implementation should treat Rockbox tick time as paused while the
iPod is hibernated. It should restart the tick source without replaying hours
of missed timeouts. RTC wall time naturally advances. A later implementation
can add subsystem-specific elapsed-time handling.

### Audio and plugins

For the first production-capable milestone:

- audio that was playing is saved as a logical resume position and returns
  paused, or is restarted through the normal playback path after hardware
  resume;
- recording vetoes hibernate;
- arbitrary plugins veto hibernate unless they opt into a suspend/resume hook;
- MPEG/video playback is stopped cleanly and can later gain an application
  checkpoint hook.

Trying to preserve live DMA or codec register state is explicitly out of scope
for the first version.

## Staged Implementation Plan

### Stage 0: Static confirmation and ONB capture

- Add no behavior changes.
- Document the OSOS addresses above in the reverse-engineering notes.
- Add a read-only method to dump and hash the user's existing ONB from NOR.
- Compare ONB's MIU/cache/clock wake sequence with Rockbox's proposed gate.
- Read the eight PMU memory bytes after RetailOS cold boot, hibernate, and wake
  to document any stock use.

ONB capture improves confidence but must not delay the retention-only probe.

### Stage 1: PMU token and SDRAM retention probe

Build a hidden debug action, disabled by default, that:

1. fills a reserved test area with deterministic patterns and guard pages,
2. records full or striped CRCs,
3. shadows IRAM,
4. arms the PMU token,
5. enters candidate self-refresh and PMU Standby,
6. lets the bootloader restore SDRAM,
7. validates and displays PASS/FAIL,
8. clears the token and performs a normal cold boot.

It must not attempt to resume the Rockbox kernel yet.

Test patterns must catch row, column, stuck-bit, and walking-bit failures. Run
at least 100 cycles, including overnight retention.

The first hardware gate uses the implemented deterministic 12 KiB probe plus
the independent 48 KiB IRAM-shadow CRC. After one clean PASS establishes that
the PMU/MIU transition works, expand the probe patterns before the 100-cycle
and overnight qualification runs.

### Stage 1 build and hardware gate

Stage 1 must be enabled in **both** images with
`-DIPOD6G_HIBERNATE_STAGE1=1`. A Rolo-loaded application alone cannot test it:
the matching bootloader must make the ownership decision from IRAM before a
disk image is loaded.

Before the first test, retain a known-good bootloader/NOR backup, verify that
DFU recovery is available, and use a charged battery. Installing the matching
experimental bootloader is the only NOR-changing step and must be separately
approved; the debug action itself never writes NOR.

After the matching images are installed:

1. cold-boot Rockbox and open `System > Debug > Test retained standby`;
2. confirm the screen says `State: ready`; otherwise do not arm it;
3. press Select once. Rockbox runs its normal shutdown, enters retained
   standby, and powers off;
4. wake with Menu or by inserting a supported cable;
5. let Rockbox load normally, then return directly to the same debug screen;
6. a successful cycle reads `State: PASSED`, `Phase:6`, and `failure:0`.

Capture the state, phase, and failure numbers before another cold boot, because
a later ordinary boot republishes the capability record. A failure or forced
reset must be reported before repeating the test.

### Stage 2: Controlled payload resume

Resume one tiny retained test function on a dedicated DRAM stack. The
implemented Stage 2 gate deliberately keeps result display and device access
out of the retained payload:

1. The application records the exact linked payload start, size, entry point,
   CRC-32, return cookie, and fixed stack bounds in the retained record. It
   writes guard words at the bottom of the 2 KiB stack.
2. On wake, the bootloader first performs all Stage 1 IRAM-shadow and SDRAM
   probe checks, then independently validates the payload bounds, entry point,
   CRC-32, stack bounds, and guards.
3. A 40-byte assembly wrapper resident in bootloader IRAM1 saves the
   bootloader stack, switches to the retained stack, and calls the payload.
4. The linked payload is exactly 256 bytes in the audited build. It has no
   calls, global-data dependencies, storage access, PMU access, LCD access, or
   branch outside its own section. It records a cookie, its observed stack
   pointer, and a return value before returning through the wrapper.
5. The bootloader verifies the returned state, cookie, stack pointer, and
   guards, commits a CRC-protected PASS or FAIL record, clears PMU ownership,
   and continues through the established normal Rockbox load.

Keeping the payload hardware-independent means this gate proves the complete
bootloader-to-retained-application-code handoff without involving the
scheduler, drivers, or a second reset. A payload hang still requires a hard
reset; automatic watchdog recovery is a later gate.

Stage 2 must be enabled in both images with both
`-DIPOD6G_HIBERNATE_STAGE1=1` and
`-DIPOD6G_HIBERNATE_STAGE2=1`. It uses token, record, and resume ABI version 2,
so it cannot accidentally handshake with the already-tested Stage 1 ABI-1
pair. A normal build keeps both gates off and does not link the retained
payload section or handoff wrapper.

The isolated Rolo application is the only application image used for the
hardware gate; the main personal firmware remains untouched. A matching
dual-boot Stage 2 bootloader is still required because the ownership decision
and retained call happen before any disk image is loaded. Never install it as
a single-boot image.

After one armed wake and a return to the Rolo debug action, success must show:

- `State:PASSED`, `mode:2`, `attempts:1`, `Phase:8`, and `failure:0`;
- expected cookie, observed cookie, and return value all equal `48365032`;
- observed SP in `0x0bfec800` through `0x0bfecfff`.

Any other value is a failed Stage 2 gate and must be documented before another
attempt. The first real-hardware cycle produced every value above and passed.

### Stage 3A: Controlled CPU-context round trip

Stage 3A is implemented as a hidden, compile-time-gated hardware test. It is
deliberately not a user-visible instant-resume feature yet. Its purpose is to
prove that the bootloader can restore Rockbox's IRAM and system stack, enter a
CRC-covered continuation with the saved CPU context, and return safely to the
bootloader before any scheduler or driver is restarted.

The application side:

1. uses the ordinary Rockbox shutdown coordinator so playback is stopped,
   filesystems are flushed, storage is put to sleep, and the display is shut
   down before target `power_off()` runs;
2. disables IRQ and FIQ and saves r4-r11, the system SP and LR, a controlled
   continuation PC, CPSR, CP15 control, translation-table base, and domain
   access control;
3. takes a final 48 KiB IRAM0 snapshot after context capture and commits its
   CRC; and
4. enters the already-qualified retained-SDRAM standby sequence.

The matching bootloader executes entirely from IRAM1. After MIU recovery it:

1. claims the one-shot PMU token and validates record/resume ABI version 5;
2. compares a CRC of its Rockbox build version with the application build,
   validates the retained payload CRC, and requires the rebuilt translation
   table and saved CP15 state to match exactly;
3. restores all 48 KiB of IRAM0 and verifies the restored image CRC;
4. saves an aligned bootloader frame in IRAM1, changes to the saved Rockbox
   system mode and stack, restores r4-r11/LR, and branches only to the
   validated continuation inside the retained payload section;
5. records the observed cookie and stack pointer, then returns through the
   supplied and range-checked IRAM1 stub; and
6. commits PASS or FAIL, clears PMU ownership, and cold-loads the normal
   Rockbox image.

The context continuation does not enable interrupts, call kernel code, touch
devices, or return to the suspended UI. A hang leaves the PMU token in the
`RESUMING` state. Stage 3A-R4 preserves the last raw boundary breadcrumb when
the next hard reset takes the fail-closed cold-boot recovery path. It also
refuses to interpret a forced reset as a wake after the final-entry code proves
that Standby was not entered. Stage 3A therefore tests the dangerous
mode/stack/code transition without pretending that full kernel resume is
already safe.

Stage 3A must be enabled in both images with:

```text
-DIPOD6G_HIBERNATE_STAGE1=1
-DIPOD6G_HIBERNATE_STAGE2=1
-DIPOD6G_HIBERNATE_STAGE3=1
```

It is tested only with an isolated `ROCKBOX_DIR="/.rbtv"` Rolo application and
its matching ABI-6 dual-boot bootloader. It must never be packaged with
`mks5lboot --single`. The normal personal Rockbox image remains the disk boot
target after the round trip. A successful wake therefore cold-loads the normal
personal firmware; it does not automatically reload the Rolo test image. Rolo
is launched manually afterward only to inspect the retained result.

After one armed wake, Rolo the matching Stage 3A application again and open
`Debug > Test retained context`. Success requires that the iPod returned on
its own after the selected wake source; a result obtained only after a forced
reset is not a Standby/wake pass. A valid automatic result must show:

- `State:PASSED`, `mode:3`, `attempts:1`, `phase:12`, and `failure:0`;
- saved PC inside the displayed retained payload range and saved SP inside
  the 8 KiB Rockbox system-stack range;
- CPSR low byte `df` (system mode with IRQ and FIQ disabled);
- identical, nonzero saved and observed TTB CRC values;
- expected cookie, observed cookie, and return value all `48365033`; and
- observed resumed SP inside the same system-stack range.

Any mismatch is a failed Stage 3A gate. Record the complete diagnostic screen
before another attempt. Passing Stage 3A authorizes work on the hardware
resume coordinator; it does not by itself authorize exposing hibernate as a
normal setting.

If no visible boot follows a wake attempt, perform one forced reset and launch
the same Rolo image once. `failure:16` proves that the CPU was still executing
250 ms after `OOCSHDWN`; no retained context was executed on that recovery
boot. Otherwise interpret the `Trail` field as follows:

| Trail text | Raw value | Last proven boundary |
| --- | --- | --- |
| `none` | `00000000` | No Stage 3 context-call boundary was preserved. |
| `boot call` | `48334231` | Bootloader validated the image and was about to enter the application continuation. |
| `app entered` | `48334132` | Retained application continuation began on the restored Rockbox stack. |
| `app returning` | `48334133` | Continuation validated its inputs and dispatched to the IRAM1 return stub. |
| `return stub` | `48334234` | IRAM1 return stub was reached before restoring the bootloader frame. |
| `boot returned` | `48334235` | Control returned to the bootloader C caller. |

The displayed 24-bit `wake` value packs PMU `OOCSTAT` in bits 0-7, `INT1` in
bits 8-15, and `INT2` in bits 16-23. Menu is a click-wheel event and is not yet
proven to map directly to the PMU ONKEY input; cable, adapter, and EXTON wake
sources must therefore be tested independently instead of assuming one button
result describes all wake inputs.

### Stage 3B: Kernel resume with hardware stopped

- add the setjmp-like CPU context trampoline;
- enter hibernate from a dedicated system coordinator;
- restore only clocks, IRQ infrastructure, input, LCD, and storage;
- require no active plugin, playback, recording, USB session, or composite
  output;
- return to the previous Rockbox screen.

This is the first true instant-resume milestone.

### Stage 4: User-session resume

- add audio logical-state save/restart;
- add codec/PCM resume hooks;
- add dock and composite restoration;
- add USB/cable wake handling;
- add opted-in plugin checkpoint hooks.

### Stage 5: RTC alarm and policy

After manual wake is reliable:

- enable and test the existing RTC alarm registers;
- add `Sleep`, `Hibernate`, and optional `Sleep then Hibernate` policy;
- keep hibernate default-off until the full validation matrix passes.

## Failure Handling and Invariants

These rules are non-negotiable:

1. RetailOS hibernation without a valid Rockbox token always follows today's
   ONB path.
2. A Rockbox-owned snapshot is never passed to ONB.
3. A build fingerprint or resume ABI mismatch causes a cold Rockbox boot.
4. A failed attempt cannot retry forever. The PMU token records the resuming
   state and the bootloader falls back on the next attempt.
5. No NOR writes are part of normal hibernate or resume.
6. No filesystem write is required during wake.
7. Storage must be flushed before the PMU token becomes armed.
8. Critical battery, recording, active USB mass storage, or a failed hardware
   quiesce vetoes hibernate.
9. Holding the established boot override must still permit a cold boot.
10. The feature remains compile-time or hidden-debug gated through Stages 1-3.

## Validation Matrix

### Retention and repetition

- wake after 10 seconds, 1 minute, 10 minutes, 1 hour, and overnight
- 100 consecutive sleep/wake cycles
- low, medium, and full battery
- warm and cold ambient conditions available during normal testing
- checksum the IRAM shadow and multiple SDRAM regions on every probe

### Wake sources

- ONKEY/menu button
- USB insertion
- FireWire/adapter insertion through the DCP750 dock
- accessory/EXTON transition
- RTC alarm

### Runtime states

- main menu
- file browser and database browser
- paused audio
- playing audio once Stage 4 exists
- DCP750 docked with composite disabled and enabled
- stale PMU token on an ordinary cold boot
- incompatible Rockbox build after hibernating

### Injected failures

- corrupt PMU CRC
- corrupt SDRAM record CRC
- corrupt build fingerprint
- corrupt IRAM shadow checksum
- reset while token state is `resuming`
- force a storage-flush failure
- force a driver resume-hook failure

Every injected failure must end in either a normal Rockbox cold boot or an
explicit diagnostic screen. None may launch ONB on a Rockbox-owned image or
loop indefinitely.

## Expected Code Areas

Likely implementation points:

- `bootloader/ipod-s5l87xx.c`
  - early token decision and resume gate
- `firmware/export/pcf5063x.h`
  - existing PMU memory-register definitions
- `firmware/target/arm/s5l8702/ipod6g/pmu-6g.c`
  - token helpers, GPIO3 ownership state, wake reason
- `firmware/target/arm/s5l8702/system-s5l8702.c`
  - explicit MIU enter/exit and hardware-only resume primitives
- `firmware/target/arm/s5l8702/app.lds`
  - fixed retained control block and IRAM-shadow reservation
- new target-specific C/assembly files
  - IRAM final-entry stub and retained CPU-context trampoline
- `firmware/powermgmt.c` and target driver hooks
  - coordinated quiesce/resume phases
- settings/UI only after the hardware path is proven

## Recommendation

Run one isolated Stage 3A-R6 USB-wake attempt against the already-installed
ABI-7 dual-boot bootloader while retaining Stages 1 and 2 as regression and
fault-injection gates. R6 changes only the app-side preflight transport; do not
install another bootloader. Arm on battery, wait for Standby, and insert USB
exactly once. Record `ENT`, PMU, IRQ, PWR, and every context field whether the
return is automatic or requires one force reset. Only a real automatic wake
may authorize the minimal Stage 3B clocks/IRQ/input/LCD/storage resume
coordinator. Do not jump directly to transparent audio or arbitrary plugin
resume.

The decompile and current Rockbox code answer the major feasibility question:
the device was designed to retain SDRAM, the bootloader already recognizes that
state, the PMU provides a retained ownership channel, and Rockbox already knows
how to leave MIU self-refresh. The exact entry sequence is now identified and
implemented. One retained-RAM cycle and one controlled retained-payload cycle
pass on real hardware. R2 exposed unconditional result replacement by the next
capability publication. R3 made the result durable and proved the bounded CPU
context round trip after a forced reset, but the device remained black and did
not wake from Menu or USB. R4 distinguished “PMU entered Standby” from
“CPU kept spinning with GPIO3 low,” proved the former, and captured an EXTON2
rising event under the wrong edge mode. R5 failed closed before arming and
proved that its raw-polled live-context write did not alter OOCMODE. R6 uses
the normal serialized PMU transport for that single preflight write while
leaving the proven cache-off entry path unchanged.
Repeat, duration, wake-source, and injected-failure testing remain mandatory
before this can become a normal user setting.
