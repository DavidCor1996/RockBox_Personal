#include "plugin.h"
#include "gwatch_platform.h"
#include "libretro.h"
#include "lib/helper.h"

#undef time
#undef localtime
#undef vsnprintf

#define GWATCH_FRAME_TICKS (HZ / 60)
#define GWATCH_BASE_DIR ROCKBOX_DIR "/games/gwatch"
#define GWATCH_SAVE_DIR GWATCH_BASE_DIR "/saves"

static uint16_t *rom_data;
static size_t rom_size;
static struct retro_game_info_ext game_info_ext;
static struct retro_system_av_info av_info;
static unsigned input_mask;
static bool quit_requested;
static bool geometry_dirty;
static bool core_error_logged;
static unsigned video_frames;
static char core_error[160];
static int source_width = 128;
static int source_height = 128;
static fb_data lcd_buf[LCD_WIDTH * LCD_HEIGHT];
static uint16_t xmap[LCD_WIDTH];
static uint16_t ymap[LCD_HEIGHT];
static int cached_source_width = -1;
static int cached_source_height = -1;
static int cached_dest_width = -1;
static int cached_dest_height = -1;

static void mkdir_if_needed(const char *path)
{
    if (!rb->dir_exists(path))
        rb->mkdir(path);
}

static void ensure_dirs(void)
{
    mkdir_if_needed(ROCKBOX_DIR "/games");
    mkdir_if_needed(GWATCH_BASE_DIR);
    mkdir_if_needed(GWATCH_SAVE_DIR);
}

static void debug_open(void)
{
    int fd;

    fd = rb->open(GWATCH_BASE_DIR "/debug.log",
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
        rb->close(fd);
}

static void debug_log(const char *fmt, ...)
{
    va_list ap;
    char line[192];
    int fd;

    fd = rb->open(GWATCH_BASE_DIR "/debug.log",
                  O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    va_start(ap, fmt);
    rb->vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = '\0';
    rb->fdprintf(fd, "%ld %s\n", *rb->current_tick, line);
    rb->close(fd);
}

static void debug_close(void)
{
}

static void show_loading_status(const char *line)
{
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_putsxy(8, 8, "Loading Game & Watch");
    rb->lcd_putsxy(8, 8 + SYSFONT_HEIGHT + 4, line);
    rb->lcd_update();
}

static fb_data rgb565_to_fb(uint16_t rgb565)
{
#if LCD_DEPTH == 16 && LCD_PIXELFORMAT == RGB565
    return (fb_data)rgb565;
#else
    int r = (rgb565 >> 11) & 0x1f;
    int g = (rgb565 >> 5) & 0x3f;
    int b = rgb565 & 0x1f;

    return LCD_RGBPACK((r << 3) | (r >> 2),
                       (g << 2) | (g >> 4),
                       (b << 3) | (b >> 2));
#endif
}

static bool read_package(const char *path)
{
    int fd;
    off_t size;
    ssize_t got;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        debug_log("open failed: %s", path);
        return false;
    }

    size = rb->filesize(fd);
    if (size <= 0)
    {
        debug_log("bad package size: %ld", (long)size);
        rb->close(fd);
        return false;
    }

    rom_data = gwatch_malloc((size_t)size);
    if (rom_data == NULL)
    {
        debug_log("package alloc failed: %ld", (long)size);
        rb->close(fd);
        return false;
    }

    debug_log("read package bytes=%ld", (long)size);
    got = rb->read(fd, rom_data, (size_t)size);
    rb->close(fd);
    if (got != size)
    {
        debug_log("short read got=%ld want=%ld", (long)got, (long)size);
        gwatch_free(rom_data);
        rom_data = NULL;
        return false;
    }

    rom_size = (size_t)size;
    return true;
}

static void free_package_buffer(void)
{
    if (rom_data != NULL)
    {
        gwatch_free(rom_data);
        rom_data = NULL;
        rom_size = 0;
    }
}

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
    va_list ap;

    if (level < RETRO_LOG_WARN || fmt == NULL)
        return;

    va_start(ap, fmt);
    rb->vsnprintf(core_error, sizeof(core_error), fmt, ap);
    va_end(ap);
    core_error[sizeof(core_error) - 1] = '\0';
    core_error_logged = true;
    debug_log("core: %s", core_error);
}

static bool environment_cb(unsigned cmd, void *data)
{
    switch (cmd)
    {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return *((const enum retro_pixel_format *)data) ==
                   RETRO_PIXEL_FORMAT_RGB565;

        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
            game_info_ext.full_path = "";
            game_info_ext.archive_path = "";
            game_info_ext.archive_file = "";
            game_info_ext.dir = "";
            game_info_ext.name = "";
            game_info_ext.ext = "mgw";
            game_info_ext.data = rom_data;
            game_info_ext.size = rom_size;
            game_info_ext.file_in_archive = false;
            game_info_ext.persistent_data = true;
            *((const struct retro_game_info_ext **)data) = &game_info_ext;
            return true;

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
            if (data != NULL)
                av_info = *((const struct retro_system_av_info *)data);
            return true;

        case RETRO_ENVIRONMENT_SET_GEOMETRY:
            if (data != NULL)
            {
                const struct retro_game_geometry *geometry = data;
                source_width = geometry->base_width;
                source_height = geometry->base_height;
                geometry_dirty = true;
            }
            return true;

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return false;

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        {
            struct retro_log_callback *log = data;
            if (log == NULL)
                return false;
            log->log = log_cb;
            return true;
        }

        default:
            return false;
    }
}

static void audio_sample_cb(int16_t left, int16_t right)
{
    (void)left;
    (void)right;
}

static size_t audio_batch_cb(const int16_t *data, size_t frames)
{
    (void)data;
    return frames;
}

static void poll_input_cb(void)
{
    int button;
    long clean;

    input_mask = 0;

    while ((button = rb->button_get(false)) != BUTTON_NONE)
    {
        if (button == SYS_USB_CONNECTED)
        {
            quit_requested = true;
            return;
        }

        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (button & BUTTON_REL)
            continue;

#ifdef BUTTON_MENU
        if (clean & BUTTON_MENU)
            quit_requested = true;
#endif
#ifdef BUTTON_LEFT
        if (clean & BUTTON_LEFT)
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
#endif
#ifdef BUTTON_RIGHT
        if (clean & BUTTON_RIGHT)
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
#endif
#ifdef BUTTON_UP
        if (clean & BUTTON_UP)
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
#endif
#ifdef BUTTON_DOWN
        if (clean & BUTTON_DOWN)
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
#endif
#ifdef BUTTON_SCROLL_BACK
        if (clean & BUTTON_SCROLL_BACK)
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
#endif
#ifdef BUTTON_SCROLL_FWD
        if (clean & BUTTON_SCROLL_FWD)
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
#endif
#ifdef BUTTON_SELECT
        if (clean & BUTTON_SELECT)
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
#endif
#ifdef BUTTON_PLAY
        if (clean & BUTTON_PLAY)
        {
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
            input_mask |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
        }
#endif
    }
}

static int16_t input_state_cb(unsigned port, unsigned device,
                              unsigned index, unsigned id)
{
    (void)index;

    if (port != 0 || device != RETRO_DEVICE_JOYPAD)
        return 0;

    if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
        return (int16_t)input_mask;

    if (id < 32 && (input_mask & (1u << id)))
        return 1;

    return 0;
}

static void rebuild_maps(int src_w, int src_h, int *dst_x, int *dst_y,
                         int *dst_w, int *dst_h)
{
    int x;
    int y;
    int width_by_height;
    int height_by_width;

    if (src_w <= 0 || src_h <= 0)
    {
        src_w = 128;
        src_h = 128;
    }

    width_by_height = LCD_HEIGHT * src_w / src_h;
    if (width_by_height <= LCD_WIDTH)
    {
        *dst_w = width_by_height;
        *dst_h = LCD_HEIGHT;
    }
    else
    {
        height_by_width = LCD_WIDTH * src_h / src_w;
        *dst_w = LCD_WIDTH;
        *dst_h = height_by_width;
    }

    *dst_x = (LCD_WIDTH - *dst_w) / 2;
    *dst_y = (LCD_HEIGHT - *dst_h) / 2;

    if (src_w == cached_source_width && src_h == cached_source_height &&
        *dst_w == cached_dest_width && *dst_h == cached_dest_height)
        return;

    for (x = 0; x < *dst_w; x++)
        xmap[x] = (uint16_t)((long)x * src_w / *dst_w);
    for (y = 0; y < *dst_h; y++)
        ymap[y] = (uint16_t)((long)y * src_h / *dst_h);

    cached_source_width = src_w;
    cached_source_height = src_h;
    cached_dest_width = *dst_w;
    cached_dest_height = *dst_h;
}

static void video_cb(const void *data, unsigned width, unsigned height,
                     size_t pitch)
{
    const uint8_t *src_bytes = data;
    int dst_x;
    int dst_y;
    int dst_w;
    int dst_h;
    int x;
    int y;

    if (data == NULL || width == 0 || height == 0)
        return;

    video_frames++;
    if (video_frames == 1)
        debug_log("first frame %ux%u pitch=%lu", width, height,
                  (unsigned long)pitch);
    rebuild_maps((int)width, (int)height, &dst_x, &dst_y, &dst_w, &dst_h);
    for (y = 0; y < LCD_HEIGHT; y++)
    {
        fb_data *dst = lcd_buf + y * LCD_WIDTH;
        if (y < dst_y || y >= dst_y + dst_h)
        {
            for (x = 0; x < LCD_WIDTH; x++)
                dst[x] = LCD_BLACK;
            continue;
        }

        for (x = 0; x < dst_x; x++)
            dst[x] = LCD_BLACK;

        for (x = 0; x < dst_w; x++)
        {
            const uint16_t *src =
                (const uint16_t *)(src_bytes + (size_t)ymap[y - dst_y] * pitch);
            dst[dst_x + x] = rgb565_to_fb(src[xmap[x]]);
        }

        for (x = dst_x + dst_w; x < LCD_WIDTH; x++)
            dst[x] = LCD_BLACK;
    }

    rb->lcd_set_viewport(NULL);
    rb->lcd_bitmap(lcd_buf, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_update();
    geometry_dirty = false;
}

static enum plugin_status run_core(const char *path)
{
    struct retro_game_info info;
    long next_frame;
    long first_frame_deadline;

    show_loading_status("Reading package");
    debug_log("read begin");
    if (!read_package(path))
    {
        rb->splash(HZ * 2, "Game & Watch load failed");
        return PLUGIN_ERROR;
    }
    debug_log("read ok");

    rb->memset(&info, 0, sizeof(info));
    info.path = path;
    info.data = rom_data;
    info.size = rom_size;

    retro_set_environment(environment_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_sample_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(poll_input_cb);
    retro_set_input_state(input_state_cb);
    show_loading_status("Core init");
    debug_log("retro_init");
    retro_init();

    show_loading_status("Loading package");
    debug_log("retro_load_game begin");
    if (!retro_load_game(&info))
    {
        rb->splash(HZ * 2, "Game & Watch init failed");
        retro_deinit();
        free_package_buffer();
        return PLUGIN_ERROR;
    }

    debug_log("retro_load_game ok");
    show_loading_status("First frame");
    retro_get_system_av_info(&av_info);
    next_frame = *rb->current_tick;
    first_frame_deadline = next_frame + HZ * 5;
    video_frames = 0;
    while (!quit_requested)
    {
        long now = *rb->current_tick;
        int wait_ticks;

        retro_run();
        if (video_frames == 0 &&
            TIME_AFTER(*rb->current_tick, first_frame_deadline))
        {
            debug_log("no frames after 5s");
            rb->splash(HZ * 2, "Game & Watch no video");
            break;
        }
        if (quit_requested)
            break;
        if (core_error_logged && video_frames == 0)
        {
            rb->splashf(HZ * 3, "G&W error: %s", core_error);
            break;
        }

        next_frame += GWATCH_FRAME_TICKS > 0 ? GWATCH_FRAME_TICKS : 1;
        now = *rb->current_tick;
        wait_ticks = next_frame - now;
        if (wait_ticks > 0)
            rb->sleep(wait_ticks);
        else if (wait_ticks < -HZ)
            next_frame = now;

        if (geometry_dirty)
            rb->yield();
    }

    retro_unload_game();
    retro_deinit();
    /* Uncompressed packages are used directly by gwrom.  Keep their backing
     * buffer alive until the core has finished with every archive entry. */
    free_package_buffer();
    debug_log("exit frames=%u", video_frames);
    return PLUGIN_OK;
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *path = parameter;
    enum plugin_status status;

    if (path == NULL || path[0] == '\0')
    {
        rb->splash(HZ * 2, "No Game & Watch package");
        return PLUGIN_ERROR;
    }

    show_loading_status("Preparing");
    ensure_dirs();
    debug_open();
    debug_log("start path=%s", path);
    show_loading_status("Memory");
    debug_log("memory begin");

    if (!gwatch_platform_init_memory())
    {
        debug_log("memory failed");
        debug_close();
        rb->splash(HZ * 2, "Not enough plugin memory");
        return PLUGIN_ERROR;
    }

    debug_log("memory ok");
    show_loading_status("Starting core");
#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
    rb->cpu_boost(true);
#endif
    backlight_ignore_timeout();
    status = run_core(path);
#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
    rb->cpu_boost(false);
#endif
    backlight_use_settings();
    gwatch_platform_release_memory();
    debug_close();
    return status;
}
