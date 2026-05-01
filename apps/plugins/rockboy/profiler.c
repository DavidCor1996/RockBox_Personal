#include "rockmacros.h"

#include "loader.h"
#include "profiler.h"

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
    if (rockboy_profile_is_enabled())
        totals.rendered_frames++;
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
             "pcm_underruns=%lu save_total_ticks=%lu\n",
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
             p->total[ROCKBOY_TIME_SAVE]);
    close(fd);
}
