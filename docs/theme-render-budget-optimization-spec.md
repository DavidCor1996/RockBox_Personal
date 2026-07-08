# Theme Render Budget Optimization Spec

## Goals

- Catch iPone Designer skins that can create avoidable redraw or parse pressure on
  iPod 6G before they are installed.
- Keep the current compact liquid-glass lockscreen clock within budget.
- Prevent stale generated clock viewports from reappearing in WPS/SBS output.
- Warn users in RockPod before deploying risky generated skins.

## Budget Checks

- Stale `iPoneClock*` generated viewports are critical because they can remain
  hidden, parse-heavy, or outside the active WPS display chain.
- More than six translucent lockscreen clock time layers is over budget.
- Large stretched clocks at 90px or above and 150% stretch or above are only
  allowed when they stay within the compact six-layer clock budget.
- Very high viewport counts are warned because they increase skin parse and redraw
  work.
- `SbsAnimPulse` is warned because it can force repeated right-pane/full-art redraw
  activity.
- Excessive bitmap load declarations are informational so they can be audited
  without blocking normal iPone themes.

## Implementation

- `ThemeDesignerService.render_budget_recommendations_for_bundle()` scans generated
  WPS/SBS assets and returns structured render-budget issues.
- Clock replacement now removes stale `iPoneClockBase`, `iPoneClockGlass`,
  `iPoneClockStretch`, and `iPoneClockShadow` lines and display tags.
- WPS `Lockscreen` display chains receive the same stale generated-clock cleanup as
  SBS `iPoneLockscreen`.
- RockPod's iPone Designer deploy flow displays a render-budget warning dialog for
  critical/warn issues before the existing readability check.
- Simulator builds include an opt-in `ROCKBOX_SKIN_PROFILE=1` viewport redraw
  summary. It prints per-viewport full/partial/idle/skipped render counts every
  five seconds and is compiled out of native firmware builds.
- Alpha-clock viewport metadata is parsed once per viewport render pass and
  reused by the full-redraw and line-draw paths, avoiding repeated label scans
  without changing the generated liquid-glass output.

## Non-Goals

- No firmware driver, codec, filesystem, PCM, mixer, or storage changes.
- No automatic deletion of user themes from a mounted iPod.
- No broad plugin optimization or removal in this pass.
- No deployment or git push without explicit approval.
