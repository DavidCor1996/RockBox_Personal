# Flash runtime port: C++ plugin support vs C conversion

## Goal

Run Flash 6 / AVM1 games such as Stick RPG on iPod 6G-class Rockbox hardware.

The Stick RPG SWF from Flashpoint is favorable for an old Flash target:

- SWF version: 6
- VM: AVM1 / ActionScript 1/2 bytecode
- `DoABC`: none
- video tags: none
- stage: 550x400
- timeline/action load: hundreds of `DoAction`, `DefineSprite`, shape, text, button,
  sound, and display-list tags

That makes a real AVM1 runtime necessary. A tiny tag player is not enough.

## Candidate runtime

`gameswf` is the best first runtime candidate:

- Source found at `https://github.com/marmalade/gameswf`
- Original project is public domain
- C++ codebase with AVM1, sprite timeline, text, shape, sound, and renderer interfaces
- Existing backend split around `gameswf::render_handler`

The critical backend contract is:

- load SWF through a file opener callback
- drive `root::notify_mouse_state`
- drive `root::advance`
- call `root::display`
- provide a Rockbox software `render_handler`

## C++ plugin support path

This repo now has a minimal C++ plugin build path:

- `.cpp` entries in `apps/plugins/SOURCES` can produce `.rock` plugins
- `.cpp` entries in `apps/plugins/lib/SOURCES` can be archived into `libplugin.a`
- C++ compile flags disable exceptions, RTTI, thread-safe statics, and unwind tables
- `HAVE_PLUGIN_CXX` is defined only when a matching `g++` driver exists
- `apps/plugins/lib/cxx_support.cpp` provides global `new/delete` using Rockbox `buflib`
- `apps/plugins/cxxprobe.cpp` is a small C++ plugin probe

Current toolchain result:

- Simulator: `/usr/bin/g++` exists, so C++ plugins can build
- Hardware: `/usr/local/bin/arm-elf-eabi-g++` is missing, and
  `/usr/local/bin/arm-elf-eabi-gcc` reports `language c++ not recognized`
- Hardware fallback: `arm-none-eabi-g++` can compile plugin C++ objects when paired
  with the Rockbox GCC 9 include-fixed path; this is now auto-selected by
  `apps/plugins/plugins.make`

So the build system side is easier than source conversion. A matching Rockbox
`arm-elf-eabi-g++` would still be cleaner, but it is no longer required for the
current Flash bring-up plugin.

## Implemented bring-up plugin

`apps/plugins/flashplayer/flashplayer.cpp` is the official SWF viewer target.

Current behavior:

- owns the `.swf` viewer association
- opens the real SWF passed by Rockbox
- inflates compressed `CWS` files using Rockbox's plugin-side tiny zlib inflater
- parses the SWF RECT, framerate, frame count, and nested tags
- reports AVM1/timeline workload from the actual file
- explicitly does not present a native remake as Stick RPG

This is the first real Flash port milestone: file load, decompression, and SWF
structure parsing on iPod hardware.

## C conversion path

Converting gameswf to C is not recommended.

The runtime uses C++ classes throughout the parser, timeline, display list, values,
ActionScript objects, garbage/ref-counting helpers, render backend, text, and shape
systems. A C conversion would be a large rewrite before any Flash behavior improves.

Expected risk:

- high regression rate in AVM1 object semantics
- high rewrite cost before first frame
- no benefit for simulator validation
- harder to track upstream gameswf/Gnash fixes

## Recommended implementation plan

1. Keep C++ plugins freestanding:
   - no exceptions
   - no RTTI
   - no libstdc++
   - no global constructors for runtime state
2. Import a tiny subset of gameswf into `apps/plugins/flashplayer`.
3. Implement the Rockbox gameswf backend:
   - file opener using `rb->open/read/close`
   - software shape rasterizer path for filled triangle lists and line strips
   - bitmap upload as in-memory decoded buffers
   - mouse translation from clickwheel cursor
   - optional sound sink later
4. Next milestone:
   - execute enough AVM1 timeline setup to reach the first stable frame
   - render first frame or menu background through a software renderer
5. Following milestone:
   - AVM1 frame advance
   - button hit testing
   - text field rendering
   - save data through Flash shared-object shim

## Implementation spec: Stick RPG bring-up gates

The real Stick RPG port is the `flashplayer` SWF viewer/runtime path, not the
native `stickrpg.c` proof plugin. Treat the native plugin as a separate toy
until the SWF runtime reaches playability.

Use the staged Stick RPG SWF as the main target and keep a smaller Stick RPG
variant available for regression and faster parser/runtime checks. Every gate
below should be proven first in the simulator, then on iPod 6G/7G hardware.

### Gate 1: deterministic runtime facts

The plugin must write a compact runtime log to:

```text
/.rockbox/flash/flashplayer.log
```

The log should include:

- selected SWF path, SWF version, compressed and decompressed sizes
- stage size, frame rate, frame count, and tag workload
- plugin buffer, shared audio buffer, C++ heap start/size, and heap free space
- gameswf loader hits/misses, last tag, last stream position
- first runtime frame, display counters, triangle budget hits, and log errors
- mouse position/button state when input is delivered

This is the first implementation gate because iPod hardware debugging needs a
stable artifact after a failed frame, crash, or bad render.

### Gate 2: memory ownership

SWF storage and the C++ allocator must not overlap.

The loader may store inflated `FWS` bytes in the shared audio buffer when that
buffer is large enough. In that case, the C++ heap may only use the aligned tail
after the `FWS` image. If the `FWS` image is in the normal plugin buffer, the
heap may only use the aligned tail after all loaded SWF bytes. Initializing the
C++ heap over `g.raw` from offset zero is not valid once the file has been read.

The plugin currently uses the shared audio buffer only as memory. Sound support
must remain deferred until visual/input playability is stable. Any later audio
work must follow `docs/plugin-audio-lifecycle-steering.md`.

Pass criteria:

- repeated load/start/exit in the simulator does not corrupt the SWF data
- log shows one selected heap range with nonzero free space
- normal and USB exits tear down runtime objects before releasing the shared
  audio buffer

### Gate 3: first frame without Stick RPG-specific hiding

The renderer must produce the first stable Stick RPG menu/background without
removing or hiding named movie clips such as `filmscreen` or `pregameMC`.
Those hacks are useful only as temporary diagnostics.

Renderer blockers to validate before performance work:

- solid fills, alpha blending, and color transforms
- line strips with sufficient width for UI strokes
- bitmap fills and direct bitmap draws
- JPEG and lossless bitmap decode paths
- mask/stencil calls, or a logged proof that this SWF does not depend on them

Pass criteria:

- first frame is recognizable in simulator
- no named-object suppression is required
- log shows renderer counters for triangles, lines, bitmaps, masks, and budget
  caps

### Gate 4: mouse and button semantics

Settle the coordinate contract with a tiny SWF probe before tuning Stick RPG
controls. `gameswf::root::notify_mouse_state()` should receive the coordinate
space that its hit-test code expects; do not rely on visual cursor position
alone.

Pass criteria:

- a probe button logs roll-over, press, and release at the visible cursor
  position
- Stick RPG menu buttons respond to Select through the normal SWF event path

### Gate 5: AVM1/timeline correctness

Do not broadly rewrite gameswf. Cover the opcodes and event paths observed in
Stick RPG first: variables, members, comparisons, frame jumps, function calls,
method calls, object construction, random/time, movie clip properties, and
button actions.

Pass criteria:

- the root timeline advances to the expected menu frame
- button/movie-clip events fire in logs
- no missing core AVM1 method blocks menu navigation

### Gate 6: text and shared saves

Stick RPG relies heavily on text and editable text. Text rendering is not a
polish item; stats, dialogs, menus, and save/load UI require it.

SharedObject persistence can be Rockbox-native at first. Store deterministic
serialized save data below the Stick RPG flash directory rather than attempting
full Adobe `.sol` compatibility unless a real compatibility need appears.

Pass criteria:

- menu/stat text is readable and variable-bound text updates
- save/load survives plugin exit and restart

### Gate 7: audio and hardware performance

Only after the game is visually navigable should sound be implemented. Stick RPG
contains both MP3 and ADPCM event sounds. Start with a no-op sound handler that
does not block ActionScript, then add a Rockbox-owned event sound path.

Pass criteria:

- simulator visual/input/save path works before sound is enabled
- real iPod tests cover Database music -> plugin, Files music -> plugin, plugin
  exit -> Database/Files music, rapid switching, volume, and pause/menu exits

## Why not Ruffle first

Ruffle is the stronger modern Flash project, but it is Rust and much larger. On this
Rockbox target, gameswf is a better first port because it is smaller, older-game focused,
and already designed around a replaceable renderer.
