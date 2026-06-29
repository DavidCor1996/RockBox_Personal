# OpenH264/RVP Video Controls Spec

## Goal

Make `openh264_player` feel like the WPS while video is playing:

- video stays full-screen and smooth;
- progress controls appear when the user touches the iPod and disappear when
  idle;
- volume changes show a temporary volume overlay;
- `Play/Pause` pauses and resumes;
- `Menu` exits back to the previous browser/menu;
- enabling the hold switch must not stop, pause, skip, or exit video.

This work is UI/control only. It must not change the RVP file format, RockPod
conversion profile, mixer channel, audio buffer ownership, or segment playback
lifecycle except where explicitly required to keep controls correct.

## References

Check these before editing:

- `docs/plugin-audio-lifecycle-steering.md`
- `apps/plugins/openh264_player.c`
- `apps/plugins/mpegplayer/mpegplayer.c`
- `apps/plugins/mpegplayer/mpeg_misc.c`
- current iPone WPS slider assets under `.rockbox/wps/iPone/`

Use `mpegplayer` OSD concepts as the reference:

- `struct osd`
- `OSD_REFRESH_VOLUME`
- `OSD_REFRESH_TIME`
- `OSD_SHOW`
- `osd_refresh_time()`
- `osd_refresh_volume()`
- `osd_set_volume()`
- WPS slider bitmap fallback behavior

Do not copy mpegplayer's stream/thread model into `openh264_player`; RVP is a
single plugin loop with raw YUV and PCM sidecars.

## User Controls

### Normal Playback

| Input | Behavior |
| --- | --- |
| `Menu` / WPS menu / cancel | Exit video to the previous menu/browser. Never skip to the next RVP segment. |
| `Play/Pause` | Pause or resume video and audio. |
| Wheel clockwise / WPS volume up | Raise volume by the same step WPS uses and show volume overlay. |
| Wheel counter-clockwise / WPS volume down | Lower volume by the same step WPS uses and show volume overlay. |
| `Select` / `ACTION_STD_OK` | Show controls overlay only. It must not pause, exit, or seek. |
| `Right` / `ACTION_WPS_SEEKFWD` | Fast-forward video and audio from the current logical position. |
| `Left` / `ACTION_WPS_SEEKBACK` | Rewind video and audio from the current logical position. |
| Any other non-system button | Wake/show the controls overlay, then continue playback. |
| USB/system event | Exit through the normal plugin shutdown path. |

### Hold Switch

When hold is enabled during video playback:

- video and audio continue;
- current segment and later segments continue normally;
- no held button/wheel event may pause, seek, exit, skip, or change volume;
- controls overlay may time out and hide normally;
- backlight policy remains video mode: keep backlight on while playback is
  active.

When hold is disabled:

- the next real button/wheel action should wake the controls overlay;
- no queued hold-era buttons should be replayed.

Implementation note: the input path should ignore actions while hold is active
instead of treating hold-generated cancel/menu/release events as commands. If
the plugin API does not expose a clean hold query for this target, use the same
button/action filtering pattern used by core WPS or mpegplayer for held input.

## On-Screen Controls

### Visibility

Add a small OSD state object to `openh264_player.c`, separate from audio state:

```c
struct raw_osd {
    bool visible;
    bool volume_visible;
    bool paused;
    long hide_tick;
    long volume_hide_tick;
    long duration_frames;
    long current_frame;
    int fps;
};
```

Required timing:

- Do not show controls automatically on video start; start playback cleanly.
- Show controls for about `2 * HZ` after any accepted user input.
- Show volume overlay for about `2 * HZ` after a volume change.
- During active playback, update logical state but avoid framebuffer redraws.
- When idle timeout expires, stop drawing controls. Do not clear the whole LCD;
  let subsequent video frames naturally cover old overlay pixels.

### Progress Bar

Draw the progress UI in the lower part of the screen, WPS-style:

- current time at lower left;
- duration at lower right;
- thin horizontal progress bar between them;
- optional pause/play icon above or near the bar;
- no large card, no opaque full-width panel, no video crop.

Preferred layout for 320x240:

- text baseline: bottom 18-22 px;
- progress bar height: 3-5 px;
- horizontal margins: 8-12 px;
- time text width should adapt to duration:
  - `< 10 min`: `M:SS`
  - `< 1 hr`: `MM:SS`
  - `>= 1 hr`: `H:MM:SS`

Progress calculation:

- Use logical playback position, not wall-clock alone.
- For one-segment files:
  - `current = frame_index`
  - `duration = frames`
- For multi-segment files:
  - precompute or accumulate total frames across all segments before playback;
  - `current = frames_completed_before_segment + frame_index`;
  - duration is total logical frames.
- Segment switching must not reset the bar to zero.

If total frames cannot be known because a sidecar is missing or unreadable,
disable only the duration/progress calculation and still allow video playback to
fail through the existing error path.

### Volume Overlay

Volume overlay appears only after volume changes:

- use `rb->adjust_volume(delta)` or the same WPS helper path currently used;
- read the displayed value from `rb->global_status->volume`;
- show a small right-side or upper-right overlay containing:
  - volume icon or short label;
  - numeric physical value plus unit, using `sound_val2phys()` and
    `sound_unit(SOUND_VOLUME)`;
  - optional compact bar.

Volume overlay must be independent from the progress overlay:

- changing volume shows volume overlay even if progress overlay is hidden;
- touching another key may show progress overlay too;
- both overlays should disappear on their own after idle timeout.

### Pause Overlay

On pause:

- stop/pause audio through the existing safe RVP pause path;
- freeze on the current video frame;
- show controls indefinitely while paused;
- show a small paused indicator;
- `Play/Pause` resumes from the same logical frame and hides controls after the
  normal idle timeout.

Do not spin in a busy loop while paused. Use `sleep(1)` or existing plugin
action polling cadence.

## Rendering Constraints

RVP playback is timing-sensitive. The OSD must be cheap:

- Do not allocate per frame.
- Do not read BMP assets per frame.
- Do not clear full screen to hide controls.
- Do not force an extra full-frame redraw for OSD only.
- On iPod 5G/6G hardware, do not draw normal framebuffer OSD over active YUV
  video blits. It flickers because the video path presents through
  `lcd_blit_yuv()` while OSD uses framebuffer LCD updates.
- Draw full controls while paused or before active video blitting resumes.
- During active playback, seek/volume/select may update state and logs, but
  visual OSD redraws must not run every video frame.
- Keep fallback drawing fully functional if WPS bitmap assets are unavailable.

Recommended implementation:

1. Keep playback on the direct YUV path with no startup OSD.
2. Show progress and WPS-style volume art when paused.
3. For live seek feedback while playing, prefer a non-flickering future path
   that composites into the YUV frame before `lcd_blit_yuv()`.
4. Keep all OSD buffers static or inside already-owned plugin memory.

The OSD should not use the PCM/audio buffer area in a way that changes current
audio memory ownership or callback lifetime.

## Integration Points

### Input

Replace `handle_playback_input()` with an input function that returns both a
command and an OSD hint:

```c
enum raw_input_command {
    RAW_INPUT_NONE,
    RAW_INPUT_EXIT,
    RAW_INPUT_TOGGLE_PAUSE,
    RAW_INPUT_VOLUME_CHANGED,
    RAW_INPUT_SHOW_OSD,
    RAW_INPUT_SEEK,
};
```

Rules:

- `ACTION_WPS_VOLUP` and `ACTION_WPS_VOLDOWN` adjust volume and return
  `RAW_INPUT_VOLUME_CHANGED`.
- `ACTION_WPS_SEEKFWD` and `ACTION_WPS_SEEKBACK` return `RAW_INPUT_SEEK` with
  signed frame delta.
- `ACTION_WPS_PLAY` returns `RAW_INPUT_TOGGLE_PAUSE`.
- `ACTION_STD_OK` returns `RAW_INPUT_SHOW_OSD`.
- `ACTION_STD_CANCEL`, `ACTION_WPS_MENU`, and `ACTION_WPS_STOP` return
  `RAW_INPUT_EXIT`, unless hold is active.
- default/system events still pass through `default_event_handler()`.

### Playback Loop

In `play_raw_segment()`:

- initialize OSD with segment/global frame counts before starting audio;
- after every frame blit:
  - update `raw_osd.current_frame`;
  - do not draw framebuffer OSD unless playback is paused;
- after every accepted input:
  - call `raw_osd_show()` for controls-only actions;
  - if volume changed, call `raw_osd_show_volume()`;
  - if seek changed position, save/log the new logical frame and restart audio
    from the resolved local frame;
- on segment transition:
  - keep global frame offset;
  - do not clear the screen for OSD reasons;
  - do not make menu/cancel act like next segment.

### Multi-Segment Duration

Before the first segment starts, compute `total_frames`:

- for each segment, open its YUV sidecar;
- divide file size by frame size;
- store segment frame counts in a small static array parallel to
  `raw_segments`;
- log and abort if a segment size is not frame-aligned.

This is fast because it only reads file metadata, not video content.

## Fast-Forward And Rewind

### Goal

Seeking must behave like one continuous video, even when RockPod split the RVP
into multiple sidecar parts. It must never seek inside only the current segment
unless the target frame actually stays in that segment.

Required behavior:

- `ACTION_WPS_SEEKFWD` seeks forward.
- `ACTION_WPS_SEEKBACK` seeks backward.
- Seeking is frame-accurate enough to keep audio/video sync. The seek target is
  always a logical global frame, not a wall-clock-only value.
- Seeking across a segment boundary must reopen or switch to the correct segment
  and start audio from the matching local frame.
- Seeking to before the start clamps to frame `0`.
- Seeking past the end clamps to the last playable frame or exits cleanly at end
  of video; it must not wrap around.
- The progress position updates immediately after the seek.
- Menu must still exit; it must not be overloaded as "next segment".

### Seek Step Policy

Use WPS-style repeated seek behavior without adding a separate seek screen:

- single accepted seek action: `5 seconds`;
- repeated seek actions within `HZ`: keep seeking in `5 second` steps;
- after 3 consecutive repeats in the same direction: `15 second` steps;
- after 10 consecutive repeats in the same direction: `60 second` steps.

All steps convert to frames as:

```c
delta_frames = seconds * fps;
```

Use the RVP marker FPS. If FPS is invalid, fall back to `RAW_DEFAULT_FPS`.

### Seek Implementation

Add a seek command to the input/result path:

```c
enum raw_input_command {
    RAW_INPUT_NONE,
    RAW_INPUT_EXIT,
    RAW_INPUT_TOGGLE_PAUSE,
    RAW_INPUT_VOLUME_CHANGED,
    RAW_INPUT_SHOW_OSD,
    RAW_INPUT_SEEK,
};

struct raw_input_result {
    enum raw_input_command command;
    long seek_delta_frames;
};
```

Implementation rules:

- Do not seek by byte time guesses.
- Convert current logical frame plus `seek_delta_frames` to a target logical
  frame.
- Resolve target logical frame with the precomputed segment frame table:
  - `segment_index`
  - `segment_frame_offset`
  - `local_frame`
- Before changing segment or local frame:
  - stop the raw audio path through the existing safe stop helper;
  - finish or cancel any prefetch that can reference old segment buffers;
  - clear pending button input that belongs to the old seek burst only if it
    would replay stale input.
- Within the same segment:
  - `lseek()` the YUV sidecar to `local_frame * frame_size`;
  - restart audio with `raw_audio_start_at_frame(audio_buf, audio_size,
    local_frame, segment_audio_frame_size)`;
  - set `frame_index = local_frame`;
  - reset timing base so the next frame deadline is relative to the new seek
    point.
- Across segments:
  - stop the current segment loop with a non-error "seek requested" result;
  - return the target logical frame to `play_raw_rvp()`;
  - reopen the target segment;
  - start at the resolved local frame.

Do not release the plugin audio buffer or rebuild global audio ownership during
a seek. This is an in-plugin reposition, not an exit/re-enter of playback.

### Seek While Paused

Seeking while paused is required:

- audio is already stopped;
- update current logical frame;
- draw the newly selected video frame if cheap and available;
- keep the paused controls visible;
- pressing `Play/Pause` resumes from the new frame.

If drawing the exact paused seek frame is risky for the first implementation,
show the updated time/progress immediately and draw the target video frame on
resume. Audio must still resume from the target frame.

### Seek Logging

Log only accepted seek state changes:

```text
mode=raw_controls stage=seek from=1234 to=1534 segment=2 local=94
mode=raw_controls stage=seek_boundary from_segment=1 to_segment=2
```

Do not log repeat polling that does not change the accepted target frame.

## Resume Playback

### Goal

Opening a video from the Videos menu should continue where the user left off,
without requiring a reboot, manual file picking, or a separate plugin menu.

Resume is logical-frame based:

- store the path of the `.rvp`;
- store the logical global frame;
- store total frames and FPS for validation;
- optionally store file sizes or mtimes of the `.rvp` marker and sidecars so a
  stale resume point can be ignored after RockPod reconverts the video.

### Resume File

Use a plugin-owned resume file:

```text
/.rockbox/openh264/openh264_resume.cfg
```

Format should be line-based and cheap to parse. Suggested record:

```text
path=/Videos/Shows/Recess/Season 01/Recess S01E01.rvp
frame=12345
total=28764
fps=20
marker_size=233
marker_mtime=...
updated=...
```

Only one active resume record is acceptable for the first implementation. A
multi-record resume database can be added later, but the first pass must work
for repeatedly opening the same video from the Videos menu.

### Save Policy

Save resume state:

- on clean `Menu` exit;
- on pause;
- after an accepted seek;
- every 5 seconds of playback at most.

Do not write the resume file every frame.

Clear or ignore resume state:

- if current frame is less than 5 seconds from the beginning;
- if current frame is at or past 95% of total frames;
- if the path no longer matches;
- if total frames or marker metadata no longer matches after reconversion;
- if parsing fails.

### Start Policy

When opening an RVP:

1. Parse marker and compute segment frame counts.
2. Load resume state for the current path.
3. If the saved frame is valid and between 5 seconds and 95%:
   - start automatically at that frame.
4. Otherwise start at frame 0.

First implementation should auto-resume without a prompt. This keeps the Videos
menu flow simple and satisfies "resume where I left off". A later settings pass
can add "Ask / Always / Never" if needed.

### Audio Lifecycle

Resume uses the same path as seek:

- resolve saved logical frame to segment/local frame;
- open the correct YUV/PCM sidecars;
- call `raw_audio_start_at_frame()` with the local frame;
- do not call `audio_stop()` directly;
- do not release/reacquire the plugin audio buffer just to resume.

Resume must pass the same Database music -> video and video -> Database music
tests as normal playback.

## Logging

Append low-volume control diagnostics to
`/.rockbox/openh264/openh264_profile.log` only at state changes:

- `mode=raw_controls stage=start`
- `mode=raw_controls stage=show_osd frame=X total=Y`
- `mode=raw_controls stage=volume volume=...`
- `mode=raw_controls stage=pause frame=X`
- `mode=raw_controls stage=resume frame=X`
- `mode=raw_controls stage=seek from=X to=Y`
- `mode=raw_controls stage=resume_load frame=X valid=0|1`
- `mode=raw_controls stage=resume_save frame=X total=Y`
- `mode=raw_controls stage=hold ignored=1`
- `mode=raw_controls stage=exit`

Do not log every frame.

## Acceptance Tests

### Simulator

Run a 320x240 simulator with a short RVP:

- playback starts without flashing from an automatic startup OSD;
- pausing shows the progress overlay;
- progress bar advances and reaches the end;
- volume input changes volume and shows the WPS-style temporary volume overlay
  while paused;
- `Play/Pause` pauses on current frame and resumes without jumping;
- `Right` seeks forward by the configured step;
- `Left` seeks backward by the configured step;
- seeking while paused updates the progress position;
- `Menu` exits back to the browser;
- no black screen after overlay hide.

### Device: iPod 6G

Use a synced RockPod `.rvp` from the Videos menu:

- fresh boot -> video plays with audio;
- Database music -> video plays with audio;
- Files music -> video plays with audio;
- while video plays, enable hold for at least 30 seconds:
  - video continues;
  - audio continues;
  - no segment skip;
  - no exit;
  - no pause;
- disable hold and press wheel/play/menu:
  - wheel changes volume and shows overlay;
  - play pauses/resumes;
  - left/right rewind and fast-forward;
  - menu exits;
- exit halfway through the video, reopen it from Videos, and verify playback
  resumes from the saved position;
- after video exit, Database and Files music both start with sound.

### Device: Long/Multi-Segment Video

Use a TV episode or movie with multiple RVP segments:

- progress bar duration is total episode/movie duration, not current segment;
- segment transition does not reset progress;
- segment transition does not flash or skip because of overlay drawing;
- `Menu` exits from any segment instead of advancing to the next segment;
- fast-forward from segment 1 into segment 2 keeps audio and video in sync;
- rewind from segment 2 into segment 1 keeps audio and video in sync;
- resume into segment 2 opens the correct part and starts with sound;
- no audio desync after pausing near a segment boundary.

### Regression

Repeat the plugin audio lifecycle matrix from
`docs/plugin-audio-lifecycle-steering.md` after the controls change. This is
required because volume, pause, hold, and menu handling all touch the same
timing loop that owns audio callback lifetime.

## Non-Goals For First Pass

- Subtitles.
- File browser overlay.
- Chapter markers.
- Reusing the full WPS skin parser.
- Multi-record resume browser.
- Resume prompt/settings UI.
