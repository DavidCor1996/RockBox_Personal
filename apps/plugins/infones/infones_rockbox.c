/* Rockbox platform glue for InfoNES. */

#include "plugin.h"
#include "lib/pluginlib_exit.h"
#include "InfoNES.h"
#include "InfoNES_System.h"

#include <stdarg.h>

#define INFONES_DISP_X ((LCD_WIDTH - NES_DISP_WIDTH) / 2)
#define INFONES_DISP_Y ((LCD_HEIGHT - NES_DISP_HEIGHT) / 2)
#define INFONES_SAVE_DIR ROCKBOX_DIR "/infones"
#define INFONES_PROFILE_LOG INFONES_SAVE_DIR "/profile.log"
#define INFONES_FPS 60
#define INFONES_AUDIO_SAMPLES 1024
#if defined(IPOD_6G)
#define INFONES_AUDIO_BUFS 10
#define INFONES_AUDIO_START_BUFS 4
#define INFONES_AUDIO_FRAMES_PER_BUF 2
#else
#define INFONES_AUDIO_BUFS 6
#define INFONES_AUDIO_START_BUFS 3
#define INFONES_AUDIO_FRAMES_PER_BUF 4
#endif
#define INFONES_AUDIO_SCALE 28
#define INFONES_WAIT_YIELD_SCANLINES 16
#define INFONES_SCALE_FILL_SCREEN \
    (LCD_WIDTH != NES_DISP_WIDTH || LCD_HEIGHT != NES_DISP_HEIGHT)

#define NES_PAD_B      (1u << 0)
#define NES_PAD_A      (1u << 1)
#define NES_PAD_SELECT (1u << 2)
#define NES_PAD_START  (1u << 3)
#define NES_PAD_UP     (1u << 4)
#define NES_PAD_DOWN   (1u << 5)
#define NES_PAD_LEFT   (1u << 6)
#define NES_PAD_RIGHT  (1u << 7)

static unsigned char *rom_buf;
static unsigned char *vrom_buf;
static fb_data *lcd_fb;
static unsigned char *alloc_ptr;
static unsigned char *alloc_end;
static bool quit_requested;
static char sram_path[MAX_PATH];
static bool sram_path_valid;
static long next_frame_tick;
static unsigned int frame_tick_accum;
static unsigned int wait_yield_count;
static short *audio_buf;
static short *audio_hwbuf;
static short *audio_write_buf;
static volatile int audio_queued;
static volatile int audio_read_idx;
static int audio_write_idx;
static int audio_pos;
static int audio_buf_samples;
static int audio_dc_in_prev;
static int audio_dc_out_prev;
static bool audio_started;
static bool audio_ready;
static bool cpu_boosted;
#ifdef HAVE_WHEEL_POSITION
static DWORD wheel_pad_latch;
static long wheel_pad_latch_tick;
#endif

struct infones_rgb
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct infones_profile
{
    char rom_path[MAX_PATH];
    size_t plugin_buf_kib;
    long start_tick;
    unsigned long frames;
    unsigned long frame_ticks_total;
    unsigned long scale_ticks_total;
    unsigned long sound_ticks_total;
    unsigned long sound_samples;
    unsigned long pcm_underruns;
    unsigned long pcm_low_water;
    unsigned long pcm_full_waits;
    unsigned long palette_writes;
    unsigned long ppumask_updates;
    long frame_ticks_peak;
    long scale_ticks_peak;
    long sound_ticks_peak;
    bool early_snapshot_written;
};

static const struct infones_rgb nes_palette_rgb[64] =
{
    {124,124,124}, {  0,  0,252}, {  0,  0,188}, { 68, 40,188},
    {148,  0,132}, {168,  0, 32}, {168, 16,  0}, {136, 20,  0},
    { 80, 48,  0}, {  0,120,  0}, {  0,104,  0}, {  0, 88,  0},
    {  0, 64, 88}, {  0,  0,  0}, {  0,  0,  0}, {  0,  0,  0},
    {188,188,188}, {  0,120,248}, {  0, 88,248}, {104, 68,252},
    {216,  0,204}, {228,  0, 88}, {248, 56,  0}, {228, 92, 16},
    {172,124,  0}, {  0,184,  0}, {  0,168,  0}, {  0,168, 68},
    {  0,136,136}, {  0,  0,  0}, {  0,  0,  0}, {  0,  0,  0},
    {248,248,248}, { 60,188,252}, {104,136,252}, {152,120,248},
    {248,120,248}, {248, 88,152}, {248,120, 88}, {252,160, 68},
    {248,184,  0}, {184,248, 24}, { 88,216, 84}, { 88,248,152},
    {  0,232,216}, {120,120,120}, {  0,  0,  0}, {  0,  0,  0},
    {252,252,252}, {164,228,252}, {184,184,248}, {216,184,248},
    {248,184,248}, {248,164,192}, {240,208,176}, {252,224,168},
    {248,216,120}, {216,248,120}, {184,248,184}, {184,248,216},
    {  0,252,252}, {248,216,248}, {  0,  0,  0}, {  0,  0,  0}
};

WORD NesPalette[64];
static WORD nes_palette_emph[8][64];
static BYTE palette_ppumask;
static struct infones_profile profile;

static void profile_write_log(void);

static int emphasis_dim(int value)
{
    return (value * 3) >> 2;
}

static void init_palette_tables(void)
{
    int emph;
    int color;

    for (emph = 0; emph < 8; emph++)
    {
        for (color = 0; color < 64; color++)
        {
            int r = nes_palette_rgb[color].r;
            int g = nes_palette_rgb[color].g;
            int b = nes_palette_rgb[color].b;

            if (emph & 1)
            {
                g = emphasis_dim(g);
                b = emphasis_dim(b);
            }
            if (emph & 2)
            {
                r = emphasis_dim(r);
                b = emphasis_dim(b);
            }
            if (emph & 4)
            {
                r = emphasis_dim(r);
                g = emphasis_dim(g);
            }

            nes_palette_emph[emph][color] = LCD_RGBPACK(r, g, b);
        }
    }

    rb->memcpy(NesPalette, nes_palette_emph[0], sizeof(NesPalette));
}

static WORD palette_native(BYTE color, bool backdrop)
{
    int emph = (palette_ppumask & R1_BACKCOLOR) >> 5;
    WORD native;

    color &= 0x3f;
    if (palette_ppumask & R1_MONOCHROME)
        color &= 0x30;

    native = nes_palette_emph[emph][color] & ~INFONES_BACKDROP_MARKER;
    return native | (backdrop ? INFONES_BACKDROP_MARKER : 0);
}

static inline fb_data display_pixel(fb_data pixel)
{
    return pixel & ~INFONES_BACKDROP_MARKER;
}

static void set_palette_slot(int slot, BYTE value)
{
    slot &= 0x1f;
    PPURAM[0x3f00 + slot] = value & 0x3f;
}

static void rebuild_palette_table(void)
{
    BYTE backdrop = PPURAM[0x3f00] & 0x3f;
    int slot;

    for (slot = 0; slot < 0x20; slot++)
    {
        bool transparent = (slot & 3) == 0;
        BYTE value = transparent ? backdrop : PPURAM[0x3f00 + slot] & 0x3f;

        PalTable[slot] = palette_native(value, transparent);
    }
}

void InfoNES_WritePalette(WORD wAddr, BYTE byData)
{
    int slot = wAddr & 0x1f;
    BYTE value = byData & 0x3f;

    profile.palette_writes++;

    if ((slot & 3) == 0)
    {
        int bg_slot = slot & 0x0f;

        set_palette_slot(bg_slot, value);
        set_palette_slot(bg_slot | 0x10, value);
    }
    else
    {
        set_palette_slot(slot, value);
    }

    rebuild_palette_table();
}

void InfoNES_SetPPUMask(BYTE byData)
{
    BYTE new_mask = byData & (R1_BACKCOLOR | R1_MONOCHROME);

    if (new_mask == palette_ppumask)
        return;

    palette_ppumask = new_mask;
    profile.ppumask_updates++;
    rebuild_palette_table();
}

static void profile_reset(const char *rom_path, size_t buf_size)
{
    rb->memset(&profile, 0, sizeof(profile));
    rb->strlcpy(profile.rom_path, rom_path, sizeof(profile.rom_path));
    profile.plugin_buf_kib = buf_size / 1024;
    profile.start_tick = *rb->current_tick;
}

static void profile_record_frame(long frame_ticks, long scale_ticks)
{
    profile.frames++;
    profile.frame_ticks_total += frame_ticks;
    profile.scale_ticks_total += scale_ticks;
    if (frame_ticks > profile.frame_ticks_peak)
        profile.frame_ticks_peak = frame_ticks;
    if (scale_ticks > profile.scale_ticks_peak)
        profile.scale_ticks_peak = scale_ticks;
}

static void profile_record_sound(long sound_ticks, int samples)
{
    profile.sound_ticks_total += sound_ticks;
    profile.sound_samples += samples;
    if (sound_ticks > profile.sound_ticks_peak)
        profile.sound_ticks_peak = sound_ticks;
}

static void profile_maybe_write_early_snapshot(void)
{
    if (!profile.early_snapshot_written && profile.frames >= INFONES_FPS * 2)
    {
        profile_write_log();
        profile.early_snapshot_written = true;
    }
}

static void profile_write_log(void)
{
    int fd;
    long elapsed = *rb->current_tick - profile.start_tick;
    unsigned long avg_frame_x1000 = 0;
    unsigned long avg_scale_x1000 = 0;
    unsigned long avg_sound_x1000 = 0;

    if (profile.frames > 0)
    {
        avg_frame_x1000 =
            (profile.frame_ticks_total * 1000) / profile.frames;
        avg_scale_x1000 =
            (profile.scale_ticks_total * 1000) / profile.frames;
    }
    if (profile.sound_samples > 0)
        avg_sound_x1000 =
            (profile.sound_ticks_total * 1000) / profile.sound_samples;

    rb->mkdir(INFONES_SAVE_DIR);
    fd = rb->open(INFONES_PROFILE_LOG, O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd,
                 "rom=\"%s\" elapsed_ticks=%ld hz=%d frames=%lu "
                 "avg_frame_ticks_x1000=%lu peak_frame_ticks=%ld "
                 "avg_scale_ticks_x1000=%lu peak_scale_ticks=%ld "
                 "audio_samples=%lu avg_sound_ticks_per_sample_x1000=%lu "
                 "peak_sound_call_ticks=%ld pcm_underruns=%lu "
                 "pcm_low_water=%lu pcm_full_waits=%lu audio_bufs=%d "
                 "audio_buf_samples=%d plugin_buf_kib=%lu "
                 "palette_writes=%lu ppumask_updates=%lu "
                 "ppu_palette="
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,"
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,"
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,"
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
                 profile.rom_path, elapsed, HZ, profile.frames,
                 avg_frame_x1000, profile.frame_ticks_peak,
                 avg_scale_x1000, profile.scale_ticks_peak,
                 profile.sound_samples, avg_sound_x1000,
                 profile.sound_ticks_peak, profile.pcm_underruns,
                 profile.pcm_low_water, profile.pcm_full_waits,
                 INFONES_AUDIO_BUFS, audio_buf_samples,
                 (unsigned long)profile.plugin_buf_kib,
                 profile.palette_writes, profile.ppumask_updates,
                 PPURAM[0x3f00] & 0x3f, PPURAM[0x3f01] & 0x3f,
                 PPURAM[0x3f02] & 0x3f, PPURAM[0x3f03] & 0x3f,
                 PPURAM[0x3f04] & 0x3f, PPURAM[0x3f05] & 0x3f,
                 PPURAM[0x3f06] & 0x3f, PPURAM[0x3f07] & 0x3f,
                 PPURAM[0x3f08] & 0x3f, PPURAM[0x3f09] & 0x3f,
                 PPURAM[0x3f0a] & 0x3f, PPURAM[0x3f0b] & 0x3f,
                 PPURAM[0x3f0c] & 0x3f, PPURAM[0x3f0d] & 0x3f,
                 PPURAM[0x3f0e] & 0x3f, PPURAM[0x3f0f] & 0x3f,
                 PPURAM[0x3f10] & 0x3f, PPURAM[0x3f11] & 0x3f,
                 PPURAM[0x3f12] & 0x3f, PPURAM[0x3f13] & 0x3f,
                 PPURAM[0x3f14] & 0x3f, PPURAM[0x3f15] & 0x3f,
                 PPURAM[0x3f16] & 0x3f, PPURAM[0x3f17] & 0x3f,
                 PPURAM[0x3f18] & 0x3f, PPURAM[0x3f19] & 0x3f,
                 PPURAM[0x3f1a] & 0x3f, PPURAM[0x3f1b] & 0x3f,
                 PPURAM[0x3f1c] & 0x3f, PPURAM[0x3f1d] & 0x3f,
                 PPURAM[0x3f1e] & 0x3f, PPURAM[0x3f1f] & 0x3f);
    rb->close(fd);
}

static void *infones_alloc(size_t bytes)
{
    unsigned char *p;

    bytes = (bytes + 3) & ~((size_t)3);
    if (!alloc_ptr || alloc_ptr + bytes > alloc_end)
        return NULL;

    p = alloc_ptr;
    alloc_ptr += bytes;
    return p;
}

static bool read_full(int fd, void *buf, size_t bytes)
{
    unsigned char *p = buf;

    while (bytes > 0)
    {
        ssize_t got = rb->read(fd, p, bytes);
        if (got <= 0)
            return false;
        p += got;
        bytes -= got;
    }

    return true;
}

static bool write_full(int fd, const void *buf, size_t bytes)
{
    const unsigned char *p = buf;

    while (bytes > 0)
    {
        ssize_t put = rb->write(fd, p, bytes);
        if (put <= 0)
            return false;
        p += put;
        bytes -= put;
    }

    return true;
}

static bool skip_full(int fd, size_t bytes)
{
    unsigned char scratch[128];

    while (bytes > 0)
    {
        size_t now = MIN(bytes, sizeof(scratch));
        if (!read_full(fd, scratch, now))
            return false;
        bytes -= now;
    }

    return true;
}

static void draw_status(const char *msg)
{
    rb->lcd_clear_display();
    rb->lcd_puts(0, 0, "InfoNES");
    rb->lcd_puts_scroll(0, 2, msg);
    rb->lcd_update();
}

static void poll_quit(void)
{
    int buttons = rb->button_status();

    rb->button_clear_queue();

    if ((buttons & (BUTTON_MENU | BUTTON_SELECT | BUTTON_PLAY)) ==
        (BUTTON_MENU | BUTTON_SELECT | BUTTON_PLAY))
        quit_requested = true;
}

#ifdef HAVE_WHEEL_POSITION
static DWORD wheel_touch_pad(int buttons)
{
    int wheel = rb->wheel_status();
    int zone;
    long now;
    DWORD pad;

    if (wheel < 0)
        return 0;

    zone = (wheel + 6) / 12;
    if (zone > 7)
        zone = 0;

    switch (zone)
    {
    case 0:
        pad = NES_PAD_UP;
        break;
    case 1:
        pad = NES_PAD_UP | NES_PAD_RIGHT;
        break;
    case 2:
        pad = NES_PAD_RIGHT;
        break;
    case 3:
        pad = NES_PAD_DOWN | NES_PAD_RIGHT;
        break;
    case 4:
        pad = NES_PAD_DOWN;
        break;
    case 5:
        pad = NES_PAD_DOWN | NES_PAD_LEFT;
        break;
    case 6:
        pad = NES_PAD_LEFT;
        break;
    case 7:
        pad = NES_PAD_UP | NES_PAD_LEFT;
        break;
    default:
        return 0;
    }

    now = *rb->current_tick;

    if (((buttons & BUTTON_PLAY) && (pad & NES_PAD_DOWN)) ||
        ((buttons & BUTTON_MENU) && (pad & NES_PAD_UP)))
    {
        if (wheel_pad_latch &&
            TIME_BEFORE(now, wheel_pad_latch_tick + HZ / 3))
            return wheel_pad_latch;

        return 0;
    }

    wheel_pad_latch = pad;
    wheel_pad_latch_tick = now;
    return pad;
}
#endif

static void audio_get_more(const void **start, size_t *size)
{
    if (audio_ready && audio_queued > 0)
    {
        rb->memcpy(audio_hwbuf, &audio_buf[audio_buf_samples * audio_read_idx],
                   audio_buf_samples * sizeof(short));
        audio_read_idx++;
        if (audio_read_idx >= INFONES_AUDIO_BUFS)
            audio_read_idx = 0;
        audio_queued--;
        if (audio_queued <= 1)
            profile.pcm_low_water++;
    }
    else if (audio_hwbuf)
    {
        profile.pcm_underruns++;
        rb->memset(audio_hwbuf, 0, audio_buf_samples * sizeof(short));
    }

    *start = audio_hwbuf;
    *size = audio_buf_samples * sizeof(short);
}

static void audio_submit_buffer(void)
{
    if (!audio_ready || audio_pos < audio_buf_samples)
        return;

    while (!quit_requested && audio_queued >= INFONES_AUDIO_BUFS - 1)
    {
        profile.pcm_full_waits++;
        poll_quit();
        rb->yield();
    }

    if (quit_requested)
        return;

    rb->pcm_play_lock();
    if (audio_queued < INFONES_AUDIO_BUFS - 1)
    {
        audio_queued++;
        audio_write_idx++;
        if (audio_write_idx >= INFONES_AUDIO_BUFS)
            audio_write_idx = 0;
        audio_write_buf = &audio_buf[audio_buf_samples * audio_write_idx];
    }
    rb->pcm_play_unlock();

    audio_pos = 0;

    if (!audio_started && audio_queued >= INFONES_AUDIO_START_BUFS)
    {
        rb->pcm_play_data(audio_get_more, NULL, NULL, 0);
        audio_started = true;
    }
}

static short clamp_audio_sample(int sample)
{
    if (sample > 32767)
        return 32767;
    if (sample < -32768)
        return -32768;
    return sample;
}

static short filter_audio_sample(int sample)
{
    int filtered = sample - audio_dc_in_prev +
                   ((audio_dc_out_prev * 255) >> 8);

    audio_dc_in_prev = sample;
    audio_dc_out_prev = filtered;
    return clamp_audio_sample(filtered);
}

static void pace_frame(void)
{
    long now = *rb->current_tick;

    if (next_frame_tick != 0)
    {
        while (!quit_requested && (now = *rb->current_tick) < next_frame_tick)
        {
            long remaining = next_frame_tick - now;
            poll_quit();
            if (remaining > 1)
                rb->sleep(remaining - 1);
            else
                rb->yield();
        }

        now = *rb->current_tick;
        if (now - next_frame_tick > HZ / 2)
        {
            next_frame_tick = now;
            frame_tick_accum = 0;
        }
    }
    else
    {
        next_frame_tick = now;
    }

    frame_tick_accum += HZ;
    next_frame_tick += frame_tick_accum / INFONES_FPS;
    frame_tick_accum %= INFONES_FPS;
}

static fb_data *get_lcd_framebuffer(void)
{
    if (!lcd_fb)
    {
        struct viewport *vp_main =
            *(rb->screens[SCREEN_MAIN]->current_viewport);
        lcd_fb = vp_main->buffer->fb_ptr;
    }
    return lcd_fb;
}

static void scale_frame_fill_screen(fb_data *dst)
{
#if LCD_WIDTH == 320 && LCD_HEIGHT == 240 && NES_DISP_WIDTH == 256 && \
    NES_DISP_HEIGHT == 240
    const fb_data *src = (const fb_data *)WorkFrame;
    int y;

    for (y = 0; y < NES_DISP_HEIGHT; y++)
    {
        int group;
        for (group = 0; group < NES_DISP_WIDTH / 4; group++)
        {
            const fb_data *s = src + group * 4;
            fb_data *d = dst + group * 5;

            d[0] = display_pixel(s[0]);
            d[1] = display_pixel(s[0]);
            d[2] = display_pixel(s[1]);
            d[3] = display_pixel(s[2]);
            d[4] = display_pixel(s[3]);
        }
        src += NES_DISP_WIDTH;
        dst += LCD_WIDTH;
    }
#else
    int y;

    for (y = 0; y < LCD_HEIGHT; y++)
    {
        const fb_data *src = (const fb_data *)WorkFrame +
                             ((y * NES_DISP_HEIGHT) / LCD_HEIGHT) *
                             NES_DISP_WIDTH;
        fb_data *line = dst + y * LCD_WIDTH;
        int x;

        for (x = 0; x < LCD_WIDTH; x++)
            line[x] = display_pixel(src[(x * NES_DISP_WIDTH) / LCD_WIDTH]);
    }
#endif
}

static void build_sram_path(const char *rom_path)
{
    const char *base = rb->strrchr(rom_path, '/');
    char name[MAX_PATH];
    char *dot;

    base = base ? base + 1 : rom_path;
    rb->strip_extension(name, sizeof(name), base);

    dot = rb->strrchr(name, '.');
    if (dot)
        *dot = '\0';

    rb->mkdir(INFONES_SAVE_DIR);
    sram_path_valid = rb->snprintf(sram_path, sizeof(sram_path),
                                   INFONES_SAVE_DIR "/%s.sav", name) <
                      (int)sizeof(sram_path);
}

static void load_sram(void)
{
    int fd;

    if (!sram_path_valid || !ROM_SRAM)
        return;

    fd = rb->open(sram_path, O_RDONLY);
    if (fd < 0)
        return;

    read_full(fd, SRAM, SRAM_SIZE);
    rb->close(fd);
}

static void save_sram(void)
{
    int fd;

    if (!sram_path_valid || !ROM_SRAM)
        return;

    fd = rb->creat(sram_path, 0666);
    if (fd < 0)
        return;

    write_full(fd, SRAM, SRAM_SIZE);
    rb->close(fd);
}

enum plugin_status plugin_start(const void *parameter)
{
    size_t buf_size;
    const char *rom_path = parameter;

    if (!rom_path)
    {
        rb->splash(HZ, "Open a .nes file");
        return PLUGIN_ERROR;
    }

    alloc_ptr = rb->plugin_get_buffer(&buf_size);
    alloc_end = alloc_ptr + buf_size;
    profile_reset(rom_path, buf_size);
    rom_buf = NULL;
    vrom_buf = NULL;
    lcd_fb = NULL;
    quit_requested = false;
    sram_path_valid = false;
    next_frame_tick = 0;
    frame_tick_accum = 0;
    wait_yield_count = 0;
    audio_buf = NULL;
    audio_hwbuf = NULL;
    audio_write_buf = NULL;
    audio_queued = 0;
    audio_read_idx = 0;
    audio_write_idx = 0;
    audio_pos = 0;
    audio_dc_in_prev = 0;
    audio_dc_out_prev = 0;
    audio_buf_samples = 0;
    audio_started = false;
    audio_ready = false;
    cpu_boosted = false;
    palette_ppumask = 0;
#ifdef HAVE_WHEEL_POSITION
    wheel_pad_latch = 0;
    wheel_pad_latch_tick = 0;
#endif
    APU_Mute = 1;
    init_palette_tables();

#if defined(HAVE_ADJUSTABLE_CPU_FREQ)
    rb->cpu_boost(true);
    cpu_boosted = true;
#endif

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->button_clear_queue();
    draw_status("Loading ROM...");
    build_sram_path(rom_path);

    InfoNES_Init();
    if (InfoNES_Load(rom_path) < 0)
    {
        InfoNES_Fin();
#if defined(HAVE_ADJUSTABLE_CPU_FREQ)
        if (cpu_boosted)
            rb->cpu_boost(false);
#endif
        rb->splash(HZ * 2, "InfoNES load failed");
        return PLUGIN_ERROR;
    }
    load_sram();

    rb->lcd_clear_display();
    rb->lcd_update();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    InfoNES_Cycle();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    profile_write_log();
    save_sram();
    InfoNES_Fin();
    rb->button_clear_queue();

#if defined(HAVE_ADJUSTABLE_CPU_FREQ)
    if (cpu_boosted)
        rb->cpu_boost(false);
#endif

    return PLUGIN_OK;
}

int InfoNES_Menu(void)
{
    return quit_requested ? -1 : 0;
}

int InfoNES_ReadRom(const char *pszFileName)
{
    int fd;
    size_t rom_size;
    size_t vrom_size;

    fd = rb->open(pszFileName, O_RDONLY);
    if (fd < 0)
    {
        rb->splash(HZ * 2, "Cannot open ROM");
        return -1;
    }

    if (!read_full(fd, &NesHeader, sizeof(NesHeader)))
        goto fail;

    if (NesHeader.byID[0] != 'N' || NesHeader.byID[1] != 'E' ||
        NesHeader.byID[2] != 'S' || NesHeader.byID[3] != 0x1a)
    {
        rb->splash(HZ * 2, "Not an iNES ROM");
        goto fail;
    }

    if (NesHeader.byInfo1 & 4)
    {
        if (!read_full(fd, &SRAM[0x1000], 512))
            goto fail;
    }

    rom_size = (size_t)NesHeader.byRomSize * 0x4000;
    if (rom_size == 0)
        goto fail;

    rom_buf = infones_alloc(rom_size);
    if (!rom_buf || !read_full(fd, rom_buf, rom_size))
        goto fail;
    ROM = rom_buf;

    vrom_size = (size_t)NesHeader.byVRomSize * 0x2000;
    if (vrom_size > 0)
    {
        vrom_buf = infones_alloc(vrom_size);
        if (!vrom_buf || !read_full(fd, vrom_buf, vrom_size))
            goto fail;
        VROM = vrom_buf;
    }
    else
    {
        off_t remaining = rb->filesize(fd) - rb->lseek(fd, 0, SEEK_CUR);
        if (remaining > 0)
            skip_full(fd, remaining);
        VROM = NULL;
    }

    rb->close(fd);
    return 0;

fail:
    rb->close(fd);
    return -1;
}

void InfoNES_ReleaseRom(void)
{
    ROM = NULL;
    VROM = NULL;
    rom_buf = NULL;
    vrom_buf = NULL;
}

void InfoNES_LoadFrame(void)
{
    long frame_start = *rb->current_tick;
    long scale_start;
    long scale_ticks;

    pace_frame();
    scale_start = *rb->current_tick;
#if INFONES_SCALE_FILL_SCREEN
    fb_data *framebuffer = get_lcd_framebuffer();
    if (framebuffer)
    {
        scale_frame_fill_screen(framebuffer);
        rb->lcd_update();
        scale_ticks = *rb->current_tick - scale_start;
        profile_record_frame(*rb->current_tick - frame_start, scale_ticks);
        profile_maybe_write_early_snapshot();
        return;
    }
#endif

    rb->lcd_bitmap((const fb_data *)WorkFrame, INFONES_DISP_X,
                   INFONES_DISP_Y, NES_DISP_WIDTH, NES_DISP_HEIGHT);
    rb->lcd_update_rect(INFONES_DISP_X, INFONES_DISP_Y,
                        NES_DISP_WIDTH, NES_DISP_HEIGHT);
    scale_ticks = *rb->current_tick - scale_start;
    profile_record_frame(*rb->current_tick - frame_start, scale_ticks);
    profile_maybe_write_early_snapshot();
}

void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem)
{
    int buttons = rb->button_status();
    DWORD pad = 0;

    if ((buttons & (BUTTON_MENU | BUTTON_SELECT | BUTTON_PLAY)) ==
        (BUTTON_MENU | BUTTON_SELECT | BUTTON_PLAY))
    {
        quit_requested = true;
    }
    else if ((buttons & (BUTTON_MENU | BUTTON_PLAY)) ==
             (BUTTON_MENU | BUTTON_PLAY))
    {
        pad |= NES_PAD_SELECT;
    }
    else
    {
#if defined(SIMULATOR)
        if (buttons & BUTTON_SELECT)
            pad |= NES_PAD_A;
        if (buttons & BUTTON_PLAY)
            pad |= NES_PAD_B;
#else
        if (buttons & BUTTON_SELECT)
            pad |= NES_PAD_B;
        if (buttons & BUTTON_PLAY)
            pad |= NES_PAD_A;
#endif
        if (buttons & BUTTON_MENU)
            pad |= NES_PAD_START;
    }

#ifdef HAVE_WHEEL_POSITION
    pad |= wheel_touch_pad(buttons);
#endif

    if (buttons & BUTTON_SCROLL_BACK)
        pad |= NES_PAD_UP;
    if (buttons & BUTTON_SCROLL_FWD)
        pad |= NES_PAD_DOWN;
    if (buttons & BUTTON_LEFT)
        pad |= NES_PAD_LEFT;
    if (buttons & BUTTON_RIGHT)
        pad |= NES_PAD_RIGHT;

    poll_quit();

    *pdwPad1 = pad;
    *pdwPad2 = 0;
    *pdwSystem = quit_requested ? PAD_SYS_QUIT : 0;
}

void *InfoNES_MemoryCopy(void *dest, const void *src, int count)
{
    return rb->memcpy(dest, src, count);
}

void *InfoNES_MemorySet(void *dest, int c, int count)
{
    return rb->memset(dest, c, count);
}

void InfoNES_DebugPrint(char *pszMsg)
{
    DEBUGF("InfoNES: %s", pszMsg);
}

void InfoNES_Wait(void)
{
    poll_quit();
    wait_yield_count++;
    if ((wait_yield_count & (INFONES_WAIT_YIELD_SCANLINES - 1)) == 0)
        rb->yield();
}

void InfoNES_SoundInit(void)
{
}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate)
{
    rb->pcm_play_stop();

#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif

    audio_buf_samples = MAX(INFONES_AUDIO_SAMPLES,
                            samples_per_sync * INFONES_AUDIO_FRAMES_PER_BUF);
    audio_buf = infones_alloc(audio_buf_samples * INFONES_AUDIO_BUFS *
                              sizeof(short));
    audio_hwbuf = infones_alloc(audio_buf_samples * sizeof(short));
    if (!audio_buf || !audio_hwbuf)
    {
        APU_Mute = 1;
        return -1;
    }

    rb->memset(audio_buf, 0, audio_buf_samples * INFONES_AUDIO_BUFS *
               sizeof(short));
    rb->memset(audio_hwbuf, 0, audio_buf_samples * sizeof(short));

    audio_write_buf = audio_buf;
    audio_queued = 0;
    audio_read_idx = 0;
    audio_write_idx = 0;
    audio_pos = 0;
    audio_dc_in_prev = 0;
    audio_dc_out_prev = 0;
    audio_started = false;
    audio_ready = true;

    rb->pcm_set_frequency(sample_rate);
    APU_Mute = 0;
    return 0;
}

void InfoNES_SoundClose(void)
{
    rb->pcm_play_stop();
    rb->pcm_set_frequency(HW_SAMPR_DEFAULT);
    audio_ready = false;
    audio_started = false;
    audio_buf = NULL;
    audio_hwbuf = NULL;
    audio_write_buf = NULL;
    audio_dc_in_prev = 0;
    audio_dc_out_prev = 0;
}

void InfoNES_SoundOutput(int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3,
                         BYTE *wave4, BYTE *wave5)
{
    long sound_start = *rb->current_tick;
    int i;

    if (!audio_ready)
        return;

    for (i = 0; i < samples; i++)
    {
        int mixed = wave1[i] + wave2[i] + wave3[i] + wave4[i] + wave5[i];
        audio_write_buf[audio_pos++] =
            filter_audio_sample(mixed * INFONES_AUDIO_SCALE);
        audio_submit_buffer();
    }

    profile_record_sound(*rb->current_tick - sound_start, samples);
}

void InfoNES_MessageBox(char *pszMsg, ...)
{
    char buf[80];
    va_list ap;

    va_start(ap, pszMsg);
    rb->vsnprintf(buf, sizeof(buf), pszMsg, ap);
    va_end(ap);
    rb->splashf(HZ * 2, "%s", buf);
}
