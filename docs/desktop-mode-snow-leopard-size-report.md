# Desktop Mode Snow Leopard size report

Generated from the pack-format-2 contract and focused 6G/simulator builds
on 2026-08-31. This report contains no Apple assets.

Pack format 2 replaced the downscaled whole-window screenshots with chrome cut
at Apple's 1:1 scale, Lucida Grande coverage atlases, and RGB565-plus-coverage
icons. The two shared window frames replaced three per-application ones, and
the separate minimize/restore scratch bitmap is gone because the compositor
scales straight into the frame it is already assembling.

## Mandatory asset contract

| Measurement | Value |
| --- | ---: |
| Required logical assets | 123 |
| Packed asset bytes on disk | 14,442,255 bytes (13.8 MiB) |
| Resident shell asset bytes | 968,491 bytes (945.8 KiB) |
| Compose buffer | 153,600 bytes (150.0 KiB) |
| Shell runtime bytes | 1,122,091 bytes (1.07 MiB) |
| Transient authentic boot bytes | 167,424 bytes (163.5 KiB) |
| Peak visual runtime bytes | 1,122,091 bytes (1.07 MiB) |
| Decoded ceiling | 1,179,648 bytes (1.125 MiB) |
| Headroom before loader bookkeeping | 57,557 bytes (56.2 KiB) |

Resident bytes by group:

| Group | Assets | Resident bytes |
| --- | ---: | ---: |
| desktop (wallpaper, menu bar, Dock shelf) | 5 | 183,276 |
| Dashboard widgets | 5 | 124,308 |
| chrome (windows, panels, selections, scroller) | 14 | 476,584 |
| icons (application, Dock magnification, file) | 31 | 105,528 |
| fonts (Lucida Grande 11, bold 11, 9) | 3 | 44,475 |
| cursors | 2 | 1,704 |
| external Dock icons | 9 | 32,616 |
| boot (transient, released before the shell loads) | 13 | 167,424 |

Opaque chrome is a 16-bit RGB565 `BI_BITFIELDS` BMP. Anything with a soft edge
- icons, cursors, the Dock running indicator - is an `RGA1` file: RGB565 plus
Apple's own 8-bit coverage, so the compositor blends the edge Apple drew
instead of hard-keying it. Fonts are raw 8-bit coverage atlases plus a
105-byte `DMF2` metrics sidecar carrying cell size, ascent, signed atlas
origin, and per-glyph advances.

The importer validates dimensions, bit depth, compression, coverage payload
length, provenance and SHA-256 before install. The authentic boot background
and twelve captured spinner phases coexist only in a 167,424-byte transient
plugin-buffer region, released before the shell loads.

## Compose buffer

The plugin API exposes no framebuffer, and real Snow Leopard chrome is full of
soft edges and antialiased Lucida Grande, so the shell needs to read back what
it has already drawn in order to blend over it. Each frame is assembled in one
320x240 RGB565 plane inside `plugin_get_buffer()` and handed to the LCD with a
single `lcd_bitmap()` plus one `lcd_update()`.

This is one buffer, not a source/destination pair: there is no second
full-screen plane, and the 105,792-byte minimize/restore scratch bitmap that
format 1 required no longer exists. `PLUGIN_BUFFER_SIZE` is 3 MiB on both
targets, so the 1.07 MiB shell leaves the buffer comfortably free and never
touches core or playback memory.

The ceiling was raised from 900 KiB to 1.125 MiB when Dashboard widgets and
iTunes gained their real Snow Leopard chrome.  It is a budget against
`PLUGIN_BUFFER_SIZE`, which is
dedicated to the running plugin and is never playback or core memory, so the
limit exists to keep the shell honest rather than because the space is
contended.

## Focused plugin builds

| Target | text | data | BSS | ELF total | `.rock` bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| iPod Classic 6G hardware | 26,738 | 284 | 29,340 | 56,362 | 27,024 |
| iPod Video 5G hardware | 27,574 | 284 | 29,340 | 57,198 | 27,860 |
| iPod Classic 6G simulator | 28,217 | 2,296 | 30,728 | 61,241 | 178,920 |

The hardware BSS includes the bounded 96-entry Finder snapshot. Desktop Mode
loads the private images from `plugin_get_buffer()` and never calls
`plugin_get_audio_buffer()`, `audio_stop()`, or a core framebuffer allocator.

## Outstanding physical measurements

The following values require physical 6G/5G runs; they must not be inferred
from simulator figures:

- largest stack frame from the release compiler configuration;
- whole-firmware `rockbox.elf` delta against the immediately preceding build;
- playback-active allocatable core memory before, during, and after ten
  Desktop Mode enter/exit cycles;
- measured frame time for full-screen composition at the 20 Hz interaction
  target, on iPod Video 5G in particular, since its PP5022 is the slower of
  the two and the simulator cannot stand in for it.

Release/hardware acceptance remains blocked until these rows are appended with
measured values.
