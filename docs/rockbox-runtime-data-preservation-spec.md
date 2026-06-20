# Rockbox Runtime Data Preservation Spec

## Problem

Playback history appears to reset after RockPod syncs, and may also lose the
latest play around abrupt reboot or disconnect cycles.

In Rockbox this data is not a separate "history" database. It is numeric
tagcache state stored in `.rockbox/database_idx.tcd`:

- `playcount`
- `rating`
- `playtime`
- `lastplayed`
- `lastelapsed`
- `lastoffset`
- `commitid`

RockPod currently generates host-side Rockbox tagcache files after a sync. That
is good for boot speed, but it can replace the on-device tagcache with files
whose runtime numeric fields are zero or stale.

## Preferred Outcome

RockPod and Rockbox should maintain the same playback/runtime view by default.
Adding music, syncing playlists, rebuilding the Rockbox database, or reconnecting
the device should not fork or reset play history.

The expected steady-state behavior is bidirectional:

- Rockbox records device playback while the iPod is used.
- RockPod imports that runtime data when the device is connected or before any
  sync action that may touch the Rockbox database.
- RockPod shows the imported runtime data in its own library, smart playlists,
  and device summaries.
- RockPod writes the same runtime data back into generated Rockbox tagcache files
  so Rockbox continues from the same play counts, last-played ordering, ratings,
  and autoresume points after sync.
- Playlists remain visible on both sides: RockPod keeps its playlist model, and
  exported Rockbox playlist files remain valid after music sync and database
  generation.

This is not a one-time preservation step. It should be a normal part of every
RockPod/Rockbox sync cycle.

## Evidence

- Rockbox defines runtime fields in `apps/tagcache.h` and names them in
  `apps/tagcache.c`.
- Playback finish updates happen in `apps/tagtree.c`.
  - `tagtree_buffer_event()` reads existing runtime values from tagcache into
    the current `mp3entry`.
  - `tagtree_track_finish_event()` increments `playcount`, updates `playtime`,
    and writes `lastplayed` through `tagcache_update_numeric()`.
  - Autoresume stores `lastelapsed` and `lastoffset` through the same path.
- Runtime writes are queued in `apps/tagcache.c`.
  - `tagcache_update_numeric()` adds `CMD_UPDATE_NUMERIC` commands.
  - Normal shutdown and USB flush through `tagcache_shutdown()` call
    `run_command_queue(true)`.
  - An abrupt reset can still lose queued updates that have not been flushed.
- Rockbox can export/import runtime data with `.rockbox/database_changelog.txt`.
  - `tagcache_create_changelog()` writes one line per dirty track with quoted
    `tag="value"` tokens.
  - `tagcache_import_changelog()` can import that file after database rebuild.
- RockPod host generation is in `rockpod/services/rockbox_tagcache.py`.
  - `write_rockbox_tagcache_from_device_inventory()` uses
    `db.get_all_device_tracks()`.
  - `device_tracks` does not store `play_count`, `play_time`, `last_played`, or
    autoresume values.
  - `_normalize_writer_entry()` therefore writes runtime fields from missing or
    stale keys, commonly zero.
- RockPod runtime import exists in `rockpod/services/rockbox_runtime.py`, but it
  does not currently protect host generation.
  - `_post_sync_rockbox_integration()` calls host generation before importing
    runtime data.
  - The importer accepts JSON, CSV/TSV, and simple one-key-per-line input.
  - Real Rockbox changelog lines contain multiple quoted key/value tokens on one
    line, so the current parser does not reliably parse Rockbox's own export.

## Likely Reset Paths

### 1. Host Sync Replaces Runtime Fields

This is the high-confidence reset path.

1. User plays tracks on Rockbox.
2. Rockbox writes updated numeric values into `.rockbox/database_idx.tcd`.
3. RockPod sync copies/resyncs/deletes tracks.
4. RockPod regenerates `.rockbox/database_*.tcd` from cached device inventory.
5. Cached inventory lacks runtime values, so generated numeric fields are zero
   or stale.
6. Rockbox boots with a valid database, but runtime history appears reset.

### 2. Runtime Import Is Not a Reliable Fallback

Rockbox's built-in export/import mechanism can preserve runtime data across a
Rockbox rebuild, but RockPod does not currently automate a compatible export
before host replacement, and its parser does not handle the real export syntax.

### 3. Abrupt Reboot Loses Latest Queued Updates

Rockbox queues numeric updates and flushes them later. Normal shutdown and USB
paths flush the queue, but a hard reset, crash, or power loss can lose the most
recent track-finish update. This should lose only recent events, not the whole
history, unless a later host rebuild overwrites the database.

### 4. Settings and Eligibility Can Look Like Resets

Rockbox only gathers play history when `gather runtime data` is enabled. It also
does not count tracks with zero elapsed time or manual skips before 15 seconds.
Autoresume has a separate 3 second threshold. These are expected filters, not
sync bugs.

## Risk Analysis

The core risk is that RockPod currently treats Rockbox tagcache files as
replaceable generated indexes, while Rockbox also uses the master tagcache file
as user runtime state.

### Data at Risk

The at-risk data is not the audio files themselves. It is Rockbox-owned numeric
metadata inside `.rockbox/database_idx.tcd`:

- play history: `playcount`, `playtime`, `lastplayed`
- rating changes made on the device
- autoresume state: `lastelapsed`, `lastoffset`
- internal ordering/age state: `commitid`

Loss of this data makes Rockbox and RockPod appear to forget what has been
played, even though all music files and playlists may still exist.

### Why Adding Music Can Trigger Loss

Adding music is risky because it is exactly the workflow that asks RockPod to
regenerate Rockbox's database:

1. new files are copied to the iPod
2. RockPod updates its own device inventory
3. RockPod writes fresh `.rockbox/database_*.tcd` files so Rockbox can see the
   new music immediately
4. the fresh files are built from RockPod's inventory rows
5. those inventory rows do not currently preserve all Rockbox runtime fields

That means a harmless "add one album" sync can replace a database containing
months of runtime state with a clean database that has the new album plus reset
runtime fields.

### Likelihood

This risk is high whenever all of these are true:

- RockPod host-side tagcache generation succeeds after sync
- the existing Rockbox database contains nonzero runtime data
- RockPod has not imported or snapshotted that runtime data first
- generated rows are written with missing runtime fields defaulting to zero

The risk is lower when RockPod falls back to Rockbox on-device auto-update,
because Rockbox's own commit path attempts to preserve/resurrect runtime
statistics for matching tracks.

### Impact

Impact is medium to high:

- play counts and "recently played" views become wrong
- smart playlists based on Rockbox runtime stats become wrong
- autoresume points for podcasts/audiobooks can disappear
- users may distrust future syncs even if music files are intact

It is not expected to delete music files. The blast radius is `.rockbox`
database/runtime metadata.

### Risk Boundaries

The spec assumes these are separate risk classes:

- Sync-time overwrite risk: can reset broad history by replacing tagcache files.
- Reboot/power-loss risk: can lose only the latest queued runtime writes.
- Parser/import risk: can make RockPod believe runtime import succeeded while
  real Rockbox changelog data was ignored or partially parsed.
- Playlist visibility risk: playlist files can still be present while database
  runtime data is wrong; these need separate validation.

### Mitigations Required Before Code Changes Are Safe

- Snapshot existing Rockbox runtime data before publishing any host-generated
  tagcache.
- Parse real Rockbox changelog syntax and binary tagcache numeric fields.
- Block or defer host database publish if runtime extraction fails and existing
  runtime-bearing database files are present.
- Validate that the generated database round-trips runtime fields before
  replacing files on the device.
- Keep playlist sync validation separate: adding music must preserve both
  runtime fields and Rockbox playlist exports.

### Risk Validation Matrix

| Scenario | Expected result |
| --- | --- |
| Add one new album to a device with play history | existing history remains; new album appears |
| Repeat same sync | database and runtime fields are unchanged |
| Sync RockPod playlists after adding music | playlists show in RockPod and exported Rockbox playlists still exist |
| Host runtime snapshot fails | RockPod does not overwrite existing runtime-bearing tagcache |
| Abrupt reboot after one track play | at most the latest event is lost; older history remains |
| Real `database_changelog.txt` exists | RockPod imports quoted-token lines correctly |

## Goals

- RockPod must never replace a Rockbox tagcache without carrying forward runtime
  numeric fields for matching tracks.
- RockPod should import device runtime data before any host-side database
  generation, sync reconciliation, or tagcache deletion action that can affect
  `.rockbox/database_*.tcd`.
- RockPod should parse both the current binary tagcache and Rockbox's real
  `database_changelog.txt` format.
- Rockbox should reduce loss of the most recent runtime event on orderly reboot,
  USB connect, and normal shutdown.
- Keep RockPod's per-device runtime view and Rockbox's tagcache runtime fields
  synchronized by default.
- Keep playlist visibility synchronized between RockPod's playlist model and
  exported Rockbox playlist files.

## Non-Goals

- Do not rewrite user music files.
- Do not merge unrelated desktop-player history into Rockbox counters unless a
  separate, explicit import policy is added.
- Do not require the user to manually run Rockbox Database Export before every
  RockPod sync.
- Do not block sync forever if runtime import fails; fail closed by preserving
  the existing tagcache or falling back to on-device Rockbox update.

## Proposed RockPod Design

### Runtime Snapshot Before Publish

Before `_generate_rockbox_database_host_side()` publishes new tagcache files:

1. Read existing `.rockbox/database_idx.tcd` and string tag files.
2. Extract runtime numeric fields for every present track.
3. Import `.rockbox/database_changelog.txt` if present, using a parser that
   supports Rockbox's quoted token format.
4. Merge the newest/best runtime values into a device runtime snapshot.
5. Generate the replacement tagcache from device inventory plus that snapshot.

The binary tagcache read should be the primary source because it does not depend
on a manual export file.

### Runtime-Enriched Writer Input

Add a RockPod database/service method that returns device tracks enriched with
runtime values:

- key by `device_track_id` first
- fall back to exact `device_path`
- fall back to `local_track_id`
- only use metadata matching when it is unique and high confidence

For the host tagcache writer, map:

- `play_count` -> Rockbox `tag_playcount`
- `rating` -> `tag_rating`
- `play_time` in milliseconds -> `tag_playtime`
- `last_played` as Rockbox serial integer -> `tag_lastplayed`
- `last_elapsed` in milliseconds -> `tag_lastelapsed`
- `last_offset` -> `tag_lastoffset`

RockPod currently stores `play_time_seconds` and ISO-ish `last_played` strings
in `runtime_stats`. For lossless round-tripping, add device-native columns or a
raw numeric side table:

- `rockbox_playtime_ms`
- `rockbox_lastplayed_serial`
- `rockbox_lastelapsed_ms`
- `rockbox_lastoffset`
- `rockbox_commitid`

Display/UI code can continue converting to friendly timestamps and seconds.

### Sync and Merge Policy

Default policy: keep RockPod's device runtime state and Rockbox's tagcache
runtime state synchronized.

- On connect or before database generation, RockPod imports Rockbox runtime data
  into its per-device runtime store.
- When RockPod generates tagcache files, it writes that per-device runtime store
  back into `.rockbox/database_idx.tcd`.
- If an existing device tagcache value is greater than RockPod's cached runtime
  value, update RockPod's runtime store and keep the existing device value.
- If RockPod has a newer imported snapshot for the same device and track, write
  it back to Rockbox during host tagcache generation.
- For `playcount` and `playtime`, prefer monotonic max unless a full-device
  reset is explicitly requested.
- For `lastplayed`, use the largest Rockbox serial for that device database.
- For `lastelapsed` and `lastoffset`, use the latest matching snapshot, because
  these are positions rather than counters.
- RockPod library views should read this synchronized per-device runtime state
  so RockPod and Rockbox present the same playback history.
- Do not add unrelated desktop-player counts into Rockbox tagcache unless a
  separate opt-in import mode is added.

### Publish Safety

When runtime snapshot extraction fails:

- Do not publish a host-generated database over an existing database unless the
  user explicitly accepts losing runtime data.
- Prefer fallback to Rockbox auto-update.
- Surface the status as "Sync complete; database refresh deferred to preserve
  playback history."

When extraction succeeds:

- Build in a temporary root as today.
- Validate metadata and runtime numeric round-trip.
- Replace only host tagcache files.
- Preserve or remove `database_changelog.txt` deliberately:
  - preserve it if it contains unimported data
  - remove or archive it after successful import to avoid duplicate imports

## Proposed Rockbox Improvements

Rockbox should keep the current queued write model, but reduce loss windows:

- Force `run_command_queue(true)` on `SYS_REBOOT` in the tagcache thread or in
  the reboot path if it is not already guaranteed.
- Consider flushing the command queue immediately after `tagtree_track_finish_event()`
  when `runtimedb` or autoresume is active and the storage is already awake.
- Add a debug/log line for queue length on shutdown/USB to confirm runtime data
  flushes before mass storage exposure.
- Consider an optional lightweight journal for runtime numeric updates. A
  journal would let boot replay recent track-finish events after a crash without
  forcing synchronous writes on every track transition.

## Implementation Plan

1. Extend RockPod tagcache reader output to include runtime numeric fields.
   `read_rockbox_tagcache_tracks()` should return playcount, rating, playtime,
   lastplayed, lastelapsed, lastoffset, commitid, and dirty flag state.
2. Add a Rockbox changelog parser for quoted token lines:
   `filename="/Music/A.mp3" playcount="3" ...`.
3. Add runtime snapshot import before host database generation in
   `_post_sync_rockbox_integration()`.
4. Add a runtime-enriched inventory method and use it in
   `write_rockbox_tagcache_from_device_inventory()`.
5. Preserve and synchronize device-native runtime values in RockPod storage
   without converting away Rockbox serials and millisecond counters.
6. Add validation tests:
   - host rebuild preserves playcount/playtime/lastplayed
   - real Rockbox changelog syntax imports correctly
   - no-runtime snapshot blocks host publish over existing runtime data
   - second sync is idempotent
   - deleted/resynced/moved tracks preserve runtime when matched by path or
     unique metadata
   - RockPod views and Rockbox tagcache report the same runtime values after
     sync
   - Rockbox playlist exports still match RockPod playlists after music sync
7. Add a simulator workflow:
   - seed a simdisk with a tagcache containing nonzero runtime fields
   - run RockPod sync against the simdisk
   - verify generated `.rockbox/database_idx.tcd` still has those values
   - boot simulator and verify database opens without rebuild

## Acceptance Criteria

- Playing a track on Rockbox, connecting to RockPod, syncing one new album, and
  ejecting keeps existing playcount, playtime, lastplayed, and autoresume data.
- After that sync, RockPod shows the same device playback history that Rockbox
  will use on next boot.
- Repeating the same sync does not change runtime fields.
- Playlists synced from RockPod remain visible in RockPod and exported for
  Rockbox after adding music.
- Host generation refuses to overwrite an existing runtime-bearing database when
  runtime extraction fails.
- RockPod imports real Rockbox `database_changelog.txt` lines.
- Normal shutdown, USB connect, and software reboot flush queued runtime writes.
- Hard reset may lose the latest in-flight event, but must not reset the whole
  runtime history.

## Open Questions

- Should RockPod expose a separate user-facing import mode for unrelated desktop
  player history, or keep the default strictly to RockPod/Rockbox device runtime
  sync?
- Should RockPod archive imported changelogs for diagnosis, or delete them after
  successful import?
- Should Rockbox write a small runtime journal, or is forced queue flushing
  enough for the hardware targets in use?
