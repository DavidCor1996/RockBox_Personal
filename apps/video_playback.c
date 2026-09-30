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
#include "gui/tv_ui.h"

#if defined(IPOD_6G) && !defined(SIMULATOR)

#include "backlight.h"
#include "bmp.h"
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
#include "video_file.h"
#include "video_read_cache.h"
#include "video_pcm.h"
#include "video_playback.h"
#include "video_completion.h"
#include "video_resume.h"
#include "video_library_state.h"
#include "vpu_h264.h"
#include "videoout.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NF_API(name) name
#include "netflix_captions.h"
#include "netflix_brand.h"
#undef NF_API

#include "video_capabilities.h"
#include "video_metrics.h"

#define VIDEO_MAX_WIDTH          VIDEO_CAP_H264_WIDTH
#define VIDEO_MAX_HEIGHT         VIDEO_CAP_H264_HEIGHT
#define VIDEO_MAX_LEVEL          VIDEO_CAP_H264_LEVEL
#define VIDEO_MAX_SAMPLES        VIDEO_CAP_VIDEO_SAMPLES
#define VIDEO_MAX_AUDIO_SAMPLES  VIDEO_CAP_AUDIO_SAMPLES
#define VIDEO_READ_BUFFER        VIDEO_CAP_SAMPLE_BUFFER_BYTES
#define VIDEO_OUTPUT_BUFFER      (LCD_WIDTH * LCD_HEIGHT * 3u / 2u)
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
#define VIDEO_TWITCH_ICON        ROCKBOX_DIR \
                                 "/ipodjs/twitch/twitch-glitch-current.18x20.bmp"
#define VIDEO_TWITCH_CHAT_W      146
#define VIDEO_TWITCH_CHAT_ROWS   5
#define VIDEO_TWITCH_CHAT_USER   25
#define VIDEO_TWITCH_CHAT_TEXT   160
#define VIDEO_TWITCH_CHAT_LINE   320
#define VIDEO_TWITCH_CHAT_ANIMATION_TICKS MAX(1, (HZ * 2) / 5)
#define VIDEO_TWITCH_CHAT_EASE_SCALE 1024
#define VIDEO_TWITCH_EMOJI_SIZE  14
#define VIDEO_TWITCH_EMOJI_CACHE 32
#define VIDEO_TWITCH_EMOJI_BYTES \
    (VIDEO_TWITCH_EMOJI_SIZE * VIDEO_TWITCH_EMOJI_SIZE * 4)
#define VIDEO_TWITCH_EMOJI_HEADER 10

enum video_style
{
    VIDEO_STYLE_STOCK = 0,
    VIDEO_STYLE_YOUTUBE,
    VIDEO_STYLE_YOUTUBE_LIVE,
    VIDEO_STYLE_TWITCH,
    VIDEO_STYLE_TWITCH_LIVE,
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
    char twitch_creator_key[17];
    char twitch_creator[49];
    char twitch_title[81];
    char twitch_game[49];
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

struct video_twitch_chat_message
{
    uint32_t offset;
    uint32_t color;
    char user[VIDEO_TWITCH_CHAT_USER];
    char text[VIDEO_TWITCH_CHAT_TEXT];
};

struct video_twitch_emoji
{
    int index;
    unsigned char rgba[VIDEO_TWITCH_EMOJI_BYTES];
};

struct video_twitch_chat_state
{
    int fd;
    int emoji_fd;
    bool available;
    bool visible;
    int panel_width;
    int animation_from;
    long animation_started;
    bool pending_valid;
    uint16_t emoji_count;
    int emoji_next;
    int first;
    int count;
    uint32_t last_position;
    struct video_twitch_chat_message pending;
    struct video_twitch_chat_message rows[VIDEO_TWITCH_CHAT_ROWS];
    struct video_twitch_emoji emojis[VIDEO_TWITCH_EMOJI_CACHE];
};

static unsigned char video_twitch_icon_data[18 * 20 * sizeof(fb_data)]
    CACHEALIGN_ATTR;
static struct bitmap video_twitch_icon;
static bool video_twitch_icon_valid;
static struct video_twitch_chat_state video_twitch_chat;

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
    if (video_prefix(parameter, "twitch-live:", &path))
    {
        const char *creator = strchr(path, ':');
        const char *separator = creator != NULL ? strchr(creator + 1, ':') : NULL;

        if (creator == NULL || separator == NULL || creator[1] == '\0' ||
            separator[1] == '\0')
            return false;
        launch->style = VIDEO_STYLE_TWITCH_LIVE;
        launch->live_epoch = strtoul(path, NULL, 10);
        launch->live = true;
        launch->allow_resume = false;
        strlcpy(launch->twitch_creator_key, creator + 1,
                MIN(sizeof(launch->twitch_creator_key),
                    (size_t)(separator - creator)));
        launch->path = separator + 1;
    }
    else if (video_prefix(parameter, "twitch-app:", &path))
    {
        launch->style = VIDEO_STYLE_TWITCH;
        launch->path = path;
    }
    else if (video_prefix(parameter, "youtube-live:", &path))
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

static void video_twitch_load(struct video_launch *launch)
{
    char metadata_path[MAX_PATH];
    char line[160];
    char *dot;
    int fd;

    if (launch->style != VIDEO_STYLE_TWITCH &&
        launch->style != VIDEO_STYLE_TWITCH_LIVE)
        return;
    video_twitch_icon.data = video_twitch_icon_data;
    video_twitch_icon_valid =
        read_bmp_file(VIDEO_TWITCH_ICON, &video_twitch_icon,
                      sizeof(video_twitch_icon_data), FORMAT_NATIVE, NULL) > 0 &&
        video_twitch_icon.width == 18 && video_twitch_icon.height == 20;
    strlcpy(metadata_path, launch->path, sizeof(metadata_path));
    dot = strrchr(metadata_path, '.');
    if (dot == NULL)
        return;
    strlcpy(dot, ".twm", sizeof(metadata_path) - (dot - metadata_path));
    fd = open(metadata_path, O_RDONLY);
    if (fd < 0)
        return;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        if (!strncmp(line, "title=", 6))
            video_metadata_value(launch->twitch_title,
                                 sizeof(launch->twitch_title), line + 6);
        else if (!strncmp(line, "creator=", 8))
            video_metadata_value(launch->twitch_creator,
                                 sizeof(launch->twitch_creator), line + 8);
        else if (!strncmp(line, "game=", 5))
            video_metadata_value(launch->twitch_game,
                                 sizeof(launch->twitch_game), line + 5);
    }
    close(fd);
}

static void video_twitch_sidecar_path(const char *video, const char *extension,
                                      char *path, size_t size)
{
    char *dot;

    strlcpy(path, video, size);
    dot = strrchr(path, '.');
    if (dot != NULL)
        strlcpy(dot, extension, size - (dot - path));
    else
        path[0] = '\0';
}

static char *video_twitch_chat_tab(char **cursor)
{
    char *value = *cursor;
    char *tab = strchr(value, '\t');

    if (tab == NULL)
        return NULL;
    *tab = '\0';
    *cursor = tab + 1;
    return value;
}

static bool video_twitch_chat_read_next(struct video_twitch_chat_state *chat)
{
    char line[VIDEO_TWITCH_CHAT_LINE];

    chat->pending_valid = false;
    while (chat->fd >= 0 && read_line(chat->fd, line, sizeof(line)) > 0)
    {
        char *cursor = line;
        char *offset;
        char *color;
        char *user;

        if (line[0] == '#')
            continue;
        offset = video_twitch_chat_tab(&cursor);
        color = video_twitch_chat_tab(&cursor);
        user = video_twitch_chat_tab(&cursor);
        if (offset == NULL || color == NULL || user == NULL || !cursor[0])
            continue;
        chat->pending.offset = strtoul(offset, NULL, 10);
        chat->pending.color = strtoul(color, NULL, 16);
        strlcpy(chat->pending.user, user, sizeof(chat->pending.user));
        strlcpy(chat->pending.text, cursor, sizeof(chat->pending.text));
        chat->pending_valid = true;
        return true;
    }
    return false;
}

static int video_twitch_chat_hex(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static int video_twitch_chat_token(const char *text)
{
    int index = 0;
    int digit;
    int i;

    if (text[0] != '~' || text[1] != 'E' || text[6] != '~')
        return -1;
    for (i = 2; i < 6; i++)
    {
        digit = video_twitch_chat_hex(text[i]);
        if (digit < 0)
            return -1;
        index = index * 16 + digit;
    }
    return index;
}

static struct video_twitch_emoji *video_twitch_chat_cached_emoji(
    struct video_twitch_chat_state *chat, int index)
{
    int i;

    for (i = 0; i < VIDEO_TWITCH_EMOJI_CACHE; i++)
        if (chat->emojis[i].index == index)
            return &chat->emojis[i];
    return NULL;
}

static void video_twitch_chat_load_emoji(struct video_twitch_chat_state *chat,
                                         int index)
{
    struct video_twitch_emoji *emoji;
    off_t offset;

    if (chat->emoji_fd < 0 || index < 0 || index >= chat->emoji_count ||
        video_twitch_chat_cached_emoji(chat, index) != NULL)
        return;
    emoji = &chat->emojis[chat->emoji_next];
    chat->emoji_next = (chat->emoji_next + 1) % VIDEO_TWITCH_EMOJI_CACHE;
    emoji->index = -1;
    offset = VIDEO_TWITCH_EMOJI_HEADER +
        (off_t)index * VIDEO_TWITCH_EMOJI_BYTES;
    if (lseek(chat->emoji_fd, offset, SEEK_SET) == offset &&
        read(chat->emoji_fd, emoji->rgba,
             sizeof(emoji->rgba)) == (ssize_t)sizeof(emoji->rgba))
        emoji->index = index;
}

static void video_twitch_chat_prefetch(struct video_twitch_chat_state *chat,
                                       const char *text)
{
    while (text[0])
    {
        int index = video_twitch_chat_token(text);

        if (index >= 0)
        {
            video_twitch_chat_load_emoji(chat, index);
            text += 7;
        }
        else
            text++;
    }
}

static void video_twitch_chat_push(struct video_twitch_chat_state *chat)
{
    int slot;

    if (chat->count < VIDEO_TWITCH_CHAT_ROWS)
    {
        slot = (chat->first + chat->count) % VIDEO_TWITCH_CHAT_ROWS;
        chat->count++;
    }
    else
    {
        slot = chat->first;
        chat->first = (chat->first + 1) % VIDEO_TWITCH_CHAT_ROWS;
    }
    chat->rows[slot] = chat->pending;
    video_twitch_chat_prefetch(chat, chat->rows[slot].text);
}

static void video_twitch_chat_reset(struct video_twitch_chat_state *chat)
{
    int i;

    chat->first = 0;
    chat->count = 0;
    chat->last_position = 0;
    for (i = 0; i < VIDEO_TWITCH_EMOJI_CACHE; i++)
        chat->emojis[i].index = -1;
    chat->emoji_next = 0;
    if (chat->fd >= 0 && lseek(chat->fd, 0, SEEK_SET) >= 0)
        video_twitch_chat_read_next(chat);
    else
        chat->pending_valid = false;
}

static void video_twitch_chat_update(struct video_twitch_chat_state *chat,
                                     uint32_t position, bool unbounded)
{
    int advanced = 0;

    if (!chat->available)
        return;
    if (position + 2 < chat->last_position)
        video_twitch_chat_reset(chat);
    while (chat->pending_valid && chat->pending.offset <= position &&
           (unbounded || advanced < 64))
    {
        video_twitch_chat_push(chat);
        video_twitch_chat_read_next(chat);
        advanced++;
    }
    chat->last_position = position;
}

static bool video_twitch_chat_animate(struct video_twitch_chat_state *chat)
{
    int target = chat->visible ? VIDEO_TWITCH_CHAT_W : 0;
    int elapsed;
    int progress;
    int eased;
    int width;

    if (chat->panel_width == target)
        return false;
    elapsed = MIN(VIDEO_TWITCH_CHAT_ANIMATION_TICKS,
                  MAX(1, (int)(current_tick - chat->animation_started)));
    progress = elapsed * VIDEO_TWITCH_CHAT_EASE_SCALE /
        VIDEO_TWITCH_CHAT_ANIMATION_TICKS;
    /* Smoothstep keeps the video reflow and chat edge moving together. */
    eased = progress * progress *
        (3 * VIDEO_TWITCH_CHAT_EASE_SCALE - 2 * progress) /
        (VIDEO_TWITCH_CHAT_EASE_SCALE * VIDEO_TWITCH_CHAT_EASE_SCALE);
    width = chat->animation_from +
        (target - chat->animation_from) * eased /
        VIDEO_TWITCH_CHAT_EASE_SCALE;
    if (elapsed >= VIDEO_TWITCH_CHAT_ANIMATION_TICKS)
        width = target;
    width = MAX(0, MIN(VIDEO_TWITCH_CHAT_W, width));
    if (width != target)
        width = MAX(2, width & ~1);
    if (width == chat->panel_width)
        return false;
    chat->panel_width = width;
    return true;
}

static void video_twitch_chat_toggle(struct video_twitch_chat_state *chat)
{
    chat->visible = !chat->visible;
    chat->animation_from = chat->panel_width;
    chat->animation_started = current_tick - 1;
    video_twitch_chat_animate(chat);
}

static void video_twitch_chat_open(struct video_twitch_chat_state *chat,
                                   const char *video)
{
    unsigned char header[VIDEO_TWITCH_EMOJI_HEADER];
    char path[MAX_PATH];

    memset(chat, 0, sizeof(*chat));
    chat->fd = -1;
    chat->emoji_fd = -1;
    video_twitch_sidecar_path(video, ".twc", path, sizeof(path));
    if (!path[0])
        return;
    chat->fd = open(path, O_RDONLY);
    if (chat->fd < 0)
        return;
    video_twitch_sidecar_path(video, ".twe", path, sizeof(path));
    chat->emoji_fd = open(path, O_RDONLY);
    if (chat->emoji_fd >= 0 &&
        read(chat->emoji_fd, header, sizeof(header)) == sizeof(header) &&
        !memcmp(header, "TWE1", 4) && header[6] == VIDEO_TWITCH_EMOJI_SIZE &&
        header[8] == VIDEO_TWITCH_EMOJI_SIZE)
        chat->emoji_count = header[4] | ((uint16_t)header[5] << 8);
    else if (chat->emoji_fd >= 0)
    {
        close(chat->emoji_fd);
        chat->emoji_fd = -1;
    }
    video_twitch_chat_reset(chat);
    chat->available = chat->pending_valid;
    if (!chat->available)
    {
        close(chat->fd);
        chat->fd = -1;
    }
}

static void video_twitch_chat_close(struct video_twitch_chat_state *chat)
{
    if (chat->fd >= 0)
        close(chat->fd);
    if (chat->emoji_fd >= 0)
        close(chat->emoji_fd);
    chat->fd = -1;
    chat->emoji_fd = -1;
    chat->available = false;
    chat->visible = false;
    chat->panel_width = 0;
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
    long *overlay_until, struct video_twitch_chat_state *chat)
{
    static bool caption_held;
    int button = button_get_w_tmo(0);
#ifdef BUTTON_RC_PLAY
    int flags = button & (BUTTON_REL | BUTTON_REPEAT);
    switch (button & ~(BUTTON_REL | BUTTON_REPEAT))
    {
        case BUTTON_RC_PLAY: button = BUTTON_PLAY | flags; break;
        case BUTTON_RC_SELECT: button = BUTTON_SELECT | flags; break;
        case BUTTON_RC_MENU: button = flags ? BUTTON_NONE : BUTTON_MENU; break;
        case BUTTON_RC_LEFT:
            if (flags & BUTTON_REPEAT) return VIDEO_INPUT_EXIT;
            button = flags & BUTTON_REL ? BUTTON_LEFT : BUTTON_NONE;
            break;
        case BUTTON_RC_RIGHT: button = flags & BUTTON_REL ? BUTTON_NONE : BUTTON_RIGHT; break;
        case BUTTON_RC_UP: case BUTTON_RC_VOL_UP:
        case BUTTON_RC_DOWN: case BUTTON_RC_VOL_DOWN:
            if (!(flags & BUTTON_REL))
            {
                video_volume_change(button & (BUTTON_RC_UP|BUTTON_RC_VOL_UP) ? 1 : -1);
                *overlay_until = current_tick + HZ * 2;
            }
            return VIDEO_INPUT_NONE;
    }
#endif

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
    if (button == BUTTON_SELECT) caption_held = false;
    if (button == (BUTTON_SELECT | BUTTON_REL) && caption_held)
        return VIDEO_INPUT_NONE;
    if (button == (BUTTON_SELECT | BUTTON_REPEAT))
    {
        if (!caption_held) nf_caption_toggle();
        caption_held = true;
        *overlay_until = current_tick + HZ*2;
        return VIDEO_INPUT_NONE;
    }
    if (button == (BUTTON_SELECT | BUTTON_REL))
    {
        if ((launch->style == VIDEO_STYLE_TWITCH ||
             launch->style == VIDEO_STYLE_TWITCH_LIVE) &&
            chat->available)
        {
            video_twitch_chat_toggle(chat);
            *overlay_until = current_tick + HZ * 2;
            return VIDEO_INPUT_NONE;
        }
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

static void video_yuv_blend_pixel(uint8_t * const *planes, int x, int y,
                                  int red, int green, int blue, int alpha)
{
    uint8_t py;
    uint8_t pu;
    uint8_t pv;
    int index;

    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT || alpha <= 0)
        return;
    if (alpha >= 255)
    {
        video_yuv_pixel(planes, x, y, red, green, blue);
        return;
    }
    video_yuv_color(red, green, blue, &py, &pu, &pv);
    index = y * LCD_WIDTH + x;
    planes[0][index] =
        (planes[0][index] * (255 - alpha) + py * alpha) / 255;
    if ((x & 1) == 0 && (y & 1) == 0)
    {
        index = (y / 2) * (LCD_WIDTH / 2) + x / 2;
        planes[1][index] =
            (planes[1][index] * (255 - alpha) + pu * alpha) / 255;
        planes[2][index] =
            (planes[2][index] * (255 - alpha) + pv * alpha) / 255;
    }
}

static void video_yuv_blend_rect(uint8_t * const *planes, int x, int y,
                                 int width, int height,
                                 int red, int green, int blue, int alpha)
{
    int row;
    int column;

    for (row = MAX(0, y); row < MIN(LCD_HEIGHT, y + height); row++)
        for (column = MAX(0, x);
             column < MIN(LCD_WIDTH, x + width); column++)
            video_yuv_blend_pixel(planes, column, row,
                                  red, green, blue, alpha);
}

static void video_twitch_chat_text(uint8_t * const *planes, int x, int y,
                                   const char *text,
                                   int red, int green, int blue)
{
    video_yuv_text(planes, x + 1, y + 1, text, 0, 0, 0);
    video_yuv_text(planes, x, y, text, red, green, blue);
}

static void video_twitch_chat_emoji(uint8_t * const *planes,
                                    struct video_twitch_emoji *emoji,
                                    int x, int y)
{
    int row;
    int column;

    if (emoji == NULL)
        return;
    for (row = 0; row < VIDEO_TWITCH_EMOJI_SIZE; row++)
        for (column = 0; column < VIDEO_TWITCH_EMOJI_SIZE; column++)
        {
            const unsigned char *pixel = emoji->rgba +
                (row * VIDEO_TWITCH_EMOJI_SIZE + column) * 4;

            video_yuv_blend_pixel(planes, x + column, y + row,
                                  pixel[0], pixel[1], pixel[2], pixel[3]);
        }
}

static void video_twitch_chat_message(uint8_t * const *planes,
                                      struct video_twitch_chat_state *chat,
                                      const char *text, int x, int y,
                                      int left, int right)
{
    struct font *font = font_get(FONT_SYSFIXED);
    int line = 0;

    if (font == NULL)
        return;
    while (*text && line < 2)
    {
        int emoji_index = video_twitch_chat_token(text);
        int width;

        if (emoji_index >= 0)
        {
            width = VIDEO_TWITCH_EMOJI_SIZE + 1;
            if (x + width > right && x > left + 6)
            {
                line++;
                x = left + 6;
                y += 14;
                if (line >= 2)
                    break;
            }
            video_twitch_chat_emoji(
                planes, video_twitch_chat_cached_emoji(chat, emoji_index),
                x, y - 2);
            x += width;
            text += 7;
            continue;
        }
        width = font_get_width(font, (unsigned char)*text);
        if (x + width > right)
        {
            line++;
            x = left + 6;
            y += 14;
            while (*text == ' ')
                text++;
            continue;
        }
        {
            char character[2] = {*text++, '\0'};

            video_twitch_chat_text(planes, x, y, character,
                                   239, 239, 241);
        }
        x += width;
    }
}

static void video_yuv_twitch_icon(uint8_t * const *planes, int x, int y);

static void video_twitch_chat_draw(uint8_t * const *planes,
                                   struct video_twitch_chat_state *chat)
{
    int panel_x = LCD_WIDTH - chat->panel_width;
    int row_y = LCD_HEIGHT - chat->count * 40;
    int i;

    if (!chat->available || chat->panel_width <= 0)
        return;
    video_yuv_blend_rect(planes, panel_x, 0, chat->panel_width,
                         LCD_HEIGHT, 14, 14, 16, 224);
    video_yuv_rect(planes, panel_x, 0, 2, LCD_HEIGHT, 145, 70, 255);
    video_yuv_blend_rect(planes, panel_x + 2, 0,
                         MAX(0, chat->panel_width - 2), 24,
                         92, 22, 197, 214);
    video_yuv_twitch_icon(planes, panel_x + 5, 2);
    video_twitch_chat_text(planes, panel_x + 29, 7, "CHAT",
                           255, 255, 255);
    row_y = MAX(28, row_y);
    for (i = 0; i < chat->count; i++)
    {
        struct video_twitch_chat_message *message =
            &chat->rows[(chat->first + i) % VIDEO_TWITCH_CHAT_ROWS];
        int red = (message->color >> 16) & 0xff;
        int green = (message->color >> 8) & 0xff;
        int blue = message->color & 0xff;

        if (red + green + blue < 210)
        {
            red = (red + 255) / 2;
            green = (green + 255) / 2;
            blue = (blue + 255) / 2;
        }
        video_twitch_chat_text(planes, panel_x + 6, row_y,
                               message->user, red, green, blue);
        video_twitch_chat_message(planes, chat, message->text,
                                  panel_x + 6, row_y + 12,
                                  panel_x, LCD_WIDTH - 5);
        row_y += 40;
    }
}

static void video_yuv_twitch_icon(uint8_t * const *planes, int x, int y)
{
    int row;
    int column;

    if (!video_twitch_icon_valid)
        return;
    for (row = 0; row < video_twitch_icon.height; row++)
        for (column = 0; column < video_twitch_icon.width; column++)
        {
            fb_data pixel = ((const fb_data *)video_twitch_icon.data)
                [row * video_twitch_icon.width + column];
            int red = FB_UNPACK_RED(pixel);
            int green = FB_UNPACK_GREEN(pixel);
            int blue = FB_UNPACK_BLUE(pixel);

            if (red > 248 && green < 8 && blue > 248)
                continue;
            video_yuv_pixel(planes, x + column, y + row,
                            red, green, blue);
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
                               uint32_t duration_ms, bool visible,
                               int content_width)
{
    char current[20];
    char duration[20];
    int video_width = MAX(2, MIN(LCD_WIDTH, content_width));
    int bar_width = video_width - 16;
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

    if (launch->style == VIDEO_STYLE_TWITCH_LIVE)
    {
        video_yuv_rect(planes, 0, LCD_HEIGHT - 28, video_width, 28,
                       14, 14, 16);
        video_yuv_rect(planes, 0, LCD_HEIGHT - 28, video_width, 3,
                       145, 70, 255);
        video_yuv_twitch_icon(planes, 5, LCD_HEIGHT - 23);
        video_yuv_rect(planes, 29, LCD_HEIGHT - 22, 39, 16,
                       204, 24, 40);
        video_yuv_text(planes, 34, LCD_HEIGHT - 18, "LIVE",
                       255, 255, 255);
        video_yuv_text(planes, 76, LCD_HEIGHT - 23,
                       launch->twitch_creator[0] ?
                       launch->twitch_creator : "Twitch",
                       191, 148, 255);
        video_yuv_text(planes, 76, LCD_HEIGHT - 13,
                       launch->twitch_title[0] ?
                       launch->twitch_title : "Twitch VOD",
                       255, 255, 255);
        return;
    }
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
        nf_brand_draw(planes, LCD_WIDTH, LCD_HEIGHT, 7, LCD_HEIGHT - 54);
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
    else if (launch->style == VIDEO_STYLE_TWITCH)
    {
        int duration_width = video_yuv_text_width(duration);
        int rail_width = MAX(1, video_width - 126);

        video_yuv_rect(planes, 0, LCD_HEIGHT - 38, video_width, 38,
                       14, 14, 16);
        video_yuv_rect(planes, 0, LCD_HEIGHT - 38, video_width, 3,
                       145, 70, 255);
        video_yuv_twitch_icon(planes, 5, LCD_HEIGHT - 34);
        video_yuv_text(planes, 29, LCD_HEIGHT - 33,
                       launch->twitch_title[0] ?
                       launch->twitch_title : "Twitch VOD",
                       255, 255, 255);
        video_yuv_text(planes, 5, LCD_HEIGHT - 12, current,
                       222, 217, 229);
        video_yuv_text(planes, video_width - 5 - duration_width,
                       LCD_HEIGHT - 12, duration, 222, 217, 229);
        /* The rail owns a separate row above both time labels. This keeps
         * hour-long timestamps from extending underneath it. */
        video_yuv_rect(planes, 59, LCD_HEIGHT - 22,
                       rail_width, 4, 63, 63, 70);
        video_yuv_rect(planes, 59, LCD_HEIGHT - 22,
                       rail_width * fill_width / MAX(1, bar_width), 4,
                       145, 70, 255);
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
                             uint32_t duration_ms, bool overlay_visible,
                             struct video_twitch_chat_state *chat)
{
    unsigned char *planes[3];
    int draw_width;
    int draw_height;
    int x;
    int y_pos;
    int content_width = LCD_WIDTH;
    bool twitch_reflow =
        (launch->style == VIDEO_STYLE_TWITCH ||
         launch->style == VIDEO_STYLE_TWITCH_LIVE) &&
        chat->panel_width > 0;

    if (y == NULL || cb == NULL || cr == NULL || scaled == NULL ||
        width <= 0 || height <= 0 || stride < width)
        return;
    static unsigned char caption[NF_CAP_BYTES];
    const unsigned char *caption_mask = nf_caption_snapshot(caption) ? caption : NULL;
    const unsigned char *tv_planes[3] = {y, cb, cr};
    tv_video_prepare(tv_planes, width, height, stride, width, height,
                     position_ms, duration_ms, paused, overlay_visible, caption_mask);
    if (twitch_reflow)
    {
        content_width = MAX(2, LCD_WIDTH - chat->panel_width);
        draw_width = content_width;
        draw_height = (int)((int64_t)height * draw_width / width);
        if (draw_height > LCD_HEIGHT)
        {
            draw_height = LCD_HEIGHT;
            draw_width = (int)((int64_t)width * draw_height / height);
        }
    }
    else if (launch->fill)
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
    if (twitch_reflow)
    {
        x = (content_width - draw_width) / 2;
        y_pos = (LCD_HEIGHT - draw_height) / 2;
    }
    else if (launch->style == VIDEO_STYLE_INSTAGRAM_FEED &&
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
    nf_caption_draw(planes, LCD_WIDTH,
        LCD_HEIGHT - (overlay_visible ? 64 : 0));
    video_draw_overlay(planes, launch, paused, position_ms, duration_ms,
                       overlay_visible, content_width);
    video_twitch_chat_draw(planes, chat);
#ifdef HAVE_VIDEOOUT_NATIVE_YUV
    const struct videoout_frame frame = {
        { y, cb, cr }, width, height, stride,
        x, y_pos, draw_width, draw_height
    };
    videoout_blit_yuv(&frame, planes);
#else
    lcd_blit_yuv(planes, 0, 0, LCD_WIDTH, 0, 0,
                 LCD_WIDTH, LCD_HEIGHT);
#endif
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
    struct video_read_cache read_cache = {0,0};
    video_trace_head = video_trace_total = video_trace_drops = 0;
    video_trace_max_late = video_trace_max_read = 0;
    video_trace_max_decode = video_trace_max_present = 0;
    video_trace_pool_reserved = 0;
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
    uint32_t position_ms = 0;
    long start_tick;
    long pause_started = 0;
    long state_saved_tick = current_tick;
    bool state_was_paused = false;
    long overlay_until;
    long last_present_tick = 0;
    long paused_overlay_tick = 0;
    bool frame_presented = false;
    bool paused = false;
    bool cpu_boosted = false;
    bool have_audio = false;
    bool audio_master = false;
    enum video_input_action action = VIDEO_INPUT_NONE;
    int result = -1;

    if (filepath == NULL || buffer == NULL || buffer_size == 0 ||
        !video_parse_launch(filepath, &launch))
        return -1;
    memset(&video_twitch_chat, 0, sizeof(video_twitch_chat));
    video_twitch_chat.fd = -1;
    video_twitch_chat.emoji_fd = -1;
    video_twitch_load(&launch);
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
    video_trace_pool_reserved = pool.cursor - (uint8_t *)buffer;
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
        struct video_library_state state;
        if (video_library_load(launch.path, &state))
            sample = video_sample_for_ms(&demux,
                state.watched ? 0 : MIN(state.position_ms, duration_ms-1), &timing);
        else
        {
            sample = video_resume_load(launch.path, demux.num_samples);
            while (sample > 0 && !mp4v_is_keyframe(&demux, sample)) sample--;
            video_timing_for_sample(&demux, sample, &timing);
        }
    }
    else
        video_timing_for_sample(&demux, 0, &timing);

    if (launch.style == VIDEO_STYLE_TWITCH ||
        launch.style == VIDEO_STYLE_TWITCH_LIVE)
    {
        video_twitch_chat_open(&video_twitch_chat, launch.path);
        video_twitch_chat_update(
            &video_twitch_chat,
            video_pts_ms(&demux, &timing) / 1000u, true);
    }

    /* Composite output already owns a boost because its memory reader
     * underruns at 54 MHz HClk. Decode plus LCD presentation needs the same
     * 108 MHz bus clock when undocked, so playback owns a separate reference. */
    nf_caption_open(launch.path, launch.style == VIDEO_STYLE_NETFLIX);
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
    if (demux.audio_num_samples > 0)
    {
        int wait = 0;
        uint32_t initial_ms = video_pts_ms(&demux, &timing);

        if (demux.audio_format != MAKEFOURCC('m', 'p', '4', 'a') ||
            demux.audio_codecdata_len == 0 ||
            video_audio_init(launch.path, &demux, audio_codec_workspace,
                             audio_codec_workspace_size) < 0)
        {
            splash(HZ * 2, "H.264 audio init failed");
            goto cleanup;
        }
        have_audio = true;
        audio_master = true;
        /* Fill the PCM queue without letting the soundtrack run ahead of
         * the first video frame during the prebuffer wait. */
        video_pcm_pause(true);
        video_audio_play();
        if (initial_ms > 0)
        {
            video_audio_seek(initial_ms);
            video_pcm_pause(true);
        }
        while (!video_audio_ready() && wait < HZ && action == VIDEO_INPUT_NONE)
        {
            action = video_input(&launch, &paused, &start_tick,
                                 &pause_started, true, &cpu_boosted,
                                 &overlay_until, &video_twitch_chat);
            sleep(1);
            wait++;
        }
        if (video_audio_failed())
        {
            splash(HZ * 2, "H.264 audio decode failed");
            goto cleanup;
        }
        video_pcm_pause(paused);
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
        uint32_t read_us=0, decode_us=0, present_us=0;
        int64_t seek_target = -1;
        bool seek_requested = false;
        int decoded = 0;
        bool sample_decoded = false;

        while (action == VIDEO_INPUT_NONE)
        {
            uint32_t clock_ms;

            action = video_input(&launch, &paused, &start_tick,
                                 &pause_started, have_audio, &cpu_boosted,
                                 &overlay_until, &video_twitch_chat);
            position_ms = audio_master ? video_pcm_get_clock_ms() : pts_ms;
            if (launch.allow_resume &&
                ((paused && !state_was_paused) ||
                 TIME_AFTER(current_tick, state_saved_tick + 30*HZ)))
            {
                video_library_save(launch.path, position_ms, duration_ms, false);
                state_saved_tick = current_tick;
            }
            state_was_paused = paused;
            nf_caption_service(position_ms);
            if (action == VIDEO_INPUT_ACTIVATE)
            {

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
                frame_presented = false;
                break;
            }
            if (action != VIDEO_INPUT_NONE)
                break;
            if (paused)
            {
                if (frame_presented && paused_overlay_tick != overlay_until)
                {
                    const uint8_t *y, *cb, *cr;
                    int width, height, stride;
                    vpu_h264_get_frame(decoder, &y, &cb, &cr,
                                       &width, &height, &stride);
                    video_draw_frame(y, cb, cr, width, height, stride,
                        scale_buffer, &launch, true, position_ms, duration_ms,
                        true, &video_twitch_chat);
                    paused_overlay_tick = overlay_until;
                }
                sleep(1);
                continue;
            }
            /* Decode ahead of the presentation deadline. Waiting first
             * adds the read/decode time to every displayed frame's PTS. */
            if (!sample_decoded)
            {
                uint32_t started = USEC_TIMER;
                const uint8_t *compressed = NULL;
                if (mp4v_get_sample_offset(&demux, sample, &offset, &size) >= 0)
                    compressed = video_read_cached(video_fd, read_buffer,
                        VIDEO_READ_BUFFER, &read_cache, offset, size);
                read_us = USEC_TIMER - started;
                if (!compressed)
                {
                    splash(HZ * 2, "Video sample read failed");
                    goto cleanup;
                }
                started = USEC_TIMER;
                decoded = vpu_h264_decode_sample(
                    decoder, compressed, size, demux.nalu_len_size);
                decode_us = USEC_TIMER - started;
                if (decoded < 0)
                {
                    splash(HZ * 2, "VPU sample decode failed");
                    goto cleanup;
                }
                sample_decoded = true;
            }
            clock_ms = audio_master ? video_pcm_get_clock_ms() :
                (uint32_t)((current_tick - start_tick) * 1000 / HZ);
            if (audio_master && video_audio_failed())
            {
                splash(HZ * 2, "H.264 audio decode failed");
                goto cleanup;
            }
            if (audio_master && !video_audio_is_active() && video_pcm_empty())
            {
                if (pts_ms + 2000u < duration_ms)
                {
                    splash(HZ * 2, "H.264 audio ended early");
                    goto cleanup;
                }
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
        if (decoded > 0 && audio_master && frame_presented &&
            sample + 1 < demux.num_samples)
        {
            struct video_timing next = timing;

            video_timing_advance(&demux, &next);
            /* Keep decoding reference pictures, but avoid expensive LCD
             * scaling/overlays for a frame whose successor is already due.
             * Still refresh at least every 100 ms under sustained overload. */
            if (video_pcm_get_clock_ms() >= video_pts_ms(&demux, &next) &&
                TIME_BEFORE(current_tick, last_present_tick + HZ / 10))
            {
                decoded = 0;
                video_trace_drops++;
            }
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
            video_twitch_chat_update(&video_twitch_chat,
                                     pts_ms / 1000u, false);
            video_twitch_chat_animate(&video_twitch_chat);
            uint32_t present_started = USEC_TIMER;
            video_draw_frame(frame_y, frame_cb, frame_cr, width, height,
                             stride, scale_buffer, &launch, paused, pts_ms,
                             duration_ms,
                             paused || TIME_BEFORE(current_tick,
                                                   overlay_until),
                             &video_twitch_chat);
            present_us = USEC_TIMER - present_started;
            last_present_tick = current_tick;
            frame_presented = true;
        }
        video_trace[video_trace_head].pts = pts_ms;
        video_trace[video_trace_head].clock = audio_master ? video_pcm_get_clock_ms() :
            (uint32_t)((current_tick-start_tick)*1000/HZ);
        video_trace[video_trace_head].read_us = read_us;
        video_trace[video_trace_head].decode_us = decode_us;
        video_trace[video_trace_head].present_us = present_us;
        video_trace[video_trace_head].width = demux.width;
        video_trace[video_trace_head].height = demux.height;
        video_trace[video_trace_head].presented = decoded > 0;
        video_trace[video_trace_head].audio_master = audio_master;
        uint32_t trace_clock = video_trace[video_trace_head].clock;
        if (trace_clock > pts_ms)
            video_trace_max_late = MAX(video_trace_max_late,trace_clock-pts_ms);
        video_trace_max_read = MAX(video_trace_max_read,read_us);
        video_trace_max_decode = MAX(video_trace_max_decode,decode_us);
        video_trace_max_present = MAX(video_trace_max_present,present_us);
        video_trace_head = (video_trace_head+1)%VIDEO_TRACE_COUNT;
        video_trace_total++;
        last_sample = sample;
        video_timing_advance(&demux, &timing);
        sample++;
    }
    if (action == VIDEO_INPUT_EXIT)
        result = launch.style == VIDEO_STYLE_NETFLIX &&
                 video_at_completion(position_ms, duration_ms,
                                     launch.credits_start_ms) ? 0 : 1;
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
        video_library_save(launch.path, position_ms, duration_ms, result == 0);
        if (result == 0)
            video_resume_clear(launch.path);
        else
            video_resume_save(launch.path, last_sample, demux.num_samples);
    }

cleanup:
    tv_video_prepare(NULL,0,0,0,0,0,0,0,false,false,NULL);
    nf_caption_close();
    video_twitch_chat_close(&video_twitch_chat);
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
    video_trace_export();
    return result;
}

#endif /* IPOD_6G && !SIMULATOR */
