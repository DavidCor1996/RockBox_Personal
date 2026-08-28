# Removed RVP Device-Catalog Follow-up

## Status

Catalog and database cleanup completed on **2026-08-27**. A Netflix
browse/play smoke test remains to be performed after the iPod is safely
unmounted.

On 2026-08-27, 21 `.rvp` pointers and their 434 raw video/audio segment
sidecars were removed from the iPod. The deletion covered 455 files and freed
about 56.8 GiB on the device. The media payloads are gone, but stale metadata
was left in the device video catalog. Consequently, removed titles can still
appear in Netflix and selecting one can launch a missing path and crash the
plugin.

Do not infer physical device state from the desktop database alone. Before the
cleanup, the desktop database still marked 15 `.rvp` paths as present even
though all 21 pointers had been physically removed.

## Cleanup record

- Full device catalog backup:
  `/home/david/.rockpod/backups/videolist-before-rvp-prune-20260827-084345`
- Consistent database backup:
  `/home/david/.rockpod/backups/library-before-rvp-prune-20260827-084345.db`
- Removed 21 missing RVP rows from the active manifest, leaving 198 physical
  video rows.
- Removed the two deleted RVP paths from `netflix-watched.tsv`; all other
  watched history was preserved.
- Removed 130 unreferenced RVP thumbnails/posters (4,633,500 bytes) only after
  checksum-verifying each corresponding backup file.
- Marked all 15 cached RVP `device_tracks` rows absent and cleared the linked
  local sync flags without deleting any local track row. All 3,187 local track
  rows remain.
- Verified zero missing paths and zero `.rvp` references in the active device
  manifest, zero present RVP database rows, and `PRAGMA quick_check = ok`.
- Added a manifest regression test proving missing carried-over device rows
  are dropped while physically present device-only media remains preserved.

## Required fix

When the iPod is next mounted:

1. Back up `.rockbox/videolist` before modifying it.
2. Inspect `.rockbox/videolist/index.tsv` and remove every row whose device
   path is one of the 21 removed paths below.
3. Remove or regenerate orphaned derivatives for those rows from:
   - `.rockbox/videolist/thumbs`
   - `.rockbox/videolist/previews`
   - `.rockbox/videolist/netflix`
   - `.rockbox/videolist/netflix-detail`
   - `.rockbox/videolist/netflix-landing`
4. Prune matching stale path lines from
   `.rockbox/videolist/netflix-watched.tsv`, if present.
5. Mark the corresponding desktop `device_tracks` paths absent so a later
   catalog export cannot resurrect missing `.rvp` entries.
6. Preserve the local `tracks` rows and all source metadata: title, movie/TV
   kind, show, season/episode, content rating, artwork relationship, hidden and
   locked flags, and intro/credits markers.
7. Resync only titles with an existing local source as MPEG, preserving locked
   destinations and TV metadata. Rebuild the video/Netflix manifest from files
   that physically exist on the iPod.
8. Verify that none of the removed paths appears in Netflix before resync,
   every replacement points to an existing `.mpg`, and selecting every visible
   Netflix row exits cleanly without a plugin crash.

Do not delete `locked.pin` or clear the user's Netflix watched history as a
whole. Only stale rows associated with the paths below should be removed.

## Exact removed device paths

### Locked videos (6)

- `Videos/.Locked/An Innocent Foot Rub - Diane Andrews.rvp`
- `Videos/.Locked/Faked His Own Death.rvp`
- `Videos/.Locked/Favorite Guitarist - Angel Anarchy.rvp`
- `Videos/.Locked/Movie Night - Angel Anarchy.rvp`
- `Videos/.Locked/Stealing Your Husband Is Easy -Diane Andrews.rvp`
- `Videos/.Locked/Sugar Daddy Vacation - Angel Anarchy.rvp`

### Downloaded, movies, music videos, and RockPodLink (4)

- `Videos/Downloaded/Pitch.rvp`
- `Videos/Movies/Trailer Park Boys - The Movie.rvp`
- `Videos/Music Videos/Tree - Karma Police.rvp`
- `Videos/RockPodLink/Rags To Bitches-5eb38ef3/Rags To Bitches.rvp`

### TV episodes (11)

- `Videos/TV Shows/Hey Arnold!/Season 01/S01E01 - Episode 1 Hey Arnold FULL EPISODE RETRO RERUN.rvp`
- `Videos/TV Shows/Hey Arnold!/Season 01/S01E02 - Episode 2 _ Hey Arnold _ FULL EPISODE _ RETRO RERUN.rvp`
- `Videos/TV Shows/Kenny Hotz's Triumph of the Will/Season 01/S01E05 - The French Reconnection.rvp`
- `Videos/TV Shows/Kenny vs. Spenny/Season 01/S01E04 - Who Can Stand Up the Longest.rvp`
- `Videos/TV Shows/Kenny vs. Spenny/Season 01/S01E25 - Who Can Live in a Van the Longest.rvp`
- `Videos/TV Shows/Kenny vs. Spenny/Season 05/S05E04 - Who Can Piss Off More People.rvp`
- `Videos/TV Shows/My Name Is Earl/Season 01/S01E01 - Pilot.rvp`
- `Videos/TV Shows/My Name Is Earl/Season 01/S01E02 - Quit Smoking.rvp`
- `Videos/TV Shows/My Name Is Earl/Season 01/S01E03 - Randy's Touchdown.rvp`
- `Videos/TV Shows/Recess/Season 03/S03E07 - Space Cadet.rvp`
- `Videos/TV Shows/Recess/Season 04/S04E12 - Randall's Friends.rvp`

## Completion condition

This follow-up is complete only when the physical iPod contains no catalog row
that points to a missing file, replacement MPEG rows retain their metadata and
privacy flags, and a full Netflix browse/play smoke test produces no crash.
