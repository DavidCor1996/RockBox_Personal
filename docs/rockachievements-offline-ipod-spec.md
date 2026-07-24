# RockAchievements offline iPod application

## Status

This document specifies an offline achievement system for the personal Rockbox
iPod build. It is a design and acceptance contract, not a claim that offline
unlocks are accepted by the RetroAchievements service.

RetroAchievements does not support starting a fully offline session and later
submitting the unlocks as official awards. RockAchievements therefore keeps
device-earned unlocks local. RockPod may import previously earned account
progress, but it must never upload or misrepresent offline unlocks as official.

The implementation uses the MIT-licensed `rcheevos` C runtime to hash supported
ROMs and evaluate official achievement definitions. `rcheevos` supplies no UI
or network transport; RockPod supplies host networking and each emulator
supplies its memory reader.

Primary technical references:

- https://github.com/RetroAchievements/rcheevos
- https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration
- https://docs.retroachievements.org/general/faq.html#can-i-earn-achievements-offline
- https://docs.retroachievements.org/general/hardcore-compliance-requirements.html

## Product goal

Add `Achievements` under the iPodJS `Extras` menu. The application presents an
Xbox 360-era achievement browser for every launchable game installed on the
device. It shows every achievement in each installed set, including unlocked,
locked, measured-progress, unsupported, and locally earned states.

The finished system must meet all of these conditions:

1. Every installed game with an official RetroAchievements set receives the
   complete compatible official set and the real official badge artwork.
2. Every installed launchable game without a compatible official set receives
   a clearly labeled local RockAchievements set backed by actual device
   telemetry. It must not masquerade as an official RetroAchievements set.
3. A game is never silently omitted because its set, badge, emulator hook, or
   catalog entry is missing.
4. RockPod automatically inventories and refreshes achievement data whenever
   games are added, updated, removed, or synchronized.
5. A device sync cannot report full success until its achievement coverage
   report contains zero missing launchable games and zero missing badge files.

BIOS images, console/system tiles, launchers that merely dispatch another
game, and non-playable cover assets are not games and are excluded from the
coverage denominator.

## Naming and service boundary

- Product name: `RockAchievements` in documentation and data files.
- iPod menu label: `Achievements`.
- Official set label: `RetroAchievements`.
- Local set label: `Local`.
- Xbox presentation is visual only. No Xbox account, Xbox Live connection, or
  Microsoft Gamerscore synchronization is implied.
- Official RetroAchievements points are retained as supplied by the set. The
  Xbox-style UI displays the number with a `G` suffix and includes `Offline
  local score` in the profile footer so it cannot be confused with an Xbox
  account balance.

## Menu placement

`Extras > Achievements` is a first-class iPodJS entry alongside the other
Extras applications.

- The entry is visible when a readable achievement catalog exists.
- If the catalog is missing, it remains visible but opens an explanatory
  `Sync games with RockPod` state instead of disappearing.
- The entry uses the real Xbox 360 sphere artwork extracted from Microsoft's
  official Xbox 360 brand material. It must not use a redrawn orb, emoji,
  generated approximation, or a logo with an unintended white rectangle.
- Entering or leaving Achievements must not rebuild the Games library, query
  tagcache, stop music, or allocate from playback memory.

## Xbox 360 visual direction

The reference period is the original 2005-2008 Xbox 360 Blades/Guide design,
which aligns with the stock 2007 iPod aesthetic. This is not an Xbox One or
current Series dashboard reskin.

Official Microsoft reference material:

- Xbox 360 logo guidelines:
  https://download.microsoft.com/download/e/5/4/e5498215-0265-4095-819f-684147b4d8e7/Xbox360_Logo_Guidelines.pdf
- Xbox 360 dashboard reference:
  https://news.xbox.com/en-us/2005/09/20/a-look-at-the-xbox-360-dashboard/
- Official dashboard/blades screenshot reference:
  https://news.xbox.com/en-us/2006/04/27/how-to-use-the-water-balloon-picture-as-your-dash-background/

The device UI uses:

- a pale silver/gray content surface;
- the Xbox 360 green accent sampled from the official Microsoft source asset;
- rounded Guide-style green selection capsules;
- thin gray dividers and restrained gloss matching the dashboard reference;
- the official Xbox 360 sphere at the left of the header;
- real game covers, real game badges, and real achievement badges;
- the stock Rockbox/iPod font fallback when a redistributable original Xbox
  font is not available. A lookalike font image must not be generated.

Colors are recorded from the selected official reference in
`assets/rockachievements/SOURCES.tsv`, including the source URL and sampled
pixel coordinate. Do not choose colors by eye and call them exact Xbox colors.

Flat panels, separators, progress bars, text, and focus rectangles are native
UI geometry, not bitmap assets. All pictorial elements are subject to the real
asset contract below.

## Screens

### Profile and installed-games landing page

The first screen is an Xbox 360-style achievement profile rather than another
plain Rockbox menu.

Header:

- official Xbox 360 sphere;
- `Achievements` title;
- total local `G` score;
- total unlocked count.

Summary card:

- real user avatar only when RockPod has downloaded the user's actual
  RetroAchievements avatar;
- otherwise use the official Xbox sphere, not an invented gamerpic;
- games started, games completed, total achievements, and percentage complete;
- `Offline` and last RockPod sync timestamp.

Installed game rail/list:

- real cover or official game badge at left;
- title and console;
- `unlocked / total` count;
- `earned points / available points`;
- progress bar;
- source chip: `RA` or `Local`;
- completion mark when every core achievement is unlocked.

Default order is most recently played, followed by never-played titles in
alphabetical order. The filter picker supports:

- All Games;
- In Progress;
- Completed;
- Not Started;
- Official RA;
- Local;
- each console currently represented on the device.

Sort choices are Recently Played, Title, Completion, Score, and Console.

### Game achievement page

Selecting a game opens its complete set.

The top card shows the real game cover or official game badge, title, console,
set source, completion percentage, and score. Below it, achievements are
grouped in this order:

1. Active Challenge;
2. In Progress;
3. Locked;
4. Unlocked;
5. Unsupported.

Each row contains:

- real 48x48 unlocked badge or the real locked badge URL supplied by the set;
- achievement title;
- point value;
- a lock state, unlock date, measured value, or `Unsupported` label;
- one-line description when space permits.

Locked achievements are not hidden. Secret achievements use the title and
description state supplied by the official set; RockAchievements does not
reveal server-hidden text.

### Achievement detail page

Selecting an achievement opens:

- a large real badge;
- title and complete description;
- points;
- locked, locally unlocked, or previously server-unlocked state;
- measured progress where supplied;
- unlock timestamp and playtime when locally known;
- game, console, set revision, and source;
- `Official progress imported by RockPod` or `Earned offline on this iPod`.

No Play button is shown here. Menu returns to the achievement list without
changing the currently selected row.

### Empty, partial, and error states

- No catalog: show the real Xbox sphere and `Connect to RockPod to sync
  achievements`.
- No installed games: show `No games installed`.
- Incomplete catalog: show the exact count and names of uncovered games. Never
  replace this with an empty list.
- Missing badge: show the real game cover cropped into the badge frame only if
  that fallback and its provenance were prepared by RockPod. Never draw a fake
  trophy, lock, question mark, or typographic placeholder.
- Corrupt state: use the last verified catalog generation and report that the
  newer generation was rejected.

## Stock iPod controls

- Wheel clockwise/counterclockwise: next/previous row using the native list
  engine's acceleration and repeat behavior.
- Select: open the highlighted game or achievement.
- Menu: return exactly one level; Menu from the landing page returns to Extras.
- Hold Select: open Filter and Sort for the current page. Wait for Select to be
  released and clear its release event before the picker accepts input.
- Previous/Next: page up/page down only on long lists; no horizontal carousel
  gesture is required.
- Play/Pause: retain the shared iPodJS music shortcut and never repurpose it as
  an achievement action.
- Hold switch, USB, shutdown, and charging events use the shared iPodJS system
  handling.

Selection remains stable while artwork becomes available. Loading or decoding
a badge must never move the row, swallow a Menu press, or delay wheel input.

## Real asset contract

Hand-drawn, AI-generated, traced, recreated, or typographic placeholder assets
are forbidden.

Allowed pictorial sources are:

1. Xbox 360 sphere/wordmark extracted from Microsoft's official Xbox 360 logo
   guidelines or another first-party Microsoft download.
2. Xbox 360 dashboard reference imagery obtained from first-party Xbox/Microsoft
   pages.
3. Achievement unlocked and locked badges downloaded from the exact image URLs
   returned by the official RetroAchievements client response.
4. Official RetroAchievements game badges and the user's real profile avatar.
5. Existing verified game cover/manual screenshot sources already recorded in
   this repository.
6. For local-only achievement badges, crops from real title artwork or actual
   captured gameplay screenshots. Cropping, scaling, and native-color
   conversion are allowed; drawing new badge imagery is not.

Every source asset has a row in `assets/rockachievements/SOURCES.tsv`:

```text
asset_path<TAB>kind<TAB>owner<TAB>source_url<TAB>retrieved_utc<TAB>sha256<TAB>notes
```

Derived device BMPs additionally record source SHA-256, crop rectangle, resize
method, target dimensions, and output SHA-256. A build or sync fails if:

- a pictorial asset has no provenance row;
- an expected downloaded checksum changed without review;
- a badge is a generated placeholder;
- an image has an accidental white matte where transparency or the native
  background is expected;
- an unofficial image is labeled as official.

The authentic Xbox 360 unlock sound may be installed only from a documented
first-party Xbox/Microsoft media source or a user-supplied personal console
capture. If no verifiable original is available, the application is silent;
it must not synthesize or imitate the sound.

## Installed-game discovery

RockPod owns canonical discovery. It merges:

- the main launcher manifest;
- per-console `games.tsv` manifests;
- SNES Lite, Rockboy, NES, SMS/Game Gear, Genesis, PokeMini, Game & Watch,
  N64, and other supported ROM roots;
- launchable native Rockbox game plugins;
- custom installed games such as Super Mario 64, RuneScape Classic, and other
  repository-packaged titles;
- any future console registered through RockPod's system manifest.

Entries are deduplicated by launch target plus content identity, not title.
Wrapper launchers, BIOS files, duplicate cover files, and console tiles do not
produce achievement games.

For each discovered game RockPod records:

```text
game_key
display_title
console_id
launch_plugin
launch_parameter
content_path
content_size
content_mtime_ns
ra_hash
sha256
ra_game_id
set_kind
set_revision
runtime_hook
coverage_status
```

`ra_hash` is produced by `rhash` from `rcheevos` using console-correct hashing.
It is not replaced by a raw file MD5, filename lookup, CRC guess, or SHA-256.
SHA-256 is retained separately for local integrity.

## Achievement coverage classes

Every launchable game must end in exactly one class:

### Official

The game hash resolves to a compatible RetroAchievements game and RockPod has
downloaded the complete core set, locked/unlocked badge pairs, metadata, and
any imported user progress.

### Local deep set

No compatible official set exists, but the game/plugin exposes deterministic
game-specific telemetry. Definitions can include scores, levels, bosses,
completion, or game-specific milestones. The page is labeled `Local`.

### Local baseline set

No official or deep set exists. The shared launcher telemetry supplies a small
honest set that is valid for any playable title:

- First Play: successfully enter the game once;
- Getting Started: accumulate 15 active play minutes;
- Settled In: accumulate 60 active play minutes;
- Regular Player: complete ten distinct launch sessions.

The timer excludes paused time, Hold time, USB mode, crashes before successful
game initialization, and time spent in launcher/details screens. Baseline
badges are made from documented real game artwork crops, never generic drawn
trophies. These achievements are local and carry no official RA status.

### Invalid/uncovered

The entry has no usable set, its assets are incomplete, or its required runtime
hook is absent. This is a hard coverage failure. The device may retain its last
verified catalog, but RockPod must not report the new sync as complete.

## RockPod synchronization

Achievement sync is integrated into the existing Games workflow rather than a
separate manual afterthought.

### User-facing controls

The RockPod Games panel adds:

- achievement status column: Official, Local, Missing, or Stale;
- `Sync Achievements` action;
- `Achievements` details tab showing set counts and real badge previews;
- filters for Missing Achievements and Stale Achievements;
- coverage summary: `covered / launchable games` and missing badge count;
- last successful achievement sync generation and timestamp.

`Sync Selected` and normal device game sync automatically include achievement
sync. Adding a game therefore stages its achievement data in the same
transaction. The explicit action is for refreshing sets and imported account
progress without recopying ROMs.

### Sync pipeline

1. Inventory the mounted target and build the canonical launchable-game list.
2. Reuse a cached hash only when path, size, modification time, and SHA-256 all
   match the last verified inventory.
3. Generate console-correct RA hashes with the bundled host `rhash` helper.
4. Resolve official game IDs and fetch the compatible core achievement set.
   `API_GetGameExtended.php` already filters the response to the requested
   core set and current responses do not include the historical per-item
   `Flags` field. A missing item flag is therefore valid; RockPod verifies the
   returned item count against `NumAchievements` instead of discarding those
   records. Any count mismatch is a partial response and blocks activation.
5. If the user linked a RetroAchievements account, import existing server
   unlock state using a desktop-held token. Never store the password or token
   on the iPod.
6. Download the official game badge and both unlocked/locked achievement badge
   URLs supplied by the set.
7. Validate MIME type, dimensions, content, and checksums; then convert badges
   into bounded native Rockbox BMP sizes.
8. Select an official, local deep, or local baseline coverage class.
9. Compile the device catalog and per-game runtime definition files.
10. Merge device-local unlock state by stable game ID, achievement ID, and set
    revision. Preserve the earliest trustworthy unlock timestamp.
11. Produce a coverage report. Any missing launchable game or badge blocks the
    new generation from becoming current.
12. Copy into a staging generation on the device, verify every file, and
    atomically switch the small `current` generation pointer.

Newly added games are detected automatically. Updated ROM content is rehashed
even when the filename is unchanged. Removed games disappear from the device
catalog but their local progress remains in RockPod's backup and can be
restored if the identical game returns.

### Network and account behavior

- All network activity happens in RockPod on the computer.
- The iPod contains no RetroAchievements credentials and performs no HTTP.
- Offline device unlocks are never submitted to RetroAchievements.
- Previously official account unlocks and device-local unlocks are a union for
  display, but retain distinct provenance flags.
- An official set refresh never erases local progress merely because an
  achievement changed or was retired. It migrates exact stable IDs and reports
  unresolved state for review.
- API errors leave the last verified generation active.

## Device data layout

```text
/.rockbox/achievements/
    current                         small generation identifier
    generations/<generation>/
        catalog.tsv
        inventory.tsv
        coverage.tsv
        profile.tsv
        provenance.tsv
        xbox/xbox360-sphere.32x32x24.bmp
        xbox/xbox360-sphere.48x48x24.bmp
        games/<game_key>/game.tsv
        games/<game_key>/runtime.rac
        games/<game_key>/badges.pack
        games/<game_key>/badge-index.tsv
        games/<game_key>/cover.bmp
    state/unlocks.v1.tsv
    state/sessions.v1.tsv
    state/runtime/<game_key>.bin
    recovery/<last-good-generation>/
```

Catalog text is UTF-8 with explicit escaping rules and fixed column counts.
Runtime definition files are bounds-checked before activation. `badges.pack`
stores prepared native-format badge frames with offsets and checksums so an
emulator can read one badge into a fixed buffer without PNG decoding.

Unlock and session state use append-to-temporary, `fsync`, rename, and backup
rotation. A power loss must leave either the old valid state or the complete
new state, never a partially rewritten unlock table.

## Device application architecture

### Achievements browser

`achievements.rock` owns the Extras browser. It reads only the active verified
generation and local state.

- No tagcache dependency.
- No network calls.
- No `core_alloc`.
- No `plugin_get_audio_buffer`, `audio_stop`, PCM/mixer ownership, or playlist
  mutation.
- Fixed artwork cache: at most four 48x48 badges, one 80x80 game image, and one
  official Xbox sphere.
- Target browser workspace, including BMP scratch and indexes: at most 128 KiB.
- Draw callbacks paint already cached pixels only. One missing badge may be
  serviced only after the selection settles, the input queue is empty, and no
  system event is pending.
- No full-screen framebuffer copies or transition allocations.

### Shared launcher telemetry

The game-launch boundary records stable game key, successful initialization,
active elapsed time, clean/unclean exit, and session count. It provides local
baseline achievements to every playable title without game-specific memory
inspection.

Telemetry must not count time until the target game reports successful startup.
It periodically checkpoints using bounded append-only records and completes the
session when control returns to the launcher. It must not alter game saves.

### Emulator runtime integration

Supported emulators link the required `rcheevos` runtime subset and provide:

- the correct console ID;
- exact content hash identity supplied by the catalog;
- an RA-address-to-emulator-memory reader;
- `rc_client_do_frame` or the lower-level runtime evaluation once for every
  emulated frame, including frames not rendered due to frameskip;
- reset handling;
- serialized achievement progress adjacent to emulator save-state metadata;
- a bounded event queue for unlocks, progress, and challenge indicators;
- fixed badge-pack reads outside the frame draw path.

Initial emulator order:

1. SNES Lite: already exposes 128 KiB system RAM, SRAM, and VRAM through the
   libretro memory API and is the reference integration.
2. Rockboy: expose the existing Game Boy/Game Boy Color memory map through a
   dedicated read callback.
3. NES runtime used by installed `.nes` games.
4. SMS/Game Gear.
5. Genesis/PicoDrive.
6. PokeMini and Game & Watch where compatible official maps exist.
7. Custom/native titles through deep local telemetry hooks.

Games whose official definitions reference unsupported memory are shown as
`Unsupported` during development and fail the final coverage gate until their
reader is corrected or they are deliberately assigned a truthful local set.

### Gameplay notification

An unlock queues an Xbox 360-style notification containing the real prepared
badge, title, points, and `Achievement Unlocked`. It uses native UI geometry
and the verified Xbox sphere asset.

- Notification work never takes the audio buffer.
- The badge pack is opened at game initialization and indexed once.
- Badge bytes are read into one fixed frame buffer from an event service point,
  never from the framebuffer draw callback.
- If the exact real badge cannot be loaded, retain the unlock in state and show
  a text-only notification with the real Xbox sphere. Do not manufacture a
  badge.
- Notification duration and position are elapsed-time driven and cannot stall
  emulation input.
- Authentic sound is optional and subject to the real asset rule. Silence is
  the only fallback.

## Performance and memory gates

- Browser artwork workspace: maximum 128 KiB, fixed and documented.
- Emulator badge workspace: one prepared badge plus index, target maximum
  16 KiB.
- Per-game runtime memory must be measured using the largest installed set.
  Target is 128 KiB and hard maximum is 256 KiB before optimization/review.
- No achievement allocation may shrink or borrow the playback/audio buffer.
- Runtime processing is profiled at every emulated frame. Median frame cost
  target is below 2% and 99th percentile below 5% of the existing emulator
  frame budget on iPod 6G.
- If achievement processing threatens audio or frame deadlines, it yields
  visual notification work, not condition evaluation. Conditions still run
  once per emulated frame as required by `rcheevos`.
- Achievement browsing while music plays must keep playback identity, elapsed
  time, codec state, and core memory stable.

## Complete-device coverage gate

The implementation supplies a host command equivalent to:

```text
tools/rockpod_achievements_coverage.py \
    --mount <mounted-ipod> \
    --require-complete \
    --verify-assets \
    --verify-runtime-hooks
```

It reports:

- launchable game count;
- excluded non-game entries and reasons;
- official set count;
- local deep set count;
- local baseline set count;
- missing/unresolved count;
- achievement and point totals;
- unlocked, locked, measured, and unsupported totals;
- missing/corrupt badge count;
- runtime hook coverage by console;
- source/provenance failures.

`--require-complete` exits nonzero unless:

```text
launchable_games == official + local_deep + local_baseline
missing_games == 0
missing_badges == 0
invalid_provenance == 0
missing_runtime_hooks == 0
```

This gate is run against the physical mounted iPod after the implementation is
installed. A simulator inventory, stale backup, cover directory, or RockPod
source library is not a substitute for the mounted-device result.

## Validation plan

### RockPod tests

- Correct `rhash` identity for fixtures from every supported console.
- Filename changes do not change identity; ROM byte changes force rehash.
- Newly added games receive catalog, set, badge pack, and coverage rows in the
  same sync transaction.
- Removed/re-added identical games retain local progress.
- Official/local provenance is never conflated.
- Failed downloads and changed assets leave the old generation active.
- Account credentials never appear in device files or logs.
- All prepared pictorial assets have provenance and matching checksums.
- Complete-device coverage test fails for one missing game, badge, hook, or
  malformed catalog row.

### Simulator tests

- Achievements appears under Extras and opens directly into the Xbox 360-style
  landing page.
- All, Locked, Unlocked, In Progress, Completed, source, and console filters
  produce stable correct counts.
- Wheel acceleration, Select, long Select, Menu, and Play/Pause match the stock
  iPod interaction contract.
- Long Select does not leak its release into the filter picker.
- Rapid wheel/Menu stress performs no draw-path I/O and does not leak file
  descriptors or memory.
- Badge loading never changes selection or blocks queued input.
- Active music remains playing through at least twenty enter/filter/detail/exit
  cycles.
- Screenshots prove official Xbox/RA assets, correct locked badges, real game
  covers, and no synthetic placeholders.

### Emulator tests

- Known fixture conditions trigger once at the correct frame.
- Frameskip does not skip achievement evaluation.
- Reset and save-state restore preserve or reset runtime state exactly as
  specified by `rcheevos`.
- Unsupported memory reads are reported rather than returning fabricated zero
  data.
- Notification events do not change game input latency, audio ownership, save
  contents, or emulator frame pacing.
- Local baseline time excludes pause, Hold, USB, and failed launches.

### Hardware acceptance

1. Mount the target iPod and run the complete-device coverage gate.
2. Require zero uncovered games and zero missing real badge assets.
3. Start database music, browse Achievements deeply, filter repeatedly, and
   return to Extras without a playback stop or restart.
4. Launch at least one game in every installed emulator family and verify an
   official or local achievement unlock persists after reboot.
5. Verify unlocked badges remain colored, locked badges use the real locked
   artwork, and no placeholder is visible.
6. Add a new supported game through RockPod and confirm its achievements arrive
   automatically in the same sync.
7. Interrupt one staged sync and confirm the prior generation and all local
   unlocks remain readable.
8. Re-run coverage, verify both Rockbox firmware copies when firmware changed,
   sync, and safely eject.

## Definition of done

The feature is complete only when the physical mounted iPod passes the full
coverage gate, every launchable installed game appears in Achievements with an
official or explicitly local set, all displayed pictorial assets have real
documented sources, RockPod automatically handles newly added games, and
achievement browsing/runtime evaluation do not interfere with music, game
audio, saves, input latency, or emulator stability.
