# Desktop Mode: Phase 1 implementation and verification

Phase 1 software work for the iPod Classic 6G. A progress package was
installed on the mounted iPod at the user's request, followed by the verified
completion package. USB HOST, hub and DisplayLink hardware have not been qualified.

## Desktop and Photos

The existing menu bar, application identities, captured Aqua chrome, Dock,
Dashboard, Launchpad, window stack and application handoffs remain in use.
The same bounded control registry handles pointer hit testing and focus.
The optional `desktop640` simulator target lays out a 640×480 desktop with
readable text and denser application windows; it is not native display support.

System Preferences → Desktop provides Default/Restore, Choose Image,
Fill/Fit/Center, and four recent wallpapers. Selection prepares RGB565 pixels
once, using at most 1 MiB of the unused plugin arena for decoding. Successful
preparation replaces the already-owned Aurora plane. Failed decoding retains
the current wallpaper. Paint paths copy cached pixels only.

Choose Image uses the **same collection discovery and supported extensions
as Photos**, shared through `lib/photo_library.c`. `/Photos` is authoritative;
the existing bounded sidecar discovery handles alternate synced roots.
Dot directories, including thumbnails/previews, are hidden. Synced BMP
previews are used after authorization of their original. BMP/JPEG originals
(including `.jpe`) work directly; GIF/PNG/PPM require the synced preview.

Photos' existing `photos.locks` records remain authoritative. The picker:

- checks folder and individual-photo codes before opening or decoding;
- checks ancestor locks when selecting a recent or saved image;
- rejects hidden sidecar paths, path aliases and paths outside the collection;
- masks code entry, supports cancellation and rejects wrong codes;
- retains at most eight verified codes in session memory, never configuration;
- clears credentials on Hold and exit; Hold restores the default background
  if the displayed wallpaper was protected;
- starts with the default when a saved wallpaper requires authentication;
- does not persist a protected subfolder as a Finder resume location.

This preserves the existing Photos access model; it does not encrypt files
or change access through ordinary Rockbox file tools.

The legacy opaque Dock shelf included the old Aurora wallpaper outside its
angled caps. Rendering now masks those corners and blends the measured
edge against the current compositor. Clean captured shelf pixels remove the
old background from edge antialiasing. No additional mask/framebuffer is
allocated, and icon reflections and magnification remain in place.

## iTunes

Artists open that artist's albums; albums open their songs. Tagcache seek IDs
carry hierarchy identity. Back restores parent selection/page. Songs, Albums,
Artists, Genres, Movies and the configured playlist catalog are available.
Search matches indexed results before pagination. Playlist browsing reads
M3U/M3U8 entries without changing the playing queue. Playback remains on the
existing Rockbox backend and changes only following explicit playback actions.

Album tiles use representative tracks for cover lookup and artist labels.
Small native covers and larger 640×480 covers are bounded plugin caches;
idle artwork service yields to input, Hold and database commits. Renderers do
not open files or query tagcache. Transport controls, elapsed progress, volume,
shuffle/repeat, empty states and current-track indication use the existing UI.

Selecting a song prepares the complete current indexed result or M3U list in
a bounded temporary file, then explicitly replaces the Rockbox dynamic queue.
Search and hierarchy filters apply before queue creation; later-page selection
retains its starting index and duplicate playlist entries retain their position.
Preparation failure leaves existing playback untouched. No library-sized RAM
array or second player is introduced. Playlist rows fall back to filenames. The global Albums category follows Rockbox's album-tag
identity; this is not a new album/artist database schema.

## Input and presentation boundaries

`lib/desktop_model.h` defines generic pointer movement, buttons, wheel, key
and text events. Clickwheel/remote controls and the simulator feed the same
control actions. Tab/arrows move focus, Enter activates, Escape unwinds and
text edits search. Ctrl+W closes the active window; TextEdit supports text and
Backspace. Routine errors and information use the existing desktop sheet with
Enter/Escape and pointer dismissal. Startup failures before assets are available
retain Rockbox's startup splash. The simulator additionally accepts sequence-numbered
`/.rockbox/desktop-event` records. USB reports are not connected to this
adapter yet. Text input currently accepts ASCII only.

`lib/desktop_surface.c` represents a caller-owned RGB565 surface with width,
height, stride, generation, clipped damage and a sink callback. A busy sink
retains damage for retry. Desktop presentation uses this boundary with the
existing LCD sink. It is synchronous: a future asynchronous transport must
copy/encode accepted damage before releasing it and re-damage failed uploads.

The existing laptop path was audited in `apps/usb_internet.c`: RPF1 over the
existing USB network transport, native LCD generation tracking, bounded
packet batches and existing YUV/LCD mirroring. Its device transport remains
intact. Desktop reuses its LCD presentation/mirroring route; it has not been
replaced by a second USB device protocol. A native external-surface allocator
and DL sink binding remain Phase 2 integration work. The 640 target proves
layout and surface contracts without claiming an attached external screen.

## USB and DisplayLink foundation

`firmware/usbhost/desktop_usb.[ch]` and `dl1xx.[ch]` are pure, host-tested C.
They are intentionally absent from firmware `SOURCES`: no new role change,
VBUS operation, interrupt callback, channel transfer or core memory reservation
runs on an iPod as a result of this work.

Implemented contracts:

- per-device capability aggregation and detach, including duplicate devices;
- None, Input Only, Display Only, Display+Keyboard, Display+Mouse and Full
  Desktop states, with Off/Display/Input/Full activation policies;
- bounded configuration/interface/endpoint and hub descriptor parsing;
- timed hub power/debounce/reset/enumeration actions and explicit completion;
- host channel records and set-role/submit/cancel controller boundary;
- Boot Keyboard changes, modifiers, rollover handling and transactional
  output-capacity failure; Boot Mouse signed motion/buttons/wheel parsing;
- base EDID header/checksum and progressive detailed-timing validation;
- DL-1xx AF 6B RGB565 uploads, literal/RLE payloads, 24-bit addresses and
  bounded 1–256-pixel commands; insufficient output space writes nothing;
- two 16 KiB command buffers with explicit submit/completion ownership.

The command-buffer helper assumes one encoder. Device transfers, key mapping
from HID usages, channel scheduling, hubs/transaction translators, DWC2 IRQ/DMA
handling, EDID control reads, DisplayLink mode programming and hotplug recovery
are not implemented controller drivers. Settings describe future policies;
they do not activate dormant hardware today.

The existing `firmware/drivers/usb-designware.c`, target `usb-s5l8702.c` and
`usb-iphone-tether.c` were audited. The latter is an existing direct-device
host experiment, not proof that the requested CCK/hub/display chain works.
Any host backend must arbitrate that controller and restore ordinary USB
DEVICE mode on every detach and failure.

## Assets and protocol provenance

No new third-party artwork was downloaded. Existing private Snow Leopard
captures and Lucida assets supply the chrome, icons, Aurora and typography.
`tools/prepare_desktop640_assets.py` verifies their hashes against the existing
private pack and writes 50 supplemental 640×480 assets plus
`desktop640-provenance.json`. It does not alter that pack's manifest. These
Apple assets retain their existing personal-use provenance; the report is not
a redistribution license.

Protocol/architecture references consulted (implementations were not vendored):

- [TinyUSB DWC2 host](https://github.com/hathach/tinyusb/blob/master/src/portable/synopsys/dwc2/hcd_dwc2.c),
  [hub](https://github.com/hathach/tinyusb/blob/master/src/host/hub.c), and
  [HID definitions](https://github.com/hathach/tinyusb/blob/master/src/class/hid/hid.h)
  for worker/controller/class separation (MIT).
- [EspUsbHost DisplayLink specification](https://github.com/tanakamasayuki/EspUsbHost/blob/main/docs/usb-display-spec.md)
  for DL-1xx wire framing, remote framebuffer behavior and transfer batching.
- [OpenBSD udl](https://github.com/openbsd/src/blob/master/sys/dev/usb/udl.c)
  and [register definitions](https://github.com/openbsd/src/blob/master/sys/dev/usb/udl.h)
  for device initialization/timing context (ISC).
- [Linux udlfb](https://github.com/torvalds/linux/blob/master/drivers/video/fbdev/udlfb.c)
  for damage, RGB565 and completion ownership (GPL-2.0).
- [tusb_libdlo](https://github.com/ianhan/tusb_libdlo) and
  [PicoGraph](https://github.com/ianhan/picograph) as additional implementation
  references; no code/assets from them were copied.

New standalone C files carry GPL-2.0-or-later notices. Wire-level unit vectors
are software evidence, not proof of compatibility with the particular DL-165.

## Verification

Focused evidence is under `/tmp/desktop-verified-ui320-v2`,
`/tmp/desktop-verified-ui640-v2`, `/tmp/desktop-final-long320`,
`/tmp/desktop-final-long640`, and `/tmp/desktop-phase1-shell-v7`.

| Check | Result |
| --- | --- |
| ASan/UBSan pure-C contract suite | Pass: hierarchy, geometry, damage/backpressure, descriptors, hub, HID, capabilities, EDID, DL raw/RLE vectors |
| Real plugin, 320×240 and 640×480 | Pass: required four-song Artist A/B fixture, hierarchy, search, playlist browsing, wallpaper, PIN rejection/cancel/accept, protected recents/restart, Dock corners |
| Playback stress, both sizes | Pass: 10 hierarchy enter/exit cycles and 20 Albums/Artists switch cycles; playlist amount and monotonic elapsed retained |
| Process file descriptors | 9 baseline and after all 10 cycles, both sizes; includes the new trace descriptor |
| Core available / allocatable | Both remain 0 throughout active playback and all 10 cycles: Rockbox has assigned the remaining arena to playback; no downward trend or playback restart |
| Complete queues | 40 indexed songs, 10 search matches, 80 M3U entries; later-page starts and duplicate-position retention verified |
| Asset pipeline fixtures | 16 passed after updating fixtures to the existing DMF2, resolution-prefixed IDs and 12-frame spinner contracts |
| Plugin arena | Constant across navigation: 1,971,200 bytes native-size simulator; 5,239,872 bytes 640 simulator |
| Existing desktop shell gate | Pass: direct pointer, Dashboard, Launchpad and DCP750 projection proof |
| `rockpod/tests/test_desktop_mode.py` | 27 passed; assertions updated for paginated hierarchy and the separate presentation sink |
| Simulator plugin builds | Pass, both resolutions; shared Photos plugin built as well |
| Native iPod 6G build | Pass: core firmware and Desktop/Photos plugins in `/tmp/desktop-native-phase1` |
| Desktop notice / keyboard | Ctrl+W, rendered desktop sheet and Enter dismissal verified at both sizes; capture waits for presented pixels rather than a fixed delay |
| Portable Music regression | Pass: 2,513 trace records, 10 hierarchy cycles, 20 Artists/Albums switches, stable descriptors, playback and memory |
| Scoped whitespace check | Pass |

The portable regression fixture initially lacked `tagnavi.config`. The gate
now supplies the shipped definition when starting from a minimal plugin fixture.
Debugging also found WPS activity updates painting over the Hold clock; the
WPS compositor now presents only Hold while locked, refreshes its clock at most
once per second and restores a complete music frame on release. Portable
stress verification passes in `/tmp/desktop-portable-stress-final2`: 2,513
trace records, 10 full-depth Music cycles, 20 Albums/Artists switches, stable
21 host descriptors after every cycle, monotonic playback and unchanged
playlist identity. Core available/allocatable remain 0 while playback owns
the arena. The ordinary trace gate's frame/timing limits pass unchanged.
The Hold screenshot confirms artwork no longer paints over the clock.

ARM audit: the resulting desktop plugin has text 78,534, data 760 and BSS
70,836 bytes. The starting plugin was text 63,354, data 496, BSS 64,696.
Large tagcache search objects were moved to bounded plugin static storage;
the inspected search frame fell from 1,980 to 540 local bytes (+36 saved
register bytes). Wallpaper preparation uses 364 local bytes (+36 saved),
lock checking 292 (+36); complete-queue preparation 428 (+36). Alternate photo-root discovery retains the existing
bounded recursion (seven directory levels). The native main stack is 8 KiB.
No added decorative core allocation or audio-buffer acquisition exists.

The native core was rebuilt amid unrelated concurrent changes in this dirty
personal tree. Its aggregate size cannot be presented as a desktop-only BSS
delta. Plugin arena/FD stability and core available/allocatable values are measured.
An opt-in simulator LCD trace observes resource and playback state in the
rendering thread without changing the plugin ABI or native firmware memory. Test fixtures use dummy SDL audio; audible hardware continuity and
native storage timing still require later qualification.

## Application and asset audit

| Application | Existing identity and treatment |
| --- | --- |
| Finder | Captured Finder icon, sidebar/list chrome, file operations and shared focus; Photos picker reuses this window |
| iTunes | Captured iTunes identity/transport; indexed hierarchy, compact split view, real covers, search and complete queues |
| Photos / Preview | Existing Preview identity and Photos plugin; shared collection rules and existing photo locks |
| TextEdit | Existing TextEdit icon and paper window; generic text entry and Backspace |
| Calculator | Existing Calculator icon and button controls retained in the common window/control system |
| System Preferences | Existing preferences icon and tabs, with Desktop settings and wallpaper sheets |
| Dashboard / Launchpad | Existing widget and application assets; shared launching, focus and dismiss behavior retained |
| DIRECTV / Sitekick / Steam / Netflix | Existing branded icons and external desktop-window handoff preserved; preparation failures use desktop notices |
| Disk / folders / Trash | Existing captured filesystem icons and selection; Trash information uses the shared sheet |

No external application's media engine or network behavior was rewritten.
The existing menu bar remains the sole menu bar, with active application names,
contextual menus and clock. All new 640 assets are derivatives of the existing
verified pack, not new downloads or hand-drawn replacement icons.

## Deployment

The database-preserving deployment script completed from `/tmp` after the
volume was remounted. Both firmware paths matched the staged progress build:
`c41f109a084bba5773dd227c64932041e1462e33b5cb4c5ff1e04a8b0637aac6`.
Desktop and Photos plugins matched the package. All 11 live database files
were byte-identical (2,929 tracks), `photos.locks` was unchanged, a verified
recovery snapshot was retained and `tagcache_autoupdate` remained enabled.
The script completed `sync`; this task did not eject the volume.

An earlier overlapping deployment hit a shared staging-directory collision
and the volume became read-only. The script now serializes deployments per
device and uses a unique recovery staging directory. No filesystem repair was
performed by this task. Successful verified installation is not a claim of
filesystem repair or audible hardware qualification. The completion package subsequently installed the final native firmware,
Desktop and Photos plugins from `/tmp/desktop-native-phase1`, retaining the
verified API-287 progress package's other runtime files. ZIP CRC and updated
entry bytes were checked before deployment. The final script run from `/tmp`
completed successfully and synced both firmware copies with SHA-256:
`14a5aa702b05af5d188074258ce373162b3b4b694645f976e3efc87db6f3519d`.
Both plugins, the original photo-lock file, all 11 databases and enabled
autoupdate were independently verified again. The installed completion build
includes complete music queues, desktop notices and corrected Hold rendering.

## First Phase 2 hardware test

Update 2026-09-08: [Phase 2 specification and handoff](desktop-mode-phase2.md)
splits this original gate into a role-only diagnostic (2A), now implemented,
and hub enumeration (2B), still pending. Use that document for the current
test procedure and preserved build/rollback artifacts.

The first test is **controller role and hub enumeration only**, after a target
backend has been implemented and reviewed. Do not begin with VGA or display
uploads.

1. Record the installed firmware/configuration/database checksums and prove
   ordinary computer USB DEVICE connection before entering the test build.
2. With no computer attached, connect the genuine A1362 CCK and externally
   powered Acer hub. Leave DL-165, keyboard and mouse disconnected initially.
   Verify the planned no-backfeed wiring; the iPod stays battery powered and
   no charging behavior is assumed.
3. Invoke the host test manually, keeping default auto-activation Off. Log
   controller role, port status and reset/completion deadlines in worker
   context. Read only the hub's device/configuration/hub descriptors.
4. Detach the hub and confirm channels drain and DEVICE mode is restored.
   Reconnect the computer and verify the ordinary USB device connection again.
5. Stop on role, reset, enumeration or DEVICE-restoration failure and preserve
   the log. Do not proceed to HID, EDID or DisplayLink mode programming.

Subsequent tests qualify hub-port hotplug, keyboard/mouse reports, EDID, a
640×480 solid-color upload, partial updates, input while updating, detach
recovery, and music/video/composite continuity. Those are future tests, not
requests to connect hardware during Phase 1.
