# Simulator-First iPod Deploy Spec

## Goal

Every Rockbox or RockPod change that can affect boot, menus, themes, plugins, sync output, database files, or device storage must be validated in a simulator before it is pushed to a physical iPod.

The simulator gate is meant to catch bad UI states, missing assets, broken plugins, bad database behavior, and unsafe copy/delete logic before removable hardware is touched.

## Scope

This applies to:

- Rockbox firmware changes under `apps/`, `firmware/`, `bootloader/`, `uisimulator/`, `tools/`, and `utils/`
- theme, WPS/SBS/FMS, icon, font, backdrop, and boot image changes
- RockPod sync, playlist, tagcache, simulator, theme, boot, game, photo, plugin, and device-management changes
- any package or script that writes into an iPod `.rockbox` directory

This does not replace hardware testing. It defines the minimum pre-hardware gate.

## Non-Goals

- Do not use the simulator to validate battery, storage-controller, USB, audio DAC, clickwheel, or bootloader-only behavior.
- Do not push directly from an unreviewed dirty working tree.
- Do not test against the real iPod mount path until the simulator checklist passes.

## Required Gates

### 1. Source Hygiene Gate

Before building, confirm the change set is understandable:

```bash
git status --short
git diff --stat
```

Expected:

- source changes are intentional
- generated object files, simulator output, local databases, caches, and personal media are not part of the deploy change set
- hardware deploy paths are not modified by test-only data

Hard stop:

- unexplained source changes exist
- generated files are mixed with code changes in a way that makes review unclear
- a real iPod mount path is referenced by a test script or command

### 2. Unit Test Gate

For RockPod changes, run the focused Python tests first:

```bash
cd rockpod
source .venv/bin/activate
python -m pytest tests/ -v
```

For a narrow change, a focused subset is acceptable during iteration, but the full RockPod suite should pass before pushing to hardware.

Hard stop:

- sync, playlist, database, device inventory, simulator, boot, or theme tests fail
- tests require a real iPod to pass

### 3. Simulator Build Gate

Build the target simulator that matches the intended iPod as closely as possible.

For iPod Video 5G/5.5G:

```bash
mkdir -p build-sim-video-5g
cd build-sim-video-5g
../tools/configure --target=ipodvideo --type=s
make -j"$(nproc)"
make install
```

If `nproc` is unavailable, use a fixed job count:

```bash
make -j4
make install
```

Expected:

- `build-sim-video-5g/rockboxui` exists
- `build-sim-video-5g/simdisk/.rockbox` exists
- build finishes without compile or link errors

Hard stop:

- simulator does not build
- install step fails
- required theme/plugin assets are missing from `simdisk/.rockbox`

### 4. Isolated Simdisk Gate

Simulator tests should run against disposable data, not a real music library or iPod mount.

Use the simulator `simdisk/` as the test root:

```bash
mkdir -p build-sim-video-5g/simdisk/Music
mkdir -p build-sim-video-5g/simdisk/Playlists
```

If testing database or sync behavior, copy a small known test album or generated sample files into `simdisk/Music`. Keep the dataset small enough to inspect manually.

Hard stop:

- `simdisk/Music` is a symlink to the host music library
- the simulator points at `/run/media`, `/media`, or another real device mount
- tests require deleting or rewriting files outside `build-sim-video-5g/simdisk`

### 5. Manual Simulator Smoke Test

Launch the simulator:

```bash
./run-sim-video-5g.sh
```

If the helper script is unavailable, run directly:

```bash
cd build-sim-video-5g
SDL_VIDEODRIVER=x11 SDL_RENDER_DRIVER=software ./rockboxui --zoom 2
```

Check at minimum:

- Rockbox boots without panic or hang
- root menu opens and scrolls correctly
- Music, Files, Settings, Plugins, and custom root menu entries open
- iPone theme, status bar, lockscreen/hold state, fonts, icons, and backdrop render without missing assets
- video browser opens if video code changed
- Game Boy/plugin entries open if plugin or game paths changed
- database scan/update status behaves as expected
- playlists open if playlist export/import changed
- no text overlaps or off-screen controls appear on the 320x240 viewport

Hard stop:

- simulator boots to a blank screen, panic, crash, or infinite progress state
- key navigation breaks for core menus
- theme assets fail to load
- a plugin that is part of the change fails to start
- database or playlist behavior regresses in the simulator

### 6. RockPod Simulator/Mock Device Gate

For RockPod changes that write files, test against a mock device or simulator disk before using a real iPod.

Mock device flow:

```bash
cd rockpod
source .venv/bin/activate
python main.py --mock --mock-path /tmp/rockpod-mock-ipod
```

Simulator disk flow:

```bash
cd rockpod
source .venv/bin/activate
python main.py --mock --mock-path ../build-sim-video-5g/simdisk
```

Check at minimum:

- device detection shows the mock/simulator target, not a real iPod
- sync plan preview matches the expected file changes
- sync writes only expected paths under `Music/`, `Videos/`, `Playlists/`, or `.rockbox`
- playlists are exported and re-imported correctly
- Rockbox database/tagcache behavior is correct for the selected mode
- stale file removal never affects files outside the mock/simulator root

Hard stop:

- RockPod detects or targets a real iPod before the simulator gate passes
- sync plan includes unexpected deletes
- generated playlists reference host absolute paths
- database generation rewrites anything outside `.rockbox`

### 7. Post-Simulator Review Gate

After the simulator pass, review changed files again:

```bash
git status --short
git diff --stat
```

Expected:

- simulator-generated files are ignored or excluded from the deploy commit
- source changes remain limited to the intended feature/fix
- documentation or tests are updated when behavior changed

Hard stop:

- `simdisk` state, generated databases, caches, or local media appear as commit candidates
- simulator testing modified source unexpectedly

## Hardware Push Gate

Only push to the physical iPod after all simulator gates pass.

Before hardware deploy:

1. Confirm the target model and build type.
2. Build the matching hardware package.
3. Keep a backup of the existing device `.rockbox` when replacing runtime files.
4. Eject cleanly after copy.
5. Reboot the iPod and perform the same manual checks used in the simulator.

For iPod Video 5G/5.5G hardware:

```bash
./build-hw.sh 5g
```

Expected hardware package:

- `build-hw-ipodvideo/rockbox.zip` or the configured hardware build directory's `rockbox.zip`

Do not copy simulator binaries to a real iPod.

## Minimum Sign-Off Checklist

Record these before pushing to hardware:

- Target: `ipodvideo`, `ipod6g`, or other target
- Simulator build directory:
- Simulator command used:
- RockPod tests run:
- Manual simulator checks passed:
- Known limitations:
- Hardware package path:
- Backup path for existing device `.rockbox`:

## Release Rule

No physical iPod deploy is allowed from this working tree unless the simulator build launches and the relevant feature path has been exercised in the simulator or RockPod mock device first.
