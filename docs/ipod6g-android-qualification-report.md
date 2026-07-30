# iPod 6G Android qualification report

Date: 2026-07-28

Scope: host builds, sparse regular-file installer tests, ARM user-mode tests,
64 MiB ARM926 full-system and instruction emulation, static N25 artifact
qualification, and the results of the volatile direct-boot attempts to date

Physical hardware actions to date: historical create-only payload staging under
`.rockbox/android` and volatile DFU execution. Earlier attempts stopped with
the Rockbox legend displayed at `3 BANDS = PID 1`; `diagnostic-trace1` changed
from that text to a full white screen. No Rockbox
firmware, database, partition-table, boot-sector, or NOR write was performed.
TRACE2 retained the `3 BANDS = PID 1` legend. A later direct U-Boot transfer
failed at wInd3x block 129 with status 3 before payload execution and left the
device in Apple Boot ROM DFU.

On 2026-07-28 the exact forced-Android transient loader
`834a80e3833e367b98e1b513744d45240c309f4892252d60943b4fc3805fc443`
completed its DFU upload and visibly reached `Verified; starting Android`.
Linux then reset repeatedly and ultimately returned to a black Boot ROM DFU
screen. This is a failed post-handoff hardware result; it does not qualify
Linux, Android, or the persistent Select+Right installer. No NOR command was
run. The ordinary transient Rockbox loader recovered the device to `05ac:1261`.
Both Rockbox firmware copies, every database/tagcache file, all staged Android
payloads, and all historical diagnostics retained their pre-test checksums.
Normal Rockbox recovery rotated its resume-state files and refreshed nine cache
timestamps, so the report does not claim byte-for-byte immutability for those
ordinary runtime files.

The subsequent storage-free Stage-A packet
`ebff6e3f0c7cd3c7293d3c09ea081801fde81246493d9c4b5ff289ce71b521b1`
also completed its Boot ROM upload but did not enumerate U-Boot's expected
`05ac:8007` endpoint; the stale `05ac:1223` endpoint became unresponsive.
No Linux FIT was sent. A physical reset recovered `05ac:1261`. After that
abnormal reset the host repeatedly remounted FAT read-only. Diagnostic staging
stopped without creating files, and no filesystem repair was attempted.

## Decision

The official AOSP `android-2.0_r1` userspace passes its host-emulation gates.
The N25 Linux boot is **not hardware-qualified**. The current Select+Right
candidate failed its first volatile direct-boot test after the visible
Rockbox-to-Linux handoff and is disqualified for installation. The timer-only,
`diagnostic-vic1`, `diagnostic-lcd1`, and `diagnostic-trace1` packets are
disqualified. TRACE1's full-white transition proves that its result differs
from the earlier frozen-text result, but its model did not represent the panel
GRAM cursor and its successive complete frames had no dwell time. The physical
white screen therefore does not map reliably to one intended color.

Review of the pinned freemyipod tree found that its S5L8702 timer driver was a
single recent addition with no Classic hardware-validation evidence. Its
channel comments do not match the S5L8702 register map. ARM926 emulation of the
exact raw kernel and DTB now reaches `stext`, processor selection, MMU enable,
`start_kernel`, and `s5l8702_timer_init`; progress beyond early calibration
requires working physical timer interrupts. The replacement N25 path uses the
Rockbox-proven Timer B/IRQ 8 10 kHz clock event, Timer E 1 MHz clock source,
and the primary timer gate. It also refuses registration if Timer E does not
advance. The Rockbox handoff now masks IRQ and FIQ, disables both VICs, clears
software interrupts, and stops/acknowledges the old Timer B tick after storage
has been unmounted and put to sleep.

A rebuilt `diagnostic-trace2` visible-probe packet passes the artifact and
ARM926 CPU gates. A
narrow S5L8702 timer model executes the exact packaged kernel through
`start_kernel` and timer registration, verifies the advancing Timer E reads,
and observes the exact Timer B transaction `CLR, PRE=74, CON=0x1240,
DATA0=100, START`. Static disassembly also verifies the Rockbox-equivalent
Timer B interrupt acknowledgement. Continued ARM926 execution of the failed
packet reached Linux delay calibration and waited for `jiffies`, exposing
interrupt delivery as the next host-side barrier. Rockbox's
S5L8702 handler reads both PL192 VICADDRESS registers before status dispatch
and writes both at interrupt completion; the tested generic Linux VIC path did
neither. The N25-specific VIC path now matches Rockbox's edge setup and
VICADDRESS entry/completion behavior. The earlier host gate stopped at the
entry of `s5l_lcd_probe`, before framebuffer allocation or any pixel write;
that was insufficient and the VIC1 physical result disqualified it.

The replacement gate enters the exact linked Rockbox handoff function and
executes panel preparation, quiescence, cache maintenance, MMU/cache disable,
the Linux trampoline, and the packaged Image. It models the panel command
state and rejects data written before the exact strap-specific full-window and
memory-write sequence. Loader, raw kernel entry, live Timer E, first Timer B
IRQ, and Linux LCD probe each append one 15,360-pixel band to that single GRAM
transaction. The required red/green/yellow/blue/amber result totals exactly
76,800 pixels with no wraparound. The gate also verifies Linux initcalls,
Timer B delivery, both PL192 VICADDRESS handshakes, and final ARM state for all
four panel straps.
The physical TRACE2 result did not show its first red band, so that packet is
disqualified and its `qualification.json` now reports
`device_test_ready: false`. No Rockbox-to-Linux visible packet is approved for
another test.

The 195,136-byte direct U-Boot IMG1 packet is likewise disqualified: it crossed
the observed Boot ROM/wInd3x transfer boundary and was rejected at block 129
before execution. Its replacement is a 101,968-byte IMG1 containing a
99,913-byte compressed stage zero. Full ARM926 execution of the exact packaged
stage zero performs the Rockbox-derived DRAM setup, copies its compressed
payload and relocator, reconstructs the qualified 193,080-byte U-Boot
byte-for-byte, passes its CRC path, and reaches the U-Boot entry after
32,880,326 instructions. The U-Boot configuration gate enforces RAM-only DFU,
environment-nowhere, USB `05ac:8007`, and no ATA/IDE/MMC/MTD/NVMe/SATA/SCSI or
USB-mass-storage support. This clears one volatile Stage-A enumeration test;
it is not hardware qualification and does not authorize a FIT/Linux upload.

In a storage-free QEMU ARM926 machine, the packaged Android initramfs boots the
official Eclair `app_process`, registers the native Android runtime, starts
Zygote, forks `system_server`, enters native `system_init()`, starts
SurfaceFlinger, publishes the SurfaceFlinger Binder service, loads the official
Android PixelFlinger 1.1 software renderer, and reaches a ready Java
`SystemServer`. The stock Eclair package manager invokes `installd` to
DEX-optimise the bundled Rockpod Launcher, resolves it as HOME, and launches
its activity through `onCreate()`. Android init and Zygote remain alive for the
45-second qualification window. Binder protocol 7, DEX 035 parsing, Dalvik,
the framework libraries, and the Zygote command accept loop also pass.

QEMU uses a RAM-only 320x240 RGB565 gralloc fallback because it has no Classic
LCD device. The same qualified Eclair gralloc now tries `/dev/graphics/fb0`
and `/dev/fb0` first, so the Android compositor can use a Linux framebuffer on
N25. A Classic fbdev bridge is compiled into the N25 kernel. It detects all
four known Classic panel strap types, applies the corresponding Rockbox-derived
8- or 16-bit initialisation sequence, transfers RGB565 frames, and performs
only the six PCF50635 writes needed for the LCD rail and LED backlight. This is
static/compile evidence only: no physical pixels have been observed, and the
broader PMU is deliberately not registered. A new evdev driver ports Rockbox's
exact Classic click-wheel setup and packet format, polls only the PCF50635 Hold
GPIO, and supplies buttons and wheel navigation through an Eclair key layout.
Menu+Select requests the Rockbox-derived S5L8702 reset after eight seconds.
Physical display, input, and reset behavior remain unvalidated;
battery/charging, controlled power-off, storage, and audio remain out of scope
for this first test.

Rockpod verifies and reports the qualified bundles. Its image-layout helper
still operates only on regular disk-image files, while a separate narrow
stager manages only the three checksum-wrapped RAM-boot files under
`.rockbox/android`. It replaces an older set only when all three files exactly
match their own canonical manifest, uses staged files and rollback copies for
the update, and rejects symlinks, unexpected files, partial sets, or modified
payloads. Six frozen historical diagnostic directories are explicitly
allowlisted, recursively checksum-snapshotted, and left untouched. It verifies
both installed Rockbox firmware copies, hashes
database/tagcache files before and after, and performs no USB/DFU, partition,
filesystem-resize, Rockbox-firmware, or NOR action. The Eclair report retains
`persistent_storage_available: false` for Linux.

A no-device-write rehearsal copied the connected iPod's actual two Rockbox
firmware files, database files, old three-file Eclair manifest, and all six
historical diagnostic trees into `/tmp`. The stager recognized the old
manifest, transactionally replaced only the three canonical root payloads,
preserved every protected checksum, reported all six historical directories
unchanged, and then passed an idempotent second run as `already_current`.

## What the existing iPod work solves—and what it does not

Rockbox and freemyipod remove much of the undocumented-platform barrier:

- Rockbox identifies N25 as S5L8702/ARM926EJ-S with 64 MiB DRAM at
  `0x08000000`, and supplies working Classic register sequences and board
  behavior for clocks, interrupts, LCD, click wheel, PMU, USB, storage, and
  power.
- freemyipod supplies S5L8702 U-Boot/Linux work, the IMG1/DFU format, and the
  temporary wInd3x recovery chain.
- The Nano 3G N46 upstream targets provide a same-SoC starting point; the
  Classic-specific wiring and memory values come from Rockbox.

This is enough to build a plausible Classic boot chain instead of guessing
registers. It does not make Rockbox drivers into Linux drivers: the new LCD,
input, and reset ports are separate Linux implementations and still require
physical validation. Battery/charger PMU policy, storage, and audio require
additional Linux drivers. The decompiled Apple firmware is useful
corroborating evidence, but it likewise does not supply a tested Linux board
support package.

## Qualified Android 2.0 emulation

`tools/ipod6g_android/emulate_eclair_system.py` boots the exact packaged
initramfs on QEMU `versatilepb` with an ARM926EJ-S CPU and 64 MiB RAM. It
passes no disk argument, uses `-nic none`, and uses a kernel with no block
storage, IP networking, N25 display, N25 input, or sound. `/data`, `/cache`,
and `/metadata` are tmpfs.

The gate requires all of these milestones:

- Android init stays alive for the qualification window;
- servicemanager becomes Binder protocol-7 context manager;
- the official DEX 035 fixture parses and executes in Dalvik;
- Eclair framework Java code executes;
- `app_process` performs native runtime registration;
- Zygote starts, forks `system_server`, and accepts command connections;
- `system_server` enters `system_init()` and Java `SystemServer`;
- SurfaceFlinger publishes its Binder service successfully;
- headless gralloc provides a 320x240 RGB565 RAM buffer; and
- `libGLES_android.so` loads and reports Android PixelFlinger 1.1;
- SettingsProvider is installed and `SystemServer` reaches the ready marker;
- stock Eclair `installd` successfully DEX-optimises the Launcher APK; and
- ActivityManager resolves the Rockpod Launcher as HOME and its activity
  reaches `onCreate()`.

The formal system-emulation evidence records:

| Item | Value |
|---|---|
| AOSP tag | `android-2.0_r1` |
| CPU / memory | ARM926EJ-S / 64 MiB |
| initramfs | 13,842,301 bytes, `2e8134f8b496ea1d8c93523f364664abdf26c8defdccfa67ce72756e543cd10d` |
| QEMU kernel | 1,385,128 bytes, `680ff5e96c4b59ec6297b858e9b498956c9f041d7ef47ffe2e89edd47f3101a2` |
| kernel config | `2db29913641c4613c7ecdb360b23601ff51019ee3e1affc845ce81740c4eb84e` |
| Binder | protocol 7 positive path passed |
| attached storage/network | none / none |

The Binder gate exposed and fixed a cross-translation-unit ABI error. Defining
`BINDER_IPC_32BIT` in `binder.c` alone left `binder_alloc.c` using 64-bit
`binder_size_t` helpers, shifted transaction arguments, and caused object
offset copies to fail with `-EFAULT`. The kernel patch now applies the define
through `drivers/android/Makefile`, so every Binder translation unit uses the
same 32-bit Eclair wire ABI. Tests reject the incomplete `binder.c`-only form.

The modern Linux test kernel requires `CONFIG_COMPAT_32BIT_TIME=y` because
Eclair Bionic uses legacy time32 signal syscalls. Without it, Dalvik can spin
on an uninitialised signal result. The N25 configuration and qualification
gate enforce the same setting.

The gralloc path also contains an Eclair-native-only correction: the stock
fallback can recursively acquire its module mutex while trying a PMEM 2D
allocation. The qualified path first opens a real Linux framebuffer and, only
when none exists, allocates volatile ashmem directly. QEMU explicitly proves
the fallback marker; the N25 artifact separately proves that the Classic fbdev
driver, panel initialisation, narrow PCF50635 display-power path, and
device-tree nodes are linked. Neither proves physical panel output.

## Current software-qualified Select+Right Android packet

The N25 cross-build uses pinned upstream revisions:

- U-Boot `bda1eed7f82bc0c8e7a3d88e19602ad6248a25f9`;
- Linux `a1bf13a446201cdd97e56815cedc6b1da14f1313`; and
- official AOSP tag `android-2.0_r1`.

The installed Rockpod bundle is
`rockpod/bin/ipod6g-android/eclair-native`. Its manifest-qualified binary
identities are:

| Artifact | Size | SHA-256 |
|---|---:|---|
| `n25-eclair-native-initramfs.cpio.gz` | 13,842,301 | `2e8134f8b496ea1d8c93523f364664abdf26c8defdccfa67ce72756e543cd10d` |
| `n25-boot-chord-emulation.json` | 8,690 | `d812e31ffafbc8984744c39e6fb137f545c280d5fc1172f0cff6211b35590453` |
| `n25-select-right-chain-emulation.json` | 3,123 | `1ca4938c16901777d2cb6113d3caa46a116b0acd5e2dcc7d39dd9568c523c2bd` |
| `n25-eclair-native.itb` | 15,531,704 | `e8c8413f78190e32965799a7b910e828f965a46a50691a74a19e5dc9863d8218` |
| `n25-eclair-native-uboot.dfu` | 195,136 | `c2d88f752900734da7ee946bab00ef43cd60502e4f68d81fb1217e2b59ecb00e` |
| `n25-eclair-select-right-bootloader.dfu` | 100,960 | `8c2ea99f44f3b2aa1f59bd5e83aeeea54fcf41268b8209e07583366107146402` |
| `n25-eclair-select-right-bootloader.ipod` | 98,920 | `820940476b35cd96d3e4c2f2c22b145ed8ff158b35501ac3733b487ef48cd694` |
| `n25-eclair-select-right-nor-installer.dfu` | 107,056 | `f527887329ae31de664d60d0e447f50192c13f4e003625e4765d6800640abb85` |
| `n25-eclair-select-right-nor-uninstaller.dfu` | 5,856 | `203e836412b71d3a16b3b57a5bf7ef034a0f5dcae1ca193685b8bfee33868a3a` |
| `n25-eclair-kernel.ipod` | 1,684,232 | `7978b2e6ddff0b65f2170aec03f516852f7ddf0e963bfd6a6f1a4c1ae06299d9` |
| `n25-eclair-initramfs.ipod` | 13,842,312 | `959c8ff59013c98ab6bdd80129e0fe70888079920902e6a8c8ec3e2b03fba372` |
| `n25-eclair-dtb.ipod` | 3,056 | `83285c5c03d37d9cc70e00b9e521c13bfe2152d85fda30c2dea766d990b0dad7` |

The modified Classic bootloader reserves exact Select+Right for Android and
moves bootloader USB to exact Menu+Play. Exact-binary emulation executes all
128 main-button patterns: only `0x09` selects Android, only `0x42` selects USB,
and no superset chord selects either. The linked payload-size contract is
`1,684,224 / 13,842,304 / 3,048` bytes and matches the three wrapped bodies.
It loads zImage at `0x08008000`, the padded initramfs at `0x09c00000`, and DTB
at `0x0ad00000`, unmounts/sleeps storage, applies the ARM Linux
SVC/cache/MMU handoff contract, and jumps.

The exact compressed zImage—not a substituted raw Image—then passes 91,242,967
ARM926 instructions from `n25_android_handoff` through zImage relocation and
decompression, `start_kernel`, platform population, and `s5l_lcd_probe`. The
model observes 2,596 Timer E reads, 616 injected and acknowledged Timer B
interrupts, and 2,464 reads plus 2,464 completions across the two PL192
VICADDRESS registers. The report is checksum-bound to the bootloader, zImage,
DTB, and initramfs.

The preservation-mode NOR installer embeds that exact bootloader body and has
a zero single-boot flag; static parsing rejects an installer that would discard
the original Apple firmware. Packaging is not physical installation, and NOR
remains untouched until the volatile candidate passes its device test. The
5,856-byte dual-boot uninstaller is independently regenerated during every
qualification and accepted only when it matches mks5lboot's exact iPod 6G
uninstaller byte-for-byte.

The FIT is staged at `0x08800000`. The earlier `0x09000000` staging choice was
rejected because a larger Android FIT can overlap its own loaded kernel. The
qualifier proves that the staging window, uncompressed kernel, initramfs, DTB,
and U-Boot reserve do not overlap.

The N25 qualification proves artifact structure and configuration only. It
rejects a kernel unless the N25 framebuffer, childless I2C controller,
click-wheel/Hold input, S5L8702 reset driver, all four Classic panel paths,
bounded PCF50635 display writes, and Android click-wheel key layout are linked
and packaged. A host model exercises direction and wrap-around, Hold lockout,
button mapping, fail-closed PMU errors, and the eight-second reset chord.
QEMU does not model the S5L8702 or N25 board, so it cannot prove that the
Classic will execute this U-Boot, initialise DRAM, enumerate USB, run its
timer/interrupt hardware, retain an initialised LCD across the boot handoff,
or safely handle the physical LCD, wheel IRQ, PMU GPIO read, and reset register.

## Enforced storage firewall

The volatile U-Boot profile has no ATA, IDE, CE-ATA, SATA, SCSI, MMC, MTD,
NAND, SPI-flash, USB-storage, filesystem, partition, network, or persistent
environment configuration. `saveenv` and persistent flash operations are not
available. Its device tree contains no iPod storage node.

The Linux profile disables `CONFIG_BLOCK` and the persistent-storage stacks;
its N25 DTS does not describe the disk. Its only enabled I2C controller is a
childless control bus. The LCD path permits six fixed PCF50635 display writes;
the input path reads only PCF50635 GPIOSTAT for Hold. There is no generic PMU
child, EEPROM, or storage peripheral.
The Android initramfs has no storage tools, storage device nodes, updater, or
filesystem writer. Its mutable Android directories are RAM-backed.

These controls prevent the qualified volatile payload from reaching the
Rockbox volume by design. They do not turn experimental physical execution
into a literal zero-risk operation: an incorrect low-level clock, DRAM, USB,
or power sequence could still hang or reset the device. That is why a spare
device, verified backup, and proven reset/recovery path remain promotion gates.

## Diagnostic-only packet

The smaller diagnostic packet under `rockpod/bin/ipod6g-android/ramdiag` was
rebuilt after the FIT-staging fix. Current identities are:

| Artifact | Size | SHA-256 |
|---|---:|---|
| `n25-ramdiag-initramfs.cpio.gz` | 1,048 | `9cbf1a691b4de51723e3ee081d401ee2f28de8b4b021031fdfa07a0aad5a592d` |
| `n25-ramdiag.itb` | 1,468,204 | `7937d44d1a910bf4c889fe1c98110a26704d142d229ed993ec2f937cacea4352` |
| `n25-ramdiag-uboot.dfu` | 195,136 | `c2d88f752900734da7ee946bab00ef43cd60502e4f68d81fb1217e2b59ecb00e` |

The DFU wrapper is byte-identical to the Android packet's storage-free U-Boot.
The direct 195,136-byte wrapper must not be retried because the physical host
transport rejected block 129 before execution. Stage A now uses only
`rockpod/bin/ipod6g-android/ramdiag-stage0/n25-ramdiag-stage0-uboot.dfu`:

| Artifact | Size | SHA-256 |
|---|---:|---|
| compressed stage-zero DFU | 101,968 | `ebff6e3f0c7cd3c7293d3c09ea081801fde81246493d9c4b5ff289ce71b521b1` |
| stage-zero body | 99,913 | `c5f8479dbbd950d133a0c034aee2276b1716b024792d2d6bb07c9a836f42bb69` |
| reconstructed U-Boot | 193,080 | `a2ba496dd0b7fc3fc2f02e3a5b707da81e7d406f932cf4975a2b2d842afca978` |

The diagnostic kernel now links the S5L8702 restart driver, panics back to the
installed boot after five seconds, and unconditionally resets after a bounded
60-second test window.

## Installer and partition status

The dedicated-partition design remains the preferred eventual persistent
layout. The partition-layout helper is deliberately image-only. Against sparse
regular files it supports 512- and 4096-byte logical sectors, MBR/FAT32/BPB/
FSInfo inspection, deterministic plans, stale-plan rejection, MBR-last
two-partition transactions, recovery metadata, read-back verification,
rollback, and injected faults. Independent Python and Rust checks reject block
and character devices.

It does **not** yet resize a real FAT32 filesystem and it cannot be promoted to
this iPod until all of the following exist:

1. a reviewed filesystem-aware FAT32 shrink implementation;
2. complete pre/post file and metadata verification plus exact recovery;
3. qualification on cloned 4096-byte-sector images and a spare iPod;
4. a working read-only N25 storage driver;
5. a lowest-layer Android-partition LBA firewall; and
6. a default Rockbox boot/chainload design that is recovery-tested.

Rockpod now also contains the create-only direct-boot payload stager described
above. On the connected Classic it created `.rockbox/android`, read back all
three files successfully, preserved matching `rockbox.ipod` copies with SHA-256
`4f524f1a7bcb1047ba478855b966586a7f09503ce543419356fe46acdf8f57d2`,
and proved the database snapshot unchanged. This is not repartitioning and
does not install the modified bootloader. No current code is authorised to
repartition the connected iPod.

## Test evidence

The current Android/Rockpod Python selection passes **63 tests**. It covers the
Rockpod adapter/UI, exact manifests, configuration gates, deterministic
initramfs construction, source locks, system-emulation report requirements,
Launcher packaging and launch requirements, and regular-file-only installer
behavior. The Rust image helper passes 14 unit/fault tests and Clippy with
warnings denied. The release-built helper also completed a packaged-service
4096-byte-sector sparse-image rehearsal: create pre-shrunk fixture, dry-run,
commit layout, verify, rollback, and confirm the layout was absent afterward.
Every protocol event retained `hardware_writes_enabled: false`.

The formal clean Android build also passed its static qualification and ARM
user-mode gates before the full-system test. Two separate N25 output trees
then produced byte-identical U-Boot deployable binaries, kernels, DTBs, FITs,
DFU wrappers, and initramfs archives. The U-Boot debug ELFs differ only in
parallel-build DWARF sections; stripping debug data makes them byte-identical.
The only warnings in the current Python run are 2,583 `pytest_asyncio`
deprecation warnings from the test environment; no Android test failed.

## Remaining physical and persistent promotion gates

1. The N25-specific PL192 VIC implementation, generic-compatible rejection,
   exact Rockbox trampoline, four-panel full-frame model, and binary handoff
   gates are complete, but the production hardware run reset after handoff.
   Persistent installation remains locked.
2. Run the already-qualified storage-free Stage-A U-Boot enumeration test,
   followed only after success by the headless Linux USB-serial heartbeat.
   This isolates the physical timer/VIC/USB substrate without framebuffer,
   I2C, input, Android, or storage code.
3. If the headless heartbeat passes, add a bounded USB-observable probe around
   the first failing production initcalls and fix the physical-only fault
   before rebuilding Eclair.
4. Only after a successful corrected volatile boot, physically
   qualify panel output, button/wheel mapping, Hold lockout, and both reset
   paths. Repeat on other panel revisions only with appropriate spares.
5. Add battery/charger/thermal telemetry and controlled power-off before any
   prolonged or untethered runtime test.
6. Only after volatile hardware qualification, develop a read-only N25 storage
   driver.
7. Add the dedicated-range LBA firewall and qualify the partition installer on
   clones and a spare device before touching this iPod's partition table.

TRACE2 has been executed, retained the Rockbox text, and is disqualified. No
Rockbox-to-Linux packet is approved for another physical test. Continue only
with the independent storage-free U-Boot Stage-A enumeration gate; do not send
its Linux FIT until U-Boot appears as `05ac:8007` and recovery is verified.
Eclair, Froyo, partition
installation, and persistent bootloader work remain blocked behind a proven
storage-free Linux substrate and successful Rockbox integrity checks.
