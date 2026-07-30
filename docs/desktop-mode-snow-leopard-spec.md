# Snow Leopard Desktop Mode Replacement Spec

Status: authoritative replacement for the former Windows XP Desktop Mode
direction. Runtime, private-pack tooling, and Rockpod parity support are
implemented; authentic personal capture and physical 5G/6G/M1 acceptance
remain release gates.

Primary targets: iPod Classic 6G/7G and iPod Video 5G/5.5G, both 320x240.

Host targets: Rockpod on this Linux laptop and Apple-silicon macOS.

## Product Goal

Desktop Mode turns the iPod into a tiny, convincing Mac running the visual
language of Mac OS X Snow Leopard. It is not a Windows skin and it is not a
generic desktop with Apple-like colors. The menu bar, Dock, wallpaper, cursor,
icons, Lucida Grande type, window chrome, controls, sheets, shadows, and
application identity must come from real Snow Leopard resources prepared for
the iPod.

The interaction should still feel like a stock iPod application:

- launch from the iPodJS home screen as `Desktop Mode` and from
  `Extras > Applications > Desktop Mode`;
- enter and leave without rebooting or disturbing music playback;
- use the click wheel as a small trackpad/trackball;
- use the center button as the primary mouse button;
- keep Menu and Play/Pause as predictable iPod escape and secondary actions;
- return from a launched Rockbox application to the same desktop state.

When the iPod is connected to a laptop, Rockpod exposes `Desktop Mode` under
that iPod in the `DEVICES` section. The first deliverable displays an
interactive parity session produced from the same source, configuration, and
personal asset pack. A later, explicitly experimental USB transport can mirror
and control the live iPod framebuffer.

## Replacement Decision

The XP implementation is removed, not retained as a selectable theme.

The following existing concepts are retired:

- `desktop_mode_xp` asset directories;
- Bliss and Luna colors or fallback drawings;
- Start button, taskbar, Start menu, Explorer, Control Panel, and My-prefixed
  labels;
- the current wheel-is-vertical and Previous/Next-is-horizontal pointer model;
- procedural imitation artwork.

The plugin filename remains `desktop_mode.rock` so existing root-menu wiring
and user configuration do not break. Its user-facing name becomes `Desktop`
inside iPodJS and `Snow Leopard Desktop Mode` in Rockpod details.

## Non-Negotiable Authenticity Rule

There are no hand-drawn, traced, procedurally imitated, or generated substitute
assets.

All identity-bearing pixels come from a user-supplied, personally owned Mac OS
X 10.6 installation, installer DVD/image, or reference capture:

- the Aurora desktop picture;
- Finder, iTunes, Preview, iPhoto, System Preferences, Dashboard, TextEdit,
  Calculator, folder, disk, Trash, and utility icons;
- black and white arrow cursors;
- Apple menu glyph and menu extras;
- menu bar, Dock, active/inactive window, toolbar, sidebar, button, checkbox,
  scrollbar, selection, sheet, and tooltip chrome;
- Lucida Grande regular and bold;
- optional Snow Leopard UI sounds.
- the gray Snow Leopard Apple-logo boot background and all twelve real spinner
  phases, captured from a lossless recording of the owned 10.6 installation.

Shrinking, cropping, color-space conversion, alpha premultiplication,
transparent-key conversion, atlas packing, and Rockbox bitmap/font conversion
are permitted. Redrawing missing pixels is not.

### Chrome is cut at 1:1, never downscaled whole

Snow Leopard chrome is drawn for a ~1440x900 screen. Reducing a whole 977x751
Finder window to the iPod's window size destroys every control in it, and the
shell then paints its own list on top of the scaled screenshot's list, so
nothing lines up. That is what the first implementation did, and it is the
single largest reason Desktop Mode did not look like a Mac.

Chrome is therefore cut out of the real captures at Apple's own 1:1 scale and
reassembled at the iPod's geometry by the importer. Only three operations
touch Apple's pixels:

- crop at 1:1;
- repeat a one-pixel strip along the axis in which the real artwork is
  constant (title bar gradients, source-list fills, menu panel bodies, Dock
  shelf middles, scroller tracks);
- fill a nine-slice centre with a colour sampled from that same artwork.

Two reductions are permitted and recorded because the real artwork has no
smaller variant: the Dock is the real Dock at exactly 1:2, so Apple's own
32-pixel Dock icons stand on it in Apple's own proportions; and the 34/38-pixel
magnification variants are reduced from Apple's 128-pixel icon frame rather
than enlarged from the 32-pixel one.

Icons come from the exact `.icns` frame Apple hand-tuned for that size. Reading
an `.icns` through a decoder that only exposes its largest frame silently
substitutes a reduced 512-pixel image for Apple's 32-pixel artwork; the
importer reads the icns directory directly instead.

The layout follows the measured artwork, not the other way round. Where the
artwork and an earlier layout number disagree, the artwork wins and the layout
table below is updated.

The repository stores the importer, manifests, layout data, and validation
hashes. It does not commit or publish Apple's proprietary Snow Leopard assets.
Generated packs are personal local artifacts and must remain ignored by Git.

If the pack is absent, invalid, or incomplete, Desktop Mode shows a plain
Rockbox diagnostic page named `Snow Leopard assets required`. It does not draw
an approximate Mac desktop. The page gives the expected pack path and tells
the user to prepare and install it through Rockpod.

## Source and Provenance Contract

Add a Rockpod importer and matching command-line entry point:

```
rockpod/services/snow_leopard_assets.py
tools/prepare_snow_leopard_desktop_assets.py
```

Accepted sources:

1. a mounted Mac OS X 10.6 system volume;
2. a mounted 10.6 installer image/DVD plus its Packages payloads;
3. an explicit user-selected folder containing extracted official resources;
4. user-supplied lossless reference captures for chrome that cannot be
   extracted as independent system resources.

Expected source families include:

```
/Library/Desktop Pictures/
/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/
/System/Library/CoreServices/Finder.app/Contents/Resources/
/System/Library/PreferencePanes/
/System/Library/Fonts/
/Applications/
```

The importer must discover resources rather than silently assuming that every
10.6 point release has identical filenames. It presents every resolved source
to the user before building.

Every generated pack contains `manifest.json` and `PROVENANCE.txt` recording:

- source volume name and detected Mac OS X version;
- source-relative path for every output;
- source and output SHA-256;
- extraction, crop, resize, and conversion operations;
- output dimensions, depth, alpha/keying mode, and byte size;
- importer version and build time;
- the statement `Personal-use Snow Leopard asset pack; do not redistribute`.

The importer refuses resources from a modern macOS installation when a Snow
Leopard resource is required. It also refuses a pack containing procedural or
fallback asset IDs.

## Device Pack Layout

Canonical hardware location:

```
/.rockbox/rocks/apps/desktop_mode_snow_leopard/
```

Hosted Rockbox simulators resolve `PLUGIN_APPS_DATA_DIR` under
`rocks.data`, so Rockpod installs and checksum-verifies the same manifest in
both locations:

```
/.rockbox/rocks.data/desktop_mode_snow_leopard/
```

Updating only one copy is not a complete Desktop Mode install.

Pack format 2, 60 mandatory logical IDs. Three payload formats:

- `.bmp` — 16-bit RGB565 `BI_BITFIELDS`, for opaque chrome;
- `.rga` — `RGA1` header plus RGB565 and Apple's 8-bit coverage, for anything
  with a soft edge, because the shell composites it over real chrome rather
  than keying it out;
- `.alpha` / `.metrics` — raw Lucida Grande coverage atlas plus a 103-byte
  `DMF1` sidecar (cell width, cell height, ascent, per-glyph advances).

```
desktop_mode_snow_leopard/
    manifest.json
    PROVENANCE.txt
    boot/
        background.320x240x16.bmp
        spinner-{00..11}.24x24x16.bmp
    desktop/
        aurora.320x240x16.bmp
        menubar.320x21x16.bmp
        apple-highlight.22x21x16.bmp
        dock-shelf.288x26x16.bmp
        dock-indicator.14x8.rga
    cursor/
        arrow.14x20.rga
        pointing-hand.16x18.rga
    chrome/
        window-sidebar.304x174x16.bmp
        window-plain.304x174x16.bmp
        list-selection.217x19x16.bmp
        sidebar-selection.86x19x16.bmp
        menu-panel.152x124x16.bmp
        menu-selection.150x19x16.bmp
        context-panel.124x71x16.bmp
        sheet.240x112x16.bmp
        tooltip.104x18x16.bmp
        scroller-track.16x97x16.bmp
        scroller-thumb.16x36x16.bmp
    icons/
        {finder,itunes,preview,textedit,calculator,directv,system-preferences,
         dashboard,disk,trash-empty,folder-desktop}.32x32.rga
        {folder,document}.16x16.rga
        {finder,itunes,preview,textedit,calculator,directv,system-preferences,
         trash-empty}-dock-{34,38}.{34,38}x{34,38}.rga
    fonts/
        lucida-grande-11.alpha       lucida-grande-11.metrics
        lucida-grande-bold-11.alpha  lucida-grande-bold-11.metrics
        lucida-grande-9.alpha        lucida-grande-9.metrics
```

The manifest maps every stable logical ID to one exact path, source SHA-256,
output SHA-256, conversion record, dimensions, and 10.6 provenance.

Two window frames serve every application: `window-sidebar` for Finder and
`window-plain` for everything else. Shipping one frame per application cost
105,792 bytes each and bought nothing, because the frames differ only in
content the shell draws itself.

### Trash

Only the empty Trash icon is imported. Desktop Mode deletes through
`rb->remove()`, so there is no Trash to be full of anything, and swapping in
the full-bin icon from an unrelated condition is a lie told in pixels. When a
real Trash with real contents exists, the full icon returns with it.

### iPhoto

No owned Snow Leopard source contains iPhoto's icon: it is not on the 10.6
installer DVD, and the iPhoto 9.1 *update* package carries no `.icns`. The
Photos slot is therefore removed from the Dock and the application list rather
than filled with substitute art, per the authenticity rule. Calculator takes
its place. If a source containing the real icon is later supplied, the slot
returns unchanged.

### Fonts

Lucida Grande is rendered on a common baseline with the left side bearing
inside the cell, so the pen position is the cell origin and the advance is the
step. Getting either of these wrong is visible immediately: centring each glyph
in its own cell gives every glyph a different baseline and renders
"Applications" as "APPlicatio", and thresholding the antialiasing to opaque
turns every soft edge into a blot.

Coverage is kept at 8 bits and blended at draw time against whatever the shell
has already composed. One atlas per face therefore serves black text on
chrome, white text on the selection gradient, and grey secondary text, instead
of one pre-composed atlas per backdrop colour that is wrong on every gradient.

The importer uses high-quality downsampling once from the largest official
source. It never repeatedly resizes an already reduced asset. Alpha artwork is
precomposed against the finite backgrounds used by the shell or converted to a
validated transparent-key bitmap. Icons are manually inspected at final
320x240 scale; no sharpening or repainting is allowed unless it is a
reversible, recorded conversion applied to the entire source.

## 320x240 Desktop Layout

Every bound below is measured from the real artwork, not chosen. The menu bar
is 21 rows because that is what the real translucent bar measures in the
capture; the Dock band is 42 rows because Apple's 32-pixel icons standing on a
half-scale real shelf occupy exactly that.

| Region | Bounds | Behavior |
| --- | --- | --- |
| Menu bar | `0,0,320,21` | Always visible; Apple glyph at left, status at right |
| Desktop work area | `0,21,320,177` | Wallpaper, volume, desktop folders, windows |
| Dock band | `0,198,320,42` | Icons from `y=198`, real shelf `16,214,288,26` |
| Pointer | asset-dependent | Always topmost, clipped only at LCD edge |

Window geometry, in the same measured units:

| Part | Bounds | Source |
| --- | --- | --- |
| Window | `8,24,304,174` | default and only size |
| Title bar | `+0,+0,304,24` | real cap with three real traffic lights |
| Toolbar | `+0,+24,304,29` | real back/forward, view segments, action gear |
| Sidebar | `+0,+53,86,97` | real source-list fill and separator |
| File list | `+87,+53,217,97` | 5 rows of 19, real selection gradient |
| Scroller | `+287,+53,16,97` | real track and knob, shown only when needed |
| Status bar | `+0,+150,304,24` | real gradient with real rounded corners |

The 320x240 Dock holds Finder, iTunes, Preview, TextEdit, Calculator, DIRECTV,
Sitekick, System Preferences and Trash at a 34-pixel pitch, magnifying the
hovered icon to 38 and its neighbours to 34. The desktop1080 host profile adds
Netflix between Sitekick and System Preferences and uses a 60-pixel pitch.
Netflix is intentionally absent from the physical-iPod Dock; the regular
iPodJS Netflix screen remains unchanged.

Default composition:

- full-screen real Aurora wallpaper, center-cropped to 4:3 by the importer;
- the connected volume named `iPod` at the upper right;
- `Documents`, `Music`, `Movies`, and `Pictures` down the right edge when
  enabled;
- translucent Snow Leopard menu bar across the top;
- centered nine-icon Dock containing Finder, iTunes, Preview, TextEdit,
  Calculator, DIRECTV, Sitekick, System Preferences, and Trash;
- black arrow pointer starting near the center, never placed under the Dock on
first launch.

Startup first displays the imported Snow Leopard boot background and twelve
captured spinner phases at 12 Hz. The background and all phases are decoded
once into a 167,424-byte transient region of `plugin_get_buffer()`, painted
only from cached pixels, and discarded by resetting the plugin arena before
the desktop assets load. Any button or Hold skips directly to the desktop;
USB is handed back through `PLUGIN_USB_CONNECTED`. The sequence never claims
an audio buffer and does not play a startup sound over active music.

Window model:

- one full application window plus one modal sheet is the supported device
  model;
- a second app launch replaces or backgrounds the front app state without
  allocating another framebuffer;
- default window bounds are `8,24,304,174`;
- title-bar traffic-light controls are the real ones, cut with the title bar
  cap they sit in;
- yellow minimize animates the window rect into its Dock icon, then releases
  the app's transient view state;
- red closes the window and returns to Finder/Desktop;
- inactive chrome is used only during a sheet or menu.

Green zoom is not implemented: at 320x240 the default bounds already fill the
work area between the menu bar and the Dock, so there is no second size to
toggle to. The control is drawn because it is part of the real title bar cap;
clicking it does nothing.

The importer assembles scalable chrome from real 1:1 slices at build time
rather than the shell nine-slicing at run time, because the window size is
fixed and slicing at run time buys nothing. The shell still composes panels of
arbitrary width from a real left cap, a repeated real middle column and a real
right cap, which is how tooltips hug their label. It may fill a nine-slice
centre using an exact sampled source colour when the manifest marks that fill
as lossless. It must not synthesize Aqua highlights, pinstripes, brushed metal,
gloss, shadows, or glyphs.

## Menu Bar and Menus

The left side contains:

- Apple glyph;
- foreground application name in bold;
- app-specific menus such as `File`, `Edit`, `View`, `Go`, `Window`, `Help`.

The right side contains, as space permits:

- playback status;
- volume;
- battery;
- USB/live-display state;
- `Sun 10:42 PM` or a shorter clock when other extras are visible.

Apple menu:

1. About This iPod
2. System Preferences…
3. Recent Items
4. Sleep Display
5. Restart Rockbox…
6. Return to iPod…

`Restart Rockbox` and `Return to iPod` use Snow Leopard-style confirmation
sheets. Neither action is bound to an accidental short wheel gesture.

Menus open under the menu bar, use the real menu selection texture, and close
on outside click or Menu. Menu items expose disabled, checked, submenu, and
keyboard-equivalent columns even where only a subset is used.

## Dock Behavior

The Dock is a launcher and running-app switcher:

- click launches or foregrounds an app;
- a real Snow Leopard indicator dot marks an app with saved foreground state;
- hover shows a real-style tooltip using Lucida Grande;
- the selected icon magnifies from 32 to at most 38 pixels;
- neighbors may magnify to at most 34 pixels;
- magnification is computed from pointer distance and repaints only the Dock
  rectangle;
- clicking a minimized app restores it;
- dragging a supported file over an app highlights valid targets;
- Trash accepts only reviewed in-shell operations and always asks before
  deleting device files.

There is no animated reflective live preview and no second Dock framebuffer.
Any minimize/restore animation is elapsed-time based, limited to six frames,
and cancels directly to its destination state when input is queued.

The iPodJS main-menu right pane shows the idle Aurora desktop with the
320x240 nine-icon Dock. It does not open or clip a Finder/application window
into the preview. The pane is generated once by the private-pack importer and
remains a cached bitmap at draw time.

Desktop Mode is a shell over the mounted iPod filesystem, not a mock desktop.
Finder opens audio files through Rockbox core playback and videos through their
registered MPEG/OpenH264 viewer. iTunes reads Songs, Albums, and Artists from
the device tagcache, fetches only the current visible page before painting,
queues the selected page or selected Album/Artist only after an explicit
double-click, and reads Videos from `.rockbox/videolist/index.tsv`. Opening a
video hands its real device path to the registered player. These handoffs never
take the shared plugin audio buffer or stop playback as a decorative side
effect.

`View iPod on Desktop` builds an isolated simulator runtime but does not copy
its demo library. Every connected-iPod root entry except `.rockbox` is exposed
through a live host symlink, while the device's `database*.tcd`,
`tagcache*.tcd`, `.rockbox/videolist`, `.rockbox/ipodjs/netflix`, and
`.rockbox/sitekick` are copied into the isolated runtime. The copies protect
the mounted database and application state from simulator writes; the media
symlinks let Finder, iTunes, Netflix, and the registered players open the
actual device files. If the device has no video index, iTunes Videos and
Netflix are empty rather than falling back to hardcoded simulator rows.
Simulator plugin autostart follows Rockbox `PLUGIN_GOTO_PLUGIN` handoffs so
selecting a video opens its real player instead of closing the desktop
session. The parity runtime also removes the build simulator's playlist-control
and resume files, so an unrelated stale playlist cannot delay the first Desktop
Mode interaction.

## Built-In Applications

### Finder

Finder replaces the existing Explorer surface.

It provides:

- sidebar favorites: iPod, Applications, Documents, Music, Movies, Pictures;
- list view as the default at 320x240;
- optional compact icon view for small folders;
- path title and Back/Forward controls;
- folders-first, case-insensitive sorting;
- 96-entry bounded directory snapshot, matching the present plugin ceiling;
- file opening through Rockbox's registered viewers;
- `.rock` application launch;
- Get Info sheet with filename, type, size, modified time, and path;
- new-folder and delete actions only after an explicit confirmation;
- no automatic filesystem indexing or thumbnail generation.

Directory scanning occurs on entry or explicit refresh, never from a paint
function. While scanning, the previous complete window remains visible with a
busy cursor. A scan can be cancelled with Menu.

### iTunes / Now Playing

The Dock label is `iTunes`, using the real Snow Leopard iTunes icon.  iTunes
is a window with its own furniture, not the Finder frame with a different
title: the round transport buttons, the status display between them, the blue
source list and the striped track view are all cut from a real iTunes 9
capture at 1:1, the same way the rest of the chrome is.

It lists the music on the device and plays it.  Double-clicking a track is the
one place Desktop Mode builds a playlist, and it does so only because the user
asked for that track by name - that is what a music application is for.  The
visible page is queued immediately on Songs; Album and Artist selections queue
their matching tagcache result. This bounds click-to-play work instead of
walking thousands of tracks before sound starts. Nothing else in the shell
touches the playlist, and iTunes otherwise reports playback rather than
commanding it.

At 320x240 the iTunes window occupies the complete work area below the menu
bar. Its native 17-pixel body stripes are repeated without scaling to expose
six rows, and the Dock is hidden only while the settled iTunes window is in
front. Closing or minimising returns to the normal desktop and real Dock. The
Rockpod host path prefers the dedicated 1920x1080 simulator target, where the
same app uses the full-size iTunes 9 chrome, three metadata columns, audio
codecs, and the registered MPEG/OpenH264 video viewers.

- current artwork only when already cached by playback;
- title, artist, album, elapsed time, play/pause, previous, and next;
- volume control;
- `Show Now Playing` exits the shell to Rockbox's existing WPS, then restores
  the desktop on return;
- it never claims a new album-art slot after playback starts;
- it never stops, restarts, shrinks, or takes ownership of the playback buffer;
- it never replaces the user's playlist.

### Preview / Photos

Photos is a window, not a takeover.  The shell lists the pictures folder
itself and hands a chosen image to the real viewer, which is the only part
that has to own the LCD.

Preview opens supported image files through the existing viewer path. The
desktop window shows metadata and a cached small preview only if Rockpod
prepared one. Decoding a full image is not performed during desktop painting.

### Netflix (desktop1080 only)

Netflix launches `netflix_desktop.rock` as a real Snow Leopard window over the
visible desktop. It reads only the mounted iPod's Video Sync manifest and
hands the selected synchronized file to MPEGPlayer or OpenH264 Player.
Live TV and YouTube/Downloaded paths are rejected even if malformed tooling
places them in the manifest. See `docs/netflix-desktop-mode-app-spec.md` for
the complete source, input, memory, and playback contract.

### TextEdit, Calculator, Calendar, Games, and Utilities

These are Snow Leopard launch surfaces for installed Rockbox plugins. The
shell keeps the app title, Dock running indicator, last Finder path, and cursor
position across plugin return. Missing applications are disabled in the menu
and Dock configuration rather than represented by imitation art.

### System Preferences

Preference panes:

- Desktop & Screen Saver: choose only verified pack wallpapers;
- Dock: size, magnification, position fixed to bottom for the first release;
- Mouse: speed, acceleration, natural/reversed wheel direction, drag lock;
- Sound: UI sounds and volume, off by default;
- Appearance: graphite or blue only when matching real imported assets exist;
- Desktop Mode: start at desktop/Finder/iTunes, show desktop folders, restore
  last session, and USB display preference;
- About: pack version, source version, provenance status, memory counters.

Settings are stored in the existing `desktop_mode.cfg` using additive keys.
Old XP-only settings are ignored once, then removed when the new config is
successfully saved.

## Click Wheel as Mouse

### Default: absolute-wheel trackpad

Both target families expose `HAVE_WHEEL_POSITION`, and the plugin API provides
`wheel_status()` and `wheel_send_events()`. Desktop Mode uses the absolute
0–95 contact position instead of reducing the wheel to Up/Down events.

While Desktop Mode owns input:

1. call `wheel_send_events(false)` after saving the prior event policy;
2. sample the wheel at the input cadence;
3. when contact changes from `-1` to a valid position, set an anchor without
   moving the pointer;
4. convert successive positions into two-dimensional tangent motion around
   the physical ring;
5. handle the 95-to-0 wrap before calculating the delta;
6. reject one- or two-count contact jitter;
7. apply elapsed-time velocity filtering and bounded acceleration;
8. reset the anchor on finger lift, Hold, USB state change, or app handoff;
9. restore normal wheel event delivery on every exit path.

This makes the wheel behave like a tiny circular trackball: the pointer follows
the direction of the finger's movement around the ring instead of jumping
through focusable rows. It supports diagonal motion and fine positioning.

### Held touch glides

Tangential motion alone means crossing the screen takes repeated strokes and
reaching a corner is tedious. Resting a finger on the ring - touching, not
clicking - therefore keeps the pointer travelling in the direction that point
of the ring faces: the top of the wheel is up, the right is right, and so on,
so the ring reads like a compass.

- the glide starts only after a quarter-second of stillness, so a stroke that
  pauses mid-way does not drift;
- it ramps from 6 to 40 sixteenths of a pixel per sample over the following
  half second, scaled by the mouse-speed preference, and then holds;
- movement accumulates in sixteenths of a pixel so a slow glide is smooth
  rather than stepping whole pixels;
- any change in contact position cancels the glide and returns to tangential
  motion; lifting resets it;
- a glide drags when the primary button is held, exactly as motion does.

### Host mouse in the Rockpod parity session

The person driving a parity session is sitting at a computer with a real
mouse, so steering a virtual pointer with a simulated click wheel would be
absurd. In simulator builds the host pointer moves the Snow Leopard pointer
directly, the left button is the primary click and double-click, and the right
button is the secondary click.

The simulator publishes the host pointer in panel coordinates through a small
fixed-width record, undoing whatever scale the renderer is using so fullscreen
and zoomed sessions both land true.  The record lives at a fixed path *inside*
the simulated filesystem, because the plugin reads through that filesystem and
an absolute host path would be resolved under the simulator root and never
found.  `ROCKPOD_SIM_HOST_POINTER` gives the simulator the host-side path of
that same file. It
goes through that record rather than a new plugin API entry because the loader
compares `PLUGIN_API_VERSION` for exact equality: adding an entry would
invalidate every `.rock` already on the user's device. The path is compiled
only into simulator builds; on hardware the wheel is the pointer.

Pointer requirements:

- slow movement: 1–2 pixels per accepted sample;
- normal movement: 3–6 pixels;
- fast sweep: capped at 14 pixels per sample;
- no pointer warp between screen edges;
- no movement from a stationary finger;
- no acceleration carry-over after lift or direction reversal;
- dirty update is the union of old cursor rect, new cursor rect, and changed
  hover rect;
- complete click-to-paint latency under 80 ms on hardware.

### Button bindings

| iPod input | Mouse/shell action |
| --- | --- |
| Select press | Primary button down |
| Select release | Primary button up/click |
| Select hold + wheel motion | Drag |
| Two Select clicks within 350 ms | Double-click |
| Play/Pause release | Secondary click / Control-click |
| Menu release | Close menu, cancel sheet, Finder Back, or close front window |
| Previous / Next | Finder Back / Forward; otherwise 1-pixel horizontal nudge |
| Menu hold | Focus Apple menu |
| Play/Pause hold | Emergency `Return to iPod` confirmation |
| Hold switch | Freeze pointer and buttons immediately |

Drag lock is optional and off by default. A Select hold without movement does
not open a launcher. The old Select-hold Start-menu behavior is removed.

### Accessibility fallback

If absolute wheel input is unavailable in a simulator or future target, the
plugin uses four-direction pointer motion:

- wheel Up/Down move the pointer vertically;
- Previous/Next move it horizontally;
- Select clicks, and Select held during motion still drags;
- a tooltip below the menu bar explains the mapping for the first four
  seconds after entry.

Fallback is entered by observation, not by build flag: the shell switches the
moment it needs to move a pointer and `wheel_status()` has never reported a
contact. On a target whose absolute wheel works, the first contact latches
absolute mode permanently for that session and Previous/Next return to Finder
Back/Forward.

This path is what makes the focused simulator gate able to exercise hover,
menus, Dock magnification and clicks at all; without it no simulator capture
can move the pointer. The 5G and 6G hardware acceptance path must still use
absolute-wheel mode.

## Rockpod Device Integration

Add `Desktop Mode` as a child of the connected iPod in the existing `DEVICES`
section, adjacent to `On This iPod` and `Not on iPod`. It is device-scoped, not
a generic item in the `ROCKBOX` authoring section.

New UI modules:

```
rockpod/ui/desktop_mode_panel.py
rockpod/services/desktop_mode.py
rockpod/services/snow_leopard_assets.py
```

The panel contains:

- connected state and Desktop Mode runtime status;
- local and connected-iPod pack version, provenance, and checksum status;
- `Import Owned 10.6 Assets…`, with exact source review before conversion;
- `Install Verified Pack to iPod`;
- `Display This iPod on This Computer`;
- `Display Local Pack`;
- `Stop Host Display`;
- `Live iPod Display (Experimental)` only when transport capability is
  detected;
- capture instructions and last validation/session diagnostics.

`Install/Repair on iPod` creates a reviewed plan and backs up only files owned
by the Desktop Mode manifest. It does not replace `.rockbox` wholesale. It
verifies every copied asset checksum and plugin/config destination before
reporting success.

### Display on This Mac: parity session

This is the first supported host-display feature.

Rockpod:

1. snapshots the connected device's Desktop Mode configuration and verified
   pack into a private simulator working root;
2. selects a simulator built for the device's 320x240 target;
3. launches `desktop_mode.rock` directly;
4. displays the 320x240 session inside the Device > Desktop Mode panel when
   embedding is supported, otherwise in a managed simulator window;
5. translates host pointer movement, click, scroll, and keyboard input into
   the same abstract Desktop Mode actions;
6. never writes simulator session state back to the iPod unless the user
   chooses `Apply Settings to iPod`.

It is the same Desktop Mode implementation and assets, but it is not described
as a live mirror. The panel labels it `Desktop parity session`.

Linux uses the existing simulator discovery and embedded-preview machinery.
Apple-silicon macOS needs a signed/universal or arm64 Rockbox simulator build;
Rockpod must not launch the current Linux binary through emulation and call it
supported. The macOS presentation uses a native child window or managed
top-level window because X11 `xdotool` embedding is Linux-specific.

Host acceptance:

- identical asset manifest and layout IDs on iPod and host;
- the session fills the host display edge to edge: Rockpod launches the
  simulator with `--fullscreen`, which uses desktop fullscreen so the host's
  own display mode is left alone, then scales each axis independently so the
  panel covers the whole screen with no pillarboxing.  A 4:3 panel on a 16:9
  display is stretched horizontally as a result; that is the cost of filling
  the display and it is deliberate;
- the fullscreen switch happens on the first render, not during window setup:
  going fullscreen needs the window manager to answer, and the event thread
  that pumps that answer does not exist yet during setup, so switching there
  blocks before the first frame is ever drawn and the session stays black;
- a windowed session is still available and then uses the largest whole-number
  scale that fits the screen Rockpod is on - 4x (1280x960) on a 1080p laptop,
  8x on a 4K display - because below fullscreen an integer scale keeps every
  imported Snow Leopard pixel square and inspectable;
- the renderer's logical size is the panel, set when the renderer is created,
  so host pointer positions map back onto the panel correctly in both modes;
- the panel reports how the session is presented;
- native Mac mouse moves the virtual pointer directly;
- clicking the rendered center button is optional; the actual desktop surface
  is directly interactive;
- closing the panel terminates only its simulator process and leaves the iPod
  untouched.

### Live iPod Display: experimental transport

A true connected-device mirror is a separate milestone because the current
plugin returns `PLUGIN_USB_CONNECTED` as soon as USB connects.

It must not be faked by writing screenshots repeatedly to the FAT volume.
Concurrent device and host filesystem traffic is unsafe and too slow.

The proposed transport is a composite USB configuration:

- existing storage/charging behavior remains recoverable;
- existing HID mouse capability remains available;
- a Rockpod vendor bulk interface carries framed Desktop Mode messages;
- framebuffer traffic never touches the shared storage filesystem;
- control traffic never enters the audio or playlist paths.

Protocol v1 messages:

```
HELLO       protocol, target, LCD format, pack id, capabilities
FRAME_KEY   sequence, 320x240 RGB565 frame
FRAME_RECT  sequence, x, y, w, h, encoding, payload
INPUT       sequence, pointer/button/key action
STATE       active app, cursor, playback-safe summary, USB state
PING/PONG   monotonic time and latency
GOODBYE     reason
```

Transport requirements:

- full keyframe on connect and after sequence loss;
- dirty rectangles thereafter;
- RGB565 raw and simple bounded RLE only; no large compression workspace;
- target 10 frames/s, minimum acceptable 6 frames/s during interaction;
- median iPod-to-host visible latency under 120 ms;
- host input-to-iPod paint under 150 ms;
- bounded two-frame queue that drops stale decorative frames;
- no blocking USB write from the UI draw function;
- disconnect returns control locally within one second;
- failure leaves the iPod operable and does not require storage repair.

The live path remains hidden behind `Experimental` until iPod 6G hardware and
M1 macOS both pass the USB, playback, disconnect, sleep/wake, and storage
recovery gates.

### Optional host mouse mode

Both target configs already advertise `HAVE_USB_HID_MOUSE`. A separate
`Use click wheel as Mac mouse` switch may expose the iPod as a standard USB HID
mouse without opening a mirror:

- wheel trackpad motion becomes HID X/Y;
- Select is left button;
- Play/Pause is right button;
- wheel rotation may be exposed as scroll only in an explicit alternate mode;
- no Rockpod driver is required on Linux or macOS;
- enabling it never implies that the iPod framebuffer is mirrored.

This mode is off by default so connecting for sync does not unexpectedly move
the host cursor.

## Runtime Architecture

Split the 1,500-line plugin into bounded components:

```
apps/plugins/desktop_mode.c                 entry/lifecycle
apps/plugins/desktop_mode/
    dm_assets.c/.h                          manifest and eager asset load
    dm_input.c/.h                           wheel trackpad and abstract actions
    dm_shell.c/.h                           app/window/menu state machine
    dm_render.c/.h                          cached-pixel composition
    dm_finder.c/.h                          bounded directory snapshot
    dm_preferences.c/.h                     config and sheets
    dm_usb_display.c/.h                     experimental transport, gated
```

The plugin remains C99 and uses the existing plugin API. The shell state is
explicit:

```
DESKTOP -> MENU -> DESKTOP
DESKTOP -> APP_OPEN -> SHEET -> APP_OPEN
APP_OPEN -> MINIMIZED -> APP_OPEN
APP_OPEN -> EXTERNAL_PLUGIN -> APP_OPEN
ANY -> RETURN_CONFIRM -> EXIT
ANY -> USB_HANDOFF -> ANY
```

### Compositor

The plugin API exposes no framebuffer, so a paint function cannot read back
what it has already drawn. Real Snow Leopard chrome needs exactly that: Lucida
Grande is antialiased, and icons, cursors and the Dock indicator all have soft
edges that have to blend against whatever is underneath them, which is a
gradient more often than not.

Desktop Mode therefore assembles each frame in one 320x240 RGB565 plane inside
`plugin_get_buffer()` and hands it to the LCD with a single `lcd_bitmap()` and
a single `lcd_update()`. The buffer costs 153,600 bytes and is accounted in
`docs/desktop-mode-snow-leopard-size-report.md`.

This is one buffer, not the source/destination pair the rendering rules forbid.
It also removes a cost: the 105,792-byte minimize/restore scratch bitmap is
gone, because the animation scales the window's own chrome straight into the
frame being assembled. Net change against pack format 1 is +47,808 bytes.

Compositing over a cached plane is not a licence to compute artwork. The
primitives are an opaque row copy, a coverage blend, and a repeated column;
nothing generates a gradient, a highlight or a glyph.

Paint functions receive immutable state and loaded bitmap handles. They do not
open files, scan directories, decode bitmaps, load fonts, query tagcache, or
launch plugins.

## Memory, Audio, and Storage Invariants

This work is governed by `docs/ipodjs-ui-memory-animation-steering.md`.
Playback-adjacent iTunes/USB work must also follow
`docs/plugin-audio-lifecycle-steering.md`.

Required invariants:

- never call `plugin_get_audio_buffer()` for Desktop Mode visuals;
- never call `audio_stop()` to make the desktop or USB display fit;
- never call `core_alloc(FRAMEBUFFER_SIZE)` for decoration or animation;
- never shrink, restart, or take ownership of playback memory;
- never mutate the current playlist;
- never claim or resize an album-art slot after playback starts;
- load verified shell assets once at plugin start from the plugin buffer;
- reject the pack before entering the shell if mandatory assets do not fit;
- do not fall back to partial/procedural rendering after a load failure;
- draw only from cached pixels;
- directory and config I/O occurs outside paint and closes every descriptor;
- suspend optional animation while storage is active or input is queued;
- preserve Hold, USB, shutdown, and default system-event handling.

Budget must be measured, not guessed. The implementation change must add a
checked-in generated size report with:

- packed asset bytes on disk;
- decoded asset bytes in the plugin buffer;
- fixed state/BSS bytes;
- largest stack frame;
- simulator and iPod 6G `rockbox.elf` text/data/BSS delta;
- playback-active available/allocatable core memory before, during, and after
  Desktop Mode.

Decoded ceiling is at most 1 MiB for the mandatory shell. Optional app
icons/chrome are loaded in bounded groups only when their window is inactive
and no input is queued. If real assets cannot meet that limit, reduce the
number of simultaneously resident assets or atlas depth; do not borrow
playback memory.

## Rendering and Animation Rules

- logical layout is always 320x240 on target and parity host;
- integer coordinate math only in the device renderer;
- one LCD update per committed frame;
- cursor-only movement updates dirty rectangles where target APIs allow it;
- active interaction targets 20 Hz; idle clock/status refresh is 1 Hz;
- Dock magnification and minimize/restore derive position from elapsed ticks;
- animation never busy-waits;
- queued input finishes or cancels decorative movement immediately;
- no animation owns input after its destination frame is committed;
- no full-screen source/destination framebuffer pair;
- wallpaper is the only full-screen decoded decorative bitmap;
- menus and sheets are composed directly over the current deterministic app
  redraw, not over a captured screen copy.

## Implementation Plan

### M0: Remove XP identity

- rename this specification and make it authoritative;
- replace all XP constants, labels, colors, settings, and asset paths;
- remove procedural Bliss/Luna/icon/cursor rendering;
- add the asset-required diagnostic;
- keep the plugin build and root-menu launch working.

Exit: no `desktop_mode_xp`, Bliss, Luna, Start menu, Explorer, or Control Panel
reference remains in runtime code or user-visible text.

### M1: Personal asset pipeline

- implement source discovery, preview, provenance, conversion, and validation;
- import real Snow Leopard wallpaper, cursor, Lucida Grande, chrome, and core
  icons;
- add Rockpod import/install/repair workflow;
- add `.gitignore` coverage for private output;
- validate pack on both app and device.

Exit: a clean checkout plus user-owned 10.6 source can reproducibly create the
same checksummed device pack without a proprietary file entering Git.

### M2: Shell and pointer

- split the plugin;
- implement absolute-wheel trackpad, mouse-down/up, drag, double-click, and
  fallback controls;
- implement menu bar, Apple menu, Dock, desktop, windows, menus, sheets, and
  System Preferences from real assets;
- implement safe state restoration around external plugins.

Exit: the physical iPod feels like a small Mac and all core operations are
possible without row-focus navigation.

### M3: Finder and iTunes

- replace Explorer with Finder;
- add safe file operations and Get Info;
- add playback-safe iTunes controller and WPS handoff;
- add Preview and utility launch bindings.

Exit: Finder and playback work through ten enter/exit cycles with active music,
stable playlist identity, stable file descriptors, and stable core memory.

### M4: Rockpod parity display

- add Device > Desktop Mode panel and connected-device child;
- reuse simulator binding, isolated working roots, and process cleanup;
- add Linux embedded/managed display;
- add native Apple-silicon simulator packaging and managed macOS display.

Exit: `Display on This Mac` runs the same 320x240 shell and verified asset pack
on this laptop and an M1 Mac without writing session state to the iPod.

### M5: Experimental live USB display

- qualify composite USB feasibility on 6G first;
- add framed vendor-bulk transport and bounded dirty-frame queue;
- add Rockpod Linux/macOS transport client;
- add capability negotiation, diagnostics, reconnect, and clean downgrade;
- optionally expose standard HID host-mouse mode.

Exit: live display passes the hardware gates below. Until then the button stays
hidden unless an experimental capability flag is enabled.

## Files Expected to Change

Device/runtime:

- `apps/plugins/desktop_mode.c`
- `apps/plugins/desktop_mode/*`
- `apps/plugins/SOURCES`
- `apps/root_menu.c`
- `apps/plugin.h` and USB class files only if M5 proves new API is required
- target USB configuration only for the qualified experimental transport

Rockpod:

- `rockpod/ui/sidebar.py`
- `rockpod/ui/main_window.py`
- `rockpod/ui/device_summary.py`
- `rockpod/ui/desktop_mode_panel.py`
- `rockpod/ui/dialogs/snow_leopard_asset_import.py`
- `rockpod/services/desktop_mode.py`
- `rockpod/services/snow_leopard_assets.py`
- `rockpod/services/rockbox_simulator.py`
- `rockpod/app/config.py`
- focused tests for each new service and surface

Tooling/docs:

- `tools/prepare_snow_leopard_desktop_assets.py`
- a focused simulator regression/capture gate
- `.gitignore`
- this specification and a generated private-pack README template

## Automated Gates

Minimum source/asset tests:

- reject an incomplete or wrong-version pack;
- reject dimensions, depth, transparency, or checksum drift;
- verify every runtime-required logical ID exists;
- prove no procedural fallback symbol or XP term remains;
- round-trip config migration;
- validate wheel wrap, lift reset, jitter filter, acceleration cap, drag, and
  double-click timing with deterministic input traces;
- verify paint call graphs contain no file, bitmap decode, directory, tagcache,
  audio-buffer, or allocation call;
- verify all plugin-open and USB exit paths restore wheel event delivery.

Build and simulator:

```
make -C build-sim-ipod6g rocks
make -C build-sim-video-5g rocks
./tools/ipodjs_navigation_sim_regression.sh
./tools/desktop_mode_snow_leopard_sim_gate.sh
```

The focused gate must capture and compare:

- clean desktop;
- Apple menu;
- Dock hover/magnification;
- Finder root and nested folder;
- Get Info sheet;
- iTunes while music is active;
- System Preferences > Mouse;
- minimize and restore destination frames;
- missing/invalid asset diagnostic;
- parity-session input.

Visual comparison uses exact asset/layout regions with zero tolerance for
unapproved pixel drift and an explicit mask only for clock, playback progress,
and pointer position.

## Physical iPod Gate

Run on both 6G and 5G before release:

1. start database music and record playlist identity and elapsed time;
2. enter Desktop ten times without stopping or restarting playback;
3. move the pointer slowly, diagonally, across wheel wrap, and with fast
   sweeps;
4. perform click, double-click, click-drag, cancel, Menu Back, and emergency
   return;
5. open Finder, descend five levels, open a registered viewer, and return;
6. open iTunes, control playback, hand off to WPS, and return;
7. open and close every menu, sheet, and preference pane;
8. minimize and restore each built-in app twenty times;
9. leave the desktop idle for ten minutes with playback active;
10. verify playback identity, elapsed time, core memory, descriptors, wheel
    behavior, and database access after exit.

Hardware acceptance:

- no audio stop, restart, click, underrun, or playlist change;
- no `Database is not ready`;
- no stale window, pointer trail, Rockbox theme, or XP frame flash;
- no descriptor or memory trend;
- no input latency over 150 ms during ordinary storage-idle interaction;
- Hold freezes input immediately;
- every exit path restores ordinary iPod click-wheel behavior.

## Live USB Hardware Gate

Additional M5 gate on iPod 6G, this Linux laptop, and an M1 Mac:

1. connect from an active local Desktop Mode session;
2. negotiate, display a keyframe, and control the same live session;
3. continue database playback for thirty minutes;
4. repeatedly open Finder and iTunes while frames stream;
5. disconnect during idle, pointer movement, a dirty-frame transfer, a menu,
   and a sheet;
6. reconnect without rebooting;
7. sleep and wake the host;
8. enter and leave storage/sync mode through an explicit transition;
9. confirm both firmware copies and device database remain valid after any
   deployment used for the test.

Hard failures:

- any simultaneous unsafe FAT ownership;
- plugin exit merely because the cable connected;
- audio lifecycle interference;
- dropped disconnect that leaves input or USB wedged;
- storage corruption or required filesystem repair;
- host kernel extension requirement on Apple silicon;
- calling parity mode a live mirror.

## Definition of Done

The replacement is complete when:

- the XP desktop and all XP runtime references are gone;
- the device renders only verified, real, user-imported Snow Leopard assets;
- the wheel acts as a two-dimensional pointer with reliable click and drag;
- Finder, Dock, menu bar, windows, sheets, iTunes controls, and preferences
  form a coherent mini-Mac shell;
- music and database behavior remain unchanged before, during, and after use;
- Rockpod exposes Desktop Mode under the connected device;
- this laptop and an M1 Mac can display an interactive parity session;
- any true live mirror is accurately labeled experimental until its USB
  hardware gate passes.
