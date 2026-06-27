# iPone Designer quick settings modernization spec

## Goal

Modernize the Rockbox quick settings screen for all iPone Designer themes using generated theme assets.

The screen should feel like part of the iPone visual system instead of the default Rockbox quickscreen. It should show a click wheel in the middle of the four quick setting options.

## Scope

Applies to:

- All themes generated through RockPod iPone Designer.
- iPod Video / 320x240 iPone Designer target first.
- Existing generated designer themes after they are regenerated/reinstalled.

Does not apply to:

- Stock `iPone` unless intentionally enabled later.
- Non-iPone themes.
- 3G/nano layouts until separate assets/layouts are specced.

## Current behavior

The quick settings screen is native Rockbox code in:

- `apps/gui/quickscreen.c`

It is not currently drawn from the iPone SBS/WPS skin files.

Current quickscreen drawing:

- Clears the parent viewport.
- Places four text/value regions: top, bottom, left, right.
- Draws small built-in monochrome arrow icons in the center.
- Uses generic viewport/font behavior, not iPone assets.

This means the correct implementation is not another SBS tag block. It needs:

- RockPod-generated bitmap assets.
- Rockbox native quickscreen drawing path that detects active iPone Designer themes and uses those assets.

## Design direction

Visual target:

- Full-screen iPone-style modal surface.
- Dark/light left-side mode compatibility.
- Accent color comes from the selected designer accent/highlight color.
- Center click wheel visually anchors the screen.
- Four quick setting options sit around the wheel.
- Text should be readable and use designer-selected colors.

Layout for 320x240:

- Background: full screen generated bitmap.
- Center wheel: centered around screen center, roughly `120x120`.
- Top option: above wheel.
- Bottom option: below wheel.
- Left option: left of wheel.
- Right option: right of wheel.
- Each option should have:
  - setting title
  - current value
  - subtle pill/card background or glow

Recommended positions:

- Wheel bounds: `100,60,120,120`
- Top card: `80,16,160,36`
- Bottom card: `80,188,160,36`
- Left card: `10,92,86,56`
- Right card: `224,92,86,56`

## Generated assets

RockPod should generate these assets into the iPone Designer SBS asset directory:

- `QuickSettingsBackdrop.bmp`
- `QuickSettingsWheel.bmp`
- `QuickSettingsWheelPressed.bmp`
- `QuickSettingsCard.bmp`
- `QuickSettingsCardActive.bmp`
- `QuickSettingsArrowUp.bmp`
- `QuickSettingsArrowDown.bmp`
- `QuickSettingsArrowLeft.bmp`
- `QuickSettingsArrowRight.bmp`

Minimum viable first pass:

- `QuickSettingsBackdrop.bmp`
- `QuickSettingsWheel.bmp`
- `QuickSettingsCard.bmp`

Asset folder:

- `/.rockbox/wps/<generated-sbs-name>/`

Example:

- `/.rockbox/wps/iPoneD-my-theme-a1b2c3/QuickSettingsWheel.bmp`

## Asset generation rules

Use designer inputs:

- `appearance_mode`
- `colors.background`
- `colors.foreground`
- `colors.selector_start`
- `colors.selector_end`
- `colors.selector_text`
- `colors.sbs_clock`
- `show_line_separators`

Backdrop:

- Dark mode: black/deep background with glass panels.
- Light mode: white/light background with soft gray panels.
- Should not use the main wallpaper by default.
- Should include subtle split/shadow language from the current SBS background so it feels consistent.

Wheel:

- Shape should read as an iPod click wheel.
- Outer ring: soft neutral, slight bevel.
- Inner button: darker/lighter center depending on mode.
- Direction marks should be subtle, not busy.
- Accent color can appear as a thin glow, selected ring, or active segment.

Cards:

- Rounded/pill-like panels.
- Use translucent/glass look baked into bitmap.
- Active card may use accent gradient.
- Text is still drawn by Rockbox, not baked, so cards must leave enough contrast.

## Rockbox implementation approach

Add an iPone Designer quickscreen path in:

- `apps/gui/quickscreen.c`

Detection:

- Active SBS path contains generated designer prefix:
  - `iPoneD-`
  - legacy `iPoneDesigner-`
  - optionally legacy `iPD-`

Behavior:

- If active theme is an iPone Designer theme and required assets exist, draw the custom quickscreen.
- If assets are missing or fail to load, fall back to the current generic quickscreen.

Do not:

- Add quickscreen drawing to SBS text.
- Preload these assets in the SBS skin.
- Depend on WPS/skin image buffers.

Reason:

- The iPone SBS is already near skin/image limits.
- Native quickscreen drawing avoids breaking the SBS parser and keeps fallback safe.

## Asset loading

Rockbox should derive the generated SBS asset folder from `global_settings.sbs_file`.

Example:

- SBS: `/.rockbox/wps/iPoneD-my-theme-a1b2c3.sbs`
- Asset folder: `/.rockbox/wps/iPoneD-my-theme-a1b2c3/`

Load:

- `QuickSettingsBackdrop.bmp`
- `QuickSettingsWheel.bmp`
- `QuickSettingsCard.bmp`

Implementation options:

- Use `read_bmp_file()` or existing bitmap-loading helpers if available in apps context.
- Cache loaded assets during quickscreen lifetime only.
- Free buffers before leaving quickscreen.

Fallback:

- If any required asset fails to load, call existing `gui_quickscreen_draw()`.

## Text rendering

Use current quickscreen item data:

- `qs->items[QUICKSCREEN_TOP]`
- `qs->items[QUICKSCREEN_BOTTOM]`
- `qs->items[QUICKSCREEN_LEFT]`
- `qs->items[QUICKSCREEN_RIGHT]`

Use existing value logic:

- `option_value_as_int()`
- `option_get_valuestring()`

Typography:

- Keep current system fonts for first pass.
- Use title/value split:
  - Title smaller or dimmer.
  - Value brighter.

Colors:

- Title text: designer foreground mixed/dimmed.
- Value text: designer foreground or selector text.
- Active/changed state: designer accent.

Since Rockbox quickscreen does not directly know designer colors unless loaded from settings:

- First pass can use `global_settings.fg_color`, `global_settings.bg_color`, `global_settings.lse_color`, and `global_settings.lst_color`.
- RockPod should set these correctly in generated CFG, which it already does for designer themes.

## Interaction behavior

Keep current quickscreen controls:

- Up changes top option.
- Down changes bottom option.
- Left changes left option.
- Right changes right option.
- Enter/quick button exits as current behavior.

Visual feedback:

- On redraw after a setting change, highlight the last changed side if practical.
- If tracking last changed side is too invasive, defer active-card feedback to a later pass.

Click wheel:

- Static first pass is acceptable.
- Direction arrows can remain drawn over the wheel or baked into the wheel asset.
- Later pass can highlight the wheel segment for the last direction pressed.

## RockPod implementation tasks

1. Add asset generation functions in `rockpod/services/theme_designer.py`.
2. Generate quick settings assets after SBS/background assets are created.
3. Place assets into both:
   - main theme asset dir if needed
   - generated SBS asset dir, required
4. Include assets automatically in bundle asset list.
5. Ensure reset/delete generated theme cleanup removes these assets with the rest of the generated theme folder.
6. Add preview support only after device/sim rendering works.

Suggested generator functions:

- `_write_quick_settings_assets(staged_sbs_wps_dir, variant)`
- `_render_quick_settings_backdrop(dest_path, resolution, variant)`
- `_render_quick_settings_wheel(dest_path, resolution, variant)`
- `_render_quick_settings_card(dest_path, resolution, variant, active=False)`

## Rockbox implementation tasks

1. Add iPone Designer quickscreen detection helper.
2. Add asset path derivation from `global_settings.sbs_file`.
3. Add custom draw function:
   - `gui_quickscreen_draw_ipone_designer(...)`
4. In `gui_quickscreen_draw()`, use custom draw when:
   - main screen
   - active theme is iPone Designer
   - required assets load
5. Preserve exact current fallback behavior.
6. Confirm USB/system events still exit correctly.

## Testing plan

Simulator:

- Generate a default iPone Designer theme.
- Generate a dark custom color theme.
- Generate a light custom color theme.
- Install each into simulator root.
- Open quick settings.
- Capture screenshots.

Required checks:

- No native Rockbox fallback.
- Click wheel appears centered.
- Four settings are visible and readable.
- Text does not overlap the wheel.
- Light mode is not dark-on-dark or white-on-white.
- Dark mode is not low contrast.
- Exiting quickscreen returns to the prior screen.
- Changing each direction still changes the correct setting.

Device:

- Install one default designer theme.
- Install one custom color designer theme.
- Verify quickscreen loads without impacting SBS, lockscreen, WPS, or full-art slideshow.

## Acceptance criteria

- All iPone Designer themes get the new quick settings assets.
- Quickscreen displays the modern iPone-style layout when a designer theme is active.
- Center click wheel is visible and aligned.
- Current setting names and values remain functional.
- Missing assets fall back to current Rockbox quickscreen.
- Stock iPone behavior remains unchanged unless intentionally enabled later.
- No SBS skin text is modified for this feature.

## Risks

- Native quickscreen bitmap loading may need careful memory management.
- Large generated assets can slow first draw if loaded every redraw.
- Text sizing varies by font and language.
- Rockbox quickscreen has limited awareness of designer-specific metadata.

Mitigations:

- Cache assets only for quickscreen lifetime.
- Keep assets small and 320x240 target-specific.
- Use conservative card sizes.
- Fall back to generic quickscreen if anything fails.

