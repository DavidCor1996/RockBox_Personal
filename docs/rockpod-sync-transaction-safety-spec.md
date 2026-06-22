# RockPod Sync Transaction Safety Spec

Scope: make RockPod device sync safe, auditable, resumable, and recoverable
across music, videos, artwork, playlists, runtime history, Rockbox tagcache
generation, and manual device delete flows.

Primary target: mock device and iPod Video / 5G simulator first. Real iPod
hardware should only receive this flow after the simulator and mock-device gates
pass.

## Problem

RockPod sync writes several classes of state to the selected device root:

- audio/video media under `Music/` and `Videos/`;
- lyric sidecars beside audio files;
- album art and WPS-sized cover assets beside albums;
- album-list and video-list indexes under `.rockbox/`;
- Rockbox playlists under `Playlists/`;
- Rockbox tagcache/database files under `.rockbox/`;
- runtime/playback history imports and generated runtime-bearing tagcache
  fields;
- optional duplicate cleanup and user-requested device deletes.

The current code has many good local safety pieces, but the whole sync is not
yet treated as one durable transaction. A crash, cancellation, unplug, write
failure, or post-sync tagcache failure can leave the device in a mixed state
that is recoverable only by rescanning or re-syncing. That is usually workable,
but it is not explicit enough for a tool that writes to removable storage and
also generates Rockbox's runtime-bearing database files.

## Preferred Outcome

RockPod sync should behave like a staged transaction:

1. Build a complete plan with every write/delete/finalize action.
2. Validate that every destination is inside the selected device root.
3. Snapshot the device state needed for rollback or recovery.
4. Stage new bytes without disturbing the previous valid state.
5. Publish files in a controlled order.
6. Finalize playlists, runtime data, and Rockbox tagcache only after media
   publish succeeds.
7. Write a durable transaction manifest that can explain or repair an
   interrupted sync.
8. Make repeat syncs idempotent.

If RockPod cannot prove a write is safe, it should skip that operation and tell
the user exactly what was skipped.

## Current Code Findings

### Existing safety helpers

- `rockpod/services/path_safety.py`
  - `validate_device_root()` rejects obvious dangerous roots such as `/`,
    `/home`, `/media`, `/mnt`, `/run/media`, and `/tmp`.
  - `resolve_under_root()` normalizes a device-relative path and rejects parent
    escapes.
  - `is_within_root()` provides common-path containment checks.
- `rockpod/services/file_safety.py`
  - `atomic_write_text()` and `atomic_write_json()` write through a temp file
    and replace the target after a complete write.
  - These helpers are already used by many generated state files and manifests.

### Sync engine

- `rockpod/services/sync_engine.py` has a `SyncPlan` that lists:
  - `to_copy`;
  - `to_resync`;
  - `artwork_to_copy`;
  - `to_delete`;
  - `preflight_linked`;
  - `preflight_removed`.
- `SyncWorker._copy_file()` copies through `dest + ".rockpod_tmp"`, flushes
  with `fdatasync()`/`fsync()`, then publishes with `os.replace()`.
- `_cleanup_stale_sync_temps()` removes interrupted `.rockpod_tmp` files before
  planning.
- Duplicate cleanup has a safety limit:
  `max_auto_duplicate_deletes_per_sync`, default 5.
- Duplicate auto-delete candidates must look like RockPod-created suffix paths,
  such as `Track (2).mp3`.
- The worker skips Rockbox album/video list manifest publication when a media
  copy failure occurred.
- The worker updates the SQLite sync cache only after copying succeeds.

### Post-sync Rockbox integration

- `rockpod/ui/main_window.py` currently finalizes successful syncs by:
  - applying the successful plan to the device cache;
  - exporting Rockbox playlists;
  - generating Rockbox tagcache/database files host-side if track files changed;
  - falling back to Rockbox auto-update if host tagcache generation fails.
- These steps happen after the worker reports success and are coordinated by the
  UI layer, not by a shared transaction coordinator.
- Runtime import is handled separately and can be delayed until Rockbox database
  state polling sees a ready database.

### Tagcache generation

- `rockpod/services/rockbox_tagcache.py` generates tagcache files in a temporary
  root under the device mount.
- It validates the generated files by reading them back.
- It removes only Rockbox database/tagcache files under `.rockbox`.
- It publishes known host-generated tagcache files with `os.replace()`.

### Theme deployment reference

- `rockpod/services/rockbox_deploy.py` has a stronger backup/rollback pattern
  than normal sync:
  - build diff;
  - validate destination containment;
  - copy overwritten files into a timestamped backup directory;
  - write a JSON manifest;
  - restore from the latest backup.
- Sync should not blindly copy this exact behavior for large media files, but
  it should reuse the manifest and containment ideas.

### Playlist export

- `rockpod/services/rockbox_playlists.py` writes managed playlist files and a
  `.rockbox/rockpod-playlists.json` manifest.
- The manifest is written atomically.
- Stale playlist removals are direct `os.remove()` calls.
- Playlist export currently uses `os.path.join(mount_path, rel_source)` rather
  than a central device-root resolver.

## Safety Gaps

### 1. No durable sync transaction manifest

If RockPod or the host crashes mid-sync, the next run can remove stale temp
files and sometimes repair completed copies, but there is no durable manifest
that says:

- which operation was in progress;
- which temp file belonged to which final path;
- which old file was replaced;
- which delete was intentional;
- whether post-sync playlist/tagcache finalization completed;
- whether a rollback is available.

### 2. Path containment is not central in sync

Theme deploy paths use `resolve_under_root()`. Sync paths often use:

`os.path.join(device.mount_path, rel_path)`

Most relative paths are generated internally, so this is not an immediate
exploit path in normal use. It is still weaker than the deploy path and should
be normalized before every write/delete, including:

- media copy destinations;
- old resync paths;
- lyric sidecars;
- artwork paths;
- playlist files;
- tagcache publish paths;
- manual device deletes;
- duplicate cleanup.

### 3. Overwrite and delete rollback is incomplete

Media and playlist writes use safe replace, but if a destination file is
overwritten and the sync later fails, RockPod has no general rollback record.

For large media, always backing up every overwritten file may be too expensive.
The spec needs a tiered policy:

- always rollback small metadata/index/playlist/tagcache files;
- optionally backup overwritten media under a size limit;
- otherwise rely on re-copy from the local source but record enough manifest
  data to explain and repair the state.

### 4. Post-sync steps are not one finalization phase

Playlist export, Rockbox tagcache generation, and runtime handling are logically
part of sync completion, but they are currently triggered from the UI after the
worker succeeds.

This can produce mixed outcomes such as:

- media files copied, but tagcache generation failed;
- playlists updated, but tagcache stale;
- runtime data not imported before host tagcache generation;
- SQLite cache updated, but Rockbox database still needs device-side rebuild.

Some of these states are acceptable, but they should be explicitly recorded and
reported.

### 5. Device identity and mount stability need a final check

Sync planning uses the connected device. A long operation should also verify
before publish/finalize that:

- the mount path still exists;
- the device key has not changed;
- required Rockbox directories still exist or are creatable;
- free space has not fallen below the planned write size plus safety margin.

### 6. Source-file changes during sync are not fully detected

The copy path checks that a source file exists, copies bytes, and flushes. It
does not require a source stat/hash check before and after copy. If a local
source file changes during sync, RockPod could publish a file that does not
match the planned metadata.

### 7. Manual delete paths need the same safety class

`delete_device_track()` and `remove_duplicate_device_tracks()` delete files and
database rows directly. They should use the same containment, manifest, and
rollback/undo rules as automatic sync deletes.

## Proposed Design

### 1. Add `SyncTransaction`

Introduce a focused transaction coordinator in RockPod, for example:

`rockpod/services/sync_transaction.py`

Responsibilities:

- validate the device root and device identity;
- convert every plan item into normalized operations;
- resolve all destination paths with `resolve_under_root()`;
- write a durable transaction manifest before touching device files;
- stage writes;
- publish writes;
- perform controlled deletes;
- run post-sync finalizers;
- mark transaction status as `prepared`, `publishing`, `finalizing`,
  `completed`, `cancelled`, or `failed`;
- provide recovery actions on the next launch.

This should be a service layer, not UI-only code. The UI should display status
and confirmation, but not own the safety rules.

### 2. Transaction manifest

Store transaction manifests under:

`.rockbox/rockpod-transactions/`

Use one directory per transaction:

`.rockbox/rockpod-transactions/20260621-153012-<shortid>/`

Required files:

- `manifest.json`
- `events.log`
- optional `backups/`

Manifest fields:

- `transaction_id`
- `created_at`
- `rockpod_version`
- `device_key`
- `mount_path`
- `status`
- `plan_summary`
- `free_space_before`
- `operations`
- `finalizers`
- `errors`
- `completed_at`

Operation fields:

- `op_id`
- `kind`: `copy`, `replace`, `delete`, `mkdir`, `playlist`, `tagcache`,
  `runtime_import`, `cache_update`, `cleanup`
- `source_abs`
- `destination_rel`
- `destination_abs`
- `temp_rel`
- `backup_rel`
- `expected_size`
- `expected_hash`
- `previous_exists`
- `previous_size`
- `previous_hash`
- `status`
- `error`

The manifest should be written through `atomic_write_json()`. Every operation
status change should append to `events.log` and periodically rewrite the
manifest.

### 3. Path safety rule

All device writes and deletes must go through a single resolver:

```python
dest_abs = resolve_under_root(device_mount, destination_rel)
```

Rules:

- reject empty paths;
- reject absolute paths after normalization;
- reject parent escapes;
- reject symlink escapes if symlinks are ever allowed in device roots;
- reject writes outside approved top-level device areas unless the operation
  type explicitly allows it.

Approved top-level areas for normal sync:

- `Music/`
- `Videos/`
- `Playlists/`
- `.rockbox/albumlist/`
- `.rockbox/videolist/`
- `.rockbox/rockpod-transactions/`
- `.rockbox/database*.tcd` and `.rockbox/tagcache*.tcd`
- `.rockbox/rockpod-playlists.json`
- `.rockbox/database_changelog*` only when runtime export/import explicitly
  owns it.

Manual or advanced operations can have their own allowlist, but they should not
reuse the normal sync allowlist silently.

### 4. Staging and publish order

Recommended order:

1. Preflight:
   - validate mount and root;
   - compute free-space estimate;
   - import/snapshot Rockbox runtime data before tagcache overwrite risk;
   - write transaction manifest as `prepared`.
2. Stage writes:
   - copy media/artwork/playlist/tagcache outputs to temp files;
   - fsync/fdatasync file handles;
   - verify size and hash when expected hash is known.
3. Publish media:
   - `os.replace()` staged media files;
   - sync parent directories where platform support allows.
4. Publish artwork and sidecars:
   - publish cover and lyric sidecars after associated media succeeds.
5. Delete old paths:
   - delete only after replacement succeeded;
   - record every delete in the manifest.
6. Finalize cache:
   - update SQLite device cache;
   - export playlists;
   - generate and publish host tagcache;
   - record fallback to Rockbox auto-update if needed.
7. Mark `completed`.

If cancellation happens before publish, remove staged files and mark
`cancelled`. If cancellation happens after publish begins, stop at the next safe
boundary and mark `failed` or `interrupted`, not `completed`.

### 5. Rollback policy

Rollback should be tiered:

#### Always backed up

- playlist files;
- playlist manifest;
- album/video list manifests;
- Rockbox tagcache/database files;
- RockPod transaction/control files;
- small sidecars such as `.lrc`;
- small artwork files below a configurable limit.

#### Conditionally backed up

- overwritten audio/video files below `sync_media_backup_size_limit_mb`;
- deleted duplicate media when the user explicitly enables media undo.

#### Not backed up by default

- large media files that can be recopied from the local library.

For large media without backup, the manifest must record:

- source path;
- source hash or size/mtime identity;
- destination path;
- previous size/hash if known;
- whether rollback means "delete new file" or "recopy old source if still
  available."

### 6. Recovery on next launch

On RockPod startup or device connection:

1. Scan `.rockbox/rockpod-transactions/` for non-completed manifests.
2. Validate the manifest device key matches the connected device.
3. Offer actions:
   - resume if all sources and temp files still exist;
   - roll back if backups exist;
   - clean staged temp files;
   - mark abandoned after user confirmation.
4. Never auto-delete published media from an interrupted transaction without a
   manifest rule proving the file was RockPod-created.

This recovery flow should also replace broad temp cleanup where possible. The
existing `.rockpod_tmp` cleanup can remain as a last-resort orphan cleanup, but
manifest-backed recovery should be preferred.

### 7. Integrate runtime preservation

Before any host tagcache generation or `.rockbox/database_*.tcd` replacement:

- import current runtime fields from existing tagcache if supported;
- import Rockbox `database_changelog.txt` with the real quoted-token parser;
- merge runtime values into the device inventory used by host tagcache
  generation;
- fail closed if a runtime-bearing database exists but runtime extraction
  fails, unless the user explicitly chooses to rebuild without preserving
  runtime data.

This connects directly to
`docs/rockbox-runtime-data-preservation-spec.md`.

### 8. Move post-sync finalizers out of UI ownership

Create a finalization API owned by the sync/transaction layer:

```python
finalize_sync_transaction(transaction, plan, device)
```

Finalizers:

- update RockPod SQLite device cache;
- export Rockbox playlists;
- generate Rockbox tagcache host-side;
- enable Rockbox auto-update fallback if host tagcache fails;
- import refreshed runtime/playlists after finalization.

The UI should call this and render the result. It should not decide the safety
sequence itself.

### 9. Dry-run and confirmation

The sync dialog should show a transaction summary before execution:

- new files;
- replacements;
- deletes;
- playlist changes;
- tagcache/database replacement;
- estimated bytes written;
- rollback coverage:
  - fully rollbackable;
  - source-recopy rollback;
  - not rollbackable.

Dangerous operations require explicit confirmation:

- deletes above a low threshold;
- deleting paths outside RockPod-managed manifests;
- replacing existing media without local source hash;
- host tagcache generation when runtime preservation could not be verified;
- using a root that does not look like a Rockbox device or mock/simulator disk.

## Implementation Plan

### Phase 1: Foundation

1. Add `sync_transaction.py`.
2. Add manifest data models and atomic manifest writes.
3. Add path normalization for sync operations using `resolve_under_root()`.
4. Add tests proving unsafe relative paths are rejected for copy, artwork,
   playlist, tagcache, delete, and manual delete operations.
5. Record transaction manifests in mock-device sync tests.

### Phase 2: Worker integration

1. Convert `SyncPlan` into explicit transaction operations.
2. Make `SyncWorker` execute through `SyncTransaction`.
3. Keep the existing temp-copy and `os.replace()` behavior, but let the
   transaction own temp names and operation status.
4. Add size/hash verification after staged writes.
5. Mark cancellation before publish as cleanly cancelled.
6. Mark cancellation after publish as interrupted and recoverable.

### Phase 3: Finalizers

1. Move post-sync playlist/tagcache/runtime finalization out of
   `MainWindow._post_sync_rockbox_integration()`.
2. Import runtime data before host tagcache generation.
3. Record playlist export result in the transaction manifest.
4. Record tagcache generation result or auto-update fallback in the manifest.
5. Update UI to show the finalization result from the service layer.

### Phase 4: Rollback and recovery

1. Backup small overwritten files and generated metadata files.
2. Add rollback for completed or interrupted metadata/index/playlist/tagcache
   writes.
3. Add startup/device-connection recovery scan.
4. Add a recovery dialog for resume, rollback, clean staged files, and abandon.
5. Add tests for crash-style interrupted manifests.

### Phase 5: Hardware gate

1. Run full mock-device tests.
2. Run simulator sync against a disposable simdisk.
3. Boot simulator and verify Music, Playlists, album list, video list, and
   database behavior.
4. Only then allow real iPod validation with a pre-sync backup.

## Validation Matrix

| Scenario | Expected result |
| --- | --- |
| Normal sync with new album | transaction completes; media, art, playlists, and tagcache are consistent |
| Repeated sync | no changed operations except timestamped transaction record |
| Copy failure before publish | temp files cleaned; manifest says failed; old device state remains valid |
| Cancellation before publish | no final files changed; manifest says cancelled |
| Cancellation after one publish | manifest says interrupted; recovery offers resume/rollback |
| Device unplug during copy | manifest remains; next connection offers recovery |
| Runtime-bearing tagcache exists | runtime is imported before tagcache generation |
| Runtime import fails | host tagcache publish is blocked unless user explicitly overrides |
| Playlist export fails | media sync remains complete; finalizer failure is visible |
| Tagcache generation fails | Rockbox auto-update fallback is recorded |
| Duplicate delete above limit | delete operation is skipped |
| Delete path without RockPod duplicate suffix | automatic delete is blocked |
| Manual delete selected track | path containment is enforced and delete is recorded |
| Unsafe relative path with `..` | operation is rejected before worker starts |
| Stale `.rockpod_tmp` without manifest | orphan cleanup reports what it removed |

## Acceptance Criteria

- Every sync creates a durable transaction manifest on mock/simulator devices.
- Every device write/delete path is resolved with `resolve_under_root()` or an
  equivalent stricter helper.
- A planned path escape fails during planning, before any device write.
- Media copy still writes through temp files and publishes with `os.replace()`.
- Staged writes are size-verified, and hash-verified when an expected hash is
  available.
- Post-sync playlist export, runtime import, and tagcache generation are
  recorded as finalizers.
- A failed host tagcache generation cannot silently look like a fully completed
  sync.
- Runtime-preservation failure blocks runtime-bearing tagcache replacement by
  default.
- Interrupted syncs are detected on next device connection.
- Metadata/index/playlist/tagcache rollback works in tests.
- Large media rollback policy is explicit in the UI and manifest.
- Full RockPod tests pass against mock roots without requiring a mounted iPod.
- Simulator-first gate passes before real hardware validation.

## Risks

- More safety bookkeeping can slow sync, especially hashing and backups on slow
  removable storage.
- Full media rollback can consume a lot of device space; use tiered backup
  limits.
- Directory fsync support differs by platform; implement best-effort parent
  directory sync where available and log when unavailable.
- Recovery UI can confuse users if it exposes too many choices. Use clear
  default actions: resume when safe, rollback when safe, otherwise clean staged
  files.
- The transaction layer can become too broad if it absorbs all RockPod logic.
  Keep it focused on file-system safety, manifests, and finalization order.

## Open Questions

- What should the default media backup size limit be for iPod Video / 5G
  devices with iFlash storage?
- Should transaction manifests be retained forever, capped by count, or deleted
  after a successful second sync?
- Should users be allowed to opt into aggressive automatic cleanup of
  interrupted published media, or should all published-media cleanup require
  confirmation?
- Should manual local-file deletes in RockPod use the same transaction manifest,
  or only device-root operations?
