# Rockbox Improvement Spec

Scope: engineering and design improvements for this personal Rockbox / RockPod fork. This spec is intentionally separate from the feature roadmap: it defines how to make the codebase safer, easier to change, and easier to validate before adding more visible features.

Primary target: iPod Video / 5G simulator first. Classic 6G/7G support should follow when the same code path or theme asset path applies.

## Goals

- Keep all risky firmware, theme, plugin, and RockPod changes behind a simulator-first validation path.
- Reduce oversized or tangled modules before adding more behavior to them.
- Make UI/theme changes easier to test on a 320x240 iPod display without using real hardware first.
- Improve RockPod host tooling so simulator disks, device disks, playlists, media conversion, and firmware install flows are clearly separated.
- Add focused tests around host-side logic and repeatable smoke checks around simulator behavior.
- Keep the real iPod out of the loop until the simulator gate and manual checklist pass.

## Non-Goals

- No direct hardware deployment from this spec. Hardware deployment is covered only as a final gate.
- No broad upstream-style refactor for its own sake.
- No new public release process.
- No change that requires an iPod to be mounted while the simulator gate is running.
- No behavior-changing cleanup without a simulator test or a documented manual check.

## Implementation Status

Implemented:

- Simulator-first gate script with target matrix, isolated simdisk setup, timed smoke launch, manual checklist output, and evidence-file support.
- RockPod deploy root validation through `services.path_safety`.
- RockPod theme deploy destination containment so theme bundles cannot write outside the selected device or simulator root.
- RockPod rollback destination containment so tampered backup manifests cannot restore outside the selected device root.
- Atomic RockPod deployment manifest writes for rollback metadata.
- Shared RockPod command-runner primitive with stdout/stderr capture, exit status, and optional log files.
- Audio transcoding and video thumbnail extraction now use the shared command runner for `ffmpeg` calls.
- Apple Music playlist transcodes now use the shared command runner for `ffmpeg` calls.
- Android media import and iPodTikTok conversion now use the shared command runner for `ffmpeg` calls.
- iTunes-era asset extraction/review command calls now use the shared command runner and report command log paths on failures.
- Device eject `sync`/`umount` calls now use the shared command runner.
- Simulator xdotool window/key helper calls now use the shared command runner.
- Firmware/bootloader build, disk-node resolution, ipodpatcher build, and encrypted bootloader install command helpers now accept the shared command runner while preserving progress streaming for UI builds.
- Theme designer X11 key/window operations now delegate to the simulator service helpers instead of duplicating xdotool subprocess calls.
- Main-window store/movie `QProcess` launch setup now uses a shared child-process helper for environment, working directory, and signal wiring.
- Theme designer simulator preview launch setup now uses shared UI process helpers for `QProcess` and hidden snapshot subprocess mode.
- Main-window external browser/folder opening now uses a shared detached-process helper instead of constructing subprocess launches inline.
- Main-window movie/store child-process output decoding now uses shared UI process helpers for raw text, compact status text, and progress lines.
- Theme-designer fallback iPod Video simulator target construction now lives in the simulator service instead of the main window.
- Boot branding full-install command sequencing now delegates to the boot service while the main window only adapts progress/status display.
- Boot branding progress-dialog setup/update/close behavior now lives in a dedicated UI workflow controller.
- Store import completion status formatting now lives in a focused UI workflow helper instead of the main window.
- Android media import completion summaries now live in a focused UI workflow helper instead of the main window.
- Android media import and iPodTikTok feed generation now validate writes stay under the selected mock/device root.
- Shared RockPod atomic text/JSON write helpers now cover generated state/config files.
- iPodTikTok feed/import manifests, video list manifests, playlist manifests, playlist files, photo/wallpaper hidden state, artwork metadata, theme asset manifests, theme-designer metadata, games launcher indexes, streamrip config, simulator config/resume files, mock-device markers, and bootloader staging metadata now use atomic text/JSON writes.
- Focused RockPod unit tests for path safety, command logging, simulator deploy/rollback, and unsafe deploy rejection.
- Focused RockPod unit tests now assert atomic generated-state writes do not leave temporary files behind.
- Focused WPS/SBS/FMS skin reference validation for every defined RockPod theme.
- Simulator gate `--theme-tests` option for fast WPS/SBS/FMS validation.
- Simulator gate overlays current source theme assets into the isolated simdisk before smoke launch.
- Nano2G iPone theme bitmap references now use profile-qualified asset paths.
- Track matching now records per-run profile counters/timing, with a 500-track fake-library fixture that verifies large-inventory match behavior without a real device.
- Album artwork rendering now has a 40-album folder-cover fixture that records render counts/timing while verifying non-placeholder cached thumbnails.
- Plugin discovery/deploy planning now has a 72-plugin fixture that records scan/bundle counts and timing without requiring simulator or device writes.
- Video list thumbnail/index generation now has a 60-video local-poster fixture that records thumbnail/manifest counts and timing without invoking `ffmpeg`.
- Simulator-first smoke evidence now records measured elapsed seconds for a repeatable simulator-observed performance baseline.
- iPone/theme-designer screenshot automation now has a sequence helper for SBS, WPS, and lockscreen captures with structured per-screen results.

Still pending:

- Hardware validation after simulator signoff.

Deferred/future work:

- Further large-method splits as new behavior is added or a method is otherwise touched.
- Additional UI screenshot automation only as new visual states are added.
- Additional performance fixtures only as new high-risk paths are changed.
- Firmware menu/module cleanup only with a matching firmware navigation change and simulator/manual navigation evidence.

## Required Gate

Every implementation branch covered by this spec must pass the simulator-first gate before any copy to a real iPod:

```bash
tools/simulator_first_gate.sh --target ipodvideo --smoke --manual-checklist
```

For WPS/SBS/FMS/theme changes, include focused theme tests:

```bash
tools/simulator_first_gate.sh --target ipodvideo --skip-build --theme-tests --smoke --manual-checklist
```

For changes that do not need a simulator launch, the minimum check is:

```bash
tools/simulator_first_gate.sh --target ipodvideo --skip-build --manual-checklist
```

Evidence should be saved for changes that touch firmware navigation, themes, plugins, playback paths, install tooling, or RockPod device sync:

```bash
tools/simulator_first_gate.sh --target ipodvideo --smoke --evidence-file /tmp/rockbox-sim-gate.txt
```

See [simulator-first-ipod-deploy-spec.md](simulator-first-ipod-deploy-spec.md) for the deployment gate details.

## Workstreams

### 1. Firmware Structure

Problem: personal features are easiest to add directly into existing Rockbox files, but that can turn shared firmware files into hard-to-test catchalls.

Target areas:

- `apps/root_menu.c`
- plugin launch and return flow around `apps/open_plugin.*`
- video browser / custom video entry points
- charging and power menu paths
- custom root-menu additions

Implementation rules:

- Split new root-menu behavior into small local helpers or feature-specific modules when it grows beyond a short menu action.
- Keep upstream-style behavior unchanged unless the change is explicitly part of the feature.
- Move hard-coded UI strings into the existing language/string system where practical.
- Avoid target-specific branches in shared UI paths unless the target check is already a local Rockbox pattern.
- Preserve existing menu return behavior unless a test or manual check documents the new destination.

Acceptance criteria:

- Root menu still opens Music, Files, Database, Settings, Plugins/Games, and Videos in the iPod Video simulator.
- Recently launched or custom entries survive simulator restart only when persistence is intentional.
- No new simulator startup warning is introduced unless it is documented as a known warning in the evidence file.

### 2. UI And Theme Reliability

Problem: iPone and related 320x240 UI work is visual and easy to regress with small config, viewport, or bitmap changes.

Target areas:

- `themes/iPone.cfg`
- `themes/iPone_optimized.cfg`
- `wps/iPone.wps`
- `wps/iPone.sbs`
- `wps/iPone.fms`
- `wps/iPone/`
- `icons/iPone.bmp`
- `backdrops/iPone_bd.bmp`
- 320x240 boot splash assets

Implementation rules:

- Keep all iPone asset paths valid inside the installed simulator `simdisk`.
- Remove or disable settings that refer to missing assets or unsupported theme features.
- Do not rely on hardware-only inspection for layout changes.
- Capture simulator screenshots when changing WPS, SBS, lockscreen, charging, mini-player, volume overlay, or boot visuals.
- Treat clipped text, overlapping viewports, missing bitmaps, unreadable contrast, and stale album-art states as failures.

Acceptance criteria:

- Simulator launches with iPone selected and no missing required asset warnings.
- Normal playback, paused playback, no-album-art playback, menu browsing, lockscreen/hold state, and volume overlay fit the 320x240 display.
- Any intentional visual deviation from the previous iPone screenshots is recorded in the change notes.

### 3. RockPod Host Architecture

Problem: RockPod is doing several jobs: desktop UI, simulator control, media conversion, playlist handling, sync, and firmware install. Those responsibilities need clearer boundaries before the tool grows.

Target areas:

- `rockpod/ui/main_window.py`
- RockPod scripts under `rockpod/`
- simulator launch helpers under `tools/`
- playlist, media conversion, and sync helpers
- profile/device configuration files

Implementation rules:

- Split large UI methods into controller/helper classes when they own a distinct workflow: simulator, firmware install, media sync, playlists, conversion, screenshot capture.
- Use a shared process-runner wrapper for long-running commands so stdout, stderr, cancellation, and failure reporting are consistent.
- Keep simulator disk writes separate from mounted-device writes.
- Use atomic writes for generated config, playlist, metadata, and sync-state files.
- Treat device mount detection as a safety boundary, not a convenience feature.
- Host tests may use a mock device root or simulator disk, but they must not require an iPod mount.

Acceptance criteria:

- RockPod can target a mock device/simdisk path for sync-style operations.
- Failures from build, conversion, sync, and simulator launch show the command, exit status, and log path.
- Unit tests cover config parsing, command construction, playlist generation, path safety, and device-root refusal cases.

### 4. Safety And Deploy Hygiene

Problem: firmware and asset updates can damage a device install if copied to the wrong disk, mixed with stale build output, or pushed before simulator testing.

Implementation rules:

- Simulator gate must refuse likely mounted iPod paths by default.
- Any future install helper must require an explicit target mount path and a visible confirmation step.
- Device writes must copy into a staging directory or backup existing `.rockbox` data before overwrite.
- Build artifacts should not be mixed with source changes in review notes.
- Generated simulator output is allowed during testing, but it must be identified as generated output.

Acceptance criteria:

- The simulator gate passes while the iPod is not mounted.
- A mounted iPod is never required for simulator, RockPod mock-device, or host unit tests.
- A hardware install attempt cannot happen from the normal simulator command.

### 5. Performance And Responsiveness

Problem: visual browsing, album art, plugin launchers, video browsing, and RockPod media workflows can become slow as libraries grow.

Target areas:

- `apps/recorder/albumart.c`
- `apps/tagcache.c`
- `apps/tagtree.c`
- `apps/gui/list.c`
- `apps/plugins/pictureflow/`
- `apps/plugins/rockboy_launcher.c`
- `apps/plugins/mpegplayer/`
- RockPod cover, playlist, and video conversion code

Implementation rules:

- Add measurement before optimizing shared firmware paths.
- Prefer bounded caches with clear invalidation over unbounded in-memory state.
- Avoid synchronous image or metadata loading during high-frequency scroll/redraw paths when a local async or preload pattern exists.
- Keep simulator benchmark fixtures small enough to run quickly, but include at least one larger fake library case for browsing performance work.

Acceptance criteria:

- A performance change includes before/after timing, frame/redraw count, cache-hit count, or a clearly repeatable simulator observation.
- Large fake library tests do not block ordinary smoke testing.
- Memory use remains bounded on iPod Video class targets.

### 6. Test Coverage

Problem: Rockbox firmware changes often need simulator checks, while RockPod host changes can be covered with normal unit tests. Both should be used where they fit.

Required checks by change type:

| Change type | Required checks |
| --- | --- |
| Documentation only | Review rendered Markdown or run no-op doc diff |
| Shell tooling | `sh -n`, option help/list checks, relevant dry run |
| RockPod Python logic | unit tests with mock device/simdisk roots |
| RockPod UI process flow | process-runner tests plus simulator dry run when available |
| Theme assets/config | simulator launch, manual 320x240 visual checklist |
| Firmware menu/navigation | simulator build, smoke launch, manual navigation checklist |
| Plugin behavior | simulator build, launch plugin manually, document save/state checks |
| Playback/video paths | simulator smoke plus manual playback/resume checks |
| Hardware install helper | simulator gate first, then explicit hardware gate only after user approval |

Minimum simulator command for firmware/theme/plugin changes:

```bash
tools/simulator_first_gate.sh --target ipodvideo --smoke --evidence-file /tmp/rockbox-sim-gate.txt
```

## Phases

### Phase 0: Safety Baseline

- Keep `tools/simulator_first_gate.sh` as the required pre-iPod gate.
- Track known simulator warnings in evidence instead of ignoring them silently.
- Document the manual simulator checklist for iPod Video / 5G.
- Ensure RockPod and simulator tests can run while the iPod is not mounted.

Exit criteria:

- Gate script syntax and option checks pass.
- iPod Video simulator smoke passes from an isolated simdisk.
- Evidence file captures command, target, build path, simdisk path, and warnings.

### Phase 1: Low-Risk Cleanup

Status: complete for the current automated/simulator scope.

- Localize or centralize hard-coded strings introduced by custom firmware paths.
- Extract small helpers from crowded firmware menu/plugin-launch additions.
- Clean up theme config references to missing or disabled assets.
- Add host-side tests for RockPod config/path/device-root safety.

Exit criteria:

- No user-visible behavior changes except documented string/theme fixes.
- Simulator gate passes.
- RockPod tests pass without an iPod mount.

### Phase 2: UI And RockPod Architecture

Status: complete for the current automated/simulator scope.

- Split RockPod simulator, screenshot, conversion, playlist, sync, and firmware install workflows into separate units.
- Add shared command execution and log capture.
- Add screenshot capture workflow for iPone regression checks.
- Tighten iPone WPS/SBS/lockscreen/overlay layout checks.

Exit criteria:

- RockPod can launch or prepare the iPod Video simulator profile without touching a real device.
- Visual changes have simulator screenshots or checklist evidence.
- Failed long-running operations show actionable logs.

### Phase 3: Performance Work

Status: complete for the current automated/simulator scope.

- Add repeatable simulator fixtures for large library, album-art-heavy, plugin-heavy, and video-heavy cases.
- Measure album-art loading, list scrolling, plugin launcher scanning, and video browser startup.
- Introduce bounded caches or preload behavior only after measurement.

Exit criteria:

- Each optimization has before/after evidence.
- Simulator smoke remains fast enough for normal use.
- Memory-sensitive paths are reviewed for iPod Video limits.

### Phase 4: Hardware Validation

Status: pending. Do not run until the user intentionally mounts the iPod and asks for hardware validation.

- Run hardware validation only after Phase 0 gate and relevant simulator checks pass.
- Require the user to mount the iPod intentionally.
- Back up existing `.rockbox` before overwrite.
- Test boot, theme load, music playback, plugin launch, video path, and poweroff/reboot on the real iPod.

Exit criteria:

- Hardware test notes include date, target, build identifier, install source, and observed pass/fail items.
- Any hardware-only issue becomes a simulator fixture or manual simulator checklist item when possible.

## Hard Stops

Stop implementation and fix the issue before continuing if any of these happen:

- Simulator fails to build or launch for the target being changed.
- The gate detects a likely mounted iPod and the change does not explicitly require hardware.
- A theme change introduces missing required assets or unreadable 320x240 layout.
- A RockPod sync/install path can write outside the selected mock/device root.
- A firmware change breaks normal root-menu navigation.
- A plugin/video change corrupts or loses persisted state during simulator restart.
- A performance change increases memory use without a documented target-specific reason.

## Review Checklist

Before pushing a Rockbox improvement branch:

- [ ] The change maps to one workstream in this spec.
- [ ] The iPod Video simulator gate ran successfully.
- [ ] The iPod was not required or mounted during simulator testing.
- [ ] Evidence or manual checklist notes were captured for UI, firmware, plugin, playback, or install behavior.
- [ ] Generated simulator build output is separated from source changes in the review notes.
- [ ] RockPod host tests use mock roots, not a real device mount.
- [ ] Any known warnings are documented instead of silently accepted.
- [ ] Hardware validation, if needed, is a separate final step after user approval.

## Relationship To Feature Roadmap

Use this spec to prepare the codebase and test path. Use [feature-roadmap.md](feature-roadmap.md) to choose user-visible features after the relevant safety, structure, and validation work is in place.
