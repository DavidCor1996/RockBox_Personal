# QR Codes iPodJS Application Specification

## Product Decision

Provide a native, offline QR Code generator for 320x240 iPodJS targets. It
appears at `Extras -> Applications -> QR Codes`, uses stock iPod list styling,
and uses the exact keyboard renderer already used by Music Search.

The application stores generated codes. Opening it always shows:

1. `Add QR Code`
2. Saved codes, named `QR Code 001`, `QR Code 002`, and so on

Selecting `Add QR Code` opens the editor. Selecting a saved code reopens its
scannable display. Scanning, camera input, networking, Wi-Fi forms, file input,
renaming, and deletion are outside the first release.

## Upstream and Build

Use Nayuki's MIT-licensed C QR Code generator pinned to commit
`2c9044de6b049ca25cb3cd1649ed7e27aa055138`.

```text
apps/gui/qrcode/
    LICENSE.upstream
    UPSTREAM.md
    qrcodegen.c
    qrcodegen.h

apps/root_menu.c
    native list, editor, rendering, persistence, and launcher
```

Compile the encoder only under `HAVE_IPODJS_UI`. This is a native application,
not a `.rock` plugin, so it can share Music Search's renderer and iPodJS
transitions without duplicating keyboard behavior or assets.

## Navigation and Controls

Saved-list screen:

| Input | Action |
| --- | --- |
| Wheel | Move through Add and saved codes |
| Center | Add a code or open the selected saved code |
| Play/Pause | Preserve the normal playback control behavior |
| Menu | Return to Applications |

Editor screen:

| Input | Action |
| --- | --- |
| Wheel | Choose a character |
| Center | Type the selected character |
| Previous | Delete the last character |
| Next | Add a space |
| Play/Pause | Cycle uppercase, lowercase, numeric, and symbol banks |
| Menu | Generate and display the QR code; exits if input is empty |

New-code display:

| Input | Action |
| --- | --- |
| Center | Save the QR code and return to the refreshed saved list |
| Menu | Return to editing |

Saved-code display:

| Input | Action |
| --- | --- |
| Menu | Return to the saved list |

## Encoding and Rendering

The input buffer is 256 bytes including the terminator. Encode with Medium
error correction, automatic version selection, automatic mask selection, and
error-correction boosting. Use fixed maximum-version work buffers: two 3,918
byte arrays and no heap allocation.

Add a four-module white quiet zone around every code. Choose the largest
integer scale that fits both display dimensions below the stock title bar,
center the complete square, and draw contiguous black module runs with
`lcd_fillrect()`. Do not antialias, crop, stretch, round corners, recolor, or
overlay artwork. No full-screen backing framebuffer is allocated.

## Persistence Contract

Saving creates a paired record:

```text
/QR Codes/QR Code NNN.bmp
/.rockbox/qrcodes/QR Code NNN.txt
```

The public file is a standalone, one-bit BMP at four pixels per QR module,
including the four-module quiet zone. The private source record contains the
exact payload needed to regenerate the display. Write the source through a
`.tmp` file and rename it into place. If either half fails, report failure and
remove the incomplete BMP.

At application entry and after a successful save, scan `/QR Codes` for numbered
BMPs that have matching source records. Sort them by filename and show at most
64 saved entries. Read a source record only when its list row is selected; do
not load, decode, or perform filesystem I/O while drawing.

The source sidecar is deliberately private but is not encrypted. Users should
not save secrets as QR payloads unless they accept that they are stored on the
iPod in plain text.

## Playback and Memory Safety

The application must not use the playback buffer, audio buffer, PCM or mixer
state, stop or restart audio, or alter the playlist. Keyboard assets are
prepared at an explicit entry service point and draw calls use cached surfaces
only. Static QR encoder and saved-label storage remain independent of playback
memory. Clear input, encoder buffers, loaded source text, and the in-memory
saved-label cache on the relevant exit paths.

## Acceptance Tests

- Host upstream suite passes all pinned QR encoder vectors.
- Simulator and iPod 6G firmware builds pass.
- The app opens on `QR Codes`, with `Add QR Code` selected.
- The editor uses the shared Music Search keyboard renderer and all four banks.
- Entering `ABC` renders the expected version-1, 21-module code at integer
  scale with the full quiet zone.
- Saving produces a 116x116 one-bit BMP that an independent decoder reads as
  `ABC`, plus the matching source record.
- The refreshed list contains `QR Code 001`; selecting it regenerates the same
  scannable display.
- Trace assertions show no core-memory loss, playback mutation, or open-file
  leak across entry, editing, saving, reopening, and exit.
- Standard iPodJS navigation and Music Search regressions remain green.
