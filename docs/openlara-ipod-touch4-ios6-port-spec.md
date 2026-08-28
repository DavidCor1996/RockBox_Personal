# OpenLara for iPod touch 4G / iOS 6

Status: full-engine source and installable package implemented on 2026-08-12;
physical-device qualification is pending because the iPod is not mounted.

## Product contract

OpenLara Touch is a standalone landscape jailbreak application for `iPod4,1`
running iOS 6.1.6. The deliverable uses the complete upstream OpenLara engine,
not the separate four-level fixed-engine alpha used by the Rockbox plugin. Its
TR1 gameflow includes Lara's Home, all fifteen main levels, four cutscene level
files, and the ending files.

No copyrighted Tomb Raider data is embedded in the source or package. The
owner supplies the `DATA` directory from a legally obtained Tomb Raider I PC
installation. Optional soundtrack replacements and supported FMVs can be
copied alongside it.

The imported engine source is pinned to official OpenLara commit
`8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea` and retains its BSD-2-Clause
license under `tools/ios/OpenLaraTouch/UpstreamFull`.

## Platform decisions

- Build a single `armv7` Mach-O with an iOS 6.0 minimum using the iPhoneOS 9.3
  SDK. No arm64 or post-iOS-6 controller framework is linked.
- Use the upstream OpenGL ES 2 renderer through `GLKViewController`. GLKit and
  OpenGL ES 2 are available on iOS 6 and the iPod touch 4G GPU.
- Set the GL view's content scale to 1.0, producing a 480x320 landscape
  framebuffer instead of a 960x640 Retina framebuffer. This is an intentional
  A4 memory/bandwidth limit; UIKit controls remain laid out in points.
- Request 30 updates/renders per second. The in-app status chip reports
  measured frames per second rather than merely echoing the requested rate.
- Default to medium filtering and low lighting, shadows, and water; disable
  reverb and cubemap mip generation. Saved user quality settings can override
  the defaults after the first run.
- Feed the upstream stereo 44.1 kHz mixer from three reusable `AudioQueue`
  buffers. Pause audio and rendering while inactive, clear held input, and stop
  the queue before destroying engine state.
- Store shader/settings cache and save slots in writable data-root
  subdirectories. The app never changes the iOS music library or another
  process's playlist/audio allocation.

## Data layout

The `.deb` application under `/Applications` prefers the mobile media path. A
sandbox/IPA build may use Documents instead. When both exist, the root with
more valid TR1 level files wins.

```text
/var/mobile/Media/OpenLara/              preferred jailbreak path
<application Documents>/OpenLara/        sandbox/IPA fallback

    DATA/
        TITLE.PHD
        GYM.PHD
        LEVEL1.PHD ... END2.PHD          all 25 TR1 PHD files required
        TITLEH.PCX                       optional original title artwork
    audio/                               optional OGG/WAV soundtrack layout
    FMV/                                 optional supported cutscene files
    cache/settings                       generated settings cache
    saves/savegame.dat                   generated save slots
    last-run.log                         generated FPS/audio diagnostics
```

The frontend verifies every required file is present and begins with the
little-endian TR1 PC PHD magic `0x20` before initializing the engine.

Prepare a normalized, checksummed copy before transfer:

```bash
tools/openlara_touch_stage.py /path/to/TR1 /tmp/OpenLara
```

The staging tool accepts either the installation root or its `DATA` directory,
requires the complete TR1 PC campaign, normalizes `DATA` and `FMV` names for
the case-sensitive iOS filesystem, preserves the optional `audio` hierarchy,
rejects symbolic links/collisions, emits SHA-256 values in `manifest.json`, and
refuses to overwrite an existing output. It never downloads or redistributes
game data.

## On-screen controls

The labeled overlay is sized for the 3.5-inch display and recomputes all active
touches as a set, allowing direction/action combinations without stuck keys.

| Control | OpenLara gamepad input | Tomb Raider action |
|---|---:|---|
| directional pad | D-pad | move/turn; diagonals supported |
| ACT | A | action, grab, fire when weapons are ready |
| JUMP | X | jump/back in inventory |
| GUN | Y | draw/holster weapons |
| WALK | right bumper | walk modifier |
| LOOK | left bumper | look modifier |
| ROLL | B | roll |
| INV | Select | inventory/pause |

Backgrounding clears touch and hardware state before rendering/audio pause.
This prevents a resign-active event from preserving a held movement key.
Hardware state also has a two-second dead-man timeout; normal HID key repeat
refreshes held inputs, while a controller disconnect cannot leave motion stuck
indefinitely.

## Bluetooth controller choices on iOS 6

Apple's public MFi `GameController` framework is not available on iOS 6. The
binary therefore implements iCade's Bluetooth HID keyboard make/break protocol
through `UIKeyInput` and does not import `GameController.framework`.

The eight iCade buttons map to action, jump, weapons, walk, look, Start, roll,
and inventory. Receiving iCade traffic reports the active controller and dims
the touch artwork; the next screen touch restores it.

Practical controller options are:

1. Pair an iCade or iCade-compatible HID controller directly in iOS Settings.
2. Use a jailbreak mapper such as Blutrol to map a conventional Bluetooth pad
   to the visible touch regions.

Modern MFi-only pads are not claimed to pair with iOS 6.

## Build and package

```bash
cd tools/ios/OpenLaraTouch
THEOS=/home/david/theos make clean package FINALPACKAGE=1
```

Output:

```text
tools/ios/OpenLaraTouch/packages/com.rockpod.openlara_0.2.4_iphoneos-arm.deb
```

The package installs `/Applications/OpenLaraTouch.app`. Before deployment its
Mach-O must report armv7, `LC_VERSION_MIN_IPHONEOS 6.0`, OpenGLES/GLKit imports,
and no GameController import.

## Qualification gates

### Host/build gate

- complete upstream engine/gameflow and its OpenGL ES renderer compile into the
  app with the OGG and inflate sources (the LGPL MP3 decoder is deliberately
  excluded from the statically linked package; use OGG or WAV tracks);
- package contains no game data and produces an armv7/iOS 6.0 Mach-O;
- package contains declared 29/57 px and Retina 58/114 px original app icons,
  512 px installation artwork, and the upstream license notice;
- dylib inspection proves OpenGLES/GLKit are linked and GameController is not;
- host C tests exercise every iCade make/break pair, simultaneous keys, all
  labeled touch regions, D-pad diagonals, and Start/Select separation;
- staging tests exercise all 25 PHDs, magic validation, case normalization,
  optional audio/FMV transfer, SHA-256 manifesting, and overwrite refusal;
- lifecycle inspection covers audio queue stop-before-engine teardown, input
  clearing, writable cache/save paths, and background timing exclusion.

### Physical iPod touch 4G gate (pending)

- install and launch on iOS 6.1.6 without a missing-symbol crash;
- verify the no-data screen in both landscape orientations;
- stage/copy a complete legal TR1 PC dataset and boot the title;
- start Lara's Home and every campaign/cutscene transition through `END2`;
- verify every labeled control and two-finger direction/action combinations;
- pair iCade, verify every make/break pair and background/disconnect behavior;
- test one available conventional Bluetooth pad through Blutrol if available;
- verify effects, optional soundtrack audio, mute/ringer behavior, and absence
  of static or repeated underruns;
- background/foreground repeatedly and verify rendering/audio recovery;
- save, terminate, relaunch, and load a playable slot;
- record FPS and memory warnings in Lara's Home, Caves, Palace Midas, Cistern,
  Atlantis, and The Great Pyramid;
- play for at least 20 minutes and transition repeatedly without unbounded
  memory growth.

The port is not complete until the physical matrix passes. If 480x320 at 30 Hz
is not sustainable on A4, first profile shader/texture pressure and lower
upstream detail defaults; do not remove campaign levels, controls, audio
lifecycle safety, or input correctness to obtain a superficial pass.
