# Maker Brawl for iPod

Status: first playable implementation  
Host: Rockpod Maker Lite creator  
Device: native `maker_lite.rock` runtime on iPod Classic 6G/7G and Video 5G  
Match format: one player versus one deterministic CPU fighter

## Product

Maker Brawl is a compact platform-fighting game type built into Maker Lite.
It borrows the readable arena structure of platform fighters without trying
to reproduce the full systems, roster, or presentation of Super Smash Bros.
Only two fighters are active: the click-wheel player and one CPU opponent.

The one-player format is deliberate. A stock iPod has one click wheel and no
reliable second simultaneous controller. Network play, alternating control,
and fake two-player input are out of scope.

## Playable slice

- three-stock matches;
- percentage damage from 0 through 999;
- damage-scaled horizontal and vertical knockback;
- walk, air drift, jump, light attack, and heavy attack;
- solid floors and passable arena platforms;
- deterministic CPU pursuit, edge jumps, attacks, hit stun, and recovery;
- left, right, bottom, and hazard ring-outs;
- win/loss HUD and best-time recording;
- the same portable 60 Hz C simulation in Rockpod preview and on iPod; and
- ordinary Maker Lite terrain, object placement, undo, covers, export, and
  device synchronization.

Controls use the existing Maker Lite input map:

| Input | Action |
| --- | --- |
| Wheel left/right | Move and air drift |
| Wheel up | Jump |
| Select / Primary | Light attack |
| Play / Secondary | Heavy attack |
| Menu | Pause |
| Previous / Next | Camera glance where configured |

## Roster

### Bundled original fighters

The public pack contains four original-generated fighters:

- **Volt Jack** — quick magnetic courier; balanced weight;
- **Mossbyte** — heavy botanical maintenance robot;
- **Cinderwing** — light volcanic glider;
- **Bone Corsair** — mid-heavy spectral pirate with an original silhouette.

Each fighter has its own Maker Lite kit so its artwork can occupy the fixed
player animation slot. The other three appear as CPU-fighter catalog parts.
This makes every ordered matchup authorable without changing RPML at runtime.

### Private guest fighters

Mario, Sonic, and Spinal are supported as private guest slots:

- Mario comes only from the existing hash-verified personal SMW importer;
- Sonic comes only from the existing hash-verified personal Sonic 3 or
  supported Sonic 2 workflow; and
- Spinal requires a new user-supplied, revision-pinned Killer Instinct
  extraction recipe before he can be marked authenticated.

No Mario, Sonic, Killer Instinct, Nintendo, Sega, or Rare/Microsoft pixels are
committed to or distributed with Rockpod. Generated or hand-drawn lookalikes
must not fill these slots. A private crossover kit may combine only the
bounded fighter frames selected from the user's locally authenticated kits;
it records every source digest and cell origin in `provenance.tsv`.

Spinal receives no authenticity claim until a precise owned-game revision,
hash allowlist entry, extraction map, and private trace set exist. The public
Bone Corsair is not Spinal and must never be labeled as him.

Rockpod's **Create Guest Matchup** action combines two already-installed local
fighter kits without copying either source game image. The first kit supplies
the playable animation table; the second supplies one exact idle metasprite
for the CPU. The resulting private kit records both input-kit IDs and art
digests. This enables Mario-versus-Sonic once both personal kits have been
authenticated, and provides the same route for Spinal after his importer is
qualified.

## Maker Lite representation

Maker Brawl uses RPML v3 with level flag `ML_LEVEL_BRAWL`.

- `gameplay` is `"brawl"`;
- exactly one `player` entity is required;
- exactly one `enemy` entity is required and becomes the CPU fighter;
- a goal entity is not required;
- the enemy parameters are `[width, height, weight, stocks]`;
- weight defaults to 100 and stocks default to three; and
- `render_cell` and `asset_id` select the opponent's verified kit art.

The player continues to use the kit's bounded 14-action directional animation
table. The CPU fighter uses its authored catalog cell in the initial slice.
A future RPML revision may add a second directional metasprite table, but
this implementation does not enlarge MLAR or allocate animation memory while
gameplay is running.

The New Project flow offers **Maker Brawl Arena** for every compatible kit.
It creates a 24x15 arena with one main floor and three platforms, asks for a
CPU fighter when fighter parts are available, and stores the roster choices
in project metadata.

## Simulation contract

The brawl state stored in `ml_world` is fixed-size:

- opponent entity index and velocity;
- player and opponent damage;
- player and opponent stocks;
- attack cooldowns; and
- opponent invulnerability timing.

There is no allocation, file access, asset decoding, or random-number source
in the tick. CPU timing derives from the simulation tick and entity index, so
the same input trace produces the same digest on host and device.

The arena branch bypasses normal Mario/Zelda/Sonic enemy damage and movement.
This prevents the opponent from being killed by platformer stomp rules or
processed a second time by generic enemy AI.

## Assets and memory

Bundled original source art lives under:

```text
assets/maker_lite/maker_brawl/
    source/roster-concepts.png
    pack/<fighter-kit-id>/
```

`tools/build_maker_lite_brawl_pack.py` converts the source into bounded MLAR
kits using nearest-neighbor presentation, hard transparency, RGB565 cells,
CRC validation, and per-cell provenance. It does not include audio, touch PCM,
or claim ownership of playback memory.

The iPod renderer reuses the existing resident atlas. The HUD adds only fixed
stack text. Runtime state grows by bounded scalar fields and one vertical
velocity array shared by authored entities; there are no new framebuffers or
dynamic allocations.

## Acceptance

Host and simulator gates must verify:

- ordinary Mario, Zelda, Sonic, and life-sim levels still parse and play;
- a brawl pack without exactly one player and one CPU fighter is rejected;
- both fighters move on solid terrain and the CPU does not use generic enemy
  processing;
- light and heavy attacks increase percentage and knockback;
- three ring-outs end with the correct winner;
- pause and resume preserve stocks and percentage;
- snapshot size matches the bridge ABI;
- repeated input traces produce identical digests;
- original kits contain no commercial source signatures; and
- private guest exports contain provenance and never copy source game images.

Physical iPod qualification must additionally measure frame cadence, click
wheel attack latency, long-match stability, and Database music playback before
and after the plugin. This pass makes no unmeasured hardware performance claim.
