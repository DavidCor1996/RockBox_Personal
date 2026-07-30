# Sitekick RockPod Media Chips and Minigames

## Scope

This extension turns the music and public video library indexed by RockPod
into a curated source for new Sitekick chips, while adding small native
Sitekick games that award persistent XP and coins.

The July 26, 2026 RockPod snapshot contains 2,766 audio tracks and 108 public
video entries. The first wave intentionally represents heavily populated or
visually distinctive library entries that are not already covered by the
Beatles, Emma Blackery, Oliver Tree, Fox and the Hound, My Name Is Earl,
iPod, or Rockbox series.

Private or locked media is never named, sampled, or used as a chip source.

## Visual contract

Every new chip must:

- look like an early-2000s YTV Sitekick/Ooze web-game asset: chunky dark
  purple outlines, flat cel shading, saturated slime green, purple, orange,
  yellow, cyan, and hot-pink accents;
- be an original cartoon interpretation of a media cue, not a photograph,
  album-cover crop, copied logo, or realistic portrait;
- ship with true alpha transparency and no white, magenta, or purple corner
  matte;
- conform to the Sitekick body at its packaged anchor, including a
  transparent head opening for hair and clothes and unobscured eyes/face;
- use the existing draw-order contract: aura behind the body, shell below
  facial layers, arms around the body, and hair/eyes above the body;
- remain inside the 240 by 192 stage and read naturally at the 320 by 240
  iPod screen size.

Source photos, show art, and album art are reference-only. Generated pieces
remain part of the asset pack alongside preserved original Sitekick assets.

## RockPod media wave 1

| ID | Series | Chip | Slot | Z | Rarity | Visual cue |
|---:|---|---|---|---:|---|---|
| 938 | The Wall | Wall Brick Halo | aura | -3 | Legendary | broken white brick ring and ooze cracks |
| 939 | The Wall | Prism Beam Shades | eyes | 5 | Rare | angular prism lenses with a rainbow beam |
| 940 | Hybrid Theory | Hybrid Mech Wings | arms | 2 | Legendary | compact stencil-like mechanical wings |
| 941 | Meteora | Meteora Spray Hoodie | shell | -1 | Rare | charcoal hoodie with cyan spray marks |
| 942 | Riot! | Riot Scribble Bob | hair | 4 | Common | orange-and-black energetic swept bob |
| 943 | Riot! | Riot Orange Jacket | shell | -1 | Rare | fitted orange punk jacket with scribbles |
| 944 | Sunnyvale | Sunnyvale Shades | eyes | 5 | Common | oversized square black trailer-park shades |
| 945 | Sunnyvale | Trailer Park Work Shirt | shell | -1 | Rare | green work shirt with checked undershirt |
| 946 | Recess | Playground Cap | hair | 4 | Common | backwards red playground cap |
| 947 | Recess | Recess Backpack | arms | 2 | Rare | backpack straps, side pack, and cartoon hands |
| 948 | 6teen | Mall Headphones | arms | 2 | Common | chunky headphones cupped by Sitekick hands |
| 949 | 6teen | Food Court Hoodie | shell | -1 | Legendary | purple/cyan mall hoodie with food-court badges |

All twelve chips participate in the existing rarity-weighted Chip Dump:
common 70 percent, rare 25 percent, and legendary 5 percent. They also appear
in RockPod collection/trade views automatically through `chips.v1.tsv`.

## Minigames

### Beat Bounce

Beat Bounce is a ten-beat timing game. A purple marker travels across a
horizontal ooze rail using elapsed Rockbox ticks. Press Select while it is
inside the yellow target zone. Hits build a combo and increase the score;
late beats and off-target presses break the combo.

- Controls: Select to hit; Menu/Left to leave without a reward.
- Completion reward: `8 + score` XP and `12 + (score * 2)` coins.
- Navigation and hit sounds use the existing Sitekick UI sound bank through
  the beep mixer channel.

### Chip Match

Chip Match is a two-by-three memory board containing three shuffled YTV-color
chip pairs. The click wheel moves the selection and Select flips a card.
Matched pairs remain visible.

- Controls: wheel or Up/Down to move; Select to flip; Menu/Left to leave.
- Completion reward: 18 XP and 30 coins, plus a speed bonus for finishing in
  fewer than twelve turns.

## Persistence and live sync

Minigames reuse the existing Sitekick save fields. A completed game updates
XP and coins, writes `save.v2.bin`, and republishes the right-pane background
so the new totals appear immediately. Cancelling never awards a partial
reward.

No high-score migration is required for this release. Score and match state
are session-only; owned chips, equipment, XP, coins, body color, background,
and Dump state retain their current persistent format.

## Playback and memory invariants

- Games allocate only small scalar and six-card arrays on the plugin stack.
- Animation is driven by `current_tick`; no decorative framebuffer is
  allocated and no asset is loaded during a game frame.
- Sitekick never takes the playback mixer channel, playback buffer, or
  codec memory. UI sounds remain on `PCM_MIXER_CHAN_BEEP`.
- USB connection exits through the normal plugin USB status path.

## Acceptance checks

- Package count increases from 419 to 431 chips and remains below the
  512-chip runtime limit.
- All twelve new BMPs retain alpha and have transparent corners.
- Hair and shell composites preserve a visible Sitekick head opening.
- The simulator loads every bitmap and icon page without failure.
- Beat Bounce and Chip Match can both complete, award rewards once, and
  refresh the right-pane XP/coin card.
- The hardware plugin and `rockbox.ipod` build successfully.
- Deployment preserves the mounted iPod database and writes matching
  firmware to both supported boot paths without ejecting.
