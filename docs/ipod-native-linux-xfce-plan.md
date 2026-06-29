# iPod Native Linux Xfce Plan

## Goal

Build a 5G/5.5G iPod-native RockPod Linux desktop that can run offline on the
iPod while sharing the same iPod storage used by the PC-side RockPod Linux VM.

This is not upstream Xfce. The iPod hardware cannot realistically run the same
Debian/Xfce desktop image used by the laptop VM. The target is an Xfce-inspired
embedded shell with the same product identity, shared folders, and a familiar
desktop metaphor.

## Target

Initial target:

- iPod Video 5G/5.5G (`ipodvideo`)
- PortalPlayer PP5022
- 320x240 LCD
- clickwheel/buttons
- FAT/VFAT iPod data volume
- existing Rockbox bootloader Linux path via `/linux.bin`

Later target:

- iPod Classic 6G/7G (`ipod6g`)
- requires separate kernel/driver research before this desktop can run

## User Model

Two Linux entries exist:

- `Start Linux on this PC`: launches the Debian/Xfce VM from the iPod disk.
- `Start Linux on iPod`: reboots the iPod into native RockPod Linux.

Both environments should see the same shared folders on the iPod disk:

```text
/RockPod/Home/
/RockPod/Desktop/
/RockPod/Documents/
/RockPod/Downloads/
/Music/
/Videos/
/.rockbox/
```

Only one environment should own writable access to the mounted iPod volume at a
time. The PC VM and native iPod Linux must not write the same FAT volume
simultaneously.

## Native Payload Layout

Recommended first layout:

```text
/linux.bin
/boot/vmlinux
/boot/boot.pzm
/Linux/iPodNative/
    manifest.json
    README.txt
    rootfs/
    bin/rockpod-xfce-shell
    share/rockpod-xfce/
        icons/
        wallpaper.raw
        theme.ini
/ZeroSlackr/
```

The exact kernel and rootfs naming can follow the ZeroSlackr/iPodLoader2 assets
that are already staged under `ipodlinux/`.

## Shell Requirements

The first shell should be deliberately small:

- native 320x240 framebuffer UI
- top panel with title, clock, battery placeholder
- desktop icons for Files, Music, Settings, Terminal, Reboot Rockbox
- file browser rooted at `/`
- quick links to shared folders
- safe shutdown/reboot actions
- no background media indexing
- no automatic write-heavy scanning

Visual direction:

- Xfce-like panel + desktop metaphor
- RockPod branding
- readable at 320x240
- clickwheel-first navigation
- no dependency on a mouse pointer for core actions

## Simulator Strategy

Rockbox simulator cannot boot iPodLinux or run the native Linux userspace. It
can still help in two ways:

1. UI parity prototype in Rockbox Desktop Mode.
2. Host build of `rockpod-xfce-shell` against SDL or a stub framebuffer.

The real boot path must be tested on iPod Video 5G hardware because the Linux
kernel, framebuffer, input, storage, power, and reboot behavior are outside the
Rockbox simulator.

## Boot Path

For `ipodvideo`, the existing Rockbox bootloader can load `/linux.bin` when the
Linux boot condition is selected. Product flow should become:

1. User selects `Start Linux on iPod` from Rockbox Desktop Mode or boot menu.
2. Rockbox stops playback and prepares for reboot.
3. Bootloader selects the native Linux target.
4. Linux mounts the iPod data volume.
5. Init launches `rockpod-xfce-shell`.

The first hardware milestone may use the current manual boot gesture before
adding a Rockbox menu/reboot trigger.

## Milestones

### N1: Native Shell Skeleton

- Add isolated `ipodlinux/rockpod-xfce/` source.
- Draw a fixed 320x240 desktop mockup.
- Define shared-folder paths and launcher names.
- Buildable later as either host SDL or iPodLinux framebuffer binary.

### N2: Host/Simulator Preview

- Add a host preview backend.
- Match the Rockbox Desktop Mode visual language enough for design iteration.
- Keep this separate from real native boot claims.

### N3: 5G Payload Assembly

- Package ZeroSlackr/iPodLinux kernel/rootfs assets.
- Install `rockpod-xfce-shell` as the default shell.
- Keep `/linux.bin` and boot assets manifest-owned.

### N4: Hardware Boot

- Boot 5G into native Linux.
- Confirm LCD, buttons, disk mount, shutdown, and return-to-Rockbox path.
- Only after this milestone add a user-facing `Start Linux on iPod` item.

### N5: Shared Storage UX

- Add visible `RockPod Home` and `Rockbox Root` entries.
- Add filesystem safety prompts.
- Add sync/repair behavior from RockPod.

## Non-goals For First 5G Pass

- Real Debian/Xfce on iPod hardware.
- Running the same live VM session offline.
- Classic 6G support.
- Concurrent writable mounts from PC and iPod.
- Replacing Rockbox as the default safe recovery path.
