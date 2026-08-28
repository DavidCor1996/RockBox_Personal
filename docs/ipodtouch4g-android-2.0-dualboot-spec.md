# Android 2.0 on iPod touch 4G: volatile dual-boot bring-up specification

Status: pinned storage-free OpeniBoot loader, narrowed no-NOR SHAtter/Image3
transport, Android-capable N81 Linux kernel, RAM diagnostic, and Android 2.0
root integration host-qualified; physical no-NOR SHAtter and reset-to-iOS
recovery proven, OpeniBoot USB liveness not yet proven

Date: 2026-08-12

Target: Apple iPod touch 4G, `iPod4,1`, board `N81AP`, iOS 6.1.6

Safety posture: preserve the installed iOS system; RAM-only experiments first;
no persistent device write path in the implemented loader

## Decision

The connected target is an iPod touch 4G, so the practical route is the
historical OpeniBoot/iDroid architecture, not a Project Sandcastle binary.
Sandcastle's published beta supports iPhone 7, iPhone 7 Plus, and iPod touch
7. Its status matrix reports working CPU, USB, NAND, display, touch, power,
Wi-Fi, and Bluetooth on those targets, but no GPU or audio. That is useful
evidence for current iOS-device Android work, but its A10/pongoOS port cannot
be installed on the A4-based Touch 4G.

The OpeniBoot repository has build targets for iPod touch 1G, 2G, and 4G and
can hand off to Linux. Its own README warns that A4 NAND write functions are
unfinished and can cause data loss. Consequently, “dual boot” in this port
first means a tethered session: reset leaves the iOS installation as the only
persistent boot path, while a host supplies OpeniBoot, Linux, and Android to
RAM. A permanent on-device boot menu is explicitly out of scope until the
volatile system works and storage safety has an independent design review.

This repository now implements the complete host-qualified RAM-boot artifact set.
It does not claim that Android currently boots on the device.

## Current device landscape

| Device family | Public Android route | Current published state | Relevance |
|---|---|---|---|
| iPhone 7 / 7 Plus (A10) | Project Sandcastle | Beta image; core display/touch/connectivity listed, GPU and audio absent | Ready-made modern research target |
| iPod touch 7 (A10) | Project Sandcastle | Beta image with the same principal limitations | Only iPod touch in Sandcastle's published beta |
| iPod touch 1G / 2G | OpeniBoot/iDroid | Historical OpeniBoot targets, not a current supported Android distribution | Prior art only |
| iPod touch 4G (A4) | OpeniBoot plus historical iDroid Linux and a new Android device integration | N81 kernel/loader source exists; no current ready-to-install Android image | This port's target |
| Other iPhone/iPad/iPod models | Device-specific exploit and driver work | No general-purpose current dual-boot package | Do not infer support from CPU family alone |

“Supported” here describes published project targets, not a fully functional
consumer OS. Sandcastle is beta software, and OpeniBoot's Touch targets are
developer infrastructure. The upstream projects are the authority for their
own device lists:

- [Project Sandcastle home](https://projectsandcastle.org/)
- [Project Sandcastle device status](https://projectsandcastle.org/status)
- [Project Sandcastle source](https://github.com/corellium/projectsandcastle)
- [OpeniBoot source and target documentation](https://github.com/iDroid-Project/openiBoot)
- [Historical iDroid Linux kernel source](https://github.com/iDroid-Project/iDroid-kernel)
- [Project Sandcastle technical history](https://projectsandcastle.org/history)

## Exact hardware baseline

A read-only `ideviceinfo` probe during this work observed:

| Property | Value |
|---|---|
| Product type | `iPod4,1` |
| Hardware model | `N81AP` |
| Installed iOS | 6.1.6 |
| SoC / CPU | Apple A4 / ARM Cortex-A8 |
| RAM | 256 MiB |
| Linux machine ID used by upstream OpeniBoot | 3564 |

The probe deliberately retains no ECID, serial number, UDID, or other unique
identifier. `device_probe.py` fails closed for every other model.

The AOSP baseline is the official `android-2.0_r1` Eclair tag already pinned
by `tools/ipod6g_android/eclair/source-lock.json`. Its ARMv5TE userspace can
execute on the Cortex-A8. The historical iDroid kernel at commit
`3f971a676096c37472aec5e139b2720245e509e1` supplies an N81 machine
description, display path, timer, interrupt controller, serial support, and
reset path, but it does not provide a finished Android device integration.

## Implemented slice

`tools/ipodtouch4_android/source-lock.json` pins upstream OpeniBoot commit
`866562fdb1cfd019bcd77885c80fbf0af65d5c15`, iDroid kernel commit
`3f971a676096c37472aec5e139b2720245e509e1`, ipwndfu commit
`0e28932ec6a2a570b10fd77e50bda4216418cd98`, all archive SHA-256 values, the
exact N81 target, host tools, and Eclair source lock.

The preparation tool accepts only that archive checksum, rejects unsafe tar
members, applies the reviewed patch, and records a provenance marker. The new
`ipt_4g_volatile_openiboot` target builds a headless USB-first loader containing
only the minimal A4 support, bounded command shell, USB ACM/upload, and Linux
handoff code. It excludes display/framebuffer, storage, and filesystem stacks
and removes persistent image and unrestricted execution/write commands. The
volatile image is linked directly at SHAtter's `0x84000000` destination and
places its exception vectors there, avoiding upstream OpeniBoot's early
relocation through address zero. The normal upstream targets remain linked at
their original address. A single fixed `n81state` command reports SCTLR, TTBR0,
VBAR, and the A4 MIU register so the physical Linux handoff can be designed
from observed state; it accepts no address or value arguments and performs no
write.

The loader's DRAM contract is:

| Region or limit | Address / size |
|---|---:|
| in-place bootstrap heap | `0x84020000`–`0x8402b000` |
| in-place supervisor stack top | `0x8402e000` |
| kernel destination | `0x4a000000` |
| initramfs destination | `0x4b000000` |
| upload staging buffer | `0x4d000000` |
| maximum kernel | 8 MiB |
| maximum initramfs/upload | 20 MiB |
| SHAtter shellcode boundary | `0x8402f198` |

The loader qualifier checks provenance, a `0x84000000` ELF entry point and
`_start`, exact canonical OpeniBoot ELF flattening (including zero-filled
loadable memory), in-place exception-vector setup, absence of the old volatile
ARM/MMU setup calls, required USB/Linux symbols, forbidden storage and write
symbols, the selected source list, build markers, heap bounds, and non-overlap
of these regions.
The kernel pipeline applies only modern-host compiler fixes and safety edits:
it removes the N81 H2FMI call/object and HSMMC platform registration, changes
the unfinished board's framebuffer request from its unsupported 24 bpp value
to the driver's supported 32 bpp mode, and builds a deterministic Linux 3.0.8
zImage. Its configuration disables block storage, MTD, MMC, SCSI, ATA, IDE,
MD, Apple VFL/FTL, disk filesystems, `/dev/mem`, `/dev/kmem`, swap, and modules.
The kernel gate then proves the exact N81/display/serial/initramfs symbols and
rejects storage symbols and enabled storage configuration.

The diagnostic initramfs contains only `/init` plus empty `/dev`, `/proc`, and
`/sys` directories. Its 2,336-byte static ARMv7 PID 1 requests three 32-bit
colour bands through `/dev/fb0`, emits console heartbeats, and requests the
kernel's N81 reset path after 30 seconds. It includes no shell, mount helper,
storage path, or persistence mechanism. `plan_ram_boot.py` accepts only loader,
kernel, and diagnostic artifacts matching all three qualification reports,
rejects local-storage root arguments, and emits a dry-run USB ACM command plan.
The corresponding USB transport is now implemented with no arbitrary-command
option: it can only upload the two qualified artifacts, select them with the
fixed `kernel` and `initrd` commands, and dispatch `boot` after an exact
profile-specific token.

The volatile boot-chain transport is also implemented and host-qualified. The
pinned upstream SHAtter source identifies S5L8930 SecureROM
`iBoot-574.4`, receives at most `0x2c000` bytes at `0x84000000`, loads an
unsigned Image3, and jumps to that RAM address. Its original shellcode called
`nor_power_on()` and `nor_init()` despite not writing NOR; the repository patch
removes both calls and definitions before rebuilding a 352-byte payload. Only
that payload is retained. The broad upstream CLI and its dump, flash, install,
restore, and generic memory surfaces are not staged.

`wrap_volatile_openiboot.py` creates a deterministic minimal `ibss` Image3 with
one DATA tag, zero padding, exact payload round-trip verification, and the
SHAtter size ceiling. The current reference wrapper is 122,752 bytes.
Normal-mode
target selection produces a salted one-way `UniqueChipID` digest; DFU must
match it along with `CPID:8930` and `SRTG:[iBoot-574.4]`. No raw unique ID is
saved or printed. Physical actions remain separately token-gated.

Three physical loader trials preceded the in-place-link fix: the original
loader, a headless USB-first loader, and a headless loader that also bypassed
the volatile ARM/MMU setup. In every trial, the continuity-bound no-NOR SHAtter
stage succeeded and transferred the loader, but the device disappeared before
OpeniBoot USB (`0525:1280`) enumerated. Each completed recovery trial used the
physical Top+Home reset and returned to the installed iOS through its normal
USB identity. No Linux or Android payload was sent and no storage interface was
present. Inspection then showed that all three variants still took upstream's
pre-platform relocation path through address zero. The fourth physical trial
linked at the actual SHAtter destination, eliminated that relocation, and added
the fixed read-only handoff-state report, but it also disappeared before USB.
Binary inspection then exposed that its first C call still changed the
supervisor stack to `0x4fff7ffc`, outside the only pre-remap region proven by
the SHAtter entry path. The current build keeps its bootstrap heap, 8 KiB ACM
task stack, and supervisor stack beside the loader below the resident SHAtter
shellcode. Its qualifier proves that the flattened image ends before the heap,
that heap and stack reserves do not overlap, and that neither reaches the
shellcode. Two fresh source preparations produced byte-identical ELF, flat
binary, and qualification outputs. Its physical trial still stopped before
USB enumeration. The current isolation build additionally skips pre-USB MIU,
power, GPIO, timer, and UART initialization, retaining only task bookkeeping,
read-only clock discovery, interrupt setup, and the event queue needed by the
USB driver. Two independent preparations of this build are byte-identical; it
has not yet been exercised on the device.

The repository's existing `android-2.0_r1` native root is also integrated
without modifying its bytes. The new N81 staging gate requires its drive-less
full-system emulation report, verifies every packaged runtime file, and proves
that it contains Android init, Binder userspace, Dalvik, Zygote,
SurfaceFlinger, PixelFlinger/software rendering, SystemServer, SettingsProvider,
and the launcher. It rejects block nodes and storage utilities, requires a
read-only root with `/data`, `/cache`, and `/metadata` on tmpfs, and confirms
that gralloc can use a real Linux framebuffer. The N81 kernel now includes
Binder protocol support, Android logger, and low-memory killer for this root;
these additions do not re-enable storage.

The reference build completed with these results:

| Artifact | Size | SHA-256 |
|---|---:|---|
| OpeniBoot ELF | 153,108 bytes | `27bd979d58d166b412b3c20706a18333731d7586a86b182c6361e63edb914170` |
| OpeniBoot flat binary | 122,720 bytes | `651111ecfefc74063aa70b3794801e1bcb5878c51d2d8d30ba3c6e14c2e07271` |
| OpeniBoot unsigned Image3 | 122,752 bytes | `0ca179742085b0a8b498b72461cf00ca1e282656e358f70eb0a541bb5c047874` |
| Narrowed SHAtter shellcode | 352 bytes | `2bc6a31554ba031636acabbd5e9b490eb3bf82a0c0671fbff5306900f7a56119` |
| N81 Linux zImage | 742,820 bytes | `931c89630fd9f8760542bafee8f6cbb343e555b5f792447d68180cec0a11086f` |
| N81 Linux vmlinux | 1,971,796 bytes | `6cbee8ca23c403743e4de5a6e9aead9b535e4b620cf8e703bf57bfa58b2dcc24` |
| N81 kernel config | 22,309 bytes | `b8bf99e5c79462012501914c1aac9bc082626c4b78a2db1f058959bc4e53b7d2` |
| RAM diagnostic `/init` | 2,336 bytes | `073d534ea7f1480199f8b437b71e987558d1bda7b59f9ba068d926976759b964` |
| RAM diagnostic initramfs | 1,254 bytes | `982f80cbfb60d02ba79c2055f9de06fb7c383d2b580804298cc6baac38bd3b7b` |
| Android 2.0 Eclair initramfs | 13,842,301 bytes | `2e8134f8b496ea1d8c93523f364664abdf26c8defdccfa67ce72756e543cd10d` |

These hashes prove the host build inspected in this milestone. They are not a
device qualification and may change after reviewed source changes.

## Required stages to Android

1. Run only the in-place volatile OpeniBoot image and require the exact USB
   `version` marker plus the bounded `n81state` report through the fixed-command
   liveness probe. Do not upload a kernel at this stage.
2. Force reset and prove another return to iOS before loading Linux.
3. Run the time-bounded RAM-only Linux diagnostic. Require framebuffer output,
   USB logs, memory stress, controlled reset, and repeatable return to iOS.
4. Boot the now host-qualified Eclair root only after the diagnostic gate.
   Qualify Android init, Binder, Zygote, SurfaceFlinger software rendering,
   launcher, and framebuffer output on N81 before adding input.
5. Add touchscreen and buttons, then qualify USB diagnostics, memory pressure,
   suspend/reset, and repeated cold recovery.
6. Only after those gates, decide whether tethered volatile dual boot is the
   final product. Any persistent design needs its own boot-chain, APFS/HFS,
   backup, rollback, and NAND-safety specification and explicit approval.

## Non-claims and hard stops

- Android 2.0 passed drive-less host full-system emulation, but no Android or
  Linux image has been booted on the physical Touch 4G. Five earlier loader
  images executed far enough to leave pwned DFU but never exposed OpeniBoot USB.
- The continuity-bound DFU and no-NOR SHAtter paths have been physically
  exercised. No recovery firmware payload was used and no device storage was
  changed.
- The loader is wrapped and both USB transports are implemented. PyUSB 1.3.1
  is wheel-hash-pinned for an isolated transport environment. The fixed
  OpeniBoot `version` probe is implemented, but its physical USB response is
  not yet qualified.
- Display, touch, USB behavior after handoff, power, audio, wireless, camera,
  and Android framework startup remain untested.
- OpeniBoot's A4 NAND implementation must remain excluded. A request to enable
  it is a new, high-risk milestone, not a build option for this profile.
- A host build passing does not authorize physical testing. The timeout/reset
  path, target-continuity session, buttons, cable, backup, and explicit user
  approval must all be checked first.
