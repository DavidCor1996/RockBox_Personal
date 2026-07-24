# iPodJS Play-Hold Power-Off Transition

## Problem

On the iPodJS home screen, holding Play for software power-off also emits
`ACTION_TREE_STOP` repeats while the button driver counts toward
`SYS_POWEROFF`.  The first repeat must be useful as a medium-hold Stop, while a
continued hold must remain on the current iPodJS frame until shutdown instead
of exposing the WPS or a stale Music frame.

## Required behavior

- A short Play press keeps its existing play/pause behavior.
- The first Play repeat stops audio once. Releasing then redraws the same menu
  with its playback indicator removed.
- Continuing to hold after that repeat keeps the last fully drawn iPodJS home
  frame unchanged until committed power-off.
- A Stop-hold release is consumed and never becomes a second Play command,
  WPS transition, or menu exit.
- A committed iPodJS power-off performs a CRT-style collapse of the current
  frame, resolves to a center line and dot, then leaves the LCD black.
- Rockbox shutdown text or theme elements are not drawn after the effect.
- If tagcache is busy and shutdown is cancelled, the CRT effect does not run
  and the usable screen remains visible.
- Reboot and non-iPodJS shutdown behavior remain unchanged.

## Implementation contract

The dashboard observes raw `BUTTON_PLAY` state because the action layer does
not emit a mapped action at initial Play-down.  While raw Play is held, it
suppresses preview, pause-state, and list redraws but continues polling system
events.  The first Stop repeat changes playback state once; its release is
consumed, while a continued hold can still deliver `SYS_POWEROFF` to core
cleanup. Custom iPodJS submenus apply the same Stop-release suppression.

Core cleanup checks tagcache before invoking the iPodJS animation.  Once the
animation has ended on black, the main LCD is excluded from the generic clear
and shutdown-message path while audio, playlist, settings, and storage cleanup
continue normally.

## Simulator acceptance

1. Start audio and confirm that the home screen and Music submenus show the
   iPodJS play indicator beside the battery.
2. Hold Play through the first repeat and release. Audio must stop, the current
   menu must remain open, and the indicator must disappear without a WPS frame.
3. Restart audio, return home, and continue holding Play through software
   power-off while capturing every presented simulator frame.
4. Frames after the first repeat through the countdown must retain the frozen
   home frame; the stopped playback state is not redrawn during the long hold.
5. The next frames must contain only the CRT collapse and black screen; no
   Music, WPS, Database, theme, or shutdown-message frame may appear.
