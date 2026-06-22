# iPone Lockscreen Wallpaper Customization Spec

## Scope

Add an iOS 26-inspired lockscreen customization section to the RockPod wallpaper
manager for iPone-class themes. The feature should let users tune the lockscreen
clock and related wallpaper presentation without hand-editing SBS/WPS files.

The first supported targets are:

- iPod Video/5G and iPod Classic/6G at 320x240.
- `iPone` and `iPone7G` SBS/WPS asset families.
- RockPod's wallpaper manager and generated wallpaper pipeline.

This is a spec only. Implementation should happen in phases because the current
Rockbox-side lock wallpaper switching API is intentionally disabled after device
renderer regressions.

## iOS 26 Reference

The design target is inspired by these iOS 26 lockscreen behaviors:

- Lock Screen time uses Liquid Glass and adapts to the wallpaper subject.
- San Francisco numerals dynamically scale weight, width, and height to fit the
  scene.
- Users can edit clock font, color, size, and a Glass/Solid style.
- Later iOS 26 builds added more transparency control for clock readability.
- Spatial wallpaper effects add depth/motion, but for Rockbox this should be
  translated into pre-rendered crop/parallax variants instead of runtime 3D.

Reference links:

- Apple Newsroom, June 9 2025:
  https://www.apple.com/newsroom/2025/06/apple-introduces-a-delightful-and-elegant-new-software-design/
- Apple Developer overview:
  https://developer.apple.com/documentation/technologyoverviews/liquid-glass
- Tom's Guide iOS 26 lockscreen customization walkthrough:
  https://www.tomsguide.com/phones/iphones/how-to-customize-your-iphone-lock-screen-in-ios-26

## Goals

- Put clock customization in the wallpaper section, because the controls are
  visually tied to the chosen lockscreen image.
- Support clock height, vertical position, horizontal alignment, font family,
  color, shadow, and style.
- Provide a safe "Liquid Glass-like" style using pre-rendered overlays and
  contrast-aware text, not expensive runtime blur/refraction.
- Generate mini-player blur/tint assets from the selected lockscreen wallpaper
  so the music card matches the lockscreen color scheme.
- Preserve readable lockscreen behavior on 5G and 6G.
- Keep charge screen, AOD, WPS lockscreen, SBS lockscreen, and menu redraw
  behavior separate.
- Generate simulator previews before writing a device bundle.
- Avoid modifying live theme source files in place unless the user applies a
  generated bundle.

## Non-Goals

- No real-time blur or full-scene refraction in Rockbox.
- No per-frame lockscreen animation on device.
- No generic iOS clone UI inside Rockbox menus.
- No device-side font rasterization pipeline beyond existing Rockbox fonts.
- No runtime subject segmentation on the iPod.

## User Experience

Add a "Lock Screen" subsection inside the current wallpaper manager.

Controls:

- Wallpaper picker: existing lock wallpaper list.
- Clock position:
  - Top
  - Center
  - Lower
  - Custom Y
- Clock height:
  - Compact
  - Standard
  - Tall
  - Poster
  - Custom pixels
- Clock alignment:
  - Left
  - Center
  - Right
- Clock font:
  - Current iPone default
  - Adobe Helvetica Bold
  - Cantarell Bold
  - Light
  - Numeric poster
- Clock color:
  - White
  - Black
  - Wallpaper sampled light
  - Wallpaper sampled dark
  - Custom RGB
- Clock style:
  - Solid
  - Soft shadow
  - Outline
  - Glass
  - Glass tinted
- Glass strength:
  - Off
  - Low
  - Medium
  - High
- Date placement:
  - Follow clock
  - Above
  - Below
  - Hidden
- Readability guard:
  - Auto contrast on/off.
  - Minimum contrast threshold.
- Preview:
  - Show lockscreen, AOD, charge, and music-playing lockscreen tabs.
  - Show 5G and 6G preview selectors when both profiles exist.

## Data Model

Extend the profile/theme customization model with a nested lockscreen block:

```json
{
  "lockscreen_customization": {
    "wallpaper_id": "lockscreen-besties-trio-v5.bmp",
    "clock": {
      "x": 0,
      "y": 32,
      "width": 320,
      "height": 55,
      "align": "center",
      "font": "35-Adobe-Helvetica-Bold.fnt",
      "style": "glass",
      "color": "FFFFFF",
      "shadow": "soft",
      "glass_strength": "medium",
      "opacity": 82
    },
    "date": {
      "mode": "below",
      "y": 101,
      "font": "16-Adobe-Helvetica-Bold.fnt",
      "color": "FFFFFF"
    },
    "readability": {
      "auto_contrast": true,
      "min_contrast": 4.5,
      "sample_region": "clock_box"
    },
    "mini_player": {
      "style": "matched_blur",
      "blur_strength": "medium",
      "tint_source": "wallpaper",
      "tint_color": "2D2936",
      "text_color": "FFFFFF",
      "secondary_text_color": "C8BED7"
    }
  }
}
```

Store this in the RockPod profile first. Only export flattened Rockbox assets and
theme files to the device.

## Rockbox Theme Export

Generate a staged SBS file instead of editing the repo source in place:

- Input source: `wps/iPone.sbs` or `wps/iPone7G.sbs`.
- Output staging path:
  `rockpod/.wallpaper_clock/<profile-id>/<theme-name>.sbs`.
- Device path:
  `.rockbox/wps/<theme-name>.sbs`.

Generated SBS clock viewport examples:

```text
%Vl(iPoneLockscreen,0,32,-,55,8)%Vf(FFFFFF)%ac%cl:%cM %cP
%Vl(iPoneLockscreen,0,101,-,20,6)%Vf(FFFFFF)%ac%cb %cd
```

Rules:

- Keep lockscreen viewports inside the hold/lockscreen branch only.
- Do not alter SBS menu/list viewports.
- Do not alter full-art right-pane drawing.
- Do not alter charge clock unless the user is editing charge wallpaper.
- Preserve AOD as a separate style with its own contrast choices.

## Liquid Glass Simulation

Rockbox cannot cheaply blur/refract the wallpaper under text on these devices.
Use a pre-rendered approximation:

1. Render the selected wallpaper to the target resolution.
2. Crop the clock bounding box.
3. Generate one or more overlay assets:
   - Light glass highlight mask.
   - Tinted translucent fill approximation.
   - Dark/bright edge stroke.
   - Optional background-dim strip only inside the clock glyph bounds.
4. Export a clock style asset or glyph-shadow companion bitmap.
5. In SBS, draw:
   - Wallpaper.
   - Optional clock glass mask.
   - Clock text.
   - Small highlight/edge overlay if readability remains acceptable.

Recommended first implementation:

- Do not render text as bitmap unless Rockbox font choices are insufficient.
- Use Rockbox text for time so it remains live.
- Use generated bitmap only for the glass treatment behind/around the text.
- Use 2-3 fixed glass strengths to avoid large asset sets.

Fallback:

- If contrast sampling fails or the selected wallpaper is too busy, downgrade
  `glass` to `soft shadow` and warn in the preview.

## Mini-Player Blur Matching

The lockscreen mini-player must be generated from the same wallpaper palette as
the clock treatment. A fixed dark card will look disconnected when the user picks
lighter, warmer, or more colorful wallpapers.

Host-side generation:

1. Sample the selected lockscreen wallpaper under the mini-player card bounds.
2. Downscale and blur that crop in RockPod.
3. Mix in a readability tint derived from the wallpaper palette:
   - dark wallpapers: lighter translucent lift,
   - light wallpapers: darker translucent scrim,
   - saturated wallpapers: desaturate the card base before tinting.
4. Generate the card layers as BMP assets:
   - card fill,
   - top highlight,
   - bottom lowlight,
   - optional outside shadow,
   - optional glass sparkle/highlight strip.
5. Export text colors alongside the generated card metadata.

Device-side SBS usage:

- Draw the generated card bitmap before album art and text.
- Keep live album art and live text as Rockbox-rendered layers.
- Use generated text colors only when they pass contrast thresholds.
- Fall back to the current dark mini-player card if no generated card exists.

Suggested output paths:

```text
.rockbox/wps/iPone/LockMiniCardGenerated.bmp
.rockbox/wps/iPone/LockMiniCardHighlight.bmp
.rockbox/wps/iPone/LockMiniCardShadow.bmp
.rockbox/wps/iPone7G/LockMiniCardGenerated.bmp
.rockbox/wps/iPone7G/LockMiniCardHighlight.bmp
.rockbox/wps/iPone7G/LockMiniCardShadow.bmp
```

Acceptance:

- The mini-player card should visually belong to the selected lockscreen
  wallpaper while keeping title/artist readable.
- No solid black rectangle in glass mode.
- No unbounded blur work on device; all blur is generated on the host.
- Music-playing lockscreen screenshots must be captured for every generated
  style preset.

## Spatial/Depth Approximation

No runtime spatial scenes on device. Support host-side composition only:

- Optional subject mask import from generated assets.
- Preview a depth crop in RockPod.
- Export a static lockscreen wallpaper where the subject appears in front of the
  clock when safe.
- If the subject overlaps the time too much, offer a lower clock height or move
  the date below.

This gives the visual impression of iOS adaptive time without asking Rockbox to
do segmentation or z-order analysis.

## Implementation Phases

### Phase 1: Spec-backed preview model

- Add `lockscreen_customization` to RockPod profile data.
- Add wallpaper manager controls behind a feature flag.
- Generate preview-only composites in `rockpod/.theme_designer/generated/`.
- Add tests for profile serialization and preview metadata.

### Phase 2: Safe SBS export

- Replace current hard-coded clock-position string replacement with a structured
  SBS template/export helper.
- Generate staged SBS files for iPone/iPone7G.
- Keep the repo theme source unchanged until an explicit "Apply" bundle.
- Add tests proving only lockscreen/AOD clock lines change.

### Phase 3: Device bundle

- Extend `IPoneWallpaperService.build_apply_bundle()` to include:
  - selected wallpaper,
  - generated glass assets,
  - generated mini-player card blur/tint assets,
  - staged SBS,
  - config metadata for the active profile.
- Write to both theme asset folder and fallback current wallpaper path where the
  existing bundle expects it.

### Phase 4: Liquid Glass style

- Add host-side glass mask generation.
- Add host-side mini-player blur/tint generation from the same wallpaper sample.
- Add contrast sampling and automatic downgrade.
- Verify on 5G and 6G simulators with screenshots.
- Keep all generated assets small and target-resolution-specific.

### Phase 5: On-device menu polish

- Add Rockbox settings only for low-risk choices:
  - clock layout preset,
  - style preset,
  - wallpaper preset.
- Do not expose arbitrary pixel values on device.
- On-device changes should select among pre-generated SBS/assets, not rewrite
  theme files.

## Testing

Unit tests:

- `rockpod/tests/test_ipone_wallpapers.py`
  - list candidates,
  - serialize customization model,
  - generate staged SBS,
  - verify only intended viewport lines change,
  - verify generated bundle includes the staged SBS and assets.

Source tests:

- Assert `iPone.sbs` and `iPone7G.sbs` still contain lockscreen, AOD, charge,
  and normal SBS branches.
- Assert full-art right pane and miniplayer branches remain unchanged.

Simulator gates:

- 5G lockscreen no playback.
- 5G lockscreen with music.
- 6G lockscreen no playback.
- 6G lockscreen with music.
- Charge screen.
- AOD/backlight-on-hold-off path.
- Return from lockscreen to SBS menu without menu drawing over wallpaper.

Visual acceptance:

- Time remains readable on light and dark wallpapers.
- Clock does not overlap hold icon, battery, date, notification card, or slide
  affordance.
- Glass style never creates a solid black box.
- Mini-player blur/tint matches the selected lockscreen color scheme and keeps
  live music text readable.
- 5G and 6G render the same selected preset unless target-specific overrides
  are present.

## Risks

- Current Rockbox-side `ipone_lock_wallpaper_apply()` is disabled for stability.
  Treat host-side bundle generation as the source of truth until live switching
  is proven safe again.
- Rockbox skin syntax is brittle. Template-based generation is safer than
  repeated ad hoc string replacement.
- Large bitmap style assets can increase theme load time. Keep glass assets
  small and only generate the selected preset.
- Low contrast glass clocks are visually attractive but easy to make unreadable
  on 320x240 LCDs. Auto contrast should win over fidelity.
- Wallpaper-matched mini-player blur can overfit a busy crop. Desaturate and
  tint before export when the sampled region is too noisy.

## Open Questions

- Should each wallpaper carry its own clock preset, or should the active profile
  have one global lockscreen preset?
- Should generated glass assets be shared between iPone and iPone7G when the
  selected wallpaper and target resolution match?
- Should charge wallpaper get the same clock customization controls later, or
  stay separate because the charge screen has different layout constraints?
- Should the first public UI expose custom pixels, or keep only named presets
  until simulator proof is reliable?
