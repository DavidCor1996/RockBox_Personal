#include "rockmacros.h"

#include "loader.h"
#include "profiler.h"

#ifdef SIMULATOR
#include <stdlib.h>
#define ROCKBOY_PROFILE_AUTOWRITE_ENV "ROCKBOY_PROFILE_AUTOWRITE_FRAMES"
static const char *profile_rom_path;
static unsigned long autowrite_frames;
static bool autowrite_done;

static unsigned long parse_autowrite_frames(void)
{
    const char *value = getenv(ROCKBOY_PROFILE_AUTOWRITE_ENV);
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

static unsigned long avg_ticks(const struct rockboy_profile_totals *p,
                               enum rockboy_profile_counter which)
{
    return p->samples[which] ? p->total[which] / p->samples[which] : 0;
}

void rockboy_profile_reset(void)
{
    memset(&totals, 0, sizeof(totals));
}

void rockboy_profile_start(const char *rom_path)
{
#ifdef SIMULATOR
    profile_rom_path = rom_path;
    autowrite_frames = parse_autowrite_frames();
    autowrite_done = false;
#else
    (void)rom_path;
#endif
}

void rockboy_profile_add(enum rockboy_profile_counter which, unsigned long ticks)
{
    if (!rockboy_profile_is_enabled() || which >= ROCKBOY_TIME_COUNT)
        return;

    totals.total[which] += ticks;
    totals.samples[which]++;
    if (ticks > totals.peak[which])
        totals.peak[which] = ticks;
}

void rockboy_profile_frame_rendered(void)
{
    if (rockboy_profile_is_enabled()) {
        totals.rendered_frames++;
#ifdef SIMULATOR
        if (autowrite_frames && !autowrite_done &&
            totals.rendered_frames >= autowrite_frames) {
            autowrite_done = true;
            rockboy_profile_log_summary(profile_rom_path);
        }
#endif
    }
}

void rockboy_profile_frame_skipped(void)
{
    if (rockboy_profile_is_enabled())
        totals.skipped_frames++;
}

void rockboy_profile_pcm_underrun(void)
{
    if (rockboy_profile_is_enabled())
        totals.pcm_underruns++;
}

void rockboy_profile_count(enum rockboy_profile_event which, unsigned long count)
{
    if (!rockboy_profile_is_enabled() || which >= ROCKBOY_EVENT_COUNT)
        return;

    totals.events[which] += count;
}

const struct rockboy_profile_totals *rockboy_profile_get_totals(void)
{
    return &totals;
}

bool rockboy_profile_is_enabled(void)
{
    return options.profile != ROCKBOY_PROFILE_OFF;
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
             "cpu_avg_ticks=%lu cpu_peak_ticks=%lu "
             "lcd_render_avg_ticks=%lu lcd_render_peak_ticks=%lu "
             "scale_avg_ticks=%lu scale_peak_ticks=%lu "
             "blit_avg_ticks=%lu blit_peak_ticks=%lu "
             "audio_mix_avg_ticks=%lu audio_mix_peak_ticks=%lu "
             "pcm_wait_avg_ticks=%lu pcm_wait_peak_ticks=%lu "
             "pcm_underruns=%lu save_total_ticks=%lu "
             "cpu_ops=%lu slow_mem_reads=%lu slow_mem_writes=%lu "
             "vram_dirty_writes=%lu lcd_lines=%lu dmg_lines=%lu cgb_lines=%lu "
             "no_sprite_lines=%lu no_window_lines=%lu "
             "dmg_bg_only_eligible=%lu dmg_bg_window_no_spr_eligible=%lu "
             "cgb_no_sprite_lines=%lu\n",
             rom_path ? rom_path : "<unknown>",
             p->rendered_frames,
             p->skipped_frames,
             avg_ticks(p, ROCKBOY_TIME_FRAME),
             p->peak[ROCKBOY_TIME_FRAME],
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
             p->events[ROCKBOY_EVENT_LCD_CGB_NO_SPRITE_LINES]);
    close(fd);
}
