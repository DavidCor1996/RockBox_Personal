# Super Mario 64 iPod 6G Port Status

Last updated: 2026-07-14

Status: **shelved, experimental, and not production-ready**

## Decision

The current port can run correctly in the Rockbox simulator, but it does not
provide a usable experience on an iPod Classic 6G. Hardware testing progressed
from an immediate black screen to a partial, very slow Nintendo logo followed
by another black screen or hard lock.

Simulator-equivalent or native-speed performance is not considered a realistic
goal with the current architecture. The iPod 6G target provides a 216 MHz
S5L8702/ARM926-class CPU and no graphics accelerator usable by this port. SM64's
PC display-list translation and software rasterizer therefore run entirely on
the CPU, including substantial floating-point work on a target without a
hardware FPU.

A future proof-of-concept may be possible at reduced resolution, frame rate,
and visual fidelity, but it would require a major renderer and scheduling
effort. Do not treat the current simulator result as evidence of hardware
feasibility.

## What is implemented

- Native SM64 engine derived from the `dos` branch of `fgsfdsfgs/sm64-port`,
  pinned as described in `apps/plugins/sm64/UPSTREAM.txt`.
- Legal asset preparation from an externally supplied, owned US v1.0 ROM.
  No ROM is committed or included in `rockbox.zip`.
- N64 system and SM64 cover art integration in Games Cover Flow.
- Hardware loader plus large overlay packaging (`sm64.rock` and `sm64.ovl`).
- TLSF arena allocation from Rockbox's shared plugin audio buffer.
- 32-bit-safe generated animation and audio tables for the ARM target.
- A 160x120 software-rendered framebuffer scaled 2x to the 320x240 LCD.
- PCM mixer playback at 32 kHz with a ring buffer and lifecycle cleanup.
- Click-wheel analog direction mapping and iPod-oriented controls:
  - wheel position: analog stick
  - Select: A
  - Play: B
  - Left: Z
  - Right: R
  - Menu: Start
  - wheel scroll events: C-left/C-right
  - Hold transition: exit
- Haptic feedback on new physical button presses.
- CPU boost held from engine startup through cleanup. On iPod 6G this requests
  the target maximum of 216 MHz.
- Simulator autoplay, framebuffer validation, save validation, package checks,
  ROM-leak checks, and a static regression gate.

## Hardware-only failures found and fixed

### Host-generated pointer width

Some generated audio tables inherited the simulator host's 64-bit pointer
layout. Native 32-bit tables are now generated under
`apps/plugins/sm64/upstream/build/us_rockbox32`.

### LCD presentation

The frontend previously confused `lcd_set_viewport()`'s returned previous
viewport with the newly selected main viewport. Presentation now reads the
current main viewport and uses a staging framebuffer plus Rockbox's LCD
blitter.

### Unsafe display-list skipping

The initial overrun strategy skipped complete display lists. This could discard
render state as well as pixels and was removed. Every engine frame currently
submits its display list.

### Per-pixel software floating point and division

The hottest rasterizer interpolation and color-combiner path was moved to
Q16.16 fixed point. Perspective attributes are corrected per vertex rather
than divided per pixel. This removed software floating-point and integer
division from the covered-pixel loop, but triangle setup and much of the game
still use software floating point.

### Native stack overflow

This was the first proven simulator-versus-hardware correctness difference.
Rockbox gives the native plugin main thread an 8 KiB stack, while the simulator
inherits a much larger host stack.

The following allocations exceeded or nearly exhausted the native stack:

- 4.25 KiB per-frame audio staging buffer
- eight texture conversion paths with 8-32 KiB local buffers
- 32 KiB Goddard OBJ parser arrays
- 4 KiB Goddard grid workspace
- a recursively inlined joint routine with a compiler-generated 4.3 KiB frame

The audio, texture, parser, and grid workspaces now use persistent storage. The
recursive joint routine is marked `noinline`. `-Wstack-usage=2048` is enabled
for SM64 sources, and the forced ARM audit completed without a warning above
that threshold.

Fixing these overflows changed hardware behavior from a fully black launch to
rendering part of the Nintendo logo. It did not make the game usable.

## Current hardware result

Latest observed behavior:

1. The overlay loads and engine initialization completes.
2. CPU boost is requested.
3. Hardware begins rendering the Nintendo logo.
4. The logo is visibly much slower than the simulator.
5. The display becomes black and does not recover normally.

The last pre-telemetry hardware log reached:

```text
init game state ready
first frame begin
first frame ready
```

A later diagnostic build kept the log descriptor open and recorded selected
frame durations. The subsequent hard failure left `sm64.log` at zero bytes, so
it did not produce usable post-logo timing data. This telemetry method should
not be considered crash-safe on FAT.

The exact post-logo failure is therefore unresolved. It may include another
correctness bug, but the severe performance gap exists independently and is a
fundamental blocker for the requested quality bar.

## Test results

The following tests passed before shelving:

- `tools/sm64_regression.py`
- 30-frame simulator launch with persistent frame telemetry
- 180-frame simulator launch/render/save/exit gate
- 1800-frame simulator autoplay through castle-ground gameplay after the stack
  fixes, with all 1800 frames submitted and a clean exit
- forced ARM compilation with the 2 KiB stack-usage warning enabled
- hardware loader/overlay checksum verification after targeted deployments
- pre/post device database checksum comparison after each targeted deployment

The simulator is a functional correctness gate only. It does not model the
ARM926 execution rate, absence of hardware floating point, native 8 KiB stack,
or LCD/storage timing accurately enough to be a performance gate.

## Build and test commands

Prepare ignored build assets from the exact owned US v1.0 ROM:

```sh
tools/sm64_prepare_assets.py "/path/to/Super Mario 64 (USA).z64"
```

Accepted ROM SHA-1:

```text
9bef1128717f958171a4afac3ed78ee2bb4e86ce
```

Regression and long simulator gate:

```sh
tools/sm64_regression.py
SM64_TEST_FRAMES=1800 tools/sm64_sim_gate.sh
```

Important simulator build detail: the full simulator plugin is the nested
target below. `build-sim-ipod6g/apps/plugins/sm64.rock` is not the full SM64
game binary used by the gate.

```sh
make -C build-sim-ipod6g \
  /home/david/Documents/RockBox_Personal-master/build-sim-ipod6g/apps/plugins/sm64/sm64.rock
```

Hardware plugin targets:

```sh
make -C build-hw-ipod6g \
  /home/david/Documents/RockBox_Personal-master/build-hw-ipod6g/apps/plugins/sm64.rock \
  /home/david/Documents/RockBox_Personal-master/build-hw-ipod6g/apps/plugins/sm64/sm64.ovl
```

Install personal ROM and covers into a simulator disk or mounted device:

```sh
tools/sm64_install_personal_assets.sh DEVICE_OR_SIM_ROOT \
  "/path/to/Super Mario 64 (USA).z64"
```

## Last deployed diagnostic build

The last targeted physical-iPod deployment updated only the SM64 loader and
overlay. It did not replace firmware or the `.rockbox` directory.

```text
sm64.rock  a5880b8a87faf23aaf1611eb1226364ad4d460692f2c8ad354f90ade7cd6ffd0
sm64.ovl   bc0a9d73637293289b4367d018d05184c26fbaa4583353b6e14b92e5356b58ab
```

Local and device checksums matched, `sync` completed, and all existing
`database*.tcd`/`tagcache*.tcd` checksums were unchanged. This records the last
known deployment; it is not a statement that the build is usable.

## Relevant files

- `apps/plugins/sm64/sm64.make`: source list, flags, stack audit, overlay link
- `apps/plugins/sm64/sm64_rockbox.c`: lifecycle, arena, CPU boost, frame loop
- `apps/plugins/sm64/sm64_audio.c`: PCM mixer and ring buffer
- `apps/plugins/sm64/sm64_input.c`: click-wheel controls, haptics, Hold exit
- `apps/plugins/sm64/sm64_video.c`: 160x120 framebuffer and LCD presentation
- `apps/plugins/sm64/upstream/src/pc/gfx/gfx_pc.c`: display-list translation
  and shared texture conversion buffer
- `apps/plugins/sm64/upstream/src/pc/gfx/gfx_soft.c`: software rasterizer
- `tools/sm64_prepare_assets.py`: owned-ROM asset generation
- `tools/sm64_prepare_audio32.py`: native audio-table conversion
- `tools/sm64_sim_gate.py` and `tools/sm64_sim_gate.sh`: simulator gate
- `tools/sm64_regression.py`: static/build/package regression checks

## If work resumes

First decide whether the target is a low-fidelity technical demonstration or a
smooth stock-iPod-style game. The latter remains infeasible with the current
CPU-only renderer.

For a technical demonstration, the most useful next steps would be:

1. Replace the current file telemetry with a crash-safe hardware timing method
   or an in-memory report that can be displayed after a clean Hold exit.
2. Profile display-list translation, triangle setup, rasterization, audio
   synthesis, and LCD presentation separately on hardware.
3. Convert triangle setup and other dominant ARM soft-float paths to fixed
   point, guided by measurements rather than simulator timing.
4. Add state-preserving render suppression: continue parsing display lists and
   applying state/texture changes on skipped presentation frames, while
   suppressing triangle rasterization and LCD swaps.
5. Evaluate 128x96 or 80x60 rendering only if reduced visual quality is
   acceptable.
6. Re-run the full plugin audio lifecycle matrix from
   `docs/plugin-audio-lifecycle-steering.md` after any audio or threading
   change.
7. Keep `tools/sm64_regression.py`, the 1800-frame simulator gate, the ARM stack
   audit, and physical database checksum verification as mandatory gates.

Do not resume by adding more on-screen markers or assuming that a simulator
pass predicts native performance.
