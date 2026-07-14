#include "sm64_rockbox.h"
#include "tlsf.h"

/* Rockbox and SM64 both use this historical macro name for unrelated ids. */
#ifdef MODEL_NUMBER
#undef MODEL_NUMBER
#endif
#include "upstream/include/sm64.h"
#include "upstream/src/audio/external.h"
#include "upstream/src/game/game_init.h"
#include "upstream/src/game/main.h"
#include "upstream/src/game/memory.h"
#include "upstream/src/game/sound_init.h"
#include "upstream/src/pc/audio/audio_api.h"
#include "upstream/src/pc/gfx/gfx_pc.h"
#include "upstream/src/pc/gfx/gfx_soft.h"
#include "upstream/src/pc/gfx/gfx_window_manager_api.h"

#include <stdarg.h>

#if UINTPTR_MAX > UINT32_MAX
/* The upstream PC port doubles pointer-heavy level allocations on 64-bit
 * hosts.  Match its doubled static pool in the simulator. */
#define SM64_MAIN_POOL_SIZE (0x165000 * 2)
#else
#define SM64_MAIN_POOL_SIZE 0x165000
#endif
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528

struct sm64_rockbox_state sm64_rb;

/* pc_main.c owns these on desktop builds.  Rockbox supplies the same port
 * entry points directly, so keep only the engine globals it would define. */
OSMesg D_80339BEC;
OSMesgQueue gSIEventMesgQueue;
s8 gResetTimer;
s8 D_8032C648;
s8 gDebugLevelSelect;
s8 gShowProfiler;
s8 gShowDebugText;

extern struct AudioAPI sm64_audio_api;
extern struct GfxWindowManagerAPI sm64_window_api;
extern void thread5_game_loop(void *arg);
extern void game_loop_one_iteration(void);
extern void create_next_audio_buffer(int16_t *samples, uint32_t count);

bool configFullscreen = false;
bool configDrawSky = true;
bool configFiltering = true;
bool configEnableSound = true;
bool configEnableFog = true;
unsigned int configFrameskip = 1;
unsigned int configScreenWidth = 160;
unsigned int configScreenHeight = 120;
unsigned int configKeyA;
unsigned int configKeyB;
unsigned int configKeyStart;
unsigned int configKeyR;
unsigned int configKeyZ;
unsigned int configKeyCUp;
unsigned int configKeyCDown;
unsigned int configKeyCLeft;
unsigned int configKeyCRight;
unsigned int configKeyStickUp;
unsigned int configKeyStickDown;
unsigned int configKeyStickLeft;
unsigned int configKeyStickRight;

static bool game_initialized;
static bool gfx_initialized;
static int log_fd = -1;
/*
 * Native Rockbox gives the main thread an 8 KiB stack. Keeping this 4.25 KiB
 * staging buffer in produce_one_frame() left too little room for the first
 * real level-script/render call (frames 1 and 2 only sleep). The simulator's
 * host stack hid that overflow.
 */
static int16_t frame_audio_buffer[SAMPLES_HIGH * 4];

static void show_boot_status(const char *status)
{
    int title_width;
    int status_width;
    int line_height;

    rb->lcd_set_viewport(NULL);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_getstringsize("Super Mario 64", &title_width, &line_height);
    rb->lcd_getstringsize(status, &status_width, NULL);
    rb->lcd_putsxy((LCD_WIDTH - title_width) / 2,
                   (LCD_HEIGHT - line_height * 2) / 2,
                   "Super Mario 64");
    rb->lcd_putsxy((LCD_WIDTH - status_width) / 2,
                   (LCD_HEIGHT - line_height * 2) / 2 + line_height,
                   status);
    rb->lcd_update();
}

static void ensure_directories(void)
{
    rb->mkdir(ROCKBOX_DIR "/games");
    rb->mkdir(ROCKBOX_DIR "/games/n64");
    rb->mkdir(SM64_SAVE_DIR);
    rb->mkdir(PLUGIN_GAMES_DATA_DIR "/sm64");
}

void sm64_logf(const char *format, ...)
{
    char line[256];
    va_list args;
    int length;

    va_start(args, format);
    length = rb->vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length < 0)
        return;
    if (log_fd < 0)
        log_fd = rb->open(SM64_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd >= 0)
        rb->fdprintf(log_fd, "%s\n", line);
#ifdef SIMULATOR
    DEBUGF("SM64: %s\n", line);
#endif
}

int sm64_sprintf(char *buffer, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = rb->vsnprintf(buffer, 1024, format, args);
    va_end(args);
    return result;
}

int sm64_snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = rb->vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int sm64_puts(const char *text)
{
    sm64_logf("%s", text ? text : "(null)");
    return 0;
}

/* N64 libc entry points still used directly by the original engine. */
void bzero(void *destination, size_t size)
{
    rb->memset(destination, 0, size);
}

void bcopy(const void *source, void *destination, size_t size)
{
    rb->memmove(destination, source, size);
}

void sm64_abort(void)
{
    sm64_rb.fatal = true;
    sm64_rb.quit = true;
    sm64_logf("fatal: upstream abort");
}

void sm64_exit(int status)
{
    if (status != 0)
        sm64_rb.fatal = true;
    sm64_rb.quit = true;
}

int sm64_rockbox_save_read(void *buffer, size_t size)
{
    int fd = rb->open(SM64_SAVE_PATH, O_RDONLY);
    int result = -1;
    if (fd >= 0)
    {
        result = rb->read(fd, buffer, size) == (ssize_t)size ? 0 : -1;
        rb->close(fd);
    }
    return result;
}

int sm64_rockbox_save_write(const void *buffer, size_t size)
{
    static const char temporary[] = SM64_SAVE_PATH ".tmp";
    int fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int result = -1;
    if (fd >= 0)
    {
        result = rb->write(fd, buffer, size) == (ssize_t)size ? 0 : -1;
        rb->close(fd);
    }
    if (result == 0)
    {
        rb->remove(SM64_SAVE_PATH);
        result = rb->rename(temporary, SM64_SAVE_PATH);
    }
    else
        rb->remove(temporary);
    return result;
}

void dispatch_audio_sptask(struct SPTask *task)
{
    (void)task;
}

void set_vblank_handler(s32 index, struct VblankHandler *handler,
                        OSMesgQueue *queue, OSMesg *message)
{
    (void)index;
    (void)handler;
    (void)queue;
    (void)message;
}

void send_display_list(struct SPTask *task)
{
    if (game_initialized)
        gfx_run((Gfx *)task->task.t.data_ptr);
}

void game_exit(void)
{
    sm64_rb.quit = true;
}

static void produce_one_frame(void)
{
    int samples_left;
    uint32_t samples;
    const unsigned long frame = sm64_rb.frames;
    const bool trace_frame = frame < 6 || (frame < 180 && frame % 10 == 0);
    const long started = *rb->current_tick;

    if (trace_frame)
        sm64_logf("frame %lu begin tick=%ld", frame, started);
    gfx_start_frame();
    game_loop_one_iteration();
    samples_left = sm64_audio_api.buffered();
    samples = samples_left < sm64_audio_api.get_desired_buffered() ?
              SAMPLES_HIGH : SAMPLES_LOW;
    create_next_audio_buffer(frame_audio_buffer, samples);
    create_next_audio_buffer(frame_audio_buffer + samples * 2, samples);
    sm64_audio_api.play((const uint8_t *)frame_audio_buffer, samples * 4 * 2);
    gfx_end_frame();
    if (trace_frame)
        sm64_logf("frame %lu complete ticks=%ld rendered=%lu", frame,
                  *rb->current_tick - started, sm64_rb.rendered_frames);
}

static bool unthrottled_test(void)
{
#ifdef SIMULATOR
    return getenv("SM64_TEST_UNTHROTTLED") != NULL;
#else
    return false;
#endif
}

static void parse_test_options(void)
{
#ifdef SIMULATOR
    const char *frames = getenv("SM64_TEST_FRAMES");
    if (frames)
        sm64_rb.test_frames = rb->atoi(frames);
#endif
}

static void pace_frame(void)
{
    long now;

    if (unthrottled_test())
        return;
    sm64_rb.next_frame_scaled += HZ;
    while (!sm64_rb.quit)
    {
        now = *rb->current_tick;
        if (now * 30 >= sm64_rb.next_frame_scaled)
            break;
        rb->yield();
    }
    now = *rb->current_tick;
    if (now * 30 - sm64_rb.next_frame_scaled > HZ * 10)
        sm64_rb.next_frame_scaled = now * 30;
}

static enum plugin_status run_game(void)
{
    void *main_pool;
    bool first_frame = true;

    show_boot_status("Loading memory...");
    sm64_logf("init main pool begin");
    main_pool = tlsf_malloc(SM64_MAIN_POOL_SIZE);
    if (!main_pool)
    {
        rb->splash(HZ * 2, "SM64: not enough game memory");
        return PLUGIN_ERROR;
    }
    main_pool_init(main_pool, (unsigned char *)main_pool + SM64_MAIN_POOL_SIZE);
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);
    sm64_logf("init main pool ready");

    show_boot_status("Starting video...");
    sm64_logf("init renderer begin");
    gfx_init(&sm64_window_api, &gfx_soft_api, "Super Mario 64", true);
    gfx_initialized = true;
    sm64_logf("init renderer ready");
    show_boot_status("Starting audio...");
    if (!sm64_audio_api.init())
        configEnableSound = false;
    sm64_logf("init engine audio begin");
    audio_init();
    sound_init();
    sm64_logf("init engine audio ready");
    show_boot_status("Starting game...");
    sm64_logf("init game state begin");
    thread5_game_loop(NULL);
    game_initialized = true;
    sm64_logf("init game state ready");

    sm64_rb.next_frame_scaled = *rb->current_tick * 30;
    while (!sm64_rb.quit)
    {
        long elapsed;
        sm64_rb.frame_started = *rb->current_tick;
        sm64_rb.render_frame = true;
        sm64_video_set_render_allowed(true);
        if (first_frame)
        {
            show_boot_status("Rendering first frame...");
            sm64_logf("first frame begin");
        }
        produce_one_frame();
        if (first_frame)
        {
            sm64_logf("first frame ready");
            first_frame = false;
        }
        sm64_rb.frames++;
        elapsed = *rb->current_tick - sm64_rb.frame_started;
        if (elapsed * 30 > HZ)
            sm64_rb.overruns++;
        if (sm64_rb.test_frames > 0 &&
            sm64_rb.frames >= (unsigned long)sm64_rb.test_frames)
            sm64_rb.quit = true;
        pace_frame();
    }
    sm64_video_dump_test_frame();
    return sm64_rb.usb_connected ? PLUGIN_USB_CONNECTED :
           (sm64_rb.fatal ? PLUGIN_ERROR : PLUGIN_OK);
}

static void cleanup(void)
{
    sm64_audio_shutdown();
    if (gfx_initialized)
    {
        gfx_shutdown();
        gfx_initialized = false;
    }
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    sm64_logf("exit frames=%lu rendered=%lu skipped=%lu overruns=%lu",
              sm64_rb.frames, sm64_rb.rendered_frames,
              sm64_rb.skipped_frames, sm64_rb.overruns);
    backlight_use_settings();
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
}

enum plugin_status plugin_start(const void *parameter)
{
    size_t size;
    unsigned char *buffer;
    enum plugin_status status;
    (void)parameter;

    rb->memset(&sm64_rb, 0, sizeof(sm64_rb));
    ensure_directories();
    rb->remove(SM64_LOG_PATH);
    sm64_logf("overlay entry");
    parse_test_options();
    buffer = rb->plugin_get_audio_buffer(&size);
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    sm64_logf("audio arena base=%p size=%lu overlay=%p-%p", buffer,
              (unsigned long)size, plugin_start_addr, plugin_end_addr);
#else
    sm64_logf("audio arena base=%p size=%lu", buffer, (unsigned long)size);
#endif
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    if ((uintptr_t)buffer < (uintptr_t)plugin_start_addr)
        size = MIN(size, (size_t)((uintptr_t)plugin_start_addr -
                                 (uintptr_t)buffer));
#endif
    if (init_memory_pool(size, buffer) == (size_t)-1)
    {
        rb->splash(HZ * 2, "SM64: memory setup failed");
        return PLUGIN_ERROR;
    }
    sm64_logf("tlsf arena ready size=%lu", (unsigned long)size);
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true);
    sm64_logf("cpu boost frequency=%ld", *rb->cpu_frequency);
#endif
    backlight_ignore_timeout();
    sm64_logf("start arena=%lu", (unsigned long)size);
    status = run_game();
    cleanup();
    if (log_fd >= 0)
    {
        rb->close(log_fd);
        log_fd = -1;
    }
    destroy_memory_pool(buffer);
    rb->plugin_release_audio_buffer();
    return status;
}
