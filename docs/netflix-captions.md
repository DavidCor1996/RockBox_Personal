# Netflix controls and captions

Hold Select on an episode to open its context menu and mark it watched.
Hold Menu in Netflix to open its settings. English subtitles and category
visibility persist in `.rockbox/videolist/netflix-settings.bin`. Hidden
categories retain their files and metadata. Subtitle availability appears as
`CC` in episode details. The panel uses the existing bounded iPodJS transition
workspace and RetailOS menu font/row metrics, with Netflix colors and the
sourced Netflix logo. The two video players also use converted pixels from
that logo, rather than a font approximation of its wordmark.

The on-device subtitle sets are ALF season 1, episodes 1–5, and all 37
Death Note episodes. Death Note captions are transcribed from the matching
English dubbed audio. The embedded English PGS track translates the Japanese
script and must not be used as dub subtitles. The dub tracks are generated
locally with small.en and can contain recognition errors. Their metadata label
is "English dub". The existing intro/ending markers bound the dialogue range;
quiet audio remains included during transcription.

ALF captions were prepared for the initial five-episode test. They were
transcribed locally from the matching source media using faster-whisper
small.en. They are machine-generated and may contain recognition errors.
English is enabled for this hardware test; episodes without a track play
normally. Companion library metadata and the video manifest were not replaced.

## Caption files

Each video has a `<complete-video-filename>.nfs` sidecar. Corresponding `.srt`
and `.nfs.json` files preserve readable text and provenance. The independent
`.rockbox/videolist/subtitles.tsv` inventory records language, cue count, source,
and path without changing the existing video manifest version.

NFS1 is a little-endian format for the two current ARM iPod targets:

- Header: uint32 magic `0x3153464e`, uint32 cue count (maximum 20,000).
- Each fixed 3,176-byte record: uint32 start milliseconds, uint32 end
  milliseconds, then 3,168 bytes of packed caption pixels.
- Pixels cover 288×44, row-major, four pixels per byte with the first pixel
  in the low two bits. Values are transparent, black, gray, and white.
- Cues are ordered, non-overlapping, and use an exclusive end timestamp.

`tools/prepare_netflix_captions.py` accepts timed-word JSON and writes NFS1,
SRT, metadata, and a preview. The initial font is DejaVu Sans Bold at 13 pixels,
with a black outline, at most two lines, 16-pixel side margins and a 14-pixel
bottom margin on the 320×240 display. Captions move above visible playback
controls. MPEG temporarily suppresses captions during its volume/skip prompt.
The rendered captions are part of the YUV video output and therefore also
reach composite TV output. Actual TV overscan and readability need hardware
confirmation.

The shared reader in `apps/netflix_captions.h` uses two fixed 3,168-byte masks.
File reads happen in player service loops, never in draw
callbacks. Normal cue changes read sequentially; seeks use binary search.
The renderer skips groups of transparent pixels, and ordinary MPEG overlay
backgrounds use bulk copies instead of the general per-pixel mapper.
In normal fit mode Netflix composes the full picture and caption/volume band
in the existing overlay buffer, then presents once. Band geometry is sampled
once per frame. This avoids independently presenting two portions of a frame.
No additional full-screen cache or playback allocation is introduced.
A mutex protects publication and rendering; file reads do not hold
that mutex. No playback-buffer allocation or audio lifecycle change is needed.
The current renderer is wired into MPEG and native iPod 6G H.264 playback.

Validation for this change: native iPod 6G build, source whitespace check,
host ASan/UBSan caption timing/seek/gap/truncation/bounds checks, and comparison
of source/device durations and installed caption checksums for all 37 Death
Note episodes. Compositor pixel checks cover full-screen video, letterboxing,
pillarboxing, cropping, and caption bands. No
simulator testing was performed, as requested. Hardware behavior is untested.
