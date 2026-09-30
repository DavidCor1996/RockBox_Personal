/***************************************************************************
 * Offline Messenger delivery. Called only from the normal UI service context.
 * The calendar is sorted and bounded by the companion; never scan from draw.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "misc.h"
#include "kernel.h"
#include "rbpaths.h"
#include "timefuncs.h"
#include "notification_manager.h"
#include "msn_notifications.h"
#include "msn_clock.h"
#include "plugin.h"
#include "pcm_mixer.h"

#define MSN_ROOT ROCKBOX_DIR "/msn"
static long next_check;
static char parse_line[1024];
static bool servicing;
static char cached_generation[32];
static long next_delivery;
#ifndef HAVE_HARDWARE_BEEP
/* Original notification clip at 11025 Hz, signed 16-bit mono. 38 KiB cap.
 * Callback only resamples resident PCM; it never reads files or waits. */
static unsigned char notice[38000];
static int16_t output[1024] CACHEALIGN_ATTR;
static unsigned int notice_samples, position, phase, rate;
static bool sound_active;

static void notice_more(const void **start, size_t *size)
{
    unsigned frames = 0;
    while (frames < 512 && position < notice_samples)
    {
        int16_t sample = (int16_t)(notice[2*position] |
                                  notice[2*position+1] << 8);
        output[frames*2] = sample;
        output[frames*2+1] = sample;
        frames++;
        phase += 11025;
        position += phase/rate;
        phase %= rate;
    }
    *start = output;
    *size = frames*4;
}
#endif

void msn_notification_sound_stop(void)
{
#ifndef HAVE_HARDWARE_BEEP
    if (sound_active)
    {
        mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
        mixer_channel_set_buffer_hook(PCM_MIXER_CHAN_BEEP, NULL);
        mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, MIX_AMP_UNITY);
        sound_active = false;
    }
#endif
}

void msn_notification_sound(void)
{
#ifndef HAVE_HARDWARE_BEEP
    if (plugin_is_loaded() && strstr(plugin_get_current_filename(),"/msn.rock")) return;
    msn_notification_sound_stop();
    rate = mixer_get_frequency();
    if (!rate) return;
    int fd = open(ROCKBOX_DIR "/ipodjs/msn/notice-11025.pcm",O_RDONLY);
    if (fd < 0) return;
    off_t length = filesize(fd);
    if (length <= 0 || length > (off_t)sizeof(notice) || length%2)
    {
        close(fd);
        return;
    }
    int got = read(fd,notice,length);
    close(fd);
    if (got != length) return;
    notice_samples = got/2; position = phase = 0;
    sound_active = true;
    mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP,MIX_AMP_UNITY/2);
    mixer_channel_play_data(PCM_MIXER_CHAN_BEEP,notice_more,NULL,0);
#endif
}

static int split(char *s, char **f, int count)
{
    int n = 0;
    f[n++] = s;
    while (*s && n < count)
    {
        if (*s == '\t') { *s = 0; f[n++] = s+1; }
        s++;
    }
    return n;
}

void msn_notifications_service(void)
{
    char generation[32], calendar_path[MAX_PATH], contact_id[33];
    long watermark = 0, latest = 0;
    struct notification_request request;
    int due = 0;
    if (servicing || TIME_BEFORE(current_tick,next_check)) return;
    next_check = current_tick + HZ*15;
    struct tm *tm = get_time();
    if (!tm || tm->tm_year < 120) return;
    long now = msn_local_timestamp(tm);
    if (!now) return;
    int fd = open(MSN_ROOT "/current.txt",O_RDONLY);
    if (fd < 0) return;
    int got = read_line(fd,generation,sizeof(generation));
    close(fd);
    if (got <= 0 || strlen(generation) != 16) return;
    for (int i = 0; i < 16; i++)
        if (!((generation[i] >= '0' && generation[i] <= '9') ||
              (generation[i] >= 'a' && generation[i] <= 'f'))) return;
    if (!strcmp(cached_generation,generation) && now < next_delivery) return;
    strlcpy(cached_generation,generation,sizeof(cached_generation));
    next_delivery = 0x7fffffff;
    fd = open(MSN_ROOT "/delivered.txt",O_RDONLY);
    if (fd >= 0)
    {
        if (read_line(fd,parse_line,sizeof(parse_line)) > 0) watermark = strtol(parse_line,NULL,10);
        close(fd);
    }
    if (watermark > now)
    {
        next_delivery = watermark;
        return; /* Clock moved back; never replay alerts. */
    }
    snprintf(calendar_path,sizeof(calendar_path),MSN_ROOT "/bundles/%s/events.tsv",generation);
    fd = open(calendar_path,O_RDONLY);
    if (fd < 0) { next_delivery = 0; return; }
    memset(&request,0,sizeof(request));
    read_line(fd,parse_line,sizeof(parse_line));
    for (int i = 0; i < 1024 && read_line(fd,parse_line,sizeof(parse_line)) > 0; i++)
    {
        char *f[7];
        if (split(parse_line,f,7) != 7) continue;
        long at = strtol(f[1],NULL,10);
        if (at > now) { next_delivery = at; break; }
        if (at <= watermark || !strcmp(f[3],"status")) continue;
        latest = at;
        due++;
        strlcpy(contact_id,f[2],sizeof(contact_id));
        strlcpy(request.body,f[5][0] ? f[5] : f[3],sizeof(request.body));
        char short_id[9];
        strlcpy(short_id,f[0],sizeof(short_id));
        request.stable_id = strtoul(short_id,NULL,16);
    }
    close(fd);
    if (!due) return;
    /* One recent message, or a catch-up summary after sleep. No alert burst. */
    if (due > 1 || now-latest > 120)
        snprintf(request.body,sizeof(request.body),"%d messages waiting in Messenger",due);
    strlcpy(request.title,"MSN Messenger",sizeof(request.title));
    snprintf(calendar_path,sizeof(calendar_path),MSN_ROOT "/bundles/%s/contacts.tsv",generation);
    fd = open(calendar_path,O_RDONLY);
    if (fd >= 0)
    {
        for (int i = 0; i < 65 && read_line(fd,parse_line,sizeof(parse_line)) > 0; i++)
        {
            char *f[5];
            if (split(parse_line,f,5) == 5 && !strcmp(f[0],contact_id))
            { strlcpy(request.title,f[1],sizeof(request.title)); break; }
        }
        close(fd);
    }
    request.source = NOTIFICATION_SOURCE_MSN;
    request.kind = NOTIFICATION_MSN_MESSAGE;
    request.timestamp = mktime(tm) - (now-latest);
    strlcpy(request.route,contact_id,sizeof(request.route));
    /* Flush progress before publishing; old bundles remain readable on failure. */
    fd = open(MSN_ROOT "/delivered.tmp",O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd < 0) { next_delivery = 0; return; }
    int length = snprintf(parse_line,sizeof(parse_line),"%ld\n",now);
    bool saved = write(fd,parse_line,length) == length;
    close(fd);
    if (!saved || rename(MSN_ROOT "/delivered.tmp",MSN_ROOT "/delivered.txt") < 0)
    {
        next_delivery = 0;
        return;
    }
    if (plugin_is_loaded() && strstr(plugin_get_current_filename(),"/msn.rock")) return;
    servicing = true;
    notification_post(&request);
    servicing = false;
}
