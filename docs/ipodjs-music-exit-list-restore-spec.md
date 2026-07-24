# iPodJS Music Exit List Restore

## Problem

On iPodJS builds, leaving the WPS and returning to an artist or album database
list can intermittently restore the themed half-width list instead of the
native full-width list. The status area can also be repainted by the stale SBS
database title, producing a black title bar.

The full album layout and the iPodJS dashboard/WPS transition both use the
viewport manager's theme stack:

1. The iPodJS dashboard disabled the SBS before entering the database.
2. The full-width album list pushes another disabled-theme frame to suppress
   SBS action-update repaints while the album viewport is rebuilt.
3. Entering WPS incorrectly left that album-owned frame active.
4. The WPS native-screen handoff tracked only its own frame.
5. On return, album-list reinitialization could pop or repaint the stale frame
   while the database list was being redrawn.

The result depended on whether the resumed level contained album rows and on
the order of the list, SBS, and WPS refreshes, which made the failure appear
intermittent.

A second entry path exposed the same ownership gap more directly. Starting
Rockbox in Database bypassed the iPodJS dashboard, so no native-screen frame
had disabled the SBS. The native list renderer then drew beneath the legacy
status bar, including its non-iPodJS battery icon.

## Required Behavior

- Artist, album, and track database lists must use the full 320x240 iPodJS
  native list surface after leaving WPS.
- The iPone/iClassic SBS must not repaint a black database title over an
  iPodJS-native list.
- No legacy SBS title, icon, battery, or other Rockbox skin element may remain
  visible when an iPodJS browser is active.
- Native browser headers must show the iPodJS battery and charging state after
  the legacy SBS is suppressed.
- The tagtree root must be presented as `Music`, not Rockbox's internal
  `Database` title.
- Engaging Hold from any iPodJS list or submenu must show the iPodJS
  lockscreen, suppress input while locked, and redraw the same submenu after
  Hold is released.
- Full album rows must retain their existing cover-art layout, row height,
  selection, and scrolling behavior.
- Legacy non-iPodJS full album lists must continue to hide and restore their
  theme while that list is active.
- No album-list-owned viewport/theme frame may cross a tree-browser exit to
  WPS, root, a plugin, or another browser.

## Design

Treat full-screen album mode as two owned concerns:

- `force_fullscreen_albumlist` controls list layout.
- A list-owned hidden-theme stack frame suppresses legacy SBS callbacks for
  the exact lifetime of the full album layout.

iPodJS bypasses `skinlist_draw()` and draws its list over the full LCD, but the
full album transition still needs a nested disabled-theme frame: rebuilding
that viewport can otherwise wake an SBS action update after the native draw,
replacing the list body with the legacy backdrop. Each iPodJS file/database
browser also enters the same native-screen lifecycle used by the dashboard and
WPS. This consumes an existing dashboard/WPS handoff when present or disables
the SBS itself for direct startup and other entry paths.

The tree browser also releases full-screen album mode before returning a
destination screen. The database re-applies the layout from its current rows
when it is entered again.

## Simulator Acceptance

Test both artist and album return paths:

1. Start the iPod 6G simulator with `ui engine: ipodjs` and
   `album list layout: full`.
2. Enter Music and open an artist list.
3. Start or resume playback, enter WPS, and use Browse/Select to return.
4. Verify the artist list occupies the full LCD and its native gradient header
   is intact.
5. Open an album list with cover rows and repeat the WPS round trip.
6. Verify album rows remain full width and no black SBS database title is
   drawn.
7. Repeat the artist/album round trips several times to exercise the former
   ordering-dependent failure.
8. Set `start in screen: db`, restart the simulator, and verify the first
   frame is titled `Music` and contains no legacy SBS battery or title
   elements.
9. Press `h` in the simulator from the Music root and from a nested album or
   settings submenu. Verify the iPodJS lockscreen appears, list input is
   ignored, and releasing Hold returns to the same full-width submenu.
