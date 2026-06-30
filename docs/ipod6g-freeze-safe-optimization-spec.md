# iPod 6G Freeze Safe Optimization Spec

## Goals

- Reduce lockscreen and WPS redraw pressure after hold/idle transitions.
- Avoid waking the display skin while hold is active during headphone or lineout
  plug events.
- Remove unfinished `ipodtiktok` and `nightcity` plugins from build/package
  outputs.
- Keep iPone Designer lockscreen clock behavior consistent between SBS and WPS.
- Prevent saved designer variants from using icon/symbol fonts for the
  lockscreen time, which can make the clock disappear.
- Do not deploy to the mounted iPod until explicitly approved.

## Changes

- `apps/gui/skin_engine/skin_render.c` keeps alpha full-redraw promotion scoped
  to the affected viewport instead of promoting the whole skin.
- `apps/misc.c` no longer calls `backlight_on()` for headphone/lineout plug-in
  events while hold is active. Pause/resume logic remains unchanged.
- `apps/plugins/SOURCES`, `apps/plugins/SUBDIRS`, and
  `apps/plugins/CATEGORIES` no longer expose `ipodtiktok` or `nightcity`.
- `apps/plugins/bitmaps/native/SOURCES` no longer builds Night City-only
  bitmap assets. The iPodTikTok bitmap assets remain because `mpegplayer` still
  includes them.
- `rockpod/services/theme_designer.py` reserves the top status row for both SBS
  `iPoneLockscreen` and WPS `Lockscreen`, and moves the date closer to stretched
  clocks without overlapping the main time.
- `rockpod/services/theme_designer.py` normalizes icon/symbol lockscreen clock
  fonts to `fonts/90-Cantarell-Regular.fnt`; the Cyberpunk variant is pinned to
  that same readable clock font so SBS and WPS render the same clock.

## Non-Goals

- No speculative codec, PCM, mixer, or headphone detection driver changes.
- No deletion of source directories in this pass; removed plugins are detached
  from the build and menu/package surfaces for safer rollback.
- No device push without approval.
