# RockPod

An iTunes 7-era desktop music manager for Rockbox iPods. Built with PySide6 (Qt),
Python, SQLite, and mutagen.

RockPod recreates the look and feel of iTunes 7 (2006-2007) — brushed-metal
toolbar, glossy sidebar, aqua scrollbars, alternating-row track table — while
targeting Rockbox devices instead of Apple's firmware.

## Status

**Milestone 1 (~90% complete):** Library scanning, metadata extraction, artwork
caching, mock device mode, sync engine with metadata fingerprint change
detection, full UI shell. Not yet tested on real hardware.

## Requirements

- Python 3.10+
- PySide6 >= 6.5
- mutagen >= 1.47
- Pillow >= 10.0
- watchdog >= 3.0
- pytest >= 7.0 (dev)
- ffmpeg (required for video thumbnails and Android media import)

## Setup

```bash
cd rockpod
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Running

```bash
# Normal launch (scans ~/Music by default)
python main.py

# Launch with mock device (no iPod needed)
python main.py --mock

# Override music directory
python main.py --music-dir /path/to/music

# Mock device at a specific path
python main.py --mock-path /tmp/my_mock_ipod

# Verbose logging
python main.py -v

# All options
python main.py --help
```

### Mock device mode

Pass `--mock` to create a fake Rockbox device directory under
`~/.rockpod/mock_device/`. This lets you test the full scan/match/sync workflow
without connecting a real iPod. The mock device includes `.rockbox/`,
`rockbox-info.txt`, `config.cfg`, and an empty `Music/` directory.

You can also point `--mock-path` at any directory — RockPod will create the
Rockbox directory structure there.

## Running tests

```bash
source .venv/bin/activate
python -m pytest tests/ -v
```

## SNES Lite game sync

The Games manager recognizes user-provided `.sfc` and `.smc` files as
`Super Nintendo`. It reads the internal title and mapper header, hashes the
ROM, flags known unsupported enhancement chips, prepares device-sized covers,
and includes the SNES Lite plugin, launcher metadata, configuration, selected
ROMs, and save handling in its normal reviewed sync plan. RockPod does not
download or bundle ROMs. SRAM is stored in `/.rockbox/saves/snes/` and is not
deleted automatically when a ROM is removed.

The iPod destination is `/.rockbox/roms/snes/`; console/game metadata and
covers use the existing `/.rockbox/games/library/` conventions consumed by the
iPod JS Games launcher. See `../docs/snes-lite-port-spec.md` for limits and the
hardware validation checklist.

## Android phone import

RockPod can import photos and videos from a mounted Android phone directly to a
connected Rockbox iPod.

1. Mount the phone on your desktop so its storage is visible.
2. In RockPod, open `Preferences > Device > Android Import` and optionally set
   the Android storage root and iPod target folder.
   Enable `Format imported clips for the iPodTikTok plugin` if you want the
   import to target `/Videos/iPodTikTok` and regenerate the plugin feed file.
3. Connect the iPod.
4. Use `Device > Import Android Photos/Videos...`.

Videos are converted to Rockbox-friendly MPEG files. Photos are converted into
short slideshow-style video clips so they can be viewed on the iPod video
player path as well.

## Device reconciliation diagnostics

For real-device matching problems, generate a local-vs-device report:

```bash
python scripts/reconcile_device.py --device-path /run/media/$USER/IPOD --limit 25
```

The report prints local track count, scanned device track count, matched,
unmatched, resync-needed, and examples of unmatched tracks with reasons such as
title mismatch, artist mismatch, album mismatch, duration mismatch, parse
failure, unsupported format, or metadata incomplete. Add `--force` to re-read
device metadata instead of reusing the cached device inventory.

## Library reconciliation diagnostics

For library indexing problems, compare the Music folder against SQLite:

```bash
python scripts/reconcile_library.py --music-dir ~/Music --limit 25
```

The report prints supported audio files found on disk, tracks present in the
DB, files missing from the DB, stale DB paths, skipped files such as
unsupported extensions, and folders with partial or fully missing imports.

## Library format coverage

RockPod scans these library formats by extension: `mp3`, `flac`, `m4a`, `aac`,
`alac`, `ogg`, `opus`, `wav`, `aiff/aif`, `wma`, `ape`, and `wv`.

Metadata extraction uses Mutagen where available. In practice:

- `mp3`, `flac`, `m4a/aac`, `ogg`, and `opus` are the primary fully tagged paths
- `alac` is handled through the MP4 tag reader path
- `wav` and `aiff/aif` are indexed, but tag coverage depends on how metadata was written
- if metadata parsing fails, RockPod still indexes the file with basic fallback metadata
- if artwork extraction fails, RockPod indexes the track and falls back to placeholder art

Test modules:

| File                  | Covers                                               |
|-----------------------|------------------------------------------------------|
| `test_metadata.py`    | Metadata hash computation, artwork hash, needs_resync |
| `test_library.py`     | SQLite CRUD, sync status, playlists, device tracks    |
| `test_matcher.py`     | 5-tier track matching, resync detection, orphans      |
| `test_sync.py`        | Sync plan building, file copy, cancel, DB updates     |

## Architecture

```
rockpod/
  main.py                  Entry point
  app/
    config.py              JSON-backed settings
    database.py            SQLite schema + CRUD
  models/
    track.py               Track/DeviceTrack dataclasses, hash functions
    playlist.py            Playlist/SmartPlaylistRule dataclasses
  services/
    metadata_reader.py     mutagen-based tag extraction
    library_scanner.py     Background QThread music folder scanner
    artwork_manager.py     Embedded + folder art extraction, thumbnails
    device_detector.py     Device polling, mock device, eject
    device_inventory.py    Cached per-device SQLite inventory verification
    playback.py            Local playback queue and QtMultimedia backend
    track_matcher.py       5-tier matching engine
    sync_engine.py         SyncPlan builder, SyncWorker, file copy
    theme_assets.py        Optional user-supplied iTunes-style asset packs
  ui/
    styles.py              iTunes 7-era QSS stylesheet
    main_window.py         Main window wiring all components
    sidebar.py             Source list (Library/Devices/Playlists)
    toolbar.py             Scan/Sync/Eject + search bar
    track_table.py         Track list model + view
    status_bar.py          "342 songs, 1.2 hours, 4.53 GB"
    storage_bar.py         Device capacity meter
    device_summary.py      Classic iTunes-style iPod Summary screen
    column_browser.py      Genre/Artist/Album tri-pane
    dialogs/
      metadata_editor.py   Get Info dialog
      preferences.py       Settings dialog
      sync_dialog.py       Sync plan + progress
  tests/
    conftest.py            Shared fixtures
    test_*.py              Test suites
```

### Metadata sync

RockPod uses metadata fingerprinting to detect changes rather than relying on
file timestamps or filenames:

- **metadata_hash**: SHA-256 of title, artist, album, album_artist,
  track_number, disc_number, genre, year, composer, duration, bitrate, codec
- **artwork_hash**: Separate SHA-256 of embedded artwork bytes
- **file_hash**: SHA-256 of the file content itself

On sync, if a track's `metadata_hash` differs from `last_synced_metadata_hash`,
the file is re-copied to the device. This avoids duplicates — the existing
device file is replaced, not duplicated.

When enabled in Preferences, RockPod also exports local playlists to
`.rockbox/playlists/RockPod/*.m3u8` after a successful sync. Only tracks that
are present on the current device are written, and the generated device
playlists are then re-imported into the sidebar cache.

### Device layout

Files are copied to `Music/{album_artist}/{album}/{track_number} - {title}.ext`
on the device. No iTunesDB is needed; Rockbox reads files directly from the
filesystem.

### Local playback

RockPod includes local library playback through a dedicated playback service.
Double-click a local track, or press Enter on a selected track, to play it.
Next/Previous operate within the active view context: Music, playlist, artist,
album, or genre. The toolbar now-playing strip shows artwork, title, artist,
album, elapsed/total time, seek progress, and volume.

Playback uses QtMultimedia when available. Codec support depends on the Qt
multimedia backend and system codecs. Device-side playback and streaming are not
implemented.

### Device inventory cache

RockPod stores remembered iPod inventories in SQLite. Each connected device is
identified by a stable key derived from Rockbox/device signature data instead of
the current mount path. On reconnect, the UI loads cached device tracks
immediately, then verifies the filesystem in a background worker. Verification
updates new, changed, and missing files for that device only, so multiple iPods
keep independent "On Device" and "Not On iPod" state.

Use **Device > Force Device Rescan** only for recovery/debugging. Normal startup
and reconnects use cached state first.

### Rockbox runtime data and smart playlists

When Rockbox runtime export data is present, RockPod imports it into separate
runtime tables and keeps that provenance distinct from local library tags. The
current importer looks for ASCII exports such as
`.rockbox/database_changelog.txt` and maps play count, last played, rating, and
play time onto local and cached device tracks only when the match is confident.

Built-in saved smart playlists include:

- Most Played
- Least Played
- Never Played
- Recently Played
- Not Played In 30 Days
- Highest Rated
- Rating >= 3
- Recently Added and Unplayed
- On iPod and Played Recently
- Not On iPod and Frequently Played

Device-specific rules degrade gracefully when no current device is available,
and runtime-backed rules fall back to library values when no runtime import has
been recorded.

### iPod Summary

Selecting the iPod itself in the Devices source list opens a classic iTunes-style
Summary screen. It loads from cached per-device state immediately, showing device
name, capacity, free space, Rockbox status, last sync, last verification, track
count, sync options, actions, and the segmented Music/Other/Free capacity bar.
Selecting **On This iPod** still opens the device music track table, and **Not on
iPod** opens the missing-track filter.

### Local-only iTunes-style personal theme

RockPod does not ship proprietary Apple assets. The built-in fallback theme
lives under `assets/theme_default/`. For personal local use, RockPod can also
load a user-supplied iTunes-era pack from `assets/theme_itunes_personal/`.

That personal pack is local-only and gitignored. Drop files there manually.
RockPod never downloads assets at runtime and does not bundle Apple proprietary
artwork by default.

Personal theme layout:

- `assets/theme_itunes_personal/theme.json` is required
- optional subdirectories include `branding/`, `chrome/`, `toolbar/`,
  `sidebar/`, `playback/`, `table/`, `statusbar/`, `device/`, `icons/`,
  and `artwork/`
- optional files include `branding/title.png`, `toolbar/sync.png`,
  `toolbar/refresh.png`, `toolbar/new_playlist.png`, `playback/play.png`,
  `playback/pause.png`, `playback/previous.png`, `playback/next.png`,
  `sidebar/music.png`, `sidebar/artists.png`, `sidebar/albums.png`,
  `sidebar/genres.png`, `sidebar/playlist.png`, `sidebar/device.png`,
  `table/header.png`, `table/selection.png`, `statusbar/background.png`,
  `statusbar/storage_frame.png`, `device/summary_header.png`,
  `device/ipod_icon.png`, `artwork/album_placeholder.png`,
  and `artwork/album_frame.png`

If any personal asset is missing, RockPod falls back to the matching file in
`assets/theme_default/`. If `assets/theme_itunes_personal/theme.json` is
missing or invalid, selecting the personal theme falls back to the default
theme without crashing.

Supported personal theme asset types:

- title/logo graphic
- toolbar button images
- playback transport buttons
- sidebar/source-list icons
- toolbar/search field slices
- table header and selection textures
- sidebar selection texture
- status bar background and divider
- storage bar frame
- device summary header and device icon
- album placeholder and frame art

### How To Extract iTunes 2007 Assets

Recommended versions:

- `7.0` for the original September 12, 2006 iTunes 7 look
- `7.0.2` as a practical default if you want the early visual style
- `7.4` for the late-2007 iPhone / iPod touch era while staying in iTunes 7
- `7.6.2` for the last 7.x package before the iTunes 8 redesign

Manual source references:

- Apple announced iTunes 7 on September 12, 2006:
  https://www.apple.com/newsroom/2006/09/12Apple-Announces-iTunes-7-with-Amazing-New-Features/
- Apple announced iTunes 7.4 on September 5, 2007:
  https://www.apple.com/newsroom/2007/09/05Apple-Unveils-the-iTunes-Wi-Fi-Music-Store/
- Apple still maintains a legacy iTunes-for-Windows software index:
  https://support.apple.com/en-us/docs/software/pl296
- Common archive references for real 7.x installers:
  https://www.oldversion.com/software/itunes/itunes-7-0/
  https://www.oldversion.com/windows/itunes-7-0-2
  https://www.oldversion.com/software/itunes/itunes-7-6-2/
- Community visual cross-check for an extracted iTunes 7 icon:
  https://theapplewiki.com/wiki/File:ITunes_7_icon.png

Windows extraction:

1. Download an iTunes 7.x installer manually.
2. Extract the installer locally with `7z x iTunesSetup.exe -oitunes_extracted`.
3. If an MSI is present, extract that too with `7z x iTunes.msi -oitunes_msi`.
4. Inspect `iTunes.exe`, resource DLLs, MSI payload folders, ICO/BMP/PNG files,
   and any image-heavy resource directories.
5. If the assets are still embedded in executables or DLLs, use a local resource
   browser such as Resource Hacker to export icons, bitmaps, PNGs, dialogs, and
   related resources.
6. You can also run `rockpod extract-itunes-assets /path/to/iTunesSetup.exe`.
7. Run `rockpod import-itunes-assets /path/to/extracted/assets`.

macOS extraction:

1. Obtain a local copy of an iTunes 7.x-era `iTunes.app` manually.
2. Open `iTunes.app/Contents/Resources/`.
3. Inspect `.icns`, `.png`, localized `.lproj` folders, nib resources, and any
   toolbar/sidebar artwork under `Resources/`.
4. You can also run `rockpod extract-itunes-assets /path/to/iTunes.app`.
5. Export the assets you want to a normal working directory if needed.
6. Run `rockpod import-itunes-assets /path/to/extracted/assets`.

Expected asset locations after manual extraction:

- Windows: MSI payload folders, `iTunes.exe`, resource DLLs, exported ICO/BMP/PNG
  resources, and image assets pulled from PE resources
- macOS: `iTunes.app/Contents/Resources/`, `.lproj` folders, `.icns` files,
  nib-adjacent resources, and toolbar/sidebar image files

### Asset Commands

RockPod includes local tooling for manual asset workflows:

- `rockpod find-itunes-assets`
  Prints recommended versions, manual sources, and extraction steps.
- `rockpod extract-itunes-assets /path/to/iTunesSetup.exe`
  Extracts image files from a local installer, app bundle, or already-extracted
  directory into a staged folder and reports embedded EXE/DLL resource candidates.
- `rockpod review-itunes-assets /path/to/extracted/assets`
  Builds an HTML review bundle and contact sheets so you can classify anonymous
  `qtr` and PE-extracted assets locally before import.
- `rockpod import-itunes-assets /path/to/extracted/assets`
  Scans a local extraction directory, maps recognized files into
  `assets/theme_itunes_personal/`, and rewrites `theme.json`.
- `rockpod validate-theme`
  Reports which personal theme assets are present, which are missing, and how
  complete the current theme is.

Research notes:

- Apple announced iTunes 7 on September 12, 2006 and highlighted album/Cover
  Flow browsing as a major visual reference:
  https://www.apple.com/newsroom/2006/09/12Apple-Announces-iTunes-7-with-Amazing-New-Features/
- Wired's iTunes 7 screenshot set is useful for source-list, device page, album
  view, and search-field references:
  https://www.wired.com/2006/09/itunes-7-screenshots/
- Apple's current iTunes download page links users toward current Windows
  downloads and previous-version support, but RockPod treats any old installer
  extraction as user-provided local input, never a bundled dependency:
  https://www.apple.com/itunes/download/

## Configuration

Settings are stored in `~/.rockpod/config.json`. Key options:

- `music_dir` — Local music library path (default: `~/Music`)
- `resync_metadata_changes` — Re-copy tracks when tags change (default: true)
- `force_full_resync` — Re-copy everything on next sync (default: false)
- `device_music_template` — Device folder layout template
- `device_file_template` — Device filename template
- `sync_playlists_to_device` — Export local playlists to Rockbox `.m3u8` files
- `mock_device_enabled` / `mock_device_path` — Mock device mode
- `theme_mode` — `default` or `personal`

## License

GPLv2 (consistent with the parent Rockbox project).
