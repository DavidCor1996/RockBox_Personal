# Classic Cover Flow implementation — 2026-09-12

The follow-up fidelity pass is documented in `coverflow-retail-fidelity.md`.
This is a device-side implementation milestone, not full acceptance of
`coverflow-album-open-and-cache-spec.md`. No physical player was deployed.

## Implemented

- iPodJS on Classic-size color screens uses a centered, vertical-axis cover
  flip into a 256-pixel track panel. Side covers remain behind it. The panel
  has an album/artist header, shaded blue selection, and right-aligned times.
  Menu reverses the opening from its current position and restores browsing.
  Reduced Motion skips directly to the ready destination.
- Movement uses elapsed ticks, approximately 300 ms, with a roughly 30 Hz
  presentation cap. Rendering uses a fixed 8,192-byte strip, not another
  framebuffer. The silver header and transport/battery resources are cached.
- Selected-album metadata prepares in batches of four tracks after an 80 ms
  navigation settle. Opening waits for readiness without replacing the cover;
  the pending request times out after two seconds. Renderers do not enumerate
  metadata, decode images, or open files.
- Completed ordered track records persist in versioned little-endian `.pft`
  files. Lengths, full album/artist keys, database generation, string bounds,
  and a payload checksum are checked. Temporary writes publish by rename.
  Warm re-entry reuses these records; closing keeps the in-memory list.
- Cache residency misses no longer draw the missing-art tile. Complete scenes
  gate LCD publication. Visible-neighborhood surfaces are protected against
  lookahead eviction. Worker reads use the existing decode staging workspace
  outside the rendering mutex and publish completed surfaces under that mutex.
- Only a verified absent source writes the negative artwork marker. Damaged or
  absent prepared artwork requests repair. Last selection persists by album
  and artist strings, with existing explicit/WPS selection policy retained.
- Playback ownership stays unchanged: no new core allocation, audio-buffer
  claim, stop/restart, PCM operation, or playlist mutation for decoration.
  Explicit track playback still uses PictureFlow's existing playback path.

## Reference and remaining visual uncertainty

Reference: [iPod Classic: The New User Interface](https://www.youtube.com/watch?v=ge56Li0PuQ0),
the album opening near 102 seconds. Source video is 30 fps, 320×240. The useful
LCD crop is approximately `248:182:39:30`. Frames around 101.8–103.0 seconds
show the front face turning edge-on, then a wider track-list back face settling
with side covers still visible. The observed transition is roughly 270–300 ms,
with at least one camera-frame uncertainty plus LCD response/encoding blur.
The video's firmware version is not identified; it depicts the Classic, not
the touch interface. The existing extracted resource set has separate iPod35
2.0.4 provenance and is not evidence that this version ran on every 6G.

The follow-up now uses projective geometry, the reference's larger typography
and row spacing, and a separately inspected closing sequence. Long-title
scrolling, ellipsis, and page movement have focused tests. Exact color values,
sub-camera-frame motion, and every font/language variant remain unqualified.
Do not describe this build as pixel-identical RetailOS.

## Native resource audit

The initial milestone linked with text 57,620, data 892, BSS 38,500 bytes.
The fidelity build links with text 62,404, data 912, BSS 96,636 bytes.
These are whole-plugin sizes, not a measured before/after delta. The principal
new fixed pixel stores are 8,192 bytes for the strip, 12,800 for the header,
1,014 for battery data, and 1,344 for transport data. Saved identity strings
add 520 bytes. Metadata continues using the existing bounded track arena.
Simulator trace storage is excluded from native builds.

Final ARM prologues: `render_retail_tracks` uses 176 bytes including saved
registers; `read_pfraw` uses 56 bytes. The preparation implementation uses
944 bytes; its guarded wrapper uses 16 bytes. These are individual frame sizes, not maximum
transitive stack usage. Native runtime pressure still requires hardware tests.
The shared resource source also gained a missing plugin-only `lseek` mapping;
the core execution path and plugin ABI are unchanged.

## Tests and acceptance gaps

Reproducible focused gate: `tools/pictureflow_album_transition_gate.py`.
It creates a disposable eight-album/24-track library with distinct artwork,
generates a matching database, starts real simulator playback, and checks
repeated album opening/closing, frame readiness, monotonic animation position,
playback status/elapsed progression, persistent cache reuse, and descriptor
counts. The bounded RAM trace is flushed only at plugin exit.

Simulator, native iPod 6G, and native iPod Video plugin builds pass. The existing
broader navigation gate was attempted but failed waiting for Now Playing
after trace 260; its trace subsequently contains the shutdown animation.
This is not a passing full-navigation qualification. No unrelated navigation
code was changed to hide that failure.

Initial milestone focused run: `/tmp/coverflow-final-qualified`, 100 open/close cycles plus
all eight center covers checked in both wheel directions. The complete trace
contains 3,476 samples, one persistent-cache hit, and zero scene waits.
Descriptors stayed at 15. All sampled playback states stayed active; the
playlist count stayed at three, and elapsed playback progressed from 1,333 to
81,320 ms without regression. This checks playlist count, not full playlist
content identity or audible output from the SDL dummy audio driver.

For 100 openings and 100 closings separately, motion duration was 300 ms
median, 300 ms p95, and 310 ms worst. This measures simulator state entry to
the completed endpoint, not button-to-first-motion, album readiness, plugin
launch, or physical storage latency. A preceding run exposed per-frame integer
rounding extending the flip to 330 ms; carrying the fractional remainder fixed
that drift in the tested final build.

The focused gate and Python syntax check pass; the scoped C diff has no
whitespace errors. Test screenshots and traces are retained in the disposable
run directory, not copied onto a player. The broader gate also failed on a
second attempt with the requested 10 hierarchy/20 switch stress settings
(waiting for Now Playing after trace 258), before those stress loops ran.

Still required for full specification acceptance:

- hardware cold/warm loading and input latency, matched baseline comparisons,
  audible continuity, core-memory trends, and exact-build physical testing;
- slow-read/error injection, per-frame cover-identity assertions, huge-library
  and small-memory cases, full Music hierarchy/rapid-switch gates, Hold/USB/
  hibernate qualification, and separately initiated Files playback;
- oversized-album paging (the bounded arena currently rejects over-capacity
  lists), incremental RockPod host preparation, and cache-pack benchmarking;
- generation-wide artwork invalidation and interrupted-sync qualification;
- physical RetailOS visual acceptance and broader Unicode/font coverage.

Warm caching removes repeated metadata work; it does not make an uncached or
sleeping disk instantaneous. Retaining a complete old scene can delay scrolling
when storage cannot keep up. Hardware deployment remains gated on the above
required navigation/resource checks.
