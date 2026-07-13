# RockPod / Rockbox Sync Optimization Spec

## Scope

Improve RockPod media-sync planning speed without weakening device-write safety
or changing which tracks are copied, updated, linked, or removed.

This pass is deliberately host-side. It does not change Rockbox firmware,
playback, playlists, device deployment, or the on-device database format.

## Safety invariants

- Preserve match priority: persistent ID, metadata hash, exact normalized
  identity, incomplete-metadata identity, fuzzy metadata, then file hash.
- Preserve device inventory order when more than one exact identity candidate is
  eligible.
- Keep one device file writer. Do not parallelize writes to removable storage.
- Keep copy-to-temporary, data sync, and atomic replace behavior.
- Keep the old device copy until a replacement copy succeeds.
- Keep cancellation checks, stale-temp recovery, lyric/video sidecar handling,
  manifest failure guards, and duplicate-delete safety limits.
- Do not skip file validation, metadata preparation, or inventory verification.

## Implemented optimization

1. Build normalized exact-identity indexes once per match run. Exact title,
   artist/album-artist, and album matching now examines only eligible candidates
   while retaining original inventory order.
2. Cache normalized device metadata for the fuzzy fallback instead of repeatedly
   normalizing the same strings for every local track.
3. Analyze unmatched-copy reasons in one device-inventory pass instead of
   rebuilding and renormalizing that inventory once per missing track.
4. Record exact-identity and fuzzy candidate counts in the existing matcher
   profile so future changes can be checked with repeatable evidence.

## Complexity target

The common exact-match path should be approximately `O(local + device +
candidates)` instead of repeatedly scanning the complete device inventory.
The fuzzy fallback remains conservative and may scan unmatched device rows; it
is intentionally unchanged semantically because narrowing fuzzy candidates can
create false negatives.

## Validation

- Run `rockpod/tests/test_matcher.py` and `rockpod/tests/test_sync.py`.
- Verify exact matching still selects the first eligible device row when artist
  and album-artist produce multiple candidates.
- Verify bulk unmatched explanations equal the existing single-row API.
- Verify copy failure retains the previous file and successful writes leave no
  sync temporary behind.
- Compare the synthetic 2,000-track exact-match fixture before and after, while
  treating timing as evidence rather than a brittle test threshold.

Measured on the same local fixture for this pass: 0.1376 seconds before and
0.0397 seconds after (about 3.5x faster). The optimized run examined 2,000 exact
identity candidates for 2,000 tracks and did not enter fuzzy matching.

## Store video metadata follow-through

Store video downloads now carry yt-dlp source metadata through the library
scan, then attempt canonical movie/show and episode lookup. Supported fields
include title, uploader, source URL, description, year, show title, season,
episode, episode title, genre/category, provider IDs, plots, and video kind.
Show matches also populate artist, album artist, season album, and track number
so RockPod grouping and Rockbox video manifests see the same identity.

The downloader persists the source thumbnail beside the converted video as a
JPEG fallback. A successful canonical lookup replaces that fallback in the
artwork cache with the provider poster; iTunes artwork requests use the larger
600-pixel variant. If canonical lookup fails, verified source metadata and its
poster are retained rather than inventing a provider match.

TV show and episode lookup now falls back to the no-key TVmaze API when iTunes
has no result. The fallback supplies canonical show identity, season/episode
details, IMDb ID, plot, genre, dates, and portrait poster art. Provider metadata
and manually locked metadata take precedence over legacy filename overrides.

If automatic resolution still fails, the video context menu exposes **Get
Info** for manual title/show/season/episode editing and **Choose Poster** for a
local canonical cover. Manual edits to formats without writable tags, including
converted MPEG files, are stored safely in the RockPod library, marked manual,
and locked so a later scan does not replace them.

## P1 reliability and planning pass

### Goals

- Keep the UI responsive during video metadata and poster network requests.
- Prevent sync planning from monopolizing the SQLite writer lock.
- Avoid rereading unchanged multi-gigabyte RVP cache bundles merely to rebuild a plan.
- Make resync status and change detection specific to the selected device.
- Remove per-device-track database round trips from local cache cleanup.

### Compatibility and safety invariants

- Device writes remain single-threaded and retain temporary-file, flush, and
  atomic-replace behavior.
- Duplicate deletion, path containment, inventory verification, matching order,
  and cancellation rules are unchanged.
- RVP digests are reused only when the absolute component path, byte size, and
  nanosecond modification time all match the digest manifest. Any mismatch
  performs the original full SHA-256 pass and atomically replaces the manifest.
- Existing track-level sync fields remain populated for compatibility. New
  resync decisions use the selected `device_tracks` row as their authoritative
  baseline.
- Schema migration initializes per-device baselines from the last verified
  device hashes; it does not mark absent tracks present or create new links.
- Metadata workers perform only network/provider computation off-thread. SQLite
  and Qt widget mutations return to the main thread through queued signals.

### Data model

Schema version 11 adds `last_synced_metadata_hash` and
`last_synced_file_hash` to `device_tracks`. Successful sync upserts both values.
Status queries join the selected device row to its local track, so syncing or
editing against one iPod cannot create a resync count for another.

### Execution model

`SyncPlanWorker` uses its dedicated SQLite connection for ordinary WAL-backed
reads. Planning no longer holds the process-wide write mutex. The existing
preflight-link transaction remains the only planning-time write section and is
kept short.

Video lookup, exact episode lookup, poster preview download, and automatic
store-import lookup use the global Qt thread pool. Results are delivered via
queued signals; stale poster responses are ignored by URL identity.

### Acceptance tests

- Construct and search the manual video metadata dialog without blocking or
  touching widgets from a worker thread.
- Confirm two device rows for one local track can report different resync state.
- Confirm schema-10 databases migrate per-device baselines without losing rows.
- Confirm a second unchanged RVP preparation does not call the full bundle
  hasher, while a changed component does.
- Confirm cache cleanup uses one batched local-track lookup and retains the same
  files as before.
- Run the metadata, RVP, matcher, library, and sync test modules.

## P2 finalization, shutdown, and cache lifecycle pass

### Goals

- Finalize successful track sync state with one batched SQLite statement.
- Stop scanner, inventory, and sync workers only at explicit safe boundaries.
- Bound artwork cache growth without deleting current covers, posters, metadata,
  manifests, or placeholders.

### Safety invariants

- Batched finalization remains inside the existing single transaction and
  writes the same four values as the former row-by-row updates.
- Cancellation never publishes a partially copied file. A cancelled media copy
  removes its `.rockpod_tmp` file and leaves the previous destination intact.
- A cancelled library scan commits no partial additions, updates, or stale-row
  deletions.
- Device inventory cancellation skips missing-track marking and later runtime,
  playlist, and final scan-state work until verification can complete safely.
- Worker shutdown may wait for a metadata parser, filesystem flush, or SQLite
  statement already in progress, but never uses `QThread.terminate()`.
- Artwork cleanup operates only inside RockPod-managed cache directories. It
  preserves every path referenced recursively by album metadata, all metadata
  JSON, placeholders, and list manifests. It never touches source media or
  artwork outside the configured cache root.

### Artwork cache policy

`artwork_cache_max_mb` defaults to 1024 MiB and
`artwork_cache_cleanup_interval_hours` defaults to 24. A throttled startup pass
measures the complete cache and, only when over budget, removes oldest
unreferenced managed files until the budget is met or no safe candidates
remain. Current originals and variants are retained even if that means the
budget cannot be reached. A public dry-run cleanup API reports the proposed
paths and byte reduction without deleting anything.

### Cancellation boundaries

- Library collection: each root, directory, and file entry.
- Library metadata: between files and before the database transaction.
- Device inventory: tagcache rows, directory walks, imported files, matching
  updates, and final integration phases.
- Sync execution: every copy operation and every 4 MiB copy chunk.

### Acceptance tests

- Verify batched finalization updates all rows and uses one `executemany` call.
- Cancel a library scan after work begins and verify the prior database remains
  unchanged.
- Cancel device inventory during traversal and verify it emits cancellation
  without marking unseen rows missing.
- Cancel a sync during a chunked copy and verify the destination remains intact
  and its temporary file is removed.
- Create referenced and stale artwork variants over a small test budget; verify
  only stale variants are removed and dry-run performs no deletion.
- Assert scanner, inventory, and sync shutdown paths contain no forced thread
  termination.

## Deferred work

- Concurrent device writes or relaxed flush behavior.
- Restricting stale-temp scans without a durable manifest of every sync target.
- Fuzzy-match candidate pruning without a larger real-world metadata corpus.
- Changes to artwork generation, transcoding policy, playlist export, or
  physical firmware deployment.
