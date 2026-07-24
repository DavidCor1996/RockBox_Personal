#include "plugin.h"
#include "lib/rockachievements.h"

#include "source/PokeMini.h"
#include "source/CommandLine.h"
#include "source/Hardware.h"
#include "source/Joystick.h"
#include "source/MinxAudio.h"
#include "source/MinxLCD.h"
#include "source/Video.h"
#include "source/Video_x3.h"

#define PM_SCREEN_W 96
#define PM_SCREEN_H 64
#define PM_SCALE 3
#define PM_FB_W (PM_SCREEN_W * PM_SCALE)
#define PM_FB_H (PM_SCREEN_H * PM_SCALE)
#define PM_FPS 72
#define PM_AUDIO_RATE 44100
#define PM_AUDIO_CHANNELS 2
#define PM_AUDIO_QUEUE 8
#define PM_AUDIO_MAX_SAMPLES 613
#define PM_AUDIO_GAIN_SHIFT 2

#define PM_BUTTON_POWER 9
#define PM_BUTTON_UP    10
#define PM_BUTTON_DOWN  11
#define PM_BUTTON_LEFT  4
#define PM_BUTTON_RIGHT 5
#define PM_BUTTON_A     1
#define PM_BUTTON_B     2
#define PM_BUTTON_SHAKE 6
#define PM_BUTTON_C     7

retro_log_printf_t log_cb = NULL;

static unsigned char *alloc_ptr;
static unsigned char *alloc_end;
static uint16_t *video_buffer;
static int16_t *audio_buffer;
static int16_t *audio_hwbuf;
static unsigned int held_buttons;
static long up_latch_tick;
static long down_latch_tick;
static unsigned int audio_sample_accum;
static volatile unsigned int audio_write_idx;
static volatile unsigned int audio_read_idx;
static volatile unsigned int audio_queued;
static unsigned int audio_buf_samples[PM_AUDIO_QUEUE];
static bool audio_ready;
static bool audio_started;
static char eeprom_path[MAX_PATH];
static struct rockachievements_runtime achievements;

static uint32_t achievements_peek(uint32_t address, uint32_t num_bytes,
                                  void *userdata)
{
    uint32_t value = 0;
    uint32_t index;

    (void)userdata;
    if (num_bytes > 4)
        num_bytes = 4;
    for (index = 0; index < num_bytes; ++index)
    {
        uint32_t current = address + index;
        uint8_t byte = 0;

        if (current < 0x1000)
            byte = PM_BIOS[current];
        else if (current < 0x2000)
            byte = PM_RAM[current - 0x1000];
        value |= (uint32_t)byte << (index * 8);
    }
    return value;
}

static void achievements_start(const char *rom_path)
{
    size_t available = alloc_ptr && alloc_end > alloc_ptr ?
                       (size_t)(alloc_end - alloc_ptr) : 0;
    void *workspace;

    if (!rockachievements_available(rom_path) ||
        available < ROCKACHIEVEMENTS_WORKSPACE_TARGET)
        return;
    workspace = pm_malloc(ROCKACHIEVEMENTS_WORKSPACE_TARGET);
    if (workspace != NULL)
        rockachievements_init(&achievements, rom_path, achievements_peek,
                              NULL, workspace,
                              ROCKACHIEVEMENTS_WORKSPACE_TARGET);
}

enum input_result
{
    INPUT_CONTINUE = 0,
    INPUT_QUIT,
    INPUT_USB,
};

static size_t align4(size_t size)
{
    return (size + 3) & ~(size_t)3;
}

void *pm_malloc(size_t size)
{
    unsigned char *result;

    if (size > (size_t)-1 - 3)
        return NULL;

    size = align4(size);
    if (!alloc_ptr || size > (size_t)(alloc_end - alloc_ptr))
        return NULL;

    result = alloc_ptr;
    alloc_ptr += size;
    return result;
}

void *pm_calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *ptr;

    if (size && bytes / size != nmemb)
        return NULL;

    ptr = pm_malloc(bytes);
    if (ptr)
        memset(ptr, 0, bytes);
    return ptr;
}

void pm_free(void *ptr)
{
    (void)ptr;
}

long pm_time(void *timer)
{
    long now = *rb->current_tick / HZ;
    if (timer)
        *(long *)timer = now;
    return now;
}

static const char *base_name(const char *path)
{
    const char *slash = rb->strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void build_eeprom_path(const char *rom_path)
{
    const char *name = base_name(rom_path);
    const char *slash = rb->strrchr(rom_path, '/');
    char base[MAX_PATH];
    char dir[MAX_PATH];
    char *dot = rb->strrchr(name, '.');

    rb->strlcpy(base, name, sizeof(base));
    if (dot && !rb->strcasecmp(dot, ".min"))
        base[dot - name] = '\0';

    if (slash)
    {
        size_t dir_len = (size_t)(slash - rom_path);
        rb->strlcpy(dir, rom_path, dir_len + 1);
        dir[dir_len] = '\0';
    }
    else
    {
        rb->strlcpy(dir, "/PokeMini", sizeof(dir));
    }

    rb->snprintf(eeprom_path, sizeof(eeprom_path), "%s/%s.eep", dir, base);
}

static bool load_min_rom(const char *path)
{
    int fd;
    ssize_t got;
    long size;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    size = rb->filesize(fd);
    if (size <= 0x2100 || size > 0x200000)
    {
        rb->close(fd);
        return false;
    }

    if (!PokeMini_NewMIN((uint32_t)size))
    {
        rb->close(fd);
        return false;
    }

    got = rb->read(fd, PM_ROM, size);
    rb->close(fd);
    return got == size;
}

static bool load_eeprom(void)
{
    int fd;
    ssize_t got;

    if (eeprom_path[0] == '\0' || !EEPROM)
        return false;

    fd = rb->open(eeprom_path, O_RDONLY);
    if (fd < 0)
        return false;

    got = rb->read(fd, EEPROM, 8192);
    rb->close(fd);
    PokeMini_EEPROMWritten = 0;
    return got == 8192;
}

static bool save_eeprom(void)
{
    int fd;
    ssize_t wrote;

    if (eeprom_path[0] == '\0' || !EEPROM)
        return false;

    fd = rb->open(eeprom_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    wrote = rb->write(fd, EEPROM, 8192);
    rb->close(fd);
    return wrote == 8192;
}

static void set_pm_button(int button, bool pressed)
{
    JoystickButtonsEvent(button, pressed ? 1 : 0);
}

static enum input_result handle_input(void)
{
    int button;
    long now = *rb->current_tick;
    bool up;
    bool down;
    bool left;
    bool right;
    bool a;
    bool b;
    bool c;
    bool shake;
    bool power;

    while ((button = rb->button_get(false)) != BUTTON_NONE)
    {
        unsigned int bare = button & ~(BUTTON_REPEAT | BUTTON_REL);

        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
            return INPUT_USB;

        if (bare & BUTTON_SCROLL_BACK)
            up_latch_tick = now + HZ / 8;
        if (bare & BUTTON_SCROLL_FWD)
            down_latch_tick = now + HZ / 8;

        if (button & BUTTON_REL)
            held_buttons &= ~bare;
        else
            held_buttons |= bare;
    }

    if ((held_buttons & BUTTON_MENU) && (held_buttons & BUTTON_PLAY))
        return INPUT_QUIT;

    up = TIME_BEFORE(now, up_latch_tick);
    down = TIME_BEFORE(now, down_latch_tick);
    left = held_buttons & BUTTON_LEFT;
    right = held_buttons & BUTTON_RIGHT;
    a = held_buttons & BUTTON_SELECT;
    b = held_buttons & BUTTON_PLAY;
    c = held_buttons & BUTTON_MENU;
    shake = (held_buttons & BUTTON_SELECT) && (held_buttons & BUTTON_PLAY);
    power = (held_buttons & BUTTON_SELECT) && (held_buttons & BUTTON_MENU);

    set_pm_button(PM_BUTTON_UP, up);
    set_pm_button(PM_BUTTON_DOWN, down);
    set_pm_button(PM_BUTTON_LEFT, left);
    set_pm_button(PM_BUTTON_RIGHT, right);
    set_pm_button(PM_BUTTON_A, a);
    set_pm_button(PM_BUTTON_B, b);
    set_pm_button(PM_BUTTON_C, c);
    set_pm_button(PM_BUTTON_SHAKE, shake);
    set_pm_button(PM_BUTTON_POWER, power);

    return INPUT_CONTINUE;
}

static void draw_frame(void)
{
    const int x = (LCD_WIDTH - PM_FB_W) / 2;
    const int y = (LCD_HEIGHT - PM_FB_H) / 2;

    if (!LCDDirty)
        return;

    PokeMini_VideoBlit(video_buffer, PM_FB_W);
    LCDDirty = 0;

    rb->lcd_bitmap((fb_data *)video_buffer, x, y, PM_FB_W, PM_FB_H);
    rb->lcd_update_rect(x, y, PM_FB_W, PM_FB_H);
}

static void audio_shutdown(void)
{
    long deadline;

    audio_ready = false;
    audio_started = false;
    audio_queued = 0;
    audio_read_idx = 0;
    audio_write_idx = 0;
    rb->pcm_play_stop();
    deadline = *rb->current_tick + HZ;
    while (rb->pcm_is_playing() && TIME_BEFORE(*rb->current_tick, deadline))
        rb->sleep(1);
    rb->pcm_set_frequency(HW_SAMPR_DEFAULT);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
}

static void audio_get_more(const void **start, size_t *size)
{
    if (audio_ready && audio_queued > 0)
    {
        unsigned int samples = audio_buf_samples[audio_read_idx];
        memcpy(audio_hwbuf,
               &audio_buffer[audio_read_idx * PM_AUDIO_MAX_SAMPLES * PM_AUDIO_CHANNELS],
               samples * PM_AUDIO_CHANNELS * sizeof(int16_t));
        audio_read_idx++;
        if (audio_read_idx >= PM_AUDIO_QUEUE)
            audio_read_idx = 0;
        audio_queued--;
        *start = audio_hwbuf;
        *size = samples * PM_AUDIO_CHANNELS * sizeof(int16_t);
        return;
    }

    memset(audio_hwbuf, 0,
           PM_AUDIO_MAX_SAMPLES * PM_AUDIO_CHANNELS * sizeof(int16_t));
    *start = audio_hwbuf;
    *size = PM_AUDIO_MAX_SAMPLES * PM_AUDIO_CHANNELS * sizeof(int16_t);
}

static void audio_submit_frame(void)
{
    unsigned int samples;
    unsigned int i;
    unsigned int write_idx;
    int16_t *dst;

    if (!audio_ready)
        return;

    audio_sample_accum += PM_AUDIO_RATE;
    samples = audio_sample_accum / PM_FPS;
    audio_sample_accum %= PM_FPS;
    if (samples > PM_AUDIO_MAX_SAMPLES)
        samples = PM_AUDIO_MAX_SAMPLES;

    rb->pcm_play_lock();
    if (audio_queued >= PM_AUDIO_QUEUE - 1)
    {
        rb->pcm_play_unlock();
        return;
    }
    write_idx = audio_write_idx;
    rb->pcm_play_unlock();

    dst = &audio_buffer[write_idx * PM_AUDIO_MAX_SAMPLES * PM_AUDIO_CHANNELS];
    MinxAudio_GetSamplesS16Ch(dst, (int)samples, PM_AUDIO_CHANNELS);
    for (i = 0; i < samples * PM_AUDIO_CHANNELS; i++)
        dst[i] >>= PM_AUDIO_GAIN_SHIFT;
    audio_buf_samples[write_idx] = samples;

    rb->pcm_play_lock();
    if (write_idx != audio_write_idx || audio_queued >= PM_AUDIO_QUEUE - 1)
    {
        rb->pcm_play_unlock();
        return;
    }
    audio_write_idx++;
    if (audio_write_idx >= PM_AUDIO_QUEUE)
        audio_write_idx = 0;
    audio_queued++;
    rb->pcm_play_unlock();

    if (!audio_started && audio_queued >= 2)
    {
        rb->pcm_play_data(audio_get_more, NULL, NULL, 0);
        audio_started = true;
    }
}

static void save_eeprom_if_possible(void)
{
    if (eeprom_path[0] == '\0')
        return;

    if (!save_eeprom())
        rb->splash(HZ, "Save failed");
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *rom = parameter;
    size_t audio_buf_size = 0;
    long next_frame_tick;
    unsigned int frame_tick_accum = 0;
    enum plugin_status status = PLUGIN_OK;
    bool core_created = false;
    bool palette_inited = false;
    bool cpu_boosted = false;
    bool audio_configured = false;
    bool rom_loaded = false;

    rb->lcd_setfont(FONT_SYSFIXED);
    eeprom_path[0] = '\0';

    if (!rom || !rom[0])
    {
        rb->splash(HZ * 2, "Open a .min ROM");
        return PLUGIN_OK;
    }

    alloc_ptr = rb->plugin_get_audio_buffer(&audio_buf_size);
    if (!alloc_ptr || audio_buf_size == 0)
    {
        rb->splash(HZ * 2, "No plugin memory");
        status = PLUGIN_ERROR;
        goto cleanup;
    }
    alloc_end = alloc_ptr + audio_buf_size;

    video_buffer = pm_calloc(PM_FB_W * PM_FB_H, sizeof(*video_buffer));
    if (!video_buffer)
    {
        rb->splash(HZ * 2, "No video memory");
        status = PLUGIN_ERROR;
        goto cleanup;
    }

    audio_buffer = pm_calloc(PM_AUDIO_QUEUE * PM_AUDIO_MAX_SAMPLES *
                             PM_AUDIO_CHANNELS, sizeof(*audio_buffer));
    audio_hwbuf = pm_calloc(PM_AUDIO_MAX_SAMPLES * PM_AUDIO_CHANNELS,
                            sizeof(*audio_hwbuf));
    if (!audio_buffer || !audio_hwbuf)
    {
        rb->splash(HZ * 2, "No audio memory");
        status = PLUGIN_ERROR;
        goto cleanup;
    }

    CommandLineInit();
    CommandLine.forcefreebios = 1;
    CommandLine.updatertc = 0;
    CommandLine.sound = MINX_AUDIO_GENERATED;
    CommandLine.lcdfilter = PokeMini_NoFilter;
    CommandLine.lcdmode = LCDMODE_2SHADES;
    CommandLine.rumblelvl = 0;
    CommandLine.synccycles = 64;

    if (!PokeMini_Create(0, 0))
    {
        rb->splash(HZ * 2, "PokeMini init failed");
        status = PLUGIN_ERROR;
        goto cleanup;
    }
    core_created = true;

    MinxAudio_ChangeEngine(CommandLine.sound);
    MinxAudio_ChangeFilter(1);
    PokeMini_VideoPalette_Init(PokeMini_BGR16, 0);
    palette_inited = true;
    PokeMini_VideoPalette_Index(CommandLine.palette, NULL,
                                CommandLine.lcdcontrast,
                                CommandLine.lcdbright);

    if (!PokeMini_SetVideo((TPokeMini_VideoSpec *)&PokeMini_Video3x3,
                           16, PokeMini_NoFilter, CommandLine.lcdmode))
    {
        rb->splash(HZ * 2, "Video init failed");
        status = PLUGIN_ERROR;
        goto cleanup;
    }

    if (!load_min_rom(rom))
    {
        rb->splashf(HZ * 2, "Bad ROM: %s", base_name(rom));
        status = PLUGIN_ERROR;
        goto cleanup;
    }
    rom_loaded = true;

    PokeMini_Reset(0);
    build_eeprom_path(rom);
    load_eeprom();
    achievements_start(rom);

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true);
    cpu_boosted = true;
#endif

    audio_ready = true;
    audio_started = false;
    audio_sample_accum = 0;
    audio_write_idx = 0;
    audio_read_idx = 0;
    audio_queued = 0;
    rb->pcm_play_stop();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->pcm_set_frequency(PM_AUDIO_RATE);
    audio_configured = true;

    rb->button_clear_queue();
    rb->lcd_clear_display();
    rb->lcd_update();
    LCDDirty = MINX_DIRTYSCR;
    next_frame_tick = *rb->current_tick;

    while (status == PLUGIN_OK)
    {
        enum input_result input_status = handle_input();
        if (input_status == INPUT_USB)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }
        if (input_status == INPUT_QUIT)
            break;

        PokeMini_EmulateFrame();
        rockachievements_do_frame(&achievements);
        audio_submit_frame();
        draw_frame();

        frame_tick_accum += HZ;
        next_frame_tick += frame_tick_accum / PM_FPS;
        frame_tick_accum %= PM_FPS;
        if (TIME_BEFORE(*rb->current_tick, next_frame_tick))
            rb->sleep(next_frame_tick - *rb->current_tick);
        else
            rb->yield();
    }

cleanup:
    rockachievements_shutdown(&achievements);
    if (cpu_boosted)
    {
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
        rb->cpu_boost(false);
#endif
    }

    if (audio_configured || alloc_ptr)
        audio_shutdown();
    if (alloc_ptr)
        rb->plugin_release_audio_buffer();

    if (palette_inited)
        PokeMini_VideoPalette_Free();
    if (rom_loaded)
        save_eeprom_if_possible();
    if (core_created)
        PokeMini_Destroy();
    rb->lcd_clear_display();
    rb->lcd_update();

    return status;
}
