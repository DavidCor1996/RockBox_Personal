# Forest Theme Spec

## Summary

Create a new fully generated Rockbox theme named `Forest`. Use `iPone` only as
the technical base/reference so Forest inherits the same Rockbox feature surface.
Do not replace, rename, or overwrite iPone.

The visual direction is an original hand-painted woodland fantasy theme: mossy
forest paths, oversized mushrooms, small toads, glowing spores, warm
leaf-filtered light, and soft painterly backgrounds. The current Galaxy
space/mono concept is not the implementation base for this theme.

## Base Theme Decision

Use `iPone` as the technical base to create a separate `Forest` theme:

- `themes/iPone.cfg`
- `wps/iPone.sbs`
- `wps/iPone.wps`
- `wps/iPone.fms`
- `wps/iPone/`
- `icons/iPone.bmp`
- `backdrops/iPone_bd.bmp`

Do not use `Galaxy` as the code base. `Galaxy` is a 160x128 2bpp split-screen
theme with a much smaller asset and feature surface. Starting from Galaxy would
lose iPone behaviors such as lockscreen, AOD, charging wallpaper rotation,
right-pane miniplayer/full-art modes, notification card, FM screen, volume
overlay, album-art framing, and current RockPod wallpaper integration.

Do not edit the existing iPone theme files in place. Copy them into Forest-owned
paths first, then make all art and config changes inside the Forest files.

## Goals

- Preserve all current iPone behavior and Rockbox settings compatibility.
- Ship as a separate `Forest` theme.
- Replace every visible asset with generated forest-themed art.
- Make the SBS/menu background white, readable, and calm.
- Generate wallpapers based on the current default wallpaper roles, not just
  one generic background.
- Keep generated assets cohesive across WPS, SBS, lockscreen, charge screen,
  AOD, FM, sliders, status icons, and menu icons.
- Preserve current 320x240 layout coordinates unless an asset absolutely needs
  minor viewport tuning.
- Keep text legible on actual iPod Video/Classic screens.

## Non-Goals

- No direct imitation of a named animation studio or existing film. The style
  target should be original: hand-painted, soft, whimsical, Japanese animation
  influenced, watercolor/gouache texture, warm environmental storytelling.
- No feature rewrite before the reskin is working.
- No replacing, renaming, or modifying the shipped iPone theme in place.
- No replacing iPone with Galaxy's 160x128 layout.
- No runtime blur, subject segmentation, or expensive image processing on the
  device.

## Visual Direction

The theme should feel like a quiet forest music player, not a dark fantasy UI.
Use light surfaces, organic accents, and small illustrated details.

Palette:

- SBS/menu base: warm white `FAF8EF` or `FBFAF5`.
- Main text on white: deep moss `203228` or ink brown `2C241C`.
- Secondary text: muted bark `6E6758`.
- Selector gradient: fern green to lichen green, roughly `B8D58B` to `6A9D5B`.
- Accent: mushroom cap red `C75844`, golden spore `D7A84E`, creek blue `4F8CA3`.
- Dark WPS panels: deep forest `17241D`, not black.
- AOD: pure white or very pale warm white with black/moss text.

Texture language:

- Painted paper grain and soft brush edges.
- Moss/leaf silhouettes as subtle edge treatment, never behind small text.
- Mushroom caps, tiny glowing spores, fern curls, and bark rings as accents.
- Toads appear as friendly scene elements in wallpapers and empty-art/fallback
  art, not as busy UI icons.

## Layout Requirements

SBS/menu:

- Background must be white or near-white.
- Left menu pane stays high contrast and readable.
- Right pane keeps both current iPone modes:
  - `ipone right pane: miniplayer`
  - `ipone right pane: full art`
- Miniplayer area should look like a small forest clearing: album art, title,
  artist, and record/spore animation over a pale panel.
- Full-art mode should retain the album-art pane behavior and use a subtle
  mossy shadow between menu and art.

WPS:

- Preserve the existing album-art centered layout and progress logic.
- Replace the current dark purple glass cards with forest glass/paper panels.
- Use dark forest background layers with subtle illustrated leaf shapes.
- Keep all metadata, play state, explicit/lossless indicators, shuffle/repeat,
  volume overlay, and slider behavior.

Lockscreen:

- Wallpapers should be full-scene generated forest images.
- Clock/date region must reserve clean sky/fog/light space for readability.
- Music notification card should look like vellum or pale mushroom paper.
- Keep AOD separate: white background, black/moss text, minimal ornament.

Charging:

- Four charge wallpapers should represent a gradual time/weather story:
  morning clearing, noon moss path, dusk mushrooms, night glow-spores.
- Maintain the copied iPone minute-based rotation behavior in `Forest.sbs`.

FM:

- Replace radio fallback art with a generated forest-radio object or a toad on a
  mossy stump holding a tiny receiver.
- Keep FM metadata and volume/progress controls unchanged.

## Asset Inventory

Generate Forest-owned replacements for every bitmap referenced by the copied
iPone theme. Preserve file names inside `wps/Forest/` and preserve pixel
dimensions for first implementation so the copied SBS/WPS/FMS files remain
stable.

Core full-screen art:

- `iPone_bd.bmp` 320x240: SBS white forest menu background.
- `iPone_bd_fullart.bmp` 320x240: full-art right-pane background/shadow base.
- `iPone_bg.bmp` 320x240: FM background.
- `SbsBackdrop.bmp` 320x240: legacy SBS backdrop if still referenced by tools.
- `Wallpaper.bmp`, `WallpaperAlt.bmp`, `WallpaperThird.bmp`,
  `WallpaperFourth.bmp`, `WallpaperFifth.bmp`, `WallpaperSixth.bmp` 320x240:
  lockscreen wallpaper set.
- `ChargeWallpaper.bmp`, `ChargeWallpaperAlt.bmp`,
  `ChargeWallpaperThird.bmp`, `ChargeWallpaperFourth.bmp` 320x240:
  charge wallpaper set.
- `AODBackdrop.bmp` 320x92: minimal white AOD top/bottom treatment.

Preview and customization assets:

- `LockscreenStyle.bmp` 125x40.
- `AlwaysOnDisplayStyle.bmp` 125x40.
- `ChargeWallpaperPreview1.bmp` through `ChargeWallpaperPreview4.bmp` 125x94.

Playback and status strips:

- `Battery.bmp` 25x264.
- `Playing Status.bmp` 16x52.
- `PlayStatus.bmp` 16x144.
- `PlayStatusPurple.bmp` 16x144.
- `PlayStatusPurpleLarge.bmp` 19x171.
- `PlaybackStatusIcons.bmp` 17x96.
- `LoadingStatus.bmp` 16x192.
- `LoadingStatusAOD.bmp` 16x192.
- `MiniRecordSpin.bmp` 20x240.
- `MiniRecordSpinSmall.bmp` 16x192.
- `Hold Icon.bmp` 9x8.
- `HoldStatus.bmp` 12x14.
- `SleepStatus.bmp` 12x14.
- `SleepStatusAOD.bmp` 12x14.
- `Memory Access.bmp` 13x78.
- `Stereo Icon.bmp` 17x14.
- `LosslessIcon.bmp` 16x16.
- `LosslessIconLock.bmp` 20x20.
- `ExplicitIcon.bmp` 16x16.

Album art frames and fallbacks:

- `PlayerFallback.bmp` 146x146.
- `Radio Icon.bmp` 136x138.
- `NotifMusic.bmp` 51x51.
- `Notification.bmp` 288x75.
- `NotifPlayIcon.bmp` 12x28.
- `NotifPlayIconLock.bmp` 16x36.
- `WpsTL.bmp`, `WpsTR.bmp`, `WpsBL.bmp`, `WpsBR.bmp` 8x8.
- `WpsBackdropT.bmp` 146x6.
- `WpsBackdropB.bmp` 146x18.
- `WpsBackdropL.bmp`, `WpsBackdropR.bmp` 18x170.
- `FrameTop.bmp` 144x13.
- `FrameBottom.bmp` 144x6.
- `FrameLeft.bmp`, `FrameRight.bmp` 4x123.

Sliders and overlays:

- `Slider*.bmp`, `SliderBackdrop*.bmp`, `PlayerSlider*.bmp`: replace purple
  with moss/fern accent versions while preserving exact dimensions.
- `PB.bmp` 204x23.
- `VolumeBackdrop.bmp` 180x45.
- `VolumePromptIcons.bmp` 24x84.
- `VolumeSlider*.bmp` and `VolumeSliderEnd*.bmp`.
- `LargeSlider*.bmp`.
- `ShuffleStatus*.bmp`, `RepeatStatus*.bmp`, `Shuffle Icon.bmp`,
  `Repeat Icon.bmp`.
- `Ratings.bmp`, `PlaylistPositionIndicators.bmp`, `PlayerStatusButton.bmp`.

Theme-level assets:

- `icons/Forest.bmp` 4x16: generate compatible tiny menu icon strip.
- `backdrops/Forest_bd.bmp` 320x240: match the Forest SBS backdrop.

## Generation Plan

Use a master art bible before generating individual assets:

```text
Original hand-painted woodland fantasy interface art for a 320x240 portable
music player. Warm white UI surfaces, moss green accents, red-capped mushrooms,
soft leaf-filtered sunlight, tiny friendly toads, glowing spores, watercolor and
gouache texture, clear negative space for readable black or white text, no logo,
no text, no existing character, no direct imitation of any named film or studio.
```

Asset-specific prompt rules:

- Full-screen wallpapers: include composition instructions for reserved text
  zones.
- SBS background: mostly warm white, very light forest border, blank left menu
  region, calm right pane.
- WPS background: darker forest vignette with a clean center for album art.
- Icons: simple high-contrast flat/painterly glyphs; avoid detailed scenes.
- Sliders: generate or procedurally draw them from sampled palette colors so
  the small pixel assets stay crisp.
- Battery strip: use simple leaf/seed charge fill shapes, but keep charge states
  instantly readable.
- Spinner: replace record spin with a small rotating spore/mushroom-ring motif.

For small UI strips, generated art should be cleaned up manually or via script
after generation. AI output alone will be too soft at 4-20 pixel sizes.

## Implementation Plan

Phase 1: clone and wire theme

- Copy `themes/iPone.cfg` to `themes/Forest.cfg`.
- Copy `wps/iPone.sbs`, `wps/iPone.wps`, `wps/iPone.fms` to matching
  `Forest` files.
- Copy `wps/iPone/` to `wps/Forest/`.
- Add `icons/Forest.bmp` and `backdrops/Forest_bd.bmp`.
- Update internal paths, theme title comments, icon path, and backdrop path to
  Forest-owned files only.
- Keep all viewport coordinates unchanged.

Phase 2: generated full-screen assets

- Generate and convert 320x240 wallpapers/backdrops.
- Replace SBS background first and verify white menu readability.
- Replace lockscreen, charge, WPS, and FM backgrounds.
- Export BMP3/8-bit or compatible Rockbox BMP format matching existing files.

Phase 3: generated UI component assets

- Replace notification, album frames, volume card, sliders, battery, play state,
  loading, repeat/shuffle, lossless, explicit, and FM fallback assets.
- For very small icons, generate at large size, simplify, pixel-fit, then
  downsample/index.
- Preserve all strip frame counts and dimensions.

Phase 4: color and config pass

- Update `themes/Forest.cfg`:
  - `background color: FAF8EF`
  - `foreground color: 203228`
  - `line selector start color: B8D58B`
  - `line selector end color: 6A9D5B`
  - `line selector text color: 102018`
  - `list separator color: DED8C8`
- Update WPS/SBS hardcoded colors from purple/near-black to moss, bark, vellum,
  and warm-white values.
- Keep AOD high contrast: white background with black or deep moss text.

Phase 5: theme list integration

- Ship `Forest` as a new selectable theme.
- Leave `themes/iPone.cfg` and existing iPone assets unchanged.
- Leave `themes/Galaxy.cfg` unchanged unless a later cleanup explicitly removes
  or hides Galaxy from a user-facing theme list.

## Verification

- Run a static reference check for all `%xl(...)`, `%X(...)`, and config
  bitmaps to ensure zero missing Forest assets.
- Use `identify` to confirm every Forest bitmap matches the original
  dimensions.
- Build/run iPod Video/5G simulator.
- Build/run iPod Classic/6G simulator if Forest also gets a 6G variant copied
  from `iPone7G`.
- Capture:
  - SBS menu idle with miniplayer.
  - SBS menu full-art right pane.
  - WPS playback.
  - Volume overlay.
  - Lockscreen with and without playback.
  - AOD.
  - Charging screen at each wallpaper branch.
  - FM screen.
- Check that white SBS background does not wash out album-art/full-art content.
- Check all text remains readable on actual 320x240 captures, not just source
  PNG/BMP previews.

## Open Design Choices

- Whether the display name should be exactly `Forest` or include a subtitle in
  comments/docs only.
- Whether to also create a matching `iPone7G` variant immediately.
- Whether RockPod's wallpaper manager should expose this as a selectable
  generated style pack.
- Whether the tiny `icons/Forest.bmp` strip should stay minimal or become a
  custom mushroom/leaf glyph set.
