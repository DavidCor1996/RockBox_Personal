# RockPod Companion Music and Media Specification

## Product contract

RockPod Link treats the iPod as the authoritative offline playback library and
the iPhone as the network, analysis, conversion, and account host. Music
features must never stop playback, replace the active playlist, take the
shared audio buffer, or edit `database*.tcd` directly. New files are committed
through the acknowledged USB sync protocol and discovered by normal tagcache
auto-update.

The Music tab is useful without any server account. A connected Plex,
Jellyfin, Navidrome, Subsonic, or personal server adds remote discovery,
playlists, permitted downloads, release metadata, and listening-position
round trips.

## Shared catalog

The firmware exports `rockpod-library-v1` from tagcache through sync messages
11/12. Each row contains:

`path, title, artist, album, album_artist, genre, year, disc, track, length_ms,
bitrate_kbps, play_count, play_time_ms, last_played, rating, last_elapsed_ms,
last_offset, mtime`.

The response is read-only, request-scoped, capped at 16 MiB on the phone, and
chunked over the same retried USB transport used by media sync. The companion
stores the last catalog and derived reports in Application Support. It does
not package personal catalog data in the IPA or simulator install.

## Features

### Discover similar music

Local discovery ranks underplayed tracks using artist, genre, album, rating,
play count, and listening time. Remote discovery augments this with server
similar-artist/radio endpoints and MusicBrainz identifiers. Every result states
its reason and whether it is already on the iPod. Recommendations never start
playback automatically.

### Streaming-to-cache

The phone streams a remote track using its provider's authenticated URL. If
the provider marks downloads as permitted, the same response is staged to the
app cache, converted if necessary, tagged, and optionally queued to
`/Music/RockPodLink/<server>/<artist>/<album>/`. Temporary or DRM-protected
responses remain stream-only and are never presented as offline copies.

Cache records include provider, remote ID, ETag/modified time, license mode,
source checksum, output checksum, selected bitrate, and iPod destination.
Interrupted downloads resume when the server supports byte ranges; iPod
commits remain atomic through `.rpspart`.

### Playlist sync

Provider adapters normalize playlists to stable remote track IDs:

- Plex: server identity, library section, metadata/rating keys, playlist APIs.
- Jellyfin: user-scoped Items and Playlists APIs.
- Navidrome/Subsonic: Subsonic REST `getPlaylists`, `getPlaylist`, `stream`,
  and `download` APIs.
- Personal server: the documented RockPod JSON feed containing playlists and
  signed or directly authenticated media URLs.

The companion writes UTF-8 M3U8 files under `/Playlists/RockPodLink/` only
after all local path mappings are resolved. Sync modes are phone-to-iPod,
iPod-to-server, or merge. Conflicts use stable IDs and modified timestamps and
are shown before destructive changes.

### Storage-aware transcoding

Settings provide preferred quality (96, 128, 160, 192, 256, or 320 kbps), a
minimum free-space reserve, and `Keep original when supported`. The planner
uses current iPod free bytes, source sizes/durations, existing destination
files, and reserve space. AAC-LC/M4A is the default constrained-space output;
MP3 and supported originals may be copied unchanged. Output is verified as a
readable AV asset before USB transfer.

### Missing albums and duplicate cleanup

The offline report identifies gaps in numbered album tracks. When online,
MusicBrainz release tracklists confirm expected discs/tracks so a locally
truncated track list is not mistaken for a complete album. Duplicate groups
use normalized artist/title, duration tolerance, MusicBrainz IDs when
available, and optional checksums. Cleanup is report-only by default. Deletion
requires an explicit review and a fresh catalog snapshot.

### Releases, concerts, and notifications

Followed artists default to artists with listening history and can be edited.
Release checks use MusicBrainz release-group data. Concert checks use a
user-configured provider/location and date radius. Results are deduplicated by
stable IDs and synced to the iPod notification inbox. Per-app notification
settings remain authoritative, and quiet/background refresh failures do not
create alert spam.

### RockPod Replay

Replay combines tagcache play counts/play time/last-played values with
companion observations. It reports top tracks, artists, albums, genres,
listening time, rediscoveries, and year-over-year deltas. Snapshots are stored
per calendar year and can be exported as JSON without uploading listening
history.

### Audiobook position sync

Tracks are treated as audiobooks when tagged Audiobook/Spoken Word or stored
in an audiobook path. The newest valid position wins using source timestamp;
positions within 15 seconds are equal. Near-complete books are marked finished
instead of being resumed at the last frame. The firmware exports
`last_elapsed` and `last_offset`; companion/provider positions are sent back
through a dedicated fixed-path position manifest and applied using tagcache's
numeric update API, never by rewriting tagcache files.

## Persistence and security

Credentials are stored in iOS Keychain, never `NSUserDefaults`, logs, exported
reports, or the iPod. Server TLS validation is enabled by default. An explicit
per-account switch is required for a local self-signed server. Provider URLs
are normalized and credentials are redacted from UI errors.

## Delivery phases and verification

1. Catalog transport and offline analysis: protocol retry tests, malformed TSV
   tests, 2,800+ track catalog, reconnect, playback-active simulator run.
2. Provider adapters and playlist mapping: recorded provider fixtures, auth
   failure, pagination, conflict, inaccessible track, and path escaping tests.
3. Download/cache/transcode: interrupted range request, low-space planning,
   output decode verification, atomic iPod transfer, and database auto-update.
4. Release/concert notifications and audiobook round trip: stable-ID
   deduplication, notification toggles, clock skew, finish threshold, and
   two-way conflict tests.
5. Physical iPod matrix: Database and Files playback remain audible throughout
   catalog reads and sync, active playlist remains unchanged, tagcache files
   remain valid, reconnect survives large transfers, and safely disconnecting
   flushes every committed file.
