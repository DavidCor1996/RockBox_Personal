/***************************************************************************
 * Standalone offline Instagram profile browser for RockPod-synchronised media.
 ***************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "pluginbitmaps/instagram_heart.h"
#include "pluginbitmaps/instagram_heart_unliked.h"
#include "pluginbitmaps/instagram_verified.h"

#if !defined(HAVE_LCD_COLOR) || LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error The Instagram application requires a 320x240 colour display
#endif

#define IG_ROOT         ROCKBOX_DIR "/instagram"
#define IG_LIBRARY      IG_ROOT "/library.tsv"
#define IG_PROFILES     IG_ROOT "/profiles.tsv"
#define IG_LIKES        IG_ROOT "/likes.tsv"
#define IG_LIKES_TMP    IG_ROOT "/likes.tmp"
#define IG_FAVORITES    IG_ROOT "/favorites.tsv"
#define IG_FAVORITES_TMP IG_ROOT "/favorites.tmp"
#define IG_STATE        IG_ROOT "/state.cfg"
#define IG_STATE_TMP    IG_ROOT "/state.tmp"
#define IG_LOGO         IG_ROOT "/assets/instagram-logo.bmp"
#define IG_LAUNCH       IG_ROOT "/assets/instagram-launch-2010.bmp"
#define IG_ZOOM_ROOT    IG_ROOT "/zoom"
#define IG_PROFILE_FEED IG_ROOT "/profile-feed"
#define IG_PLAYER       VIEWERS_DIR "/mpegplayer.rock"
#define IG_PREFIX       "instagram-app:"
#define IG_FEED_PREFIX  "instagram-feed:"
#define IG_MAX_POSTS    96
#define IG_MAX_PROFILES 16
#define IG_MAX_LIKES    256
#define IG_LINE_SIZE    1024
/* Keep capacity for already-synced 96x72 thumbnails; new 2010-style syncs
 * decode square 72x72 art into the same bounded buffer. */
#define IG_THUMB_W      96
#define IG_THUMB_H      72
#define IG_AVATAR_W     40
#define IG_AVATAR_H     40
#define IG_FEED_W       160
#define IG_FEED_H       160
#define IG_VIEW_W       480
#define IG_VIEW_H       300
#define IG_ZOOM_LEVELS  2
#define IG_BMP_SCRATCH_ELEMS(w) \
    ((((w) * 4 + 8) + (int)sizeof(fb_data) - 1) / (int)sizeof(fb_data))
#define IG_BLUE         LCD_RGBPACK(0x3f, 0x72, 0x96)
#define IG_NAVY         LCD_RGBPACK(0x2b, 0x4f, 0x6b)
#define IG_LIGHT_BLUE   LCD_RGBPACK(0xe7, 0xee, 0xf3)
#define IG_CREAM        LCD_RGBPACK(0xf7, 0xf5, 0xef)
#define IG_GRAY         LCD_RGBPACK(0x76, 0x76, 0x76)
#define IG_BORDER       LCD_RGBPACK(0xd5, 0xd9, 0xdc)
#define IG_SELECTED     LCD_RGBPACK(0xf1, 0xfa, 0xfe)

enum
{
    IG_ACTION_LIKE = LAST_PLUGINLIB_ACTION + 1,
};

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping ig_context[] =
{
    { PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK,               BUTTON_NONE },
    { PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD,                BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_SELECT_REL,         BUTTON_SELECT|BUTTON_REL,         BUTTON_NONE },
    { PLA_SELECT_REPEAT,      BUTTON_SELECT|BUTTON_REPEAT,      BUTTON_NONE },
    { PLA_LEFT,               BUTTON_LEFT,                      BUTTON_NONE },
    { PLA_RIGHT,              BUTTON_RIGHT,                     BUTTON_NONE },
    { PLA_LEFT_REPEAT,        BUTTON_LEFT|BUTTON_REPEAT,        BUTTON_NONE },
    { PLA_RIGHT_REPEAT,       BUTTON_RIGHT|BUTTON_REPEAT,       BUTTON_NONE },
    { PLA_CANCEL,             BUTTON_MENU,                      BUTTON_NONE },
    { IG_ACTION_LIKE,         BUTTON_PLAY|BUTTON_REL,           BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *contexts[] = { ig_context };
#else
static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

struct ig_post
{
    char id[32];
    char group_id[32];
    char username[64];
    char type[8];
    char title[80];
    char caption[184];
    char media[MAX_PATH];
    /* Device-generated path is bounded to /.rockbox/instagram/autoplay/<id>.mpg.
     * Keeping it compact avoids spending another 25 KiB across 96 posts. */
    char autoplay[96];
    char thumbnail[MAX_PATH];
    char display[MAX_PATH];
    int likes;
    bool liked;
    int group_index;
    int group_count;
    char post_date[20];
};

struct ig_profile
{
    char username[64];
    char display_name[80];
    char bio[184];
    char avatar[MAX_PATH];
    char cover[MAX_PATH];
    char name_art[MAX_PATH];
    char bio_art[MAX_PATH];
    int posts;
    int photos;
    int videos;
    bool is_my_profile;
    int followers;
    int following;
    bool verified;
    int synced_posts;
};

static struct ig_post posts[IG_MAX_POSTS];
static struct ig_profile profiles[IG_MAX_PROFILES];
static int post_count;
static int profile_count;
static int profile_selection;
static int active_profile = -1;
static int visible_posts[IG_MAX_POSTS];
static int visible_count;
static int selection;
static int home_selection;
static int profile_post_offset;
static int profile_post_total;
static int home_post_total;
static char liked_ids[IG_MAX_LIKES][32];
static int liked_count;
static char favorite_users[IG_MAX_PROFILES][64];
static int favorite_count;
static int favorite_selection;

enum ig_screen
{
    IG_SCREEN_HOME = 0,
    IG_SCREEN_FAVORITES,
    IG_SCREEN_PROFILES,
    IG_SCREEN_PROFILE,
    IG_SCREEN_VIEWER,
};

/* Bounded BSS: three grid thumbnails (41,472 bytes), one 160px feed image
 * (51,200 bytes), one 480x300 viewer image plus decoder scratch (289,928
 * bytes), logo/avatar buffers (6,400 bytes), host-rasterized detail text
 * (25,620 bytes), and five visible profile-name rows (36,900 bytes). Draw
 * functions perform no I/O, decoding, allocation, font loading, or
 * playback-memory ownership changes. */
static fb_data thumb_pixels[3][IG_THUMB_W * IG_THUMB_H];
static struct bitmap thumb_bm[3];
static int thumb_index[3] = { -1, -1, -1 };
static fb_data feed_pixels[IG_FEED_W * IG_FEED_H];
static struct bitmap feed_bm;
static bool feed_loaded;
static fb_data viewer_pixels[IG_VIEW_W * IG_VIEW_H +
                             IG_BMP_SCRATCH_ELEMS(IG_VIEW_W)];
static struct bitmap viewer_bm;
static bool viewer_loaded;
static int viewer_zoom;
static int viewer_post_index = -1;
enum ig_viewer_pan
{
    IG_PAN_CENTER = 0,
    IG_PAN_UP,
    IG_PAN_RIGHT,
    IG_PAN_DOWN,
    IG_PAN_LEFT,
};
static int viewer_pan;
static fb_data logo_pixels[40 * 40];
static struct bitmap logo_bm;
static bool logo_loaded;
static fb_data avatar_pixels[IG_AVATAR_W * IG_AVATAR_H];
static struct bitmap avatar_bm;
static bool avatar_loaded;
static fb_data name_pixels[205 * 18];
static struct bitmap name_bm;
static bool name_loaded;
#define IG_PROFILE_LIST_ROWS 5
static fb_data list_name_pixels[IG_PROFILE_LIST_ROWS][205 * 18];
static struct bitmap list_name_bm[IG_PROFILE_LIST_ROWS];
static int list_name_profile[IG_PROFILE_LIST_ROWS] = { -1, -1, -1, -1, -1 };
static fb_data bio_pixels[304 * 30];
static struct bitmap bio_bm;
static bool bio_loaded;
static int avatar_profile_index = -1;
static int profile_art_index = -1;
static enum ig_screen viewer_return_screen = IG_SCREEN_HOME;
static enum ig_screen profile_return_screen = IG_SCREEN_PROFILES;

static struct ig_post *ig_current_post(void);
static void ig_draw_nav(const char *title, const char *right);

static int ig_split(char *line, char **fields, int count)
{
    int found = 1;
    char *cursor = line;
    char *end = line;

    while (*end && *end != '\r' && *end != '\n')
        end++;
    *end = '\0';

    fields[0] = line;
    while (*cursor && found < count)
    {
        if (*cursor == '\t')
        {
            *cursor = '\0';
            fields[found++] = cursor + 1;
        }
        cursor++;
    }
    while (found < count)
        fields[found++] = end;
    return found;
}

static int ig_liked_index(const char *id)
{
    int index;

    for (index = 0; index < liked_count; index++)
        if (!rb->strcmp(liked_ids[index], id))
            return index;
    return -1;
}

static void ig_load_likes(void)
{
    char line[64];
    int fd = rb->open(IG_LIKES, O_RDONLY);

    liked_count = 0;
    if (fd < 0)
        return;
    while (liked_count < IG_MAX_LIKES &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *end = line;

        while (*end && *end != '\r' && *end != '\n')
            end++;
        *end = '\0';
        if (line[0] && ig_liked_index(line) < 0)
            rb->strlcpy(liked_ids[liked_count++], line,
                        sizeof(liked_ids[0]));
    }
    rb->close(fd);
}

static bool ig_save_likes(void)
{
    int index;
    int fd = rb->open(IG_LIKES_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        return false;
    for (index = 0; index < liked_count; index++)
        rb->fdprintf(fd, "%s\n", liked_ids[index]);
    rb->close(fd);
    rb->remove(IG_LIKES);
    if (rb->rename(IG_LIKES_TMP, IG_LIKES) < 0)
    {
        rb->remove(IG_LIKES_TMP);
        return false;
    }
    return true;
}

static int ig_favorite_index(const char *username)
{
    int index;

    for (index = 0; index < favorite_count; index++)
        if (!rb->strcmp(favorite_users[index], username))
            return index;
    return -1;
}

static void ig_load_favorites(void)
{
    char line[72];
    int fd = rb->open(IG_FAVORITES, O_RDONLY);

    favorite_count = 0;
    if (fd < 0)
        return;
    while (favorite_count < IG_MAX_PROFILES &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        int end = 0;

        while (line[end] && line[end] != '\r' && line[end] != '\n')
            end++;
        line[end] = '\0';
        if (line[0] && ig_favorite_index(line) < 0)
            rb->strlcpy(favorite_users[favorite_count++], line,
                        sizeof(favorite_users[0]));
    }
    rb->close(fd);
}

static bool ig_save_favorites(void)
{
    int index;
    int fd = rb->open(IG_FAVORITES_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        return false;
    for (index = 0; index < favorite_count; index++)
        rb->fdprintf(fd, "%s\n", favorite_users[index]);
    rb->close(fd);
    rb->remove(IG_FAVORITES);
    if (rb->rename(IG_FAVORITES_TMP, IG_FAVORITES) < 0)
    {
        rb->remove(IG_FAVORITES_TMP);
        return false;
    }
    return true;
}

static bool ig_toggle_profile_favorite(int profile_index)
{
    const char *username;
    int index;

    if (profile_index < 0 || profile_index >= profile_count)
        return false;
    username = profiles[profile_index].username;
    index = ig_favorite_index(username);
    if (index >= 0)
    {
        for (; index + 1 < favorite_count; index++)
            rb->strlcpy(favorite_users[index], favorite_users[index + 1],
                        sizeof(favorite_users[index]));
        favorite_count--;
    }
    else
    {
        if (favorite_count >= IG_MAX_PROFILES)
            return false;
        rb->strlcpy(favorite_users[favorite_count++], username,
                    sizeof(favorite_users[0]));
    }
    if (favorite_selection >= favorite_count)
        favorite_selection = MAX(0, favorite_count - 1);
    return ig_save_favorites();
}

static int ig_favorite_profile_at(int ordinal)
{
    int index;
    int found = 0;

    for (index = 0; index < profile_count; index++)
    {
        if (ig_favorite_index(profiles[index].username) < 0)
            continue;
        if (found++ == ordinal)
            return index;
    }
    return -1;
}

static int ig_favorite_profile_count(void)
{
    int index;
    int count = 0;

    for (index = 0; index < profile_count; index++)
        if (ig_favorite_index(profiles[index].username) >= 0)
            count++;
    return count;
}

static bool ig_save_home_state(void)
{
    struct ig_post *post = ig_current_post();
    int fd;

    if (!post)
        return false;
    fd = rb->open(IG_STATE_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    /* The id is stable even when a sync reorders a carousel group. Keep the
     * group id as a compatible fallback for state files from older builds. */
    rb->fdprintf(fd, "%s\t%s\n", post->id, post->group_id);
    rb->close(fd);
    rb->remove(IG_STATE);
    if (rb->rename(IG_STATE_TMP, IG_STATE) < 0)
    {
        rb->remove(IG_STATE_TMP);
        return false;
    }
    return true;
}

static void ig_restore_home_state(void)
{
    char state[80];
    char *group_id = NULL;
    int fd = rb->open(IG_STATE, O_RDONLY);
    int index;

    if (fd < 0)
        return;
    if (rb->read_line(fd, state, sizeof(state)) <= 0)
    {
        rb->close(fd);
        return;
    }
    rb->close(fd);
    for (index = 0; state[index] && state[index] != '\r' &&
         state[index] != '\n' && state[index] != '\t'; index++)
        ;
    if (state[index] == '\t')
    {
        int group_len = 0;

        state[index++] = '\0';
        group_id = &state[index];
        while (group_id[group_len] && group_id[group_len] != '\r' &&
               group_id[group_len] != '\n')
            group_len++;
        group_id[group_len] = '\0';
    }
    else
        state[index] = '\0';
    for (index = 0; index < visible_count; index++)
    {
        struct ig_post *post = &posts[visible_posts[index]];

        if (!rb->strcmp(post->id, state) ||
            (group_id && !rb->strcmp(post->group_id, group_id)) ||
            (!group_id && !rb->strcmp(post->group_id, state)))
        {
            selection = index;
            home_selection = index;
            return;
        }
    }
}

static void ig_copy_post(struct ig_post *post, char **field)
{
    rb->memset(post, 0, sizeof(*post));
    rb->strlcpy(post->id, field[0], sizeof(post->id));
    rb->strlcpy(post->username, field[1], sizeof(post->username));
    rb->strlcpy(post->type, field[2], sizeof(post->type));
    rb->strlcpy(post->title, field[3], sizeof(post->title));
    rb->strlcpy(post->caption, field[4], sizeof(post->caption));
    rb->strlcpy(post->media, field[5], sizeof(post->media));
    rb->strlcpy(post->autoplay, field[14], sizeof(post->autoplay));
    rb->strlcpy(post->thumbnail, field[6], sizeof(post->thumbnail));
    rb->strlcpy(post->display, field[7], sizeof(post->display));
    post->likes = rb->atoi(field[8]);
    rb->strlcpy(post->post_date, field[9], sizeof(post->post_date));
    rb->strlcpy(post->group_id, field[11], sizeof(post->group_id));
    if (!post->group_id[0])
        rb->strlcpy(post->group_id, post->id, sizeof(post->group_id));
    post->group_index = MAX(1, rb->atoi(field[12]));
    post->group_count = MAX(post->group_index, rb->atoi(field[13]));
    post->liked = ig_liked_index(post->group_id) >= 0;
}

static bool ig_post_newer(const struct ig_post *left,
                          const struct ig_post *right);

static bool ig_profile_feed_path(int index, char *path, size_t size)
{
    const char *cursor;

    if (index < 0 || index >= profile_count)
        return false;
    for (cursor = profiles[index].username; *cursor; cursor++)
    {
        if (!((*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '.'))
            return false;
    }
    return rb->snprintf(path, size, IG_PROFILE_FEED "/%s.tsv",
                        profiles[index].username) < (int)size;
}

static void ig_load_profile_page(int index, int offset)
{
    char line[IG_LINE_SIZE];
    char path[MAX_PATH];
    int fd = -1;
    int group_ordinal = -1;
    bool include_group = false;
    bool dedicated = false;
    int slot;

    post_count = 0;
    visible_count = 0;
    profile_post_offset = MAX(0, offset);
    profile_post_total = index >= 0 && index < profile_count ?
                         profiles[index].synced_posts : 0;
    thumb_index[0] = -1;
    thumb_index[1] = -1;
    thumb_index[2] = -1;
    if (index < 0 || index >= profile_count)
        return;
    if (ig_profile_feed_path(index, path, sizeof(path)))
    {
        fd = rb->open(path, O_RDONLY);
        dedicated = fd >= 0;
    }
    if (fd < 0)
        fd = rb->open(IG_LIBRARY, O_RDONLY);
    if (fd < 0)
        return;
    if (!dedicated)
        profile_post_total = 0;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[15];
        int group_index;

        ig_split(line, field, 15);
        if (!dedicated && rb->strcmp(field[1], profiles[index].username))
            continue;
        group_index = MAX(1, rb->atoi(field[12]));
        if (group_index == 1)
        {
            group_ordinal++;
            if (!dedicated)
                profile_post_total++;
            if (dedicated && group_ordinal >= profile_post_offset &&
                post_count >= IG_MAX_POSTS)
                break;
            include_group = group_ordinal >= profile_post_offset &&
                            post_count < IG_MAX_POSTS;
        }
        if (include_group && post_count < IG_MAX_POSTS)
            ig_copy_post(&posts[post_count++], field);
    }
    rb->close(fd);
    /* RockPod writes each dedicated profile feed newest-first. Avoid an
     * O(n^2) reorder every time a large profile is opened; legacy devices
     * without that compact feed retain the compatibility sort. */
    if (!dedicated)
    {
        for (slot = 1; slot < post_count; slot++)
        {
            int insert = slot;

            while (insert > 0 &&
                   ig_post_newer(&posts[insert], &posts[insert - 1]))
            {
                struct ig_post swap = posts[insert - 1];

                posts[insert - 1] = posts[insert];
                posts[insert] = swap;
                insert--;
            }
        }
    }
    visible_count = 0;
    for (slot = 0; slot < post_count; slot++)
        if (posts[slot].group_index == 1)
            visible_posts[visible_count++] = slot;
}

static bool ig_post_newer(const struct ig_post *left,
                          const struct ig_post *right)
{
    int date_order = rb->strcmp(left->post_date, right->post_date);

    if (date_order != 0)
        return date_order > 0;
    return rb->strcmp(left->id, right->id) > 0;
}

static void ig_load_home(void)
{
    char line[IG_LINE_SIZE];
    int fd = rb->open(IG_LIBRARY, O_RDONLY);

    post_count = 0;
    visible_count = 0;
    home_post_total = 0;
    thumb_index[0] = -1;
    thumb_index[1] = -1;
    thumb_index[2] = -1;
    if (fd < 0)
        return;
    rb->read_line(fd, line, sizeof(line));
    while (post_count < IG_MAX_POSTS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[15];

        ig_split(line, field, 15);
        ig_copy_post(&posts[post_count], field);
        if (posts[post_count].group_index == 1)
            home_post_total++;
        post_count++;
    }
    rb->close(fd);
    visible_count = 0;
    for (fd = 0; fd < post_count; fd++)
        if (posts[fd].group_index == 1)
            visible_posts[visible_count++] = fd;
}

static void ig_load_profiles(void)
{
    char line[IG_LINE_SIZE];
    int fd = rb->open(IG_PROFILES, O_RDONLY);

    profile_count = 0;
    if (fd < 0)
        return;
    rb->read_line(fd, line, sizeof(line));
    while (profile_count < IG_MAX_PROFILES &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[15];
        struct ig_profile *profile = &profiles[profile_count];

        rb->memset(profile, 0, sizeof(*profile));
        ig_split(line, field, 15);
        rb->strlcpy(profile->username, field[0], sizeof(profile->username));
        rb->strlcpy(profile->display_name, field[1],
                    sizeof(profile->display_name));
        rb->strlcpy(profile->bio, field[2], sizeof(profile->bio));
        rb->strlcpy(profile->avatar, field[3], sizeof(profile->avatar));
        rb->strlcpy(profile->cover, field[4], sizeof(profile->cover));
        profile->posts = rb->atoi(field[5]);
        profile->photos = rb->atoi(field[6]);
        profile->videos = rb->atoi(field[7]);
        profile->is_my_profile = rb->atoi(field[8]) != 0;
        profile->followers = rb->atoi(field[9]);
        profile->following = rb->atoi(field[10]);
        profile->verified = rb->atoi(field[11]) != 0;
        rb->strlcpy(profile->name_art, field[12],
                    sizeof(profile->name_art));
        rb->strlcpy(profile->bio_art, field[13],
                    sizeof(profile->bio_art));
        profile->synced_posts = rb->atoi(field[14]);
        if (profile->synced_posts <= 0)
            profile->synced_posts = profile->posts;
        profile_count++;
    }
    rb->close(fd);
}

static bool ig_read_bmp(const char *path, struct bitmap *bm, fb_data *pixels,
                        size_t bytes)
{
    bm->data = (unsigned char *)pixels;
    return path[0] && rb->read_bmp_file(path, bm, bytes,
                                        FORMAT_NATIVE, NULL) > 0;
}

static void ig_load_assets(void)
{
    logo_loaded = ig_read_bmp(IG_LOGO, &logo_bm, logo_pixels,
                              sizeof(logo_pixels));
    avatar_loaded = false;
    name_loaded = false;
    bio_loaded = false;
    avatar_profile_index = -1;
    profile_art_index = -1;
}

/* Paint a complete frame before any TSV or media work. The launch bitmap is
 * a mechanically resized crop of the real 2010 Instagram welcome screen,
 * decoded into the existing viewer buffer so startup adds no framebuffer or
 * playback-memory allocation. */
static long ig_show_launch(void)
{
    struct bitmap launch_bm;

    rb->lcd_set_background(IG_CREAM);
    rb->lcd_set_foreground(IG_CREAM);
    rb->lcd_clear_display();
    if (ig_read_bmp(IG_LAUNCH, &launch_bm, viewer_pixels,
                    sizeof(viewer_pixels)) &&
        launch_bm.width == LCD_WIDTH && launch_bm.height == LCD_HEIGHT)
    {
        rb->lcd_bitmap((const fb_data *)launch_bm.data, 0, 0,
                       LCD_WIDTH, LCD_HEIGHT);
    }
    else if (logo_loaded)
    {
        rb->lcd_bitmap_transparent((const fb_data *)logo_bm.data,
                                   (LCD_WIDTH - logo_bm.width) / 2,
                                   (LCD_HEIGHT - logo_bm.height) / 2,
                                   logo_bm.width, logo_bm.height);
    }
    rb->lcd_update();
    rb->button_clear_queue();
    return *rb->current_tick;
}

static int ig_find_profile(const char *username)
{
    int index;

    for (index = 0; index < profile_count; index++)
        if (!rb->strcmp(profiles[index].username, username))
            return index;
    return -1;
}

static int ig_find_my_profile(void)
{
    int index;

    for (index = 0; index < profile_count; index++)
        if (profiles[index].is_my_profile)
            return index;
    return -1;
}

static void ig_load_profile_avatar(int index)
{
    if (index == avatar_profile_index)
        return;
    if (index < 0 || index >= profile_count)
    {
        avatar_loaded = false;
        avatar_profile_index = -1;
        return;
    }
    avatar_loaded = ig_read_bmp(profiles[index].avatar,
                                &avatar_bm, avatar_pixels,
                                sizeof(avatar_pixels));
    avatar_profile_index = index;
}

static void ig_load_profile_art(int index)
{
    ig_load_profile_avatar(index);
    if (index == profile_art_index)
        return;
    if (index < 0 || index >= profile_count)
    {
        name_loaded = false;
        bio_loaded = false;
        profile_art_index = -1;
        return;
    }
    name_loaded = ig_read_bmp(profiles[index].name_art,
                              &name_bm, name_pixels,
                              sizeof(name_pixels));
    bio_loaded = ig_read_bmp(profiles[index].bio_art,
                             &bio_bm, bio_pixels,
                             sizeof(bio_pixels));
    profile_art_index = index;
}

static void ig_select_profile(int index)
{
    if (index < 0 || index >= profile_count)
        return;
    active_profile = index;
    ig_load_profile_page(index, 0);
    selection = 0;
    ig_load_profile_art(active_profile);
}

static bool ig_change_profile_page(int direction, bool select_last)
{
    int offset;

    if (direction > 0)
    {
        if (profile_post_offset + visible_count >= profile_post_total)
            return false;
        offset = profile_post_offset + visible_count;
    }
    else
    {
        if (profile_post_offset <= 0)
            return false;
        offset = MAX(0, profile_post_offset - IG_MAX_POSTS);
    }
    ig_load_profile_page(active_profile, offset);
    if (visible_count <= 0)
        return false;
    selection = select_last ? visible_count - 1 : 0;
    return true;
}

static struct ig_post *ig_current_post(void)
{
    if (selection < 0 || selection >= visible_count)
        return NULL;
    return &posts[visible_posts[selection]];
}

static bool ig_toggle_current_like(void)
{
    struct ig_post *post = ig_current_post();
    int index;
    bool liked;

    if (viewer_post_index >= 0 && viewer_post_index < post_count)
        post = &posts[viewer_post_index];
    if (!post)
        return false;
    index = ig_liked_index(post->group_id);
    if (index >= 0)
    {
        for (; index + 1 < liked_count; index++)
            rb->strlcpy(liked_ids[index], liked_ids[index + 1],
                        sizeof(liked_ids[index]));
        liked_count--;
        liked = false;
    }
    else
    {
        if (liked_count >= IG_MAX_LIKES)
            return false;
        rb->strlcpy(liked_ids[liked_count++], post->group_id,
                    sizeof(liked_ids[0]));
        liked = true;
    }
    for (index = 0; index < post_count; index++)
        if (!rb->strcmp(posts[index].group_id, post->group_id))
            posts[index].liked = liked;
    return ig_save_likes();
}

static void ig_load_feed_art(void)
{
    struct ig_post *post = ig_current_post();
    char path[MAX_PATH];
    int owner;

    feed_loaded = false;
    if (!post)
        return;
    owner = ig_find_profile(post->username);
    if (owner >= 0)
        ig_load_profile_avatar(owner);
    else
        ig_load_profile_avatar(-1);
    rb->snprintf(path, sizeof(path), IG_ROOT "/feed/%s.bmp", post->id);
    feed_loaded = ig_read_bmp(path, &feed_bm, feed_pixels,
                              sizeof(feed_pixels));
    if (!feed_loaded && post->thumbnail[0])
        feed_loaded = ig_read_bmp(post->thumbnail, &feed_bm, feed_pixels,
                                  sizeof(feed_pixels));
}

static bool ig_load_viewer_level(int zoom, int pan)
{
    char path[MAX_PATH];
    struct ig_post *post = viewer_post_index >= 0 &&
                           viewer_post_index < post_count ?
                           &posts[viewer_post_index] : ig_current_post();

    if (!post || rb->strcmp(post->type, "photo") || !post->display[0] ||
        zoom < 0 || zoom >= IG_ZOOM_LEVELS)
        return false;
    if (zoom > 0)
        rb->snprintf(path, sizeof(path), IG_ZOOM_ROOT "/%s.zoom%d.bmp",
                     post->id, zoom);
    else
        rb->strlcpy(path, post->display, sizeof(path));
    if (!ig_read_bmp(path, &viewer_bm, viewer_pixels,
                     sizeof(viewer_pixels)))
        return false;
    viewer_zoom = zoom;
    viewer_pan = zoom > 0 ? pan : IG_PAN_CENTER;
    return true;
}

static int ig_group_member_index(const char *group_id, int member)
{
    int index;

    for (index = 0; index < post_count; index++)
        if (posts[index].group_index == member &&
            !rb->strcmp(posts[index].group_id, group_id))
            return index;
    return -1;
}

static bool ig_step_viewer_photo(int direction)
{
    int original_offset = profile_post_offset;
    int original_selection = selection;
    int original_viewer = viewer_post_index;
    struct ig_post *current;

    if (viewer_post_index < 0 || viewer_post_index >= post_count)
        return false;
    current = &posts[viewer_post_index];
    if (current->group_count > 1)
    {
        int member = current->group_index + direction;

        while (member >= 1 && member <= current->group_count)
        {
            int index = ig_group_member_index(current->group_id, member);

            if (index >= 0 && !rb->strcmp(posts[index].type, "photo"))
            {
                viewer_post_index = index;
                if (ig_load_viewer_level(0, IG_PAN_CENTER))
                    return true;
                viewer_post_index = original_viewer;
                return false;
            }
            member += direction;
        }
        return false;
    }

    if (viewer_return_screen == IG_SCREEN_HOME)
    {
        int next = selection + direction;

        while (next >= 0 && next < visible_count)
        {
            selection = next;
            viewer_post_index = visible_posts[selection];
            if (!rb->strcmp(posts[visible_posts[selection]].type, "photo") &&
                ig_load_viewer_level(0, IG_PAN_CENTER))
                return true;
            next += direction;
        }
        selection = original_selection;
        viewer_post_index = original_viewer;
        return false;
    }
    while (true)
    {
        int next = selection + direction;
        struct ig_post *candidate;

        if (next < 0 || next >= visible_count)
        {
            if (!ig_change_profile_page(direction, direction < 0))
                break;
            next = selection;
        }
        selection = next;
        candidate = &posts[visible_posts[selection]];
        viewer_post_index = visible_posts[selection];
        if (!rb->strcmp(candidate->type, "photo"))
        {
            if (ig_load_viewer_level(0, IG_PAN_CENTER))
                return true;
        }
    }
    ig_load_profile_page(active_profile, original_offset);
    selection = MIN(original_selection, MAX(visible_count - 1, 0));
    viewer_post_index = visible_count > 0 ? visible_posts[selection] : -1;
    return false;
}

static void ig_draw_wrapped_text(int x, int y, int width, int max_lines,
                                 const char *text)
{
    char line[184];
    char candidate[184];
    const char *cursor = text;
    int row = 0;

    line[0] = '\0';
    while (*cursor && row < max_lines)
    {
        const char *word;
        int word_length;
        int text_width;
        int text_height;

        while (*cursor == ' ')
            cursor++;
        if (!*cursor)
            break;
        word = cursor;
        while (*cursor && *cursor != ' ')
            cursor++;
        word_length = cursor - word;
        rb->snprintf(candidate, sizeof(candidate), "%s%s%.*s",
                     line, line[0] ? " " : "", word_length, word);
        rb->lcd_getstringsize(candidate, &text_width, &text_height);
        if (line[0] && text_width > width)
        {
            rb->lcd_putsxy(x, y + row * 15, line);
            row++;
            line[0] = '\0';
            cursor = word;
            continue;
        }
        rb->strlcpy(line, candidate, sizeof(line));
    }
    if (line[0] && row < max_lines)
        rb->lcd_putsxy(x, y + row * 15, line);
}

static void ig_puts_clipped(int x, int y, int width, const char *text)
{
    char shown[112];
    int text_width, text_height, length;

    rb->strlcpy(shown, text && text[0] ? text : " ", sizeof(shown));
    length = rb->strlen(shown);
    rb->lcd_getstringsize(shown, &text_width, &text_height);
    while (text_width > width && length > 3)
    {
        shown[--length] = '\0';
        rb->lcd_getstringsize(shown, &text_width, &text_height);
    }
    if (text && length < (int)rb->strlen(text) && length > 2)
    {
        shown[length - 1] = '.';
        shown[length - 2] = '.';
    }
    rb->lcd_putsxy(x, y, shown);
}

static int ig_clipped_text_width(const char *text, int maximum)
{
    int width = 0;

    rb->lcd_getstringsize(text && text[0] ? text : " ", &width, NULL);
    return MIN(width, maximum);
}

static int ig_profile_name_art_width(void)
{
    const fb_data *pixels;
    int x;
    int y;

    if (!name_loaded || name_bm.width <= 0 || name_bm.height <= 0)
    {
        struct ig_profile *profile =
            active_profile >= 0 && active_profile < profile_count ?
            &profiles[active_profile] : NULL;
        const char *name = profile && profile->display_name[0] ?
                           profile->display_name :
                           (profile ? profile->username : "Instagram");

        return ig_clipped_text_width(name, 205);
    }
    pixels = (const fb_data *)name_bm.data;
    for (x = MIN(name_bm.width, 205) - 1; x >= 0; x--)
    {
        for (y = 0; y < MIN(name_bm.height, 18); y++)
        {
            if (pixels[y * name_bm.width + x] != IG_CREAM)
                return x + 1;
        }
    }
    return 0;
}

static void ig_draw_verified_badge(int x, int y)
{
    rb->lcd_bitmap_transparent(
        instagram_verified, x, y,
        BMPWIDTH_instagram_verified, BMPHEIGHT_instagram_verified);
}

static void ig_scrollbar(int top, int height, int selected, int total)
{
    int thumb_height;
    int thumb_y;

    if (total <= 1)
        return;
    rb->lcd_set_foreground(IG_BORDER);
    rb->lcd_fillrect(LCD_WIDTH - 4, top, 2, height);
    thumb_height = MAX(14, height / MIN(total, 8));
    thumb_y = top + ((height - thumb_height) * selected) / (total - 1);
    rb->lcd_set_foreground(IG_BLUE);
    rb->lcd_fillrect(LCD_WIDTH - 5, thumb_y, 3, thumb_height);
}

static void ig_tabbar(int active)
{
    static const char * const labels[4] = {
        "HOME", "FAVORITES", "+", "PROFILE"
    };
    int item;

    rb->lcd_set_foreground(IG_NAVY);
    rb->lcd_fillrect(0, 216, LCD_WIDTH, 24);
    rb->lcd_set_foreground(IG_BORDER);
    rb->lcd_hline(0, LCD_WIDTH - 1, 216);
    for (item = 0; item < 4; item++)
    {
        int width, height;
        int x = item * 80;

        if (item == active)
        {
            rb->lcd_set_foreground(IG_LIGHT_BLUE);
            rb->lcd_fillrect(x + 1, 217, 78, 23);
            rb->lcd_set_foreground(IG_NAVY);
        }
        else
            rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_getstringsize(labels[item], &width, &height);
        rb->lcd_putsxy(x + (80 - width) / 2, 222, labels[item]);
    }
}

static void ig_cache_list_names(enum ig_screen screen)
{
    int first = screen == IG_SCREEN_FAVORITES ?
                (favorite_selection / IG_PROFILE_LIST_ROWS) *
                IG_PROFILE_LIST_ROWS :
                (profile_selection / IG_PROFILE_LIST_ROWS) *
                IG_PROFILE_LIST_ROWS;
    int row;

    for (row = 0; row < IG_PROFILE_LIST_ROWS; row++)
    {
        int ordinal = first + row;
        int profile_index = screen == IG_SCREEN_FAVORITES ?
                            ig_favorite_profile_at(ordinal) : ordinal;

        if (profile_index < 0 || profile_index >= profile_count)
            profile_index = -1;
        if (list_name_profile[row] == profile_index)
            continue;
        list_name_profile[row] = -1;
        if (profile_index >= 0 && profiles[profile_index].name_art[0] &&
            ig_read_bmp(profiles[profile_index].name_art,
                        &list_name_bm[row], list_name_pixels[row],
                        sizeof(list_name_pixels[row])))
            list_name_profile[row] = profile_index;
    }
}

static void ig_draw_favorites(void)
{
    int count = ig_favorite_profile_count();
    int first;
    int row;

    rb->lcd_set_background(IG_CREAM);
    rb->lcd_set_foreground(IG_CREAM);
    rb->lcd_clear_display();
    ig_draw_nav("Favorites", "FAVORITES");
    if (count <= 0)
    {
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(48, 92, "No favorite profiles yet");
        rb->lcd_set_foreground(IG_GRAY);
        rb->lcd_putsxy(32, 116, "Open a profile and press Play");
        ig_tabbar(1);
        rb->lcd_update();
        return;
    }
    favorite_selection = MIN(favorite_selection, count - 1);
    first = (favorite_selection / 5) * 5;
    for (row = 0; row < 5 && first + row < count; row++)
    {
        int ordinal = first + row;
        int profile_index = ig_favorite_profile_at(ordinal);
        struct ig_profile *profile = profile_index >= 0 ?
                                     &profiles[profile_index] : NULL;
        int y = 39 + row * 34;

        if (!profile)
            continue;
        rb->lcd_set_foreground(ordinal == favorite_selection ?
                              IG_SELECTED : IG_CREAM);
        rb->lcd_fillrect(5, y - 3, 310, 30);
        rb->lcd_set_foreground(ordinal == favorite_selection ? IG_BLUE :
                              LCD_BLACK);
        if (list_name_profile[row] == profile_index)
            rb->lcd_bitmap_transparent(
                (const fb_data *)list_name_bm[row].data, 16, y - 4,
                MIN(list_name_bm[row].width, 198),
                MIN(list_name_bm[row].height, 18));
        else
            ig_puts_clipped(16, y, 168, profile->username);
        rb->lcd_set_foreground(IG_GRAY);
        rb->lcd_putsxyf(16, y + 14, "@%s", profile->username);
        rb->lcd_putsxyf(220, y + 7, "%d posts", profile->posts);
    }
    rb->lcd_set_foreground(IG_GRAY);
    rb->lcd_putsxy(16, 202, "Center open  Left home");
    ig_tabbar(1);
    rb->lcd_update();
}

static void ig_draw_profiles(void)
{
    int first;
    int row;

    rb->lcd_set_background(IG_CREAM);
    rb->lcd_set_foreground(IG_CREAM);
    rb->lcd_clear_display();
    ig_draw_nav("Profiles", "ALL");
    if (profile_count <= 0)
    {
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(55, 92, "No synced profiles yet");
        rb->lcd_set_foreground(IG_GRAY);
        rb->lcd_putsxy(36, 116, "Add profiles from RockPod");
        ig_tabbar(3);
        rb->lcd_update();
        return;
    }
    profile_selection = MAX(0, MIN(profile_selection, profile_count - 1));
    first = (profile_selection / 5) * 5;
    for (row = 0; row < 5 && first + row < profile_count; row++)
    {
        int ordinal = first + row;
        struct ig_profile *profile = &profiles[ordinal];
        int y = 39 + row * 34;

        rb->lcd_set_foreground(ordinal == profile_selection ?
                              IG_SELECTED : IG_CREAM);
        rb->lcd_fillrect(5, y - 3, 310, 30);
        rb->lcd_set_foreground(ordinal == profile_selection ? IG_BLUE :
                              LCD_BLACK);
        if (list_name_profile[row] == ordinal)
            rb->lcd_bitmap_transparent(
                (const fb_data *)list_name_bm[row].data, 16, y - 4,
                MIN(list_name_bm[row].width, 198),
                MIN(list_name_bm[row].height, 18));
        else
            ig_puts_clipped(16, y, 168, profile->username);
        if (profile->verified)
            ig_draw_verified_badge(186, y);
        rb->lcd_set_foreground(IG_GRAY);
        rb->lcd_putsxyf(16, y + 14, "@%s", profile->username);
        rb->lcd_putsxyf(220, y + 7, "%d posts", profile->posts);
    }
    rb->lcd_set_foreground(IG_GRAY);
    rb->lcd_putsxy(16, 202, "Center open  Left favorites");
    ig_scrollbar(36, 164, profile_selection, profile_count);
    ig_tabbar(3);
    rb->lcd_update();
}

static void ig_cache_thumbnails(void)
{
    int slot;
    int first = (selection / 3) * 3;

    for (slot = 0; slot < 3; slot++)
    {
        int ordinal = first + slot;
        int index = ordinal < visible_count ? visible_posts[ordinal] : -1;
        if (thumb_index[slot] == index)
            continue;
        thumb_index[slot] = -1;
        if (index >= 0 && ig_read_bmp(posts[index].thumbnail,
                                     &thumb_bm[slot], thumb_pixels[slot],
                                     sizeof(thumb_pixels[slot])))
            thumb_index[slot] = index;
    }
}

static void ig_draw_nav(const char *title, const char *right)
{
    rb->lcd_set_background(IG_NAVY);
    rb->lcd_set_foreground(IG_NAVY);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 28);
    rb->lcd_set_foreground(IG_BLUE);
    rb->lcd_hline(0, LCD_WIDTH - 1, 0);
    rb->lcd_set_foreground(IG_BORDER);
    rb->lcd_hline(0, LCD_WIDTH - 1, 27);
    if (logo_loaded && logo_bm.width >= 26 && logo_bm.height >= 26)
        rb->lcd_bitmap_part((const fb_data *)logo_bm.data,
                            (logo_bm.width - 26) / 2,
                            (logo_bm.height - 26) / 2,
                            logo_bm.width, 2, 1, 26, 26);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_WHITE);
    ig_puts_clipped(33, 6, 190, title);
    if (right && right[0])
        ig_puts_clipped(246, 6, 68, right);
}

static void ig_draw_home(void)
{
    struct ig_post *post = ig_current_post();
    struct ig_profile *profile = NULL;
    const char *caption;
    char position[24];
    int image_x;
    int image_y;
    int owner;

    rb->lcd_set_background(IG_CREAM);
    rb->lcd_set_foreground(IG_CREAM);
    rb->lcd_clear_display();
    ig_draw_nav("Instagram", "HOME");
    if (!post)
    {
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(62, 93, "Your Instagram feed is empty");
        rb->lcd_set_foreground(IG_GRAY);
        rb->lcd_putsxy(44, 116, "Sync profiles from RockPod to begin");
        ig_tabbar(false);
        rb->lcd_update();
        return;
    }
    owner = ig_find_profile(post->username);
    if (owner >= 0)
        profile = &profiles[owner];
    rb->lcd_set_foreground(IG_SELECTED);
    rb->lcd_fillrect(0, 28, LCD_WIDTH, 28);
    if (avatar_loaded && avatar_bm.width >= 24 && avatar_bm.height >= 24)
        rb->lcd_bitmap_part((const fb_data *)avatar_bm.data,
                            (avatar_bm.width - 24) / 2,
                            (avatar_bm.height - 24) / 2,
                            avatar_bm.width, 3, 30, 24, 24);
    else
    {
        rb->lcd_set_foreground(IG_LIGHT_BLUE);
        rb->lcd_fillrect(3, 30, 24, 24);
    }
    rb->lcd_set_foreground(IG_BLUE);
    ig_puts_clipped(33, 33, 164, post->username);
    if (profile && profile->verified)
        ig_draw_verified_badge(
            36 + ig_clipped_text_width(post->username, 164), 32);
    rb->snprintf(position, sizeof(position), "%d/%d",
                 selection + 1, visible_count);
    rb->lcd_set_foreground(IG_GRAY);
    ig_puts_clipped(267, 33, 47, position);
    rb->lcd_set_foreground(IG_BORDER);
    rb->lcd_drawrect(3, 56, 162, 160);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(4, 57, 160, 158);
    if (feed_loaded)
    {
        image_x = 4 + MAX(0, (160 - feed_bm.width) / 2);
        image_y = 57 + MAX(0, (158 - feed_bm.height) / 2);
        rb->lcd_bitmap((const fb_data *)feed_bm.data, image_x, image_y,
                       MIN(feed_bm.width, 160), MIN(feed_bm.height, 158));
    }
    rb->lcd_bitmap_transparent(
        post->liked ? instagram_heart : instagram_heart_unliked,
        174, 60, BMPWIDTH_instagram_heart, BMPHEIGHT_instagram_heart);
    rb->lcd_set_foreground(IG_BLUE);
    rb->lcd_putsxyf(210, 67, "%d likes", post->likes +
                    (post->liked ? 1 : 0));
    rb->lcd_set_foreground(IG_GRAY);
    if (post->group_count > 1)
        rb->lcd_putsxyf(172, 96, "%d-PHOTO GROUP", post->group_count);
    else
        rb->lcd_putsxy(172, 96, !rb->strcmp(post->type, "video") ?
                       "VIDEO POST" : "PHOTO POST");
    rb->lcd_putsxyf(172, 113, "%.10s", post->post_date);
    rb->lcd_set_foreground(IG_BLUE);
    ig_puts_clipped(172, 134, 142, post->username);
    caption = post->caption[0] ? post->caption : post->title;
    rb->lcd_set_foreground(LCD_BLACK);
    ig_draw_wrapped_text(172, 152, 142, 3, caption);
    rb->lcd_set_foreground(IG_GRAY);
    ig_puts_clipped(172, 202, 142, "Center open  Play like");
    ig_scrollbar(57, 158, selection, visible_count);
    ig_tabbar(false);
    rb->lcd_update();
}

static void ig_draw_viewer(void)
{
    struct ig_post *post = viewer_post_index >= 0 &&
                           viewer_post_index < post_count ?
                           &posts[viewer_post_index] : ig_current_post();
    static const char * const zoom_labels[IG_ZOOM_LEVELS] = {
        "1x", "1.5x"
    };
    int source_x = 0;
    int source_y = 0;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    if (viewer_loaded)
    {
        int max_x = MAX(0, viewer_bm.width - LCD_WIDTH);
        int max_y = MAX(0, viewer_bm.height - 200);

        source_x = max_x / 2;
        source_y = max_y / 2;
        if (viewer_zoom > 0)
        {
            if (viewer_pan == IG_PAN_UP)
                source_y = 0;
            else if (viewer_pan == IG_PAN_RIGHT)
                source_x = max_x;
            else if (viewer_pan == IG_PAN_DOWN)
                source_y = max_y;
            else if (viewer_pan == IG_PAN_LEFT)
                source_x = 0;
        }
        rb->lcd_bitmap_part((const fb_data *)viewer_bm.data,
                            source_x, source_y, viewer_bm.width,
                            0, 22, MIN(LCD_WIDTH, viewer_bm.width),
                            MIN(200, viewer_bm.height));
    }
    rb->lcd_set_foreground(IG_NAVY);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 22);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(7, 4, post ? post->username : "Instagram");
    if (post && post->group_count > 1)
        rb->lcd_putsxyf(180, 4, "%d/%d",
                        post->group_index, post->group_count);
    else
        rb->lcd_putsxyf(180, 4, "%d/%d",
                        profile_post_offset + selection + 1,
                        profile_post_total);
    rb->lcd_putsxy(230, 4, zoom_labels[viewer_zoom]);
    if (post && post->liked)
        rb->lcd_putsxy(135, 4, "LIKED");
    rb->lcd_putsxy(282, 4, "Back");
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, 222, LCD_WIDTH, 18);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(7, 224, post && post->group_count > 1 ?
                   "Wheel: Pan  Center: Zoom  Left/Right: Group" :
                   "Wheel: Pan  Center: Zoom  Previous/Next: Photo");
    rb->lcd_update();
}

static bool ig_restore_home_path(const char *path)
{
    int index;

    if (path == NULL || path[0] == '\0')
        return false;
    for (index = 0; index < visible_count; index++)
    {
        struct ig_post *post = &posts[visible_posts[index]];

        if (!rb->strcmp(post->media, path) ||
            !rb->strcmp(post->autoplay, path))
        {
            selection = index;
            home_selection = index;
            return true;
        }
    }
    return false;
}

static enum plugin_status ig_open_home_autoplay(void)
{
    struct ig_post *post = ig_current_post();
    static char launch[MAX_PATH + 24];
    const char *video;

    if (!post || rb->strcmp(post->type, "video"))
        return PLUGIN_ERROR;
    /* The player scales the original media into the fixed feed card. Reusing
     * the full-quality file removes duplicate preview storage and lets Select
     * expand the exact same stream without reopening or changing position. */
    video = post->media;
    if (!video[0])
        return PLUGIN_ERROR;
    ig_save_home_state();
    rb->snprintf(launch, sizeof(launch), IG_FEED_PREFIX "%s",
                 video);
    return rb->plugin_open(IG_PLAYER, launch);
}

static bool ig_restore_path(const char *path)
{
    char line[IG_LINE_SIZE];
    char username[64];
    char group_id[32];
    int fd = rb->open(IG_LIBRARY, O_RDONLY);
    int ordinal = 0;
    int owner;

    username[0] = '\0';
    group_id[0] = '\0';
    if (fd < 0)
        return false;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[15];

        ig_split(line, field, 15);
        if (!rb->strcmp(field[5], path) || !rb->strcmp(field[14], path))
        {
            rb->strlcpy(username, field[1], sizeof(username));
            rb->strlcpy(group_id, field[11], sizeof(group_id));
            if (!group_id[0])
                rb->strlcpy(group_id, field[0], sizeof(group_id));
            break;
        }
    }
    rb->close(fd);
    owner = ig_find_profile(username);
    if (owner < 0)
        return false;

    fd = rb->open(IG_LIBRARY, O_RDONLY);
    if (fd < 0)
        return false;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[15];
        const char *candidate_group;

        ig_split(line, field, 15);
        if (rb->strcmp(field[1], username))
            continue;
        if (MAX(1, rb->atoi(field[12])) != 1)
            continue;
        candidate_group = field[11][0] ? field[11] : field[0];
        if (!rb->strcmp(candidate_group, group_id))
            break;
        ordinal++;
    }
    rb->close(fd);
    profile_selection = owner;
    active_profile = owner;
    ig_load_profile_page(owner, (ordinal / IG_MAX_POSTS) * IG_MAX_POSTS);
    selection = ordinal - profile_post_offset;
    ig_load_profile_art(owner);
    return selection >= 0 && selection < visible_count;
}

#ifdef HAVE_WHEEL_POSITION
static bool ig_touch_pan(void)
{
    int wheel;
    int pan;

    if (viewer_zoom <= 0)
        return false;
    wheel = rb->wheel_status();
    if (wheel < 0)
        return false;
    /* Position 0 is the top, increasing clockwise around the wheel. */
    switch (((wheel + 12) / 24) & 3)
    {
    case 0: pan = IG_PAN_UP; break;
    case 1: pan = IG_PAN_RIGHT; break;
    case 2: pan = IG_PAN_DOWN; break;
    default: pan = IG_PAN_LEFT; break;
    }
    if (pan == viewer_pan)
        return false;
    viewer_pan = pan;
    return true;
}
#endif

static void ig_format_count(int value, char *buffer, size_t size)
{
    if (value >= 1000000)
        rb->snprintf(buffer, size, "%d.%dM", value / 1000000,
                     (value % 1000000) / 100000);
    else if (value >= 1000)
        rb->snprintf(buffer, size, "%d.%dK", value / 1000,
                     (value % 1000) / 100);
    else
        rb->snprintf(buffer, size, "%d", value);
}

static void ig_draw_profile(void)
{
    struct ig_profile *profile;
    char count[3][16];
    char position[24];
    int first;
    int slot;

    rb->lcd_set_background(IG_CREAM);
    rb->lcd_set_foreground(IG_CREAM);
    rb->lcd_clear_display();
    if (active_profile < 0 || active_profile >= profile_count)
    {
        ig_draw_nav("Profile", "PROFILE");
        rb->lcd_set_foreground(IG_GRAY);
        rb->lcd_putsxy(45, 112, "No Instagram profile selected");
        ig_tabbar(3);
        rb->lcd_update();
        return;
    }
    profile = &profiles[active_profile];
    rb->snprintf(position, sizeof(position), "%d/%d",
                 active_profile + 1, profile_count);
    ig_draw_nav(profile->username, position);
    if (avatar_loaded)
    {
        rb->lcd_set_foreground(IG_BORDER);
        rb->lcd_drawrect(7, 33, 42, 42);
        rb->lcd_bitmap((const fb_data *)avatar_bm.data, 8, 34,
                       MIN(avatar_bm.width, IG_AVATAR_W),
                       MIN(avatar_bm.height, IG_AVATAR_H));
    }
    else
    {
        rb->lcd_set_foreground(IG_LIGHT_BLUE);
        rb->lcd_fillrect(8, 34, 40, 40);
    }
    if (name_loaded)
        rb->lcd_bitmap_transparent(
            (const fb_data *)name_bm.data, 56, 32,
            MIN(name_bm.width, 205), MIN(name_bm.height, 18));
    else
    {
        rb->lcd_set_foreground(LCD_BLACK);
        ig_puts_clipped(56, 34, 205,
                        profile->display_name[0] ? profile->display_name :
                        profile->username);
    }
    rb->lcd_set_foreground(IG_BLUE);
    rb->lcd_putsxyf(56, 52, "@%s", profile->username);
    if (profile->verified)
        ig_draw_verified_badge(
            MIN(300, 60 + ig_profile_name_art_width()), 34);
    ig_format_count(profile->posts, count[0], sizeof(count[0]));
    ig_format_count(profile->followers, count[1], sizeof(count[1]));
    ig_format_count(profile->following, count[2], sizeof(count[2]));
    rb->lcd_set_foreground(IG_SELECTED);
    rb->lcd_fillrect(5, 80, 310, 26);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxyf(16, 83, "%s posts", count[0]);
    rb->lcd_putsxyf(112, 83, "%s followers", count[1]);
    rb->lcd_putsxyf(231, 83, "%s following", count[2]);
    if (bio_loaded)
        rb->lcd_bitmap((const fb_data *)bio_bm.data, 8, 108,
                       MIN(bio_bm.width, 304), MIN(bio_bm.height, 30));
    else
    {
        rb->lcd_set_foreground(IG_GRAY);
        if (profile->bio[0])
            ig_draw_wrapped_text(8, 109, 304, 2, profile->bio);
    }
    rb->lcd_set_foreground(IG_BORDER);
    rb->lcd_hline(0, LCD_WIDTH - 1, 139);
    first = (selection / 3) * 3;
    for (slot = 0; slot < 3; slot++)
    {
        int ordinal = first + slot;
        int index = ordinal < visible_count ? visible_posts[ordinal] : -1;
        int x = 16 + slot * 100;

        rb->lcd_set_foreground(ordinal == selection ? IG_BLUE : IG_BORDER);
        rb->lcd_drawrect(x - 2, 141, 76, 74);
        if (index >= 0 && thumb_index[slot] == index)
        {
            rb->lcd_bitmap_part((const fb_data *)thumb_bm[slot].data,
                                0, 0, thumb_bm[slot].width,
                                x, 142,
                                MIN(72, thumb_bm[slot].width),
                                MIN(72, thumb_bm[slot].height));
            if (!rb->strcmp(posts[index].type, "video"))
            {
                rb->lcd_set_foreground(LCD_BLACK);
                rb->lcd_fillrect(x + 43, 143, 28, 15);
                rb->lcd_set_foreground(LCD_WHITE);
                rb->lcd_putsxy(x + 47, 144, "VID");
            }
            if (posts[index].group_count > 1)
            {
                rb->lcd_set_foreground(LCD_BLACK);
                rb->lcd_fillrect(x + 43, 143, 28, 15);
                rb->lcd_set_foreground(LCD_WHITE);
                rb->lcd_putsxyf(x + 47, 144, "%dX",
                                posts[index].group_count);
            }
            if (posts[index].liked)
                rb->lcd_bitmap_transparent(
                    instagram_heart, x + 39, 179,
                    BMPWIDTH_instagram_heart,
                    BMPHEIGHT_instagram_heart);
        }
        else
        {
            rb->lcd_set_foreground(IG_LIGHT_BLUE);
            rb->lcd_fillrect(x, 142, 72, 72);
        }
    }
    if (visible_count == 0)
    {
        rb->lcd_set_foreground(IG_GRAY);
        rb->lcd_putsxy(86, 167, "No synced posts");
    }
    rb->lcd_set_foreground(IG_GRAY);
    ig_puts_clipped(8, 202, 300,
                    ig_favorite_index(profile->username) >= 0 ?
                    "Center open  Play remove favorite" :
                    "Center open  Play favorite profile");
    ig_tabbar(3);
    rb->lcd_update();
}

enum plugin_status plugin_start(const void *parameter)
{
    bool running = true;
    bool redraw = true;
    enum ig_screen screen = IG_SCREEN_HOME;
    bool select_held = false;
    bool returning_home = false;
    bool suppress_autoplay = false;
    int home_direction = 0;
    const char *home_path = NULL;
    long launch_started = 0;
    bool cold_launch = parameter == NULL;

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_RGB565)
    rb->lcd_set_mode(LCD_MODE_RGB565);
#endif
    rb->lcd_set_drawmode(DRMODE_SOLID);
    /* Reuse the already-loaded UI font. Loading a plugin font here could
     * shrink playback's shared audio buffer on hardware. */
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_backdrop(NULL);
    /* A cold launch needs an immediate non-black fallback. On a player
     * return, leave the last decoded frame untouched until the next post's
     * complete home frame is ready; painting this cream frame there caused a
     * full-screen white flash on every wheel swipe away from a video. */
    if (cold_launch)
    {
        rb->lcd_set_background(IG_CREAM);
        rb->lcd_set_foreground(IG_CREAM);
        rb->lcd_clear_display();
        rb->lcd_update();
    }
    ig_load_assets();
    if (cold_launch)
        launch_started = ig_show_launch();
    ig_load_likes();
    ig_load_profiles();
    ig_load_favorites();
    profile_selection = 0;
    selection = 0;
    home_selection = 0;
    {
        int owner = ig_find_my_profile();
        if (owner >= 0)
            profile_selection = owner;
    }
    if (parameter &&
        !rb->strncmp((const char *)parameter, "return-home-next:", 17))
    {
        returning_home = true;
        home_direction = 1;
        home_path = (const char *)parameter + 17;
    }
    else if (parameter &&
             !rb->strncmp((const char *)parameter,
                          "return-home-prev:", 17))
    {
        returning_home = true;
        home_direction = -1;
        home_path = (const char *)parameter + 17;
    }
    else if (parameter &&
             !rb->strncmp((const char *)parameter, "return-home:", 12))
    {
        returning_home = true;
        suppress_autoplay = true;
        home_path = (const char *)parameter + 12;
    }
    else if (parameter && !rb->strncmp((const char *)parameter, "return:", 7))
    {
        if (ig_restore_path((const char *)parameter + 7))
            screen = IG_SCREEN_PROFILE;
    }
    if (screen == IG_SCREEN_PROFILE)
        ig_cache_thumbnails();
    else
    {
        ig_load_home();
        if (returning_home)
        {
            ig_restore_home_path(home_path);
            selection = MAX(0, MIN(selection + home_direction,
                                   MAX(visible_count - 1, 0)));
            home_selection = selection;
        }
        else
            ig_restore_home_state();
        ig_save_home_state();
        if (cold_launch)
        {
            long launch_until = launch_started + HZ * 2 / 3;

            while (TIME_BEFORE(*rb->current_tick, launch_until))
                rb->sleep(1);
            rb->button_clear_queue();
        }
        if (!suppress_autoplay && visible_count > 0)
        {
            struct ig_post *post = ig_current_post();

            if (post && !rb->strcmp(post->type, "video") &&
                post->media[0])
                return ig_open_home_autoplay();
        }
        ig_load_feed_art();
    }
    while (running)
    {
        int action;
        int timeout = TIMEOUT_BLOCK;

        if (redraw)
        {
            if (screen == IG_SCREEN_VIEWER)
                ig_draw_viewer();
            else if (screen == IG_SCREEN_FAVORITES)
                ig_draw_favorites();
            else if (screen == IG_SCREEN_PROFILES)
                ig_draw_profiles();
            else if (screen == IG_SCREEN_PROFILE)
                ig_draw_profile();
            else
                ig_draw_home();
            redraw = false;
        }
#ifdef HAVE_WHEEL_POSITION
        if (screen == IG_SCREEN_VIEWER && viewer_zoom > 0)
            timeout = MAX(1, HZ / 12);
#endif
        action = pluginlib_getaction(timeout, contexts,
                                     ARRAYLEN(contexts));
#ifdef HAVE_WHEEL_POSITION
        if (screen == IG_SCREEN_VIEWER && ig_touch_pan())
        {
            redraw = true;
            continue;
        }
#endif
        switch (action)
        {
        case PLA_SELECT_REPEAT:
            if (!select_held &&
                (screen == IG_SCREEN_HOME || screen == IG_SCREEN_PROFILE) &&
                ig_toggle_current_like())
                redraw = true;
            select_held = true;
            break;
        case PLA_SCROLL_BACK:
        case PLA_SCROLL_BACK_REPEAT:
            if (screen == IG_SCREEN_VIEWER && viewer_zoom > 0)
            {
                if (ig_load_viewer_level(viewer_zoom - 1, IG_PAN_CENTER))
                    redraw = true;
            }
            else if (screen == IG_SCREEN_FAVORITES &&
                     favorite_selection > 0)
            {
                favorite_selection--;
                ig_cache_list_names(IG_SCREEN_FAVORITES);
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILES && profile_selection > 0)
            {
                profile_selection--;
                ig_cache_list_names(IG_SCREEN_PROFILES);
                redraw = true;
            }
            else if (screen == IG_SCREEN_HOME && selection > 0)
            {
                selection--;
                home_selection = selection;
                ig_save_home_state();
                if (!rb->strcmp(ig_current_post()->type, "video") &&
                    ig_current_post()->media[0])
                    return ig_open_home_autoplay();
                ig_load_feed_art();
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILE && selection > 0)
            {
                selection--;
                ig_cache_thumbnails();
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILE &&
                     ig_change_profile_page(-1, true))
            {
                ig_cache_thumbnails();
                redraw = true;
            }
            break;
        case PLA_SCROLL_FWD:
        case PLA_SCROLL_FWD_REPEAT:
            if (screen == IG_SCREEN_VIEWER &&
                viewer_zoom + 1 < IG_ZOOM_LEVELS)
            {
                if (ig_load_viewer_level(viewer_zoom + 1, IG_PAN_CENTER))
                    redraw = true;
            }
            else if (screen == IG_SCREEN_FAVORITES &&
                     favorite_selection + 1 < ig_favorite_profile_count())
            {
                favorite_selection++;
                ig_cache_list_names(IG_SCREEN_FAVORITES);
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILES &&
                     profile_selection + 1 < profile_count)
            {
                profile_selection++;
                ig_cache_list_names(IG_SCREEN_PROFILES);
                redraw = true;
            }
            else if (screen == IG_SCREEN_HOME &&
                     selection + 1 < visible_count)
            {
                selection++;
                home_selection = selection;
                ig_save_home_state();
                if (!rb->strcmp(ig_current_post()->type, "video") &&
                    ig_current_post()->media[0])
                    return ig_open_home_autoplay();
                ig_load_feed_art();
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILE &&
                     selection + 1 < visible_count)
            {
                selection++;
                ig_cache_thumbnails();
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILE &&
                     ig_change_profile_page(1, false))
            {
                ig_cache_thumbnails();
                redraw = true;
            }
            break;
        case PLA_SELECT_REL:
            if (select_held)
            {
                select_held = false;
                redraw = true;
                break;
            }
            if (screen == IG_SCREEN_PROFILES)
            {
                if (profile_selection >= 0 &&
                    profile_selection < profile_count)
                {
                    profile_return_screen = IG_SCREEN_PROFILES;
                    ig_select_profile(profile_selection);
                    ig_cache_thumbnails();
                    screen = IG_SCREEN_PROFILE;
                    redraw = true;
                }
            }
            else if (screen == IG_SCREEN_FAVORITES)
            {
                int owner = ig_favorite_profile_at(favorite_selection);

                if (owner >= 0)
                {
                    profile_return_screen = IG_SCREEN_FAVORITES;
                    profile_selection = owner;
                    ig_select_profile(owner);
                    ig_cache_thumbnails();
                    screen = IG_SCREEN_PROFILE;
                    redraw = true;
                }
            }
            else if ((screen == IG_SCREEN_HOME ||
                      screen == IG_SCREEN_PROFILE) && visible_count > 0)
            {
                struct ig_post *post = ig_current_post();

                if (post && !rb->strcmp(post->type, "video") &&
                    post->media[0])
                {
                    static char launch[MAX_PATH + 16];
                    if (screen == IG_SCREEN_HOME)
                        ig_save_home_state();
                    rb->snprintf(launch, sizeof(launch), IG_PREFIX "%s",
                                 post->media);
                    return rb->plugin_open(IG_PLAYER, launch);
                }
                viewer_zoom = 0;
                viewer_post_index = visible_posts[selection];
                viewer_loaded = ig_load_viewer_level(0, IG_PAN_CENTER);
                if (viewer_loaded)
                {
                    viewer_return_screen = screen;
                    screen = IG_SCREEN_VIEWER;
#ifdef HAVE_WHEEL_POSITION
                    rb->wheel_send_events(false);
#endif
                    redraw = true;
                }
                else
                    viewer_post_index = -1;
            }
            else if (screen == IG_SCREEN_VIEWER)
            {
                int next_zoom = (viewer_zoom + 1) % IG_ZOOM_LEVELS;
                viewer_loaded = ig_load_viewer_level(next_zoom,
                                                     IG_PAN_CENTER) ||
                                viewer_loaded;
                redraw = true;
            }
            break;
        case IG_ACTION_LIKE:
            if (screen == IG_SCREEN_PROFILE &&
                ig_toggle_profile_favorite(active_profile))
                redraw = true;
            else if (ig_toggle_current_like())
                redraw = true;
            break;
        case PLA_LEFT:
        case PLA_LEFT_REPEAT:
            if (screen == IG_SCREEN_VIEWER)
            {
                viewer_loaded = ig_step_viewer_photo(-1) || viewer_loaded;
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILE)
            {
                screen = profile_return_screen;
                ig_cache_list_names(screen);
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILES)
            {
                favorite_selection = 0;
                screen = IG_SCREEN_FAVORITES;
                ig_cache_list_names(screen);
                redraw = true;
            }
            else if (screen == IG_SCREEN_FAVORITES)
            {
                screen = IG_SCREEN_HOME;
                ig_load_home();
                ig_restore_home_state();
                ig_load_feed_art();
                redraw = true;
            }
            else if (screen == IG_SCREEN_HOME)
            {
                favorite_selection = 0;
                screen = IG_SCREEN_FAVORITES;
                ig_cache_list_names(screen);
                redraw = true;
            }
            break;
        case PLA_RIGHT:
        case PLA_RIGHT_REPEAT:
            if (screen == IG_SCREEN_VIEWER)
            {
                viewer_loaded = ig_step_viewer_photo(1) || viewer_loaded;
                redraw = true;
            }
            else if (screen == IG_SCREEN_HOME)
            {
                home_selection = selection;
                ig_save_home_state();
                screen = IG_SCREEN_PROFILES;
                ig_cache_list_names(screen);
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILE && profile_count > 1)
            {
                profile_selection = (active_profile + 1) % profile_count;
                ig_select_profile(profile_selection);
                ig_cache_thumbnails();
                redraw = true;
            }
            else if (screen == IG_SCREEN_FAVORITES)
            {
                screen = IG_SCREEN_PROFILES;
                ig_cache_list_names(screen);
                redraw = true;
            }
            break;
        case PLA_CANCEL:
            if (screen == IG_SCREEN_VIEWER)
            {
                screen = viewer_return_screen;
                viewer_loaded = false;
                viewer_post_index = -1;
#ifdef HAVE_WHEEL_POSITION
                rb->wheel_send_events(true);
#endif
                if (screen == IG_SCREEN_PROFILE)
                {
                    ig_load_profile_art(active_profile);
                    ig_cache_thumbnails();
                }
                else
                    ig_load_feed_art();
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILE)
            {
                screen = profile_return_screen;
                ig_cache_list_names(screen);
                redraw = true;
            }
            else if (screen == IG_SCREEN_PROFILES)
            {
                screen = IG_SCREEN_FAVORITES;
                ig_cache_list_names(screen);
                redraw = true;
            }
            else if (screen == IG_SCREEN_FAVORITES)
            {
                screen = IG_SCREEN_HOME;
                ig_load_home();
                ig_restore_home_state();
                ig_load_feed_art();
                redraw = true;
            }
            else
            {
                ig_save_home_state();
                running = false;
            }
            break;
        case PLA_EXIT:
            if (screen == IG_SCREEN_HOME)
                ig_save_home_state();
            running = false;
            break;
        default:
            if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                return PLUGIN_USB_CONNECTED;
            break;
        }
    }
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    return PLUGIN_OK;
}
