# iPod Stock Clock, Weather, and Lyrics Pass

## Reference Targets

- iPod Classic/Nano split menus show the selected app preview in a full-height
  right pane. The stock Extras/Clock preview uses a muted grey world-map
  background, a white analog face with shadow, and a date label below it.
- The iPod nano Clock guide describes Clock as the entry point for clock faces,
  stopwatch, timer, and alarms, with stopwatch/timer screens as peer Clock
  functions.
- Music select-hold should behave like the standard WPS path in this tree:
  launch `lrcplayer.rock`, not the playlist viewer.
- Weather icons are magenta-keyed BMP assets. Any resized/dithered decode must
  clean near-magenta edge pixels after decode so halos do not render.

Reference URLs:

- https://www.walmart.ca/en/ip/Apple-iPod-Nano-3rd-Gen-8GB-Black-MP3-Player-Excellent-Condition/79MZXHWJBS5C
- https://ipod-nano.helpnox.com/en-us/ipod-nano-user-guide/chapter-9-other-features/tracking-time/
- https://en.wikipedia.org/wiki/Natural_Earth

## Implemented In This Pass

- Clock preview assets were regenerated as stock-style full-height panes:
  `assets/ipodjs/rockbox/previews/clock.174x240x24*.bmp` and matching 220px
  fallbacks.
- Extras Clock no longer draws its own analog face/ticks/hands in C. It uses the
  Clock pane asset and overlays only live text for date/time.
- Active-playback home previews no longer run slideshow image decodes. This
  avoids expensive album/game/video preview work while audio is playing and the
  user returns from WPS/home.
- Custom iPod WPS select-hold now launches `lrcplayer.rock`. Playlist viewer
  remains on the explicit playlist action.
- Home Music select-hold and Music > Now Playing select-hold launch lyrics.
- Weather plugin icon decode now post-processes near-magenta pixels after
  resize/dither, matching the root menu preview transparency cleanup.

## Recommended Next Asset Passes

- `apps/plugins/clock`: replace legacy plugin bitmap/style with the same stock
  Clock asset family and Clock/Stopwatch/Timer navigation.
- `apps/plugins/stopwatch`: make it visually match the stock dark nano
  stopwatch/timer screens.
- `apps/plugins/calendar`: replace old drawn calendar chrome with an Apple
  Extras-style calendar pane.
- `apps/plugins/alarmclock` and `apps/plugins/chessclock`: move away from
  drawn utility UI toward stock iPod utility screens.
- Weather backgrounds: keep the current app, but source/prepare a complete
  Apple-like weather asset set with clean keyed transparency at native sizes.
