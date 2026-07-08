# Rockbox MPEG Player FPS Improvement Spec

Scope: improve Rockbox MPEG Player playback smoothness and achievable FPS,
with iPod Video / 5G as the first target and simulator profiling as the main
development loop.

RockPod is in scope for host-side video conversion profiles because encoded
shape, bitrate, GOP, and frame rate have a direct impact on Rockbox playback.

## Research Conclusion

There are credible FPS improvements worth pursuing, but the right first step is
measurement, not a blind decoder rewrite.

The strongest opportunities are:

- add persistent MPEG Player profiling so simulator runs can compare clips,
  display modes, overlays, and encode profiles without relying on the flashing
  on-screen FPS overlay;
- audit and fix the `FILL` display path for undersized videos, where the code
  can scale into a temporary YUV buffer every frame;
- add measured RockPod performance profiles for higher-FPS playback, with
  native/padded 320x240 remaining the safe default;
- reduce iPodTikTok/feed overlay work during playback when the user wants max
  FPS;
- only investigate libmpeg2 or ARM assembly changes after profiling proves the
  hot function and the win is large enough to justify the risk.

The preferred user outcome is "more FPS if possible." The practical target is:

- stable 24 fps with no skips for normal converted videos;
- a measured 30 fps profile only if benchmark data shows enough headroom;
- fewer skipped frames and lower frame time variance when the source cannot
  sustain realtime.

## Online Research

Primary sources checked:

- Rockbox manual MPEG Player section:
  <https://raw.githubusercontent.com/Rockbox/rockbox/master/manual/plugins/mpegplayer.tex>
- libmpeg2 project homepage:
  <https://libmpeg2.sourceforge.io/>
- Rockbox source tree mirrors for MPEG Player files:
  <https://github.com/Rockbox/rockbox/tree/master/apps/plugins/mpegplayer>

Relevant findings:

- The Rockbox manual documents MPEG Player as a plugin for MPEG-1/MPEG-2 video
  with MPEG audio in `.mpg` files.
- `Display FPS` shows decoded FPS and skipped frames, but it draws on screen and
  is not ideal for repeatable profiling.
- `Limit FPS` off displays video as fast as possible and is intended for
  benchmarking.
- `Skip frames` tries to maintain realtime playback by skipping display of
  frames. The manual notes these frames are still decoded, though current code
  has a more nuanced decoder-skip path for B frames and severe lateness.
- libmpeg2 upstream emphasizes speed and warns that real-player display paths
  can cost as much as decode. That matches this codebase: iPod Video has a
  target-specific YUV blit path, while MPEG Player can add scaling and overlay
  work above it.

## Current Code Findings

### MPEG Player architecture

- `apps/plugins/mpegplayer/mpegplayer.c` owns plugin setup, UI, OSD, FPS
  display, and iPodTikTok/feed overlay behavior.
- The comment in `mpegplayer.c` describes separate responsibilities:
  - main thread for UI and input;
  - stream manager;
  - buffer thread;
  - video thread, on COP for PortalPlayer targets;
  - audio thread on the main CPU;
  - audio clock as the reference timing source.
- `apps/plugins/mpegplayer/video_thread.c` owns decode timing, render/drop
  decisions, and counters.
- `apps/plugins/mpegplayer/video_out_rockbox.c` owns YUV blitting, native
  cropping, and CPU scaling for thumbnails and fill display mode.
- `firmware/target/arm/ipod/video/lcd-video.c` provides the iPod Video
  `lcd_blit_yuv()` implementation, with ARM assembly helper
  `lcd_write_yuv420_lines()` in `lcd-as-video.S`.

### Existing FPS behavior

- MPEG Player already tracks `video_num_drawn` and `video_num_skipped`.
- `settings.limitfps == 0` disables waiting and dropping, so it is the existing
  "run as fast as possible" benchmark mode.
- `settings.skipframes == 1` permits skip behavior to preserve A/V sync.
- Severe lateness can call `mpeg2_skip()` so some B frames are not decoded, and
  deeper skips can wait for an I frame.
- The current stats do not split decode, blit, scale, overlay, wait, parser, or
  buffer-stall time. That makes optimization decisions too speculative.

### Display output and scaling

- Normal drawing reaches `vo_draw_frame()` and then either:
  - calls `vo_draw_frame_scaled()` for `MPEG_VIDEO_DISPLAY_FILL` on undersized
    video;
  - falls back to `yuv_blit()` with source crop and output rectangle.
- `vo_draw_frame_scaled()` stretches Y, U, and V planes into a temporary buffer
  with `stretch_image_plane()`, then calls `lcd_blit_yuv()`.
- `stretch_image_plane()` is nearest-neighbor CPU scaling. It is simple and
  likely expensive compared with a direct blit.
- `vo_setup()` computes a fill-scale rectangle, then clamps `scaled_w` and
  `scaled_h` to the available source dimensions after the display-mode switch.
  For undersized content this can leave the final output rectangle at source
  size while `vo_draw_frame_scaled()` still does per-frame full-screen scaling.
  This should be profiled and either fixed to truly render the scaled output or
  bypassed when it has no visual effect.

### Target YUV path

- The iPod Video LCD driver has a direct YUV blit entry point:
  `lcd_blit_yuv()`.
- The helper writes two YUV420 lines at a time through
  `lcd_write_yuv420_lines()` assembly.
- This suggests the first FPS work should avoid extra work above the target
  blitter before attempting to rewrite the target blitter itself.

### libmpeg2 version

- `apps/plugins/mpegplayer/libmpeg2/README.rockbox` says the bundled decoder is
  based on `mpeg2dec-0.4.0b`, imported in 2006.
- Upstream libmpeg2 later released 0.5.x with additional fixes and some ARM
  work. A wholesale import is high risk because Rockbox has local memory,
  platform, and assembly integration. Treat it as a research branch only after
  instrumentation identifies decode as the main limiter.

### RockPod conversion profile

`rockpod/services/android_media.py` currently uses:

- 320x240 padded output;
- 20 fps;
- MPEG-2 video;
- YUV420p;
- no B frames (`-bf 0`);
- GOP 12;
- low-delay flag;
- qscale 8;
- maxrate 900k;
- buffer size 512k;
- MP2 audio at 44.1 kHz stereo.

This is conservative and likely stable. It should remain the default until
benchmarks prove a higher-FPS profile is safe.

The existing simdisk also contains older MPEG clips with 24 fps, 29.97 fps,
352x240, 320x240, 220x176, and B frames. That is useful for regression coverage
but not enough for controlled FPS decisions.

## Goals

- Increase measured MPEG Player FPS where possible.
- Reduce skipped frames in realtime playback.
- Build a repeatable simulator benchmark gate for MPEG Player.
- Identify safe RockPod conversion profiles for 24 fps and possible 30 fps.
- Preserve audio sync, seeking, resume, start menu behavior, and plugin
  compatibility.
- Keep a clear fallback path if a change improves simulator FPS but regresses
  hardware playback.

## Non-Goals

- Do not add new video codecs.
- Do not promise 30 fps on all content.
- Do not replace MPEG Player with a different media player.
- Do not make 30 fps the RockPod default unless both simulator and hardware
  evidence support it.
- Do not import a new libmpeg2 release as the first implementation step.

## Proposed Work

### 1. Add persistent MPEG Player profiling

Add an optional MPEG Player profile mode that writes a log under:

`/.rockbox/mpegplayer/profile.log`

The log should be append-only by default and one line per run. It should not
draw an overlay.

Required fields:

- `clip`
- `display_mode`
- `limitfps`
- `skipframes`
- `width`
- `height`
- `nominal_fps`
- `duration_ticks`
- `drawn_frames`
- `skipped_frames`
- `avg_drawn_fps_x100`
- `max_unlimited_fps_x100`
- `decode_ticks`
- `parse_ticks`
- `wait_ticks`
- `blit_ticks`
- `scale_ticks`
- `osd_ticks`
- `buffer_wait_ticks`
- `scaled_frames`
- `native_blit_frames`
- `decoder_skip_frames`
- `frame_type_i`
- `frame_type_p`
- `frame_type_b`

Implementation notes:

- Use `*rb->current_tick` for low-overhead timing first.
- Keep measurement blocks coarse enough to avoid changing playback behavior.
- Gate the feature behind a setting, debug option, or simulator-only build flag
  until the overhead is understood.
- Do not use the existing visible FPS overlay as the benchmark source because
  it draws into playback and can perturb results.

### 2. Add a MPEG Player simulator profile gate

Create `tools/mpegplayer_profile_gate.py`, modeled on
`tools/rockboy_profile_gate.py`.

Responsibilities:

- copy an isolated simdisk from `build-sim-video-5g/simdisk`;
- copy the freshly built `mpegplayer.rock` into the isolated simdisk;
- write `config.cfg` and `plugin.dat` direct-start entries for MPEG Player;
- write per-run `mpegplayer.cfg` settings:
  - display mode: fit, fill, native;
  - limit FPS: on/off;
  - skip frames: on/off;
  - profile log: on;
- launch instructions for `./build-sim-video-5g/rockboxui`;
- validate `/.rockbox/mpegplayer/profile.log` after a run;
- emit a tabular summary suitable for comparing variants.

The first gate can still require the user to close the simulator after each
run. Full automation can follow if the simulator exposes a reliable exit path.

### 3. Build a controlled benchmark corpus

Generate test clips from a common source so only one variable changes per run.

Recommended matrix:

- resolution:
  - 320x240 padded;
  - 240x180 padded or native;
  - 220x176 native;
- FPS:
  - 20;
  - 24;
  - 30;
- codec:
  - MPEG-2 baseline profile matching current RockPod;
  - MPEG-1 comparison only if Rockbox compatibility is confirmed;
- B frames:
  - `-bf 0`;
  - existing B-frame clips as compatibility/regression cases;
- bitrate:
  - 500k;
  - 700k;
  - 900k;
  - 1150k;
- display mode:
  - native;
  - fit;
  - fill.

Each clip should be named with its parameters, for example:

`fpsbench_320x240_24fps_mpeg2_bf0_900k.mpg`

### 4. Fix or remove wasteful fill scaling

Add counters around `vo_draw_frame_scaled()` first:

- `scaled_frames`
- `scale_ticks`
- `scale_buffer_failures`
- `scale_output_w`
- `scale_output_h`

Then validate whether the final output rectangle is actually full-screen for
undersized clips in `FILL` mode.

Potential fixes:

- make `FILL` truly output the scaled dimensions and crop from the temporary
  full-screen buffer;
- or bypass per-frame scaling when `vo_setup()` has clamped the output rectangle
  back to source size;
- or expose a performance mode that prefers direct native blit for undersized
  content.

Acceptance:

- If the user selected `FILL`, the visual result must either actually fill the
  screen or avoid paying for invisible scaling.
- On undersized clips, `scale_ticks` must drop materially when a no-scale path
  is selected.
- Native 320x240 clips must not regress.

### 5. Add RockPod performance profiles

Keep the current profile as the safe default. Add opt-in profiles only after the
benchmark gate provides data.

Candidate profiles:

- `ipod_video_safe`: current 320x240, 20 fps, 900k, no B frames.
- `ipod_video_24fps`: 320x240, 24 fps, no B frames, bitrate selected from test
  results.
- `ipod_video_30fps`: only exposed if 320x240 or lower-res tests show enough
  decode headroom.
- `ipod_video_high_fps_small`: 220x176 or 240x180, 24/30 fps, no B frames,
  intended to reduce macroblock decode work.

Important tradeoff:

- Lower resolution can increase decode headroom, but if Rockbox scales it every
  frame the gain can disappear. This is why display-mode measurement and fill
  scaling fixes must come before promoting lower-res profiles.

### 6. Reduce optional overlay cost

Profile iPodTikTok/feed playback separately from plain MPEG playback.

Potential changes:

- avoid installing a post-frame callback unless an overlay animation is active;
- throttle decorative overlay refresh work;
- add a "Max FPS playback" or "Lean video playback" setting that disables
  nonessential overlay updates;
- ensure FPS benchmark runs can disable feed overlays completely.

Acceptance:

- Plain MPEG Player and iPodTikTok/feed runs have separate profile lines.
- Overlay cost is visible in `osd_ticks` or a feed-specific counter.
- Turning on max-FPS mode reduces overlay ticks without breaking likes, resume,
  or normal controls.

### 7. Investigate decoder hot paths only after profiling

If profile logs show decode dominates:

- add counters around libmpeg2 parse/decode phases;
- identify whether IDCT, motion compensation, bitstream parsing, or memory copy
  dominates;
- inspect existing ARM motion compensation assembly coverage in
  `libmpeg2/motion_comp_arm*.c` and `.S`;
- try small, target-specific changes in isolation;
- treat a libmpeg2 0.5.x import as a separate high-risk branch.

Decoder changes must prove a clear gain on simulator and hardware before
becoming part of the main path.

## Simulator Test Plan

Run each candidate clip in at least these modes:

- `limitfps=off`, `skipframes=off`: maximum decode/render throughput.
- `limitfps=on`, `skipframes=on`: normal realtime playback.
- `limitfps=on`, `skipframes=off`: A/V sync stress and smoothness comparison.

For display mode:

- native for native-size and smaller clips;
- fit for normal user behavior;
- fill for scaling/regression coverage.

Report:

- average unlimited FPS;
- skipped frames over a fixed playback interval;
- scale ticks per drawn frame;
- blit ticks per drawn frame;
- decode ticks per drawn frame;
- buffer wait ticks;
- OSD/overlay ticks.

Benchmark rules:

- use isolated simdisks;
- delete or archive old profile logs before each batch;
- run each variant at least three times;
- compare medians;
- keep the simulator window size and host load stable;
- do not compare visible FPS overlay runs with profile-log runs.

## Hardware Validation

Simulator results are necessary but not sufficient. The iPod Video target has a
different CPU, cache, memory bus, LCD controller, and YUV assembly path.

Hardware validation should use:

- the same benchmark corpus copied to the device;
- profile logging enabled on hardware if overhead is acceptable;
- visible playback checks for tearing, black frames, wrong crop, bad aspect
  ratio, and audio drift;
- battery/thermal sanity checks for any profile promoted as default.

Promote a 30 fps profile only if:

- unlimited FPS shows at least 10 percent headroom above 30 fps on representative
  clips;
- realtime playback produces no sustained skipped-frame growth;
- audio stays synchronized;
- controls and seeking remain responsive.

## Risks

- **Simulator mismatch:** x86 simulator timing does not represent iPod Video CPU
  or LCD hardware. Use it to find relative wins and regressions, then validate
  on device.
- **Measurement overhead:** too many timers in the video thread can reduce FPS.
  Keep profiling coarse and optional.
- **A/V sync regressions:** increasing displayed FPS or disabling skipping can
  make audio drift obvious. Audio clock behavior must remain the authority.
- **Display mode regressions:** changing fill/native behavior can alter crop,
  aspect ratio, or centering. Add screenshot and visual checks for every display
  mode.
- **Quality/file-size tradeoff:** no B frames and lower bitrates can improve
  decode behavior but may reduce quality or increase artifacts.
- **Lower-res scaling trap:** smaller videos decode faster only if Rockbox does
  not spend the savings scaling every frame.
- **Decoder update risk:** libmpeg2 is embedded and adapted for Rockbox. A new
  upstream import can break memory assumptions, assembly hooks, or target
  portability.
- **Overlay pollution:** FPS overlay and feed overlays can change the thing
  being measured. Benchmarks need a no-overlay mode.

## Acceptance Criteria

The project is successful if one of these is true:

- unlimited FPS improves by at least 15 percent on at least two representative
  clips without visual regressions;
- realtime skipped frames drop by at least 50 percent on a clip that currently
  skips;
- RockPod gains a measured 24 fps profile that stays realtime on iPod Video;
- RockPod gains a measured 30 fps profile for at least one defined class of
  clips, such as lower-res high-FPS videos.

The project should not ship a code optimization if it only improves one
simulator-only metric and fails hardware validation.

## Implementation Order

1. Add MPEG Player profile logging.
2. Add `tools/mpegplayer_profile_gate.py`.
3. Generate the controlled corpus.
4. Run baseline simulator batches for current RockPod profile and existing
   simdisk clips.
5. Add fill-scaling counters and fix/bypass wasteful scaling if confirmed.
6. Add RockPod opt-in performance profiles and tests.
7. Profile overlay cost and add lean playback mode if it moves FPS.
8. Reassess decoder hot paths only if the data still points there.

## Open Questions

- Does the iPod Video hardware sustain 30 fps for any 320x240 MPEG-2 profile
  with acceptable quality?
- Is 220x176 or 240x180 visually acceptable to the user if it enables higher
  FPS?
- Should lower-res performance clips default to native display mode to avoid
  scaling, or should Rockbox provide a fast scaled mode?
- How much overhead does profile logging add on actual hardware?
- Are current iPodTikTok clips generated by the current RockPod profile, or are
  they older clips with B frames that should be regenerated for fair testing?
