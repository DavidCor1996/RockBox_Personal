# iPod Classic 6G/7G Hibernate and Instant Resume Specification

## Status

This is a research and implementation specification, not a claim that full
resume is already safe. The hardware path is plausible and unusually well
supported by the existing bootloader, but retained-RAM resume must remain an
explicit experimental feature until the retention and fault-injection gates in
this document pass on real hardware.

The target is the existing `IPOD_6G` Rockbox target. Rockbox uses that target
for both the 6th- and 7th-generation Classic hardware.

### Implementation status

The first non-destructive implementation slice is now present:

- the application and bootloader linker scripts protect the fixed 64 KiB
  range `0x0bfec000` through `0x0bffbfff`;
- `hibernate-6g.c` implements the retained PCF `MEMBYTE0..7` ownership token,
  ordered publication, CRC-8 validation, the fixed SDRAM control record,
  CRC-32 validation, the 48 KiB IRAM shadow, and a 12 KiB deterministic
  retention pattern;
- the bootloader has an early Rockbox ownership decision and Stage 1
  validation/fallback path;
- the application-side Stage 1 preparation and arming functions are separate,
  so a caller cannot publish ownership merely by preparing a pattern;
- an enabled bootloader publishes a CRC-protected capability record on cold
  Rockbox boots, and the application refuses to prepare or arm without that
  matching resume ABI handshake;
- the bootloader gate is compile-time disabled by default through
  `IPOD6G_HIBERNATE_STAGE1=0`.

No sleep command, user-visible setting, kernel resume jump, or hardware deploy
is part of this slice. Enabling the gate only teaches the bootloader how to
handle a deliberately armed Rockbox probe; it does not make Rockbox enter
retained standby. The next implementation boundary is the IRAM-only MIU/PMU
entry stub after the exact MIU self-refresh entry command is confirmed.

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

The analyzed image is the decrypted iPod Classic 2.0.4 OSOS body, loaded at
virtual address `0x08000800`.

### Confirmed locations

| Body offset | Virtual address | Finding | Confidence |
| --- | --- | --- | --- |
| `0x15175c` | `0x08151f5c` | `CanHibernate` machine-capability key | High |
| `0x265760` | `0x08265f60` | Deep-sleep statistics text | High |
| `0x2667e0` | `0x08266fe0` | `Enter Deep Sleep` usage-event label | High |
| `0x2667f4` | `0x08266ff4` | `Exit Deep Sleep` usage-event label | High |
| `0x2ad1ac` | `0x082ad9ac` | `PCFPowerMgr` class/component name | High |
| `0x000364` | `0x08000b64` | Low-level PCF state writer | High |
| `0x00244c` | `0x08002c4c` | MIU/SDRAM initialization routine | High |
| `0x2bca38` | `0x082bd238` | MIU mode-field helper | High |

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
public PCF50633 manual and must not be guessed. Rockbox only needs the proven
Standby request (`0x0c = 1`) for the first retention probe.

### MIU routines

The stock routine at `0x08002c4c` programs the same S5L8702 MIU constants used
by Rockbox, including:

- controller base `0x38100000`
- `MIUSDPARA = 0x1fb621`
- mode registers `0x33` and `0x8040`
- the same bank/timing values now present in `miu_preinit()`

The helper at `0x082bd238` changes the low three bits of `MIUCON`:

| Helper input | Resulting low bits |
| --- | --- |
| 0 or 1 | 0 |
| 2 | 5 |
| 3 | 3 |
| 4 | 1 |

Rockbox uses `MIUCON = 0x11` while leaving self-refresh and then restores the
normal controller configuration whose low bits are 5. This strongly suggests
that the stock helper contains the missing self-refresh transition states, but
the exact entry command still needs a hardware retention probe. The first probe
must test the candidate transition rather than treating this inference as fact.

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

Required final sequence:

1. prepare and checksum the SDRAM control record,
2. copy the used 48 KiB IRAM window to the SDRAM shadow,
3. clean the complete data cache and drain the write buffer,
4. write the PMU ownership token,
5. disable IRQ and FIQ,
6. stop DMA and mask external interrupts,
7. drive PMU GPIO3 low,
8. issue the candidate MIU self-refresh transition from IRAM,
9. issue `OOCSHDWN.GOSTDBY` through an IRAM-safe polling I2C primitive,
10. loop in IRAM if the PMU does not remove power.

The generic PMU driver and normal I2C stack cannot be assumed safe after step
8 because their code, data, locks, or stack may live in SDRAM.

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

### Stage 2: Controlled payload resume

Resume a tiny retained test function on a dedicated DRAM stack. It should:

- verify its context and IRAM shadow,
- draw a bootloader-independent result,
- read the wake reason,
- clear ownership,
- reboot normally.

This proves the complete bootloader-to-retained-code handoff without involving
the scheduler.

### Stage 3: Kernel resume with hardware stopped

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

Proceed, but begin with Stage 1 rather than implementing UI or transparent
audio resume.

The decompile and current Rockbox code answer the major feasibility question:
the device was designed to retain SDRAM, the bootloader already recognizes that
state, the PMU provides a retained ownership channel, and Rockbox already knows
how to leave MIU self-refresh. The largest remaining unknown is the exact,
repeatable MIU entry transition and the complete list of peripherals that need
hardware-only resume hooks. Both can be answered safely by the staged probes
above.
