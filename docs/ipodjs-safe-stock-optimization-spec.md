# iPodJS Safe Stock Optimization Specification

## Objective

Improve iPodJS responsiveness, battery efficiency, navigation reliability,
and stock iPod classic fidelity without changing music playback behavior or
ownership.

The work is deliberately ordered so that executable regression evidence
exists before renderer changes. The stock alphabet fast-scroll feature is
included after the common list renderer is made incremental, because it must
reuse that renderer rather than add a second drawing path.

## Non-Negotiable Contracts

### Music playback is outside scope

No phase may change:

- codecs, DSP, PCM, mixer, pcmbuf, audio drivers, or the CS42L55 path;
- playback thread timing, WPS action timing, pause/resume, seek, skip, stop,
  playlist creation, playlist ordering, resume state, or buffering policy;
- the shared plugin/audio buffer or any plugin audio lifecycle;
- playback album-art slot ownership, dimensions, handles, or loading policy;
- tagcache files, tagcache playback queries, or database contents.

The renderer may read `audio_status()`, `audio_current_track()`, playlist
display index/count, elapsed time, battery state, and existing album-art
handles. It must not issue a playback command.

The following code is frozen for this project unless a separate playback
change is explicitly approved:

- the action switch in `gui_wps_show()`;
- `wps_do_action()` and all seek/skip helpers;
- `root_menu_video_handle_play_pause()`;
- `root_menu_video_handle_tree_stop()`;
- `ipodjs_video_play_pause()`;
- `root_menu_video_ensure_aa_slot()` and the playback album-art claim path;
- all `audio_*`, `playlist_*`, PCM, mixer, pcmbuf, codec, and plugin-buffer
  implementation files.

The WPS optimization must therefore live behind the existing
`root_menu_ipodjs_draw_wps_frame()` call. `gui_wps_show()` continues polling
and dispatching actions exactly as it does in the accepted build.

### Visuals must come from approved stock references

No new visible element may be approximated with C drawing primitives, an
AI-generated image, a hand-drawn SVG, guessed geometry, or guessed RGB
values. In the stock fidelity profile:

- frames, icons, masks, gloss, shadows, backgrounds, and overlays are decoded
  from approved, pre-rendered native BMP assets;
- variable fills use clipping of an approved fill-strip/mask asset rather
  than a procedurally invented gradient;
- dynamic text uses the approved stock-matching bitmap font and measured
  baseline, kerning, alignment, and colour;
- animations use frame geometry and timing measured from an approved stock
  iPod classic reference recording;
- if an exact reference or asset is unavailable, that visual change is
  blocked. An approximate fallback must not be added.

Each new asset requires a provenance entry containing its source reference,
source checksum, crop coordinates, output dimensions, conversion command,
and output checksum. Conversion may resize, crop, change pixel format, and
apply Rockbox transparency encoding; it may not repaint the source.

Existing dark mode, accent choices, density choices, and other non-stock
features remain available. The pixel-identical requirement applies to the
default light, solid, blue **Stock Fidelity** profile. Other profiles must
retain their current appearance unless an approved reference asset exists for
the change.

## Required Implementation Order

Every phase has its own stop gate. A failed phase is fixed or reverted before
the next phase begins.

### Phase 1: Freeze a playback-safe visual baseline

Create an automated simulator journey before modifying rendering:

1. Home -> Music -> Artist -> Album -> track -> While Playing.
2. While Playing -> Menu -> the exact originating track list.
3. Cover Flow -> album -> track -> While Playing -> Menu -> the same album in
   Cover Flow.
4. Repeated Menu back through every parent to Home.
5. Hold lockscreen from Home, Music, artist, album, track, Settings, and While
   Playing.
6. Medium Play hold stops music; long Play hold enters shutdown without a WPS
   frame appearing first.
7. Album art in Cover Flow and While Playing represents the same intact image.

Capture golden frames for Home, a normal submenu, Artists, Albums, While
Playing, the Hold lockscreen, and Cover Flow return. Add an event trace with:

- screen identifier and navigation origin;
- selected row and scroll window;
- current track path, playlist index/count, play/pause state, and elapsed
  second;
- full-frame versus dirty-rectangle updates;
- album-art handle identity, read only.

The trace is simulator-only and must not call or wrap playback functions.

#### Phase 1 gate

- The accepted deployed behavior passes the complete journey.
- The journey is repeatable from a clean simulator root.
- Golden images and trace assertions detect a grey frame, legacy Rockbox
  theme frame, incorrect return destination, corrupt art, or shutdown flash.
- A static diff guard rejects added calls to `audio_stop`, `audio_pause`,
  `audio_resume`, `audio_next`, `audio_prev`, `playlist_*`, PCM/mixer APIs,
  `plugin_get_audio_buffer`, or `plugin_release_audio_buffer` in this project.

### Phase 2: Make While Playing updates event-driven

Keep the existing WPS loop and action polling byte-for-byte unchanged. Add an
iPodJS renderer state snapshot behind `root_menu_ipodjs_draw_wps_frame()`.

The snapshot contains only display inputs:

- current track path or stable metadata identity;
- existing album-art handle and dimensions;
- elapsed whole second and duration;
- play/pause, repeat, shuffle, rating, playlist display index/count;
- battery percentage, charger state, Hold state;
- volume-overlay visibility/value;
- iPodJS visual-settings generation.

Update classes:

| Change | Allowed LCD work |
| --- | --- |
| No snapshot change | No drawing and no LCD update |
| Elapsed second | Progress/time rectangle only |
| Battery/play/repeat/shuffle | Status-bar rectangle only |
| Volume value while overlay is visible | Volume rectangle only |
| Volume overlay begins or ends | Bottom information area only |
| Track, artwork, Hold, font, palette, or surface changes | Full frame |

The static cover, reflection, title, artist, album, and background must not be
redrawn for elapsed-time changes. The renderer continues using the existing
opaque native album-art blit and existing playback-owned art handle.

#### Phase 2 gate

- WPS controls and `gui_wps_show()` action dispatch have no source diff.
- An idle WPS produces at most one progress update per elapsed second and no
  full-frame update.
- Pause, resume, volume, Hold, track change, LCD wake, and menu return redraw
  the correct regions without stale pixels.
- Golden WPS pixels are unchanged outside the intentionally updated region.
- Continuous MP3 and FLAC playback show monotonic elapsed time, unchanged
  path/index/count, no pause, no restart, and no underrun while the test runs.

### Phase 3: Make the common iPodJS list renderer incremental

Add a renderer-only cache keyed by:

- `gui_synclist` identity;
- title and visual-settings generation;
- first visible item, selected item, selection size, row height, and item
  count;
- scrollbar geometry;
- header playback/battery state;
- callback-draw generation when the owner can provide one.

When only the selection changes inside the same visible window, repaint the
old row, new row, and changed scrollbar pixels. When only the header state
changes, repaint the header. Perform a full repaint for a new owner, title,
window, font, palette, callback generation, LCD wake, Hold transition, USB
event, or uncertain state.

Custom draw callbacks default to full repaint until explicitly marked safe
for row-only updates. Correctness takes precedence over an incremental update.

This phase changes drawing only. It does not change `gui_synclist_do_button()`,
wheel acceleration, selected indices, list callbacks, tagtree, or playback.

#### Phase 3 gate

- One ordinary wheel step transfers no more than the two affected rows and
  scrollbar/header damage, unless the visible window changes.
- Fast wheel input never leaves duplicate highlights or stale labels.
- Every stock and custom list callback passes forced-full versus incremental
  image comparison.
- Playback safety assertions from Phase 2 remain green while navigating every
  tested list.

### Phase 4: Use one reference-derived stock status bar

Replace the duplicate root-menu and generic-list status renderers with one
`ipodjs_ui` compositor. It is used by Home, all list screens, Settings, While
Playing, and the Hold presentation.

Required stock states:

- menu title placement;
- play and pause status;
- Hold/lock status;
- approximate remaining battery fill;
- low battery;
- charging lightning bolt;
- charged plug.

Required assets include the complete status background, battery frame,
battery fill strip/mask, low-battery fill, charging bolt, charged plug, play,
pause, and lock icon. The existing repository assets may be reused only after
pixel comparison proves they match the approved reference. Missing bolt,
plug, lock, or background assets block this phase; they are not drawn with
lines, polygons, gradients, or guessed colours.

The current forced minimum 15-percent battery fill is removed in the stock
profile. Fill is clipped to the measured reference geometry. Battery state is
display-only and may not affect power or playback behavior.

#### Phase 4 gate

- There is one implementation and one asset set for every iPodJS status bar.
- Golden comparisons cover playing, paused, Hold, low battery, half battery,
  full battery, charging, and charged states.
- Non-text reference pixels have zero mismatches at native 320x240 scale.
- Text baseline and title bounds match the approved reference mask.
- Missing assets fail the build/test for Stock Fidelity; no procedural visual
  fallback is accepted.

### Phase 5: Add stock-identical alphabet fast scrolling

Implement the iPod classic long-list alphabet navigator in the common list UI,
after incremental list drawing is stable. This is a UI index over the existing
list; it must not query or mutate the active playlist, playback engine, audio
buffer, or tagcache files.

#### Eligible lists

- alphabetically sorted Artists, Album Artists, Albums, and Songs;
- other explicitly marked, alphabetically sorted long lists;
- only when the list is long enough to require fast scrolling.

Album Artists is evaluated independently from Artists and Albums. Because it
is a deduplicated credit field, a real library can have a long artist/album
catalogue but fewer than twelve album artists. The stock navigator remains
available there as soon as the browser contains a real metadata row; the two
tagtree utility rows do not suppress it. This route has its own short-library
simulator fixture and must not rely on the Artists test for coverage.

It is disabled for shuffled, date-ranked, numeric, manually ordered, search
result, settings, context, playlist-order, and uncertain lists. Unsupported
scripts retain normal accelerated scrolling. This follows the stock behavior
that fast alphabetical navigation is not available for every language/list.

#### Index behavior

Build a transient GUI-owned table containing the first row for `#`, `A` through
`Z`, using the list's existing item-name callback. The table stores row
indices only. It does not retain tagcache pointers and is discarded when the
list owner, generation, sort mode, or item count changes.

Use Rockbox's existing wheel acceleration signal. Normal wheel motion remains
ordinary row scrolling. When the measured stock entry threshold is crossed:

1. Present the approved stock alphabet overlay.
2. Move through available initials in list order using the physical wheel.
3. Keep the current initial centered in the overlay.
4. Select the first row belonging to that initial.
5. On the measured stock release/deceleration timeout, remove the overlay and
   reveal the selected row with no intermediate blank frame.

Symbols and numbers group under `#` and precede `A`. Case folding must not
change the displayed metadata. Empty initials are skipped. Timing, wheel
threshold, overlay position, opacity, dimensions, and dismissal behavior are
measured from an approved physical iPod reference recording; guessed constants
are not accepted.

#### Visual construction

- The overlay frame/background is a cropped and converted stock-reference BMP.
- The letter uses the approved stock-matching bitmap font or an approved
  reference-derived glyph atlas.
- If exact glyph rasterization cannot be demonstrated, the feature remains
  disabled; a close font, outlined letter, procedural rounded rectangle, or
  hand-built transparency is not permitted.
- Drawing uses opaque/transparent bitmap blits and font rendering only. It
  must not use `drawline`, `drawrect`, `fill_polygon`, or procedural gradients
  to construct the overlay.

#### Phase 5 gate

- Overlay background pixels match the native reference asset exactly.
- Reference glyphs match their golden masks exactly.
- Entry, step, and dismissal timings are within one captured reference frame.
- Every available initial lands on the first matching row; `#` ordering and
  missing letters are correct.
- Ordinary slow scrolling is pixel- and behavior-identical to Phase 4.
- Music continues uninterrupted during repeated fast scrolling, with
  unchanged track path, playlist index/count, pause state, and monotonic
  elapsed time.

### Phase 6: Replace special-case navigation flags with a UI origin stack

Introduce a small iPodJS-only navigation stack containing display state:

```text
screen identifier
origin identifier
selected row
first visible row
database filter/seek identifiers
Cover Flow origin marker
```

Do not store or restore playlist state, codec state, elapsed position, or
album-art ownership in this stack. Existing Rockbox screen return values remain
the external interface.

Short Menu pops exactly one UI level. Playback entered from Cover Flow returns
to Cover Flow. Playback entered from an artist/album/track or Files list
returns to that saved browser. A native handoff preserves the complete source
frame until the destination has produced its first complete frame.

Long-Menu behavior is not changed in this phase. Any future stock long-Menu
mapping is a separate input proposal because Quick Settings currently uses a
long-Menu path and no feature may be silently removed.

#### Phase 6 gate

- All Phase 1 navigation journeys pass for normal, rapid, and repeated Menu.
- No legacy theme, grey gradient, wrong title, or intermediate Home frame is
  captured.
- Starting or returning from While Playing does not call a playback command.
- Playlist identity, index/count, track path, and elapsed progression are
  unchanged across every navigation-only operation.

### Phase 7: Right-size UI-owned preview caches

Reduce only iPodJS-owned static preview storage. Keep two slideshow slots so
panning/crossfade behavior is retained. First evaluate a 240x240 decode bound,
which is sufficient for the 160x240 preview pane while reducing the current
two-slot 320x320 allocation by about 175 KiB.

Compact duplicated preview-source path storage using UI-owned indices or one
active source list. Do not allocate from, borrow, resize, or release:

- the playback or plugin audio buffer;
- core playback buffers;
- the album-art playback slot;
- PCM/mixer memory;
- plugin memory.

Decoding remains rate-limited and may not begin on the same tick as a playback
track transition. Existing slideshow, pan, crossfade, privacy filtering, and
failure retry behavior must remain available.

#### Phase 7 gate

- Hardware ELF reports the expected static-memory reduction.
- Every preview source passes before/after image comparison at pane resolution.
- Panning, crossfade, cache retry, dark mode, and locked-photo filtering pass.
- MP3 and FLAC playback remain uninterrupted while preview sources are
  exercised repeatedly.

### Phase 8: Stock hierarchical slide transitions

Use a single iPodJS transition compositor for forward submenu entry and
backward Menu return. The compositor snapshots display pixels only. It uses
two transient UI-owned core handles, frees them immediately, expires an
unpresented transition after one second, and snaps directly to the destination
if either allocation fails. It must never borrow the playback/plugin buffer,
change an audio handle, or delay the playback thread.

#### Phase 8 gate

- Forward and backward traces contain ten monotonic eased frames from 0 to
  320 pixels and end on the complete destination framebuffer.
- Allocation failure and an expired handoff both produce a complete static
  destination frame.
- Home, Music, database levels, Settings, Quick Settings, context menus, and
  Menu returns never expose a grey or legacy-theme frame.

### Phase 9: Normalize Cover Flow animation timing

Retain PictureFlow's existing art cache, metadata, playlist, and typed return
paths. Normalize cover movement to elapsed Rockbox ticks so speed does not
depend on loop throughput, cap a stalled-frame delta, and advance cover-in and
cover-out keyframes at most once per tick. Keep the stock three-cover side
spacing, parallel side covers, reflection, and center-distance z ordering.

#### Phase 9 gate

- Idle, one-wheel-step, album-track entry, album return, and Home return
  screenshots are complete and use the same artwork/metadata.
- Animation timing remains bounded under fast and slow simulator loops.
- Starting a track and returning to Cover Flow preserves playlist identity and
  does not add any audio, playlist, or shared-buffer call.

### Phase 10: Use exact Apple volume and brightness controls

Extract paMB resources `24282`, `24283`, `24286`, `24287`, `24292`, `24293`,
`24343`, and `30239` from the checksum-approved iPod 13.1.3 IPSW. Conversion
is pixel-format-only: no repainting, tracing, interpolation, resampling, or
procedural replacement is allowed. Variable values clip the approved blue
fill strip inside the approved frame.

For the 200x22 While Playing track, the packaged fill uses only paMB `24343`
blue pixels with paMB `30239` alpha and track pixels. Its 16x16 moving-end cap
is a direct crop precomposited over the matching `30239` track crop so
Rockbox's bitmap loader cannot darken a partially transparent edge. The
operation adds no painted colour, traced mask, resampling, or C-drawn shape;
the output files and hashes are recorded by the private Apple-asset tool.

Quick Settings uses one full-width stock header and one battery. Selecting
Volume or Brightness opens the full stock adjustment screen; Menu first
returns to Quick Settings, then Home.

#### Phase 10 gate

- Every resource ID, dimension, and SHA-256 is verified before output.
- The 32-bit resource alpha plane is retained and the blue fill remains
  visible with rounded, seam-free ends at low and moderate partial values.
- Volume and brightness adjustment change only their existing settings paths;
  navigation and rendering add no playback command.

### Phase 11: Match stock While Playing spacing and title behavior

Position the 128-pixel cover, reflection, metadata, and exact Apple progress
frame against the approved 320x240 Apple guide screenshot. An overlong title
uses the existing Rockbox scroll speed, step, delay, bitmap font, and a bounded
title viewport while avoiding list-selection styling;
artist, album, rating, playlist sequence, progress, and controls remain intact.

#### Phase 11 gate

- The current cover and reflection are intact and match the active track.
- A long title visibly advances while remaining clipped to the title column,
  without a selection-colour bar or stale glyphs.
- Elapsed progress remains monotonic and MP3/FLAC path, index/count, and audio
  state remain unchanged.

### Phase 12: Stock secondary menus and settings handoff

Apply the common iPodJS list/status compositor and Phase 8 transitions to
Settings, nested Rockbox setting lists, and context/secondary menus. Preserve
all existing rows and functions. No new icon or artwork is accepted without a
checksum-approved Apple source; unavailable decorations remain absent rather
than being hand drawn.

#### Phase 12 gate

- Settings -> Sound -> Settings -> Home returns one level per Menu press.
- A short Menu press from a song context menu returns to the same selected
  song row rather than jumping to Home; Hold and unlock preserve the context
  menu and its selection.
- Captures contain a single stock status bar, complete rows, and no half-slide.
- Hold lockscreen, weather, clock, playback indicator, and battery remain
  available from every submenu.

### Phase 13: Stock click-wheel Search

Replace only the root Music `Search` tagtree link with the classic live
click-wheel search. The existing tagtree, playlist construction, WPS, and
playback lifecycle remain authoritative everywhere else.

- wheel motion selects A-Z while the input control is visible;
- Center appends the selected letter, Previous deletes, and Next adds a space;
- results update after each entered character;
- Menu switches between input and results, then returns one level;
- Hold presents the same weather/clock lockscreen and returns to the same
  query/results state;
- song selection uses the existing database-play path, artist and album
  selection use the existing filtered browsers, and playlist selection opens
  the existing playlist viewer;
- podcast and audiobook tracks remain searchable through their title metadata
  and use the normal audio-track lifecycle.

Result glyphs are direct checksum-verified Apple paMB resources: `30240`
(song), `30242` (artist), `30244` (album), and `30248` (playlist). The result
model copies bounded strings and numeric seeks; it retains no tagcache result
pointer. Merely entering, typing, drawing, or leaving Search may not call an
audio or playlist mutator. A missing verified result asset leaves the original
Rockbox Search submenu available instead of drawing a substitute icon.

The input control is also entirely Apple-derived. Its outer panel is paMB
`24413` (95x82), the graphite query field is paMB `22203` (97x32), and the
blue selected-letter field is paMB `22206` (97x32), all from the
checksum-approved iPod 13.1.3 IPSW. The renderer uses bitmap-part tiling of
the original edge and centre pixels to accommodate the live query and letter
strip; it does not construct the panel with rounded rectangles, gradients,
polygons, or guessed colours. Stock Search is asset-gated as one unit: if any
panel, field, selection, font, or result-icon asset is unavailable, the
original Rockbox Search remains available.

#### Phase 13 gate

- a short A-Z fixture returns artist, album, song, and playlist results for a
  live `X` query;
- Hold enters and leaves the lockscreen without losing the query;
- selecting the song reaches the playing iPodJS WPS with the expected path;
- source guards prove the result builder and renderer contain no audio,
  playlist, PCM, mixer, or shared-buffer mutation;
- the four Search icons and three input-control surfaces match the verified
  source/output hashes and are packaged only by the private Apple-asset
  preparation route;
- captured empty, live-result, Hold, result-list, and playback paths contain
  no procedural substitute or legacy Search frame.

## Playback-Safety Enforcement

### Source and build guards

For every phase:

1. Reject changes under codec, DSP, PCM, mixer, pcmbuf, playback, target audio,
   and plugin-audio paths.
2. Reject added playback-mutating calls in the patch.
3. Reject new shared-buffer or playback album-art ownership calls. The only
   permitted core allocations are the two bounded, fail-safe, immediately
   freed display snapshots specified by Phase 8.
4. Compare codec binaries and playback-sensitive object checksums with the
   baseline build; they must be byte-identical.
5. Build both the iPod 6G simulator and hardware firmware.
6. Run `git diff --check` and the iPodJS static safety tests.

Expected firmware checksum changes are caused by UI code/assets. Codec output
and unchanged playback object files must not change.

### Simulator playback matrix

Run each scenario once from Database playback and once from Files playback:

- idle While Playing for ten minutes;
- continuous wheel movement through long Artists and Albums lists;
- repeated entry/exit between list and While Playing;
- Cover Flow -> While Playing -> Cover Flow;
- pause/resume and volume before and after UI stress;
- Hold in every submenu;
- rapid Menu presses and medium/long Play holds.

Assert throughout:

- same track path unless the test explicitly skips;
- same playlist index/count and playlist control-file identity;
- no spontaneous pause, stop, resume, or codec restart;
- elapsed time is monotonic within the normal metadata update tolerance;
- no audio-buffer ownership event or underrun is logged.

### Physical iPod gate

No phase reaches the mounted device until its simulator gate passes. On the
iPod 6G, play one MP3 and one FLAC continuously while exercising the phase's UI
behavior. Listen for gaps and verify track, pause state, index/count, and
elapsed progression before and after the run.

Deployment must use `tools/deploy_ipod6g_preserve_database.sh`. Both firmware
copies must match the local build checksum, all tagcache files must remain
byte-identical, and `tagcache_autoupdate` must remain enabled.

Any playback interruption, state change, playlist change, database change,
buffer ownership change, or unexplained codec restart is a hard stop. The
phase is reverted rather than worked around in audio code.

## Completion Criteria

The project is complete only when:

- WPS and lists use bounded dirty updates with no stale pixels;
- every iPodJS screen uses the single approved stock status compositor;
- stock alphabet navigation is reference-identical in pixels, glyphs,
  interaction, and timing;
- navigation returns to the exact originating screen without legacy frames;
- UI-owned preview memory is reduced without removing preview behavior;
- hierarchical slides always settle on a complete destination frame;
- Cover Flow timing is tick-normalized and preserves typed return;
- volume, brightness, and progress use only checksum-approved Apple assets;
- While Playing long titles scroll inside the stock metadata column;
- Settings and context paths retain every feature and return one level at a
  time;
- all simulator and hardware playback-safety matrices pass;
- no playback, playlist, codec, PCM, mixer, shared-buffer, album-art ownership,
  or database behavior changed.
