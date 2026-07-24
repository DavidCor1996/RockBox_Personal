# Plugin Audio Lifecycle Steering

This document is mandatory steering for plugin audio work in this custom
iPod-focused Rockbox tree, especially iPod Classic 6G/7G (`ipod6g`) with the
S5L8702 SoC and CS42L55 codec.

The recurring failure mode was not only a plugin bug. Logs showed cases where
the plugin/mixer path was feeding samples, but iPod 6G produced no usable sound
after transitions such as Database music -> video. Rapid switching also exposed
freeze risk when plugin audio memory could be released while PCM or mixer
callbacks were still live. Future plugin work must preserve the lifecycle below
instead of guessing locally inside one plugin.

## Code To Check First

Before changing plugin audio behavior, compare against these files:

- `apps/plugin.c`
- `apps/plugin.h`
- `apps/plugins/mpegplayer/pcm_output.c`
- `apps/plugins/mpegplayer/stream_mgr.c`
- `apps/pcmbuf.c`
- `firmware/pcm_mixer.c`
- `firmware/target/arm/s5l8702/pcm-s5l8702.c`
- `firmware/drivers/audio/cs42l55.c`

Use `mpegplayer` as the reference for full-screen media playback. Use shorter
effect/tone plugins only as examples for short sounds, not video or long-form
media.

## Required Lifecycle

### 1. Taking The Shared Audio Buffer

If a plugin needs the shared audio buffer, call `plugin_get_audio_buffer()` and
let core stop playback and transfer ownership.

Do not pre-stop playback with:

```c
rb->audio_stop();
buffer = rb->plugin_get_audio_buffer(&size);
```

That pattern can create a different transition path from Database playback than
from Files playback and can hide the real sample-rate/codec ordering issue.

If the plugin needs to restore previous state, capture only the minimal state it
can actually restore before taking the buffer.

### 2. Full Media Playback

Long-form audio/video playback must use:

```c
PCM_MIXER_CHAN_PLAYBACK
```

Do not use a side channel for TV, movies, music, or other primary media. Side
channels are for short UI sounds, tones, or effects.

For media playback:

- set the intended mixer frequency before starting output;
- use the playback mixer channel;
- stop stale channel state before installing new callbacks;
- call `pcmbuf_fade(false, true)` when taking over the playback path if the
  plugin uses that path;
- restore the old mixer/sample-rate state on exit when the plugin saved it.

### 3. Direct PCM And iPod 6G

Direct PCM code must assume the codec and clock path might be asleep or set for
the previous playback mode.

On iPod 6G/CS42L55:

- do not bypass central PCM/mixer helpers without a specific reason;
- make sure MCLK is available before codec/sample-rate programming depends on
  it;
- respect the firmware wake path in `pcm_play_dma_start()`;
- use the plugin API `audiohw_idle_powerup()` and `audiohw_idle_powerdown()`
  only where direct PCM code truly needs target-specific codec control;
- always pair direct PCM start/stop paths so the next Database or Files track
  can start without reboot.

### 4. Pause, Seek, Segment Switch, And Restart

Before changing buffers, replacing callbacks, seeking, or switching video
segments:

- pause or stop the mixer channel cleanly;
- clear callbacks that may reference old plugin memory;
- rebuild queue state before resuming;
- keep video/audio timestamps continuous across segment boundaries;
- avoid audible gaps unless the container really has a gap.

Menu must exit back to the previous menu. Play must pause/resume. Volume must
use the same path as the WPS volume behavior.

### 5. Exiting A Plugin

Before returning from a plugin that used PCM, mixer output, or the shared audio
buffer:

- stop plugin mixer channels;
- stop direct PCM if used;
- wait briefly until `pcm_is_playing()` clears;
- clear or replace callbacks that point into plugin memory;
- restore playback source/output source if changed;
- restore sample rate or mixer frequency if changed;
- undo `pcmbuf_fade(false, true)` with `pcmbuf_fade(false, false)` if used;
- only then call `plugin_release_audio_buffer()` if the plugin took it.

Plugin memory must not be freed while any callback can still read from it.

### 6. User Music And Playlists

Games and plugins must not rewrite or replace the user's active playlist just
to play their own music. If user music is active:

- leave it alone when possible;
- skip plugin background music if needed;
- use local PCM/mixer output only for plugin-owned sound;
- restore a clean state so Database and Files playback both work after exit.

Avoid `playlist_create()`, `playlist_remove_all_tracks()`, or similar global
playlist mutation from plugins unless the plugin is explicitly a playlist tool.

Playback-adjacent display plugins must also avoid ordinary `font_load()` when
entering from WPS with active playback. Its default core glyph cache can invoke
playback's audio-buffer shrink callback, evict the codec, and force a codec and
metadata reload even though the plugin never requested the shared audio buffer.
Reuse an already-loaded UI font, or use an explicitly bounded allocation path
that has been proven not to shrink playback. For iPodJS Lyrics, restore WPS
state only for exits that actually return to WPS. A Lyrics Menu exit must carry
its intent through normal plugin teardown and return to the saved Music browser
without reconstructing WPS first. WPS was already left before `plugin_load()`;
normal plugin teardown restores the theme, so an extra WPS enter/leave pair is
both unnecessary and a source of playback-adjacent redraw work. Do not depend
on the raw Menu release surviving `plugin_load()`, because plugin cleanup clears
the button queue.

## Anti-Patterns

Do not introduce these patterns:

- `audio_stop(); plugin_get_audio_buffer();`
- `plugin_release_audio_buffer()` while mixer/PCM callbacks still reference
  plugin memory.
- `pcm_play_data()` from plugin memory followed by exit without stop/wait.
- Long-form video or music on a non-playback mixer channel.
- Direct CS42L55 power manipulation without matching stop/restore behavior.
- Replacing the active playlist from a game or emulator so it can play music.
- Segment switching that stops audio hard enough to create a skip between
  parts of one logical video.

## Required Device Test Matrix

For any plugin audio change, test on real iPod hardware when possible:

- fresh boot -> video/plugin with sound;
- Database music -> video/plugin with sound;
- Files music -> video/plugin with sound;
- video/plugin -> Database music starts with sound;
- video/plugin -> Files music starts with sound;
- Database music -> game/emulator -> music still works;
- game/emulator with sound -> Database music still works;
- rapid plugin/menu/music switching does not freeze;
- volume changes during plugin audio do not create static or mute output;
- play pauses/resumes video;
- menu exits video to the previous menu;
- long videos and multi-part videos remain in sync across segment boundaries.

For OpenH264/video work, check mounted-device logs before guessing. Current
logs are expected under:

```text
/.rockbox/openh264/openh264_profile.log
```

Useful log fields include:

- `audio_status()`
- `pcm_is_playing()`
- `mixer_get_frequency()`
- mixer channel status and waiting bytes
- saved playback status/elapsed/offset
- audio buffer pointer and size
- video segment index and audio/video timestamps

## Build And Deployment Notes

Firmware or plugin API changes require rebuilding and rebooting the device into
the new firmware. Plugin-only changes usually require copying the rebuilt
`.rock` file and restarting the plugin.

If a change affects `apps/plugin.h`, bump the plugin API version and rebuild all
plugins. A stale plugin/core API mix can look like a runtime audio bug.
