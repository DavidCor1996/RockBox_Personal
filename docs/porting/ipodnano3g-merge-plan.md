# iPod Nano 3G Port Merge Plan

Date: 2026-04-22

This document is the first-pass merge plan for bringing the nano 3G Rockbox
port work from `/home/david/Documents/RockBox_Personal-fresh` into
`/home/david/Documents/RockBox_Personal-master` without blindly copying the
entire working tree.

## Goal

Move the nano 3G port source, simulator support, and bringup documentation into
`RockBox_Personal-master` while minimizing regression risk for shared S5L8702
targets such as iPod 6G and nano 4G.

## Current Facts

- The source tree in `RockBox_Personal-fresh` is not a clean commit. `git
  status --porcelain` shows 32 modified or untracked files related to nano 3G.
- The port is still experimental. The fresh tree documents the target as
  "Experimental / In-Progress" and still lists NAND storage as stubbed.
- `RockBox_Personal-master` already contains an `ipodnano3g` target directory,
  so this is an overwrite/reconcile job, not a simple add-only copy.

## Merge Strategy

Apply the port in four phases.

### Phase 1: Bring over target-local files and docs

These files are nano 3G specific and have the lowest regression surface. They
should be copied first.

- `docs/porting/ipodnano3g-roadmap.md`
- `docs/porting/ipodnano3g-boot-flow.md`
- `docs/porting/ipodnano3g-hardware-bringup.md`
- `firmware/target/arm/s5l8702/ipodnano3g/bringup-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/bringup-nano3g.h`
- `firmware/export/config/ipodnano3g.h`
- `firmware/target/arm/s5l8702/ipodnano3g/audio-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/backlight-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/cscodec-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/lcd-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/nand-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/piezo-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/pmu-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/power-nano3g.c`
- `firmware/target/arm/s5l8702/ipodnano3g/rtc-nano3g.c`

Notes:

- `git diff --no-index --stat` for the target-local directory shows 11 changed
  files with 362 insertions and 41 deletions, plus the two new bringup files.
- `powermgmt-nano3g.c`, `adc-nano3g.c`, `serial-nano3g.c`, and the target
  headers were not dirty in `fresh`, so they should stay as-is unless a later
  manual review shows a dependency mismatch.

### Phase 2: Reconcile required shared-source changes

These files are required for the target to build or boot, but they touch shared
paths used by other S5L8702 devices. They must be merged by hand, not copied
wholesale.

- `bootloader/ipod-s5l87xx.c`
- `firmware/SOURCES`
- `firmware/target/arm/s5l8702/lcd-s5l8702.c`
- `firmware/target/arm/s5l8702/pcm-s5l8702.c`
- `firmware/target/arm/s5l8702/system-s5l8702.c`
- `firmware/target/arm/s5l8702/usb-s5l8702.c`
- `tools/configure`

Observed change size from `master` to `fresh`:

- `firmware/SOURCES`: 1 insertion
- `firmware/export/config/ipodnano3g.h`: 8 insertions, 1 deletion
- `lcd-s5l8702.c`: 18 insertions
- `pcm-s5l8702.c`: 88 insertions
- `system-s5l8702.c`: 36 insertions
- `usb-s5l8702.c`: 34 insertions
- `bootloader/ipod-s5l87xx.c`: changed, but small enough to review manually

Review rule:

- Preserve any `RockBox_Personal-master` custom changes unrelated to nano 3G.
- Merge only `#if defined(IPOD_NANO3G)` or nano 3G bringup hooks unless a
  shared fix is clearly required by the port.
- Rebuild `ipod6g` after every shared-file batch.

### Phase 3: Merge simulator and debug tooling separately

These files help simulator bringup and debugging, but they are not required for
the first source import if the goal is only to stage the port.

- `uisimulator/bitmaps/UI-ipodnano3g.bmp`
- `firmware/target/hosted/sdl/button-sdl.c`
- `firmware/target/hosted/sdl/pcm-sdl.c`
- `firmware/target/hosted/sdl/sim-ui-defines.h`
- `uisimulator/buttonmap/ipod.c`
- `uisimulator/common/powermgmt-sim.c`
- `uisimulator/common/sim_tasks.c`
- `uisimulator/common/sim_tasks.h`
- `uisimulator/common/stubs.c`
- `apps/debug_menu.c`
- `apps/gui/statusbar-skinned.c`
- `apps/gui/statusbar-skinned.h`

Observed change size from `master` to `fresh`:

- `button-sdl.c`: 35 insertions, 12 deletions
- `pcm-sdl.c`: 161 insertions, 2 deletions
- `sim-ui-defines.h`: 7 insertions
- `buttonmap/ipod.c`: 11 insertions
- `powermgmt-sim.c`: 102 insertions, 38 deletions
- `sim_tasks.c`: 67 insertions, 28 deletions
- `stubs.c`: 3 insertions, 2 deletions
- `debug_menu.c`: 218 insertions
- `statusbar-skinned.h`: 1 insertion

Recommendation:

- Land simulator support in a separate commit from hardware-target changes.
- Land debug UI changes in a separate commit from simulator changes.

### Phase 4: Exclude generated and analysis artifacts

Do not copy these into `RockBox_Personal-master`.

- `build-native-ipodnano3g/`
- any other `build-*` output generated from the fresh tree
- `tools/ghidra/` unless there is a later reason to version reverse-engineering
  notes inside this repo

## Proposed Commit Breakdown

Use small commits in this order:

1. `docs: add nano 3G porting docs`
2. `firmware: import nano 3G target-local bringup sources`
3. `firmware: wire nano 3G into shared s5l8702 paths`
4. `tools: add nano 3G configure target wiring`
5. `sim: add nano 3G simulator assets and SDL wiring`
6. `debug: add nano 3G bringup diagnostics`

## Build And Regression Checklist

Minimum verification after each phase:

1. Configure and build `ipod6g`
2. Configure and build `ipodnano3g`
3. Configure and build `sim` target for nano 3G if simulator files are merged
4. Sanity-check that `ipod6g` and `ipodvideo` still build after shared-file
   changes

Suggested focus areas during review:

- bootloader conditionals shared with `IPOD_6G`
- LCD mode changes shared by 6G, nano 3G, and nano 4G
- PCM and USB hooks that now branch on `IPOD_NANO3G`
- simulator power-management behavior changes

## Recommendation

Proceed with the merge, but do it as a staged porting branch rather than a file
dump. The target-local files and docs are straightforward. The shared S5L8702
and simulator files are the real regression surface and should be merged in
reviewable commits with rebuilds between them.
