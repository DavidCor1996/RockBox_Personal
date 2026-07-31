# RockPod Setup for macOS: implementation and deviations

`tools/build_macos_installer.sh` builds an offline installer, `RockPod
Setup.app`, from `tools/macos-installer/`. This document records where that
implementation deviates from `docs/rockpod-easy-installer-spec.md` (the
target design) and what has and has not been verified.

## Why this exists instead of the full spec

The spec describes a signed, notarized PySide6 desktop application with a
downloaded, signature-verified release manifest. That cannot be produced from
this repository's Linux build host: there is no Qt/PySide6 packaging
pipeline here, no Apple signing identity, and no macOS SDK.

What is built instead is a small bash + AppleScript installer, assembled on
Linux, that carries an already-built hardware release inside the `.app`
bundle ("local-offline" channel in `manifest.json`). The safety rules the
spec requires — hash verification, hard stops, database preservation, a host
recovery bundle, never touching Apple firmware — are implemented in full;
only the delivery mechanism (bundled vs. downloaded, AppleScript dialogs vs.
Qt) differs. See `tools/macos-installer/setup/main.sh` and the `lib/*.sh`
modules for the actual state machine.

## The bootloader and stock-Rockbox steps

The original version of this installer explicitly refused to touch the boot
partition ("This installer does not write a bootloader"). It now offers two
independent, optional, skippable steps, asked about right after Welcome:

- **Install the Rockbox bootloader** (`lib/bootloader.sh`) — Classic 6G/7G
  via `mks5lboot`'s DFU dual-boot install, Video 5G/5.5G via `ipodpatcher
  --add-bootloader`. Regardless of the order the two questions are asked in,
  `main.sh` always installs and verifies Rockpod firmware *before* touching
  the boot partition, so a bootloader can never point at absent firmware
  (matching the spec's "Bootloader Workflows" section). The review screen
  says so explicitly rather than silently reordering the user's choices.
- **Install stock Rockbox first** (`lib/stock.sh`) — extracts the current
  upstream release package for the target before Rockpod's own overlay, for
  users who want to confirm plain Rockbox boots before trusting a heavily
  customized build. It reuses the same zip-safety checks
  (`verify_zip_entries` in `lib/package.sh`, generalized from what used to be
  Rockpod-only) and composes with the existing "update vs. fresh install"
  database guard: a device that had nothing on it before this step is still
  treated as a first-time Rockpod install afterward, since a stock package
  carries no tagcache for `guard_existing_database` to preserve.

### Where the bundled assets come from

`tools/fetch_stock_rockbox_assets.sh` downloads the universal Rockbox
bootloader and the current stock release zip for a target from
`download.rockbox.org` (the same server and paths Rockbox Utility's
`utils/rbutilqt/rbutil.ini` uses) and checks each against a SHA-256 pinned in
that script. Nothing is trusted on the strength of TLS alone; a hash mismatch
refuses to cache the file at all. Results are cached under
`tools/.vendor-cache/` (gitignored — these are large binaries, not source).

`tools/build_macos_installer.sh` calls that fetch script automatically and
bundles the results into the payload (`bootloader-<target>.ipod`,
`stock/rockbox-<target>.zip`, hashes appended to `SHA256SUMS`). If fetching
fails — no network, e.g. — the build continues without those two assets, and
`bootloader_asset_available` / `stock_rockbox_available` in the installer
simply report the step as unavailable rather than failing the build.
`--skip-stock-assets` opts out explicitly for a fully offline dev build.

### Compiling the bootloader helpers on the Mac

`mks5lboot` and `ipodpatcher` are ordinary C tools (`utils/mks5lboot/`,
`utils/ipodpatcher/`) that, on Apple platforms, need only IOKit and
CoreFoundation — no libusb, no generated headers beyond an `APPVERSION`
string passed on the `make` command line. That means they can be compiled
by the Xcode Command Line Tools already present on a developer's Mac, with
no macOS cross-toolchain needed on this Linux build host. Their sources are
bundled read-only in the payload (`tool-src/mks5lboot`, `tool-src/ipodpatcher`,
`tool-src/libtools.make`) and compiled into
`~/Library/Application Support/RockPod/tools/` the first time a bootloader
install is requested, then reused on later runs.

One real bug surfaced while building this: `utils/libtools.make` derives
`TARGET_DIR`/`OBJDIR` from the current directory and uses them unquoted in
make rules. Both `RockPod Setup.app` and `Application Support` contain a
space, which breaks GNU Make's word-splitting there ("target ... given more
than once in the same rule"). `build_bootloader_tool()` in `lib/bootloader.sh`
works around this by copying the source into a fresh `mktemp -d` directory
(under `$TMPDIR`, which macOS keeps space-free) before running `make`, then
copying only the resulting binary back into Application Support. This was
verified by actually building both tools from the bundled source tree with a
path containing a literal space, on this Linux host — see the commit history
for the build log.

## What has *not* been verified

This is the important caveat: **no DFU flash or raw boot-partition write has
been exercised against real hardware from this Linux build environment.**
That is structurally impossible here — DFU access and DAX-level disk I/O
need macOS's IOKit and CoreFoundation, running on a Mac, with a physical
iPod attached. `tools/macos_installer_gate.sh` stubs `mks5lboot` and
`ipodpatcher` and checks the *ordering and safety invariants* the spec
requires:

- `mks5lboot --single`/`-S` is never constructed anywhere in `lib/bootloader.sh`
  (enforced by a static grep in the gate, not just a runtime check).
- Classic: the bootloader install never runs before a DFU device is
  detected.
- Video: the boot partition is read, hashed, and confirmed non-empty
  *before* any write; a failed read never reaches `--add-bootloader`; a
  successful write is re-read afterward to verify it.
- Both tools' C source builds cleanly from the exact bundled tree (proved on
  this host via the POSIX/Linux build path, which shares the same
  `ipodio-posix.c`/plain-Makefile logic the macOS Darwin branch uses).

None of that proves the real flash succeeds on an iPod. Before trusting this
build against hardware you care about, or before promoting the bootloader
step out of "optional, ask first every time," run the full physical matrix
in the spec's "Testing and Release Gates" section on a real Mac: DFU entry
timing, `--dfuscan` behavior against the actual VID/PID, and a Video
boot-partition write/read-back/recovery cycle on a disposable or
well-backed-up device first.

## Building, gating, and deploying

```bash
# Optional: pre-warm the stock asset cache (build_macos_installer.sh does
# this automatically otherwise).
tools/fetch_stock_rockbox_assets.sh --target ipod6g

# Regression gate: hash/zip verification, discovery, database guard,
# install/preservation, and the bootloader-path invariants above.
tools/macos_installer_gate.sh

# Build and copy to this machine's Desktop for transfer to a Mac.
tools/build_macos_installer.sh --target ipod6g --desktop-copy
tools/build_macos_installer.sh --target ipodvideo --desktop-copy
```

The resulting `RockPod-Setup-<target>.zip` is unsigned and will arrive on
the Mac quarantined by Gatekeeper. After transferring it:

```bash
xattr -dr com.apple.quarantine "RockPod-Setup-<target>.zip"
```

or right-click → Open on `RockPod Setup.app` and confirm the "unidentified
developer" prompt once. Match the target build to the iPod: `ipod6g` for
iPod Classic 6G/7G, `ipodvideo` for iPod Video 5G/5.5G.

## Known pre-existing gate issues

While extending `tools/macos_installer_gate.sh`, a real bug was found and
fixed in the gate's own `plutil` stub: it read the fixture file path from
the wrong argument position (`$5` instead of `$6` in
`plutil -extract KEY raw -o - FILE`), which silently broke device discovery
and several other pre-existing cases. That is now fixed.

A separate, still-open set of pre-existing failures remains in the
"database guard" and "install and preservation" sections
(`healthy database passes the structural check` and others) — reproducible
on an unmodified checkout via `git stash`, so they predate and are unrelated
to the bootloader/stock-Rockbox work this document describes. They look
environment-dependent (likely the `stat`/`od` stubs on this particular
build host) rather than a defect in the installer logic itself, but they
have not been root-caused here and are worth a dedicated look before relying
on that section of the gate.
