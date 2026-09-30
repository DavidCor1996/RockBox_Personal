# Composite album artwork: feasibility and implementation specification

Date: 2026-09-12. Status: research and proposed design; no implementation or
hardware qualification performed for this proposal.

## Decision

An artwork-only upscale/sync cannot improve spatial resolution in the current
Now Playing composite path. A useful improvement is feasible at the application
layer by showing a larger cover. Preserving more detail at the same apparent
cover size requires a higher-resolution composite source surface, which is
plausible within existing reserved memory but not yet hardware-proven.

Recommended sequence: qualify a higher-resolution still-image output path first,
then build a bounded Now Playing compositor and artwork-only sync. If hardware
qualification fails, offer a larger-art layout using the proven 320x240 path.
Do not mass-generate enlarged covers before selecting a working consumer.

This is an iPod Classic 6G/7G proposal. It does not establish feasibility for
the iPod Video's different video hardware. No firmware, music, device artwork,
configuration, or database was changed during this research.

## Observed baseline

Read-only inspection of `/run/media/david/DAVID_S IPO/Music` found 314 folders
containing recognized audio files; every one has `cover.jpg`. Of these, 299 are
320x320, three are 300x300, and twelve are slightly rectangular with a maximum
axis of 320. This is a folder inventory, not a tag-based unique-album count or
an audit of tracks under `iPod_Control`.

The mounted configuration selects `iPoneCustom.wps`. Its main artwork slot is
138x138 and its lock-screen artwork slot is 51x51. Size-specific BMPs are present
alongside the JPEGs. The current source searches exact-size artwork before
falling back to a generic cover; replacing only the JPEG can leave the displayed
BMP unchanged. Runtime UI selection can override a configured skin, so the
actual running screen and firmware identity must be confirmed before testing.

The RockPod export path normally caps generic covers at 320 pixels. Thus these
files need not reflect the resolution of the original source. A metadata-only
scan of the local artwork cache found some 1200x1200 and 1280x1280 sources, but
that cache mixes media types and has not been matched to the mounted albums.
There is no verified high-resolution coverage percentage yet.

### Current rendering chain

1. RockPod exports `cover.jpg` and exact-size WPS BMPs.
2. Playback finds and buffers artwork at the skin's requested dimensions.
3. `draw_album_art()` paints the buffered bitmap into the LCD viewport.
4. The LCD driver passes composed RGB565 updates to the composite driver.
5. The composite driver converts to a native 320x240 YUV420 surface.
6. The video processor scales this to destination `(36,24) 648x432` within
   the NTSC output raster.

Direct YUV video updates also use the native LCD-coordinate bounds. There is
currently no generic high-resolution artwork submission interface;
`firmware/export/videoout.h` exposes only active-state availability.

The 138x138 artwork therefore contains only 19,044 source pixels before TV
scaling. Enlarging its source JPEG cannot recover detail discarded at step 2.
Nor does increasing the output raster automatically increase source detail.

## Feasibility options

| Option | Expected result | Assessment |
| --- | --- | --- |
| Upscale JPEGs and sync only | Same selected BMP or same reduced cover | No spatial-resolution benefit |
| Regenerate current BMPs from better originals | Possible improvement to crop, compression or color | Small, image-dependent benefit; current pixel limit remains |
| Larger cover in a 320x240 layout | More source pixels devoted to art; less room for text | Feasible with normal UI/artwork lifecycle work |
| Independent 640x480 TV surface with direct high-resolution art | More detail at the same displayed size, or a larger detailed cover | Preferred research candidate; hardware gate required |
| Separate hardware overlay plane | Potential independent high-resolution composition | Defer: mixer RGB/XRGB paths have failed qualification |

A 220x220 cover in the native layout provides about 2.54 times the source pixels
of 138x138; 240x240 provides about 3.02 times, but consumes the full frame height.
These are sample-count comparisons, not measured perceived-quality gains.

For a 640x480 surface, a 276x276 cover corresponds to twice the current logical
cover dimensions. A TV-specific layout could use approximately 400–432 pixels
for artwork, with placement and text designed separately. Final geometry must
be chosen from physical captures. Output raster samples are not guaranteed
square display pixels; preserve the measured viewport/aspect mapping and verify
with circles and square grids, rather than treating 648x432 as a square-pixel
canvas. Composite decoding, interlacing and the display still limit quality.

## Hardware evidence and first gate

The local DCP750 experiment ledger is the strongest target-specific evidence.
It records failures with 640-wide and 480-line planar surfaces, then a later
successful 320x240 configuration after mixer scan configuration changed from
0x10 to 0x12. Earlier failures therefore do not prove a permanent 320x240
hardware ceiling. They also do not prove larger frames work after the fix.

The qualified contract is private format 8, three Y/Cb/Cr planes, native spans
320/160, plane mode 1, destination `(36,24) 648x432`, H/V ratios 252/142 and
mixer configuration 0x12. Keep this as the production fallback. The ledger's
later two-buffer handoff candidate still calls for physical qualification;
verify the tested baseline rather than assuming current source equals tested
firmware.

Before changing geometry, reconstruct a RetailOS higher-resolution decoded
frame descriptor/setup or obtain equivalent specific S5L8702 evidence. Record
plane sizes, alignment, strides, image/crop fields, scaler ratios and field
state. Do not repeat a blind width/height/ratio sweep or transplant register
semantics from later Samsung chips. If that evidence cannot be established,
mark the high-resolution route blocked and proceed only with the native layout.

Then build an isolated static-pattern experiment with genuinely independent
640x480 detail, not duplicated 320x240 rows. Preserve the known color ordering,
interlaced mixer state, clock ownership, PCM-safe reset ordering and overscan
mapping. Require numbered rows through the bottom of the frame, quadrant
labels, color bars, circles, and alternating-line patterns. Confirm all rows,
correct colors/aspect, stable fields, no lower-half repeat, and clean fallback.
A simulator image cannot qualify private DMA or analog output.

## Proposed software architecture after the hardware gate

Keep album identity, file selection, artwork loading and WPS visibility in the
application. The target driver accepts pixels and geometry only. Extend a
target-neutral `videoout` interface under `HAVE_COMPOSITE_VIDEO_OUT`, with inert
unsupported-target stubs. Do not add shared-code model checks or file lookup
inside LCD/target rendering.

For the first version, support the main Now Playing cover only. The normal WPS
remains the input and playback-screen owner; no second action loop. Preserve
ordinary LCD rendering. Do not expand album browser thumbnails, lock-screen art,
notifications or arbitrary third-party skin overlays in this first version.

The application publishes a bounded request containing copied track/art identity,
WPS session generation, cover destination and visibility. An idle service point
loads at most one current-cover request when input is empty, navigation has
settled and storage/playback conditions allow. Never look up files, decode,
query tagcache or allocate in a draw callback. An explicit sidecar path avoids
a database search for an optional TV decoration.

Suggested lifecycle:

- Dormant: output inactive or WPS not visible; no artwork I/O.
- Pending: new WPS/track generation; ordinary mirror stays available.
- Loading: bounded chunk reads into an unpublished staging region; yield to
  input and discard stale work after every chunk.
- Ready: validate dimensions, payload length and checksum; publish only if
  the track and WPS generations still match.
- Invalidate: navigation, track change, modal overlay, USB, theme reload,
  video/plugin entry, output disable or undock immediately revoke the cover.

Fallback is the normal mirror and ordinary artwork, with no retry loop. Same-
album track changes may reuse a validated asset; different tracks with distinct
cover identities must not be merged solely by folder name.

Composition must be ordered: compose the mirrored background into the inactive
TV surface, then apply the cached high-resolution cover, then publish one
complete frame. Later LCD dirty updates must not erase the cover or place it
above a modal dialog. Initially suspend enhanced artwork for any overlapping
popup, nonstandard overlay, transition, or unsupported skin geometry. Restore
the mirror with a full redraw. Never retain pointers to movable playback
bitmap data or to a revoked staging buffer.

All composition and submission need explicit serialization with ordinary RGB
updates, direct YUV updates, output shutdown and hibernate. Do not hold the LCD
mutex during disk reads or long image work. Keep waits interruptible and put
only the minimum descriptor register writes in the atomic section. Cache clean
and field handoff must complete before reusing a DMA-visible front buffer.

## Memory and throughput budget

The current target owns a 640x480 array of 32-bit elements: 1,228,800 bytes.
Current two-buffer 320x240 YUV420 storage uses 230,400 bytes within that array.
This reserved allocation is not general free heap.

| Proposed resident data | Bytes |
| --- | ---: |
| One 640x480 YUV420 frame | 460,800 |
| Two 640x480 YUV420 frames | 921,600 |
| Remaining within existing target array | 307,200 |
| One 432x432 preconverted YUV420 cover | 279,936 |
| Remaining after frames and cover | 27,264 |

This establishes arithmetic fit only. Alignment, metadata, ownership and all
other diagnostic/format users require a linker and lifetime audit. RGB/XRGB
diagnostic modes currently reuse the same allocation; they must revoke the
cover before taking ownership. Do not silently overlap formats. Use explicit
regions and compile-time bounds, with runtime mode exclusion.

Preconversion on the host avoids an on-device JPEG decoder workspace and a
second RGB cover. Version the sidecar color conversion to match the qualified
target range/order. Keep skin layout and source pixels separate; the sidecar
must not contain a screenshot of a whole UI.

Double-buffering a full 640x480 plane increases a full copy from 115,200 to
460,800 bytes. At 10 full copies per second that alone is 4.608 MB/s, excluding
conversion, cache publication and DMA reads. Measure actual latency and memory
bus load. Update on cover/layout changes and existing WPS cadence; no new
animation timer. The existing field wait has an approximately 25 ms polling
budget, so do not multiply descriptor presentations per artwork update.

No `core_alloc()` or plugin audio-buffer use is permitted for this decoration.
Do not claim or resize playback artwork slots when docking or entering WPS:
`playback_update_aa_dims()` sends `Q_AUDIO_REMAKE_AUDIO_BUFFER`. Existing PCM,
playlist and playback-buffer ownership must remain intact. A native larger-art
fallback must arrange its required slot before playback, with its own memory
review; do not switch slot dimensions while music is playing.

## Artwork preparation and sync contract

After the renderer/profile is qualified, inventory every intended music root
and match tracks to album/artwork identities. Prefer an existing verified local
original, then embedded source art, then an accurately matched online original
when requested. Record source dimensions and provenance. Preserve editions and
track-specific covers; uncertain matches are reported, not replaced by guesses.

Use the best original at least as large as the chosen cover dimensions where
available. Conventional resampling of a 320-pixel original can smooth edges but
cannot supply missing detail. Do not label such a derivative high-resolution
source art. Generative reconstruction is not the default for album logos or text.

Retain current `cover.jpg` and WPS BMPs. Add a versioned, namespaced TV sidecar
(for example `cover.tvout-v1.yuv`) with a fixed header describing dimensions,
plane layout, payload length, conversion version and checksum. The final format
is contingent on hardware qualification. Reject malformed dimensions, overflow,
truncation or unsupported versions before publication; a sidecar failure never
blocks music.

Extend RockPod's existing artwork export/sync services with an artwork-only plan:
source hash + conversion parameters + profile version determine cache identity.
The current generic JPEG cache checks source hash but not all export parameters;
changing only its size setting is not a reliable forced regeneration strategy.

Stage locally, validate, back up any replaced sidecars, copy via temporary files,
rename completed files, verify checksums, then flush. Repeated sync must make
zero writes when inputs are unchanged. Do not rewrite audio files or trigger a
library rebuild. Record missing/low-resolution sources separately. Keep
firmware/configuration/database checksums unchanged across an artwork-only sync.
At 432x432 YUV420, 314 sidecar payloads total 87,899,904 bytes (about 83.83 MiB),
excluding headers; the final inventory may differ from the folder count.

## Delivery gates and acceptance

1. Establish baseline firmware identity and hardware evidence; qualify the
   high-resolution static pattern or explicitly select the native fallback.
2. Add generic pixel submission and bounded ownership, then the WPS service.
   Build native 6G, simulator, and a target without the capability. Check ARM
   stack, linker BSS and total allocation before/after. No additive full-screen
   allocation beyond the audited existing target region.
3. Test truncated/invalid sidecars, rapid track changes, same-album reuse,
   missing covers, stale generations, popups, USB, hibernate and dock changes.
   Simulator verifies composition and lifecycle only.
4. Run active-playback navigation regression: ten full Music hierarchy cycles,
   twenty rapid Albums/Artists switches, rapid Menu exits, stable descriptor
   counts and stable core memory. No database-loading regression, playback
   restart, elapsed-time reset, playlist change or PCM underrun.
5. Hardware A/B on identical artwork: current 138 cover, larger native cover,
   and high-resolution TV cover. Capture the same display/settings and assess
   readable cover text, edges, field flicker, color, aspect and overscan.
   Record p50/p95 cover-load and update times, input latency, CPU/bus behavior
   and failures. Initial targets: no input stall over 50 ms attributable to art,
   ready cover within 1 second on the mounted library's storage, and zero audio
   discontinuities; these are proposed targets, not measured results.
6. Qualify RGB UI and direct YUV video restoration, volume, pause/resume,
   Database/Files music before and after video/plugins, rapid dock/undock and
   output Off/Auto. Verify no optional work or boost ownership remains undocked.
7. Only after those gates, generate the full artwork-only plan and sync.

Hardware experiments should use the isolated Rolo test image and runtime,
preserving the personal installation and its verified database. Implementation
or deployment is outside this research deliverable.

## Evidence and limitations

Local source references (paths relative to repository root):

- `apps/recorder/albumart.c`: `find_albumart()` and exact-size lookup precedence.
- `apps/playback.c`: `audio_load_albumart()` and `playback_update_aa_dims()`.
- `apps/gui/skin_engine/skin_display.c`: `draw_album_art()`.
- `firmware/target/arm/s5l8702/lcd-s5l8702.c`: composed RGB and direct YUV hooks.
- `firmware/target/arm/s5l8702/ipod6g/videoout-6g.c`: reserved storage, format-8
  geometry, two-buffer copy/presentation and field wait.
- `firmware/export/videoout.h`: current generic capability surface.
- `rockpod/services/artwork_manager.py`: source/export/cache behavior.
- `rockpod/services/rockbox_wps_art.py`: device theme dimension discovery.
- `docs/ipod6g-dcp750-videoout-results.md`: historical physical results,
  including the scan-state correction and later unqualified handoff candidate.
- `docs/wps-album-art-first-load-performance-spec.md`: existing artwork sync work.

The [upstream Rockbox album-art manual source](https://raw.githubusercontent.com/Rockbox/rockbox/master/manual/appendix/album_art_info.tex)
confirms external BMP/JPEG support and excludes RLE BMP and progressive/multiscan
JPEG. This fork's inspected code is authoritative for its lookup extensions.
Public web research did not establish a documented S5L8702 high-resolution
format-8 configuration; do not infer one from general NTSC or later-SoC docs.

This working tree contains substantial pre-existing changes. Conclusions describe
the inspected local source and recorded hardware history, not a verified match
between that source and the firmware presently installed on the iPod. No new
build, physical test, image quality measurement, or full source-to-album matching
was performed for this spec.

## Qualification update: first physical chart

The isolated 640x480 chart displays rows 00–15 in the user's photo. The user
reports shimmer/apparent motion below the circle, where the pattern alternates
black and white every source row. This is consistent with interlaced fine-line
flicker, but larger-feature stability has not yet been confirmed.

Add a host-side vertical anti-flicker comparison to the artwork quality gate:
no filter versus a bounded three-tap vertical low-pass candidate (1,2,1)/4,
with replicated edge samples and identical geometry/color conversion. This is
a proposed comparison, not a selected production filter. Judge small cover text,
edge detail and temporal stability on the physical display. The filter should
be exported once with the artwork, not computed in the WPS draw path.

Thin horizontal details are known to flicker on interlaced displays; see
[Adobe's explanation of interlaced-image flicker](https://helpx.adobe.com/sg/premiere/desktop/troubleshooting/media-issues/eliminate-flicker.html).
This supports the mechanism, not a diagnosis of the particular hardware capture.
