# RockAchievements Xbox 360 Avatar Page Specification

Status: implemented; hardware redeploy pending mounted target

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

## RockPod Avatar Creator

### Location

Add `Achievements & Avatar` under Rockbox in the sidebar and link to the same
creator from Game Sync. RetroAchievements credentials stay in Game Sync.

### Layout

- Left: profile name, XNA Boy/XNA Girl body preset, authentic animation picker,
  and editable Skin & Face, Hair, Top, Bottom, and Shoes material palettes.
- Center: a hardware-accelerated Qt Quick 3D viewport using the real converted
  XNA mesh, with mouse drag for unrestricted rotation; a toggle retains the
  high-resolution motion preview.
- Right: the exact iPod gamercard and 320x240 export preview.
- Footer actions: Randomize Preset, Reset, Play/Pause Preview, Save Profile,
  and Build & Sync to Target.

Both previews use frames rendered from the actual XNA rig, textures, and
animation curves at the device export cadence. Appearance editing selects
semantic material regions from those real source pixels and changes color while
preserving their original shading, silhouette, transparency, pose, and texture
edges. It does not redraw or replace any character part. The editor never
advertises unavailable headwear, clothes meshes, or accessories as editable.

Each Rockbox target profile stores one active offline avatar. Display names are
limited to 15 visible characters to retain Xbox 360 gamercard proportions.

### Authentic asset setup

The installed personal asset pack records one of three provenance classes:

1. `official-download`: Microsoft/Xbox-hosted files, including the original
   Achievement Unlocked master.
2. `owned-import`: avatar/system assets imported by the user from their own
   Xbox 360 storage, backup, or already-installed Xbox Original Avatars data.
3. `archive-extracted`: real extracted Xbox 360 Marketplace items from an
   archival source such as The Models Resource.

This personal repository stores device-ready renders and original source sound
resources under `assets/ipodjs/sources/xbox360/avatar/`, beside source URLs,
SHA-256 values, transformation notes, and the Microsoft Permissive License.
The large raw FBX files remain in their cited archive.

A source is rejected if it is AI-generated, fan-remade, traced, lacks a usable
character/rig license for the user's personal pack, or cannot be tied to real
Xbox 360 content. Archived Marketplace props may extend an owned base rig but
must never be mislabeled as an official Microsoft download.

The verified Microsoft XNA Avatar Animation Pack supplies the original boy,
girl/sneakers, and girl/heels mesh configurations and all seven implemented
motions. It does not supply the full Xbox 360 Avatar Editor item catalog, so
unsupported customization is shown as fixed instead of being recreated.

### Normalized desktop pack

The normalized source set is:

```text
assets/ipodjs/sources/xbox360/avatar/
    build-manifest.json
    README.md
    Microsoft Permissive License.rtf
    master/xna-boy/{jump,throw,faint,sit-idle,punch,kick,walk,turntable}.rgba.png
    master/xna-girl/{jump,throw,faint,sit-idle,punch,kick,walk,turntable}.rgba.png
    master/xna-girl-heels/{jump,throw,faint,sit-idle,punch,kick,walk,turntable}.rgba.png
    faces/SOURCES.tsv
    faces/xna-boy/{left-eye,right-eye,eyebrow,mouth}.tga
    faces/xna-girl/{left-eye,right-eye,eyebrow,mouth}.tga
    AchievementUnlocked.master.mp3
    snd_channelup.xma
    snd_buttonselect.xma
    snd_buttonback.xma
```

The RGBA strips are 416x672 transparent textured renders directly derived from
the archived Microsoft FBX files and original Microsoft UV color maps. Compact
Qt Quick 3D `.mesh` conversions under `rockpod/ui/qml/xbox_avatar_models/`
retain the actual triangles and texture coordinates for RockPod's live view.
RockPod deterministically derives semantic material regions at preview/export
time. The large raw FBX files remain in the cited archive, while the original
license and exact transformation/provenance manifest stay with the derived
model and device-ready source renders.
The Maya scenes render facial expressions through separate `layeredTexture`
nodes that legacy FBX renderers omit. RockPod restores the genuine `BoyAnim`
and `GirlAnim` eye, eyebrow, and mouth texture layers before both desktop
preview and RAV2 encoding; it does not draw replacement facial features.

RockPod stores display name, body preset ID, favorite authentic clip, and five
normalized material palette IDs in the target profile. Widget state and
absolute asset paths are not stored. `Randomize Avatar` changes every editable
category; Reset returns every material to its untouched XNA source appearance.

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
  called by the existing Achievement sync service.
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

- Unit tests cover profile schema, bounds, CRC failure, RAV2 encode/decode, PCM
  bank construction, bundled real assets, target sync, and preference survival.
- Creator tests cover real-pack decode, profile switching, exact iPod preview,
  persistence signals, sidebar routing, and target generation.
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

The feature is complete when RockPod can build a provenance-valid real Xbox
360 avatar, preview its authentic rigged animation and exact iPod export,
atomically sync it, and the iPod can open it with Hold Menu, animate it, play
authentic optional sounds, and return without disturbing achievements, active
music, playback memory, or database state.
