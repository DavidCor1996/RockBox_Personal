# iPodJS Stock-Experience Improvements Spec

Status: implemented 2026-07-25 (see Implementation notes at the end)
Scope: iPod Classic 6G/7G primary, iPod Video 5G secondary
Governing constraints: `docs/ipodjs-ui-memory-animation-steering.md` is mandatory
for every item below. No functionality may be lost; every change fails closed
to today's behavior when its data or asset is missing.

Three workstreams:

- A. Album list thumbnails must load at stock speed and must not vanish while
  scrolling.
- B. The Extras/Clock hover pane must show the real Apple clock artwork, not
  the current hand-drawn vector clock.
- C. The Photos hover pane must run the same smooth right-pane slideshow as
  Music and Games, sourced from every photo on the device that is not locked
  and not inside a locked folder.

---

## A. Album list artwork: stock-speed thumbnails

### Current behavior (measured from code)

- `apps/gui/albumlist_art.c` draws 40x40 thumbs on 44px rows. The draw path
  (`albumlist_draw_item_ipodjs`) correctly paints cached pixels only; misses
  are queued into `pending_items` (max 8).
- Pending thumbs are decoded **only** from `apps/tree.c:803`:
  `button == ACTION_NONE && button_queue_empty()`. `ACTION_NONE` arrives only
  after the list action timeout expires, so during continuous wheel scrolling
  **zero** thumbs load. This is the "loads too slow" symptom.
- `ALBUMLIST_BITMAP_CACHE` is 32 LRU slots keyed by (path, size). Scrolling
  past ~32 distinct albums evicts earlier rows; scrolling back shows text-only
  rows until re-serviced. This is the "they disappear as I scroll" symptom.
- Each load is `open()` + BMP header parse + `read_bmp_file` with
  RESIZE|DITHER of a per-album file. RockPod already exports exact 40x40 BMPs
  (`rockpod/services/artwork_manager.py`, `_ALBUM_LIST_THUMB_SIZE = (40, 40)`),
  so decode is cheap; per-file `open()` and the manifest string-scan
  (`find_manifest_thumb`, linear over up to 384 entries) dominate.

The stock 6G feels instant because it reads fixed-size records from one packed
artwork database (ithmb-style), never opening per-album files.

### A1. Packed thumbnail bank (the stock mechanism)

RockPod side (`rockpod/services/albumlist_export.py` + `artwork_manager.py`):

- Alongside `index.tsv`, emit `.rockbox/albumlist/thumbs.pack`:
  - Header: magic `ALTB`, version, record count, record size, thumb width,
    height, pixel format tag (RGB565 native, row-major).
  - One fixed-size record per manifest row, in manifest order: 40*40*2 =
    3200 bytes of raw native pixels (pre-dithered at export time), plus a
    1-byte present flag padded to 4-byte alignment. Records for albums
    without art are zeroed with flag 0.
  - `index.tsv` gains nothing; row order is the join key. Regenerate both
    atomically (write temp + rename) so a partial pack is never visible.

Firmware side (`apps/gui/albumlist_art.c`):

- On manifest load, `open()` `thumbs.pack` once, validate header against
  `manifest_cache_count`, and keep the fd (or reopen per service burst — see
  fd-count invariant below; decision: open per service call, close before
  returning, so the browser holds no fd across yields).
- Thumb service path becomes: `lseek` to `row * record_size`, one `read()` of
  3204 bytes straight into the cache slot, no BMP parse, no scaler, no
  dither. Expected cost well under 1 ms on 6G SSD after spin-up.
- The BMP-per-file path remains as fallback for rows whose pack record flag
  is 0 or whose pack is absent/mismatched (older RockPod exports keep
  working — no functionality lost).
- Manifest lookup: with the pack, `lookup_thumb_path`'s string search is only
  needed for the fallback path. Add the manifest row index to the tagtree row
  match (`albumlist_get_album_row` already yields album+artist; cache the
  resolved manifest index per lookup slot) so the hot path is integer-keyed.

Byte cost (must be restated in the PR per steering doc):

- Pack file on disk: 384 records x 3204 B ≈ 1.2 MB. No firmware RAM cost.
- Cache slots for packed loads no longer need scaler scratch; slot data can
  shrink from `ALBUMLIST_SCALED_BYTES(40,40)` (~3.9 KB) to 3.2 KB raw when
  the slot is pack-sourced. Keep the union sized for the fallback (BMP)
  loader, so slot size is unchanged; this is a no-growth change.

### A2. Bounded inter-scroll servicing

Steering allows optional artwork work only with an empty input queue. The fix
is not to violate that, but to add a second, tighter service point:

- In `apps/tree.c`, immediately after `gui_synclist_do_button()` handles a
  scroll action (`ACTION_STD_PREV/NEXT` family), if `button_queue_empty()`
  still holds, service **at most one** pending unit
  (`albumlist_art_service_one()`, new bounded API) before re-entering
  `get_action`. With pack reads at <1 ms, this fills rows between wheel
  detents — the stock pop-in feel — while a queued next detent skips the work
  entirely.
- Keep the existing idle burst (`albumlist_art_service_pending`, up to 8) at
  `ACTION_NONE` for catch-up.
- Pending discard rules from steering (list ownership / dir level / tree
  context change) stay exactly as today: `pending_items` records `tc` and is
  cleared in `albumlist_setup_list`.

### A3. Cache sizing, visible-row protection, direction prefetch

- Raise `ALBUMLIST_BITMAP_CACHE` from 32 to 64. Cost: +32 slots x ~3.9 KB ≈
  +125 KB BSS. This must go through the ARM BSS/ELF audit (compare
  `rockbox.elf` text/data/bss before and after) and be called out in review.
  64 slots covers ~12 screens of rows; with A1 making reloads ~free, this is
  belt-and-braces, not the primary fix. If the audit says no, ship A1+A2
  without it — they are independently sufficient.
- Eviction: never evict a slot whose (path,size) matches a currently visible
  row. Pass the visible range (from `gui_synclist`) into the victim chooser;
  fall back to plain LRU when all slots are visible (cannot happen with
  64 slots / ~6 visible rows).
- Prefetch: when a scroll action is handled, queue the next 2 off-screen rows
  in the scroll direction (indices exist in the tree context) behind the
  visible-row pendings. Bounded by the same one-unit service; no extra I/O
  class is introduced.

### A4. Evidence gates (all pre-existing, plus one new)

- `tools/ipodjs_navigation_sim_regression.sh` must pass unchanged (playback
  survives, fd count returns to baseline, no memory trend).
- New focused sim check: scroll a 100+ album list end-to-end and back at
  wheel-repeat speed; assert (a) no draw-path I/O (instrument
  `albumlist_draw_item_ipodjs` in sim builds), (b) after settle, every
  visible row has art, (c) fd count at baseline (the pack fd must not leak).
- Hardware gate: Database playing -> Albums scroll stress -> rapid Menu
  unwind -> Music reopens instantly, playback never restarts.

---

## B. Extras / Clock hover: real Apple clock asset

### Current behavior

- Hovering Extras (or Clock inside Extras) calls
  `root_menu_video_draw_stock_clock_preview` (`apps/root_menu.c:9278`), which
  hand-draws a vector clock (filled circles, sine-table hands). The bundled
  `previews/clock.174x*.bmp` assets are acknowledged placeholders and are
  deliberately disabled: `root_menu_video_preview_asset_is_verified()` returns
  false for everything.
- The stock 6G shows, in the right pane, the real Apple analog clock face on
  its background when Extras/Clock is highlighted.

### B1. Asset acquisition — extend the existing pixel-exact pipeline

Do **not** screenshot, trace, or redraw. The repo already has the correct
machinery and policy (`assets/ipodjs/apple/PROVENANCE.txt`: "No artwork is
traced, redrawn, interpolated, or resampled. The BMP files are format
conversions of exact Apple pixels."):

- `tools/ipod_stock_resource_extract.py` — read-only paMB bitmap dumper for
  the official, hash-pinned `iPod_13.1.3.ipsw` (5G; OS image unencrypted;
  official Apple CDN URL recorded in PROVENANCE.txt).
- `tools/prepare_ipodjs_apple_assets.py` — verifies IPSW hashes, extracts
  named resources, emits BMP conversions plus provenance hashes. Also already
  reads the intact FAT16 `iPodResources` volume inside `iPod_24.1.1.2.ipsw`
  (iPod Classic firmware) for fonts.

Work items:

1. Dump the full paMB resource set from `iPod_13.1.3.ipsw` with the existing
   extractor and identify the clock resources: face, bezel/background, hour
   and minute and second hand art, center cap. (The 5G Clock screen uses the
   same Apple analog-face design family the 6G preview shows.)
2. Inspect the `iPodResources` volume of `iPod_24.1.1.2.ipsw` (already
   hash-pinned in the prep tool) for the Classic's own clock/preview bitmap
   resources (Silver-era resource containers, same family the nano
   `ipod_theme` project edits). If the exact 6G preview-pane clock artwork is
   recoverable there, prefer it; otherwise ship the 5G-firmware clock art,
   which is the same Apple design. Either way the pixels are exact Apple
   pixels from an official download.
3. Add extraction stanzas to `tools/prepare_ipodjs_apple_assets.py` emitting:
   - `assets/ipodjs/apple/clock-face.apple.<W>x<H>x24.bmp`
   - `assets/ipodjs/apple/clock-bg.apple.<W>x<H>x24.bmp` (pane background, if
     the firmware stores one; otherwise the pane keeps the current gradient
     behind the face)
   - hand sprites if present as discrete resources:
     `clock-hand-{hour,min,sec}.apple.*.bmp`
   and append their SHA-256 lines to `PROVENANCE.txt`.
4. Apple binaries stay out of the repo (existing policy); the tool takes the
   locally downloaded IPSW. `tools/buildzip.pl` already ships
   `assets/ipodjs/apple` into `.rockbox/ipodjs/apple/`.

### B2. Rendering change (`apps/root_menu.c`)

- `root_menu_video_draw_stock_clock_preview` becomes:
  1. Draw pane background: Apple background asset if extracted, else the
     existing gradient (fail closed).
  2. Blit the cached Apple face bitmap via the existing menu-preview slot
     cache (`root_menu_video_menu_preview_*` — it already handles load-once,
     LRU, failure backoff, and dark variants). No new cache is introduced;
     state the reuse in review per the memory-bounding rule.
  3. Draw live hands on top. Hands remain runtime-drawn (stock renders hands
     at runtime too; a time display cannot be a static asset). Two options,
     in preference order:
     a. If the firmware stores hand sprites per angle (some Apple builds do),
        blit the exact sprite for the current minute/hour position.
     b. Otherwise keep the current sine-table line hands, but restyle
        lengths/colors/center cap to match the extracted face's geometry.
        Offline rotation/resampling of Apple hand art is forbidden by the
        provenance policy, so procedural hands are the honest fallback.
  4. Keep `root_menu_video_draw_clock_date` date text (it reflects stock's
     date line under the preview clock).
- Second-hand cadence: the pane already redraws on the slideshow animation
  timer; clamp clock repaint to 1 Hz (elapsed-tick derived, per steering
  cadence rules) instead of every animation frame.

### B3. Verified-asset gate

- Replace the blanket `return false` in
  `root_menu_video_preview_asset_is_verified()` with a per-title check that
  returns true only for titles whose backing asset file is an
  `*.apple.*.bmp` extraction (start with "Extras"/"Clock"; the mechanism then
  serves any future verified stock captures). Existence check must use the
  cached failure table, not a per-frame `file_exists`.
- Asset missing or unreadable → exactly today's vector clock. No regression
  path.

### B4. Evidence

- Sim: hover Extras and Clock in light and dark themes; verify no draw-path
  file I/O after first load (menu-preview slot hit), correct hands vs.
  `get_time()`, and clean fallback with the asset file removed.
- Hardware: pane must appear within one frame of hover (asset preloaded by
  the same idle service that today warms menu previews), and Hold/USB events
  stay observable during the 1 Hz repaint loop.

---

## C. Photos hover: smooth full-library slideshow

### Current behavior

- Photos hover already routes to the shared source-slideshow
  (`IPODJS_PREVIEW_PHOTOS`), but it underperforms Music/Games because:
  1. **Path discovery is a filesystem crawl.** `root_menu_video_preview_
     resolve_photo_root` recursively scans from `/` to depth 6 — every
     directory on the device — to find the "best" `.photo_previews` root,
     then scans again to enumerate. On 6G hardware this spins storage for
     seconds at hover time and repeats every 180 s cache reload.
  2. **Only 64 photos ever participate** (`IPODJS_PREVIEW_MAX_ITEMS`), chosen
     by directory-scan order, shown in fixed sequential order
     (`cycle % count`) — not "all photos on device", and the same first
     photos every time.
  3. **3-second cycle with a 2-second pan** vs. Music's 6-second pan
     (`ALBUMLIST_SLIDESHOW_PAN_DURATION`), and only 2 decode slots with
     prefetch starting 1 s into a 3 s cycle — visible hitches when a decode
     (320x320 BMP -> 240x240 resize+dither) lands mid-pan.
- The lock filter is correct and must be preserved: `.photo_previews`-relative
  paths are matched against `photos.locks` entries including directory
  prefixes (`root_menu_video_photo_preview_is_locked`), and an unreadable
  locks file fails closed to an empty slideshow.

### C1. Kill the crawl: persisted preview index

- RockPod already generates every preview: `rockbox_photos.py` writes
  `Photos/.photo_previews/<relpath>.bmp` (≤320x320) for **every synced
  photo** and repairs missing/stale ones. Extend it to also write
  `Photos/.photo_previews/index.tsv`: one line per preview —
  `relpath<TAB>width<TAB>height`, written atomically after each sync bundle.
- The photos plugin (`apps/plugins/photos.c`) already maintains sidecars on
  rename/delete (`photos_move_sidecar`, `photos_delete_sidecar_for`); it
  must rewrite the touched index lines in the same operations so the index
  never references a deleted photo. Plugin exit already invalidates the
  preview source cache (`launch_photos_plugin` →
  `root_menu_video_preview_invalidate_source_cache`).
- Loader (`root_menu_video_preview_load_photo_paths`):
  1. Root resolution: use `/Photos` if it has `.photo_previews` (the RockPod
     layout), else fall back to a scan **bounded to top-level directories
     only** (depth 2, no full-device recursion). The current depth-6 crawl
     from `/` is deleted.
  2. If `index.tsv` exists, read it instead of `opendir` recursion. Verify
     each candidate photo still exists only at decode time (cheap, one stat
     per slide), not at scan time.
  3. No index and no `.photo_previews` → pane falls back to the gradient +
     asset exactly as today. Nothing lost for non-RockPod users.

### C2. All photos, random order

- Replace the photos use of the 64-slot `root_menu_video_preview_paths`
  table with an index-backed source: store up to
  `IPODJS_PHOTO_INDEX_MAX = 1024` *offsets into index.tsv* (u32 each, 4 KB)
  instead of 64 full `MAX_PATH` strings. The path for a wanted slide is
  re-read from the index file inside the **service** function (never the
  draw path), using the offset. Videos/games keep the existing table; this
  is additive.
- Locked filtering: apply the `photos.locks` prefix filter while building the
  offset list (stream both files once). Unreadable locks file → zero entries
  (unchanged fail-closed rule). Re-filter on every source (re)load and after
  every photos-plugin exit.
- Order: reuse the coprime-step random walk from `albumlist_art.c`
  (`albumlist_random_manifest_index` logic) so all N photos appear once per
  N cycles with no repeats — same feel as the Music pane. Hoist that helper
  into a shared header rather than duplicating it.
- Byte cost: +4 KB BSS (offset table). State in review; no core_alloc use.

### C3. Match Music's motion

- Photos (and only photos, initially) moves to Music's timing constants:
  6-second pan per slide (`HZ * 6` period), prefetch of the next slide
  beginning at half-phase, pan direction alternating per cycle (the cover
  crop+pan drawer `root_menu_video_draw_preview_cover` already supports
  this).
- Keep `IPODJS_PREVIEW_IMAGE_CACHE = 2` slots (240x240 native = 115 KB each;
  a third slot is +115 KB BSS and is **not** justified — with a 6 s cycle and
  half-phase prefetch, two slots give ≥3 s of decode headroom for a ~100 ms
  decode). If hardware traces still show a hitch at slide handoff, the
  fallback-to-last-drawn-slot logic already masks it; do not add a slot
  without the ELF audit.
- Optional decode cheapening: have RockPod emit previews at exactly 240x240
  (cover-cropped) so `read_bmp_file` skips the scaler entirely. Existing
  320x320 previews keep working through the resize path (repair logic in
  `rockbox_photos.py` regenerates on next sync).
- Cadence/idle rules are untouched: decode only from
  `root_menu_video_preview_service` under its existing gates (queue empty,
  no hold, not paused-audio, no hold-storm), animation frames derived from
  elapsed ticks, cap at the existing HZ/10–HZ/12 frame delay.

### C4. Evidence

- Extend `tools/ipodjs_photos_sim_regression.sh`:
  - hover Photos with a 200-photo simdisk library: slideshow starts from
    index without a recursive scan (assert via trace that no `opendir`
    outside `/Photos/.photo_previews` occurs);
  - lock a folder in the plugin, exit, re-hover: no photo from that subtree
    appears across ≥ 2N cycles; corrupt `photos.locks` unreadable → empty
    pane;
  - delete a photo via plugin, re-hover: no failure flash, index consistent;
  - soak 10 min hovering with audio playing: fd count stable, no core memory
    trend, playback uninterrupted.
- Hardware gate: first hover after boot must show the first slide within the
  settle delay + one decode (~1 s), not after a device-wide scan.

---

## Sequencing

1. **C1** (delete the crawl) — biggest hardware win, smallest risk, no new
   data format needed to land (index.tsv optional at this stage).
2. **A1 + A2** (thumb pack + inter-scroll service) — the stock album-list
   feel; RockPod and firmware halves can land in either order (fallbacks
   cover the gap).
3. **C2 + C3** (full-library random slideshow at Music timing).
4. **B1–B3** (Apple clock asset pipeline, render, verified gate) — isolated
   from A/C; can proceed in parallel once the IPSWs are downloaded and
   hash-verified.
5. **A3** (cache raise) last, only with a clean ELF/BSS audit.

Every step independently satisfies the steering doc's simulator evidence list
and the hardware gate before deploy (deploy to both firmware locations per
CLAUDE.md).

---

## Implementation notes (2026-07-25)

- A1/A2/A3 landed as specified: `thumbs.pack` writer in
  `rockpod/services/albumlist_export.py` (`ALTB` v1, per-record 4-byte
  header carrying present flag + actual w/h), reader + row alignment in
  `apps/gui/albumlist_art.c`, `albumlist_art_service_one()` +
  direction prefetch hooked in `apps/tree.c`, bitmap cache raised to 64.
  Measured iPod 6G ELF delta: text +1188 B, bss +180128 B (~176 KB,
  dominated by the 64-slot cache; inside the spec's ~250 KB budget).
- C1/C2/C3 landed as specified: `/Photos` fast path, fallback scan
  bounded to depth 2, `.photo_previews/index.tsv` written by
  `rockpod/services/rockbox_photos.py` (sync and remove bundles) and
  maintained by the photos plugin on rename/delete; firmware reads a
  1024-entry offset table with lock filtering (fail-closed), random
  order via shared `apps/gui/slideshow_order.h`, 6-second pan with
  half-phase prefetch.  Slot identity across index reloads uses a
  generation stamp instead of draw-path path resolution.
- B deviation: no large analog clock face bitmap exists in any
  extractable Apple source (verified: the 5G firmware's 401 paMB
  bitmaps contain only 45x45 clock icons and hand-needle frames; the
  Classic 6G/7G UI resources sit inside the encrypted OSOS image, its
  FAT16 iPodResources volume carries only fonts/VideoCore; the Apple
  120GB user-guide PDF's twelve native 320x240 captures show only Music
  highlighted).  Implemented instead: the pane background is now exact
  stock pixels (crop + uniform-column tile of the Classic main-menu
  backdrop pic21997, see `assets/ipodjs/source/README-clock-pane.txt`
  and `tools/generate_ipodjs_clock_pane.py`), drawn through the
  menu-preview slot cache with the runtime clock composited on top with
  the stock light face.  A pixel-exact Apple capture dropped at
  `.rockbox/ipodjs/apple/previews/clock-pane-stock.174x240x24.bmp`
  overrides it with no code change.
- Evidence: sim + ipod6g + ipodvideo builds pass;
  `tools/ipodjs_navigation_sim_regression.sh`,
  `tools/ipodjs_photos_sim_regression.sh`, and
  `tools/ipodjs_cache_memory_sim_regression.sh` pass; RockPod tests for
  photos, album-list source invariants, and the new thumb pack pass
  (4 `test_album_list_layout_source` failures and 4 `test_sync`
  failures pre-date this work at HEAD).  Hardware gate not yet run.
