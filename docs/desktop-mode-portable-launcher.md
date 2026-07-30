# Portable Desktop Mode on a Mounted iPod

Status: implemented packaging and runtime contract. Native release artifacts
are built on their matching operating systems and staged with
`tools/build_desktop_mode_portable_bundle.py`.

## User contract

A fully packaged Rockbox install places these files at the mounted volume
root:

- `Open Desktop Mode.command` for macOS;
- `Open Desktop Mode.cmd` for Windows;
- `Portable Desktop Mode README.txt` (installed from the packaging README).

No RockPod installation is required on the computer. The launchers select a
native runtime below `.rockbox/desktop-host`, start the `desktop1080`
simulator full-screen, and open `desktop_mode.rock` directly.

The mounted iPod remains the simulator media root. Consequently Finder,
iTunes, DIRECTV Live TV, Sitekick state/assets, Netflix static assets,
tagcache, and `.rockbox/videolist` use the media and metadata on that iPod
rather than demo files. Netflix reads only Video Sync rows from that manifest;
it does not use the DIRECTV or YouTube catalogues.

## Native plugin isolation

Hardware `.rock` files are ARM binaries and cannot be loaded by a macOS or
Windows simulator. They must never be replaced to make host Desktop Mode
work.

The simulator accepts `ROCKBOX_SIM_SYSTEM_ROOT`. Only these device paths are
resolved from that native system overlay:

- `/.rockbox/rocks`
- `/.rockbox/rocks.data`
- `/.rockbox/fonts`
- `/.rockbox/langs`
- `/.rockbox/icons`

All other paths resolve from `--root`, the mounted iPod. Platform bundles
therefore live at:

```
.rockbox/desktop-host/
    windows-x86_64/
        rockboxui.exe
        system-root/.rockbox/...
    macos-x86_64/
        rockboxui
        system-root/.rockbox/...
    macos-arm64/
        rockboxui
        system-root/.rockbox/...
    manifest.json
```

The installer copies the mounted device's verified
`desktop_mode_snow_leopard` pack into every native system root. The native
runtime also carries `sitekick.rock`, `netflix_desktop.rock`, MPEGPlayer, and
OpenH264 Player; Sitekick's chip catalogue, save state and art remain under
`/.rockbox/sitekick` on the iPod. Netflix's static presentation assets come from
`/.rockbox/ipodjs/netflix`, its catalogue and posters come from
`/.rockbox/videolist`, and its selected media path resolves against the
mounted iPod. DIRECTV media and logos remain under `/Videos/LiveTV` and are
read directly from the iPod.

## Release build

On each matching build host, configure and install the `desktop1080`
simulator, then stage its runtime:

```
tools/build_desktop_mode_portable_bundle.py \
  --build-dir build-sim-desktop1080 \
  --platform windows-x86_64 \
  --output .rockpod-private/desktop-mode-portable
```

Use `macos-x86_64` on an Intel Mac and `macos-arm64` on Apple Silicon. Merge
the three platform directories, then install them into a mounted or staged
Rockbox root:

```
tools/install_desktop_mode_portable.py \
  --ipod-root /path/to/IPOD \
  --bundle-root .rockpod-private/desktop-mode-portable
```

The full iPod 6G deploy script performs this step automatically when the
complete bundle is present. Installation is transactional for
`.rockbox/desktop-host`, leaves the hardware plugins untouched, copies both
root launchers, and writes a SHA-256 manifest.
