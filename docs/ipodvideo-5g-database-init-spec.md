# iPod Video 5G Database Initialization Spec

## Goals

- Initialize the Rockbox music database automatically when it is missing or invalid.
- Keep boot responsive while still making Music ready by the time the user opens it.
- Auto-update the database when new songs are added.
- Avoid any code path that deletes user music files.
- Make scan/commit progress visually clear and consistent with the iPone theme.

## Non-Goals

- Do not alter, rewrite, move, or delete files under `/Music`.
- Do not scan photo, video, game, system, or recovered filesystem folders when `/Music` scan roots are configured.
- Do not require manual database initialization for normal use.

## Behavior

- On boot, tagcache checks existing database headers.
- If the database is missing or invalid and no commit is delayed, Rockbox starts a fresh database build automatically.
- If a scan finds no new music and an existing database is ready, Rockbox removes only the temporary database file and reports the scan as up to date.
- If new tracks are found, Rockbox writes a temporary database and commits it into the normal tagcache index files.
- The default database auto-update setting is enabled so added songs are found on subsequent scan/update cycles.

## User Interface

- Boot and Music-entry progress uses a modern rounded progress bar on color screens.
- Commit progress shows the current commit stage, such as artists, albums, tracks, artist map, or track data.
- Scan status is tracked as idle, building, updating, committing, or up to date.
- The debug database screen exposes the current scan status for troubleshooting.

## Safety

- Database rebuild removes only Rockbox tagcache database files and temporary tagcache files.
- Music files are read for metadata only.
- iPod deployment must copy Rockbox runtime files only unless the user explicitly requests music file changes.
- Simulator tests should use copied simdisk data under `/tmp` and must replace any host music symlink with an isolated test `/Music` directory.

## Validation

- Build the iPod Video 5G simulator.
- Smoke-test first boot with no database and an empty isolated `/Music` folder.
- Smoke-test first boot with a generated MP3 in isolated `/Music`; verify Rockbox reads metadata and finalizes database index files.
- Run whitespace checks on touched source files before commit.
