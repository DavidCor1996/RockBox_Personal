# iPodJS stock-style Quick Settings improvement pass

## Goal and scope

Make native iPodJS Quick Settings feel continuous with the recently ported
Apple menu and control assets. Preserve every available setting, adjustment,
action, shortcut, saved value, and platform guard. The implementation is recorded in
`docs/ipodjs-stock-quick-settings-validation.md`; physical-device validation
remains pending.

Target the 320x240 iPod Classic and Video shell in `apps/root_menu.c`, its
Quick Settings children, and the shared helpers in `apps/gui/ipodjs_ui.c`.
The separate iPone Designer four-direction quickscreen is outside this pass.
Quick Settings remains a RockPod extension; the design borrows the ported
stock menu language without claiming to reproduce an original Apple screen.

## Local reference baseline

Use these repository sources as the design and implementation baseline:

- `docs/personal-apple-asset-overrides.md`: existing Apple header, fonts,
  control icons and sliders, override precedence and fallback behavior.
- `docs/ipodjs-stock-6g-animation-spec.md`: horizontal push/pop, cubic
  smoothstep, 240-280 ms duration and 24-30 fps presentation target.
- `docs/ipodjs-stock-list-engine-spec.md`: stable selection, viewport
  restoration, row-level updates and text fitting.
- `docs/ipodjs-quick-settings-cache-memory-spec.md`: refresh/restart semantics.
- `docs/ipodjs-ui-memory-animation-steering.md`: mandatory resource gates.

Current code already provides the stock title renderer, retail scrollbar,
Apple Volume/Brightness detail sliders and a transition presentation hook.
The remaining visual differences include custom separator-heavy rows,
hard-coded blue highlight borders, mixed icon/text alignment, inline meters,
and a prominent gradient instruction footer. Wheel navigation currently
wraps and redraws the complete screen; the viewport is derived from selection
rather than retained independently.

## Visual design

### Header and list

Reuse the exact shared stock menu header and status indicators. Keep the
`Quick Settings` title, playback state, battery and Hold feedback. Use the
ported menu font and baseline for the default appearance. User-selected Font,
Density, Surface, Accent and Dark Mode must still visibly work; the stock
appearance is the default treatment, not a forced settings reset.

Use one full-width list below the header. Match sibling stock menus for row
height, horizontal insets, text baseline, selection artwork and scrollbar.
Derive row height from the active font and density with enough space for all
content; never squeeze the complete list onto one screen. Remove unselected
row rules in the default stock surface. Use the shared selected-row renderer
instead of adding custom cyan/navy borders or an adjustment outline.

Reserve a consistent leading icon column on every row, including when an
asset is missing. Retain the current small icons, using ported control icons
where available; no enlarged tiles or additional icon cache. Align all labels
and right-align values in a measured trailing column. Reserve arrow space
separately. Fit labels and values without overlap using existing UTF-8-safe
helpers; test the longest values and largest supported font.

Show a disclosure arrow only for rows that open a child: Volume, Brightness,
Kokkia and Cache & Memory where available. Cycling/toggle rows display their
current value without suggesting another screen. Remove inline Volume and
Brightness mini-meters; retain their exact textual values and the existing
full slider editor. This removes duplicate visual controls without removing
adjustment access or precision.

### Help and control screens

Retain contextual help in the existing bottom inset, rendered as a quiet,
flat continuation of the surface with the detail font. Remove its decorative
gradient. Keep useful device-specific explanations; fit or wrap within a
bounded area, expanding the inset and recomputing visible rows only when
necessary for the active font. No timed help carousel or scrolling animation.

Keep the Apple Volume/Brightness slider assets, layout and fallback meter.
The displayed slider position represents the committed setting immediately;
do not interpolate the value after a wheel step. Preserve numeric precision
and existing adjustment limits even though the artwork uses a percentage.

Apply the same header, rows, values and help treatment to Cache & Memory and
Kokkia children. Keep all reconnect/help/pause-on-unplug actions, refresh
feedback, reboot confirmation and charger guards intact.

## Functionality contract

Preserve the enum order and existing conditional visibility:

| Control | Required behavior |
| --- | --- |
| Volume, Brightness | Existing increments, bounds, direct left/right adjustment and Select editor |
| Dark Mode, Shuffle, Repeat | Existing values, cycling and immediate application |
| Accent, Surface, Density, Font, Extras Pane | Every current choice and visual effect |
| Haptics, Hold | Existing capability guards and behavior |
| USB Connection, Kokkia | Existing modes, status updates and child actions |
| Composite Out, TV Backlight | Existing target-specific choices and application path |
| Cache & Memory | UI refresh, graceful confirmed restart, Back |
| Screensaver | Existing toggle and behavior |

Wheel moves selection outside an editor and adjusts inside it. Preserve the
current list endpoint wrapping in this compatibility pass, despite the
non-wrapping general stock-list specification. Do not silently change a
navigation shortcut under a visual-only brief. Retain a separate viewport top
so reversing direction scrolls naturally; wrapping explicitly reveals the
first/last row. Restore both selection and viewport after each child.

Keep Select, left/right, Menu/back, context/quickscreen dismissal,
Play/Pause, long-press actions, Hold and system-event handling as implemented.
Menu from an editor returns to its selected row; dismissal returns to the
actual calling screen. Preserve pending settings/status saves and their exit
paths, including restart. Styling must not invoke adjustment handlers.

## Motion and redraw contract

Use the existing horizontal navigation compositor for Quick Settings entry,
child entry and return where the caller owns a valid source frame: forward
enters from the right, back enters from the left. Audit current transition
ownership before adding begin calls; exactly one owner captures each handoff.
The Apple slider renderer must participate in the same presentation path as
the list rather than independently publishing a frame before composition.

Use the existing 240-280 ms smoothstep timing at 24-30 fps, derived from elapsed
ticks. These values come from the local animation spec, not a new measurement.
No bounce, zoom, fade, staggered rows or animated selection chase. Wheel
selection and live adjustments respond immediately. Fast input finishes or
cancels decorative motion to a complete destination without losing actions.

For a wheel step within the viewport, update only the previous/new rows and
changed help/status regions. A viewport shift redraws the list region and
scrollbar. Editor adjustment redraws only the slider/value region. Full
redraws remain valid for entry, theme/font changes, Hold overlays and recovery.
Restore the viewport/font/draw state needed by each partial repaint. Accessory
status updates invalidate the corresponding row and indicators even when
selection is stationary. Idle screens must not repaint at animation cadence.

## Implementation and resource boundaries

Refactor the existing renderer into bounded row/list/editor paint helpers and
retain selection, viewport top and dirty state in the owning menu loop.
Reuse shared menu geometry and selection helpers rather than creating another
stock renderer. Leave value formatting, adjustment handlers and persistence
logic as the behavioral source of truth.

New bitmap/framebuffer cache budget: zero bytes. Reuse the existing transition
workspace only when its lifetime is available; otherwise present the complete
destination directly. Small scalar state is allowed and must be measured in
the native stack/BSS comparison. Do not allocate from core or playback memory.

Load fonts/assets through preparation or bounded idle service, never inside
row painting or animation. Audit current icon asset access for lazy I/O before
reusing it in partial draws. Missing assets use cached fallback rendering and
must not cause repeated storage probes. Honor light/dark invalidation and UI
cache refresh. Never stop/restart playback, claim artwork slots, mutate a
playlist, or change PCM/mixer ownership for decoration.

## Verification and completion gates

1. Capture before/after list, bottom-of-list, Volume, Brightness, Kokkia,
   maintenance and Hold screens in light/dark modes. Check all font/density
   extremes and missing-asset fallback at native 320x240 resolution.
2. Exercise every available row and value in both directions. Compare settings
   and status persistence, editor cancel/return, list wrapping, shortcut
   dismissal, accessory events and restart cancellation with the baseline.
3. Confirm row-only updates, no stale arrows/text, exact final destination
   pixels and one transition per handoff. Trace monotonic transition positions
   and 240-280 ms duration; queued input must remain responsive.
4. Run the closest focused simulator gate and
   `tools/ipodjs_navigation_sim_regression.sh`: active playback, ten full-depth
   hierarchy cycles, twenty rapid Albums/Artists switches, rapid Menu return,
   stable file descriptors and core memory, unchanged playlist/track identity
   and elapsed time allowing ordinary playback progress.
5. Build simulator, native ipod6g and ipodvideo; inspect native text/data/BSS
   deltas and ARM stack use. Build a configuration without optional controls
   to verify guards and layout. Run diff whitespace checks on changed files.
6. Before readiness is claimed, test the exact build on hardware with Database
   and Files playback, paused playback, repeated Quick Settings/editor/child
   entry and return, Hold and USB handling. Follow repository deployment rules
   if a deployment is requested. Record simulator and hardware evidence
   separately; screenshots alone do not establish playback or memory safety.

Deliver the visual/redraw changes together with verified behavior parity.
Any missing hardware evidence remains explicitly pending.
