# Rockbox Loading Popup Modernization Spec

Scope: modernize Rockbox core loading, wait, and progress popups on bitmap LCD
targets, with iPod Video / 5G and the iPone theme as the first validation
target.

This spec is for Rockbox firmware UI. RockPod may later benefit from the same
visual language, but RockPod is not the implementation target here.

## Goals

- Make common loading states feel consistent with the modern iPone UI.
- Match the polished popup treatment used by the iPone volume overlay: compact
  centered card, soft highlight/lowlight, icon/status area, and a thin modern
  activity/progress element.
- Improve high-visibility popups such as `Loading...`, `Loading tracks...`,
  database build/update progress, playlist loading, and scan/save operations.
- Generate any bitmap assets needed to make the popup look intentional instead
  of relying only on primitive rectangles.
- Keep existing Rockbox callers simple: code that already calls `splash()`,
  `splashf()`, or `splash_progress()` should either improve automatically or
  migrate through a small compatibility wrapper.
- Preserve voice output, abort hints, RTL layout behavior, and small-screen
  fallbacks.
- Keep the first implementation simulator-first and easy to disable if a target
  exposes rendering regressions.

## Non-Goals

- Do not redesign every Rockbox menu, list, dialog, plugin popup, or theme.
- Do not require WPS skin syntax changes for normal loading popups.
- Do not make loading work asynchronous as part of the visual modernization.
- Do not remove text-only fallbacks for monochrome, tiny, or constrained
  targets.
- Do not change playlist, database, file operation, or scan semantics.

## Current Code Findings

### Generic splash path

- `apps/gui/splash.c` owns the shared popup rendering path.
- `splash()` is a macro over `splashf()` in `apps/gui/splash.h`.
- `splashf(int ticks, const char *fmt, ...)`:
  - resolves language IDs with `P2ID()` / `P2STR()`;
  - preserves voice behavior through `cond_talk_ids_fq()`;
  - draws on every Rockbox screen through `FOR_NB_SCREENS`;
  - sleeps only when `ticks` is nonzero.
- `splash_internal()` handles text wrapping, centered overlay viewport sizing,
  scroll stopping, theme foreground/background selection, and viewport updates.
- The current working tree already adds a color `splash_draw_modern_panel()`
  path in `apps/gui/splash.c`. That gives normal splashes a dark rounded panel
  on color screens while preserving the old inverse/border fallback.

### Progress popup path

- `splash_progress()` also lives in `apps/gui/splash.c`.
- It rate-limits updates to 20 fps and supports a first-display delay through
  `splash_progress_set_delay()`.
- It preserves periodic spoken progress using `LANG_LOADING_PERCENT`.
- The current working tree already adds a color rounded progress pill through
  `splash_progress_draw_pill()`.
- Callers use progress popups when total work is known, for example:
  - database scan/commit progress in `apps/root_menu.c`;
  - playlist control-file loading in `apps/playlist.c`;
  - database playlist insertion in `apps/tagtree.c`;
  - file operations in `apps/fileop.c`;
  - selected plugins through the plugin API.

### "Loading tracks" path

- The visible text comes from `LANG_PLAYLIST_SEARCH_MSG` in
  `apps/lang/english.lang`:
  `Loading tracks... %d found (%s)`.
- Playlist search in `apps/playlist_viewer.c` repeatedly calls:
  `splashf(0, str(LANG_PLAYLIST_SEARCH_MSG), found_indicies_count,
  str(LANG_OFF_ABORT))`.
- Playlist insert/save count feedback in `apps/playlist.c` uses the same
  repeated `splashf(0, ...)` pattern through `display_playlist_count()`.
- These call sites know a count, but not always a reliable total, so they are
  better modeled as an indeterminate loading state with a count badge/text than
  as a fake percentage bar.

### iPone volume popup reference

- The iPone WPS volume popup is defined in `wps/iPone.wps`, not directly in
  `wps/iPone.sbs`.
- The popup is shown with `%?mv(1.2)<...>` and composes:
  - `VolumeBackdrop.bmp`, a 180x45 card;
  - `VolumePromptIcons.bmp`, a 4-frame icon strip;
  - `VolumeSliderBackdropPurple.bmp`, `VolumeSliderPurple.bmp`, and
    `VolumeSliderEndPurple.bmp`, a 117x5 slider set.
- The key design target is not the volume behavior itself; it is the visual
  treatment: centered compact card, generated bitmap backdrop, small icon/status
  glyph, thin active line, and enough polish that the popup feels like part of
  the theme.
- Core `apps/gui/splash.c` cannot directly reuse WPS/SBS-loaded assets. For a
  Rockbox core popup, asset-backed rendering should use compiled native bitmaps
  under `apps/bitmaps/native` with a primitive-drawn fallback.

### Other common indeterminate paths

Examples of current zero-tick loading/wait splashes include:

- `apps/filetree.c`: settings, keyboard, language, theme, and playlist waits.
- `apps/tree.c`: disk scanning.
- `apps/main.c`: disk scanning during startup.
- `apps/plugin.c`: plugin loading wait.
- `apps/onplay.c`: file/context operations.
- `apps/radio/presets.c`: radio preset waits and scans.

These paths should improve if the shared splash renderer is modernized, but
they should not all be hand-edited in the first pass.

## Problem

Rockbox has one shared splash primitive, but loading UX has three different
meanings:

- short message: success, failure, warning, or confirmation;
- indeterminate work: loading/scanning/searching with no reliable total;
- determinate work: progress is known and should use a progress bar.

Today those meanings are mostly encoded by convention:

- `splash(HZ, ...)` or `splashf(HZ, ...)` for timed messages;
- `splash(0, ...)` or `splashf(0, ...)` for ongoing work;
- `splash_progress(...)` for determinate progress.

That makes the UI easy to call, but limits visual polish. A plain
`splashf(0, "Loading tracks...")` cannot express that it is an ongoing loading
state, cannot show an activity indicator, and cannot reserve stable space for
rapidly changing counts without relying on the current max-width artifact
prevention behavior.

## Preferred Outcome

Rockbox should have modern, consistent loading popups by default on color
bitmap targets:

- timed messages continue to use a compact modern panel;
- indeterminate loading uses a volume-overlay-style card plus a small generated
  activity indicator;
- known progress uses the same card plus a generated or primitive rounded
  progress pill;
- count-bearing loading states such as `Loading tracks... %d found (OFF to
  abort)` keep a stable layout as the count grows;
- voice output and abort behavior remain unchanged;
- monochrome and very small targets keep the old text/border behavior.

## Proposed Design

### 0. Visual target

The loading popup should deliberately follow the iPone volume overlay language:

- centered 180x45-ish card on 320x240 screens;
- dark iPone-compatible variant for normal Rockbox/iPone use;
- optional light/AOD variant only if a caller can prove it is drawn on an AOD or
  lockscreen-style light background;
- one small generated status glyph area, equivalent in weight to the volume
  icon;
- one thin activity/progress strip, equivalent in weight to the volume slider;
- text placed with stable margins so count changes do not make the card jump.

The first pass should target iPod Video / 5G geometry. Other display sizes
should use scaled generated assets only when the asset size is explicitly
checked into `apps/bitmaps/native`; otherwise they should fall back to primitive
panel drawing.

### 1. Keep `splash()` compatible

Do not break existing callers. Continue to route `splash()` through
`splashf()`, and keep language-string voicing behavior intact.

The generic path should remain safe for:

- boot and early startup before a theme is fully loaded;
- multi-screen devices;
- voice menus;
- RTL strings;
- monochrome/greyscale targets;
- plugins that call the splash API through `plugin_api`.

### 2. Add a typed loading overlay helper

Introduce a small Rockbox core UI helper rather than teaching every caller a new
drawing protocol:

```c
enum splash_kind {
    SPLASH_KIND_MESSAGE,
    SPLASH_KIND_LOADING,
    SPLASH_KIND_PROGRESS,
};

void splash_loadingf(const char *fmt, ...);
void splash_loading_countf(int count, const char *fmt, ...);
```

The first implementation can live in `apps/gui/splash.c` and reuse
`splash_internal()` layout for text measurement and wrapping. On supported
color-screen geometry, it should draw a generated popup card asset first, then
draw text and a small generated activity indicator. On unsupported geometry, it
should fall back to the current primitive modern panel.

The helper should not sleep. Callers remain responsible for abort polling and
work loops, matching the current `splashf(0, ...)` pattern.

### 3. Use stable content sizing for repeated updates

Repeated loading updates should not resize the popup every frame. The current
`max_width[NB_SCREENS]` behavior prevents some artifacts, but it is activity
based and applies to all splashes.

For loading popups:

- reserve space for the expected dynamic portion when possible;
- keep a minimum panel width on iPod Video class displays;
- cap panel width to the centered overlay viewport;
- truncate or wrap long strings using the same word-wrap path as current
  splashes.

The first caller to benefit should be `Loading tracks... %d found (%s)`, where
the count grows during playlist search.

### 4. Add an indeterminate activity indicator

Use a very cheap indicator. On iPod Video / 5G, prefer generated bitmap assets
that match the iPone volume overlay style:

- `LoadingPopupBackdrop.180x45x16.bmp` or equivalent card backdrop;
- `LoadingPopupSpinner.16x192x16.bmp` or equivalent 12-frame activity strip;
- optional `LoadingPopupTrack.117x5x16.bmp` and
  `LoadingPopupFill.117x5x16.bmp` if the progress pill looks better as an asset
  than primitive drawing.

Fallback behavior:

- color LCD without matching assets: segmented line, dot cycle, or short pill
  sweep drawn with primitives inside the panel;
- non-color LCD: no indicator, or a minimal text-only fallback;
- update rate: no faster than the existing `splash_progress()` 20 fps limit;
- no extra allocation.

Do not load WPS/theme bitmaps at runtime from `splash.c`. The splash path must
work before theme assets are available and inside plugins. Any polished assets
needed for the core popup should be generated and compiled through Rockbox's
native bitmap pipeline.

### 5. Keep progress and loading visually related

The existing rounded progress pill in `splash_progress()` can remain the
determinate fallback. The new loading helper and progress path should share:

- generated card asset when available;
- panel colors and fallback geometry;
- spacing;
- text alignment;
- minimum dimensions;
- display-delay/rate-limit behavior where useful.

This keeps database initialization, playlist loading, and track search from
looking like separate UI systems.

## Asset Generation

Generate assets as source-controlled build inputs, not as one-off manual edits.

Recommended first asset set for iPod Video / 5G:

- `apps/bitmaps/native/loadingpopup.180x45x16.bmp`
  - dark compact card matching the iPone volume popup footprint;
  - subtle top highlight and bottom lowlight;
  - enough interior contrast for white and lavender text.
- `apps/bitmaps/native/loadingpopup_spinner.16x192x16.bmp`
  - 12 frames, 16x16 each, vertical strip like existing `LoadingStatus.bmp`;
  - iPone purple/lavender accent;
  - visible on the dark popup card.
- `apps/bitmaps/native/loadingpopup_progress_track.117x5x16.bmp`
  - thin track matching the volume slider footprint.
- `apps/bitmaps/native/loadingpopup_progress_fill.117x5x16.bmp`
  - active fill with the same accent color family as the iPone volume slider.
- `apps/bitmaps/native/loadingpopup_progress_end.3x5x16.bmp`
  - end cap if the primitive/progressbar path cannot match the asset style.

Add the assets to `apps/bitmaps/native/SOURCES` behind an iPod Video class
condition, for example `LCD_WIDTH == 320`, `LCD_HEIGHT == 240`, and
`LCD_DEPTH >= 16`.

Add a repeatable generator under `tools/`, for example
`tools/generate_loading_popup_assets.sh`, using the same ImageMagick style
already used by `tools/generate_springpod3_assets.sh`. The generator should:

- write deterministic BMP3/native-compatible outputs;
- optionally write PNG previews beside temporary outputs for review;
- document source colors and dimensions in comments;
- regenerate the full first asset set from scratch;
- avoid requiring private or external image files.

The final assets should be checked into the repo so firmware builds do not
depend on ImageMagick.

## First Callers

Migrate only the high-value, low-risk callers first:

1. `apps/playlist_viewer.c`
   - Replace the repeated `splashf(0, str(LANG_PLAYLIST_SEARCH_MSG), ...)`
     during playlist search with the new loading helper.
   - Preserve voice timing, abort polling, and final result behavior.

2. `apps/playlist.c`
   - Update `display_playlist_count()` so insert/save count feedback uses the
     loading helper while preserving spoken count announcements.

3. `apps/filetree.c` and `apps/plugin.c`
   - Consider replacing simple `splash(0, ID2P(LANG_WAIT))` wait states only
     after the first two playlist call sites pass simulator review.

Do not migrate all `splash(0, ...)` uses in one change. Some zero-tick splashes
are warnings or persistent status messages, not loading states.

## Risk Analysis

### Asset pipeline risk

Core splash rendering cannot depend on WPS/SBS runtime assets. Generated popup
assets must be compiled native bitmaps or the firmware may fail before themes
are loaded. Keep a primitive fallback for targets without matching compiled
assets.

### Shared UI primitive risk

`splash.c` is a shared primitive used across core apps and plugins. A visual bug
there can affect many screens. Keep the generic `splash()` behavior compatible
and put new loading-specific visuals behind a typed helper.

### Early boot/theme readiness risk

Some splashes happen while Rockbox is still scanning disk or before theme state
is fully reliable. Loading indicators must use only screen primitives and
default viewport/font state already used by `splash_internal()`.

### Small display risk

Rockbox supports displays much smaller than iPod Video. A modern panel that
looks good at 320x240 can clip on small screens. The implementation must retain
the old border/inverse fallback for monochrome and should disable decorative
extras when the overlay viewport is too small.

### Text and localization risk

Loading strings are localized and can be longer than English. The design must
not rely on fixed English phrase lengths, and it must keep the existing wrap
behavior. Count and abort hints must remain visible when possible.

### Voice UX risk

The loading UI must not cause repeated or interrupted voice output. Keep
existing caller-side talk throttling and `cond_talk_ids_fq()` behavior.

### Performance risk

Loading popups can be drawn inside tight loops. Animated indicators can become
expensive on slow targets if they redraw too often. Reuse the existing 20 fps
limit and avoid runtime bitmap decode, heap allocation, or full-screen redraws.
Compiled bitmap blits are acceptable for the iPod Video target if simulator
profiling shows no visible slowdown.

### Firmware size risk

Compiled native bitmaps increase firmware size. Keep the first asset set small:
one card, one compact spinner strip, and only progress strip pieces that clearly
look better than primitive drawing.

### Behavioral risk

Some zero-tick splashes are not loading states. Blanket migration could make
warnings or confirmations look like spinners. Migrate only known loading loops
first.

## Implementation Plan

### Phase 1: Stabilize current modern splash/progress rendering

- Review `apps/gui/splash.c` for color constants, small-screen bounds, and
  text-fit behavior.
- Keep the existing modern panel and progress pill as color-screen-only paths.
- Add a small internal helper for common panel geometry so message, loading,
  and progress paths share dimensions.
- Confirm old fallback rendering remains reachable for depth <= 1 screens.

### Phase 2: Generate popup assets

- Add a deterministic asset generator under `tools/`.
- Generate the first iPod Video / 5G asset set.
- Add the assets to `apps/bitmaps/native/SOURCES` behind 320x240 color guards.
- Include generated headers in `apps/gui/splash.c` only when the assets are
  available.
- Keep the existing primitive modern panel as the fallback.

### Phase 3: Add loading helper API

- Add `splash_loadingf()` and optionally `splash_loading_countf()` to
  `apps/gui/splash.h`.
- Implement them in `apps/gui/splash.c` using the existing `splash_internal()`
  layout path plus the generated card and activity indicator on supported color
  screens.
- Add rate limiting equivalent to `splash_progress()`.
- Do not expose complex styling knobs to callers.

### Phase 4: Migrate playlist loading states

- Change `apps/playlist_viewer.c` playlist search to use the loading helper for
  `LANG_PLAYLIST_SEARCH_MSG`.
- Change `apps/playlist.c` `display_playlist_count()` to use the loading helper
  for insert/save count updates.
- Keep existing voice announcement throttling and `OFF to abort` text.

### Phase 5: Expand selectively

After simulator review, consider migrating:

- `apps/plugin.c` plugin load wait;
- `apps/filetree.c` settings/theme/language load waits;
- `apps/tree.c` and `apps/main.c` disk scanning wait;
- `apps/onplay.c` file/context operation waits.

Each migration should verify that the splash represents loading, not an error
or confirmation.

## Validation

Build and test in the iPod Video simulator first.

Required simulator checks:

- Start Rockbox with iPone selected.
- Open a playlist and run playlist search so `Loading tracks...` appears.
- Confirm the popup resembles the iPone volume overlay: compact centered card,
  generated backdrop, small status glyph/activity indicator, and thin active
  strip.
- Insert a folder or database selection into a playlist and confirm count
  feedback remains readable.
- Enter Music/Database while the database is building or updating and confirm
  determinate progress uses the same modern card language and still reads as
  progress.
- Open a plugin or load a settings/theme file if those call sites were touched.
- Confirm no clipped text, overlapping panel content, or stale panel remnants.
- Confirm abort actions still work during playlist search and long inserts.

Suggested commands:

```bash
tools/simulator_first_gate.sh --target ipodvideo --smoke --manual-checklist
tools/simulator_first_gate.sh --target ipodvideo --skip-build --theme-tests --smoke --manual-checklist
```

## Acceptance Criteria

- `Loading tracks...` uses a modern volume-overlay-style popup on iPod Video /
  5G color screens.
- Any assets needed for that popup are generated by a repeatable tool and
  checked into the Rockbox bitmap pipeline.
- Database scan/commit progress keeps the same modern popup language and remains
  clearly determinate.
- Timed success/error splashes still display as normal message panels.
- Voice output for loading, count, and progress states remains no noisier than
  before.
- Monochrome and very small displays still get readable fallback splashes.
- No changed loading loop allocates memory or decodes runtime image assets per
  update.
- iPod Video simulator screenshots show no clipped strings, overlap, or redraw
  artifacts.

## Open Questions

- Should the activity indicator animate continuously, or only advance when the
  caller updates the loading text?
- Should plugin API expose `splash_loadingf()`, or should plugins continue to
  use generic `splash()`/`splash_progress()` until core behavior is proven?
- Should the loading helper reserve numeric width for common count strings, or
  should callers provide an expected maximum count when they know it?
