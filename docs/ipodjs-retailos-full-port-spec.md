# iPodJS iPod35 RetailOS 2.0.4 Resource Port

## Result and scope

The resource archive contains every raster resource and source animation
frame in the iPod35 RetailOS 2.0.4 image. Runtime visual integration is still
in progress; complete extraction does not mean every screen is pixel-identical
to RetailOS. This is not a port of Apple's executable code, controllers,
databases, DRM, codecs, or unsupported hardware features.

The boundary is deliberate:

- every bitmap resource is extracted and retained losslessly;
- every source frame referenced by a named RetailOS animation is retained;
- current iPodJS surfaces that intentionally reproduce RetailOS use the exact
  corresponding resources;
- existing Rockbox/iPodJS behavior, application routing, and feature set stay
  in place; and
- a resource does not create a feature whose hardware or data model is absent.

In particular, the iPod 6G does not gain a fake FM radio merely because the
firmware archive contains radio artwork.  The assets remain complete and
addressable for a target that has an actual radio owner.

## Authoritative source identity

The extractor accepts only these byte identities of iPod35 2.0.4:

| Form | SHA-256 | Resource table offset |
| --- | --- | ---: |
| wrapped decrypted firmware | `f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4` | `0x0040b930` |
| unwrapped body | `7910c276c85a0aa67c4f36fb2d72476791b7d7fe9a21442dae53bff27b48d684` | `0x0040b130` |

Any other input is rejected before extraction.  The source binary and
generated Apple files are private, gitignored build inputs.  Public source
contains the extractor, verifier, runtime loader, and this provenance contract
but no Apple pixel payload.

## Complete resource ledger

`tools/ipod_classic_resource_extract.py` emits one manifest entry, PNG audit
image, and ordinal `RGA1` runtime image for each of the 598 resource records.
The six source formats are all supported and their exact counts are fixed:

| Source format | Resources |
| --- | ---: |
| `0x0004` | 183 |
| `0x0008` | 35 |
| `0x0064` | 263 |
| `0x0065` | 43 |
| `0x0565` | 9 |
| `0x1888` | 65 |
| Total | 598 |

The firmware image registry contains 658 symbol strings.  Runtime-address
translation proves 566 symbol-to-resource associations; 92 strings have no
registry token and are retained explicitly as unregistered rather than being
guessed.  The generated archive also exposes 151 stable semantic component
names used by the current compositor.  A name is an alias to a verified
ordinal, never an independently drawn replacement.

Every ordinal remains runtime-addressable through
`ipodjs_retailos_load_resource_rga()`, including resources that do not yet
belong to a visible iPodJS screen.  This is what makes the resource port
complete without inventing behavior for otherwise unused art.

## Complete animation ledger

The following eleven RetailOS source sequences are mandatory.  Counts are
source frames, not generated transitions or captures.

| Sequence | Native frame | Source frames | Runtime use |
| --- | ---: | ---: | --- |
| white status battery | 26x13 | 25 | header levels 0--22, plug, charge |
| black status battery | 26x13 | 25 | header levels 0--22, plug, charge |
| clock hours | 73x73 | 30 | World Clock/Clock hand source sprites |
| clock minutes | 73x73 | 16 | World Clock/Clock hand source sprites |
| clock seconds | 73x73 | 16 | World Clock/Clock hand source sprites |
| Now Playing idle battery | 72x40 | 8 | low-power playback screen |
| radio scanning | 19x15 | 7 | retained for a radio-capable owner |
| Now Playing equalizer | 130x79 | 22 | no-art playback animation |
| stopwatch minutes | 18x18 | 30 | native Stopwatch |
| stopwatch seconds | 124x124 | 30 | native Stopwatch |
| Disk Mode sync arrows | 76x76 | 18 | USB Disk Mode animation |
| Total | | 227 | |

Each sequence is emitted as an exact RGBA atlas.  Native 4-bit/8-bit sources
also retain a lossless `IAF1` pack with row bytes, format, frame bytes, source
token, and frame count in the header.  Clock transformations use the source
sprites as RetailOS does; transformed presentations are not falsely counted
as additional source frames.

Charging is not omitted from this ledger.  RetailOS stores that surface as ten
composition resources, not as a multi-frame animation record.  Ordinals
588--597 are all retained and are specified separately in
`docs/ipodjs-stock-charging-screen-spec.md`.

## Runtime wiring

The exact resource loader in `apps/gui/ipodjs_retailos.c` validates file
headers, dimensions, source formats, frame counts, frame sizes, and first
tokens before publishing a cache.  It never resizes, interpolates, repairs, or
silently substitutes an Apple resource.

The existing behavior owners use those caches as follows:

| Existing surface | Exact RetailOS resources now used |
| --- | --- |
| shared header/status | white/black backgrounds, lock, play/pause, repeat/repeat-one, shuffle, all 50 battery frames |
| Search and fast scroll | both 74x70 quick-scroll plates, input-field pieces, option-bar selection pieces |
| locked-video, Photos, Comics and Magazines PIN | the same exact Search components; PIN actions and storage are unchanged |
| selected menu submenu arrow | original 10x15 white chevron; colored extension arrows retain their existing rendering |
| Now Playing | status/mode glyphs, progress pieces, option bar and volume icons, pause image, 22 equalizer frames, idle digits/icons and all 8 idle-battery frames |
| Quick Settings | white/black option bars and brightness/volume endpoint icons |
| Clock/World Clock | map, bars, clock faces/caps/shadow, and all clock source frames |
| Stopwatch | face, caps, controls, and all 60 source frames |
| USB Disk Mode | exact static icons/progress parts and all 18 sync-arrow frames |
| charging | all ten source components at ordinals 588--597 |

The WPS file remains the Rockbox playback lifecycle and album-art declaration
owner.  The native iPodJS compositor paints the exact visible chrome after a
skin update.  It does not introduce a second action loop, claim album-art
memory, or reconstruct playback after a menu/plugin return.

### Status-bar reference correction

The menu screenshot on page 8 of Apple's [2009 Classic user guide](https://cdsassets.apple.com/live/6GJYWVAV/user/ma1195_ipod_classic_160gb_user_guide.pdf)
shows a light silver gradient, black title and blue playback glyph. Normal
light menus and white music WPS use `statusbar-white-background` (ordinal 8,
320x20), not `statusbar-black-background` (ordinal 10, 320x24). Paint those
20 rows without stretching; light Apple-themed menu rows begin at y=20.
Dark menus retain their 24px boundary. The application grid explicitly retains
its original 24px boundary, label font, coordinates, icons and focus rendering.
The shared cache owns this one source image; WPS does not keep a duplicate.
Its 19,200 pixel bytes move from the WPS cache to the status cache, leaving
the combined cache payload unchanged. Dark mode and the explicitly protected
Hold presentation retain their previous assets and text/icon selection.
Wi-Fi, Bluetooth, AirPods and custom-app artwork remain unchanged.

The lifecycle WPS uses a background-preserving viewport and a hidden album-art
declaration. It does not clear or draw over the native frame on subsequent
skin refreshes. Lifecycle-only trace records are distinct from actual full
frame and dirty-rectangle paint records.

### Official font faces and resolved WPS layout

`tools/prepare_ipod_classic_fonts.py` accepts the pinned official 2.0.4 IPSW
(`7ef835c74b08f0bda3566001496cb764afbe0600cb1afec1145c259bc34ad7d0`).
It extracts and verifies the actual Helvetica regular and bold faces from
`Resources/Fonts`, then emits private Rockbox fonts and an output hash ledger.
`convttf` must select the source Unicode cmap: falling back to raw glyph
indices incorrectly assigns letters for Apple's font tables.

Classic conversion also opts into `convttf -A`. The legacy converter used ink
width instead of advance width and centered each bitmap rather than retaining
its left bearing, tightening labels such as "iPod". The opt-in path preserves
source advances and bearings without changing legacy conversions used by Hold,
the application grid or branded apps. Converted outlines still use FreeType;
this fixes spacing, not proof of identical Apple glyph rasterization.

The firmware font roles and the guide's pages 8 and 24 establish separate
menu, status, title, detail and time roles. At the converter's 60 dpi, the
15-point real bold face supplies the status/detail/time glyphs; 19-point bold
supplies normal menu rows and the WPS title. Small/large menu settings retain
their behavior, selecting 15/23-point faces. Header titles use x=6, y=4,
foreground-only text blending and no centering. Clock/Stopwatch headers use
the same role. Hold and the application grid keep their previous font paths.
Clock utility rows, charging text and USB text now select the same verified
cache roles instead of loading separate Adobe substitutions.

The Home Extras/Clock preview now calls the same cached RetailOS renderer as
the Clock menu: ordinal 149's face/reflection plus the native cap and complete
hour/minute/second frame sets. Its old procedural circles, ticks, hands and
draw-time background loader are removed. The fixed Clock/Stopwatch union is
prepared before playback, with no new cache allocation. Center 73x73 hand
sprites at offset (29,29) within the 132x132 dial, excluding the reflection.
No replacement clock is drawn when the private resource cache is unavailable.

WPS resolved text top-left y coordinates are title 59, artist 80, album 98,
sequence 134 and time 209; raw firmware view anchors are not directly usable
as Rockbox text coordinates. Secondary text is source color #3d3d3d. The
progress strip uses native blue active pieces 296/297 at x=58, y=207, width
204, including their source reflection. Elapsed text is right-aligned at
x=48 and remaining text starts at x=272.

Three bounded 96-glyph font caches are prepared before playback and locked;
cached font getters neither allocate nor open files. The submenu source cache
adds 450 pixel bytes. The Photos PIN buffers shrink to 19,968 bytes; Comics
and Magazines reuse their existing plugin arenas for the same payload rather
than taking playback memory. No additional fullscreen buffer is introduced.

Glyph outlines and the real bold face are official, but FreeType rasterization
is not Apple's original text rasterizer. The artwork projection is likewise
reference-derived rendering, not a recovered Apple transform. Procedural list
scrollbars/checkmarks and source-defined gradient matching remain to be
audited; Classic video overlays still have older official Apple resources.
These are explicit parity gaps, not grounds to restyle custom app screens.

### Validation of the menu/status follow-up (2026-09-07)

Both simulator and full native builds pass. The full navigation gate records
4,378 trace entries, ten full-depth Music cycles and twenty rapid
Albums/Artists switches with the same track and playlist. File descriptors
remain at 22 for baseline and all ten cycles (three additional open font
faces, not a per-navigation leak). Simulator core counters are unavailable
(zero), so native memory-pressure testing is still required.

The focused source/provenance suite passes 31 tests, including font corruption
and omission rejection. Hold header crops have zero differing pixels. WPS
captures are taken after the existing track notification finishes its exit
animation, avoiding an intermediate toast as a false header reference.

Native text/data/BSS are 2,863,916 / 10,824 / 8,665,764 bytes for this build.
Compared with the preceding header build, BSS grows by 480 bytes. The three
font glyph buffers total 61,056 bytes plus bounded font/path bookkeeping,
allocated before playback. ARM header stack use is 72 bytes including saved
registers; utility header use is 32 bytes and font preparation 24 bytes.
No full-screen workspace or audio-buffer allocation was added.

Deployment follow-up: the new package built successfully, but the physical
deploy stopped at recovery-snapshot comparison. The volume subsequently read
as read-only. The root firmware remains the earlier `dfaf4bb7...` build while
`.rockbox/rockbox.ipod` matches the new `c41f109a...` build; this is NOT a valid
completed deploy. All 2,929 indexed track paths remain readable. A host copy
of the readable live database, recovery snapshot and settings is retained at
`/tmp/ipodjs-readonly-device-snapshot.gLmQBA`. The live index differs from the
older recovery index, so do not restore it blindly. No repair, remount or eject
was attempted; filesystem inspection/repair needs explicit authorization
before retrying the required database-preserving deployment.

### Charging/font/clock follow-up validation (2026-09-08)

The large clock hand canvas now begins at face offset `(29, 29)`, correcting
the previous 14-pixel upward displacement. A fresh simulator capture at
`/tmp/ipodjs-clock-centered-verified/corrected-extras-clock.png` confirms the
shared pivot is centered in the dial, not the reflection-inclusive canvas.
No hand frames, clock timing, input handling or playback behavior changed.

The source battery casing/overlay charging gate passes in both display modes.
The focused source/provenance suite passes 36 tests, and simulator and native
builds pass. Native text/data/BSS are 2,862,556 / 10,820 / 8,665,764 bytes:
no BSS growth relative to the previous validation. The clock preview function
saves 32 bytes of ARM registers and has no additional local stack reservation.
Font glyph buffers now total 63,360 bytes, an increase of 2,304 bytes from
source-advance preservation; preparation remains before playback.

The current full navigation gate is NOT passing. One attempt used a fixture
order inconsistent with the gate's new Cover Flow-first assumption; a second
attempt with the default order stopped at the artist/album list transition
before playback. Neither supplies the required ten full-depth and twenty
rapid-switch cycles for this build. The earlier successful gate is historical,
not validation of these changes. Hardware validation/deployment remains
blocked, and no additional device writes or filesystem repairs were made.

### USB connection-screen follow-up (2026-09-08)

The user confirmed the reported album-selection failure cleared after a
reboot. No database recovery or playback change was made for that report.

The USB screen now uses source backdrop ordinal 11, gold badge ordinal 392,
the source dark status strip, source battery/lock artwork and all eighteen
76x76 SyncArrow masks in black (`DiskMode_Arrows_Color = 0x00000000`).
The former blue/teal gradients, procedural badge, drawn battery/lock and
handmade arrow fallback are removed from this surface. Dark-mode preference
does not recolor this Apple-replica screen.

The symbol registry resolves `DiskModeImage_SyncIcon` to ordinal 392, not
562. The extractor and independent verifier now agree on that correction;
562 remains the distinct connector mask. The private extraction was
regenerated from the pinned firmware and independently reverified.

Apple's Classic guide page 10 and the `DiskMode_View` source records guide
the native-size placement: badge at (104,46), arrow canvas at (122,62),
centered text below. The source regular Helvetica 16-point export is added
for the small USB label, alongside the existing 19-point bold title face.
All labels are warmed before font descriptors close for USB. These are source
outlines rendered by Rockbox/FreeType, not a claim of Apple rasterizer parity.

The existing USB-owned two-frame transition workspace is reused. Preparation
cancels the pending decorative transition, validates the opaque backdrop,
converts it in-place to native pixels (reverse order for 32-bit simulator
pixels), and loads the badge into the second frame. First paint snapshots
the completed static screen into the first frame. Subsequent paints restore
only the 76x76 arrow rectangle and composite the cached source frame. Every
USB re-entry reloads the workspace because menu transitions may have reused it.
Native BSS stays at 8,665,764 bytes; no full-screen or playback allocation is
added. One bounded 96-glyph regular font cache (15,744 glyph-buffer bytes plus
font/path bookkeeping and one retained font descriptor) is prepared before
playback. ARM USB preparation uses 56 stack bytes including saved registers;
the USB draw function uses 48 bytes including saved registers.

The existing 12-fps elapsed-tick animation and Connected/eject wording remain
the Rockbox presentation contract requested here. Apple's native idle
Connected view shows a connector; its Synchronizing view animates arrows.
This visual change does not invent an iTunes-sync state, alter USB events,
host handshakes, disconnect handling, playlist restoration or Hold behavior.
Missing assets produce a plain unbranded USB text screen, not a drawn replica.

Both native and simulator builds and forty focused tests pass. The focused
USB gate is `tools/ipodjs_usb_retail_sim_regression.sh`; it checks all eighteen
source frame indices, fixed damage bounds and repeated connections in both
display modes. It passes at `/tmp/ipodjs-usb-final-gate`; the missing-badge
fixture also visibly confirms the plain unbranded fallback. The broader
navigation gate still encounters desktop input
delivery failures; no new hardware deployment is claimed for this follow-up.

## Preservation contract

This port changes resource provenance and pixels, not product behavior.

- Do not change the Hold screen, its layout, entry/exit behavior, dimming, or
  input handling.
- Do not remove, rename, reorder, or restyle custom applications merely to
  resemble RetailOS.  Netflix remains Netflix; Steam and every other added app
  remain first-class additions to the Classic menu.
- Keep the existing Wi-Fi, Bluetooth, and AirPods artwork and behavior.  They
  are extension features and are not replaced by unrelated RetailOS symbols.
- Keep H.264, composite output, video routing, playback controls, resume data,
  and codecs unchanged.
- Keep click-wheel mappings, Menu history, Select/long-Select semantics,
  charging/USB precedence, timers, alarms, notifications, settings, and
  database behavior unchanged unless a separate behavior change is requested.
- Do not add a fake app or screen solely because an unused Apple resource is
  present.

Legacy official Apple assets may remain available to extension code that has
no exact iPod35 counterpart.  Such a surface must be described by its actual
provenance, not as an exact RetailOS 2.0.4 element.  No generated, traced,
recolored, or resized substitute may enter the RetailOS namespace.

## Memory and lifecycle contract

Resource preparation happens at bounded screen-entry/service points.  Every
draw and animation tick consumes only fixed cache memory:

- no file or directory I/O;
- no bitmap decode;
- no `core_alloc()` or movable buffer;
- no shared playback-buffer claim, shrink, reset, or restart;
- no playlist, PCM, mixer, codec, or tagcache mutation; and
- no unbounded frame cache.

Clock and Stopwatch share a fixed workspace because they cannot own the LCD
simultaneously.  Status, controls, charging, WPS, idle, and USB use bounded
target-scoped caches.  Animation selection is derived from elapsed ticks so a
dropped paint cannot alter state or timing.

## Fail-closed packaging

`tools/verify_ipod_classic_resource_dump.py` checks the complete resource
manifest, all 598 ordinal RGAs, named aliases, symbol map, all eleven animation
manifests, all 227 frame records, and every runtime atlas/raw pack.  It also
rejects path escapes, wrong dimensions, count drift, invalid headers, changed
hashes, missing files, and unexpected ledger members.

`tools/buildzip.pl` runs this verifier whenever the private RetailOS directory
is present.  A partial or corrupt directory aborts packaging rather than
shipping a surface that claims completeness.  If no private archive is
installed, iPodJS may use an explicitly unbranded Rockbox fallback; it must not
manufacture Apple-looking assets.

When the private `retailos-fonts` directory is present, packaging also runs
`prepare_ipod_classic_fonts.py --verify`: all six output roles, source face
identities, sizes, hashes and Unicode ranges must pass before copying fonts.

## Settings/menu reference pass (2026-09-08, earlier deployed pass)

This pass is **not acceptance of stock-identical Settings/About**. The user
rejected the draft Settings appearance. Exact asset provenance and complete
extraction are insufficient proof that a composed screen matches RetailOS.
At that checkpoint, About pages and dialog/button families were unwired; the
Settings hierarchy intentionally retains all existing Rockbox options.

Verified source wiring in this pass:

| Component | Source / native geometry | Placement / role |
| --- | --- | --- |
| checkmark | 448/449, 14x12 | black normal / white selected; no drawn substitute |
| scrollbar | 0/1/2, 6x10 / 6x8 / 6x9 | native caps and tiled middle, 6px wide |
| Main Menu panel | 429, 78x121 | (202,81), including original reflection |
| Main Menu overlay | 435, 78x121, 4-bit mask | same position, source #eeeeee tint |
| About preview logo | 427, 119x119, 8-bit mask | (180,45); final anchor parity still needs confirmation |
| simple capacity | 401–406, native 13px high | source rectangle (179,173), width 130 |
| preview backdrop | right 160x240 crop of 11 | original colours, no replacement gradient |

`SettingsInfo_About_Template` assigns `System_Font` to the free-space label
(word offset 516), not `StatusBar_Font`. The corrected preview uses 19pt bold
there. The title is `iPod`; the left header now respects `Settings` instead of
the split renderer's hard-coded `iPod`. Generic list full paints restore the
19pt System font after the 15pt status title, matching dirty-row paints.
Non-stock row separators were removed from the Apple list renderer.

Visual reference: Apple's 2009 iPod classic guide, printed pp. 8–9, plus the
front-on stock Settings photograph at
<https://i5.walmartimages.ca/asr/d646a3e3-5e3f-4162-aa97-e32914468966.e418c70fa77c7d7256f93f6b836a46e1.jpeg>.
The photograph confirms the split header, menu font and free-space label
scale. It is not a calibrated colour reference. Background treatment and
every unresolved anchor must be checked before calling the port identical.

The private font package now includes 15pt regular for source small/plain
roles, in addition to 13/16 regular and 15/19/23 bold. Quick-scroll retains
Apple's embedded 23px bitmap strike. Outline roles still use FreeType, not
Apple's original glyph rasterizer; identical source outlines do not establish
pixel-identical text. Hold/grid/branded application fonts are not replaced.

Memory: the fixed menu cache is 167,896 native bytes, including one half-screen
crop, not an additional full-screen compositor. Native text/data/BSS changes
from 2,862,280 / 10,824 / 8,665,764 to 2,863,812 / 10,828 / 8,828,068 bytes
(net BSS +162,304, after removing the superseded preview cache). ARM local
frames including saved registers are 64 bytes for menu preparation, 56 for
preview drawing and 320 for the crop loader. All preparation precedes audio;
draw functions only access cached pixels. Playback buffers are not resized.

The simulator-only gated wheel fixture now supplies a real one-detent event;
zero-data wheel events were ignored by the generic list engine. Native button
handling is unchanged. `tools/ipodjs_menu_assets_sim_regression.sh` runs the
existing navigation gate with headless file-gated input and a disposable
runtime, without changing desktop focus or device configuration.

Both builds and 32 focused source/provenance checks pass. The corrected light
and dark captures are in `/tmp/ipodjs-menu-focused-v4` and
`/tmp/ipodjs-menu-focused-dark-v4`. The first full navigation run retained all
frames but failed the timing bound on one 42-tick transition (maximum 35).
The isolated rerun's trace validation passes: 3,994 records, ten full-depth
cycles with file descriptors fixed at 18, twenty rapid Albums/Artists cycles,
stable playback identity and no core-arena regression. Evidence is in
`/tmp/ipodjs-menu-full-v4-recheck`. That build was subsequently deployed to both
firmware paths with matching checksums and all eleven database files preserved.
The following implementation is newer and has not been deployed.

## Settings, About, dialogs and text completion (2026-09-08)

Only Apple-themed controls are replaced. Settings retains every prior option
and adds About first. H.264/composite, music actions, codecs, tagtree/database
ownership, app order, branding, Wi-Fi/Bluetooth and Hold rendering are unchanged.
The user additionally requested the Applications header use the Apple status
font, removal of its four-pixel background seam, and readable overflowing grid
labels. Those changes keep the grid's original icon coordinates and label font.

### Source composition

The pinned firmware layout audit supplies About's band (393 at 0,61), logo
(425 at 128,40), native capacity pieces (408–422), category edge (407), legends
(397–400), and selected/unselected dots (423/424). Capacity endcaps round only
the outside ends; category joins use the extracted edge. Label/value roles use
the extracted 15pt bold/plain faces, while menu rows retain 19pt System_Font.
Identity values use source x=192, not the earlier draft's x=174. Data too long
for its source rectangle moves horizontally at the original face size; it is
not replaced with an ellipsis or rendered in a smaller font.

The light status image is exactly 20px tall, whereas dark/Hold images are 24px.
Header clearing now respects that height. The grid's existing background is
extended into only the exposed four rows, without moving icons or labels.
Applications uses the same 15pt Apple status title as other menus. Hold keeps
its existing face, coordinates and 24px header.

Yes/no presentation uses extracted overlay 83/85/84 and normal/active button
parts 89–94 at the TwoButtonDialog source positions: panel (18,168), normal pill
(91,190), active choices (92/152,192). The Reset/Cancel layouts put the
5px right caps at x=144/220, so the active widths are 57/73px, not equal
59px spans. This covers the complete No region with the original inset.
Normal/active heights remain 26/22px. The
user-requested removal of both “Any Other = No” and “Select = Yes” accompanies
real wheel selection: counter-clockwise highlights Yes, clockwise highlights
No, and Select confirms the highlighted choice. Blocking prompts start on No;
timed prompts retain the caller's initial default and timeout outcome. Menu
still cancels and USB handling is unchanged. This interaction applies only
when the Apple two-button presentation succeeds. Long questions fall back to
the ordinary scrolling confirmation, at the Apple detail font; they are never
silently shortened. Result notices use the source overlay and 15pt detail face.
Plugin-owned and Hold dialogs retain their prior presentation.

### Rockbox storage, not a retail iTunes model

The user requested a deliberate extension of stock About: the bar now shows
**Music / Apps / Other**, with an additional About page for app storage. Center cycles
capacity, app storage, counts and identity. On app storage, wheel events page
through four rows at a time. Names use the Apple bold detail face, values use
the plain face, and the original capacity/header/dot/scrollbar assets remain.
Long values/labels scroll; no smaller substitute font is used.

`tools/ipodjs_about_inventory.py` reads Rockbox tagcache and app catalogs and
measures allocated file bytes, including FAT cluster allocation. Each app's
binary, private assets, downloaded media and data are combined. Music includes
its files, covers, lyrics and album-list cache. Hard links are counted once;
symlinks are not followed and index paths cannot escape the volume. Unreadable
or oversized inventories fail rather than publishing a partial-looking chart.
OnlyFans paths and its catalog-referenced media are explicitly assigned to
Other, including media stored under Videos. No OnlyFans app-storage row is
emitted. Shared firmware, system assets, filesystem overhead and unidentified
files also belong to Other. App rows are sorted by allocated space descending.

The bounded `ipodjs-about-v2` snapshot stores Music/Apps KiB, counts, real volume
geometry and at most 256 named apps. The player verifies that individual app
sizes sum to the Apps total. It computes Other from live volume-used space less
Music and Apps. It reads at most 272 lines once on entering About, never scans
directories or starts a tagcache search in paint/animation code. Native Song
count comes from ready tagcache RAM state; missing identity is “Unavailable”,
not a fabricated Apple serial/version. Sub-GB values use MB/KB as appropriate.
Simulator directories do not borrow the host disk's capacity: a real-device
snapshot is supplied for device-data captures, otherwise capacity is unavailable.

The deploy helper refreshes this snapshot after package/app installation, before
sync. Refresh it again after a later media sync; it is not a live category scan.
Native free/used volume values remain live. This turn only read the physical
device; it did not write firmware, config, databases or inventory to the iPod.

Read-only device audit: 2,929 indexed songs, 226 app storage entries, Music
118,732,784 KiB and Apps 255,490,592 KiB. Each per-app entry sums exactly to the
Apps total. Other is 10,411,744 KiB at the audited used/free state. These are
actual Rockbox contents, not stock sample values.

### Cadence, memory and verification

Horizontal menu transitions and preview fades derive position from elapsed
ticks. They retain eight scheduled samples over 250ms, but once a late sample
reaches the destination they finish immediately instead of repainting identical
end frames. The trace gate accepts only an ordered prefix ending in sample 7,
with exact endpoints, monotonic positions and the original timing bound.
Already-queued input bypasses decorative handoff without consuming events or
rewriting navigation history. Vertical custom effects and the 227 extracted
animation frames remain unchanged.

About/grid overflowing labels use bounded, manually serviced text strips, with
cached background restoration only. Selected native Settings/list labels use
the existing scroll scheduler, a static viewport and explicit teardown before
screen handoff. Hold and queued input suppress optional text painting.

Native text/data/BSS: 2,874,612 / 10,828 / 8,976,516 bytes, versus this turn's
2,863,812 / 10,828 / 8,828,068 baseline: BSS +148,448 bytes. The full opaque
background replaces the prior half-screen RGA cache; it is not an additional
compositor framebuffer. Fixed About app rows cost 22,528 bytes; source-control
cache costs 84,544 bytes. No core allocation or playback-buffer claim is added.
ARM local frames including saved registers: About loop 472 bytes, snapshot
248 bytes, grid label painter 64 bytes, opaque loader 400 bytes. App rows are
static, not a large main-thread stack object.

Both target and simulator builds and 50 source/inventory/provenance tests pass.
The first two loaded simulator stress runs exposed late presentation samples;
they are not claimed as timing passes. The isolated navigation run passed its
trace validation: 3,980 records, ten full-depth Music cycles, twenty rapid
Albums/Artists cycles, stable 18 process FDs, stable core-memory states, and
unchanged playing track/playlist identity with non-regressing elapsed time.
Evidence: `/tmp/ipodjs-settings-navigation-isolated/ipodjs-trace.tsv` and
`fd-stress.tsv`; the trace validator also passed independently. The outer shell
wrapper subsequently returned 127 because its source was edited during the
run; the completed navigation gate and saved trace passed before that error.
This run includes wheel-based confirmation; the subsequent button-width-only
correction is covered by the focused visual gate rather than another full
navigation run. Full stock pixel identity remains
unproven: the fonts retain genuine Apple outlines/point sizes but use FreeType,
and the transition easing is a bounded port, not Apple's recovered timing code.

Final light/dark focused captures pass in
`/tmp/ipodjs-settings-button-geometry` and `/tmp/ipodjs-settings-button-dark`.
Both selected states use the corrected original cap positions, and selecting
No produces “Settings Cancelled”. The grid's intermediate scroll capture
reveals the end of “Spotify Wrapped”; its title uses the matching Apple status
font. Source-scoped whitespace checks, 50 tests, both builds and package
creation pass. Firmware SHA-256:
`c19976b0cfe09e4feecbe2bbe400f73816daebcafde2ec9fc10edff7a683dcae`.
The iPod has not been updated or unmounted by this work.

## Extras, album scrolling and Desktop RuneScape (2026-09-08)

The user's final scope explicitly excludes **all video overlay changes**, not
only branded applications. The experimental shared Classic video renderer and
its core/Raw/MPEG integrations were removed before packaging. Existing video
controls, Netflix/Twitch/YouTube overlays and playback lifecycles stay intact.

- About moves from Settings to Extras, before System. Its existing capacity,
  per-application storage, counts and identity pages are retained. Applications
  stays at Extras index 3; neither its grid nor installed app order changes.
- Long WPS album text scrolls in its original Apple detail font, at y=98.
  Short labels retain their previous placement. A fixed 320-byte native state
  holds the viewport, bounded metadata copy and elapsed-tick scroll position;
  it is independent of the track-title scroller and disabled on WPS exit.
  Draws use cached metadata, yield to pending input/Hold, and never allocate
  playback memory or change the playlist.
- Desktop Steam keeps NiBiRu and adds the installed RuneScape Classic plugin
  with its existing cover. Only this desktop launch passes `-desktop`.
  RuneScape uses Desktop Mode's default click-wheel pointer conventions and
  simulator host-pointer records; Select is left click and Play is right
  click. Normal non-desktop game controls and Hold remain unchanged. The
  desktop pointer does not import personalized pointer speed/reverse settings.

Focused Extras/About and Settings captures are in
`/tmp/ipodjs-extras-about-final`. Native text/data/BSS after removing the video
work is `2875264 / 10828 / 8976836`, versus the prior deployed
`2874612 / 10828 / 8976516`: +652 text, +320 BSS, no data growth or new
full-screen buffer. Both targets build and 54 focused source/input tests pass.
Deployment must not force the currently read-only iPod filesystem writable or
unmount it; the database-preserving deploy guard remains mandatory.

## Time/battery screensaver quick toggle (2026-09-08)

Quick Settings ends with **Time/Battery**, showing **On** or **Off**. Select
toggles it; leaving Quick Settings saves it through Rockbox's existing settings
writer. The persistent key is `ui engine playback screensaver: on/off`, with
On as the default to preserve existing behavior. The field is appended to
`user_settings` so existing plugin field offsets do not move.

Off makes the existing idle-WPS renderer return without painting, leaving the
normal WPS in place. It does not change backlight timing, the Hold screen,
charging/connection screens, audio, playlists or any video overlay. On retains
the original eligibility conditions and all original artwork/animation frames.
No assets, framebuffer or playback-memory allocations were added. Native
text/data/BSS is `2875440 / 10844 / 8977508` (+176/+16/+672 bytes over the
preceding build, including the fixed Quick Settings row-cache slot).

Validation: both builds and 56 focused tests pass. The simulator proves On/Off
and persistence across restart (`/tmp/ipodjs-screensaver-toggle`). The full
10-cycle/20-rapid-switch playback navigation gate passes at
`/tmp/ipodjs-screensaver-navigation`. This toggle has not been deployed.

## Cover Flow artwork and WPS rating pages (2026-09-08)

The screensaver toggle was deployed first as requested, with firmware SHA-256
`6f865bf9b54443f3d03d2f4d967fa0d97357ef4242b38853b56fd6536cc4be1f` in both
device locations and all eleven database files preserved. The device remained
mounted. The following WPS changes are a subsequent build, not that deployment.

Short Select now cycles artwork → rating → source equalizer → artwork inside
the normal WPS lifecycle. Long Select still opens Lyrics. Menu history, Hold,
the idle screensaver setting, app grid and all video overlays are unchanged.
New WPS sessions and track changes start on the artwork page.

The rating editor uses `NowPlaying_Large_Blue_Star_Image` (ordinal 301) and
`NowPlaying_Dot_Image` (302), both 22×26, in the extracted
`NowPlaying_Rating_Template` 130×26 strip at (95,200). Five 26px cells center
the original images at x=97+26i. The normal metadata rating still uses the
13×13 small grey star (300). These are distinct source resources, not white
stars repurposed as empty placeholders. This follows the five-bullet,
click-wheel rating interaction described on page 26 of
[Apple's Classic guide](https://cdsassets.apple.com/live/6GJYWVAV/user/ma1195_ipod_classic_160gb_user_guide.pdf).
The overall three-page order follows the user's explicit request; it is not
a claim that the other stock Genius/shuffle/scrubber pages are implemented.

Wheel input in rating mode selects 0–5 whole stars (0 clears), clamps at both
ends, and does not change volume. Rockbox's numeric 0–10 field stores twice
the displayed star count. Updates use the existing tagcache command queue;
unindexed tracks report that the database is needed instead of pretending to
save. A missing runtime index, or a stored rating when runtime gathering is
disabled, is resolved only on explicit Select, never in draw callbacks.
No iTunes synchronization was added.

The artwork reader previously rejected any bitmap that was not exactly
128×128. Playback actually uses `FORMAT_KEEP_ASPECT`, so rectangular covers
can legitimately be 128×96 or 86×128. It now accepts valid positive dimensions
within that box and chooses the largest existing buffered cover. No new art
slot, decode, search or audio-buffer restart is used. The square-cover Cover
Flow path was reproduced successfully before this change; the user's exact
failing album has not yet been identified.

Native text/data/BSS: `2876448 / 10844 / 8980964`, a delta of +1008/0/+3456
from the screensaver build. The two editor images use 3,432 fixed pixel bytes;
there is no additional framebuffer or playback allocation. The native Select
handler uses 984 bytes of local stack plus 16 saved-register bytes; database
lookup is outside the rendering/animation call chain.
Both builds and 60 focused tests pass, including compiled C rectangular-art
selection and rating-boundary tests.
The focused simulator run verifies clearing, five-star clamping, three-star
editing, reopening the editor with runtime gathering off and autoresume on,
and the saved database value of 6. Screenshots and trace are in
`/tmp/ipodjs-wps-pages-persistence`. The navigation stress test passed ten
full hierarchy loops and twenty rapid album/artist loops with playback and
the playlist intact (`/tmp/ipodjs-wps-pages-navigation`). These WPS changes
have not yet been deployed or verified on physical hardware.

### Physical Select remap correction (2026-09-08)

The WPS build was deployed with SHA-256
`6619652b96d56148cc58af6fc118311f0b015dbe1e4cd6b92835916ca1b435a1`
matching both firmware locations. Hardware then exposed a fixture gap: the
device's saved `/.rockbox/keyremap.kmf` overrides WPS short Select with
`ACTION_WPS_VIEW_PLAYLIST`, which iPodJS intentionally blocks. The original
simulator fixture did not contain that saved remap.

`tools/ipodjs_fix_wps_keymap.py` validates the current iPod 6G keymap ABI and
changes only that Select action to `ACTION_WPS_BROWSE`. On this device exactly
one byte changed (offset 300, 33 to 18); all other mappings are identical.
The corrected file passed the full focused rating/page simulator journey via
`IPODJS_NAVIGATION_KEYMAP`, including saved rating 6 and page sequence
1/2/0/1/2/0. Seven repair tests and four WPS tests passed. This configuration
correction changes no firmware code, playback memory, UI assets or overlays.

The corrected keymap was installed and verified with SHA-256
`31b1a13e27b22d93cf04c2ffba31ef59f093f4e633540cd19a64790f64948871`.
The previous file is retained beside it as
`keyremap.kmf.before-wps-select-20260908`. All eleven database checksums and
both firmware copies remained unchanged; sync completed without unmounting.
The keymap loads at startup, so the device needs another reboot to activate
this correction. Physical button confirmation remains with the user.

The user subsequently confirmed a restart and reported that Select still did
nothing. The keymap repair is therefore not a confirmed hardware fix. Missing
startup entries in the playback log do not override the user's observation.
The next build provides opt-in `ipodjs-wps-input.enable` diagnostics: 64 fixed
records capture mapped input, raw button, selected page and paint outcome.
No draw callback writes files. Leaving WPS writes `ipodjs-wps-input.log` with
the unique `WPS-INPUT-20260908-1` identifier. It changes neither input handling
nor playback behavior. Native text/data/BSS are 2877280/10844/8982276,
an increase of 832/0/1312 bytes over the deployed WPS build. A physical trace
is still required to distinguish input filtering, mapping and render reset.
Both builds and 68 focused tests passed. The diagnostic log distinguished
action 33 (blocked, old remap) from action 18 (page cycle, corrected remap)
in simulator runs. Headless navigation passed ten full-depth cycles and
twenty rapid switches, with 18 descriptors, unchanged playlist (1,13), and
3,874 trace records (`/tmp/ipodjs-wps-input-stress`). The windowed attempt
failed to reach Home and its stuck process was stopped; it is not passing
evidence. Headless execution uses the existing gated button input method.

The diagnostic firmware was installed to both device locations with SHA-256
`1b9dc3d7f1ca7df415b747c7f66e7736f0d632e793876c83921473bcdf5ed4f9`.
The enable marker was installed, all eleven database files verified unchanged,
and sync completed with the volume mounted. The previous firmware is retained
at `/tmp/ipodjs-wps-input-deploy/previous-rockbox.ipod`. No hardware fix is
claimed: the next test is three short Select presses, Menu to save the trace,
then reconnect and inspect `/.rockbox/ipodjs-wps-input.log`.

## Acceptance criteria

The port is complete when:

1. extraction and independent verification report 598/598 resources,
   11/11 sequences, and 227/227 source frames;
2. fresh extraction from either pinned source form is deterministic;
3. corruption and omission tests fail closed;
4. all current Apple-replica engine surfaces listed above use only exact
   iPod35 resources;
5. the Hold implementation, custom app list/branding, Wi-Fi/Bluetooth/AirPods,
   H.264, composite output, and all interaction behavior remain intact;
6. simulator and target builds, packaging, WPS parsing, and focused lifecycle
   regressions pass; and
7. no physical device is modified without separate user authorization.
