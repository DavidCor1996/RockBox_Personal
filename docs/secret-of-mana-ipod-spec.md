# Secret of Mana for iPod 6G

## Delivery decision

Secret of Mana is delivered through the existing ARM-native SNES Lite
frontend and core. No complete, game-specific Secret of Mana source port is
currently available to substitute for the cartridge program. The public
projects reviewed were either asset tools, incomplete disassemblies, or a
generic static-recompilation framework that still requires game-specific
integration. This is therefore the user's permitted emulator fallback, not a
claim of a native game port.

The entire retail game is present in one verified clean US cartridge image;
this is not a demo or rewritten approximation. Only display, input, audio,
performance, library metadata, and save integration are adapted for iPod.

## Authentic personal assets

| Asset | Installed path | SHA-256 |
|---|---|---|
| Secret of Mana (USA) cartridge | `/.rockbox/roms/snes/Secret of Mana (USA).sfc` | `4c15013131351e694e05f22e38bb1b3e4031dedac77ec75abecebe8520d82d5f` |
| Original box-art cover, iPod BMP conversion | `/.rockbox/roms/snes/Secret of Mana (USA).bmp` | `c87776847f3f9ed5a4eab8a11f156e5e599e7c76107b13446c41b02ce09a84db` |

The 2 MiB ROM identifies as `Secret of MANA`, map mode `0x21`, cartridge type
`0x02`. Its CRC32 is `D0176B24` and SHA-1 is
`8133041A363E3CC68CEDEF40B49B6D20D03C505D`, matching the clean US release.
No generated or hand-drawn art is used.

## Native iPod presentation

- First-class Steam tile: **Secret of Mana**, Super Nintendo, Square, 1993.
- Tile launches SNES Lite directly with the installed cartridge path.
- Fullscreen RGB565 path scales the SNES 256×224 framebuffer to the iPod's
  complete 320×240 panel in the familiar 4:3 television presentation.
- Balanced performance preset, automatic frameskip initially at zero, 32 kHz
  audio, and Action input profile are stored per game.
- The emulator menu remains reachable by holding Menu. The hold switch exits
  safely back to Rockbox.

## Click-wheel controls

| SNES control | iPod control |
|---|---|
| D-pad, including diagonals | Eight click-wheel zones |
| B | Center |
| A | Play/Pause |
| Y | Previous |
| X | Next |
| Start | Menu + Center |
| Select | Menu + Play/Pause |
| L | Menu + Previous |
| R | Menu + Next |

This mapping exposes every control used by Secret of Mana, including Start,
Select, and both shoulder buttons.

## Saving

Battery-backed SRAM is loaded from and saved to
`/.rockbox/saves/snes/Secret of Mana (USA).srm`. The pre-deployment personal
save is 8192 bytes with SHA-256
`11400e235b4bfb471ea76814cf8fc35372b70fb4eac9dd785db4fb11d43b53ab`.
Deployment preserved that file byte-for-byte. Normal in-game saves update it
when SNES Lite exits cleanly.

## Qualification gates

- Control self-test: pass, coverage `0xfff`, expected `0xfff` (all 12 SNES
  joypad bits).
- 3600-frame timed run: 3600 rendered, 0 skipped, 58.98 fps aggregate against
  the game's 59.923 Hz timing (98.4% real-time), fullscreen and 32 kHz audio.
- Final 600-frame repeat: 600 rendered, 0 skipped, 58.82 fps aggregate.
- ARM iPod 6G plugin built successfully using the portable C renderer; the
  rejected experimental ARM renderer is not used.
- Hardware deployment completed with
  `tools/deploy_ipod6g_preserve_database.sh`. Both installed firmware copies
  match SHA-256
  `654b376bb0afdb7fd5abb3cf0921e5e3fe3a53e7b1d29753674cee99a5a55122`;
  all 11 tagcache files remained byte-identical and all 2904 indexed tracks
  validated.

These gates establish launch, core timing, rendered-frame completeness,
fullscreen scaling, audio initialization, input reachability, SRAM wiring,
and ARM build integrity. They do not replace a human end-to-end playthrough
of every game branch on physical hardware.

## Port-availability references

- `snesrecomp`: <https://github.com/mstan/snesrecomp/releases>
- Secret of Mana graphics decompressor (asset utility, not a game port):
  <https://github.com/MatthewCallis/Secret-of-Mana-Graphic-Decompressor>
- Cartridge identity reference: <https://superfamicom.org/info/seiken-densetsu-2>
