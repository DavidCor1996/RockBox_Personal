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
- `HOUSE_EXIT_SAFE_X/Y` (meadow spawn after leaving house)
- `SMITH_INTRO_X/Y`, `ZELDA_SMITH_X/Y`, `SMITH_POST_X/Y` (decompile-aligned Smith room entity positions)
