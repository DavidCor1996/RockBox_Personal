# iPodJS Classic Utilities Experience Specification

## Decision

The next Classic-experience milestone is a **native Classic Utilities suite**:

```text
Home -> Extras -> Clock
                     -> World Clock
                     -> Stopwatch
                     -> Timer
                     -> Alarms
```

It replaces the current visual/lifecycle break where the iPodJS Clock list
opens the generic `clock.rock`, `stopwatch.rock`, or `alarmclock.rock` plugin,
and where Timer merely toggles Rockbox's sleep timer.  The suite remains part
of the normal iPodJS application layer: it uses the standard status bar,
click-wheel navigation, Hold treatment, Menu history, charging surface, and
WPS shortcut.  It is not a theme applied on top of legacy plugins.

This is deliberately a daily-use pass.  It does not attempt to clone RetailOS
C++ objects, replace the music player, or make the iPod 6G claim unsupported
hardware wake alarms.

The primary target is iPod Classic 6G/7G at 320x240.  iPod Video 5G gets the
same information architecture and controls where its target capabilities
allow them.  Other targets retain their present plugin routes.

## Why this is next

Music, Search, album artwork, home previews, status indicators, charging,
navigation, and WPS already have iPodJS-native ownership.  The most visible
remaining discontinuity in normal use is Extras: the Clock list itself is
Classic-shaped, but Select opens unrelated full-screen plugin UIs and loses
the Classic return path.  A focused utility suite fixes that discontinuity
without adding storage-heavy artwork, database work, or a new media path.

The existing implementations remain the behavioral sources until replaced:

- `apps/plugins/clock/` provides time/date display preferences;
- `apps/plugins/stopwatch.c` provides elapsed and lap semantics;
- `apps/plugins/alarmclock.c` remains the fallback for targets outside this
  specification; and
- Rockbox's sleep timer remains a separate power-management feature.

No existing option, clock format, timer, alarm configuration, or plugin must
be silently discarded.  Migration imports what can be represented exactly and
leaves the legacy plugin reachable under `Extras -> Plugins` until a user has
explicitly saved the new equivalent configuration.

## Current baseline and required correction

`apps/root_menu.c` already supplies an iPodJS Clock list and contextual pane.
Its current routes are:

| Row | Current Select action | Required action |
| --- | --- | --- |
| World Clock | Launch generic Clock plugin | Open native World Clock screen |
| Stopwatch | Launch generic Stopwatch plugin | Open native Stopwatch screen |
| Timer | Toggle global sleep timer | Open native countdown editor/running screen |
| Alarms | Launch generic Alarm Clock plugin | Open native alarm list/editor, capability guarded |

The new screens must return one level to Clock, and then one level to Extras,
on every short Menu press.  They must not first return to a plugin browser,
Home, a generic Rockbox menu, or a stale preview frame.

## Information architecture

### Extras

The existing Extras list remains extensible for RockPod features.  Its Classic
utility entries are ordered first:

1. Clock
2. Calendar (future organizer bridge; not implemented in this milestone)
3. Notes (future organizer bridge; not implemented in this milestone)
4. Games
5. Applications and the existing additional RockPod entries

If Calendar or Notes has no native implementation, it is not shown in the
first milestone.  Do not add a dead row, a faux Apple screen, or a launcher
that only displays an installation error.  The current Files, Playlists,
Plugins, Shortcuts, and System routes remain reachable below the Classic
entries.

### Clock

Clock owns four full-screen views.  The list/preview screen is a stable
parent, not a shortcut to unrelated plugins.

| View | Select | Menu | Persistent state |
| --- | --- | --- | --- |
| World Clock | enters city/time-zone editor | returns to Clock | chosen cities and 12/24-hour preference |
| Stopwatch | start/pause; long Select records a lap | returns to Clock, preserving a running stopwatch | elapsed baseline, run state, bounded lap list |
| Timer | starts/pauses the configured countdown; Select on a stopped timer edits it | returns to Clock, preserving a running timer | target wall-clock deadline and alert acknowledgement |
| Alarms | opens the selected alarm editor; Select on Add creates one | returns to Clock | bounded alarm records, target capability data |

The existing global **Sleep Timer** stays in Quick Settings and playback/power
menus.  It is not re-labelled as the Clock Timer and is never toggled as a
side effect of opening or operating the new Timer screen.

## Interaction contract

### Shared rules

- Wheel movement changes one focused row, digit, city, or lap at a time; it
  never wraps a list unless the current explicit picker has documented wrap.
- Select activates the focused control.  Long Select has only the documented
  stopwatch-lap behavior and must be ignored elsewhere rather than falling
  through to a context menu.
- Previous deletes one editable digit/field; Next advances to the next field
  only where a picker describes that behavior.  No custom gesture may steal
  the existing WPS, USB, shutdown, or Hold actions.
- Menu always returns one level, retaining draft input and the previous
  selection.  Menu cancels an unsaved editor after an explicit discard prompt
  only when a persistent value would otherwise be lost.
- Hold continues to present the existing iPodJS dim or lockscreen effect.  On
  unlock, the exact utility screen, cursor, running state, and elapsed time
  are restored.
- The normal iPodJS play/pause shortcut, status playback indicator, volume
  overlay, charging screen, and USB transition retain their existing owners.

### World Clock

World Clock has a local-time card plus a bounded list of selected cities.  A
city row presents its name, abbreviated zone, and current time; daylight-time
calculation uses the host-provided RTC/time-zone data only where Rockbox has a
reliable source.  Otherwise the editor uses an explicit UTC offset and labels
it as an offset, never inventing DST rules.

The initial release supports at most eight additional city/offset rows.  The
city catalogue is a read-only, compiled table or a small validated packaged
data file loaded once at screen entry.  It must not query a network service,
scan arbitrary directories, or allocate from the playback arena.

### Stopwatch

Use a monotonic tick baseline while the firmware is awake; display elapsed
time as `HH:MM:SS.t`.  A running stopwatch keeps its baseline rather than
incrementing a counter once per paint.  This prevents visual cadence from
changing elapsed time.

Store at most 20 laps.  When full, retain the most recent 20 and show a clear
`Laps full` status rather than reallocating or silently corrupting history.
If the RTC is valid, a running stopwatch saved across a normal reboot may
reconstruct elapsed time from a wall-clock start/end pair; if it is not valid,
the screen reports that an interrupted run was stopped.  Hibernate/resume is
treated as an ordinary elapsed-time discontinuity: the next paint derives the
correct value from the retained baseline or valid wall clock.

### Timer

Timer is a countdown, not a sleep command.  The editor supports hours,
minutes, and seconds with a bounded maximum of 23:59:59.  Starting a timer
records a wall-clock deadline when the RTC is valid and a monotonic fallback
while the device stays awake.  The screen continues to paint only once per
second, except for the final ten seconds where it may repaint at 5 Hz.

Timer completion is notification-first:

- while music plays, show the standard non-destructive utility notification
  and do not stop, pause, fade, restart, or replace the playlist;
- while no music plays, use the existing safe UI alert route if available;
- a new direct PCM callback, raw audio buffer, or playlist-backed alert sound
  is out of scope for this pass; and
- on an iPod 6G power-off, the timer cannot wake hardware.  It becomes due on
  the next boot/wake and is shown as missed rather than claiming it rang.

The notification behavior must be implemented through the existing
notification/overlay ownership, not a utility-local framebuffer or an
unbounded redraw loop.

### Alarms

Alarms are capability guarded.  The iPod 6G configuration does **not** define
`HAVE_RTC_ALARM`; the new UI must therefore never claim to wake a powered-off
6G, program an unimplemented PMU alarm, or enable an unknown wake source.

For targets with `HAVE_RTC_ALARM`, the editor may expose `Wake device` and
uses the existing RTC alarm API.  Its hardware programming and boot behavior
remain target code, not iPodJS code.

For the 6G, alarms may be presented as **while-awake reminders** only if the
implementation has a clear on-screen explanation and preserves the current
alarmclock-plugin behavior as a fallback.  If that contract cannot be made
useful without pretending to provide a real wake alarm, omit the Alarms row on
the 6G in the first patch and retain Alarm Clock under Plugins.  Correctness
is preferable to a visually complete but false Classic feature.

## Visual contract

All utility screens are 320x240 iPodJS-native screens:

- use the existing header/status implementation, list geometry, selection
  gradient, typography, dim/lockscreen effect, and 240--280 ms directional
  transition infrastructure;
- draw time text and the analog clock with the existing bounded primitives;
- use full-screen utility layouts only after Select from the split Clock
  screen; and
- keep the active value large and high contrast, with secondary actions in
  the normal iPodJS list style.

### Apple battery and motion system

The Classic battery is a shared system surface, not a utility-specific
drawing.  The native header continues to use the five direct Apple battery
states already prepared at
`.rockbox/ipodjs/apple/status-battery.apple.26x65x24.bmp`; it selects a
native 26x13 frame for empty, low, normal, high, and charging/full status.
It must not synthesize an in-between battery level, recolor the Apple pixels,
or let a utility screen substitute its own battery artwork.

The charging surface is upgraded by the accompanying amendment to
`docs/ipodjs-stock-charging-screen-spec.md`: Classic-authentic personal builds
use an Apple-sourced, frame-indexed 320x240 charging animation prepared by
`tools/prepare_ipodjs_apple_assets.py`.  The utility suite inherits that
surface on charger insertion, USB classification, Hold, dismissal, and return;
it does not redraw or cache a competing charge frame.

Motion is similarly shared.  Clock-parent and utility entry/return transitions
reuse the existing 240--280 ms directional iPodJS transition only after both
complete frames are available.  Timer/stopwatch ticking and analog-clock
hands are state updates, not decorative transitions: they repaint bounded
damage rectangles from elapsed ticks and must remain correct if frames are
dropped.  A queued button, USB, Hold, or shutdown event cancels a decorative
transition before it can allocate, load, or decode another asset.

No screenshot tracing, recreated Apple bitmap, proprietary font, or generic
plugin bitmap may be added.  Exact Apple pixels are allowed only through the
existing hash-pinned asset preparation route with provenance.  The Apple
battery/charging animation is a required exception to the former
primitive-only charging screen: its files are generated for personal packages
only and are never checked into the public tree.  If the verified Apple frame
set is absent, the build falls back to the existing procedural charging screen
without claiming that its battery is pixel-authentic.

Clock redraws are dirty and bounded: the status/header remains stable, a
world-clock row updates at most once per second, and an analog hand update
changes only the clock area.  The stopwatch and timer similarly repaint only
their changing value and controls.  Animations derive their position from
elapsed ticks, never from assumed frame delivery.

## Implementation boundaries

Create a small app-layer controller, proposed as
`apps/gui/ipodjs_utilities.c` and `apps/gui/ipodjs_utilities.h`, with a narrow
entry point for the Clock parent and a screen result enum.  `apps/root_menu.c`
continues to own Home/Extras routing, native-screen entry/exit, and Menu
history.  The controller owns only utility state, layout, input, and drawing.

The implementation must not:

- invoke `clock.rock`, `stopwatch.rock`, or `alarmclock.rock` from an iPodJS
  Clock row after the corresponding native screen lands;
- alter the WPS render loop, playback status, playlist, codec, PCM, mixer,
  shared audio buffer, album-art slot, or the global sleep timer;
- issue storage resets or database operations;
- perform file I/O, bitmap decode, directory enumeration, tagcache search, or
  allocation from a draw/input callback; or
- add a duplicate status bar, alternative `get_action()` WPS loop, or a new
  full-screen framebuffer cache.

The data model is fixed-size.  The initial budget is:

| Storage | Limit | BSS target |
| --- | ---: | ---: |
| Utility navigation/editor state | one controller | <= 2 KiB |
| World-clock rows | 8 rows | <= 1 KiB |
| Stopwatch laps | 20 records | <= 1 KiB |
| Alarm/reminder records | 8 records | <= 1 KiB |
| Total new static state | all above | <= 6 KiB |

Any persistent configuration uses a single small, versioned file written
atomically only after explicit Save or a state transition that must survive a
normal restart.  It is loaded at the controller's screen-entry service point,
validated into the fixed model, and then closed.  Rendering consumes cached
state only.  A corrupt or missing file falls back to defaults and leaves the
legacy plugin data untouched.

Before code review, report the `rockbox.elf` text/data/BSS delta and inspect
ARM stack use for all controller draw, editor, persistence, and alert paths.

## Delivery sequence

### Phase 1 -- native Clock and Stopwatch

Land the controller, World Clock, Stopwatch, coherent Extras/Clock return
history, and the shared status/Hold/WPS/USB behavior.  No timer sound, alarm
hardware, or organizer changes are included.

### Phase 2 -- countdown Timer

Add the separate countdown data model, editor, persistence, due/missed state,
and notification-first completion behavior.  Verify that the existing Sleep
Timer remains independent.

### Phase 3 -- Alarms behind real capabilities

Integrate existing RTC alarm support only on targets that advertise
`HAVE_RTC_ALARM`.  Decide separately whether a 6G while-awake reminder is
worth exposing; do not use this phase to reverse-engineer PMU wake behavior.

### Phase 4 -- organizers, separate specification

Calendar, Notes, and Contacts are intentionally excluded from the Clock
implementation patch.  A follow-up must specify their import/export formats,
privacy model, edit behavior, and storage lifecycle before adding rows to
Extras.  Calendar may bridge the existing plugin only after its full-screen
Classic UI and return semantics are designed; Contacts requires a portable
vCard-backed model rather than a private RetailOS database import.

## Verification

Add a focused simulator regression, proposed as
`tools/ipodjs_classic_utilities_sim_regression.sh`, and a source-contract test
under `rockpod/tests/`.  It must cover:

1. Home -> Extras -> Clock -> every shipped utility -> Menu walks exactly one
   level back to Home, retaining row selection.
2. A Database track and a Files track both continue without restart, seek
   loss, playlist mutation, or unexpected WPS entry while navigating and
   leaving every utility.
3. World Clock updates after one real second; Stopwatch runs across at least
   20 laps and rejects the 21st cleanly; Timer reaches due and missed states
   without changing the sleep timer.
4. Hold/unlock preserves each utility's screen/cursor/running state.  USB and
   charger events preempt and return to the right utility state.  With the
   Apple frame set installed, captures prove the native header chooses only
   the five extracted battery frames and the charging screen presents the
   selected source frames at their original 320x240 geometry.
5. An invalid utility configuration file fails closed to defaults; a saved
   file round-trips through a simulator restart; no file descriptor remains
   open after return to Extras.
6. Draw-path instrumentation proves no `open`, `close`, `read`, `write`,
   `file_exists`, directory scan, bitmap decode, tagcache call, `core_alloc`,
   or audio/playlist mutation occurs from a draw or periodic update path.
   Charging-animation frames are loaded only at the existing bounded charge
   surface entry/service point; a per-frame file read or decoded-frame
   allocation is a failure.
7. `tools/ipodjs_navigation_sim_regression.sh`,
   `tools/ipodjs_cache_memory_sim_regression.sh`, and the new focused gate
   pass for iPod 6G; iPod Video gets its applicable simulator/build coverage.

Hardware acceptance on the personal 6G requires active music followed by
repeated Clock/Stopwatch/Timer navigation, Hold/unlock, charger insertion,
USB enumeration, and ten rapid Menu unwinds.  Playback must never stop,
restart, or show a database-loading failure.  Test a timer expiry during music
and after retained resume.  Do not test an alarm as a power-off wake on the
6G; that behavior is explicitly unsupported.

## Completion criteria

The milestone is complete when Clock and Stopwatch are native Classic screens,
Timer is an actual independent countdown, every return path remains inside
the iPodJS hierarchy, and all rendering/persistence stays within the stated
memory and lifecycle bounds.  A generic-plugin launch, sleep-timer side
effect, false 6G wake-alarm claim, or playback-memory regression is a release
blocker.
