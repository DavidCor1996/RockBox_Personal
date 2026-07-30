# RockAchievements Xbox 360 Avatar Page Specification

Status: implemented on the real XNA rig; hardware redeploy pending a
mounted target

## Purpose

Add an authentic Xbox 360 avatar/profile page to the offline Achievements
plugin. Holding Menu from any Achievements screen opens the page. RockPod owns
avatar creation, asset acquisition, preview, animation baking, and device sync;
the iPod displays a bounded pre-rendered version of the same character.

This remains a personal, offline feature. No Xbox account login, Xbox Live
write, marketplace purchase, or cloud avatar mutation is in scope.

## Authenticity Baseline

Xbox avatars arrived with the 2008 New Xbox Experience, so the avatar page uses
the 2008 NXE `My Xbox` profile language rather than pretending it existed in
the 2005 Blades dashboard. The existing Achievements browser keeps its
Blades/Guide styling. The transition between them should feel like opening the
Xbox Guide into the NXE profile channel.

Primary references:

- [Xbox Wire: New Xbox Experience avatar editor](https://news.xbox.com/en-us/2008/09/19/new-xbox-experience-a-look-at-avatars/)
- [Xbox Wire: avatar creation and Gamer Picture controls](https://news.xbox.com/en-us/2008/11/19/top-10-things-to-do-when-you-get-the-new-xbox-experience/)
- [Microsoft XNA AvatarRenderer documentation](https://learn.microsoft.com/en-us/previous-versions/windows/xna/dd940226%28v%3Dxnagamestudio.40%29)
- [Microsoft XNA standard animation presets](https://learn.microsoft.com/en-us/previous-versions/windows/xna/dd940206%28v%3Dxnagamestudio.40%29)
- [Xbox Wire: original master Achievement Unlocked sound](https://news.xbox.com/en-us/2011/10/07/download-the-achievement-unlocked-sound/)
- [The Models Resource: archived Xbox 360 Avatar Marketplace models](https://models.spriters-resource.com/xbox_360/avatarmarketplace/)

The Xbox Wire sound is explicitly offered for non-commercial personal use and
is the canonical achievement notification source. The Models Resource contains
real extracted Marketplace costumes and props, but it is an archival source,
not an official Microsoft distribution. RockPod must identify that distinction
in asset provenance.

No generated, hand-drawn, traced, or stylistically similar substitute character
art is accepted. Native rectangles, gradients, text, masks, and transition
geometry are UI, not pictorial assets, and remain allowed.

Every character pixel is rasterized from Microsoft's own meshes, UV channels,
texture maps, skin weights, and animation curves. Customization changes which
real mesh parts are worn and tints the original colour maps; it never redraws,
reshapes, or substitutes a part.

## User Experience

### Entry and return

- Hold Menu on Games, Console Filter, Achievement List, or Achievement Detail
  to open `SCREEN_AVATAR`.
- Use the existing iPod keymap's `ACTION_STD_QUICKSCREEN` Menu-repeat action;
  do not invent a second timing threshold.
- Consume the following Menu release so it cannot immediately close the page.
- Remember the exact originating screen, selection, filter, and artwork owner.
- Short Menu returns to that exact state with no catalog reload.
- USB, Hold, shutdown, and default system events remain observable throughout.

### Avatar page composition

The 320x240 page is an NXE profile channel:

- Silver Xbox header with the real Xbox sphere and `My Xbox` title.
- Left gamercard, approximately 122 pixels wide:
  - offline display name;
  - total Gamerscore;
  - unlocked/total achievements;
  - games with achievement sets;
  - current console filter, if one is active.
- Right avatar stage, approximately 178x178 pixels:
  - pale NXE floor and green background sweep;
  - full-body real avatar with transparency;
  - a soft native-code floor shadow, not a bitmap illustration.
- Bottom stock-iPod legend: `WHEEL Rotate   SELECT Emote   MENU Back`.
- If no avatar pack exists, show an explicit `Create or sync an avatar in
  RockPod` card with the official Xbox sphere. Never silently substitute a
  generic illustrated person.

### Avatar interaction

| Input | Avatar page behavior |
| --- | --- |
| Wheel | Rotate the real model through a full 360 degrees |
| Select | Play the favorite authentic animation |
| Hold Select | Open local Avatar Settings |
| Play/Pause | Pause/resume model auto-rotation or the active emote |
| Menu | Return to the originating Achievements screen |
| Hold Menu | No second action; remain on the avatar page |

Avatar Settings contains `Motion: Full / Reduced / Off`, `Sounds: Full /
Achievement only / Off`, `Sounds over music: On / Off`, and `Idle emotes: On /
Off`. These are device-local preferences and must survive RockPod catalog
regeneration.

Appearance is chosen in RockPod, not on the device. The iPod plugin reads only
the display name, favourite clip, and gamerscore totals from `profile.v1.tsv`,
so widening the wardrobe never changes device code or device budgets.

### Animation behavior

Use only animations derived from the real Xbox 360 avatar system. The initial
required set is the genuine motion data present in Microsoft's XNA Avatar
Animation Pack 4.0: `Jump`, `Throw`, `Faint`, `SitIdle`, `Punch`, `Kick`, and
`Walk`. The model stage also has a 24-angle turntable rendered from the same
geometry. The app does not invent Wave, Clap, Celebrate, or Laugh clips.

With Idle Emotes on, the selected authentic clip loops. With it off, the clip
holds its last frame until Select replays it. Animation time is calculated from
elapsed Rockbox ticks; missed display frames never delay input.

The whole Achievements plugin uses authentic navigation/select/back sounds.
Avatar entry adds a 200 ms NXE-style stage slide followed by the selected clip;
other achievement screens keep immediate stock-iPod navigation. Achievement
unlock toasts remain explicitly deferred to the follow-up notification system.

The authentic export cadence is 8 fps on iPod and in RockPod's exact preview.
Reduced Motion halves the clip cadence; Motion Off keeps a static frame.

## Real Rig Pack

`tools/xbox_avatar_rigpack.py` reads Microsoft's binary FBX 6100 scenes from
`AvatarAnimPack_4_0_FBX` and writes a compact pack to
`assets/ipodjs/sources/xbox360/avatar/rig/`:

```text
rig/
    rig.json          part, channel, texture, material and clip manifest
    geometry.npz      per part: triangles, every original UV set, per-polygon
                      texture and material bindings
    textures/*.png    the Microsoft colour, intensity, decal and facial maps
                      embedded in the scene, keyed by their original filenames
    clips/<clip>.npz  per part, per frame, baked skinned vertex positions
```

The extractor evaluates Microsoft's own node hierarchy, animation curves, skin
clusters, and inverse bind matrices, then bakes world-space vertex positions at
the device cadence. The iPod never sees any of this; RockPod renders from it.

Extracted real parts:

| Slot | Real meshes |
| --- | --- |
| Body | `boy-body`, `girl-body`, `girl-body-heelleg` |
| Head | `boy-head`, `girl-head` (blend-shape ear default) |
| Hair | `boy-hair`, `girl-hair` |
| Top | `boy-top` (item 0599-0), `girl-top` (item 0320-0) |
| Bottom | `boy-bottoms` (item 0010-1), `girl-bottoms` (item 0687-0) |
| Shoes | `boy-shoes` (item 0468-0), `girl-shoes` (item 0334-0), `girl-highheels` |

Replacing the previous pre-rendered 416x672 sprite strips with the rig pack
also removed 23 MB of derived images from the tree.

### Authentic asset setup

The installed personal asset pack records one of three provenance classes:

1. `official-download`: Microsoft/Xbox-hosted files, including the original
   Achievement Unlocked master and the Ms-PL XNA Avatar Animation Pack.
2. `owned-import`: avatar/system assets imported by the user from their own
   Xbox 360 storage, backup, or installed Xbox Original Avatars data.
3. `archive-extracted`: real extracted Xbox 360 Marketplace items from an
   archival source such as The Models Resource. The imported item pack records
   this class, and every item keeps its source page and archive SHA-256.

`rig.json` records the source archive, the Ms-PL licence, and an explicit
`hand_drawn_or_generated_geometry: false` flag. The original licence text and
sound resources stay beside it under
`assets/ipodjs/sources/xbox360/avatar/`. The ~10 MB raw FBX files remain in
their cited archive; the rig pack is a derived form of them.

A source is rejected if it is AI-generated, fan-remade, traced, lacks a usable
character/rig licence for the user's personal pack, or cannot be tied to real
Xbox 360 content. Archived Marketplace props may extend an owned base rig but
must never be mislabeled as an official Microsoft download.

The licensed base pack supplies no additional headwear or accessory meshes, so
those categories stay unavailable rather than being recreated.

## Marketplace Item Pack

`tools/xbox_avatar_marketplace_fetch.py` caches the archived Xbox 360 Avatar
Marketplace models preserved by The Models Resource, and
`tools/xbox_avatar_marketplace.py` fits them onto the real rig into
`assets/ipodjs/sources/xbox360/avatar/marketplace/`. These carry
`archive-extracted` provenance, never `official-download`.

The archive holds 166 items. Everything wearable is imported; pets, vehicles,
scenery, and toys are recorded as unsupported with a reason rather than being
placed by guesswork, as are the few archives that ship only COLLADA.

| Slot | Imported | Attachment |
| --- | ---: | --- |
| Headwear (hats, helmets, masks, hoods) | 26 (8 exact) | rigid, head bone |
| Costumes | 31 | skinned |
| Tops | 3 | skinned |
| Held props | 9 | rigid, right-hand bone |

### Placement

None of the archives contain the avatar body, so there is no reference frame in
the file to align against. What each capture does or does not preserve decides
how faithfully it can be placed, and every item records which case it is:

- `placement: exact` — the vertex buffer was dumped while the item sat at head
  height above the avatar's own origin, so it still carries the real Xbox
  placement and already shares this rig's lateral origin. These are reproduced
  with a single uniform scale and **no translation**; moving them would discard
  the placement being reproduced. Eight headwear items qualify.
- `placement: fitted` — the item was exported as a standalone model resting on
  its own origin, so the archive simply does not record where it sat. These are
  fitted to the matching real part: helmets, masks, and mascot heads are sized
  to the head and made concentric with it, hats are sized across the brim and
  sat on the crown, and held props are sized against the avatar and carried by
  the right-hand bone.

Fitted placement is an approximation and is not claimed to match the Xbox
exactly. Making more items exact needs either a capture source that preserves
avatar-space coordinates or a per-item offset recorded by hand.

Rigid items are expressed once in their bone's space and then follow that bone
exactly. Worn items are recorded in the same T-pose the XNA rig binds in, so
each imported vertex takes an inverse-distance blend of the bone weights of its
nearest real avatar vertices and is then deformed by Microsoft's own skin
matrices. A chosen costume hides the ordinary top, bottom, and shoes, exactly
as it does on a real Xbox 360 avatar.

Nothing is redrawn, reshaped, or retextured; each item keeps its original mesh
and its own extracted texture maps.

## Renderer

`rockpod/services/xbox_avatar_render.py` rasterizes the real parts through the
authentic Xbox avatar material model:

- a depth-buffered rasterizer with back-face culling, batched by triangle
  bounding-box size so a full frame costs a handful of array operations;
- the item's original `ColorMap` as the base, tinted by the chosen swatch so
  the source weave, seams, shading, and silhouette survive;
- the item's own `DecalMap` where Microsoft supplied one;
- the head composed from its six real per-feature channels
  (`SkinFeatures`, `FacialHair`, `EyeShadow`, `Mouth`, `Eye`, `EyeBrow`), each
  bound per polygon to the correct left or right texture with clamped UVs.

Batching resolves the nearest candidate per pixel before any texture work, so
it is pixel-identical to a plain per-triangle rasterizer; a full eight-clip
device export takes about 7 seconds and one creator angle about 70 ms.

### Face placement

The face is the head's own UV mapping. Eyes, brows, and mouth are sampled
through `__EyeIntensityMap`, `__EyeBrowIntensityMap`, and `__MouthIntensityMap`
on the head mesh, so they land exactly where Microsoft mapped them in every
pose and at every turntable angle, and they disappear correctly when the head
turns away.

This replaces the previous approach, which detected a skin island in the
rendered 2D sprite and pasted scaled facial textures at fixed fractions of its
bounding box. That estimate drifted off the head whenever the pose, head tilt,
or rotation moved the silhouette, which is the defect this revision fixes.

### Fitting rule

Microsoft cut each garment for one body. Hair sits above the shoulders and
swaps freely, and the girl body is the narrower of the two, so it wears either
wardrobe. The broader boy body pushes through the girl-fitted top, and the
heels require the girl heel-leg body, so those two stay family-bound and are
not offered on the boy body. `garment_clipping()` measures body pixels that win
the depth test through a worn top and gates this in tests.

Selecting heels automatically swaps in Microsoft's heel-leg body, which is the
leg shape that pose was authored against.

## RockPod Avatar Creator

### Location

`Achievements & Avatar` under Rockbox in the sidebar, also reachable from Game
Sync. RetroAchievements credentials stay in Game Sync.

### Layout

- Left: display name, body preset, favourite animation, and the wardrobe and
  colour pickers below.
- Centre: a live model view rendered by the export renderer itself, dragged
  through all 24 real turntable angles, with a toggle for the motion preview.
- Right: the exact iPod gamercard and 320x240 export preview.
- Footer: Randomize Avatar, Reset, Play/Pause Preview, Save Profile, and
  Build & Sync to Target.

The centre view is rendered by the same code that produces the device clips, so
the creator preview and the export cannot diverge.

### Editable appearance

| Field | Choices |
| --- | --- |
| Body | XNA Boy, XNA Girl, XNA Girl (heels) |
| Hair style | Original, Short crop, Bob, Shaved |
| Top | Original, Crew tee, Scoop tee (girl body), No top |
| Bottom | Original, Jeans, Shorts |
| Shoes | Original, Sneakers, Flats, Heels (girl body), Barefoot |
| Skin tone | Original plus six real tints |
| Hair colour | Original plus seven |
| Top / bottom / shoe colour | Original plus five to eight each |
| Eye, eyebrow, lip colour | Original plus four to five each |
| Chest print | See below |

`Original XNA` always leaves that material untouched. Eye and lip colours tint
only the pixels Microsoft's own textures already colour, so the sclera and
teeth stay white. Facial hair and eye shadow are real channels in the pack but
Microsoft shipped them empty, so they are not advertised as editable.

Display names remain limited to 15 visible characters to retain Xbox 360
gamercard proportions, and each target profile stores one active avatar.

### Chest prints

The Xbox garment shader carries a decal layer, and RockPod fills it with real
artwork already in this repository rather than anything drawn for the feature:

| Print | Real source |
| --- | --- |
| Original XNA | the garment's own Microsoft decal map |
| Rockbox logo / icon | `apps/bitmaps/native/rockboxlogo`, `rockboxicon` |
| iPod Apple mark | `.rockbox/ipodjs/apple-logo-white` |
| iPod album art print | `.rockbox/ipodjs/default_album_artwork` |
| iPod play / volume glyph | `.rockbox/ipodjs/play`, `volume_full` |

Prints are projected onto the front of the real garment mesh from the standing
pose, so the graphic stays attached to the fabric through every frame and wraps
out of view as the avatar turns. Flat backgrounds are keyed out; the Rockbox
marks are amber badges by design and print full frame.

### Generated designs

Beyond the fixed colour swatches, each of the top, bottom, and shoes can take a
procedurally generated pattern. These are computed, not drawn: every design is
a closed-form function of the pixel grid, so one is fully described by a name,
a repeat size, two colours, and a seed.

Available designs: solid, stripes, pinstripe, diagonal, chevron, checks,
plaid, tartan, polka dots, halftone, rings, sunburst, waves, argyle,
houndstooth, camo, marble, fade, grid, triangles, and static, each at five
repeat sizes.

A design supplies the colour at each texel while the item's real Microsoft
colour map still supplies the weave, seams, and lighting, so a patterned
garment keeps reading as fabric and the silhouette never moves. The pattern is
drawn in the garment's own colour swatch against a shared accent colour.

### Stored profile

RockPod stores the display name, body preset, favourite clip, four style
choices, eight colour choices, three design and three design-size choices, the
accent colour, and the chest print. Widget state and absolute
asset paths are not stored. Profiles written before the rig landed stored one
palette name per clothing slot; those names still load as the matching colour.

`Randomize Avatar` changes every editable category; Reset returns every field
to its untouched XNA source appearance.

## Device Pack

RockPod atomically exports:

```text
/.rockbox/achievements/avatar/
    current
    generations/<generation>/profile.v1.tsv
    generations/<generation>/portrait.80x80x24.bmp
    generations/<generation>/clips/jump.rav
    generations/<generation>/clips/throw.rav
    generations/<generation>/clips/faint.rav
    generations/<generation>/clips/sit-idle.rav
    generations/<generation>/clips/punch.rav
    generations/<generation>/clips/kick.rav
    generations/<generation>/clips/walk.rav
    generations/<generation>/clips/turntable.rav
    generations/<generation>/sounds/ui-44100.uib
    generations/<generation>/sounds/ui-48000.uib
    generations/<generation>/sounds/unlock-44100.pcm
    generations/<generation>/sounds/unlock-48000.pcm
    generations/<generation>/manifest.v1.tsv
/.rockbox/achievements/state/avatar-preferences.v1.tsv
```

RockPod stages the complete immutable generation and deploys `current` last.
Device-local preferences live outside the generation and are seeded only when
missing, so later syncs never overwrite them.

### RAV2 clip format

`RAV2` is a little-endian, bounded, pre-rendered animation format:

- fixed 104x168 maximum avatar canvas;
- RGB565 color with a reserved transparent pixel value;
- delta-RLE and constant-color fill runs for motion clips;
- independent keyframes for all 24 turntable angles, permitting instant
  clockwise or counterclockwise wheel rotation;
- per-frame duration in milliseconds;
- frame index table for elapsed-time seeking and dropped-frame recovery;
- CRC-32 per clip plus SHA-256 in the manifest;
- maximum 24 frames;
- maximum 1 MiB compressed per loaded clip; the largest current textured
  turntable is approximately 824 KiB and ordinary motion clips stay below
  190 KiB.

RockPod renders the original 3D avatar to `RAV2`; the iPod never performs 3D
skinning, texture sampling, or arbitrary model parsing.

## iPod Runtime Architecture

### Screen state

Extend `apps/plugins/achievements.c` with:

- `SCREEN_AVATAR` and `SCREEN_AVATAR_SETTINGS`;
- a saved return screen and selection snapshot;
- an avatar manifest/profile reader;
- a bounded clip loader and RAV2 decoder;
- an elapsed-tick animation controller;
- a small optional UI sound controller.

Catalog, achievement, cover, badge, and avatar state remain separate. Entering
the avatar page must not reload the achievement catalog or disturb the current
cover/badge cache.

### Memory budget

No `core_alloc()`, `plugin_get_audio_buffer()`, full-screen transition copy, or
per-frame file decode is allowed.

Hard additional peak budget:

| Resource | Maximum |
| --- | ---: |
| Compressed current RAV2 clip | 1 MiB |
| One decoded RGB565 avatar frame | 40 KiB |
| Navigation/select/back PCM bank | 96 KiB |
| Manifest/profile and indexes | 8 KiB |
| Total additional peak | 1.14 MiB |

Before hardware approval, compare plugin BSS and load size before/after and
verify the total remains within the configured plugin buffer with the existing
large catalog arrays. The existing cover/badge cache remains untouched so
returning to the originating screen is immediate.

The complete active clip is loaded once during page-entry service after input
settles. Frames decode from memory into one fixed frame buffer. Draw functions
only paint cached pixels. Switching emotes may read one new clip as a direct
user action; the UI shows the standing frame until validation and loading are
complete.

### Animation loop

- Derive the desired frame from `current_tick - animation_start_tick`.
- Decode sequential motion deltas in memory to the elapsed-time destination;
  turntable angles decode directly from independent keyframes. Never delay
  input to display every missed frame.
- Redraw only from cached data; no file I/O occurs during paint.
- If input is queued, cancel decorative motion and draw the final state.
- Never sleep longer than one 10 fps interval, and use `get_action()` timeouts
  so system events remain observable.
- Never perform file I/O, image decode, or allocation from a draw function.

## Sound Design and Lifecycle

### Sound set

- Achievement unlock uses the Xbox Wire master, transcoded by RockPod without
  changing its timing or pitch.
- Navigation, select, back, and Guide-open sounds must come from the user's
  authentic imported system pack. If missing, omit them; do not synthesize or
  imitate them.
- Wheel navigation emits at most one sound per Rockbox action event.
- No ambient loop or dashboard music is added.

### Playback rules

Short UI effects use `PCM_MIXER_CHAN_BEEP`, not the playback channel. The
plugin does not take the shared audio buffer, stop music, change the playlist,
duck user audio, or change the global mixer frequency.

RockPod exports 44.1 kHz and 48 kHz signed 16-bit stereo PCM banks. The plugin
chooses the bank matching `mixer_get_frequency()`. At another active mixer rate,
the effect is skipped rather than changing the user's playback frequency.

Before every effect, stop only a stale BEEP-channel effect and install the
new bounded buffer. On page exit and plugin exit:

1. stop `PCM_MIXER_CHAN_BEEP`;
2. clear its buffer hook/callback;
3. wait until its status is stopped;
4. only then allow plugin memory to be released.

If the BEEP channel or authentic asset is unavailable, UI continues silently.
Sound failure never blocks input or animation.

## RockPod Sync Integration

- Avatar pack generation is a sibling transaction to achievement generation,
  called by the existing Achievement sync service. The generation hash covers
  the profile, the rig pack geometry and manifest, and the sound banks, so any
  wardrobe, colour, or print change produces a new immutable generation.
- Syncing newly added games updates gamercard totals without rebuilding avatar
  meshes or clips when the avatar profile hash is unchanged.
- Avatar profile changes rebuild only the avatar generation.
- RockPod's deployment diff reports add/overwrite/unchanged assets before sync.
- RAV2 decode validates dimensions, bounds and CRC-32; the generation manifest
  validates SHA-256 and byte size for every deployed file.
- Full package deployment continues to preserve Rockbox database files and
  device-local achievement/avatar state.

## Failure Behavior

- Missing avatar pack: static setup card, no default fake character.
- Missing or corrupt current generation: show the explicit RockPod setup card.
- Missing emote or bad frame CRC: leave the stage static and keep input live.
- Missing or incompatible sound: remain silent.
- The build fails approval if the fixed plugin image exceeds the target plugin
  buffer; runtime never borrows playback memory.
- Queued navigation: finish immediately at the destination state.
- USB connection: stop BEEP audio, close files, and return
  `PLUGIN_USB_CONNECTED`.

## Test and Acceptance Gates

### RockPod

- Unit tests cover profile schema, legacy palette migration, bounds, CRC
  failure, RAV2 encode/decode, PCM bank construction, target sync, generation
  hashing, and preference survival.
- Rig tests assert the pack declares Ms-PL provenance and
  `hand_drawn_or_generated_geometry: false`, that both heads expose Microsoft's
  dedicated eye, brow, and mouth UV channels, and that every worn part animates
  across a clip.
- A face-placement test renders the head with and without its facial layers and
  requires the difference to sit inside the head silhouette, cluster on the
  upper face rather than the neck, stay centred horizontally, and vanish when
  the head is turned to the back.
- A fitting test requires every offered top to keep body pixels showing through
  it at or below 6 percent, and records that the excluded boy/scoop-tee pairing
  exceeds 10 percent.
- Chest-print tests require every print to resolve to a real in-tree bitmap and
  to land on the chest band rather than the head or legs.
- Colour tests require a swatch to change pixels without moving the silhouette.
- Creator tests cover profile round-trip, persistence signals, and that only
  body-appropriate garments are offered.
- No test may fall back to generated placeholder character art.

### Simulator

- Hold Menu opens Avatar from every Achievements screen and does not trigger an
  immediate Menu-back on release.
- Menu returns to the exact screen, selection, filter, and artwork.
- The wheel traverses all 24 model angles in both directions without file I/O,
  Select plays the favorite authentic emote, Play/Pause freezes it, Hold Select
  opens settings, and two Menu presses restore the exact originating detail
  screen.
- Source-contract tests prevent I/O from draw paths and playback-memory use.

### Audio

- Source-contract and build tests require BEEP-only playback, forbid playback
  channel and mixer-rate changes, and require stop/callback-clear/status-wait
  teardown. Sound Off and `Sounds over music: Off` are runtime preferences.

### Physical iPod

- Audit plugin text/data/BSS and stack use before deployment.
- Deploy through the guarded iPod 6G package path, preserve every tagcache
  checksum, verify both firmware copies and the plugin checksum, validate the
  avatar manifest and achievement coverage, then sync/eject cleanly.

## Definition of Done

The feature is complete when RockPod can compose a provenance-valid real Xbox
360 avatar out of Microsoft's own mesh parts, with the face carried by the
head's own UV channels, preview its authentic rigged animation and exact iPod
export, atomically sync it, and the iPod can open it with Hold Menu, animate
it, play authentic optional sounds, and return without disturbing achievements,
active music, playback memory, or database state.
