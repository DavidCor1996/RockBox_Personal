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

The automated gate is:

```bash
tools/simulator_first_gate.sh --target ipodvideo --smoke
```

Use `--rockpod-tests` when the change touches RockPod code, and use
`--theme-tests` when the change touches WPS/SBS/FMS/theme assets. Use
`--skip-build` only when the simulator has already been built and installed.

Theme-focused changes should use:

```bash
tools/simulator_first_gate.sh --target ipodvideo --skip-build --theme-tests --smoke
```

The gate copies current source `themes/`, `wps/`, `backdrops/`, `icons/`, and
`fonts/` into the isolated simulator disk before launching the simulator, so WPS
and SBS checks are run against the current source tree rather than stale build
output.

## Target Matrix

Use the simulator closest to the hardware that will receive the build.

| Hardware target | Simulator target | Default build directory | Required before hardware |
| --- | --- | --- | --- |
| iPod Video 5G/5.5G | `ipodvideo` | `build-sim-video-5g` | Always |
| iPod Classic 6G/7G | `ipod6g` | `build-sim-ipod6g` | Always when Classic-specific code, theme assets, or storage paths changed |
| iPod 3G | `ipod3g` | `build-sim-3g` | Always when 160x128 UI, grayscale assets, or 3G menu paths changed |
| iPod nano 2G | closest available simulator plus code review | target-specific build dir | Required if a simulator exists; otherwise document the gap before hardware |

If a change is shared across all targets, test at least the intended hardware target and one second screen size when the change touches layout, fonts, bitmaps, WPS/SBS, list rendering, or menu behavior.

## Evidence

Each simulator-first run should leave a short deploy note in the PR, commit message, issue, or local handoff. Use this format:

```text
Simulator gate:
- Date:
- Target:
- Command:
- Build result:
- Smoke result:
- RockPod tests:
- WPS/SBS/FMS theme tests:
- Manual checks:
- Known warnings:
- Hardware deploy allowed: yes/no
```

Expected warnings must be written down. Unexpected warnings are a hard stop until understood. Current known simulator-only warnings may include missing translated strings for local hard-coded theme/menu labels; those are allowed only if the UI path still renders correctly and the change did not add new untranslated strings.

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

The automated gate copies the simulator `simdisk/` to `/tmp/rockbox-sim-gate-*` and replaces `Music/` with an isolated directory. Manual tests should use the same rule.

Use the simulator `simdisk/` as the source root, then test against a disposable copy:

```bash
tmp_root="$(mktemp -d /tmp/rockbox-sim-manual.XXXXXX)"
cp -a build-sim-video-5g/simdisk "$tmp_root/simdisk"
rm -rf "$tmp_root/simdisk/Music"
mkdir -p "$tmp_root/simdisk/Music" "$tmp_root/simdisk/Playlists" "$tmp_root/simdisk/Videos"
```

If testing database or sync behavior, copy a small known test album or generated sample files into the temporary `Music/` directory. Keep the dataset small enough to inspect manually.

Hard stop:

- the active test `Music/` directory is a symlink to the host music library
- the simulator points at `/run/media`, `/media`, or another real device mount
- tests require deleting or rewriting files outside the temporary simulator root

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

### 6. Manual Feature Matrix

Run the relevant manual cases for the files changed.

| Change area | Manual simulator checks |
| --- | --- |
| Root menu or settings | open root menu, scroll top to bottom, enter each changed item, back out cleanly |
| WPS/SBS/FMS/theme | load the theme, open WPS, open menus, toggle hold/lockscreen state, check charging/USB screen if possible |
| Fonts/icons/bitmaps | verify no missing image placeholder, clipped title, blank backdrop, or invalid color on 320x240 |
| Video browser | open Videos, verify empty state, verify at least one supported `.mpg` entry, verify unsupported format message |
| Plugins/games | launch the changed plugin, exit cleanly, verify save/config file paths stay under `simdisk` |
| Database/tagcache | start with no database, scan a small `Music/` set, reboot simulator, verify Music opens without rebuilding unexpectedly |
| Playlists | export/import `.m3u8`, open playlist in Rockbox, verify paths are device-relative or Rockbox-absolute, not host paths |
| Boot splash/runtime assets | verify boot reaches the root menu and the first visible screen is not blank or corrupted |
| RockPod sync | sync to mock/simulator disk, inspect created/updated/deleted paths, then open the simulator on that disk |

### 7. RockPod Simulator/Mock Device Gate

For RockPod changes that write files, test against a mock device or simulator disk before using a real iPod.

Mock device flow:

```bash
cd rockpod
source .venv/bin/activate
python main.py --mock --mock-path /tmp/rockpod-mock-ipod
```

Simulator disk flow should use a disposable copy:

```bash
tmp_root="$(mktemp -d /tmp/rockpod-simdisk.XXXXXX)"
cp -a ../build-sim-video-5g/simdisk "$tmp_root/simdisk"
rm -rf "$tmp_root/simdisk/Music"
mkdir -p "$tmp_root/simdisk/Music"
python main.py --mock --mock-path "$tmp_root/simdisk"
```

Check at minimum:

- device detection shows the mock/simulator target, not a real iPod
- sync plan preview matches the expected file changes
- sync writes only expected paths under `Music/`, `Videos/`, `Playlists/`, or `.rockbox`
- playlists are exported and re-imported correctly
- Rockbox database/tagcache behavior is correct for the selected mode
- stale file removal never affects files outside the mock/simulator root
- a second sync is idempotent unless files changed

Hard stop:

- RockPod detects or targets a real iPod before the simulator gate passes
- sync plan includes unexpected deletes
- generated playlists reference host absolute paths
- database generation rewrites anything outside `.rockbox`
- mock/simulator root contains symlinks to the host library

### 8. Post-Simulator Review Gate

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

## Failure Handling

If a gate fails:

1. Stop before hardware deploy.
2. Record the failing command and the first meaningful error.
3. Preserve logs or screenshots only if they help diagnosis.
4. Fix the issue in source, not by editing generated simulator output.
5. Rerun the failed gate, then rerun the full automated gate.

Do not bypass a failure because the iPod is not mounted. The absence of mounted hardware is the intended safe state for this gate.

## Hardware Push Gate

Only push to the physical iPod after all simulator gates pass.

Before hardware deploy:

1. Confirm the target model and build type.
2. Build the matching hardware package.
3. Keep a backup of the existing device `.rockbox` when replacing runtime files.
4. Copy the built `rockbox.ipod` to both firmware locations on the iPod.
5. Verify both device firmware checksums match the local build.
6. Eject cleanly after copy.
7. Reboot the iPod and perform the same manual checks used in the simulator.

For iPod Video 5G/5.5G hardware:

```bash
./build-hw.sh 5g
```

Expected hardware package:

- `build-hw-ipodvideo/rockbox.zip` or the configured hardware build directory's `rockbox.zip`

Do not copy simulator binaries to a real iPod.

For direct firmware deploys, both locations are required:

```bash
cp build-hw-ipod6g/rockbox.ipod "/run/media/$USER/<IPOD>/rockbox.ipod"
cp build-hw-ipod6g/rockbox.ipod "/run/media/$USER/<IPOD>/.rockbox/rockbox.ipod"
sha256sum build-hw-ipod6g/rockbox.ipod \
    "/run/media/$USER/<IPOD>/rockbox.ipod" \
    "/run/media/$USER/<IPOD>/.rockbox/rockbox.ipod"
sync
```

Use the matching hardware build directory for the target. Hardware deploy is a
hard stop if either device checksum differs from the local build. Some iPod
bootloader paths can load `/.rockbox/rockbox.ipod`, so updating only the volume
root `rockbox.ipod` can leave the device running stale firmware.

Run physical iPod write probes, package installs, and cleanup commands outside the
workspace sandbox. Sandboxed probes against `/run/media/...` mounts can report a
false read-only failure even when the mounted iPod accepts writes from the host
session. Verify with a harmless outside-sandbox write before `fullinstall`, then
remove the probe file before deploying.

## Minimum Sign-Off Checklist

Record these before pushing to hardware:

- Target: `ipodvideo`, `ipod6g`, or other target
- Simulator build directory:
- Simulator command used:
- Simulator smoke result:
- RockPod tests run:
- Manual simulator checks passed:
- Mock/simulator sync root used:
- Known limitations:
- Known warnings:
- Hardware package path:
- Backup path for existing device `.rockbox`:
- Root firmware checksum verified:
- `.rockbox` firmware checksum verified:
- Hardware deploy approved by:

## Release Rule

No physical iPod deploy is allowed from this working tree unless the simulator build launches and the relevant feature path has been exercised in the simulator or RockPod mock device first.
