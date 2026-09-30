# Desktop Mode Plugin Improvements Spec

Status: implementation specification for the firmware Desktop Mode plugin.

## Availability

Desktop Mode is a video-out companion surface, not a general local plugin.
The root-menu entry and the iPodJS Applications entry are visible only when a
qualified video dock has brought the 6G video-out pipeline active. The
simulator keeps the entry available so the plugin can be tested without a
physical accessory. Other targets, and a 6G that is undocked or still being
identified, must hide the entry. A direct stale shortcut must fail before the
desktop boot animation and must not allocate or change playback state.

The simulator exception is an explicit session, not an emulated hardware dock.
The root menu and Rockpod host preview launch `desktop_mode.rock` with the
`simulator-desktop` token. The plugin rejects an unqualified simulator launch,
while hardware accepts only `videoout-active` and continues to consult live
video-out state. `tools/desktop_mode_sim_gate.py` stages an isolated private
asset pack, launches that session headlessly, and verifies the rendered Aurora
desktop before preserving a screenshot.

If a dock is removed while the plugin is running, Desktop Mode stops at the
next loop boundary and returns to the normal Rockbox screen. It must not leave
the video-out clock, wheel-event policy, or display state owned by the plugin.

## DCP750 source and output scaling

The firmware plugin is a 320x240 source-space application. It does not render
at the television's output size. The iPod 6G video-out compositor converts the
finished RGB565 frame to the DCP750's NTSC 720x480 raster and uses the
qualified `(36,24) 648x432` destination viewport. The VP owns this final
non-integer scale, so the plugin's Snow Leopard assets remain at their
verified 320x240 sizes and all window, Dock, icon, and pointer coordinates
stay in one consistent space.

Fullscreen means filling the source work area below the 21-row menu bar
(`320x219`), followed by the same video-out scale. It never selects a 720p or
1080p firmware canvas and never allocates a TV-sized scratch buffer. The
separate 1920x1080 simulator profile is host-only and is not part of the
DCP750 path.

## Dock and Launchpad

The physical 320x240 Dock has eight fixed slots: Finder, iTunes, Preview,
TextEdit, Calculator, Dashboard, System Preferences, and Trash. Eight slots at
the existing 34px pitch fit inside the captured 288px shelf, including the
normal 34/38px magnification envelope. DIRECTV, Sitekick, Netflix, Steam, and
any other optional applications are launched from the in-window Launchpad
available from an application's View menu instead of extending the Dock.

Launchpad is an in-plugin grid backed only by the already-loaded application
icons and names. It has one control per application, uses the same click-wheel
pointer and hit testing as the desktop, and does not allocate another
framebuffer. Selecting an application closes Launchpad and opens that
application in its normal Desktop Mode window state.

## Pointer responsiveness

The plugin polls input at up to 60Hz and commits pointer-only dirty rectangles
at up to 50Hz. Pointer movement keeps the existing clipped composition and
partial LCD update path; it must not force a full-screen redraw or perform
filesystem work. Wheel contact by itself does not move the pointer. Slow wheel
motion uses a low-gain subpixel curve, while sustained fast motion receives a
bounded acceleration term. A stationary directional glide begins only after a
short settle interval and ramps slowly enough to remain controllable.

Select freezes stationary glide from mouse-down through mouse-up, including
the release poll, so the click lands on the pixel the user chose. Reversing
direction clears acceleration, a lift clears all fractional motion, and a
fresh touch never inherits velocity from the previous gesture. The target is
under 30ms from an accepted host or wheel sample to a visible pointer update
on simulator and hardware.

## Window and fullscreen behavior

Every application opened from the Desktop Mode Dock or Launchpad starts in
windowed mode. Every application window exposes the green title-bar control
as a real hit target. Activating it toggles an in-plugin fullscreen state that
keeps the Desktop Mode event loop, menu bar, pointer, and application state
alive; it is not a `PLUGIN_GOTO_ROOT` or a second plugin session. The Dock and
desktop icons are hidden while the app is fullscreen, and the application
surface uses the available area below the menu bar.

Menu has one predictable escape ladder:

1. from a fullscreen or windowed application, Menu returns to the Desktop;
2. from the Desktop, Menu opens the existing return confirmation;
3. confirming returns to ordinary Rockbox.

Menu still dismisses an open overlay first. This rule also applies when Finder
is showing a nested directory; the back/forward toolbar remains available for
navigation without changing the global Menu escape.

## Resource limits

Launchpad reuses the existing decoded icon assets and the single compositor
plane. It adds no full-screen cache, core allocation, audio-buffer ownership,
playlist mutation, or per-frame file access. The dock reduction removes
unreachable overflow controls rather than scaling or clipping application
artwork.

## Acceptance checks

- Desktop Mode is absent when undocked, pending, blocked, or on non-6G native
  targets; it remains available in the simulator.
- A 320x240 Dock's normal and magnified icons stay within the shelf bounds.
- Launchpad reaches every app omitted from the fixed Dock.
- Green toggles fullscreen without leaving the plugin; Menu reaches Desktop,
  then the return confirmation.
- Host pointer movement and wheel motion use the partial update path and do
  not perform I/O in paint functions.
- Touch-down alone produces no cursor jump; slow strokes can approach every
  title-bar, Dock, and Launchpad target without overshoot, and Select release
  cannot restart glide before dispatching the click.
- Existing asset, playback, USB, and cleanup gates continue to pass.

## Biggest-wins implementation pass (2026-08-31)

This pass targets the interaction defects that are most visible during a
stock-desktop session. It deliberately stays inside the existing compositor
and does not change playback ownership, the personal-library asset path, or
the physical deploy package.

### P0: keep the shell reachable from iTunes

The Dock remains an active control band while the native 320x240 iTunes window
is open. Finder, Dashboard, Preferences, and the other fixed Dock apps must be
reachable without first returning to the Desktop. Launchpad remains a separate
application and keeps its existing Dock-hidden presentation.

Acceptance: open iTunes, click another fixed Dock app, and return to iTunes
without leaving Desktop Mode or losing the current tagcache-backed list.

### P0: one scrollbar geometry contract

The iTunes scrollbar uses one minimum thumb height for painting and pointer
mapping. The thumb must remain large enough to acquire on the TV-scaled 320x240
surface, while its page position remains clamped to the same track travel used
by the visual thumb.

Acceptance: clicking or dragging anywhere on the visible track selects the
corresponding page, the thumb never becomes a single-pixel mark, and the
rendered thumb and hit-test mapping agree at both ends of the list.

### P0: Menu uses the shell's window lifecycle

Menu from a normal application returns to the Desktop through the existing
bounded Dock minimize animation, preserving the application in the running
window stack for Dock restore. Dashboard still closes directly because it is a
desktop layer rather than a window; Menu from the Desktop continues to open the
explicit return confirmation.

Acceptance: Menu from iTunes or Finder visibly minimizes to its Dock slot,
clicking that slot restores it, and Menu from Dashboard returns cleanly to the
Aurora Desktop.

### Verification

- Focused Desktop Mode and Snow Leopard asset tests pass.
- Simulator and native iPod 6G `desktop_mode.rock` builds pass.
- The simulator gate verifies Dashboard, Launchpad, and the DCP750 source-space
  projection; a device-backed capture verifies iTunes artwork, selection,
  scrollbar, and Dock switching.
- No physical iPod deploy is part of this pass.

## Next visual pass (2026-08-31)

The next pass keeps the same single compositor plane and improves the three
most visible desktop surfaces at the native TV-out size.

### P0: focused window lifecycle

Closing the front application window exposes the most recently raised
survivor. Window-menu Show Desktop and the global Menu escape use the bounded
Dock minimize path, so running applications remain restorable from their Dock
slots. The visible stack remains capped at the existing three windows.

### P0: native Albums browser

The 320x240 iTunes Albums source becomes a two-by-two tile browser. Each tile
uses the existing bounded, idle-loaded personal cover sidecar and displays the
album and artist. Album paging advances by the visible tile count; songs,
artists, genres, and videos retain their existing readable list layouts.

The native iTunes transport strip is reflowed from the captured 42px band to
34px, using the same captured Aqua pixels at the smaller source-space size.
The freed eight pixels increase the song list to five rows; the compact 34px
Now Playing panel has no separator crossing its text.

### P0: TV-out safe area

Native source-space window motion and desktop labels remain inside an 8px edge
margin. Dock magnification and the existing DCP750 viewport remain unchanged;
the margin only prevents text or window edges from being lost at the TV
boundary.

Launchpad labels are measured against the same compact tile as their icon and
anchored to the icon centre before being clamped to that tile. This keeps
short labels visually centred and prevents a clipped long label from looking
randomly left-aligned on the 320x240 source.

### Acceptance

- Close an active window with another window open and see the previous window
  immediately become active.
- Open iTunes > Albums and see four real cover-backed tiles; next/previous page
  controls advance one complete tile page.
- Drag a normal window and inspect the desktop on the DCP750 projection: no
  desktop label or window edge crosses the source safe margin.
- No paint path performs I/O, tagcache work, or bitmap decoding, and no new
  framebuffer or playback buffer is introduced.
