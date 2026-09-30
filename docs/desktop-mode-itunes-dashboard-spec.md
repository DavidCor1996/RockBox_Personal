# Desktop Mode iTunes and Dashboard Spec

Status: implementation specification for the native media and widget pass.

## Output and performance contract

- The application canvas remains the iPod's native 320x240 RGB565 plane.
  DCP750 scales the completed frame to the qualified 648x432 TV viewport.
- iTunes and Dashboard share the existing Desktop Mode compositor. Neither
  allocates a framebuffer, takes playback memory, decodes artwork in paint,
  scans storage in paint, or launches a replacement shell.
- Static desktop applications no longer force a full repaint every second.
  The menu bar, iTunes Now Playing strip, and Dashboard request bounded damage.
- Pointer cadence remains 50Hz and window drag composition remains capped at
  25Hz. Dashboard has no animation loop.

## iTunes

- Normal mode is a draggable 304x174 Aqua window above the Dock. Fullscreen
  fills only the Desktop work area and never exits Desktop Mode.
- Native one-pixel furniture replaces the reduced captured toolbar on 320x240
  so text, separators, selection, and controls stay crisp after TV scaling.
- The browser exposes Songs, Albums, Artists, Genres, and Movies from the
  existing tagcache/video index. Windowed mode presents four readable
  two-line rows; fullscreen presents six.
- Now Playing includes title, artist/album, elapsed progress, WPS access,
  previous, play/pause, next, click-to-seek, and volume controls.
- Footer controls provide page filtering, shuffle, repeat, and previous/next
  library pages. Playlist construction remains user-triggered and yields
  during larger tagcache results.

## Dashboard

- The Dashboard Dock icon opens an in-shell Dashboard instead of the
  fullscreen Clock plugin.
- Five fixed widgets are composed directly: Clock, Calendar, Weather,
  Stickies, and Now Playing. Their Snow Leopard skins are kept intact; live
  metadata is added only where Desktop Mode has a matching source (RTC,
  cached tagcache totals, and current-track metadata). There is no network
  weather substitute.
- The widget layer uses a quiet dark scrim with no generated pinstripe texture;
  the owned widget skins provide the visual detail and remain crisp after TV
  scaling. Their original PNG alpha is retained as 8-bit RGA1 coverage, so
  rounded corners reveal the scrim instead of a white or opaque rectangle.
- Now Playing offers previous, play/pause, next, and Open iTunes. Menu returns
  to the Desktop; Menu again retains the existing explicit Desktop exit flow.
- Dashboard is deliberately static apart from one-second bounded refreshes.

## Acceptance checks

- The Dock remains visible behind normal iTunes and iTunes can be dragged,
  focused, minimized, closed, and enlarged with the green title-bar button.
- Source tabs, readable metadata rows, page controls, filtering, seek, volume,
  shuffle, repeat, and playback controls work without taking audio ownership.
- Dashboard opens immediately, remains within Desktop Mode, and its playback
  controls preserve the active playlist and audio lifecycle.
- Hardware and simulator plugin builds, the Snow Leopard asset gate, plugin
  header/model checks, and mounted database validation all pass.
