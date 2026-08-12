# Uxn/Varvara runtime for iPod

Status: implemented for the 320×240 iPod Video 5G/5.5G and iPod
Classic 6G/7G Rockbox targets, plus their simulators.

## Goal and compatibility baseline

The `uxn.rock` viewer runs standard `.rom` Uxn programs with the Varvara
device model. The VM and device semantics are based on the final maintained
C snapshot in the official Uxn repository,
`156ef01226c069ad5930c2655a10f22380a387a8` (2025-11-12). The interpreter is
qualified with that repository's historical opcode test ROM. Source-level
license and provenance are retained in `apps/plugins/uxn/LICENSE.uxn` and
`tests/uxn/README.md`.

## Runtime architecture

- The VM exposes 16 contiguous 64 KiB banks. Programs begin at `0x0100` and
  ROMs may occupy the remaining 1,048,320 bytes.
- The two 256-byte stacks and 256-byte device page preserve Uxn's wrapping
  byte semantics.
- The System expansion port supports bank fill, forward copy, and reverse
  copy across all 16 banks.
- The runtime owns one allocation from the Rockbox plugin buffer. It contains
  VM RAM, two screen layers, and the 320×240 presentation buffer. It never
  borrows or resizes the playback buffer.
- The event loop schedules the Screen vector at 60 Hz using a fractional
  Rockbox-tick accumulator and dispatches device vectors synchronously.

## Varvara devices

| Address | Device | iPod implementation |
| --- | --- | --- |
| `0x00` | System | Expansion, stack pointers, palette, debug, metadata registers, halt |
| `0x10` | Console | Output/error lines go to log/debug builds when available; no interactive stdin |
| `0x20` | Screen | Pixel, fill, 1bpp/2bpp sprite, auto and palette operations |
| `0x30`–`0x60` | Audio 0–3 | Four PCM/synth voices, ADSR, pitch, loop, VU, position and completion vectors |
| `0x80` | Controller | Buttons, key register and vector |
| `0x90` | Mouse | Position, three buttons, scroll and vector through pointer mode |
| `0xa0`, `0xb0` | File 1–2 | Read, write, append, stat, delete and directory enumeration |
| `0xc0` | DateTime | Local RTC fields from Rockbox |

Screen storage is bounded at 640×480. A ROM-requested size outside 8×8 to
640×480 is clamped. Images at or below 320×240 are centered without scaling;
larger images are downscaled with aspect ratio preserved. This cap is the one
intentional desktop-Varvara compatibility limit and keeps the runtime within
the 3 MiB plugin arena on both supported iPod families.

Both file devices are rooted at `/.rockbox/uxn/data`. Absolute-looking names
are interpreted relative to that root, and `.` or `..` path components are
rejected. Programs cannot access music, settings, firmware, or another ROM's
host path through the Varvara file ports.

## Audio ownership and lifecycle

Varvara audio uses `PCM_MIXER_CHAN_PLAYBACK` with a 512-frame, plugin-owned
stereo buffer at 44.1 kHz. Entry stops only that mixer channel, enables low
latency, records the prior mixer frequency, and begins the callback. Exit
first stops the callback, disables low latency and fade state, restores the
prior mixer frequency, and only then releases VM memory. Voice completion is
latched by the callback and evaluated on the plugin thread, so Uxn code never
runs in the PCM callback. The plugin does not stop or mutate playlists.

## Click-wheel controls

Controller mode is the default:

- Menu / Play: Up / Down
- Previous / Next: Left / Right
- Select: A
- wheel counter-clockwise / clockwise: B / Start pulses
- Select + Menu: Select pulse
- Select + Play: toggle pointer mode
- Menu + Play: exit the runtime

Pointer mode maps Previous/Next/Menu/Play to pointer movement, Select to the
left mouse button, and the wheel to vertical scroll. Select+Play returns to
controller mode. Chords depend on the normal Rockbox combined-button events;
ROMs should not reserve Menu+Play for gameplay.

## Packaging and use

The build registers `rom,viewers/uxn,6`, so selecting a `.rom` file opens the
runtime. Starting `uxn.rock` directly opens a browser rooted at `/Uxn` and
filters for `.rom` files.

Install the bundled screen demo and any personal ROMs on a mounted iPod or
simulator disk with:

```sh
tools/uxn_install_roms.sh /path/to/mount game.rom another.rom
```

The helper only creates `/Uxn`, checks the Uxn ROM size limit, copies the
specified ROMs, installs the source-controlled demo, and syncs the volume. It
does not deploy firmware or replace `.rockbox`.

## Qualification

Automated checks are:

```sh
tools/uxn_regression.py
tools/uxn_sim_gate.py
make -C build-hw-ipod6g \
  /absolute/path/to/build-hw-ipod6g/apps/plugins/uxn/uxn.rock -j4
make -C build-hw-ipodvideo-5g-armelf \
  /absolute/path/to/build-hw-ipodvideo-5g-armelf/apps/plugins/uxn/uxn.rock -j4
```

Before calling hardware qualification complete, run representative ROMs for
each device class and verify these transitions on an iPod:

1. stopped playback → ROM → exit;
2. active playback → ROM → exit → resume playback;
3. an audio ROM → a silent ROM → exit;
4. controller and pointer modes, including release events and exit chord;
5. file create/read/append/stat/delete under `/.rockbox/uxn/data`;
6. clean USB/power event exit with no callback after teardown.

No physical-device deployment is part of this port change.
