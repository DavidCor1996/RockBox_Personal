# iPodJS iOS 3 Applications View

## Product intent

The optional Applications grid should feel like a stock iPod feature first
and an iPhone OS 3 visual reference second.  It keeps the existing iPodJS
status bar, click-wheel actions, Hold behavior, screen transitions, and app
launch lifecycle.  The skeuomorphic icon grid replaces only the Applications
content pane.

The view is selected at:

`Settings > Theme Settings > iPod Applications Appearance > iOS 3 Grid`

`Classic` remains the default and preserves the split list/preview view.

## Interaction contract

- Turning the wheel advances exactly one application at a time in reading
  order.  Movement clamps at the first and last item, matching the native
  Applications list.
- The selected application receives a bright blue, glass-like focus plate
  behind both its icon and label.  It remains obvious without changing the
  source icon artwork.
- Select launches the focused application.  Menu returns one level.  The
  play/pause shortcut and Hold overlay retain their existing behavior.
- A page holds twelve applications in a four-column, three-row grid.  Crossing
  a page boundary swaps to the next complete page without a blocking
  animation; page dots provide position feedback.
- The native status title remains `Applications`, including the normal iPodJS
  status indicators.  The grid does not imitate an iPhone status bar.

## Visual specification (320 x 240)

- Content starts below the 24-pixel native status bar.
- Icons are real 46 x 46, 24-bit BMP assets in a 4 x 3 grid.
- Labels use Rockbox's packaged `12-Adobe-Helvetica.fnt`; no lettering is
  painted into an icon or substituted with a drawn approximation.
- The body uses a restrained navy-to-black gradient.  Labels are white with a
  one-pixel black shadow, consistent with the legibility treatment used on
  period iPhone OS home screens.
- The focused cell uses the familiar iPod blue selection material with a pale
  top highlight.  It is deliberately stronger than the page dots and
  background so selection remains unambiguous on the physical LCD.

## Artwork policy

Authentic pre-flat App Store icons are used when a suitable period asset
exists.  Modern services retain their real official brand mark, placed on a
period-appropriate glass backplate.  RockPod-specific utilities without a
historical icon use checked-in generated source renders made expressly in a
late-2000s skeuomorphic style.  Nothing is hand drawn in C, and no substitute
font or emoji is used as an icon.

The checked-in source/provenance manifest is
`assets/ipodjs/rockbox/applications/SOURCES.md`.  The final bitmaps are
reproducible with `tools/prepare_ipodjs_application_icons.sh`.

## Memory and lifecycle

The twenty-two decoded icons occupy at most 93,104 bytes
(`22 * 46 * 46 * 2`) on the RGB565 targets.  Their slots are an arm of the
existing root-menu preview-cache union, whose other arms have non-overlapping
screen lifetimes.  This adds no BSS and performs no `core_alloc()`.

All packaged icons, the real Adobe Helvetica font, and conditional app
visibility are prepared at screen entry.  Draw and wheel-update paths consume
only cached state: they do not stat, open, scan, resize, or decode files.  The
view does not touch playback memory, PCM, the playlist, or tagcache.

## Acceptance criteria

- Both appearance choices survive a settings save/reload and can be changed
  without rebooting.
- Every visible application has a correctly mapped, non-placeholder icon and
  readable label.
- Wheel movement, Select, Menu, WPS access, Hold, USB, and app return paths
  behave exactly like the Classic Applications view.
- Simulator and native iPod 6G builds pass, followed by the focused iPodJS
  navigation regression with playback active.
- On hardware, rapid wheel changes and repeated Applications/app/Menu cycles
  introduce no decode stalls, file-descriptor growth, playback interruption,
  or stale preview frames.
