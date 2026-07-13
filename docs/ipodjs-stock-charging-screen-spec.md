# iPodJS Stock iPod Classic Charging Screen Specification

## Decision

When the iPodJS engine is active, attaching charge-capable power without
entering USB data mode opens a stock iPod Classic-style charging surface. The
surface is rendered entirely from LCD primitives. It must not load, ship, or
generate a battery bitmap, sprite sheet, SVG, traced Apple artwork, or any
other hand-drawn asset.

This is a UI feature, not a new charging policy. It reports Rockbox's existing
power state and must not change USB negotiation, requested charge current,
power-management filtering, shutdown behavior, audio ownership, or playlists.

## Reference findings

The reference target is the color-screen iPod classic 6G/7G, 320 x 240.

- Apple's iPod classic guide distinguishes an awake status-area icon from a
  large, safe-to-disconnect charging screen. It specifies a lightning bolt
  while charging, a plug when full, and separate `Charging` and `Charged`
  screens. It also documents the exceptional `Charging, Please Wait` state for
  a battery too empty to communicate with a computer.
- Apple's illustrated 6G guide plate shows a metallic cylindrical battery, a
  partially green left side while charging, a graphite unfilled side, a dark
  lightning bolt, and a soft mirror reflection below the battery. The full
  state is entirely green and replaces the bolt with a dark plug.
- Photographed stock screens confirm the small centered `Charging`/`Charged`
  title and large centered battery. There is no percentage, wallpaper, album
  art, dock drawing, footer, or instruction text.

Sources:

- [Apple iPod classic User Guide, battery section](https://s3.amazonaws.com/szmanuals/5ff0e94c8c72cd9756bfe7cec3ebae19)
- [Apple manual mirror with the large-battery disconnect rule](https://documents.cdn.ifixit.com/sAWhtIhkMHJVpPyX.pdf)
- [Photographed `Charged` screen](https://forums.macrumors.com/threads/updating-the-ipod-5g-ui-with-ipod-wizard.2220186/)
- [Large asleep icon versus small awake icon](https://www.ifixit.com/Answers/View/678378/iPod+classic+photo+battery)
- [Real 6G report distinguishing the charging animation from merely waking on cable insertion](https://www.reddit.com/r/ipod/comments/1rnhiw9/ipod_classic_6th_gen_not_charging_when_plugged/)

The words, icon semantics, centered composition, cylindrical battery, moving
green fill, and reflection are the stock contract. iPodJS applies that
composition to its light and dark lockscreen palettes. The pixel values below
are implementation geometry inferred
from the 320 x 240 references and should be calibrated against real-device
captures before being treated as archival measurements.

## Surface ownership and precedence

The charge surface is allowed only while the iPodJS engine owns the main LCD.
It applies to the iPodJS home, native music/database, settings, extras, games,
clock, quick-settings, native WPS, and iPodJS-styled core list surfaces. It does
not cover bootloader screens, the USB data screen, recording, a running plugin,
video playback, firmware update/restore, panic screens, or the low-battery boot
trap.

Precedence from highest to lowest is:

1. panic, update/restore, or critical low-battery boot handling;
2. USB data mode (`SYS_USB_CONNECTED` and `gui_usb_screen_run()`);
3. active recording or full-screen plugin/video ownership;
4. iPodJS charge surface;
5. the prior iPodJS screen.

The USB screen must always win. A computer connection may first look like
charge-capable power and then enumerate as a host, so do not draw the charge
surface synchronously on the first power edge.

## Entry and exit state machine

Use one connection-generation latch so the screen opens at most once for each
physical insertion.

| State | Event | Result |
| --- | --- | --- |
| `unplugged` | `SYS_CHARGER_CONNECTED` | Arm a 750 ms classification window and turn on the backlight. |
| `classifying` | `SYS_USB_CONNECTED` | Cancel charge entry and run the existing USB screen. |
| `classifying` | charger disappears | Return to `unplugged`; draw nothing. |
| `classifying` | deadline expires with charger present and no USB data session | Save the underlying surface identity and enter `charging`. |
| `charging` | `charging_state()` true | Show the animated `Charging` surface. |
| `charging` | charger present, `charging_state()` false, and level is stably full | Show the static `Charged` surface. |
| `charging` | `SYS_USB_CONNECTED` | Leave immediately and run the existing USB screen. |
| `charging` | charger disconnects | Leave, invalidate the connection latch, and redraw the underlying screen. |
| `charging` | click-wheel button while Hold is off | Leave and consume that press as a wake/dismiss action. Do not reopen until the next insertion. |

For FireWire/main power, which cannot become a USB data session on these
targets, the classification delay may be skipped. For USB power, the 750 ms
window is mandatory.

`charger_inserted()` says that a charge-capable source exists.
`charging_state()` says that current is actively flowing. Treat full as
`charger_inserted() && !charging_state() && battery_level() >= 99` for two
consecutive one-second samples. Until that debounce completes, retain the
`Charging` state. A transient `!charging_state()` at 80-98% is not `Charged`;
the iPod6G target explicitly filters brief charger dropouts at high level.

If the reported battery level is negative or unavailable, animate `Charging`
while `charging_state()` is true and never infer `Charged` from the level.

## 320 x 240 visual contract

The charge surface follows the iPodJS light/dark mode setting. It ignores the
accent, wallpaper, surface, row-density, and font-scale preferences so those
preferences cannot alter the stock layout or charging-state colors.

### Layout

| Element | Geometry |
| --- | --- |
| Canvas | `(0, 0, 320, 240)` opaque radial/vertical lockscreen gradient |
| Black glass title strip | `(0, 0, 320, 23)`, title horizontally centered |
| Battery body outer box | `(92, 75, 145, 76)`, radius 8 |
| Left recessed cap | `(86, 82, 11, 62)`, radius 5 |
| Battery collar / terminal | `(231, 81, 9, 66)` / `(238, 94, 8, 40)` |
| Battery inner well | `(96, 81, 136, 64)`, radius 4 |
| Lightning glyph optical center | `(164, 113)` |
| Plug glyph optical center | `(164, 113)` |
| Battery and reflection damage box | `(84, 70, 165, 130)` |

Use the fixed 14-pixel Adobe/Helvetica-compatible bold iPodJS font. The title
baseline is derived from the loaded font metrics, not a baked text image. The
only title strings are localized equivalents of `Charging` and `Charged`; if a
translation cannot fit in 300 pixels, use the normal UTF-8-safe fitting helper.
No percentage is shown.

### Procedural battery frame

All coordinates are integer LCD pixels and all drawing is clipped to the main
viewport.

1. Clear the whole screen to the active mode's iPodJS lockscreen gradient.
2. Mirror the lower half of the battery into a 43-row reflection. Blend each
   row back toward the canvas until it is fully transparent at the bottom.
3. Draw the recessed black left cap, silver right collar, and rounded positive
   terminal behind the main shell.
4. Draw the main shell as a mathematically clipped radius-8 vertical gradient:
   bright silver at the top, neutral metal at mid-height, graphite at the
   bottom. Rounded scanline extents are calculated from a circle equation;
   there is no corner mask or traced asset.
5. Draw the radius-4 inner well with the stock graphite cylindrical gradient.
6. Clip the green gradient to the same rounded well and to the current moving
   fill boundary. Keep the boundary vertical and blend its final five columns
   into the graphite glass; do not add a bright divider.
7. Add one-pixel top and bottom rim highlights. These give the stock polished
   cylinder on the RGB565 LCD without bitmap antialiasing.

The initial colors are deliberately close to the existing iPodJS status
battery palette. RGB565 quantization is authoritative; visual comparisons
must use frames captured from the simulator or physical LCD after packing.

### Mode palettes

Light mode uses the silver lockscreen glass treatment already established by
iPodJS. Dark mode retains the same geometry, green charge cue, and state
glyphs over a graphite version of that gradient. Accent selection never
recolors the battery. The charging animation is therefore a lockscreen-class
system surface rather than an unrelated flat page.

| Role | Light | Dark |
| --- | --- | --- |
| Canvas base, top/mid/bottom | `#8495a2` / `#65717d` / `#464f56` | `#424b59` / `#272e39` / `#11161e` |
| Center spotlight, top/mid/bottom | `+25` / `+14` / `+7` RGB | `+11` / `+7` / `+3` RGB |
| Title strip, top/mid/bottom | `#444444` / `#1f2326` / `#091015` | same |
| Title | `#f4f6f8` | `#f4f6f8` |
| Outer shell, top/mid/bottom | `#f8f9fa` / `#a0a5ab` / `#272b30` | `#eef1f4` / `#848b94` / `#272c33` |
| Graphite well, top/mid/bottom | `#b5b5ba` / `#45494d` / `#5d5f61` | `#a6acb5` / `#424851` / `#494f57` |
| Green well, top/mid/bottom | `#b5f29e` / `#3e9b2d` / `#4a7042` | `#aeef92` / `#3ea02f` / `#3c6939` |

### Fill

The green region advances continuously from the rounded left edge to the
right edge over the graphite well. It is a symbolic repeating charge sweep,
not the reported battery percentage. Do not add percentage text, divisions,
individual cells, or discrete level steps.

### Glyphs

Draw the lightning bolt as a dark filled seven-vertex polygon centered in the
well. Draw the full-charge plug as a rounded dark body, two right-facing pins,
and a left cable stub. Both shapes are scanline/rectangle geometry; neither is
an image asset. The bolt stays fixed while the green boundary passes behind
it. The plug is static and replaces the bolt only in `Charged`.

## Animation and redraw

Render at 20 fps from `current_tick`. The green boundary takes 4.0 seconds to
cross the 136-pixel well, rests at full for 750 ms, then restarts at the left.
This gives 80 computed traversal frames without drift or accumulated sleeps.
On transition to `Charged`, stop the timer, draw a full-green well with the
plug once, and replace the title with `Charged`.

Only update the title region and `(84, 70, 165, 130)` battery/reflection damage
rectangle.
Do not clear or redraw the full framebuffer every half second after the first
frame. Pause animation redraws while the backlight/LCD is asleep. Power-state
sampling may continue at 1 Hz without boosting the CPU.

The plugged-in backlight timeout remains the user's Rockbox setting. The charge
surface must not force the backlight on indefinitely. When a button dismisses
the surface, restore the previous screen through that screen's normal full
redraw path; do not attempt to preserve a framebuffer snapshot.

## Hold and input behavior

- Cable insertion may show the charge surface even when Hold is engaged.
- While Hold is engaged, click-wheel input does not dismiss it.
- Disconnect always exits.
- With Hold off, the first physical button press dismisses the surface and is
  consumed. Wheel touch/rotation alone does not dismiss it.
- The charge surface must not change playback state. Music continues with the
  same pause state, volume, playlist, elapsed position, and codec state.

## Code placement

Keep rendering and state out of the already large dashboard renderer as much
as possible.

- Add the procedural renderer and charge-session state to
  `apps/gui/ipodjs_ui.c`, with declarations in `apps/gui/ipodjs_ui.h`.
- Expose a small event helper that accepts a system action and reports whether
  it consumed the action, requested a normal redraw, or handed off to USB.
- Route system events through that helper in every native iPodJS action loop in
  `apps/root_menu.c`: empty WPS, WPS, database level, music menu, clock,
  extras, games, quick settings, settings, and dashboard.
- In `default_event_handler_ex()` cover iPodJS-styled core list surfaces that
  already route charger events through the default handler. Guard this path by
  current activity so plugin, recording, and video owners are untouched.
- Reuse `ipodjs_ui_gradient()`, `ipodjs_ui_puts_fit()`, LCD line/rectangle
  operations, and the fixed bold iPodJS system font. Do not add anything under
  `assets/ipodjs`, `.rockbox/ipodjs`, `apps/bitmaps`, or `apps/plugins/bitmaps`.

The helper owns no audio buffer and calls no audio, mixer, PCM, playlist,
storage-flush, or shutdown function. `SYS_USB_CONNECTED` remains acknowledged
only by the existing USB path.

## Simulator and hardware verification

### Simulator

Extend the existing F10/F11 power-toggle capture path so it records:

- at least eight closely spaced `Charging` captures;
- at least five distinct rendered frames and four distinct green extents;
- static `Charged` frame;
- dismissal back to home and WPS;
- unplug exit;
- equivalent light- and dark-mode charging frames with identical geometry;
- Hold preventing dismissal;
- USB handoff with no visible charge-frame flash.

Compare damage-rectangle captures to ensure pixels outside the title and
battery rectangles remain unchanged between animation frames. A static source
check must fail if the implementation references a charge-screen image path or
introduces a battery asset.

### Physical iPod Classic 6G

Test at minimum:

- Apple-style wall power, generic 5 V wall power, computer USB, and FireWire
  charging if available;
- insertion at 20%, 80-98%, and 99-100%;
- the high-battery brief `charging_state()` dropouts handled by
  `power-6g.c`;
- insertion and removal from home, database, Files, paused WPS, playing WPS,
  settings, and Hold;
- backlight timeout followed by a wake/dismiss press;
- repeated plug/unplug and USB enumeration races;
- uninterrupted audio before, during, and after the charge surface.

Build gates are `ipod6g` and `ipodvideo` plus the ipod6g simulator. This change
does not authorize a physical deploy. If it is later deployed, follow the
repository rule to copy `rockbox.ipod` to both the volume root and
`.rockbox/rockbox.ipod`, verify both checksums against the local build, and only
then sync/eject.

## Acceptance criteria

- A charge-capable non-data insertion displays `Charging` with the procedural
  left-to-right green sweep after the USB classification window.
- Full charge displays a static full-green battery, plug glyph, and `Charged`.
- USB data mode always wins and behaves exactly as before.
- The surface contains no status bar, percentage, custom theme treatment, or
  image-derived artwork.
- Light/dark mode changes the documented system palette and nothing else;
  accent, wallpaper, surface, density, and font scale have no effect.
- No new bitmap, SVG, sprite, generated image, or decoded-image buffer is used.
- Dismissal or unplug restores the correct prior surface and selection.
- Playback, recording exclusions, USB acknowledgement, shutdown, and charge
  current behavior are unchanged.
- Animation stops drawing while the LCD is asleep and does not keep the CPU
  boosted.
- ipod6g, ipodvideo, and simulator builds pass with no new warnings.
