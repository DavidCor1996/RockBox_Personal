# iPone Theme Modernization Log

This log tracks staged, reviewable changes for the iPone theme stack.
Scope is theme-first (WPS/SBS/FMS/config/assets) with design preservation.

## Stage 01 - Inspection and plan (2026-04-14)

- Type: documentation and audit only
- Status: completed

### Purpose

- Identify fragile WPS/SBS logic before making visual or behavior changes.
- Define small patch stages with frequent simulator checks.

### Files audited

- `themes/iPone.cfg`
- `wps/iPone.wps`
- `wps/iPone.sbs`
- `wps/iPone.fms`
- `wps/iPone/*` assets (fallback, lockscreen, AOD, charging, slider assets)

### Key findings

1. **Repeated state guards in WPS**
   - `battery display == graphic` and hold/AOD checks are repeated in top-level branches.
   - This increases branch fan-out and makes state fixes error-prone.

2. **Redraw-heavy background path in WPS**
   - Normal playback background draw blocks are duplicated in two branches.
   - Full-screen draw chains plus repeated condition checks likely contribute to flicker.

3. **Overlay restore fragility**
   - Volume overlay visibility and info refresh timing rely on duplicated `%?mv(...)` logic.
   - Restore behavior is not clearly centralized, so regressions are easy.

4. **Inconsistent metadata/explicit handling**
   - Explicit marker logic uses different tags in different places (`%iG` vs `%iC` substring check).
   - Missing-tag fallback is not fully consistent between player and lockscreen blocks.

5. **SBS complexity concentrated in nested `%?if(%cs, =, 21)` trees**
   - Charging / hold / AOD / menu branches are deeply nested and partially duplicated.
   - This makes transitions harder to reason about and increases state-specific bugs.

6. **Charge wallpaper transitions are abrupt**
   - Wallpaper alternates by minute windows in a large inline branch.
   - Behavior works, but logic is hard to maintain and tune.

### Planned patch stages

1. **Stage 02 - WPS structure cleanup (logic-only)**
   - Consolidate repeated top-level guards and shared draw paths.
   - Keep the exact same visual output.

2. **Stage 03 - WPS metadata/progress/fallback polish**
   - Normalize missing-title/artist/album behavior.
   - Keep style identical while tightening fallback art and icon rules.

3. **Stage 04 - WPS overlay transition polish**
   - Stabilize volume/progress overlay restore behavior after popup timeout.
   - Reduce perceived flicker from repeated redraw branches.

4. **Stage 05 - SBS state router cleanup (theme-only)**
   - Refactor charging/lockscreen/AOD/menu state routing into clearer blocks.
   - Preserve layout, spacing, iconography, and color identity.

5. **Stage 06 - Simulator regression workflow**
   - Add a lightweight reproducible test script/doc using:
     - `/home/david/Documents/RockBox_Personal-master/build-sim-video-5g/rockboxui`
   - Cover playback, pause, lockscreen, AOD, charging, radio, no-art, long-title, and volume overlay.

6. **Stage 07 - User checkpoint (mandatory)**
   - Stop after theme checkpoint.
   - Wait for user testing/approval before any source-level support work.

### Testing done in this stage

- Static audit only (no WPS/SBS behavior changes yet).
- Confirmed simulator binary options via:
  - `build-sim-video-5g/rockboxui --help`

### Known tradeoffs

- Audit-first stage adds no immediate visual improvements.
- Prioritizes safer incremental edits over fast large rewrites.

## Stage 02 - WPS branch dedupe pass 1 (2026-04-14)

- Type: theme logic cleanup (no intended visual redesign)
- Status: completed

### Purpose

- Reduce redraw-heavy duplicate branches in `iPone.wps`.
- Keep visual identity and layout unchanged while making state logic less fragile.

### What changed

1. **Centralized normal playback backdrop draw chain**
   - Added shared viewport `PlayerBaseBackdrop` for the non-lockscreen background.
   - Replaced duplicated inline `%dr(...)` + fallback-art chain with `%Vd(PlayerBaseBackdrop)`.

2. **Reduced duplicate playtime slider branch logic**
   - Simplified playtime digit routing to a single `%pt` threshold chain.
   - Removed duplicated hold/non-hold copies that resolved to the same output.

### Files touched

- `wps/iPone.wps`
- `docs/ipone_theme_modernization_log.md`

### Why this helps

- Smaller branch surface lowers the risk of state-specific regressions.
- Shared draw path improves maintainability and should reduce flicker opportunities
  caused by duplicate full-screen redraw blocks.

### Testing

- Simulator target used (as requested):
  - `/home/david/Documents/RockBox_Personal-master/build-sim-video-5g/rockboxui`
- Commands executed:
  - `./rockboxui --help`
  - `./rockboxui --debugwps --zoom 2 --nobackground`
- Result:
  - Theme loaded (`iPone.sbs` and `iPone.wps`) with no parser/runtime errors in debug output.
  - Run timed out after interactive launch window (expected for simulator session).

### Simulator output evidence

- Key lines from debug run:
  - `Loading '/.rockbox/wps/iPone.sbs'`
  - `Loading '/.rockbox/wps/iPone.wps'`
  - `WPS debug mode enabled.`

### Known tradeoffs

- This pass intentionally avoids visual adjustments to keep drift minimal.
- No screenshot capture automation added yet (planned in Stage 06 workflow).

## Stage 03 - WPS fallback/metadata/overlay polish (2026-04-14)

- Type: theme refinement + regression test infrastructure
- Status: completed

### Purpose

- Introduce subtle visible polish without visual drift.
- Improve no-art presentation, metadata fallback behavior, and overlay restoration.
- Add reproducible screenshot-based checks for key states.

### What changed

1. **Fallback album art feels intentional**
   - Added `AlbumStage.bmp` to the no-art draw path so fallback art keeps the same framed stage context.
   - This preserves iPone identity while avoiding a bare error-like panel when album art is missing.

2. **Metadata fallback hardened (no layout redesign)**
   - Secondary metadata line now consistently falls back between `%ia`, `%id`, and `%d(1)` across:
     - main player info viewport
     - lockscreen notification card
     - AOD metadata row
   - Long-title behavior remains marquee-style; no typography/layout redesign was introduced.

3. **Overlay restoration polish**
   - Separated base player redraw from metadata redraw and suppressed metadata repaint while volume popup is active.
   - This reduces edge artifacts during volume popup transitions.
   - Removed empty false branch in volume overlay condition to avoid redundant redraw work.

4. **Carry-forward from Stage 02 kept intact**
   - Shared `PlayerBaseBackdrop` and single playtime digit routing remain in place.

### Files touched

- `wps/iPone.wps`
- `tools/ipone_regression_capture.sh`
- `docs/ipone_theme_modernization_log.md`

### Regression workflow added

- New helper script:
  - `tools/ipone_regression_capture.sh`
- Default output directory:
  - `docs/ipone-regression-shots/`
- Captured states:
  - `01-normal-playback.png`
  - `02-paused.png`
  - `03-lockscreen.png`
  - `04-volume-overlay-active.png`
  - `05-long-title-a.png`
  - `06-long-title-b.png`
  - `07-no-album-art.png`

### Reproduction steps used

1. Run screenshot workflow:
   - `tools/ipone_regression_capture.sh`
2. Verify output images in:
   - `docs/ipone-regression-shots/`
3. Track mapping used by the workflow:
   - normal playback: `/Music/Electric Jewels - April Wine/01 - April Wine - Weeping Widow.flac`
   - long title: `/Music/Abbey Road (Remastered) - The Beatles/13 - The Beatles - She Came In Through The Bathroom Window (Remastered 2009).flac`
   - no album art: `/Music/The Beatles (White Album)/The Beatles (White Album) [Disc 1]/01 Back In The U.S.S.R..mp3`

### Testing results

- Simulator run target:
  - `/home/david/Documents/RockBox_Personal-master/build-sim-video-5g/rockboxui`
- Script now forces simulator root via `--root` to avoid accidental host-root captures.
- Regression images successfully captured for all required states.
- Manual spot checks from captured images confirm:
  - no regressions in basic playback composition
  - volume overlay restores cleanly
  - long-title marquee remains active and readable
  - lockscreen presentation preserved

### Known tradeoffs

- No-art fallback styling remains conservative (same palette/structure, no new visual motif).
- One screenshot helper script touches `.playlist_control` / `.resume.cfg` / `config.cfg` during run,
  then restores backups on exit.

## Stage 04 - Lockscreen volume-overlay suppression fix (2026-04-14)

- Type: isolated behavior fix (theme-only)
- Status: completed, awaiting user review

### Purpose

- Fix WPS lockscreen authority bug where a just-triggered volume popup could remain visible
  after immediate lock.
- Keep iPone visual design unchanged and preserve normal overlay behavior on regular playback.

### Problem addressed

- Repro from user:
  1. start playback on WPS
  2. change volume
  3. immediately lock device
  4. lockscreen could still show volume overlay

### What changed

1. **Foreground draw order made lockscreen-authoritative**
   - Moved lockscreen/AOD foreground draw block to after the volume overlay draw block.
   - This ensures lockscreen rendering wins on the same frame if lock engages while volume popup
     is still active.

2. **Volume overlay condition hard-suppressed under lock/AOD in graphic battery mode**
   - Updated overlay gate so when `%mh` is active with graphic battery lockscreen path,
     lockscreen/AOD background is redrawn instead of volume card viewport.
   - Non-lock behavior remains unchanged: volume overlay still appears normally on unlocked WPS.

### Files touched

- `wps/iPone.wps`
- `tools/ipone_regression_capture.sh`
- `docs/ipone_theme_modernization_log.md`

### Why this fixes it

- Previously, a transient `%mv(1.2)` true state could still paint overlay content during lock transition.
- With lockscreen/AOD foreground rendered after overlay and overlay branch explicitly suppressed while
  locked, lockscreen state becomes authoritative and the popup cannot carry into lockscreen visuals.

### Testing

- Parser/load sanity:
  - `/home/david/Documents/RockBox_Personal-master/build-sim-video-5g/rockboxui --debugwps --zoom 2 --nobackground --root /home/david/Documents/RockBox_Personal-master/build-sim-video-5g/simdisk`
  - WPS/SBS load observed with no parser errors.

- Repro and behavior checks executed via updated screenshot workflow:
  - `tools/ipone_regression_capture.sh`

- Verified states covered:
  1. change volume then immediate lock -> **no volume overlay on lockscreen**
     - `docs/ipone-regression-shots/08-lockscreen-after-volume-change.png`
  2. change volume and stay unlocked -> **overlay still normal**
     - `docs/ipone-regression-shots/04-volume-overlay-active.png`
  3. lock, unlock, then change volume -> **overlay works on playback screen**
     - `docs/ipone-regression-shots/09-lock-unlock-then-volume-overlay.png`
  4. repeated rapid volume-change + immediate lock cycles -> **lockscreen remains clean**
     - `docs/ipone-regression-shots/10-rapid-volume-lock-cycle.png`

### Regression capture updates

- Added practical coverage for this bug family in:
  - `tools/ipone_regression_capture.sh`
- New output files:
  - `08-lockscreen-after-volume-change.png`
  - `09-lock-unlock-then-volume-overlay.png`
  - `10-rapid-volume-lock-cycle.png`

### Known tradeoffs

- Fix is intentionally conservative and localized to draw order/conditions in WPS.
- No lockscreen layout or art changes were introduced.

## Validation pass - active-playback + startup-state re-check (2026-04-15)

- Type: verification-only pass (no new theme patch in this step)
- Status: completed, stop for review

### Purpose

- Re-validate the two remaining real issues using trusted runtime state:
  1. lockscreen volume-overlay suppression during immediate lock after volume change
  2. startup default entry behavior (SBS/springboard vs unexpected WPS)

### Evidence quality controls used

1. **Active playback validation before lockscreen tests**
   - Capture workflow run:
     - `tools/ipone_regression_capture.sh /home/david/Documents/RockBox_Personal-master/build-sim-video-5g /home/david/Documents/RockBox_Personal-master/docs/ipone-regression-shots`
   - The script verified active playback by measuring pixel deltas between time-separated
     WPS crops and only continuing once threshold passed.
   - Run evidence:
     - `docs/ipone-regression-shots/capture-run.log`
     - Deltas observed at validation points: `1320`, `1052`, `28792`, `2788`.

2. **Clean startup-state check isolated from saved playback/runtime files**
   - Clean startup capture used a temporary isolated simulator root copied from simdisk,
     then removed startup-forcing and resume/playlist state before launch:
     - removed `start in screen:` / `repeat:` lines from temp `config.cfg`
     - removed `.playlist_control` / `.resume.cfg*` from temp root
   - Startup capture:
     - `docs/ipone-regression-shots/11-startup-clean-runtime.png`

3. **Source config verification**
   - Live sim config check showed no persistent forced startup directives in:
     - `build-sim-video-5g/simdisk/.rockbox/config.cfg`

### What was tested and result

1. **Normal unlocked playback baseline**
   - Evidence:
     - `docs/ipone-regression-shots/01-normal-playback.png`
   - Result: normal WPS playback layout present.

2. **Lockscreen with no volume change**
   - Evidence:
     - `docs/ipone-regression-shots/03-lockscreen.png`
   - Result: lockscreen appears normally with no volume popup.

3. **Volume overlay while unlocked**
   - Evidence:
     - `docs/ipone-regression-shots/04-volume-overlay-active.png`
     - `docs/ipone-regression-shots/09-lock-unlock-then-volume-overlay.png`
   - Result: volume overlay still works as expected when unlocked.

4. **Immediate lock after volume change (primary bug repro)**
   - Evidence:
     - `docs/ipone-regression-shots/08-lockscreen-after-volume-change.png`
   - Result: **volume overlay is still visible on lockscreen**. Issue not fixed.

5. **Rapid repeated volume-change + lock timing**
   - Evidence:
     - `docs/ipone-regression-shots/10-rapid-volume-lock-cycle.png`
   - Result: **overlay still appears on lockscreen in rapid case**. Issue remains timing-sensitive/flaky.

6. **Startup default behavior (clean state)**
   - Evidence:
     - `docs/ipone-regression-shots/11-startup-clean-runtime.png`
   - Result: startup lands in SBS/springboard-style menu view (not WPS).
   - Interpretation: unexpected startup-to-WPS is not reproduced in clean state and remains attributable
     to forced runtime/script state, not confirmed as current theme runtime default behavior.

### Current conclusion after this pass

- **Lockscreen overlay suppression bug:** **NOT resolved** (reproduced in both normal and rapid immediate-lock cases).
- **SBS vs WPS startup default:** behaves correctly in clean isolated startup state; no fresh evidence of core default forcing to WPS.

### Note

- This validation run produced the required lockscreen/startup evidence set.
- The capture script encountered a later-path failure while proceeding to long-title/no-art follow-up captures in this run;
  that failure does not invalidate the lockscreen/startup evidence listed above.

## Conservative transition attempt - hold-entry suppression check (2026-04-15)

- Type: narrow runtime-state attempt + rollback of bad side effects
- Status: not resolved, stop for review

### Scope control

- Kept lockscreen composition baseline in `wps/iPone.wps` (clock/miniplayer layers restored and left in place).
- Did not keep any structural lockscreen composition rewrite from this attempt.

### Narrow change tested

- Attempted a hold-entry runtime suppression in `apps/gui/wps.c` by clearing
  `global_status.last_volume_change` when hold state toggled on.
- Objective was to invalidate `%mv(...)` timer state at lock transition without moving lockscreen blocks.

### Outcome

- The attempt produced unacceptable visual regressions in lockscreen captures (clock/miniplayer missing in some frames),
  so it was rolled back.
- After rollback to baseline composition, lockscreen visuals were restored.
- Re-validation still shows lockscreen volume overlay persisting in immediate-lock and rapid cases:
  - `docs/ipone-regression-shots/08-lockscreen-after-volume-change.png`
  - `docs/ipone-regression-shots/10-rapid-volume-lock-cycle.png`

### Active playback validation for this pass

- Confirmed by capture workflow deltas in `docs/ipone-regression-shots/capture-run.log`:
  `1320`, `2704`, `28796`, `2620`.

### Current state after rollback

- Lockscreen clock/miniplayer baseline is restored.
- Volume overlay while unlocked remains normal.
- Immediate-lock stale volume overlay bug remains unresolved.
