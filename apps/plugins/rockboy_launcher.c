/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Rockboy launcher with lightweight coverflow browsing so Rockboy can return
 * directly to the launcher after exiting.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/pluginlib_bmp.h"
#include "lib/read_image.h"
#include "pluginbitmaps/game_system_arduboy.h"
#include "pluginbitmaps/game_system_doom.h"
#include "pluginbitmaps/game_system_gameboy.h"
#include "pluginbitmaps/game_system_gwatch.h"
#include "pluginbitmaps/game_system_native.h"
#include "pluginbitmaps/game_system_nes.h"
#include "pluginbitmaps/game_system_pokemini.h"
#include "pluginbitmaps/game_system_smsgg.h"
#include "pluginbitmaps/game_system_tamagotchi.h"
#include "rockboy/settings.h"
#include <ctype.h>

#define ROCKBOY_LAUNCHER_DIR  PLUGIN_GAMES_DATA_DIR "/rockboy_launcher"
#define ROCKBOY_INDEX_PATH    ROCKBOY_LAUNCHER_DIR "/games.tsv"
#define ROCKBOY_CONFIG_PATH   ROCKBOY_LAUNCHER_DIR "/config.cfg"
#define ROCKBOY_STATE_PATH    ROCKBOY_LAUNCHER_DIR "/state.dat"
#define GAME_LIBRARY_DIR      ROCKBOX_DIR "/games/library"
#define GAME_LIBRARY_SYSTEMS_DIR GAME_LIBRARY_DIR "/systems"
#define GAME_LIBRARY_COVERS_DIR  GAME_LIBRARY_DIR "/covers"
#define GAME_LIBRARY_CACHE_DIR   GAME_LIBRARY_DIR "/cache"
#define GAME_LIBRARY_CONFIG_PATH GAME_LIBRARY_DIR "/config.cfg"
#define GAME_LIBRARY_SYSTEMS_PATH GAME_LIBRARY_DIR "/systems.tsv"
#define GAME_LIBRARY_STATE_PATH GAME_LIBRARY_CACHE_DIR "/state.dat"
#define ROCKBOY_PLUGIN_PATH   VIEWERS_DIR "/rockboy.rock"
#define INFONES_PLUGIN_PATH   VIEWERS_DIR "/infones.rock"
#define FLASHPLAYER_PLUGIN_PATH VIEWERS_DIR "/flashplayer.rock"
#define SMSGG_PLUGIN_PATH     PLUGIN_GAMES_DIR "/smsgg.rock"
#define ARDUBOY_PLUGIN_PATH   PLUGIN_GAMES_DIR "/arduboy.rock"
#define POKEMINI_PLUGIN_PATH  VIEWERS_DIR "/pokemini.rock"
#define TAMAGOTCHI_PLUGIN_PATH PLUGIN_APPS_DIR "/tamagotchi.rock"
#define GWATCH_PLUGIN_PATH   PLUGIN_GAMES_DIR "/gwatch.rock"
#define STICKRPG_SWF_PATH     ROCKBOX_DIR "/flash/stickrpg/stickrpg.swf"
#define STICKRPG_COVER_BMP    ROCKBOX_DIR "/ipodjs/stickrpg/covers/Stick RPG.bmp"
#define DOOM_PLAY_PLUGIN_PATH PLUGIN_GAMES_DIR "/doom_play.rock"
#define DOOM_PLUGIN_PATH      PLUGIN_GAMES_DIR "/doom.rock"
#define DOOM_COVER_BMP        ROCKBOY_LAUNCHER_DIR "/covers/Doom.bmp"
#define WWE_BACKSTAGE_PLUGIN_PATH PLUGIN_GAMES_DIR "/wwe_backstage.rock"
#define WWE_BACKSTAGE_MANIFEST_PATH PLUGIN_GAMES_DATA_DIR "/wwe_backstage/wwe-backstage.twv"
#define WWE_BACKSTAGE_COVER_BMP PLUGIN_GAMES_DATA_DIR "/wwe_backstage/covers/Can You Survive Backstage in WWE.bmp"
#define RUNESCAPE_CLASSIC_PLUGIN_PATH PLUGIN_GAMES_DIR "/runescape_classic.rock"
#define RUNESCAPE_CLASSIC_COVER_BMP PLUGIN_GAMES_DATA_DIR "/runescape_classic/covers/RuneScape Classic.bmp"
#define ROCKBOY_ROM_DIR       "/gameboy"
#define NES_ROM_DIR           ROCKBOY_ROM_DIR
#define SMSGG_ROM_DIR         ROCKBOX_DIR "/games/smsgg/roms"
#define ARDUBOY_ROM_DIR       ROCKBOX_DIR "/games/arduboy/roms"
#define POKEMINI_ROM_DIR      "/PokeMini"
#define POKEMINI_ALT_ROM_DIR  ROCKBOX_DIR "/games/pokemini/roms"
#define POKEMINI_COVERS_DIR   PLUGIN_GAMES_DATA_DIR "/pokemini_launcher/covers"
#define TAMAGOTCHI_ROM_DIR    ROCKBOX_DIR "/games/tamagotchi/roms"
#define TAMAGOTCHI_ROM_PATH   TAMAGOTCHI_ROM_DIR "/tama.b"
#define TAMAGOTCHI_APP_ROM_DIR ROCKBOX_DIR "/apps/tamagotchi/roms"
#define TAMAGOTCHI_APP_ROM_PATH TAMAGOTCHI_APP_ROM_DIR "/tama.b"
#define GWATCH_ROM_DIR        ROCKBOX_DIR "/games/gwatch/roms"
#define DOOM_WAD_DIR          ROCKBOX_DIR "/games/doom/wads"
#define NATIVE_GAMES_DIR      PLUGIN_GAMES_DIR
#define ROCKBOY_LOADING_BACKGROUND_BMP ROCKBOY_LAUNCHER_DIR "/loading_bg.bmp"
#define ROCKBOY_LOADING_BAR_HEIGHT 22
#define ROCKBOY_LOADING_BAR_MARGIN 10
#define GAME_LIBRARY_NOTIFY_SOURCE "Game Library"

#define MAX_ENTRY_TITLE       96
#define MAX_SAVE_BASENAME     24
#define MAX_ENTRY_YEAR        8
#define MAX_ENTRY_GENRE       32
#define MAX_ENTRY_PUBLISHER   48
#define MAX_ENTRY_DEVELOPER   48
#define MAX_SYSTEM_ID         24
#define MAX_SYSTEM_SUBTITLE   64
#define MAX_SYSTEM_EXTS       64
#define MAX_SYSTEMS           16
#define MIN_ENTRY_CAPACITY    16
#define MAX_SCAN_DEPTH        6
#define MAX_SYSTEM_QUICK_COUNT 99

#define SAVE_HINT_UNKNOWN     0
#define SAVE_HINT_NO          1
#define SAVE_HINT_YES         2

#define FLAG_FAVORITE         0x01
#define FLAG_NEEDS_SETUP      0x02

enum launcher_view_mode {
    VIEW_SYSTEMS = 0,
    VIEW_GAMES,
};

enum launcher_start_view {
    START_SYSTEMS = 0,
    START_LAST_SYSTEM,
    START_LAST_GAME,
};

struct system_entry {
    char id[MAX_SYSTEM_ID];
    char title[MAX_ENTRY_TITLE];
    char subtitle[MAX_SYSTEM_SUBTITLE];
    char plugin_path[MAX_PATH];
    char rom_path[MAX_PATH];
    char cover_path[MAX_PATH];
    char extensions[MAX_SYSTEM_EXTS];
    char setup_message[128];
    char controls[128];
    int sort;
    int game_count;
    bool enabled;
    bool native_plugins;
    bool old_index;
    bool has_saves;
    bool has_haptics;
};

#define COVER_SLOT_COUNT      7
#define COVER_CACHE_RADIUS    3
#define FLOW_ANIMATION_FRAMES 4
#define FLOW_REFLECTION_GAP   4
#define FLOW_FOCUS_LIMIT      256

enum flow_pose {
    FLOW_OFF_LEFT = -3,
    FLOW_FAR_LEFT = -2,
    FLOW_LEFT = -1,
    FLOW_CENTER = 0,
    FLOW_RIGHT = 1,
    FLOW_FAR_RIGHT = 2,
    FLOW_OFF_RIGHT = 3
};

enum launcher_sort_mode {
    SORT_TITLE = 0,
    SORT_FAVORITES_FIRST,
    SORT_SAVES_FIRST,
};

enum launcher_filter_mode {
    FILTER_ALL = 0,
    FILTER_FAVORITES,
    FILTER_SAVED,
};

struct game_entry {
    char title[MAX_ENTRY_TITLE];
    char system_id[MAX_SYSTEM_ID];
    char subtitle[MAX_SYSTEM_SUBTITLE];
    char rom_path[MAX_PATH];
    char plugin_path[MAX_PATH];
    char plugin_param[MAX_PATH];
    char cover_path[MAX_PATH];
    char save_name[MAX_SAVE_BASENAME];
    char year[MAX_ENTRY_YEAR];
    char genre[MAX_ENTRY_GENRE];
    char publisher[MAX_ENTRY_PUBLISHER];
    char developer[MAX_ENTRY_DEVELOPER];
    unsigned char flags;
    unsigned char save_hint;
    signed char has_save;
    int system_index;
    bool is_system;
};

struct cover_slot {
    int entry_index;
    char path[MAX_PATH];
    struct bitmap bitmap;
    fb_data *data;
    size_t bytes;
    bool loaded;
    bool fallback;
    unsigned int last_used;
};

struct launcher_state {
    struct game_entry *entries;
    int entry_capacity;
    int entry_count;
    int total_count;
    int selected;
    bool used_index;
    bool show_builtin_doom;
    bool show_builtin_stickrpg;
    bool show_builtin_runescape;
    int sort_mode;
    int filter_mode;
    int start_view;
    int coverflow_mode;
    bool show_empty_systems;
    bool show_missing_systems;
    bool haptic_ticks;
    bool show_save_indicators;
    enum launcher_view_mode view_mode;
    int current_system;
    int system_count;
    int system_selected;
    struct system_entry systems[MAX_SYSTEMS];

    int line_height;
    int cover_box_x;
    int cover_box_y;
    int cover_box_w;
    int cover_box_h;
    int side_cover_w;
    int side_cover_h;
    int detail_y;
    int detail_w;
    int cache_cover_w;
    int cache_cover_h;
    int reflection_max_h;
    int reflection_gap;

    struct viewport vp;

    char state_rom[MAX_PATH];
    char state_system[MAX_SYSTEM_ID];

    struct bitmap decode_scratch;
    fb_data *decode_scratch_data;
    size_t decode_scratch_bytes;
    struct bitmap reflection_bitmap;
    fb_data *reflection_data;
    size_t reflection_bytes;
    struct bitmap posed_bitmap;
    fb_data *posed_data;
    size_t posed_bytes;
    struct bitmap fallback_cover;
    fb_data *fallback_cover_data;
    size_t fallback_cover_bytes;
    bool fallback_cover_loaded;
    struct cover_slot cover_slots[COVER_SLOT_COUNT];
    unsigned int cover_use_clock;
    int cache_warm_center;
    int cache_warm_step;
};

static struct launcher_state launcher;

static void request_cover_cache_warm(void);
static void warm_cover_cache_step(int budget);
static enum plugin_status launcher_context_menu(void);
static void draw_loading_splashscreen(void);
static void draw_loading_progress(int step, int count, const char *msg);
static void load_selected_cover_with_progress(const char *msg);
static void load_selected_cover_quiet(void);
static bool entry_matches_filter(const struct game_entry *entry);
static void apply_launcher_filter(void);
static bool reload_game_library_with_current_modes(void);
static bool allocate_launcher_buffers(void);
static bool load_system_games(int system_index);
static bool load_system_browser(void);
static enum plugin_status draw_empty_system_library(struct system_entry *system);
static void draw_system_browser_screen(void);

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping launcher_main_ctx[] = {
    { PLA_LEFT,             BUTTON_LEFT,                        BUTTON_NONE },
    { PLA_RIGHT,            BUTTON_RIGHT,                       BUTTON_NONE },
    { PLA_LEFT_REPEAT,      BUTTON_LEFT|BUTTON_REPEAT,          BUTTON_NONE },
    { PLA_RIGHT_REPEAT,     BUTTON_RIGHT|BUTTON_REPEAT,         BUTTON_NONE },
#ifdef HAVE_SCROLLWHEEL
    { PLA_SCROLL_BACK,      BUTTON_SCROLL_BACK,                 BUTTON_NONE },
    { PLA_SCROLL_FWD,       BUTTON_SCROLL_FWD,                  BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,  BUTTON_NONE },
#endif
    { PLA_CANCEL,           BUTTON_MENU,                        BUTTON_NONE },
    { PLA_EXIT,             BUTTON_PLAY|BUTTON_SELECT,          BUTTON_NONE },
    { PLA_SELECT,           BUTTON_SELECT,                      BUTTON_NONE },
    { PLA_SELECT_REL,       BUTTON_SELECT|BUTTON_REL,           BUTTON_SELECT },
    { PLA_SELECT_REPEAT,    BUTTON_SELECT|BUTTON_REPEAT,        BUTTON_NONE },
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_PLUGIN),
};
#define ROCKBOY_LAUNCHER_MAIN_CTX launcher_main_ctx
#else
#define ROCKBOY_LAUNCHER_MAIN_CTX pla_main_ctx
#endif

static const struct button_mapping *plugin_contexts[] = {
    ROCKBOY_LAUNCHER_MAIN_CTX,
#if defined(HAVE_REMOTE_LCD)
    pla_remote_ctx,
#endif
};

static bool launcher_dark_mode(void)
{
    return rb->global_settings->ui_engine_dark_mode;
}

static unsigned launcher_bg_color(void)
{
    return launcher_dark_mode() ? LCD_BLACK : LCD_RGBPACK(236, 238, 241);
}

static unsigned launcher_fg_color(void)
{
    return launcher_dark_mode() ? LCD_WHITE : LCD_RGBPACK(36, 36, 38);
}

static unsigned launcher_muted_fg_color(void)
{
    return launcher_dark_mode() ? LCD_RGBPACK(196, 198, 204) :
                                  LCD_RGBPACK(70, 70, 76);
}

static unsigned launcher_selected_text_color(void)
{
    return launcher_dark_mode() ? LCD_WHITE : LCD_RGBPACK(20, 20, 24);
}

static unsigned launcher_selected_muted_color(void)
{
    return launcher_dark_mode() ? LCD_RGBPACK(222, 224, 230) :
                                  LCD_RGBPACK(70, 70, 76);
}

static unsigned launcher_selected_outline_color(void)
{
    return launcher_dark_mode() ? LCD_RGBPACK(92, 94, 104) :
                                  LCD_RGBPACK(202, 203, 208);
}

static bool has_supported_rom_ext(const char *path)
{
    const char *ext = rb->strrchr(path, '.');
    if (!ext)
        return false;

    return !rb->strcasecmp(ext, ".gb") ||
           !rb->strcasecmp(ext, ".gbc") ||
           !rb->strcasecmp(ext, ".nes") ||
           !rb->strcasecmp(ext, ".rock");
}

static bool has_extension_in_list(const char *path, const char *extensions)
{
    const char *ext = rb->strrchr(path, '.');
    const char *cursor;

    if (!ext || !extensions || !*extensions)
        return false;

    cursor = extensions;
    while (*cursor)
    {
        char token[12];
        size_t len = 0;

        while (*cursor == ',' || *cursor == ' ' || *cursor == '\t')
            cursor++;

        while (cursor[len] && cursor[len] != ',' &&
               cursor[len] != ' ' && cursor[len] != '\t' &&
               len + 1 < sizeof(token))
        {
            token[len] = cursor[len];
            len++;
        }
        token[len] = '\0';

        if (token[0] != '\0' && !rb->strcasecmp(ext, token))
            return true;

        cursor += len;
        while (*cursor && *cursor != ',' && *cursor != ' ' && *cursor != '\t')
            cursor++;
    }

    return false;
}

static bool is_nes_rom(const char *path)
{
    const char *ext = rb->strrchr(path, '.');
    return ext && !rb->strcasecmp(ext, ".nes");
}

static bool is_plugin_entry(const char *path)
{
    const char *ext = rb->strrchr(path, '.');
    return ext && !rb->strcasecmp(ext, ".rock");
}

static bool is_doom_entry(const char *path)
{
    return !rb->strcmp(path, DOOM_PLAY_PLUGIN_PATH);
}

static bool has_supported_cover_ext(const char *path)
{
    const char *ext = rb->strrchr(path, '.');
    if (!ext)
        return false;

    return !rb->strcasecmp(ext, ".bmp") ||
           !rb->strcasecmp(ext, ".png") ||
           !rb->strcasecmp(ext, ".jpg") ||
           !rb->strcasecmp(ext, ".jpeg");
}

static bool is_bmp_cover(const char *path)
{
    const char *ext = rb->strrchr(path, '.');
    return ext && !rb->strcasecmp(ext, ".bmp");
}

static bool tamagotchi_rom_available(void)
{
    return rb->file_exists(TAMAGOTCHI_ROM_PATH) ||
           rb->file_exists(TAMAGOTCHI_APP_ROM_PATH);
}

static const char *tamagotchi_rom_path(void)
{
    if (rb->file_exists(TAMAGOTCHI_ROM_PATH))
        return TAMAGOTCHI_ROM_PATH;
    return TAMAGOTCHI_APP_ROM_PATH;
}

static char *trim_whitespace(char *text)
{
    char *end;

    while (*text && isspace((unsigned char)*text))
        text++;

    end = text + rb->strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return text;
}

static void launcher_notify(const char *body, int priority)
{
    (void)body;
    (void)priority;
}

static bool launcher_notify_handle(int action)
{
    (void)action;
    return false;
}

static void launcher_notify_overlay(void)
{
}

static bool parse_bool(const char *value)
{
    return !rb->strcasecmp(value, "1") ||
           !rb->strcasecmp(value, "true") ||
           !rb->strcasecmp(value, "yes") ||
           !rb->strcasecmp(value, "y") ||
           !rb->strcasecmp(value, "favorite");
}

static unsigned char parse_save_hint(const char *value)
{
    if (!value || !*value)
        return SAVE_HINT_UNKNOWN;
    if (parse_bool(value))
        return SAVE_HINT_YES;
    if (!rb->strcasecmp(value, "0") ||
        !rb->strcasecmp(value, "false") ||
        !rb->strcasecmp(value, "no") ||
        !rb->strcasecmp(value, "n"))
    {
        return SAVE_HINT_NO;
    }
    return SAVE_HINT_UNKNOWN;
}

static void derive_title_from_path(const char *path, char *title, size_t title_size)
{
    const char *name;
    char *ext;
    size_t i;

    name = rb->strrchr(path, '/');
    if (name)
        name++;
    else
        name = path;

    rb->strlcpy(title, name, title_size);
    ext = rb->strrchr(title, '.');
    if (ext)
        *ext = '\0';

    for (i = 0; title[i] != '\0'; i++)
    {
        if (title[i] == '_' || title[i] == '-')
            title[i] = ' ';
    }
}

static void derive_stem_from_path(const char *path, char *stem, size_t stem_size)
{
    const char *name;
    char *ext;

    name = rb->strrchr(path, '/');
    if (name)
        name++;
    else
        name = path;

    rb->strlcpy(stem, name, stem_size);
    ext = rb->strrchr(stem, '.');
    if (ext)
        *ext = '\0';
}

static void make_path_absolute(const char *base_dir, const char *value,
                               char *out, size_t out_size)
{
    if (!value || !*value)
    {
        out[0] = '\0';
        return;
    }

    if (value[0] == '/')
    {
        rb->strlcpy(out, value, out_size);
        return;
    }

    rb->snprintf(out, out_size, "%s/%s", base_dir, value);
}

static bool ensure_launcher_dir(void)
{
    DIR *dir;

    dir = rb->opendir(ROCKBOY_LAUNCHER_DIR);
    if (dir)
    {
        rb->closedir(dir);
        return true;
    }

    return rb->mkdir(ROCKBOY_LAUNCHER_DIR) >= 0;
}

static void mkdir_if_needed(const char *path)
{
    if (!rb->dir_exists(path))
        rb->mkdir(path);
}

static bool ensure_library_dirs(void)
{
    mkdir_if_needed(ROCKBOX_DIR "/games");
    mkdir_if_needed(GAME_LIBRARY_DIR);
    mkdir_if_needed(GAME_LIBRARY_SYSTEMS_DIR);
    mkdir_if_needed(GAME_LIBRARY_COVERS_DIR);
    mkdir_if_needed(GAME_LIBRARY_COVERS_DIR "/systems");
    mkdir_if_needed(GAME_LIBRARY_COVERS_DIR "/smsgg");
    mkdir_if_needed(GAME_LIBRARY_COVERS_DIR "/arduboy");
    mkdir_if_needed(GAME_LIBRARY_COVERS_DIR "/doom");
    mkdir_if_needed(GAME_LIBRARY_COVERS_DIR "/native");
    mkdir_if_needed(GAME_LIBRARY_CACHE_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/smsgg");
    mkdir_if_needed(SMSGG_ROM_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/arduboy");
    mkdir_if_needed(ARDUBOY_ROM_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/arduboy/saves");
    mkdir_if_needed(ROCKBOX_DIR "/games/arduboy/states");
    mkdir_if_needed(ROCKBOX_DIR "/games/pokemini");
    mkdir_if_needed(POKEMINI_ROM_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/tamagotchi");
    mkdir_if_needed(TAMAGOTCHI_ROM_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/gwatch");
    mkdir_if_needed(GWATCH_ROM_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/doom");
    mkdir_if_needed(DOOM_WAD_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/native");

    return rb->dir_exists(GAME_LIBRARY_DIR);
}

static bool write_default_system_manifest(void)
{
    int fd;

    if (rb->file_exists(GAME_LIBRARY_SYSTEMS_PATH))
        return true;
    if (!ensure_library_dirs())
        return false;

    fd = rb->open(GAME_LIBRARY_SYSTEMS_PATH,
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    rb->fdprintf(fd, "id\ttitle\tsubtitle\tplugin\tpath\tcover\tenabled\tsort\n");
    rb->fdprintf(fd, "gameboy\tGame Boy\tRockboy library\t%s\t%s\t%s\t1\t5\n",
                 ROCKBOY_PLUGIN_PATH, ROCKBOY_ROM_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/gameboy.bmp");
    rb->fdprintf(fd, "nes\tNES\tNintendo Entertainment System\t%s\t%s\t%s\t1\t8\n",
                 INFONES_PLUGIN_PATH, NES_ROM_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/nes.bmp");
    rb->fdprintf(fd, "smsgg\tSega\tMaster System / Game Gear\t%s\t%s\t%s\t1\t10\n",
                 SMSGG_PLUGIN_PATH, SMSGG_ROM_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/smsgg.bmp");
    rb->fdprintf(fd, "arduboy\tArduboy\tTiny homebrew handheld\t%s\t%s\t%s\t1\t20\n",
                 ARDUBOY_PLUGIN_PATH, ARDUBOY_ROM_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/arduboy.bmp");
    rb->fdprintf(fd, "tamagotchi\tTamagotchi\tVirtual pet\t%s\t%s\t%s\t1\t30\n",
                 TAMAGOTCHI_PLUGIN_PATH, TAMAGOTCHI_ROM_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/tamagotchi.bmp");
    rb->fdprintf(fd, "pokemini\tPokemon Mini\tNintendo mini handheld\t%s\t%s\t%s\t1\t40\n",
                 POKEMINI_PLUGIN_PATH, POKEMINI_ROM_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/pokemini.bmp");
    rb->fdprintf(fd, "gwatch\tGame & Watch\tLCD handhelds\t%s\t%s\t%s\t1\t50\n",
                 GWATCH_PLUGIN_PATH, GWATCH_ROM_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/gwatch.bmp");
    rb->fdprintf(fd, "doom\tDoom\tWAD launcher\t%s\t%s\t%s\t1\t60\n",
                 DOOM_PLUGIN_PATH, DOOM_WAD_DIR,
                 GAME_LIBRARY_COVERS_DIR "/systems/doom.bmp");
    rb->fdprintf(fd, "native\tNative Games\tRockbox plugins\t\t%s\t%s\t1\t100\n",
                 NATIVE_GAMES_DIR, GAME_LIBRARY_COVERS_DIR "/systems/native.bmp");
    rb->close(fd);
    return true;
}

static void set_system_extensions(struct system_entry *system)
{
    if (!rb->strcmp(system->id, "gameboy"))
    {
        rb->strlcpy(system->extensions, ".gb,.gbc,.rock",
                    sizeof(system->extensions));
        system->old_index = true;
    }
    else if (!rb->strcmp(system->id, "nes"))
        rb->strlcpy(system->extensions, ".nes", sizeof(system->extensions));
    else if (!rb->strcmp(system->id, "smsgg"))
        rb->strlcpy(system->extensions, ".sms,.gg", sizeof(system->extensions));
    else if (!rb->strcmp(system->id, "arduboy"))
        rb->strlcpy(system->extensions, ".hex,.arduboy,.bin",
                    sizeof(system->extensions));
    else if (!rb->strcmp(system->id, "pokemini"))
        rb->strlcpy(system->extensions, ".min", sizeof(system->extensions));
    else if (!rb->strcmp(system->id, "gwatch"))
        rb->strlcpy(system->extensions, ".mgw,.gw,.gwz",
                    sizeof(system->extensions));
    else if (!rb->strcmp(system->id, "doom"))
        rb->strlcpy(system->extensions, ".wad", sizeof(system->extensions));
    else if (!rb->strcmp(system->id, "native"))
    {
        rb->strlcpy(system->extensions, ".rock", sizeof(system->extensions));
        system->native_plugins = true;
    }
}

static void set_system_setup_message(struct system_entry *system)
{
    if (!rb->strcmp(system->id, "arduboy"))
        rb->strlcpy(system->setup_message,
                    "No Arduboy games found. Put .hex files in .rockbox/games/arduboy/roms/",
                    sizeof(system->setup_message));
    else if (!rb->strcmp(system->id, "smsgg"))
        rb->strlcpy(system->setup_message,
                    "No Sega ROMs found. Put .sms or .gg files in .rockbox/games/smsgg/roms/",
                    sizeof(system->setup_message));
    else if (!rb->strcmp(system->id, "tamagotchi"))
        rb->strlcpy(system->setup_message,
                    "Missing tama.b. Put your legally obtained Tamagotchi P1 ROM in .rockbox/games/tamagotchi/roms/ or .rockbox/apps/tamagotchi/roms/",
                    sizeof(system->setup_message));
    else if (!rb->strcmp(system->id, "gwatch"))
        rb->strlcpy(system->setup_message,
                    "No Game & Watch packages found. Put .mgw files in .rockbox/games/gwatch/roms/",
                    sizeof(system->setup_message));
    else if (!rb->strcmp(system->id, "pokemini"))
        rb->strlcpy(system->setup_message,
                    "No Pokemon Mini games found. Put .min files in /PokeMini/",
                    sizeof(system->setup_message));
    else
        rb->snprintf(system->setup_message, sizeof(system->setup_message),
                     "No games found. Put supported files in %s",
                     system->rom_path);
}

static void set_system_controls(struct system_entry *system)
{
    if (!rb->strcmp(system->id, "arduboy"))
    {
        rb->strlcpy(system->controls,
                    "Menu Up, Play Down, Left/Right D-pad, Select A, Long Select B",
                    sizeof(system->controls));
    }
    else if (!rb->strcmp(system->id, "smsgg"))
    {
        rb->strlcpy(system->controls,
                    "Wheel or click buttons move, Select A, Play/Pause B, Menu emulator menu",
                    sizeof(system->controls));
    }
    else if (!rb->strcmp(system->id, "gwatch"))
    {
        rb->strlcpy(system->controls,
                    "Wheel or click buttons move, Select action, Play/Pause secondary, Menu exits",
                    sizeof(system->controls));
    }
    else
    {
        rb->strlcpy(system->controls,
                    "Wheel scrolls, Select launches, Menu goes back",
                    sizeof(system->controls));
    }
}

static struct system_entry *add_system_entry(const char *id, const char *title,
                                             const char *subtitle,
                                             const char *plugin_path,
                                             const char *rom_path,
                                             const char *cover_path,
                                             bool enabled, int sort)
{
    struct system_entry *system;

    if (launcher.system_count >= MAX_SYSTEMS || !id || !*id ||
        !title || !*title || !rom_path || !*rom_path)
        return NULL;

    system = &launcher.systems[launcher.system_count++];
    rb->memset(system, 0, sizeof(*system));
    rb->strlcpy(system->id, id, sizeof(system->id));
    rb->strlcpy(system->title, title, sizeof(system->title));
    if (subtitle)
        rb->strlcpy(system->subtitle, subtitle, sizeof(system->subtitle));
    if (plugin_path)
        rb->strlcpy(system->plugin_path, plugin_path, sizeof(system->plugin_path));
    rb->strlcpy(system->rom_path, rom_path, sizeof(system->rom_path));
    if (cover_path)
        rb->strlcpy(system->cover_path, cover_path, sizeof(system->cover_path));
    system->enabled = enabled;
    system->sort = sort;
    set_system_extensions(system);
    set_system_setup_message(system);
    set_system_controls(system);
    return system;
}

static void load_default_systems(void)
{
    launcher.system_count = 0;
    add_system_entry("gameboy", "Game Boy", "Rockboy library",
                     ROCKBOY_PLUGIN_PATH, ROCKBOY_ROM_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/gameboy.bmp", true, 5);
    add_system_entry("nes", "NES", "Nintendo Entertainment System",
                     INFONES_PLUGIN_PATH, NES_ROM_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/nes.bmp", true, 8);
    add_system_entry("smsgg", "Sega", "Master System / Game Gear",
                     SMSGG_PLUGIN_PATH, SMSGG_ROM_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/smsgg.bmp", true, 10);
    add_system_entry("arduboy", "Arduboy", "Tiny homebrew handheld",
                     ARDUBOY_PLUGIN_PATH, ARDUBOY_ROM_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/arduboy.bmp", true, 20);
    add_system_entry("tamagotchi", "Tamagotchi", "Virtual pet",
                     TAMAGOTCHI_PLUGIN_PATH, TAMAGOTCHI_ROM_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/tamagotchi.bmp", true, 30);
    add_system_entry("pokemini", "Pokemon Mini", "Nintendo mini handheld",
                     POKEMINI_PLUGIN_PATH, POKEMINI_ROM_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/pokemini.bmp", true, 40);
    add_system_entry("gwatch", "Game & Watch", "LCD handhelds",
                     GWATCH_PLUGIN_PATH, GWATCH_ROM_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/gwatch.bmp", true, 50);
    add_system_entry("doom", "Doom", "WAD launcher",
                     DOOM_PLUGIN_PATH, DOOM_WAD_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/doom.bmp", true, 60);
    add_system_entry("native", "Native Games", "Rockbox plugins",
                     "", NATIVE_GAMES_DIR,
                     GAME_LIBRARY_COVERS_DIR "/systems/native.bmp", true, 100);
}

static int compare_systems(const void *a, const void *b)
{
    const struct system_entry *left = a;
    const struct system_entry *right = b;

    if (left->sort != right->sort)
        return left->sort - right->sort;
    return rb->strcasecmp(left->title, right->title);
}

static bool system_entry_exists(const char *id)
{
    int i;

    for (i = 0; i < launcher.system_count; i++)
    {
        if (!rb->strcmp(launcher.systems[i].id, id))
            return true;
    }

    return false;
}

static struct system_entry *find_system_entry(const char *id)
{
    int i;

    for (i = 0; i < launcher.system_count; i++)
    {
        if (!rb->strcmp(launcher.systems[i].id, id))
            return &launcher.systems[i];
    }

    return NULL;
}

static void apply_builtin_system_defaults(struct system_entry *system)
{
    if (system == NULL)
        return;

    if (!rb->strcmp(system->id, "gwatch"))
    {
        rb->strlcpy(system->title, "Game & Watch", sizeof(system->title));
        rb->strlcpy(system->subtitle, "LCD handhelds",
                    sizeof(system->subtitle));
        rb->strlcpy(system->plugin_path, GWATCH_PLUGIN_PATH,
                    sizeof(system->plugin_path));
        rb->strlcpy(system->rom_path, GWATCH_ROM_DIR,
                    sizeof(system->rom_path));
        rb->strlcpy(system->cover_path,
                    GAME_LIBRARY_COVERS_DIR "/systems/gwatch.bmp",
                    sizeof(system->cover_path));
        system->enabled = true;
        system->sort = 50;
    }
    else if (!rb->strcmp(system->id, "pokemini"))
    {
        rb->strlcpy(system->title, "Pokemon Mini", sizeof(system->title));
        rb->strlcpy(system->subtitle, "Nintendo mini handheld",
                    sizeof(system->subtitle));
        rb->strlcpy(system->plugin_path, POKEMINI_PLUGIN_PATH,
                    sizeof(system->plugin_path));
        rb->strlcpy(system->rom_path, POKEMINI_ROM_DIR,
                    sizeof(system->rom_path));
        rb->strlcpy(system->cover_path,
                    GAME_LIBRARY_COVERS_DIR "/systems/pokemini.bmp",
                    sizeof(system->cover_path));
        system->enabled = true;
        system->sort = 40;
    }

    set_system_extensions(system);
    set_system_setup_message(system);
    set_system_controls(system);
}

static void add_missing_builtin_systems(void)
{
    apply_builtin_system_defaults(find_system_entry("gwatch"));

    if (!system_entry_exists("gameboy"))
        add_system_entry("gameboy", "Game Boy", "Rockboy library",
                         ROCKBOY_PLUGIN_PATH, ROCKBOY_ROM_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/gameboy.bmp",
                         true, 5);
    if (!system_entry_exists("nes"))
        add_system_entry("nes", "NES", "Nintendo Entertainment System",
                         INFONES_PLUGIN_PATH, NES_ROM_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/nes.bmp",
                         true, 8);
    if (!system_entry_exists("smsgg"))
        add_system_entry("smsgg", "Sega", "Master System / Game Gear",
                         SMSGG_PLUGIN_PATH, SMSGG_ROM_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/smsgg.bmp",
                         true, 10);
    if (!system_entry_exists("arduboy"))
        add_system_entry("arduboy", "Arduboy", "Tiny homebrew handheld",
                         ARDUBOY_PLUGIN_PATH, ARDUBOY_ROM_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/arduboy.bmp",
                         true, 20);
    if (!system_entry_exists("tamagotchi"))
        add_system_entry("tamagotchi", "Tamagotchi", "Virtual pet",
                         TAMAGOTCHI_PLUGIN_PATH, TAMAGOTCHI_ROM_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/tamagotchi.bmp",
                         true, 30);
    if (!system_entry_exists("pokemini"))
        add_system_entry("pokemini", "Pokemon Mini", "Nintendo mini handheld",
                         POKEMINI_PLUGIN_PATH, POKEMINI_ROM_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/pokemini.bmp",
                         true, 40);
    if (!system_entry_exists("gwatch"))
        add_system_entry("gwatch", "Game & Watch", "LCD handhelds",
                         GWATCH_PLUGIN_PATH, GWATCH_ROM_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/gwatch.bmp",
                         true, 50);
    if (!system_entry_exists("doom"))
        add_system_entry("doom", "Doom", "WAD launcher",
                         DOOM_PLUGIN_PATH, DOOM_WAD_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/doom.bmp",
                         true, 60);
    if (!system_entry_exists("native"))
        add_system_entry("native", "Native Games", "Rockbox plugins",
                         "", NATIVE_GAMES_DIR,
                         GAME_LIBRARY_COVERS_DIR "/systems/native.bmp",
                         true, 100);
}

static bool load_systems_from_manifest(void)
{
    int fd;
    char line[768];
    ssize_t len;

    launcher.system_count = 0;
    fd = rb->open(GAME_LIBRARY_SYSTEMS_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    while ((len = rb->read_line(fd, line, sizeof(line))) > 0 &&
           launcher.system_count < MAX_SYSTEMS)
    {
        char *id;
        char *title;
        char *subtitle;
        char *plugin;
        char *path;
        char *cover;
        char *enabled;
        char *sort;
        char *next;

        (void)len;
        id = trim_whitespace(line);
        if (id[0] == '\0' || id[0] == '#')
            continue;

        next = rb->strchr(id, '\t');
        if (!next)
            continue;
        *next++ = '\0';
        if (!rb->strcasecmp(id, "id"))
            continue;

        title = next;
        next = rb->strchr(next, '\t');
        if (!next)
            continue;
        *next++ = '\0';

        subtitle = next;
        next = rb->strchr(next, '\t');
        if (!next)
            continue;
        *next++ = '\0';

        plugin = next;
        next = rb->strchr(next, '\t');
        if (!next)
            continue;
        *next++ = '\0';

        path = next;
        next = rb->strchr(next, '\t');
        if (!next)
            continue;
        *next++ = '\0';

        cover = next;
        next = rb->strchr(next, '\t');
        enabled = "";
        sort = "";
        if (next)
        {
            *next++ = '\0';
            enabled = next;
            next = rb->strchr(next, '\t');
            if (next)
            {
                *next++ = '\0';
                sort = next;
                next = rb->strchr(next, '\t');
                if (next)
                    *next = '\0';
            }
        }

        add_system_entry(trim_whitespace(id), trim_whitespace(title),
                         trim_whitespace(subtitle), trim_whitespace(plugin),
                         trim_whitespace(path), trim_whitespace(cover),
                         enabled[0] == '\0' || parse_bool(trim_whitespace(enabled)),
                         sort[0] == '\0' ? 100 : rb->atoi(trim_whitespace(sort)));
    }

    rb->close(fd);
    add_missing_builtin_systems();
    if (launcher.system_count > 1)
        rb->qsort(launcher.systems, launcher.system_count,
                  sizeof(struct system_entry), compare_systems);
    return launcher.system_count > 0;
}

static bool load_system_library(void)
{
    ensure_library_dirs();
    write_default_system_manifest();
    if (!load_systems_from_manifest())
        load_default_systems();
    if (launcher.system_count > 1)
        rb->qsort(launcher.systems, launcher.system_count,
                  sizeof(struct system_entry), compare_systems);
    return launcher.system_count > 0;
}

#if CONFIG_KEYPAD == IPOD_4G_PAD && defined(IPOD_VIDEO)
static bool ensure_rockboy_save_dir(void)
{
    DIR *dir;

    dir = rb->opendir(ROCKBOY_SAVE_DIR);
    if (dir)
    {
        rb->closedir(dir);
        return true;
    }

    return rb->mkdir(ROCKBOY_SAVE_DIR) >= 0;
}

static void launcher_apply_performance_preset(struct options *options, int preset)
{
    options->performance_preset = preset;

    switch (preset)
    {
        case ROCKBOY_PERF_PERFORMANCE:
            options->maxskip = 3;
            options->sound = 0;
            options->scaling = 2;
            break;
        case ROCKBOY_PERF_QUALITY:
            options->maxskip = 1;
            options->sound = 1;
            options->scaling = 1;
            break;
        default:
            options->maxskip = 2;
            options->sound = 1;
            options->scaling = 2;
            break;
    }

#if CONFIG_KEYPAD == IPOD_4G_PAD && defined(IPOD_VIDEO)
    if (preset == ROCKBOY_PERF_BALANCED)
    {
        options->maxskip = 4;
        options->sound = 1;
        options->scaling = 0;
    }
    else if (preset == ROCKBOY_PERF_PERFORMANCE)
    {
        options->maxskip = 5;
        options->scaling = 0;
    }
#endif
}

static void launcher_set_default_rockboy_options(struct options *options)
{
    rb->memset(options, 0, sizeof(*options));
    options->sound = 1;
    options->autosave = 1;
    options->control_preset = ROCKBOY_CTRL_IPOD5G;
    launcher_apply_performance_preset(options, ROCKBOY_PERF_BALANCED);
}

static void sanitize_rockboy_options(struct options *options)
{
#if CONFIG_KEYPAD == IPOD_4G_PAD && defined(IPOD_VIDEO)
    if (options->performance_preset == ROCKBOY_PERF_BALANCED &&
        options->maxskip == 2 &&
        options->sound == 1 &&
        options->scaling == 2)
    {
        launcher_apply_performance_preset(options, ROCKBOY_PERF_BALANCED);
    }
#endif

    if (options->performance_preset < ROCKBOY_PERF_BALANCED ||
        options->performance_preset > ROCKBOY_PERF_QUALITY)
    {
        launcher_apply_performance_preset(options, ROCKBOY_PERF_BALANCED);
    }

    if (options->autosave < 0 || options->autosave > 1)
        options->autosave = 1;
    if (options->sound < 0 || options->sound > 1)
        options->sound = 1;
    if (options->maxskip < 0)
        options->maxskip = 0;
    if (options->maxskip > 20)
        options->maxskip = 20;
    if (options->control_preset < ROCKBOY_CTRL_CLASSIC ||
        options->control_preset > ROCKBOY_CTRL_IPOD5G)
    {
        options->control_preset = ROCKBOY_CTRL_IPOD5G;
    }
}

static bool load_rockboy_options(struct options *options)
{
    int fd;
    int filesize;
    size_t bytes;
    char options_path[MAX_PATH];

    launcher_set_default_rockboy_options(options);

    rb->snprintf(options_path, sizeof(options_path), "%s/%s",
                 ROCKBOY_SAVE_DIR, ROCKBOY_OPTIONS_FILE);
    fd = rb->open(options_path, O_RDONLY);
    if (fd < 0)
    {
        sanitize_rockboy_options(options);
        return true;
    }

    filesize = rb->filesize(fd);
    if (filesize > 0)
    {
        bytes = filesize < (int)sizeof(*options) ? (size_t)filesize : sizeof(*options);
        rb->read(fd, options, bytes);
    }
    rb->close(fd);

    sanitize_rockboy_options(options);
    return true;
}

static bool save_rockboy_options(struct options *options)
{
    int fd;
    char options_path[MAX_PATH];

    if (!ensure_rockboy_save_dir())
        return false;

    options->dirty = 0;
    rb->snprintf(options_path, sizeof(options_path), "%s/%s",
                 ROCKBOY_SAVE_DIR, ROCKBOY_OPTIONS_FILE);
    fd = rb->open(options_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    if (rb->write(fd, options, sizeof(*options)) != (ssize_t)sizeof(*options))
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

static enum plugin_status launcher_rockboy_settings_menu(void)
{
    struct options options;
    int selection = 0;
    int result;
    bool changed = false;

    static const struct opt_items performance[] = {
        { "Balanced", -1 },
        { "Performance", -1 },
        { "Quality", -1 },
    };

    static const struct opt_items frameskip[] = {
        { "0 Max", -1 },  { "1 Max", -1 },  { "2 Max", -1 },
        { "3 Max", -1 },  { "4 Max", -1 },  { "5 Max", -1 },
        { "6 Max", -1 },  { "7 Max", -1 },  { "8 Max", -1 },
        { "9 Max", -1 },  { "10 Max", -1 }, { "11 Max", -1 },
        { "12 Max", -1 }, { "13 Max", -1 }, { "14 Max", -1 },
        { "15 Max", -1 }, { "16 Max", -1 }, { "17 Max", -1 },
        { "18 Max", -1 }, { "19 Max", -1 }, { "20 Max", -1 },
    };

    static const struct opt_items onoff[] = {
        { "Off", -1 },
        { "On", -1 },
    };

    static const struct opt_items controls[] = {
        { "Classic Wheel", -1 },
        { "5G D-Pad", -1 },
    };

    static const struct opt_items rotate[] = {
        { "No rotation", -1 },
        { "Rotate Right", -1 },
        { "Rotate Left", -1 },
    };

    static const struct opt_items scaling[] = {
        { "Scaled", -1 },
        { "Scaled - Maintain Ratio", -1 },
        { "Unscaled", -1 },
    };

    static const struct opt_items palette[] = {
        { "Brown (Default)", -1 },
        { "Gray", -1 },
        { "Light Gray", -1 },
        { "Multi-Color 1", -1 },
        { "Multi-Color 2", -1 },
        { "Adventure Island", -1 },
        { "Adventure Island 2", -1 },
        { "Balloon Kid", -1 },
        { "Batman", -1 },
        { "Batman: Return of Joker", -1 },
        { "Bionic Commando", -1 },
        { "Castlvania Adventure", -1 },
        { "Donkey Kong Land", -1 },
        { "Dr. Mario", -1 },
        { "Kirby", -1 },
        { "Metroid", -1 },
        { "Zelda", -1 },
    };

    MENUITEM_STRINGLIST(menu, "Rockboy Settings", NULL,
                        "Performance", "Max Frameskip",
                        "Autosave", "Sound", "Controls",
                        "Screen Size", "Screen Rotate", "Set Palette",
                        "Back");

    if (!load_rockboy_options(&options))
        return PLUGIN_OK;

    while (true)
    {
        int previous;

        result = rb->do_menu(&menu, &selection, NULL, false);
        switch (result)
        {
            case 0:
                previous = options.performance_preset;
                rb->set_option("Performance", &options.performance_preset, RB_INT,
                               performance, ARRAYLEN(performance), NULL);
                if (previous != options.performance_preset)
                {
                    launcher_apply_performance_preset(&options,
                                                      options.performance_preset);
                    changed = true;
                }
                break;

            case 1:
                previous = options.maxskip;
                rb->set_option("Max Frameskip", &options.maxskip, RB_INT,
                               frameskip, ARRAYLEN(frameskip), NULL);
                changed |= previous != options.maxskip;
                break;

            case 2:
                previous = options.autosave;
                rb->set_option("Autosave", &options.autosave, RB_INT,
                               onoff, ARRAYLEN(onoff), NULL);
                changed |= previous != options.autosave;
                break;

            case 3:
                previous = options.sound;
                rb->set_option("Sound", &options.sound, RB_INT,
                               onoff, ARRAYLEN(onoff), NULL);
                changed |= previous != options.sound;
                break;

            case 4:
                previous = options.control_preset;
                rb->set_option("Controls", &options.control_preset, RB_INT,
                               controls, ARRAYLEN(controls), NULL);
                changed |= previous != options.control_preset;
                break;

            case 5:
                previous = options.scaling;
                rb->set_option("Screen Size", &options.scaling, RB_INT,
                               scaling, ARRAYLEN(scaling), NULL);
                changed |= previous != options.scaling;
                break;

            case 6:
                previous = options.rotate;
                rb->set_option("Screen Rotate", &options.rotate, RB_INT,
                               rotate, ARRAYLEN(rotate), NULL);
                changed |= previous != options.rotate;
                break;

            case 7:
                previous = options.pal;
                rb->set_option("Set Palette", &options.pal, RB_INT,
                               palette, ARRAYLEN(palette), NULL);
                changed |= previous != options.pal;
                break;

            case 8:
            default:
                rb->button_clear_queue();
                if (!changed)
                    return PLUGIN_OK;
                if (!save_rockboy_options(&options))
                    rb->splash(HZ * 2, "Could not save Rockboy settings");
                return PLUGIN_OK;

            case MENU_ATTACHED_USB:
                return PLUGIN_USB_CONNECTED;
        }
    }
}
#endif

static void write_launcher_state_file(const char *path, const char *system_id,
                                      const char *rom_path)
{
    int fd;

    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd, "system=%s\n", system_id ? system_id : "");
    rb->fdprintf(fd, "game=%s\n", rom_path ? rom_path : "");
    rb->close(fd);
}

static void save_launcher_state(const char *rom_path)
{
    const char *system_id = "";

    if (launcher.view_mode == VIEW_SYSTEMS &&
        launcher.selected >= 0 && launcher.selected < launcher.entry_count &&
        launcher.entries[launcher.selected].is_system)
    {
        system_id = launcher.entries[launcher.selected].system_id;
    }
    else if (launcher.current_system >= 0 &&
             launcher.current_system < launcher.system_count)
    {
        system_id = launcher.systems[launcher.current_system].id;
    }

    if (ensure_library_dirs())
        write_launcher_state_file(GAME_LIBRARY_STATE_PATH, system_id, rom_path);
    if (ensure_launcher_dir())
        write_launcher_state_file(ROCKBOY_STATE_PATH, system_id, rom_path);
}

static void load_launcher_config_file(const char *path)
{
    int fd;
    char line[128];

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *eq = rb->strchr(line, '=');
        char *key;
        char *val;
        bool enabled;

        if (!eq)
            continue;
        *eq++ = '\0';
        key = trim_whitespace(line);
        val = trim_whitespace(eq);
        enabled = parse_bool(val);

        if (!rb->strcmp(key, "show_builtin_doom"))
            launcher.show_builtin_doom = enabled;
        else if (!rb->strcmp(key, "show_builtin_stickrpg"))
            launcher.show_builtin_stickrpg = enabled;
        else if (!rb->strcmp(key, "show_builtin_runescape"))
            launcher.show_builtin_runescape = enabled;
        else if (!rb->strcmp(key, "start_view"))
        {
            if (!rb->strcasecmp(val, "last_system"))
                launcher.start_view = START_LAST_SYSTEM;
            else if (!rb->strcasecmp(val, "last_game"))
                launcher.start_view = START_LAST_GAME;
            else
                launcher.start_view = START_SYSTEMS;
        }
        else if (!rb->strcmp(key, "show_empty_systems"))
            launcher.show_empty_systems = enabled;
        else if (!rb->strcmp(key, "show_missing_systems"))
            launcher.show_missing_systems = enabled;
        else if (!rb->strcmp(key, "haptic_ticks"))
            launcher.haptic_ticks = enabled;
        else if (!rb->strcmp(key, "show_save_indicators"))
            launcher.show_save_indicators = enabled;
    }

    rb->close(fd);
}

static void load_launcher_config(void)
{
    launcher.show_builtin_doom = true;
    launcher.show_builtin_stickrpg = true;
    launcher.show_builtin_runescape = true;
    launcher.start_view = START_SYSTEMS;
    launcher.show_empty_systems = true;
    launcher.show_missing_systems = true;
    launcher.haptic_ticks = false;
    launcher.show_save_indicators = true;
    launcher.coverflow_mode = 0;

    load_launcher_config_file(GAME_LIBRARY_CONFIG_PATH);
    load_launcher_config_file(ROCKBOY_CONFIG_PATH);
    launcher.haptic_ticks = false;
}

static bool load_launcher_state_file(const char *path)
{
    int fd;
    ssize_t bytes;
    char *trimmed;

    launcher.state_rom[0] = '\0';
    launcher.state_system[0] = '\0';

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    bytes = rb->read(fd, launcher.state_rom, sizeof(launcher.state_rom) - 1);
    rb->close(fd);
    if (bytes <= 0)
    {
        launcher.state_rom[0] = '\0';
        return false;
    }

    launcher.state_rom[bytes] = '\0';
    if (!rb->strncmp(launcher.state_rom, "system=", 7) ||
        rb->strstr(launcher.state_rom, "\ngame="))
    {
        char *line = launcher.state_rom;

        while (line && *line)
        {
            char *next = rb->strchr(line, '\n');
            char *eq;

            if (next)
                *next++ = '\0';
            eq = rb->strchr(line, '=');
            if (eq)
            {
                *eq++ = '\0';
                trimmed = trim_whitespace(eq);
                if (!rb->strcmp(trim_whitespace(line), "system"))
                    rb->strlcpy(launcher.state_system, trimmed,
                                sizeof(launcher.state_system));
                else if (!rb->strcmp(trim_whitespace(line), "game"))
                    rb->strlcpy(launcher.state_rom, trimmed,
                                sizeof(launcher.state_rom));
            }
            line = next;
        }
    }
    else
    {
        trimmed = trim_whitespace(launcher.state_rom);
        if (trimmed != launcher.state_rom)
            rb->memmove(launcher.state_rom, trimmed, rb->strlen(trimmed) + 1);
        rb->strlcpy(launcher.state_system, "gameboy",
                    sizeof(launcher.state_system));
    }

    return launcher.state_system[0] != '\0' || launcher.state_rom[0] != '\0';
}

static void load_launcher_state(void)
{
    if (load_launcher_state_file(GAME_LIBRARY_STATE_PATH))
        return;

    load_launcher_state_file(ROCKBOY_STATE_PATH);
}

static bool derive_rockboy_save_name(const char *rom_path,
                                     char *save_name, size_t save_name_size)
{
    unsigned char header[0x150];
    int fd;
    ssize_t bytes;
    char *trimmed;

    (void)save_name_size;

    fd = rb->open(rom_path, O_RDONLY);
    if (fd < 0)
        return false;

    bytes = rb->read(fd, header, sizeof(header));
    rb->close(fd);
    if (bytes < 0x144)
        return false;

    rb->memcpy(save_name, &header[0x134], 16);
    if (save_name[14] & 0x80)
        save_name[14] = 0;
    if (save_name[15] & 0x80)
        save_name[15] = 0;
    save_name[16] = '\0';

    trimmed = trim_whitespace(save_name);
    if (trimmed != save_name)
        rb->memmove(save_name, trimmed, rb->strlen(trimmed) + 1);

    return save_name[0] != '\0';
}

static bool game_has_local_save(struct game_entry *entry)
{
    char save_base[MAX_SAVE_BASENAME];
    char path[MAX_PATH];

    if (is_plugin_entry(entry->rom_path))
        return entry->save_hint == SAVE_HINT_YES;

    if (entry->save_name[0] == '\0')
    {
        if (derive_rockboy_save_name(entry->rom_path, save_base, sizeof(save_base)))
            rb->strlcpy(entry->save_name, save_base, sizeof(entry->save_name));
    }

    if (entry->save_name[0] == '\0')
        return entry->save_hint == SAVE_HINT_YES;

    rb->snprintf(path, sizeof(path), "%s/%s.sav", ROCKBOY_SAVE_DIR, entry->save_name);
    if (rb->file_exists(path))
        return true;

    rb->snprintf(path, sizeof(path), "%s/%s.rtc", ROCKBOY_SAVE_DIR, entry->save_name);
    if (rb->file_exists(path))
        return true;

    rb->snprintf(path, sizeof(path), "%s/%s.sn", ROCKBOY_SAVE_DIR, entry->save_name);
    if (rb->file_exists(path))
        return true;

    return entry->save_hint == SAVE_HINT_YES;
}

static void prime_game_metadata(void)
{
    int i;

    for (i = 0; i < launcher.entry_count; i++)
    {
        launcher.entries[i].has_save = game_has_local_save(&launcher.entries[i]) ? 1 : 0;
    }
}

static bool entry_matches_filter(const struct game_entry *entry)
{
    switch (launcher.filter_mode)
    {
        case FILTER_FAVORITES:
            return (entry->flags & FLAG_FAVORITE) != 0;
        case FILTER_SAVED:
            return entry->has_save > 0;
        default:
            return true;
    }
}

static void apply_launcher_filter(void)
{
    int read_index;
    int write_index;

    launcher.total_count = launcher.entry_count;
    if (launcher.filter_mode == FILTER_ALL)
        return;

    write_index = 0;
    for (read_index = 0; read_index < launcher.entry_count; read_index++)
    {
        if (!entry_matches_filter(&launcher.entries[read_index]))
            continue;

        if (write_index != read_index)
            launcher.entries[write_index] = launcher.entries[read_index];
        write_index++;
    }

    launcher.entry_count = write_index;
}

static bool cover_for_known_plugin(const char *path, const char *title,
                                   char *cover_path, size_t cover_path_size);
static bool cover_for_pokemini_rom(const char *path,
                                   char *cover_path, size_t cover_path_size);
static bool cover_for_system_id(const char *system_id,
                                char *cover_path, size_t cover_path_size);

static void add_game_entry(const char *title, const char *rom_path,
                           const char *cover_path, const char *system_id,
                           const char *plugin_path, unsigned char flags,
                           unsigned char save_hint, const char *year,
                           const char *genre, const char *publisher,
                           const char *developer, const char *plugin_param)
{
    struct game_entry *entry;

    if (!rom_path || !*rom_path || launcher.entry_count >= launcher.entry_capacity)
        return;
    if ((!plugin_path || !*plugin_path) && !has_supported_rom_ext(rom_path))
        return;

    entry = &launcher.entries[launcher.entry_count++];
    rb->memset(entry, 0, sizeof(*entry));

    if (title && *title)
        rb->strlcpy(entry->title, title, sizeof(entry->title));
    else
        derive_title_from_path(rom_path, entry->title, sizeof(entry->title));

    rb->strlcpy(entry->rom_path, rom_path, sizeof(entry->rom_path));
    if (system_id && *system_id)
        rb->strlcpy(entry->system_id, system_id, sizeof(entry->system_id));
    if (plugin_path && *plugin_path)
        rb->strlcpy(entry->plugin_path, plugin_path, sizeof(entry->plugin_path));
    if (plugin_param && *plugin_param)
        rb->strlcpy(entry->plugin_param, plugin_param,
                    sizeof(entry->plugin_param));
    if (cover_path && *cover_path && rb->file_exists(cover_path))
        rb->strlcpy(entry->cover_path, cover_path, sizeof(entry->cover_path));
    else if (is_plugin_entry(rom_path))
        cover_for_known_plugin(rom_path, entry->title,
                               entry->cover_path, sizeof(entry->cover_path));
    if (entry->cover_path[0] == '\0' &&
        !rb->strcmp(entry->system_id, "pokemini"))
        cover_for_pokemini_rom(rom_path, entry->cover_path,
                               sizeof(entry->cover_path));
    if (entry->cover_path[0] == '\0')
        cover_for_system_id(entry->system_id,
                            entry->cover_path, sizeof(entry->cover_path));
    if (year && *year)
        rb->strlcpy(entry->year, year, sizeof(entry->year));
    if (genre && *genre)
        rb->strlcpy(entry->genre, genre, sizeof(entry->genre));
    if (publisher && *publisher)
        rb->strlcpy(entry->publisher, publisher, sizeof(entry->publisher));
    if (developer && *developer)
        rb->strlcpy(entry->developer, developer, sizeof(entry->developer));

    entry->flags = flags;
    entry->save_hint = save_hint;
    entry->system_index = launcher.current_system;
}

static bool game_entry_path_exists(const char *path)
{
    int i;

    for (i = 0; i < launcher.entry_count; i++)
    {
        if (!rb->strcmp(launcher.entries[i].rom_path, path))
            return true;
    }

    return false;
}

static bool copy_cover_if_exists(char *out, size_t out_size, const char *path)
{
    if (path && path[0] != '\0' && rb->file_exists(path))
    {
        rb->strlcpy(out, path, out_size);
        return true;
    }

    return false;
}

static bool cover_for_known_plugin(const char *path, const char *title,
                                   char *cover_path, size_t cover_path_size)
{
    char stem[MAX_ENTRY_TITLE];

    cover_path[0] = '\0';
    derive_stem_from_path(path, stem, sizeof(stem));

    rb->snprintf(cover_path, cover_path_size, "%s/native/%s.bmp",
                 GAME_LIBRARY_COVERS_DIR, stem);
    if (rb->file_exists(cover_path))
        return true;
    cover_path[0] = '\0';

    if (!rb->strcasecmp(stem, "clubpenguin") ||
        (title && !rb->strcasecmp(title, "Club Penguin")))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                    PLUGIN_GAMES_DIR "/clubpenguin/covers/Club Penguin.bmp") ||
               copy_cover_if_exists(cover_path, cover_path_size,
                    ROCKBOX_DIR "/ipodjs/clubpenguin/covers/Club Penguin.bmp");
    }

    if (!rb->strcasecmp(stem, "runescape_classic") ||
        (title && !rb->strcasecmp(title, "RuneScape Classic")))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    RUNESCAPE_CLASSIC_COVER_BMP) ||
               copy_cover_if_exists(cover_path, cover_path_size,
                    ROCKBOX_DIR "/ipodjs/runescape_classic/covers/RuneScape Classic.bmp");
    }

    if (!rb->strcasecmp(stem, "wwe_backstage") ||
        (title && !rb->strcasecmp(title, "WWE Backstage")))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    WWE_BACKSTAGE_COVER_BMP) ||
               copy_cover_if_exists(cover_path, cover_path_size,
                    PLUGIN_GAMES_DIR "/wwe_backstage/cover.bmp");
    }

    if (!rb->strcasecmp(stem, "doom") || !rb->strcasecmp(stem, "doom_play") ||
        (title && !rb->strcasecmp(title, "Doom")))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    DOOM_COVER_BMP) ||
               copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/doom.bmp");
    }

    if (!rb->strcasecmp(stem, "smsgg"))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                    PLUGIN_GAMES_DIR "/smsgg/covers/Sega Master System - Game Gear.bmp") ||
               copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/smsgg.bmp");
    }

    if (!rb->strcasecmp(stem, "pocketcatch"))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                    PLUGIN_GAMES_DIR "/pocketcatch/backgrounds/new_bark_town_hgss.bmp");
    }

    if (!rb->strcasecmp(stem, "pokemini") ||
        !rb->strcasecmp(stem, "pokemini_launcher"))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/pokemini.bmp");
    }

    if (!rb->strcasecmp(stem, "arduboy"))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/arduboy.bmp");
    }

    if (!rb->strcasecmp(stem, "tamagotchi"))
    {
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/tamagotchi.bmp");
    }

    return false;
}

static bool cover_for_pokemini_rom(const char *path,
                                   char *cover_path, size_t cover_path_size)
{
    char stem[MAX_ENTRY_TITLE];

    cover_path[0] = '\0';
    derive_stem_from_path(path, stem, sizeof(stem));

    rb->snprintf(cover_path, cover_path_size, "%s/%s.bmp",
                 POKEMINI_COVERS_DIR, stem);
    if (rb->file_exists(cover_path))
        return true;

    rb->snprintf(cover_path, cover_path_size, "%s/%s.png",
                 POKEMINI_COVERS_DIR, stem);
    if (rb->file_exists(cover_path))
        return true;

    cover_path[0] = '\0';
    return false;
}

static bool cover_for_system_id(const char *system_id,
                                char *cover_path, size_t cover_path_size)
{
    int i;

    if (!system_id || system_id[0] == '\0')
        return false;

    for (i = 0; i < launcher.system_count; i++)
    {
        if (!rb->strcmp(launcher.systems[i].id, system_id) &&
            copy_cover_if_exists(cover_path, cover_path_size,
                                 launcher.systems[i].cover_path))
        {
            return true;
        }
    }

    if (!rb->strcmp(system_id, "native"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/native.bmp");
    if (!rb->strcmp(system_id, "gameboy"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/gameboy.bmp");
    if (!rb->strcmp(system_id, "nes"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/nes.bmp");
    if (!rb->strcmp(system_id, "smsgg"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/smsgg.bmp");
    if (!rb->strcmp(system_id, "arduboy"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/arduboy.bmp");
    if (!rb->strcmp(system_id, "tamagotchi"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/tamagotchi.bmp");
    if (!rb->strcmp(system_id, "pokemini"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/pokemini.bmp");
    if (!rb->strcmp(system_id, "gwatch"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/gwatch.bmp");
    if (!rb->strcmp(system_id, "doom"))
        return copy_cover_if_exists(cover_path, cover_path_size,
                                    GAME_LIBRARY_COVERS_DIR "/systems/doom.bmp");

    return false;
}

static void add_builtin_game_entries(void)
{
    if (launcher.show_builtin_stickrpg &&
        rb->file_exists(FLASHPLAYER_PLUGIN_PATH) &&
        rb->file_exists(STICKRPG_SWF_PATH) &&
        !game_entry_path_exists(FLASHPLAYER_PLUGIN_PATH))
    {
        add_game_entry("Stick RPG", FLASHPLAYER_PLUGIN_PATH,
                       STICKRPG_COVER_BMP, "native", "",
                       FLAG_FAVORITE, SAVE_HINT_NO,
                       "2003", "Flash RPG", "XGen Studios", "XGen Studios",
                       STICKRPG_SWF_PATH);
    }

    if (launcher.show_builtin_doom &&
        rb->file_exists(DOOM_PLAY_PLUGIN_PATH) &&
        !game_entry_path_exists(DOOM_PLAY_PLUGIN_PATH))
    {
        add_game_entry("Doom", DOOM_PLAY_PLUGIN_PATH, DOOM_COVER_BMP,
                       "doom", "",
                       FLAG_FAVORITE, SAVE_HINT_NO,
                       "1993", "Shooter", "Rockbox", "Rockdoom", NULL);
    }

    if (rb->file_exists(WWE_BACKSTAGE_PLUGIN_PATH) &&
        rb->file_exists(WWE_BACKSTAGE_MANIFEST_PATH) &&
        !game_entry_path_exists(WWE_BACKSTAGE_PLUGIN_PATH))
    {
        add_game_entry("WWE Backstage", WWE_BACKSTAGE_PLUGIN_PATH,
                       WWE_BACKSTAGE_COVER_BMP, "native", "",
                       FLAG_FAVORITE, SAVE_HINT_NO,
                       "2015", "Interactive Video", "YouTube", "WWE",
                       WWE_BACKSTAGE_MANIFEST_PATH);
    }

    if (launcher.show_builtin_runescape &&
        rb->file_exists(RUNESCAPE_CLASSIC_PLUGIN_PATH) &&
        !game_entry_path_exists(RUNESCAPE_CLASSIC_PLUGIN_PATH))
    {
        add_game_entry("RuneScape Classic", RUNESCAPE_CLASSIC_PLUGIN_PATH,
                       RUNESCAPE_CLASSIC_COVER_BMP, "native", "",
                       FLAG_FAVORITE,
                       SAVE_HINT_NO, "2001", "RPG", "Jagex",
                       "Offline Lumbridge", NULL);
    }
}

static void detect_sidecar_cover(const char *rom_path, char *cover_path, size_t cover_path_size)
{
    static const char *cover_exts[] = { ".bmp", ".png", ".jpg", ".jpeg" };
    char base[MAX_PATH];
    char *ext;
    size_t i;

    rb->strlcpy(base, rom_path, sizeof(base));
    ext = rb->strrchr(base, '.');
    if (ext)
        *ext = '\0';

    for (i = 0; i < ARRAYLEN(cover_exts); i++)
    {
        rb->snprintf(cover_path, cover_path_size, "%s%s", base, cover_exts[i]);
        if (rb->file_exists(cover_path))
            return;
    }

    cover_path[0] = '\0';
}

static void scan_rom_dir(const char *dir_path, int depth)
{
    DIR *dir;
    struct dirent *entry;
    char child[MAX_PATH];

    if (depth > MAX_SCAN_DEPTH || launcher.entry_count >= launcher.entry_capacity)
        return;

    dir = rb->opendir(dir_path);
    if (!dir)
        return;

    while ((entry = rb->readdir(dir)) != NULL &&
           launcher.entry_count < launcher.entry_capacity)
    {
        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        rb->snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if (rb->dir_get_info(dir, entry).attribute & ATTR_DIRECTORY)
        {
            scan_rom_dir(child, depth + 1);
            continue;
        }

        if (has_supported_rom_ext(entry->d_name))
        {
            char cover[MAX_PATH];
            detect_sidecar_cover(child, cover, sizeof(cover));
            add_game_entry(NULL, child, cover, "gameboy", "",
                           0, SAVE_HINT_UNKNOWN, "", "", "", "", NULL);
        }
    }

    rb->closedir(dir);
}

static void scan_system_rom_dir(struct system_entry *system,
                                const char *dir_path, int depth)
{
    DIR *dir;
    struct dirent *entry;
    char child[MAX_PATH];

    if (depth > MAX_SCAN_DEPTH || launcher.entry_count >= launcher.entry_capacity)
        return;

    dir = rb->opendir(dir_path);
    if (!dir)
        return;

    while ((entry = rb->readdir(dir)) != NULL &&
           launcher.entry_count < launcher.entry_capacity)
    {
        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        rb->snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if (rb->dir_get_info(dir, entry).attribute & ATTR_DIRECTORY)
        {
            scan_system_rom_dir(system, child, depth + 1);
            continue;
        }

        if (has_extension_in_list(entry->d_name, system->extensions))
        {
            char cover[MAX_PATH];
            detect_sidecar_cover(child, cover, sizeof(cover));
            add_game_entry(NULL, child, cover, system->id, system->plugin_path,
                           0, SAVE_HINT_UNKNOWN, "", "", "", "", NULL);
        }
    }

    rb->closedir(dir);
}

static int count_system_files_quick(struct system_entry *system,
                                    const char *dir_path)
{
    DIR *dir;
    struct dirent *entry;
    char child[MAX_PATH];
    int count = 0;

    dir = rb->opendir(dir_path);
    if (!dir)
        return 0;

    while ((entry = rb->readdir(dir)) != NULL && count < MAX_SYSTEM_QUICK_COUNT)
    {
        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        rb->snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if ((rb->dir_get_info(dir, entry).attribute & ATTR_DIRECTORY) == 0 &&
            has_extension_in_list(entry->d_name, system->extensions))
        {
            count++;
        }
    }

    rb->closedir(dir);
    return count;
}

static bool load_games_from_system_manifest(struct system_entry *system)
{
    int fd;
    char line[1024];
    char index_path[MAX_PATH];
    char index_dir[MAX_PATH];
    char *last_slash;
    ssize_t len;

    rb->snprintf(index_path, sizeof(index_path), ROCKBOX_DIR "/games/%s/games.tsv",
                 system->id);
    fd = rb->open(index_path, O_RDONLY);
    if (fd < 0)
        return false;

    rb->strlcpy(index_dir, index_path, sizeof(index_dir));
    last_slash = rb->strrchr(index_dir, '/');
    if (last_slash)
        *last_slash = '\0';

    while ((len = rb->read_line(fd, line, sizeof(line))) > 0 &&
           launcher.entry_count < launcher.entry_capacity)
    {
        char *id;
        char *title;
        char *file;
        char *cover;
        char *favorite;
        char *last_played;
        char *haptic_profile;
        char *next;
        char resolved_file[MAX_PATH];
        char resolved_cover[MAX_PATH];
        unsigned char flags = 0;

        (void)len;
        id = trim_whitespace(line);
        if (id[0] == '\0' || id[0] == '#')
            continue;

        next = rb->strchr(id, '\t');
        if (!next)
            continue;
        *next++ = '\0';
        if (!rb->strcasecmp(id, "id"))
            continue;

        title = next;
        next = rb->strchr(next, '\t');
        if (!next)
            continue;
        *next++ = '\0';

        file = next;
        next = rb->strchr(next, '\t');
        if (next)
        {
            *next++ = '\0';
            cover = next;
            next = rb->strchr(next, '\t');
        }
        else
            cover = "";

        favorite = "";
        last_played = "";
        haptic_profile = "";
        if (next)
        {
            *next++ = '\0';
            favorite = next;
            next = rb->strchr(next, '\t');
            if (next)
            {
                *next++ = '\0';
                last_played = next;
                next = rb->strchr(next, '\t');
                if (next)
                {
                    *next++ = '\0';
                    haptic_profile = next;
                    next = rb->strchr(next, '\t');
                    if (next)
                        *next = '\0';
                }
            }
        }

        (void)last_played;
        make_path_absolute(index_dir, trim_whitespace(file),
                           resolved_file, sizeof(resolved_file));
        make_path_absolute(index_dir, trim_whitespace(cover),
                           resolved_cover, sizeof(resolved_cover));
        if (!rb->file_exists(resolved_file))
            continue;
        if (resolved_cover[0] != '\0' && !rb->file_exists(resolved_cover))
            resolved_cover[0] = '\0';
        if (resolved_cover[0] == '\0')
            detect_sidecar_cover(resolved_file, resolved_cover,
                                 sizeof(resolved_cover));
        if (parse_bool(trim_whitespace(favorite)))
            flags |= FLAG_FAVORITE;
        if (trim_whitespace(haptic_profile)[0] != '\0')
            system->has_haptics = true;

        add_game_entry(trim_whitespace(title), resolved_file, resolved_cover,
                       system->id, system->plugin_path, flags,
                       SAVE_HINT_UNKNOWN, "", "", "", "", NULL);
    }

    rb->close(fd);
    return launcher.entry_count > 0;
}

static bool load_games_from_index(void)
{
    int fd;
    char line[1024];
    char index_dir[MAX_PATH];
    char *last_slash;
    ssize_t len;

    fd = rb->open(ROCKBOY_INDEX_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    rb->strlcpy(index_dir, ROCKBOY_INDEX_PATH, sizeof(index_dir));
    last_slash = rb->strrchr(index_dir, '/');
    if (last_slash)
        *last_slash = '\0';

    while ((len = rb->read_line(fd, line, sizeof(line))) > 0 &&
           launcher.entry_count < launcher.entry_capacity)
    {
        char *title;
        char *rom_path;
        char *cover_path;
        char *favorite;
        char *save_hint;
        char *year;
        char *genre;
        char *publisher;
        char *developer;
        char *description;
        char *plugin_param;
        char *cursor;
        char *next;
        char resolved_rom[MAX_PATH];
        char resolved_cover[MAX_PATH];
        char resolved_plugin_param[MAX_PATH];
        unsigned char flags;

        cursor = trim_whitespace(line);
        if (cursor[0] == '\0' || cursor[0] == '#')
            continue;

        title = cursor;
        next = rb->strchr(cursor, '\t');
        if (!next)
            continue;
        *next++ = '\0';

        rom_path = next;
        next = rb->strchr(next, '\t');
        if (next)
        {
            *next++ = '\0';
            cover_path = next;
            next = rb->strchr(next, '\t');
        }
        else
        {
            cover_path = "";
        }

        favorite = "";
        save_hint = "";
        year = "";
        genre = "";
        publisher = "";
        developer = "";
        description = "";
        plugin_param = "";
        if (next)
        {
            *next++ = '\0';
            favorite = next;
            next = rb->strchr(next, '\t');
            if (next)
            {
                *next++ = '\0';
                save_hint = next;
                next = rb->strchr(next, '\t');
                if (next)
                {
                    *next++ = '\0';
                    year = next;
                    next = rb->strchr(next, '\t');
                    if (next)
                    {
                        *next++ = '\0';
                        genre = next;
                        next = rb->strchr(next, '\t');
                        if (next)
                        {
                            *next++ = '\0';
                            publisher = next;
                            next = rb->strchr(next, '\t');
                            if (next)
                            {
                                *next++ = '\0';
                                developer = next;
                                next = rb->strchr(next, '\t');
                                if (next)
                                {
                                    *next++ = '\0';
                                    description = next;
                                    next = rb->strchr(next, '\t');
                                    if (next)
                                    {
                                        *next++ = '\0';
                                        plugin_param = next;
                                        next = rb->strchr(next, '\t');
                                        if (next)
                                            *next = '\0';
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        title = trim_whitespace(title);
        rom_path = trim_whitespace(rom_path);
        cover_path = trim_whitespace(cover_path);
        favorite = trim_whitespace(favorite);
        save_hint = trim_whitespace(save_hint);
        year = trim_whitespace(year);
        genre = trim_whitespace(genre);
        publisher = trim_whitespace(publisher);
        developer = trim_whitespace(developer);
        description = trim_whitespace(description);
        plugin_param = trim_whitespace(plugin_param);
        (void)description;

        make_path_absolute(index_dir, rom_path, resolved_rom, sizeof(resolved_rom));
        make_path_absolute(index_dir, cover_path, resolved_cover, sizeof(resolved_cover));
        make_path_absolute(index_dir, plugin_param, resolved_plugin_param,
                           sizeof(resolved_plugin_param));
        if (!rb->file_exists(resolved_rom))
            continue;
        if (is_nes_rom(resolved_rom))
            continue;
        if (resolved_plugin_param[0] != '\0' &&
            !rb->file_exists(resolved_plugin_param))
            resolved_plugin_param[0] = '\0';
        if (resolved_cover[0] != '\0' && !rb->file_exists(resolved_cover))
            resolved_cover[0] = '\0';
        if (resolved_cover[0] == '\0')
            detect_sidecar_cover(resolved_rom, resolved_cover, sizeof(resolved_cover));

        flags = 0;
        if (parse_bool(favorite))
            flags |= FLAG_FAVORITE;

        add_game_entry(title, resolved_rom, resolved_cover, "gameboy", "",
                       flags,
                       parse_save_hint(save_hint), year, genre, publisher,
                       developer, resolved_plugin_param);
    }

    rb->close(fd);
    return launcher.entry_count > 0;
}

static int compare_entries(const void *a, const void *b)
{
    const struct game_entry *left = a;
    const struct game_entry *right = b;

    switch (launcher.sort_mode)
    {
        case SORT_FAVORITES_FIRST:
            if (!!(left->flags & FLAG_FAVORITE) != !!(right->flags & FLAG_FAVORITE))
                return (right->flags & FLAG_FAVORITE) ? 1 : -1;
            break;

        case SORT_SAVES_FIRST:
            if ((left->has_save > 0) != (right->has_save > 0))
                return (right->has_save > 0) ? 1 : -1;
            break;

        default:
            break;
    }

    return rb->strcasecmp(left->title, right->title);
}

static void restore_selection(void)
{
    int i;

    if (launcher.entry_count <= 0)
    {
        launcher.selected = 0;
        return;
    }

    if (launcher.state_rom[0] != '\0')
    {
        for (i = 0; i < launcher.entry_count; i++)
        {
            if (!rb->strcmp(launcher.entries[i].rom_path, launcher.state_rom))
            {
                launcher.selected = i;
                return;
            }
        }
    }

    if (launcher.selected >= launcher.entry_count)
        launcher.selected = launcher.entry_count - 1;
    if (launcher.selected < 0)
        launcher.selected = 0;
}

static void clear_cover_cache(void)
{
    int i;

    for (i = 0; i < COVER_SLOT_COUNT; i++)
    {
        launcher.cover_slots[i].entry_index = -1;
        launcher.cover_slots[i].path[0] = '\0';
        launcher.cover_slots[i].loaded = false;
        launcher.cover_slots[i].fallback = false;
        launcher.cover_slots[i].last_used = 0;
        launcher.cover_slots[i].bitmap.width = 0;
        launcher.cover_slots[i].bitmap.height = 0;
        launcher.cover_slots[i].bitmap.format = FORMAT_NATIVE;
        launcher.cover_slots[i].bitmap.data = (unsigned char *)launcher.cover_slots[i].data;
    }
}

static void reset_entry_list(void)
{
    launcher.entry_count = 0;
    launcher.total_count = 0;
    launcher.selected = 0;
    launcher.used_index = false;
    if (launcher.entries && launcher.entry_capacity > 0)
    {
        rb->memset(launcher.entries, 0,
                   (size_t)launcher.entry_capacity * sizeof(struct game_entry));
    }
}

static void add_system_card(struct system_entry *system, int index)
{
    char subtitle[MAX_SYSTEM_SUBTITLE];
    struct game_entry *entry;
    unsigned char flags = 0;

    if (launcher.entry_count >= launcher.entry_capacity)
        return;

    entry = &launcher.entries[launcher.entry_count++];
    rb->memset(entry, 0, sizeof(*entry));
    rb->strlcpy(entry->title, system->title, sizeof(entry->title));
    rb->strlcpy(entry->system_id, system->id, sizeof(entry->system_id));
    rb->strlcpy(entry->subtitle, system->subtitle, sizeof(entry->subtitle));
    rb->strlcpy(entry->rom_path, system->rom_path, sizeof(entry->rom_path));
    rb->strlcpy(entry->plugin_path, system->plugin_path, sizeof(entry->plugin_path));

    if (launcher.view_mode != VIEW_SYSTEMS &&
        system->cover_path[0] != '\0' && rb->file_exists(system->cover_path))
    {
        rb->strlcpy(entry->cover_path, system->cover_path, sizeof(entry->cover_path));
    }

    if (system->game_count == 0)
        flags |= FLAG_NEEDS_SETUP;

    rb->snprintf(subtitle, sizeof(subtitle), "%s", system->subtitle);
    if (system->game_count < 0)
        rb->strlcpy(entry->genre, "Open to scan", sizeof(entry->genre));
    else if (system->game_count == 1)
        rb->strlcpy(entry->genre, "1 game", sizeof(entry->genre));
    else if (system->game_count >= MAX_SYSTEM_QUICK_COUNT)
        rb->snprintf(entry->genre, sizeof(entry->genre), "%d+ games",
                     MAX_SYSTEM_QUICK_COUNT);
    else
        rb->snprintf(entry->genre, sizeof(entry->genre), "%d games",
                     system->game_count);
    rb->strlcpy(entry->publisher, subtitle, sizeof(entry->publisher));
    if (system->game_count == 0)
        rb->strlcpy(entry->developer, "Needs setup", sizeof(entry->developer));

    entry->flags = flags;
    entry->system_index = index;
    entry->is_system = true;
}

static void count_system_games(void)
{
    int i;

    for (i = 0; i < launcher.system_count; i++)
    {
        struct system_entry *system = &launcher.systems[i];

        system->game_count = 0;
        if (!system->enabled)
            continue;

        if (!rb->strcmp(system->id, "tamagotchi"))
        {
            system->game_count = tamagotchi_rom_available() ? 1 : 0;
        }
        else if (system->old_index)
        {
            system->game_count = count_system_files_quick(system,
                                                          system->rom_path);
            if (system->game_count == 0 &&
                (rb->file_exists(ROCKBOY_INDEX_PATH) ||
                 rb->file_exists(DOOM_PLAY_PLUGIN_PATH) ||
                 rb->file_exists(FLASHPLAYER_PLUGIN_PATH) ||
                 rb->file_exists(WWE_BACKSTAGE_PLUGIN_PATH) ||
                 rb->file_exists(RUNESCAPE_CLASSIC_PLUGIN_PATH)))
            {
                system->game_count = -1;
            }
        }
        else
        {
            if (system->native_plugins)
                system->game_count = -1;
            else
                system->game_count = count_system_files_quick(system,
                                                              system->rom_path);
            if (!rb->strcmp(system->id, "doom") &&
                system->game_count == 0 &&
                rb->file_exists(DOOM_PLAY_PLUGIN_PATH))
            {
                system->game_count = 1;
            }
        }
    }
}

static bool load_system_browser(void)
{
    int i;

    if (!launcher.entries && !allocate_launcher_buffers())
        return false;

    reset_entry_list();
    launcher.view_mode = VIEW_SYSTEMS;
    launcher.current_system = -1;
    count_system_games();

    for (i = 0; i < launcher.system_count; i++)
    {
        if (!launcher.systems[i].enabled)
            continue;
        if (!launcher.show_empty_systems && launcher.systems[i].game_count <= 0)
            continue;
        add_system_card(&launcher.systems[i], i);
    }

    launcher.total_count = launcher.entry_count;
    if (launcher.state_system[0] != '\0')
    {
        for (i = 0; i < launcher.entry_count; i++)
        {
            if (!rb->strcmp(launcher.entries[i].system_id,
                            launcher.state_system))
            {
                launcher.selected = i;
                break;
            }
        }
    }
    if (launcher.selected >= launcher.entry_count)
        launcher.selected = launcher.entry_count - 1;
    if (launcher.selected < 0)
        launcher.selected = 0;
    request_cover_cache_warm();
    return launcher.entry_count > 0;
}

static void reset_cover_slot(struct cover_slot *slot)
{
    slot->entry_index = -1;
    slot->path[0] = '\0';
    slot->loaded = false;
    slot->fallback = false;
    slot->bitmap.width = 0;
    slot->bitmap.height = 0;
    slot->bitmap.format = FORMAT_NATIVE;
    slot->bitmap.data = (unsigned char *)slot->data;
}

static int interpolate_value(int from, int to, int progress)
{
    return from + (((to - from) * progress) / 256);
}

static int ease_flow_progress(int progress)
{
    int t;

    if (progress <= 0)
        return 0;
    if (progress >= 256)
        return 256;

    t = progress;
    return (t * t * (768 - (t << 1))) >> 16;
}

static int pose_focus(enum flow_pose pose)
{
    switch (pose)
    {
        case FLOW_LEFT:
        case FLOW_FAR_LEFT:
        case FLOW_OFF_LEFT:
            return -FLOW_FOCUS_LIMIT;
        case FLOW_RIGHT:
        case FLOW_FAR_RIGHT:
        case FLOW_OFF_RIGHT:
            return FLOW_FOCUS_LIMIT;
        default:
            return 0;
    }
}

static int cover_crop_width(const struct cover_slot *slot, int focus)
{
    int crop_w;
    int focus_abs;

    focus_abs = abs(focus);
    crop_w = slot->bitmap.width - ((slot->bitmap.width * focus_abs) / (FLOW_FOCUS_LIMIT * 3));
    if (crop_w < slot->bitmap.width / 2)
        crop_w = slot->bitmap.width / 2;
    if (crop_w > slot->bitmap.width)
        crop_w = slot->bitmap.width;
    return crop_w;
}

static int cover_pose_width(enum flow_pose pose, int crop_w)
{
    switch (pose)
    {
        case FLOW_LEFT:
        case FLOW_RIGHT:
            return (crop_w * 72) / 100;
        case FLOW_FAR_LEFT:
        case FLOW_FAR_RIGHT:
            return (crop_w * 58) / 100;
        case FLOW_OFF_LEFT:
        case FLOW_OFF_RIGHT:
            return (crop_w * 44) / 100;
        default:
            return crop_w;
    }
}

static int cover_pose_height(enum flow_pose pose, const struct cover_slot *slot)
{
    switch (pose)
    {
        case FLOW_LEFT:
        case FLOW_RIGHT:
            return (slot->bitmap.height * 82) / 100;
        case FLOW_FAR_LEFT:
        case FLOW_FAR_RIGHT:
            return (slot->bitmap.height * 68) / 100;
        case FLOW_OFF_LEFT:
        case FLOW_OFF_RIGHT:
            return (slot->bitmap.height * 56) / 100;
        default:
            return slot->bitmap.height;
    }
}

static int placeholder_pose_height(enum flow_pose pose)
{
    switch (pose)
    {
        case FLOW_LEFT:
        case FLOW_RIGHT:
            return (launcher.cover_box_h * 82) / 100;
        case FLOW_FAR_LEFT:
        case FLOW_FAR_RIGHT:
        case FLOW_OFF_LEFT:
        case FLOW_OFF_RIGHT:
            return (launcher.cover_box_h * 68) / 100;
        default:
            return launcher.cover_box_h;
    }
}

static int cover_pose_draw_x(enum flow_pose pose, int draw_w)
{
    int side_tuck;
    int side_stride;
    int near_left_x;
    int near_right_x;
    int far_left_x;
    int far_right_x;

    side_tuck = launcher.cover_box_w / 8;
    if (side_tuck < 14)
        side_tuck = 14;
    if (side_tuck > 22)
        side_tuck = 22;

    side_stride = launcher.side_cover_w * 3 / 4;
    if (side_stride < 34)
        side_stride = 34;

    near_left_x = launcher.cover_box_x - draw_w + side_tuck;
    near_right_x = launcher.cover_box_x + launcher.cover_box_w - side_tuck;
    far_left_x = near_left_x - side_stride;
    far_right_x = near_right_x + side_stride;

    switch (pose)
    {
        case FLOW_LEFT:
            return near_left_x;
        case FLOW_FAR_LEFT:
            return far_left_x;
        case FLOW_RIGHT:
            return near_right_x;
        case FLOW_FAR_RIGHT:
            return far_right_x;
        case FLOW_OFF_LEFT:
            return far_left_x - side_stride;
        case FLOW_OFF_RIGHT:
            return far_right_x + side_stride;
        default:
            return launcher.cover_box_x + (launcher.cover_box_w - draw_w) / 2;
    }
}

static int cover_pose_draw_y(enum flow_pose pose, int draw_h)
{
    int side_y;
    int far_y;

    side_y = launcher.cover_box_y + 18;
    far_y = side_y + 8;
    if (pose == FLOW_CENTER)
        return launcher.cover_box_y + (launcher.cover_box_h - draw_h) / 2;
    if (pose == FLOW_FAR_LEFT || pose == FLOW_FAR_RIGHT ||
        pose == FLOW_OFF_LEFT || pose == FLOW_OFF_RIGHT)
    {
        return far_y + (launcher.side_cover_h - draw_h) / 2;
    }

    return side_y + (launcher.side_cover_h - draw_h) / 2;
}

static fb_data blend_to_background(fb_data pixel, unsigned alpha)
{
    unsigned r;
    unsigned g;
    unsigned b;
    fb_data bg = launcher_bg_color();

    if (alpha >= 255)
        return pixel;

    r = (FB_UNPACK_RED(pixel) * alpha +
         FB_UNPACK_RED(bg) * (255 - alpha)) / 255;
    g = (FB_UNPACK_GREEN(pixel) * alpha +
         FB_UNPACK_GREEN(bg) * (255 - alpha)) / 255;
    b = (FB_UNPACK_BLUE(pixel) * alpha +
         FB_UNPACK_BLUE(bg) * (255 - alpha)) / 255;
    return FB_RGBPACK(r, g, b);
}

static bool build_reflection_bitmap(const struct bitmap *source,
                                    int src_x, int src_w, int alpha_scale)
{
    fb_data *src;
    fb_data *dst;
    int out_h;
    int row;
    int col;

    if (!source || src_w <= 0 || alpha_scale <= 0)
        return false;

    out_h = source->height / 3;
    if (out_h > launcher.reflection_max_h)
        out_h = launcher.reflection_max_h;
    if (out_h <= 0)
        return false;

    if ((size_t)src_w * out_h * sizeof(fb_data) > launcher.reflection_bytes)
        return false;

    src = (fb_data *)source->data;
    dst = launcher.reflection_data;
    for (row = 0; row < out_h; row++)
    {
        int sample_y = source->height - 1 - ((row * source->height) / (out_h * 2));
        unsigned row_alpha = (unsigned)((alpha_scale * (out_h - row)) / out_h);

        if (sample_y < 0)
            sample_y = 0;

        for (col = 0; col < src_w; col++)
        {
            dst[row * src_w + col] = blend_to_background(
                src[sample_y * source->width + src_x + col],
                row_alpha
            );
        }
    }

    launcher.reflection_bitmap.width = src_w;
    launcher.reflection_bitmap.height = out_h;
    launcher.reflection_bitmap.format = FORMAT_NATIVE;
    launcher.reflection_bitmap.data = (unsigned char *)launcher.reflection_data;
    return true;
}

static bool allocate_launcher_buffers(void)
{
    unsigned char *buffer;
    size_t buffer_size;
    size_t entry_bytes;
    size_t remaining;
    size_t slot_bytes;
    size_t scratch_bytes;
    size_t reflection_bytes;
    size_t posed_bytes;
    int i;

    buffer = rb->plugin_get_buffer(&buffer_size);
    if (!buffer || buffer_size < (64 * 1024))
        return false;

    slot_bytes = launcher.cache_cover_w * launcher.cache_cover_h * sizeof(fb_data);
    reflection_bytes = launcher.cache_cover_w * launcher.reflection_max_h * sizeof(fb_data);
    posed_bytes = slot_bytes;
    scratch_bytes = (LCD_WIDTH >= 320) ? (160 * 1024) : (96 * 1024);
    if (slot_bytes == 0 || reflection_bytes == 0 ||
        buffer_size < scratch_bytes + reflection_bytes + posed_bytes +
                      slot_bytes * (COVER_SLOT_COUNT + 1))
        return false;

    launcher.decode_scratch_bytes = scratch_bytes;
    launcher.decode_scratch_data = (fb_data *)buffer;
    buffer += launcher.decode_scratch_bytes;
    buffer_size -= launcher.decode_scratch_bytes;

    launcher.reflection_bytes = reflection_bytes;
    launcher.reflection_data = (fb_data *)buffer;
    buffer += launcher.reflection_bytes;
    buffer_size -= launcher.reflection_bytes;

    launcher.posed_bytes = posed_bytes;
    launcher.posed_data = (fb_data *)buffer;
    buffer += launcher.posed_bytes;
    buffer_size -= launcher.posed_bytes;

    launcher.fallback_cover_bytes = slot_bytes;
    launcher.fallback_cover_data = (fb_data *)buffer;
    buffer += launcher.fallback_cover_bytes;
    buffer_size -= launcher.fallback_cover_bytes;

    for (i = 0; i < COVER_SLOT_COUNT; i++)
    {
        launcher.cover_slots[i].data = (fb_data *)buffer;
        launcher.cover_slots[i].bytes = slot_bytes;
        launcher.cover_slots[i].bitmap.data = (unsigned char *)launcher.cover_slots[i].data;
        launcher.cover_slots[i].bitmap.format = FORMAT_NATIVE;
        launcher.cover_slots[i].entry_index = -1;

        buffer += slot_bytes;
        buffer_size -= slot_bytes;
    }

    entry_bytes = sizeof(struct game_entry);
    launcher.entry_capacity = (int)(buffer_size / entry_bytes);
    if (launcher.entry_capacity < MIN_ENTRY_CAPACITY)
        return false;

    remaining = (size_t)launcher.entry_capacity * entry_bytes;
    launcher.entries = (struct game_entry *)buffer;
    rb->memset(launcher.entries, 0, remaining);
    launcher.fallback_cover.width = 0;
    launcher.fallback_cover.height = 0;
    launcher.fallback_cover.format = FORMAT_NATIVE;
    launcher.fallback_cover.data = (unsigned char *)launcher.fallback_cover_data;
    launcher.reflection_bitmap.width = 0;
    launcher.reflection_bitmap.height = 0;
    launcher.reflection_bitmap.format = FORMAT_NATIVE;
    launcher.reflection_bitmap.data = (unsigned char *)launcher.reflection_data;
    launcher.posed_bitmap.width = 0;
    launcher.posed_bitmap.height = 0;
    launcher.posed_bitmap.format = FORMAT_NATIVE;
    launcher.posed_bitmap.data = (unsigned char *)launcher.posed_data;
    launcher.decode_scratch.width = 0;
    launcher.decode_scratch.height = 0;
    launcher.decode_scratch.format = FORMAT_NATIVE;
    launcher.decode_scratch.data = (unsigned char *)launcher.decode_scratch_data;
    launcher.fallback_cover_loaded = false;
    launcher.cover_use_clock = 0;
    clear_cover_cache();

    return true;
}

static bool load_game_library(void)
{
    if (!launcher.entries && !allocate_launcher_buffers())
        return false;

    load_launcher_config();
    load_launcher_state();
    if (!load_system_library())
        return false;
    return load_system_browser();
}

static bool load_system_games(int system_index)
{
    struct system_entry *system;

    if (system_index < 0 || system_index >= launcher.system_count)
        return false;

    system = &launcher.systems[system_index];
    clear_cover_cache();
    reset_entry_list();
    launcher.view_mode = VIEW_GAMES;
    launcher.current_system = system_index;

    if (!rb->strcmp(system->id, "tamagotchi"))
    {
        if (tamagotchi_rom_available())
        {
            add_game_entry("Tamagotchi", tamagotchi_rom_path(),
                           system->cover_path, system->id, system->plugin_path,
                           0, SAVE_HINT_YES, "", "Virtual pet",
                           "Bandai", "TamaLIB", NULL);
        }
    }
    else if (system->old_index)
    {
        launcher.used_index = load_games_from_index();
        if (!launcher.used_index)
            scan_rom_dir(ROCKBOY_ROM_DIR, 0);
        add_builtin_game_entries();
    }
    else
    {
        launcher.used_index = load_games_from_system_manifest(system);
        if (!launcher.used_index)
            scan_system_rom_dir(system, system->rom_path, 0);
        if (!rb->strcmp(system->id, "doom") &&
            launcher.entry_count == 0 &&
            rb->file_exists(DOOM_PLAY_PLUGIN_PATH))
        {
            add_game_entry("Doom Setup", DOOM_PLAY_PLUGIN_PATH,
                           DOOM_COVER_BMP, system->id, "",
                           FLAG_FAVORITE, SAVE_HINT_NO,
                           "1993", "Shooter", "Rockbox", "Rockdoom", NULL);
        }
    }

    prime_game_metadata();
    apply_launcher_filter();
    if (launcher.entry_count > 1)
    {
        rb->qsort(launcher.entries, launcher.entry_count,
                  sizeof(struct game_entry), compare_entries);
    }
    restore_selection();
    load_selected_cover_quiet();
    return launcher.entry_count > 0;
}

static bool reload_game_library(void)
{
    clear_cover_cache();
    load_launcher_state();
    if (launcher.view_mode == VIEW_GAMES)
    {
        if (!load_system_games(launcher.current_system))
            return false;
    }
    else if (!load_system_library() || !load_system_browser())
    {
        return false;
    }

    request_cover_cache_warm();
    return launcher.entry_count > 0;
}

static bool reload_game_library_with_current_modes(void)
{
    if (!reload_game_library())
        return false;

    load_selected_cover_quiet();
    return true;
}

static void truncate_to_width(const char *src, char *dst, size_t dst_size, int max_width)
{
    int width;
    size_t len;

    if (dst_size == 0)
        return;

    rb->strlcpy(dst, src, dst_size);
    rb->lcd_getstringsize(dst, &width, NULL);
    if (width <= max_width)
        return;

    len = rb->strlen(dst);
    while (len > 3)
    {
        dst[--len] = '\0';
        dst[len - 1] = '.';
        dst[len - 2] = '.';
        dst[len - 3] = '.';
        rb->lcd_getstringsize(dst, &width, NULL);
        if (width <= max_width)
            return;
    }
}

static void launcher_layout_init(void)
{
    int font_height;
    int margin;
    int detail_rows_h;
    int detail_top_max;
    int detail_gap;

    margin = 8;

    rb->viewportmanager_theme_enable(SCREEN_MAIN, false, NULL);
    rb->viewport_set_fullscreen(&launcher.vp, SCREEN_MAIN);

    rb->lcd_setfont(FONT_UI);
    rb->lcd_getstringsize("Games", NULL, &font_height);
    launcher.line_height = font_height + 6;

    launcher.cover_box_w = launcher.vp.width / 2 - 34;
    if (launcher.cover_box_w > 128)
        launcher.cover_box_w = 128;
    if (launcher.cover_box_w < 96)
        launcher.cover_box_w = 96;

    detail_rows_h = launcher.line_height * 3;
    detail_gap = margin - 2;

    launcher.cover_box_h = launcher.vp.height / 2 + 4;
    if (launcher.cover_box_h > 140)
        launcher.cover_box_h = 140;
    if (launcher.cover_box_h < 96)
        launcher.cover_box_h = 96;

    launcher.side_cover_w = (launcher.cover_box_w * 58) / 100;
    if (launcher.side_cover_w < 38)
        launcher.side_cover_w = 38;

    launcher.side_cover_h = launcher.cover_box_h - 34;
    if (launcher.side_cover_h < 72)
        launcher.side_cover_h = 72;

    launcher.cache_cover_w = launcher.cover_box_w;
    launcher.cache_cover_h = launcher.cover_box_h;
    launcher.reflection_gap = FLOW_REFLECTION_GAP;
    launcher.reflection_max_h = launcher.cover_box_h / 4;
    if (launcher.reflection_max_h < 18)
        launcher.reflection_max_h = 18;
    if (launcher.reflection_max_h > 34)
        launcher.reflection_max_h = 34;

    detail_top_max = launcher.vp.height - detail_rows_h - 2;
    while (margin + launcher.line_height + 4 + launcher.cover_box_h +
           launcher.reflection_gap + launcher.reflection_max_h + detail_gap >
           detail_top_max &&
           launcher.cover_box_h > 96)
    {
        launcher.cover_box_h -= 4;
        launcher.reflection_max_h = launcher.cover_box_h / 5;
        if (launcher.reflection_max_h < 14)
            launcher.reflection_max_h = 14;
        if (launcher.reflection_max_h > 28)
            launcher.reflection_max_h = 28;
    }

    launcher.cover_box_x = (launcher.vp.width - launcher.cover_box_w) / 2;
    launcher.cover_box_y = margin + launcher.line_height + 4;
    launcher.detail_y = launcher.cover_box_y + launcher.cover_box_h +
                        launcher.reflection_gap + launcher.reflection_max_h +
                        detail_gap;
    if (launcher.detail_y > detail_top_max)
        launcher.detail_y = detail_top_max;
    launcher.detail_w = launcher.vp.width - margin * 2;
}

static bool decode_cover_bitmap(const char *path, struct bitmap *bitmap,
                                fb_data *data, size_t data_bytes)
{
    int rc;
    size_t copy_bytes;

    if (!path || !*path || !has_supported_cover_ext(path))
        return false;

    if (is_bmp_cover(path))
    {
        bitmap->data = (unsigned char *)data;
        bitmap->format = FORMAT_NATIVE;
        rc = rb->read_bmp_file(path, bitmap, (int)data_bytes, FORMAT_NATIVE, NULL);
        if (rc > 0 && bitmap->width > 0 && bitmap->height > 0)
            return true;
    }

    launcher.decode_scratch.width = launcher.cache_cover_w;
    launcher.decode_scratch.height = launcher.cache_cover_h;
    launcher.decode_scratch.format = FORMAT_NATIVE;
    launcher.decode_scratch.data = (unsigned char *)launcher.decode_scratch_data;
    rc = read_image_file(path, &launcher.decode_scratch, (int)launcher.decode_scratch_bytes,
                         FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT | FORMAT_DITHER,
                         NULL);
    if (rc <= 0 || launcher.decode_scratch.width <= 0 || launcher.decode_scratch.height <= 0)
        return false;

    copy_bytes = BM_SIZE(launcher.decode_scratch.width, launcher.decode_scratch.height,
                         FORMAT_NATIVE, false);
    if (copy_bytes > data_bytes)
        return false;

    rb->memcpy(data, launcher.decode_scratch.data, copy_bytes);
    bitmap->width = launcher.decode_scratch.width;
    bitmap->height = launcher.decode_scratch.height;
    bitmap->format = FORMAT_NATIVE;
    bitmap->data = (unsigned char *)data;
    return true;
}

static struct cover_slot *find_cover_slot(int entry_index)
{
    int i;

    for (i = 0; i < COVER_SLOT_COUNT; i++)
    {
        if (launcher.cover_slots[i].entry_index == entry_index)
            return &launcher.cover_slots[i];
    }

    return NULL;
}

static struct cover_slot *peek_cover_slot(int entry_index)
{
    struct cover_slot *slot;

    if (entry_index < 0 || entry_index >= launcher.entry_count)
        return NULL;

    slot = find_cover_slot(entry_index);
    if (slot)
        slot->last_used = ++launcher.cover_use_clock;
    return slot;
}

static struct cover_slot *choose_cover_slot(int entry_index)
{
    int i;
    int best_index;
    int best_distance;
    unsigned int best_age;

    for (i = 0; i < COVER_SLOT_COUNT; i++)
    {
        if (launcher.cover_slots[i].entry_index < 0)
            return &launcher.cover_slots[i];
    }

    best_index = 0;
    best_distance = -1;
    best_age = 0;
    for (i = 0; i < COVER_SLOT_COUNT; i++)
    {
        int distance = launcher.cover_slots[i].entry_index - entry_index;
        if (distance < 0)
            distance = -distance;

        if (distance > best_distance ||
            (distance == best_distance && launcher.cover_slots[i].last_used < best_age))
        {
            best_index = i;
            best_distance = distance;
            best_age = launcher.cover_slots[i].last_used;
        }
    }

    return &launcher.cover_slots[best_index];
}

static struct cover_slot *get_cover_slot(int entry_index)
{
    struct cover_slot *slot;
    const char *path;

    if (entry_index < 0 || entry_index >= launcher.entry_count)
        return NULL;

    slot = find_cover_slot(entry_index);
    if (!slot)
    {
        slot = choose_cover_slot(entry_index);
        reset_cover_slot(slot);
        slot->entry_index = entry_index;
        path = launcher.entries[entry_index].cover_path;

        if (path[0] != '\0')
            rb->strlcpy(slot->path, path, sizeof(slot->path));

        if (path[0] != '\0' &&
            decode_cover_bitmap(path, &slot->bitmap, slot->data, slot->bytes))
        {
            slot->loaded = true;
            slot->fallback = false;
        }
    }

    slot->last_used = ++launcher.cover_use_clock;
    return slot;
}

static void request_cover_cache_warm(void)
{
    if (launcher.view_mode == VIEW_SYSTEMS)
    {
        launcher.cache_warm_center = -1;
        launcher.cache_warm_step = COVER_CACHE_RADIUS * 2 + 1;
        return;
    }

    launcher.cache_warm_center = launcher.selected;
    launcher.cache_warm_step = 0;
}

static void warm_cover_cache_step(int budget)
{
    if (launcher.view_mode == VIEW_SYSTEMS || launcher.entry_count <= 0 ||
        budget <= 0)
        return;

    while (budget > 0)
    {
        int step = launcher.cache_warm_step;
        int offset;
        int index;

        if (step == 0)
            offset = 0;
        else
        {
            int distance = (step + 1) / 2;
            if (distance > COVER_CACHE_RADIUS)
                break;
            offset = (step & 1) ? -distance : distance;
        }

        launcher.cache_warm_step++;
        index = launcher.cache_warm_center + offset;
        if (index < 0 || index >= launcher.entry_count)
            continue;
        if (find_cover_slot(index))
            continue;

        get_cover_slot(index);
        budget--;
    }
}

static void draw_loading_splashscreen(void)
{
    struct bitmap background;
    int ret;

    if (!launcher.decode_scratch_data || launcher.decode_scratch_bytes == 0)
        return;

    rb->memset(&background, 0, sizeof(background));
    background.width = LCD_WIDTH;
    background.height = LCD_HEIGHT;
    background.format = FORMAT_NATIVE;
    background.data = (unsigned char *)launcher.decode_scratch_data;

    ret = rb->read_bmp_file(ROCKBOY_LOADING_BACKGROUND_BMP, &background,
                            (int)launcher.decode_scratch_bytes,
                            FORMAT_NATIVE, NULL);
    if (ret > 0)
    {
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_clear_display();
        rb->screens[SCREEN_MAIN]->bitmap(background.data, 0, 0,
                                         background.width, background.height);
    }
    else
    {
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_clear_display();
    }

    return;
}

static struct bitmap *prepare_cover_pose_bitmap(struct cover_slot *slot,
                                                int crop_w, int draw_w, int draw_h)
{
    struct bitmap source_bitmap;
    int crop_x;
    size_t crop_bytes;

    if (!slot || !slot->loaded || draw_w <= 0 || draw_h <= 0 || crop_w <= 0)
        return NULL;

    if (crop_w == slot->bitmap.width &&
        draw_w == slot->bitmap.width &&
        draw_h == slot->bitmap.height)
    {
        return &((struct cover_slot *)slot)->bitmap;
    }

    if ((size_t)draw_w * draw_h * sizeof(fb_data) > launcher.posed_bytes)
        return &slot->bitmap;

    source_bitmap = slot->bitmap;
    if (crop_w < slot->bitmap.width)
    {
        fb_data *src;
        fb_data *dst;
        int row;

        crop_x = (slot->bitmap.width - crop_w) / 2;
        crop_bytes = (size_t)crop_w * slot->bitmap.height * sizeof(fb_data);
        if (crop_bytes > launcher.decode_scratch_bytes)
            return &slot->bitmap;

        src = (fb_data *)slot->bitmap.data;
        dst = launcher.decode_scratch_data;
        for (row = 0; row < slot->bitmap.height; row++)
        {
            rb->memcpy(&dst[row * crop_w],
                       &src[row * slot->bitmap.width + crop_x],
                       (size_t)crop_w * sizeof(fb_data));
        }

        source_bitmap.width = crop_w;
        source_bitmap.data = (unsigned char *)launcher.decode_scratch_data;
    }

    launcher.posed_bitmap.width = draw_w;
    launcher.posed_bitmap.height = draw_h;
    launcher.posed_bitmap.format = FORMAT_NATIVE;
    launcher.posed_bitmap.data = (unsigned char *)launcher.posed_data;
    smooth_resize_bitmap(&source_bitmap, &launcher.posed_bitmap);
    return &launcher.posed_bitmap;
}

static void draw_loading_progress(int step, int count, const char *msg)
{
    static int text_w;
    static int text_h;
    static int y = 0;
    const int width = LCD_WIDTH - (ROCKBOY_LOADING_BAR_MARGIN * 2);
    const int x = ROCKBOY_LOADING_BAR_MARGIN;
    int fill_width;

    if (count <= 0)
        count = 1;
    if (step < 0)
        step = 0;
    if (step > count)
        step = count;

    if (msg && *msg)
    {
        draw_loading_splashscreen();
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_getstringsize(msg, &text_w, &text_h);
        y = (LCD_HEIGHT - text_h) / 2;
        rb->lcd_putsxy((LCD_WIDTH - text_w) / 2, y, msg);
        y += text_h + 5;
    }
    else if (y <= 0)
    {
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_clear_display();
        y = (LCD_HEIGHT - ROCKBOY_LOADING_BAR_HEIGHT) / 2;
    }

    rb->lcd_set_foreground(LCD_DARKGRAY);
    rb->lcd_drawrect(x, y, width + 2, ROCKBOY_LOADING_BAR_HEIGHT);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(165, 231, 82));
#else
    rb->lcd_set_foreground(LCD_WHITE);
#endif
    fill_width = (step * width) / count;
    if (fill_width > 0)
    {
        rb->lcd_fillrect(x + 1, y + 1,
                         fill_width, ROCKBOY_LOADING_BAR_HEIGHT - 2);
    }
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_update();
    rb->yield();
}

static void load_selected_cover_with_progress(const char *msg)
{
    if (launcher.entry_count <= 0)
        return;
    if (launcher.view_mode == VIEW_SYSTEMS)
        return;

    draw_loading_progress(0, 1, msg);
    get_cover_slot(launcher.selected);
    draw_loading_progress(1, 1, NULL);
    request_cover_cache_warm();
}

static void load_selected_cover_quiet(void)
{
    if (launcher.entry_count <= 0 || launcher.view_mode == VIEW_SYSTEMS)
        return;

    get_cover_slot(launcher.selected);
    request_cover_cache_warm();
}

static const struct bitmap *system_art_for_id(const char *system_id)
{
    if (!rb->strcmp(system_id, "gameboy"))
        return &bm_game_system_gameboy;
    if (!rb->strcmp(system_id, "nes"))
        return &bm_game_system_nes;
    if (!rb->strcmp(system_id, "smsgg"))
        return &bm_game_system_smsgg;
    if (!rb->strcmp(system_id, "arduboy"))
        return &bm_game_system_arduboy;
    if (!rb->strcmp(system_id, "tamagotchi"))
        return &bm_game_system_tamagotchi;
    if (!rb->strcmp(system_id, "pokemini"))
        return &bm_game_system_pokemini;
    if (!rb->strcmp(system_id, "gwatch"))
        return &bm_game_system_gwatch;
    if (!rb->strcmp(system_id, "doom"))
        return &bm_game_system_doom;
    if (!rb->strcmp(system_id, "native"))
        return &bm_game_system_native;
    return NULL;
}

static void draw_system_badge(int x, int y, int w, int h,
                              const struct game_entry *entry, bool selected)
{
    char initials[4];
    int len = 0;
    int text_w;
    int text_h;
    const char *p = entry->title;
    const struct bitmap *art = system_art_for_id(entry->system_id);

    if (art != NULL)
    {
        rb->lcd_bmp_part(art, 0, 0,
                         x + (w - art->width) / 2,
                         y + (h - art->height) / 2,
                         art->width, art->height);
        return;
    }

    while (*p && len < 3)
    {
        while (*p == ' ' || *p == '&' || *p == '/')
            p++;
        if (*p)
            initials[len++] = (char)toupper((unsigned char)*p++);
        while (*p && *p != ' ' && *p != '&' && *p != '/')
            p++;
    }
    initials[len] = '\0';
    if (initials[0] == '\0')
        rb->strlcpy(initials, "SYS", sizeof(initials));

    rb->lcd_set_foreground(selected ? LCD_RGBPACK(92, 86, 110) :
                           LCD_RGBPACK(180, 182, 188));
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(selected ? LCD_WHITE : LCD_RGBPACK(60, 60, 64));
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_getstringsize(initials, &text_w, &text_h);
    rb->lcd_putsxy(x + (w - text_w) / 2, y + (h - text_h) / 2, initials);
}

static void draw_system_row(int index, int y, int row_h)
{
    struct game_entry *entry = &launcher.entries[index];
    bool selected = index == launcher.selected;
    char line[96];
    char title[MAX_ENTRY_TITLE];
    int text_w;
    int badge_w = 50;
    int margin = 8;
    int text_x = margin + badge_w + 10;
    int text_max = launcher.vp.width - text_x - margin;

    if (selected)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(104, 96, 128));
        rb->lcd_fillrect(4, y + 4, 3, row_h - 8);
        rb->lcd_set_foreground(launcher_selected_outline_color());
        rb->lcd_drawrect(4, y - 2, launcher.vp.width - 8, row_h);
    }

    draw_system_badge(margin, y + 4, badge_w, row_h - 8, entry, selected);

    rb->lcd_set_foreground(selected ? launcher_selected_text_color() :
                           launcher_fg_color());
    truncate_to_width(entry->title, title, sizeof(title), text_max);
    rb->lcd_putsxy(text_x, y + 2, title);

    rb->lcd_set_foreground(selected ? launcher_selected_muted_color() :
                           launcher_muted_fg_color());
    rb->snprintf(line, sizeof(line), "%s", entry->publisher);
    truncate_to_width(line, title, sizeof(title), text_max);
    rb->lcd_putsxy(text_x, y + launcher.line_height, title);

    rb->snprintf(line, sizeof(line), "%s", entry->genre);
    truncate_to_width(line, title, sizeof(title), text_max);
    rb->lcd_getstringsize(title, &text_w, NULL);
    rb->lcd_putsxy(launcher.vp.width - margin - text_w,
                   y + launcher.line_height, title);
}

static void draw_system_browser_screen(void)
{
    struct viewport *last_vp;
    struct screen *display;
    int row_h;
    int visible;
    int first;
    int i;
    char line[40];
    int text_w;

    if (launcher.entry_count <= 0)
        return;

    display = rb->screens[SCREEN_MAIN];
    last_vp = rb->lcd_set_viewport(&launcher.vp);

    rb->lcd_set_background(launcher_bg_color());
    rb->lcd_set_foreground(launcher_fg_color());
    display->clear_viewport();

    rb->lcd_set_foreground(launcher_fg_color());
    rb->lcd_putsxy(8, 4, "Systems");
    rb->snprintf(line, sizeof(line), "%d / %d",
                 launcher.selected + 1, launcher.entry_count);
    rb->lcd_getstringsize(line, &text_w, NULL);
    rb->lcd_putsxy(launcher.vp.width - 8 - text_w, 4, line);

    row_h = (launcher.vp.height - launcher.line_height - 12) / 3;
    if (row_h < launcher.line_height * 2 + 8)
        row_h = launcher.line_height * 2 + 8;

    visible = (launcher.vp.height - launcher.line_height - 10) / row_h;
    if (visible < 1)
        visible = 1;

    first = launcher.selected - visible / 2;
    if (first < 0)
        first = 0;
    if (first + visible > launcher.entry_count)
        first = launcher.entry_count - visible;
    if (first < 0)
        first = 0;

    for (i = 0; i < visible && first + i < launcher.entry_count; i++)
        draw_system_row(first + i, launcher.line_height + 8 + i * row_h, row_h);

    rb->lcd_set_viewport(last_vp);
    rb->lcd_update();
}

static void draw_flow_cover_pose(const struct game_entry *entry, struct cover_slot *slot,
                                 enum flow_pose from_pose, enum flow_pose to_pose,
                                 int progress)
{
    int focus;
    int crop_w;
    int draw_w;
    int draw_h;
    int draw_x;
    int draw_y;
    int reflection_alpha;
    struct bitmap *draw_bitmap;

    progress = ease_flow_progress(progress);
    draw_w = interpolate_value(
        cover_pose_width(from_pose, launcher.cover_box_w),
        cover_pose_width(to_pose, launcher.cover_box_w),
        progress
    );
    draw_h = interpolate_value(
        placeholder_pose_height(from_pose),
        placeholder_pose_height(to_pose),
        progress
    );
    if (draw_w <= 0)
        draw_w = 1;
    if (draw_h <= 0)
        draw_h = 1;
    draw_x = interpolate_value(
        cover_pose_draw_x(from_pose, draw_w),
        cover_pose_draw_x(to_pose, draw_w),
        progress
    );
    draw_y = interpolate_value(
        cover_pose_draw_y(from_pose, draw_h),
        cover_pose_draw_y(to_pose, draw_h),
        progress
    );

    if (!slot || !slot->loaded)
    {
        const struct bitmap *art = system_art_for_id(entry->system_id);
        if (art != NULL)
        {
            int art_x = draw_x + (draw_w - art->width) / 2;
            int art_y = draw_y + (draw_h - art->height) / 2;

            rb->lcd_bmp_part(art, 0, 0, art_x, art_y,
                             art->width, art->height);
        }
        return;
    }

    focus = interpolate_value(pose_focus(from_pose), pose_focus(to_pose), progress);
    crop_w = cover_crop_width(slot, focus);
    draw_w = interpolate_value(
        cover_pose_width(from_pose, crop_w),
        cover_pose_width(to_pose, crop_w),
        progress
    );
    draw_h = interpolate_value(
        cover_pose_height(from_pose, slot),
        cover_pose_height(to_pose, slot),
        progress
    );
    if (draw_w <= 0)
        draw_w = 1;
    if (draw_h <= 0)
        draw_h = 1;
    draw_x = interpolate_value(
        cover_pose_draw_x(from_pose, draw_w),
        cover_pose_draw_x(to_pose, draw_w),
        progress
    );
    draw_y = interpolate_value(
        cover_pose_draw_y(from_pose, draw_h),
        cover_pose_draw_y(to_pose, draw_h),
        progress
    );

    draw_bitmap = prepare_cover_pose_bitmap(slot, crop_w, draw_w, draw_h);
    if (!draw_bitmap)
        return;

    rb->lcd_bmp_part(draw_bitmap, 0, 0, draw_x, draw_y,
                     draw_bitmap->width, draw_bitmap->height);

    reflection_alpha = 132 - ((abs(focus) * 72) / FLOW_FOCUS_LIMIT);
    if (reflection_alpha > 0 &&
        build_reflection_bitmap(draw_bitmap, 0, draw_bitmap->width, reflection_alpha))
    {
        rb->lcd_bmp_part(&launcher.reflection_bitmap, 0, 0,
                         draw_x,
                         draw_y + draw_bitmap->height + launcher.reflection_gap,
                         launcher.reflection_bitmap.width,
                         launcher.reflection_bitmap.height);
    }
}

static void draw_entry_details(struct game_entry *entry)
{
    char line[96];
    char title[MAX_ENTRY_TITLE];
    char meta_line[96];
    char maker_line[96];
    char badges[48];
    char status[32];
    int y;
    int text_w;
    int margin;
    y = launcher.detail_y;
    margin = 8;

    if (entry->is_system)
        rb->snprintf(line, sizeof(line), "Systems  %d / %d",
                     launcher.selected + 1, launcher.entry_count);
    else if (launcher.total_count > launcher.entry_count)
        rb->snprintf(line, sizeof(line), "%d / %d (%d total)",
                     launcher.selected + 1, launcher.entry_count, launcher.total_count);
    else
        rb->snprintf(line, sizeof(line), "%d / %d", launcher.selected + 1, launcher.entry_count);
    rb->lcd_getstringsize(line, &text_w, NULL);
    rb->lcd_putsxy(launcher.vp.width - margin - text_w, 0, line);

    badges[0] = '\0';
    if ((entry->flags & FLAG_NEEDS_SETUP) != 0)
        rb->strlcpy(badges, "SETUP", sizeof(badges));
    if (!entry->is_system && launcher.show_save_indicators && entry->has_save > 0)
        rb->strlcpy(badges, "SAVE", sizeof(badges));
    if (entry->flags & FLAG_FAVORITE)
    {
        if (badges[0] != '\0')
            rb->strlcat(badges, "  ", sizeof(badges));
        rb->strlcat(badges, "FAV", sizeof(badges));
    }
    if (badges[0] != '\0')
        rb->lcd_putsxy(margin, 0, badges);

    truncate_to_width(entry->title, title, sizeof(title), launcher.detail_w);
    rb->lcd_getstringsize(title, &text_w, NULL);
    rb->lcd_putsxy((launcher.vp.width - text_w) / 2, y, title);

    meta_line[0] = '\0';
    if (entry->year[0] != '\0' && entry->genre[0] != '\0')
        rb->snprintf(meta_line, sizeof(meta_line), "%s  %s", entry->year, entry->genre);
    else if (entry->year[0] != '\0')
        rb->strlcpy(meta_line, entry->year, sizeof(meta_line));
    else if (entry->genre[0] != '\0')
        rb->strlcpy(meta_line, entry->genre, sizeof(meta_line));

    if (entry->is_system)
    {
        truncate_to_width(entry->publisher, line, sizeof(line), launcher.detail_w);
        rb->lcd_getstringsize(line, &text_w, NULL);
        rb->lcd_putsxy((launcher.vp.width - text_w) / 2,
                       y + launcher.line_height, line);

        truncate_to_width(entry->genre, line, sizeof(line), launcher.detail_w);
        rb->lcd_getstringsize(line, &text_w, NULL);
        rb->lcd_putsxy((launcher.vp.width - text_w) / 2,
                       y + launcher.line_height * 2, line);
        return;
    }

    if (meta_line[0] != '\0')
    {
        truncate_to_width(meta_line, line, sizeof(line), launcher.detail_w);
        rb->lcd_getstringsize(line, &text_w, NULL);
        rb->lcd_putsxy((launcher.vp.width - text_w) / 2, y + launcher.line_height, line);
    }
    else if (entry->has_save > 0 || (entry->flags & FLAG_FAVORITE))
    {
        status[0] = '\0';
        if (entry->has_save > 0)
            rb->strlcpy(status, "Save available", sizeof(status));
        if (entry->flags & FLAG_FAVORITE)
        {
            if (status[0] != '\0')
                rb->strlcat(status, "  ", sizeof(status));
            rb->strlcat(status, "Favorite", sizeof(status));
        }
        rb->lcd_getstringsize(status, &text_w, NULL);
        rb->lcd_putsxy((launcher.vp.width - text_w) / 2, y + launcher.line_height, status);
    }

    maker_line[0] = '\0';
    if (entry->publisher[0] != '\0' && entry->developer[0] != '\0' &&
        rb->strcmp(entry->publisher, entry->developer))
    {
        rb->snprintf(maker_line, sizeof(maker_line), "%s / %s",
                     entry->publisher, entry->developer);
    }
    else if (entry->publisher[0] != '\0')
    {
        rb->strlcpy(maker_line, entry->publisher, sizeof(maker_line));
    }
    else if (entry->developer[0] != '\0')
    {
        rb->strlcpy(maker_line, entry->developer, sizeof(maker_line));
    }

    if (maker_line[0] != '\0')
    {
        truncate_to_width(maker_line, line, sizeof(line), launcher.detail_w);
        rb->lcd_getstringsize(line, &text_w, NULL);
        rb->lcd_putsxy((launcher.vp.width - text_w) / 2,
                       y + launcher.line_height * 2, line);
    }
}

static void draw_coverflow(void)
{
    if (launcher.selected > 1)
    {
        draw_flow_cover_pose(&launcher.entries[launcher.selected - 2],
                             peek_cover_slot(launcher.selected - 2),
                             FLOW_FAR_LEFT, FLOW_FAR_LEFT, 256);
    }

    if (launcher.selected + 2 < launcher.entry_count)
    {
        draw_flow_cover_pose(&launcher.entries[launcher.selected + 2],
                             peek_cover_slot(launcher.selected + 2),
                             FLOW_FAR_RIGHT, FLOW_FAR_RIGHT, 256);
    }

    if (launcher.selected > 0)
    {
        draw_flow_cover_pose(&launcher.entries[launcher.selected - 1],
                             peek_cover_slot(launcher.selected - 1),
                             FLOW_LEFT, FLOW_LEFT, 256);
    }

    if (launcher.selected + 1 < launcher.entry_count)
    {
        draw_flow_cover_pose(&launcher.entries[launcher.selected + 1],
                             peek_cover_slot(launcher.selected + 1),
                             FLOW_RIGHT, FLOW_RIGHT, 256);
    }

    draw_flow_cover_pose(&launcher.entries[launcher.selected],
                         peek_cover_slot(launcher.selected),
                         FLOW_CENTER, FLOW_CENTER, 256);
}

static void draw_coverflow_transition(int old_selected, int new_selected, int progress)
{
    if (new_selected > old_selected)
    {
        if (old_selected > 1)
            draw_flow_cover_pose(&launcher.entries[old_selected - 2],
                                 peek_cover_slot(old_selected - 2),
                                 FLOW_FAR_LEFT, FLOW_OFF_LEFT, progress);

        if (old_selected + 2 < launcher.entry_count)
            draw_flow_cover_pose(&launcher.entries[old_selected + 2],
                                 peek_cover_slot(old_selected + 2),
                                 FLOW_FAR_RIGHT, FLOW_RIGHT, progress);

        if (new_selected + 2 < launcher.entry_count)
            draw_flow_cover_pose(&launcher.entries[new_selected + 2],
                                 peek_cover_slot(new_selected + 2),
                                 FLOW_OFF_RIGHT, FLOW_FAR_RIGHT, progress);

        if (old_selected > 0)
            draw_flow_cover_pose(&launcher.entries[old_selected - 1],
                                 peek_cover_slot(old_selected - 1),
                                 FLOW_LEFT, FLOW_FAR_LEFT, progress);

        if (progress < 128)
        {
            draw_flow_cover_pose(&launcher.entries[new_selected],
                                 peek_cover_slot(new_selected),
                                 FLOW_RIGHT, FLOW_CENTER, progress);
            draw_flow_cover_pose(&launcher.entries[old_selected],
                                 peek_cover_slot(old_selected),
                                 FLOW_CENTER, FLOW_LEFT, progress);
        }
        else
        {
            draw_flow_cover_pose(&launcher.entries[old_selected],
                                 peek_cover_slot(old_selected),
                                 FLOW_CENTER, FLOW_LEFT, progress);
            draw_flow_cover_pose(&launcher.entries[new_selected],
                                 peek_cover_slot(new_selected),
                                 FLOW_RIGHT, FLOW_CENTER, progress);
        }
    }
    else
    {
        if (old_selected + 2 < launcher.entry_count)
            draw_flow_cover_pose(&launcher.entries[old_selected + 2],
                                 peek_cover_slot(old_selected + 2),
                                 FLOW_FAR_RIGHT, FLOW_OFF_RIGHT, progress);

        if (old_selected > 1)
            draw_flow_cover_pose(&launcher.entries[old_selected - 2],
                                 peek_cover_slot(old_selected - 2),
                                 FLOW_FAR_LEFT, FLOW_LEFT, progress);

        if (new_selected > 1)
            draw_flow_cover_pose(&launcher.entries[new_selected - 2],
                                 peek_cover_slot(new_selected - 2),
                                 FLOW_OFF_LEFT, FLOW_FAR_LEFT, progress);

        if (old_selected + 1 < launcher.entry_count)
            draw_flow_cover_pose(&launcher.entries[old_selected + 1],
                                 peek_cover_slot(old_selected + 1),
                                 FLOW_RIGHT, FLOW_FAR_RIGHT, progress);

        if (progress < 128)
        {
            draw_flow_cover_pose(&launcher.entries[new_selected],
                                 peek_cover_slot(new_selected),
                                 FLOW_LEFT, FLOW_CENTER, progress);
            draw_flow_cover_pose(&launcher.entries[old_selected],
                                 peek_cover_slot(old_selected),
                                 FLOW_CENTER, FLOW_RIGHT, progress);
        }
        else
        {
            draw_flow_cover_pose(&launcher.entries[old_selected],
                                 peek_cover_slot(old_selected),
                                 FLOW_CENTER, FLOW_RIGHT, progress);
            draw_flow_cover_pose(&launcher.entries[new_selected],
                                 peek_cover_slot(new_selected),
                                 FLOW_LEFT, FLOW_CENTER, progress);
        }
    }
}

static void draw_launcher_screen(void)
{
    struct game_entry *selected;
    struct viewport *last_vp;
    struct screen *display;

    if (launcher.entry_count <= 0)
        return;
    if (launcher.view_mode == VIEW_SYSTEMS)
    {
        draw_system_browser_screen();
        return;
    }

    selected = &launcher.entries[launcher.selected];
    display = rb->screens[SCREEN_MAIN];
    last_vp = rb->lcd_set_viewport(&launcher.vp);

    rb->lcd_set_background(launcher_bg_color());
    rb->lcd_set_foreground(launcher_fg_color());
    display->clear_viewport();
    draw_coverflow();
    draw_entry_details(selected);

    rb->lcd_set_viewport(last_vp);
    launcher_notify_overlay();
    rb->lcd_update();
}

static void draw_launcher_transition(int old_selected, int new_selected, int progress)
{
    struct game_entry *selected;
    struct viewport *last_vp;
    struct screen *display;

    if (new_selected < 0 || new_selected >= launcher.entry_count)
        return;

    selected = &launcher.entries[new_selected];
    display = rb->screens[SCREEN_MAIN];
    last_vp = rb->lcd_set_viewport(&launcher.vp);

    rb->lcd_set_background(launcher_bg_color());
    rb->lcd_set_foreground(launcher_fg_color());
    display->clear_viewport();
    draw_coverflow_transition(old_selected, new_selected, progress);
    draw_entry_details(selected);

    rb->lcd_set_viewport(last_vp);
    launcher_notify_overlay();
    rb->lcd_update();
}

static enum plugin_status draw_empty_library(void)
{
    struct viewport *last_vp;
    struct screen *display;

    display = rb->screens[SCREEN_MAIN];
    launcher_layout_init();
    last_vp = rb->lcd_set_viewport(&launcher.vp);

    rb->lcd_set_background(launcher_bg_color());
    rb->lcd_set_foreground(launcher_fg_color());
    display->clear_viewport();
    rb->lcd_putsxy(8, 8, "No game systems found");
    rb->lcd_putsxy(8, 8 + launcher.line_height, "Check " GAME_LIBRARY_SYSTEMS_PATH);
    rb->lcd_putsxy(8, 8 + launcher.line_height * 4, "Back: Exit");

    rb->lcd_set_viewport(last_vp);
    launcher_notify("No game systems found", 0);
    launcher_notify_overlay();
    rb->lcd_update();

    while (true)
    {
        int action;

        action = pluginlib_getaction(TIMEOUT_BLOCK, plugin_contexts,
                                     ARRAYLEN(plugin_contexts));
        if (launcher_notify_handle(action))
            continue;
        if (action == PLA_CANCEL || action == PLA_EXIT || action == ACTION_STD_CANCEL)
            return PLUGIN_OK;
        if (action == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
    }
}

static enum plugin_status draw_empty_system_library(struct system_entry *system)
{
    struct viewport *last_vp;
    struct screen *display;
    char line[128];

    display = rb->screens[SCREEN_MAIN];
    launcher_layout_init();
    last_vp = rb->lcd_set_viewport(&launcher.vp);

    rb->lcd_set_background(launcher_bg_color());
    rb->lcd_set_foreground(launcher_fg_color());
    display->clear_viewport();
    rb->lcd_putsxy(8, 8, system->title);
    rb->lcd_putsxy(8, 8 + launcher.line_height, "Needs setup");
    rb->strlcpy(line, system->setup_message, sizeof(line));
    truncate_to_width(line, line, sizeof(line), launcher.vp.width - 16);
    rb->lcd_putsxy(8, 8 + launcher.line_height * 3, line);
    rb->lcd_putsxy(8, 8 + launcher.line_height * 5, "Menu: Back to Systems");

    rb->lcd_set_viewport(last_vp);
    launcher_notify(system->setup_message, 0);
    launcher_notify_overlay();
    rb->lcd_update();

    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_BLOCK, plugin_contexts,
                                         ARRAYLEN(plugin_contexts));

        if (launcher_notify_handle(action))
            continue;
        if (action == PLA_CANCEL || action == PLA_EXIT ||
            action == ACTION_STD_CANCEL || action == PLA_SELECT ||
            action == ACTION_STD_OK)
            return PLUGIN_OK;
        if (action == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
    }
}

static enum plugin_status launch_selected_game(void)
{
    struct game_entry *entry;
    char launch_param[MAX_PATH];

    entry = &launcher.entries[launcher.selected];
    if (entry->is_system)
    {
        enum plugin_status status;
        int system_index = entry->system_index;

        save_launcher_state("");
        launcher.system_selected = launcher.selected;
        if (!load_system_games(system_index))
        {
            if (system_index >= 0 && system_index < launcher.system_count)
            {
                status = draw_empty_system_library(
                    &launcher.systems[system_index]);
                load_system_browser();
                return status;
            }
            return PLUGIN_OK;
        }
        return PLUGIN_OK;
    }

    save_launcher_state(entry->rom_path);
    if (entry->plugin_path[0] != '\0')
    {
        if (!rb->file_exists(entry->plugin_path))
        {
            launcher_notify("Plugin missing", 0);
            rb->splash(HZ * 2, "Plugin missing");
            return PLUGIN_OK;
        }

        if (!rb->strcmp(entry->plugin_path, ROCKBOY_PLUGIN_PATH))
        {
            rb->snprintf(launch_param, sizeof(launch_param), "@%s",
                         entry->rom_path + 1);
            return rb->plugin_open(entry->plugin_path, launch_param);
        }

        return rb->plugin_open(entry->plugin_path, entry->rom_path);
    }

    if (is_plugin_entry(entry->rom_path))
        return rb->plugin_open(entry->rom_path,
                               entry->plugin_param[0] ?
                               entry->plugin_param : NULL);

    if (is_nes_rom(entry->rom_path))
        return rb->plugin_open(INFONES_PLUGIN_PATH, entry->rom_path);

    rb->snprintf(launch_param, sizeof(launch_param), "@%s", entry->rom_path + 1);
    return rb->plugin_open(ROCKBOY_PLUGIN_PATH, launch_param);
}

static enum plugin_status launch_selected_game_setup(void)
{
    struct game_entry *entry = &launcher.entries[launcher.selected];

    if (entry->is_system)
    {
        if (entry->system_index >= 0 && entry->system_index < launcher.system_count)
            return draw_empty_system_library(&launcher.systems[entry->system_index]);
        return PLUGIN_OK;
    }

    save_launcher_state(entry->rom_path);
    if (is_doom_entry(entry->rom_path))
        return rb->plugin_open(PLUGIN_GAMES_DIR "/doom.rock", "--setup");

    return launcher_context_menu();
}

static enum plugin_status handle_select_press(void)
{
    long timeout = *rb->current_tick + HZ / 3;

    while (true)
    {
        int action = pluginlib_getaction(HZ / 20, plugin_contexts,
                                         ARRAYLEN(plugin_contexts));

        switch (action)
        {
            case ACTION_NONE:
                if (TIME_AFTER(*rb->current_tick, timeout))
                    return launch_selected_game();
                break;

            case PLA_SELECT_REPEAT:
            {
                enum plugin_status status = launch_selected_game_setup();
                if (status == PLUGIN_USB_CONNECTED)
                    return status;
                return PLUGIN_OK;
            }

            case PLA_SELECT_REL:
                return launch_selected_game();

            case SYS_USB_CONNECTED:
                return PLUGIN_USB_CONNECTED;
        }
    }
}

static void move_selection(int delta, bool animate)
{
    int old_selected;
    int next;

    if (launcher.view_mode == VIEW_SYSTEMS)
        animate = false;

    next = launcher.selected + delta;
    if (next < 0)
        next = 0;
    if (next >= launcher.entry_count)
        next = launcher.entry_count - 1;

    if (next != launcher.selected)
    {
        old_selected = launcher.selected;
        if (animate)
        {
            get_cover_slot(old_selected);
            get_cover_slot(next);
        }

        if (animate)
        {
            int frame;

            for (frame = 1; frame <= FLOW_ANIMATION_FRAMES; frame++)
            {
                int progress = (frame * 256) / FLOW_ANIMATION_FRAMES;
                draw_launcher_transition(old_selected, next, progress);
            }
        }

        launcher.selected = next;
        request_cover_cache_warm();
    }
}

static enum plugin_status launcher_context_menu(void)
{
    int selection = 0;
    int result;
    int previous_mode;
#if CONFIG_KEYPAD == IPOD_4G_PAD && defined(IPOD_VIDEO)
    bool changed = false;
#endif

    static const struct opt_items sort_modes[] = {
        { "Title (A-Z)", -1 },
        { "Favorites First", -1 },
        { "Saves First", -1 },
    };

    static const struct opt_items filter_modes[] = {
        { "All Games", -1 },
        { "Favorites Only", -1 },
        { "Saved Games", -1 },
    };

    MENUITEM_STRINGLIST(menu, "Games", NULL,
                        "Play Game",
#if CONFIG_KEYPAD == IPOD_4G_PAD && defined(IPOD_VIDEO)
                        "Rockboy Settings",
#endif
                        "Sort Games",
                        "Filter Games",
                        "Refresh Covers",
                        "Refresh Library",
                        "Exit Games");

    result = rb->do_menu(&menu, &selection, NULL, false);
    rb->button_clear_queue();
    switch (result)
    {
        case 0:
            return launch_selected_game();

#if CONFIG_KEYPAD == IPOD_4G_PAD && defined(IPOD_VIDEO)
        case 1:
        {
            enum plugin_status status = launcher_rockboy_settings_menu();
            if (status == PLUGIN_USB_CONNECTED)
                return status;
            return PLUGIN_OK;
        }

        case 2:
            previous_mode = launcher.sort_mode;
            rb->set_option("Sort Games", &launcher.sort_mode, RB_INT,
                           sort_modes, ARRAYLEN(sort_modes), NULL);
            if (previous_mode != launcher.sort_mode)
            {
                save_launcher_state(launcher.entries[launcher.selected].rom_path);
                if (!reload_game_library_with_current_modes())
                    return draw_empty_library();
            }
            return PLUGIN_OK;

        case 3:
            previous_mode = launcher.filter_mode;
            rb->set_option("Filter Games", &launcher.filter_mode, RB_INT,
                           filter_modes, ARRAYLEN(filter_modes), NULL);
            if (previous_mode != launcher.filter_mode)
            {
                changed = false;
                if (launcher.entry_count > 0)
                {
                    save_launcher_state(launcher.entries[launcher.selected].rom_path);
                    changed = true;
                }
                if (!reload_game_library_with_current_modes())
                    return draw_empty_library();
                if (!changed)
                    rb->button_clear_queue();
            }
            return PLUGIN_OK;

        case 4:
            clear_cover_cache();
            load_selected_cover_with_progress("Preparing Covers");
            return PLUGIN_OK;

        case 5:
            draw_loading_progress(0, 3, "Refreshing Library");
            if (!reload_game_library())
            {
                draw_empty_library();
                return PLUGIN_ERROR;
            }
            draw_loading_progress(2, 3, "Refreshing Library");
            load_selected_cover_with_progress("Preparing Covers");
            return PLUGIN_OK;

        case 6:
            save_launcher_state(launcher.entries[launcher.selected].rom_path);
            return PLUGIN_ERROR;
#else
        case 1:
            previous_mode = launcher.sort_mode;
            rb->set_option("Sort Games", &launcher.sort_mode, RB_INT,
                           sort_modes, ARRAYLEN(sort_modes), NULL);
            if (previous_mode != launcher.sort_mode)
            {
                save_launcher_state(launcher.entries[launcher.selected].rom_path);
                if (!reload_game_library_with_current_modes())
                    return draw_empty_library();
            }
            return PLUGIN_OK;

        case 2:
            previous_mode = launcher.filter_mode;
            rb->set_option("Filter Games", &launcher.filter_mode, RB_INT,
                           filter_modes, ARRAYLEN(filter_modes), NULL);
            if (previous_mode != launcher.filter_mode)
            {
                if (launcher.entry_count > 0)
                    save_launcher_state(launcher.entries[launcher.selected].rom_path);
                if (!reload_game_library_with_current_modes())
                    return draw_empty_library();
            }
            return PLUGIN_OK;

        case 3:
            clear_cover_cache();
            load_selected_cover_with_progress("Preparing Covers");
            return PLUGIN_OK;

        case 4:
            draw_loading_progress(0, 3, "Refreshing Library");
            if (!reload_game_library())
            {
                draw_empty_library();
                return PLUGIN_ERROR;
            }
            draw_loading_progress(2, 3, "Refreshing Library");
            load_selected_cover_with_progress("Preparing Covers");
            return PLUGIN_OK;

        case 5:
            save_launcher_state(launcher.entries[launcher.selected].rom_path);
            return PLUGIN_ERROR;
#endif

        case MENU_ATTACHED_USB:
            return PLUGIN_USB_CONNECTED;
    }

    return PLUGIN_OK;
}

static enum plugin_status launcher_run(void)
{
    while (true)
    {
        int action;

        draw_launcher_screen();
        action = pluginlib_getaction(HZ / 25, plugin_contexts,
                                     ARRAYLEN(plugin_contexts));
        if (launcher_notify_handle(action))
            continue;

        switch (action)
        {
            case ACTION_NONE:
                warm_cover_cache_step(1);
                break;

            case PLA_LEFT:
            case PLA_UP:
                move_selection(-1, true);
                break;

            case PLA_LEFT_REPEAT:
            case PLA_UP_REPEAT:
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                move_selection(-1, false);
                break;

            case PLA_RIGHT:
            case PLA_DOWN:
                move_selection(1, true);
                break;

            case PLA_RIGHT_REPEAT:
            case PLA_DOWN_REPEAT:
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                move_selection(1, false);
                break;

            case PLA_SELECT:
            {
                enum plugin_status status = handle_select_press();
                if (status != PLUGIN_OK)
                    return status;
                break;
            }

            case ACTION_STD_OK:
            {
                enum plugin_status status = launch_selected_game();
                if (status != PLUGIN_OK)
                    return status;
                break;
            }

            case PLA_CANCEL:
            case PLA_EXIT:
            case ACTION_STD_CANCEL:
                if (launcher.view_mode == VIEW_GAMES)
                {
                    save_launcher_state(launcher.entries[launcher.selected].rom_path);
                    if (!load_system_browser())
                        return PLUGIN_OK;
                    break;
                }

                save_launcher_state("");
                return PLUGIN_OK;

            case SYS_USB_CONNECTED:
                return PLUGIN_USB_CONNECTED;
        }
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;

    rb->lcd_setfont(FONT_UI);
    launcher_layout_init();
    if (!allocate_launcher_buffers())
        return PLUGIN_ERROR;

    draw_loading_progress(0, 2, "Loading Games");
    if (!load_game_library())
        return draw_empty_library();

    if (launcher.view_mode != VIEW_SYSTEMS)
        load_selected_cover_quiet();
    return launcher_run();
}
