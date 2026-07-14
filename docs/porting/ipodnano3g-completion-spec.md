# iPod Nano 3G Completion Specification

Date: 2026-07-13

This is the acceptance specification for turning the current tethered Nano 3G
bring-up into a repeatable Rockbox installation with working plugins and music
playback.  It supersedes the optimistic phase labels in
`ipodnano3g-roadmap.md`; historical bring-up notes remain useful evidence but
are not completion criteria.

## Validated Baseline

The following behavior has been observed on the personally owned 4 GB Nano 3G:

- transient wInd3x type-2 DFU execution and recovery;
- native Rockbox `crt0`, system, kernel tick, click wheel, LCD and backlight;
- panel ID `00 58 91 71`, type 4, with correct colors after the P9 transfer
  correction;
- read-only storage initialization and a bounded synthetic FAT view;
- a full application boot to the Rockbox menu with the packaged iPodJS theme;
- no NAND programming, NAND erase, NOR programming, or permanent boot change.

The validated P9 application is still an exact-layout experiment.  Its fast
mount exposes a synthetic directory/FAT view plus physically mapped firmware
and bounded theme files.  It does **not** establish arbitrary-volume reads,
codec identity, PCM output, persistent settings, or a permanent installation.

## Definition of Done

The port is ready for a consistent user installation only when all of these are
true on hardware:

1. A clean checkout produces the core, complete plugin set, codec set, fonts,
   themes, and one installable package without hand-created files.
2. The installer selects only the intended Nano 3G, preserves user data,
   installs the complete package, writes both firmware copies, verifies both
   copies against the local build, and records an auditable manifest.
3. Read-only storage resolves the current FTL metadata rather than a particular
   file extent and supports arbitrary aligned and unaligned FAT reads across
   reboot, file replacement, and a changed allocation layout.
4. Rockbox loads native plugins and codecs from ordinary FAT paths.  Plugin/core
   API versions match and missing or transient storage failures are reported
   without corrupting the volume.
5. The actual audio codec is identified from read-only evidence.  Its power,
   reset, clock, I2C, headphone, mute, and volume paths are verified before
   ordinary playback is enabled.
6. PCM/I2S/DMA plays a bounded diagnostic tone, then WAV, FLAC, and MP3 content.
   Pause, resume, seek, volume, headphone insertion/removal, and clean stop work.
7. Database and Files playback survive transitions to and from audio plugins as
   required by `docs/plugin-audio-lifecycle-steering.md`.
8. Settings, resume state, playlists, and database state persist only after the
   write path has its own recovery and power-loss test campaign.
9. A permanent boot path is offered only after transient boot, recovery, storage
   reads, audio, and controlled storage writes are independently proven.

## Implementation and Gate Status

| Area | Current implementation | Remaining hardware gate |
| --- | --- | --- |
| Reproducible build | A clean parallel `make -j4 zip` now orders generated plugin headers correctly and links the configured large plugins; the package contains 193 `.rock` and 43 `.codec` files. The plugin arena matches the proven 3 MiB reservation used by comparable 32 MiB iPods. `tools/nano3g_release_audit.py` records the artifact, source, toolchain, and sorted file manifests. | Run selected plugin/codec smoke tests from a non-synthetic FAT view. |
| Package/install | `tools/nano3g_deploy.py` defaults to a read-only plan, identifies the Nano by USB/removable geometry and exact serial, rejects the 512 GB iPod 6G, validates the `nn3g` model-117 firmware checksum, overlays the package, backs up firmware, atomically installs both firmware copies, syncs, and verifies every package file. | Use `--apply` only after the new firmware extent is mapped or general storage is working; then cold-boot and verify. |
| DFU payload safety | `tools/nano3g_mkdfu.py` creates and verifies only volatile type-2 wrappers, enforces the 128 KiB staging limit, and rejects persistent type-3 installers. | Confirm the next read-only probe screen and recovery. |
| LCD/input/kernel | Native path validated, including correct P9 colors and all click-wheel controls. | Regression-check after storage/audio changes. |
| Storage | NAND writes and erases are hard-disabled. A fast exact P9 mount boots the current image. A separately guarded general read-only FTL mount is prepared for metadata-driven testing. | Prove current metadata selection, FAT BPB/root, random files, full-package plugins/codecs, and repeated cold boots with different file extents. |
| Plugins | Full plugin package builds. Core loading includes bounded retry for transient storage reads. | Load simple, large, overlay, game, and audio plugins from ordinary FAT paths; verify clean exit and API matching. |
| Codec identity | A transient bootloader probe reads the NOR `SysCfg` `Codc` entry and displays text plus all 16 raw bytes. It cannot program or erase NAND/NOR. | Report the `CODC` text and hex from the Nano screen. Do not choose a driver from guesswork. |
| Audio hardware | S5L8702 PCM and codec shims compile, but safe bring-up deliberately suppresses output and the active CS42L55 selection is documented as a placeholder. | Identify codec, then prove read-only identity/register access, power/reset/clock sequence, and a guarded diagnostic tone. |
| Music playback | Software codecs build, but storage and hardware PCM are not yet end-to-end. | WAV first, then FLAC/MP3, followed by lifecycle and long-play tests. |
| Writes/persistence | Disabled by `FTL_READONLY`; deploy is performed only by Apple Disk Mode on the host. | Implement only after read-only FTL has broad coverage; require power-loss, remap, free-space, and recovery validation. |
| Permanent boot | Intentionally absent. | Last milestone, with a documented restore path and explicit user authorization. |

## Reproducible Build and Package Contract

Configure a normal out-of-tree Nano 3G build, run `make clean`, then run
`make -j4 zip`.  This single clean package target must generate the core,
plugins, codecs, languages, and package without a warm build directory.  A
release candidate fails if any configured plugin silently falls through to an
incomplete generic make rule or if a plugin exceeds the declared arena.  Run
`tools/nano3g_release_audit.py` against the resulting firmware, zip, and build
directory and retain its JSON plus sorted manifests.  The record includes:

- source commit and dirty-state declaration;
- `rockbox.ipod` byte count, SHA-256, model tag, and model checksum;
- `rockbox.zip` SHA-256 and entry count;
- counts and sorted manifests of `.rock` and `.codec` files;
- toolchain version and configure summary.

The zip is the installation source.  Individual file copying is a diagnostic
operation, not a consistent install method.

## Host Deployment Contract

The deployment tool must remain fail-closed:

- dry-run unless `--apply` is explicit;
- select serial `000A27001AF57313` and reject serial
  `000A27002101824D` even when mount labels are similar;
- require a mounted FAT partition belonging to a removable USB iPod of the
  expected 3--5 GB size;
- never format, repartition, raw-write, unmount, or eject;
- reject traversal, symlinks, duplicate paths, special files, and implausibly
  large packages;
- reject conflicting FAT case aliases while coalescing byte-identical legacy
  aliases whose names differ only by case;
- overlay `.rockbox` without deleting settings or media;
- create a host-side backup and JSON manifest before replacement;
- install the same local `rockbox.ipod` at `/rockbox.ipod` and
  `/.rockbox/rockbox.ipod`;
- verify both firmware SHA-256 values and every package file before reporting
  success; then sync.

Changing `rockbox.ipod` can change its physical NAND extent.  Until the general
FTL path is proven, a new deployed application also requires a newly verified
transient exact-map loader.  Therefore package deployment and boot-loader
selection are one release operation, not independent copy commands.

## Read-Only Storage Acceptance

The existing exact mount remains the known-good fallback.  The general mount
must be enabled by a separate build flag until it passes all gates below:

1. Select the newest valid type-43 context/control metadata by its generation,
   including the observed rotating control block pairs, without a whole-device
   data sweep on every boot.
2. Load and validate every referenced type-44 map page and type-45 log page;
   reject out-of-range banks, blocks, pages, indices, and duplicates.
3. Resolve the real MBR and FAT32 BPB and validate partition bounds, sector
   size, cluster geometry, FAT size, root cluster, backup BPB, and FSInfo.
4. Translate host 512-byte sectors correctly when the FAT logical sector is
   4096 bytes, including all four NAND page lanes and log-block replacements.
5. Return an error for unmapped, stale, wrong-type, wrong-LPN, ECC, or remap
   failures.  Never substitute zero-filled data as a successful read.
6. Pass repeated and shuffled reads of host-generated checksum fixtures at the
   beginning, middle, and end of the volume, across cluster boundaries and after
   recreating files so their extents change.
7. Mount and load the complete package, settings, themes, fonts, language,
   plugins, codecs, and test music without any synthetic directory entries.

No write implementation is implied by a successful read-only mount.

## Plugin Acceptance

Use a package built with the same core/API.  The smoke sequence is:

1. a small resident plugin such as Credits;
2. a simple file-reading plugin;
3. an overlay plugin;
4. a plugin near the arena limit (ScummVM is the build-size gate);
5. an iPodJS/theme-engine workload with all ordinary assets;
6. a non-audio game and clean return to the menu;
7. an audio plugin only after PCM is proven.

For each, test launch, input, file access, exit, relaunch, and a subsequent core
file read.  Retrying transient open/header/seek/image reads may absorb a brief
storage wake-up, but it must not mask a missing file, API mismatch, corrupt
plugin, or persistent I/O failure.

## Codec and PCM Sequence

Audio work is evidence-gated:

1. Run the read-only `SysCfg` probe and record `Codc`, `HwVr`, and model data.
2. Compare that identity and the board wiring with the nearest Apple targets;
   remove the CS42L55 placeholder only when a supported driver is selected.
3. Add a read-only I2C identity/register probe.  A missing ACK or mismatched ID
   must safe-halt without trying alternate write sequences.
4. Implement target-specific codec power, reset, MCLK, I2S format, mute, and
   headphone routing behind a diagnostic flag.
5. Play a short low-amplitude PCM tone from a fixed buffer, stop DMA, clear its
   callback, mute, and power down.  Recovery and UI input must still work.
6. Play a short PCM WAV through the core, then FLAC and MP3 through loadable
   codecs.  Add AAC/ALAC/Vorbis/Opus only after the basic pipeline is stable.
7. Test volume bounds, silence, pause/resume, seek, track change, USB insertion,
   headphone insertion/removal, one-hour playback, and battery/thermal behavior.
8. Run the complete plugin-audio transition matrix from
   `docs/plugin-audio-lifecycle-steering.md` before declaring plugins complete.

## Write, Persistence, and Permanent Install Gates

Write support is a separate project.  It must have bounded allocation and log
replay, spare/remap handling, generation ordering, flush semantics, and a test
that interrupts power at each commit boundary.  Required evidence includes a
byte-for-byte host backup, Apple Disk Mode recovery, filesystem checks after
every fault point, free-space exhaustion, repeated settings updates, database
creation, playlist edits, and large sequential media copies.

Until then:

- native Rockbox remains read-only;
- settings changes are session-only;
- all file installation is done through Apple Disk Mode;
- no NOR bootloader or persistent DFU installer is used.

## Current Next Hardware Boundary

The next action is deliberately narrow: reset the Nano into BootROM DFU, run the
volatile type-2 `SysCfg` codec probe, and report the complete `CODC` text and
hex shown on screen.  That single read-only result determines the real audio
driver; codec or PCM register writes before it are prohibited.
