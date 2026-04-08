# Minish Cap Used Assets Registry

Updated: 2026-04-08

## Runtime-loaded external assets (from `PLUGIN_GAMES_DIR`)
- `minishcap_smith_real.bmp` (preferred)
- `minishcap_smith_real.24x24x24.bmp` (fallback)
- `minishcap_zelda_real.bmp` (preferred)
- `minishcap_zelda_real.24x24x24.bmp` (fallback)

## Compiled plugin bitmap assets
- `pluginbitmaps/minishcap_link_back.h`
- `pluginbitmaps/minishcap_link_front.h`
- `pluginbitmaps/minishcap_link_front_step.h`
- `pluginbitmaps/minishcap_link_left.h`
- `pluginbitmaps/minishcap_link_left_step.h`
- `pluginbitmaps/minishcap_link_right.h`
- `pluginbitmaps/minishcap_link_right_step.h`
- `pluginbitmaps/minishcap_room_links_house_bedroom.h`
- `pluginbitmaps/minishcap_room_links_house_entrance.h`
- `pluginbitmaps/minishcap_room_links_house_smith.h`
- `pluginbitmaps/minishcap_room_creek_real.h`
- `pluginbitmaps/minishcap_room_meadow_real.h`
- `minishcap_south_hyrule_collision.h`
- `minishcap_south_hyrule_full.bmp` (runtime-loaded full meadow background, 1008x688)

## Rendering notes (decompile-aligned)
- Meadow room must render from `minishcap_room_meadow_real` (no procedural fallback)
- Meadow uses world coordinates + camera tracking (`camera_x/camera_y`) for correct viewport
- Creek room renders from `minishcap_room_creek_real` using outdoor camera mode
- Full-map meadow pass loads `minishcap_south_hyrule_full.bmp`; 320x208 meadow header remains fallback-only

## Gameplay progression notes
- Added outdoor area transition: South Hyrule (meadow) <-> Minish Creek
- Meadow movement now respects `minishcap_south_hyrule_collision.h` during runtime movement (not just spawn validation)

## HUD notes
- Removed placeholder-style HUD labels (e.g. `LIFE`, overlapping `A`/`ROLL` look)
- Current HUD uses compact framed counters/actions (`R###`, `B:SWORD`, `A:ROLL`) as a closer in-game presentation
- Current pass split A/B into separate framed button panels with icon-style action hints (no `Aroll`-style text mash)

## Simulator sync targets required
When updating external NPC assets, copy to both:
- `build-sim-video/simdisk/.rockbox/rocks/games/`
- `build-sim-video/simdisk/rocks/games/`

When updating plugin binary, sync:
- `build-sim-video/apps/rocks/minishcap.rock`
- `build-sim-video/apps/rocks/games/minishcap.rock`
- `build-sim-video/simdisk/.rockbox/rocks/minishcap.rock`
- `build-sim-video/simdisk/.rockbox/rocks/games/minishcap.rock`
- `build-sim-video/simdisk/rocks/minishcap.rock`
- `build-sim-video/simdisk/rocks/games/minishcap.rock`

## Current gameplay-critical constants (for debug tracing)
- `HOUSE_FRONT_EXIT_X/Y/W/H` (expanded entrance front-door exit trigger)
- `HOUSE_EXIT_SAFE_X/Y` (meadow spawn after leaving house; Y aligned to decompile transition `0x188`)
- `SMITH_INTRO_X/Y`, `ZELDA_SMITH_X/Y`, `SMITH_POST_X/Y` (decompile-aligned Smith room entity positions)
