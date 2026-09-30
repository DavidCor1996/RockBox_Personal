# Desktop Mode Fast Desktop Interaction Spec

Status: implementation specification for the high-impact stock-desktop pass.

## Experience target

Desktop Mode should behave like a compact Snow Leopard desktop expressed in
the iPod's native 320x240 source space. The DCP750 remains responsible for the
qualified 648x432 TV viewport; no UI geometry, cache, or pointer calculation
uses a 720p or 1080p firmware canvas.

The shell prioritizes immediate input response over decorative work. Pointer
updates remain clipped, window state is fixed-size, and no interaction may
decode assets, scan storage, query tagcache, allocate a framebuffer, or take
playback memory.

## Windows and focus

- Finder, iTunes, Preview, TextEdit, Calculator, Preferences, and Launchpad
  remain running as in-shell applications until closed.
- The visual focus stack contains at most three windows: the active window and
  two inactive windows. Older running applications remain represented in the
  Dock and can be brought forward without retaining another rendered surface.
- Inactive windows reuse the loaded plain chrome, receive a subdued title bar,
  and do not redraw application content. The active window alone paints live
  content and colored controls.
- Clicking a visible inactive title bar or its Dock icon raises that app.
- A normal title bar can be dragged. Each app remembers its bounded position
  for the session. iTunes' 320x240 media layout and fullscreen windows do not
  drag.
- Window motion is committed at no more than 25Hz while the cursor continues
  at the normal interaction rate. This bounds full-window composition during
  a drag without making the pointer feel attached to a slow frame.

## Dock

- Dock magnification uses three timed stages: settle, small magnification, and
  full magnification with neighboring lift.
- Tooltips appear only after a stable hover, preventing labels and large icons
  from flashing while the pointer crosses the Dock.
- Running indicators remain visible. Minimize/restore keeps the bounded genie
  transition and Dock clicking raises an already-running app.
- Dock stage changes invalidate only the Dock band through the existing damage
  path. They do not force a full desktop update.

## Desktop selection

- One click selects the iPod, Documents, or Music icon and paints the native
  blue label selection.
- Double-click opens the selected icon.
- Clicking empty wallpaper clears the selection.
- Hidden desktop icons have no stale hit targets.

## Application menus

The active app exposes File, Edit, View, and Window menus in the top bar. The
menus use the loaded Aqua panel and selection artwork and provide bounded
shell actions: open Finder/TextEdit/Preferences/Launchpad, close or minimize,
show Desktop, switch windows, toggle fullscreen/icons, refresh, and request a
Desktop exit. Menu actions never chain to the ordinary Rockbox Applications
screen.

## Performance and memory budgets

- Additional persistent state is under 128 bytes: three app identifiers,
  per-app signed-byte positions, and interaction timestamps/flags.
- No additional bitmap, full-screen plane, audio buffer, file descriptor, or
  heap/core allocation is introduced.
- Steady pointer movement remains a clipped partial update at up to 50Hz.
- Dock animation repaints only the Dock band. Window drag is capped at 25Hz.
- At most two inactive chrome windows are composed, and only through the
  current damage clip. Their body content is never rendered.
- No paint function performs file I/O, tagcache work, or bitmap decoding.

## Acceptance checks

- Slow pointer motion and Select still land on traffic lights and menu rows.
- Three internal apps can be opened, raised from the Dock, and cycled from the
  Window menu; a fourth does not increase the visual stack or memory use.
- Window dragging stays inside the visible desktop and does not make pointer
  motion stall.
- Desktop selection, double-click, wallpaper clear, and hidden-icon hit tests
  behave consistently.
- Dock magnification settles progressively and delayed tooltips do not flicker.
- Menu, fullscreen, close, minimize, and Desktop return never leave Desktop
  Mode unless the explicit exit confirmation is accepted.
- Plugin size/load address, database, playback ownership, personal assets, and
  video-out qualification remain unchanged by deployment.
