# NiBiRu: Age of Secrets on iPod 6G — feasibility gate

## Current decision

Do not expose a NiBiRu icon in Desktop Mode yet. A credible engine route now
exists, the first retail room process runs on Rockbox, and its original 3D
character scene renders correctly. The authentic New Game region, handler,
resource initialization, and first screen transition now run too, but broader
room interaction and the complete inventory/use matrix are not complete. The
initial retail inventory and bounded save/load path now work, but a
non-working launcher would
misrepresent support.

The original game is a Windows title. Scaling its 2D output to the 320×240
Rockbox canvas would reduce rendering and video cost, but it would not make the
x86/Win32 executable run on the iPod's ARM Rockbox environment. The correct
route is an asset-compatible AGDS runtime that reads data from the user's own
installation.

Current ScummVM `master` contains an experimental `engines/agds` runtime and
labels English and Russian NiBiRu data as AGDS 2.511 and unstable. The owned
North American executable identifies itself as AGDS 2.509, however, and its
dispatch loop uses opcode base 2217. Treating this edition as 2.511 produces
invalid instructions. This engine landed after the old ScummVM 1.9.0 slice bundled in this
Rockbox tree, so it cannot simply be enabled: its required modern ScummVM
common/runtime APIs must be backported or its AGDS behavior adapted to the
existing small Rockbox backend.

The useful data split is now known:

- `agds.cfg` selects the `gfx*.grp` archives and original video mode.
- `data.adb` stores named AGDS game/script records.
- `gfx*.grp` stores named BMP/PCX/FLC visual resources and OGG/WAV audio.
- The owned North American edition requests 1024x768x32 in `agds.cfg`.
  Scaling to 320x240 is an exact 5:16 reduction on both axes, preserving 4:3.
  The iPod
  renderer should scale during decode and retain only bounded RGB565 surfaces.
- Gameplay remains script-driven. AGDS objects, globals, regions, handlers,
  inventory, transitions, and saves are runtime state; they are not converted
  into a video or a list of prerecorded game frames.
- Original skeletal 3D cutscenes use a separate iPod optimization: the personal
  installer rasterizes their owned meshes, animation tracks, textures, and
  cameras into sparse 320x240 RGB565 deltas. This replaces only the expensive
  Direct3D draw stage and never consumes a screen recording.

## Implemented retail-data bootstrap

`tools/nibiru_ipod_qualify.py` accepts a user-owned ISO or installed folder and
emits a read-only JSON inventory. For an ISO it records the SHA-256 and uses 7z
in list-only mode. For an installed folder it now validates bounded ADB and GRP
indexes, encryption markers, member bounds, first-5000-byte signatures,
2.509 `main` metadata, configured archive order, and resource extension counts.
It extracts nothing and copies no game data.

The two owned North American discs were extracted into the ignored private
asset area. No game data is committed or included in a firmware package.

`tools/nibiru_extract_owned_assets.py` performs bounded, name-based extraction
from those owned ADB/GRP archives. Its first-cutscene preset copies the exact
room scripts/descriptors, room layers, Martin model, three speaking
animations, hand and telephone meshes, two 512x512 texture atlases, ambient
track, dialogue, and scene effects into
the ignored private asset tree with a SHA-256 manifest. It does not derive any
asset from a screen capture. `tools/nibiru_model_probe.py` validates the direct
model data: 73 named skeleton tracks, six setup/bind matrices per track, 310
intro frames, and 31 frames in each speaking animation.

`tools/nibiru_build_owned_scene.py` renders those resources with the original
room occlusion and camera transform, interpolates the authored 150 ms skeletal
keys at an exact 24 fps clock, caches each reproducible source pose, and packs
sparse absolute deltas into `intro1864.nbs`. Martin, the chair, and the handset
all use their own retail animation tracks; a build invariant rejects a static
chair or mismatched root travel. Skinning follows the retail loop's four
weights, per-vertex influence count, and direct uint32 bone indices rather
than interpreting those indices as bit flags. Additional invariants reject
torn triangles, changed seated scale/anchor, wrong chair-roll direction, an
empty opening pose, or geometry below room 1864's authored y=696 scissor.
The game still advances the cutscene through
its AGDS process and phase globals. The stream is therefore a bounded
replacement for real-time 3D rasterization, not a replacement for game logic.
True Ogg resources used by the scene are transcoded from the owned files to
private 44.1 kHz PCM sidecars; RIFF resources stream directly from the GRP
archives.

`apps/plugins/scummvm/agds_loader.c` implements the same bounded metadata gate
on Rockbox without allocations. It can now resolve and stream named ADB and
GRP members, including encrypted GRP indexes. The native bootstrap validates
the AGDS `main` object, decrypts ADB text indirection, and decodes either BMP
or PCX directly from the archive
into a 320x240 RGB565 framebuffer. A 1024x768 intermediate image is never
allocated.

`apps/plugins/scummvm/agds_vm.c` validates bounded 2.509 object code, recovers
opcode base 2217, validates `Enter`, and bounds-checks object string tables.
Its bounded process scheduler executes the first room's object tree, suspended
processes, globals, branches, object loads, positions, and animation/sample
phase commands. The Martin process remains authoritative for cutscene timing;
the direct scene renderer publishes its phases back to the VM. AGDS sample
load/restart/stop opcodes feed an eight-voice bounded PCM mixer on Rockbox's
playback channel, with clean mixer-frequency restoration on exit.
The same VM now parses the retail polygon-region format, runs mouse-area
enter/leave objects, records embedded look/use handlers, supports cloned
objects, and preserves globals across `SetNextScreen` transitions. New Game is
no longer a coordinate shortcut: `1009.1011.new` runs its original `OnUse`
block, starts `1009.1067`, executes `107a` resource/inventory initialization,
and requests screen `1864` through opcode 79.
The inventory bootstrap runs its retail `inv.1090`/`inv.18f6` objects and
retains the six authored atlas descriptors. Inventory objects execute in their
own bounded VM contexts rather than becoming room sprites; their original
object IDs, 83x72 tile metadata, look/use handlers, and keyed `OnUseObject`
tables survive save/load. The runtime reveals the authored y=696 strip and
draws the real `invsipkaL.bmp`/`invsipkaP.bmp` controls plus atlas tiles at the
exact 5:16 output scale. The initial wallet and cellphone therefore come from
`inv_spolecne.bmp`, not replacement art.
The bootstrap follows the retail `main` sequence: `main.1008` string index 7
resolves through ADB text to `TACblack.bmp`, string index 6 resolves to
`loga_uvod.bmp`, and `1009.1011.1012` resolves to `menu_pozadi.bmp`. The two
logos advance in retail order or on click, then the exact 5:16 coordinate
transform activates the original main-menu hit regions. It no longer
stretches an arbitrary first archive member.

`tools/nibiru_sim_gate.py` builds synthetic, non-game ADB/GRP fixtures and
launches the real simulator plugin. Both the 1024x768 BMP and PCX paths are
verified as four exact 320x240 color regions. A retail mode mounts the private
installation by symlink and checks eight visible pixels from the authentic
Adventure Company startup logo. The runtime then advances through the
Unknown Identity/Future Games logo to the authentic main menu. This proves
archive lookup, encrypted index handling, ADB entry lookup, image decode,
exact 5:16 scaling, object/string parsing, retail bootstrap ordering, menu
hit-region mapping, and real-data decode. The interaction mode also moves the
simulated pointer to New Game, verifies the retail selector at its script
position, clicks it, and verifies room 1864's backdrop, keyed foreground, and
model-derived character pixels. The scene gate also verifies that the Martin
process queues the original telephone sample through AGDS bytecode; an extended
audio gate waits for the script-timed ringing and dialogue events. It does not
yet prove every inventory combination or every later room. A focused inventory
gate opens the bottom strip and requires the two exact initial assets. The
extended progression gate completes the 1,524-frame opening, follows the
retail transition/dialog chain through rooms 10bf, 10e0, 10fb, 1352, 10f9 and
10e6, and dispatches a real room interaction with no pending-opcode failure.
A separate save gate runs the retail save/load opcodes and requires globals,
scene state, initial inventory, object metadata, and handlers to rebuild.

## Go/no-go sequence

1. Inventory a personally owned English disc and identify executables,
   archives, scripts, video, audio, and image formats. Done.
2. Compare the exact edition against upstream AGDS detection hashes and record
   its config, ADB version, GRP encryption, resource types, and video mode.
   Done; this is an unlisted 2.509 data revision with a matching English
   `gfx1.grp` prefix signature.
3. Measure the AGDS code, heap, decoded frame, and audio mixer
   requirements against the 64 MiB iPod 6G target and Rockbox plugin buffer.
4. Prototype one room with 320×240 output, nearest-neighbour pointer mapping,
   bounded caches, and no ownership of Rockbox playback memory. The bounded
   bootstrap, interactive menu selection, first-room process tree, static room
   composition, direct model rendering, and initial sample event path are done;
   retail polygon hit testing, click-handler dispatch, and the authentic first
   screen transition are also done. The exact initial inventory UI and bounded
   save/load round-trip are done; broader room actions and the complete item-use
   matrix remain.
5. Only after the prototype reaches interactive input, scene transitions, and
   save/load should Desktop Mode advertise the game.

## Personal-data boundary

The owned ISOs and extracted copyrighted data remain in the ignored private
asset area and must stay outside the repository and deploy packages. The port
contains only independently implemented readers/runtime behavior. A personal
installer may copy or transcode data only from the user's owned installation
into the private iPod game directory.

Official references:

- https://github.com/scummvm/scummvm/tree/master/engines/agds
- https://github.com/scummvm/scummvm/blob/master/engines/agds/detection_tables.h
