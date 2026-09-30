# Cover Flow album opening and persistent artwork

Status: implementation in progress, 2026-09-12. The device-side flip, scene
readiness gate, settled metadata preparation, and persistent track cache are
implemented. See `docs/coverflow-album-implementation.md` for evidence and open
acceptance items. Hardware performance targets below remain unverified.

## Product contract

Pressing Center on an album must reproduce RetailOS Classic's album-opening
presentation, including the cover flip, destination track-list appearance,
and reverse motion. Menu returns to the same cover and browsing position. Artwork
must remain visible throughout browsing, opening, returning, and slow storage.
Improving speed must never introduce placeholder flashes, blank slots, covers
changing identity, or visible low-resolution-to-high-resolution replacement.

Priority order: correct persistent covers, uninterrupted music, responsive
controls, fast readiness, then animation. “Instant” is a warm-path target, not
a promise that an unprepared library or sleeping disk needs no reads.

Target: iPod Classic 6G/7G, 320×240. Keep existing 5G functionality and qualify
its timing separately. This is a PictureFlow feature, not a replacement Music
browser or WPS. Opening an album does not change playback; selecting a track
uses the existing explicit playback operation.

Apple's user guide documents wheel browsing, Center to open an album, then
wheel/Center to select and play a song. It does not establish Apple's cache
implementation or animation timing. RetailOS visual fidelity is a requirement,
not a description of the current PictureFlow rendering. Numeric performance
budgets below are proposed engineering goals, not measured Apple timings.

Reference: [Apple iPod classic User Guide](https://cdsassets.apple.com/live/6GJYWVAV/user/ma1195_ipod_classic_160gb_user_guide.pdf).

## Current source findings

Primary implementation: `apps/plugins/pictureflow/pictureflow.c`.
The findings below describe the pre-implementation baseline; the companion
implementation report records which paths have changed.

| Existing path | Finding and implication |
| --- | --- |
| `update_cover_in_animation()` / `update_cover_out_animation()` | Zoom/rotation already exists, driven by increments per rendered tick. Delayed frames extend duration; replace with elapsed-time interpolation. |
| `pf_cover_in` in the event loop | Calls `create_track_index()` during animation for SSD/RAM-tagcache configurations. Synchronous metadata work can interrupt movement. |
| `show_track_list()` | May build the track index on first draw and display a waiting screen. Separate readiness/service from rendering. |
| `create_track_index()` | Enumerates titles and retrieves filenames under the shared buffer mutex. Prepare ahead of Select and avoid holding a rendering lock across storage. |
| `surface()` | Returns `empty_slide_hid` on a residency miss. This conflates temporary absence with genuinely missing artwork. |
| `read_pfraw()` / `load_new_slide()` | Reads prepared raw images, evicting by distance from center; 200 cache records do not mean 200 images fit in RAM. |
| Background `thread()` | Holds the buffer mutex while loading a slide. Storage waits may block other users of that mutex. |
| Initialization | Loads a saved album index, but can rebuild album/art caches before browsing. Warm launch and first preparation need separate paths and measurements. |
| Memory layout | Playback-capable builds use the plugin buffer, reserve artwork workspace and a 16–256 KiB track arena, then initialize the slide pool. Preserve that ownership model. |

These are reachable mechanisms, not hardware profiling results. Measure their
actual contribution before reporting a cause or speedup. RockPod also has
`invalidate_pictureflow_cache()` callers; inspect their invalidation scope
before implementing incremental sync.

## Visual behavior

Default effect: RetailOS Classic album opening, with a direct-draw Reduced
Motion option. The earlier proposed shrinking 64×64 cover/header layout is
withdrawn: it is not the requested retail appearance. Do not introduce a custom
slide-up track pane, persistent miniature cover, bounce, or arbitrary fade.

First establish a reference sheet from the applicable Classic RetailOS version,
using existing lawful firmware/resources and real-device footage available in
the workspace. Record source identity, firmware version, timestamps, and crop
coordinates. Inspect opening and closing separately. The shell-menu timing in
`docs/ipodjs-stock-6g-animation-spec.md` does not measure the album flip and must
not be reused as evidence for it. iPhone/iPod touch Cover Flow is not the Classic
reference. Existing extracted resources alone do not prove their placement.

Before implementing the visual change, document:

- Resting center/side cover geometry, perspective, spacing, reflection extent,
  background, album/artist text, and status-bar behavior.
- Flip axis, direction, pivot, projected width/height and position over time;
  when the cover face gives way to the track-list face; neighboring-cover and
  reflection behavior throughout that change.
- Final track-list bounds, background, title/artist placement, font metrics,
  row spacing, selection highlight, scrolling, and any visible artwork.
- Actual opening/closing durations and sampled positions, with uncertainty
  from camera frame rate and LCD response explicitly stated.

Produce matched source/destination screenshots and a frame strip of both
directions. Unknown geometry/timing remains an explicit reference gap; do not
label guessed coordinates or the existing zoom/rotation as RetailOS accurate.
Repair resting Cover Flow and the destination layout as part of this feature
where reference comparison shows differences, not just the moving frames.

Use the existing selected cover pixels for the front face and cached text/assets
for the destination face. The reference determines what is visible at each
point. A cover turning edge-on or being intentionally replaced by its track-list
face is valid; an artwork cache miss or placeholder substitution is not.

Menu reproduces the measured return motion and restores the exact center
album, side covers, scroll position, and selection. The returning neighborhood
remains pinned for the entire album view. Returning from WPS after the plugin
was unloaded follows the warm-launch readiness gate; plugin pointers cannot
survive unloading.

Use elapsed-tick fixed-point interpolation of the measured trajectory, with a
presentation cap justified by the album-flip reference and physical LCD timing.
Do not assume smoothstep describes RetailOS before comparing sampled positions.
Skip intermediate samples when late and always draw the final frame. A missed
frame must not extend the motion or change the final geometry.

## Readiness and input state machine

`BROWSE_READY -> OPEN_PENDING -> OPENING -> TRACKS_READY -> CLOSING -> BROWSE_READY`

`OPEN_PENDING` is normally bypassed because the selected album was prefetched.
It retains the complete cover scene while preparing missing metadata. After
150 ms, show a small “Opening album…” text indicator in reserved footer space;
do not blank or replace artwork. At a proposed 2-second deadline, end the open
request with a dismissible error and keep the browser usable. A late result
must not open the album after cancellation.

Start OPENING only when the cover, first track page, retail destination, and return scene
are ready. All animation rendering is I/O-free. Menu during OPEN_PENDING cancels;
Menu during OPENING reverses continuously from the current position. Coalesce
wheel movement while opening into the destination track selection only after
the opening gesture is consumed. Never interpret the original Center release
as a track-play command. Repeated Center during opening must not auto-play.
Menu during CLOSING completes the close, then follows ordinary one-level Menu
semantics for a distinct subsequent press. Do not infer intent from raw queue
length. Hold, USB, power-off, and existing hibernate events stay observable.

Every work request carries an album key, cache generation, and screen/request
generation. Drop stale completions before publishing. Preserve existing
PictureFlow context actions, auto-WPS preference, and hibernate contracts.

## Artwork residency: no pop-in

Separate four states: RESIDENT, PREPARED_ON_DISK, SOURCE_MISSING, and ERROR.
Only SOURCE_MISSING may use the permanent no-art tile. A pending disk read or
bad cache file must never be represented as a missing cover. Real absent art
cannot be invented: show one stable album-labeled tile, and report it in sync.

Before presenting a scene, pin all covers that it exposes. Before moving to a
new scene, pin the union of source and destination covers, including those
crossing the screen edge. Evict only unpinned images. Renderers acquire stable
handles under a short lock and must not retain unprotected pointers across
buflib relocation. Publish completed reads atomically; never fill a surface
that a renderer can already see.

Prefetch order: next required scene, current album track page, directional
lookahead, opposite-direction reversal margin, then optional distant content.
Service one bounded unit at a time after an initial 80 ms navigation settle
interval with no queued user input; visible-scene requests get priority at the
next eligible service point. A worker reads into private bounded staging
storage outside the rendering lock, then briefly locks to validate and publish.
Check input/cancellation between bounded read chunks. Chunking does not bound
a physical disk command; measure worst-case storage latency explicitly.

If scrolling outruns storage, hold the last fully populated scene and accumulate
the requested destination. A distant jump prepares its entire destination
before presenting it; it need not animate every skipped album. Cap travel speed
to ready scenes. This is the explicit tradeoff that preserves the user's
no-pop-in requirement on arbitrary library sizes. Labels must continue to
identify the displayed scene, not an unseen pending album.

No visible resolution promotion. The initial implementation uses the existing
prepared quality for every visible cover. A later multi-size format may choose
a representation before exposure, but cannot upgrade it while visible.

## Faster launch and album data

Separate three timings: plugin launch to first complete scene, Center to first
motion, and Center to selectable track list.

On an unchanged prepared library, read the compact album index and only the
initial scene's prepared surfaces before display. Restore the last album by
stable identity rather than a fragile numeric position. Do not scan the entire
library, decode original artwork, or verify every file at entry. Subsequent
scenes are fetched on demand and in idle time.

Prepare the current album's ordered track metadata while its cover is settled,
then one neighboring album if memory permits. Cache ordered titles and stable
track identifiers/paths; retain disc/track ordering and compilation grouping.
The first page should already be ready when Center is pressed. Do not move the
same synchronous full-album scan from one animation frame into another.

Persist per-album track records alongside artwork, with explicit format version,
lengths, bounds, generation, and checksum. Never serialize C pointers or reuse
tagcache seek offsets across database generations. For oversized albums, page
records from disk into bounded slots; retain the previous complete page while
the next loads. Explicit playback may stream the full ordered path list through
the normal playlist operation rather than silently truncating the album.

RockPod should prepare artwork and metadata during sync where practical. Match
device grouping exactly, including same-title albums by different artists and
multi-disc compilations. Host metadata is not permission to replace Rockbox's
database. Validate path correspondence and database generation before using it.
Device-only imports need an incremental preparation path that yields to input
and tagcache commits. Until an initial scene is complete, retain the entry
screen with preparation progress; never display an incomplete Cover Flow.

Proposed cache evolution: a versioned manifest plus indexed raw artwork and
track-data packs, reducing repeated FAT directory lookup/open overhead. Use
explicit byte order, fixed-width fields, offset/length overflow checks, stable
album keys with collision verification, and per-record integrity checks.
Benchmark against existing `.pfraw` before requiring a pack migration.

Publish generation files first, validate them, then atomically replace a small
manifest on the same volume; retain the prior generation until the next clean
open. Interrupted sync must leave one usable generation. Update only changed
albums; a bad image requests repair of that image, not a whole-library rebuild.
Do not publish records that reference a mixed database generation. Unchanged
old records may be reused only after identity validation. A live browser keeps
its pinned generation until a safe idle handoff or next entry.

## Memory and playback budget

No new core allocations, shared audio-buffer acquisition, PCM changes, codec
restarts, dynamic font loads, or late WPS artwork-slot resizing for this work.
Use the existing plugin arena. Prefer reuse of artwork preparation workspace
for staging only after documenting nonoverlapping decode/read lifetimes.

| Proposed bounded resource | Maximum payload |
| --- | ---: |
| Compositor strip, 320×16 RGB565 | 10,240 bytes |
| Transition/request/pin bookkeeping budget | 4,096 bytes |
| Read staging chunk | 16,384 bytes |
| Additional bounded scratch ceiling | 30,720 bytes |
| Track metadata cache | Repartition existing arena, at most 262,144 bytes total |
| New full-screen copies | 0 bytes |

These are implementation ceilings, not measured available headroom. Include
alignment and allocator metadata in the final accounting. Pinned covers are
existing slide allocations, but prevent their eviction: account for the union
of scenes, not just the centered image. A 160×160 RGB565 surface costs 51,200
pixel bytes; nine such surfaces cost 460,800 bytes before headers. Actual
`.pfraw` dimensions determine the real budget; do not assume that example is
the current format. Compute readiness capacity using actual dimensions and
configured slide count/zoom. Reject unsupported visual settings gracefully.

If memory cannot hold the required visible scenes, shorten travel to a direct
complete-scene handoff and reuse the source allocations only after its last
frame. Never evict a cover that is still on screen. If the configured scene
itself cannot fit, retain the prior valid configuration and explain the limit.
Animation scratch exhaustion falls back to a fully prepared direct album view.

Audit native ELF text/data/BSS, plugin arena free space, mutex hold times, and
ARM stack usage before hardware testing. A simulator allocation success is not
evidence of native capacity.

## Measurement and acceptance

Proposed goals, to be validated on the user's physical storage:

| Prepared/warm operation | Target |
| --- | --- |
| Center acknowledgement / first motion | p95 ≤50 ms |
| Complete selectable album view | p95 ≤300 ms including animation |
| Menu back to complete Cover Flow | p95 ≤250 ms |
| Warm plugin entry to complete first scene | p95 ≤500 ms |
| Artwork misses exposed on screen | Zero |
| Image reads/decodes or tagcache work in draw/animation paths | Zero |

If measured RetailOS motion exceeds an end-to-end target above, preserve its
duration and judge responsiveness by time to first motion and avoidable waiting.
Do not speed up the stock effect merely to meet an invented timing budget.

Record cold boot, warm re-entry, cold disk, and first-time cache preparation
separately. Report sample counts, median, p95, worst case, CPU boost state,
storage type, album/track counts, arena usage, and playback state. At least 30
samples per timed warm case. Log to a bounded RAM trace and flush after the
interaction so logging does not manufacture the stalls being measured.

Trace input, request identity, cache hit/miss, read start/end, lock hold time,
first/final frame, readiness, pin counts, and visible misses. Compare unchanged
baseline and candidate with the same library; report measured results even
when a target is missed.

Qualification must include:

- Libraries of 10, 100, 1,000+ albums; small memory budgets; extreme supported
  cover dimensions; a large multi-disc album; long/non-ASCII titles; duplicate
  album titles; missing art; damaged/truncated records; interrupted sync.
- Slow-read injection, failed reads, reversals, distant jumps, repeated opens,
  rapid Menu, Center during motion, Hold, USB, and hibernate/re-entry.
- Pixel/identity checks on every captured frame: no known-art cover becomes
  a placeholder, no wrong-album image, no blank intermediate frame, and no
  status-bar/WPS/theme flash. Intentional shrinking/fading is not a cache miss.
- Matched RetailOS comparisons for resting browse, opening keyframes, final
  track list, selection movement, and return. Document tolerances before visual
  acceptance, separating camera/LCD artifacts from actual renderer differences.
  Persistent artwork and speed alone cannot pass the RetailOS appearance gate.
- Active Database and Files playback through 100 open/close cycles, 10 full
  Music hierarchy cycles, and 20 rapid Albums/Artists switches. Track and
  playlist identity remain unchanged; elapsed position never resets; file
  descriptors and memory return to baseline without a downward trend.
- Existing focused Cover Flow/menu asset gate plus
  `tools/ipodjs_navigation_sim_regression.sh`; simulator and native 6G builds,
  5G compatibility build, and generated stack/ELF review.
- Hardware: repeated browsing, fast scrolling, album open/return and reopening
  Music without reboot; audible playback stays uninterrupted. Simulator timing
  alone does not meet performance acceptance.

## Implementation sequence

1. Establish the RetailOS visual reference and difference inventory; instrument
   and capture the current baseline; audit actual artwork sizes and arena use.
2. Introduce explicit residency states, scene pinning, transactional publication,
   and ready-scene scrolling. Prove zero exposed misses under slow storage.
3. Separate track preparation from rendering, add settled-album prefetch and
   bounded persistent records, and measure opening/entry improvement.
4. Correct browse/track-list appearance and implement the measured RetailOS
   flip trajectory and return, driven by elapsed time and cached surfaces.
5. Add incremental RockPod preparation and evaluate indexed packs against the
   existing files; migrate only if measurements justify it.
6. Complete simulator/native qualification and physical validation of the exact
   build before declaring the speed and continuity goals achieved.

Before code edits, inspect all paths required by
`docs/ipodjs-ui-memory-animation-steering.md` and
`docs/plugin-audio-lifecycle-steering.md`, including core/plugin teardown, WPS,
tagcache, and root-menu ownership. Preserve current hibernate tests. This spec
authorizes no physical deployment; any later deploy follows repository rules.
