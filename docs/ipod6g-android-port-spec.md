# Android 2.0 on iPod Classic 6G: feasibility and data-safe port specification

Status: Eclair userspace host-qualified; new N25 LCD1 RAM-only probe passes the
exact-handoff and four-panel full-frame gates; hardware unqualified

Date: 2026-07-21

Target: Rockbox `ipod6g`, Apple iPod Classic 6G/6.5G/7G family using S5L8702

Safety posture: storage-less bring-up first; dedicated Android partition only
through a verified, recoverable installer

## 1. Executive decision

A native Android 2.0 (`android-2.0_r1`, Eclair) research port is technically
plausible, but it is a substantial Linux board-port project rather than a
Rockbox plugin or a conventional Android ROM installation.

The only acceptable first implementation under the requirement that existing
Rockbox data must not be put at risk is still:

1. enter the S5L8702 Boot ROM's temporary USB recovery path;
2. upload a volatile U-Boot build with wInd3x;
3. upload a Linux kernel, device tree, and initramfs/FIT image into RAM;
4. run Android from RAM, or keep `/system` on a read-only network root and
   `/data` and `/cache` in RAM or on a host NFS export; and
5. compile all local-storage controllers and persistent-environment features
   out of both U-Boot and Linux.

That design deliberately has no code path capable of reading or writing the
iPod's Rockbox volume. Removing USB power or resetting the iPod discards the
Android session and returns to the installed Apple/Rockbox boot flow.

After that route proves the loader, Linux drivers, Android memory budget, and
recovery path, the preferred end state is persistent Android in one dedicated
partition. The existing Rockbox FAT32 volume remains the default boot and is
not mounted by Android. A small Rockbox-loadable chainloader starts Android,
and a host-side installer performs a filesystem-aware FAT32 shrink, creates the
new partition, installs redundant boot/system slots, and verifies every
pre-existing Rockbox file before reporting success.

Creating a partition necessarily changes the FAT32 filesystem geometry and the
MBR, even when no Rockbox file content is changed. It therefore cannot meet a
literal “do not touch any Rockbox-volume metadata” rule. It can meet the more
useful requirement that all Rockbox files, firmware copies, settings, music,
and database/tagcache files remain byte-identical and recoverable. If that is
not acceptable, only the volatile boot profile is in scope.

The alternatives have these dispositions:

| Proposal | Feasibility | Data-safety disposition |
|---|---|---|
| Run an Android ARM emulator inside Rockbox | Not credible | Reject |
| Run QEMU and an Android virtual device under Linux on the iPod | Not credible | Reject |
| Native Eclair userspace on a Linux S5L8702 port | Plausible research project | Proceed only with volatile, storage-less stages |
| Persistent Android in a file on the Rockbox FAT volume | Technically conceivable after drivers exist | Reject; Android writes share the data filesystem |
| Dedicated Android container partition | Preferred persistent design after volatile gates | Conditional on verified shrink, backup, range firewall, and installer |
| Reformat/repartition and restore all Rockbox files | Feasible but destructive during installation | Recovery-only fallback, not the default installer |
| Permanent boot-menu or NOR modification | Possible only after considerable port work | Unnecessary for the first persistent design |

“No risk” cannot be promised for experimental code executing on physical
hardware. Firmware defects, power faults, and recovery mistakes remain possible.
The enforceable claims are narrower: Profile V contains no reachable local-
storage path, while Profile P can address only its container. Testing should
preferably use a spare iPod and must begin only after an independently
restorable backup.

## 2. What “Android 1.0 or 2.0” means here

### 2.1 Selected baseline

Use the official AOSP tag `android-2.0_r1` as the reproducible target. Its build
configuration defaults to ARMv5TE, matching the ARM926EJ-S instruction set in
the S5L8702. It also predates the assumption that a useful Android device has a
GPU, so its generic framebuffer/gralloc path is a better starting point than a
modern Android release.

Android 1.0 is not a sound primary target: the public AOSP manifest does not
provide an official `android-1.0_r*` release tag. Calling an arbitrary old
snapshot “Android 1.0” would make the build difficult to reproduce. If Eclair
cannot be reduced enough for 64 MiB, the next controlled experiment should be
the official `android-1.6_r1` tag, not an unversioned 1.0 source archive.

The port would not be Android-compatible in the certification sense. Android's
1.6 Compatibility Definition requires a touchscreen, while the iPod Classic
has only a click wheel and buttons. The honest goal is “AOSP-derived Eclair
userspace running on an iPod,” not an Android-compatible product.

### 2.2 Minimum useful product

The first Android product must contain only:

- init, ueventd-equivalent Eclair device setup, servicemanager, Zygote,
  system_server, SurfaceFlinger, and the software framebuffer stack;
- a minimal launcher and minimal settings/control activity;
- ADB over USB networking for diagnostics and text input; and
- one tiny native or Java test application.

Initially omit Browser/WebKit, telephony and RIL, Phone, MMS, Wi-Fi, Bluetooth,
camera, GPS, media scanning, live wallpapers, bundled media, and large codecs.
The iPod has no radio hardware to support most of these services. Add packages
only after measuring their resident memory and boot-time effects.

## 3. Known target facts and evidence

### 3.1 Hardware baseline

| Area | iPod Classic 6G fact | Port consequence |
|---|---|---|
| CPU | Samsung S5L8702, ARM926EJ-S, ARMv5TE; up to 216 MHz | AOSP must remain ARMv5TE; no hardware virtualization |
| RAM | 64 MiB mapped from `0x08000000` | Android is memory-constrained; every service is optional until measured |
| internal SRAM | two 128 KiB banks from `0x22000000` | Boot-ROM payloads are small; normal Linux lives in DRAM |
| display | 320 x 240, RGB565 | Generic fbdev/gralloc can be used; one frame is 153,600 bytes |
| controls | capacitive click wheel, Select/Menu/Play/Previous/Next, Hold | Requires Linux evdev and a custom Android key layout/policy |
| USB | DesignWare USB OTG/device controller | Volatile upload, ADB, EEM/ECM, and NFS are possible candidates |
| storage | CE-ATA/ATA disk or flash variants | Absent in Profile V; lowest-layer LBA-bounded in Profile P |
| audio | S5L I2S plus Cirrus CS42L55 | New ALSA SoC CPU/codec/machine support; post-MVP |
| firmware storage | SPI NOR/boot firmware path | Must be absent from experimental U-Boot/Linux builds |

The hardware values above are consistent with the
[freemyipod hardware table](https://freemyipod.org/wiki/Hardware), the
[Classic 1G/6G page](https://freemyipod.org/wiki/Classic_1G), and this tree's
S5L8702 target code.

Important local implementation references are:

- `firmware/export/s5l87xx.h` for registers and memory mapping;
- `firmware/target/arm/s5l8702/crt0.S` and `system-s5l8702.c` for clock,
  interrupt, cache, and low-level initialization;
- `firmware/target/arm/s5l8702/ipod6g/lcd-6g.c` and the common S5L8702 LCD
  driver for the Classic display;
- `firmware/target/arm/ipod/button-clickwheel.c` and the iPod 6G button target
  header for GPIO and wheel behavior;
- the S5L8702 ATA/CE-ATA driver as documentation only during safe stages; and
- `utils/mks5lboot/` for the Boot ROM and DFU memory constraints.

The existing Rockbox bootloader loads `rockbox.ipod` into DRAM at
`0x08000000`, flushes caches, and transfers control. The mks5lboot DFU path uses
IRAM at `0x22000000`. Rockbox maps two contiguous 128 KiB IRAM banks; the
volatile target enforces a 240 KiB payload ceiling and reserves the top 16 KiB
for U-Boot's early stack/global data. The proposed route avoids squeezing a
Linux loader into that payload: wInd3x first loads a volatile U-Boot, and
U-Boot accepts a larger FIT image into DRAM.

### 3.2 Observed storage geometry of this iPod

A read-only host inventory on 2026-07-18 identified the connected device as an
Apple iPod Classic USB device with these properties:

| Property | Observed value |
|---|---|
| exposed capacity | 477.3 GiB, consistent with a nominal 512 GB flash conversion |
| logical sector | 4096 bytes |
| physical sector | 4096 bytes |
| partition table | DOS/MBR |
| visible partitions | one FAT32 primary partition, type `0x0b` |
| Rockbox volume label | `DAVID'S IPO` |
| mount state during inventory | mounted read/write by the host |

The device disconnected before a second read-only sector-boundary capture, so
exact start/end LBAs are deliberately not guessed here. The installer must
capture them again while the volume is unmounted and bind its plan to the
device serial, total byte size, 4096-byte logical sector size, MBR hash, and
FAT32 volume ID.

This geometry invalidates two common assumptions. It is not the traditional
512-byte-sector, firmware-partition-plus-data layout expected by old
`ipodpatcher` code, and it does not presently expose unallocated space. The
repository's `utils/ipodpatcher` writer explicitly supports only 512-byte
sectors and normalizes the first two partition types, so it must **not** be
used as the Android partition editor. The installer needs a new 4-KiB-safe MBR
implementation and disk-image tests. Existing Rockbox `firmware/common/disk.c`
does parse four primary MBR entries and the target storage stack already
reports variable logical sector sizes, but an additional `0x83` entry still needs
Rockbox and Apple-firmware compatibility tests before physical installation.

### 3.3 Current upstream starting point

Research was pinned to these upstream snapshots so later work can reproduce
the assessment:

| Component | Branch/tag | Researched commit | What exists |
|---|---|---|---|
| freemyipod Linux | `s5l87xx` | `a1bf13a446201cdd97e56815cedc6b1da14f1313` | Linux 6.14 S5L87xx and Nano 3G/S5L8702 support |
| freemyipod U-Boot | `s5l87xx` | `bda1eed7f82bc0c8e7a3d88e19602ad6248a25f9` | S5L87xx boards and volatile DFU-to-RAM boot on Nano 3G |
| freemyipod wInd3x | `main` | `e97a90843a6277d94fe585bca93b835845c3593c` | S5L exploit/temporary firmware-loading workflow |
| AOSP | `android-2.0_r1` | official tag | Reproducible Eclair userspace, generic ARMv5TE product |

The upstream Linux and U-Boot trees support the iPod Nano 3G (N46), which uses
the same S5L8702 SoC. They do **not** currently provide a general iPod Classic
board target. This repository now layers a deliberately restricted N25
RAM-diagnostic target on those pinned revisions; it is not yet an upstream or
full-featured Classic port.
Reusable SoC work includes interrupt controllers, clocks, timer, I2C, SPI,
USB PHY/DWC2, and a Nano-specific framebuffer path. This repository now has a
conservative Classic framebuffer bridge and N25 LCD node with all four
Rockbox-derived panel-init paths and a narrow display-only PCF50635 power
sequence. It also has Rockbox-derived click-wheel/button input, Hold lockout,
the Android key layout, and the exact S5L8702 reset sequence. Their physical
behavior remains untested. Battery/charger/thermal telemetry, controlled
shutdown, storage, audio, and the rest of the PMU remain absent.

The current [freemyipod Linux status page](https://freemyipod.org/wiki/Linux)
explicitly reports that storage and sound are unavailable in its current Linux
work and describes initramfs/NFS use. That incompleteness is useful for the
safe prototype: it reinforces a storage-less first system, though it also means
there is no near-ready Android port.

[wInd3x](https://github.com/freemyipod/wInd3x) can run unsigned code through a
temporary USB recovery chain. The Classic workflow remains experimental and
must be proven with a harmless diagnostic payload before relying on it.

## 4. Why emulation is rejected

Android's historical emulator uses a virtual Goldfish machine. Running that
image does not remove the need for a host operating system, a complete device
model, translated CPU execution, guest RAM, and display/input/USB integration.

The S5L8702 has no ARM virtualization extension. QEMU system emulation would
therefore use Tiny Code Generator dynamic translation and a software memory
management unit. Even an ARM-to-ARM guest does not become a cheap native call:
privileged behavior and virtual devices must still be emulated. See QEMU's
[system emulation overview](https://www.qemu.org/docs/master/system/introduction.html)
and [TCG design documentation](https://www.qemu.org/docs/master/devel/tcg.html).

With only 64 MiB total, there is no defensible memory budget for Linux, QEMU,
the emulator device model, an Android kernel, and the Eclair userspace at once.
At 216 MHz, translated graphics and framework startup would also be unusably
slow. Porting QEMU into Rockbox would add an operating-system compatibility
layer before any Android work could start. It is strictly more work and less
usable than running Android userspace natively on Linux.

QEMU remains valuable on the development host for build smoke tests and memory
experiments. It is not an iPod runtime architecture.

## 5. Safety contract

There are two separately built profiles. Profile V is the volatile bring-up
image and has no local storage code. Profile P is the later persistent image;
it has a dedicated-partition driver but must be incapable of addressing any
LBA outside the installer-recorded Android range. Profile P is not produced or
accepted until Profile V passes through Android framework boot.

### 5.1 Non-negotiable properties

Every artifact allowed onto the physical device before a separately approved
persistent phase must satisfy all of these properties:

1. It is uploaded to volatile memory and is not installed by the Apple or
   Rockbox firmware update mechanisms.
2. U-Boot has no SPI flash, NAND, ATA, IDE, SCSI, MMC, block, filesystem,
   environment-save, flash-write, or raw-memory-to-persistent-device command.
3. U-Boot's environment is compiled-in or RAM-only. `saveenv` is unavailable.
4. The Linux device tree contains no node for the iPod disk, CE-ATA controller,
   ATA controller, NOR, NAND, or other persistent iPod storage.
5. The Linux configuration omits the corresponding controller, MTD, block-disk,
   partition, and filesystem drivers. A disabled device-tree node alone is not
   considered sufficient.
6. The initramfs has no `dd`, `flashcp`, `mtd`, `fdisk`, filesystem repair,
   firmware-update, or general block-writing utility.
7. `/system` is immutable; `/data`, `/cache`, `/metadata`, logs, and temporary
   application state are tmpfs/ramfs or an explicitly configured host export.
8. No swap, zram backing store, loop image, journal, or log is placed on the
   iPod disk.
9. Loss of the host connection or a watchdog reset cannot trigger an Apple or
   Rockbox update operation.
10. The normal installed Apple/Rockbox boot remains the reset default.

These are layered controls. A shell-level `mount -o ro` or
`blockdev --setro` is not an adequate primary barrier because a buggy driver,
privileged process, or recovery script could bypass it.

### 5.2 Build-time storage firewall

CI must produce and archive a machine-readable safety report containing:

- the exact U-Boot and kernel commits;
- complete `.config` files;
- the compiled device tree decompiled back to DTS;
- U-Boot's compiled command list and environment;
- symbol scans proving storage-controller write entry points are absent;
- the initramfs file manifest and hashes; and
- SHA-256 hashes of every uploaded artifact.

The report fails closed if it finds any local storage node, writable flash
command, persistent environment backend, block writer, updater, or unexplained
driver. Review the final binaries, not only source fragments.

Suggested checks include negative configuration assertions for `CONFIG_ATA`,
`CONFIG_MTD`, local disk controller symbols, partition support, filesystem
writers, and U-Boot `CONFIG_CMD_FLASH`, `CONFIG_CMD_MTD`, `CONFIG_CMD_SF`,
`CONFIG_CMD_IDE`, `CONFIG_CMD_MMC`, `CONFIG_CMD_SCSI`, `CONFIG_CMD_PART`,
`CONFIG_CMD_FAT`, and environment-in-flash options. The exact symbol names must
be derived from the pinned tree because Kconfig names can change.

### 5.3 Pre-test preservation gate

Before the first physical test:

- use a spare Classic if one is available;
- confirm exact hardware generation, storage type, battery condition, and USB
  stability;
- make a full raw image and separate file-level backup to a different physical
  disk;
- save the partition table, current bootloader/firmware hashes, SMART data when
  read-only retrieval is supported, and checksums of Rockbox database files;
- prove the backup can be opened and sampled without involving the iPod;
- record a baseline hash set for the entire device, or at minimum all critical
  sectors plus every Rockbox file; and
- place the host's view of the iPod block device in read-only mode before any
  inspection. Do not run a repairing filesystem check.

A backup that has never been read back is not a passed gate. Apple restore is a
last-resort recovery path and normally erases the device; it is not a substitute
for the backup.

### 5.4 Post-test integrity gate

After every Profile V hardware boot, reset back into the existing firmware and
verify:

- the partition table and protected-sector hashes are unchanged;
- the chosen full or sampled raw-sector hashes match the baseline;
- file-level checksums and the Rockbox database/tagcache files match;
- both installed Rockbox firmware copies still have the expected hashes;
- Rockbox boots normally, mounts the volume, opens the database, and plays a
  known track; and
- no unexpected file, dirty-filesystem state, or SMART change attributable to
  the test appears.

Any mismatch is a hard stop. Preserve logs and images; do not “repair and
continue.”

### 5.5 Persistent-profile write boundary

Profile P replaces the total storage omission with an enforceable LBA range.
Do not expose the physical disk as `/dev/sda`, `/dev/hda`, or any other raw
whole-disk block device. The Classic storage driver must:

1. receive the expected Android partition start, length, layout UUID, and
   manifest digest from the validated boot container;
2. cross-check those values against the MBR entry before enabling writes;
3. register only bounded synthetic devices for the active read-only system
   slot and Android data region;
4. add checked offset arithmetic and reject overflow, negative, crossing, or
   out-of-range requests before they reach ATA/CE-ATA DMA descriptors;
5. reject discard, secure erase, sanitize, device firmware, raw pass-through,
   and any command whose affected LBA range cannot be proven;
6. translate flush only to a safe device cache flush with no caller-selected
   LBA; and
7. fail closed and stay in the initramfs recovery shell if the partition table,
   container headers, bounds, or hashes disagree.

The Android SELinux era postdates Eclair, so Unix permissions alone are not a
sufficient boundary. The lowest storage layer must make an out-of-range write
impossible even for root or a compromised Android process. Device-mapper can
provide convenient subdevices, but it is defense in depth; it does not replace
the low-level range check.

For Profile P, the expected post-boot invariants change: the MBR, FAT32
partition boundary, FAT32 metadata written by the installer, and Android
partition are expected to differ from the pre-install disk. The immutable
baseline becomes the committed post-install layout plus byte-identical hashes
for every pre-existing Rockbox file. Normal Android runs may change only LBAs
inside the Android partition; all sectors before its start and all FAT32 files
must remain unchanged apart from explicitly documented Rockbox-side launcher
logs, which should be disabled by default.

## 6. Proposed boot architecture

```text
host PC
  |
  | USB: wInd3x temporary recovery chain
  v
S5L8702 Boot ROM / defanged WTF
  |
  | volatile upload only
  v
Classic-specific U-Boot in DRAM
  |  capabilities: UART, timer, USB gadget DFU, RAM, reset
  |  deliberately absent: disk, flash, filesystem, saveenv
  |
  | USB DFU: signed/hash-checked FIT into DRAM
  v
Linux kernel + Classic DTB + minimal initramfs
  |  no local-storage device exists
  |  USB EEM/ECM/ACM to host
  +--> read-only /system in initramfs or NFS
  +--> tmpfs /data and /cache, or host NFS for persistence
  v
minimal Android 2.0 userspace
```

The official ARM Linux boot contract requires the loader to initialize RAM,
pass the device tree address in `r2`, quiesce DMA, and enter the kernel with the
expected MMU/cache state. The Classic U-Boot port must follow the
[ARM booting protocol](https://docs.kernel.org/arch/arm/booting.html) rather
than copying Nano memory addresses without validation.

The existing Nano U-Boot is a valuable template: it uses a DRAM text base,
DesignWare USB gadget support, a RAM-only DFU target, FIT images, and `bootm`.
Its Nano configuration assumes different RAM and peripherals. The Classic port
must validate its own load address, decompression destination, DTB placement,
initramfs placement, stack, malloc arena, and overwrite margins across the full
64 MiB map.

### 6.1 Root filesystem choices

| Root/data arrangement | Local data risk | Independence | Use |
|---|---:|---:|---|
| Entire minimal Android in initramfs; tmpfs data/cache | Lowest | Tether needed only for upload/debug | First Android boot |
| Small initramfs plus read-only NFS `/system`; tmpfs data/cache | Lowest locally | Host required | Main development mode |
| NFS `/system`, `/data`, and `/cache` | Lowest locally; host files change | Host required | Repeatable persistent development state |
| File-backed image on Rockbox FAT volume | Writes FAT and image metadata | Untethered after boot | Reject |
| Dedicated bounded Android container partition | Isolates normal writes; installation still resizes FAT32/MBR | Untethered after Rockbox starts | Preferred persistent profile after gates |

USB gadget Ethernet should be evaluated with EEM first because current
freemyipod Linux experiments use it; ECM is a compatibility alternative. Linux
documents composite gadget construction through
[configfs](https://docs.kernel.org/usb/gadget_configfs.html). A serial console
over ACM is useful, but the gadget configuration and endpoint budget must be
tested before combining networking and ADB.

### 6.2 Persistent boot without a NOR update

The preferred persistent flow keeps the currently installed bootloader and
Rockbox as the default:

```text
power/reset
  -> existing Rockbox bootloader and existing Rockbox firmware
  -> user selects "Start Android" in a tiny Rockbox launcher
  -> ROLO loads /.rockbox/android/android-loader.ipod
  -> loader stops Rockbox cleanly, parses MBR entry 3 read-only,
     validates both container headers and selected FIT slot,
     then enters Linux with the Android-only storage bounds
  -> Linux mounts read-only system slot and writable data region
```

Rockbox already supports loading another correctly wrapped firmware image with
ROLO, including stopping audio, flushing storage buffers, disabling interrupts,
and transferring control. The Android loader must still implement S5L8702-
specific quiescing and the ARM Linux boot protocol; ROLO is a delivery method,
not the Linux loader itself.

This design writes two small launcher artifacts to the FAT32 volume during
installation but avoids the much riskier act of replacing the NOR bootloader.
The installer must stage them under temporary names, verify hashes, rename
atomically, and leave both `rockbox.ipod` copies and every database file
unchanged. If the launcher is absent or corrupt, normal Rockbox still boots.

The volatile wInd3x/U-Boot route remains the rescue and independent test path.
It must be able to boot the installed Android partition read-only without
depending on either launcher file. A polished early three-way Apple/Rockbox/
Android boot menu can be considered later, but it would require a NOR
bootloader update and is not necessary to deliver usable dual boot.

### 6.3 Single-partition Android container

Use the next free MBR primary entry—expected to be the second table slot and to
appear as `/dev/sda2` if this iPod still has only its observed first entry.
Identify it by type plus container UUID, never by a hard-coded Linux device
name, and use type `0x83`. Align every boundary to at least 1 MiB and also to
the observed 4096-byte logical/physical sector.

The partition is a versioned container rather than one filesystem. A proposed
1 GiB default layout is:

| Region | Nominal size | Access | Purpose |
|---|---:|---|---|
| primary metadata | 1 MiB | read-mostly | magic, layout UUID/version, geometry, active slot, hashes, generation, CRC |
| boot A | 16 MiB | read-only at runtime | FIT containing kernel, DTB, and initramfs |
| boot B | 16 MiB | read-only at runtime | rollback/update FIT |
| system A | 96 MiB | read-only | SquashFS minimal Eclair `/system` |
| system B | 96 MiB | read-only | rollback/update `/system` |
| data | remainder, about 774 MiB | read/write | ext4 `/data`; `/cache` remains tmpfs initially |
| backup metadata | final 1 MiB | read-mostly | independently checksummed header/manifest copy |

The exact sizes are build outputs, not constants: calculate them from worst-
case artifacts plus margin and reject images that do not fit. A 512 MiB
container is the tentative minimum for a stripped product; 1 GiB is the
recommended starting allocation on this approximately 512 GB device. The
installer should offer an advanced size choice but never less than the
manifest minimum or more than a conservative fraction of verified FAT32 free
space.

The two system slots permit an installer to write and verify an inactive slot,
then switch one small redundant metadata record. Android never rewrites its
boot or system slots. The ext4 data filesystem must use checksums where
supported, barriers/flushes proven by testing, bounded journal size, no local
swap, and reserved free space. The first persistent boot may format only the
predeclared data subrange; it must not format the MBR partition itself.

### 6.4 Container and boot validation

Both metadata copies contain the physical logical-sector size, partition start
and length, every interior offset and length, layout UUID, generation number,
active/known-good slots, artifact sizes, and SHA-256 digests. All arithmetic is
performed in 64-bit checked byte units and converted to sectors only after
alignment validation.

The chainloader accepts a boot only when:

- MBR signature, entry type, start, and size match both headers;
- the two headers agree or one is demonstrably older but otherwise valid;
- every region is aligned, non-overlapping, inside the partition, and below
  the physical device size reported by storage;
- the selected FIT and system image hashes match the manifest;
- kernel, DTB, initramfs, and stack addresses fit the validated 64 MiB DRAM map;
- the Android partition does not overlap the FAT32 entry; and
- the requested slot is marked complete and either known-good or within a
  bounded trial-boot count.

CRC detects torn metadata; SHA-256 protects artifacts from accidental
corruption. If untrusted image distribution becomes a goal, add a signature
and embedded public key. Hashes alone do not authenticate a malicious image.

## 7. Linux and bootloader work breakdown

### 7.1 Volatile U-Boot target

Create a distinct Classic target; do not mutate the Nano configuration into an
ambiguous multi-board binary. Required work:

- Classic board identification and 64 MiB DRAM initialization/validation;
- S5L8702 clock, timer, interrupt, watchdog/reset, cache, and serial setup;
- Classic GPIO/pinmux and PMU setup needed for safe powered operation;
- DWC2 device-mode USB and a single bounded DFU-to-RAM alternate setting;
- FIT verification and `bootm`; and
- optionally, read-only LCD/status and buttons after the USB path is reliable.

Forbidden work in this target is equally important: no disk probing, no SPI
flash probing, no persistent environment, no general filesystem loader, and no
firmware installation command.

The first payload must do no more than initialize serial/USB, report board/RAM
facts, accept a bounded RAM upload, and reset. Only then should Linux be loaded.

The current volatile target implements the Classic cold DRAM sequence directly
from Rockbox's `miu_preinit(false)`. It keeps the Boot ROM's documented
CPU/AHB/APB clocks, selects the 12 MHz oscillator for the MIU refresh clock,
enables Rockbox's `SMx`/`SM1` gates, and probes both 32 MiB banks for aliasing
before relocation. The source and binary symbols are qualification inputs;
real electrical behavior remains a first-hardware-test question.

### 7.2 Classic Linux board support

Use the upstream S5L87xx/N46 work for SoC-common infrastructure and make a new
Classic device tree. Driver order is intentionally risk-driven:

| Order | Subsystem | Minimum exit condition | Rockbox reference |
|---:|---|---|---|
| 1 | early clock, VICs, timer, UART, reset/watchdog | deterministic boot log and reset, no storage access | S5L8702 startup/system code |
| 2 | USB PHY and DWC2 gadget | repeated RAM boot; stable ACM or EEM link | S5L USB target code and upstream N46 Linux |
| 3 | initramfs userspace | shell, proc/sysfs/devtmpfs, watchdog handling | not Android-specific |
| 4 | LCD framebuffer | stable RGB565 test patterns and blanking | `ipod6g/lcd-6g.c` and `lcd-s5l8702.c` |
| 5 | click wheel/buttons/Hold | correct evdev press/release/rotation events | iPod 6G button target code |
| 6 | PMU, battery, charger, thermal policy | safe readings, cutoff, and controlled shutdown | iPod 6G power/PMU code |
| 7 | Android legacy ABI bridge | Binder/ashmem/log/alarm/wakelock probes pass | historical Android kernels |
| 8 | audio | ALSA playback with click/pop and lifecycle tests | S5L I2S/PCM and CS42L55 code |
| Profile P only | CE-ATA/ATA container access | synthetic bounded devices; traced rejection outside Android range | storage code plus new firewall |
| never | NOR/NAND firmware writes | intentionally absent | boot-flash code is documentation only |

Suspend, deep idle, automatic frequency scaling, and unattended charging should
remain disabled until their PMU and wake paths have dedicated tests. Start at
known boot-safe clocks; use 216 MHz only after clock/voltage/thermal validation.

### 7.3 Display and graphics

Eclair's generic gralloc opens `/dev/graphics/fb0` or `/dev/fb0`, requests a
16-bit RGB565 mode, and prefers two vertically stacked buffers. At 320 x 240:

- one framebuffer is 320 x 240 x 2 = 153,600 bytes;
- two framebuffers are 307,200 bytes, excluding alignment; and
- a single-buffer fallback requires an extra software copy for presentation.

The Linux framebuffer driver must implement fixed/variable screen information,
pan/display or an explicit copy path, blanking, and cache-safe updates. There is
no usable GPU target, so the product must use software rendering. Disable
copybit and hardware-2D assumptions; patch generic gralloc if it attempts to
make mandatory pmem allocations.

### 7.4 Input contract

Expose controls through Linux evdev, then use an Android `.kl` file and a small
policy/IME layer. Proposed navigation:

| Physical action | Android action |
|---|---|
| wheel clockwise / counter-clockwise | DPAD_DOWN / DPAD_UP |
| Next / Previous | DPAD_RIGHT / DPAD_LEFT |
| Select | DPAD_CENTER / ENTER |
| Menu short press | BACK |
| Play/Pause | MEDIA_PLAY_PAUSE |
| Hold enabled | release and suppress normal keys and wheel |

The implemented driver preserves Menu+Select as an eight-second emergency
reset chord and uses Rockbox's 96-position wheel geometry and sensitivity-four
threshold. Hold releases every reported key, stops the wheel, and fails closed
after three consecutive PMU GPIO read errors. Physical debounce, repeat/rate,
wake behavior, and Hold transitions still need tests below the Android
framework and again at the UI layer.

Android's stock UI expects touch in many places. The minimal launcher and
settings surface must therefore be DPAD-navigable. For early text entry, use
ADB from the host. A click-wheel/T9 input method is a later usability feature,
not an MVP dependency.

### 7.5 Audio, deliberately post-MVP

Audio needs an ALSA SoC CPU DAI for S5L I2S/PCM, CS42L55 codec support matching
the Classic board, and a machine driver for clocks, routing, headphone state,
mute, and safe power sequencing. Current S5L87xx Linux work does not make this
a ready subsystem. Port register behavior from Rockbox cautiously rather than
copying its bare-metal lifecycle into Linux unchanged.

Eclair then needs a compatible audio HAL. First prove fixed-rate PCM playback;
later test rate changes, mute/unmute, suspend/resume, USB reconnect, rapid
application exits, and amplifier/codec wake transitions. Audio is not required
to prove Android framework feasibility.

## 8. Android kernel ABI decision spike

Eclair userspace expects historical Android-specific kernel interfaces,
including Binder, ashmem, logger devices, lowmemorykiller behavior, alarm, and
legacy wake locks. The current freemyipod Linux 6.14 tree has modern Binder but
does not simply reproduce that entire 2009 ABI.

Do not choose the long-term kernel by intuition. Build a small ABI probe suite
and compare two strategies:

### Strategy A: current S5L87xx Linux plus a compatibility layer

- retain the current SoC/USB enablement;
- enable Binder and test its protocol with Eclair's servicemanager;
- forward-port the smallest necessary legacy interfaces, or patch Eclair
  userspace to use supported replacements;
- provide ashmem semantics or a narrowly reviewed userspace/kernel substitute;
- route old logger use to an available logging mechanism;
- replace old alarm/wakelock assumptions explicitly; and
- use standard memory cgroups/oom policy only after proving framework behavior.

This avoids backporting the whole Classic hardware platform to an ancient
kernel but can expose subtle ABI mismatches.

### Strategy B: Android-era kernel plus S5L8702 backport

The historical Goldfish source is available around Linux 2.6.29 and contains
the old Android drivers. It is useful as an ABI reference, not a Classic BSP.
This strategy would backport all S5L87xx CPU, USB, clock, LCD, input, and PMU
work to the ancient kernel. It carries a much larger hardware, maintenance, and
security burden.

Select Strategy A unless the probe demonstrates an unbounded compatibility
problem. The decision record must include tests of Binder transactions,
process death notifications, ashmem pin/unpin/mapping behavior, logging, alarms,
wake handling, Zygote startup, SurfaceFlinger startup, and repeated boot.

Useful primary references are the historical Android
[Goldfish 2.6.29 kernel](https://android.googlesource.com/kernel/goldfish/+/android-goldfish-2.6.29/)
and its [Android driver Kconfig](https://android.googlesource.com/kernel/goldfish/+/ba3cd5796223e0971d30e910e0d5b953576f8629/drivers/android/Kconfig).

## 9. Memory and performance plan

Sixty-four MiB is the central feasibility risk. Static arithmetic is not enough
because kernel slab use, page tables, filesystem cache, initramfs unpacking,
Zygote sharing, and service behavior determine the real margin.

Known fixed or bounded costs include:

| Item | Approximate RAM consequence |
|---|---:|
| physical RAM | 64 MiB total |
| RGB565 framebuffer | 0.147 MiB single / 0.293 MiB double |
| kernel image/FIT during boot | temporary placement must not overlap decompression or initramfs |
| unpacked initramfs | charged at uncompressed size; must be aggressively minimized |
| Eclair default lowmemorykiller thresholds | 6, 8, 16, 20, 22, and 24 MiB |

The stock Eclair lowmemorykiller thresholds are enormous relative to this
machine and can kill processes in a loop. They must be measured and tuned only
after a minimal product boots; lowering them blindly can replace early kills
with total out-of-memory failure.

Required host experiments before hardware Android boot:

1. build the pinned minimal Eclair product in a hermetic legacy toolchain;
2. boot it in a host emulator at 96, 64, and 48 MiB to establish behavior, while
   recognizing that Goldfish memory use is not identical to the Classic;
3. collect bootchart, `/proc/meminfo`, slab data, process RSS/PSS, Zygote
   sharing, lowmemorykiller/oom events, and launch latency;
4. remove services and packages with evidence, not just APK size; and
5. establish a hardware measurement script before launching the framework.

Do not use local swap. Zram may be tested later, but it consumes RAM and CPU and
is not automatically beneficial on a 216 MHz processor. Host-backed network
swap complicates failure handling and should not be needed for the MVP.

Performance acceptance is intentionally modest: deterministic boot, responsive
DPAD navigation, stable display updates, and one small application. Web
browsing, video playback, modern application compatibility, and phone-like
multitasking are not goals.

## 10. Build and repository layout

Do not import Linux, U-Boot, or the full AOSP tree into this Rockbox source tree.
Keep separate pinned repositories and generate an integration manifest. A
proposed companion layout is:

```text
ipod6g-android/
  manifest/                 pinned upstream URLs and commits
  u-boot/                   Classic board patches/config
  linux/                    Classic DTS and driver patches/config
  aosp-device/apple/ipod6g/ BoardConfig, product, init, key layout, overlays
  initramfs/                reproducible minimal rescue and Android roots
  tools/                    upload, config audit, artifact manifest, log capture
  tests/                    ABI, framebuffer, input, USB, power, integrity gates
  artifacts/<build-id>/     hashes, configs, DTB decompile, manifests, logs
```

The installer belongs in this repository's Rockpod application, not in the
companion OS tree:

```text
rockpod/
  native/ipod6g_android_installer/  Rust transaction/recovery engine
  bin/<platform>/                   packaged helper built by CI/release
  services/android_installer.py    typed protocol and maintenance-mode service
  ui/android_manager.py            plan/install/update/recover/uninstall page
  tests/android_installer/         service tests and disk-image corpus
```

AOSP Eclair's original build checks for Java/Javac 1.5 and uses its bundled old
ARM-EABI compiler. Use a pinned container or virtual machine rather than
weakening the host globally. Record the container image digest, host tools,
locale, build variables, AOSP manifest revision, patches, and artifact hashes.

The integration build must emit one signed or cryptographically hashed FIT with
an explicit memory map. It must not quietly fetch unpinned branches.

## 11. Dedicated installer specification

The installer is built into Rockpod as a first-class feature. Its privileged
transaction engine is a new host-side program, not a shell wrapper around
`parted` and not code that runs from the iPod being modified. Rockpod is the
primary planner and installer UI; the same engine retains a headless recovery
CLI. The first release supports Linux hosts only so block-device identity,
exclusive access, privilege elevation, and tool versions can be controlled.

### 11.1 Installer packages and modes

Produce three independent packages:

- `ipod6g-android-plan`: read-only inventory and JSON plan generation;
- `ipod6g-android-install`: explicit install/update/uninstall transactions; and
- `ipod6g-android-recover`: restore an original MBR or finish/roll back a
  recognized interrupted transaction.

The install program has these modes:

| Mode | FAT32 effect | Use |
|---|---|---|
| use-existing-space | no FAT resize; consumes already unallocated aligned tail | preferred if at least the requested container size already exists |
| shrink-in-place | filesystem-aware FAT32 shrink, then shorter MBR entry | expected path on the currently observed full-disk volume |
| backup-rebuild-restore | recreates layout and restores verified files | recovery/factory conversion only; never automatic |
| update | writes only inactive Android boot/system slots and Android metadata | normal Android upgrade |
| uninstall | removes launcher/container, then optionally regrows FAT32 | must preserve a recoverable stop point at every step |

Modern GNU Parted's `resizepart` changes only the partition boundary; its
[manual explicitly says](https://www.gnu.org/software/parted/manual/parted.html)
that it does not resize the filesystem and that a filesystem must be shrunk
first. Therefore, the installer must never treat `parted resizepart` as the
shrink operation. A pinned `fatresize`/`libparted-fs-resize` implementation is
a candidate engine, but only after destructive tests on cloned 4-KiB-sector
disk images representing this exact FAT32 geometry. The tool and library
versions become part of the installer manifest.

The current `fatresize` source calls the filesystem resizer and then
`ped_disk_commit()` in one execution. Do not run that unmodified for this
transaction: it cannot provide the required verify-before-MBR checkpoint. The
installer must call a reviewed, pinned resize library or an audited fork that
separates FAT geometry/data relocation from the later explicit MBR commit.

### 11.2 Read-only planning phase

Planning performs no unmount, repair, resize, or write. It records:

- USB vendor/product, device serial, model, capacity, logical/physical sector
  sizes, removable status, and stable `/dev/disk/by-id` identity;
- raw MBR bytes/hash and all four entries, including gaps and end-of-disk;
- FAT32 BPB, backup boot sector, FSInfo, FAT count, cluster size, volume ID,
  declared filesystem length, allocation/free-space counts, and dirty flags;
- mounted source/target/options and every process with the volume open;
- hashes and sizes of both `rockbox.ipod` copies, all `.rockbox/database*.tcd`
  and `tagcache*.tcd` files, config, and the complete pre-existing file tree;
- minimum shrink size reported independently by the pinned resize library;
- requested Android size, aligned start/end, and interior layout; and
- host destination and free capacity for a full-device image plus manifests.

Refuse devices with ambiguous identity, multiple matching iPods, changing
geometry, unexpected partition overlaps, an unsupported sector size, HFS/APM,
unreadable files, a dirty FAT, active Rockbox database transaction files, a
failing storage-health preflight, or insufficient external backup capacity.
Do not run a repair automatically; report the exact prerequisite and exit.

The plan is content-addressed and contains a short confirmation fingerprint.
The write phase accepts only that exact plan and rechecks every input after the
device is unmounted.

### 11.3 Required backup set

Before repartitioning, create on a different physical disk:

1. a sparse-capable or compressed full-device image with SHA-256 chunks and an
   overall manifest;
2. raw copies of the MBR, FAT32 boot/backup/FSInfo sectors, and the final region
   that will be removed from the FAT32 geometry;
3. a file-level archive preserving names, timestamps, attributes, and content;
4. a separate Rockbox database/tagcache recovery set using the same validation
   principles as `tools/deploy_ipod6g_preserve_database.sh`; and
5. the original geometry/identity plan and recovery instructions.

Read every backup chunk back from the destination and verify it. For a 477.3
GiB device this requires substantial time and external capacity; the installer
must display estimates and may not downgrade to a partial backup merely for
convenience. A partial backup can be offered only as an explicitly unsafe
developer mode that does not satisfy this specification.

### 11.4 Transaction order for in-place shrink

The FAT32 volume must be unmounted and exclusively held for the entire write
transaction. Disable desktop auto-mount for this device, inhibit suspend, use a
host on reliable power, require a well-charged iPod, and flush/read back after
each numbered boundary.

1. Reidentify the device from immutable properties and rehash the MBR/BPB.
2. Verify the complete backup and plan fingerprint.
3. Perform a read-only FAT32 check with the pinned checker; abort on every
   inconsistency. Never invoke automatic repair.
4. Ask the filesystem-aware resize engine to shrink the FAT32 filesystem while
   keeping its start fixed. The target ends before the aligned Android start.
5. With the original larger MBR extent still present, reread the resized FAT32
   BPB/FATs and verify every pre-existing file hash. A filesystem whose internal
   size is temporarily smaller than its partition provides a recoverable stop
   point.
6. Write a redundant installer journal and original MBR copy into the now-free
   tail, outside the resized FAT32 internal extent but before the future
   container payload. Read it back and verify.
7. Commit one new 4096-byte logical sector 0 image that shortens the FAT32 MBR
   entry and adds the aligned Android entry. Preserve all non-partition bytes
   from the original sector and recalculate only fields defined by the plan.
8. Force a device rescan, then verify the raw MBR, both non-overlap bounds,
   FAT32 mount/read behavior, full file manifest, Rockbox databases, and both
   firmware copies before writing Android content.
9. Initialize the Android container metadata, inactive slots, data subrange,
   then active slots; read back and hash every byte written.
10. Mount the FAT32 volume and transactionally add only the Android launcher
    directory. Reverify every pre-existing file and database byte.
11. Flush device and host caches, read back random and boundary samples through
    a fresh device open, write the final signed/hash manifest to the host, then
    release exclusive access.

If power or USB fails before step 7, recovery uses the unchanged original MBR
and the now-smaller but intact FAT32 filesystem. If it fails after step 7,
Rockbox remains in the shortened FAT32 partition and Android may simply be
absent/incomplete. The recovery tool reads the host plan plus on-device journal
and either completes the exact pending step or restores the prior MBR; it never
guesses boundaries.

No software can make the single-sector MBR write provably atomic on unknown
flash translation hardware. The external image, redundant journal, MBR read-
back, and recovery tool address this residual risk; they do not turn it into
zero risk.

### 11.5 Installer write allowlist

The transaction engine opens the whole disk only after confirmation and wraps
every write with a plan-derived byte-range allowlist. The only permitted
destinations are:

- filesystem sectors selected by the pinned FAT32 shrink engine;
- logical sector 0 for the one planned MBR transition or exact recovery copy;
- the future Android partition extent; and
- the named FAT32 launcher staging/final files.

Log offset, length, old hash, new hash, phase, and read-back result for every
raw write. Reject shell-expanded device paths, unresolved symlinks, size
changes, hotplug identity changes, writes that cross an allowlist boundary,
and any command that would operate on the host system disk. Never pass a
whole-disk device chosen only by a short name such as `/dev/sda` to a generic
partition command.

### 11.6 Update and rollback

An Android update never resizes partitions. It writes the inactive boot and
system slots, verifies them, marks the new generation as trial, and leaves the
old pair known-good. After the new system reaches a boot-complete health gate,
the bounded Android metadata driver marks it good. Repeated failed trials make
the chainloader select the previous pair.

The data format needs an explicit compatibility version. An update that cannot
read old data must migrate inside the Android partition with a backup, or
start a new data region; it must not silently format it. The launcher and
chainloader are updated last through FAT32 temp-file, read-back, and rename
transactions.

### 11.7 Uninstall

Safe uninstall has two levels:

- **Disable Android:** remove/rename the launcher only. The Android partition
  remains recoverable and Rockbox geometry does not change.
- **Reclaim space:** back up Android if requested, remove the MBR Android entry,
  extend the FAT32 partition boundary, then use the pinned filesystem tool to
  grow FAT32. Verify all Rockbox files before and after each transition.

Growing uses the inverse safe order: remove access to Android data, change the
MBR so FAT32 owns the adjacent space, then grow the filesystem into it. Never
grow FAT32 while an overlapping Android entry still exists. Keep the original
post-install and pre-install plans so recovery can reconstruct either layout.

### 11.8 Implementation requirements

Implement the transaction core as a memory-safe compiled host program, with
Rust as the recommended choice, and keep its privileged surface small. Do not
construct shell command strings. Invoke any pinned external checker/resizer
with an argument vector, closed inherited file descriptors, fixed locale and
PATH, captured output, and the already resolved stable device identity.

The planner is read-only and should run without elevated privileges when the
host permits raw reads. The writer obtains elevation only after presenting the
complete plan, backup location, changed byte ranges, estimated duration, and a
confirmation fingerprint. It defaults to dry-run, never accepts “yes” from a
pipe for first installation, and refuses operation while any partition is
mounted or open.

Use checked `u64` byte arithmetic internally. Sector offsets are a property of
the opened device, not a compile-time 512-byte assumption. Direct raw writes
use positioned I/O, exact-length loops, flush, cache invalidation where
available, and independent read-back. Unit tests must cover short I/O, EINTR,
overflow, media size changes, and device removal. Release packages include
source, SBOM/tool versions, hashes/signatures, and the qualification corpus.

The installer may reuse parsing and database-validation concepts from
`utils/ipodpatcher` and `tools/deploy_ipod6g_preserve_database.sh`, but not the
old ipodpatcher MBR writer: it rejects non-512-byte sectors and rewrites a
normalized table that does not represent this device.

### 11.9 Rockpod product integration

Add a distinct Rockpod sidebar item named **Android on iPod** under ROCKBOX.
Do not repurpose the existing Linux page: `ui/linux_manager.py` and
`services/linux_payload.py` manage a PC Debian virtual-machine payload stored
as ordinary iPod files, not native iPod Linux. Naming and tests must prevent
users from confusing those workflows.

The Android page is a state machine, not a collection of raw action buttons:

| UI state | Available actions | Required presentation |
|---|---|---|
| no supported device | open documentation only | why target/host is unsupported |
| connected/mounted | read-only Analyze | model, raw identity, 4-KiB warning, current layout |
| plan ready | Back Up, export plan | exact FAT32 change, Android size, backup capacity/time |
| backup verified | Install after typed fingerprint | immutable plan hash and all stop checkpoints |
| transaction active | safe Cancel only at declared boundaries | phase, bytes, current recovery state, host log path |
| interrupted/recovery needed | Resume Exact Step or Restore MBR | no normal sync or install actions |
| installed | Verify, Update Inactive Slot, Start instructions, Disable, Uninstall | active/good slot, data use, last verification |

Rockpod's Python process never receives a writable whole-disk file descriptor
and never calculates a raw write offset. `services/android_installer.py`
launches the packaged helper through `QProcess`, consumes a versioned JSON
Lines protocol, validates event schemas, and maps helper phases to UI state.
Only the helper may obtain elevation. The UI may select a size and backup
destination and confirm a content-addressed plan; it cannot submit arbitrary
device paths, LBAs, commands, or environment variables.

The helper protocol contains at least:

- `hello`: protocol/helper/build versions and supported features;
- `inventory`: stable raw-device identity and read-only facts;
- `plan`: plan digest, layouts, ranges, backup demand, warnings, and gates;
- `progress`: monotonic phase/step/bytes plus whether cancellation is safe;
- `checkpoint`: durable recovery state and exact next legal operation;
- `verification`: named manifest/database/sector results;
- `error`: stable code, safe user message, diagnostic detail, and recovery flag;
- `complete`: installed/recovered layout UUID and artifact manifest; and
- Rockpod-to-helper `confirm`/`cancel` messages tied to transaction ID and plan
  digest, never a general command channel.

### 11.10 Rockpod maintenance mode

Installation unmounts the FAT32 volume, so Rockpod's normal mount-based device
detector will report a disconnect. A dedicated maintenance coordinator must
hold the transaction by USB serial, raw by-id path, capacity, sector size,
original MBR digest, volume ID, and layout UUID—not by mount path or Rockpod's
current label/capacity-derived `stable_device_key`.

Before unmount, maintenance mode must:

- pause device polling transitions, inventory verification, database/tagcache
  monitoring, auto-sync, artwork/video/game/plugin deploys, Linux-VM actions,
  and every other worker that can open the mount;
- wait for those workers to quiesce and list remaining open processes;
- inhibit system suspend and desktop auto-mount for this one device;
- checkpoint Rockpod's SQLite database and close device-related handles; and
- keep the transaction page alive if the mount disappears or the app window is
  hidden.

On clean completion, reacquire the mount, reconstruct `DeviceInfo`, perform a
full Rockpod inventory and tagcache validation, then release maintenance mode.
On application crash, the helper must either continue to the next durable safe
checkpoint or stop without guessing; restarting Rockpod must discover the
transaction journal before offering any normal device action.

### 11.11 Rockpod persistence and packaging

Add Rockpod database tables for Android installations and transactions. Store
only metadata: raw identity digest, layout UUID/version, plan digest, helper
build, active/known-good slots, installed Android build, backup/manifest host
paths, state, last safe checkpoint, and verification timestamps. Do not store
raw backup sectors or privileged credentials in SQLite.

Build the Rust helper from `rockpod/native/ipod6g_android_installer/` in CI and
release packaging, run its unit/integration/fuzz corpus, strip it reproducibly,
sign/hash it, and package it under a platform-specific `rockpod/bin/` path.
Rockpod verifies the helper hash before launch. Package a narrowly scoped
polkit policy or invoke `pkexec` only for the helper's `apply`/`recover`
subcommands; planning remains unprivileged/read-only where host permissions
allow. Missing pinned `libparted-fs-resize`/FAT checker dependencies disable
Install with an actionable diagnostic, never an automatic package-manager run.

The standalone helper remains mandatory even though Rockpod is the main UI: a
partition recovery path must not depend on PySide, Rockpod's SQLite database,
or a working desktop session. Rockpod exports the plan and exact recovery
command into the external backup directory before the first write.

### 11.12 Rockpod test additions

At minimum add:

- `tests/test_android_installer_service.py` for schema, helper hash, identity,
  stale-plan, cancellation, and error mapping;
- `tests/test_android_manager.py` for every UI state and destructive-action
  confirmation gate;
- `tests/test_android_maintenance_mode.py` proving all device workers quiesce
  and remain blocked across unmount/reconnect;
- `tests/test_android_installer_database.py` for crash-safe transaction rows;
- `tests/test_android_installer_packaging.py` proving the exact qualified
  helper and dependency manifest ship; and
- native Rust unit, property, fuzz, and disk-image power-cut tests invoked by
  the Rockpod CI entry point.

Tests must use mock/file/device-mapper targets only. The ordinary Rockpod test
suite must have no code path that discovers and opens a real whole disk for
writing.

## 12. Phased implementation and gates

Each phase begins only after the preceding phase's artifacts, logs, and
integrity checks are archived. A failure returns to offline analysis; it never
authorizes enabling more hardware.

### Phase 0: offline reproducibility and safety audit

Deliver:

- pinned source manifest and patch queues;
- legacy AOSP build container;
- host-emulator memory report;
- storage-firewall audit script and known-bad fixture proving it can fail; and
- backup/restore and device-integrity runbook.

Exit gate: two clean builds have identical meaningful manifests, the bad fixture
is rejected, and an independent reviewer finds no storage capability in the
intended boot artifacts.

### Phase 1: harmless volatile loader

Run wInd3x and a Classic U-Boot diagnostic containing only clock/timer, bounded
RAM testing, USB, serial if available, and reset. Do not load Linux yet.

Exit gate: 100 upload/reset cycles without touching persistent storage, stable
USB enumeration, verified 64 MiB bounds, working emergency reset, and unchanged
post-test disk/Rockbox integrity checks.

### Phase 2: storage-less Linux shell

Boot a FIT containing Linux, Classic DTB, and a tiny initramfs. Use earlycon or
USB serial. The build still has no LCD, input, Android, or storage support.

Exit gate: 100 boots, one-hour powered run, watchdog/reset recovery, clean USB
disconnect/reconnect behavior where supported, and unchanged integrity checks.

### Phase 3: USB networking and host root

Bring up EEM or ECM, deterministic host addressing, NFS read-only root, and
central log capture. Test unplug behavior; the device must fail safely into RAM
or reset, never search local storage.

Exit gate: repeated large read-only transfers, NFS loss/recovery tests, and no
kernel memory corruption or local device node.

### Phase 4: framebuffer

Add the Classic LCD and framebuffer test application before SurfaceFlinger.
Test solid colors, gradients, clipping, pan/copy paths, blank/unblank, and hours
of update traffic.

Exit gate: correct geometry/colors, no out-of-bounds DMA, stable USB, safe
blanking, and acceptable temperature.

### Phase 5: input

Add wheel/buttons/Hold as evdev. Capture raw traces and test every transition,
repeat, acceleration, wake case, and emergency chord.

Exit gate: deterministic raw-event suite and a DPAD-only Linux UI can be fully
navigated without trapping the user.

### Phase 6: power-safety baseline

Add read-only battery/charger/thermal measurements first, then controlled
shutdown and only the minimum power writes needed for safe operation. Do not
enable suspend or DVFS yet.

Exit gate: calibrated plausible readings, low-battery cutoff, USB-power loss
behavior, watchdog reset, and temperature limits tested under CPU/LCD/USB load.

### Phase 7: Android ABI smoke tests

Run native probes for Binder, ashmem/replacement, logger, alarm, and wake
interfaces. Then start servicemanager alone and exercise repeated IPC.

Exit gate: written Strategy A/B decision record and repeatable ABI tests without
kernel faults or leaks.

### Phase 8: minimal Android graphics boot

Start init, servicemanager, Zygote, SurfaceFlinger, system_server, and the
minimal launcher. Keep data/cache in tmpfs and debugging over the host link.

Exit gate: 20 consecutive boots to the launcher, ten minutes of stable idle,
no lowmemorykiller/oom restart loop, stable framebuffer, and all integrity
checks unchanged.

### Phase 9: usable click-wheel prototype

Integrate the key layout, non-touch launcher/settings, screen blanking, ADB text
entry, and one test application. Audio remains optional.

Exit gate: every supported feature is operable without touch; reset and Hold
behavior remain reliable; 60-minute interaction stress run passes.

### Phase 10: optional audio

Implement ALSA SoC and the Eclair audio HAL only after the core is stable.

Exit gate: clean fixed-rate playback, volume/mute, plug/unplug if detectable,
repeated start/stop, framework restart, and power-cycle tests without clicks,
hangs, or unsafe codec state.

### Phase 11: persistent storage driver on disk images

Implement the Android-container parser, low-level LBA firewall, synthetic
system/data devices, A/B state machine, and fault injection against file-backed
disk images first. Generate valid, truncated, overlapping, overflowing, torn,
and adversarial MBR/container layouts with both 512- and 4096-byte sectors.

Exit gate: every out-of-range request is rejected before the mock ATA layer,
all power-cut points recover to either the old or new valid generation, and
coverage includes every bounds/overflow branch.

### Phase 12: installer qualification on clones

Clone the exact iPod image to sacrificial 4-KiB-sector media or a device-mapper
test target. Run install, interrupted install at every transaction boundary,
update, rollback, disable, uninstall, and FAT32 regrow. Compare all pre-existing
file hashes and database contents each time. Repeat with nearly-full FAT32,
fragmented tail data, maximum filenames, bad backup destination, disconnect,
host crash simulation, and stale plans.

Drive the same corpus both through the standalone helper and the packaged
Rockpod Android page. Exercise maintenance mode against mocked concurrent sync,
inventory, database monitor, deploy, and device-disconnect workers. Verify that
restarting Rockpod discovers every interrupted journal before normal device
features become available.

Exit gate: at least 100 clean full cycles and every injected interruption is
recovered by the documented tool without losing a pre-existing file. The exact
pinned shrink stack, helper build, Rockpod protocol, and packaged binary hash
are frozen after qualification.

### Phase 13: persistent install on a spare iPod

Install on a spare matching Classic/flash geometry before this device. Boot
Android through both ROLO and volatile recovery, then stress writes within the
data region while continuously auditing sent ATA ranges.

Exit gate: 100 boots, repeated power-loss tests, A/B rollback, update/uninstall,
and raw-sector guards show no write outside the Android partition after install.

### Phase 14: guarded install on this iPod

Only after the user reviews the final plan fingerprint and verified full backup
does the installer modify this device. Stop after the FAT shrink verification,
again after MBR commit, and again after Android installation so Rockbox can be
booted and checked at each recoverable boundary.

Exit gate: Rockbox, Apple firmware if retained, databases, music, settings, and
both firmware copies pass; Android persists data only in its partition; disable
and recovery paths are proven. This gate authorizes normal Android updates, not
future repartitioning.

Any Rockbox deployment from this repository must follow `AGENTS.md`: deploy the
built `rockbox.ipod` to both the volume root and `.rockbox/rockbox.ipod`, verify
both checksums, and use `tools/deploy_ipod6g_preserve_database.sh` for a full
package so database and tagcache files are preserved and verified.

## 13. Test matrix

| Domain | Required cases | Failure condition |
|---|---|---|
| storage isolation | Profile V binary/config/DT audit; Profile P LBA-firewall traces; `/dev` inventory; pre/post hashes | V exposes storage, or P writes outside its container |
| installer | 4-KiB disk-image cycles; interrupted shrink/MBR/content writes; stale/wrong device; install/update/uninstall | any lost pre-existing file, guessed recovery, or unplanned byte write |
| boot | cold DFU, repeated upload, corrupted/truncated FIT, bad DTB, USB removal, emergency reset | write attempt, hang without reset path, unchecked image accepted |
| memory | boot at target product size; stress allocation; framework restart; leak loops | kernel corruption, recurring OOM/LMK loop, overlap in boot map |
| USB | enumerate, reconnect, host reboot, bad packet/short transfer, NFS loss | kernel fault, unsafe fallback, permanent device state |
| display | bounds, colors, page flip/copy, blanking, rapid updates | DMA/buffer overrun, corruption outside framebuffer |
| input | all key pairs, wheel rates/directions, Hold transitions, long press, reset chord | missed release/stuck key, reset chord blocked |
| power | USB power loss, low battery, thermal load, watchdog, controlled poweroff | over-temperature, invalid charging behavior, uncontrolled brownout loop |
| Android ABI | IPC, process death, shared memory, logs, alarm, wake, restarts | incompatible semantics or unbounded compatibility patch set |
| UI | DPAD-only traversal, Back/Home, focus visibility, ADB text entry | touch-only trap or inaccessible recovery |
| audio, later | rate/start/stop/mute/volume/restart/power transitions | click/pop, hang, hot codec, unsafe clock sequence |

All physical-test logs must name the exact artifact hashes. “It booted once” is
not evidence for promotion to the next phase.

## 14. Open questions that must be answered experimentally

1. Does the current wInd3x `run` Classic path enumerate and upload reliably
   on this exact hardware revision without persistent changes?
2. Does the exact Rockbox Classic cold-DRAM sequence and four-point 64 MiB
   alias probe pass on this device's DRAM vendor/revision before relocation?
3. Can the upstream N46 DWC2/PHY implementation be reused with only Classic
   board pin/PMU changes?
4. Which Classic LCD controller/panel revisions need runtime detection?
5. What event representation best preserves click-wheel direction and rate?
6. What are safe battery voltage, charger, thermal, and shutdown controls for
   prolonged USB-powered Linux development?
7. Does Eclair's Binder userspace work against the selected modern Binder
   configuration, including transaction ABI and death notifications?
8. Is an ashmem compatibility implementation smaller and safer than patching
   Eclair's gralloc/framework consumers?
9. What is the measured minimum PSS and boot-time peak of the stripped product
   on the real kernel?
10. Can DWC2 expose the required networking/debug functions simultaneously
    with stable endpoint allocation?
11. Is 216 MHz sustainable under framework load at validated voltage and
    temperature, or must the product target 108/54 MHz behavior?
12. Which features, if any, remain usable enough on a 320x240 non-touch display
    to justify work beyond the framework demonstration?
13. Does the Apple firmware on this exact flash-converted Classic tolerate and
    preserve an additional `0x83` MBR primary entry, or does it offer a restore,
    rewrite the table, or refuse sync? Test only on a clone/spare first.
14. Can the pinned FAT32 resize stack safely shrink this exact 4096-byte-sector
    volume while preserving its boot-sector conventions and every long name?
15. Does an S5L8702 ROLO chainloader reliably quiesce Rockbox storage, USB,
    audio, DMA, caches, and interrupts before Linux entry?
16. Can the ATA/CE-ATA driver prove and log every translated request range,
    including flush, retry, error recovery, and DMA splitting?

The first twelve questions can be answered without enabling iPod storage.
Questions 13–16 belong only to the qualified persistent track.

## 15. Risk register

| Risk | Likelihood / impact | Mitigation or decision |
|---|---|---|
| accidental Rockbox/storage modification | Low only with enforced firewall / catastrophic | omit hardware and writers at bootloader, DT, kernel, and initramfs layers; pre/post hashes |
| Classic board differs materially from N46 | High / high | new board target, validate each subsystem against Rockbox, staged gates |
| 64 MiB cannot sustain Eclair framework | High / high | minimal product, measured ABI/framework spike, Android 1.6 fallback, stop if unstable |
| modern-kernel/Eclair ABI mismatch | High / high | ABI probe and explicit strategy decision before framework integration |
| USB is the only load/debug/root channel | Medium / high | simple gadget first, watchdog/reset, bounded RAM image, loss testing |
| PMU/clock mistake damages or browns out device | Medium / high | boot-safe clocks, read-only telemetry first, thermal and low-battery gates |
| non-touch UI is unusable | High / medium | DPAD-only product UI; treat stock app compatibility as out of scope |
| ancient Android toolchain is irreproducible/insecure | High / medium | offline pinned container, no production network trust, artifact manifests |
| experimental recovery flow fails | Medium / high | spare device, harmless first payload, verified backup, documented reset/DFU path |
| FAT32 shrink moves or loses Rockbox data | Medium / catastrophic | full verified image, frozen resize stack, clone qualification, byte-identical manifest gate |
| 4-KiB MBR write tears or wrong device is selected | Low / catastrophic | by-id/serial/geometry binding, exclusive access, journal, raw read-back, recovery tool |
| Apple firmware rewrites or rejects the extra partition | Unknown / high | test clone/spare; keep Apple boot out of acceptance if incompatible; never discover on sole copy |
| Android escapes its LBA range | Low after proof / catastrophic | lowest-driver range firewall, no whole-disk node, adversarial and traced I/O tests |
| another Rockpod worker touches FAT32 during resize | Medium without coordination / catastrophic | global maintenance mode, worker quiesce barrier, exclusive raw-device lock |
| Rockpod UI and privileged helper disagree | Low / high | versioned strict protocol, plan digest on every control message, helper is final policy authority |

## 16. Effort, skills, and distribution obligations

This is not a weekend ROM repack. A credible prototype needs embedded ARM
bootloader and Linux experience, device-tree/driver work, USB gadget debugging,
Android platform-build knowledge, and access to hardware instrumentation. A
serial connection or equivalent early logging, a USB protocol capture option,
a current-limited supply or reliable battery telemetry, and preferably a spare
Classic materially reduce bring-up risk.

Very rough elapsed engineering ranges for one experienced developer are:

| Milestone | Optimistic focused effort | Main uncertainty |
|---|---:|---|
| reproducible AOSP/host experiment and safety tooling | 1–3 weeks | old build environment and product trimming |
| volatile Classic U-Boot | 2–8 weeks | DRAM, USB, PMU, and Classic board differences |
| storage-less Linux shell and USB networking | 1–3 months | Classic board support and sole debug channel |
| LCD, input, and safe power baseline | 1–3 months | panel revisions, wheel, PMU behavior |
| Eclair ABI and minimal graphics boot | 1–4 months | 64 MiB and legacy ABI compatibility |
| usable UI and optional audio | 1–4+ months | framework trimming, non-touch UX, new audio drivers |
| container driver, chainloader, Rockpod installer, and qualification | 2–6+ months | 4-KiB FAT shrink, maintenance mode, fault injection, Apple-firmware compatibility |

These ranges overlap and are not commitments. A solo effort with no existing
S5L87xx Linux experience can reasonably take much longer, and the memory or
power gates may end the project. The first decision-quality milestone is Phase
2, not a promise of full Android.

Distribution also needs license review. Linux and U-Boot are GPL projects;
distributed binaries require the corresponding source and license compliance.
AOSP combines Apache-licensed and other components, and the old toolchain has
its own notices. Keep source manifests, patches, notices, and reproducible
build instructions with every public artifact. An unmaintained 2009 Android
userspace must never be represented as secure for general network use.

## 17. Stop conditions

Stop the project, preserve evidence, and do not advance if any of the following
occurs:

- any pre-existing Rockbox file, firmware, database, or tagcache byte changes;
- any partition or sector changes outside the exact installer transaction plan,
  or any runtime write outside the Android container;
- the final boot artifact contains an unreviewed persistent-storage capability;
- reset/DFU recovery is unreliable on the exact device;
- battery, charger, or thermal behavior cannot be bounded safely;
- the memory spike cannot keep the minimal framework stable in 64 MiB;
- supporting Eclair requires an open-ended set of unsafe ancient-kernel
  backports; or
- useful navigation cannot be achieved without touch.

The first definition of done is a repeatable volatile boot to a DPAD-navigable
minimal Eclair UI, with a documented storage-incapable chain and unchanged
Rockbox data after every test. The final definition of done adds a qualified
installer, dedicated range-confined partition, ROLO and recovery boot paths,
A/B rollback, persistent Android data, and byte-identical pre-existing Rockbox
files after install, use, update, rollback, disable, and uninstall testing.

## 18. Primary source index

- [freemyipod hardware matrix](https://freemyipod.org/wiki/Hardware)
- [freemyipod iPod Classic page](https://freemyipod.org/wiki/Classic_1G)
- [freemyipod Linux status](https://freemyipod.org/wiki/Linux)
- [freemyipod wInd3x documentation](https://freemyipod.org/wiki/WInd3x)
- [freemyipod Linux repository](https://github.com/freemyipod/linux)
- [freemyipod U-Boot repository](https://github.com/freemyipod/u-boot)
- [freemyipod wInd3x repository](https://github.com/freemyipod/wInd3x)
- [AOSP Eclair build tag](https://android.googlesource.com/platform/build/+/refs/tags/android-2.0_r1)
- [AOSP Eclair manifest](https://android.googlesource.com/platform/manifest/+/android-2.0_r1/default.xml)
- [Android release/build-number reference](https://source.android.com/docs/setup/reference/build-numbers)
- [Android 1.6 Compatibility Definition](https://source.android.com/docs/compatibility/1.6/android-1.6-cdd)
- [historical Android Goldfish kernel](https://android.googlesource.com/kernel/goldfish/+/android-goldfish-2.6.29/)
- [historical Goldfish virtual-hardware description](https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT)
- [QEMU system emulation](https://www.qemu.org/docs/master/system/introduction.html)
- [QEMU TCG internals](https://www.qemu.org/docs/master/devel/tcg.html)
- [Linux ARM boot protocol](https://docs.kernel.org/arch/arm/booting.html)
- [Linux USB gadget configfs](https://docs.kernel.org/usb/gadget_configfs.html)
- [GNU Parted manual: `resizepart` does not resize a filesystem](https://www.gnu.org/software/parted/manual/parted.html)
- [fatresize upstream source](https://github.com/ya-mouse/fatresize)
- [fatresize transaction source showing filesystem resize and disk commit](https://github.com/ya-mouse/fatresize/blob/master/fatresize.c)
- [current Debian fatresize package and dependencies](https://packages.debian.org/trixie/fatresize)
- [freemyipod Classic Rockbox installation and flash-adapter warning](https://files.freemyipod.org/~user890104/bootloader-ipodclassic.html)

Source pages and repositories are living upstreams. The commit hashes in
Section 3.3, local artifact hashes, and the integration manifest—not a moving
wiki page—must govern an implementation build.

## 19. Implementation checkpoint (2026-07-21)

The Phase 0 image-only planner, reversible MBR transaction, Rockpod page, and
host cross-build gates described in this specification are implemented. Exact
test evidence and the staged volatile-test decision are recorded in
[`ipod6g-android-qualification-report.md`](ipod6g-android-qualification-report.md).

The Classic storage-free U-Boot target, Linux DTS/defconfig, syscall-only
initramfs, deterministic FIT/DFU builder, artifact qualifier, generic ARM926
emulation gate, and Rockpod bundle verifier now exist. Their current hashes and
test evidence are recorded in the qualification report.

The official `android-2.0_r1` native core, Dalvik VM, `dexopt`, framework JARs,
native Android runtime, Zygote, `system_server`, SurfaceFlinger, dual-path
framebuffer/headless gralloc, and official PixelFlinger software renderer are now built for
ARMv5TE and packaged in the storage-free N25 initramfs. A 64 MiB ARM926
full-system QEMU gate boots the exact initramfs, proves Binder protocol 7 and
Dalvik/framework execution, enters native `system_init()` and Java
`SystemServer`, publishes SurfaceFlinger with a RAM-only 320x240 RGB565 buffer,
installs SettingsProvider, DEX-optimises the minimal Rockpod Launcher through
stock Eclair `installd`, resolves it as HOME, reaches the Launcher's
`onCreate()`, and keeps Android init and Zygote alive for the 45-second strict
window. It attaches neither a drive nor a network backend. Canonical packaging
removes timestamp/order variance from the deployable framework JARs; APKs stay
byte-for-byte because Eclair depends on their original stored/aligned resource
entries.

The N25 kernel now compiles and links a Classic fbdev bridge. It reads the
known panel strap, applies the exact Rockbox-derived initialisation path for
each of the four 8- and 16-bit panel families, transfers RGB565 pixels, and
uses a childless S5L8702 I2C controller for only the fixed PCF50635 LCD-rail
and LED-backlight writes. It does not register the generic PCF MFD or expose a
storage peripheral. Eclair gralloc tries that real framebuffer before falling
back to ashmem. Qualification proves the driver/config/device-tree linkage,
while QEMU proves the no-framebuffer fallback; it cannot model or validate
Classic LCD or PMU registers.

The kernel also compiles a Classic click-wheel evdev driver, active-low Hold
polling through the one required PCF50635 GPIO register, an eight-second
Menu+Select reset chord, and the S5L8702 restart handler. Eclair packages the
device-specific DPAD/BACK/media key layout. The smaller Linux diagnostic has a
60-second forced reset; the Eclair packet has a 180-second forced reset. A
host-side exact-packet model passes wheel direction/wrap, button, Hold,
fail-closed PMU-error, and reset-chord cases.

The Classic Rockbox bootloader now reserves exact Menu+Play for a direct,
fail-closed Linux handoff. It loads only three fixed ip6g model/checksum-wrapped
files from `.rockbox/android` into qualified non-overlapping RAM ranges, checks
their exact padded sizes, unmounts and sleeps storage, disables caches/MMU, and
enters Linux with the DTB in `r2`. A qualified wInd3x wrapper runs this
bootloader only from volatile DFU; the persistent `.ipod` form is packaged but
not installed. Rockpod's narrow stager creates only the three component files,
refuses overwrite, and verifies both Rockbox copies plus unchanged
database/tagcache snapshots.

Two independent source/build trees produced byte-identical Eclair and
diagnostic packets, and the Eclair initramfs passed a 64 MiB ARM926 QEMU boot
through Launcher `onCreate()`. That validates userspace, not the N25 machine.
Repeated volatile N25 direct boots, including `diagnostic-lcd1`, retained the
Rockbox white handoff legend. Because those packets had no distinct final
loader or raw kernel entry frame, the framebuffer result did not identify the
stopping boundary. The corrected raw
kernel and DTB now reach `start_kernel`, execute timer registration, and emit
Rockbox's exact Timer B programming transaction in ARM926 instruction
emulation. The N25 path uses Timer B/IRQ 8 for the tick and Timer E for the
clocksource. The replacement also uses a board-specific PL192 path matching
Rockbox's edge setup and VICADDRESS entry/completion handshake. TRACE1 changed
from Rockbox text to a full white screen and was disqualified: its model
omitted panel GRAM cursor state and changed complete frames too quickly to
identify visually. Exact-kernel ARM926 emulation now executes the complete
Rockbox panel-prepare/quiesce/cache/jump path, models the exact 8- and 16-bit
panel command sequences, progresses through Linux initcalls, and requires five
persistent 320x48 bands to fill one 76,800-pixel GRAM transaction for all four
N25 straps. TRACE2 nevertheless retained the physical Rockbox text and is
disqualified. No further Rockbox-to-Linux packet is approved; the next gate is
the storage-free volatile U-Boot `05ac:8007` enumeration test, with no FIT or
Linux upload. Physical timer/VIC and LCD behavior remain unqualified.
LCD/input/reset,
battery/charging, controlled shutdown, persistent storage, audio, Froyo, dual
boot, and partitioning all remain blocked. The prior payload staging did not
modify Rockbox firmware, its database, the partition table, or NOR.
