#include "rockmacros.h"

#include "loader.h"
#include "profiler.h"

static struct rockboy_profile_totals totals;

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
             "rom=%s frames=%lu frame_avg_ticks=%lu render_avg_ticks=%lu "
             "scale_avg_ticks=%lu blit_avg_ticks=%lu audio_avg_ticks=%lu "
             "save_total_ticks=%lu\n",
             rom_path ? rom_path : "<unknown>",
             p->rendered_frames,
             p->samples[ROCKBOY_TIME_FRAME] ?
                 p->total[ROCKBOY_TIME_FRAME] / p->samples[ROCKBOY_TIME_FRAME] : 0,
             p->samples[ROCKBOY_TIME_RENDER] ?
                 p->total[ROCKBOY_TIME_RENDER] / p->samples[ROCKBOY_TIME_RENDER] : 0,
             p->samples[ROCKBOY_TIME_SCALE] ?
                 p->total[ROCKBOY_TIME_SCALE] / p->samples[ROCKBOY_TIME_SCALE] : 0,
             p->samples[ROCKBOY_TIME_BLIT] ?
                 p->total[ROCKBOY_TIME_BLIT] / p->samples[ROCKBOY_TIME_BLIT] : 0,
             p->samples[ROCKBOY_TIME_AUDIO] ?
                 p->total[ROCKBOY_TIME_AUDIO] / p->samples[ROCKBOY_TIME_AUDIO] : 0,
             p->total[ROCKBOY_TIME_SAVE]);
    close(fd);
}
