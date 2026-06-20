# Testing classic_6g Visuals

## Goal

This test path validates the visual shell work without touching boot logic, disk mounting, or audio playback.

## Generate Screenshots

Run:

```bash
python3 tools/classic_6g/render_mockups.py --profile classic_6g
```

Optional fallback profile:

```bash
python3 tools/classic_6g/render_mockups.py --profile legacy_zeroslackr
```

Outputs are written to:

- `screenshots/classic_6g/`

Expected generated files:

- `classic_6g_main_menu.png`
- `classic_6g_music.png`
- `classic_6g_now_playing.png`
- `classic_6g_settings.png`
- `classic_6g_extras_games.png`
- `classic_6g_about_debug.png`
- `reference_contact_sheet.png`

## What To Compare

Compare the generated `classic_6g_*` screens against:

- `assets/classic_6g/mockups/reference-rockbox/menu-iClassic_v1.0_Menu.png`
- `assets/classic_6g/mockups/reference-rockbox/wps-iClassic_v1.0_WPS.png`
- `assets/classic_6g/mockups/reference-rockbox/3-iClassic_v1.0_FMS.png`

Look for:

- top header height and proportions
- left menu width
- right preview panel width and spacing
- light overall background
- blue highlight tone
- dark text weight and spacing
- simple Apple-style information density

## Safe On-iPod Test Guidance

For this milestone, the iPod itself should only be used as a visual reference target.

Safe approach:

1. Keep the current working Apple / Rockbox / ZeroSlackr boot chain intact.
2. Do not patch firmware.
3. Do not replace the live ZeroLauncher binary yet.
4. Use the mockups to decide whether the layout direction is correct first.

## Current Limits

- The generated visuals are shell mockups, not a rebuilt runtime shell.
- They prove the target layout and screen vocabulary, not final in-device rendering.
- A real on-device `classic_6g` runtime profile still requires a later `podzilla2` rebuild/integration step.
