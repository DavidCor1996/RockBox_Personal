# RockPod Designer Lockscreen Clock Alpha Slider Spec

## Goal

Add a transparency/alpha control for the lockscreen clock font in RockPod's
iPone Designer. The control should let users soften the lockscreen time against
custom wallpapers while keeping the Rockbox clock text live.

This is a RockPod Designer change first. It should not require a Rockbox
runtime setting or device-side theme rewriting.

## Scope

Applies to:

- RockPod iPone Designer variants.
- iPod Video/5G and iPod Classic/6G 320x240 iPone-class exports.
- The lockscreen clock time and date lines generated from
  `lockscreen_clock`.
- Designer preview, saved variant JSON, generated SBS/WPS files, and deployed
  theme bundles.

Does not apply to:

- Menu/SBS right-side clock color.
- Mini-player metadata color.
- Charge screen clock unless a later charge-clock spec opts in.
- AOD unless explicitly enabled in a follow-up.

## Current State

The designer already exposes lockscreen clock controls in
`rockpod/ui/theme_designer.py`:

- font
- color
- alignment
- x/y position
- width/height box

The service already has `DEFAULT_LOCKSCREEN_CLOCK["opacity"] = 82` in
`rockpod/services/theme_designer.py`, but the value is not normalized by
`_normalize_lockscreen_clock()`, not surfaced in the UI, and not used when
generating skin foreground colors.

Clock export currently emits RGB-only skin foreground colors:

```text
%Vl(iPoneLockscreen,... )%Vf(FFFFFF)%ac%cl:%cM %cP
```

Rockbox skin foreground colors are not a runtime alpha channel. The first
implementation must therefore simulate font alpha during RockPod export by
compositing the selected clock color against the selected lockscreen wallpaper
or a deterministic fallback background.

## User Experience

Add one control to the existing "Lockscreen Clock" section:

- Label: `Font alpha`
- Widget: horizontal slider plus numeric percent spin box.
- Range: `20` to `100`.
- Step: `1`, page step `5`.
- Default: `82`.
- Display suffix: `%`.

Behavior:

- Moving the slider updates the embedded preview immediately.
- The color button continues to choose the source clock color.
- The alpha slider controls how strongly that source color is blended into the
  wallpaper/background for exported Rockbox `%Vf(...)` colors.
- `100%` exports the selected color unchanged.
- Lower values produce a softer blended clock color.
- Do not allow `0%` in the UI because fully invisible live clock text is easy
  to save accidentally.

## Data Model

Keep the field in the existing nested clock block:

```json
{
  "lockscreen_clock": {
    "font_rel": "fonts/66-Cantarell-Light.fnt",
    "x": 0,
    "y": 32,
    "width": 320,
    "height": 55,
    "align": "center",
    "color": "FFFFFF",
    "opacity": 82
  }
}
```

Normalization rules:

- Accept integer-like values.
- Clamp to `20..100`.
- If missing or invalid, use `DEFAULT_LOCKSCREEN_CLOCK["opacity"]`.
- Preserve backward compatibility with saved variants that do not have the
  field.

## Export Semantics

Define opacity as source-over-background compositing:

```text
effective_rgb = round(background_rgb * (1 - alpha) + source_rgb * alpha)
alpha = opacity / 100
```

Use `effective_rgb` for generated `%Vf(...)` clock/date foreground colors.

Background selection:

1. If `variant["wallpaper_source"]` points to a readable image, resize/crop it
   exactly like the lockscreen export path and sample the average RGB under the
   clock viewport.
2. If the viewport cannot be sampled, use the average RGB of the full fitted
   wallpaper.
3. If no wallpaper is available, use `000000` for dark appearance mode and
   `FFFFFF` for light appearance mode.

Sampling region:

- Time line: use the normalized clock box `(x, y, width, effective_height)`.
- Date line: use `(x, date_y, width, 18)`.
- Clamp regions to `320x240`.
- Use the same effective height logic already used by
  `_replace_lockscreen_clock_block()` so large font slots sample the area the
  text actually occupies.

Generated skin behavior:

- When `opacity == 100`, keep the selected clock `color` unchanged.
- When `opacity < 100`, replace only the exported `%Vf(...)` color for the
  lockscreen time/date lines with the computed effective RGB.
- Keep the saved source color in variant JSON unchanged.
- Do not add nonstandard alpha syntax to SBS/WPS files.

## Glass And Outline Interaction

Solid style:

- Apply opacity compositing to the time and date foreground colors.

Soft shadow:

- Apply opacity to the foreground color.
- Keep any shadow/contrast helper derived from the effective foreground color,
  not the raw source color.

Outline style:

- Apply opacity to the main foreground color.
- Compute the outline/contrast color from the effective foreground color.

Glass style:

- Apply opacity to the live text foreground color.
- Do not reuse this slider for glass panel strength. `glass_strength` remains a
  separate control/field.
- If generated glass makes the sampled background brighter, sample from the
  final glass crop when available; otherwise sample from the wallpaper crop.

## Implementation Plan

1. Update `ThemeDesignerService._normalize_lockscreen_clock()` to normalize
   `opacity`.
2. Add a small helper such as `_effective_lockscreen_clock_color(...)` that
   returns the composited RGB for a given clock/date region.
3. Pass the variant wallpaper/appearance context into
   `_replace_lockscreen_clock_block()` or return generated lines from a new
   helper that has that context.
4. Use effective RGB values in generated time/date `%Vf(...)` lines.
5. Add a `QSlider` and `QSpinBox` pair to the `Lockscreen Clock` form in
   `ThemeDesignerWidget`.
6. Load/save `lockscreen_clock.opacity` in `load_variant()` and
   `current_variant_data()`.
7. Update the embedded preview path to use the same effective RGB calculation
   as export, so preview and installed theme match.

## Tests

Add or update `rockpod/tests/test_theme_designer.py` tests for:

- Missing opacity defaults to `82`.
- Opacity values below/above range clamp to `20`/`100`.
- Saved variant JSON preserves `lockscreen_clock.opacity`.
- Generated SBS exports the raw color at `100%`.
- Generated SBS exports a composited color at a lower opacity with a known test
  wallpaper.
- Only lockscreen clock/date `%Vf(...)` lines change; right-pane clock,
  mini-player text, menu text, charge clock, and AOD remain unchanged.
- UI `current_variant_data()` includes the slider value.

Manual verification:

- Generate one dark wallpaper variant at `100%`, `82%`, and `45%`.
- Generate one light wallpaper variant at `100%`, `82%`, and `45%`.
- Check simulator screenshots for lockscreen no playback and lockscreen with
  music.
- Confirm the clock is still readable and live, not baked into the wallpaper.

## Acceptance Criteria

- The designer exposes a `Font alpha` control in the lockscreen clock section.
- Existing variants without the field continue to load and export.
- New variants persist the value under `lockscreen_clock.opacity`.
- Generated SBS/WPS files stay valid RGB-only Rockbox skins.
- Preview and exported theme use the same composited font color.
- `100%` is visually identical to the current behavior.
- Lower alpha values visibly soften the clock against the selected lockscreen
  wallpaper without changing unrelated theme colors.

## Follow-Ups

- Add an optional readability warning when the effective foreground color drops
  below the configured contrast threshold.
- Consider a separate AOD alpha control after lockscreen behavior is stable.
- Consider paired date alpha only if users need date text dimmer than the time.
