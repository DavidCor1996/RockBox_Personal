# Video phase two: software candidate and hardware acceptance

The desktop preparation path is shared by Videos and app-video staging. It
inspects the source and installed firmware before selecting copy, remux,
audio-only conversion, or video conversion. Explicit H.264 requires the matching
installed capability record and audited patched x264; Automatic uses conservative
software MPEG until a profile is hardware-qualified. The 5G remains MPEG-only.

## Preparation and publication

Space Saver, Balanced and TV Quality use distinct dimensions/bitrates. Custom
size uses two-pass encoding and an enforced final byte limit. Cadence preserves
supported rational rates, geometry normalizes SAR/rotation without TV bars,
interlaced input uses metadata/override, and HDR runs a tone-mapping filter.
Unknown color uses the documented HD/SD fallback and has an override. Audio
selection is explicit; positive offsets become silence and early audio is trimmed.
The review and 20-second preview run off the UI thread.

The audited Apple x264 build crashes while reading macroblock-tree statistics
on its second pass. Budget encoding disables that optional allocation heuristic
on both passes; ordinary two-pass rate control and final-size enforcement remain.
New encodes use regular four-second keyframes to bound seek-table growth.
The fixed seek index now permits 8,192 entries (16 KiB additional static storage)
and rejects oversized or malformed tables instead of silently truncating them.
The existing 640x360 Mortal Kombat copy has 4,678 keyframes: it passes full-stream
and host decode validation with this limit, so no media resync is needed.

Source fingerprints, selected streams/filters, encoder identity and compatibility
contract determine the cache key. Full syntax, table, packet, duration and host
decode validation precede publication. Stable logical identity includes the
library item and source fingerprint; quality/container changes preserve playback
state, while replaced source content conservatively starts a new timeline.

Sync stages and fsyncs bytes, hashes the source during copy, checks destination
size and reads the entire staged destination back for SHA-256 verification.
Immutable rendition names prevent replacing the working file prematurely. An
on-device journal distinguishes publication from library commit. The old movie
is retained until both local sync bookkeeping and device inventory commits finish.
A retry checks complete destination bytes; interrupted partial copies restart.
Indexed cache movies can be evicted and reused from the device after read-back,
without encoding or copying again. Metadata-only updates retain the physical path.

Playback state is device-owned. Desktop imports it but never pushes an older
snapshot over an iPod session. A decreased position is an intentional Start Over,
not a conflict to resolve using potentially unsynchronized clocks. Legacy resume
migration requires matching durations and a compatible sample timeline; existing
logical state always wins.

## Playback and remote

The captured dock sends identical Center/Play packets and no Menu packet.
The selected fallback uses Center/Play to select in menus and pause in playback;
holding Left returns. Short Left moves left in grids or seeks backward in a movie.
Up/Down scroll menus and retain playback volume control. Physical click-wheel
buttons retain their native functions. Hold Select toggles selected captions;
hold the merged Center/Play in menus to open the context menu.

Both movie paths submit borrowed native YUV planes synchronously to the TV
adapter before LCD presentation. The descriptor supplies plane strides, visible
crop, geometry, color and time. Scanout owns separate completed surfaces with
FREE/WRITING/READY/SCANNING states; a field-edge timeout retains the last frame.
CPU ownership tests do not prove physical field latching.

**The installed safe raster is still 320x240. Higher-resolution private SVID
scanout remains gated by VIDEOOUT_ENHANCED_TEST and is not qualified.** Native
input reaches the adapter, but this candidate must not be described as qualified
640x480 composite output. MPEG remains software-decoded; no hardware MPEG claim.

H.264 uses the existing 512 KiB compressed buffer for read-ahead, prepares one
picture ahead and skips late presentation while decoding dependent references.
Its clock now subtracts the mixer's unconsumed media samples; underrun silence
freezes media time. DMA/analogue output latency is not measured. MPEG retains its
scheduler; subtitle/services and resume conversion use its actual 44.1 kHz clock,
not its separate 45 kHz MPEG timestamp unit.

Resume is saved at 30-second intervals, on pause and on exit. Versioned bounded
chapter sidecars use ordinary library/context navigation. Selected embedded text
or external UTF-8 SRT is normalized to bounded outlined caption masks. Complex
or image subtitles require explicit burn-in. None selected means no subtitles;
burned captions have no simultaneous soft track. Existing app branding/assets,
Home-only category bar and accepted TV geometry are preserved.

## Evidence and limits

Host checks cover real conversion/copy/remux/audio-offset/HDR/budget cases,
corrupt streams, interrupted publication/cache eviction, shared resume records,
remote mappings, native crop/stride/surface transitions, read-ahead/clock math,
and renderer memory/layout bounds. Classic, 5G and simulator are build targets.
The new chapter table is 25,600 static bytes, the bounded H.264 trace is 3,584
bytes and its caption snapshot is 3,168 bytes. Read-ahead reuses existing storage;
no new full-frame decode queue or decorative playback allocation is introduced.

H.264 exports `/.rockbox/video-playback-trace.tsv` after callbacks stop, with
last 128 PTS/clock/cost samples, full-session presentation-drop/underrun counts,
maximum costs/lateness and reserved player pool bytes. Presentation cost combines
LCD scale and TV copy; it is not a separate hardware scanout measurement. MPEG
uses its established diagnostics. Intended NTSC field repeats, output latency,
whole-system peak memory and physical scanout remain unmeasured.

Hardware acceptance requires the user's short clips and full movie on LCD/TV,
4:3/16:9, repeated pause/seek/resume, remote wake/navigation, and music→video→music
transitions. Targets: approximately ±50 ms steady-state A/V alignment (record
measurement accuracy), no accumulating drift, no steady-state audio underruns,
no sustained stale-frame queue, no mixed fields/tearing, and responsive controls.
Report cadence repeats separately from drops; do not claim 60 fps from fields.
No throughput, image parity, field safety or macOS encoder qualification is
established by a host build. Milestone C and performance acceptance remain open
until hardware results are available.

Relevant filter semantics: https://ffmpeg.org/ffmpeg-filters.html
