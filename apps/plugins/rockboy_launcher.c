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
#include "rockboy/settings.h"
#include <ctype.h>

#define ROCKBOY_LAUNCHER_DIR  PLUGIN_GAMES_DATA_DIR "/rockboy_launcher"
#define ROCKBOY_INDEX_PATH    ROCKBOY_LAUNCHER_DIR "/games.tsv"
#define ROCKBOY_STATE_PATH    ROCKBOY_LAUNCHER_DIR "/state.dat"
#define ROCKBOY_PLUGIN_PATH   VIEWERS_DIR "/rockboy.rock"
#define ROCKBOY_ROM_DIR       "/gameboy"
#define ROCKBOY_LOADING_BACKGROUND_BMP ROCKBOY_LAUNCHER_DIR "/loading_bg.bmp"
#define ROCKBOY_FALLBACK_COVER_BMP PLUGIN_DEMOS_DIR "/pictureflow_emptyslide.bmp"
#define ROCKBOY_LOADING_BAR_HEIGHT 22
#define ROCKBOY_LOADING_BAR_MARGIN 10

#define MAX_ENTRY_TITLE       96
#define MAX_SAVE_BASENAME     24
#define MAX_ENTRY_YEAR        8
#define MAX_ENTRY_GENRE       32
#define MAX_ENTRY_PUBLISHER   48
#define MAX_ENTRY_DEVELOPER   48
#define MIN_ENTRY_CAPACITY    16
#define MAX_SCAN_DEPTH        6

#define SAVE_HINT_UNKNOWN     0
#define SAVE_HINT_NO          1
#define SAVE_HINT_YES         2

#define FLAG_FAVORITE         0x01

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
    char rom_path[MAX_PATH];
    char cover_path[MAX_PATH];
    char save_name[MAX_SAVE_BASENAME];
    char year[MAX_ENTRY_YEAR];
    char genre[MAX_ENTRY_GENRE];
    char publisher[MAX_ENTRY_PUBLISHER];
    char developer[MAX_ENTRY_DEVELOPER];
    unsigned char flags;
    unsigned char save_hint;
    signed char has_save;
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
    int sort_mode;
    int filter_mode;

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

static void warm_cover_cache(void);
static void request_cover_cache_warm(void);
static void warm_cover_cache_step(int budget);
static enum plugin_status launcher_context_menu(void);
static void draw_loading_splashscreen(void);
static void draw_loading_progress(int step, int count, const char *msg);
static void load_selected_cover_with_progress(const char *msg);
static bool entry_matches_filter(const struct game_entry *entry);
static void apply_launcher_filter(void);
static bool reload_game_library_with_current_modes(void);

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

static bool has_supported_rom_ext(const char *path)
{
    const char *ext = rb->strrchr(path, '.');
    if (!ext)
        return false;

    return !rb->strcasecmp(ext, ".gb") || !rb->strcasecmp(ext, ".gbc");
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

static void save_launcher_state(const char *rom_path)
{
    int fd;

    if (!ensure_launcher_dir())
        return;

    fd = rb->open(ROCKBOY_STATE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    rb->write(fd, rom_path, rb->strlen(rom_path));
    rb->close(fd);
}

static void load_launcher_state(void)
{
    int fd;
    ssize_t bytes;
    char *trimmed;

    launcher.state_rom[0] = '\0';

    fd = rb->open(ROCKBOY_STATE_PATH, O_RDONLY);
    if (fd < 0)
        return;

    bytes = rb->read(fd, launcher.state_rom, sizeof(launcher.state_rom) - 1);
    rb->close(fd);
    if (bytes <= 0)
    {
        launcher.state_rom[0] = '\0';
        return;
    }

    launcher.state_rom[bytes] = '\0';
    trimmed = trim_whitespace(launcher.state_rom);
    if (trimmed != launcher.state_rom)
        rb->memmove(launcher.state_rom, trimmed, rb->strlen(trimmed) + 1);
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

static void add_game_entry(const char *title, const char *rom_path,
                           const char *cover_path, unsigned char flags,
                           unsigned char save_hint, const char *year,
                           const char *genre, const char *publisher,
                           const char *developer)
{
    struct game_entry *entry;

    if (!rom_path || !*rom_path || launcher.entry_count >= launcher.entry_capacity)
        return;
    if (!has_supported_rom_ext(rom_path))
        return;

    entry = &launcher.entries[launcher.entry_count++];
    rb->memset(entry, 0, sizeof(*entry));

    if (title && *title)
        rb->strlcpy(entry->title, title, sizeof(entry->title));
    else
        derive_title_from_path(rom_path, entry->title, sizeof(entry->title));

    rb->strlcpy(entry->rom_path, rom_path, sizeof(entry->rom_path));
    if (cover_path && *cover_path)
        rb->strlcpy(entry->cover_path, cover_path, sizeof(entry->cover_path));
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
            add_game_entry(NULL, child, cover, 0, SAVE_HINT_UNKNOWN, "", "", "", "");
        }
    }

    rb->closedir(dir);
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
        char *cursor;
        char *next;
        char resolved_rom[MAX_PATH];
        char resolved_cover[MAX_PATH];
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
                                    *next = '\0';
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

        make_path_absolute(index_dir, rom_path, resolved_rom, sizeof(resolved_rom));
        make_path_absolute(index_dir, cover_path, resolved_cover, sizeof(resolved_cover));
        if (!rb->file_exists(resolved_rom))
            continue;
        if (resolved_cover[0] != '\0' && !rb->file_exists(resolved_cover))
            resolved_cover[0] = '\0';
        if (resolved_cover[0] == '\0')
            detect_sidecar_cover(resolved_rom, resolved_cover, sizeof(resolved_cover));

        flags = 0;
        if (parse_bool(favorite))
            flags |= FLAG_FAVORITE;

        add_game_entry(title, resolved_rom, resolved_cover, flags,
                       parse_save_hint(save_hint), year, genre, publisher, developer);
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

static fb_data fade_to_black(fb_data pixel, unsigned alpha)
{
    unsigned r;
    unsigned g;
    unsigned b;

    if (alpha >= 255)
        return pixel;

    r = (FB_UNPACK_RED(pixel) * alpha) / 255;
    g = (FB_UNPACK_GREEN(pixel) * alpha) / 255;
    b = (FB_UNPACK_BLUE(pixel) * alpha) / 255;
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
            dst[row * src_w + col] = fade_to_black(
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

    reset_entry_list();
    load_launcher_state();
    launcher.used_index = load_games_from_index();
    if (!launcher.used_index)
        scan_rom_dir(ROCKBOY_ROM_DIR, 0);

    prime_game_metadata();
    apply_launcher_filter();
    if (launcher.entry_count > 1)
    {
        rb->qsort(launcher.entries, launcher.entry_count,
                  sizeof(struct game_entry), compare_entries);
    }
    restore_selection();
    return launcher.entry_count > 0;
}

static bool reload_game_library(void)
{
    reset_entry_list();
    clear_cover_cache();
    load_launcher_state();
    launcher.used_index = load_games_from_index();
    if (!launcher.used_index)
        scan_rom_dir(ROCKBOY_ROM_DIR, 0);

    prime_game_metadata();
    apply_launcher_filter();
    if (launcher.entry_count > 1)
    {
        rb->qsort(launcher.entries, launcher.entry_count,
                  sizeof(struct game_entry), compare_entries);
    }
    restore_selection();
    warm_cover_cache();
    return launcher.entry_count > 0;
}

static bool reload_game_library_with_current_modes(void)
{
    if (!reload_game_library())
        return false;

    load_selected_cover_with_progress("Preparing Covers");
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

    margin = 8;

    rb->viewportmanager_theme_enable(SCREEN_MAIN, false, NULL);
    rb->viewport_set_fullscreen(&launcher.vp, SCREEN_MAIN);

    rb->lcd_setfont(FONT_UI);
    rb->lcd_getstringsize("Games", NULL, &font_height);
    launcher.line_height = font_height + 6;

    launcher.cover_box_w = launcher.vp.width / 2 - 20;
    if (launcher.cover_box_w > 150)
        launcher.cover_box_w = 150;
    if (launcher.cover_box_w < 110)
        launcher.cover_box_w = 110;

    launcher.cover_box_h = launcher.vp.height / 2 + 4;
    if (launcher.cover_box_h > 140)
        launcher.cover_box_h = 140;
    if (launcher.cover_box_h < 96)
        launcher.cover_box_h = 96;

    launcher.side_cover_w = launcher.cover_box_w / 2;
    if (launcher.side_cover_w < 42)
        launcher.side_cover_w = 42;

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

    launcher.cover_box_x = (launcher.vp.width - launcher.cover_box_w) / 2;
    launcher.cover_box_y = margin + launcher.line_height + 4;
    launcher.detail_y = launcher.cover_box_y + launcher.cover_box_h +
                        launcher.reflection_gap + launcher.reflection_max_h + margin - 2;
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

static bool ensure_fallback_cover(void)
{
    if (launcher.fallback_cover_loaded)
        return true;

    launcher.fallback_cover_loaded = decode_cover_bitmap(
        ROCKBOY_FALLBACK_COVER_BMP,
        &launcher.fallback_cover,
        launcher.fallback_cover_data,
        launcher.fallback_cover_bytes
    );
    return launcher.fallback_cover_loaded;
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
        else if (ensure_fallback_cover())
        {
            slot->bitmap = launcher.fallback_cover;
            slot->loaded = true;
            slot->fallback = true;
            rb->strlcpy(slot->path, ROCKBOY_FALLBACK_COVER_BMP, sizeof(slot->path));
        }
    }

    slot->last_used = ++launcher.cover_use_clock;
    return slot;
}

static void warm_cover_cache(void)
{
    int start;
    int end;
    int i;

    if (launcher.entry_count <= 0)
        return;

    start = launcher.selected - COVER_CACHE_RADIUS;
    end = launcher.selected + COVER_CACHE_RADIUS;
    if (start < 0)
        start = 0;
    if (end >= launcher.entry_count)
        end = launcher.entry_count - 1;

    for (i = start; i <= end; i++)
        get_cover_slot(i);
}

static void request_cover_cache_warm(void)
{
    launcher.cache_warm_center = launcher.selected;
    launcher.cache_warm_step = 0;
}

static void warm_cover_cache_step(int budget)
{
    if (launcher.entry_count <= 0 || budget <= 0)
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

    draw_loading_progress(0, 1, msg);
    get_cover_slot(launcher.selected);
    draw_loading_progress(1, 1, NULL);
    request_cover_cache_warm();
}

static void draw_cover_placeholder(const struct game_entry *entry,
                                   int x, int y, int w, int h)
{
    char title[MAX_ENTRY_TITLE];
    int text_w;
    int text_h;

    truncate_to_width(entry->title, title, sizeof(title), w - 20);
    rb->lcd_getstringsize(title, &text_w, &text_h);
    rb->lcd_putsxy(x + (w - text_w) / 2, y + (h - text_h) / 2, title);
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
        draw_cover_placeholder(entry,
                               draw_x, draw_y, draw_w, draw_h);
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

    if (launcher.total_count > launcher.entry_count)
        rb->snprintf(line, sizeof(line), "%d / %d (%d total)",
                     launcher.selected + 1, launcher.entry_count, launcher.total_count);
    else
        rb->snprintf(line, sizeof(line), "%d / %d", launcher.selected + 1, launcher.entry_count);
    rb->lcd_getstringsize(line, &text_w, NULL);
    rb->lcd_putsxy(launcher.vp.width - margin - text_w, 0, line);

    badges[0] = '\0';
    if (entry->has_save > 0)
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
        if (old_selected > 0)
            draw_flow_cover_pose(&launcher.entries[old_selected - 1],
                                 peek_cover_slot(old_selected - 1),
                                 FLOW_LEFT, FLOW_OFF_LEFT, progress);

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
        if (old_selected + 1 < launcher.entry_count)
            draw_flow_cover_pose(&launcher.entries[old_selected + 1],
                                 peek_cover_slot(old_selected + 1),
                                 FLOW_RIGHT, FLOW_OFF_RIGHT, progress);

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

    selected = &launcher.entries[launcher.selected];
    display = rb->screens[SCREEN_MAIN];
    last_vp = rb->lcd_set_viewport(&launcher.vp);

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    display->clear_viewport();
    draw_coverflow();
    draw_entry_details(selected);

    rb->lcd_set_viewport(last_vp);
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

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    display->clear_viewport();
    draw_coverflow_transition(old_selected, new_selected, progress);
    draw_entry_details(selected);

    rb->lcd_set_viewport(last_vp);
    rb->lcd_update();
}

static enum plugin_status draw_empty_library(void)
{
    struct viewport *last_vp;
    struct screen *display;

    display = rb->screens[SCREEN_MAIN];
    launcher_layout_init();
    last_vp = rb->lcd_set_viewport(&launcher.vp);

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    display->clear_viewport();
    rb->lcd_putsxy(8, 8, "No Game Boy ROMs found");
    rb->lcd_putsxy(8, 8 + launcher.line_height, "Place ROMs in " ROCKBOY_ROM_DIR);
    rb->lcd_putsxy(8, 8 + launcher.line_height * 2, "or add " ROCKBOY_INDEX_PATH);
    rb->lcd_putsxy(8, 8 + launcher.line_height * 4, "Back: Exit");

    rb->lcd_set_viewport(last_vp);
    rb->lcd_update();

    while (true)
    {
        int action;

        action = pluginlib_getaction(TIMEOUT_BLOCK, plugin_contexts,
                                     ARRAYLEN(plugin_contexts));
        if (action == PLA_CANCEL || action == PLA_EXIT || action == ACTION_STD_CANCEL)
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
    save_launcher_state(entry->rom_path);
    rb->snprintf(launch_param, sizeof(launch_param), "@%s", entry->rom_path + 1);
    return rb->plugin_open(ROCKBOY_PLUGIN_PATH, launch_param);
}

static enum plugin_status handle_select_press(void)
{
    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_BLOCK, plugin_contexts,
                                         ARRAYLEN(plugin_contexts));

        switch (action)
        {
            case PLA_SELECT_REPEAT:
            {
                enum plugin_status status = launcher_context_menu();
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
    bool changed;

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
            case ACTION_STD_OK:
                return handle_select_press();

            case PLA_CANCEL:
            case PLA_EXIT:
            case ACTION_STD_CANCEL:
                save_launcher_state(launcher.entries[launcher.selected].rom_path);
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

    load_selected_cover_with_progress("Preparing Covers");
    return launcher_run();
}
