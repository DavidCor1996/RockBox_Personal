#include "anarch_platform.h"

#include <fcntl.h>

#define PROFILE_FILE ROCKBOX_DIR "/games/anarch/profile.log"
#define PROFILE_BUCKETS 201

static uint32_t render_histogram[PROFILE_BUCKETS];
static uint32_t render_samples;
static uint32_t render_total;
static unsigned int render_worst;
static uint32_t late_frames;
static uint32_t lcd_samples;
static uint32_t lcd_total;
static unsigned int lcd_worst;
static unsigned int input_queue_worst;

void anarch_profile_reset(void)
{
    rb->memset(render_histogram, 0, sizeof(render_histogram));
    render_samples = 0;
    render_total = 0;
    render_worst = 0;
    late_frames = 0;
    lcd_samples = 0;
    lcd_total = 0;
    lcd_worst = 0;
    input_queue_worst = 0;
}

void anarch_profile_render(unsigned int percent)
{
    unsigned int bucket = MIN(percent, PROFILE_BUCKETS - 1);

    render_histogram[bucket]++;
    render_samples++;
    render_total += percent;
    render_worst = MAX(render_worst, percent);
    if (percent > 100)
        late_frames++;
}

void anarch_profile_lcd(unsigned int ticks)
{
    lcd_samples++;
    lcd_total += ticks;
    lcd_worst = MAX(lcd_worst, ticks);
}

void anarch_profile_input_queue(unsigned int depth)
{
    input_queue_worst = MAX(input_queue_worst, depth);
}

uint32_t anarch_profile_render_samples(void)
{
    return render_samples;
}

uint32_t anarch_profile_late_frames(void)
{
    return late_frames;
}

unsigned int anarch_profile_input_queue_worst(void)
{
    return input_queue_worst;
}

void anarch_profile_write(uint32_t frames, unsigned long underruns,
                          bool guards_ok)
{
    uint32_t threshold = (render_samples * 95 + 99) / 100;
    uint32_t cumulative = 0;
    unsigned int p95 = 0;
    unsigned int i;
    int fd;

    for (i = 0; i < PROFILE_BUCKETS; ++i)
    {
        cumulative += render_histogram[i];
        if (cumulative >= threshold)
        {
            p95 = i;
            break;
        }
    }
    fd = rb->open(PROFILE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd,
        "ANARCH_PROFILE_V1\nprofile=balanced\nresolution=160x120@2x\n"
        "target_fps=30\nframes=%lu\nrender_samples=%lu\n"
        "render_average_percent=%lu\nrender_p95_percent=%u\n"
        "render_worst_percent=%u\nlate_frames=%lu\nlcd_samples=%lu\n"
        "lcd_average_ticks=%lu\nlcd_worst_ticks=%u\n"
        "input_queue_worst=%u\naudio_underruns=%lu\n"
        "framebuffer_guards=%d\narena_used=%lu\narena_free=%lu\n",
        (unsigned long)frames, (unsigned long)render_samples,
        (unsigned long)(render_samples ? render_total / render_samples : 0),
        p95, render_worst, (unsigned long)late_frames,
        (unsigned long)lcd_samples,
        (unsigned long)(lcd_samples ? lcd_total / lcd_samples : 0),
        lcd_worst, input_queue_worst, underruns, guards_ok ? 1 : 0,
        (unsigned long)anarch_video_arena_used(),
        (unsigned long)anarch_video_arena_free());
    rb->close(fd);
}
