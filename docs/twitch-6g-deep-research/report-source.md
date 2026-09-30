# Twitch on iPod 6G: playback, audio, sync, and composite failure analysis

**Date:** 5 September 2026  
**Target:** RockPod / Rockbox `ipod6g` only  
**Reserved validation VOD:** `https://www.twitch.tv/videos/2860855364`

## Executive finding

The Emma H.264 files did not lose their audio during download or host
conversion. The synced files contain AAC-LC, 44,100 Hz, two-channel audio, and
Rockbox's own AAC codec produces non-silent PCM from them. Two separate device
defects made that good media appear broken:

1. Rockbox's AAC chunk helper stored valid unsigned 32-bit MP4 `stco`
   positions, but returned them through a signed 32-bit `int`. In files larger
   than 2 GiB, a later audio position could become negative. Sequential decode
   or a live-mode seek then stopped following interleaved audio chunks.
2. Composite output converted every update directly into the single planar
   buffer currently being scanned by the TV processor. The display could mix
   old and new rows, producing the reported horizontal/glitch lines.

The implemented correction keeps H.264 as Twitch's default. Twitch audio is
decoded and re-encoded to AAC-LC 44.1 kHz stereo during H.264 preparation; it
is not copied from the Twitch source. The AAC path now carries MP4 positions
through a signed 64-bit return value, preserving the complete unsigned 32-bit
range supported by the player. An MPEG file is cached beside each H.264 only
as a last-resort device fallback. Sync errors are isolated per creator and per
VOD, so one failed download or conversion cannot discard successful items.

## Evidence from the supplied VOD

The complete local source for VOD 2860855364 is 8,109.383 seconds long and
1,389,143,197 bytes. It contains 852×480 H.264 Main video at 30 fps and AAC-LC
48 kHz stereo. RockPod's prepared M4V is 8,109.355 seconds long and contains
Apple-contract H.264 plus AAC-LC 44.1 kHz stereo. The host source is silent for
approximately its first 190.87 seconds, then contains normal programme audio;
that opening silence is in the Twitch media and is retained because the user
requested the full stream.

The cleaned MPEG recovery copy is also complete: 8,109.375 seconds,
552,396,800 bytes, 320×240 MPEG-2 at 30 fps, and MP2 44.1 kHz stereo. Its
SHA-256 is
`6bf68aa20792aecb988352b55765fb98c48e3b617e0f631b0e028be8d61bcb8a`.
The embedded MMA chat strip was removed from this test VOD's recovery copy;
the native slide-in chat remains a separate device sidecar/UI.

The current local library has six VODs under the two Emma creator aliases
(`emma` and `emmablackerytv`). Four already had H.264 copies on the device;
two older entries had only MPEG copies from prior partial runs. The H.264 sync
policy now applies to all of them. Unrelated `sweet_anita` rows were not used
as evidence for the Emma-specific media inventory, although the decoder fix
also applies to those files.

## Failure gap matrix

| User-visible failure | Reproduced evidence | Root cause | Implemented control | Remaining physical gate |
| --- | --- | --- | --- | --- |
| Audio works on the PC but disappears later/on live join | Four synced M4Vs exceed 2 GiB; late AAC packet positions include 2,271,061,838 and 2,302,661,400 | `uint32_t` chunk offset returned as signed `int` | `m4a_check_sample_offset()` and its AAC caller now use `int64_t` | Listen across a late seek/live join on 6G |
| Some files appear silent at the beginning | VOD 2860855364 is source-silent through about 190.87 s; VOD 2855234786 through about 362.63 s | Silence is present in Twitch source, not introduced by iPod conversion | Preserve full stream; tests sample a known non-silent interval | User should scrub past source-silent intro when checking |
| H.264 playback fails and gives no useful recovery | Native H.264/VPU qualification remains incomplete; old player discarded audio errors | H.264 or AAC error was not surfaced and no alternate was launched | Explicit audio init/decode/early-end errors; sibling MPEG opens only after H.264 failure | Confirm the physical VPU path and fallback handoff |
| One bad item aborts a long Twitch sync | Previous refresh/sync exception scope covered the whole operation | Monolithic failure boundary | Per-creator/per-VOD isolation; old media retained on partial refresh; staged files replaced atomically | Disconnect/failure injection during a real sync |
| Retry repeats expensive MPEG work | MPEG conversion wrote only to a temporary device staging path | No durable host cache | Content-signature MPEG cache with atomic completion | Cache-hit timing on second physical sync |
| Composite menu/video has horizontal lines | One active 115,200-byte planar YUV buffer was rewritten during scanout | Read/write race with TV processor | Two buffers inside existing static SVID memory, full cache publication, bounded field-edge pointer handoff | 30-second moving menu and chat test on composite |
| Chat covers video or baked chat remains | Native rail and embedded video chat were independent | Source pixels cannot be hidden by UI reflow | Full-height 146 px slide-in rail reflows video; test VOD recovery has source chat removed | Observe animation and source crop on composite |

## Media and decoder acceptance evidence

The four device files larger than 2 GiB were tested at non-silent positions
whose AAC packets are beyond signed 32-bit range:

| VOD | File size | Test start | First observed late AAC position | Result |
| --- | ---: | ---: | ---: | --- |
| 2857240220 | 2,283,328,465 B | 11,800 s | 2,271,061,838 | 10 s non-silent PCM pass |
| 2858925101 | 2,179,532,988 B | 11,100 s | 2,169,250,615 | 10 s non-silent PCM pass |
| 2862600186 | 2,171,177,441 B | 11,100 s | 2,159,998,910 | 10 s non-silent PCM pass |
| 2864411857 | 2,335,023,638 B | 12,000 s | 2,302,661,400 | 30 s and 10 s non-silent PCM passes |

These simulator gates execute the same MP4 parser, `libm4a` chunk lookup,
dynamic Rockbox AAC codec, video PCM ring, and playback mixer service used by
the device. They do not execute the S5L8702 H.264 VPU or analog video-out
hardware. A successful simulator audio gate therefore qualifies the host
decoder path but does not replace the physical 6G acceptance matrix.

Focused service and policy tests pass: 38 passed and 1 skipped. The iPod 6G
hardware build links successfully, its static composite gate passes, and the
complete package contains the rebuilt AAC codec, Twitch plugin, H.264 player,
and firmware.

## Implemented behavior

- Twitch requests the best complete video+audio rendition at or below 480p.
  A download without both streams is rejected rather than silently accepted.
- H.264 is the first conversion and first playback choice.
- H.264 muxing maps the source audio, re-encodes it as AAC-LC 44.1 kHz stereo,
  writes audio as track zero, and copies only the Apple-contract H.264 video
  bitstream into the final M4V.
- A source with audio cannot pass staging if the output lacks an audio stream.
- Each successful H.264 gets an MPEG recovery sibling; failure of that backup
  is reported but does not invalidate the primary.
- If H.264 preparation fails, the same VOD can use MPEG as its primary without
  stopping later VODs.
- If both formats fail, an existing playable copy is preserved and later VODs
  continue.
- Device launch prefers `.m4v`, then `.mp4`, `.mov`, and finally `.mpg`.
- The H.264 player opens `.mpg` only after a reported VPU/AAC failure.
- Composite presentation swaps complete YUV frames at a bounded field edge;
  it allocates no plugin, codec, audio, or playback memory.

## Physical 6G acceptance checklist

1. Reboot into the newly deployed firmware and confirm Applications → Twitch
   opens without a plugin/file-open error.
2. Play VOD 2860855364 from the start; its first approximately 191 seconds are
   genuinely silent. Seek beyond 600 seconds and confirm audible programme
   audio, volume control, pause/resume, and clean Menu exit.
3. Play an Emma file larger than 2 GiB, seek beyond the test position in the
   table, and listen for at least 60 seconds.
4. Enter an Emma live channel whose wall-clock position is late in the file;
   confirm audio begins and remains the master clock.
5. Toggle chat with Select; confirm the full-height rail slides smoothly and
   continuously reduces video width instead of covering it.
6. With composite enabled, scroll menus and toggle chat for at least 30
   seconds; reject any mixed rows, horizontal tearing, stale frames, or field
   roll.
7. Force one H.264 failure and confirm only its same-ID MPEG sibling opens.
8. Test fresh boot → Twitch, Database music → Twitch, Files music → Twitch,
   and Twitch → both music paths without mute, freeze, or retained callbacks.

## Authoritative constraints and source trail

Apple's late-2009 iPod classic specification permits H.264 Baseline/Low
Complexity video up to 640×480 at 30 fps and AAC-LC stereo audio. See
[Apple: iPod classic 160 GB (Late 2009) technical specifications](https://support.apple.com/en-ie/112601).

FFmpeg documents that a trailing `?` makes a mapped stream optional. That is
why `-map 0:a:0?` can legally produce a silent output when no input audio is
present; RockPod now separately requires output audio whenever the Twitch
source had it. See [FFmpeg documentation: stream selection and `-map`](https://ffmpeg.org/ffmpeg.html).

The Linux DRM/KMS documentation describes page flips in relation to vertical
blanking and the requirement to synchronize display updates for tear-free
presentation. The iPod target does not use Linux DRM, but this is authoritative
corroboration for the same scanout rule applied by the bounded field-edge YUV
handoff. See [Linux kernel DRM/KMS documentation](https://docs.kernel.org/gpu/drm-kms.html).

Repository evidence and implementation:

- `docs/plugin-audio-lifecycle-steering.md`
- `docs/ipodjs-ui-memory-animation-steering.md`
- `docs/ipod6g-hardware-h264-video-spec.md`
- `docs/ipod6g-dcp750-videoout-results.md`
- `lib/rbcodec/codecs/aac.c`
- `lib/rbcodec/codecs/libm4a/m4a.c`
- `apps/video_audio.c`, `apps/video_pcm.c`, `apps/video_playback.c`
- `apps/plugins/openh264_player.c`, `apps/plugins/twitch.c`
- `firmware/target/arm/s5l8702/ipod6g/videoout-6g.c`
- `rockpod/services/twitch_app.py`
- `rockpod/services/app_video_sync.py`
- `rockpod/services/video_rvp.py`
- `tools/h264_audio_sim_gate.py`

## Conclusion

The missing-audio report is explained by a deterministic large-file integer
boundary plus genuine silent source intros—not by absent audio tracks. The
software correction covers every compatible AAC/M4A file below Rockbox's 4 GiB
limit, retains H.264 as the primary Twitch format, and leaves MPEG as a
last-resort recovery path. Host and simulator evidence passes. Physical VPU,
composite, and full lifecycle qualification must be judged on the deployed 6G;
it is not inferred from a successful build.
