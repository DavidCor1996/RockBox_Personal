# RockPod Store Jailbreak App Specification

## Product contract

RockPod Store is a standalone jailbreak application for the iPod touch 4G.
It reproduces the current RockPod desktop Music Store experience on the
device, while the existing RockPod installation on the computer remains the
catalog, account, import, and library authority.

The first release targets both operating systems in the current dual-boot
setup:

- iPod touch 4G (`iPod4,1`, `N81AP`), armv7;
- iOS 6.1.6 primary;
- iOS 7.1.2 secondary;
- 3.5-inch Retina display, 320 x 480 UIKit points;
- rootful jailbreak application package.

"Same music store page and logic" means behavioral and visual parity with
`rockpod/ui/web_browser.py`, backed by the same `StreamripImporter`, store
search script, streamrip state directory, music library, duplicate detection,
and import scanner used by desktop RockPod. The desktop page is PyQt rather
than HTML, so its widget code cannot literally run on iOS; the device UI is a
faithful native UIKit rendering of the same store contract.

"Same TIDAL account" means all TIDAL operations execute on the RockPod host
with the already-authenticated streamrip configuration and XDG state. TIDAL
access tokens, refresh tokens, passwords, and streamrip configuration are not
copied to the iPod.

The app is not a MobileSubstrate tweak and does not hook SpringBoard, Music,
or any system daemon. It is a normal UIKit application installed through the
jailbreak package system.

## First release

The first release must provide:

- pairing with one RockPod desktop host on the local network;
- TIDAL home tabs matching RockPod: Featured, New Releases, Top Albums, Just
  Added, Alternative, Rock, and Hip-Hop/Rap;
- album search, with TIDAL selected by default and optional Qobuz/Deezer
  sources when those accounts are configured on the host;
- the same normalized album cards and album-detail track list;
- album and individual-track Buy actions;
- Owned state based on the current RockPod library;
- the existing FLAC, ALAC, and MP3 import choices for the host library;
- import progress, completion, failure, retry, and duplicate messages;
- optional download of an iPod-compatible copy into RockPod Store's own local
  library;
- preview playback when the catalog result includes a preview URL;
- cached home/search/detail responses for read-only offline viewing;
- no dependency on the iPod Classic or its mounted filesystem.

Live catalog and Buy operations require the paired RockPod host to be running.
The app must explain this clearly instead of showing an empty or permanently
loading page.

## Deliberate non-goals

The first release does not:

- authenticate directly with TIDAL from iOS;
- ship Python, streamrip, or provider credentials in the application;
- bypass provider DRM or entitlement checks;
- modify the dual-boot partitions or boot chain;
- write to the iOS MediaLibrary/iTunes database;
- place downloaded tracks in Apple's Music app;
- sync anything to an iPod Classic;
- provide background downloads after iOS terminates the app;
- add social sharing, movies, games, or the other desktop Store tabs.

Device copies live in RockPod Store's application data and play inside RockPod
Store. Apple Music-library integration can be evaluated separately only after
a safe, version-specific database strategy exists.

## Architecture

The product has two components:

```text
RockPod Store on iPod touch
        |
        | versioned JSON API + ranged media transfer
        v
RockPod desktop Store service
        |
        +-- existing StreamripImporter
        +-- existing streamrip_store_search.py
        +-- existing streamrip auth/XDG state
        +-- existing RockPod music directory and database scan
```

### iOS client

The app is written in Objective-C and UIKit with iOS 6-compatible APIs. It is
responsible for presentation, pairing, caching, previews, job monitoring,
resumable device-copy transfer, and playback of local device copies.

Use `NSURLConnection`, not `NSURLSession`, for the common iOS 6/7 networking
path. Use `NSJSONSerialization`, Security/Keychain, `AVAudioPlayer` or
`AVPlayer`, and normal Foundation file APIs. Every iOS 7-only call must be
availability-guarded and have an iOS 6 path.

### RockPod host service

The host adds a small HTTP JSON service. It is a transport adapter over the
existing store services, not a second implementation of TIDAL logic.

The service must:

- call `StreamripImporter.prepare_store_homepage()` for home tabs;
- call `StreamripImporter.prepare_album_search()` for search;
- call `StreamripImporter.prepare_album_detail()` for album detail;
- call `StreamripImporter.prepare_import()` for Buy jobs;
- launch requests with the importer's `env()` so the current streamrip XDG
  state and logged-in TIDAL session are reused;
- apply existing URL validation, source quality caps, file discovery, cover
  creation, duplicate matching, and library rescan behavior;
- permit only one active streamrip import initially, matching the desktop
  application's current concurrency rule;
- serialize all public results into stable API objects rather than exposing
  subprocess paths or raw provider payloads.

UI-owned duplicate and library matching logic currently in
`rockpod/ui/main_window.py` should be extracted into a service-level helper so
the desktop page and API use one implementation. The iOS client must never
decide Owned state from album title alone.

## Account and pairing model

The RockPod host has one setting: `Enable RockPod Store companion`. Enabling
it starts the service and advertises `_rockpod-store._tcp` through Bonjour.
The settings page shows the host name, address, port, one-time six-digit
pairing code, and a manual refresh button for that code.

First pairing:

1. The app discovers hosts through Bonjour or accepts a manually entered
   `http[s]://host:port` address.
2. The user enters the six-digit code shown by RockPod.
3. The host exchanges it once for a random 256-bit client token.
4. The app stores the token in iOS Keychain under service
   `com.rockpod.store.host`.
5. The host stores only a hash of the token, a client name, creation time, and
   last-used time.

The pairing code expires after ten minutes or one successful use. Five failed
attempts from one address trigger a ten-minute cooldown. The persistent token
can be revoked from RockPod.

Every authenticated request uses:

```text
Authorization: Bearer <client-token>
X-RockPod-Store-Version: 1
```

The app stores the host pairing token only. It never receives TIDAL secrets.
`GET /v1/account` reports status such as `tidal: ready` or
`tidal: login_required`, but never account tokens or the full streamrip config.

The listener defaults to disabled. Once enabled, it binds only to explicitly
selected private/local interfaces. HTTPS is preferred. Plain HTTP requires a
visible `Allow unencrypted local-network pairing` switch and must be rejected
for non-private destination addresses.

## Store API v1

All JSON replies use UTF-8 and this envelope:

```json
{
  "api_version": 1,
  "request_id": "uuid",
  "data": {},
  "error": null
}
```

Errors replace `data` with `null` and contain stable `code`, human-readable
`message`, and optional `retry_after_seconds`. Provider stack traces, local
paths, commands, tokens, and raw subprocess output are never sent to the app.

### Discovery and account

```text
GET  /v1/status
POST /v1/pair
GET  /v1/account
```

`/v1/status` is unauthenticated and exposes only service name, API version,
pairing availability, and server time. `/v1/account` returns:

```json
{
  "providers": {
    "tidal": {"state": "ready"},
    "qobuz": {"state": "not_configured"},
    "deezer": {"state": "not_configured"}
  },
  "preferred_format": "flac",
  "allowed_formats": ["flac", "alac", "mp3"],
  "device_copy_format": "aac",
  "device_copy_bitrate_kbps": 256
}
```

### Catalog

```text
GET /v1/store/home?tab=featured&limit=24
GET /v1/store/search?source=tidal&type=album&q=<query>&limit=24
GET /v1/store/albums/<source>/<album-id>
GET /v1/store/artwork/<artwork-id>
```

Valid home tabs are exactly:

```text
featured, new_releases, top_albums, just_added,
alternative, rock, hip_hop
```

Homepage requests always use the logged-in TIDAL home page, preserving the
current `pages/home` behavior and section fallbacks. Search and detail support
TIDAL, Qobuz, and Deezer only, matching the current service.

Album objects preserve the current normalized fields and add server-owned
state:

```json
{
  "source": "tidal",
  "media_type": "album",
  "id": "123",
  "title": "Album",
  "artist": "Artist",
  "date": "2026-01-01",
  "tracks": 10,
  "url": "https://tidal.com/album/123",
  "artwork_url": "/v1/store/artwork/abc",
  "section": "Popular albums",
  "owned": false
}
```

Album detail adds ordered `track_items`:

```json
{
  "source": "tidal",
  "media_type": "track",
  "id": "456",
  "title": "Track",
  "artist": "Artist",
  "track_number": 1,
  "disc_number": 1,
  "duration": 213,
  "url": "https://tidal.com/track/456",
  "preview_url": "https://example.invalid/preview",
  "owned": false
}
```

The host proxies cached artwork instead of exposing its local `cover_path`.
Artwork responses use ETag and a maximum 320 x 320 JPEG representation. The
client accepts a missing image and shows the existing two-letter fallback.

### Imports

```text
POST   /v1/imports
GET    /v1/imports
GET    /v1/imports/<job-id>
POST   /v1/imports/<job-id>/cancel
GET    /v1/imports/<job-id>/files
GET    /v1/imports/<job-id>/files/<file-id>
```

Create body:

```json
{
  "source": "tidal",
  "media_type": "album",
  "id": "123",
  "url": "https://tidal.com/album/123",
  "host_format": "flac",
  "destination": "host_and_device"
}
```

Valid destinations are `host_library` and `host_and_device`. A device copy is
always derived from a successfully imported, verified host file; it is not a
second provider download.

Job states are:

```text
queued, checking, importing, scanning, preparing_device_copy,
ready, completed, duplicate, cancelling, cancelled, failed
```

The job response includes item identity, state, phase label, completed track
count, expected track count when known, byte progress when known, timestamps,
retryability, device files, and a sanitized final message. The host retains
the last 50 job summaries and removes device-transfer artifacts after seven
days or after every requested file is acknowledged, whichever is later.

Duplicate detection runs before import using the existing provider metadata
tag check and normalized library match. A duplicate returns state `duplicate`
and the normal `Already in library` message. It is a successful terminal
result, not HTTP 500.

Cancellation is best-effort. It terminates only the child import/transcode
process associated with that job, retains already valid host files, scans
them, and reports exactly what was kept.

Device files support HTTP Range requests, ETag, content length, SHA-256 in the
manifest, and an explicit `Content-Disposition` filename. Paths are server
generated; the client never submits or receives arbitrary host filesystem
paths.

## Buy behavior

The blue action remains labelled `Buy` for parity with the desktop page, but
it does not represent an App Store or monetary transaction. It means import
content available to the user's configured provider account through the
existing RockPod path.

The first tap presents two choices:

- `Add to RockPod Library` — exact desktop behavior;
- `Add + Download to This iPod` — same host import plus a compatible device
  copy.

The chosen action may be saved as the default. Holding the Buy button or using
the detail action menu always shows both choices. Owned disables a redundant
import but offers `Download Device Copy` when the host owns the item and no
local device copy exists.

For `host_and_device`, RockPod keeps its normal host format preference. Device
copies are AAC-LC in M4A at 256 kbps by default, with MP3 192/256/320 kbps as
an app setting. ALAC pass-through is allowed when the source is already ALAC
and free-space planning succeeds. FLAC is never offered as the default device
copy because the iOS 6/7 system playback path is not the compatibility target.

The host validates every device copy as a readable audio asset before marking
it ready. The app downloads to a `.rpspart` file, verifies content length and
SHA-256, then atomically renames it.

## On-device user interface

The visual language follows the current iTunes Store 2006-2008 shell:

- blue-grey gradient chrome;
- white content panels with restrained borders;
- dark selected category pills;
- glossy blue Buy buttons;
- dark blue album titles and grey metadata;
- square cover art with a two-letter fallback;
- the same store terminology and status messages.

The UI must be native and scroll smoothly on 256 MB hardware. It may reproduce
the desktop gradients with stretchable one-pixel assets or `CAGradientLayer`;
it must not ship a modern web application inside `UIWebView`.

### Navigation

A four-item bottom tab bar contains:

- Store;
- Downloads;
- Library;
- Settings.

The Store navigation stack contains home/search results and album detail.
Returning from detail restores the previous tab, result set, and scroll
position.

### Store screen

At 320 points wide, the desktop page adapts as follows:

- navigation title: `RockPod Store`;
- compact search field plus Search button;
- provider selector in a small action sheet, default TIDAL;
- horizontally scrolling home-tab strip;
- compact hero showing the current TIDAL section and up to four covers;
- two-column album grid;
- `Top Downloads` becomes a full-width list below the first album rows rather
  than a desktop sidebar;
- pull-to-refresh on iOS 6/7 through a custom refresh header;
- persistent one-line host/account status beneath the navigation bar.

Home chrome follows current logic:

- kicker: `TIDAL Store - <Tab>`;
- headline: first result section when present, otherwise the tab header;
- subhead: `<Tab> albums from TIDAL with one-click Buy imports.`;
- header: `Popular and New on TIDAL` for Featured, otherwise
  `<Tab> on TIDAL`.

Each album card shows artwork, title, artist, section/source, release year,
track count, and Buy/Owned. Tapping anywhere except Buy opens detail.

### Album detail

The detail screen shows:

- Back button;
- 150-point artwork where space permits, reduced to 120 points on iOS 6 with
  large accessibility text;
- title, artist, source, release date, and track count;
- ordered multi-disc track rows;
- duration in `m:ss`;
- Preview/No Preview;
- Buy/Owned for each track;
- Buy Album/Owned beneath the album metadata.

Only one preview may play at a time. Preview playback stops when the user
starts another preview, starts a local device track, leaves the app, unpairs,
or begins a phone-call/audio interruption. It must not auto-advance into a
full provider stream.

### Downloads

Downloads lists active and recent jobs with:

- album/track name and artwork;
- host import phase;
- track count and byte progress when available;
- host-only or host-and-device destination;
- Cancel, Retry, and Remove Local Copy actions where valid;
- exact failure message plus a short recovery action.

Polling begins at one second while a job changes rapidly, backs off to five
seconds during long imports, and stops for terminal jobs. Reopening the app
reconciles jobs with the host and resumes incomplete ranged downloads.

### Library

Library is the app-local device-copy library, not the host's full library. It
groups tracks by album and supports play, pause, seek, previous/next, delete,
and `Show on Host`. Deleting a device copy never deletes the RockPod host file.

### Settings

Settings contains:

- paired host and connection state;
- reconnect, change host, and forget host;
- TIDAL status from the host, without credentials;
- default Buy destination;
- host import format: FLAC, ALAC, or MP3;
- device copy format/bitrate;
- Wi-Fi-only device transfers, enabled by default;
- cache size and Clear Cache;
- local library size;
- API/app version and diagnostics export.

Diagnostics redact authorization headers, pairing tokens, provider data,
streamrip configuration, local host paths, and query parameters that may
contain private identifiers.

## Caching and storage

Store API responses are cached as bounded JSON records with fetch time, ETag,
and API version. Artwork uses a separate LRU cache.

Initial limits:

- catalog JSON: 5 MB total;
- artwork: 32 MB total;
- no more than 24 decoded cover images retained in memory;
- previews: streamed, not permanently cached;
- local music: limited by a user-configurable free-space reserve, default
  500 MB.

On memory warning, decoded artwork and off-screen detail state are released
immediately while downloaded music, resumable transfer metadata, and pairing
remain intact.

Offline mode shows the last successful home/search/detail data with `Cached`
and its age. Buy, retry, and account refresh are disabled with `RockPod host
offline`. Cached results are never presented as freshly loaded provider data.

## Error contract

Stable host error codes include:

```text
PAIRING_REQUIRED
PAIRING_CODE_INVALID
HOST_BUSY
ACCOUNT_LOGIN_REQUIRED
PROVIDER_UNAVAILABLE
INVALID_SOURCE
INVALID_ITEM
INVALID_FORMAT
ALREADY_IN_LIBRARY
IMPORT_FAILED
IMPORT_EMPTY
DEVICE_COPY_FAILED
INSUFFICIENT_SPACE
JOB_NOT_FOUND
RATE_LIMITED
API_VERSION_UNSUPPORTED
```

The client maps them to concise recovery actions. Examples:

- `ACCOUNT_LOGIN_REQUIRED`: `Open RockPod on the computer and sign in to
  TIDAL.`;
- `HOST_BUSY`: show the active job and offer to queue this item;
- `IMPORT_EMPTY`: reuse the existing auth/configuration hint;
- `INSUFFICIENT_SPACE`: offer host-only import;
- connection loss during import: keep the job, reconnect, then query its
  current state rather than starting a duplicate.

Requests that start jobs require an `Idempotency-Key`. Repeating a timed-out
POST with the same key returns the original job.

## Packaging and deployment

Create a separate source target at:

```text
tools/ios/RockPodStore/
```

Suggested initial files:

```text
Makefile
control
main.m
RPStoreAppDelegate.h/.m
RPStoreRootViewController.h/.m
RPStoreClient.h/.m
RPStorePairingController.h/.m
RPStoreViewController.h/.m
RPAlbumDetailViewController.h/.m
RPDownloadsViewController.h/.m
RPLocalLibraryViewController.h/.m
RPStoreCache.h/.m
RPStoreDownloadManager.h/.m
RPStorePlayer.h/.m
Resources/Info.plist
Resources/*.png
```

Package identity:

```text
Package: com.rockpod.store
Name: RockPod Store
Executable: RockPodStore
Bundle identifier: com.rockpod.store
Architecture: iphoneos-arm
MinimumOSVersion: 6.0
Device family: iPhone/iPod touch
Required architecture: armv7
Install type: rootful jailbreak .deb
```

The Theos target is a separate armv7/iOS 6 application. The existing
`tools/ios/RockPodLink` target is arm64 with a minimum of iOS 12 and cannot be
reused as the build target. Small service and UI patterns may be ported, but
RockPod Store must not link RockPodLink's modern frameworks or embedded
Rockbox/FFmpeg payload.

The package installs the application bundle, icon variants required by iOS 6
and 7, and invokes `uicache` through normal package scripts. It does not modify
system applications. Installation on the iOS 7 side requires that side to
have a functioning jailbreak bootstrap/package installation path; booting a
patched kernel alone is not the deployment mechanism.

## Host implementation layout

Add these host-side units under `rockpod/`:

```text
services/store_catalog.py
services/store_jobs.py
services/store_companion_api.py
tests/test_store_catalog.py
tests/test_store_jobs.py
tests/test_store_companion_api.py
```

`store_catalog.py` owns normalized catalog/detail and Owned state.
`store_jobs.py` owns the one-at-a-time persistent import queue.
`store_companion_api.py` owns pairing, authentication, HTTP routes, artwork,
and media delivery.

Configuration keys:

```text
store_companion_enabled = false
store_companion_bind = ""
store_companion_port = 47720
store_companion_allow_plain_http = false
store_companion_device_copy_format = "aac"
store_companion_device_copy_bitrate_kbps = 256
store_companion_artifact_days = 7
```

Job records and generated device copies live under the existing RockPod cache,
for example `cache/store-companion/`. Provider auth remains in
`cache/streamrip/` and is never duplicated into the companion directory.

## Verification

### Host tests

- home tabs call `prepare_store_homepage()` with the exact current tab keys;
- search/detail use the existing importer requests and normalized JSON fields;
- the streamrip environment points at the existing RockPod XDG auth state;
- API output never contains `cover_path`, config paths, commands, or tokens;
- invalid sources, formats, IDs, URLs, traversal, and oversized requests fail;
- Owned agrees with the desktop store for provider tags and normalized library
  matches;
- concurrent Buy requests queue without launching parallel streamrip jobs;
- idempotent retries return one job;
- duplicate, empty, partial, successful, failed, cancelled, and auth-required
  imports produce stable states/messages;
- library rescan occurs after a successful or partial import;
- device-copy verification, ETag, Range, resume, checksum, expiry, and cleanup
  work;
- pairing expiry, rate limit, token revocation, and redaction work;
- service disabled means no listener and no Bonjour advertisement.

### iOS client tests

- one binary launches on clean iOS 6.1.6 and iOS 7.1.2 armv7 installs;
- first pairing, reconnect, revoked token, wrong code, and manual-host entry;
- all seven home tabs, TIDAL search, empty search, and album detail;
- two-column grid at 320 x 480 points with long titles and multi-disc albums;
- artwork fallback, cache eviction, offline age, and memory warning;
- Buy album, Buy track, Owned, duplicate, queue, cancel, failure, and retry;
- connection loss before and after POST does not create duplicate imports;
- ranged transfer resumes after Wi-Fi loss and rejects a bad checksum;
- local AAC/MP3 playback, interruption, lock, seek, delete, and low storage;
- no TIDAL secret or full host path appears in app files or diagnostics.

### Physical acceptance gate

On the dual-boot iPod touch 4G:

1. Boot iOS 6.1.6, pair with the current RockPod installation, load Featured,
   search TIDAL, open an album, and import one track to the host.
2. Import one album with a device copy, interrupt and resume its transfer,
   verify checksum, then play it in RockPod Store.
3. Reboot into iOS 7.1.2 and repeat browse, detail, Buy, resume, and playback.
4. Confirm both boots use the same host TIDAL account without either app
   containing provider credentials.
5. Confirm the desktop Music Store immediately shows the imported item as
   Owned after its normal rescan.
6. Confirm no files on any mounted iPod Classic were read, written, or used as
   a deployment target.

## Delivery order

1. Extract shared catalog/Owned logic and add read-only host API endpoints.
2. Build the iOS 6/7 UIKit shell, pairing, home tabs, search, and detail.
3. Add persistent import jobs and host-only Buy.
4. Add device-copy conversion, ranged transfer, local library, and playback.
5. Add offline cache, diagnostics, package scripts, and the full physical
   acceptance gate.

The first shippable milestone is complete when the iPod touch can pair, browse
the live personalized TIDAL home page, search, inspect an album, press Buy,
watch the existing RockPod import finish, and see the item become Owned using
the same host account and library as the desktop store.
