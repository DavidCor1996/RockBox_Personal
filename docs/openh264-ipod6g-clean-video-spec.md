# OpenH264 Clean Video Player Spec for iPod 6G+

Scope: evaluate and, only if performance proves out, port an OpenH264-based
video player for Rockbox on iPod Classic 6G/7G (`ipod6g`). The goal is cleaner
video than the current MPEG Player without audio glitches, frame pacing
problems, or UI regressions.

This is a performance-gated project. The first deliverable is a decoder and
playback benchmark, not a full replacement player.

## Decision

The best open-source candidate for better visual quality is OpenH264:

- Source: https://github.com/cisco/openh264
- License: BSD-2-Clause
- Codec: H.264 / MPEG-4 AVC
- Relevant profile: Constrained Baseline
- Output: YUV 4:2:0 planar, compatible with Rockbox's existing YUV LCD path
- Fit: cleaner than MPEG-2 at the same bitrate, actively maintained, smaller
  and cleaner than FFmpeg for a focused port

Use `minimp4` only after raw H.264 Annex-B decode performance is proven:

- Source: https://github.com/lieff/minimp4
- Purpose: tiny MP4 demux path for `.mp4` / `.m4v`

Do not start with FFmpeg/libavcodec. It has better format coverage, but it is
too large for this target and adds avoidable licensing, build, memory, and
integration complexity.

## Existing Local Context

The repo already contains:

- `apps/plugins/mpegplayer/`: current MPEG-1/MPEG-2 player using bundled
  `libmpeg2`
- `apps/plugins/h264_poc.c`: S5L8702 hardware decoder register probe
- `tools/import_ipodtiktok_videos.sh`: existing `ffmpeg` MPEG conversion
  workflow
- `docs/rockbox-mpegplayer-fps-improvement-spec.md`: MPEG player benchmark and
  FPS improvement plan

The hardware H.264 PoC is not the first implementation path. It does not decode
actual video yet and touches undocumented SoC blocks. The first OpenH264 work is
software decode. Hardware decode becomes a later acceleration path only if the
software path proves the UX is worth pursuing.

## User Outcome

Required outcome:

- no audible stalls, pops, underruns, or drift during playback
- no visible slowdowns at the accepted profile
- no steady frame dropping in normal playback
- cleaner visual quality than the MPEG-2 baseline at equal or lower bitrate
- playback exits cleanly and leaves Rockbox audio state stable

If the accepted profile cannot meet these requirements, the project must stop
or fall back to improving MPEG Player.

## Format Strategy

Phase 1 accepts raw Annex-B H.264 elementary streams:

- extension: `.h264`
- video only
- no audio
- used for pure decode, blit, and frame pacing benchmarks

Phase 2 accepts a simple paired test format:

- `clip.h264`
- `clip.wav` or `clip.pcm`
- plugin plays video and uncompressed/downsampled audio from separate files
- purpose: prove A/V scheduling without MP4 demux complexity

Phase 3 accepts MP4:

- extension: `.mp4` / `.m4v`
- video: H.264 Constrained Baseline, YUV420p
- audio: AAC-LC only if Rockbox codec integration is practical; otherwise MP3
  or PCM sidecar remains the first supported audio path
- demux: `minimp4` or a smaller local MP4 reader if only one constrained layout
  is needed

## Encoding Profiles

All profiles must scale and pad to the iPod screen instead of scaling every
frame on device.

Screen target:

- 320x240
- YUV420p
- square pixels
- fit mode: contain inside 320x240 with black padding
- landscape/wide clips must use the full width or height without cropping
- portrait clips must remain upright and pillarboxed; do not rotate, stretch, or
  crop them just to fill the screen
- no B-frames
- low reference count
- short GOP for seeking and recovery

RockPod RVP sync profile:

- extension: `.rvp`
- sidecars: same-basename `.yuv` and `.pcm`
- video: raw YUV420p, 320x240, 20 fps
- audio: raw signed 16-bit little-endian stereo PCM, 44.1 kHz
- ffmpeg filter:
  `scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=20`
- `.rvp` marker must include `fit=contain`
- long clips whose raw `.yuv` sidecar would exceed 2 GB must be segmented as
  `basename.segNN.yuv` / `basename.segNN.pcm` sidecars behind one visible
  `basename.rvp` marker; the Videos tab must show the single marker only and
  playback must advance through the segments in order

### Baseline Realtime Profile

This is the first pass/fail profile.

```sh
ffmpeg -y -i "$src" \
  -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=20" \
  -an \
  -c:v libx264 -profile:v baseline -level:v 1.3 -pix_fmt yuv420p \
  -preset veryslow -tune animation \
  -x264-params "ref=1:bframes=0:cabac=0:weightp=0:8x8dct=0:subme=6:me=hex:keyint=40:min-keyint=40:scenecut=0" \
  -b:v 350k -maxrate 450k -bufsize 900k \
  -f h264 "$out.h264"
```

Expected result: no frame drops and headroom above realtime.

### Quality Candidate Profile

Use only after the baseline profile passes on hardware.

```sh
ffmpeg -y -i "$src" \
  -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=24" \
  -an \
  -c:v libx264 -profile:v baseline -level:v 1.3 -pix_fmt yuv420p \
  -preset veryslow \
  -x264-params "ref=1:bframes=0:cabac=0:weightp=0:8x8dct=0:subme=6:me=hex:keyint=48:min-keyint=48:scenecut=0" \
  -b:v 450k -maxrate 600k -bufsize 1200k \
  -f h264 "$out.h264"
```

Expected result: visually cleaner than 20 fps MPEG-2 with no audio or video
slowdowns. If this drops frames, keep the 20 fps profile as the accepted target.

### Stress Profile

This is for measuring limits, not for release.

```sh
ffmpeg -y -i "$src" \
  -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=30" \
  -an \
  -c:v libx264 -profile:v baseline -level:v 1.3 -pix_fmt yuv420p \
  -preset veryslow \
  -x264-params "ref=1:bframes=0:cabac=0:weightp=0:8x8dct=0:keyint=60:min-keyint=60:scenecut=0" \
  -b:v 500k -maxrate 700k -bufsize 1400k \
  -f h264 "$out.h264"
```

Expected result: benchmark data only. Do not use 30 fps as a goal unless it has
large hardware headroom.

## Local Test Inputs

Use local files already found on the laptop:

1. Quick iteration:
   `docs/ipone-original-full-art-slideshow/slideshow-glide.mp4`
   - H.264, 640x480, 30 fps, short duration
   - useful for fast decode/blit smoke tests

2. Phone vertical stress:
   `/home/david/Downloads/snaptik_7240088802050641198_v3.mp4`
   - HEVC, 1080x1920, 24 fps, AAC audio
   - useful for conversion pipeline and portrait padding

3. Long-form H.264:
   `/home/david/Videos/TV Shows/6teen_2004_complete_series_202508/6teen S02E27 Girlie Boys.mp4`
   - H.264, 852x480, about 29.97 fps, AAC audio
   - useful for thermal, battery, audio drift, and long playback tests

4. Current MPEG baseline:
   `/home/david/Videos/YouTube/The Mean Kitty Song.mpg`
   - MPEG-2 video, 320x240, 20 fps, MP2 audio
   - useful for direct quality and smoothness comparison against current
     `mpegplayer`

Generated test outputs should live under:

`test-videos/openh264-ipod6g/`

This directory should not be committed unless the clips are explicitly cleared
for redistribution.

## Plugin Architecture

Add a new experimental viewer plugin:

- source directory: `apps/plugins/openh264_player/`
- rock name: `openh264_player.rock`
- initial viewer extension: `.h264`
- later viewer extensions: `.mp4`, `.m4v`
- category: viewers

Do not modify `mpegplayer` in the first milestone except for shared benchmark
helpers if absolutely necessary.

Core modules:

- `openh264_player.c`: plugin entry, input, UI, lifecycle
- `h264_decode.c`: OpenH264 wrapper and frame decode loop
- `h264_stream.c`: Annex-B NAL reader and later MP4 sample reader
- `h264_video_out.c`: YUV420 frame blit and optional fit/fill/native modes
- `h264_audio.c`: phase-2 audio clock and PCM output
- `h264_profile.c`: stats logging

Use Rockbox APIs for all memory, file I/O, input, ticks, LCD, and PCM output.
No libc file I/O, heap allocation, threads, or platform calls should leak in
from OpenH264.

## Build Integration

Vendor OpenH264 only after the benchmark target is approved:

- keep the imported subtree isolated under `apps/plugins/openh264_player/openh264/`
- strip encoder, tests, platform demos, and unused assembly from the Rockbox
  build
- start with C/C++ fallback decode
- only enable ARM assembly after a clean C path builds and runs

OpenH264 must be compiled with:

- no exceptions
- no RTTI if C++ is involved
- no pthreads
- no dynamic allocation outside Rockbox-controlled wrappers
- no stdout/stderr dependencies

If C++ integration becomes invasive, stop and evaluate extracting only the
decoder C interface and required C++ objects behind a narrow wrapper.

## Benchmark Milestones

### M0: Host Conversion

Create a script:

`tools/openh264_prepare_ipod6g_samples.sh`

Inputs:

- optional source path
- output directory, default `test-videos/openh264-ipod6g`

Outputs:

- `quick_20.h264`
- `quick_24.h264`
- `phone_20.h264`
- `phone_24.h264`
- `long_20.h264`
- `long_24.h264`
- a text manifest from `ffprobe`

Acceptance:

- all generated files are 320x240 H.264 Constrained Baseline Annex-B
- all generated files are YUV420p
- no generated video contains B-frames or CABAC

### M1: Decode-Only Benchmark

Implement `.h264` decode without LCD drawing.

Log path:

`/.rockbox/openh264_profile.log`

Required fields:

- `clip`
- `width`
- `height`
- `fps_num`
- `fps_den`
- `frames_decoded`
- `decode_ticks`
- `avg_decode_fps_x100`
- `max_frame_decode_ticks`
- `decode_errors`
- `profile`
- `level`
- `refs`

Acceptance on hardware:

- 20 fps profile decodes at least 1.50x realtime
- 24 fps profile decodes at least 1.25x realtime before any audio work
- zero decode errors on the quick and phone clips
- long clip can decode at least 5 minutes without memory growth or crash

If 20 fps cannot decode at 1.50x realtime, stop. The OpenH264 path is not good
enough for no-slowdown playback.

### M2: Decode + YUV Blit Benchmark

Draw every decoded frame using `lcd_blit_yuv()`.

Acceptance on hardware:

- 20 fps profile sustains realtime with zero dropped frames
- 24 fps profile sustains realtime with zero dropped frames or remains marked
  experimental
- max frame time stays under the frame budget for 99% of frames:
  - 20 fps: 50 ms
  - 24 fps: 41.67 ms
- no per-frame scaling on device
- screen updates are stable in native 320x240 output

### M3: Audio Clock and No-Underrun Playback

Add audio only after M2 passes.

First audio path may be PCM/WAV sidecar to remove demux and codec variables.
The audio clock must drive playback. Video may wait or drop only during stress
tests; accepted profiles must not need drops.

Required log fields:

- `audio_underruns`
- `pcm_low_watermark_hits`
- `av_drift_ms_max`
- `video_late_frames`
- `video_dropped_frames`
- `audio_ticks`
- `video_ticks`

Acceptance on hardware:

- `audio_underruns=0`
- `pcm_low_watermark_hits=0`
- `video_dropped_frames=0` for accepted profile
- max A/V drift no worse than +/- 40 ms after 10 minutes
- playback remains responsive to pause, resume, seek test, and exit

If audio underruns occur, lower the video profile before adding complexity.
Clean audio is non-negotiable.

### M4: MP4 Demux

Only after sidecar audio/video passes:

- integrate `minimp4` or a constrained local demuxer
- support one video track and one audio track
- reject unsupported profiles loudly before playback
- build an index for seeking if memory allows

Acceptance:

- same no-underrun/no-drop criteria as M3
- unsupported files fail with a clear message
- supported files play without conversion to sidecar files

## Comparison Against MPEG Player

For every candidate profile, create an MPEG-2 comparison with the existing
pipeline:

```sh
ffmpeg -y -i "$src" \
  -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=20" \
  -c:v mpeg2video -pix_fmt yuv420p -q:v 6 -maxrate 900k -bufsize 512k \
  -c:a mp2 -ar 44100 -ac 2 -b:a 128k \
  "$out.mpg"
```

OpenH264 is worth continuing only if one of these is true:

- it is visibly cleaner than MPEG-2 at equal bitrate with no playback slowdown;
- it matches MPEG-2 quality at substantially lower bitrate with no playback
  slowdown;
- it unlocks a better user workflow with MP4 files while preserving realtime
  playback.

## Test Matrix

Run each accepted candidate through:

- simulator smoke test on `build-sim-ipod6g`
- real iPod 6G+ cold boot playback
- 10 minute long-form playback
- repeat pause/resume 20 times
- seek forward/backward 20 times after seek support exists
- low battery playback check if practical
- exit back to Rockbox and play an audio file immediately

Pass criteria:

- no crashes
- no audio glitches
- no frozen video
- no stuck backlight/LCD state
- no leaked settings or broken audio state after exit
- accepted profile has zero dropped frames during normal playback

## Risks

- Software H.264 decode may be too slow on S5L8702 without hardware assistance.
- OpenH264's optimized ARM paths target newer ARM/NEON-class devices; iPod 6G
  may rely mostly on slower C fallback.
- MP4/AAC support may be more work than video decode.
- H.264 patents and distribution rules may matter depending on how binaries are
  shipped. Keep this under review before publishing builds.
- The hardware decoder PoC may eventually outperform software decode, but it is
  not yet a safe foundation for user playback.

## Stop Conditions

Stop the OpenH264 player path if any of these are true:

- 20 fps 320x240 decode cannot reach 1.50x realtime without drawing
- decode plus blit cannot sustain 20 fps with zero drops
- audio playback underruns under the 20 fps profile
- memory use forces unsafe compromises in Rockbox plugin buffers
- integrating OpenH264 requires broad build-system or runtime changes outside
  the plugin boundary

If stopped, redirect work to:

- `docs/rockbox-mpegplayer-fps-improvement-spec.md`
- better MPEG-2 conversion presets
- MPEG Player UI, seeking, and profiling improvements

## First Implementation Order

1. Add `tools/openh264_prepare_ipod6g_samples.sh`.
2. Build sample `.h264` files from the local quick, phone, and long clips.
3. Import the minimum OpenH264 decoder source into an experimental branch.
4. Build a decode-only `.h264` benchmark plugin.
5. Run on simulator only for smoke, then hardware for real performance.
6. Add YUV blit only after decode speed passes.
7. Add audio only after decode plus blit passes.
8. Add MP4 demux only after sidecar A/V playback is clean.

## Implemented Bridge: RVP Raw Playback

The installed working path is `.rvp`, routed to `openh264_player.rock`.

Files:

- `clip.rvp`: small text marker/control file shown in Videos.
- `clip.yuv`: raw planar YUV420, 320x240, 20 fps.
- `clip.pcm`: signed 16-bit little-endian stereo PCM, 44100 Hz.

Playback behavior:

- `openh264_player` derives `.yuv` and `.pcm` from the selected `.rvp` basename.
- audio is loaded into the plugin audio buffer and played through Rockbox PCM.
- video frames are streamed from disk one frame at a time and drawn with
  `lcd_blit_yuv`.
- per-run logs append to `/.rockbox/openh264/openh264_profile.log`.

Installed sample files:

- `quick_raw.rvp`: short silent slideshow smoke test.
- `phone_raw.rvp`: phone video with audio.

Acceptance checks for this bridge:

- selecting `quick_raw.rvp` from Videos opens full-screen video and exits cleanly
  on any button press.
- selecting `phone_raw.rvp` plays video and audio together.
- `late_frames=0` or near-zero in `/.rockbox/openh264/openh264_profile.log`.
- after exit, normal Rockbox audio playback still works.

This bridge is intentionally storage-heavy. It is a clean playback baseline and
a device test harness while the OpenH264 decoder runtime shim is completed.
