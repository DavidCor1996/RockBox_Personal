# RockPod TODO

## Milestone 1 — Scaffolding, UI shell, library scan, artwork

### Done
- [x] Project scaffolding and directory structure
- [x] `app/config.py` — JSON-backed configuration
- [x] `app/database.py` — SQLite schema v1 with metadata hash columns
- [x] `models/track.py` — Track, DeviceTrack, compute_metadata_hash, compute_artwork_hash
- [x] `models/playlist.py` — Playlist, SmartPlaylistRule dataclasses
- [x] `services/metadata_reader.py` — mutagen-based extraction for all formats
- [x] `services/library_scanner.py` — Background QThread scanner
- [x] `services/artwork_manager.py` — Embedded + folder artwork, thumbnail cache
- [x] `services/device_detector.py` — Device polling, mock device, eject
- [x] `services/track_matcher.py` — 5-tier matching engine
- [x] `services/sync_engine.py` — SyncPlan builder, SyncWorker, crash-safe copy
- [x] `ui/styles.py` — iTunes 7-era QSS stylesheet
- [x] `ui/sidebar.py` — Source list with Library/Devices/Playlists sections
- [x] `ui/toolbar.py` — Scan/Sync/Eject + capsule search field
- [x] `ui/track_table.py` — Track list model + view with 12 columns
- [x] `ui/status_bar.py` — Song count / duration / size display
- [x] `ui/storage_bar.py` — Custom-painted device capacity meter
- [x] `ui/column_browser.py` — Genre/Artist/Album tri-pane browser
- [x] `ui/dialogs/metadata_editor.py` — Get Info dialog
- [x] `ui/dialogs/preferences.py` — General/Sync/Device/Advanced tabs
- [x] `ui/dialogs/sync_dialog.py` — Sync plan summary + progress
- [x] `ui/main_window.py` — Full integration, menu bar, signal wiring
- [x] `main.py` — Entry point with CLI args
- [x] `tests/conftest.py` — Shared fixtures
- [x] `tests/test_metadata.py` — Metadata hash + resync tests
- [x] `tests/test_library.py` — Database CRUD tests
- [x] `tests/test_matcher.py` — Track matching tests
- [x] `tests/test_sync.py` — Sync engine tests
- [x] `README.md`
- [x] `TODO.md`

### Remaining
- [ ] Integration test — verify app launches end-to-end without errors
- [ ] Verify all imports resolve; fix any runtime bugs found during first launch

---

## Milestone 2 — Mock device, compare, sync, storage bar

- [ ] End-to-end sync test: scan library -> build plan -> execute sync -> verify files on mock device
- [ ] Storage bar updates live after sync completes
- [ ] "Not on iPod" sidebar count updates after sync
- [ ] Sync progress reporting in status bar during sync
- [ ] Handle edge cases: 0-byte files, corrupt metadata, permission errors
- [ ] Error dialog for sync failures

---

## Milestone 3 — Real device, metadata editor, playlists

- [ ] Test with real iPod Classic 6G hardware
- [ ] Test with real iPod Video 5G hardware
- [ ] Inline metadata editing in track table (double-click to edit)
- [ ] Multi-track Get Info (select multiple, edit shared fields)
- [ ] Drag-and-drop from library to playlists
- [ ] Drag-and-drop to device sidebar item to queue sync
- [ ] Playlist import/export (.m3u, .m3u8)
- [x] Playlist sync to device as `.m3u8` files

---

## Milestone 4 — Smart playlists, browser views, polish, prefs, eject

- [ ] Smart playlist rule evaluation engine
- [ ] Smart playlist auto-refresh on library changes
- [ ] Album art browser view (grid of album covers)
- [ ] Rockbox database generation/refresh support
- [ ] Keyboard shortcuts (Cmd+I for Get Info, Delete to remove, Space to play)
- [ ] Context menu on track table (Get Info, Reveal in Finder, Remove, Sync)
- [ ] Mini-player or playback preview (optional)
- [ ] Safe eject with sync-before-unmount confirmation
- [ ] Window state persistence (geometry, column widths, sort order)
- [ ] macOS portability testing and DMG packaging
- [ ] Performance profiling for large libraries (10k+ tracks)
- [ ] Watchdog-based automatic library rescan on file changes
