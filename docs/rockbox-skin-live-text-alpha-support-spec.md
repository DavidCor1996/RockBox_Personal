# Rockbox Skin Live Text Alpha Support Spec

## Goal

Add real alpha/transparency support for live Rockbox skin text so the RockPod
lockscreen clock alpha slider can render translucent time/date text instead of
only exporting a pre-blended RGB approximation.

The first consumer is the RockPod iPone Designer lockscreen clock, but the
implementation should be general enough for skin viewports without breaking
existing WPS/SBS/FMS theme rendering.

## Current Limitation

Rockbox skin foreground colors are currently parsed as RGB only:

- `%Vf(FFFFFF)` sets a six-digit foreground color.
- `apps/misc.c::hex_to_rgb()` reads six hex digits and packs them with
  `LCD_RGBPACK(red, green, blue)`.
- The skin renderer eventually calls `display->set_foreground(...)` and draws
  text directly with that opaque color.

This means `%Vf(80FFFFFF)` or `%Vf(FFFFFF80)` cannot currently express alpha.
The parser either treats only the first six digits as color or rejects invalid
forms depending on the path.

RockPod's current slider therefore simulates alpha by blending the clock color
against the sampled wallpaper during export. That is useful, but it is not true
transparency and cannot adapt to dynamic backgrounds or animated/conditional
layers.

## Scope

Applies to:

- Color LCD targets first, especially iPod Video/5G and iPod Classic/6G.
- Skin text drawn through WPS/SBS/FMS viewport rendering.
- Foreground text color alpha for live text lines, initially the lockscreen
  clock/date lines.
- Simulator and checkwps validation.

Does not apply initially to:

- Mono or 2-bit grayscale targets.
- Bitmap image alpha in skin files.
- Runtime alpha for all drawing primitives.
- Plugin drawing APIs.
- General LCD framebuffer compositing outside the skin engine.

## Compatibility Requirements

- Existing `%Vf(RRGGBB)` skins must render exactly as before.
- Existing invalid color strings should not become silently accepted unless they
  match the new documented alpha syntax.
- Themes without alpha must not pay a meaningful performance cost.
- Alpha support must be optional at render time and safe on targets that do not
  expose a practical blend path.
- checkwps must validate the new syntax and reject malformed alpha colors.

## Proposed Syntax

Support alpha as an optional fourth channel on skin viewport foreground colors:

```text
%Vf(RRGGBB)
%Vf(RRGGBBAA)
```

Where:

- `RRGGBB` is the existing opaque RGB color.
- `AA` is alpha, `00` fully transparent and `FF` fully opaque.

Examples:

```text
%Vf(FFFFFFFF)  # white, opaque
%Vf(FFFFFF80)  # white, 50% alpha
%Vf(FFFFFF33)  # white, 20% alpha
```

Use `RRGGBBAA`, not `AARRGGBB`, because it extends the existing six-digit value
without changing the meaning of the first six characters.

## Data Model

Extend skin parsed color data to carry:

- packed RGB color,
- alpha byte,
- whether alpha was explicitly provided.

Recommended shape:

```c
struct skin_color
{
    unsigned int colour;
    unsigned char alpha;
    bool has_alpha;
    bool is_default;
};
```

Exact structure names should follow existing skin parser conventions. The key
rule is that opaque colors continue through the current fast path.

## Parser Plan

Add a parser helper instead of changing all `hex_to_rgb()` callers:

- Keep `hex_to_rgb()` as RGB-only for settings, file types, and existing users.
- Add a skin-specific helper, for example:
  - `parse_skin_color(enum screen_type screen, char *text, int *rgb, unsigned char *alpha, bool *has_alpha)`
- Accept exactly six or eight hex digits on color LCDs.
- For six digits:
  - `alpha = 0xff`
  - `has_alpha = false`
- For eight digits:
  - parse first six as RGB,
  - parse last two as alpha,
  - `has_alpha = true`
- Reject all other lengths.

Use the new helper in `apps/gui/skin_engine/skin_parser.c` for `%Vf` and any
other skin-only color tags chosen for alpha support.

## Renderer Plan

### Phase 1: Alpha For Skin Text Only

When a viewport has no explicit alpha or has `alpha == 0xff`, keep the existing
render path:

```c
display->set_foreground(skin_viewport->vp.fg_pattern);
```

When a viewport foreground has alpha in the implemented first pass:

1. Resolve the foreground RGB and alpha.
2. Blend the foreground RGB against the viewport background color.
3. Draw text through the existing skin text renderer with the blended color.

The implementation should avoid changing global LCD draw mode semantics for
all callers.

### Blend Source

The first implementation blends against `skin_viewport->vp.bg_pattern`:

```text
out = src * alpha + viewport_bg * (1 - alpha)
```

This makes alpha visibly work for generated clock skins while keeping the
existing text renderer and scroll behavior intact. It is not yet full
per-framebuffer-pixel translucency over arbitrary already-drawn content.

A later, more complete implementation can blend against the current framebuffer
pixel under each glyph pixel:

```text
out = src * alpha + dst * (1 - alpha)
```

This gives true transparency over:

- wallpaper bitmaps,
- generated glass panels,
- album art/notification cards behind lockscreen text,
- conditional layers that are already drawn before the text.

### Candidate Implementation Options

Option A: LCD text alpha primitive

- Add a display driver/screen-access function for alpha text, e.g.
  `putsxy_alpha(x, y, text, rgb, alpha)`.
- Implement for color LCD targets by reading the framebuffer pixel, blending,
  and writing back for glyph mask pixels.
- Keep existing `putsxy` untouched.

Option B: Skin-engine local glyph blender

- In skin rendering, access font glyph bitmaps and blend them into the main
  framebuffer directly.
- This is more contained but risks duplicating LCD/font rendering behavior.

Recommended first pass: Option A if the existing LCD/framebuffer API has a
clean place for it; otherwise Option B scoped to skin text only.

## RockPod Integration

After Rockbox supports `%Vf(RRGGBBAA)`, update RockPod:

- Keep `lockscreen_clock.color` as `RRGGBB`.
- Keep `lockscreen_clock.opacity` as `20..100`.
- Export alpha-capable skins as:

```text
%Vf(FFFFFF52)
```

Where:

```text
AA = round(opacity * 255 / 100)
```

- Stop pre-blending colors for targets that advertise alpha skin support.
- Keep RGB pre-blending as fallback for older firmware or non-alpha targets.

Add a capability flag to RockPod metadata/export settings:

```json
{
  "skin_text_alpha": true
}
```

Default should remain fallback/pre-blend until the deployed firmware is known to
support the new syntax.

## Rendering Safety

Do not change existing theme rendering by default:

- Six-digit `%Vf` must stay opaque and use the current path.
- Alpha path only activates when the color string has eight hex digits and the
  alpha byte is not `FF`.
- If alpha rendering fails or is unsupported on a target, treat the color as
  opaque RGB rather than crashing or leaving stale draw state.
- Avoid modifying viewport dimensions, conditional evaluation, image loading,
  font selection, or line layout.

## Performance Constraints

The lockscreen clock is small, but skin rendering can be frequent. Keep alpha
work bounded:

- Blend only glyph pixels, not the whole viewport.
- Skip alpha work for `AA == FF`.
- Skip drawing entirely for `AA == 00`.
- Cache parsed alpha in skin data; do not parse hex during render.
- Benchmark simulator and iPod 5G/6G lockscreen redraws.

## Tests

Parser/checkwps tests:

- `%Vf(FFFFFF)` remains valid and opaque.
- `%Vf(FFFFFF80)` is valid and stores alpha `0x80`.
- `%Vf(FFFFFFFF)` is valid and equivalent to opaque white.
- `%Vf(FFFFFF00)` is valid and transparent.
- `%Vf(FFFFF)`, `%Vf(FFFFFFFFF)`, and non-hex strings are rejected.

Renderer tests/manual checks:

- Existing iPone and generated RockPod themes render unchanged with six-digit
  colors.
- Lockscreen clock at 100%, 82%, 50%, and 20% visibly blends over wallpaper.
- Text blends over both bitmap wallpaper and generated `%dr(...)` backgrounds.
- AOD, charge screen, WPS lockscreen, SBS lockscreen, and menu views do not
  regress.
- Full-art right pane and mini-player conditional layers remain intact.

RockPod tests:

- Alpha-capable export writes `%Vf(RRGGBBAA)`.
- Fallback export still writes six-digit pre-blended RGB.
- Existing saved variants without opacity continue to export.

## Rollout Plan

1. Implement parser support behind skin-engine handling, keeping RGB behavior
   unchanged.
2. Add alpha text rendering for simulator/color LCD.
3. Validate with checkwps and simulator screenshots.
4. Enable RockPod alpha export behind a capability flag.
5. Test on iPod 5G and 6G hardware/simulator.
6. Flip RockPod default to alpha export only after firmware support is present
   in the target build.

## Open Questions

- Should alpha apply only to `%Vf`, or should `%Vb` eventually support alpha
  for translucent viewport backgrounds?
- Should `AA == 00` make text fully skipped, or should it still reserve layout
  exactly as current text does?
- Is a generic LCD alpha text primitive acceptable for all color targets, or
  should the first implementation stay inside the skin engine?
- Should RockPod expose a compatibility toggle per deployed firmware version?
