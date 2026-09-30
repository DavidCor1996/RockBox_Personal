# Docked album lock screen

Implemented in the personal iPod 6G tree. This supersedes the earlier
color-only and last-played-album proposals.

## Experience

When composite output is active, media is stopped and an eligible screen has
been unattended for the configured delay, show full-screen album photography.
Choose from the entire synced album catalog at random, excluding the current
album. Hold each cover for 24 seconds and dissolve into the next over six
seconds with eased opacity. Slight image drift preserves aspect ratio and
keeps the screen filled. Reduced motion disables image drift.

A centered 12-hour clock, date and AM/PM sit over the photograph. A separate
centered Liquid Glass-inspired weather capsule contains a licensed modern
condition icon, temperature, conditions and location. The capsule samples a
blurred version of the actual artwork; its rim and translucent type are
procedural. Clock numerals have two-pixel beveled reflections and translucent
interiors refracting cached artwork, rather than a solid white fill. This is an RGB565 software approximation of the requested Apple TV
lock-screen aesthetic. No hand-drawn or generated picture assets are used.

The source framebuffer is 320x240. Artwork fills that frame by cropping to its
aspect ratio with bilinear sampling; composite hardware scales the result to
TV output. This cannot create native HD detail. A restrained dark scrim keeps
text legible over bright covers. The clock uses licensed Adwaita Sans/Inter
font masks and weather uses pinned, unmodified Lucide SVG sources.

## Library contract

Read `/.rockbox/albumlist/index.tsv`, the existing synced album catalog. Both
six-column legacy and seven-column slide-aware rows are supported. Prefer the
explicit slide path or `/.rockbox/albumlist/slides/<album_id>.bmp`, falling back
to that album's synced thumbnail if decoding fails. Use
`tools/generate_albumlist_slides_from_ipod.py` to create 384px slides from the
synced original covers when needed. No demo images are installed as user art.

Selection uses a streamed reservoir over all records, without the ordinary
album-list preview's 384-record limit. Every eligible record participates;
there is no immediate repeat, but this is random sampling, not a guaranteed
once-per-cycle shuffle. A single-album library keeps its one image. Missing or
corrupt images get bounded retries while the current image stays on screen.
Without readable catalog artwork the existing muted color field is the
fallback. The screen does not scan audio files or embedded metadata.

## Rendering and resources

Two covers borrow the existing album-list slideshow buffers exclusively for
the ambient UI lifetime. Both entry and exit invalidate the former cached
images, and the album-list service refuses work during the lease. There is no
new full-screen buffer, core allocation, playback-memory claim, PCM change or
playlist mutation. Additional state remains below 16 KiB.

Artwork service starts after a half-second settling period and requires no
queued/raw input, Hold, wheel touch, active audio or database commit. Each call
scans at most 16 catalog chunks or decodes one image. Overlong directory fields
are drained without treating continuation chunks as albums. The file closes
at the end of each scan and on every exit. Failed/missing catalogs back off.
Drawing accesses only cached cover pixels, ROM glyphs, a 41x31 color/blur grid
and one scanline. It performs no filesystem access, decoding or allocation.
Frames use elapsed time, targeting four updates per second at rest and eight
while a next image is pending; slow hardware skips frames rather than extending
the dissolve. Each new image gets a complete six-second transition even if
prefetch was delayed. Actual target cadence still needs device measurement.

## Weather and handoff

Read `/.rockbox/rockpod/weather/forecast.tsv` at entry, when the local hour
changes, and on explicit USB weather-cache generation changes. The normal action service continues pumping
the companion relay. No weather fetch is triggered by drawing. Use the synced
location, conditions, day/night state and Celsius/Fahrenheit units. Prefer a
live observation less than three hours old; otherwise select the synced hourly
forecast covering the current hour and label it “Forecast: <location>”. An
older live observation remains a fallback, with its age shown. Never substitute
a future hour or an expired hourly row. This supports the normal RockPod sync,
which supplies daily/hourly rows without a live `current` row.
The RTC must match the location's local time. Observations older than three
hours show their age; malformed, future, missing or over-24-hour observations
show “Weather unavailable.”

Settings → General Settings → Display → LCD Settings → Ambient clock includes
Preview. Preview requires stopped audio but can run without a connected TV.
The color preference controls only the fallback when artwork is unavailable.
Automatic entry supports iPodJS Home, stock Home, ordinary Music/Files browsers
and the idle Desktop Mode shell. Paused media remains in its player. Input
exits, transport/system actions return to the host, and external playback
immediately yields to the player. Theme, viewport, backdrop and notification
suppression are restored. Plugin API remains 288 and requires matching plugins.

See `docked-ambient-clock-validation.md` for actual test evidence and remaining
hardware checks. The package and subsequent glass-clock refinement are deployed to the connected
iPod 6G; physical composite visual validation remains outstanding.

## Latest refinement awaiting installation

The hourly-weather compatibility fix and softened glass treatment are built
and simulator-tested locally. The iPod was disconnected after the earlier
glass-clock deployment; this newer revision still needs installation. The
weather capsule is now (56,162)–(264,218), with a brighter frosted surface and
more separation from AM/PM. Numeral interiors have a softer reflection gradient
while retaining cached-image refraction and beveled edges.

Latest status: the hourly-weather compatibility fix and refined glass design
are installed and verified on the iPod 6G. See the latest deployment record
in the validation document; earlier pending-installation notes are superseded.
