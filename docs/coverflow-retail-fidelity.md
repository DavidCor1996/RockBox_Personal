# Classic Cover Flow fidelity pass

## References

The primary motion reference is [iPod Classic: The New User Interface](https://www.youtube.com/watch?v=ge56Li0PuQ0).
Its 320×240, 30 fps video shows opening near 102.1–102.5 seconds and closing
near 111.7–112.1 seconds. The LCD occupies approximately x=39, y=30, w=248,
h=182 in the filmed frame. Normalize that crop to 320×240 for layout comparison.
The firmware version is not identified. LCD persistence and video compression
blend neighboring frames, so an exact Apple easing curve cannot be recovered.

A second contemporary reference is Julie Strietelmeier's September 2007
[Classic review](https://the-gadgeteer.com/2007/09/19/apple_ipod_classic/),
particularly its [album track panel photograph](https://the-gadgeteer.com/assets/apple-ipod-classic-25.jpg).
It independently confirms the blue heading, separated title/artist roles,
seven visible track rows, right-aligned times, and covers behind the panel.
It is a photographed display, not a color-calibrated framebuffer capture.

## Corrected mismatches

| Element | Implementation after comparison |
| --- | --- |
| Destination bounds | x=32, y=28, width=256, height=212; reaches the LCD bottom |
| Album heading | 48 px high; title at x=40/y=36, artist at x=40/y=57 |
| Track rows | 24 px cadence; first row at y=76, text at y=79; seven visible rows |
| Fonts | Existing loaded Apple 19-point bold for album/track text, 15-point regular for artist, 15-point bold for status title |
| Album labels | Separate 20 px lines starting at y=194 and y=214; no overlapping baselines |
| Reflection | Lower initial opacity and a 48 px quadratic fade, instead of a bright reflection continuing through the labels |
| Background covers | 70° canonical side yaw, nearest outer edges x=55/264, repeated at 28 px intervals; replaces the shallower 55° PictureFlow defaults |
| Perspective | Inverse projective sampling using the cover renderer's camera distance; near-edge enlargement affects glyph width and height together |
| Front face | Corrected the tilt sign to join the back face consistently through the edge-on position |
| Closing | Back face retreats in roughly the first quarter; the cover settles over the remaining interval. It is not a literal time-reversed opening. |
| Layer order | Returning album labels sit behind the turning track pane, not painted over it |
| Overflow | Unselected titles use a Unicode ellipsis; selected titles scroll inside their own clip region, with stationary durations |

The comparison uses the reference's Guitar Hero / Megadeath labels and seven
track names/durations. Synthetic colored covers deliberately distinguish
artwork identity from UI layout. Four albums precede the reference album, so
both side stacks remain visible in the matched captures.

The layout was visually compared against normalized footage with an intended
±4 px camera/crop tolerance; this is not an automated pixel-difference pass.
The blue gradients are visual approximations supported by both references.
The side-cover correction follows subsequent user feedback: the initial
fidelity pass still inherited PictureFlow's shallow 55° side geometry. The
70° yaw is a projection fit to the filmed edge slopes, not a recovered Apple
constant. Side placement is translated after canonical projection, preserving
the same edge slope across each stack. Non-iPodJS geometry remains configurable.
At the steeper yaw, fixed-point inverse sampling exposed a negative fractional
first column that unsigned-wrapped and discarded whole covers. Clamping that
border sample fixes the missing faces without additional storage or I/O.
The focused gate now checks visible side-cover identities as well as all four
resting center-cover corners. A fixed 650 ms wheel delay occasionally captured
a correct cover a few pixels before rest; the corner check waits at most two
seconds for settlement instead of misclassifying the adjacent white pixel as
missing artwork. This does not constitute per-frame artwork validation.
Neutral-gray resource 393 was investigated and rejected: it does not establish
the Classic Cover Flow heading color. The stock silver status/transport assets
retain their existing separately verified resource provenance.

## Rendering and memory safety

Font metrics identify already-loaded matching Apple fonts. No new `font_load`,
core allocation, audio-buffer claim, or playback restart is introduced. If a
matching private font is unavailable, the existing UI font remains the fallback.

Glyph reads/rasterization occur during idle/pending preparation, with input and
Hold gates and an 80 ms settle for optional work. Drawing consumes private
packed coverage masks, so eviction from the core font cache cannot cause a
glyph-file read during a flip or marquee. New pages keep the previous complete
text mask until preparation finishes. The selected-title marquee uses only its
cached mask; its 2,048-pixel limit ends in an ellipsis for extreme strings.
Mask preparation reuses Rockbox's bidirectional ordering and diacritic
placement helpers; glyph coverage still depends on the already-loaded font.

Added fixed coverage stores: 27,136 bytes for the track pane, 6,400 bytes for
album labels, and 24,576 bytes for the selected-title strip. Total: 58,112 bytes.
These are four-bit coverage masks, not additional RGB framebuffers. The existing
8,192-byte RGB compositor strip remains. This deliberately trades about 1.9%
of the 6G's 0x2f0000-byte plugin pool for I/O-free font rendering. It does not
shrink playback memory. Required visible-cover capacity still needs native
stress testing with large libraries.

Relative to the preceding milestone, the 6G ELF changes by +4,784 text,
+20 data, +58,136 BSS bytes. Final totals: text 62,404, data 912, BSS 96,636.
The 5G plugin totals are text 65,372, data 912, BSS 96,652.
Inspected ARM frames include 176 bytes for the compositor, 344 bytes for the
ellipsis helper, 104 bytes for glyph rasterization, and 80 bytes for text service.
The side-cover renderer uses 232 bytes including saved registers after the
geometry correction; its existing reflection table is included in that frame.
These are per-function frames including saved registers, not a complete
transitive stack or hardware-pressure measurement.

## Qualification boundary

Final side-angle build: `/tmp/coverflow-retail-side-final` passed 100 playback
open/close cycles, visible side-cover identity checks, and the full eight-cover
wheel sweep in both directions with bounded resting-corner checks. Descriptors
stayed at 15, all 3,579 trace samples had complete scenes and active playback,
and elapsed time advanced from 1,333 to 83,540 ms with seven playlist entries.
There was one persistent metadata-cache hit and zero scene waits. Opening was
300 ms median / 310 ms p95 / 320 ms worst; closing was 300 / 300 / 320 ms.
These remain simulator motion-only measurements. Native 6G/5G builds, Python
syntax, and scoped whitespace checks pass on this revision. Geometry adds no
new fixed memory; the earlier long-title qualification used the same text code.

The typography/flip focused evidence is retained under `/tmp/coverflow-retail-validated-final`:
100 open/close cycles, eight albums/56 tracks, reference album centered with
four preceding albums, and an artwork-identity sweep in both directions.
Descriptors remained at 15; the trace contains 3,575 samples, one persistent
metadata-cache hit and zero scene waits. Playback remained active with seven
playlist entries and monotonically advancing elapsed time (1,333–83,305 ms).
Opening and closing each measured 300 ms median / 300 ms p95; worst cases were
320 ms opening and 310 ms closing. These are simulator motion-only timings.

`/tmp/coverflow-retail-unicode-final` additionally passed the 15-track album test
with accented/non-ASCII long titles, selected-row marquee, stationary durations,
ellipsis, movement into the next page, and three playback open/close cycles.
All six transitions measured 300 ms; descriptors stayed at 15 and playback
elapsed time advanced from 1,333 to 11,285 ms without regression. This fixture
uses precomposed accented characters, not exhaustive RTL/combining-mark cases.
The gate retains first-frame/final-frame screenshots and strips for both flip
directions. These checks do not establish native audible output or full
playlist-content identity beyond the reported count and current elapsed time.

The focused gate records opening/closing frames and supports long Unicode
titles, multi-page albums, reference labels, artwork sweeps, and active-playback
stress. Native 6G, native 5G, and simulator plugin builds pass. The resource
adapter's missing plugin `lseek` mapping was fixed without changing the ABI.

Reproduce the focused reference test with a disposable installed simulator
runtime and its matching host database tool:

```sh
python3 tools/pictureflow_album_transition_gate.py \
  --build build-sim-ipod6g --runtime /tmp/coverflow-fixture \
  --database-tool /tmp/qs-db-build/database.ipod6g \
  --output /tmp/coverflow-retail-side-final \
  --cycles 100 --tracks 7 --retail-reference
```

Use a fresh output directory for each run. The long-title variant uses
`--cycles 3 --tracks 15 --long-titles`. Test runtimes are generated copies;
their large music fixtures may be removed after preserving reports/captures.
The preview GIF assembles captured simulator frames with viewing pauses; use
the trace, not GIF playback speed, for timing evidence.

Final plugin SHA-256 values:

```text
6G  649cafa9dce5111e4e002f68656f897aaa08729b241381d569169e6dbd80e758
5G  f56940f71cf79020506ef081b36ba5a02d0dd45d5714c8ca237ceeba8cfa19ef
sim 0e824487c11819c8fe848385eeed979550fa6f5635778a7a9a31c16552c21672
```

This is a reference-matched visual implementation, not a claim of bit-identical
RetailOS or physical-device acceptance. Exact color calibration, stock marquee
velocity/loop policy, sub-frame easing, native LCD response, and hardware
loading/continuity remain outside what these photographs and simulator tests
prove. The earlier full-navigation gate failures remain recorded in the main
implementation report and still block deployment readiness. No player was
modified or deployed during this pass. A later full-navigation retry exhausted
the temporary filesystem before a usable first capture; it adds no behavioral
qualification evidence. Only generated test-runtime copies were cleaned up,
then both focused final-build gates were rerun successfully.
