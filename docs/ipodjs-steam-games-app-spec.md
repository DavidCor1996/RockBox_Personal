# iPodJS Steam Games appearance

## Goal

Add an optional Steam-style Games experience without replacing the stock
2007 iPod Games menu. `Settings > Theme Settings > iPod Games Appearance`
selects `Classic` or `Steam`; Classic remains the default.

Steam mode is a cover-led library, not a recolored menu. It presents the
games that are actually installed, opens a metadata page for the selected
title, and launches that title from a large Play button.

## Visual language

- Valve's real Steam Store header logo is used in a 40-pixel charcoal header.
- The content surface uses Steam's exact core client palette: header
  `#171a21`, body `#1b2838`, panel `#2a475e`, accent `#66c0f4`, and text
  `#c7d5e0`.
- The landing page is a horizontally wrapping carousel. The selected cover is
  large and centered while the previous and next covers peek in at the edges.
- The detail page shows cover art, platform, year, genre, developer,
  publisher, description, and a green Steam-style Play button.
- Menu returns and Select opens details/plays, matching click-wheel habits.
- Holding Select opens a console filter list. The wheel chooses `All Games` or
  an installed platform and Select applies it; the console names are derived
  from the installed catalog rather than a hardcoded mock storefront.

## Library and artwork contract

The app merges three real installed-library sources:

1. RockPod's launcher manifests for ROM-based games. Their title, launch
   plugin/ROM parameter, cover path, year, genre, publisher, developer, and
   description are used directly.
2. Native Rockbox game plugins with matching cover assets. Packaging admits
   only covers marked as official Rockbox manual screenshots or existing
   title artwork in `assets/game_covers/native/SOURCES.tsv`; generated
   typographic placeholders are intentionally excluded.
3. Installed Uxn ROMs from the packaged Uxn launcher manifest. Donsol, Niju,
   and Worm ship as MIT-licensed test ROMs with exact source revisions and
   SHA-256 hashes in `assets/uxn_games/SOURCES.tsv`. Their covers are real
   simulator gameplay frames or the upstream game's preview, never generated
   title cards. The library admits each entry only while its ROM, Uxn viewer,
   and cover are all readable.

Special installed titles such as Super Mario 64 and Stick RPG use their real
existing repository covers and launch parameters. Entries without a readable
cover or playable installed target do not appear, so the Steam library never
advertises a mock or unavailable game.

Uxn entries use `Uxn` as their platform, which creates a dedicated console
filter automatically. The manifest is read once on Games entry alongside the
other installed-library indexes; no ROM or cover directory scan occurs in a
draw callback.

## Interaction

- Wheel up/down: previous/next title, wrapping at both ends.
- Select: title detail page; Select again: launch selected title.
- Hold Select: console filters; wheel chooses, Select applies, Menu cancels.
- Menu: back from details, then back to Extras/root.
- Play/Pause: retains the existing iPodJS music shortcut behavior.
- Hold switch and USB events retain the shared native iPodJS handling.

## Performance and memory

Manifest and directory I/O runs once on Games entry, outside draw callbacks.
Artwork decode runs only after selection settles and the button queue is
empty. Draw callbacks consume cache-only bitmaps.

The artwork cache is fixed at three 144x108 native-format frames plus bounded
BMP scaling scratch space (about 114 KiB at 16 bpp). It holds only the selected
cover and its neighbors and is cleared on exit. Images resize with aspect ratio
preserved. The UI does not call
`core_alloc`, request a plugin/audio buffer, resize playback memory, or own a
full-screen framebuffer.

## Fallbacks

- Missing logo: render a text `STEAM` wordmark in the header.
- Cover decode failure: render Valve's real Steam logo on the client surface;
  no synthetic game thumbnail is invented.
- Empty verified library: explain that no games with cover art were found.
- Classic mode: run the pre-existing Games menu and launcher behavior.
