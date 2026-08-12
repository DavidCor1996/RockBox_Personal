# OpenLara for iPod touch 4G / iOS 6

Status: source and installable-package milestone implemented on 2026-08-12;
physical-device qualification remains pending because the iPod is not mounted.

## Product contract

OpenLara Touch is a standalone, landscape-only jailbreak application for
`iPod4,1` running iOS 6.1.6. It reuses the repository's fixed-point OpenLara
engine and its 320x240 indexed software renderer. It does not embed copyrighted
Tomb Raider data. The owner converts a legally obtained Tomb Raider I PC data
set with the upstream OpenLara packer and supplies the resulting files. That
pinned packer is Windows-oriented and expects the complete TR1 level set; it is
not shipped as a misleading four-level Linux utility in this tree.

The first supported campaign slice matches the Rockbox fixed-engine port:
title, Lara's Home, Caves, and City of Vilcabamba. Enabling the remainder of
the campaign requires extending the fixed engine and packer; the iOS wrapper
does not claim that work is complete.

## Platform decisions

- Build one `armv7` application with an iOS 6.0 minimum and the iPhoneOS 9.3
  SDK. The output remains valid for the Cortex-A8 iPod touch 4G.
- Render the engine's 320x240 palette-index frame into explicit RGBA bytes and
  aspect-fit it in the 480x320-point landscape display. Explicit bytes avoid
  red/blue channel reversal on the little-endian ARM framebuffer. This
  preserves engine geometry and leaves narrow pillarbox regions.
- Keep the RGBA buffer, data provider, and color space alive for the screen's
  lifetime. A frame creates only the lightweight image wrapper used by the
  current draw pass; it does not allocate a new framebuffer, provider, color
  space, and `UIImage` thirty times per second.
- Run simulation and rendering at 30 Hz using `CADisplayLink` with a frame
  interval of two.
- Feed the existing 22,050 Hz mixer into a bounded stereo `AudioQueue` ring.
  Audio is paused while the app is inactive and the queue is stopped before
  engine memory is torn down.
- Store settings and saves under the app's sandbox. The app never changes the
  iOS music library or another process's audio playlist.

## Data layout

The jailbreak `.deb` prefers the mobile media directory because a system app
under `/Applications` is not guaranteed an iTunes-managed sandbox. It also
detects a sandbox Documents copy for an IPA-style installation. If both exist,
the root containing more of the four required PKDs wins.

```text
/var/mobile/Media/OpenLara/              preferred jailbreak path
<application Documents>/OpenLara/        sandbox/IPA fallback

    TITLE.PKD       required
    GYM.PKD         required to play Lara's Home
    LEVEL1.PKD      required to play Caves
    LEVEL2.PKD      required to play City of Vilcabamba
    TITLE.SCR       optional, 320x240 or legacy 240x160 indexed image
    TRACKS.AD4      optional fixed-engine music pack
    openlara.cfg    generated settings
    savegame.dat    generated save
    last-run.log    generated performance/audio diagnostic snapshot
```

Files may be copied to the media path with iFunBox or SSH. The app keeps iTunes
File Sharing enabled for a sandboxed installation, where the Documents path is
exposed. It refuses undersized, oversized, structurally invalid, or out-of-range
PKD level headers before passing them to the engine.

Before copying the converted files, validate them and build a normalized,
checksummed staging directory:

```bash
tools/openlara_touch_stage.py /path/to/converted /tmp/OpenLara
```

The output contains uppercase asset names and `manifest.json`. The tool accepts
required PKDs either in the source root or its `levels` subdirectory, checks the
same header bounds as the app, validates optional screen/music containers, and
refuses to overwrite an existing output directory. It does not download,
convert, or redistribute game data.

## On-screen controls

The overlay is deliberately large enough for the 3.5-inch display and accepts
multiple simultaneous touches.

| Control | Engine input | Tomb Raider action |
|---|---:|---|
| directional pad | arrows | move/turn; diagonals supported |
| ACT | A | action, grab, fire when weapons are ready |
| JUMP | B | jump/back in inventory |
| GUN | C | draw/holster weapons |
| WALK | X | walk modifier |
| LOOK | Y | look modifier |
| ROLL | Z | simultaneous forward/back roll command |
| INV | Select | inventory/pause |

Touches are recomputed as a set on every move/end event, preventing a released
finger from leaving a direction or action stuck.

## Bluetooth controller choices on iOS 6

Apple's public MFi `GameController` framework is not available to iOS 6 apps,
so linking it would make the application unloadable on the requested OS. The
port instead implements the complete iCade HID keyboard make/break protocol
through `UIKeyInput`. Direction, action, jump, weapons, walk, look, roll, and
inventory are mapped, and the overlay reports when iCade traffic is received.
Direct iCade input dims the touch artwork for an unobstructed controller view;
the next screen touch restores full opacity.

Supported practical configurations are:

1. An iCade or iCade-compatible Bluetooth HID controller directly paired in
   iOS Settings.
2. A conventional Bluetooth controller mapped to the visible controls by a
   jailbreak utility such as Blutrol. This uses the same on-screen regions and
   requires no private controller framework in OpenLara.

No claim is made that an arbitrary modern MFi-only controller will pair with
iOS 6.

## Build and package

```bash
cd tools/ios/OpenLaraTouch
THEOS=/home/david/theos make clean package FINALPACKAGE=1
```

Expected output:

```text
tools/ios/OpenLaraTouch/packages/
    com.rockpod.openlara_0.1.0_iphoneos-arm.deb
```

The package installs `/Applications/OpenLaraTouch.app`. Its Mach-O must report
`armv7` and `LC_VERSION_MIN_IPHONEOS 6.0` before it is copied to a device.

## Qualification gates

### Host/build gate (implemented)

- fixed engine compiles with the isolated `__IOS__` platform definition;
- app, fixed renderer, sound mixer, UIKit frontend, and compatibility shim
  link into one armv7 Mach-O;
- package builds without copyrighted game data;
- Mach-O minimum OS is 6.0 and imports only iOS-6-era public frameworks;
- static regression confirms touch, iCade, audio, save, and asset-validation
  paths remain wired.
- the host staging test exercises normalized filenames, SHA-256 manifests,
  refusal to overwrite, and rejection of an invalid PKD header;
- host C tests exercise every iCade make/break pair, simultaneous held keys,
  touch hit regions and diagonals, and RGB555-to-RGBA primary colors.
- the in-app status chip reports measured display-link FPS; backgrounding or
  clean teardown writes elapsed frames/FPS, slow engine ticks, worst engine
  tick, audio underruns, and active controller mode to `last-run.log`.

### Physical iPod touch 4G gate (pending)

- install and launch on iOS 6.1.6 without a missing-symbol crash;
- no-data screen is readable in both landscape orientations;
- copied fixed pack boots the title and loads all four enabled levels;
- touch D-pad diagonals and every labeled button work with two fingers;
- iCade connect/disconnect and every make/break pair leave no stuck keys;
- optional Blutrol mapping works with one available Bluetooth pad;
- effects and optional music play without underrun/static;
- Home/background/foreground resumes video and sound without a crash;
- save, terminate, relaunch, and load restore a playable state;
- sustain 30 Hz when possible, record worst level/frame rate and memory warning
  behavior on the A4/256 MB device;
- play for 20 minutes and switch levels repeatedly without unbounded memory
  growth.

The port is not complete until this device matrix is run. If the A4 cannot
hold 30 Hz in Caves, the next optimization order is: eliminate per-frame image
object creation with a persistent Core Graphics surface, profile rasterizer
hot spots, reduce transparent overdraw, then consider a 240x180 internal mode.
Do not reduce simulation frequency or omit controls to make a superficial gate
pass.
