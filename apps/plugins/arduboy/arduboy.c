/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Arduboy launcher/runtime shell for Rockbox.
 *
 ****************************************************************************/

#include "plugin.h"
#include "arduboy_audio.h"
#include "arduboy_avr.h"
#include "lib/pluginlib_actions.h"
#include "lib/read_image.h"
#include <ctype.h>

#define ARDUBOY_BASE_DIR      ROCKBOX_DIR "/games/arduboy"
#define ARDUBOY_ROM_DIR       ARDUBOY_BASE_DIR "/roms"
#define ARDUBOY_SAVE_DIR      ARDUBOY_BASE_DIR "/saves"
#define ARDUBOY_STATE_DIR     ARDUBOY_BASE_DIR "/states"
#define ARDUBOY_CACHE_DIR     ARDUBOY_BASE_DIR "/cache"
#define ARDUBOY_CONFIG_PATH   ARDUBOY_BASE_DIR "/config.cfg"
#define ARDUBOY_COVER_DIR     ROCKBOX_DIR "/games/library/covers/arduboy"

#define ARDUBOY_SCREEN_W      128
#define ARDUBOY_SCREEN_H      64
#define ARDUBOY_DISPLAY_W     LCD_WIDTH
#define ARDUBOY_DISPLAY_H     ((LCD_WIDTH * ARDUBOY_SCREEN_H) / ARDUBOY_SCREEN_W)
#define ARDUBOY_FLASH_SIZE    32768
#define ARDUBOY_MAX_ROMS      128
#define ARDUBOY_TARGET_FPS    63

enum arduboy_button_bits {
    ARDUBOY_UP    = 0x01,
    ARDUBOY_DOWN  = 0x02,
    ARDUBOY_LEFT  = 0x04,
    ARDUBOY_RIGHT = 0x08,
    ARDUBOY_A     = 0x10,
    ARDUBOY_B     = 0x20,
};

struct arduboy_rom_entry {
    char title[96];
    char path[MAX_PATH];
};

struct arduboy_rom_list {
    struct arduboy_rom_entry entries[ARDUBOY_MAX_ROMS];
    int count;
};

struct arduboy_runtime {
    unsigned char flash[ARDUBOY_FLASH_SIZE];
    unsigned char framebuffer[ARDUBOY_SCREEN_W * ARDUBOY_SCREEN_H / 8];
    struct arduboy_avr avr;
    fb_data scaled_frame[ARDUBOY_DISPLAY_W * ARDUBOY_DISPLAY_H];
    uint8_t scale_x[ARDUBOY_DISPLAY_W];
    uint8_t scale_page[ARDUBOY_DISPLAY_H];
    uint8_t scale_mask[ARDUBOY_DISPLAY_H];
    char rom_path[MAX_PATH];
    char status[96];
    unsigned int checksum;
    int frame;
    long blank_start_tick;
    long last_haptic_tick;
    bool haptics;
    bool show_fps;
    bool sound;
    bool ever_displayed_pixels;
    bool scale_ready;
    bool screen_cleared;
};

static struct arduboy_runtime arduboy;
static struct arduboy_rom_list arduboy_roms;

static void mkdir_if_needed(const char *path)
{
    if (!rb->dir_exists(path))
        rb->mkdir(path);
}

static bool ensure_dirs(void)
{
    mkdir_if_needed(ROCKBOX_DIR "/games");
    mkdir_if_needed(ARDUBOY_BASE_DIR);
    mkdir_if_needed(ARDUBOY_ROM_DIR);
    mkdir_if_needed(ARDUBOY_SAVE_DIR);
    mkdir_if_needed(ARDUBOY_STATE_DIR);
    mkdir_if_needed(ARDUBOY_CACHE_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games/library");
    mkdir_if_needed(ROCKBOX_DIR "/games/library/covers");
    mkdir_if_needed(ARDUBOY_COVER_DIR);
    return rb->dir_exists(ARDUBOY_ROM_DIR);
}

static bool has_rom_ext(const char *path)
{
    const char *ext = rb->strrchr(path, '.');

    return ext && (!rb->strcasecmp(ext, ".hex") ||
                   !rb->strcasecmp(ext, ".arduboy") ||
                   !rb->strcasecmp(ext, ".bin"));
}

static void title_from_path(const char *path, char *title, size_t title_size)
{
    const char *name = rb->strrchr(path, '/');
    char *ext;
    size_t i;

    rb->strlcpy(title, name ? name + 1 : path, title_size);
    ext = rb->strrchr(title, '.');
    if (ext)
        *ext = '\0';

    for (i = 0; title[i] != '\0'; i++)
    {
        if (title[i] == '_' || title[i] == '-')
            title[i] = ' ';
    }
}

static bool parse_bool(const char *value)
{
    return !rb->strcasecmp(value, "1") ||
           !rb->strcasecmp(value, "true") ||
           !rb->strcasecmp(value, "yes") ||
           !rb->strcasecmp(value, "on");
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

static void load_config(void)
{
    int fd;
    char line[96];

    arduboy.haptics = false;
    arduboy.sound = true;
    arduboy.show_fps = false;

    fd = rb->open(ARDUBOY_CONFIG_PATH, O_RDONLY);
    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *eq = rb->strchr(line, '=');
        char *key;
        char *value;

        if (!eq)
            continue;

        *eq++ = '\0';
        key = trim_whitespace(line);
        value = trim_whitespace(eq);
        if (!rb->strcmp(key, "haptics"))
            arduboy.haptics = parse_bool(value);
        else if (!rb->strcmp(key, "sound"))
            arduboy.sound = parse_bool(value);
        else if (!rb->strcmp(key, "show_fps"))
            arduboy.show_fps = parse_bool(value);
    }

    rb->close(fd);
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool parse_hex_byte(const char *text, unsigned char *value)
{
    int hi = hex_value((unsigned char)text[0]);
    int lo = hex_value((unsigned char)text[1]);

    if (hi < 0 || lo < 0)
        return false;

    *value = (unsigned char)((hi << 4) | lo);
    return true;
}

static bool load_hex_file(const char *path)
{
    int fd;
    char line[256];
    unsigned int base = 0;
    bool saw_data = false;

    rb->memset(arduboy.flash, 0xff, sizeof(arduboy.flash));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        unsigned char len;
        unsigned char addr_hi;
        unsigned char addr_lo;
        unsigned char type;
        unsigned int addr;
        int i;

        if (line[0] != ':')
            continue;
        if (!parse_hex_byte(line + 1, &len) ||
            !parse_hex_byte(line + 3, &addr_hi) ||
            !parse_hex_byte(line + 5, &addr_lo) ||
            !parse_hex_byte(line + 7, &type))
        {
            rb->close(fd);
            return false;
        }

        addr = base + (((unsigned int)addr_hi << 8) | addr_lo);
        if (type == 0x00)
        {
            for (i = 0; i < len; i++)
            {
                unsigned char byte;

                if (!parse_hex_byte(line + 9 + i * 2, &byte))
                {
                    rb->close(fd);
                    return false;
                }
                if (addr + (unsigned int)i < sizeof(arduboy.flash))
                {
                    arduboy.flash[addr + (unsigned int)i] = byte;
                    saw_data = true;
                }
            }
        }
        else if (type == 0x01)
            break;
        else if (type == 0x04)
        {
            unsigned char msb;
            unsigned char lsb;

            if (!parse_hex_byte(line + 9, &msb) ||
                !parse_hex_byte(line + 11, &lsb))
            {
                rb->close(fd);
                return false;
            }
            base = (((unsigned int)msb << 8) | lsb) << 16;
        }
    }

    rb->close(fd);
    return saw_data;
}

static bool load_binary_file(const char *path)
{
    int fd;
    ssize_t bytes;

    rb->memset(arduboy.flash, 0xff, sizeof(arduboy.flash));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    bytes = rb->read(fd, arduboy.flash, sizeof(arduboy.flash));
    rb->close(fd);
    return bytes > 0;
}

static bool flash_cache_path(const char *path, char *cache_path,
                             size_t cache_path_size)
{
    const char *base = rb->strrchr(path, '/');
    const char *dot;
    char name[64];
    int fd;
    long size;
    size_t i = 0;

    base = base ? base + 1 : path;
    dot = rb->strrchr(base, '.');
    if (!dot)
        dot = base + rb->strlen(base);

    while (base < dot && i + 1 < sizeof(name))
    {
        unsigned char c = (unsigned char)*base++;

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'))
            name[i++] = (char)c;
        else
            name[i++] = '_';
    }
    name[i] = '\0';
    if (name[0] == '\0')
        rb->strlcpy(name, "game", sizeof(name));

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    size = rb->filesize(fd);
    rb->close(fd);
    if (size <= 0)
        return false;

    rb->snprintf(cache_path, cache_path_size, "%s/%s.%ld.bin",
                 ARDUBOY_CACHE_DIR, name, size);
    return true;
}

static bool load_flash_cache(const char *cache_path)
{
    int fd;
    ssize_t bytes;

    fd = rb->open(cache_path, O_RDONLY);
    if (fd < 0)
        return false;
    if (rb->filesize(fd) != (long)sizeof(arduboy.flash))
    {
        rb->close(fd);
        return false;
    }

    bytes = rb->read(fd, arduboy.flash, sizeof(arduboy.flash));
    rb->close(fd);
    return bytes == (ssize_t)sizeof(arduboy.flash);
}

static void save_flash_cache(const char *cache_path)
{
    int fd = rb->open(cache_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        return;
    rb->write(fd, arduboy.flash, sizeof(arduboy.flash));
    rb->close(fd);
}

static unsigned int checksum_flash(void)
{
    unsigned int checksum = 0;
    size_t i;

    for (i = 0; i < sizeof(arduboy.flash); i++)
        checksum = (checksum << 5) - checksum + arduboy.flash[i];

    return checksum;
}

static void haptic_tick(int duration, int strength)
{
    long now = *rb->current_tick;

    if (!arduboy.haptics)
        return;
    if (TIME_BEFORE(now, arduboy.last_haptic_tick + HZ / 12))
        return;
    if (rb->haptic_feedback_enabled == NULL || rb->haptic_feedback == NULL)
        return;
    if (!rb->haptic_feedback_enabled())
        return;
    rb->haptic_feedback(duration, strength);
    arduboy.last_haptic_tick = now;
}

static bool load_rom(const char *path)
{
    const char *ext = rb->strrchr(path, '.');
    bool ok;

    if (!ext)
        return false;

    rb->strlcpy(arduboy.rom_path, path, sizeof(arduboy.rom_path));
    if (!rb->strcasecmp(ext, ".hex"))
    {
        char cache_path[MAX_PATH];

        cache_path[0] = '\0';
        ok = flash_cache_path(path, cache_path, sizeof(cache_path)) &&
             load_flash_cache(cache_path);
        if (!ok)
        {
            ok = load_hex_file(path);
            if (ok && cache_path[0] != '\0')
                save_flash_cache(cache_path);
        }
    }
    else
        ok = load_binary_file(path);

    if (!ok)
        return false;

    arduboy.checksum = checksum_flash();
    arduboy_avr_reset(&arduboy.avr, arduboy.flash, sizeof(arduboy.flash));
    title_from_path(path, arduboy.status, sizeof(arduboy.status));
    return true;
}

static void scan_roms(struct arduboy_rom_list *list)
{
    DIR *dir;
    struct dirent *entry;

    list->count = 0;
    dir = rb->opendir(ARDUBOY_ROM_DIR);
    if (!dir)
        return;

    while ((entry = rb->readdir(dir)) != NULL && list->count < ARDUBOY_MAX_ROMS)
    {
        struct arduboy_rom_entry *rom;

        if (!has_rom_ext(entry->d_name))
            continue;

        rom = &list->entries[list->count++];
        rb->snprintf(rom->path, sizeof(rom->path), "%s/%s",
                     ARDUBOY_ROM_DIR, entry->d_name);
        title_from_path(entry->d_name, rom->title, sizeof(rom->title));
    }

    rb->closedir(dir);
}

static int choose_rom(struct arduboy_rom_list *list)
{
    int selected = 0;

    if (list->count <= 0)
        return -1;

    while (true)
    {
        int i;
        int line_h;
        int visible;
        int first;
        int button;

        rb->lcd_set_background(LCD_RGBPACK(234, 235, 237));
        rb->lcd_set_foreground(LCD_RGBPACK(35, 35, 37));
        rb->lcd_clear_display();
        rb->lcd_getstringsize("Arduboy", NULL, &line_h);
        line_h += 4;
        visible = (LCD_HEIGHT - line_h * 2) / line_h;
        if (visible < 1)
            visible = 1;
        first = selected - visible / 2;
        if (first < 0)
            first = 0;
        if (first + visible > list->count)
            first = list->count - visible;
        if (first < 0)
            first = 0;

        rb->lcd_putsxy(8, 6, "Arduboy");
        for (i = 0; i < visible && first + i < list->count; i++)
        {
            int y = line_h + 8 + i * line_h;

            if (first + i == selected)
            {
                rb->lcd_set_foreground(LCD_RGBPACK(92, 86, 110));
                rb->lcd_fillrect(4, y - 2, LCD_WIDTH - 8, line_h);
                rb->lcd_set_foreground(LCD_WHITE);
            }
            else
                rb->lcd_set_foreground(LCD_RGBPACK(35, 35, 37));

            rb->lcd_putsxy(10, y, list->entries[first + i].title);
        }
        rb->lcd_set_foreground(LCD_RGBPACK(35, 35, 37));
        rb->lcd_update();

        button = rb->button_get(true);
        switch (button)
        {
            case BUTTON_LEFT:
            case BUTTON_MENU:
#ifdef BUTTON_SCROLL_BACK
            case BUTTON_SCROLL_BACK:
            case BUTTON_SCROLL_BACK | BUTTON_REPEAT:
#endif
                if (selected > 0)
                {
                    selected--;
                    haptic_tick(7, 22);
                }
                break;
            case BUTTON_RIGHT:
            case BUTTON_PLAY:
#ifdef BUTTON_SCROLL_FWD
            case BUTTON_SCROLL_FWD:
            case BUTTON_SCROLL_FWD | BUTTON_REPEAT:
#endif
                if (selected + 1 < list->count)
                {
                    selected++;
                    haptic_tick(7, 22);
                }
                break;
            case BUTTON_SELECT:
                haptic_tick(18, 45);
                return selected;
            case BUTTON_MENU | BUTTON_REPEAT:
                return -1;
            case SYS_USB_CONNECTED:
                return -1;
            default:
                break;
        }
    }
}

static bool framebuffer_has_pixels(void);

static void prepare_scale_tables(void)
{
    int x;
    int y;

    if (arduboy.scale_ready)
        return;

    for (x = 0; x < ARDUBOY_DISPLAY_W; x++)
        arduboy.scale_x[x] = (uint8_t)((x * ARDUBOY_SCREEN_W) /
                                       ARDUBOY_DISPLAY_W);

    for (y = 0; y < ARDUBOY_DISPLAY_H; y++)
    {
        int src_y = (y * ARDUBOY_SCREEN_H) / ARDUBOY_DISPLAY_H;

        arduboy.scale_page[y] = (uint8_t)(src_y >> 3);
        arduboy.scale_mask[y] = (uint8_t)(1 << (src_y & 7));
    }

    arduboy.scale_ready = true;
}

static void draw_frame(unsigned int buttons)
{
    int screen_w = ARDUBOY_DISPLAY_W;
    int screen_h = ARDUBOY_DISPLAY_H;
    int x0 = (LCD_WIDTH - screen_w) / 2;
    int y0 = (LCD_HEIGHT - screen_h) / 2;
    int x;
    int y;
    char line[64];
    fb_data black = FB_RGBPACK(0, 0, 0);
    fb_data white = FB_RGBPACK(255, 255, 255);
    fb_data *dst = arduboy.scaled_frame;

    prepare_scale_tables();
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);

    if (!arduboy.ever_displayed_pixels && !framebuffer_has_pixels())
    {
        rb->lcd_clear_display();
        rb->lcd_putsxy((LCD_WIDTH - 13 * 8) / 2, LCD_HEIGHT / 2 - 8,
                       "Starting game");
        rb->lcd_update();
        return;
    }

    for (y = 0; y < screen_h; y++)
    {
        const uint8_t *src = arduboy.framebuffer +
                             arduboy.scale_page[y] * ARDUBOY_SCREEN_W;
        uint8_t mask = arduboy.scale_mask[y];
        fb_data *row = dst + y * screen_w;

        if (y > 0 &&
            arduboy.scale_page[y] == arduboy.scale_page[y - 1] &&
            arduboy.scale_mask[y] == arduboy.scale_mask[y - 1])
        {
            rb->memcpy(row, row - screen_w, screen_w * sizeof(*row));
            continue;
        }

        for (x = 0; x < screen_w; x++)
        {
            row[x] = (src[arduboy.scale_x[x]] & mask) ? white : black;
        }
    }
    if (!arduboy.screen_cleared)
    {
        rb->lcd_clear_display();
        arduboy.screen_cleared = true;
    }

    rb->lcd_bitmap(arduboy.scaled_frame, x0, y0, screen_w, screen_h);

    if (arduboy.show_fps)
    {
        rb->snprintf(line, sizeof(line), "frame %d", arduboy.frame);
        rb->lcd_putsxy(4, LCD_HEIGHT - 18, line);
    }
    if (arduboy.show_fps)
        rb->lcd_update();
    else
        rb->lcd_update_rect(x0, y0, screen_w, screen_h);
}

static bool framebuffer_has_pixels(void)
{
    size_t i;

    for (i = 0; i < sizeof(arduboy.framebuffer); i++)
    {
        if (arduboy.framebuffer[i] != 0)
            return true;
    }

    return false;
}

static bool wait_for_input_release(void)
{
    long timeout = *rb->current_tick + HZ * 3;

    rb->button_clear_queue();
    while (TIME_BEFORE(*rb->current_tick, timeout))
    {
        int held = rb->button_status();

        if ((held & (BUTTON_MENU | BUTTON_PLAY | BUTTON_LEFT |
                     BUTTON_RIGHT | BUTTON_SELECT)) == 0)
        {
            rb->button_clear_queue();
            return true;
        }

        if (rb->button_get_w_tmo(HZ / 20) == SYS_USB_CONNECTED)
            return false;
        rb->yield();
    }

    rb->button_clear_queue();
    return true;
}

static enum plugin_status run_loop(void)
{
    unsigned int buttons = 0;
    unsigned int sticky_buttons = 0;
    long next_frame = *rb->current_tick;
    int frame_tick_accum = 0;
    int frame_tick_step;

    rb->memcpy(arduboy.framebuffer, arduboy.avr.display,
               sizeof(arduboy.framebuffer));
    draw_frame(buttons);

    while (true)
    {
        long now = *rb->current_tick;
        int wait_ticks = 0;
        int button;
        int held = rb->button_status();

        if (TIME_BEFORE(now, next_frame))
        {
            long remaining = next_frame - now;

            int max_wait = HZ / ARDUBOY_TARGET_FPS;

            if (max_wait < 1)
                max_wait = 1;
            if (remaining > max_wait)
                remaining = max_wait;
            wait_ticks = (int)remaining;
        }

        button = rb->button_get_w_tmo(wait_ticks);
        held = rb->button_status();

        switch (button)
        {
            case BUTTON_NONE:
                break;
            case BUTTON_SELECT | BUTTON_PLAY:
                sticky_buttons |= ARDUBOY_B;
                haptic_tick(18, 45);
                break;
            case BUTTON_SELECT:
            case BUTTON_SELECT | BUTTON_REPEAT:
                sticky_buttons |= ARDUBOY_A;
                haptic_tick(10, 30);
                break;
            case BUTTON_MENU | BUTTON_REPEAT:
                return PLUGIN_OK;
            case SYS_USB_CONNECTED:
                return PLUGIN_USB_CONNECTED;
            default:
                break;
        }

        if (!TIME_BEFORE(*rb->current_tick, next_frame))
        {
            buttons = sticky_buttons;
            if (held & BUTTON_MENU)
                buttons |= ARDUBOY_UP;
            if (held & BUTTON_PLAY)
                buttons |= ARDUBOY_DOWN;
            if (held & BUTTON_LEFT)
                buttons |= ARDUBOY_LEFT;
            if (held & BUTTON_RIGHT)
                buttons |= ARDUBOY_RIGHT;
            if ((held & (BUTTON_SELECT | BUTTON_PLAY)) ==
                (BUTTON_SELECT | BUTTON_PLAY))
                buttons |= ARDUBOY_B;
            else if (held & BUTTON_SELECT)
                buttons |= ARDUBOY_A;

            bool complete_display_frame;

            arduboy_avr_set_buttons(&arduboy.avr, buttons);
            complete_display_frame =
                arduboy_avr_run_display_frame(&arduboy.avr);
            if (arduboy.avr.quit_requested)
                return PLUGIN_OK;
            arduboy_audio_update(arduboy.avr.speaker_level,
                                 arduboy.avr.audio_frequency);

            if (!arduboy.ever_displayed_pixels || arduboy.show_fps ||
                complete_display_frame)
            {
                rb->memcpy(arduboy.framebuffer, arduboy.avr.display,
                           sizeof(arduboy.framebuffer));
                if (framebuffer_has_pixels())
                {
                    arduboy.ever_displayed_pixels = true;
                    arduboy.blank_start_tick = 0;
                }
                draw_frame(buttons);
            }

            if (!arduboy.ever_displayed_pixels &&
                arduboy.blank_start_tick == 0)
                arduboy.blank_start_tick = *rb->current_tick;
            if (!arduboy.ever_displayed_pixels &&
                arduboy.blank_start_tick != 0 &&
                TIME_AFTER(*rb->current_tick, arduboy.blank_start_tick + HZ * 15))
            {
                rb->splash(HZ * 2, "Game did not start video");
                return PLUGIN_ERROR;
            }
            sticky_buttons = 0;
            arduboy.frame++;
            frame_tick_accum += HZ;
            frame_tick_step = frame_tick_accum / ARDUBOY_TARGET_FPS;
            if (frame_tick_step < 1)
                frame_tick_step = 1;
            next_frame += frame_tick_step;
            frame_tick_accum %= ARDUBOY_TARGET_FPS;
            if (TIME_AFTER(*rb->current_tick, next_frame + HZ))
                next_frame = *rb->current_tick;

            if (arduboy.avr.halted)
            {
                rb->splash(HZ * 2, arduboy_avr_error());
                return PLUGIN_ERROR;
            }
        }
        rb->yield();
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *path = parameter;
    int selected;

    rb->lcd_setfont(FONT_UI);
    rb->memset(&arduboy, 0, sizeof(arduboy));

    if (!ensure_dirs())
    {
        rb->splash(HZ * 2, "Could not create .rockbox/games/arduboy");
        return PLUGIN_ERROR;
    }
    load_config();
    arduboy.haptics = false;

    if (path == NULL || path[0] == '\0')
    {
        scan_roms(&arduboy_roms);
        selected = choose_rom(&arduboy_roms);
        if (selected < 0 || selected >= arduboy_roms.count)
        {
            rb->splash(HZ * 4,
                       "No Arduboy games found. Put .hex files in .rockbox/games/arduboy/roms/");
            return PLUGIN_OK;
        }
        path = arduboy_roms.entries[selected].path;
    }

    if (!load_rom(path))
    {
        haptic_tick(70, 80);
        rb->splash(HZ * 2, "Could not load Arduboy game");
        return PLUGIN_ERROR;
    }

    if (!wait_for_input_release())
        return PLUGIN_USB_CONNECTED;

    {
        enum plugin_status status;

#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
        rb->cpu_boost(true);
#endif
        arduboy_audio_init(arduboy.sound);
        status = run_loop();
        arduboy_audio_shutdown();
#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
        rb->cpu_boost(false);
#endif
        return status;
    }
}
