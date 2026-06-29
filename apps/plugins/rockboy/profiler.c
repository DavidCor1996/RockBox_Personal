#include "rockmacros.h"

#include "loader.h"
#include "pcm.h"
#include "profiler.h"

#ifdef SIMULATOR
#include <stdlib.h>
#define ROCKBOY_PROFILE_AUTOWRITE_ENV "ROCKBOY_PROFILE_AUTOWRITE_FRAMES"
#define ROCKBOY_PERF_AUTOWRITE_ENV "ROCKBOY_PERF_AUTOWRITE_FRAMES"
static const char *profile_rom_path;
static unsigned long autowrite_frames;
static bool autowrite_done;
static const char *perf_rom_path;
static unsigned long perf_autowrite_frames;
static bool perf_autowrite_done;
static unsigned long perf_start_tick;
static unsigned long perf_rendered_frames;
static unsigned long perf_skipped_frames;

static unsigned long parse_frame_count_env(const char *name)
{
    const char *value = getenv(name);
    char *end;
    unsigned long frames;

    if (!value || !value[0])
        return 0;

    frames = strtoul(value, &end, 10);
    if (frames == 0 || *end != '\0')
        return 0;

    return frames;
}
#endif

static struct rockboy_profile_totals totals;

#define ROCKBOY_GB_CPU_HZ 4194304ULL
#define ROCKBOY_GB_CYCLES_PER_FRAME 70224ULL

static unsigned long avg_ticks(const struct rockboy_profile_totals *p,
                               enum rockboy_profile_counter which)
{
    return p->samples[which] ? p->total[which] / p->samples[which] : 0;
}

static unsigned long avg_ticks_x1000(const struct rockboy_profile_totals *p,
                                     enum rockboy_profile_counter which)
{
    if (!p->samples[which])
        return 0;

    return (unsigned long)
        ((((unsigned long long)p->total[which] * 1000) +
          (p->samples[which] / 2)) / p->samples[which]);
}

static unsigned long rockboy_target_fps_x1000(void)
{
    return (unsigned long)
        (((ROCKBOY_GB_CPU_HZ * 1000) + (ROCKBOY_GB_CYCLES_PER_FRAME / 2)) /
         ROCKBOY_GB_CYCLES_PER_FRAME);
}

static unsigned long rockboy_target_frame_ticks_x1000(void)
{
    return (unsigned long)
        ((((unsigned long long)HZ * ROCKBOY_GB_CYCLES_PER_FRAME * 1000) +
          (ROCKBOY_GB_CPU_HZ / 2)) / ROCKBOY_GB_CPU_HZ);
}

void rockboy_profile_reset(void)
{
    memset(&totals, 0, sizeof(totals));
}

void rockboy_profile_start(const char *rom_path)
{
#ifdef SIMULATOR
    profile_rom_path = rom_path;
    autowrite_frames = parse_frame_count_env(ROCKBOY_PROFILE_AUTOWRITE_ENV);
    autowrite_done = false;
#else
    (void)rom_path;
#endif
}

void rockboy_perf_start(const char *rom_path)
{
#ifdef SIMULATOR
    perf_rom_path = rom_path;
    perf_autowrite_frames = parse_frame_count_env(ROCKBOY_PERF_AUTOWRITE_ENV);
    perf_autowrite_done = false;
    perf_start_tick = *rb->current_tick;
    perf_rendered_frames = 0;
    perf_skipped_frames = 0;
#else
    (void)rom_path;
#endif
}

void rockboy_perf_frame_rendered(void)
{
#ifdef SIMULATOR
    perf_rendered_frames++;
#endif
}

void rockboy_perf_frame_skipped(void)
{
#ifdef SIMULATOR
    perf_skipped_frames++;
#endif
}

void rockboy_perf_log_if_due(void)
{
#ifdef SIMULATOR
    int fd;
    char path[128];
    unsigned long total_frames = perf_rendered_frames + perf_skipped_frames;
    unsigned long elapsed_ticks;
    unsigned long frame_avg_ticks_x1000;
    unsigned long effective_fps_x1000;

    if (!perf_autowrite_frames || perf_autowrite_done ||
        total_frames < perf_autowrite_frames)
        return;

    elapsed_ticks = *rb->current_tick - perf_start_tick;
    if (!elapsed_ticks)
        return;

    frame_avg_ticks_x1000 = (unsigned long)
        ((((unsigned long long)elapsed_ticks * 1000) + (total_frames / 2)) /
         total_frames);
    effective_fps_x1000 = (unsigned long)
        ((((unsigned long long)total_frames * HZ * 1000) + (elapsed_ticks / 2)) /
         elapsed_ticks);

    snprintf(path, sizeof(path), "%s/performance.log", savedir);
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    perf_autowrite_done = true;
    fdprintf(fd,
             "rom=%s total_frames=%lu rendered_frames=%lu skipped_frames=%lu "
             "elapsed_ticks=%lu frame_avg_ticks_x1000=%lu "
             "target_frame_ticks_x1000=%lu effective_fps_x1000=%lu "
             "target_fps_x1000=%lu final_frameskip=%d maxskip=%d "
             "sound=%d pcm_hz=%d scaling=%d performance_preset=%d\n",
             perf_rom_path ? perf_rom_path : "<unknown>",
             total_frames,
             perf_rendered_frames,
             perf_skipped_frames,
             elapsed_ticks,
             frame_avg_ticks_x1000,
             rockboy_target_frame_ticks_x1000(),
             effective_fps_x1000,
             rockboy_target_fps_x1000(),
             options.frameskip,
             options.maxskip,
             options.sound,
             pcm.hz,
             options.scaling,
             options.performance_preset);
    close(fd);
#endif
}

void rockboy_profile_add_enabled(enum rockboy_profile_counter which,
                                 unsigned long ticks)
{
    if (which >= ROCKBOY_TIME_COUNT)
        return;

    totals.total[which] += ticks;
    totals.samples[which]++;
    if (ticks > totals.peak[which])
        totals.peak[which] = ticks;
}

void rockboy_profile_frame_rendered_enabled(void)
{
    totals.rendered_frames++;
#ifdef SIMULATOR
    if (autowrite_frames && !autowrite_done &&
        totals.rendered_frames >= autowrite_frames) {
        autowrite_done = true;
        rockboy_profile_log_summary(profile_rom_path);
    }
#endif
}

void rockboy_profile_frame_skipped_enabled(void)
{
    totals.skipped_frames++;
}

void rockboy_profile_pcm_underrun_enabled(void)
{
    totals.pcm_underruns++;
}

void rockboy_profile_pcm_drop_enabled(void)
{
    totals.pcm_dropped_buffers++;
}

void rockboy_profile_count_enabled(enum rockboy_profile_event which,
                                   unsigned long count)
{
    if (which >= ROCKBOY_EVENT_COUNT)
        return;

    totals.events[which] += count;
}

const struct rockboy_profile_totals *rockboy_profile_get_totals(void)
{
    return &totals;
}

void rockboy_profile_log_summary(const char *rom_path)
{
    int fd;
    char path[128];
    const struct rockboy_profile_totals *p;

    if (options.profile != ROCKBOY_PROFILE_OVERLAY_AND_LOG)
        return;

    p = rockboy_profile_get_totals();
    snprintf(path, sizeof(path), "%s/profile.log", savedir);
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    fdprintf(fd,
             "rom=%s rendered_frames=%lu skipped_frames=%lu "
             "frame_avg_ticks=%lu frame_peak_ticks=%lu "
             "frame_avg_ticks_x1000=%lu target_frame_ticks_x1000=%lu "
             "target_fps_x1000=%lu "
             "cpu_avg_ticks=%lu cpu_peak_ticks=%lu "
             "lcd_render_avg_ticks=%lu lcd_render_peak_ticks=%lu "
             "scale_avg_ticks=%lu scale_peak_ticks=%lu "
             "blit_avg_ticks=%lu blit_peak_ticks=%lu "
             "audio_mix_avg_ticks=%lu audio_mix_peak_ticks=%lu "
             "pcm_wait_avg_ticks=%lu pcm_wait_peak_ticks=%lu "
             "pcm_underruns=%lu pcm_dropped_buffers=%lu save_total_ticks=%lu "
             "cpu_ops=%lu slow_mem_reads=%lu slow_mem_writes=%lu "
             "vram_dirty_writes=%lu lcd_lines=%lu dmg_lines=%lu cgb_lines=%lu "
             "no_sprite_lines=%lu no_window_lines=%lu "
             "dmg_bg_only_eligible=%lu dmg_bg_window_no_spr_eligible=%lu "
             "cgb_no_sprite_lines=%lu "
             "dmg_bg_only_used=%lu dmg_bg_only_rejected=%lu "
             "dmg_bg_window_no_spr_used=%lu "
             "dmg_bg_window_no_spr_rejected=%lu "
             "cgb_bg_only_eligible=%lu cgb_bg_only_used=%lu "
             "cgb_bg_only_rejected=%lu "
             "cgb_bg_window_no_spr_eligible=%lu "
             "cgb_bg_window_no_spr_used=%lu "
             "cgb_bg_window_no_spr_rejected=%lu\n",
             rom_path ? rom_path : "<unknown>",
             p->rendered_frames,
             p->skipped_frames,
             avg_ticks(p, ROCKBOY_TIME_FRAME),
             p->peak[ROCKBOY_TIME_FRAME],
             avg_ticks_x1000(p, ROCKBOY_TIME_FRAME),
             rockboy_target_frame_ticks_x1000(),
             rockboy_target_fps_x1000(),
             avg_ticks(p, ROCKBOY_TIME_CPU),
             p->peak[ROCKBOY_TIME_CPU],
             avg_ticks(p, ROCKBOY_TIME_RENDER),
             p->peak[ROCKBOY_TIME_RENDER],
             avg_ticks(p, ROCKBOY_TIME_SCALE),
             p->peak[ROCKBOY_TIME_SCALE],
             avg_ticks(p, ROCKBOY_TIME_BLIT),
             p->peak[ROCKBOY_TIME_BLIT],
             avg_ticks(p, ROCKBOY_TIME_AUDIO_MIX),
             p->peak[ROCKBOY_TIME_AUDIO_MIX],
             avg_ticks(p, ROCKBOY_TIME_PCM_WAIT),
             p->peak[ROCKBOY_TIME_PCM_WAIT],
             p->pcm_underruns,
             p->pcm_dropped_buffers,
             p->total[ROCKBOY_TIME_SAVE],
             p->events[ROCKBOY_EVENT_CPU_OPS],
             p->events[ROCKBOY_EVENT_SLOW_MEM_READS],
             p->events[ROCKBOY_EVENT_SLOW_MEM_WRITES],
             p->events[ROCKBOY_EVENT_VRAM_DIRTY_WRITES],
             p->events[ROCKBOY_EVENT_LCD_LINES],
             p->events[ROCKBOY_EVENT_LCD_DMG_LINES],
             p->events[ROCKBOY_EVENT_LCD_CGB_LINES],
             p->events[ROCKBOY_EVENT_LCD_NO_SPRITE_LINES],
             p->events[ROCKBOY_EVENT_LCD_NO_WINDOW_LINES],
             p->events[ROCKBOY_EVENT_LCD_DMG_BG_ONLY_ELIGIBLE],
             p->events[ROCKBOY_EVENT_LCD_DMG_BG_WINDOW_NO_SPR_ELIGIBLE],
             p->events[ROCKBOY_EVENT_LCD_CGB_NO_SPRITE_LINES],
             p->events[ROCKBOY_EVENT_LCD_DMG_BG_ONLY_USED],
             p->events[ROCKBOY_EVENT_LCD_DMG_BG_ONLY_REJECTED],
             p->events[ROCKBOY_EVENT_LCD_DMG_BG_WINDOW_NO_SPR_USED],
             p->events[ROCKBOY_EVENT_LCD_DMG_BG_WINDOW_NO_SPR_REJECTED],
             p->events[ROCKBOY_EVENT_LCD_CGB_BG_ONLY_ELIGIBLE],
             p->events[ROCKBOY_EVENT_LCD_CGB_BG_ONLY_USED],
             p->events[ROCKBOY_EVENT_LCD_CGB_BG_ONLY_REJECTED],
             p->events[ROCKBOY_EVENT_LCD_CGB_BG_WINDOW_NO_SPR_ELIGIBLE],
             p->events[ROCKBOY_EVENT_LCD_CGB_BG_WINDOW_NO_SPR_USED],
             p->events[ROCKBOY_EVENT_LCD_CGB_BG_WINDOW_NO_SPR_REJECTED]);
    close(fd);
}
