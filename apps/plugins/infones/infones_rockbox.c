/* Rockbox platform glue for InfoNES. */

#include "plugin.h"
#include "lib/pluginlib_exit.h"
#include "InfoNES.h"
#include "InfoNES_System.h"

#include <stdarg.h>

#define INFONES_DISP_X ((LCD_WIDTH - NES_DISP_WIDTH) / 2)
#define INFONES_DISP_Y ((LCD_HEIGHT - NES_DISP_HEIGHT) / 2)
#define INFONES_SAVE_DIR ROCKBOX_DIR "/infones"
#define INFONES_FPS 60
#define INFONES_AUDIO_BUFS 6
#define INFONES_AUDIO_SAMPLES 1024
#define INFONES_AUDIO_START_BUFS 3
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

WORD NesPalette[64] =
{
    LCD_RGBPACK(102,102,102), LCD_RGBPACK(  0, 42,136),
    LCD_RGBPACK( 20, 18,167), LCD_RGBPACK( 59,  0,164),
    LCD_RGBPACK( 92,  0,126), LCD_RGBPACK(110,  0, 64),
    LCD_RGBPACK(108,  7,  0), LCD_RGBPACK( 86, 29,  0),
    LCD_RGBPACK( 51, 53,  0), LCD_RGBPACK( 11, 72,  0),
    LCD_RGBPACK(  0, 82,  0), LCD_RGBPACK(  0, 79,  8),
    LCD_RGBPACK(  0, 64, 77), LCD_RGBPACK(  0,  0,  0),
    LCD_RGBPACK(  0,  0,  0), LCD_RGBPACK(  0,  0,  0),
    LCD_RGBPACK(173,173,173), LCD_RGBPACK( 21, 95,217),
    LCD_RGBPACK( 66, 64,255), LCD_RGBPACK(117, 39,254),
    LCD_RGBPACK(160, 26,204), LCD_RGBPACK(183, 30,123),
    LCD_RGBPACK(181, 49, 32), LCD_RGBPACK(153, 78,  0),
    LCD_RGBPACK(107,109,  0), LCD_RGBPACK( 56,135,  0),
    LCD_RGBPACK( 12,147,  0), LCD_RGBPACK(  0,143, 50),
    LCD_RGBPACK(  0,124,141), LCD_RGBPACK(  0,  0,  0),
    LCD_RGBPACK(  0,  0,  0), LCD_RGBPACK(  0,  0,  0),
    LCD_RGBPACK(255,254,255), LCD_RGBPACK(100,176,255),
    LCD_RGBPACK(146,144,255), LCD_RGBPACK(198,118,255),
    LCD_RGBPACK(243,106,255), LCD_RGBPACK(254,110,204),
    LCD_RGBPACK(254,129,112), LCD_RGBPACK(234,158, 34),
    LCD_RGBPACK(188,190,  0), LCD_RGBPACK(136,216,  0),
    LCD_RGBPACK( 92,228, 48), LCD_RGBPACK( 69,224,130),
    LCD_RGBPACK( 72,205,222), LCD_RGBPACK( 79, 79, 79),
    LCD_RGBPACK(  0,  0,  0), LCD_RGBPACK(  0,  0,  0),
    LCD_RGBPACK(255,254,255), LCD_RGBPACK(192,223,255),
    LCD_RGBPACK(211,210,255), LCD_RGBPACK(232,200,255),
    LCD_RGBPACK(251,194,255), LCD_RGBPACK(254,196,234),
    LCD_RGBPACK(254,204,197), LCD_RGBPACK(247,216,165),
    LCD_RGBPACK(228,229,148), LCD_RGBPACK(207,238,150),
    LCD_RGBPACK(189,244,171), LCD_RGBPACK(179,243,204),
    LCD_RGBPACK(181,235,242), LCD_RGBPACK(184,184,184),
    LCD_RGBPACK(  0,  0,  0), LCD_RGBPACK(  0,  0,  0)
};

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
    }
    else if (audio_hwbuf)
    {
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

            d[0] = s[0];
            d[1] = s[0];
            d[2] = s[1];
            d[3] = s[2];
            d[4] = s[3];
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
            line[x] = src[(x * NES_DISP_WIDTH) / LCD_WIDTH];
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
    APU_Mute = 1;

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
    InfoNES_Cycle();
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
    pace_frame();
#if INFONES_SCALE_FILL_SCREEN
    fb_data *framebuffer = get_lcd_framebuffer();
    if (framebuffer)
    {
        scale_frame_fill_screen(framebuffer);
        rb->lcd_update();
        return;
    }
#endif

    rb->lcd_bitmap((const fb_data *)WorkFrame, INFONES_DISP_X,
                   INFONES_DISP_Y, NES_DISP_WIDTH, NES_DISP_HEIGHT);
    rb->lcd_update_rect(INFONES_DISP_X, INFONES_DISP_Y,
                        NES_DISP_WIDTH, NES_DISP_HEIGHT);
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
        if (buttons & BUTTON_SELECT)
            pad |= NES_PAD_A;
        if (buttons & BUTTON_PLAY)
            pad |= NES_PAD_B;
        if (buttons & BUTTON_MENU)
            pad |= NES_PAD_START;
    }

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

    audio_buf_samples = MAX(INFONES_AUDIO_SAMPLES, samples_per_sync * 4);
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
