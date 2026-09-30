# Rockbox upstream application

The iPod Classic 6G/7G Applications grid contains a separate entry named
`Rockbox`, copied from RoloLauncher's RoLo pattern. RoloLauncher continues to
use `/rockbox-tvout-test.ipod` and `/.rbtv`.

## Upstream-only steering

`Rockbox` is exclusively for official upstream Rockbox nightly builds. Never
put personal RockPod code, unmerged Gerrit candidates, or experimental builds
in this slot. Download the latest official daily source and package, record
their date, revision and checksums, and rebuild that unmodified source with
`tools/configure --target=ipod6g --type=n --rbdir=/.rbupstream`.
The alternate runtime build option is the only permitted customization.
Ship its matching codecs, plugins, themes and runtime files together.

- Firmware: `/rockbox-upstream.ipod`
- Runtime: `/.rbupstream`, including a matching `rockbox.ipod`
- Personal boot firmware remains `/rockbox.ipod` and `/.rockbox/rockbox.ipod`.
- Reboot returns to the normal boot path; the Apple bootloader is unchanged.
- Keep upstream configuration and databases separate from personal RockPod.

Do not unpack the official `.rockbox` package over the personal runtime.
The downloaded stock binary expects `/.rockbox`; moving that binary alone
does not isolate it. Install the alternate-runtime rebuild instead.
Updating the personal launcher firmware must still follow the normal dual
firmware checksum rule and database-preserving package deploy script.

The launcher checks for firmware, runtime build information and codecs before
calling `rolo_load()`. RoLo owns playback shutdown and firmware validation.
Missing files return to Applications; the simulator reports that loading
requires hardware. The entry uses the existing iPod 6G target guard.

## Icon and memory

The icon uses the existing Rockbox project mark from
`assets/ipodjs/sources/sitekick/ipod-exclusive/sourced/rockbox-icon.svg`.
`tools/prepare_ipodjs_application_icons.sh` renders it on a dark glass tile
to `assets/ipodjs/rockbox/applications/rockbox.46x46x24.bmp`.
The fixed icon cache has 32 slots inside the existing preview storage union,
with a compile-time capacity check against the Applications item count.
Each slot uses a 4,232-byte RGB565 pixel buffer plus bitmap metadata;
the slots remain smaller than the existing 338,520-byte preview storage. No playback
allocation, buffer ownership, animation or drawing lifecycle changes.

## Validation

Build the simulator and native personal firmware, and build/package the exact
upstream nightly source. Verify the Rockbox icon in the Applications grid,
missing-file messages, and simulator hardware-only message. Run the existing
navigation regression. Before syncing, verify installed firmware/runtime
checksums and preservation of personal databases and the TV-out runtime.
Actual RoLo startup and reboot return require a physical device test.

## September 13, 2026 installation

- Official daily: September 13; package version `57a91121f6-260912`.
- All 10,998 source archive files matched the extracted source byte for byte.
- Build with `VERSION=57a91121f6-260912` so the archive does not inherit the
  enclosing personal repository's Git version; this is build metadata only.
- Personal native and simulator builds and packages passed. The native
  launcher adds 532 bytes of text; data and BSS remain unchanged, including
  the 338,520-byte preview union. Launcher prologue uses 8 bytes of stack.
- Simulator captures verified the grid icon, missing firmware, missing
  runtime, and hardware-only message with both paths staged.
- The full headless navigation regression stopped at the existing WPS Hold
  unlock return check (`Now Playing`); hardware deployment was explicitly
  requested for device testing. This is not a claimed full navigation pass.
- Download/build manifest and captures are stored locally under
  `build-rockbox-upstream-nightly-20260913/`.
