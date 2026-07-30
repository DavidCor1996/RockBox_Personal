# RockPod Easy Installer Specification

## Product Summary

RockPod Setup is a signed desktop installer and guided device setup experience
for Linux and macOS. It installs the RockPod desktop application, identifies a
supported iPod, installs or updates the correct Rockbox bootloader, and installs
the current Personal Rockbox release.

The experience should feel like a 2007 iPod Classic brought forward to a modern
desktop: polished metal, black glass, blue list selections, compact typography,
and direct language. It must still behave like a current installer with
accessible controls, clear progress, resumable downloads, and conservative
device safety.

The default first-run journey is:

1. Install and launch RockPod.
2. Connect one supported iPod.
3. Review the detected model, storage, filesystem, and installed software.
4. Download and verify the matching release bundle.
5. Install Personal Rockbox to the mounted data volume.
6. Install the bootloader using the target-specific guided flow.
7. Verify the device, safely eject it, and show first-boot instructions.

The user should not need Python, a compiler, Homebrew, a Linux package manager,
or command-line knowledge.

## Goals

- Provide one obvious `Set Up iPod` action for a first-time installation.
- Install RockPod itself as a normal desktop application.
- Support iPod Classic 6G/7G and iPod Video 5G/5.5G.
- Support current 64-bit Linux desktops and current Intel and Apple silicon
  Macs.
- Select artifacts from positively identified hardware. Never ask a novice to
  choose a firmware filename.
- Preserve Apple dual boot when the supported bootloader path allows it.
- Preserve music, playlists, RockPod data, Rockbox settings, themes, saved
  games, and the Rockbox tagcache/database during updates.
- Make every downloaded and written artifact verifiable.
- Leave a useful recovery bundle and human-readable installation report.
- Make interruption before bootloader flashing recoverable by rerunning Setup.

## Non-Goals

- Windows support in version 1.
- Unsupported iPod generations, iPod touch, iPhone, or unknown USB storage.
- Automatic HFS-to-FAT32 conversion.
- Repartitioning, formatting, or restoring an iPod.
- Installing an unreviewed build from the repository default branch.
- Building RockPod, Rockbox, a cross-compiler, or bootloader tools on the
  user's computer.
- Single-boot installation. The dangerous `mks5lboot --single` path must never
  be exposed by the GUI or bundled helper.
- Silently replacing a working bootloader just because a newer one exists.
- Removing Apple firmware.

## Supported Matrix

| Device | Rockbox target | Bootloader path | Firmware package |
| --- | --- | --- | --- |
| iPod Classic 6G/7G | `ipod6g` | `mks5lboot` dual-boot DFU installer | `rockbox-ipod6g.zip` |
| iPod Video 5G 30 GB | `ipodvideo` 32 MB | `ipodpatcher` boot-partition update | 32 MB Video package |
| iPod Video 5G/5.5G 60/80 GB | `ipodvideo` 64 MB | `ipodpatcher` boot-partition update | 64 MB Video package |

The release manifest is authoritative for exact filenames. If the 32 MB and
64 MB builds become byte-identical, they may refer to one artifact, but the
installer must retain the model distinction and compatibility check.

Initial host support:

| Host | Package | Minimum |
| --- | --- | --- |
| Linux x86_64 | AppImage plus desktop integration | glibc 2.35-era distribution, Wayland or X11 |
| Linux arm64 | AppImage, once CI and USB hardware tests are available | same |
| macOS universal | signed and notarized `.dmg` containing `RockPod.app` | macOS 13 |

An arm64 Linux build must not be advertised until it passes the same physical
USB bootloader matrix as x86_64. A platform absent from the signed release
manifest is unsupported even if the Python source could run there.

Both Linux and macOS support FAT32-formatted iPods. If an iPod is HFS-formatted,
Setup explains why Rockbox cannot run from that volume and links to a separate
preparation guide. It does not offer a one-click destructive conversion.

## Meaning of "Current Version"

`Current` means the newest signed release in the user's selected channel, not
the newest commit and not an unqualified GitHub archive.

Channels:

- `Stable`, the default and only channel shown during normal onboarding.
- `Preview`, opt-in under Advanced and clearly labeled as pre-release.
- `Local bundle`, a developer-only offline path enabled with an explicit
  command-line flag.

Each release is an immutable, internally compatible set:

- RockPod desktop version;
- Personal Rockbox firmware version per device target;
- Rockbox bootloader version per device target;
- host bootloader helper version;
- release notes and minimum supported versions.

The app checks for a newer manifest at launch and when `Check for Updates` is
pressed. A cached, still-valid manifest can be used to resume an interrupted
install. Setup must never silently switch releases after the review screen.

## Release Manifest

The release pipeline publishes a small signed `channel.json`. The signature is
verified with a public key embedded in RockPod Setup. TLS is required but is
not the only integrity control.

Minimum logical shape:

```json
{
  "schema": 1,
  "channel": "stable",
  "release": "4.1.0",
  "published_at": "2026-07-25T18:00:00Z",
  "minimum_setup_version": "1.0.0",
  "notes_url": "https://example.invalid/releases/4.1.0",
  "artifacts": {
    "rockpod_macos_universal": {
      "url": "https://example.invalid/RockPod-4.1.0.dmg",
      "size": 123,
      "sha256": "..."
    },
    "rockbox_ipod6g": {
      "url": "https://example.invalid/rockbox-ipod6g.zip",
      "size": 123,
      "sha256": "...",
      "firmware_sha256": "..."
    },
    "bootloader_ipod6g": {
      "url": "https://example.invalid/bootloader-ipod6g.ipod",
      "size": 123,
      "sha256": "..."
    }
  },
  "compatibility": {
    "ipod6g": {
      "firmware": "rockbox_ipod6g",
      "bootloader": "bootloader_ipod6g",
      "bootloader_method": "mks5lboot-dual"
    }
  }
}
```

The production schema also includes all Video variants, helper binaries,
archive entry hashes, minimum bootloader versions, signing key ID, manifest
expiry, and a detached signature. URLs shown above are placeholders.

Release rules:

- Artifacts are immutable after publication.
- SHA-256 is recorded for every downloaded file and every firmware payload
  extracted from an archive.
- The manifest signature and artifact hashes are checked before elevation or
  any device write.
- Zip entries containing absolute paths, `..`, symlinks, special files, or
  case-colliding paths are rejected.
- Setup rejects a package whose embedded `rockbox-info.txt` target differs
  from the detected target.
- A release cannot be promoted to Stable unless both target simulator gates
  and the host/device test matrix pass.
- Key rotation uses a manifest signed by both the old and new keys.

## Application Packaging

RockPod is frozen into a self-contained Qt application using PyInstaller or
Nuitka. The packaging proof of concept should compare startup time, bundle
size, Qt plugin discovery, multimedia support, and macOS notarization before
choosing one.

The application bundle contains:

- RockPod Python code and Python runtime;
- PySide6/Qt libraries and required platform plugins;
- application icons and redistributable theme assets;
- a small unprivileged setup orchestrator;
- signed, target-specific privileged helpers;
- `mks5lboot`, `ipodpatcher`, and their license/source notices;
- the embedded manifest verification public key;
- no Personal Rockbox firmware or bootloader unless building an explicitly
  versioned offline installer.

Linux installation:

- The downloaded AppImage can run directly.
- `Install RockPod` copies it to
  `~/.local/opt/rockpod/RockPod-<version>.AppImage`, creates a stable
  `RockPod.AppImage` link, and writes a `.desktop` entry and icons under the
  user's local application directories.
- System-wide installation is an Advanced option, not the default.
- Application updates are downloaded beside the existing AppImage, verified,
  switched atomically through the stable link, and retain the prior version
  for rollback.

macOS installation:

- The user drags `RockPod.app` to `/Applications`, or launches it from the
  mounted image and accepts a single `Move to Applications` offer.
- The app is hardened-runtime signed and notarized.
- Updates use a signed appcast/update framework or a complete signed
  replacement bundle. Setup must not patch files inside the app bundle.
- App data remains under `~/Library/Application Support/RockPod`, caches under
  `~/Library/Caches/RockPod`, and logs under `~/Library/Logs/RockPod`.

Linux data follows the XDG directories. Existing RockPod config and databases
are migrated on first launch and are never stored inside the application
bundle.

## Privilege Boundary

Most of Setup runs without elevated privileges. Raw disk and DFU USB access is
isolated in a small helper with a narrow command surface.

Allowed helper operations:

- inspect a specific resolved disk and return model/partition metadata;
- read a bounded iPod boot partition to a user-selected backup file;
- add or remove a supported Video bootloader;
- scan for the supported Classic DFU USB VID/PID;
- send a manifest-approved Classic dual-boot DFU installer;
- unmount and remount one positively identified iPod volume when required;
- flush writes for that device.

The helper must not accept an arbitrary executable, arbitrary shell command,
arbitrary destination path, or the `mks5lboot --single` option.

Linux uses a reviewed `pkexec` policy or narrowly scoped udev access. macOS uses
a signed privileged helper installed through the supported Service Management
API. The GUI displays the exact device model, stable disk identity, and action
before the operating system authorization prompt.

The elevated helper independently rechecks:

- the release manifest signature;
- requested artifact SHA-256;
- whole-disk identity and expected iPod partition structure;
- supported model and target;
- that exactly one candidate matches the request;
- that the request has not expired;
- that the disk identity still matches the unprivileged preflight.

The helper rejects raw operations against the host boot disk, mounted system
volumes, generic removable storage, and unresolved paths.

## Detection and Preflight

Detection cannot depend on an existing `.rockbox` directory because first-time
installs do not have one.

Linux discovery combines udev/`lsblk` metadata, mount information, USB
identity, partition layout, and a read-only `ipodpatcher` or `mks5lboot` probe.
macOS discovery combines Disk Arbitration, IOKit USB identity, `diskutil`
metadata, and the same target-specific read-only probe.

RockPod's existing mounted-device detector must be extended to scan `/Volumes`
on macOS and to use native eject behavior there. The installer discovery
service should be separate from the normal Rockbox-volume polling service.

Preflight report fields:

- host OS, architecture, and Setup version;
- device family, generation/model, capacity, and serial or stable hardware
  signature;
- whole-disk node and mounted data-volume path;
- FAT32 status and available space;
- existing Rockbox target/version, if present;
- bootloader presence/version/status, if confidently detectable;
- Apple firmware compatibility for Classic dual boot;
- Personal Rockbox release selected;
- files that will be created or replaced;
- database and recovery status;
- whether elevation and DFU mode are required.

Hard stops:

- no supported iPod or more than one candidate iPod;
- device identity changes between detection and write;
- unsupported, ambiguous, or contradictory model information;
- HFS or unknown filesystem;
- mounted read-only volume;
- insufficient free space;
- failed SMART/storage I/O observed during read-only checks, when available;
- missing or invalid release signature/hash;
- wrong-target firmware or bootloader;
- an active RockPod sync, database build, or media copy;
- incomplete Rockbox database on an update;
- inability to create and verify the recovery backup;
- host sleep or imminent shutdown notification during a write stage.

The review screen never allows `Continue anyway` for a hard stop.

## Installation State Machine

The operation is journaled after every successful transition:

```text
idle
  -> detecting
  -> preflight
  -> downloading
  -> verified
  -> recovery-backed-up
  -> database-ready
  -> firmware-staged
  -> firmware-verified
  -> bootloader-waiting
  -> bootloader-writing
  -> bootloader-verified
  -> final-device-verified
  -> synced
  -> safely-ejected
  -> complete
```

The journal records no secrets and lives in the user's RockPod data directory.
It contains the release ID, hashes, device identity, completed stages, backup
location, and sanitized tool results.

Cancel behavior:

- Safe and immediate during detection and download.
- Safe after finishing the current file while staging firmware.
- Disabled while the bootloader helper is writing.
- Re-enabled after the helper returns and verification completes.

Closing the window during a protected stage minimizes it and explains why.
After a crash or power interruption, the next launch offers `Resume checks` and
starts with read-only inspection. It never assumes the prior write completed.

## Firmware Installation

The same transactional engine serves both targets, with the stricter 6G
deployment contract retained.

### Existing Personal Rockbox Installation

1. Confirm the installed target matches the selected package.
2. Verify all existing `database*.tcd` and `tagcache*.tcd` files are readable.
3. Copy database files and selected configuration/recovery metadata into a
   timestamped host backup.
4. Stage and validate the release archive outside the device.
5. Apply the package without replacing the mounted `.rockbox` directory
   wholesale.
6. Restore the preserved database files byte-for-byte.
7. Remove only known transient tagcache transaction artifacts.
8. Keep `tagcache_autoupdate` enabled.
9. Install a verified last-known-good database snapshot on the device.
10. Copy the matching `rockbox.ipod` to both:
    - `/rockbox.ipod`
    - `/.rockbox/rockbox.ipod`
11. Verify both device copies match the release firmware SHA-256.
12. Parse the database again and verify indexed media paths.

For a full iPod 6G package, this path must use
`tools/deploy_ipod6g_preserve_database.sh` or a reviewed library extraction of
that exact logic. The script remains the executable source of truth until the
shared transactional library exists.

Missing, unreadable, changed, or empty database files on an existing 6G
installation are a hard stop. Setup offers database repair as a separate,
non-install workflow.

### Fresh Personal Rockbox Installation

A fresh device has no database to preserve. To retain the repository's 6G
database safety invariant, Setup performs these steps before the full package
deploy:

1. Create the target `.rockbox` staging layout on the mounted volume.
2. Create `Music` if it is absent; never alter existing media.
3. Generate tagcache files on the host from the device's existing supported
   media using `services/rockbox_tagcache.py`.
4. Validate the generated database with the same parser used by the deployment
   guard and confirm every indexed path remains under the device mount.
5. Enable `tagcache_autoupdate`.
6. Invoke an explicit, tested fresh-install mode of
   `tools/deploy_ipod6g_preserve_database.sh`, which accepts only the
   just-generated validated database and then follows the normal preservation
   path.

The fresh-install mode is an implementation prerequisite, not permission to
weaken the existing default guard. Calling the current script against a device
without a validated database must continue to fail.

If host tagcache generation cannot parse existing media, Setup stops before
device firmware installation and offers a diagnostic report. It does not
install an empty placeholder database over a device containing music.

### Rollback

The host recovery bundle contains:

- install report and signed release manifest;
- original `rockbox.ipod` copies, if present;
- original Rockbox database/tagcache files;
- original config and theme selection;
- a file manifest with size and SHA-256;
- for Video devices, the original boot partition backup;
- the exact helper/tool versions used.

Firmware rollback is offered only when the backup target and current device
identity match. Bootloader removal is a separate Advanced recovery action and
is never part of ordinary application uninstall.

## Bootloader Workflows

Firmware is installed and verified before a first bootloader install, matching
the `mks5lboot` recommendation and preventing a bootloader from pointing at an
absent firmware.

### Classic 6G/7G

1. Confirm a supported FAT32 Classic and compatible Apple firmware.
2. Download and verify `bootloader-ipod6g.ipod` and the host helper.
3. Show an animated, accessible DFU guide:
   - safely unmount the data volume;
   - hold Select and Menu;
   - continue holding through the reset until DFU is detected.
4. Poll read-only with the equivalent of `mks5lboot --dfuscan`.
5. Stop polling and acquire exclusive access after exactly one supported DFU
   device is found.
6. Run the dual-boot equivalent of:

   ```text
   mks5lboot --bl-inst bootloader-ipod6g.ipod
   ```

7. Never pass `--single`.
8. Report the tool result and the expected audible success tones.
9. Wait for the iPod to reboot and reappear, then re-identify it before final
   volume verification.

If macOS launches a process that takes the DFU device, Setup reports the
specific conflict and gives a retry path. It must not kill unrelated processes
without explicit approval.

Classic bootloader updates are optional and separately confirmed. If the
installed bootloader is compatible with the selected firmware, the default
button is `Keep current bootloader`.

### Video 5G/5.5G

1. Confirm the exact Video model, RAM class, FAT32 layout, and whole-disk node.
2. Unmount the data partition while retaining the resolved whole-disk identity.
3. Read the boot partition to the host recovery bundle.
4. Hash the backup, inspect it with `ipodpatcher`, and verify it can be read
   back before continuing.
5. Add the matching dual-boot bootloader using the non-interactive equivalent
   of:

   ```text
   ipodpatcher <resolved-device> --add-bootloader \
       bootloader-ipodvideo.ipod
   ```

6. Re-read and inspect the boot partition after the write.
7. Remount the data volume, re-identify the iPod, and verify both firmware
   copies and database files.

`ipodpatcher`'s own iPod identity checks are required but not sufficient; the
privileged helper applies the independent disk identity checks described
above.

## User Experience

### Visual Language

The setup window uses a restrained 2007 iPod vocabulary rather than a literal
skin:

- a satin silver frame around a black-glass content panel;
- white and cool-gray surfaces with one iPod-style cobalt selection color;
- subtle one-pixel separators and controlled highlights;
- a compact source list on the left and a focused detail pane on the right;
- an optional click-wheel-inspired progress ring, used only as status, not as
  the sole navigation method;
- device illustrations drawn in-house without Apple logos;
- short labels such as `Connect`, `Review`, `Install`, and `Ready`.

Avoid fake LCD blur, heavy skeuomorphic reflections, tiny text, excessive
animation, and copied Apple artwork. Use redistributable fonts and assets only.

Suggested window:

```text
┌──────────────────────────────────────────────────────────────────────┐
│ RockPod Setup                                          —  □  ×       │
├──────────────────┬───────────────────────────────────────────────────┤
│ ● Welcome        │  Your iPod Classic is ready to set up            │
│ ● Connect        │                                                   │
│ ● Review         │      ┌──────── device illustration ────────┐     │
│ ○ Install        │      │ iPod Classic 160 GB                  │     │
│ ○ Ready          │      │ FAT32 · Personal Rockbox 4.1.0       │     │
│                  │      └──────────────────────────────────────┘     │
│                  │                                                   │
│                  │  RockPod app        Ready                         │
│                  │  Personal Rockbox   4.1.0                         │
│                  │  Bootloader         Install required              │
│                  │                               [Set Up iPod]        │
└──────────────────┴───────────────────────────────────────────────────┘
```

### Screens

1. **Welcome** — Install RockPod, Set Up an iPod, or Restore from Backup.
2. **Connect** — Live device detection and disk/DFU instructions.
3. **Review** — Exact model, release, backup location, changes, and warnings.
4. **Install** — One overall progress indicator plus the current plain-language
   action. Technical log is collapsed under `Show Details`.
5. **DFU** — Classic-only visual button timing guide with keyboard-accessible
   text instructions and live detection state.
6. **Ready** — Verified checks, safe-eject result, first boot controls, dual
   boot reminder, and `Open RockPod`.

Primary actions stay in a consistent lower-right location. Dangerous recovery
actions live under Advanced and use explicit verbs rather than color alone.

### Accessibility and Motion

- Full keyboard and screen-reader navigation.
- Minimum 4.5:1 text contrast.
- Status never communicated only by color or animation.
- Text remains usable at 200% scaling.
- Honor reduced-motion settings.
- DFU timing animation has an equivalent numbered text procedure.
- Do not play UI sounds by default.

## Error Design

Every failure includes:

- what Setup was doing;
- whether the device was changed;
- the last verified safe state;
- the exact next action;
- `Save Diagnostic Report`;
- a stable error code suitable for support documentation.

Examples:

| Code | Meaning | Next action |
| --- | --- | --- |
| `RP-DISK-AMBIGUOUS` | More than one iPod or model mismatch | Disconnect other devices and retry |
| `RP-FS-HFS` | HFS-formatted iPod | Follow the FAT32 preparation guide |
| `RP-DB-INVALID` | Existing tagcache missing or unreadable | Open separate database repair |
| `RP-HASH-MISMATCH` | Download or device copy failed verification | Redownload; do not eject |
| `RP-DFU-NOT-FOUND` | Classic did not enter supported DFU mode | Repeat the guided button sequence |
| `RP-BOOT-WRITE` | Bootloader helper failed during write | Keep connected and run recovery checks |
| `RP-REMOUNT-FAILED` | Write finished but volume did not return | Reconnect, then resume verification |

Logs redact usernames from display paths when exported, exclude music metadata
by default, and never upload automatically.

## Internal Architecture

Suggested new modules:

```text
rockpod/
  installer/
    release_feed.py          signed channel and artifact verification
    artifact_cache.py        resumable downloads and content-addressed cache
    discovery.py             common host/device model
    discovery_linux.py       udev, lsblk, mounts, udisks
    discovery_macos.py       Disk Arbitration, IOKit, diskutil
    preflight.py             compatibility and hard-stop rules
    journal.py               resumable state machine
    recovery.py              backup manifest and restore validation
    firmware_install.py      transactional package orchestration
    bootloader_classic.py    DFU guidance and helper protocol
    bootloader_video.py      boot partition backup/helper protocol
    reports.py               redacted diagnostic/install reports
  ui/
    setup/
      window.py
      welcome_page.py
      connect_page.py
      review_page.py
      install_page.py
      dfu_page.py
      ready_page.py
```

The privileged helper is a separately built, minimal native component. It
communicates with the GUI using structured, versioned messages and emits
machine-readable progress. Existing command output may be retained in the
technical log, but must not be parsed as the primary API once the helper exists.

RockPod's normal sync UI consumes the same release, discovery, firmware,
recovery, and eject services so first-run setup and later `Update iPod`
operations cannot drift into separate safety implementations.

## Testing and Release Gates

### Automated

- Manifest canonicalization, signature, expiry, key rotation, wrong key, and
  downgrade tests.
- Artifact resume, corrupted cache, wrong size/hash, hostile zip, and
  wrong-target package tests.
- Device-model fixtures for every supported capacity/RAM/partition variant.
- Host discovery tests using captured and synthetic Linux/macOS metadata.
- State-machine interruption test at every transition.
- Privileged helper allowlist and adversarial argument tests.
- Host-system-disk and generic-USB rejection tests.
- Fresh install tagcache generation and validation tests.
- Update preservation tests proving database files remain byte-identical.
- Verification that both `rockbox.ipod` destinations match the release.
- Existing RockPod unit suite and both relevant simulator-first gates.
- Packaging smoke test on a clean host with no Python installation.

### Physical Hardware Matrix

At least:

- Classic 6G 80 GB;
- Classic 6G/7G 120 GB;
- Classic 7G 160 GB;
- Video 5G 30 GB / 32 MB;
- Video 5G or 5.5G 60/80 GB / 64 MB.

For each device, test on Linux and macOS:

- fresh FAT32 install;
- existing Personal Rockbox update with populated database;
- compatible bootloader kept;
- bootloader update;
- download interruption;
- disconnect before writes;
- attempted disconnect during firmware copy;
- failed remount followed by resume;
- database corruption hard stop;
- wrong-target and generic-drive rejection;
- dual boot into Rockbox and Apple firmware;
- safe eject and reconnect;
- firmware rollback;
- Video bootloader recovery from the installer-created backup.

No automated power-loss test is performed during an actual bootloader flash.
The helper and journal can be fault-injected before and after the raw write,
while destructive interruption testing uses a disk image or sacrificial lab
device.

### Simulator and Repository Gates

Run:

```bash
tools/simulator_first_gate.sh --target ipod6g --rockpod-tests --smoke
tools/simulator_first_gate.sh --target ipodvideo --rockpod-tests --smoke
```

The 6G physical package path must continue through
`tools/deploy_ipod6g_preserve_database.sh`. Both on-device firmware checksums
must match before sync/eject.

## Delivery Plan

### Milestone 1 — Safe Updater

- Signed release feed and artifact cache.
- Linux mounted-device discovery.
- Update an existing Personal Rockbox installation.
- Database preservation and two-copy firmware verification.
- No bootloader writes.

### Milestone 2 — Packaged RockPod

- Self-contained Linux AppImage and notarized universal macOS app.
- User-data migration and application updater.
- `/Volumes` discovery and native macOS eject.
- Setup visual shell and accessibility baseline.

### Milestone 3 — Fresh Firmware Install

- First-time FAT32 device detection.
- Host tagcache bootstrap and explicit guarded fresh-install deploy mode.
- Recovery bundle and resumable journal.
- Physical firmware matrix on both hosts.

### Milestone 4 — Bootloaders

- Minimal signed privileged helpers.
- Video boot-partition backup/install/verification.
- Classic DFU guide and dual-boot install.
- Recovery and fault-injection testing.

### Milestone 5 — Stable Release

- External clean-machine tests.
- Code signing, notarization, SBOM, bundled GPL notices, and corresponding
  source offer/download.
- Stable channel promotion only after the complete physical matrix passes.

## Definition of Done

The installer is ready for Stable when a non-technical user can start with a
clean supported Linux or Mac, install RockPod, connect any device in the
supported matrix, and complete a verified dual-boot Personal Rockbox install
without opening a terminal.

Completion additionally requires:

- no unsupported or ambiguous disk can reach a write operation;
- no first-time or update path replaces `.rockbox` wholesale;
- existing database files survive an update byte-for-byte;
- a fresh install has a generated, parsed, valid initial database;
- both firmware locations match the signed release;
- a verified host recovery bundle exists before bootloader writes;
- Classic installation never exposes or invokes single boot;
- Video installation verifies a boot-partition backup before modification;
- safe eject succeeds or the UI clearly keeps the operation incomplete;
- all automated, simulator, packaging, and physical-host/device gates pass.
