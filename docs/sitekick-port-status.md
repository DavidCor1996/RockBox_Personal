# Sitekick for iPod — implementation status

Native offline Sitekick for the 320×240 colour iPod targets. It appears in
**Extras ▸ Applications** and uses preserved Sitekick Remastered art plus the
period 2003 YTV wordmark for this personal, non-commercial build.

## Implemented

- Authentic prefab placement: the asset builder walks Unity transform parent
  chains, sprite pivots, scale, flip and sorting order.
- Complete base Sitekick: shell, both eyes, antenna base, curved antenna stalk
  and knob are composed from `EditorSitekick.prefab`.
- A 240×192 transparent character stage with the body origin raised enough
  for complete arm/leg costume sets. The Dock fills the content area; menu
  selection is shown in the iPod title bar.
- Eight independent equip positions matching the original CHIPS inventory
  model. Any wearable chip can occupy any position. Whole-look chips and
  matching arm, leg and face chips can therefore be combined without the old
  heuristic body-part restrictions.
- Stable `SKS2` saves: owned and equipped chips are persisted by chip ID.
  Positional `SKS1` saves migrate on load. Dump state and cooldown are also
  persisted. The extended format also stores body-colour and background
  choices while remaining compatible with earlier 32- and 40-byte headers.
- Seven body-colour choices generated from the authentic Sitekick body sprite,
  preserving the original shading, eye, antenna and equipped-chip colours.
  Classic Yellow extends the original Classic, Slime, YTV Purple, Ooze
  Orange, Aqua and Hot Pink choices.
- Eight selectable stage backgrounds: six are sourced from the official Sitekick
  Remastered web repository: Sitekick Splash, Butterfly, Purple Gear, Blue
  Gear, Aqua Leaf and Ooze Grid. Oliver Scrapyard and Emma Neon Box are
  original YTV-style artist-era backgrounds generated from reference-only
  album palettes; no photographic pixels or cover text are shipped.
- Chip Dump: an unowned chip spawns by rarity (70% common, 25% rare,
  5% legendary), can be claimed once, grants rarity-scaled XP, and respawns
  after a 30–120 second cooldown.
- Collection, Trading Post inbox/outbox, stats, starter wearable chips,
  deterministic RockPod drops, redemption and trade settlement.
- Thirty-eight iPod-only chips across common, rare and legendary tiers. The set
  includes YTV/Sitekick interpretations of an iPod with original Sitekick
  hands and earbuds, Beatles-inspired hair/drum/crossing disc, Emma
  Blackery-inspired stage pieces, Rockbox badges, and Oliver Tree-inspired
  hair, glasses and jacket. Real-world photos and product art are retained as
  reference-only provenance; no realistic pixels are shipped in the chips.
  It also includes original fox/hound ear, tail and friendship-collar pieces
  plus Emma Blackery album-era clothes and hair: the short brown
  *Villains* bob/tee and *Girl in a Box* pigtails/striped sweater.
  The *Love You Madly Hate You Badly* Oliver Tree set adds long cover-era
  locks, a connected oversized white tee/loose jeans/work-boots costume with
  both sleeves and legs, and a mechanical scrap-heart aura.
  The *My Name Is Earl* set adds Earl's seated Camden tousle, compact
  moustache and connected plaid/tee/jeans outfit, Joy's high blonde ponytail,
  a hand-held karma list and an open-centre lucky-ticket/clover aura.
  Three paired gesture chips add peace signs, double thumbs-up and rock horns
  with inward wrists at the original arm points. The classic Oliver Tree
  Turbo set adds a seated bowl cut, compact red shades, and a connected
  blue/hot-pink windbreaker outfit with huge aqua jeans, white socks and red
  slides.
  Body costumes use transparent neck openings that reveal the original
  Sitekick head and render behind the body so no collar can cross the face,
  while hair overlaps the scalp/temples and handheld props use original
  Sitekick hands.
- Original preserved UI sounds on Rockbox's beep mixer channel, without
  taking playback memory or the playback PCM channel.
- Period YTV purple/yellow/orange/ooze-green interface and authentic
  transparent 2003 YTV wordmark.
- RockPod and the iPod Extras/Applications right pane compose the same saved
  loadout, colour and background. Split pane assets are replaced atomically,
  and the iPod refreshes its static preview cache on screen entry. The plugin
  republishes the complete layered character after every equip, unequip or
  body-colour change. It also republishes the pane template, including current
  XP and coin totals, after equipment, Dump rewards and background changes,
  so returning from Sitekick immediately shows the new state without waiting
  for a RockPod sync.
- RockPod's Sitekick screen includes a Secret Codes card that merges rewards
  into the mounted iPod's one-shot inbox without replacing a pending trade or
  timed drop. Codes are case-insensitive, skip owned/pending chips, and unlock
  when Sitekick next opens. `Oliver` grants all nine Oliver Tree costume chips;
  `Blackery` grants every Emma Blackery stage and album-era chip.

## Asset pack

Run:

```bash
rockpod/.venv/bin/python tools/sitekick_package_assets.py \
  --art-root <clone-of-github.com/SitekickRemastered/Art>
```

Current generated pack:

| Item | Value |
|---|---:|
| Catalogue entries | 419 |
| Wearable composites | 391 |
| Icon pages | 9 |
| Character stage | 240×192 |
| UI sound banks | 44.1 and 48 kHz |
| Installed size | about 13 MiB |

The BMP writer emits bottom-up 32-bit BI_RGB BGRA. Rockbox derives an alpha
plane from those files and the plugin draws them in `DRMODE_FG`, preventing
the white rectangles that previously appeared around helmets and other chips.
The stage is composed once per loadout change into a 92,160-byte static
framebuffer; drawing a frame only copies cached pixels.

Collection categories in `chips.v1.tsv` are footprint-derived browsing
metadata. They do not constrain equip positions.

## Verification — 2026-07-25

- Simulator build and link: passed.
- Simulator asset self-test: 419 catalogue entries, complete 240×192 base,
  391/391 wearable BMPs, 9/9 icon pages and UI sound bank loaded.
- Device-native preview export: matching arm, leg and face chips were composed
  in z/slot order and exported with a magenta transparency key for the
  animated Extras right pane.
- Live visual pass: complete base face/antenna, Viking helmet transparency,
  and the matching 317 arms + 318 legs + 319 face outfit all render correctly.
  Both feet and both arm pieces remain inside the stage.
- Fox/Hound and Emma album-era combination pass: ears, tail, collars, clothes
  and hair render as unified YTV costumes. The Hound band, *Villains* bob and
  *Girl in a Box* pigtails overlap the scalp with no purple/floating gap;
  true-alpha face and neck openings keep both eyes and the mouth unobstructed.
- *Love You Madly Hate You Badly* combination pass: the long-lock crown
  overlaps the scalp, both eyes and mouth remain visible, the collar stays
  behind the chin, sleeves/jean legs/boots form one outfit, and the mechanical
  heart remains fully behind the character.
- *My Name Is Earl* combination pass: Earl's hair overlaps the scalp, his
  moustache sits below the eye, the plaid collar leaves the complete face
  visible, both sleeves/jean legs/shoes connect naturally, the original
  Sitekick hand holds the karma list, and the lucky aura remains behind the
  whole character. Joy's ponytail uses a true-alpha face opening and seats
  directly on the head.
- Turbo/hand combination pass: the classic bowl cut overlaps the scalp, red
  shades remain inside the face, the yellow-lined collar stays below it, both
  windbreaker sleeves and huge jean legs/slides connect naturally, and peace,
  thumbs-up and rock-horns pairs attach at both wrists without crossing the
  face or body centre.
- Appearance pass: Classic Yellow preserves the authentic body shading and
  alpha, Oliver Scrapyard keeps the character clear against an aqua
  gear/scrap border, and Emma Neon Box keeps the character clear between
  red-left and green-right stage panels in both stage and right-pane crops.
- `Oliver` and `Blackery` code-only staging passed case-insensitive,
  already-owned and already-pending checks without replacing inbox rewards.
- Isolated `ipod6g` hardware plugin build: passed.
- Isolated `ipodvideo` hardware plugin build: passed.
- Focused RockPod/Sitekick device suite: 27 passed.
- iPodJS navigation regression: 499 trace records passed with active playback
  and stable track/playlist identity, including Applications and Quick
  Settings Sitekick coverage.
- `build-hw-ipod6g/rockbox.zip`: integrity test passed; plugin assets are at
  `.rockbox/sitekick`.
- Inbox and Dump persistence were exercised in isolated simulator roots.

Physical iPod 6G deployment passed the guarded database-preserving installer:
2,771 indexed tracks validated, 11 tag-cache files remained byte-identical,
and the root and `.rockbox` firmware copies matched the local build.

## Authored metadata limitation

The public preservation repository does not contain the retired server's chip
names or rarity table. Names remain `Chip NNNN` and rarity is deterministic
hash-derived unless curated through `data/overrides.v1.tsv`. This does not
affect art, placement, equipment, persistence or Dump behaviour.

## Main files

- `apps/plugins/sitekick.c`
- `tools/sitekick_package_assets.py`
- `assets/ipodjs/rockbox/sitekick/`
- `assets/ipodjs/sources/sitekick/`
- `rockpod/services/sitekick.py`
- `rockpod/scripts/sitekick_sync.py`
- `rockpod/tests/test_sitekick.py`
