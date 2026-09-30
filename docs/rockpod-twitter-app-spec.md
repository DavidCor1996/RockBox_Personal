# RockPod Twitter Application

## Product contract

RockPod imports a user-selected number (1–500) of previous posts from an X
profile URL or `@handle`. Share-link parameters such as `?s=11` are ignored.
The official Twitter for iPhone and iPod touch app of 2010 is the visual
reference: blue navigation bar, white timeline, profile photos, compact post
rows, and the historic Twitter wordmark and bird. This is an offline reader.

The Rockbox application appears in Extras → Applications with a real Twitter
bird icon. Its account list opens a chronological feed with Tweets, Media,
and Saved tabs (Left/Right). The wheel browses tweets and scrolls full text.
Select opens a tweet, then its attachment browser; Left/Right moves between
photos and videos, and Select starts a video. Menu returns one level at a
time, including from the video player to the same tweet. Holding Select
offers local bookmarks and links to quoted/replied-to tweets when those
tweets are present in the synced account. Bookmarks are saved on the device;
they do not send favorites or other actions to X.

## Import

The desktop UI accepts the profile, desired post count, and a media toggle.
`gallery-dl` reads the profile through the user's signed-in Firefox session.
Text, date, author, avatar URL, reply/retweet/like counts, quote/reply IDs,
and up to four attached media files are retained. A per-profile download
archive makes repeat imports incremental. Authentication, unavailable posts,
or extractor failures are shown as errors; an empty response is never
committed as a successful import. The configured post count caps the saved
timeline.

Originals, metadata, gallery-dl's extractor cache, download archives, video conversion cache, and the
device-output index live under `twitter_cache_dir` on an external drive. When
the setting is empty, RockPod discovers a mounted non-iPod drive with an
existing `RockPod` directory under `/run/media/$USER` and uses its `twitter`
subdirectory. The desktop UI shows the resolved path and can select another
external folder. A missing drive is reported; there is no internal-cache
fallback.

## Device package

Sync writes `.rockbox/twitter/accounts.tsv` and `library.tsv`, plus bounded
BMP avatars, 296×64 feed previews, 320×180 image views, video renditions, and the
historic wordmark. The 46×46 Applications icon is also installed in
`.rockbox/ipodjs/applications`. Videos use the selected MPEG or iPod H.264
profile. Preparation happens on the host; the plugin reads only prepared
device files. Originals remain on the external drive. Existing device media
are retained until a replacement is ready, and individual files are replaced
through staging. Device catalog files are written atomically.

## Artwork provenance

The icon and wordmark are raster conversions of the historic Twitter vector
from Wikimedia Commons, not drawn approximations. The source and conversion
script are kept at `assets/ipodjs/sources/twitter/` and
`tools/prepare_twitter_brand_assets.py`. The source page identifies the
Twitter copyright and Apache 2.0 license and marks the logo as a trademark.

## Current scope and validation

The iPod app reads synced posts and media offline. It does not post, like, or
message from the device. Import depth depends on the authenticated X session
and what the extractor can access. The supplied account
`https://x.com/reiivalentinaa?s=11` requires Firefox sign-in; an anonymous
or expired session can return `AuthRequired` even when the profile URL resolves.

Validation completed on September 22, 2026: nine live posts and five media
files (three photos, two videos) were imported into the external Data1 cache.
All nine posts and five media files synced to a mock device directory, with
the photos and video renditions present. The iPod 6G firmware, Twitter plugin,
and ZIP package built successfully; the package contains the plugin and icon.
The package and nine-post Twitter library were deployed to the mounted iPod
6G. Both firmware copies, the plugin, and the icon matched the local build by
SHA-256. All 11 tagcache files were preserved, and the 2,929 indexed tracks
remained readable with no missing files.

## Feed corrections after device feedback

Text now uses foreground-only glyph drawing and clears inherited backdrops,
avoiding opaque white rectangles over the navigation bar and tweet content.
The feed wraps text, displays author photos and real attachment previews,
and retains up to 2,047 characters of each tweet on the device. Video previews
come from the imported video frames. Both video players support returning to
Twitter; MPEG opens immediately without its generic startup menu.

The Applications icon cache now covers 32 entries, with a compile-time check
against the item count. Twitter had pushed Rockbox into index 24, outside the
old 24-entry cache. The existing 338,520-byte preview union is unchanged.
Twitter's static BSS is about 2.1 MiB, within the iPod 6G plugin buffer; it
does not allocate playback memory for drawing.

`tools/twitter_ui_sim_gate.py` captures the real profile, full tweet, photo,
mixed attachments, playing video, return to the same tweet, and Saved view.
The focused gate passes. Native and simulator builds and importer tests pass.
The broader navigation script was attempted but stopped while copying an
existing recursively nested simulator preview fixture; that fixture also
lacks its Music directory. This is not a claimed full navigation-suite pass.
