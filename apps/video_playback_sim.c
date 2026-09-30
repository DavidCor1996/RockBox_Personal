/***************************************************************************
 * iPod 6G simulator audio path for Apple-contract H.264 movies.
 *
 * The simulator has no S5L8702 VPU, but it can and should exercise the exact
 * AAC service used by hardware: MP4 parsing, the complete Apple chunk map,
 * the 8 KiB file cache, the codec thread, and the playback mixer channel.
 ****************************************************************************/
#include "config.h"

#if (defined(IPOD_6G) || defined(IPOD_VIDEO)) && defined(SIMULATOR)

#include "button.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "mp4_demux.h"
#include "rbpaths.h"
#include "video_audio.h"
#include "video_pcm.h"
#include "video_playback.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_SIM_LOG ROCKBOX_DIR "/openh264/sim_audio.log"

static const char *video_sim_path(const char *parameter)
{
    static const char * const prefixes[] = {
        "youtube-app:", "youtube:", "netflix-restart:", "netflix:",
        "instagram-app:", "instagram-feed:", "tiktok-app:",
        "reddit-app:", "onlyfans-app:", "spotify-wrapped:",
        "twitch-app:", "-mapsdash:",
    };
    size_t i;

    if (!strncmp(parameter, "youtube-live:", 13))
    {
        const char *separator = strchr(parameter + 13, ':');

        return separator != NULL ? separator + 1 : parameter;
    }
    if (!strncmp(parameter, "twitch-live:", 12))
    {
        const char *separator = strchr(parameter + 12, ':');

        separator = separator != NULL ? strchr(separator + 1, ':') : NULL;
        return separator != NULL ? separator + 1 : parameter;
    }
    for (i = 0; i < ARRAYLEN(prefixes); i++)
    {
        size_t length = strlen(prefixes[i]);

        if (!strncmp(parameter, prefixes[i], length))
            return parameter + length;
    }
    return parameter;
}

static uint32_t video_sim_audio_duration_ms(
    const struct mp4v_demux_res *demux)
{
    uint64_t ticks = 0;
    uint32_t i;

    if (demux->audio_timescale == 0)
        return 0;
    for (i = 0; i < demux->audio_num_stts; i++)
        ticks += (uint64_t)demux->audio_stts[i].sample_count *
                 demux->audio_stts[i].sample_delta;
    return (uint32_t)(ticks * 1000u / demux->audio_timescale);
}

static uint32_t video_sim_test_limit_ms(void)
{
    const char *value = getenv("ROCKPOD_SIM_H264_AUDIO_TEST_MS");
    unsigned long limit;

    if (value == NULL || value[0] == '\0')
        return 0;
    limit = strtoul(value, NULL, 10);
    return limit > UINT32_MAX ? UINT32_MAX : (uint32_t)limit;
}

static uint32_t video_sim_test_start_ms(void)
{
    const char *value = getenv("ROCKPOD_SIM_H264_AUDIO_START_MS");
    unsigned long start;

    if (value == NULL || value[0] == '\0')
        return 0;
    start = strtoul(value, NULL, 10);
    return start > UINT32_MAX ? UINT32_MAX : (uint32_t)start;
}

static void video_sim_log(const char *path, uint32_t elapsed,
                          uint32_t duration, uint32_t target,
                          bool failed, int result)
{
    int fd;

    mkdir(ROCKBOX_DIR "/openh264", 0777);
    fd = open(VIDEO_SIM_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;
    fdprintf(fd, "path=\"%s\" elapsed_ms=%lu duration_ms=%lu "
             "target_ms=%lu decoder_failed=%d result=%d\n",
             path, (unsigned long)elapsed, (unsigned long)duration,
             (unsigned long)target, failed ? 1 : 0, result);
    close(fd);
}

int video_h264_play(const char *parameter, void *buffer, size_t buffer_size)
{
    static struct mp4v_demux_res demux;
    static uint32_t probe_video_sample[1];
    static uint32_t probe_video_chunk[1];
    static uint32_t probe_audio_sample[1];
    static uint32_t probe_audio_chunk[1];
    const char *path;
    uintptr_t workspace_start;
    size_t workspace_size;
    uint32_t duration_ms;
    uint32_t elapsed_ms = 0;
    uint32_t test_start_ms;
    uint32_t test_limit_ms;
    uint32_t test_target_ms;
    long deadline = 0;
    bool failed = false;
    int result = -1;

    if (parameter == NULL || buffer == NULL || buffer_size == 0)
        return -1;
    path = video_sim_path(parameter);
    memset(&demux, 0, sizeof(demux));
    if (mp4v_demux_open(path, &demux,
                        probe_video_sample, ARRAYLEN(probe_video_sample),
                        probe_video_chunk, ARRAYLEN(probe_video_chunk),
                        NULL, 0,
                        probe_audio_sample, ARRAYLEN(probe_audio_sample),
                        probe_audio_chunk, ARRAYLEN(probe_audio_chunk),
                        NULL, 0) < 0 ||
        demux.audio_format != MAKEFOURCC('m', 'p', '4', 'a'))
        return -1;

    workspace_size = video_audio_workspace_size(&demux);
    workspace_start = ((uintptr_t)buffer + CACHEALIGN_SIZE - 1) &
                      ~(uintptr_t)(CACHEALIGN_SIZE - 1);
    if (workspace_size == 0 || workspace_start < (uintptr_t)buffer ||
        workspace_start > (uintptr_t)buffer + buffer_size ||
        workspace_size > (uintptr_t)buffer + buffer_size - workspace_start)
        return -1;
    if (video_audio_init(path, &demux, (void *)workspace_start,
                         workspace_size) < 0)
        return -1;

    duration_ms = video_sim_audio_duration_ms(&demux);
    test_start_ms = video_sim_test_start_ms();
    test_limit_ms = video_sim_test_limit_ms();
    if (test_start_ms > duration_ms)
        test_start_ms = duration_ms;
    test_target_ms = test_limit_ms > UINT32_MAX - test_start_ms ?
        UINT32_MAX : test_start_ms + test_limit_ms;
    if (test_limit_ms > 0)
        /* The SDL simulator deliberately runs slower than the device when it
         * is decoding AAC and repainting the status screen.  Keep this a
         * deadlock watchdog, not a real-time playback deadline. */
        deadline = current_tick +
            (long)(((uint64_t)test_limit_ms * 2u + 30000u) * HZ / 1000u);
    video_audio_play();
    if (test_start_ms > 0)
        video_audio_seek(test_start_ms);

    while (video_audio_is_active() || !video_pcm_empty())
    {
        int button = button_get_w_tmo(HZ / 20);

        elapsed_ms = video_pcm_get_clock_ms();
        failed = video_audio_failed();
        lcd_clear_display();
        lcd_puts(0, 0, "H.264 audio simulator");
        lcd_putsf(0, 2, "%lu / %lu ms",
                  (unsigned long)elapsed_ms, (unsigned long)duration_ms);
        lcd_putsf(0, 4, "buffered: %lu",
                  (unsigned long)video_pcm_buffered_samples());
        lcd_puts(0, 6, failed ? "AAC decoder: FAILED" :
                              "AAC decoder: running");
        lcd_puts(0, 8, "MENU exits");
        lcd_update();

        if (failed)
            break;
        if (test_limit_ms > 0 && elapsed_ms >= test_target_ms)
        {
            result = 0;
            break;
        }
        if (test_limit_ms > 0 && TIME_AFTER(current_tick, deadline))
            break;
        if (button == BUTTON_MENU || button == (BUTTON_MENU | BUTTON_REL))
        {
            result = 1;
            break;
        }
    }
    if (!failed && result < 0 && test_limit_ms == 0 &&
        !video_audio_is_active() && video_pcm_empty())
        result = 0;
    video_audio_stop();
    video_sim_log(path, elapsed_ms, duration_ms, test_target_ms,
                  failed, result);
    return result;
}

#endif /* (IPOD_6G || IPOD_VIDEO) && SIMULATOR */
