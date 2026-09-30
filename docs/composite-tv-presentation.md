# Composite presentation investigation and candidate contract

Baseline: local personal source at 04984f554e, including pre-existing uncommitted
changes, copied into an isolated source snapshot. Original checkout is untouched.

## Cause established by source inspection (before correction)

The production S5L8702 `ipod6g_videoout_show_framebuffer` unconditionally passes
(36,24) 648x432 to `svid_show_planar`. The viewport is documented as qualified
on the Philips DCP750. The input is a 320x240 square-pixel UI (or its optional
2x interpolation). There is no TV-screen preference and no output sample-aspect
calculation. The SDO emits the same interlaced NTSC signal for every accessory.
Accessory policy gates activation; it does not actually select a widescreen
register configuration. The 640x480 constant also used in this file describes
scratch storage, not the 720x480 destination coordinate system.

648/432 is 3:2 in samples, not the physical picture aspect. A television
interpreting the full 720x480 signal as 4:3 gives this centered rectangle 4:3
physical proportions; a television interpreting it as 16:9 stretches the
same rectangle to 16:9. Thus the existing implementation has a fixed full-TV
mapping and cannot preserve a 4:3 UI on a widescreen display. The old dock
qualification established visibility, color and height, not calibrated aspect.

This establishes a missing presentation policy, not the precise cause of any
individual CRT photograph. The TV's own zoom/stretch setting and its effective
active-picture width remain physical-test inputs. This candidate assumes the
existing full-frame 720x480 coordinates represent the physical TV aspect,
equivalent to sample aspects 8:9 (4:3) and 32:27 (16:9). Some receivers use a
704-sample clean aperture. The diagnostic must establish whether this receiver
needs a calibrated aperture before declaring geometrical accuracy.

No NTSC encoder timing, decoder registers, decoder allocation, MP4 parser,
codec clock, A/V synchronization, seek or frame cadence is changed.

Overscan per edge: Off 0%, Small 3%, Medium 5%, Large 8%. Insets affect UI and
controls; movie fit/crop is independent. All rectangles align to even samples.
The same TV preference is used regardless of accessory identity.

## Settings and operation

Open Settings > General Settings > Display > Composite Video in the ordinary
Rockbox menu. In the custom iPone Home, open Settings > General > Display >
Composite Video. All TV preferences are together there, including the test
screen and iAP Remote Debug. The existing LCD-off-during-video option remains
available. New configurations default to Standard 4:3, Fit Off, Overscan Off,
Mirror Current UI, Large text, Follow Current WPS, Playback Only, Remote Wake On.
Existing Off/Auto/On policy and electrical accessory checks are retained. On
cannot bypass those checks. The encoder does not signal the TV to change shape:
set the television's own aspect mode to match, and turn off automatic zoom.

| Configuration key | Values |
| --- | --- |
| composite video output | off, auto, on |
| tv screen | standard, widescreen |
| tv fit | off, on |
| tv overscan | off, small, medium, large |
| tv interface | mirror, safe |
| tv text size | standard, large, extra large |
| tv now playing | current, cover art |
| dock remote mode | playback, navigation |
| remote wake | off, on |

Settings use the normal persistence machinery. Changing aspect changes the
presentation rectangle/scaling without restarting the output hardware or music.
Fit affects full-screen video, not lists. TV-safe text uses the built-in GPL
Rockbox font at measured integer scales, with no required new font file and no
change to the saved handheld font. Existing compatible one-bit UI font glyphs
can supply Unicode; large anti-aliased CJK fonts cannot be reused by this small
renderer and fall back to the built-in missing-glyph behavior.

The separate 320x240 / 426x240 canvases reflow Home, Music, native database,
Settings, standard lists, file browser, confirmations/splashes, and cover-art
WPS. Native database windows retain their original selection and data ownership.
Custom screens without a semantic hook (for example games, notification panels,
advanced app-specific views) retain a proportion-preserving mirror. They do not
yet acquire a TV-sized font automatically. WPS title/artist/album scroll when
long. Its time/status line is independent of the handheld WPS.

Artwork is resolved from the existing resolver at the required output size,
fits without distortion inside a square region, and is cached for the current
track. A one-second idle/input-empty settle interval precedes one decode attempt.
No artwork I/O occurs in redraw. A path change clears validity before decode and
is checked again afterward. This is synchronous bounded artwork service on the
existing WPS loop, not an asynchronous decoder: once a decode starts it cannot
be interrupted, but an obsolete result cannot become visible. Unusable/busy
Music tagcache defers artwork. Missing/corrupt art leaves the music-note
placeholder and metadata. No playback/playlist/buflib-shrink API is called.

Workspace is statically bounded: 204,480-byte UI canvas, 98,304-byte current-art
scratch/cache, 27,264-byte video band, a small sampler lookup and 64-entry trace
plus snapshot. These BSS reservations reduce free firmware RAM at boot and
remain reserved when disconnected; no TV heap allocations or font handles can
leak. Disconnect/leave invalidates cached ownership, and the hardware driver
releases its existing output resources. This is a limitation compared with
returning all TV-only memory to the general allocator on disconnect.

## Video boundary

H.264 changes are restricted to `video_input` remote-to-existing-action mapping
and staging the original Y/Cb/Cr frame at `video_draw_frame`'s existing LCD blit.
No decoder implementation, VPP programming, compressed parser, allocations,
clocking, seek, frame scheduling, DMA or A/V synchronization was changed.
H.264 has only width/height here, so non-square source sample aspect is not
inferred. Anamorphic MP4 that needs unavailable aspect metadata remains limited.
MPEG uses already-available sequence display/pixel-aspect values. The existing
LCD video and overlays still run. Composite consumes the staged raw frame once;
later LCD overlay bands cannot overwrite the TV frame during that call. End of
presentation releases the source pointers immediately. There is no extra field
handoff. Per-column sample maps avoid division for every video pixel.

Composite video uses a generic safe pause/time/progress band. Rich app-specific
chat, interactive caption, and transition graphics remain on the handheld LCD;
they are not reflowed on TV in this candidate. Embedded Live TV picture-in-guide,
PIN and hidden weather panels retain the old mirrored path. TV fit is intended
for full-screen video. Scaling is nearest-sample in the existing bounded scanout
workspace; this is an aspect/presentation candidate, not a quality enhancement.
The delivered build uses the native planar path, not VIDEOOUT_ENHANCED_TEST.

## Remote protocol and diagnostics

Reference reviewed: Rockpod commit
[951d17c0575fbb61f3c585b5f0c3a0a941dcb10f](https://github.com/nuxcodes/rockpod/commit/951d17c0575fbb61f3c585b5f0c3a0a941dcb10f).
The adopted mechanisms are IDPS transaction offsets, independent bitmap bytes,
bounded packet reads, explicit short releases, and press/release delivery
barriers. No wholesale merge. Physical Universal Dock packets have NOT been
observed. Navigation depends on the dock forwarding distinguishable buttons;
software cannot recover Menu/Center when the dock does not send them, or split
Center/Play if they share one packet.

Playback Only preserves the playback keymap. Full UI maps either arrow or volume
Up/Down to list movement, Left/Menu to back, Right/Center to enter; supported
holds request context/Main Menu. Play remains a playback toggle, never Enter.
WPS retains volume, previous/next, seek, Select and Menu meanings. Kokkia retains
its existing playback keymap. Plugins using the action API inherit navigation;
plugins reading raw buttons need their own support. Existing MPEG remote mapping
is retained; MP4 input gains aliases into its existing controls.

Remote Wake bypasses only display first-key filtering and does not reinject
buttons or bypass Hold/softlock. On a visible composite display the existing
LCD-off policy remains authoritative. No shutdown power-on is implemented.

Open Composite Video > iAP Remote Debug to enable tracing (default is disabled
after boot). This screen uses LOCAL iPod controls so incoming remote presses can
be observed without leaving it:

- Local Right: clear the 64-event RAM ring.
- Local Left: toggle capture.
- Local Center release: export `/.rockbox/iap-remote-trace.txt`.
- Local Menu: leave; tracing continues until toggled off or rebooted.

For contextual action tests, enable tracing, leave to the test menu/WPS, perform
one short test, then reopen debug and export. Each export overwrites the previous
file: copy/rename it on the computer before the next capture. Frequent holds can
wrap 64 entries quickly; use one button/gesture per capture. Connect USB only
after export. The trace includes device ID, lingoes, IDPS/authentication state,
mode, wake policy, ticks, raw first 8 bytes (complete supported remote packets),
bitmap, BUTTON bits, context and ACTION numeric values.

States: P changed press, R release, H same held bitmap, M malformed, Q startup
quarantine, T stale timeout, D session reset, A resolved action, F first-key
filtered, W remote wake policy allowed delivery, B ordinary button path.
W is a policy decision, not proof of a physical display waking. Debug's own raw
button loop does not resolve actions: inspect A entries from normal menus/WPS.

## Physical acceptance — AWAITING USER TEST

Do not infer aspect, overscan, picture quality, Dock navigation or wake success
from host tests or a successful build. Keep photographs and traces tied to the
exact source/build hash in the handoff.

1. **A: 4:3 CRT FIRST.** Output On/Auto as appropriate, Standard 4:3, Fit Off,
   Overscan Off. TV in normal 4:3 with zoom off. Open Composite Video Test Screen.
   Photograph the whole outer frame, square and circle. Check equal square sides,
   circular shape, centering, and unstretched text. If A fails, STOP; report the
   photograph, accessory, TV setting, and visible edges before testing anything
   else. The test screen's Local Center toggles the saved TV Screen live.
2. **B: 16:9.** Only after A passes, use a TV configured to 16:9, select Widescreen,
   and repeat the same diagnostic. Check the circle/square; then verify TV-safe
   layout uses the width and Mirror Current UI remains 4:3 with side bars.
3. **C: live switch.** 4:3 -> 16:9 -> 4:3 while output is connected; confirm
   distinct geometry and identical return without reboot, reconnect or music
   restart. Remember the TV itself must match each mode when judging shapes.
4. Repeat A/B/C with each supported accessory available: Philips DCP750/37,
   Apple Universal Dock, composite cable. Their identity must never select DAR.
5. Overscan Off/Small/Medium/Large: controls, corners, lists/dialogs/WPS/status
   stay inside safe frame. Record smallest readable inset; movie must not be
   cropped merely because Overscan changes.
6. Fonts: all three sizes in Home, Files, database, Settings and confirmations;
   long names, Unicode and CJK; check clipping, interlace flicker and distance.
7. WPS: both aspects, square and nonsquare covers, missing/corrupt art, long
   title/artist/album, pause, stop, rapid skips; no stale art or audio disruption.
8. Video: 4:3 and 16:9 sources on both TVs, Fit Off contains, Fit On crops and
   preserves shapes. Compare the same existing MP4/H.264 file against baseline;
   verify audio, seek/resume and overlays. Report any cadence/performance change.
9. Raw remote capture: Up, Down, Left, Right, Center, Menu, Play; each tap, hold,
   repeat, release, and tap after display sleep. Capture identity and transaction
   state. Establish which physical buttons the dock actually forwards first.
10. Full UI navigation in Home, Files, database artists/albums/tracks, Settings,
    WPS, supported plugin menus and video. Play must not open folders.
11. Wake On after timeout, composite active and inactive: Up/Down one row,
    Menu one back, Select one enter, Play one toggle, WPS volume one step.
    Check release/hold, Wake Off, Hold/keylock, and LCD-dark TV control.
12. Regress click wheel, normal WPS, dock audio, playlist and position, repeat
    connect/disconnect, output Off/Auto/On, saved handheld theme/font restoration,
    reboot persistence and removal during a held remote button.

All items above, actual JPEG/artwork I/O latency, physical interlace readability,
active-aperture assumptions, live aspect register changes and accessory events
remain specifically awaiting user hardware confirmation.

## TV WPS revision: Apple assets and stable ownership

The user reports correct circle/square geometry and sizing on their TV at 4:3
with Large overscan. They also report improved artwork in TV mode, but repeated
flashing and an unwanted purple/black appearance. This does not validate 16:9,
live switching, other accessories or the revised firmware.

The stock WPS lifecycle now acquires a separate composite UI ownership flag
before its first skin/LCD update and releases it on leave. Short drawing batches
cannot clear that flag. It remains held across skin waits, scrolling and status
callbacks, which previously could publish handheld frames over the TV canvas.
The target resets ownership on teardown. LCD drawing and video frame staging
remain unchanged; no decoder, DMA, clock or playback-buffer changes are made.

TV chrome now borrows existing cached RetailOS 2.0.4 pixels from
`ipodjs/apple/retailos-2.0.4`: resource 11 background, white status-bar header,
white option-bar thumb pieces for selection, Now Playing progress pieces,
play/pause/repeat/shuffle icons, and resource 6 CoverFlow proxy for missing art.
These are extracted Apple source assets, not recreated gradients or icons.
The independent canvas blends their existing RGB565/alpha pixels and preserves
control end caps. Both TV aspects use the selected artwork-left, metadata-right layout.
TV text uses the already loaded Apple Helvetica 15/19/23
faces, including their antialiasing. No asset loads or new caches are added to
drawing; the handheld theme and font preferences are not modified.

Native text grows 1,492 bytes; data and BSS remain unchanged in the initial
revision build. ARM stack frames including saved registers: asset painter 56,
three-part control 56, WPS draw 168, artwork service 384, video presenter 176
bytes, excluding callees. The original fixed canvas/art buffers are reused.

The host renderer test uses the actual deployed RGA assets and FNT glyphs for
24 aspect/text-size/overscan combinations under ASan/UBSan. The ownership test
executes the actual driver mirror guard and setters for 20,000 intervening LCD
updates and 100 acquire/release cycles. These tests cannot establish whether
physical composite flashing has stopped. Revised hardware: AWAITING USER TEST.


## 2009 Apple TV revision

TV Home now uses a category row (Apps, Videos, Music, Settings) with the
selected category's actual application/library entries below. Left/Right
changes category; Up/Down moves through that category; Select or the silver
Apple Remote's combined Play/Select button opens the selected entry. Short
Menu returns to the parent; held Menu returns Home. In Now Playing, Select
pauses/resumes, Left/Right retains skip/seek, and Up/Down opens Home. TV
navigation activates automatically even when an older config still says
`dock remote mode: playback`. Kokkia isolation and non-TV playback controls
are retained. Held Menu also exits the video library from nested folders.
The Home loop calls existing application and library launch paths; raw plugin
screens retain their own UI. It does not create another music playback loop.

The category labels use their actual Apple font advance widths and equal,
centered cells. All semantic list/dialog/WPS screens share the same top area;
video playback's status strip is placed at the same even-aligned top safe
margin and uses the matching Apple Helvetica glyphs. The stock 4:3/16:9
geometry and the user's Large overscan preference are retained.

The graphics now come directly from Apple's November 2009 Apple TV 3.0.1
image (2Z694-6004-003.dmg): MainMenuTopGradient, MainMenuBarGlow,
BlueGlowSelection pieces, GrayProgress controls, StatusPlay/StatusPause,
shuffle/repeat symbols, and the original shelf_Music missing-art image.
The dark lower portion of the original gradient is cropped to retain text
contrast within the small composite header. The source PNGs are assembled,
cropped and resized without replacement illustration. The missing-art image
is stored at 180x180 to avoid enlarging a small thumbnail. Original resource
hashes and the Apple download URL are in `tv-apple-assets.json`; the generator
is `tools/prepare_tv_apple_assets.py`. Fonts remain the existing Apple RetailOS
Helvetica; a small fixed status alphabet is compiled for the video thread,
so video status rendering cannot open a font file or disturb its glyph cache.

These assets are constant firmware data. They add no heap buffers or open
files, and BSS remains unchanged. List teardown and plugin entry release TV
canvas ownership; lists keep ownership across asynchronous LCD status and
scroll updates. WPS retains its own longer-lived ownership. Video decode,
parser, DMA, clocks, pacing, sync, audio buffers and playlist ownership are
unchanged by this revision.

Focused host tests execute the production Home loop (100 launch/return
routes), remote activation/keymaps, lingo-2 packet parser, driver ownership,
and renderer under ASan/UBSan. Rendering covers 24 layout combinations,
asserts category centering within one pixel, checks video status placement,
and exercises artwork settling/cache/error/obsolete-result cleanup. The
simulator tests handheld regression only: neither it nor the host stubs
validates physical TV output or actual dock packets. Physical remote
navigation, screen flashing and this revised appearance remain
AWAITING USER TEST.
