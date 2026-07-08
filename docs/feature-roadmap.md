# RockBox_Personal Forward Feature Roadmap

Scope: forward-looking user-visible improvements for iPod Video / 5G first, with iPod Classic 6G/7G support where the same UI, plugin, or asset path applies. This roadmap intentionally avoids old bug/regression threads and treats WPS/SBS/lockscreen work as new polish only when directly useful.

## Recommended Implementation Order

1. Rockboy launcher developer metadata display. Small, low-risk, implemented first in this pass.
2. Recently launched plugins list in the Games/plugin surface.
3. Favorites section in the Games launcher.
4. Improved charging screen visual polish for iPone.
5. Now Playing mini-player polish in iPone SBS/WPS.
6. Rockboy cover cache preload tuning and validation.
7. Video player resume/recently watched list.
8. RockPod simulator launcher and screenshot capture manager.
9. Plugin metadata/category index.
10. Artist/album grid browser prototype.
11. Cover Flow album browser prototype.
12. Low-power UI mode for animation-heavy screens.
13. Dashboard/home screen with widgets.
14. Video thumbnail preview experiments.
15. Plugin profiling tools and redraw instrumentation.

## Modern iPone UI Upgrades

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 4 | Improved charging screen visuals | Charging looks like a deliberate OS screen instead of a static fallback. | `wps/iPone.wps`, `wps/iPone.sbs`, theme bitmap assets, `apps/root_menu.c` charging path. | small | low | Launch sim, enter charging/USB power state where possible, verify no WPS/SBS overlap and readable battery state. |
| 5 | Better Now Playing mini-player polish | Root/menu screens feel more modern while music plays. | `wps/iPone.sbs`, `wps/Blackery.sbs`, shared bitmap assets. | small | medium | Play a track in sim, browse root/menu screens, verify mini-player text/art/status stay aligned. |
| 16 | Polished volume HUD | Volume changes feel native and easier to read. | `apps/gui/`, `apps/screens.c`, theme viewport/WPS support. | medium | medium | Adjust volume while WPS and menus are visible, screenshot before/after states. |
| 17 | Smoother menu transitions and highlight animation | Menu navigation feels less abrupt and more like a modern iPod OS. | `apps/gui/list.c`, `apps/gui/skin_engine/`, `apps/root_menu.c`. | large | high | Scroll long menus in sim with frame/time logging, compare redraw cost and input latency. |
| 18 | Richer battery/charging visuals | Battery state is easier to understand at a glance. | `wps/iPone.wps`, `wps/iPone.sbs`, battery tags/assets, `apps/status.c`. | medium | medium | Simulate battery states if available, otherwise validate static skin parsing and screenshots. |
| 19 | Optional lockscreen clock/weather-style layout | Gives the device a fresh idle/lock appearance without replacing the main WPS. | `wps/iPone.wps`, lockscreen assets/settings docs. | medium | medium | Enable hold/backlight mode in sim, verify clock/music fallback layout and no clipped text. |
| 20 | Dynamic accent colors from album art | UI can feel tailored to current music. | Skin engine color tags, album art loader/cache, `wps/iPone.*`. | large | high | Play tracks with distinct album art, verify accent changes without redraw stalls. |
| 21 | Theme-safe UI components shared across screens | New visual pieces can be reused without hand-copying fragile theme code. | `wps/`, theme asset conventions, docs/theme packaging. | medium | medium | Install iPone and Blackery variants in sim and verify shared components render consistently. |

## Cover Flow / Music Browsing

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 10 | Artist/album grid view prototype | Faster visual browsing than deep text lists. | `apps/tagtree.c`, `apps/tagcache.c`, `apps/plugins/pictureflow/`, `apps/root_menu.c`. | large | high | Seed sim database and album art, open grid, scroll 100+ albums while watching responsiveness. |
| 11 | Album Cover Flow browser | Brings the signature iPod browsing mode into the modern skin flow. | `apps/plugins/pictureflow/`, `apps/root_menu.c`, album art cache. | large | high | Open Cover Flow from root, scroll across albums, launch selected album/track. |
| 22 | Fast alphabet jump | Large libraries become practical on clickwheel devices. | `apps/gui/list.c`, `apps/tagtree.c`, database browser actions. | medium | medium | Build a large fake database, hold scroll and verify jump overlay/index behavior. |
| 23 | Recently played / favorites | Common listening paths become one click away. | `apps/tagcache.c`, `apps/bookmark.c`, playlist catalog, root menu entries. | medium | medium | Play several tracks, reboot sim, verify recents/favorites persist and open. |
| 24 | Visual smart playlists | Smart lists feel curated instead of text-only. | Playlist catalog, `apps/tagtree.c`, cover/art lookup. | large | medium | Create sample smart playlists, verify cover tiles and playback queue generation. |
| 25 | Better artwork cache | Album art appears faster with fewer disk hits. | Album art loader, `apps/recorder/albumart.c`, image cache helpers. | medium | medium | Scroll album-heavy views and compare cache misses/load time. |
| 6 | Smoother scrolling with preloaded covers | Reduces blank art and stutter during visual browsing. | `apps/plugins/pictureflow/`, shared image cache ideas from `apps/plugins/rockboy_launcher.c`. | medium | medium | Scroll rapidly through covers in sim, verify adjacent covers are ready before selection lands. |

## Rockboy Improvements

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Rockboy launcher developer metadata display | Game cover browsing shows richer per-game credits from the existing TSV index. | `apps/plugins/rockboy_launcher.c`, `docs/rockboy_launcher_index.example.tsv`. | small | low | Add developer values to `games.tsv`, open Games, verify publisher/developer line appears and truncates cleanly. |
| 26 | Better audio timing | Games sound less choppy and closer to hardware timing. | `apps/plugins/rockboy/`, PCM/audio buffer code. | large | high | Run known audio-sensitive ROMs, compare audio drift/clicks against baseline. |
| 27 | Frame pacing improvements | Scrolling and gameplay feel steadier. | `apps/plugins/rockboy/`, timer/frame loop, LCD update path. | large | high | Run test ROMs and capture frame timing/visual smoothness in sim and on 5G hardware. |
| 28 | Clickwheel-native controls | Default controls match iPod muscle memory. | `apps/plugins/rockboy/`, `apps/plugins/rockboy/settings.h`, launcher settings menu. | medium | medium | In sim, verify D-pad/clickwheel mappings in menus and gameplay. |
| 29 | Save-state UI | Saves feel like a normal console feature instead of a hidden menu. | `apps/plugins/rockboy/`, `apps/plugins/rockboy_launcher.c`, save directory. | medium | medium | Create/load/delete save states from launcher and in-game menu. |
| 3 | Favorites/recent games | Common games are immediately available. | `apps/plugins/rockboy_launcher.c`, launcher state files under plugin data dir. | medium | low | Mark favorites, launch games, restart sim, verify favorites/recents persist. |
| 30 | Cover-art launcher polish | Cover browsing becomes the primary Game Boy entry point. | `apps/plugins/rockboy_launcher.c`, generated cover assets, `docs/rockboy_launcher_validation_checklist.md`. | medium | medium | Use 30+ indexed games with mixed cover formats, verify no blank or stale covers. |
| 31 | Per-game settings | Problem ROMs can use tuned controls, scaling, or performance presets. | `apps/plugins/rockboy_launcher.c`, `apps/plugins/rockboy/settings.*`. | medium | medium | Configure two ROMs differently, launch both, verify settings swap correctly. |
| 32 | Optional Game Boy border skins | Games feel more polished on the iPod display. | `apps/plugins/rockboy/`, bitmap skin assets, scaler/render path. | medium | medium | Enable borders in sim, test scaled and unscaled modes for clipping. |

## Video Player Improvements

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 7 | Resume playback and recently watched | Videos behave like a modern media app. | `apps/plugins/mpegplayer/`, `apps/root_menu.c`, plugin data state. | medium | medium | Open two videos, exit mid-playback, relaunch from Videos and verify resume/recent list. |
| 33 | iPone-style playback controls | Video controls visually match the rest of the OS. | `apps/plugins/mpegplayer/`, theme assets, button overlay drawing. | medium | medium | Play video in sim, show controls, verify seek/play/pause states and no text clipping. |
| 34 | Cleaner seek bar | Scrubbing position is easier to read. | `apps/plugins/mpegplayer/` UI drawing. | small | medium | Seek through a test video and compare control visibility over dark/light scenes. |
| 14 | Thumbnail previews | Seeking and browsing videos becomes visual. | `apps/plugins/mpegplayer/`, RockPod video conversion output, cache files. | large | medium | Use generated thumbnails, seek in sim, verify thumbnail timing and cache load speed. |
| 35 | Better frame pacing | Playback feels smoother on 5G hardware. | `apps/plugins/mpegplayer/`, timer/decode/display loop. | large | high | Run known 5G-compatible clips and measure dropped frames before/after. |
| 36 | Documented 5G conversion profiles | Users can create videos that actually play well. | `docs/`, RockPod conversion scripts. | small | low | Convert sample clips using documented profile and verify playback in 5G sim/hardware. |

## Plugin Launcher Modernization

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 2 | Recently launched plugins list | Frequently used games/apps become quick to reopen. | `apps/root_menu.c`, `apps/open_plugin.*`, plugin data state, possibly `apps/menus/plugin_menu.c`. | small | medium | Launch 3 plugins, return to Games/Plugins, verify recent order survives restart. |
| 3 | Favorites section in Games launcher | Favorite plugins/games appear above the generic browser. | `apps/root_menu.c`, plugin metadata index, shortcuts/open_plugin support. | medium | low | Mark favorites, restart sim, verify favorites open directly. |
| 9 | Per-plugin metadata/categories | Games and apps can be grouped with names, art, and descriptions. | `apps/root_menu.c`, `apps/plugins/`, plugin build metadata, docs. | medium | medium | Seed metadata for several plugins, verify categories and fallback for missing data. |
| 37 | Cover/artwork support for plugins | Plugin browsing feels like an app launcher. | `apps/root_menu.c`, image loader/cache, plugin data assets. | large | medium | Browse plugin grid/list with mixed art, verify placeholders and scroll speed. |
| 38 | Nicer exit/return flow | Returning from games/apps feels predictable. | `apps/root_menu.c`, `apps/open_plugin.*`, plugin return codes. | medium | medium | Launch multiple plugins from different origins and verify Back/Menu destination. |

## RockPod Desktop Integration

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 8 | Simulator launcher and screenshot capture manager | Faster visual iteration for themes and plugins. | `rockpod/main.py`, `tools/`, simulator build dirs, screenshot output dir. | medium | low | Start 5G sim from RockPod, capture screenshots, verify files are named by profile/time. |
| 39 | One-click theme install/update | Theme testing becomes less manual. | `rockpod/`, theme asset folders, install scripts. | medium | low | Install iPone into simdisk, relaunch sim, verify theme selection/assets. |
| 40 | Cover art downloader | Music and game browsing can be visually complete. | `rockpod/scripts/`, metadata lookup, album/game cover folders. | medium | medium | Dry-run download into simdisk cache, verify filenames match expected lookup paths. |
| 41 | Game cover sync | Rockboy launcher setup becomes push-button. | `rockpod/`, `docs/rockboy_launcher_index.example.tsv`, `/gameboy` sync. | medium | low | Sync covers and `games.tsv`, open Games in sim and verify art/metadata. |
| 42 | Video conversion queue | 5G-compatible videos are easier to produce. | `rockpod/`, ffmpeg wrapper/scripts, docs conversion profile. | medium | medium | Queue multiple clips, inspect output, play converted files in sim. |
| 43 | Playlist/smart playlist sync | Desktop curation transfers cleanly to device. | `rockpod/`, playlist catalog paths, database refresh flow. | medium | medium | Sync sample playlists to simdisk and open from Playlists screen. |
| 44 | Firmware/theme profile manager | Multiple device profiles become manageable. | `rockpod/`, build/install scripts, profile config. | large | medium | Switch between iPod Video and Classic profiles and verify copied artifacts differ correctly. |

## Performance And Core Features

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 12 | Optional low-power UI mode | Battery-conscious users can disable expensive animations. | Settings list, `apps/root_menu.c`, skin/plugin animation gates. | medium | medium | Toggle mode, verify animations/preloads reduce while navigation still works. |
| 45 | Faster database browsing | Large music libraries feel less sluggish. | `apps/tagcache.c`, `apps/tagtree.c`, database browser UI. | large | high | Seed large database in sim, time common browse paths before/after. |
| 46 | Smarter disk caching for iFlash/SSD | Less stutter and better battery behavior on storage mods. | `firmware/storage.*`, buffering, config settings. | large | high | Use repeatable file browse/playback tests; final validation needs hardware. |
| 47 | Reduced redraw cost | Menus, overlays, and plugins feel smoother. | `apps/gui/`, skin engine, plugin draw loops. | medium | medium | Instrument redraw counts and compare scrolling frame budget. |
| 13 | Faster boot path where safe | Device reaches music/menu sooner. | Boot/init path, database/theme load ordering. | large | high | Time cold sim startup and hardware boot, verify no missing theme/database state. |
| 15 | Plugin performance profiling tools | Future plugin work can be measured instead of guessed. | `apps/plugins/`, debug/profiling helpers, simulator tooling. | medium | low | Run profiler in sim around Rockboy launcher and mpegplayer UI paths. |

## Fun "New OS" Features

| Order | Feature | User-visible benefit | Likely files/modules involved | Difficulty | Risk | iPod Video / 5G simulator test |
| --- | --- | --- | --- | --- | --- | --- |
| 13 | Dashboard/home screen with widgets | The device feels like a small media OS rather than a list launcher. | `apps/root_menu.c`, `wps/iPone.sbs`, custom menu/widget code. | large | high | Open root/home in sim with music playing, verify widgets update and navigation remains clear. |
| 48 | Widgets: now playing, battery, games, recent albums | Useful information is visible without deep navigation. | Root menu/dashboard, tagcache, plugin recents state. | large | medium | Seed music/games, verify each widget state and empty fallback. |
| 49 | Notification-style overlays | Actions like favorite saved, charging, or game launched get modern feedback. | `apps/gui/splash.c`, root menu/plugin launch paths, theme assets. | medium | medium | Trigger overlay events in sim and verify timeout/input behavior. |
| 50 | App-like launcher | Games, Videos, Photos, Music, and Settings can share a visual launch surface. | `apps/root_menu.c`, plugin metadata/art cache. | large | medium | Navigate app launcher with wheel/buttons and verify fallback on missing art. |
| 51 | Sleep timer UI | Sleep timer becomes easier to set quickly. | Settings/menu code, playback settings, root shortcut. | small | low | Set/cancel timer in sim and verify visible countdown/status where available. |
| 52 | Charging screen | Docked charging can become a useful glanceable mode. | `wps/iPone.wps`, battery/power tags, optional root power menu. | medium | medium | Enter charging state in sim/hardware and verify clock/battery/music fallback. |
| 53 | Boot splash/profile picker | Multiple looks/build profiles can be selected cleanly. | Boot assets, RockPod profile manager, settings/profile storage. | large | high | Test simulated profile selection and verify correct theme/assets load. |
| 54 | Seasonal/dynamic wallpapers | Adds personality without changing core behavior. | Theme assets, RockPod asset sync, optional date/profile setting. | medium | low | Switch profile/date assets in sim and verify WPS/SBS render. |

## First Implemented Feature Notes

Implemented first: Rockboy launcher developer metadata display.

What changed:
- `apps/plugins/rockboy_launcher.c` now stores the optional `developer` column from `/.rockbox/rocks/games/rockboy_launcher/games.tsv`.
- The launcher detail area now shows `publisher / developer` when both are present and distinct, or the available single credit when only one is present.
- Existing scan fallback behavior is unchanged; ROMs discovered from `/gameboy` simply have no developer credit unless provided by the TSV index.

Quick simulator test:
1. Build the iPod Video sim: `make -C build-sim-video-5g -j2`.
2. Add a test `games.tsv` under `build-sim-video-5g/simdisk/.rockbox/rocks/games/rockboy_launcher/` using the documented columns.
3. Include a row with distinct publisher and developer values, for example `Nintendo` and `Game Freak`.
4. Launch `./build-sim-video-5g/rockboxui`, open `Games`, and verify the selected game detail line shows both credits and truncates cleanly on the 320x240 display.
