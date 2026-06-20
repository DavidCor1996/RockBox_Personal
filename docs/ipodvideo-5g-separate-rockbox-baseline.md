# Separate Rockbox Baseline For iPod Video 5G/5.5G

This baseline is for the `ipodvideo` target only. The hardware stays 5G/5.5G.
The later Apple Classic 6G/7G work is only a UI reference, not a target change.

## Goal

Use Rockbox as the code baseline for a future custom iPod OS while keeping it
fully separate from the current live Rockbox install on the device.

That means:

- the existing `/.rockbox` tree stays untouched
- the new build gets its own runtime directory
- the new build gets its own `config.cfg`, database, themes, fonts, plugins,
  logs, shortcuts, and `playername.txt`
- `ipodloader2` can boot it as a separate menu item

## Why This Works

Rockbox already supports an alternate runtime root via `tools/configure
--rbdir=...`.

That value becomes `ROCKBOX_DIR` in the generated build configuration:

- [`tools/configure`](/home/david/Documents/RockBox_Personal-master/tools/configure:1514)
- [`tools/configure`](/home/david/Documents/RockBox_Personal-master/tools/configure:4767)
- [`firmware/export/rbpaths.h`](/home/david/Documents/RockBox_Personal-master/firmware/export/rbpaths.h:124)

So a build configured with `--rbdir=/.rockbox-video5g-baseline` will keep its
own runtime files under that directory instead of `/.rockbox`.

## Separation Boundaries

Not shared with the current Rockbox install:

- `config.cfg`
- `.resume.cfg`
- database files
- themes and fonts
- plugins and plugin data
- logs
- `playername.txt`
- skin and WPS assets

Still shared because both builds run on the same FAT32 data partition:

- music and video files outside the alternate Rockbox directory
- any manually shared folders you point both builds at

## Build And Install Flow

Build a separate 5G baseline:

```bash
scripts/ipod/build-rockbox-video5g-baseline.sh
```

Default outputs:

- build dir: `build-hw-ipodvideo-5g-video5g-baseline`
- runtime dir on iPod: `/.rockbox-video5g-baseline`
- full package: `build-hw-ipodvideo-5g-video5g-baseline/rockbox-full.zip`
- curated package:
  `build-hw-ipodvideo-5g-video5g-baseline/rockbox-video5g-baseline-curated.zip`
- loader snippet:
  `build-hw-ipodvideo-5g-video5g-baseline/ipodloader2-video5g-baseline.entry.txt`
- plugin allowlist:
  `scripts/ipod/plugin-allowlist-video5g-baseline.txt`

Dry-run install onto the mounted iPod:

```bash
scripts/ipod/install-rockbox-video5g-baseline.sh
```

Real install after reviewing the dry run:

```bash
CONFIRM_IPOD_INSTALL=1 DRY_RUN=0 \
scripts/ipod/install-rockbox-video5g-baseline.sh
```

Optional loader entry append:

```bash
CONFIRM_IPOD_INSTALL=1 DRY_RUN=0 UPDATE_LOADER=1 \
MENU_LABEL="Video 5G Baseline" \
scripts/ipod/install-rockbox-video5g-baseline.sh
```

Example `ipodloader2` entry:

```txt
Video 5G Baseline @ (hd0,1)/.rockbox-video5g-baseline/rockbox.ipod

## Curated Plugin Set

This variant is meant to stay lean, not plugin-free.

The default curated package keeps:

- `PictureFlow`
- lyrics support via `lrcplayer`
- text/property/search helpers used for normal browsing
- image/video viewers that are still useful on the 5G media device

The allowlist lives in:

- [`scripts/ipod/plugin-allowlist-video5g-baseline.txt`](/home/david/Documents/RockBox_Personal-master/scripts/ipod/plugin-allowlist-video5g-baseline.txt)

The build script keeps the full upstream-capable build output, then produces a
second curated zip from it for safer installation of the separate baseline.
```

## Naming

The separate build can be given its own visible runtime name through
`playername.txt` inside the alternate runtime directory. The install script
writes that file so the new build does not identify itself with the default
live install state.

## Constraints

- This does not rename every internal `Rockbox` string in the codebase.
- This does not change firmware, partitioning, or the current Apple/Rockbox
  boot paths by itself.
- This is the safe baseline for later UI work on the 5G hardware.
