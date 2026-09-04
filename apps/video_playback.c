/***************************************************************************
 * S5L8702 hardware H.264 player.
 *
 * This is deliberately a narrow production contract: non-fragmented MP4/M4V,
 * the measured iTunes 9.2.1 Constrained Baseline syntax (Level 1.3 or 3.0,
 * one or two advertised reference pictures and exactly two ordered slices),
 * <= 640x480, <= 30 fps, with optional AAC-LC audio. RockPod's validated
 * Apple-exact profile is the normal source of files.
 ****************************************************************************/
#include "config.h"

#if defined(IPOD_6G) && !defined(SIMULATOR)

#include "backlight.h"
#include "button.h"
#include "crc32.h"
#include "dir.h"
#include "file.h"
#include "font.h"
#include "kernel.h"
#include "lcd.h"
#include "misc.h"
#include "mp4_demux.h"
#include "pcmbuf.h"
#include "rbpaths.h"
#include "settings.h"
#include "sound.h"
#include "splash.h"
#include "string-extra.h"
#include "system.h"
#include "timefuncs.h"
#include "video_audio.h"
#include "video_pcm.h"
#include "video_playback.h"
#include "vpu_h264.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_MAX_WIDTH          640
#define VIDEO_MAX_HEIGHT         480
#define VIDEO_MAX_LEVEL          30
#define VIDEO_MAX_SAMPLES        400000u
#define VIDEO_MAX_AUDIO_SAMPLES  600000u
#define VIDEO_READ_BUFFER        (512u * 1024u)
#define VIDEO_OUTPUT_BUFFER      (LCD_WIDTH * LCD_HEIGHT * 3u / 2u)
#define VIDEO_RESUME_MAGIC       0x52565031u
#define VIDEO_RESUME_FILE        PLUGIN_APPS_DATA_DIR \
                                 "/openh264-resume-%08lx.dat"
#define VIDEO_NETFLIX_INDEX      ROCKBOX_DIR "/videolist/index.tsv"
#define VIDEO_NETFLIX_FIELDS     27
#define VIDEO_INSTAGRAM_LIKES    ROCKBOX_DIR "/instagram/likes.tsv"
#define VIDEO_INSTAGRAM_LIKES_TMP ROCKBOX_DIR "/instagram/likes.h264.tmp"
#define VIDEO_TIKTOK_LIKES       ROCKBOX_DIR "/tiktok/likes.h264.tsv"
#define VIDEO_TIKTOK_LIKES_TMP   ROCKBOX_DIR "/tiktok/likes.h264.tmp"
#define VIDEO_INSTAGRAM_CARD_X   4
#define VIDEO_INSTAGRAM_CARD_Y   57
#define VIDEO_INSTAGRAM_CARD_W   160
#define VIDEO_INSTAGRAM_CARD_H   158

enum video_style
{
    VIDEO_STYLE_STOCK = 0,
    VIDEO_STYLE_YOUTUBE,
    VIDEO_STYLE_YOUTUBE_LIVE,
    VIDEO_STYLE_NETFLIX,
    VIDEO_STYLE_INSTAGRAM,
    VIDEO_STYLE_INSTAGRAM_FEED,
    VIDEO_STYLE_TIKTOK,
    VIDEO_STYLE_MAPS,
};

enum video_input_action
{
    VIDEO_INPUT_NONE = 0,
    VIDEO_INPUT_EXIT,
    VIDEO_INPUT_PREVIOUS,
    VIDEO_INPUT_NEXT,
    VIDEO_INPUT_SEEK_BACK,
    VIDEO_INPUT_SEEK_FORWARD,
    VIDEO_INPUT_ACTIVATE,
    VIDEO_INPUT_COMPLETE,
    VIDEO_INPUT_PROFILE,
};

struct video_launch
{
    const char *path;
    enum video_style style;
    uint32_t live_epoch;
    bool live;
    bool restart;
    bool allow_resume;
    bool fill;
    bool instagram_liked;
    bool instagram_feed_expanded;
    char instagram_group_id[33];
    char instagram_username[65];
    char instagram_caption[181];
    int instagram_likes;
    bool tiktok_liked;
    char tiktok_id[97];
    char tiktok_title[81];
    char tiktok_creator[49];
    char tiktok_description[181];
    int tiktok_likes;
    int tiktok_comments;
    uint32_t intro_start_ms;
    uint32_t intro_end_ms;
    uint32_t credits_start_ms;
};

struct video_resume_record
{
    uint32_t magic;
    uint32_t path_crc;
    int32_t frame;
    int32_t total_frames;
};

struct video_pool
{
    uint8_t *cursor;
    uint8_t *end;
};

struct video_timing
{
    uint32_t run;
    uint32_t in_run;
    uint64_t ticks;
};

static bool video_prefix(const char *parameter, const char *prefix,
                         const char **path)
{
    size_t length = strlen(prefix);

    if (strncmp(parameter, prefix, length))
        return false;
    *path = parameter + length;
    return true;
}

static bool video_parse_launch(const char *parameter,
                               struct video_launch *launch)
{
    const char *path = parameter;

    memset(launch, 0, sizeof(*launch));
    launch->path = parameter;
    launch->style = VIDEO_STYLE_STOCK;
    launch->allow_resume = true;
    if (video_prefix(parameter, "youtube-live:", &path))
    {
        const char *separator = strchr(path, ':');

        if (separator == NULL || separator[1] == '\0')
            return false;
        launch->style = VIDEO_STYLE_YOUTUBE_LIVE;
        launch->live_epoch = strtoul(path, NULL, 10);
        launch->live = true;
        launch->allow_resume = false;
        launch->path = separator + 1;
    }
    else if (video_prefix(parameter, "youtube-app:", &path) ||
             video_prefix(parameter, "youtube:", &path))
    {
        launch->style = VIDEO_STYLE_YOUTUBE;
        launch->path = path;
    }
    else if (video_prefix(parameter, "netflix-restart:", &path))
    {
        launch->style = VIDEO_STYLE_NETFLIX;
        launch->restart = true;
        launch->path = path;
    }
    else if (video_prefix(parameter, "netflix:", &path))
    {
        launch->style = VIDEO_STYLE_NETFLIX;
        launch->path = path;
    }
    else if (video_prefix(parameter, "instagram-feed:", &path))
    {
        launch->style = VIDEO_STYLE_INSTAGRAM_FEED;
        launch->allow_resume = false;
        launch->path = path;
    }
    else if (video_prefix(parameter, "instagram-app:", &path))
    {
        launch->style = VIDEO_STYLE_INSTAGRAM;
        launch->path = path;
    }
    else if (video_prefix(parameter, "tiktok-app:", &path))
    {
        launch->style = VIDEO_STYLE_TIKTOK;
        launch->allow_resume = false;
        launch->path = path;
    }
    else if (video_prefix(parameter, "-mapsdash:", &path))
    {
        launch->style = VIDEO_STYLE_MAPS;
        launch->allow_resume = false;
        launch->fill = true;
        launch->path = path;
    }
    else if (video_prefix(parameter, "reddit-app:", &path) ||
             video_prefix(parameter, "onlyfans-app:", &path) ||
             video_prefix(parameter, "spotify-wrapped:", &path))
        launch->path = path;

    return launch->path != NULL && launch->path[0] != '\0';
}

static void video_metadata_value(char *target, size_t size,
                                 const char *value)
{
    char *end;

    strlcpy(target, value, size);
    end = target;
    while (*end != '\0' && *end != '\r' && *end != '\n')
        end++;
    *end = '\0';
}

static void video_instagram_load(struct video_launch *launch)
{
    char metadata_path[MAX_PATH];
    char line[256];
    char *dot;
    int fd;

    if (launch->style != VIDEO_STYLE_INSTAGRAM &&
        launch->style != VIDEO_STYLE_INSTAGRAM_FEED)
        return;
    strlcpy(metadata_path, launch->path, sizeof(metadata_path));
    dot = strrchr(metadata_path, '.');
    if (dot != NULL)
        strlcpy(dot, ".igm", sizeof(metadata_path) - (dot - metadata_path));
    fd = dot != NULL ? open(metadata_path, O_RDONLY) : -1;
    if (fd >= 0)
    {
        while (read_line(fd, line, sizeof(line)) > 0)
        {
            if (!strncmp(line, "group_id=", 9))
                video_metadata_value(launch->instagram_group_id,
                                     sizeof(launch->instagram_group_id),
                                     line + 9);
            else if (!strncmp(line, "username=", 9))
                video_metadata_value(launch->instagram_username,
                                     sizeof(launch->instagram_username),
                                     line + 9);
            else if (!strncmp(line, "title=", 6))
                video_metadata_value(launch->instagram_caption,
                                     sizeof(launch->instagram_caption),
                                     line + 6);
            else if (!strncmp(line, "likes=", 6))
                launch->instagram_likes = atoi(line + 6);
        }
        close(fd);
    }
    if (launch->instagram_group_id[0] == '\0')
        return;
    fd = open(VIDEO_INSTAGRAM_LIKES, O_RDONLY);
    if (fd < 0)
        return;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        video_metadata_value(metadata_path, sizeof(metadata_path), line);
        if (!strcmp(metadata_path, launch->instagram_group_id))
        {
            launch->instagram_liked = true;
            break;
        }
    }
    close(fd);
}

static bool video_instagram_toggle_like(struct video_launch *launch)
{
    char line[96];
    char clean[96];
    int source;
    int target;
    bool found = false;

    if (launch->instagram_group_id[0] == '\0')
        return false;
    source = open(VIDEO_INSTAGRAM_LIKES, O_RDONLY);
    target = open(VIDEO_INSTAGRAM_LIKES_TMP,
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (target < 0)
    {
        if (source >= 0)
            close(source);
        return false;
    }
    if (source >= 0)
    {
        while (read_line(source, line, sizeof(line)) > 0)
        {
            video_metadata_value(clean, sizeof(clean), line);
            if (clean[0] == '\0')
                continue;
            if (!strcmp(clean, launch->instagram_group_id))
            {
                found = true;
                if (launch->instagram_liked)
                    continue;
            }
            fdprintf(target, "%s\n", clean);
        }
        close(source);
    }
    if (!launch->instagram_liked && !found)
        fdprintf(target, "%s\n", launch->instagram_group_id);
    close(target);
    remove(VIDEO_INSTAGRAM_LIKES);
    if (rename(VIDEO_INSTAGRAM_LIKES_TMP, VIDEO_INSTAGRAM_LIKES) < 0)
    {
        remove(VIDEO_INSTAGRAM_LIKES_TMP);
        return false;
    }
    launch->instagram_liked = !launch->instagram_liked;
    return true;
}

static void video_tiktok_load(struct video_launch *launch)
{
    char metadata_path[MAX_PATH];
    char line[256];
    char *dot;
    int fd;

    if (launch->style != VIDEO_STYLE_TIKTOK)
        return;
    strlcpy(metadata_path, launch->path, sizeof(metadata_path));
    dot = strrchr(metadata_path, '.');
    if (dot != NULL)
        strlcpy(dot, ".ttm", sizeof(metadata_path) - (dot - metadata_path));
    fd = dot != NULL ? open(metadata_path, O_RDONLY) : -1;
    if (fd >= 0)
    {
        while (read_line(fd, line, sizeof(line)) > 0)
        {
            if (!strncmp(line, "id=", 3))
                video_metadata_value(launch->tiktok_id,
                                     sizeof(launch->tiktok_id), line + 3);
            else if (!strncmp(line, "title=", 6))
                video_metadata_value(launch->tiktok_title,
                                     sizeof(launch->tiktok_title), line + 6);
            else if (!strncmp(line, "creator=", 8))
                video_metadata_value(launch->tiktok_creator,
                                     sizeof(launch->tiktok_creator), line + 8);
            else if (!strncmp(line, "description=", 12))
                video_metadata_value(launch->tiktok_description,
                                     sizeof(launch->tiktok_description),
                                     line + 12);
            else if (!strncmp(line, "likes=", 6))
                launch->tiktok_likes = atoi(line + 6);
            else if (!strncmp(line, "comments=", 9))
                launch->tiktok_comments = atoi(line + 9);
        }
        close(fd);
    }
    if (launch->tiktok_id[0] == '\0')
        video_metadata_value(launch->tiktok_id,
                             sizeof(launch->tiktok_id), launch->path);
    fd = open(VIDEO_TIKTOK_LIKES, O_RDONLY);
    if (fd < 0)
        return;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        video_metadata_value(metadata_path, sizeof(metadata_path), line);
        if (!strcmp(metadata_path, launch->tiktok_id))
        {
            launch->tiktok_liked = true;
            break;
        }
    }
    close(fd);
}

static bool video_tiktok_toggle_like(struct video_launch *launch)
{
    char line[128];
    char clean[128];
    int source;
    int target;
    bool found = false;

    if (launch->tiktok_id[0] == '\0')
        return false;
    source = open(VIDEO_TIKTOK_LIKES, O_RDONLY);
    target = open(VIDEO_TIKTOK_LIKES_TMP,
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (target < 0)
    {
        if (source >= 0)
            close(source);
        return false;
    }
    if (source >= 0)
    {
        while (read_line(source, line, sizeof(line)) > 0)
        {
            video_metadata_value(clean, sizeof(clean), line);
            if (clean[0] == '\0')
                continue;
            if (!strcmp(clean, launch->tiktok_id))
            {
                found = true;
                if (launch->tiktok_liked)
                    continue;
            }
            fdprintf(target, "%s\n", clean);
        }
        close(source);
    }
    if (!launch->tiktok_liked && !found)
        fdprintf(target, "%s\n", launch->tiktok_id);
    close(target);
    remove(VIDEO_TIKTOK_LIKES);
    if (rename(VIDEO_TIKTOK_LIKES_TMP, VIDEO_TIKTOK_LIKES) < 0)
    {
        remove(VIDEO_TIKTOK_LIKES_TMP);
        return false;
    }
    launch->tiktok_liked = !launch->tiktok_liked;
    return true;
}

static bool video_split_marker_row(char *line,
                                   char *fields[VIDEO_NETFLIX_FIELDS])
{
    int i;

    for (i = 0; i < VIDEO_NETFLIX_FIELDS; i++)
    {
        char *tab;

        fields[i] = line;
        tab = strchr(line, '\t');
        if (tab == NULL)
            return i == VIDEO_NETFLIX_FIELDS - 1;
        *tab = '\0';
        line = tab + 1;
    }
    return true;
}

static void video_load_netflix_markers(struct video_launch *launch,
                                       uint32_t duration_ms)
{
    char line[1024];
    const char *device_path = launch->path[0] == '/' ?
                              launch->path + 1 : launch->path;
    int fd;

    if (launch->style != VIDEO_STYLE_NETFLIX)
        return;
    fd = open(VIDEO_NETFLIX_INDEX, O_RDONLY);
    if (fd < 0)
        return;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[VIDEO_NETFLIX_FIELDS];
        uint32_t credits_duration_ms;

        if (line[0] == '#' || !video_split_marker_row(line, fields) ||
            strcmp(fields[6], device_path))
            continue;
        launch->intro_start_ms =
            (uint32_t)MAX(0, atoi(fields[23])) * 1000u;
        launch->intro_end_ms =
            (uint32_t)MAX(0, atoi(fields[24])) * 1000u;
        launch->credits_start_ms =
            (uint32_t)MAX(0, atoi(fields[25])) * 1000u;
        credits_duration_ms =
            (uint32_t)MAX(0, atoi(fields[26])) * 1000u;
        if (launch->credits_start_ms == 0 &&
            credits_duration_ms > 0 && duration_ms > credits_duration_ms)
            launch->credits_start_ms = duration_ms - credits_duration_ms;
        if (launch->intro_end_ms <= launch->intro_start_ms)
        {
            launch->intro_start_ms = 0;
            launch->intro_end_ms = 0;
        }
        if (launch->credits_start_ms >= duration_ms)
            launch->credits_start_ms = 0;
        break;
    }
    close(fd);
}

static void *video_pool_take(struct video_pool *pool, size_t size,
                             size_t alignment)
{
    uintptr_t current;
    uintptr_t aligned;

    if (pool == NULL || alignment == 0)
        return NULL;
    current = (uintptr_t)pool->cursor;
    aligned = (current + alignment - 1) & ~(uintptr_t)(alignment - 1);
    if (aligned < current || aligned > (uintptr_t)pool->end ||
        size > (size_t)(pool->end - (uint8_t *)aligned))
        return NULL;
    pool->cursor = (uint8_t *)aligned + size;
    return (void *)aligned;
}

static bool video_tables_sane(const struct mp4v_demux_res *demux)
{
    bool video_sane =
        demux->num_samples > 0 &&
        demux->num_samples <= VIDEO_MAX_SAMPLES &&
        demux->num_stco > 0 &&
        demux->num_stco <= demux->num_samples &&
        demux->num_stsc > 0 &&
        demux->num_stsc <= demux->num_stco;
    bool audio_sane =
        (demux->audio_num_samples == 0 &&
         demux->audio_num_stco == 0 && demux->audio_num_stsc == 0) ||
        (demux->audio_num_samples > 0 &&
         demux->audio_num_samples <= VIDEO_MAX_AUDIO_SAMPLES &&
         demux->audio_num_stco > 0 &&
         demux->audio_num_stco <= demux->audio_num_samples &&
         demux->audio_num_stsc > 0 &&
         demux->audio_num_stsc <= demux->audio_num_stco);

    return video_sane && audio_sane;
}

/* Validate the actual video mapping after the second, full table pass.  The
 * old independent stco/stsc caps rejected long files authored by iTunes,
 * whose normal layout is one video sample per chunk and may therefore have
 * hundreds of thousands of chunks.  The MP4 relationships themselves give
 * tighter bounds: every chunk contains samples and every stsc run starts at
 * a real chunk. */
static bool video_stsc_sane(const struct mp4v_demux_res *demux)
{
    uint64_t mapped_samples = 0;
    uint32_t i;

    if (demux->stsc == NULL || demux->num_stsc == 0 ||
        demux->stsc_cap < demux->num_stsc)
        return false;
    for (i = 0; i < demux->num_stsc; i++)
    {
        const struct mp4v_stsc_entry *entry = &demux->stsc[i];
        uint32_t next_chunk = i + 1 < demux->num_stsc ?
            demux->stsc[i + 1].first_chunk : demux->num_stco + 1;

        if ((i == 0 && entry->first_chunk != 1) ||
            entry->first_chunk == 0 || entry->first_chunk > demux->num_stco ||
            next_chunk <= entry->first_chunk ||
            next_chunk > demux->num_stco + 1 ||
            entry->samples_per_chunk == 0 || entry->sample_desc_index == 0)
            return false;
        mapped_samples +=
            (uint64_t)(next_chunk - entry->first_chunk) *
            entry->samples_per_chunk;
        if (mapped_samples > demux->num_samples)
            return false;
    }
    return mapped_samples == demux->num_samples;
}

static uint32_t video_pts_ms(const struct mp4v_demux_res *demux,
                             const struct video_timing *timing)
{
    if (demux->timescale == 0)
        return 0;
    return (uint32_t)(timing->ticks * 1000u / demux->timescale);
}

static void video_timing_advance(const struct mp4v_demux_res *demux,
                                 struct video_timing *timing)
{
    const struct mp4v_stts_entry *entry;

    if (timing->run >= demux->num_stts)
        return;
    entry = &demux->stts[timing->run];
    timing->ticks += entry->sample_delta;
    timing->in_run++;
    if (timing->in_run >= entry->sample_count)
    {
        timing->run++;
        timing->in_run = 0;
    }
}

static uint32_t video_duration_ms(const struct mp4v_demux_res *demux)
{
    uint64_t ticks = 0;
    uint32_t i;

    if (demux->timescale == 0)
        return 0;
    for (i = 0; i < demux->num_stts; i++)
        ticks += (uint64_t)demux->stts[i].sample_count *
                 demux->stts[i].sample_delta;
    return (uint32_t)(ticks * 1000u / demux->timescale);
}

static void video_timing_for_sample(const struct mp4v_demux_res *demux,
                                    uint32_t sample,
                                    struct video_timing *timing)
{
    uint32_t remaining = sample;

    memset(timing, 0, sizeof(*timing));
    while (timing->run < demux->num_stts)
    {
        const struct mp4v_stts_entry *entry =
            &demux->stts[timing->run];

        if (remaining < entry->sample_count)
        {
            timing->in_run = remaining;
            timing->ticks += (uint64_t)remaining * entry->sample_delta;
            return;
        }
        timing->ticks += (uint64_t)entry->sample_count *
                         entry->sample_delta;
        remaining -= entry->sample_count;
        timing->run++;
    }
}

static uint32_t video_sample_for_ms(const struct mp4v_demux_res *demux,
                                    uint32_t target_ms,
                                    struct video_timing *timing)
{
    uint64_t target_ticks;
    uint64_t ticks = 0;
    uint32_t sample = 0;
    uint32_t run;

    if (demux->timescale == 0)
    {
        memset(timing, 0, sizeof(*timing));
        return 0;
    }
    target_ticks = (uint64_t)target_ms * demux->timescale / 1000u;
    for (run = 0; run < demux->num_stts; run++)
    {
        const struct mp4v_stts_entry *entry = &demux->stts[run];
        uint64_t run_ticks = (uint64_t)entry->sample_count *
                             entry->sample_delta;

        if (target_ticks < ticks + run_ticks)
        {
            uint32_t in_run = entry->sample_delta == 0 ? 0 :
                (uint32_t)((target_ticks - ticks) / entry->sample_delta);

            if (in_run >= entry->sample_count)
                in_run = entry->sample_count - 1;
            sample += in_run;
            break;
        }
        ticks += run_ticks;
        sample += entry->sample_count;
    }
    if (sample >= demux->num_samples)
        sample = demux->num_samples - 1;
    while (sample > 0 && !mp4v_is_keyframe(demux, sample))
        sample--;
    video_timing_for_sample(demux, sample, timing);
    return sample;
}

static void video_resume_filename(const char *path, char *filename,
                                  size_t size, uint32_t *crc_out)
{
    uint32_t crc = crc_32(path, strlen(path), 0xffffffff);

    snprintf(filename, size, VIDEO_RESUME_FILE, (unsigned long)crc);
    if (crc_out != NULL)
        *crc_out = crc;
}

static uint32_t video_resume_load(const char *path, uint32_t total_samples)
{
    struct video_resume_record record;
    char filename[MAX_PATH];
    uint32_t crc;
    int fd;

    video_resume_filename(path, filename, sizeof(filename), &crc);
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return 0;
    if (read(fd, &record, sizeof(record)) != sizeof(record))
    {
        close(fd);
        return 0;
    }
    close(fd);
    if (record.magic != VIDEO_RESUME_MAGIC || record.path_crc != crc ||
        record.total_frames != (int32_t)total_samples || record.frame <= 0 ||
        record.frame >= (int32_t)(total_samples * 95u / 100u))
        return 0;
    return (uint32_t)record.frame;
}

static void video_resume_save(const char *path, uint32_t sample,
                              uint32_t total_samples)
{
    struct video_resume_record record;
    char filename[MAX_PATH];
    uint32_t crc;
    int fd;

    video_resume_filename(path, filename, sizeof(filename), &crc);
    if (sample == 0 || sample >= total_samples * 95u / 100u)
    {
        remove(filename);
        return;
    }
    mkdir(PLUGIN_APPS_DATA_DIR);
    record.magic = VIDEO_RESUME_MAGIC;
    record.path_crc = crc;
    record.frame = (int32_t)sample;
    record.total_frames = (int32_t)total_samples;
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    (void)write(fd, &record, sizeof(record));
    close(fd);
}

static void video_resume_clear(const char *path)
{
    char filename[MAX_PATH];

    video_resume_filename(path, filename, sizeof(filename), NULL);
    remove(filename);
}

static void video_volume_change(int delta)
{
    int minimum = sound_min(SOUND_VOLUME);
    int maximum = sound_max(SOUND_VOLUME);
    int volume = global_status.volume + delta;

    if (global_settings.volume_limit >= minimum &&
        global_settings.volume_limit < maximum)
        maximum = global_settings.volume_limit;
    if (volume < minimum)
        volume = minimum;
    if (volume > maximum)
        volume = maximum;
    if (volume != global_status.volume)
    {
        global_status.volume = volume;
        sound_set_volume(volume);
    }
}

static enum video_input_action video_input(
    const struct video_launch *launch, bool *paused, long *start_tick,
    long *pause_started, bool have_audio, bool *cpu_boosted,
    long *overlay_until)
{
    int button = button_get_w_tmo(0);

    if (button == BUTTON_NONE)
        return VIDEO_INPUT_NONE;
    if (button == BUTTON_MENU || button == (BUTTON_MENU | BUTTON_REL))
        return VIDEO_INPUT_EXIT;
    if (button == BUTTON_SCROLL_FWD ||
        button == (BUTTON_SCROLL_FWD | BUTTON_REPEAT))
    {
        if (launch->style == VIDEO_STYLE_TIKTOK)
            return VIDEO_INPUT_NEXT;
        if (launch->style == VIDEO_STYLE_INSTAGRAM_FEED &&
            !launch->instagram_feed_expanded)
            return VIDEO_INPUT_NEXT;
        video_volume_change(1);
        *overlay_until = current_tick + HZ * 2;
        return VIDEO_INPUT_NONE;
    }
    if (button == BUTTON_SCROLL_BACK ||
        button == (BUTTON_SCROLL_BACK | BUTTON_REPEAT))
    {
        if (launch->style == VIDEO_STYLE_TIKTOK)
            return VIDEO_INPUT_PREVIOUS;
        if (launch->style == VIDEO_STYLE_INSTAGRAM_FEED &&
            !launch->instagram_feed_expanded)
            return VIDEO_INPUT_PREVIOUS;
        video_volume_change(-1);
        *overlay_until = current_tick + HZ * 2;
        return VIDEO_INPUT_NONE;
    }
    if (button == BUTTON_LEFT || button == (BUTTON_LEFT | BUTTON_REL))
    {
        if (launch->style == VIDEO_STYLE_TIKTOK)
            return VIDEO_INPUT_PREVIOUS;
        if (!launch->live)
            return VIDEO_INPUT_SEEK_BACK;
        *overlay_until = current_tick + HZ * 2;
        return VIDEO_INPUT_NONE;
    }
    if (button == BUTTON_RIGHT || button == (BUTTON_RIGHT | BUTTON_REL))
    {
        if (launch->style == VIDEO_STYLE_INSTAGRAM_FEED ||
            launch->style == VIDEO_STYLE_TIKTOK)
            return VIDEO_INPUT_PROFILE;
        if (!launch->live)
            return VIDEO_INPUT_SEEK_FORWARD;
        *overlay_until = current_tick + HZ * 2;
        return VIDEO_INPUT_NONE;
    }
    if (button == (BUTTON_SELECT | BUTTON_REL))
    {
        if (!launch->live)
            return VIDEO_INPUT_ACTIVATE;
        *overlay_until = current_tick + HZ * 2;
        return VIDEO_INPUT_NONE;
    }
    if (button == (BUTTON_PLAY | BUTTON_REL))
    {
        if (launch->live)
        {
            *overlay_until = current_tick + HZ * 2;
            return VIDEO_INPUT_NONE;
        }
        if (!*paused)
        {
            *paused = true;
            *pause_started = current_tick;
            if (have_audio)
            {
                video_audio_pause();
                video_pcm_pause(true);
            }
            if (*cpu_boosted)
            {
                cpu_boost(false);
                *cpu_boosted = false;
            }
        }
        else
        {
            if (!*cpu_boosted)
            {
                cpu_boost(true);
                *cpu_boosted = true;
            }
            *paused = false;
            *start_tick += current_tick - *pause_started;
            if (have_audio)
            {
                video_audio_resume();
                video_pcm_pause(false);
            }
        }
        *overlay_until = current_tick + HZ * 2;
    }
    return VIDEO_INPUT_NONE;
}

static void video_scale_plane(uint8_t *destination, int destination_stride,
                              int destination_width, int destination_height,
                              const uint8_t *source, int source_stride,
                              int source_width, int source_height)
{
    uint32_t source_y = 0;
    uint32_t step_y;
    uint32_t step_x;
    int row;

    if (destination_width <= 0 || destination_height <= 0 ||
        source_width <= 0 || source_height <= 0)
        return;
    step_x = ((uint32_t)source_width << 16) / destination_width;
    step_y = ((uint32_t)source_height << 16) / destination_height;
    for (row = 0; row < destination_height; row++)
    {
        const uint8_t *source_row = source +
            (source_y >> 16) * source_stride;
        uint8_t *destination_row = destination +
            row * destination_stride;
        uint32_t source_x = 0;
        int column;

        for (column = 0; column < destination_width; column++)
        {
            destination_row[column] = source_row[source_x >> 16];
            source_x += step_x;
        }
        source_y += step_y;
    }
}

static uint8_t video_clamp_yuv(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return (uint8_t)value;
}

static void video_yuv_color(int red, int green, int blue,
                            uint8_t *y, uint8_t *u, uint8_t *v)
{
    *y = video_clamp_yuv(
        ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16);
    *u = video_clamp_yuv(
        ((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128);
    *v = video_clamp_yuv(
        ((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128);
}

static void video_yuv_pixel(uint8_t * const *planes, int x, int y,
                            int red, int green, int blue)
{
    uint8_t py;
    uint8_t pu;
    uint8_t pv;

    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
        return;
    video_yuv_color(red, green, blue, &py, &pu, &pv);
    planes[0][y * LCD_WIDTH + x] = py;
    planes[1][(y / 2) * (LCD_WIDTH / 2) + x / 2] = pu;
    planes[2][(y / 2) * (LCD_WIDTH / 2) + x / 2] = pv;
}

static void video_yuv_rect(uint8_t * const *planes, int x, int y,
                           int width, int height,
                           int red, int green, int blue)
{
    int row;
    int column;

    for (row = MAX(0, y); row < MIN(LCD_HEIGHT, y + height); row++)
        for (column = MAX(0, x);
             column < MIN(LCD_WIDTH, x + width); column++)
            video_yuv_pixel(planes, column, row, red, green, blue);
}

static int video_yuv_text_width(const char *text)
{
    struct font *font = font_get(FONT_SYSFIXED);
    int width = 0;

    if (font == NULL)
        return 0;
    while (*text != '\0')
        width += font_get_width(font, (unsigned char)*text++);
    return width;
}

static void video_yuv_text(uint8_t * const *planes, int x, int y,
                           const char *text,
                           int red, int green, int blue)
{
    struct font *font = font_get(FONT_SYSFIXED);

    if (font == NULL || font->depth != 0)
        return;
    while (*text != '\0' && x < LCD_WIDTH)
    {
        unsigned char ch = *text++;
        int glyph_width = font_get_width(font, ch);
        const unsigned char *bits = font_get_bits(font, ch);
        int column;

        for (column = 0; column < glyph_width; column++)
        {
            const unsigned char *source = bits + column;
            int row;

            for (row = 0; row < (int)font->height; row++)
                if (source[(row >> 3) * glyph_width] &
                    (1u << (row & 7)))
                    video_yuv_pixel(planes, x + column, y + row,
                                    red, green, blue);
        }
        x += glyph_width;
    }
}

static void video_time_text(uint32_t milliseconds, char *text, size_t size)
{
    uint32_t seconds = milliseconds / 1000u;

    if (seconds >= 3600u)
        snprintf(text, size, "%lu:%02lu:%02lu",
                 (unsigned long)(seconds / 3600u),
                 (unsigned long)((seconds / 60u) % 60u),
                 (unsigned long)(seconds % 60u));
    else
        snprintf(text, size, "%lu:%02lu",
                 (unsigned long)(seconds / 60u),
                 (unsigned long)(seconds % 60u));
}

static void video_draw_overlay(uint8_t * const *planes,
                               const struct video_launch *launch,
                               bool paused, uint32_t position_ms,
                               uint32_t duration_ms, bool visible)
{
    char current[20];
    char duration[20];
    int bar_width = LCD_WIDTH - 16;
    int fill_width = duration_ms == 0 ? 0 :
        (int)((uint64_t)MIN(position_ms, duration_ms) * bar_width /
              duration_ms);
    bool netflix_intro = launch->style == VIDEO_STYLE_NETFLIX &&
        launch->intro_end_ms > launch->intro_start_ms &&
        position_ms >= launch->intro_start_ms &&
        position_ms < launch->intro_end_ms;
    bool netflix_credits = launch->style == VIDEO_STYLE_NETFLIX &&
        launch->credits_start_ms > 0 &&
        position_ms >= launch->credits_start_ms;

    if (launch->style == VIDEO_STYLE_YOUTUBE_LIVE)
    {
        /* The persistent mark is the live-state signal used by YouTube's
         * player.  It deliberately has no draggable timeline beside it. */
        video_yuv_rect(planes, 6, LCD_HEIGHT - 22, 48, 17, 204, 0, 0);
        video_yuv_text(planes, 12, LCD_HEIGHT - 18, "LIVE",
                       255, 255, 255);
        return;
    }
    if (launch->style == VIDEO_STYLE_MAPS)
        return;
    if (launch->style == VIDEO_STYLE_TIKTOK)
    {
        char likes[24];
        char comments[24];

        snprintf(likes, sizeof(likes), "%d",
                 launch->tiktok_likes +
                 (launch->tiktok_liked ? 1 : 0));
        snprintf(comments, sizeof(comments), "%d",
                 launch->tiktok_comments);
        video_yuv_rect(planes, 0, 0, LCD_WIDTH, 35, 12, 12, 16);
        video_yuv_text(planes, 9, 8, "TikTok", 255, 255, 255);
        video_yuv_text(planes, 126, 8, "Following  |  For You",
                       255, 255, 255);
        video_yuv_rect(planes, 0, LCD_HEIGHT - 66, LCD_WIDTH, 66,
                       12, 12, 16);
        video_yuv_text(planes, 9, LCD_HEIGHT - 57,
                       launch->tiktok_creator[0] ?
                       launch->tiktok_creator : "@creator",
                       255, 255, 255);
        video_yuv_text(planes, 9, LCD_HEIGHT - 38,
                       launch->tiktok_description[0] ?
                       launch->tiktok_description :
                       launch->tiktok_title,
                       255, 255, 255);
        video_yuv_text(planes, LCD_WIDTH - 52, LCD_HEIGHT - 58,
                       launch->tiktok_liked ? "HEART" : "LIKE",
                       launch->tiktok_liked ? 254 : 255,
                       launch->tiktok_liked ? 44 : 255,
                       launch->tiktok_liked ? 85 : 255);
        video_yuv_text(planes, LCD_WIDTH - 49, LCD_HEIGHT - 38,
                       likes, 255, 255, 255);
        video_yuv_text(planes, LCD_WIDTH - 49, LCD_HEIGHT - 19,
                       comments, 255, 255, 255);
        return;
    }
    if (launch->style == VIDEO_STYLE_INSTAGRAM_FEED &&
        !launch->instagram_feed_expanded)
    {
        char likes[24];

        video_yuv_rect(planes, 0, 0, LCD_WIDTH,
                       VIDEO_INSTAGRAM_CARD_Y, 247, 245, 239);
        video_yuv_rect(planes, 0,
                       VIDEO_INSTAGRAM_CARD_Y + VIDEO_INSTAGRAM_CARD_H,
                       LCD_WIDTH,
                       LCD_HEIGHT - (VIDEO_INSTAGRAM_CARD_Y +
                                     VIDEO_INSTAGRAM_CARD_H),
                       247, 245, 239);
        video_yuv_rect(planes, 0, VIDEO_INSTAGRAM_CARD_Y,
                       VIDEO_INSTAGRAM_CARD_X, VIDEO_INSTAGRAM_CARD_H,
                       247, 245, 239);
        video_yuv_rect(planes,
                       VIDEO_INSTAGRAM_CARD_X + VIDEO_INSTAGRAM_CARD_W,
                       VIDEO_INSTAGRAM_CARD_Y,
                       LCD_WIDTH - (VIDEO_INSTAGRAM_CARD_X +
                                    VIDEO_INSTAGRAM_CARD_W),
                       VIDEO_INSTAGRAM_CARD_H, 247, 245, 239);
        video_yuv_rect(planes, 0, 0, LCD_WIDTH, 28, 43, 79, 107);
        video_yuv_rect(planes, 0, 28, LCD_WIDTH, 28, 241, 250, 254);
        video_yuv_text(planes, 33, 7, "Instagram", 255, 255, 255);
        video_yuv_text(planes, 267, 34, "HOME", 118, 118, 118);
        video_yuv_text(planes, 7, 37, "@", 63, 114, 150);
        video_yuv_text(planes, 28, 35,
                       launch->instagram_username[0] ?
                       launch->instagram_username : "Instagram",
                       63, 114, 150);
        video_yuv_text(planes, 174, 65,
                       launch->instagram_liked ? "LIKED" : "LIKE",
                       63, 114, 150);
        snprintf(likes, sizeof(likes), "%d likes",
                 launch->instagram_likes +
                 (launch->instagram_liked ? 1 : 0));
        video_yuv_text(planes, 210, 68, likes, 63, 114, 150);
        video_yuv_text(planes, 172, 96, "VIDEO POST", 118, 118, 118);
        video_yuv_text(planes, 172, 122,
                       launch->instagram_username[0] ?
                       launch->instagram_username : "Instagram",
                       63, 114, 150);
        video_yuv_text(planes, 172, 142,
                       launch->instagram_caption[0] ?
                       launch->instagram_caption : "Video",
                       0, 0, 0);
        video_yuv_text(planes, 172, 202, "Select full screen",
                       118, 118, 118);
        video_yuv_rect(planes, 0, 222, LCD_WIDTH, 18, 43, 79, 107);
        video_yuv_text(planes, 8, 226,
                       "Home       Favorites       Profile",
                       255, 255, 255);
        return;
    }
    if (launch->style == VIDEO_STYLE_INSTAGRAM ||
        launch->style == VIDEO_STYLE_INSTAGRAM_FEED)
    {
        video_yuv_rect(planes, 0, 0, LCD_WIDTH, 24, 43, 79, 107);
        video_yuv_text(planes, 8, 7, "Instagram", 255, 255, 255);
        if (launch->instagram_liked)
            video_yuv_text(planes, LCD_WIDTH - 42, 7, "LIKED",
                           255, 255, 255);
        video_yuv_rect(planes, 0, LCD_HEIGHT - 18, LCD_WIDTH, 18,
                       43, 79, 107);
        video_yuv_text(planes, 7, LCD_HEIGHT - 14,
                       launch->style == VIDEO_STYLE_INSTAGRAM_FEED ?
                       "Wheel  Browse       MENU  Back" :
                       "SELECT  Like        MENU  Back",
                       255, 255, 255);
        if (!visible && !paused)
            return;
    }
    else if (!visible && !paused && !netflix_intro && !netflix_credits)
        return;

    video_time_text(position_ms, current, sizeof(current));
    video_time_text(duration_ms, duration, sizeof(duration));
    if (launch->style == VIDEO_STYLE_NETFLIX)
    {
        int duration_width = video_yuv_text_width(duration);

        if (!visible && !paused)
        {
            const char *label = netflix_intro ?
                                "SKIP INTRO" : "SKIP CREDITS";
            int label_width = video_yuv_text_width(label);
            int button_width = label_width + 24;
            int button_x = LCD_WIDTH - button_width - 10;

            video_yuv_rect(planes, button_x, LCD_HEIGHT - 42,
                           button_width, 30, 20, 20, 20);
            video_yuv_rect(planes, button_x, LCD_HEIGHT - 42,
                           button_width, 2, 255, 255, 255);
            video_yuv_rect(planes, button_x, LCD_HEIGHT - 14,
                           button_width, 2, 255, 255, 255);
            video_yuv_text(planes, button_x + 12, LCD_HEIGHT - 33,
                           label, 255, 255, 255);
            return;
        }

        video_yuv_rect(planes, 0, LCD_HEIGHT - 58, LCD_WIDTH, 58,
                       20, 20, 20);
        video_yuv_rect(planes, 0, LCD_HEIGHT - 58, LCD_WIDTH, 3,
                       180, 19, 29);
        video_yuv_text(planes, 7, LCD_HEIGHT - 49, "NETFLIX",
                       180, 19, 29);
        video_yuv_text(planes, 8, LCD_HEIGHT - 30, current,
                       255, 255, 255);
        video_yuv_text(planes, LCD_WIDTH - 8 - duration_width,
                       LCD_HEIGHT - 30, duration, 255, 255, 255);
        video_yuv_text(planes, (LCD_WIDTH -
                       video_yuv_text_width(paused ? "PAUSED" : "PLAYING")) /
                       2, LCD_HEIGHT - 30,
                       paused ? "PAUSED" : "PLAYING", 180, 19, 29);
        video_yuv_rect(planes, 8, LCD_HEIGHT - 10, bar_width, 4,
                       72, 72, 72);
        video_yuv_rect(planes, 8, LCD_HEIGHT - 10, fill_width, 4,
                       180, 19, 29);
    }
    else if (launch->style == VIDEO_STYLE_YOUTUBE)
    {
        int duration_width = video_yuv_text_width(duration);

        video_yuv_rect(planes, 0, LCD_HEIGHT - 28, LCD_WIDTH, 28,
                       35, 35, 35);
        video_yuv_rect(planes, 56, LCD_HEIGHT - 8, LCD_WIDTH - 112, 3,
                       92, 92, 92);
        video_yuv_rect(planes, 56, LCD_HEIGHT - 8,
                       (LCD_WIDTH - 112) * fill_width / bar_width, 3,
                       204, 0, 0);
        video_yuv_text(planes, 6, LCD_HEIGHT - 21,
                       paused ? "PAUSE" : ">", 255, 255, 255);
        video_yuv_text(planes, 34, LCD_HEIGHT - 21, current,
                       255, 255, 255);
        video_yuv_text(planes, LCD_WIDTH - 6 - duration_width,
                       LCD_HEIGHT - 21, duration, 255, 255, 255);
    }
    else if (launch->style != VIDEO_STYLE_INSTAGRAM &&
             launch->style != VIDEO_STYLE_INSTAGRAM_FEED)
    {
        int duration_width = video_yuv_text_width(duration);

        video_yuv_rect(planes, 0, LCD_HEIGHT - 34, LCD_WIDTH, 34,
                       25, 21, 35);
        video_yuv_text(planes, 8, LCD_HEIGHT - 27,
                       paused ? "PAUSED" : "PLAYING", 255, 255, 255);
        video_yuv_text(planes, LCD_WIDTH - 8 - duration_width,
                       LCD_HEIGHT - 27, duration, 255, 255, 255);
        video_yuv_rect(planes, 8, LCD_HEIGHT - 10, bar_width, 4,
                       70, 65, 80);
        video_yuv_rect(planes, 8, LCD_HEIGHT - 10, fill_width, 4,
                       220, 220, 225);
    }
}

static void video_draw_frame(const uint8_t *y, const uint8_t *cb,
                             const uint8_t *cr, int width, int height,
                             int stride, uint8_t *scaled,
                             const struct video_launch *launch,
                             bool paused, uint32_t position_ms,
                             uint32_t duration_ms, bool overlay_visible)
{
    unsigned char *planes[3];
    int draw_width;
    int draw_height;
    int x;
    int y_pos;

    if (y == NULL || cb == NULL || cr == NULL || scaled == NULL ||
        width <= 0 || height <= 0 || stride < width)
        return;
    if (launch->fill)
    {
        draw_width = LCD_WIDTH;
        draw_height = LCD_HEIGHT;
    }
    else if (launch->style == VIDEO_STYLE_INSTAGRAM_FEED &&
             !launch->instagram_feed_expanded)
    {
        draw_width = VIDEO_INSTAGRAM_CARD_W;
        draw_height = (int)((int64_t)height * draw_width / width);
        if (draw_height > VIDEO_INSTAGRAM_CARD_H)
        {
            draw_height = VIDEO_INSTAGRAM_CARD_H;
            draw_width = (int)((int64_t)width * draw_height / height);
        }
    }
    else
    {
        draw_width = LCD_WIDTH;
        draw_height = (int)((int64_t)height * LCD_WIDTH / width);
        if (draw_height > LCD_HEIGHT)
        {
            draw_height = LCD_HEIGHT;
            draw_width = (int)((int64_t)width * LCD_HEIGHT / height);
        }
    }
    draw_width &= ~1;
    draw_height &= ~1;
    if (launch->style == VIDEO_STYLE_INSTAGRAM_FEED &&
        !launch->instagram_feed_expanded)
    {
        x = VIDEO_INSTAGRAM_CARD_X +
            (VIDEO_INSTAGRAM_CARD_W - draw_width) / 2;
        y_pos = VIDEO_INSTAGRAM_CARD_Y +
                (VIDEO_INSTAGRAM_CARD_H - draw_height) / 2;
    }
    else
    {
        x = (LCD_WIDTH - draw_width) / 2;
        y_pos = (LCD_HEIGHT - draw_height) / 2;
    }
    x &= ~1;
    y_pos &= ~1;
    planes[0] = scaled;
    planes[1] = scaled + LCD_WIDTH * LCD_HEIGHT;
    planes[2] = planes[1] + LCD_WIDTH * LCD_HEIGHT / 4;
    memset(planes[0], 16, LCD_WIDTH * LCD_HEIGHT);
    memset(planes[1], 128, LCD_WIDTH * LCD_HEIGHT / 4);
    memset(planes[2], 128, LCD_WIDTH * LCD_HEIGHT / 4);
    video_scale_plane(planes[0] + y_pos * LCD_WIDTH + x, LCD_WIDTH,
                      draw_width, draw_height, y, stride, width, height);
    video_scale_plane(planes[1] + (y_pos / 2) * (LCD_WIDTH / 2) + x / 2,
                      LCD_WIDTH / 2, draw_width / 2, draw_height / 2,
                      cb, stride / 2, width / 2, height / 2);
    video_scale_plane(planes[2] + (y_pos / 2) * (LCD_WIDTH / 2) + x / 2,
                      LCD_WIDTH / 2, draw_width / 2, draw_height / 2,
                      cr, stride / 2, width / 2, height / 2);
    video_draw_overlay(planes, launch, paused, position_ms, duration_ms,
                       overlay_visible);
    lcd_blit_yuv(planes, 0, 0, LCD_WIDTH, 0, 0,
                 LCD_WIDTH, LCD_HEIGHT);
}

int video_h264_play(const char *filepath, void *buffer, size_t buffer_size)
{
    static struct mp4v_demux_res demux;
    static uint32_t probe_video_sample[1];
    static uint32_t probe_video_chunk[1];
    static uint32_t probe_audio_sample[1];
    static uint32_t probe_audio_chunk[1];
    struct video_launch launch;
    struct video_pool pool;
    struct video_timing timing;
    struct vpu_h264 *decoder = NULL;
    struct mp4v_stsc_entry *video_stsc;
    uint32_t *video_samples;
    uint32_t *video_chunks;
    uint8_t *decoder_buffer;
    uint8_t *read_buffer;
    uint8_t *scale_buffer;
    void *audio_codec_workspace;
    size_t decoder_size;
    size_t audio_codec_workspace_size;
    int video_fd = -1;
    uint32_t duration_ms;
    uint32_t sample = 0;
    uint32_t last_sample = 0;
    long start_tick;
    long pause_started = 0;
    long overlay_until;
    bool paused = false;
    bool cpu_boosted = false;
    bool have_audio = false;
    bool audio_master = false;
    enum video_input_action action = VIDEO_INPUT_NONE;
    int result = -1;

    if (filepath == NULL || buffer == NULL || buffer_size == 0 ||
        !video_parse_launch(filepath, &launch))
        return -1;
    video_instagram_load(&launch);
    video_tiktok_load(&launch);
    memset(&demux, 0, sizeof(demux));
    if (mp4v_demux_open(launch.path, &demux,
                        probe_video_sample, 1,
                        probe_video_chunk, 1, NULL, 0,
                        probe_audio_sample, 1,
                        probe_audio_chunk, 1, NULL, 0) < 0)
    {
        splash(HZ * 2, "Invalid MP4/M4V file");
        return -1;
    }
    if (demux.format != MAKEFOURCC('a', 'v', 'c', '1') ||
        demux.avc_profile != 66 || demux.avc_level > VIDEO_MAX_LEVEL ||
        demux.width == 0 || demux.height == 0 ||
        demux.width > VIDEO_MAX_WIDTH || demux.height > VIDEO_MAX_HEIGHT ||
        demux.nalu_len_size < 1 || demux.nalu_len_size > 4)
    {
        splash(HZ * 3, "Need H.264 Baseline <= L3.0\nMax 640x480");
        return -1;
    }
    if (!video_tables_sane(&demux))
    {
        splash(HZ * 3, "Movie tables exceed safe limit");
        return -1;
    }

    pool.cursor = buffer;
    pool.end = (uint8_t *)buffer + buffer_size;
    video_samples = video_pool_take(
        &pool, demux.num_samples * sizeof(*video_samples), 32);
    video_chunks = video_pool_take(
        &pool, demux.num_stco * sizeof(*video_chunks), 32);
    video_stsc = video_pool_take(
        &pool, demux.num_stsc * sizeof(*video_stsc), 32);
    decoder_size = vpu_h264_buf_size(
        (demux.width + 15) & ~15, (demux.height + 15) & ~15);
    decoder_buffer = video_pool_take(&pool, decoder_size, 4096);
    read_buffer = video_pool_take(&pool, VIDEO_READ_BUFFER, 32);
    scale_buffer = video_pool_take(&pool, VIDEO_OUTPUT_BUFFER, 32);
    audio_codec_workspace_size = demux.audio_num_samples > 0 ?
        video_audio_workspace_size(&demux) : 0;
    audio_codec_workspace = audio_codec_workspace_size > 0 ?
        video_pool_take(&pool, audio_codec_workspace_size,
                        CACHEALIGN_SIZE) : NULL;
    if (video_samples == NULL || video_chunks == NULL ||
        video_stsc == NULL || decoder_buffer == NULL || read_buffer == NULL ||
        scale_buffer == NULL ||
        (demux.audio_num_samples > 0 && audio_codec_workspace == NULL))
    {
        splash(HZ * 2, "Not enough video memory");
        return -1;
    }
    if (mp4v_demux_open(launch.path, &demux,
                        video_samples, demux.num_samples,
                        video_chunks, demux.num_stco,
                        video_stsc, demux.num_stsc,
                        probe_audio_sample, 1,
                        probe_audio_chunk, 1,
                        NULL, 0) < 0 ||
        !video_tables_sane(&demux) || !video_stsc_sane(&demux))
    {
        splash(HZ * 2, "MP4 table parse failed");
        return -1;
    }
    duration_ms = video_duration_ms(&demux);
    video_load_netflix_markers(&launch, duration_ms);
    if (launch.live && duration_ms > 0)
    {
        time_t now = mktime(get_time());
        uint64_t elapsed = now > (time_t)launch.live_epoch ?
            (uint64_t)(now - launch.live_epoch) * 1000u : 0;

        sample = video_sample_for_ms(
            &demux, (uint32_t)(elapsed % duration_ms), &timing);
    }
    else if (launch.allow_resume && !launch.restart)
    {
        sample = video_resume_load(launch.path, demux.num_samples);
        while (sample > 0 && !mp4v_is_keyframe(&demux, sample))
            sample--;
        video_timing_for_sample(&demux, sample, &timing);
    }
    else
        video_timing_for_sample(&demux, 0, &timing);

    /* Composite output already owns a boost because its memory reader
     * underruns at 54 MHz HClk. Decode plus LCD presentation needs the same
     * 108 MHz bus clock when undocked, so playback owns a separate reference. */
    cpu_boost(true);
    cpu_boosted = true;
    decoder = vpu_h264_open(
        decoder_buffer, decoder_size,
        (demux.width + 15) & ~15, (demux.height + 15) & ~15);
    if (decoder == NULL ||
        vpu_h264_configure(decoder, demux.codecdata,
                           demux.codecdata_len) < 0)
    {
        splash(HZ * 2, "H.264 decoder init failed");
        goto cleanup;
    }
    video_fd = open(launch.path, O_RDONLY);
    if (video_fd < 0)
        goto cleanup;

    pcmbuf_fade(false, true);
    sound_settings_apply();
    start_tick = current_tick -
        (long)((uint64_t)video_pts_ms(&demux, &timing) * HZ / 1000u);
    overlay_until = current_tick + HZ * 4;
    if (demux.audio_format == MAKEFOURCC('m', 'p', '4', 'a') &&
        demux.audio_codecdata_len > 0 &&
        video_audio_init(launch.path, &demux, audio_codec_workspace,
                         audio_codec_workspace_size) == 0)
    {
        int wait = 0;
        uint32_t initial_ms = video_pts_ms(&demux, &timing);

        have_audio = true;
        audio_master = true;
        video_audio_play();
        if (initial_ms > 0)
            video_audio_seek(initial_ms);
        while (!video_audio_ready() && wait < HZ && action == VIDEO_INPUT_NONE)
        {
            action = video_input(&launch, &paused, &start_tick,
                                 &pause_started, true, &cpu_boosted,
                                 &overlay_until);
            sleep(1);
            wait++;
        }
        if (video_audio_failed())
        {
            video_audio_stop();
            have_audio = false;
            audio_master = false;
        }
    }

    lcd_set_foreground(LCD_BLACK);
    lcd_clear_display();
    lcd_update();
    backlight_on();
    backlight_set_timeout(0);
    while (sample < demux.num_samples && action == VIDEO_INPUT_NONE)
    {
        uint32_t offset;
        uint32_t size;
        uint32_t pts_ms = video_pts_ms(&demux, &timing);
        int64_t seek_target = -1;
        bool seek_requested = false;
        int decoded;

        while (action == VIDEO_INPUT_NONE)
        {
            uint32_t clock_ms;

            action = video_input(&launch, &paused, &start_tick,
                                 &pause_started, have_audio, &cpu_boosted,
                                 &overlay_until);
            if (action == VIDEO_INPUT_ACTIVATE)
            {
                uint32_t position_ms = audio_master ?
                    video_pcm_get_clock_ms() : pts_ms;

                if (launch.style == VIDEO_STYLE_NETFLIX &&
                    launch.intro_end_ms > launch.intro_start_ms &&
                    position_ms >= launch.intro_start_ms &&
                    position_ms < launch.intro_end_ms)
                {
                    seek_target = launch.intro_end_ms;
                    action = VIDEO_INPUT_SEEK_FORWARD;
                }
                else if (launch.style == VIDEO_STYLE_NETFLIX &&
                         launch.credits_start_ms > 0 &&
                         position_ms >= launch.credits_start_ms)
                {
                    action = VIDEO_INPUT_COMPLETE;
                    break;
                }
                else
                {
                    if (launch.style == VIDEO_STYLE_YOUTUBE)
                        launch.fill = !launch.fill;
                    else if (launch.style == VIDEO_STYLE_INSTAGRAM_FEED &&
                             !launch.instagram_feed_expanded)
                        launch.instagram_feed_expanded = true;
                    else if (launch.style == VIDEO_STYLE_INSTAGRAM ||
                             launch.style == VIDEO_STYLE_INSTAGRAM_FEED)
                        video_instagram_toggle_like(&launch);
                    else if (launch.style == VIDEO_STYLE_TIKTOK)
                        video_tiktok_toggle_like(&launch);
                    overlay_until = current_tick + HZ * 3;
                    action = VIDEO_INPUT_NONE;
                }
            }
            if (action == VIDEO_INPUT_SEEK_BACK ||
                action == VIDEO_INPUT_SEEK_FORWARD)
            {
                int64_t target = seek_target >= 0 ? seek_target :
                    (int64_t)(audio_master ? video_pcm_get_clock_ms() :
                              pts_ms) +
                    (action == VIDEO_INPUT_SEEK_BACK ? -10000 : 10000);

                if (target < 0)
                    target = 0;
                if ((uint64_t)target >= duration_ms && duration_ms > 0)
                    target = duration_ms - 1;
                sample = video_sample_for_ms(&demux, (uint32_t)target,
                                             &timing);
                pts_ms = video_pts_ms(&demux, &timing);
                vpu_h264_close(decoder);
                decoder = vpu_h264_open(
                    decoder_buffer, decoder_size,
                    (demux.width + 15) & ~15,
                    (demux.height + 15) & ~15);
                if (decoder == NULL ||
                    vpu_h264_configure(decoder, demux.codecdata,
                                       demux.codecdata_len) < 0)
                {
                    splash(HZ * 2, "H.264 seek failed");
                    decoder = NULL;
                    goto cleanup;
                }
                if (have_audio)
                    video_audio_seek(pts_ms);
                start_tick = current_tick -
                    (long)((uint64_t)pts_ms * HZ / 1000u);
                overlay_until = current_tick + HZ * 3;
                action = VIDEO_INPUT_NONE;
                seek_requested = true;
                break;
            }
            if (action != VIDEO_INPUT_NONE)
                break;
            if (paused)
            {
                sleep(1);
                continue;
            }
            clock_ms = audio_master ? video_pcm_get_clock_ms() :
                (uint32_t)((current_tick - start_tick) * 1000 / HZ);
            if (audio_master &&
                (video_audio_failed() ||
                 (!video_audio_is_active() && video_pcm_empty())))
            {
                video_audio_stop();
                have_audio = false;
                audio_master = false;
                start_tick = current_tick -
                    (long)((uint64_t)clock_ms * HZ / 1000u);
            }
            if (clock_ms + 2 >= pts_ms)
                break;
            sleep(1);
        }
        if (action != VIDEO_INPUT_NONE)
            break;
        if (seek_requested)
            continue;
        if (mp4v_get_sample_offset(&demux, sample, &offset, &size) < 0 ||
            size == 0 || size > VIDEO_READ_BUFFER ||
            lseek(video_fd, offset, SEEK_SET) < 0 ||
            read(video_fd, read_buffer, size) != (ssize_t)size)
        {
            splash(HZ * 2, "Video sample read failed");
            goto cleanup;
        }
        decoded = vpu_h264_decode_sample(
            decoder, read_buffer, size, demux.nalu_len_size);
        if (decoded < 0)
        {
            splash(HZ * 2, "VPU sample decode failed");
            goto cleanup;
        }
        if (decoded > 0)
        {
            const uint8_t *frame_y;
            const uint8_t *frame_cb;
            const uint8_t *frame_cr;
            int width;
            int height;
            int stride;

            vpu_h264_get_frame(decoder, &frame_y, &frame_cb, &frame_cr,
                               &width, &height, &stride);
            video_draw_frame(frame_y, frame_cb, frame_cr, width, height,
                             stride, scale_buffer, &launch, paused, pts_ms,
                             duration_ms,
                             paused || TIME_BEFORE(current_tick,
                                                   overlay_until));
        }
        last_sample = sample;
        video_timing_advance(&demux, &timing);
        sample++;
    }
    if (action == VIDEO_INPUT_EXIT)
        result = 1;
    else if (action == VIDEO_INPUT_PREVIOUS)
        result = 2;
    else if (action == VIDEO_INPUT_NEXT)
        result = 3;
    else if (action == VIDEO_INPUT_PROFILE)
        result = 4;
    else
        result = 0;

    if (launch.allow_resume)
    {
        if (result == 0)
            video_resume_clear(launch.path);
        else
            video_resume_save(launch.path, last_sample, demux.num_samples);
    }

cleanup:
    if (have_audio)
        video_audio_stop();
    if (video_fd >= 0)
        close(video_fd);
    if (decoder != NULL)
        vpu_h264_close(decoder);
    pcmbuf_fade(false, false);
    backlight_set_timeout(global_settings.backlight_timeout);
    lcd_set_foreground(LCD_BLACK);
    lcd_clear_display();
    lcd_update();
    if (cpu_boosted)
        cpu_boost(false);
    return result;
}

#endif /* IPOD_6G && !SIMULATOR */
