/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__ /\_ \
 *                     \/            \/     \/    \/         \/
 *
 * Offline 2007-era YouTube library for RockPod-synchronised videos.
 *
 * Copyright (C) 2026 David Cor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/video_player.h"

#if !defined(HAVE_LCD_COLOR) || LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error The YouTube application requires a 320x240 colour display
#endif

#define YT_ROOT             ROCKBOX_DIR "/youtube"
#define YT_LIBRARY          YT_ROOT "/library.tsv"
#define YT_PROFILE          YT_ROOT "/profile.cfg"
#define YT_STATE            YT_ROOT "/state.tsv"
#define YT_STATE_TEMP       YT_ROOT "/state.tmp"
#define YT_MPEG_CONFIG      VIEWERS_DIR "/mpegplayer.cfg"
#define YT_ASSET_ROOT       ROCKBOX_DIR "/ipodjs/youtube"
#define YT_LOGO             YT_ASSET_ROOT "/youtube-logo-2006.bmp"
#define YT_STARS            YT_ASSET_ROOT "/youtube-stars-5-2007.bmp"
#define YT_STARS_ACTIVE     YT_ASSET_ROOT "/youtube-stars-active-2007.bmp"
#define YT_PLAYER_PREFIX    "youtube-app:"
#define YT_LIVE_PLAYER_PREFIX "youtube-live:"

#define YT_MAX_VIDEOS       96
#define YT_LINE_SIZE        1024
#define YT_THUMB_W          96
#define YT_THUMB_H          72
#define YT_LOGO_W           100
#define YT_LOGO_H           40
#define YT_STARS_W          70
#define YT_STARS_H          14
#define YT_BANNER_W         312
#define YT_BANNER_H         56
#define YT_LIST_TOP         64
#define YT_LIST_ROW_H       78
#define YT_LIST_VISIBLE     2
#define YT_TS_SECOND        45000

#define YT_BLUE             LCD_RGBPACK(0x00, 0x33, 0xcc)
#define YT_LIGHT_BLUE       LCD_RGBPACK(0xe6, 0xf1, 0xfa)
#define YT_BLUE_BORDER      LCD_RGBPACK(0xb4, 0xcf, 0xe7)
#define YT_LIGHT_GRAY       LCD_RGBPACK(0xf3, 0xf3, 0xf3)
#define YT_MID_GRAY         LCD_RGBPACK(0xc6, 0xc6, 0xc6)
#define YT_DARK_GRAY        LCD_RGBPACK(0x66, 0x66, 0x66)
#define YT_RED              LCD_RGBPACK(0xcc, 0x00, 0x00)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping yt_main_ctx[] =
{
    { PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK,               BUTTON_NONE },
    { PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD,                BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_LEFT,               BUTTON_LEFT,                      BUTTON_NONE },
    { PLA_RIGHT,              BUTTON_RIGHT,                     BUTTON_NONE },
    { PLA_LEFT_REPEAT,        BUTTON_LEFT|BUTTON_REPEAT,        BUTTON_NONE },
    { PLA_RIGHT_REPEAT,       BUTTON_RIGHT|BUTTON_REPEAT,       BUTTON_NONE },
    { PLA_SELECT_REL,         BUTTON_SELECT|BUTTON_REL,         BUTTON_NONE },
    { PLA_SELECT_REPEAT,      BUTTON_SELECT|BUTTON_REPEAT,      BUTTON_NONE },
    { PLA_CANCEL,             BUTTON_MENU,                      BUTTON_NONE },
    { PLA_EXIT,               BUTTON_PLAY|BUTTON_REL,           BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *plugin_contexts[] = { yt_main_ctx };
#else
static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };
#endif

enum yt_page
{
    YT_PAGE_HOME = 0,
    YT_PAGE_VIDEOS,
    YT_PAGE_LIVE,
    YT_PAGE_SUBSCRIPTIONS,
    YT_PAGE_PROFILE,
    YT_PAGE_COUNT,
};

enum yt_screen
{
    YT_SCREEN_BROWSER = 0,
    YT_SCREEN_DETAIL,
};

enum yt_video_filter
{
    YT_FILTER_RECENT = 0,
    YT_FILTER_MOST_VIEWED,
    YT_FILTER_TOP_RATED,
    YT_FILTER_FAVORITES,
    YT_FILTER_RELATED,
    YT_FILTER_COUNT,
};

struct yt_video
{
    char id[32];
    char title[80];
    char uploader[48];
    char duration[16];
    char upload_date[20];
    char views[24];
    char category[32];
    char video_path[MAX_PATH];
    char thumb_path[MAX_PATH];
    char description[240];
    char tags[120];
    int rating_x100;
    int rating_count;
    int home_order;
    int my_rating;
    unsigned long resume_seconds;
    unsigned long live_start_epoch;
    bool show_home;
    bool show_profile;
    bool subscription;
    bool favorite;
    bool is_live;
};

struct yt_profile
{
    char username[48];
    char display_name[64];
    char about[192];
    char city[48];
    char country[48];
    char occupation[64];
    char interests[96];
    char movies[96];
    char music[96];
    char books[96];
    char website[96];
    char joined[24];
    char profile_image[MAX_PATH];
    char banner_image[MAX_PATH];
    unsigned long channel_views;
    unsigned long video_views;
    unsigned long subscribers;
    unsigned long friends;
    int background_color;
    int module_color;
    int text_color;
    int link_color;
    bool show_about;
    bool show_videos;
    bool show_favorites;
};

static struct yt_video videos[YT_MAX_VIDEOS];
static struct yt_profile profile;
static int home_indices[YT_MAX_VIDEOS];
static int profile_indices[YT_MAX_VIDEOS];
static int video_indices[YT_MAX_VIDEOS];
static int live_indices[YT_MAX_VIDEOS];
static int subscription_indices[YT_MAX_VIDEOS];
static int video_count;
static int home_count;
static int profile_count;
static int filtered_video_count;
static int live_count;
static int subscription_count;
static enum yt_video_filter video_filter;
static enum yt_video_filter saved_video_filter;
static enum yt_page related_return_page;
static int related_anchor = -1;
static int profile_focus;

static enum yt_page current_page;
static enum yt_screen current_screen;
static int selection[YT_PAGE_COUNT];
static int detail_video = -1;
static int detail_action;
static bool running;

/* lcd_bitmap() consumes native RGB565 halfwords on iPod hardware.  Keep the
 * decoded pixels typed and cache-aligned: byte arrays may otherwise land at
 * odd plugin-BSS addresses, which x86 tolerates but ARM renders as rainbow
 * byte-rotated pixels. */
static fb_data logo_data[YT_LOGO_W * YT_LOGO_H] CACHEALIGN_ATTR;
static fb_data stars_data[YT_STARS_W * YT_STARS_H] CACHEALIGN_ATTR;
static fb_data stars_active_data[YT_STARS_W * YT_STARS_H] CACHEALIGN_ATTR;
static fb_data thumb_data[YT_LIST_VISIBLE][YT_THUMB_W * YT_THUMB_H]
    CACHEALIGN_ATTR;
static fb_data profile_data[64 * 64] CACHEALIGN_ATTR;
static fb_data banner_data[YT_BANNER_W * YT_BANNER_H] CACHEALIGN_ATTR;
static struct bitmap logo_bmp;
static struct bitmap stars_bmp;
static struct bitmap stars_active_bmp;
static struct bitmap thumb_bmp[YT_LIST_VISIBLE];
static struct bitmap profile_bmp;
static struct bitmap banner_bmp;
static bool logo_valid;
static bool stars_valid;
static bool stars_active_valid;
static bool thumb_valid[YT_LIST_VISIBLE];
static bool profile_valid;
static bool banner_valid;
static int thumb_video[YT_LIST_VISIBLE];
static int thumb_next_slot;

static int yt_hex_color(const char *value, int fallback)
{
    unsigned long color;
    char *end;

    if (value == NULL || value[0] != '#' || rb->strlen(value) != 7)
        return fallback;
    color = rb->strtoul(value + 1, &end, 16);
    if (end == value + 1 || *end != '\0')
        return fallback;
    return LCD_RGBPACK((color >> 16) & 0xff, (color >> 8) & 0xff,
                       color & 0xff);
}

static void yt_defaults(void)
{
    rb->memset(&profile, 0, sizeof(profile));
    rb->strlcpy(profile.username, "You", sizeof(profile.username));
    rb->strlcpy(profile.about, "This is my YouTube channel.",
                sizeof(profile.about));
    profile.background_color = LCD_WHITE;
    profile.module_color = YT_LIGHT_BLUE;
    profile.text_color = LCD_BLACK;
    profile.link_color = YT_BLUE;
    profile.show_about = true;
    profile.show_videos = true;
    profile.show_favorites = true;
}

static int yt_split_tabs(char *line, char **fields, int maximum)
{
    int count = 0;
    char *cursor = line;

    while (count < maximum)
    {
        char *tab;

        fields[count++] = cursor;
        tab = rb->strchr(cursor, '\t');
        if (tab == NULL)
            break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static unsigned long yt_view_count(const struct yt_video *video)
{
    const char *cursor = video->views;
    unsigned long value = 0;

    while (*cursor != '\0')
    {
        if (*cursor >= '0' && *cursor <= '9')
            value = value * 10 + (*cursor - '0');
        cursor++;
    }
    return value;
}

static bool yt_video_before(int left, int right)
{
    const struct yt_video *a = &videos[left];
    const struct yt_video *b = &videos[right];

    if (video_filter == YT_FILTER_RELATED && related_anchor >= 0)
    {
        const struct yt_video *anchor = &videos[related_anchor];
        bool a_uploader = !rb->strcmp(a->uploader, anchor->uploader);
        bool b_uploader = !rb->strcmp(b->uploader, anchor->uploader);

        if (a_uploader != b_uploader)
            return a_uploader;
    }
    if (video_filter == YT_FILTER_MOST_VIEWED)
        return yt_view_count(a) > yt_view_count(b);
    if (video_filter == YT_FILTER_TOP_RATED)
    {
        if (a->rating_x100 != b->rating_x100)
            return a->rating_x100 > b->rating_x100;
        return a->rating_count > b->rating_count;
    }
    return rb->strcmp(a->upload_date, b->upload_date) > 0;
}

static void yt_sort_video_indices(int *indices, int count)
{
    int i;

    for (i = 1; i < count; i++)
    {
        int value = indices[i];
        int position = i;

        while (position > 0 &&
               yt_video_before(value, indices[position - 1]))
        {
            indices[position] = indices[position - 1];
            position--;
        }
        indices[position] = value;
    }
}

static void yt_build_indices(void)
{
    int i;
    int j;

    home_count = 0;
    profile_count = 0;
    filtered_video_count = 0;
    live_count = 0;
    subscription_count = 0;
    for (i = 0; i < video_count; i++)
    {
        if (videos[i].show_home)
            home_indices[home_count++] = i;
        if (videos[i].is_live)
            live_indices[live_count++] = i;
        if (videos[i].show_profile && !videos[i].subscription)
            profile_indices[profile_count++] = i;
        if (videos[i].subscription)
            subscription_indices[subscription_count++] = i;
        if (!videos[i].subscription &&
            (video_filter != YT_FILTER_FAVORITES || videos[i].favorite) &&
            (video_filter != YT_FILTER_RELATED ||
             (i != related_anchor && related_anchor >= 0 &&
              (!rb->strcmp(videos[i].uploader,
                           videos[related_anchor].uploader) ||
               !rb->strcmp(videos[i].category,
                           videos[related_anchor].category)))))
            video_indices[filtered_video_count++] = i;
    }
    for (i = 1; i < home_count; i++)
    {
        int value = home_indices[i];

        j = i;
        while (j > 0 && videos[value].home_order <
               videos[home_indices[j - 1]].home_order)
        {
            home_indices[j] = home_indices[j - 1];
            j--;
        }
        home_indices[j] = value;
    }
    if (video_filter == YT_FILTER_RELATED &&
        filtered_video_count == 0 && related_anchor >= 0)
    {
        for (i = 0; i < video_count; i++)
            if (i != related_anchor)
                video_indices[filtered_video_count++] = i;
    }
    yt_sort_video_indices(video_indices, filtered_video_count);
    yt_sort_video_indices(live_indices, live_count);
}

static bool yt_load_library(void)
{
    char line[YT_LINE_SIZE];
    int fd = rb->open(YT_LIBRARY, O_RDONLY);

    video_count = 0;
    if (fd < 0)
        return false;
    while (video_count < YT_MAX_VIDEOS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[23];
        struct yt_video *video;
        int count;

        if (line[0] == '#' || line[0] == '\0')
            continue;
        count = yt_split_tabs(line, fields, ARRAYLEN(fields));
        if (count < 18 || !rb->strcmp(fields[0], "id"))
            continue;
        video = &videos[video_count];
        rb->memset(video, 0, sizeof(*video));
        rb->strlcpy(video->id, fields[0], sizeof(video->id));
        rb->strlcpy(video->title, fields[1], sizeof(video->title));
        rb->strlcpy(video->uploader, fields[2], sizeof(video->uploader));
        rb->strlcpy(video->duration, fields[3], sizeof(video->duration));
        rb->strlcpy(video->upload_date, fields[4],
                    sizeof(video->upload_date));
        rb->strlcpy(video->views, fields[5], sizeof(video->views));
        video->rating_x100 = rb->atoi(fields[6]);
        video->rating_count = rb->atoi(fields[7]);
        rb->strlcpy(video->category, fields[8], sizeof(video->category));
        video->show_home = fields[9][0] == '1';
        video->show_profile = fields[10][0] == '1';
        video->home_order = rb->atoi(fields[11]);
        rb->strlcpy(video->video_path, fields[12],
                    sizeof(video->video_path));
        rb->strlcpy(video->thumb_path, fields[13],
                    sizeof(video->thumb_path));
        rb->strlcpy(video->description, fields[14],
                    sizeof(video->description));
        rb->strlcpy(video->tags, fields[15], sizeof(video->tags));
        video->favorite = fields[16][0] == '1';
        video->my_rating = rb->atoi(fields[17]);
        video->subscription = count > 18 &&
                              !rb->strcmp(fields[18], "youtube-channel");
        video->is_live = count > 20 && fields[20][0] == '1';
        video->live_start_epoch = count > 21 ?
            rb->strtoul(fields[21], NULL, 10) : 0;
        video_count++;
    }
    rb->close(fd);
    yt_build_indices();
    return true;
}

static void yt_profile_field(const char *key, const char *value)
{
    if (!rb->strcmp(key, "username"))
        rb->strlcpy(profile.username, value, sizeof(profile.username));
    else if (!rb->strcmp(key, "display_name"))
        rb->strlcpy(profile.display_name, value, sizeof(profile.display_name));
    else if (!rb->strcmp(key, "about_me"))
        rb->strlcpy(profile.about, value, sizeof(profile.about));
    else if (!rb->strcmp(key, "city"))
        rb->strlcpy(profile.city, value, sizeof(profile.city));
    else if (!rb->strcmp(key, "country"))
        rb->strlcpy(profile.country, value, sizeof(profile.country));
    else if (!rb->strcmp(key, "occupation"))
        rb->strlcpy(profile.occupation, value, sizeof(profile.occupation));
    else if (!rb->strcmp(key, "interests"))
        rb->strlcpy(profile.interests, value, sizeof(profile.interests));
    else if (!rb->strcmp(key, "movies"))
        rb->strlcpy(profile.movies, value, sizeof(profile.movies));
    else if (!rb->strcmp(key, "music"))
        rb->strlcpy(profile.music, value, sizeof(profile.music));
    else if (!rb->strcmp(key, "books"))
        rb->strlcpy(profile.books, value, sizeof(profile.books));
    else if (!rb->strcmp(key, "website"))
        rb->strlcpy(profile.website, value, sizeof(profile.website));
    else if (!rb->strcmp(key, "joined"))
        rb->strlcpy(profile.joined, value, sizeof(profile.joined));
    else if (!rb->strcmp(key, "profile_image"))
        rb->strlcpy(profile.profile_image, value,
                    sizeof(profile.profile_image));
    else if (!rb->strcmp(key, "banner_image"))
        rb->strlcpy(profile.banner_image, value,
                    sizeof(profile.banner_image));
    else if (!rb->strcmp(key, "channel_views"))
        profile.channel_views = rb->strtoul(value, NULL, 10);
    else if (!rb->strcmp(key, "video_views"))
        profile.video_views = rb->strtoul(value, NULL, 10);
    else if (!rb->strcmp(key, "subscribers"))
        profile.subscribers = rb->strtoul(value, NULL, 10);
    else if (!rb->strcmp(key, "friends"))
        profile.friends = rb->strtoul(value, NULL, 10);
    else if (!rb->strcmp(key, "background_color"))
        profile.background_color = yt_hex_color(value, LCD_WHITE);
    else if (!rb->strcmp(key, "module_color"))
        profile.module_color = yt_hex_color(value, YT_LIGHT_BLUE);
    else if (!rb->strcmp(key, "text_color"))
        profile.text_color = yt_hex_color(value, LCD_BLACK);
    else if (!rb->strcmp(key, "link_color"))
        profile.link_color = yt_hex_color(value, YT_BLUE);
    else if (!rb->strcmp(key, "show_about"))
        profile.show_about = value[0] != '0';
    else if (!rb->strcmp(key, "show_videos"))
        profile.show_videos = value[0] != '0';
    else if (!rb->strcmp(key, "show_favorites"))
        profile.show_favorites = value[0] != '0';
}

static void yt_load_profile(void)
{
    char line[320];
    int fd;

    yt_defaults();
    fd = rb->open(YT_PROFILE, O_RDONLY);
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *equals = rb->strchr(line, '=');

        if (equals == NULL || line[0] == '#')
            continue;
        *equals++ = '\0';
        yt_profile_field(line, equals);
    }
    rb->close(fd);
}

static void yt_load_state(void)
{
    char line[128];
    int fd = rb->open(YT_STATE, O_RDONLY);

    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[3];
        int count = yt_split_tabs(line, fields, ARRAYLEN(fields));
        int i;

        if (count < 3 || !rb->strcmp(fields[0], "id"))
            continue;
        for (i = 0; i < video_count; i++)
        {
            if (!rb->strcmp(videos[i].id, fields[0]))
            {
                videos[i].my_rating = rb->atoi(fields[1]);
                videos[i].favorite = fields[2][0] == '1';
                break;
            }
        }
    }
    rb->close(fd);
}

static void yt_load_resume_positions(void)
{
    char line[MAX_PATH + 32];
    int fd;
    int i;

    for (i = 0; i < video_count; i++)
        videos[i].resume_seconds = 0;

    fd = rb->open(YT_MPEG_CONFIG, O_RDONLY);
    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *name;
        char *value;

        rb->settings_parseline(line, &name, &value);
        if (name == NULL || value == NULL || name[0] != '/')
            continue;

        for (i = 0; i < video_count; i++)
        {
            if (videos[i].is_live)
                continue;
            if (!rb->strcmp(videos[i].video_path, name))
            {
                long ticks = rb->strtol(value, NULL, 10);

                if (ticks >= YT_TS_SECOND * 3)
                    videos[i].resume_seconds = ticks / YT_TS_SECOND;
                break;
            }
        }
    }
    rb->close(fd);
}

static bool yt_save_state(void)
{
    int fd;
    int i;

    rb->mkdir(YT_ROOT);
    fd = rb->open(YT_STATE_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    rb->fdprintf(fd, "id\tmy_rating\tfavorite\n");
    for (i = 0; i < video_count; i++)
        rb->fdprintf(fd, "%s\t%d\t%d\n", videos[i].id,
                     videos[i].my_rating, videos[i].favorite ? 1 : 0);
    rb->close(fd);
    rb->remove(YT_STATE);
    return rb->rename(YT_STATE_TEMP, YT_STATE) == 0;
}

static bool yt_load_bitmap(const char *path, struct bitmap *bitmap,
                           void *buffer, size_t size,
                           int width, int height)
{
    rb->memset(bitmap, 0, sizeof(*bitmap));
    bitmap->data = buffer;
    return rb->read_bmp_file(path, bitmap, size, FORMAT_NATIVE, NULL) > 0 &&
           bitmap->width == width && bitmap->height == height;
}

static void yt_load_assets(void)
{
    logo_valid = yt_load_bitmap(YT_LOGO, &logo_bmp, logo_data,
                                sizeof(logo_data), YT_LOGO_W, YT_LOGO_H);
    stars_valid = yt_load_bitmap(YT_STARS, &stars_bmp, stars_data,
                                 sizeof(stars_data), YT_STARS_W, YT_STARS_H);
    stars_active_valid =
        yt_load_bitmap(YT_STARS_ACTIVE, &stars_active_bmp,
                       stars_active_data, sizeof(stars_active_data),
                       YT_STARS_W, YT_STARS_H);
    profile_valid = false;
    if (profile.profile_image[0])
        profile_valid = yt_load_bitmap(profile.profile_image, &profile_bmp,
                                       profile_data, sizeof(profile_data),
                                       64, 64);
    banner_valid = false;
    if (profile.banner_image[0])
        banner_valid = yt_load_bitmap(profile.banner_image, &banner_bmp,
                                      banner_data, sizeof(banner_data),
                                      YT_BANNER_W, YT_BANNER_H);
}

static void yt_invalidate_thumbnails(void)
{
    int slot;

    for (slot = 0; slot < YT_LIST_VISIBLE; slot++)
    {
        thumb_video[slot] = -1;
        thumb_valid[slot] = false;
    }
    thumb_next_slot = 0;
}

static int yt_load_thumbnail(int index)
{
    int slot;

    for (slot = 0; slot < YT_LIST_VISIBLE; slot++)
        if (index == thumb_video[slot])
            return slot;

    slot = thumb_next_slot;
    thumb_next_slot = (thumb_next_slot + 1) % YT_LIST_VISIBLE;
    thumb_video[slot] = index;
    thumb_valid[slot] = false;
    if (index < 0 || index >= video_count || !videos[index].thumb_path[0])
        return slot;
    thumb_valid[slot] =
        yt_load_bitmap(videos[index].thumb_path, &thumb_bmp[slot],
                       thumb_data[slot], sizeof(thumb_data[slot]),
                       YT_THUMB_W, YT_THUMB_H);
    return slot;
}

static void yt_puts_fit(int x, int y, int width, const char *text)
{
    char buffer[128];
    int text_width;
    int height;
    int length;

    rb->strlcpy(buffer, text ? text : "", sizeof(buffer));
    rb->lcd_getstringsize(buffer, &text_width, &height);
    length = rb->strlen(buffer);
    while (text_width > width && length > 3)
    {
        buffer[--length] = '\0';
        rb->lcd_getstringsize(buffer, &text_width, &height);
    }
    if (text && buffer[0] && rb->strlen(text) > rb->strlen(buffer) &&
        length > 3)
    {
        buffer[length - 3] = '.';
        buffer[length - 2] = '.';
        buffer[length - 1] = '.';
    }
    rb->lcd_putsxy(x, y, buffer);
}

static void yt_draw_logo(int x, int y, int width)
{
    if (logo_valid)
    {
        if (logo_bmp.width <= width)
            rb->lcd_bitmap((const fb_data *)logo_bmp.data, x, y,
                           logo_bmp.width, logo_bmp.height);
    }
    else
    {
        rb->lcd_set_foreground(YT_RED);
        rb->lcd_putsxy(x, y + 12, "YouTube");
    }
}

static void yt_draw_stars(int x, int y, int rating_x100)
{
    int fill = MAX(0, MIN(YT_STARS_W, rating_x100 * YT_STARS_W / 500));

    if (stars_valid)
    {
        rb->lcd_bitmap_transparent((const fb_data *)stars_bmp.data, x, y,
                                   stars_bmp.width, stars_bmp.height);
        if (stars_active_valid && fill > 0)
            rb->lcd_bitmap_transparent_part(
                (const fb_data *)stars_active_bmp.data, 0, 0,
                stars_active_bmp.width, x, y, fill,
                stars_active_bmp.height);
    }
    else
    {
        char rating[16];
        rb->snprintf(rating, sizeof(rating), "%d.%02d / 5",
                     rating_x100 / 100, rating_x100 % 100);
        rb->lcd_set_foreground(YT_RED);
        rb->lcd_putsxy(x, y, rating);
    }
}

static void yt_draw_header(void)
{
    static const char * const tabs[] = {
        "Home", "Videos", "Live", "Subs", "Profile"
    };
    int i;
    int tab_x[] = { 108, 140, 184, 216, 271 };
    int tab_w[] = { 30, 36, 24, 30, 42 };

    /* The native iPodJS menu and other plugins may leave a masked draw mode
     * active.  This application owns the whole LCD while it is running, so
     * establish the stock opaque canvas before every full-screen repaint. */
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    yt_draw_logo(7, 1, 106);
    rb->lcd_setfont(FONT_SYSFIXED);
    for (i = 0; i < YT_PAGE_COUNT; i++)
    {
        rb->lcd_set_foreground(i == (int)current_page ? YT_RED : YT_BLUE);
        rb->lcd_putsxy(tab_x[i], 20, tabs[i]);
        if (i == (int)current_page)
            rb->lcd_hline(tab_x[i], tab_x[i] + tab_w[i], 37);
    }
    rb->lcd_set_foreground(YT_MID_GRAY);
    rb->lcd_hline(0, LCD_WIDTH - 1, 42);
}

static int yt_page_count(enum yt_page page)
{
    if (page == YT_PAGE_HOME)
        return home_count;
    if (page == YT_PAGE_SUBSCRIPTIONS)
        return subscription_count;
    if (page == YT_PAGE_LIVE)
        return live_count;
    if (page == YT_PAGE_PROFILE)
        return profile.show_videos ? profile_count : 0;
    return filtered_video_count;
}

static int yt_page_video(enum yt_page page, int selected)
{
    if (page == YT_PAGE_HOME)
        return selected >= 0 && selected < home_count ?
               home_indices[selected] : -1;
    if (page == YT_PAGE_SUBSCRIPTIONS)
        return selected >= 0 && selected < subscription_count ?
               subscription_indices[selected] : -1;
    if (page == YT_PAGE_LIVE)
        return selected >= 0 && selected < live_count ?
               live_indices[selected] : -1;
    if (page == YT_PAGE_PROFILE)
        return profile.show_videos && selected >= 0 &&
               selected < profile_count ?
               profile_indices[selected] : -1;
    return selected >= 0 && selected < filtered_video_count ?
           video_indices[selected] : -1;
}

static const char *yt_filter_name(void)
{
    static const char * const names[] = {
        "Recently Added", "Most Viewed", "Top Rated", "Favorites",
        "Related Videos"
    };

    return names[video_filter];
}

static void yt_cycle_filter(void)
{
    if (video_filter == YT_FILTER_RELATED)
        video_filter = saved_video_filter;
    video_filter = (video_filter + 1) % YT_FILTER_RELATED;
    yt_build_indices();
    selection[YT_PAGE_VIDEOS] = 0;
}

static void yt_draw_empty(const char *title, const char *line1,
                          const char *line2)
{
    rb->lcd_set_foreground(YT_LIGHT_BLUE);
    rb->lcd_fillrect(10, 67, LCD_WIDTH - 20, 112);
    rb->lcd_set_foreground(YT_BLUE_BORDER);
    rb->lcd_drawrect(10, 67, LCD_WIDTH - 20, 112);
    rb->lcd_set_foreground(YT_BLUE);
    rb->lcd_putsxy(22, 82, title);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(22, 112, line1);
    rb->lcd_putsxy(22, 130, line2);
    rb->lcd_set_foreground(YT_BLUE);
    rb->lcd_putsxy(8, 222,
                   current_page == YT_PAGE_PROFILE ||
                   video_filter == YT_FILTER_RELATED ? "MENU Back" :
                                                       "MENU Exit");
    if (current_page == YT_PAGE_VIDEOS)
        rb->lcd_putsxy(126, 222, "HOLD SELECT Sort");
}

static int yt_list_first(int position, int count)
{
    int first;

    if (count <= YT_LIST_VISIBLE || position <= 0)
        return 0;

    first = position - 1;
    return MIN(first, count - YT_LIST_VISIBLE);
}

static void yt_draw_video_row(int video_index, int row, bool selected)
{
    struct yt_video *video = &videos[video_index];
    int y = YT_LIST_TOP + row * YT_LIST_ROW_H;
    bool channel_colors = current_page == YT_PAGE_PROFILE;
    int page_background = channel_colors ? profile.background_color :
                                           LCD_WHITE;
    int module_color = channel_colors ? profile.module_color : YT_LIGHT_BLUE;
    int link_color = channel_colors ? profile.link_color : YT_BLUE;
    int text_color = channel_colors ? profile.text_color : LCD_BLACK;
    int background = selected ? module_color : page_background;
    int thumb_slot;
    char line[96];

    thumb_slot = yt_load_thumbnail(video_index);
    rb->lcd_set_background(background);
    rb->lcd_set_foreground(background);
    rb->lcd_fillrect(2, y, LCD_WIDTH - 8, YT_LIST_ROW_H);
    rb->lcd_set_foreground(selected ? YT_BLUE_BORDER : YT_LIGHT_GRAY);
    rb->lcd_drawrect(2, y, LCD_WIDTH - 8, YT_LIST_ROW_H);
    if (selected)
    {
        rb->lcd_set_foreground(link_color);
        rb->lcd_fillrect(2, y, 4, YT_LIST_ROW_H);
    }

    if (thumb_valid[thumb_slot])
        rb->lcd_bitmap((const fb_data *)thumb_bmp[thumb_slot].data, 8, y + 3,
                       thumb_bmp[thumb_slot].width,
                       thumb_bmp[thumb_slot].height);
    else
    {
        rb->lcd_set_foreground(YT_LIGHT_GRAY);
        rb->lcd_fillrect(8, y + 3, YT_THUMB_W, YT_THUMB_H);
        rb->lcd_set_foreground(YT_MID_GRAY);
        rb->lcd_drawrect(8, y + 3, YT_THUMB_W, YT_THUMB_H);
        rb->lcd_putsxy(24, y + 32, "No image");
    }
    if (video->is_live)
    {
        rb->lcd_set_background(YT_RED);
        rb->lcd_set_foreground(YT_RED);
        rb->lcd_fillrect(12, y + 55, 34, 15);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_putsxy(15, y + 58, "LIVE");
        rb->lcd_set_background(background);
    }
    rb->lcd_set_foreground(link_color);
    yt_puts_fit(110, y + 5, 199, video->title);
    rb->lcd_set_foreground(text_color);
    rb->snprintf(line, sizeof(line), "from %s", video->uploader);
    yt_puts_fit(110, y + 22, 199, line);
    yt_draw_stars(110, y + 37, video->rating_x100);
    rb->lcd_set_foreground(YT_DARK_GRAY);
    yt_puts_fit(184, y + 39, 125, video->views);
    if (video->is_live)
        rb->snprintf(line, sizeof(line), "LIVE NOW  %s", video->uploader);
    else if (video->resume_seconds > 0)
        rb->snprintf(line, sizeof(line), "Resume %lu:%02lu  %s",
                     video->resume_seconds / 60,
                     video->resume_seconds % 60, video->duration);
    else
        rb->snprintf(line, sizeof(line), "%s  %s", video->duration,
                     video->upload_date);
    yt_puts_fit(110, y + 56, 199, line);
    rb->lcd_set_background(page_background);
}

static void yt_draw_scrollbar(int first, int count)
{
    const int x = LCD_WIDTH - 5;
    const int y = YT_LIST_TOP + 3;
    const int height = YT_LIST_VISIBLE * YT_LIST_ROW_H - 6;
    int thumb_height;
    int thumb_y;

    rb->lcd_set_foreground(YT_LIGHT_GRAY);
    rb->lcd_fillrect(x, y, 3, height);
    rb->lcd_set_foreground(YT_MID_GRAY);
    rb->lcd_drawrect(x, y, 3, height);

    if (count <= YT_LIST_VISIBLE)
    {
        thumb_height = height;
        thumb_y = y;
    }
    else
    {
        thumb_height = MAX(20, height * YT_LIST_VISIBLE / count);
        thumb_y = y + (height - thumb_height) * first /
                  (count - YT_LIST_VISIBLE);
    }
    rb->lcd_set_foreground(YT_DARK_GRAY);
    rb->lcd_fillrect(x, thumb_y, 3, thumb_height);
}

static void yt_draw_video_list(const char *section, int position, int count)
{
    int first = yt_list_first(position, count);
    int shown = MIN(YT_LIST_VISIBLE, count - first);
    int row;
    char line[40];
    char section_label[48];
    bool channel_colors = current_page == YT_PAGE_PROFILE;
    int module_color = channel_colors ? profile.module_color : YT_LIGHT_BLUE;
    int link_color = channel_colors ? profile.link_color : YT_BLUE;
    int page_background = channel_colors ? profile.background_color :
                                           LCD_WHITE;

    if (current_page == YT_PAGE_VIDEOS)
        rb->snprintf(section_label, sizeof(section_label), "Videos: %s",
                     yt_filter_name());
    else
        rb->strlcpy(section_label, section, sizeof(section_label));

    rb->lcd_set_foreground(page_background);
    rb->lcd_fillrect(0, 43, LCD_WIDTH, LCD_HEIGHT - 43);
    rb->lcd_set_foreground(module_color);
    rb->lcd_fillrect(4, 46, LCD_WIDTH - 8, 17);
    rb->lcd_set_foreground(YT_BLUE_BORDER);
    rb->lcd_drawrect(4, 46, LCD_WIDTH - 8, 17);
    rb->lcd_set_foreground(link_color);
    yt_puts_fit(9, 50, 232, section_label);
    rb->snprintf(line, sizeof(line), "%d-%d of %d",
                 first + 1, first + shown, count);
    rb->lcd_set_foreground(YT_DARK_GRAY);
    yt_puts_fit(250, 50, 62, line);

    for (row = 0; row < shown; row++)
    {
        int position_index = first + row;
        int video_index = yt_page_video(current_page, position_index);

        yt_draw_video_row(video_index, row, position_index == position);
    }
    yt_draw_scrollbar(first, count);
    rb->lcd_set_background(page_background);
    rb->lcd_set_foreground(link_color);
    rb->lcd_putsxy(8, 222,
                   current_page == YT_PAGE_PROFILE ||
                   video_filter == YT_FILTER_RELATED ? "MENU Back" :
                                                       "MENU Exit");
    if (current_page == YT_PAGE_VIDEOS)
        rb->lcd_putsxy(204, 222, "HOLD SEL Sort");
    else
        rb->lcd_putsxy(244, 222, "SELECT");
    rb->lcd_putsxy(112, 222, "PLAY Watch");
}

static const char *yt_profile_value(const char *value)
{
    return value && value[0] ? value : "-";
}

static void yt_draw_profile_field(int y, const char *label,
                                  const char *value)
{
    rb->lcd_set_foreground(profile.text_color);
    yt_puts_fit(8, y, 82, label);
    rb->lcd_set_foreground(profile.link_color);
    yt_puts_fit(92, y, LCD_WIDTH - 100, yt_profile_value(value));
}

static void yt_draw_profile(void)
{
    char line[128];

    if (profile_focus >= 2 && profile.show_videos && profile_count > 0)
    {
        selection[YT_PAGE_PROFILE] =
            MAX(0, MIN(profile_focus - 2, profile_count - 1));
        yt_draw_video_list("My Videos", selection[YT_PAGE_PROFILE],
                           profile_count);
        return;
    }

    rb->lcd_set_background(profile.background_color);
    rb->lcd_set_foreground(profile.background_color);
    rb->lcd_fillrect(0, 43, LCD_WIDTH, LCD_HEIGHT - 43);

    if (profile_focus == 0)
    {
        int module_y = banner_valid ? 107 : 48;
        int module_h = banner_valid ? 56 : 71;
        int picture_x = 10;
        int picture_y = banner_valid ? 111 : 52;
        int picture_size = banner_valid ? 48 : 64;

        if (banner_valid)
            rb->lcd_bitmap((const fb_data *)banner_bmp.data,
                           4, 47, YT_BANNER_W, YT_BANNER_H);
        rb->lcd_set_foreground(profile.module_color);
        rb->lcd_fillrect(4, module_y, LCD_WIDTH - 8, module_h);
        rb->lcd_set_foreground(YT_BLUE_BORDER);
        rb->lcd_drawrect(4, module_y, LCD_WIDTH - 8, module_h);
        if (profile_valid)
        {
            if (banner_valid)
                rb->lcd_bitmap_part((const fb_data *)profile_bmp.data,
                                    8, 8, 64, picture_x, picture_y,
                                    picture_size, picture_size);
            else
                rb->lcd_bitmap((const fb_data *)profile_bmp.data,
                               picture_x, picture_y, 64, 64);
        }
        else
        {
            rb->lcd_set_foreground(LCD_WHITE);
            rb->lcd_fillrect(picture_x, picture_y,
                             picture_size, picture_size);
            rb->lcd_set_foreground(YT_MID_GRAY);
            rb->lcd_drawrect(picture_x, picture_y,
                             picture_size, picture_size);
        }
        rb->lcd_set_foreground(profile.link_color);
        yt_puts_fit(banner_valid ? 66 : 82, banner_valid ? 110 : 52,
                    banner_valid ? 244 : 228,
                    profile.display_name[0] ? profile.display_name :
                                              profile.username);
        rb->lcd_set_foreground(profile.text_color);
        rb->snprintf(line, sizeof(line), "User: %s  Joined: %s",
                     profile.username, yt_profile_value(profile.joined));
        yt_puts_fit(banner_valid ? 66 : 82, banner_valid ? 126 : 70,
                    banner_valid ? 244 : 228, line);
        rb->snprintf(line, sizeof(line), "Channel: %lu  Video: %lu",
                     profile.channel_views, profile.video_views);
        yt_puts_fit(banner_valid ? 66 : 82, banner_valid ? 142 : 87,
                    banner_valid ? 244 : 228, line);
        rb->snprintf(line, sizeof(line), "Subscribers: %lu  Friends: %lu",
                     profile.subscribers, profile.friends);
        if (!banner_valid)
            rb->lcd_putsxy(82, 103, line);

        rb->lcd_set_foreground(profile.module_color);
        rb->lcd_fillrect(4, banner_valid ? 166 : 125,
                         LCD_WIDTH - 8, 18);
        rb->lcd_set_foreground(YT_BLUE_BORDER);
        rb->lcd_drawrect(4, banner_valid ? 166 : 125,
                         LCD_WIDTH - 8, 18);
        rb->lcd_set_foreground(profile.link_color);
        rb->lcd_putsxy(10, banner_valid ? 169 : 128, "About Me");
        rb->lcd_set_foreground(profile.text_color);
        yt_puts_fit(10, banner_valid ? 187 : 148, LCD_WIDTH - 20,
                    profile.show_about ? profile.about : "Private");
        rb->snprintf(line, sizeof(line), "%s%s%s", profile.city,
                     profile.city[0] && profile.country[0] ? ", " : "",
                     profile.country);
        if (!banner_valid)
            yt_puts_fit(10, 166, LCD_WIDTH - 20, line);

        rb->lcd_set_foreground(profile.module_color);
        rb->lcd_fillrect(4, banner_valid ? 207 : 187,
                         LCD_WIDTH - 8, banner_valid ? 14 : 18);
        rb->lcd_set_foreground(YT_BLUE_BORDER);
        rb->lcd_drawrect(4, banner_valid ? 207 : 187,
                         LCD_WIDTH - 8, banner_valid ? 14 : 18);
        rb->lcd_set_foreground(profile.link_color);
        rb->snprintf(line, sizeof(line), "Videos (%d)   Scroll for more >>",
                     profile_count);
        rb->lcd_putsxy(10, banner_valid ? 207 : 190, line);
    }
    else
    {
        rb->lcd_set_foreground(profile.module_color);
        rb->lcd_fillrect(4, 47, LCD_WIDTH - 8, 20);
        rb->lcd_set_foreground(YT_BLUE_BORDER);
        rb->lcd_drawrect(4, 47, LCD_WIDTH - 8, 20);
        rb->lcd_set_foreground(profile.link_color);
        rb->lcd_putsxy(10, 51, "Member Details");
        yt_draw_profile_field(73, "Joined:", profile.joined);
        yt_draw_profile_field(91, "Website:", profile.website);
        yt_draw_profile_field(109, "Occupation:", profile.occupation);
        yt_draw_profile_field(127, "Interests:", profile.interests);
        yt_draw_profile_field(145, "Music:", profile.music);
        yt_draw_profile_field(163, "Movies:", profile.movies);
        yt_draw_profile_field(181, "Books:", profile.books);
        rb->lcd_set_foreground(profile.module_color);
        rb->lcd_fillrect(4, 203, LCD_WIDTH - 8, 16);
        rb->lcd_set_foreground(profile.link_color);
        rb->snprintf(line, sizeof(line), "My Videos (%d)  scroll down >>",
                     profile_count);
        rb->lcd_putsxy(10, 205, line);
    }

    rb->lcd_set_foreground(profile.link_color);
    rb->lcd_putsxy(8, 224, profile_focus == 0 ? "MENU Exit" : "MENU Back");
    rb->lcd_putsxy(198, 224, "WHEEL Scroll");
}

static void yt_draw_browser(void)
{
    int count;

    yt_draw_header();
    count = yt_page_count(current_page);
    if (current_page == YT_PAGE_PROFILE)
    {
        yt_draw_profile();
        rb->lcd_update();
        return;
    }
    if (count <= 0)
    {
        if (current_page == YT_PAGE_VIDEOS &&
            video_filter == YT_FILTER_FAVORITES)
            yt_draw_empty("No Favorites Yet",
                          "Open a video and choose Favorite.",
                          "Hold SELECT to change the view.");
        else if (current_page == YT_PAGE_LIVE)
            yt_draw_empty("Nothing Live Now",
                          "Add a live video in RockPod",
                          "then sync it to this iPod.");
        else
            yt_draw_empty(current_page == YT_PAGE_HOME ?
                          "No Videos Synced" :
                          "Your video library is empty",
                          "Connect your iPod to RockPod",
                          "to add YouTube videos.");
        rb->lcd_update();
        return;
    }
    selection[current_page] = MAX(0, MIN(selection[current_page], count - 1));
    yt_draw_video_list(current_page == YT_PAGE_HOME ?
                       "Featured Videos" :
                       current_page == YT_PAGE_SUBSCRIPTIONS ?
                       "Subscription Uploads" :
                       current_page == YT_PAGE_LIVE ?
                       "Live Now" : "Videos",
                       selection[current_page], count);
    rb->lcd_update();
}

static void yt_draw_button(int x, int y, int width, const char *label,
                           bool selected)
{
    int text_width;
    int height;
    int background = selected ? YT_BLUE : YT_LIGHT_GRAY;

    rb->lcd_set_background(background);
    rb->lcd_set_foreground(background);
    rb->lcd_fillrect(x, y, width, 20);
    rb->lcd_set_foreground(selected ? YT_BLUE : YT_MID_GRAY);
    rb->lcd_drawrect(x, y, width, 20);
    rb->lcd_set_foreground(selected ? LCD_WHITE : LCD_BLACK);
    rb->lcd_getstringsize(label, &text_width, &height);
    rb->lcd_putsxy(x + (width - text_width) / 2, y + 5, label);
    rb->lcd_set_background(LCD_WHITE);
}

static void yt_draw_detail(void)
{
    struct yt_video *video = &videos[detail_video];
    int thumb_slot;
    char line[120];
    char rate_label[24];

    yt_draw_header();
    thumb_slot = yt_load_thumbnail(detail_video);
    rb->lcd_set_foreground(YT_BLUE);
    yt_puts_fit(6, 48, LCD_WIDTH - 12, video->title);
    if (thumb_valid[thumb_slot])
        rb->lcd_bitmap((const fb_data *)thumb_bmp[thumb_slot].data, 6, 67,
                       thumb_bmp[thumb_slot].width,
                       thumb_bmp[thumb_slot].height);
    else
    {
        rb->lcd_set_foreground(YT_LIGHT_GRAY);
        rb->lcd_fillrect(6, 67, YT_THUMB_W, YT_THUMB_H);
        rb->lcd_set_foreground(YT_MID_GRAY);
        rb->lcd_drawrect(6, 67, YT_THUMB_W, YT_THUMB_H);
    }
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(111, 67, "From:");
    rb->lcd_set_foreground(YT_BLUE);
    yt_puts_fit(111, 84, 200, video->uploader);
    yt_draw_stars(111, 104, video->rating_x100);
    rb->snprintf(line, sizeof(line), "%d.%02d  (%d ratings)",
                 video->rating_x100 / 100, video->rating_x100 % 100,
                 video->rating_count);
    rb->lcd_set_foreground(YT_DARK_GRAY);
    yt_puts_fit(184, 106, 128, line);
    yt_puts_fit(111, 123, 200, video->views);
    if (video->is_live)
    {
        rb->lcd_set_background(YT_RED);
        rb->lcd_set_foreground(YT_RED);
        rb->lcd_fillrect(111, 123, 38, 15);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_putsxy(115, 126, "LIVE");
        rb->lcd_set_background(LCD_WHITE);
    }

    rb->lcd_set_foreground(YT_LIGHT_BLUE);
    rb->lcd_fillrect(6, 145, LCD_WIDTH - 12, 48);
    rb->lcd_set_foreground(YT_BLUE_BORDER);
    rb->lcd_drawrect(6, 145, LCD_WIDTH - 12, 48);
    rb->lcd_set_foreground(LCD_BLACK);
    if (video->is_live)
        rb->snprintf(line, sizeof(line), "LIVE NOW  from %s", video->uploader);
    else
        rb->snprintf(line, sizeof(line), "Added: %s  %s", video->upload_date,
                     video->category);
    yt_puts_fit(11, 151, LCD_WIDTH - 22, line);
    yt_puts_fit(11, 169, LCD_WIDTH - 22, video->description);

    yt_draw_button(4, 200, 76,
                   video->is_live ? "Watch Live" :
                   video->resume_seconds > 0 ? "Resume" : "Watch",
                   detail_action == 0);
    yt_draw_button(83, 200, 76,
                   video->favorite ? "Favorited" : "Favorite",
                   detail_action == 1);
    rb->snprintf(rate_label, sizeof(rate_label), "Rate: %d",
                 video->my_rating);
    yt_draw_button(162, 200, 73, rate_label, detail_action == 2);
    yt_draw_button(238, 200, 78, "Related", detail_action == 3);
    rb->lcd_set_foreground(YT_BLUE);
    rb->lcd_putsxy(6, 224, "MENU Back");
    rb->lcd_putsxy(72, 224, "<< Prev");
    rb->lcd_putsxy(132, 224, "PLAY Watch");
    rb->lcd_putsxy(244, 224, "Next >>");
    rb->lcd_update();
}

static void yt_draw(void)
{
    if (current_screen == YT_SCREEN_DETAIL && detail_video >= 0)
        yt_draw_detail();
    else
        yt_draw_browser();
}

static void yt_select_delta(int delta)
{
    int count = yt_page_count(current_page);
    int next;

    if (count <= 0)
        return;
    next = selection[current_page] + delta;
    selection[current_page] = MAX(0, MIN(next, count - 1));
}

static void yt_profile_delta(int delta)
{
    int maximum = 1;

    if (profile.show_videos && profile_count > 0)
        maximum = profile_count + 1;
    profile_focus = MAX(0, MIN(profile_focus + delta, maximum));
    if (profile_focus >= 2)
        selection[YT_PAGE_PROFILE] = profile_focus - 2;
}

static void yt_detail_delta(int delta)
{
    int count = yt_page_count(current_page);
    int position = selection[current_page];
    int next;
    int i;

    if (count <= 0)
        return;
    if (position < 0 || position >= count ||
        yt_page_video(current_page, position) != detail_video)
    {
        position = 0;
        for (i = 0; i < count; i++)
            if (yt_page_video(current_page, i) == detail_video)
            {
                position = i;
                break;
            }
    }
    next = MAX(0, MIN(position + delta, count - 1));
    if (next == position)
        return;
    selection[current_page] = next;
    detail_video = yt_page_video(current_page, next);
    detail_action = 0;
}

static int yt_find_by_path(const char *path)
{
    int i;

    for (i = 0; i < video_count; i++)
        if (!rb->strcmp(videos[i].video_path, path))
            return i;
    return -1;
}

static bool yt_resolve_video_path(const char *path, char *resolved,
                                 size_t resolved_size)
{
    char base[MAX_PATH];
    char *dot;
    static const char *exts[] = {".mpg", ".m4v", ".m4a", ".mp4", ".mov", NULL};
    size_t i;

    rb->strlcpy(base, path, sizeof(base));
    rb->strlcpy(resolved, path, resolved_size);

    if (rb->file_exists(resolved))
        return true;

    dot = rb->strrchr(base, '.');
    if (dot == NULL)
        return false;

    for (i = 0; exts[i] != NULL; i++)
    {
        if (!rb->strcasecmp(dot, exts[i]))
            continue;
        rb->strcpy(dot, exts[i]);
        rb->strlcpy(resolved, base, resolved_size);
        if (rb->file_exists(resolved))
            return true;
    }

    return false;
}

static enum plugin_status yt_play_video(void)
{
    static char launch[MAX_PATH + 24];
    char path[MAX_PATH];
    struct yt_video *video = &videos[detail_video];

    if (!yt_resolve_video_path(video->video_path, path, sizeof(path)))
    {
        rb->splash(HZ * 2, "Video Not Found");
        return PLUGIN_OK;
    }
    if (video->is_live)
        rb->snprintf(launch, sizeof(launch), "%s%lu:%s",
                     YT_LIVE_PLAYER_PREFIX, video->live_start_epoch,
                     path);
    else
        rb->snprintf(launch, sizeof(launch), "%s%s", YT_PLAYER_PREFIX,
                     path);
    return rb->plugin_open(plugin_video_player_for(path), launch);
}

static enum plugin_status yt_activate(void)
{
    if (current_screen == YT_SCREEN_BROWSER)
    {
        if (current_page == YT_PAGE_PROFILE && profile_focus < 2)
        {
            yt_profile_delta(1);
            return PLUGIN_OK;
        }
        int index = yt_page_video(current_page, selection[current_page]);

        if (index >= 0)
        {
            detail_video = index;
            detail_action = 0;
            current_screen = YT_SCREEN_DETAIL;
        }
        return PLUGIN_OK;
    }

    if (detail_action == 0)
        return yt_play_video();
    if (detail_action == 1)
        videos[detail_video].favorite = !videos[detail_video].favorite;
    else if (detail_action == 2)
    {
        videos[detail_video].my_rating++;
        if (videos[detail_video].my_rating > 5)
            videos[detail_video].my_rating = 0;
    }
    else
    {
        if (video_filter != YT_FILTER_RELATED)
        {
            saved_video_filter = video_filter;
            related_return_page = current_page;
        }
        related_anchor = detail_video;
        video_filter = YT_FILTER_RELATED;
        current_page = YT_PAGE_VIDEOS;
        current_screen = YT_SCREEN_BROWSER;
        selection[YT_PAGE_VIDEOS] = 0;
        yt_build_indices();
        return PLUGIN_OK;
    }
    yt_save_state();
    yt_build_indices();
    return PLUGIN_OK;
}

static void yt_splash(void)
{
    int ticks = HZ / 2;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    yt_draw_logo((LCD_WIDTH - YT_LOGO_W) / 2,
                 (LCD_HEIGHT - YT_LOGO_H) / 2 - 5, YT_LOGO_W);
    rb->lcd_set_foreground(YT_DARK_GRAY);
    rb->lcd_putsxy(118, 172, "Loading...");
    rb->lcd_update();
    rb->sleep(ticks);
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status = PLUGIN_OK;
    bool returned = false;
    bool redraw = true;
    bool select_held = false;

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_RGB565)
    /* A video plugin interrupted while the hardware is in YUV mode must not
     * tint the browser framebuffer. Reassert the stock RGB UI mode first. */
    rb->lcd_set_mode(LCD_MODE_RGB565);
#endif
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_backdrop(NULL);
    video_filter = YT_FILTER_RECENT;
    saved_video_filter = YT_FILTER_RECENT;
    related_anchor = -1;
    yt_load_library();
    yt_load_profile();
    yt_load_state();
    yt_load_resume_positions();
    yt_build_indices();
    yt_load_assets();
    current_page = YT_PAGE_HOME;
    current_screen = YT_SCREEN_BROWSER;
    selection[YT_PAGE_HOME] = 0;
    selection[YT_PAGE_VIDEOS] = 0;
    selection[YT_PAGE_LIVE] = 0;
    selection[YT_PAGE_SUBSCRIPTIONS] = 0;
    selection[YT_PAGE_PROFILE] = 0;
    profile_focus = 0;
    detail_video = -1;
    yt_invalidate_thumbnails();

    if (parameter != NULL &&
        !rb->strncmp((const char *)parameter, "return:", 7))
    {
        detail_video = yt_find_by_path((const char *)parameter + 7);
        if (detail_video >= 0)
        {
            current_screen = YT_SCREEN_DETAIL;
            current_page = videos[detail_video].is_live ? YT_PAGE_LIVE :
                           videos[detail_video].subscription ?
                           YT_PAGE_SUBSCRIPTIONS : YT_PAGE_VIDEOS;
            {
                int i;
                int count = yt_page_count(current_page);

                selection[current_page] = 0;
                for (i = 0; i < count; i++)
                    if (yt_page_video(current_page, i) == detail_video)
                    {
                        selection[current_page] = i;
                        break;
                    }
            }
            returned = true;
        }
    }
    if (!returned)
        yt_splash();

    running = true;
    while (running)
    {
        int action;

        if (redraw)
        {
            yt_draw();
            redraw = false;
        }
        action = pluginlib_getaction_remote(HZ / 5, plugin_contexts,
                                     ARRAYLEN(plugin_contexts));
        switch (action)
        {
#ifdef HAVE_SCROLLWHEEL
        case PLA_SCROLL_BACK:
        case PLA_SCROLL_BACK_REPEAT:
            if (current_screen == YT_SCREEN_DETAIL)
                detail_action = (detail_action + 3) % 4;
            else if (current_page == YT_PAGE_PROFILE)
                yt_profile_delta(-1);
            else
                yt_select_delta(-1);
            redraw = true;
            break;
        case PLA_SCROLL_FWD:
        case PLA_SCROLL_FWD_REPEAT:
            if (current_screen == YT_SCREEN_DETAIL)
                detail_action = (detail_action + 1) % 4;
            else if (current_page == YT_PAGE_PROFILE)
                yt_profile_delta(1);
            else
                yt_select_delta(1);
            redraw = true;
            break;
#endif
        case PLA_UP:
        case PLA_UP_REPEAT:
            if (current_screen == YT_SCREEN_DETAIL)
                detail_action = (detail_action + 3) % 4;
            else if (current_page == YT_PAGE_PROFILE)
                yt_profile_delta(-1);
            else
                yt_select_delta(-1);
            redraw = true;
            break;
        case PLA_DOWN:
        case PLA_DOWN_REPEAT:
            if (current_screen == YT_SCREEN_DETAIL)
                detail_action = (detail_action + 1) % 4;
            else if (current_page == YT_PAGE_PROFILE)
                yt_profile_delta(1);
            else
                yt_select_delta(1);
            redraw = true;
            break;
        case PLA_LEFT:
        case PLA_LEFT_REPEAT:
            if (current_screen == YT_SCREEN_DETAIL)
                yt_detail_delta(-1);
            else
            {
                current_page = (current_page + YT_PAGE_COUNT - 1) %
                               YT_PAGE_COUNT;
            }
            redraw = true;
            break;
        case PLA_RIGHT:
        case PLA_RIGHT_REPEAT:
            if (current_screen == YT_SCREEN_DETAIL)
                yt_detail_delta(1);
            else
            {
                current_page = (current_page + 1) % YT_PAGE_COUNT;
            }
            redraw = true;
            break;
        case PLA_SELECT_REPEAT:
            if (!select_held && current_screen == YT_SCREEN_BROWSER &&
                current_page == YT_PAGE_VIDEOS)
            {
                yt_cycle_filter();
                redraw = true;
            }
            select_held = true;
            break;
        case PLA_SELECT_REL:
        {
            enum plugin_status result;

            if (select_held)
            {
                select_held = false;
                redraw = true;
                break;
            }
            result = yt_activate();

            if (result == PLUGIN_GOTO_PLUGIN)
                return result;
            redraw = true;
            break;
        }
        case PLA_EXIT:
        {
            enum plugin_status result = PLUGIN_OK;

            if (current_screen == YT_SCREEN_DETAIL)
            {
                detail_action = 0;
                result = yt_activate();
            }
            else
            {
                int index = current_page == YT_PAGE_PROFILE &&
                            profile_focus < 2 ? -1 :
                            yt_page_video(current_page,
                                          selection[current_page]);

                if (index >= 0)
                {
                    detail_video = index;
                    detail_action = 0;
                    result = yt_play_video();
                }
            }
            if (result == PLUGIN_GOTO_PLUGIN)
                return result;
            redraw = true;
            break;
        }
        case ACTION_STD_MENU:
            running = false;
            break;
        case PLA_CANCEL:
            if (current_screen == YT_SCREEN_DETAIL)
            {
                current_screen = YT_SCREEN_BROWSER;
                detail_video = -1;
                redraw = true;
            }
            else if (video_filter == YT_FILTER_RELATED &&
                     current_page == YT_PAGE_VIDEOS &&
                     related_anchor >= 0)
            {
                video_filter = saved_video_filter;
                yt_build_indices();
                current_page = related_return_page;
                current_screen = YT_SCREEN_DETAIL;
                detail_video = related_anchor;
                detail_action = 3;
                redraw = true;
            }
            else if (current_page == YT_PAGE_PROFILE && profile_focus > 0)
            {
                profile_focus = profile_focus >= 2 ? 1 : 0;
                redraw = true;
            }
            else
                running = false;
            break;
        default:
            if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            {
                status = PLUGIN_USB_CONNECTED;
                running = false;
            }
            break;
        }
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(rb->global_settings->bg_color);
    rb->lcd_set_foreground(rb->global_settings->fg_color);
    rb->lcd_setfont(FONT_UI);
    return status;
}
