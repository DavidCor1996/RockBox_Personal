# classic_6g Assets

## Source

The first-pass `classic_6g` visual profile uses the Rockbox `iClassic` theme for `ipod6g` as its visual reference source.

Reference page:

- `https://themes.rockbox.org/index.php?themeid=1310&target=ipod6g`

Downloaded theme payload:

- `https://themes.rockbox.org/download.php?themeid=1310`

## License / Attribution

The theme metadata embedded in `iClassic.cfg`, `iClassic.sbs`, and `iClassic.wps` states:

- Theme: `iClassic`
- Author: `Humberto Santana`
- License: `Creative Commons Attribution-Share Alike 3.0`

Required attribution text from the theme header:

`Modified/Derived from iClassic theme for iPod Video (CC-BY-SA 3.0) by Humberto Santana`

## Staged Repo Paths

Backgrounds:

- `assets/classic_6g/backgrounds/iClassic_bd.bmp`
- `assets/classic_6g/backgrounds/iClassic_bg.bmp`

Icons:

- `assets/classic_6g/icons/Battery.bmp`
- `assets/classic_6g/icons/Hold Icon.bmp`
- `assets/classic_6g/icons/Memory Access.bmp`
- `assets/classic_6g/icons/PB.bmp`
- `assets/classic_6g/icons/Playing Status.bmp`
- `assets/classic_6g/icons/Radio Icon.bmp`
- `assets/classic_6g/icons/Ratings.bmp`
- `assets/classic_6g/icons/Repeat Icon.bmp`
- `assets/classic_6g/icons/Shuffle Icon.bmp`
- `assets/classic_6g/icons/Stereo Icon.bmp`
- `assets/classic_6g/icons/Volume Left.bmp`
- `assets/classic_6g/icons/Volume Right.bmp`

Masks / framing pieces:

- `assets/classic_6g/masks/FrameBottom.bmp`
- `assets/classic_6g/masks/FrameLeft.bmp`
- `assets/classic_6g/masks/FrameRight.bmp`
- `assets/classic_6g/masks/FrameTop.bmp`

Font reference:

- `assets/classic_6g/fonts/24 iLike.fnt`

Reference screenshots and original Rockbox theme text files:

- `assets/classic_6g/mockups/reference-rockbox/`

Included there:

- menu screenshot
- WPS screenshot
- volume screenshot
- FMS screenshot
- original `iClassic.cfg`
- original `iClassic.wps`
- original `iClassic.sbs`
- original `iClassic.fms`
- original known-issues/readme-style text files

## How Assets Are Used In This Milestone

This milestone uses the Rockbox `iClassic` theme in two ways:

1. As a reference for layout, colors, spacing, and icon treatment
2. As a staged asset pool for a future real ZeroLauncher shell rebuild

The current screenshot generator does **not** attempt a 1:1 WPS port into ZeroSlackr.
Instead it uses the theme as the authoritative art/reference layer while building a safe mockup path for a future custom shell.

## Known Limits

- Rockbox theme files describe Rockbox WPS/SBS behavior, not ZeroLauncher UI logic
- ZeroSlackr/ZeroLauncher cannot adopt the full split-pane Apple Classic look from assets alone
- a true runtime port will require rebuilding the underlying `podzilla2` shell, not just swapping bitmaps
