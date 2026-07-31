# Pokédex — Specification

Status: implementation spec for this custom iPod tree (iPod Classic 6G/7G
and iPod Video 5G/5.5G, 320x240, `HAVE_LCD_COLOR`). Standalone feature — it
does not extend PocketCatch (`apps/plugins/pocketcatch/`) and does not use
`rockpod/`.

## 1. Product behaviour

1. From the Applications menu, "Pokédex" opens a scrolling list of the 151
   National Dex entries in number order (`#001 Bulbasaur` ... `#151 Mew`),
   using the real stock Rockbox list widget (`gui_synclist`) — wheel
   acceleration, scrollbar, theme colours, and statusbar all come for free,
   the same as any other Rockbox list screen.
2. Selecting an entry (SELECT / RIGHT) opens a full-screen dex detail page
   styled after the species-info screen from Pokémon FireRed/LeafGreen: a
   real species sprite, dex number, name, genus ("SEED POKéMON" etc.),
   height and weight, and the real English flavour-text description for
   that species.
3. From the detail page, MENU flips to a second page showing type(s) and
   the six real base stats (HP/Attack/Defense/Sp.Atk/Sp.Def/Speed) as
   proportional bars; MENU again flips back to the info page.
4. From either detail page, PREV/NEXT (scroll wheel / Left-Right) moves to
   the adjacent species without returning to the list — browsing the dex
   sequentially feels like flipping pages on a real handheld Pokédex.
5. CANCEL (LEFT) returns from the detail page to the list at the
   previously-selected row; CANCEL from the list exits the plugin.
6. If the on-device data pack (`.rockbox/pokedex/`) is missing or empty,
   the plugin shows one plain-text screen explaining that
   `pokedex_fetch_pokeapi.py` must be run first and naming the expected
   device path — never a placeholder sprite or invented entry.

## 2. Why this is a standalone plugin, not a PocketCatch screen

PocketCatch already has `PC_WORLD_VIEW_POKEDEX`
(`apps/plugins/pocketcatch/pocketcatch.h:67,364`), but it is catch-game
state: `pc_creature_def[]` is a small hardcoded array coupled to
`world->caught_counts[]`/`family_candy[]`, capped at `PC_POKEDEX_MAX` (128),
and it only ever shows caught-status, candy, and evolution — not real base
stats, types, or flavour text. Growing it into a full 151-entry reference
dex would mean carrying real stats/type/flavour-text data inside a plugin
whose entire reason for existing is catching mechanics, and would tie a
pure reference screen to catch-state that a user who has never played
PocketCatch shouldn't need.

Sitekick (`apps/plugins/sitekick.c`) is the closer precedent: a TSV catalog
+ sprite pack + `gui_synclist`/`get_action` browsing loop, no tagcache, no
`core_alloc()`. Pokédex follows that shape as its own plugin, opened from
the Applications menu like Weather or Sitekick. The TSV schema (`§4`) is
deliberately something `pc_world_render.c`'s dex overlay *could* read from
later if that integration is ever wanted — but nothing wires that up now.

## 3. Why this is standalone from `rockpod/`

Weather and Live TV need `rockpod/` because their data changes continuously
(forecasts expire, schedules roll forward) and because they already have a
PC-side sync UI users rely on for those features. The Pokédex's data is a
fixed, versioned reference set — 151 species that don't change once
fetched. Wiring it through `rockpod/services/*.py` + `Config` defaults +
a sidebar panel would add a permanent UI surface and config keys for data
that only ever needs to be fetched once (or re-run after a schema bump).

Instead, `tools/pokedex_fetch_pokeapi.py` (`§5`) is a single self-contained
script: it fetches, converts, and deploys in one run, with no dependency on
the `rockpod` package. A user (or `pokedex_sim_gate.py`) runs it directly.

## 4. On-device data model

`.rockbox/pokedex/pokedex.v1.tsv` — one header row, then 151 data rows,
tab-separated, no embedded tabs/newlines (sanitized at fetch time):

```
pokedex_v1<TAB><generated_at ISO8601><TAB><source><TAB><count>
<dex_id><TAB><name><TAB><genus><TAB><type1><TAB><type2><TAB><height_display><TAB><weight_display><TAB><hp><TAB><atk><TAB><def><TAB><spa><TAB><spd><TAB><spe><TAB><flavor_text>
```

`type2` and any missing field is an empty string, not omitted (fixed column
count). `height_display`/`weight_display` are pre-formatted host-side
(`1'00"`, `4.0 lbs.`) so the plugin never does float math or unit
conversion. Sprites live at `.rockbox/pokedex/sprites/<dex_id 3-digit>.bmp`
(32bpp BGRA `FORMAT_TRANSPARENT` BMP, same convention as
`sitekick_package_assets.py`'s `write_bmp32()`). `source.manifest` records
one `<path>\t<sha256>` line per shipped file, mirroring Sitekick's
provenance manifest.

On-device static memory (no `core_alloc()`, no tagcache/database access):

| Table                              | Cap | Bytes each | Total    |
|-------------------------------------|-----|------------|----------|
| `struct pokedex_entry[]`             | 151 | ~300 B     | ~45 KB   |
| Current-species sprite decode buffer | 1   | 48 KB      | 48 KB    |
| TSV line-read buffer                 | 1   | 512 B      | 512 B    |

Only one sprite is ever resident — loaded fresh from disk each time the
detail page's species changes, never the whole 151-sprite set.

## 5. Host-side tool: `tools/pokedex_fetch_pokeapi.py`

Standalone script, no `rockpod` import. Mirrors `services/weather.py`'s
`urllib.request`/JSON idiom (`_fetch_open_meteo`, `weather.py:73`) for real
data, and `sitekick_package_assets.py`'s `write_bmp32()` for real sprite
art:

1. For dex ids 1-151, `GET https://pokeapi.co/api/v2/pokemon/{id}` (types,
   height, weight, base stats) and
   `GET https://pokeapi.co/api/v2/pokemon-species/{id}` (English genus,
   English Red/Blue/Yellow flavour text, preferring the earliest Gen 1
   version present) — same no-API-key `urllib.request` + `User-Agent`
   pattern as the weather fetch.
2. Downloads the real Game Boy front sprite for each species from
   `PokeAPI/sprites`' `sprites/pokemon/versions/generation-i/red-blue/
   transparent/{id}.png` (96x96, alpha already keyed) and writes it out via
   `write_bmp32()` — the exact 32bpp BGRA BMP writer already proven against
   Rockbox's `apps/recorder/bmp.c` reader by Sitekick.
3. Writes `pokedex.v1.tsv` and `source.manifest` (sha256 per shipped file).
4. `--deploy-root <path>` copies the pack straight onto a mounted device or
   a `build-sim-*/simdisk` tree under `.rockbox/pokedex/`; without it, the
   pack is only written to `--out-dir` for inspection.
5. Soft-fails per species (skips + records a warning) rather than aborting
   the whole run — same non-fatal-per-item norm as `weather.py`.

No sprite is invented or hand-drawn when a fetch fails: a missing sprite
means that species is skipped from the pack entirely (the plugin's list
simply won't show it) rather than a placeholder icon.

## 6. Appearance — sampled from a real reference screen

Colours are sampled directly from a real screencap of the original-series
anime Pokédex ("Dexter", the red clamshell device Ash carries) actively
scanning a Mr. Mime (Bulbapedia Archives, `File:Ash Original Pokédex
scan.png`,
`https://archives.bulbagarden.net/media/upload/1/1a/Ash_Original_Pok%C3%A9dex_scan.png`,
640x480), not invented or hand-picked. An earlier revision of this palette
used a different reference photo that only showed the device's shell with
its screen switched off; this frame shows the screen lit, which is why the
"screen" colour below is a pale sage green rather than dark glass — that
green is what the show's Pokédex screen actually looks like on, not a
design choice:

| Element                          | Sampled px (in 640x480 source) | RGB              |
|-----------------------------------|-------------------------------|------------------|
| Shell red (title/bottom bar)       | (380,140)                     | `(210,28,46)` `#D21C2E` |
| Screen (sage green, one continuous panel) | (250,220)               | `(188,206,131)` `#BCCE83` |
| Bezel white (borders/ink on red)   | (300,160)                      | `(217,219,221)` `#D9DBDD` |
| Lens blue (stat bar fill, sprite reticle) | (195,80)                | `(39,178,229)` `#27B2E5` |
| Status-strip blue-grey (stat bar track) | (480,475)                 | `(74,81,104)` `#4A5168` |
| Indicator green (text ink on the green screen) | (331,62)           | `(39,55,43)` `#27372B` |

Because the real screen shows the scanned species directly on the green
display with no card-within-card chrome, the plugin draws one continuous
green "screen" (not a light/dark two-panel layout as an earlier revision
did) and puts dark ink text directly on it, matching the reference.

```
Info page (320x240):
y   0.. 22  title bar (#D21C2E): "<GENUS>" centered, bezel-white text
            (PokeAPI's genus is already the full category name, e.g.
            "Seed Pokemon")
y  24..218  one continuous screen (#BCCE83), 1px #D9DBDD border
    top-left  "No.001 Bulbasaur"                (dark-green ink)
              "HT 2'04\"      WT 15.2 lbs."
    centered  a square lens-blue (#27B2E5) reticle frames the real
              96x96 sprite -- a deliberate "viewfinder" so a
              native-resolution sprite reads as a framed scan target
              rather than a small icon adrift in the green (see the
              note on upscaling below)
    bottom    word-wrapped flavor text (dark-green ink), up to 3 lines
y 220..240  bottom bar (#D21C2E): "<PREV MENU:STATS NEXT> LEFT:LIST"

Stats page: same chrome (title bar/screen/bottom bar), screen shows
type chip(s) (standard published Pokémon type colours, Bulbapedia's
type-colour chart) and six labeled bars for HP/ATK/DEF/SPA/SPD/SPE in
lens blue on a blue-grey track, each bar length proportional to
value/255.
```

**Sprites are drawn at their real 96x96, not upscaled.** `read_bmp_file()`
supports requesting a larger target size via `FORMAT_RESIZE` +
`FORMAT_KEEP_ASPECT` (used elsewhere in this tree, e.g.
`apps/plugins/photos.c`'s thumbnails), and an earlier revision of this
plugin used that to make the real sprite fill more of the screen the way
the anime reference does. On this target, combining `FORMAT_RESIZE` with
`FORMAT_TRANSPARENT` corrupts the alpha compositing — the resized sprite
renders as a solid black silhouette instead of its real colours. Rather
than ship a broken-looking upscale, the sprite is drawn at its real,
correctly-coloured native size and framed with the lens-blue reticle
above so it still reads as a deliberate, polished focal point.

## 7. Controls

List screen (`CONTEXT_LIST`, stock `gui_synclist` handling):

| Input                        | Action                          |
|-------------------------------|----------------------------------|
| Scroll wheel                  | Move selection (with acceleration) |
| SELECT / RIGHT                 | Open dex detail page for selection |
| LEFT / Menu button (long)      | Exit plugin                     |

Detail page (`CONTEXT_STD`, matches `keymap-ipod.c`'s stock mapping):

| Input                        | Action                          |
|-------------------------------|----------------------------------|
| Scroll wheel                   | Previous / next species (`ACTION_STD_PREV/NEXT`) |
| RIGHT / SELECT (`ACTION_STD_OK`) | (reserved — no-op on this page) |
| MENU                            | Flip Info page <-> Stats page   |
| LEFT (`ACTION_STD_CANCEL`)      | Back to list, selection preserved |

No new button contexts or keymaps are introduced — both screens use
existing stock Rockbox contexts exactly as Shopper and Sitekick do.

## 8. Sound

`rb->beep_play(frequency, duration, amplitude)` is Rockbox's own real
square-wave tone generator (`apps/beep.c`), already used throughout the
firmware for keyclicks. It runs on the dedicated `PCM_MIXER_CHAN_BEEP`
mixer channel -- never `PCM_MIXER_CHAN_PLAYBACK`, never the shared plugin
audio buffer -- so none of the precautions in
`docs/plugin-audio-lifecycle-steering.md` apply to it; it composes safely
with whatever else may be playing (Database music, a plugin's own audio,
or nothing).

Ripping the actual anime sound effects/voice clips was considered and
rejected: unlike species data (PokeAPI) and sprites (PokeAPI/sprites),
there is no open mirror of the show's audio to fetch from, and copying
dialogue/SFX audio verbatim out of copyrighted episodes is a meaningfully
bigger act of reproduction than a single static sprite or a data table --
it was not something this project could source the way it sources
everything else here, through a real, attributable, redistributable-in-spirit
channel. Instead, a real two-tone chirp (`pd_beep_scan()`, 1046 Hz then
1568 Hz) plays when opening a dex entry, echoing the cadence of the
device's real scanning sound without reproducing it, and a short single
tick (`pd_beep_tick()`, 1568 Hz) plays on paging between species and on
flipping Info <-> Stats.

## 9. Assets and legal policy

Real Game Boy sprite art extracted from the games is commercial-derived,
the same legal posture as PocketCatch's `pret/pokered` imports
(`POCKETCATCH_ASSET_PACK_SPEC.md`: "do not bundle copyrighted commercial
assets in public plugin builds"). The split:

- **Committed to git:** the fetch/deploy tool
  (`tools/pokedex_fetch_pokeapi.py`), the TSV *schema* and plugin C code,
  the sim gate, and this spec. No sprite, no flavour text, no stats data
  is committed to the repository or bundled in release zips.
- **Never committed, device/user-local only:** `pokedex.v1.tsv`,
  `sprites/*.bmp`, and `source.manifest`, generated by running the tool
  and living only under `.rockbox/pokedex/` on a device or simulator
  `simdisk/`.
- **No hand-drawn substitutes, ever.** If a species' sprite fetch fails,
  that species is omitted from the pack — never replaced with an invented
  icon or redrawn silhouette. If the whole pack is absent, the plugin
  shows the plain-text "run the importer" state (`§1.6`), never a mocked
  dex screen.

## 10. Testing

- `tools/pokedex_sim_gate.py`: a fixture-based acceptance gate mirroring
  `cps1_sim_gate.py`'s shape. It writes a small *fixture* pack (a
  handful of fixture sprites + a fixture TSV — fixture test art is not a
  "real asset" violation, the same way Live TV's gates use fixture
  channel/guide TSVs rather than real recordings) to a temp `simdisk`,
  launches `build-sim-ipod6g/rockboxui` with the plugin open on that
  root, and asserts on the sampled colours from `§6` at their expected
  (x,y) plus on real string content (dex number, name) via a screenshot
  diff against fixture data.
- Manual simulator pass: list scrolling/acceleration, opening a detail
  page, flipping Info<->Stats, PREV/NEXT paging across the 1/151
  boundary (should not wrap past the ends), CANCEL back to list at the
  right row, CANCEL from the list exiting, and the missing-pack state.
- Hardware builds for `ipod6g` and `ipodvideo` after the simulator pass
  is clean.
- No PCM/mixer/audio path is touched by this plugin, so the plugin-audio
  lifecycle matrix in `docs/plugin-audio-lifecycle-steering.md` does not
  apply — Pokédex draws to the LCD only and never opens the audio buffer.

## 11. Sources

- Reference screencap for sampled colours (active screen): Bulbapedia
  Archives, `File:Ash Original Pokédex scan.png`,
  `https://archives.bulbagarden.net/media/upload/1/1a/Ash_Original_Pok%C3%A9dex_scan.png`.
- Species data (base stats, types, height/weight, genus, English flavour
  text): PokéAPI, `https://pokeapi.co/api/v2/pokemon/{id}` and
  `https://pokeapi.co/api/v2/pokemon-species/{id}`.
- Sprite art: `PokeAPI/sprites` GitHub repository,
  `sprites/pokemon/versions/generation-i/red-blue/transparent/{id}.png`
  (real extracted Game Boy Pokémon Red/Blue sprites).
- BMP writer convention: `tools/sitekick_package_assets.py`'s
  `write_bmp32()`, proven against `apps/recorder/bmp.c`.
