# iPod Classic 6G/7G Apple-exact H.264 video specification

Status: Apple desktop contract qualified; native firmware builds; physical
iPod decode and composite qualification pending
Date: 2026-08-31
Target: `ipod6g` (S5L8702; iPod Classic 6G/7G)
RockPod profile: `h264_apple_exact`
Contract: `itunes-9.2.1-quicktime-7.6.6-ipod-exact-v1`

## Outcome

RockPod now has one deliberately named Apple-exact conversion option. It does
not expose speculative “improved”, “TV quality”, or “Apple safe” H.264 modes.
Old experimental H.264 profile names normalize to `h264_apple_exact` so an old
configuration cannot silently keep a different recipe.

The profile reproduces the decoder-visible output of iTunes 9.2.1.5 and
QuickTime 7.6.6's **Advanced > Create iPod or iPhone Version** operation:

- non-fragmented `.m4v`, H.264 Constrained Baseline and optional AAC-LC;
- Apple's two resolution/rate-control buckets;
- source aspect ratio retained, no upscale, even display dimensions up to
  640x480, and SPS cropping where the coded macroblock frame is larger;
- source CFR retained at no more than 30 fps, including 23.976, 25, 29.97,
  and 30 fps, with higher rates deterministically decimated;
- Apple's SPS, PPS, VUI, HRD, SEI, NAL priority, two-slice, track-order,
  handler-name, timescale, file-brand, and top-level atom contracts; and
- Apple-like average rate: about 700 kbit/s for the small bucket and
  1.5 Mbit/s for the SD bucket, plus roughly 38-42 kbit/s AAC-LC.

This reduces normal video storage from raw RVP's approximately 8.9 GB/hour to
roughly 0.35 GB/hour for the small bucket or 0.70 GB/hour for the SD bucket.
The patched x264 encoder is normally equal or better visually than the old
QuickTime encoder at those measured rates; compatibility, not byte identity,
is the qualification target.

The iTunes Wine UI later became unresponsive. That is not a provenance or
qualification blocker: all six authentic outputs had already completed, were
copied out of the prefix, hashed, parsed, and placed behind the strict gate.

## Hardware addresses

The Reddit addresses are useful S5L8720/iPod touch 2G clues, but they are not
the iPod Classic S5L8702 map. The implementation uses the address evidence
already exercised by this repository's `rockpod/h264` history.

| Function | Reddit/S5L8720 lead | S5L8702 address used here |
| --- | ---: | ---: |
| LCD YUV compositor/scaler | `0x39000000` | `0x38900000` |
| H.264 decoder | `0x38f00000` | `0x39800000` VPU-B |
| ADM/CalmRISC host interface | not separated | `0x39000000` |
| Composite video processor | `0x39100000` | `0x39100000` |
| Composite mixer/router | `0x39200000` | `0x39200000` |
| Analog SDO encoder | `0x39300000` | `0x39300000` |

Never probe `0x39000000` as the Classic's scaler: this tree identifies it as
`ADM_SFR_BASE`. VPU-B register access remains target-owned in
`firmware/target/arm/s5l8702/ipod6g/vpu-6g.c`.

## Apple research provenance

No Apple binary is committed or used as a build dependency. The binaries were
research inputs used to identify and measure a public file-format contract.

| Artifact | Version | Hash |
| --- | --- | --- |
| Official 64-bit installer | iTunes 9.2.1.5 | SHA-1 `461d9cb0053d74f8b8d1804be3d4c50176a6036d` |
| Official 32-bit installer | iTunes 9.2.1.5 | SHA-1 `fd86e82bc52dd5a22d922aedf2a6063c224ca48c` |
| `QuickTimeH264.qtx` | QuickTime 7.6.6 (1673) | SHA-256 `29b2483de82be654440c88d99dfbb7f81b03a6216546ee583c45f8e650fb7222` |
| `QuickTimeAuthoring.qtx` | QuickTime 7.6.6 (1673) | SHA-256 `ad6aa2bb35d1ed895e9e046182d10893fa9b2b561b4a01c11ea188bbe9fff5be` |

Static analysis established the division of work:

- iTunes exposes the iPod conversion action and device capability records,
  including `h264SDVideoCodecInfo`, `H.264LC`, average/peak rate, buffer,
  profile, level, and complexity fields.
- `QuickTimeH264.qtx` contains the encoder and the `-iPodSP`, `-iPodMP`,
  `-iPodSP2`, and `-iPodMP2` preset families, buffered rate control, CPB
  enforcement, and low-complexity-decode logic.
- `QuickTimeAuthoring.qtx` supplies exporter and MP4 authoring settings.

The final recipe is based on authentic black-box outputs, not on guessed
numeric option names or copied proprietary implementation code.

## Qualified golden corpus

`tools/apple_video_golden.py` builds deterministic 4-second MJPEG/PCM MOV
sources. The authentic outputs were made with the documented iTunes action.
The manifest is accepted only when the installer/component hashes, operator
attestation, all source/output hashes, and every strict syntax check pass.

| Case | Apple output SHA-256 | Apple bytes | Apple video rate |
| --- | --- | ---: | ---: |
| 320x240 30 silent flat | `5e3eacbc692bac505cca219d6f872e1b6a6e1fe56fdb64aa4deb53da30740caa` | 6,594 | 6,310 bit/s |
| 320x240 29.97 stereo motion | `91afbb413518e1e5672306b1ad3c26d7f0becdd469fbbd8575f5e6c7084ebb4e` | 373,896 | 700,724 bit/s |
| 640x360 23.976 mono bars | `2bd21ab648d682cb44f1e8fe80839a112bda47d3a3317ea0a58ab607db211610` | 34,869 | 21,843 bit/s |
| 640x480 30 stereo motion | `dfed8b4a097618b3d1fb0139703de1e427c4b65c2ee88c4272568f4f88c9c73b` | 777,487 | 1,502,504 bit/s |
| 240x320 25 stereo motion | `40ab7f260bf1b961493bb501912641b65ce6b5db35f04aaba9cc8651258538f7` | 372,027 | 692,002 bit/s |
| 640x480 60 -> 30 stereo motion | `c47d6930f590f1c26e2ceaddd1c56844606d797f347154d7c81d495a518a2994` | 778,590 | 1,504,690 bit/s |

The strict RockPod candidate comparison is qualified with no failures:

| Case | RockPod bytes | RockPod video rate | RockPod/Apple video rate |
| --- | ---: | ---: | ---: |
| flat small | 4,619 | 6,554 bit/s | 1.039 |
| motion small | 382,944 | 719,157 bit/s | 1.026 |
| widescreen bars | 34,209 | 18,694 bit/s | 0.856 |
| motion SD | 764,041 | 1,476,044 bit/s | 0.982 |
| portrait small | 389,046 | 727,690 bit/s | 1.052 |
| 60 -> 30 SD | 762,629 | 1,473,220 bit/s | 0.979 |

Flat content is expected to collapse far below the nominal ABR. The gate
therefore compares the exact syntax contract for every case and permits only a
narrow content-dependent rate band; it does not demand needless padding.

## Exact observable contract

### Shared H.264 syntax

| Field | Required value |
| --- | --- |
| Profile | `profile_idc=66`, constraint byte `0xe0` (Constrained Baseline) |
| Chroma/depth | progressive 8-bit 4:2:0, CAVLC, no B pictures |
| Frame number | `log2_max_frame_num_minus4=1` |
| POC | type 0; `log2_max_pic_order_cnt_lsb_minus4=3` |
| Slices | exactly two ordered slices, split at half the picture macroblocks |
| Parameter NAL priority | SPS, PPS, and reference slices use `nal_ref_idc=1` |
| PPS | IDs 0, bottom-field POC-present 1, active refs minus1 = 0/0, weighted prediction off, initial QP minus26 = 2, chroma offset 0 |
| AVC sample layout | `avc1`/`avcC`; four-byte NAL lengths |

The PPS bottom-field POC flag is present even though the pictures are
progressive. Slice parsing must consume `delta_pic_order_cnt_bottom`; rejecting
or ignoring it desynchronizes the rest of the Apple slice header.

### VUI shared fields

| Field | Required value |
| --- | --- |
| Aspect ratio | present, `aspect_ratio_idc=0` (unspecified) |
| MP4 pixel aspect | no square-pixel `pasp` atom |
| Video signal | present, format 5, TV range |
| Colour | primaries 6, transfer 1, matrix 6 |
| Chroma location | present, top 2, bottom 2 |
| Timing info | absent; timing is in MP4 sample tables |
| HRD | NAL HRD present, VCL HRD absent, low-delay false |
| Other | overscan absent, pic-struct absent, bitstream restriction absent |

### Apple size buckets

| Property | Small bucket (up to 76,800 pixels) | SD bucket |
| --- | ---: | ---: |
| Level | 1.3 | 3.0 |
| Nominal RockPod ABR | 768 kbit/s | 1,500 kbit/s |
| SPS max reference frames | 2 | 1 |
| P-picture NAL priority | all reference (`1`) | alternating disposable/reference (`0`,`1`) |
| Deblocking | normal; PPS control absent | PPS control present; slice value 1 (disabled) |
| HRD bitrate scale/value-minus1 | 7 / 93 | 7 / 488 |
| HRD CPB scale/value-minus1 | 10 / 121 | 10 / 244 |
| Buffering-period SEI | `06000781f63b8000004080` | `0600078493e0000003004080` |
| Per-picture user-data SEI | absent | `0605110387f44ecd0a4bdca1943ac3d49b171f0180` |
| IDR NAL order | SEI, slice, slice | SEI, user SEI, slice, slice |
| P NAL order | slice, slice | user SEI, slice, slice |

Both HRD branches have one CPB entry, `cbr_flag=0`, 24-bit initial, CPB, and
DPB delay fields, and a 24-bit time offset.

### Audio and MP4 authoring

- AAC-LC, 44,100 Hz, stereo. A mono input is converted to stereo.
- The measured sine corpus lands at 37.6-41.7 kbit/s. RockPod uses FFmpeg AAC
  quality `0.28`, which lands at about 42 kbit/s for the same material.
- Audio is track 0 and video is track 1 when audio exists.
- Handlers are `Apple Sound Media Handler` and `Apple Video Media Handler`.
- Video media timescale is 15,360 at 30 fps, 12,800 at 25 fps, 30,000 at
  29.97 fps, and 24,000 at 23.976 fps. Audio timescale is 44,100.
- `ftyp`: major brand `M4V `, minor version 1, compatible brands `M4V `,
  `M4A `, `mp42`, `isom`.
- Top-level atom order is exactly `ftyp`, `moov`, `free`, `free`, `mdat`.

## Encoder implementation

The host encoder is a pinned, independently authored patch over x264 commit
`c24e06c2e184345ceb33eb20a15d1024d9fd3497`. The source archive SHA-256 is
`090d730e867fc63631782a1287974635d1237d0fa7c6fd1d09fd543620a56689`.

Run:

```sh
tools/build_x264_apple_ipod.sh
```

The script downloads that exact archive, checks the hash, applies
`tools/patches/x264-apple-ipod-exact.patch`, builds it, and verifies the marker
`RockPod Apple-iPod exact bitstream patch v5`. The local binary lives at
`rockpod/.tools/x264-apple-ipod` and is intentionally ignored by git.

RockPod invokes x264 directly because the stock FFmpeg/libx264 interface cannot
produce several of Apple's unusual observable fields. Important effective
arguments include:

```text
--profile baseline --threads 24-or-32 --bframes 0 --weightp 0 --ref 1
--slices 2 --nal-hrd vbr --preset slow --range tv
--videoformat undef --colorprim smpte170m --transfer bt709
--colormatrix smpte170m --chromaloc 2
```

Level, ABR, VBV, deblocking, geometry, frame selection, GOP, and timescale are
selected from the measured bucket and source rate. x264 emits an intermediate
MP4. FFmpeg then encodes AAC and muxes audio first without re-encoding video.
The final authoring pass rewrites the brands/atom layout and removes the
square-pixel `pasp` atom.

Every completed temp file is parsed by the same strict contract code before an
atomic cache rename. A zero exit code is insufficient. The cache key includes
the Apple contract, patch marker, dimensions, rational frame rate, effective
rates, audio quality, source fingerprint, and device key.

The v4 patch suppresses the duplicate first-frame buffering-period SEI that
stock x264 inserts only when frame threading is active. This lets adaptive
frame threading preserve the same measured Apple NAL sequence and exact
two-slice macroblock split as the single-thread path. RockPod uses 32 threads
for sources at or below 640x480, and 24 while downscaling larger sources so the
decoder/scaler keeps enough host CPU. On the 20-logical-core reference host,
the 1920x1080 qualification clip measured 110.65 fps at 24 threads versus
81.99 fps at eight (35% more throughput; 0.04 dB PSNR delta). A native 640x360
Emiru qualification clip measured 166.53 fps at 32 threads versus 135.92 fps
at eight (23% more throughput; 0.014 dB PSNR delta). Both outputs passed the
strict Apple bitstream validator. Slice threading is not used because it moves
the SD split to a row boundary, which is observably different from Apple's
midpoint split.

The v5 patch also removes x264's rounding bias when a picture contains an odd
number of macroblocks. Slice two therefore starts at `floor(total_mbs / 2)`,
the same midpoint rule enforced by the Apple-contract validator; even-grid
outputs remain byte-for-byte unchanged by this correction.

## RockPod UI and storage

The Video Sync panel exposes `Sync Apple Exact`, with MPEG and RVP retained as
fallbacks. Device Settings exposes `H.264 Apple Exact (5G-7G hardware)` and
maps all obsolete H.264 profile names to the exact profile. The sync summary
shows an H.264-versus-RVP estimate before conversion; the actual cached size
replaces the estimate after conversion.

Approximate payload per hour:

| Format | Size/hour |
| --- | ---: |
| Apple-exact small, 768 kbit/s + about 42 kbit/s AAC | about 0.36 GB |
| Apple-exact SD, 1.5 Mbit/s + about 42 kbit/s AAC | about 0.69 GB |
| Existing MPEG-2, 1.6 Mbit/s + 112 kbit/s MP2 | about 0.77 GB |
| RVP 320x240x20 YUV420 + 44.1-kHz stereo PCM | about 8.9 GB |
| RVP 160x120x20 YUV420 + 22.05-kHz stereo PCM | about 2.4 GB |

These are planning values. ABR output depends on content; the golden flat and
bars cases demonstrate why the encoder should not pad simple material to the
nominal rate.

## Native playback implementation

The current tree contains:

- `apps/mp4_demux.c`: bounded non-fragmented MP4/M4V video/audio sample table
  parser;
- `apps/vpu_h264.c`: checked SPS/PPS/slice parsing, Apple two-slice assembly,
  SPS cropping, POC-bottom consumption, and a bounded multi-frame DPB;
- `firmware/target/arm/s5l8702/ipod6g/vpu-6g.c`: VPU-B ownership, IRQ,
  register sequence, DMA/cache, reset, and teardown;
- `apps/video_audio.c` and `apps/video_pcm.c`: AAC codec loading and audio
  master clock using the normal Rockbox playback buffer/mixer lifecycle;
- `apps/video_playback.c`: MP4/AAC/H.264 scheduling, pause, volume, LCD YUV
  presentation, and composite mirror entry; and
- `apps/plugins/openh264_player.c`: the plugin front door for
  `.m4v`/`.mp4`/`.mov` plus legacy RVP.

The native validator accepts Apple's Level 1.3 SPS advertising two reference
frames and the SD SPS advertising one. P slices still use one active list-0
reference, exactly as the PPS signals. Exactly two ordered slices are required;
unsupported CABAC, B pictures, interlace, FMO/ASO, weighted prediction,
reference-list modification, long-term references, malformed NAL lengths, and
out-of-bounds tables fail before MMIO.

The hardware path uses the plugin-owned audio buffer. It does not call
`audio_stop()` or `audio_hard_stop()` before ownership transfer, and it
quiesces AAC/PCM/VPU users before returning the buffer.

## App coverage

The Videos browser and file viewer recognize `.mp4`, `.m4v`, and `.mov` on
native iPod 6G. Shared extension routing is used by Netflix, YouTube, Reddit,
OnlyFans, Instagram, Spotify Wrapped, Maps, and Offline Web, so those direct
launches reach the hardware backend while retaining their launch prefix.

iPodTikTok and Live TV are not falsely marked complete. Their feed/guide,
overlays, next/previous logic, picture-in-guide, weather, and commercial state
currently run inside `mpegplayer`, not in a codec-neutral session layer. Their
RockPod services must continue emitting MPEG until that session code owns an
H.264 backend. Changing only `.mpg` to `.m4v` would play one file but destroy
the app behavior. “All apps” is complete only when this last migration and its
mixed-codec tests pass.

Required migration boundary:

```text
app launch/context
        |
codec-neutral session: input, overlays, resume, next/previous, return
        |
        +-- MPEG backend
        +-- Apple-exact H.264 backend
        +-- RVP backend
        |
LCD/composite presentation and shared audio clock
```

This is the remaining implementation gap, separate from the now-qualified
Apple encoder.

## Composite output

Decoded YUV420 currently reaches the normal LCD YUV path, whose existing
iPod 6G video-out hook mirrors it when composite output is active:

```text
VPU-B 0x398... -> YUV420 frame
                         |
             LCD compositor 0x389...
                         |
             TV processor 0x391...
                  mixer 0x392...
                     SDO 0x393...
```

Build success does not qualify analog output. Physical acceptance requires
correct luma/chroma and aspect ratio for small, SD, widescreen, and portrait
files; cable removal/reinsert; pause and volume; music/video transitions;
and a long run without field roll, stale frames, tearing, drift, VPU timeout,
or DPB overwrite.

## Verification and release gates

### Host gates

1. Build and hash-check the pinned x264 tool.
2. Generate or reuse the six deterministic sources.
3. Analyze authenticated Apple outputs with operator attestation.
4. Produce candidates through `VideoRvpTranscoder`, not a test-only recipe.
5. Require strict comparison `qualified: true` with no failures.
6. Run focused RockPod command, validator, UI, cache, and size tests.
7. Build the complete iPod 6G firmware and retain MPEG/RVP fallbacks.

### Physical iPod gates

| Gate | Required result |
| --- | --- |
| H0 | Passive confirmation of VPU/clock/IRQ identity; no writes to the Reddit addresses |
| H1 | One Apple small-bucket IDR; Y/Cb/Cr CRC equals FFmpeg |
| H2 | Small and SD two-slice I/P sequences; per-frame pixel equality |
| H3 | AAC-LC with correct 44.1-kHz playback, pause, volume, and clean exit |
| H4 | 30-minute small and SD files; no timeouts, memory growth, or audio stall |
| H5 | Videos plus every direct-launch app; correct return and audio restoration |
| H6 | iPodTikTok/Live TV after their codec-neutral session migration |
| H7 | Composite matrix and two-hour drift/temperature/drop test |

The reserved final content test is
`https://www.youtube.com/watch?v=U2zCCFNT6Vo`. Its download is permitted only
after the strict Apple corpus and the current firmware build pass. Creating a
local `.m4v` test artifact does not authorize a physical iPod deploy or sync.

Do not advertise physical hardware/composite qualification until those gates
have actually run on the device.

## Deployment safety

For a physical firmware deploy, copy the built `rockbox.ipod` to both the
volume root and `.rockbox/rockbox.ipod`, verify both checksums against the local
build, then `sync`/eject. For a full iPod 6G package deploy, use
`tools/deploy_ipod6g_preserve_database.sh`; never replace `.rockbox` wholesale
or lose `database*.tcd`/`tagcache*.tcd`.

## Source trail

- Apple iPod classic 160GB (Late 2009) technical specifications:
  https://support.apple.com/en-euro/112601
- Apple iTunes 9.2.1 for Windows and published installer checksum:
  https://support.apple.com/en-gb/106406
- x264 pinned-source patch and build:
  `tools/patches/x264-apple-ipod-exact.patch`,
  `tools/build_x264_apple_ipod.sh`
- Golden-corpus generator/analyzer/comparator:
  `tools/apple_video_golden.py`
- RockPod encoder and strict parser:
  `rockpod/services/video_rvp.py`,
  `rockpod/services/apple_video_exact.py`
- Native implementation:
  `apps/mp4_demux.c`, `apps/vpu_h264.c`, `apps/video_audio.c`,
  `apps/video_pcm.c`, `apps/video_playback.c`,
  `firmware/target/arm/s5l8702/ipod6g/vpu-6g.c`
- Related-device prior art, not an S5L8702 register specification:
  https://github.com/devos50/qemu-ios
- Mandatory local lifecycle rules:
  `docs/plugin-audio-lifecycle-steering.md`,
  `docs/ipodjs-ui-memory-animation-steering.md`
