# iPodJS RetailOS Charging Surface Specification

## Decision

The iPodJS charging surface keeps the existing Rockbox power, USB, input,
Hold, playback, and return behavior.  Its Apple-looking pixels come only from
the exact iPod35 RetailOS 2.0.4 resource archive prepared for a personal build.
No screenshot, traced battery, generated frame, resized substitute, or
interpolated Apple bitmap is accepted.

RetailOS does not store this surface as a sequence of 320x240 screenshots.  It
stores ten compositing resources at ordinals 588--597.  The implementation
therefore reconstructs the screen from those source components, in their
native dimensions, just as a resource-driven UI does.  Calling a fabricated
set of full-screen captures an Apple animation is forbidden.

The complete archive and its validation contract are specified in
`docs/ipodjs-retailos-full-port-spec.md`.

## Exact source components

| Ordinal | Runtime name | Native data |
| ---: | --- | --- |
| 588 | `charging-empty-cap-left` | 38x142 RGBA |
| 589 | `charging-empty-cap-right` | 45x142 RGBA |
| 590 | `charging-empty-middle` | 8x142 RGBA |
| 591 | `charging-green-cap-left` | 38x142 RGBA |
| 592 | `charging-green-cap-right` | 45x142 RGBA |
| 593 | `charging-green-middle` | 8x142 RGBA |
| 594 | `charging-green-middle-cap` | 8x142 RGBA |
| 595 | `charging-charge` | 23x57 native 4-bit mask |
| 596 | `charging-plug` | 47x29 native 4-bit mask |
| 597 | `charging-critical` | 205x163 RGBA |

The empty and green middle resources may be tiled; that is their native
purpose.  The charge and plug resources remain lossless native mask data and
are colored by the screen compositor.  All color/alpha resources are decoded
once to exact RGBA and are never scaled, rotated, recolored, or repaired.

The source is admitted only when the complete private archive passes
`tools/verify_ipod_classic_resource_dump.py`.  A missing or corrupt component
invalidates the entire RetailOS charging set.  The renderer then uses the
clearly unbranded Rockbox fallback already present in the application; it must
never label that fallback as Apple or RetailOS output.

## Composition and animation

The source components are centered on the 320x240 main LCD.  The empty left
cap, a 123-pixel run of empty middle tiles, and empty right cap form the well.
Charging adds the green left cap, a tick-derived run of green middle tiles,
and the green moving cap. Full charge fills the middle plus the green right
cap.  The exact charge mask is shown while charging and the exact plug mask is
shown when full.  At a reported level of 0--2 percent, the exact critical
battery resource replaces the normal composition.

Center the masks within the visible casing, not within the component image
dimensions: the 142px-high parts include 22 transparent rows above the casing
and a reflection below it. The body occupies y=22..96 and x=22 through x=14
of the right cap (excluding the terminal). Apple's 2009 Classic guide page 12
matches the native charging left cap at (63,53) and right cap at (224,53),
and charged caps at (60,63) and (221,63). Thus the middle is 123 pixels, not
the previously guessed 80 pixels. Tile fifteen complete 8px strips plus three
unaltered source columns. The complete component canvas is 206x142; the casing
is 154x75, not the previous 111x75. Center the native masks within that casing.
Use the firmware's `ChargingMode_Toggle_Color` (#000000); the masks supply
their own opacity. Source pixels and the fill timing are unchanged.

Animation remains driven by `current_tick`, not by completed paints.  The
existing four-second fill plus 750 ms full hold and 20 fps service cadence are
unchanged.  Frame drops therefore change only presentation smoothness, never
state or elapsed time.  The component sequence is finite and deterministic;
no tweened bitmap, resampling, or generated in-between asset is created.

The title strip, localized `Charging`/`Charged` text, and neutral background
remain normal iPodJS drawing.  They are not represented as Apple assets.  The
Apple claim applies only to the ten verified source resources above.

## State and event behavior

Use one connection-generation latch so the surface opens at most once per
physical insertion.

| State | Event | Existing behavior to preserve |
| --- | --- | --- |
| `unplugged` | `SYS_CHARGER_CONNECTED` | Arm classification and turn on the backlight. |
| `classifying` | `SYS_USB_CONNECTED` | Cancel charging entry and hand off to the normal USB screen. |
| `classifying` | charger removed | Return to `unplugged` without drawing. |
| `classifying` | 750 ms expires with power but no data session | Enter the charging surface. |
| `charging` | current flowing | Show `Charging` and advance the component fill. |
| `charging` | two one-second samples report not charging at 99--100% | Show static `Charged`. |
| `charging` | `SYS_USB_CONNECTED` | Exit immediately through the existing USB path. |
| `charging` | charger removed | Exit and allow the underlying owner to redraw. |
| `charging` | physical button while Hold is off | Consume the wake/dismiss press and do not reopen until reinsertion. |

Wheel rotation alone does not dismiss the surface.  Hold blocks click-wheel
dismissal but never blocks charger removal or USB handoff.  The plugged-in
backlight timeout remains the user's setting.

This is presentation only.  The implementation must not change charge
current, USB enumeration, shutdown, recording, audio state, elapsed playback,
playlist contents, codecs, PCM, mixer state, or the album-art allocation.

## Ownership and memory

`apps/gui/ipodjs_ui.c` owns one fixed charging cache.  It loads the ten source
components at the bounded charging-screen entry point.  Drawing and animation
ticks perform no file I/O, image decoding, allocation, tagcache operation, or
playback call.  The cache neither borrows nor resizes the core/playback buffer
and does not use a full-screen framebuffer per animation frame.

The charging surface is allowed only while iPodJS owns the main LCD.  Panic,
firmware update/restore, critical boot handling, USB data mode, recording,
plugins, and video playback keep their existing precedence and ownership.

## Header battery

The awake header is a separate exact animation sequence.  It uses every one
of the 25 white and 25 black 26x13 RetailOS source frames: levels 0--22, plug,
and charge. Battery percentage selects one complete level frame. The plug and
charge records contain only transparent overlay glyphs: power state composites
the corresponding glyph over that level frame at the same origin, rather than
replacing the casing with the glyph. Menu and music headers share this painter;
the explicitly protected Hold presentation retains its previous path.
No five-frame reduction, primitive
reconstruction, or recoloring is permitted when the verified archive is
available.

## Verification

Release gates are:

1. The full 598-resource archive and all 227 source animation frames pass the
   standalone verifier before packaging.
2. A corrupt, truncated, renamed, missing, or extra ledger member fails the
   verifier and prevents that archive from being packaged as complete.
3. Source inspection proves all ten charging names and both 25-frame header
   atlases are loaded at bounded entry points, with no draw-time I/O or
   playback-memory ownership.
4. Simulator captures cover charging, full, critical, dismissal, unplug,
   Hold, and USB handoff.  The fill changes while the exact source pixels stay
   byte-identical before framebuffer conversion.
5. iPod 6G target and simulator builds pass.  Any physical deployment remains
   a separate, explicitly authorized operation.

Acceptance requires correct Apple components when the complete private corpus
is present, an unbranded fallback when it is not, and no change to behavior or
feature ownership in either case.
