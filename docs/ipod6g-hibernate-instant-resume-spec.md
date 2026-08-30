# iPod Classic 6G/7G Hibernate and Instant Resume Specification

## Status

This is a research and implementation specification, not a claim that full
resume is already production-safe. Retained RAM, a controlled retained
payload, automatic USB wake, IRAM restoration, and a controlled CPU-context
round trip all pass on the real personal iPod Classic. A matching Stage 3A-R6
application and bootloader reached `PASSED`, mode 3, phase 12 with zero
failures, but the panel stayed black. That result is now explained exactly:
R6 returned to the bootloader, which then ran `bss_init()` and the ordinary
cold-start path. The bootloader BSS range overlaps the retained Rockbox
application BSS, so cold startup destroyed the kernel state that had just been
validated.

Stage 3B-R7 used resume ABI 8 and followed the recovered RetailOS model instead:
the bootloader restores IRAM, MMU context, banked stacks, and the saved system
frame, then branches directly to the suspended application continuation. It
does not return to bootloader startup. The application rebuilds only reset
hardware around retained kernel objects. Its first hardware run proved the
direct handoff, saved stack, MMU/TTB, context cookie, and every hardware hook,
then froze at phase 13 when the complete saved VIC mask and CPU IRQ/FIQ state
were restored while the suspending thread still owned the retained I2C mutex.

Stage 3B-R8 / resume ABI 9 fixed the I2C/VIC ordering but its hardware run still
remained black. The matching recovery image preserved phase 16 (`core IRQ
live`) and raw VIC0 `00000120` before the minimal mask. Bit 8 is `IRQ_TIMER`, so
Timer B had elapsed while CPU IRQ/FIQ were masked; LCD DMA bit 16 was not
pending. Because the phase-16 CRC was committed after CPU IRQ release, the
release returned to the retained thread. The next uninstrumented operation was
the complete `lcd_awake()` path, so R8 did not distinguish its mutex, panel
delays, frame DMA, and activation event.

Stage 3B-R9 / resume ABI 10 tested that scheduler-first theory and disproved
it. The real iPod reached phase 18 (`tick waiting`) with the retained PC, SP,
TTB, and cookie intact, Timer B reset, and its pending edge cleared, then never
returned from the first `sleep(1)`. No LCD operation had begun. This is a
measured scheduler-transition boundary, not a panel-format or DMA failure.

Stage 3B-R10 / resume ABI 11 now follows the recovered RetailOS ordering. The
stock resume entry initializes Timer E with the exact register sequence
`TECON=0x440`, `TEPRE=11`, `TEDATA0=0xffffffff`, `TECMD=3`, restores retained
execution, and uses polling controller paths rather than a scheduler delay for
the low-level display work. R10 keeps CPU IRQ/FIQ and both VICs closed while it
runs the panel wake delays from Timer E, repaints one complete frame with a
polled PL080 transfer, restores the backlight, rearms Timer B, and only then
restores the complete saved VIC and CPU state. PMU and LCD events remain
deferred until after that boundary. This is still experimental: only a
visible, responsive phase-28 result can authorize repetition, long-duration,
storage-resume, or injected-failure qualification.

The first normal-power-off integration test on 2026-08-29 restored the exact
pre-sleep framebuffer but left the UI frozen. This was not a failed retained
context or display repair: the runtime path called `sys_poweroff()`, which
broadcast `SYS_POWEROFF` before `clean_shutdown()` reached the Stage 3 suspend.
Plugins and several worker queues correctly treat that event as terminal, so
the retained image contained a deliberately half-shut-down Rockbox. The first
correction moved the opt-in suspend directly into `sys_poweroff()` before
`sys_shutdown_common()` and `queue_broadcast()`. Hardware then exposed a second
deterministic problem: holding Play invokes `sys_poweroff()` from
`button_tick()`, a tick/interrupt callback. Entering storage, LCD, and I2C
quiesce from that context froze before power-off.

The runtime path now posts one private `SYS_POWEROFF_REQUEST` to the button
queue. `button_get_w_tmo()` consumes it in the normal UI/plugin thread and
calls the retained coordinator there, before any terminal broadcast. A
successful wake clears stale button presses and returns without broadcasting
shutdown; a refused attempt follows the original legacy shutdown unchanged.
Explicit Stage 1/2 diagnostic requests bypass this runtime interception.

The first deferred runtime attempt then restored a responsive UI after a
button wake and froze only when the user began scrolling. The retained
continuation, display, timer, and click-wheel path therefore passed. The first
scroll can demand artwork or menu data and exposed a concrete storage-state
mismatch: this tree's iFlash/SSD sleep path leaves adapter power on and marks
the disk logically inactive, while its ordinary fast wake only ungates the ATA
clock because it assumes controller registers survived. Standby resets that
controller. The hibernate resume hook now marks this as a full-controller wake
so the first access uses the existing proven PATA initialization instead of
the runtime clock-gate fast path. Media wake remains lazy. Persistent
`ATA init enter/ready` and `ATA I/O enter/ready` breadcrumbs identify the exact
boundary if the adapter still fails.

RetailOS also performs an explicit USB transition: recovered code stops the
OTG PHY clock through `PCGCCTL`, changes PHY power/reset state, and its
resume-side device manager restarts the clock and performs complete PHY and
controller setup before normal device work continues. Rockbox now resets only
the USB hardware to its proven cold-boot off state before publishing a cable
edge. It does not recreate retained USB software objects. This hook is still
required for cable wake, but it was not the cause of the measured button-wake
freeze.

Application and bootloader compatibility is no longer tied to the Git-derived
`rbversion`. Both sides validate the explicit stable contract
`ipod6g-hibernate-abi11-record11`. Ordinary application/UI commits can therefore
use the installed compatible bootloader. Changing the retained record layout,
token protocol, direct-resume semantics, fixed memory layout, or bootloader
resume behavior requires a contract/ABI bump and one matching bootloader DFU.

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

The compile-time-gated controlled-context implementation is also present.
R2-R6 established durable diagnostics, genuine Standby entry, the EXTON2
rising-edge wake mode, serialized live-context PMU preflight, automatic USB
wake, strict app/bootloader compatibility validation, IRAM restoration, and a valid
system-stack/context round trip. The matching R6 pair completed that bounded
round trip, but its intentional return-to-bootloader design could never resume
the kernel because subsequent cold initialization cleared overlapping retained
application BSS.

R7 replaces that obsolete return ABI with a non-returning stock-style handoff.
Its setjmp boundary saves r4-r11, system SP/LR, a local continuation PC, CPSR,
CP15 control/TTB/domain, and the IRQ/FIQ/SVC/ABT/UND stack pointers. The
bootloader validates the complete retained image, restores IRAM and MMU state,
then branches directly to the continuation before bootloader `bss_init()`.
The resumed thread rebuilds clocks, GPIO, VIC/EIC, DMA, timer, click wheel,
UART, PMU masks/inputs, power GPIOs, stopped PCM, and the LCD controller without
recreating threads, queues, semaphores, or mutexes. The suspend coordinator
owns I2C bus 0 across the context boundary and masks IRQ/FIQ after acquiring it,
closing the last PMU transaction and scheduler race. The old asynchronous
Stage-3 `sys_poweroff()` request path is fail-closed; only the coordinated
in-thread path can arm controlled-context resume.

R7 hardware evidence showed that this direct path reached `hardware restored`
with matching PC, SP, CPSR, TTB, and context cookie, then stopped at its first
full interrupt release. R8 preserves the handoff but replaces that final
single step with an ordered unlock/core-IRQ/display/full-IRQ sequence and
records both saved VIC enables and raw pending sources around the transition.
Its phase-16 result localized the remaining stop to the scheduler/display
boundary. R9 reset the measured Timer B edge and then stopped at the first
explicit scheduler block. R10 removes every sleep, yield, mutex, and event
from the interrupt-masked display repair and admits the scheduler only after
the panel and interrupt substrate are complete.

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
can write GPIO3CFG and OOCSHDWN and enter Standby.

The first R6 run reported:

```text
State:FAILED mode:3
Attempts:1 phase:5 fail:12
TTB:599fd0c7/599fd0c7
Cookie:48865033/00000000
Trail:none 00000000
PMU:ed58c700 IRQ:00401c80
PWR:27606100
ENT:e927c5c7/e927c5c7
```

This is not a PMU-preflight failure. `Attempts:1`, the wake snapshot, and the
automatic USB boot prove the complete Standby/wake path through MIU restoration.
`fail:12` is the fail-closed context-metadata gate. In addition to ABI 7, that
historical gate required
`record->build_fingerprint[0] == build_version_crc32()`. The R6
application embedded `46e15bc9e9-260829`; the installed R5 bootloader embedded
`f4303c93fc-260829`. The matching R6 dual-boot bootloader is therefore required
even though the record layout and ABI did not change. It must use the exact
same explicit version string and must never be packaged as single boot. The
current ABI-11 implementation supersedes that per-commit check with the stable
resume-contract fingerprint described above; all other record, payload, and
CRC gates remain mandatory.

### Matching Stage 3A-R6 context result and black-screen diagnosis - 2026-08-29

After installing the matching R6 dual-boot bootloader, the same isolated Rolo
application completed the bounded context test and retained a `PASSED`, mode 3,
attempt 1, phase 12, failure 0 result. USB caused a real automatic wake and all
context, stack, TTB, IRAM, cookie, and return checks passed. The screen still
remained black after the wake.

This is a successful Stage 3A gate but not a kernel-resume pass. Linked-map and
control-flow audit identified the deterministic cause:

- the R6 application continuation returned through an IRAM1 stub to
  `ipod6g_hibernate_validate_after_wake()`;
- the bootloader then continued ordinary startup and reached `bss_init()`;
- bootloader BSS occupied `0x08800000..0x088a9440`;
- the retained Rolo application's BSS extended through `0x088b86d8`; and
- zeroing the overlapping bootloader range destroyed retained kernel, driver,
  queue, mutex, and UI state before a normal disk load replaced the image.

The fix is architectural, not another panel timing change. A successful resume
must never return through bootloader startup. R7 branches directly from the
early validator to the retained application continuation before `bss_init()`
and performs hardware-only restoration inside the still-live Rockbox kernel.

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

The analyzed firmware is iPod Classic 2.0.4. Both the decrypted OSOS image and
the actual ONB image were extracted and disassembled. OSOS is not a flat blob:

| OSOS body range | Runtime address | Size |
| --- | --- | --- |
| `+0x000000` | `0x22000000` | `0x00aed8` |
| `+0x00aed8` | `0x08000000` | `0xa0fc88` |
| `+0xa1ab60` | `0x08a0fc88` | `0x000a84` |

Using this segmented mapping resolves the earlier 0x800/header and flat-base
ambiguities. The addresses below are runtime addresses verified against the
actual instruction stream.

### RetailOS suspend coordinator

The high-level call at `0x0807b270` performs a real two-sided transition:

1. `0x0835eb94(2)` tells the power/device manager to enter its pre-sleep state.
2. `0x080d84e4` saves both VICs, all vector-slot state, and the EIC state around
   `0x39a00000`, then masks interrupt sources.
3. The wrapper at `0x220005a0` saves the caller frame and interrupt state.
4. Veneers at `0x22003478..0x22003498` shadow active IRAM, save r0-r12, retain
   SP at `0x08a0fc80`, retain continuation `0x220005e0` at `0x08a0fc84`, and
   enter retained Standby.
5. On continuation, `0x220034a0` restores r0-r12; the wrapper restores its
   controller words and caller frame.
6. `0x080d8e94` restores the retained VIC/EIC state.
7. `0x0835eb94(5)` performs the post-wake device-manager transition.

This proves that stock does not create a second OS. It resumes the suspended
instruction inside the retained OS, then restores interrupt and peripheral
managers around that continuation.

### Retained descriptor and ONB dispatch

Before poweroff, OSOS copies 96 KiB from `0x22000000` to `0x08a25940` and
publishes this descriptor at retained address `0x08000000`:

| Offset | Value |
| --- | --- |
| `+0x00` | `0x68696265` (`hibe`) |
| `+0x04` | `1` |
| `+0x08` | `1` |
| `+0x0c` | dynamic retained value |
| `+0x10` | resume entry `0x080d24d0` |

The extracted ONB dispatcher at runtime `0x220105cc` checks `hibe` and version
1, switches to the retained firmware's expected SVC mode through
`0x220106dc`, loads descriptor field `+0x10`, and calls it directly with
`blx`. A successful resume does not return to ONB cold-start logic.

### OSOS resume entry

The saved entry `0x080d24d0` reconstructs exactly the execution substrate that
was lost while keeping all retained OS objects intact:

1. set SP from `0x08a0fc80`;
2. initialize timer E through `0x083600b8`;
3. select MIU resume mode through `0x082b1b60(2)`;
4. restore IRAM and consume the descriptor through `0x080aa328`;
5. restore UND/ABT/IRQ/FIQ stacks through `0x0802d0e8 -> 0x22004620`;
6. rebuild cache/MMU state through `0x0802d0f0 -> 0x22004534`;
7. repair a retained memory/object region through
   `0x0807db74 -> 0x0808f8c0`; and
8. load saved SP and continuation, then tail-call `0x0802ddb0`, whose complete
   handoff is `mov sp, r0; bx r1`.

The continuation is therefore a direct longjmp-style transfer. There is no
bootloader return frame, no BSS clearing, and no second kernel initialization.

### PMU and MIU entry details

The low-level PCF writer uses device address `0x73`. Its retained-standby mode
drives `GPIO3CFG = 0`, enters MIU self-refresh, waits, and writes
`OOCSHDWN = 2`; ordinary shutdown writes `OOCSHDWN = 1`. The public PCF50633
manual does not document the PCF50635-specific meaning of bit 1, but the Apple
instruction stream and successful Rockbox retention tests independently prove
the distinction.

Immediately before Standby, stock disables I-cache and D-cache while leaving
the MMU enabled, then performs:

```
MIUCON = (MIUCON & ~0x0f000000) | 0x0a100000;
MIU_REG(0x14) = 1;
```

Rockbox's final IRAM entry reproduces those directly observed operations.

### Consequence for Rockbox

The Rockbox resume gate must run before `launch_onb(1)` and only for a valid
Rockbox ownership token. After restoring retained memory, it must branch
directly to the saved Rockbox continuation before bootloader `bss_init()` or
any ordinary application load. Every hardware resume hook must repair reset
registers around retained software state; it must not call normal one-time
initializers that recreate mutexes, queues, threads, or driver ownership.

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
- explicit resume-contract fingerprint
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

R2 through R6 were deliberately bounded return-to-bootloader tests. Together
they prove the context, stack, MMU, retained-memory, and automatic-wake gates.
The matching R6 pair passed phase 12, but its return to ordinary bootloader
startup also proved why that design cannot resume a kernel: bootloader BSS
initialization overlaps and clears retained application BSS.

Those revisions remain useful historical fault-isolation gates; they are not
the implementation model for further work.

Stage 3A must be enabled in both images with:

```text
-DIPOD6G_HIBERNATE_STAGE1=1
-DIPOD6G_HIBERNATE_STAGE2=1
-DIPOD6G_HIBERNATE_STAGE3=1
```

It is tested only with an isolated `ROCKBOX_DIR="/.rbtv"` Rolo application and
its exact-version experimental dual-boot bootloader. It must never be packaged
with `mks5lboot --single` or copied over the normal personal Rockbox image.

If no visible boot follows a wake attempt, perform one forced reset and launch
the same Rolo image once. `failure:16` proves that the CPU was still executing
250 ms after `OOCSHDWN`; no retained context was executed on that recovery
boot. Otherwise interpret the `Trail` field as follows:

| Trail text | Raw value | Last proven boundary |
| --- | --- | --- |
| `none` | `00000000` | No Stage 3 context-call boundary was preserved. |
| `boot direct` | `48334231` | Bootloader validated the image and is about to branch directly to Rockbox. |
| `app continue` | `48334132` | The retained Rockbox setjmp continuation is executing. |
| `hardware restored` | `48334133` | Reset hardware and interrupt infrastructure were rebuilt. |
| `I2C released` | `48334134` | The retained I2C mutex was released while CPU IRQ/FIQ remained masked. |
| `tick rearmed` | `48334135` | Timer B was reset with the normal target configuration under CPU IRQ/FIQ mask. |
| `tick IRQ armed` | `48334136` | Only the Timer B VIC source is enabled. |
| `tick IRQ live` | `48334137` | CPU IRQ/FIQ release returned to the retained thread. |
| `tick waiting` | `48334138` | The retained thread is about to execute a real one-tick sleep. |
| `tick proved` | `48334139` | The scheduler blocked and woke the retained thread. |
| `DMA IRQ armed` | `4833413a` | LCD DMA is enabled only after the scheduler proof. |
| `LCD enter` | `4833413b` | The original panel-wake sequence is about to run. |
| `LCD mutex` | `4833413c` | The retained LCD mutex was acquired. |
| `LCD clocks` | `4833413d` | LCD clocks were enabled. |
| `LCD command` | `4833413e` | Command mode was entered. |
| `LCD sequence` | `4833413f` | The panel wake command/delay sequence returned. |
| `LCD frame` | `48334140` | The full framebuffer update was queued. |
| `LCD DMA done` | `48334141` | The full-frame DMA wait returned. |
| `LCD event done` | `48334142` | LCD mutex release and activation event both returned. |
| `display ready` | `48334143` | Backlight restoration returned. |
| `all IRQ armed` | `48334144` | The complete saved VIC mask was written. |
| `resume complete` | `48334145` | Full IRQ release and deferred USB publication returned. |

The displayed 24-bit `wake` value packs PMU `OOCSTAT` in bits 0-7, `INT1` in
bits 8-15, and `INT2` in bits 16-23. Menu is a click-wheel event and is not yet
proven to map directly to the PMU ONKEY input; cable, adapter, and EXTON wake
sources must therefore be tested independently instead of assuming one button
result describes all wake inputs.

### Stage 3B-R7 hardware result — 2026-08-29

The first ABI-8 direct-resume cycle woke by USB but remained black. After one
forced reset, the exact matching Rolo build preserved:

- `State:FAILED`, mode 3, attempt 1, phase 13, failure 15;
- saved PC `080001c8`, SP `0000abb8`, and CPSR `600000df`;
- matching TTB CRC `83e99591/83e99591`;
- matching context cookie and return `48365033/48365033`;
- `Trail:hardware restored` (`48334133`); and
- matching direct-entry guard `e927c5c7/e927c5c7`.

Failure 15 was assigned by the recovery boot; it does not mean a hardware hook
failed. Phase 13 and `H3A3` prove every hook returned. Linked control flow then
showed the next operation was full saved-VIC restoration followed by CPU
IRQ/FIQ release, with `i2c_bus_unlock(0)` still later in the instruction stream.
That is the measured R8 fix boundary.

### Stage 3B-R8 hardware result — 2026-08-29

R8 woke automatically by USB but again remained black. One forced reset and
the exact matching ABI-9 Rolo image preserved:

- `State:FAILED`, mode 3, attempt 1, phase 16, failure 15;
- saved PC `080001c8`, SP `0000ab98`, and CPSR `600000df`;
- matching TTB CRC `88ea9591/88ea9591`;
- matching context cookie and return `48365033/48365033`;
- `Trail:core IRQ live` (`48334136`);
- saved VIC enable masks `27616100/00001000`;
- raw masks before the core mask `00000120/00000000`; and
- no `RAW1` capture, proving display wake never returned.

VIC0 bit 8 is `IRQ_TIMER`; bit 16, `IRQ_DMAC0`, was clear. The unrelated raw
bit 5 was not enabled by R8's minimal mask. Phase 16 was CRC-committed after
`restore_interrupt(oldlevel)` returned, so this result does not prove an IRQ
handler itself froze. Linked control flow shows the next call was
`stage3_finish_display()` and therefore `lcd_awake()`. R9 separates the timer
and every LCD sub-operation instead of changing another unmeasured display
parameter.

### Stage 3B-R9 hardware result — 2026-08-29

R9 woke automatically by USB and remained black. The matching ABI-10 recovery
image preserved:

- `State:FAILED`, mode 3, attempt 1, phase 18, failure 15;
- saved PC `080001c8`, SP `0000ab80`, and CPSR `600000df`;
- matching TTB CRC `9df7e812/9df7e812`;
- matching context cookie and return `48365033/48365033`;
- `Trail:tick waiting` (`48334138`);
- saved VIC enable masks `27610100/00001000`;
- raw VIC0 `00000120` before repair and `00000018` after Timer B reset;
- Timer B control/count `00111240/00000002` before reset and
  `00011240/00000000` after reset.

Phase 18 is committed immediately before `sleep(1)`, and phase 19 is committed
only after it returns. Therefore the first scheduler block/context switch did
not return to the retained suspending thread. The panel wake sequence, frame
copy, and LCD DMA were never entered. R10 does not tune another timer or panel
parameter around this failure. It removes the disproven scheduler dependency
from the low-level repair boundary.

The stock comparison is concrete. RetailOS coordinator `0x0807b270` runs its
pre-sleep manager, saves/masks interrupt state, enters the IRAM hibernate
wrapper, restores full interrupt state, and invokes its post-wake manager. Its
retained resume entry at `0x080d24d0` calls `0x083600b8` before restoring MIU,
IRAM, MMU, and the saved continuation. That timer routine consists of exactly
four Timer E writes: `0x440`, `11`, `0xffffffff`, and `3`. Its low-level LCD
wait at `0x080c31f0` polls controller status bit `0x10`; it does not require a
kernel sleep to make the controller usable. The post-wake manager also reaches
`0x083602dc`, which calls LCD controller initializer `0x080c9c00`; that routine
writes `LCD_CON=0x80000000`, then `0x80100db1`, offset `0x88=0x01000000`,
`LCD_PHTIME=0x33`, and offset `0x7c=0x804`. R10 reproduces those measured
controller writes and polling properties without claiming that Rockbox and
RetailOS share higher-level scheduler or driver objects.

### Stage 3B: Kernel resume with hardware stopped

Stage 3B-R10 / resume ABI 11 is the current true instant-resume implementation:

1. Veto active core or direct PCM audio, USB, and composite output.
2. For normal power-off, queue one private request from `sys_poweroff()` and
   consume it from the normal button/UI thread. Attempt retained suspend there
   before broadcasting the terminal `SYS_POWEROFF` event. A veto or preparation
   failure falls through to the unchanged legacy shutdown path.
3. Flush and sleep storage; quiesce UART, LCD DMA, panel, and backlight.
4. Arm the exact PMU wake configuration.
5. Acquire I2C bus 0 in the suspending thread, then mask IRQ/FIQ so no new
   background transaction can cross the context boundary.
6. Save r4-r11, system SP/LR, continuation PC, CPSR, CP15 state, and every
   banked exception stack; re-shadow all used IRAM0 and enter Standby.
7. In the early bootloader validator, verify token, resume contract, record,
   TTB, payload, probe, and IRAM CRCs; restore IRAM and branch directly to the
   saved continuation before `bss_init()`.
8. Rebuild clocks, GPIO, VIC/EIC, DMA, timer, click wheel, UART, PMU, power,
   stopped PCM, LCD-controller registers, and the USB PHY/controller's
   hardware-only cold-off state without recreating retained software objects.
9. Return with all VIC sources masked, release retained I2C ownership, and
   commit phase 14 while CPU IRQ/FIQ remain masked.
10. Verify the stock-style Timer E substrate and run the panel wake sequence
   with polling delays while IRQ/FIQ and both VICs remain closed.
11. Copy the complete retained framebuffer, queue one normal PL080 frame, poll
    its raw terminal/error state, service the completed DMA task synchronously,
    and restore the backlight without entering the scheduler.
12. Reset Timer B, capture raw pending sources, clear stale 32-bit timer
    status, and restore the complete saved VIC mask before releasing CPU
    IRQ/FIQ.
13. Publish the deferred USB edge and LCD activation event only after that
    release, then commit phase 28 PASS.

The linked-binary gate must prove both images were built with all three Stage
defines. Incremental objects compiled without those defines are invalid even
if a stale assembly object still contains the trampoline.

For the first hardware attempt, launch the matching R10 image with Rolo, verify
`Stage 3B-R10 / ABI 11 / /.rbtv`, open `Debug > Test retained context`, and press
Select once while undocked and on battery. After the screen goes black, insert
USB once. Success means the same Rolo screen becomes visible without any boot
logo or disk image load and reports:

- `State:PASSED`, `mode:3`, `attempts:1`, `phase:28`, `failure:0`;
- `Trail:resume complete` (`48334143`);
- matching nonzero TTB/IRAM/probe evidence; and
- a responsive click wheel and display.

If it remains black, wait ten seconds, force-reset once, Rolo the exact same
R10 image, and record the full diagnostic screen without rearming. The final
breadcrumb, Timer E interval, DMA raw status, and VIC masks identify whether
progress stopped in the panel sequence, frame DMA, full-mask release, or a
deferred event. `app
continue` still isolates a hardware-only resume hook and `boot direct` isolates
the non-returning handoff itself.

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
3. A resume-contract or resume-ABI mismatch causes a cold Rockbox boot.
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
- incompatible resume-contract build after hibernating

### Injected failures

- corrupt PMU CRC
- corrupt SDRAM record CRC
- corrupt resume-contract fingerprint
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

Run exactly one isolated Stage 3B-R10 / ABI-11 USB-wake attempt with a clean
Rolo application and an experimental dual-boot bootloader carrying the same
explicit resume contract.
The binary gate must verify that neither image contains disabled Stage-3 stubs,
that the bootloader direct branch occurs before `bss_init()`, and that the
application owns I2C and masks IRQ/FIQ before its setjmp boundary. Arm on
battery, insert USB once after Standby, and record `ENT`, PMU, IRQ, PWR, every
context field, and the final breadcrumb. Do not run a second armed attempt
before preserving any failure evidence.

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
proved that its raw-polled live-context write did not alter OOCMODE. R6 fixed
that preflight, woke automatically, and passed the complete bounded context
round trip; its black screen was caused by the now-removed return to overlapping
bootloader BSS/cold startup. R7 then proved that direct continuation and every
hardware hook worked, but its single-step interrupt release occurred while I2C
was still owned and stopped at phase 13. R8 released that mutex and proved CPU
IRQ release returned, then stopped at phase 16 before the monolithic LCD wake;
its raw mask measured Timer B already pending and LCD DMA clear. R9 reset that
timer state and proved the first explicit scheduler block never returns. R10
therefore reproduces the stock Timer E and polling-controller properties,
repairs the panel and one full frame with IRQ/FIQ masked, and exposes the
scheduler only after the full saved VIC is restored. Only a visible,
responsive phase-28 R10 result authorizes
repetition testing. Repeat, duration, wake-source, storage, and
injected-failure testing remain mandatory before this can become a normal user
setting.
