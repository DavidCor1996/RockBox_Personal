# iPodJS UI Memory And Animation Steering

This document is mandatory for iPodJS UI, artwork, transition, tagtree
decoration, framebuffer, and animation work. It records the failure pattern
behind hardware-only `Database is not ready` faults and the constraints that
keep visual work from interfering with music playback.

## Failure Pattern To Avoid

The iPodJS navigation path previously combined three kinds of work at screen
handoff time:

- tagtree hierarchy teardown and reload;
- album-art path lookup plus bitmap decode;
- full-screen transition allocation and composition.

Rapid Menu presses could make each abandoned hierarchy level perform optional
artwork work before processing the next input. Full-screen transition code also
requested two `FRAMEBUFFER_SIZE` core allocations. When playback owned most of
the remaining core arena, those allocations could invoke playback's shrink
callback. The simulator's host memory and filesystem timing did not reproduce
the same pressure as native iPod 6G hardware.

This is a lifecycle/resource-contention failure class. Do not treat a
`Database is not ready` screen as proof that the database files are corrupt,
and do not add rebuild/retry loops before auditing the work performed during
the transition.

## Non-Negotiable Invariants

### Playback memory is not UI scratch memory

- Do not use `core_alloc(FRAMEBUFFER_SIZE)` or another large transient core
  allocation for decorative navigation while playback can be active.
- UI allocation must not invoke an audio-buffer shrink, stop, resume, or
  restart callback.
- UI code must not call `audio_stop()`, change PCM/mixer state, take the plugin
  audio buffer, or mutate a playlist to make an effect fit.
- A visual effect must fail closed to a direct, fully drawn destination frame
  when its bounded workspace is unavailable.

### Draw paths paint cached pixels only

A function called from a screen draw, list-row callback, dirty-rectangle
update, or transition compositor must not:

- open, close, scan, stat, or enumerate files;
- call `read_bmp_file()` or resize/dither an image;
- start or advance a tagcache search;
- allocate a full-screen or per-row transient buffer;
- spin storage merely because a decorative frame is due.

Split optional visuals into two APIs:

1. a cached draw function that cannot perform I/O or allocation;
2. a bounded service function that loads at most one missing unit of work.

### Optional work yields to navigation and tagcache

Artwork service points must require all applicable conditions:

- the input queue is empty;
- the current screen has been idle for a documented settle interval;
- Hold/power-off handling is inactive;
- tagcache is usable and not committing when the work depends on Music data;
- no stale row from an abandoned hierarchy can be serviced.

Pending list artwork must be discarded when list ownership, directory level,
or backing tree context changes.

Rapid Menu presses must follow Rockbox's ordinary one-level-at-a-time browser
unwind. Do not infer intent from `button_queue_count()`: the queue includes raw
press/release and unrelated Select or wheel events, so treating a count as a
Menu storm can incorrectly jump Home. Do not clear the saved WPS origin or
rewrite tagtree history in response to queued input. Transitions may discard
releases received while animation owns input; once settled, the next Menu must
continue from the intact browser state. The one-shot source-list cache remains
valid across WPS and must not be rejected using a raw CRC containing pointers
that buflib legitimately relocates while the codec starts.

The iPodJS WPS must not enter the current-playlist viewer. A short center-button
press is consumed without leaving Now Playing; a center-button hold launches
Lyrics directly. Suppress `ACTION_WPS_VIEW_PLAYLIST` as a defensive boundary,
so no skin or alternate mapping can bypass that behavior. Playlist list-row
formatting elsewhere may copy the current track or tagcache RAM-cache metadata,
but must never fall back to opening and parsing each track file under iPodJS.
If cached metadata is unavailable, display the filename; explicit Track Info
remains an interactive operation and may read the selected file.

On iPod 6G, the stock Rockbox WPS skin/action loop is the sole music-screen
owner. Do not load the WPS skin and then bypass it with a second renderer or a
separate `get_action()` loop. The skin engine claims and sizes album-art slots
before playback; dashboard and preview code may reuse a completed existing WPS
handle, but must not call `playback_claim_aa_slot()` followed by
`playback_update_aa_dims()` after a track starts. That update synchronously
halts decoding, clears the buffered track list, remakes the audio buffer, and
restarts playback. A decorative cover request must never cause that lifecycle.

Hardware keeps `tagcache_autoupdate` enabled. A foreground database open must
not wait without bound for a background update scan: pause the scan (which is
still writing only its temporary file), recover the startup-validated live
database, and schedule autoupdate to resume after the foreground interaction.
Discard only the paused scan's partial temporary file before resuming it. The
loading state must retain a hard deadline even while a scan is active.

Never issue an ATA/storage reset from UI database recovery. Playback, codecs,
plugins, artwork and tagcache share that path; a local search failure must not
turn into device-wide file-open failure.

### Bound memory explicitly

- Prefer a fixed, target-scoped workspace with a compile-time size over
  unpredictable core-arena pressure.
- Reuse storage with a union when lifetimes provably do not overlap.
- State the byte cost of new caches in the spec or code review.
- For iPod 6G, inspect `rockbox.elf` text/data/BSS before and after the change.
- Do not add multiple full-screen copies without explaining why a smaller
  pane, row, strip, or existing cache cannot satisfy the effect.
- Keep ARM UI call chains comfortably inside the main-thread stack. Inspect
  generated prologues or `.su` data for functions that decode, search, or
  compose images.

### Bound cadence and input latency

- Use elapsed ticks to derive animation position; never assume every frame is
  delivered.
- Cap navigation and preview effects near the measured stock cadence. More
  frames are not automatically smoother on the 6G LCD.
- Do not busy-wait. Sleep/yield between scheduled frames.
- If input is already queued, finish or cancel a decorative effect without
  decoding or allocating more work.
- Keep Hold, USB, shutdown, and system events observable during long-running
  non-navigation animations.

## Required Review Before Editing

Inspect these paths together:

- `apps/gui/ipodjs_ui.c` and `apps/gui/ipodjs_ui.h`
- `apps/root_menu.c`
- `apps/tree.c`
- `apps/tagtree.c` and `apps/tagcache.c`
- `apps/gui/albumlist_art.c`
- `apps/gui/statusbar-skinned.c` for the working iPone full-art lifecycle
- `firmware/core_alloc.c` and playback's core-allocation callbacks when a core
  allocation is being considered

For playback-adjacent changes, also follow
`docs/plugin-audio-lifecycle-steering.md`.

## Required Simulator Evidence

An iPodJS navigation/artwork change is not ready for hardware until all of the
following are true:

- simulator and native iPod 6G builds succeed;
- active playback remains active and its playlist identity/elapsed time do not
  regress across transitions;
- at least ten full-depth Music hierarchy enter/exit cycles pass;
- at least twenty rapid Albums/Artists switches pass;
- rapid Menu-to-Home presses emit no `Loading Music` or database failure;
- process file-descriptor count returns to baseline after every cycle;
- core available/allocatable memory does not trend downward across cycles;
- transition traces contain complete monotonic frames at the specified timing;
- no WPS, Rockbox theme, database, or stale preview frame flashes at handoff;
- cached draw paths are reviewed to prove they contain no I/O or decode call.

Run the closest focused gate plus
`tools/ipodjs_navigation_sim_regression.sh`. A visual screenshot alone is not
memory or playback evidence.

## Hardware Gate

Hardware remains authoritative for storage timing and memory pressure. Test:

1. start a database track and leave it playing;
2. enter Albums, scroll, return, enter Artists, and return;
3. rapidly press Menu through every hierarchy level to Home;
4. repeat without rebooting;
5. confirm Music immediately reopens and playback never stops or restarts.

If it fails only on hardware, preserve the database and collect resource state
before attempting recovery. Do not deploy a speculative rebuild loop as the
next fix.
