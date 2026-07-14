# Super Mario 64 native iPod 6G port

## Product specification

SM64 appears in Games Cover Flow under `Consoles > Nintendo 64`. Selecting
the owned `Super Mario 64 (USA).z64` entry launches a dedicated native
Rockbox overlay. The launcher uses an archival US box scan for the game and
the official Nintendo 64 wordmark for the console; neither image is drawn by
the firmware. Source URLs are recorded in `assets/game_covers/n64/SOURCES.tsv`.

The port is isolated from iPodJS and the main-menu navigation stack. A small
launcher plugin loads the 13 MiB game overlay into the shared game/audio
arena, outside Rockbox's 3 MiB plugin buffer. Returning, USB attachment, a
long Menu press, or a new Hold-switch engagement tears down PCM, restores the
previous mixer rate and wheel event mode, releases the shared buffer, and
returns through the normal plugin boundary.

## Click-wheel controls

The touch surface acts as a true 16-direction analog stick. Its angle maps
to N64 stick direction, which preserves gradual diagonal movement without a
hand-drawn on-screen controller.

| iPod control | N64 action |
| --- | --- |
| Touch wheel position | Analog movement |
| Center/Select | A: jump, swim stroke, confirm |
| Play/Pause | B: attack, grab, cancel |
| Left | Z: crouch, ground pound |
| Right | R: camera mode |
| Wheel scroll clockwise/counter-clockwise | C-right/C-left camera |
| Menu | Start/pause |
| Hold Menu | Exit to launcher |
| Engage Hold switch | Immediate safe exit |

New physical button presses use the existing Rockbox haptic preference. The
game does not draw control hints over Nintendo's presentation.

## Performance design

The original 30 Hz game clock is preserved. Rendering uses the upstream
software RDP at 160x120 and performs an exact nearest-neighbor 2x presentation
to the iPod's 320x240 framebuffer. Logic and audio continue at the native
rate; the frontend adaptively skips a presentation frame after an overrun
instead of changing game speed. The CPU is boosted only for the plugin's
lifetime. The simulator test records rendered/skipped counts, but physical
iPod frame pacing must be measured after an explicitly requested device push.

Audio is generated at the engine's native 32 kHz into a bounded 20-block
stereo ring. Playback starts after six blocks, provides a short fade-to-zero
concealment block on underrun, and never mutates the user's playlist.

## Owned-ROM preparation and build

```sh
tools/sm64_prepare_assets.py '/path/to/Super Mario 64 (USA).z64'
make -C build-sim-ipod6g /absolute/path/to/build-sim-ipod6g/apps/plugins/sm64/sm64.rock -j8
make -C build-hw-ipod6g /absolute/path/to/build-hw-ipod6g/apps/plugins/sm64/sm64.ovl -j8
make -C build-hw-ipod6g zip -j8
```

Only the exact US v1.0 SHA-1
`9bef1128717f958171a4afac3ed78ee2bb4e86ce` is accepted. The ROM and generated
assets remain ignored. To stage the personal ROM and covers into a simulator
or mounted-volume root without installing firmware:

```sh
tools/sm64_install_personal_assets.sh build-sim-ipod6g/simdisk '/path/to/rom.z64'
```

Run `tools/sm64_sim_gate.sh` for launch/render/audio/save/exit coverage and
`tools/sm64_regression.py` for integration, package, cover, and no-ROM gates.
Set `SM64_TEST_REALTIME=1 SM64_TEST_FRAMES=300` to run a ten-second pacing
check at the original 30 Hz instead of the gate's unthrottled gameplay run.
