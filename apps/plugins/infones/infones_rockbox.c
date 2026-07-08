/* Rockbox platform glue for InfoNES. */

#include "plugin.h"
#include "lib/pluginlib_exit.h"
#include "InfoNES.h"
#include "InfoNES_System.h"

#include <stdarg.h>

#define INFONES_DISP_X ((LCD_WIDTH - NES_DISP_WIDTH) / 2)
#define INFONES_DISP_Y ((LCD_HEIGHT - NES_DISP_HEIGHT) / 2)
#define INFONES_SAVE_DIR ROCKBOX_DIR "/infones"
#define INFONES_OPTIONS_PATH INFONES_SAVE_DIR "/options.cfg"
#define INFONES_PROFILE_LOG INFONES_SAVE_DIR "/profile.log"
#define INFONES_NTSC_FPS_X1000 60099
#define INFONES_PACE_SCALE 1000L
#define INFONES_FRAME_PERIOD_SCALED \
    ((HZ * INFONES_PACE_SCALE * 1000L + INFONES_NTSC_FPS_X1000 / 2) / \
     INFONES_NTSC_FPS_X1000)
#define INFONES_EARLY_PROFILE_FRAMES 120
#define INFONES_AUDIO_SAMPLES 1024
#if defined(IPOD_6G)
#define INFONES_AUDIO_BUFS 16
#define INFONES_AUDIO_START_BUFS 8
#define INFONES_AUDIO_FRAMES_PER_BUF 2
#define INFONES_DEFAULT_AUDIO_QUALITY 2
#define INFONES_HARDWARE_FRAMESKIP 1
#define INFONES_WAIT_YIELD_SCANLINES 256
#else
#define INFONES_AUDIO_BUFS 6
#define INFONES_AUDIO_START_BUFS 3
#define INFONES_AUDIO_FRAMES_PER_BUF 4
#define INFONES_DEFAULT_AUDIO_QUALITY 1
#define INFONES_WAIT_YIELD_SCANLINES 16
#define INFONES_HARDWARE_FRAMESKIP 0
#endif
#define INFONES_RUN_DOUBLE_TAP (HZ / 3)
#define INFONES_PROFILE_HIST_SIZE 32
#define INFONES_PULSE_TABLE_SIZE 31
#define INFONES_TND_TABLE_SIZE 203
#define INFONES_AUDIO_GAIN_NUM 1
#define INFONES_AUDIO_GAIN_DEN 4
#define INFONES_AUDIO_LIMIT 16000
#define INFONES_SAFE_VOLUME_DB (-50)
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
static long next_frame_tick_scaled;
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
static int audio_lp_prev;
static short audio_last_sample;
static int audio_sample_rate;
static bool audio_started;
static bool audio_ready;
static bool cpu_boosted;
static bool sound_enabled;
static bool autosave_enabled;
static int audio_quality;
static bool right_was_down;
static bool right_run_active;
static long right_last_tap_tick;
static bool left_was_down;
static bool left_run_active;
static long left_last_tap_tick;
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

struct infones_profile_bucket
{
    unsigned long total;
    unsigned long count;
    unsigned long hist[INFONES_PROFILE_HIST_SIZE];
    long peak;
};

struct infones_profile
{
    char rom_path[MAX_PATH];
    size_t plugin_buf_kib;
    long start_tick;
    long first_frame_tick;
    unsigned long frames;
    unsigned long frames_emulated;
    unsigned long frames_rendered;
    unsigned long frames_late;
    unsigned long frames_skipped;
    unsigned long pacer_resets;
    unsigned long frame_ticks_total;
    unsigned long scale_ticks_total;
    unsigned long sound_ticks_total;
    unsigned long sound_samples;
    unsigned long wait_yields;
    unsigned long pcm_underruns;
    unsigned long pcm_silence_fills;
    unsigned long pcm_low_water;
    unsigned long pcm_full_waits;
    unsigned long pcm_dropped_buffers;
    unsigned long audio_limited_samples;
    unsigned long audio_peak_abs;
    unsigned long palette_writes;
    unsigned long ppumask_updates;
    long frame_ticks_peak;
    long scale_ticks_peak;
    long sound_ticks_peak;
    struct infones_profile_bucket frame_bucket;
    struct infones_profile_bucket wait_bucket;
    struct infones_profile_bucket lcd_bucket;
    struct infones_profile_bucket cpu_bucket;
    struct infones_profile_bucket hsync_bucket;
    struct infones_profile_bucket apu_bucket;
    struct infones_profile_bucket sound_bucket;
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
static short nes_pulse_table[INFONES_PULSE_TABLE_SIZE];
static short nes_tnd_table[INFONES_TND_TABLE_SIZE];
static BYTE palette_ppumask;
static struct infones_profile profile;

static long pace_frame(void);
static void profile_write_log(void);
static void save_sram(void);

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

static void init_audio_mixer_tables(void)
{
    int i;

    nes_pulse_table[0] = 0;
    for (i = 1; i < INFONES_PULSE_TABLE_SIZE; i++)
        nes_pulse_table[i] = (short)((3130000L * i) / (8128 + 100 * i));

    nes_tnd_table[0] = 0;
    for (i = 1; i < INFONES_TND_TABLE_SIZE; i++)
        nes_tnd_table[i] = (short)((5363000L * i) / (24329 + 100 * i));
}

static void apply_safe_launch_volume(void)
{
    int safe_volume = INFONES_SAFE_VOLUME_DB;
    int min_volume = rb->sound_min(SOUND_VOLUME);
    int max_volume = rb->sound_max(SOUND_VOLUME);

    if (safe_volume < min_volume)
        safe_volume = min_volume;
    if (safe_volume > max_volume)
        safe_volume = max_volume;

    if (rb->global_status->volume > safe_volume)
        rb->sound_set(SOUND_VOLUME, safe_volume);
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

static void profile_record_bucket(struct infones_profile_bucket *bucket,
                                  long ticks)
{
    unsigned int hist_idx;

    if (ticks < 0)
        ticks = 0;

    bucket->total += ticks;
    bucket->count++;
    if (ticks > bucket->peak)
        bucket->peak = ticks;

    hist_idx = MIN((unsigned long)ticks,
                   (unsigned long)INFONES_PROFILE_HIST_SIZE - 1);
    bucket->hist[hist_idx]++;
}

static unsigned long profile_bucket_avg_x1000(
    const struct infones_profile_bucket *bucket)
{
    if (bucket->count == 0)
        return 0;

    return (bucket->total * 1000) / bucket->count;
}

static unsigned long profile_bucket_p99(
    const struct infones_profile_bucket *bucket)
{
    unsigned long wanted;
    unsigned long seen = 0;
    unsigned int idx;

    if (bucket->count == 0)
        return 0;

    wanted = (bucket->count * 99 + 99) / 100;
    for (idx = 0; idx < INFONES_PROFILE_HIST_SIZE; idx++)
    {
        seen += bucket->hist[idx];
        if (seen >= wanted)
            return idx;
    }

    return INFONES_PROFILE_HIST_SIZE - 1;
}

static void profile_record_frame(long frame_ticks, long wait_ticks,
                                 long lcd_ticks)
{
    if (profile.first_frame_tick == 0)
        profile.first_frame_tick = *rb->current_tick;

    profile.frames++;
    profile.frame_ticks_total += frame_ticks;
    profile.scale_ticks_total += lcd_ticks;
    if (frame_ticks > profile.frame_ticks_peak)
        profile.frame_ticks_peak = frame_ticks;
    if (lcd_ticks > profile.scale_ticks_peak)
        profile.scale_ticks_peak = lcd_ticks;

    profile_record_bucket(&profile.frame_bucket, frame_ticks);
    profile_record_bucket(&profile.wait_bucket, wait_ticks);
    profile_record_bucket(&profile.lcd_bucket, lcd_ticks);
}

static void profile_record_sound(long sound_ticks, int samples)
{
    profile.sound_ticks_total += sound_ticks;
    profile.sound_samples += samples;
    if (sound_ticks > profile.sound_ticks_peak)
        profile.sound_ticks_peak = sound_ticks;

    profile_record_bucket(&profile.sound_bucket, sound_ticks);
}

static void profile_maybe_write_early_snapshot(void)
{
    if (!profile.early_snapshot_written &&
        profile.frames >= INFONES_EARLY_PROFILE_FRAMES)
    {
        profile_write_log();
        profile.early_snapshot_written = true;
    }
}

static void options_reset(void)
{
    sound_enabled = true;
    autosave_enabled = true;
    audio_quality = INFONES_DEFAULT_AUDIO_QUALITY;
}

static void options_parse_line(const char *line)
{
    if (rb->strncmp(line, "sound=", 6) == 0)
        sound_enabled = line[6] != '0';
    else if (rb->strncmp(line, "autosave=", 9) == 0)
        autosave_enabled = line[9] != '0';
    else if (rb->strncmp(line, "audio_quality=", 14) == 0)
    {
        if (line[14] == '0')
            audio_quality = 0;
        else if (line[14] == '2')
            audio_quality = 2;
        else
            audio_quality = 1;
    }
}

static void options_load(void)
{
    char buf[128];
    char line[32];
    int fd;
    ssize_t got;
    int i;
    int j = 0;

    options_reset();

    fd = rb->open(INFONES_OPTIONS_PATH, O_RDONLY);
    if (fd < 0)
        return;

    got = rb->read(fd, buf, sizeof(buf) - 1);
    rb->close(fd);
    if (got <= 0)
        return;

    buf[got] = '\0';
    for (i = 0; i <= got; i++)
    {
        char c = buf[i];

        if (c == '\n' || c == '\r' || c == '\0')
        {
            if (j > 0)
            {
                line[j] = '\0';
                options_parse_line(line);
                j = 0;
            }
        }
        else if (j < (int)sizeof(line) - 1)
        {
            line[j++] = c;
        }
    }
}

long InfoNES_GetTicks(void)
{
    return *rb->current_tick;
}

int InfoNES_GetAudioQuality(void)
{
    return audio_quality;
}

void InfoNES_ProfileCpu(long ticks)
{
    profile_record_bucket(&profile.cpu_bucket, ticks);
}

void InfoNES_ProfileHSync(long ticks)
{
    profile_record_bucket(&profile.hsync_bucket, ticks);
}

void InfoNES_ProfileApu(long ticks)
{
    profile_record_bucket(&profile.apu_bucket, ticks);
}

void InfoNES_ProfileFrameEnd(int rendered)
{
    if (profile.first_frame_tick == 0)
        profile.first_frame_tick = *rb->current_tick;

    profile.frames_emulated++;
    if (rendered)
    {
        profile.frames_rendered++;
    }
    else
    {
        long wait_ticks = pace_frame();

        profile.frames_skipped++;
        profile_record_bucket(&profile.wait_bucket, wait_ticks);
    }

}

static void profile_write_log(void)
{
    int fd;
    long elapsed = *rb->current_tick - profile.start_tick;
    long emu_elapsed = elapsed;
    unsigned long effective_fps_x1000 = 0;
    unsigned long avg_frame_x1000 = 0;
    unsigned long avg_scale_x1000 = 0;
    unsigned long avg_sound_x1000 = 0;

    if (profile.first_frame_tick != 0)
        emu_elapsed = *rb->current_tick - profile.first_frame_tick;
    if (emu_elapsed > 0)
        effective_fps_x1000 =
            (profile.frames_emulated * HZ * 1000) / emu_elapsed;

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
                 "rom=\"%s\" elapsed_ticks=%ld emu_elapsed_ticks=%ld "
                 "hz=%d frames=%lu "
                 "target_fps_x1000=%d effective_fps_x1000=%lu "
                 "frames_emulated=%lu frames_rendered=%lu "
                 "frames_late=%lu frames_skipped=%lu pacer_resets=%lu "
                 "avg_frame_ticks_x1000=%lu peak_frame_ticks=%ld "
                 "p99_frame_ticks=%lu "
                 "avg_scale_ticks_x1000=%lu peak_scale_ticks=%ld "
                 "p99_lcd_ticks=%lu "
                 "avg_wait_ticks_x1000=%lu peak_wait_ticks=%ld "
                 "p99_wait_ticks=%lu "
                 "avg_cpu_ticks_x1000=%lu peak_cpu_ticks=%ld "
                 "p99_cpu_ticks=%lu "
                 "avg_hsync_ticks_x1000=%lu peak_hsync_ticks=%ld "
                 "p99_hsync_ticks=%lu "
                 "avg_apu_ticks_x1000=%lu peak_apu_ticks=%ld "
                 "p99_apu_ticks=%lu "
                 "audio_samples=%lu avg_sound_ticks_per_sample_x1000=%lu "
                 "avg_sound_call_ticks_x1000=%lu "
                 "peak_sound_call_ticks=%ld p99_sound_call_ticks=%lu "
                 "pcm_underruns=%lu pcm_silence_fills=%lu "
                 "pcm_low_water=%lu pcm_full_waits=%lu "
                 "pcm_dropped_buffers=%lu "
                 "audio_limited_samples=%lu audio_peak_abs=%lu "
                 "wait_yields=%lu wait_yield_scanlines=%d "
                 "audio_sample_rate=%d audio_quality=%d audio_bufs=%d "
                 "audio_buf_samples=%d configured_frameskip=%d "
                 "plugin_buf_kib=%lu "
                 "palette_writes=%lu ppumask_updates=%lu "
                 "ppu_palette="
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,"
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,"
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,"
                 "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
                 profile.rom_path, elapsed, emu_elapsed, HZ, profile.frames,
                 INFONES_NTSC_FPS_X1000, effective_fps_x1000,
                 profile.frames_emulated, profile.frames_rendered,
                 profile.frames_late, profile.frames_skipped,
                 profile.pacer_resets, avg_frame_x1000,
                 profile.frame_ticks_peak,
                 profile_bucket_p99(&profile.frame_bucket),
                 avg_scale_x1000, profile.scale_ticks_peak,
                 profile_bucket_p99(&profile.lcd_bucket),
                 profile_bucket_avg_x1000(&profile.wait_bucket),
                 profile.wait_bucket.peak,
                 profile_bucket_p99(&profile.wait_bucket),
                 profile_bucket_avg_x1000(&profile.cpu_bucket),
                 profile.cpu_bucket.peak,
                 profile_bucket_p99(&profile.cpu_bucket),
                 profile_bucket_avg_x1000(&profile.hsync_bucket),
                 profile.hsync_bucket.peak,
                 profile_bucket_p99(&profile.hsync_bucket),
                 profile_bucket_avg_x1000(&profile.apu_bucket),
                 profile.apu_bucket.peak,
                 profile_bucket_p99(&profile.apu_bucket),
                 profile.sound_samples, avg_sound_x1000,
                 profile_bucket_avg_x1000(&profile.sound_bucket),
                 profile.sound_ticks_peak,
                 profile_bucket_p99(&profile.sound_bucket),
                 profile.pcm_underruns, profile.pcm_silence_fills,
                 profile.pcm_low_water, profile.pcm_full_waits,
                 profile.pcm_dropped_buffers, profile.audio_limited_samples,
                 profile.audio_peak_abs,
                 profile.wait_yields,
                 INFONES_WAIT_YIELD_SCANLINES, audio_sample_rate,
                 audio_quality, INFONES_AUDIO_BUFS, audio_buf_samples,
                 INFONES_HARDWARE_FRAMESKIP,
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

static void restore_playback_state(void)
{
    audio_ready = false;
    rb->pcm_play_stop();
    rb->pcm_set_frequency(HW_SAMPR_DEFAULT);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
}

static bool hold_switch_exit(void)
{
#ifdef HAS_BUTTON_HOLD
    return rb->button_hold();
#else
    return false;
#endif
}

static void poll_quit(void)
{
    int buttons = rb->button_status();

    rb->button_clear_queue();

    if (hold_switch_exit() ||
        (buttons & (BUTTON_MENU | BUTTON_SELECT | BUTTON_PLAY)) ==
        (BUTTON_MENU | BUTTON_SELECT | BUTTON_PLAY))
        quit_requested = true;
}

static void drain_input_after_exit(void)
{
    long deadline = *rb->current_tick + HZ;

    rb->button_clear_queue();
    while (TIME_BEFORE(*rb->current_tick, deadline))
    {
        if (rb->button_status() == 0 && !hold_switch_exit())
            break;

        rb->button_clear_queue();
        rb->sleep(1);
    }
    rb->button_clear_queue();
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
        rb->memcpy(audio_hwbuf,
                   &audio_buf[audio_buf_samples * 2 * audio_read_idx],
                   audio_buf_samples * 2 * sizeof(short));
        audio_read_idx++;
        if (audio_read_idx >= INFONES_AUDIO_BUFS)
            audio_read_idx = 0;
        audio_queued--;
        if (audio_queued <= 1)
            profile.pcm_low_water++;
    }
    else if (audio_hwbuf)
    {
        int i;

        for (i = 0; i < audio_buf_samples; i++)
        {
            short sample = audio_last_sample -
                ((int)audio_last_sample * (i + 1)) / audio_buf_samples;

            audio_hwbuf[i * 2] = sample;
            audio_hwbuf[i * 2 + 1] = sample;
        }
        audio_last_sample = 0;
        profile.pcm_underruns++;
        profile.pcm_silence_fills++;
    }

    *start = audio_hwbuf;
    *size = audio_buf_samples * 2 * sizeof(short);
}

static void audio_submit_buffer(void)
{
    if (!audio_ready || audio_pos < audio_buf_samples)
        return;

    if (quit_requested)
        return;

    rb->pcm_play_lock();
    if (audio_queued >= INFONES_AUDIO_BUFS - 1)
    {
        profile.pcm_full_waits++;
        profile.pcm_dropped_buffers++;
        rb->pcm_play_unlock();
        audio_pos = 0;
        return;
    }

    audio_queued++;
    audio_write_idx++;
    if (audio_write_idx >= INFONES_AUDIO_BUFS)
        audio_write_idx = 0;
    audio_write_buf = &audio_buf[audio_buf_samples * 2 * audio_write_idx];
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
    int abs_sample;

    audio_dc_in_prev = sample;
    audio_dc_out_prev = filtered;
    audio_lp_prev += (filtered - audio_lp_prev) >> 2;
    if (audio_lp_prev > INFONES_AUDIO_LIMIT)
    {
        profile.audio_limited_samples++;
        return INFONES_AUDIO_LIMIT;
    }
    if (audio_lp_prev < -INFONES_AUDIO_LIMIT)
    {
        profile.audio_limited_samples++;
        return -INFONES_AUDIO_LIMIT;
    }
    abs_sample = audio_lp_prev < 0 ? -audio_lp_prev : audio_lp_prev;
    if ((unsigned long)abs_sample > profile.audio_peak_abs)
        profile.audio_peak_abs = abs_sample;
    return clamp_audio_sample(audio_lp_prev);
}

static int nes_channel_4bit(BYTE sample)
{
    int value = sample >> 4;

    return value > 15 ? 15 : value;
}

static int nes_mix_sample(BYTE pulse1, BYTE pulse2, BYTE triangle,
                          BYTE noise, BYTE dmc)
{
    int pulse_index = nes_channel_4bit(pulse1) + nes_channel_4bit(pulse2);
    int tnd_index = 3 * nes_channel_4bit(triangle) +
                    2 * (noise > 15 ? 15 : noise) +
                    (dmc > 127 ? 127 : dmc);

    if (pulse_index >= INFONES_PULSE_TABLE_SIZE)
        pulse_index = INFONES_PULSE_TABLE_SIZE - 1;
    if (tnd_index >= INFONES_TND_TABLE_SIZE)
        tnd_index = INFONES_TND_TABLE_SIZE - 1;

    return ((nes_pulse_table[pulse_index] + nes_tnd_table[tnd_index]) *
            INFONES_AUDIO_GAIN_NUM) / INFONES_AUDIO_GAIN_DEN;
}

static long pace_frame(void)
{
    long wait_start = *rb->current_tick;
    long now = wait_start;
    long now_scaled = now * INFONES_PACE_SCALE;
    long wait_ticks;

    if (next_frame_tick_scaled != 0)
    {
        long late_scaled = now_scaled - next_frame_tick_scaled;

        if (late_scaled > 2 * INFONES_PACE_SCALE)
            profile.frames_late++;

        while (!quit_requested && now_scaled < next_frame_tick_scaled)
        {
            long remaining_scaled = next_frame_tick_scaled - now_scaled;
            long remaining_ticks =
                (remaining_scaled + INFONES_PACE_SCALE - 1) /
                INFONES_PACE_SCALE;

            poll_quit();
            if (remaining_ticks > 1)
                rb->sleep(remaining_ticks - 1);
            else
                rb->yield();

            now = *rb->current_tick;
            now_scaled = now * INFONES_PACE_SCALE;
        }

        now = *rb->current_tick;
        now_scaled = now * INFONES_PACE_SCALE;
        if (now_scaled - next_frame_tick_scaled >
            (HZ / 2) * INFONES_PACE_SCALE)
        {
            next_frame_tick_scaled = now_scaled;
            profile.pacer_resets++;
        }
    }
    else
    {
        next_frame_tick_scaled = now_scaled;
    }

    next_frame_tick_scaled += INFONES_FRAME_PERIOD_SCALED;

    wait_ticks = *rb->current_tick - wait_start;
    return wait_ticks > 0 ? wait_ticks : 0;
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

    alloc_ptr = rb->plugin_get_audio_buffer(&buf_size);
    alloc_end = alloc_ptr + buf_size;
    profile_reset(rom_path, buf_size);
    rom_buf = NULL;
    vrom_buf = NULL;
    lcd_fb = NULL;
    quit_requested = false;
    sram_path_valid = false;
    next_frame_tick_scaled = 0;
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
    audio_lp_prev = 0;
    audio_last_sample = 0;
    audio_sample_rate = 0;
    audio_buf_samples = 0;
    audio_started = false;
    audio_ready = false;
    cpu_boosted = false;
    options_reset();
    right_was_down = false;
    right_run_active = false;
    right_last_tap_tick = 0;
    left_was_down = false;
    left_run_active = false;
    left_last_tap_tick = 0;
    palette_ppumask = 0;
#ifdef HAVE_WHEEL_POSITION
    wheel_pad_latch = 0;
    wheel_pad_latch_tick = 0;
#endif
    APU_Mute = 1;
    init_palette_tables();
    init_audio_mixer_tables();
    apply_safe_launch_volume();

#if defined(HAVE_ADJUSTABLE_CPU_FREQ)
    rb->cpu_boost(true);
    cpu_boosted = true;
#endif

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->button_clear_queue();
    draw_status("Loading ROM...");
    build_sram_path(rom_path);
    options_load();

    InfoNES_Init();
    if (InfoNES_Load(rom_path) < 0)
    {
        InfoNES_Fin();
        restore_playback_state();
        rb->plugin_release_audio_buffer();
#if defined(HAVE_ADJUSTABLE_CPU_FREQ)
        if (cpu_boosted)
            rb->cpu_boost(false);
#endif
        rb->splash(HZ * 2, "InfoNES load failed");
        return PLUGIN_ERROR;
    }
    load_sram();
    FrameSkip = INFONES_HARDWARE_FRAMESKIP;

    rb->lcd_clear_display();
    rb->lcd_update();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    InfoNES_Cycle();
    drain_input_after_exit();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    profile_write_log();
    if (autosave_enabled)
        save_sram();
    InfoNES_Fin();
    restore_playback_state();
    rb->plugin_release_audio_buffer();
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
    long wait_ticks;
    long scale_ticks;

    wait_ticks = pace_frame();
    scale_start = *rb->current_tick;
#if INFONES_SCALE_FILL_SCREEN
    fb_data *framebuffer = get_lcd_framebuffer();
    if (framebuffer)
    {
        scale_frame_fill_screen(framebuffer);
        rb->lcd_update();
        scale_ticks = *rb->current_tick - scale_start;
        profile_record_frame(*rb->current_tick - frame_start, wait_ticks,
                             scale_ticks);
        profile_maybe_write_early_snapshot();
        return;
    }
#endif

    rb->lcd_bitmap((const fb_data *)WorkFrame, INFONES_DISP_X,
                   INFONES_DISP_Y, NES_DISP_WIDTH, NES_DISP_HEIGHT);
    rb->lcd_update_rect(INFONES_DISP_X, INFONES_DISP_Y,
                        NES_DISP_WIDTH, NES_DISP_HEIGHT);
    scale_ticks = *rb->current_tick - scale_start;
    profile_record_frame(*rb->current_tick - frame_start, wait_ticks,
                         scale_ticks);
    profile_maybe_write_early_snapshot();
}

void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem)
{
    int buttons = rb->button_status();
    bool left_down = (buttons & BUTTON_LEFT) != 0;
    bool right_down = (buttons & BUTTON_RIGHT) != 0;
    long now = *rb->current_tick;
    DWORD pad = 0;

    if (left_down && !left_was_down)
    {
        if (left_last_tap_tick != 0 &&
            TIME_BEFORE(now, left_last_tap_tick + INFONES_RUN_DOUBLE_TAP))
            left_run_active = true;

        left_last_tap_tick = now;
    }
    else if (!left_down)
    {
        left_run_active = false;
    }
    left_was_down = left_down;

    if (right_down && !right_was_down)
    {
        if (right_last_tap_tick != 0 &&
            TIME_BEFORE(now, right_last_tap_tick + INFONES_RUN_DOUBLE_TAP))
            right_run_active = true;

        right_last_tap_tick = now;
    }
    else if (!right_down)
    {
        right_run_active = false;
    }
    right_was_down = right_down;

    if (hold_switch_exit())
    {
        quit_requested = true;
    }
    else if ((buttons & (BUTTON_MENU | BUTTON_SELECT | BUTTON_PLAY)) ==
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
    if (left_run_active && left_down)
    {
        pad &= ~NES_PAD_A;
        pad |= NES_PAD_LEFT | NES_PAD_B;
    }
    if (right_run_active && right_down)
    {
        pad &= ~NES_PAD_A;
        pad |= NES_PAD_RIGHT | NES_PAD_B;
    }

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
    wait_yield_count++;
    if ((wait_yield_count & (INFONES_WAIT_YIELD_SCANLINES - 1)) == 0)
    {
        poll_quit();
        profile.wait_yields++;
        rb->yield();
    }
}

void InfoNES_SoundInit(void)
{
}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate)
{
    if (!sound_enabled)
    {
        APU_Mute = 1;
        audio_ready = false;
        audio_sample_rate = 0;
        return -1;
    }

    rb->pcm_play_stop();

#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif

    audio_buf_samples = MAX(INFONES_AUDIO_SAMPLES,
                            samples_per_sync * INFONES_AUDIO_FRAMES_PER_BUF);
    audio_buf = infones_alloc(audio_buf_samples * 2 * INFONES_AUDIO_BUFS *
                              sizeof(short));
    audio_hwbuf = infones_alloc(audio_buf_samples * 2 * sizeof(short));
    if (!audio_buf || !audio_hwbuf)
    {
        APU_Mute = 1;
        return -1;
    }

    rb->memset(audio_buf, 0, audio_buf_samples * 2 * INFONES_AUDIO_BUFS *
               sizeof(short));
    rb->memset(audio_hwbuf, 0, audio_buf_samples * 2 * sizeof(short));

    audio_write_buf = audio_buf;
    audio_queued = 0;
    audio_read_idx = 0;
    audio_write_idx = 0;
    audio_pos = 0;
    audio_dc_in_prev = 0;
    audio_dc_out_prev = 0;
    audio_lp_prev = 0;
    audio_last_sample = 0;
    audio_started = false;
    audio_ready = true;
    audio_sample_rate = sample_rate;

    rb->pcm_set_frequency(sample_rate);
    APU_Mute = 0;
    return 0;
}

void InfoNES_SoundClose(void)
{
    restore_playback_state();
    audio_started = false;
    audio_buf = NULL;
    audio_hwbuf = NULL;
    audio_write_buf = NULL;
    audio_dc_in_prev = 0;
    audio_dc_out_prev = 0;
    audio_lp_prev = 0;
    audio_last_sample = 0;
    audio_sample_rate = 0;
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
        int mixed = nes_mix_sample(wave1[i], wave2[i], wave3[i], wave4[i],
                                   wave5[i]);
        short sample = filter_audio_sample(mixed);
        audio_last_sample = sample;
        audio_write_buf[audio_pos * 2] = sample;
        audio_write_buf[audio_pos * 2 + 1] = sample;
        audio_pos++;
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
