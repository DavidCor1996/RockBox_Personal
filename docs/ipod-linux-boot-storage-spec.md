# iPod Linux Boot Storage Spec

## Goal

Add a managed Linux VM payload to the iPod disk so the iPod can carry a
portable desktop Linux environment for a PC or Mac, while keeping Rockbox usable
on the same device.

The initial hardware/storage target is an 80 GB iPod hard drive. RockPod must
budget Linux, Rockbox, media, and free space against that device size instead of
assuming a larger Classic/iFlash layout.

The user-facing idea is:

- RockPod installs and updates the Linux VM payload;
- RockPod shows Linux as its own storage segment in the sync capacity meter;
- a launcher under `/Linux/RockPodVM/` presents the action as `Start Linux`;
- booted Linux can mount and browse the Rockbox root filesystem.

## Recommendation

Implement this first as a portable QEMU VM stored on the iPod, not as Linux
running on the iPod CPU and not as an iPod-booted OS.

This distinction matters. When the iPod is plugged into a laptop, the laptop is
the USB host and the iPod is normally a USB device. The iPod cannot take over
the laptop's display, keyboard, and mouse as if they were local peripherals.
Those peripherals are available naturally to a VM running on the PC/Mac.

The first milestone should therefore be:

1. RockPod installs a verified Debian live ISO, launchers, README, and manifest
   under `/Linux/RockPodVM/`.
2. Rockbox and the iPod bootloader ignore those files completely.
3. The user runs `start-linux-linux.sh`, `start-linux-windows.bat`, or
   `start-linux-macos.command` from the iPod on the host computer.
4. Linux can mount the Rockbox/iPod data volume at `/mnt/ipod` so `.rockbox`,
   `Music`, playlists, videos, games, and other root files are accessible.

## Non-Goals

- Do not try to present the iPod as an external GPU, keyboard host, or mouse
  host to the laptop.
- Do not promise that Rockbox can force a running laptop OS to reboot into the
  Linux payload.
- Do not add a Rockbox root menu item, plugin, shortcut, bootloader branch, or
  iPod-side UI for Linux.
- Do not make Rockbox check for, parse, validate, boot, hide, delete, or
  otherwise understand `/EFI`, `/Linux`, `.rockpod-linux`, or RockPod Linux
  manifests.
- Do not require repartitioning in the first milestone.
- Do not run a full desktop Linux on the iPod's ARM SoC for this feature.
- Do not replace Rockbox USB mass storage, HID, or audio behavior.

## Architecture Options

### Option A: Portable VM From Existing iPod Volume

This is the preferred first implementation.

Store a QEMU VM layout on the existing iPod data volume:

```text
/Linux/RockPodVM/
    debian-live-13.5.0-amd64-xfce.iso
    rockpod-linux-data.vmdk
    rockpod-linux-data-s001.vmdk
    rockpod-linux-data-s002.vmdk
    ...
    start-linux-linux.sh
    start-linux-linux.command
    start-linux-windows.bat
    start-linux-macos.command
    README.txt
    manifest.json
```

Behavior:

- Rockbox continues to boot normally from `/.rockbox`.
- Rockbox does not know Linux is present. The payload files are plain files on
  the exported storage volume.
- QEMU runs the Linux VM on the host PC or Mac without rebooting the host.
- The VM opens in a desktop window and uses the host display, keyboard, and
  mouse through QEMU.
- The iPod data volume can be shared into Linux at `/mnt/ipod`.

Advantages:

- matches the desired laptop display/input behavior;
- avoids iPod SoC Linux kernel work;
- works with existing Rockbox storage exposure;
- preserves the iPod as a normal Rockbox player.

Risks:

- Host must have QEMU installed.
- Apple Silicon runs the current x86_64 Debian live image through emulation, so
  it is compatible but slower than a future native arm64 VM image.
- The first VM session is live-desktop based. Persistent user changes require
  using the capped split VMDK data disk or a future customized image.
- The iPod data volume is often FAT/vfat, so the VM disk must not be stored as
  one file larger than 4 GiB. RockPod uses QEMU's `twoGbMaxExtentSparse` VMDK
  layout to keep each extent under that limit.

### Option B: Laptop-Boot Linux From Existing iPod Volume

This is optional later work. It would write `/EFI/BOOT` and require rebooting
the PC into the iPod. Do not implement until the VM path is stable.

### Option C: Separate Linux Partition

Create a dedicated Linux boot/root partition on the iPod disk.

This is not recommended for the first milestone because repartitioning an iPod
is higher risk and can break Apple firmware, Rockbox boot assumptions, or large
disk/iFlash layouts. It may become useful later if UEFI boot from the existing
FAT volume is unreliable.

### Option D: Native Linux On iPod

Boot an ARM Linux kernel on the iPod itself.

This is a separate research project. It would need target-specific kernel,
storage, LCD, clickwheel, power, and USB work. It also would not automatically
use a plugged-in laptop's display, keyboard, and mouse. At best it could expose
a network or USB gadget service and require a client on the laptop.

## Linux Payload

Target the laptop, not the iPod:

- architecture: `x86_64`;
- runtime: QEMU portable VM launched from the mounted iPod;
- desktop: Debian Live Xfce;
- base image: verified Debian live ISO;
- persistence/data: optional grow-on-demand split VMDK data disk capped by the
  selected allocation;
- target drive size: 80 GB total iPod storage, shared with Rockbox and media;
- Linux allocation cap: maximum 80 GB; default 16 GB;
- mount point for the iPod/Rockbox root inside the guest: `/mnt/ipod`;
- default user: passwordless local desktop user for offline utility use;
- host requirement: QEMU installed on the PC or Mac.

Recommended distro base for first prototype:

- Debian 13 `trixie` stable, using the official Debian Live Xfce image.
- Xfce desktop by default.
- Include Debian `main`, `non-free-firmware`, and only the minimum packages
  needed for common laptop boot, Wi-Fi, display, storage, and a file manager.

Rationale:

- Debian stable is the safest base for a bootable utility OS because it has a
  long maintenance window, broad laptop hardware support, standard package
  tooling, and official live-build support.
- Xfce is light enough for a USB live system but still gives users familiar
  settings, panels, themes, window-manager customization, display controls, and
  Thunar file-manager integration.
- Alpine should remain a later ultra-small experiment. It is attractive for
  image size, but the first milestone should prioritize boot success on random
  laptops over shaving hundreds of megabytes.

RockPod should treat the VM payload as a versioned bundle:

```json
{
  "schema": 1,
  "mode": "portable-vm",
  "name": "RockPod Linux VM",
  "version": "0.1",
  "arch": "x86_64",
  "desktop": "xfce",
  "allocation_gb": 80,
  "files": [
    {"path": "/Linux/RockPodVM/debian-live-13.5.0-amd64-xfce.iso", "sha256": "...", "size": 0},
    {"path": "/Linux/RockPodVM/start-linux-linux.sh", "sha256": "...", "size": 0},
    {"path": "/Linux/RockPodVM/start-linux-windows.bat", "sha256": "...", "size": 0},
    {"path": "/Linux/RockPodVM/start-linux-macos.command", "sha256": "...", "size": 0}
  ],
  "installed_at": "",
  "rockbox_mount": "/mnt/ipod"
}
```

## Rockbox Isolation

Rockbox and the iPod boot path must remain unaware of the Linux payload.

Required invariants:

- No Rockbox root menu entry named `Start Linux`.
- No Rockbox plugin named `linux_boot.rock`.
- No bootloader check for Linux files.
- No firmware or app-layer reference to `/Linux/RockPodVM`, `.rockpod-linux`,
  the Debian ISO, `rockpod-linux-data.vmdk`, split VMDK extents, or
  `linux_system`.
- No change to stock boot, Rockbox boot, hold-button boot, disk mode, or
  Rockbox USB behavior.
- Linux files may only be classified by RockPod while the iPod is mounted on a
  PC.

From the iPod's perspective, Linux is just unused disk content. Starting Linux
is a PC-side action performed by running the launcher from the mounted iPod.

## RockPod Integration

RockPod owns installation, updates, deletion, and storage accounting for Linux.

### Device Settings

Add a Linux section to the device settings or boot workflows:

- `Start Linux on this PC`;
- `Install Linux desktop`;
- `Update Linux payload`;
- `Remove Linux payload`;
- allocation selector capped at 80 GB;
- installed size display;
- `Uninstall Linux`;
- host support status: QEMU present/missing, installed/missing.

### Sync Planning

The sync planner must reserve Linux bytes before copying music, video, themes,
games, photos, or generated Rockbox assets.

When Linux is selected for install or update, the sync confirmation dialog
should include it as a planned operation:

```text
Linux: install RockPod Linux VM, 3.6 GB payload, 80 GB cap
```

Capacity checks must fail before copying if:

- Linux install plus pending sync exceeds available free space;
- the selected allocation exceeds 80 GB;
- the VM payload is larger than the selected allocation.

### Storage Meter

RockPod's device capacity meter currently has segments for:

- Music;
- Games;
- Themes;
- System;
- Other;
- Free.

Add a first-class Linux segment:

```text
Linux
```

Suggested storage key:

```text
linux_system
```

Paths classified as Linux:

```text
/Linux/
/.rockpod-linux/
/.rockpod-linux/
```

For the VM implementation, RockPod should classify `/Linux/RockPodVM/` as
Linux. Generic user Linux folders outside RockPod's manifest may still be
classified as Linux for capacity visibility, but uninstall must remove only
manifest-owned VM files.

The sync screen capacity meter should show both current and pending Linux use:

- current installed Linux bytes from device scan;
- pending install/update delta from the sync plan;
- free space after sync;
- warning if Linux reduces the media budget below the selected sync set.

Implementation touch points:

- `rockpod/services/device_storage.py`: add `linux_system` category and path
  classifier.
- `rockpod/ui/storage_bar.py`: add `("linux_system", "Linux")` to segment order
  and assign a distinct color.
- `rockpod/ui/dialogs/sync_dialog.py`: include Linux install/update/delete in
  sync summary and pending capacity preview.
- `rockpod/ui/main_window.py`: include `linux_system` in cached storage totals.
- `rockpod/tests/test_device_storage.py`: assert Linux paths are categorized
  separately from Rockbox system and Other.

## Filesystem Access From Linux

When the VM is running, the Rockbox volume can be mounted at:

```text
/mnt/ipod
```

The desktop should include a file manager shortcut named:

```text
Rockbox Root
```

Rules:

- The launcher exposes the iPod root to QEMU as a 9p share named
  `rockbox_root` where the host supports it.
- Do not run Rockbox and laptop Linux against the same mounted filesystem at the
  same time.
- Include a desktop eject/shutdown action that syncs writes before poweroff.

## Boot Flow

### Install

1. User connects iPod to RockPod.
2. RockPod detects Rockbox root.
3. User enables Linux payload.
4. RockPod downloads or uses a local payload bundle.
5. RockPod verifies hashes.
6. RockPod writes `/Linux/RockPodVM`.
7. RockPod updates the capacity meter with Linux as its own segment.
8. RockPod leaves Rockbox files and media intact.

### Start From PC

1. User connects the iPod to a PC or laptop.
2. Rockbox exposes normal USB mass storage, or the user uses Apple/Rockbox disk
   mode as they already would for file access.
3. User runs the matching launcher from `/Linux/RockPodVM`.
4. QEMU opens Debian Live Xfce in a desktop window.

### Return To Rockbox

1. User shuts down Linux cleanly.
2. User disconnects the iPod.
3. iPod remains in normal Rockbox USB behavior, reboots, or exits disk mode
   exactly as it would without Linux files present.
4. Rockbox storage scan sees any files changed under the shared root.

## Safety Requirements

- RockPod must record ownership in the Linux manifest before later repair or
  removal.
- Removal must delete only manifest-owned files.
- Sync must not overwrite `.rockbox`, music, playlists, games, photos, or videos
  while installing Linux.
- Linux persistence should be optional and bounded by a user-selected size.
- The Linux desktop should not auto-index or rewrite the whole iPod media
  library on first boot.
- All writes must be flushed before eject or shutdown.
- Add a source-level regression test that fails if Rockbox or bootloader source
  starts referencing RockPod Linux payload paths or names.

## Milestones

### M1: Spec And RockPod Accounting

- Add `linux_system` storage category.
- Add Linux segment to the storage bar.
- Add tests for `/Linux/RockPodVM` and RockPod-owned files.
- Add sync-dialog copy text and capacity math for a fake Linux payload.

### M2: Payload Installer

- Add a RockPod Linux bundle manifest.
- Install payload from local bundle.
- Verify hashes after copy.
- Support remove/repair.
- Keep a backup of pre-existing boot files.

### M3: PC-Side Start Linux Workflow

- Add `Start Linux on this PC` to RockPod, not Rockbox.
- Confirm the Linux payload is installed.
- Tell the user whether QEMU is installed and which launcher to run.
- Leave iPod boot and Rockbox boot untouched.

### M4: First VM Desktop

- Install the verified Debian Live Xfce ISO and launchers.
- Start the VM on a Linux host with QEMU.
- Mount the Rockbox root at `/mnt/ipod`.
- Verify clean shutdown and return to Rockbox.

### M5: Compatibility

- Add native arm64 Apple Silicon image support.
- Test macOS launcher on M1 with QEMU installed.
- Validate iFlash and large-disk layouts.

## Open Questions

- Should RockPod add native arm64 Debian cloud image support for M1/M2/M3 Macs?
- Should persistence be a data disk only, or should RockPod generate a custom
  persistent Debian desktop image?
- Should RockPod bundle QEMU binaries for Windows/macOS or require user install?
