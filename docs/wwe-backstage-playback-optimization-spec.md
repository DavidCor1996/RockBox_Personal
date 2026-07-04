# WWE Backstage Playback Optimization Spec

## Goal

Improve perceived smoothness for the WWE Backstage interactive video package
without weakening the plugin audio lifecycle, playlist isolation, or RVP package
compatibility.

The current player is a copied RVP playback path inside
`apps/plugins/wwe_backstage.c`. It plays raw YUV420 frames plus signed 16-bit
stereo PCM, uses `PCM_MIXER_CHAN_PLAYBACK`, and logs `mode=raw_rvp` profile
records. Optimization work should preserve that behavior first, then measure.

## Constraints

- Follow `docs/plugin-audio-lifecycle-steering.md` for every audio-adjacent
  change.
- Keep long-form clip audio on `PCM_MIXER_CHAN_PLAYBACK`.
- Do not call `audio_stop()` before `plugin_get_audio_buffer()`.
- Do not release the shared audio buffer while mixer callbacks can still read
  plugin memory.
- Do not bypass mixer/PCM wake paths on iPod 6G/CS42L55.
- Do not mutate the user's playlist.
- Keep generated RVP marker metadata internally consistent with sidecar data.

## Baseline To Capture First

Before changing playback behavior, capture at least one full WWE route on
device and preserve:

```text
/.rockbox/rocks/games/wwe_backstage/wwe_backstage.log
```

Minimum fields to compare:

- clip path
- width, height, fps, sample rate, channels
- frames
- late frames
- play ticks
- error
- audio state records around prepare/start/shutdown

If no WWE log exists, use the existing `mode=raw_rvp` log shape as the baseline
format and add only narrow extra timing fields.

## Safe Track

These changes are intended to be low-risk and independently reversible.

### S1: Replace PCM Sample Loop With `memcpy`

Current behavior:

- `pcm_more()` copies a chunk from `pcm_cursor` into `raw_mixbuf`.
- The copy is byte-for-byte; no scaling, conversion, or endian adjustment is
  performed.

Change:

- Replace the `int16_t` loop with `rb->memcpy(raw_mixbuf, pcm_cursor, chunk)`.
- Keep the existing 4-byte alignment mask on `chunk`.
- Keep `raw_mixbuf` as the returned buffer unless a separate direct-pointer
  change is tested later.

Expected benefit:

- Removes avoidable callback CPU work.
- No package or timing format changes.

Acceptance:

- WWE clips still play with audio.
- Volume changes still use the normal Rockbox path.
- Database music -> WWE -> Database music still works without reboot.
- `mode=raw_audio_state` shutdown records show stopped PCM/mixer state.

### S2: Add Narrow Frame Timing Instrumentation

Current behavior:

- `late_frames` is logged, but it does not identify whether lateness came from
  storage reads, scaling, blitting, or scheduling.

Change:

- Add optional counters in `play_raw_segment()`:
  - total frame read ticks
  - worst frame read ticks
  - total scale ticks
  - worst scale ticks
  - total blit ticks
  - worst blit ticks
  - skipped frames from audio-clock catch-up
- Append those fields to `mode=raw_rvp`.

Rules:

- Do not log per frame.
- Do not add file I/O inside the frame loop.
- Keep all counters local to the segment and write once at segment end.

Expected benefit:

- Distinguishes storage stalls from CPU/display cost.
- Makes medium-risk changes easier to justify.

Acceptance:

- Log remains line-oriented.
- Existing parsers that ignore unknown fields continue working.
- Added timing overhead is negligible compared with a 50 ms frame budget at
  20 fps.

### S3: Add Package FPS Presets To The WWE Builder

Current behavior:

- `tools/wwe_backstage_package.py` hardcodes `FPS = 20`.

Change:

- Add `--fps` with default `20`.
- Validate a conservative range such as `10..20`.
- Use the selected FPS in:
  - ffmpeg video filter
  - RVP marker
  - silence fallback PCM sizing
  - segment splitting metadata

Recommended presets:

- `20`: current quality target.
- `18`: first smoothness test.
- `15`: fallback for weak storage or heavy late-frame counts.

Expected benefit:

- Reduces YUV reads and LCD blits linearly.
- Does not alter runtime audio ownership.

Acceptance:

- Generated `.rvp` markers match the selected FPS.
- `.yuv` frame counts and `.pcm` duration remain aligned.
- WWE route plays without progressive audio/video drift.

### S4: Enforce No Runtime Scaling For WWE Packages

Current behavior:

- Current generated WWE assets are 320x240, so runtime scaling should not run.
- The runtime still has a generic scaling path for non-native dimensions.

Change:

- Keep the builder default at 320x240.
- Add a package validation check that rejects or warns on WWE source output
  dimensions other than 320x240.

Expected benefit:

- Prevents accidental packages from hitting `scale_yuv420_nearest()` every
  frame.

Acceptance:

- WWE package markers all report `width=320` and `height=240`.
- Runtime logs show no dimension mismatch.

## Medium Track

These changes can improve smoothness more, but they alter buffering or playback
flow enough to require hardware testing.

### M1: Add YUV Frame Read-Ahead

Current behavior:

- Playback reads one full YUV420 frame at a time.
- The next frame is loaded at the end of the current frame loop.
- A single slow storage read can push playback late and force audio-clock frame
  skipping.

Change:

- Allocate a small ring of frame buffers from the shared audio/plugin pool after
  PCM and optional scaled-frame storage.
- Start with 2 frames; allow 3 or 4 only if memory permits.
- Fill available ring slots while waiting for the next target frame.
- Consume one ready frame per presentation.
- If the ring under-runs, fall back to the existing synchronous read path.

Rules:

- Do not allocate from moving memory.
- Do not add another thread in the first version.
- Do not prefetch past segment end.
- Keep all reads on the same video fd.
- Keep audio as the master clock.

Expected benefit:

- Smooths short iFlash/storage stalls.
- Reduces frame skips caused by isolated read latency spikes.

Risks:

- Less memory available for double-buffered PCM.
- More complex seek behavior when skipping late frames.
- Possible cache effects from multiple frame buffers.

Acceptance:

- If memory is insufficient, player falls back to the current single-frame path.
- Seeking forward after late-frame catch-up invalidates queued stale frames.
- Late-frame count improves or stays neutral on the same WWE route.
- No new audio lifecycle differences appear in logs.

### M2: Tune Segment Length For WWE Clips

Current behavior:

- The WWE builder only splits very large YUV sidecars.
- Current WWE clips are mostly single segments.

Change:

- Add a `--segment-seconds` option to `tools/wwe_backstage_package.py`.
- Pass it through to `tools/rvp_split_device_video.py`.
- Test candidate values: `120`, `90`, `60`.

Expected benefit:

- Smaller PCM chunks can reduce preload time and memory pressure.
- Enables existing next-segment PCM prefetch more often.

Risks:

- Segment boundaries stop/restart audio.
- Too many segments can increase transition overhead or audible gaps.
- Incorrect PCM bytes-per-frame math can create sync drift.

Acceptance:

- Multi-segment clips remain audio/video synced across boundaries.
- No audible gap regression versus current single-segment clips.
- Logs show segment totals match full clip frame counts.

### M3: Prefetch Next Node During Choice Screen

Current behavior:

- After a clip ends, WWE presents choices.
- The next node starts loading only after the user selects it.

Change:

- For a single-choice node, preload that target's first audio chunk or first
  segment metadata during the choice screen.
- For multi-choice nodes, optionally prefetch only the highlighted target after
  the selection has been stable for a short delay.

Rules:

- Do not start PCM for the next node until selected.
- Do not keep callbacks pointing at old node memory.
- Cancel and discard prefetched state when selection changes.
- Keep this separate from in-clip playback smoothness metrics.

Expected benefit:

- Faster branch transitions.
- Less dead time between clips.

Risks:

- Shared buffer lifetime becomes more complex between nodes.
- Multi-choice prefetch may waste I/O and memory.

Acceptance:

- MENU exit from the choice screen releases all prefetched state.
- Restart/go-back paths do not reuse stale target buffers.
- Database and Files playback still resume after exiting WWE.

### M4: Factor Shared RVP Player After Behavior Stabilizes

Current behavior:

- `wwe_backstage.c` duplicates the RVP playback implementation from
  `openh264_player.c`.

Change:

- Move common RVP parsing, raw audio lifecycle, segment playback, OSD, and
  profiling primitives into a shared plugin library.
- Keep WWE-specific manifest, node, and choice code in `wwe_backstage.c`.
- Keep OpenH264 scan/profiling behavior in `openh264_player.c`.

Expected benefit:

- Future smoothness fixes apply to both `.rvp` viewer playback and WWE.
- Reduces drift between two full-screen video paths.

Risks:

- Larger refactor blast radius.
- Plugin build/link changes can break unrelated viewers.

Acceptance:

- Selecting a normal `.rvp` still opens in `openh264_player`.
- Launching `.twv` still opens WWE.
- Audio lifecycle logs are unchanged or improved.
- iPod hardware still plays a known raw RVP sample.

## Recommended Order

1. Capture baseline WWE logs.
2. Apply S1.
3. Apply S2.
4. Generate/test 20, 18, and 15 fps packages with S3.
5. Only if logs still show read-related lateness, prototype M1.
6. Use M2 for memory/preload pressure or long clips, not as the first fix.
7. Use M3 for branch transition polish.
8. Do M4 once the playback behavior is stable enough to share.

## Hardware Test Matrix

Run on target hardware when possible:

- fresh boot -> WWE with sound
- Database music -> WWE with sound
- Files music -> WWE with sound
- WWE -> Database music starts with sound
- WWE -> Files music starts with sound
- rapid WWE/menu/music switching
- pause/resume during a clip
- volume changes during a clip
- MENU exit during clip playback
- MENU exit from choice screen
- route with at least five node transitions
- route containing the longest generated clip

