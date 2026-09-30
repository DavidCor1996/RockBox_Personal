# MPEG native TV presentation and Live TV storage

## MPEG integration

The current tree's restored TV implementation already routes MPEG decoded
planes through `tv_video_prepare()` before LCD thumbnail/overlay rendering.
This work qualifies that path for higher-resolution MPEG rather than adding
a second competing presenter. Both H.264 and MPEG use the shared native TV
surface, aspect/fit settings, captions and status presentation.

The MPEG integration now checks planar 4:2:0, non-null planes, even visible
dimensions and visible-versus-coded bounds before submission. The same check
protects the guide's video thumbnail. Display aspect arithmetic uses 64-bit
products so MPEG pixel-aspect metadata cannot overflow before normalization.
Decoded planes are copied synchronously; LCD updates cannot overwrite the
owned TV picture. Hide, cleanup, guide, PIN and weather transitions retain
their existing release behavior. No PCM, mixer, playlist,
clock or plugin API changes were made in this follow-up.

An interactive 640x480 MPEG test exposed a separate decoder allocation
failure: the old 2 MiB libmpeg2 pool spent approximately 1.2 MiB on its
compressed chunk/state, then ran out while allocating three decoded pictures.
The first I-frame subsequently wrote through a null chroma pointer. Classic
composite builds now reserve 4 MiB for libmpeg2, an additional 2 MiB taken
from the already plugin-owned arena before the disk read buffer is assigned.
PCM buffers and callbacks retain their existing sizes and lifecycle. Other
targets retain the 2 MiB reservation. This is decoder workspace, not an
extra TV canvas or a decorative allocation from active music playback.

`test_tv_mpeg_native.py` runs the actual target renderer with ASan/UBSan and
checks every output sample for 640x480, widescreen 640x360, 480x360 and
anamorphic 720x480 sources, including padded strides. Native 640x480 on a
4:3 canvas preserves its source samples exactly. The lifetime test also runs
the actual MPEG frame entry, verifies original dimensions/stride at submission,
rejects unsupported layouts and exercises both players through pause, hidden
controls and guide handoffs. The separate YUV ownership test verifies LCD
thumbnails cannot overwrite the TV canvas.

`test_mpeg_tv_workspace.py` reproduces the old allocation failure and verifies
that the 4 MiB reservation holds the decoder state, three VGA frames and the
LCD thumbnail while preserving the separate PCM partition. All five focused
workspace, renderer, ownership, lifetime and guide-handoff tests passed.

Clean iPod 6G firmware/plugin/codec and simulator builds passed. A real
640x480 MPEG clip decoded in the simulator, passed two pause/resume cycles
and exited through Menu. This smoke test does not establish held-seek or
volume-overlay behavior; the older seek gate has expectations that differ
from the current controls.

The final `build-tv-mpeg-native/rockbox.zip` passed its archive CRC check;
its firmware and MPEG plugin match the built files. It uses the isolated
`/.rbtv` runtime and has not been installed on the iPod. SHA-256:

* Firmware: `394da7191d88b1c1ed622312c4d75f993f21bbe41c367ca58463b3f222a528c3`
* MPEG plugin: `94e5285eea06074937b00b83ae98cce5336a19a2579b712e651e10cb1164c9e2`

Real-time software MPEG decoding at larger sizes is not established by these
tests. Higher-resolution MPEG can require substantially more CPU than the
current 320x240 files. Hardware playback, frame drops and audio sync must be
qualified before a library-wide conversion. No device deployment or library
conversion is part of this change.

## Measured collection

The mounted `Videos/LiveTV` collection contains 496 MPEG files, all identified
as 320x240 from their MPEG sequence headers. Their combined size is
114,703,400,959 bytes (114.7 decimal GB). Free device space at measurement is
82,702,139,392 bytes (82.7 GB).

The existing sync profile is `mpeg2-320x240-fill-v2`: MPEG-2, 20 fps, no B
frames, GOP 12, quality 2, maximum video bitrate 1600 kbit/s, 800 kbit VBV,
and 112 kbit/s stereo MP2 audio. Its filter center-crops to 4:3. A larger
conversion made from an already synced file cannot restore lost detail.

Read-only manifest lookup and source probing found:

| Source availability | Synced files | Current size |
| --- | ---: | ---: |
| Higher-resolution original available | 426 | 85.8 GB |
| Original not found in the checked locations | 63 | 27.4 GB |
| Original already low-resolution | 7 | 1.5 GB |

Many original paths had moved from `~/Videos/Live` to the mounted Data video
library. Only unique exact-filename matches were used to locate moved files;
the manifest itself was not changed. Higher-resolution does not mean every
original supplies full 640x480 detail after the existing 4:3 center crop.

## Sample encodes and estimate

Seven 20-second excerpts were encoded from original sources: animation,
sitcom, sports, archive television, an advertisement, news and weather. The
last two sample originals are already 320x240 and were excluded from the
upgrade ratio. The five useful higher-resolution samples total 100 seconds
per profile. All samples were written under `/tmp/livetv-resolution-study`;
source files, sync configuration, cache and iPod files were untouched.

Comparison settings keep quality 2, 20 fps, GOP, audio and crop behavior
constant. Maximum video bitrate/VBV scale with pixel count: 3600/1800 kbit
for 480x360 and 6400/3200 kbit for 640x480. These are measurement settings,
not hardware-qualified playback profiles. Simply raising resolution while
keeping the 1600 kbit cap would make a different size/quality tradeoff.

Across the five higher-resolution excerpts, total output size relative to
320x240 was 2.028x at 480x360 and 3.084x at 640x480. Individual ratios ranged
from 1.71–2.17x and 2.32–3.64x respectively. The aggregate is a sample-based
estimate, not a prediction for every episode or a content-weighted survey.

Applying these ratios only to the 426 files with larger available originals,
while retaining the other 70 files unchanged:

| Conversion | Estimated total Live TV size | Additional space |
| --- | ---: | ---: |
| Keep current files | 114.7 GB | 0 |
| Upgrade available originals to 480x360 | 202.9 GB | 88.2 GB |
| Upgrade available originals to 640x480 | 293.5 GB | 178.8 GB |

The midpoint 480x360 estimate exceeds current free space by 5.5 GB; the
640x480 estimate exceeds it by 96.1 GB. Estimates exclude temporary staging
space. Keeping both old and new sets simultaneously requires more space.
If all missing originals were recovered and the entire collection were
converted, the same crude ratios project roughly 232.6 GB or 353.8 GB.

Per-sample byte counts and assumptions are saved in
`livetv-resolution-measurements.json`. No automatic upscale or full sync was
started. First test a small representative higher-resolution MPEG clip on
the physical Classic, then consider selective conversion or a lower bitrate.
