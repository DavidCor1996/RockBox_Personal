/***************************************************************************
 * Standalone offline OnlyFans profile browser for RockPod-synchronised media.
 ***************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"

#if !defined(HAVE_LCD_COLOR) || LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error The OnlyFans application requires a 320x240 colour display
#endif

#define OF_ROOT         ROCKBOX_DIR "/onlyfans"
#define OF_LIBRARY      OF_ROOT "/library.tsv"
#define OF_PROFILES     OF_ROOT "/profiles.tsv"
#define OF_PROFILE_FEED OF_ROOT "/profile-feed"
#define OF_LOGO         OF_ROOT "/assets/onlyfans-logo.bmp"
#define OF_PLAYER       VIEWERS_DIR "/mpegplayer.rock"
#define OF_PREFIX       "onlyfans-app:"
#define OF_MAX_POSTS    96
#define OF_MAX_PROFILES 16
#define OF_LINE_SIZE    1024
#define OF_THUMB_W      96
#define OF_THUMB_H      72
#define OF_AVATAR_W     40
#define OF_AVATAR_H     40
#define OF_VIEW_W       320
#define OF_VIEW_H       200
#define OF_ZOOM_LEVELS  4
#define OF_BLUE         LCD_RGBPACK(0x00, 0xaf, 0xf0)
#define OF_LIGHT_BLUE   LCD_RGBPACK(0xe9, 0xf8, 0xfd)
#define OF_GRAY         LCD_RGBPACK(0x76, 0x76, 0x76)
#define OF_BORDER       LCD_RGBPACK(0xd5, 0xd9, 0xdc)
#define OF_SELECTED     LCD_RGBPACK(0xf1, 0xfa, 0xfe)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping of_context[] =
{
    { PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK,               BUTTON_NONE },
    { PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD,                BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_SELECT_REL,         BUTTON_SELECT|BUTTON_REL,         BUTTON_NONE },
    { PLA_LEFT,               BUTTON_LEFT,                      BUTTON_NONE },
    { PLA_RIGHT,              BUTTON_RIGHT,                     BUTTON_NONE },
    { PLA_LEFT_REPEAT,        BUTTON_LEFT|BUTTON_REPEAT,        BUTTON_NONE },
    { PLA_RIGHT_REPEAT,       BUTTON_RIGHT|BUTTON_REPEAT,       BUTTON_NONE },
    { PLA_CANCEL,             BUTTON_MENU,                      BUTTON_NONE },
    { PLA_EXIT,               BUTTON_PLAY|BUTTON_REL,           BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *contexts[] = { of_context };
#else
static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

struct of_post
{
    char id[32];
    char username[64];
    char type[8];
    char title[80];
    char caption[184];
    char media[MAX_PATH];
    char thumbnail[MAX_PATH];
    char display[MAX_PATH];
};

struct of_profile
{
    char username[64];
    char display_name[80];
    char bio[184];
    char avatar[MAX_PATH];
    char cover[MAX_PATH];
    int posts;
    int photos;
    int videos;
    bool is_my_profile;
};

static struct of_post posts[OF_MAX_POSTS];
static struct of_profile profiles[OF_MAX_PROFILES];
static int post_count;
static int profile_count;
static int profile_selection;
static int active_profile = -1;
static int visible_posts[OF_MAX_POSTS];
static int visible_count;
static int selection;
static int profile_post_offset;
static int profile_post_total;

enum of_screen
{
    OF_SCREEN_PROFILES = 0,
    OF_SCREEN_POSTS,
    OF_SCREEN_VIEWER,
};

/* Bounded BSS: two thumbnails (27,648 bytes), one viewer frame
 * (128,000 bytes), one logo and one avatar (6,400 bytes). Drawing performs
 * no I/O, decoding, allocation, or playback-memory ownership changes. */
static fb_data thumb_pixels[2][OF_THUMB_W * OF_THUMB_H];
static struct bitmap thumb_bm[2];
static int thumb_index[2] = { -1, -1 };
static fb_data viewer_pixels[OF_VIEW_W * OF_VIEW_H];
static struct bitmap viewer_bm;
static bool viewer_loaded;
static int viewer_zoom;
enum of_viewer_pan
{
    OF_PAN_CENTER = 0,
    OF_PAN_UP,
    OF_PAN_RIGHT,
    OF_PAN_DOWN,
    OF_PAN_LEFT,
};
static int viewer_pan;
static fb_data logo_pixels[40 * 40];
static struct bitmap logo_bm;
static bool logo_loaded;
static fb_data avatar_pixels[OF_AVATAR_W * OF_AVATAR_H];
static struct bitmap avatar_bm;
static bool avatar_loaded;
static struct bitmap cover_bm;
static bool cover_loaded;
static int profile_art_index = -1;

static int of_split(char *line, char **fields, int count)
{
    int found = 1;
    char *cursor = line;

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
        fields[found++] = "";
    cursor = fields[count - 1];
    while (*cursor && *cursor != '\r' && *cursor != '\n')
        cursor++;
    *cursor = '\0';
    return found;
}

static void of_copy_post(struct of_post *post, char **field)
{
    rb->memset(post, 0, sizeof(*post));
    rb->strlcpy(post->id, field[0], sizeof(post->id));
    rb->strlcpy(post->username, field[1], sizeof(post->username));
    rb->strlcpy(post->type, field[2], sizeof(post->type));
    rb->strlcpy(post->title, field[3], sizeof(post->title));
    rb->strlcpy(post->caption, field[4], sizeof(post->caption));
    rb->strlcpy(post->media, field[5], sizeof(post->media));
    rb->strlcpy(post->thumbnail, field[6], sizeof(post->thumbnail));
    rb->strlcpy(post->display, field[7], sizeof(post->display));
}

static bool of_profile_feed_path(int index, char *path, size_t size)
{
    const char *cursor;

    if (index < 0 || index >= profile_count)
        return false;
    for (cursor = profiles[index].username; *cursor; cursor++)
    {
        if (!((*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '.' || *cursor == '-'))
            return false;
    }
    return rb->snprintf(path, size, OF_PROFILE_FEED "/%s.tsv",
                        profiles[index].username) < (int)size;
}

static void of_load_profile_page(int index, int offset)
{
    char line[OF_LINE_SIZE];
    char path[MAX_PATH];
    int fd = -1;
    int matched = 0;
    int slot;
    bool dedicated = false;

    post_count = 0;
    visible_count = 0;
    profile_post_offset = MAX(0, offset);
    profile_post_total = 0;
    thumb_index[0] = -1;
    thumb_index[1] = -1;
    if (index < 0 || index >= profile_count)
        return;
    if (of_profile_feed_path(index, path, sizeof(path)))
    {
        fd = rb->open(path, O_RDONLY);
        dedicated = fd >= 0;
    }
    if (fd < 0)
        fd = rb->open(OF_LIBRARY, O_RDONLY);
    if (fd < 0)
        return;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[8];

        of_split(line, field, 8);
        if (!dedicated && rb->strcmp(field[1], profiles[index].username))
            continue;
        if (matched >= profile_post_offset && post_count < OF_MAX_POSTS)
            of_copy_post(&posts[post_count++], field);
        matched++;
    }
    rb->close(fd);
    profile_post_total = matched;
    visible_count = post_count;
    for (slot = 0; slot < visible_count; slot++)
        visible_posts[slot] = slot;
}

static void of_load_profiles(void)
{
    char line[OF_LINE_SIZE];
    int fd = rb->open(OF_PROFILES, O_RDONLY);

    profile_count = 0;
    if (fd < 0)
        return;
    rb->read_line(fd, line, sizeof(line));
    while (profile_count < OF_MAX_PROFILES &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[9];
        struct of_profile *profile = &profiles[profile_count];

        rb->memset(profile, 0, sizeof(*profile));
        of_split(line, field, 9);
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
        profile_count++;
    }
    rb->close(fd);
}

static bool of_read_bmp(const char *path, struct bitmap *bm, fb_data *pixels,
                        size_t bytes)
{
    bm->data = (unsigned char *)pixels;
    return path[0] && rb->read_bmp_file(path, bm, bytes,
                                        FORMAT_NATIVE, NULL) > 0;
}

static void of_load_assets(void)
{
    logo_loaded = of_read_bmp(OF_LOGO, &logo_bm, logo_pixels,
                              sizeof(logo_pixels));
    avatar_loaded = false;
    cover_loaded = false;
    profile_art_index = -1;
}

static int of_find_profile(const char *username)
{
    int index;

    for (index = 0; index < profile_count; index++)
        if (!rb->strcmp(profiles[index].username, username))
            return index;
    return -1;
}

static int of_find_my_profile(void)
{
    int index;

    for (index = 0; index < profile_count; index++)
        if (profiles[index].is_my_profile)
            return index;
    return -1;
}

static void of_load_profile_art(int index)
{
    if (index < 0 || index >= profile_count)
    {
        avatar_loaded = false;
        cover_loaded = false;
        profile_art_index = -1;
        return;
    }
    if (index == profile_art_index && avatar_loaded && cover_loaded)
        return;
    avatar_loaded = of_read_bmp(profiles[index].avatar,
                                &avatar_bm, avatar_pixels,
                                sizeof(avatar_pixels));
    /* Cover and full photo viewer never draw concurrently, so they reuse the
     * same fixed 128,000-byte buffer instead of claiming playback memory. */
    cover_loaded = of_read_bmp(profiles[index].cover,
                               &cover_bm, viewer_pixels,
                               sizeof(viewer_pixels));
    profile_art_index = index;
}

static void of_select_profile(int index)
{
    if (index < 0 || index >= profile_count)
        return;
    active_profile = index;
    of_load_profile_page(index, 0);
    selection = 0;
    of_load_profile_art(active_profile);
}

static bool of_change_profile_page(int direction, bool select_last)
{
    int offset;

    if (direction > 0)
    {
        if (profile_post_offset + visible_count >= profile_post_total)
            return false;
        offset = profile_post_offset + OF_MAX_POSTS;
    }
    else
    {
        if (profile_post_offset <= 0)
            return false;
        offset = MAX(0, profile_post_offset - OF_MAX_POSTS);
    }
    of_load_profile_page(active_profile, offset);
    if (visible_count <= 0)
        return false;
    selection = select_last ? visible_count - 1 : 0;
    return true;
}

static struct of_post *of_current_post(void)
{
    if (selection < 0 || selection >= visible_count)
        return NULL;
    return &posts[visible_posts[selection]];
}

static bool of_load_viewer_level(int zoom, int pan)
{
    char path[MAX_PATH];
    struct of_post *post = of_current_post();
    char *extension;
    static const char * const pan_names[] = {
        "", "up", "right", "down", "left"
    };

    if (!post || rb->strcmp(post->type, "photo") || !post->display[0] ||
        zoom < 0 || zoom >= OF_ZOOM_LEVELS)
        return false;
    rb->strlcpy(path, post->display, sizeof(path));
    if (zoom > 0)
    {
        extension = rb->strrchr(path, '.');
        if (!extension)
            return false;
        if (pan > OF_PAN_CENTER && pan <= OF_PAN_LEFT)
            rb->snprintf(extension, sizeof(path) - (extension - path),
                         ".zoom%d.%s.bmp", zoom, pan_names[pan]);
        else
            rb->snprintf(extension, sizeof(path) - (extension - path),
                         ".zoom%d.bmp", zoom);
    }
    if (!of_read_bmp(path, &viewer_bm, viewer_pixels,
                     sizeof(viewer_pixels)))
    {
        if (zoom <= 0 || pan == OF_PAN_CENTER)
            return false;
        rb->strlcpy(path, post->display, sizeof(path));
        extension = rb->strrchr(path, '.');
        if (!extension)
            return false;
        rb->snprintf(extension, sizeof(path) - (extension - path),
                     ".zoom%d.bmp", zoom);
        if (!of_read_bmp(path, &viewer_bm, viewer_pixels,
                         sizeof(viewer_pixels)))
            return false;
    }
    viewer_zoom = zoom;
    viewer_pan = zoom > 0 ? pan : OF_PAN_CENTER;
    return true;
}

static bool of_step_viewer_photo(int direction)
{
    int original_offset = profile_post_offset;
    int original_selection = selection;

    while (true)
    {
        int next = selection + direction;
        struct of_post *candidate;

        if (next < 0 || next >= visible_count)
        {
            if (!of_change_profile_page(direction, direction < 0))
                break;
            next = selection;
        }
        selection = next;
        candidate = &posts[visible_posts[selection]];
        if (!rb->strcmp(candidate->type, "photo"))
        {
            if (of_load_viewer_level(0, OF_PAN_CENTER))
                return true;
        }
    }
    of_load_profile_page(active_profile, original_offset);
    selection = MIN(original_selection, MAX(visible_count - 1, 0));
    return false;
}

static void of_draw_wrapped_text(int x, int y, int width, int max_lines,
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

static void of_cache_thumbnails(void)
{
    int slot;
    int first = selection;

    for (slot = 0; slot < 2; slot++)
    {
        int ordinal = first + slot;
        int index = ordinal < visible_count ? visible_posts[ordinal] : -1;
        int cached;

        if (thumb_index[slot] == index)
            continue;
        for (cached = slot + 1; cached < 2; cached++)
        {
            if (thumb_index[cached] == index)
            {
                struct bitmap swap_bm = thumb_bm[slot];
                int swap_index = thumb_index[slot];

                thumb_bm[slot] = thumb_bm[cached];
                thumb_index[slot] = thumb_index[cached];
                thumb_bm[cached] = swap_bm;
                thumb_index[cached] = swap_index;
                break;
            }
        }
        if (thumb_index[slot] == index)
            continue;
        thumb_index[slot] = -1;
        if (index >= 0 && of_read_bmp(posts[index].thumbnail,
                                     &thumb_bm[slot], thumb_pixels[slot],
                                     sizeof(thumb_pixels[slot])))
            thumb_index[slot] = index;
    }
}

static void of_header(void)
{
    struct of_profile *profile = &profiles[active_profile];

    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 68);
    if (logo_loaded)
        rb->lcd_bitmap_part((const fb_data *)logo_bm.data, 0, 8, logo_bm.width,
                            2, 1, 40, 24);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(OF_BLUE);
    rb->lcd_putsxy(44, 5, "OnlyFans");
    rb->lcd_putsxy(259, 5, "PROFILE");
    rb->lcd_set_foreground(OF_BORDER);
    rb->lcd_hline(0, LCD_WIDTH - 1, 25);
    if (cover_loaded)
        rb->lcd_bitmap_part((const fb_data *)cover_bm.data,
                            MAX(0, (cover_bm.width - 160) / 2),
                            MAX(0, (cover_bm.height - 42) / 2),
                            cover_bm.width, 160, 26, 160, 42);
    if (avatar_loaded)
        rb->lcd_bitmap((const fb_data *)avatar_bm.data, 7, 28,
                       avatar_bm.width, avatar_bm.height);
    else
    {
        rb->lcd_set_foreground(OF_LIGHT_BLUE);
        rb->lcd_fillrect(7, 28, OF_AVATAR_W, OF_AVATAR_H);
    }
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(55, 31, profile->is_my_profile ? "My Profile" :
                   (profile->display_name[0] ? profile->display_name :
                   "Offline profile"));
    rb->lcd_set_foreground(OF_GRAY);
    rb->lcd_putsxyf(55, 48, "@%s", profile->username);
    rb->lcd_set_foreground(OF_BORDER);
    rb->lcd_hline(0, LCD_WIDTH - 1, 68);
}

static void of_draw_browser(void)
{
    int slot;

    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    of_header();
    if (visible_count == 0)
    {
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(18, 91,
                       profiles[active_profile].is_my_profile &&
                       profiles[active_profile].display_name[0] ?
                       profiles[active_profile].display_name :
                       "No synced posts");
        rb->lcd_set_foreground(OF_GRAY);
        if (profiles[active_profile].bio[0])
            of_draw_wrapped_text(18, 111, 284, 4,
                                 profiles[active_profile].bio);
        rb->lcd_putsxy(18, 180, profiles[active_profile].is_my_profile ?
                       "Add photos or videos in RockPod" :
                       "Sync this profile from RockPod");
        rb->lcd_putsxy(18, 218, "Menu: Profiles");
        rb->lcd_update();
        return;
    }
    for (slot = 0; slot < 2; slot++)
    {
        int ordinal = selection + slot;
        int index = ordinal < visible_count ? visible_posts[ordinal] : -1;
        int y = 71 + slot * 83;
        struct of_post *post;

        if (index < 0)
            break;
        post = &posts[index];
        rb->lcd_set_foreground(slot == 0 ? OF_SELECTED : LCD_WHITE);
        rb->lcd_fillrect(3, y, 314, 80);
        rb->lcd_set_foreground(OF_BORDER);
        rb->lcd_drawrect(3, y, 314, 80);
        if (slot == 0)
        {
            rb->lcd_set_foreground(OF_BLUE);
            rb->lcd_fillrect(3, y, 3, 80);
        }
        if (thumb_index[slot] == index)
            rb->lcd_bitmap((const fb_data *)thumb_bm[slot].data, 9, y + 4,
                           thumb_bm[slot].width, thumb_bm[slot].height);
        rb->lcd_set_drawmode(DRMODE_FG);
        rb->lcd_set_foreground(OF_BLUE);
        rb->lcd_putsxyf(111, y + 7, "@%s", post->username);
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(111, y + 26, post->title);
        rb->lcd_set_foreground(OF_GRAY);
        rb->lcd_putsxy(111, y + 45,
                       !rb->strcmp(post->type, "video") ?
                       "Video  -  Select to play" : "Photo  -  Select to view");
        rb->lcd_putsxyf(111, y + 63, "%d of %d",
                        profile_post_offset + ordinal + 1,
                        profile_post_total);
    }
    rb->lcd_update();
}

static void of_draw_viewer(void)
{
    struct of_post *post = of_current_post();
    static const char * const zoom_labels[OF_ZOOM_LEVELS] = {
        "1x", "1.5x", "2x", "3x"
    };

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    if (viewer_loaded)
        rb->lcd_bitmap((const fb_data *)viewer_bm.data, 0, 22,
                       viewer_bm.width, viewer_bm.height);
    rb->lcd_set_foreground(OF_BLUE);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 22);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(7, 4, post ? post->username : "OnlyFans");
    rb->lcd_putsxyf(180, 4, "%d/%d",
                    profile_post_offset + selection + 1,
                    profile_post_total);
    rb->lcd_putsxy(230, 4, zoom_labels[viewer_zoom]);
    rb->lcd_putsxy(282, 4, "Back");
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, 222, LCD_WIDTH, 18);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(7, 224, "Touch: Pan  Select: Zoom  Left/Right: Photo");
    rb->lcd_update();
}

static bool of_restore_path(const char *path)
{
    char line[OF_LINE_SIZE];
    char username[64];
    int fd = rb->open(OF_LIBRARY, O_RDONLY);
    int ordinal = 0;
    int owner;

    username[0] = '\0';
    if (fd < 0)
        return false;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[8];

        of_split(line, field, 8);
        if (!rb->strcmp(field[5], path))
        {
            rb->strlcpy(username, field[1], sizeof(username));
            break;
        }
    }
    rb->close(fd);
    owner = of_find_profile(username);
    if (owner < 0)
        return false;

    fd = rb->open(OF_LIBRARY, O_RDONLY);
    if (fd < 0)
        return false;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[8];

        of_split(line, field, 8);
        if (rb->strcmp(field[1], username))
            continue;
        if (!rb->strcmp(field[5], path))
            break;
        ordinal++;
    }
    rb->close(fd);
    profile_selection = owner;
    active_profile = owner;
    of_load_profile_page(owner, (ordinal / OF_MAX_POSTS) * OF_MAX_POSTS);
    selection = ordinal - profile_post_offset;
    of_load_profile_art(owner);
    return selection >= 0 && selection < visible_count;
}

#ifdef HAVE_WHEEL_POSITION
static bool of_touch_pan(void)
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
    case 0: pan = OF_PAN_UP; break;
    case 1: pan = OF_PAN_RIGHT; break;
    case 2: pan = OF_PAN_DOWN; break;
    default: pan = OF_PAN_LEFT; break;
    }
    if (pan == viewer_pan)
        return false;
    return of_load_viewer_level(viewer_zoom, pan);
}
#endif

static void of_draw_profiles(void)
{
    int row;
    int top = profile_selection > 2 ? profile_selection - 2 : 0;

    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    if (logo_loaded)
        rb->lcd_bitmap_part((const fb_data *)logo_bm.data, 0, 8,
                            logo_bm.width, 4, 1, 40, 24);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(OF_BLUE);
    rb->lcd_putsxy(46, 5, "OnlyFans Profiles");
    rb->lcd_set_foreground(OF_BORDER);
    rb->lcd_hline(0, LCD_WIDTH - 1, 27);
    if (profile_count == 0)
    {
        rb->lcd_set_foreground(OF_GRAY);
        rb->lcd_putsxy(41, 112, "Sync a profile from RockPod");
        rb->lcd_update();
        return;
    }
    for (row = 0; row < 3 && top + row < profile_count; row++)
    {
        int index = top + row;
        int y = 31 + row * 62;
        struct of_profile *profile = &profiles[index];

        rb->lcd_set_foreground(index == profile_selection ?
                               OF_SELECTED : LCD_WHITE);
        rb->lcd_fillrect(4, y, 312, 58);
        rb->lcd_set_foreground(OF_BORDER);
        rb->lcd_drawrect(4, y, 312, 58);
        if (index == profile_selection)
        {
            rb->lcd_set_foreground(OF_BLUE);
            rb->lcd_fillrect(4, y, 4, 58);
        }
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(16, y + 8, profile->is_my_profile ?
                       "My Profile" : (profile->display_name[0] ?
                       profile->display_name : profile->username));
        rb->lcd_set_foreground(OF_BLUE);
        rb->lcd_putsxyf(16, y + 27, "@%s", profile->username);
        rb->lcd_set_foreground(OF_GRAY);
        rb->lcd_putsxyf(184, y + 8, "%d posts", profile->posts);
        rb->lcd_putsxyf(184, y + 27, "%d photos  %d videos",
                        profile->photos, profile->videos);
    }
    rb->lcd_set_foreground(OF_GRAY);
    rb->lcd_putsxy(16, 222, "Wheel: Browse   Select: Open   Menu: Exit");
    rb->lcd_update();
}

enum plugin_status plugin_start(const void *parameter)
{
    bool running = true;
    bool redraw = true;
    enum of_screen screen = OF_SCREEN_PROFILES;

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_RGB565)
    rb->lcd_set_mode(LCD_MODE_RGB565);
#endif
    rb->lcd_set_drawmode(DRMODE_SOLID);
    /* Reuse the already-loaded UI font. Loading a plugin font here could
     * shrink playback's shared audio buffer on hardware. */
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_backdrop(NULL);
    of_load_profiles();
    of_load_assets();
    profile_selection = 0;
    selection = 0;
    {
        int owner = of_find_my_profile();
        if (owner >= 0)
            profile_selection = owner;
    }
    if (parameter && !rb->strncmp((const char *)parameter, "return:", 7))
    {
        if (of_restore_path((const char *)parameter + 7))
            screen = OF_SCREEN_POSTS;
    }
    if (screen == OF_SCREEN_POSTS)
        of_cache_thumbnails();
    while (running)
    {
        int action;
        int timeout = TIMEOUT_BLOCK;

        if (redraw)
        {
            if (screen == OF_SCREEN_VIEWER)
                of_draw_viewer();
            else if (screen == OF_SCREEN_POSTS)
                of_draw_browser();
            else
                of_draw_profiles();
            redraw = false;
        }
#ifdef HAVE_WHEEL_POSITION
        if (screen == OF_SCREEN_VIEWER && viewer_zoom > 0)
            timeout = MAX(1, HZ / 12);
#endif
        action = pluginlib_getaction(timeout, contexts,
                                     ARRAYLEN(contexts));
#ifdef HAVE_WHEEL_POSITION
        if (screen == OF_SCREEN_VIEWER && of_touch_pan())
        {
            redraw = true;
            continue;
        }
#endif
        switch (action)
        {
        case PLA_SCROLL_BACK:
        case PLA_SCROLL_BACK_REPEAT:
            if (screen == OF_SCREEN_VIEWER && viewer_zoom > 0)
            {
                if (of_load_viewer_level(viewer_zoom - 1, OF_PAN_CENTER))
                    redraw = true;
            }
            else if (screen == OF_SCREEN_PROFILES && profile_selection > 0)
            {
                profile_selection--;
                redraw = true;
            }
            else if (screen == OF_SCREEN_POSTS && selection > 0)
            {
                selection--;
                of_cache_thumbnails();
                redraw = true;
            }
            else if (screen == OF_SCREEN_POSTS &&
                     of_change_profile_page(-1, true))
            {
                of_cache_thumbnails();
                redraw = true;
            }
            break;
        case PLA_SCROLL_FWD:
        case PLA_SCROLL_FWD_REPEAT:
            if (screen == OF_SCREEN_VIEWER &&
                viewer_zoom + 1 < OF_ZOOM_LEVELS)
            {
                if (of_load_viewer_level(viewer_zoom + 1, OF_PAN_CENTER))
                    redraw = true;
            }
            else if (screen == OF_SCREEN_PROFILES &&
                profile_selection + 1 < profile_count)
            {
                profile_selection++;
                redraw = true;
            }
            else if (screen == OF_SCREEN_POSTS &&
                     selection + 1 < visible_count)
            {
                selection++;
                of_cache_thumbnails();
                redraw = true;
            }
            else if (screen == OF_SCREEN_POSTS &&
                     of_change_profile_page(1, false))
            {
                of_cache_thumbnails();
                redraw = true;
            }
            break;
        case PLA_SELECT_REL:
            if (screen == OF_SCREEN_PROFILES && profile_count > 0)
            {
                of_select_profile(profile_selection);
                of_cache_thumbnails();
                screen = OF_SCREEN_POSTS;
                redraw = true;
            }
            else if (screen == OF_SCREEN_POSTS && visible_count > 0)
            {
                struct of_post *post = of_current_post();

                if (post && !rb->strcmp(post->type, "video") &&
                    post->media[0])
                {
                    static char launch[MAX_PATH + 16];
                    rb->snprintf(launch, sizeof(launch), OF_PREFIX "%s",
                                 post->media);
                    return rb->plugin_open(OF_PLAYER, launch);
                }
                viewer_zoom = 0;
                viewer_loaded = of_load_viewer_level(0, OF_PAN_CENTER);
                if (viewer_loaded)
                {
                    /* The viewer reuses the cover buffer; force a single
                     * reload only when Menu returns to the profile browser. */
                    cover_loaded = false;
                    profile_art_index = -1;
                    screen = OF_SCREEN_VIEWER;
#ifdef HAVE_WHEEL_POSITION
                    rb->wheel_send_events(false);
#endif
                    redraw = true;
                }
            }
            else if (screen == OF_SCREEN_VIEWER)
            {
                int next_zoom = (viewer_zoom + 1) % OF_ZOOM_LEVELS;
                viewer_loaded = of_load_viewer_level(next_zoom,
                                                     OF_PAN_CENTER) ||
                                viewer_loaded;
                redraw = true;
            }
            break;
        case PLA_LEFT:
        case PLA_LEFT_REPEAT:
            if (screen == OF_SCREEN_VIEWER)
            {
                viewer_loaded = of_step_viewer_photo(-1) || viewer_loaded;
                redraw = true;
            }
            break;
        case PLA_RIGHT:
        case PLA_RIGHT_REPEAT:
            if (screen == OF_SCREEN_VIEWER)
            {
                viewer_loaded = of_step_viewer_photo(1) || viewer_loaded;
                redraw = true;
            }
            break;
        case PLA_CANCEL:
            if (screen == OF_SCREEN_VIEWER)
            {
                screen = OF_SCREEN_POSTS;
                viewer_loaded = false;
#ifdef HAVE_WHEEL_POSITION
                rb->wheel_send_events(true);
#endif
                of_load_profile_art(active_profile);
                of_cache_thumbnails();
                redraw = true;
            }
            else if (screen == OF_SCREEN_POSTS)
            {
                screen = OF_SCREEN_PROFILES;
                avatar_loaded = false;
                redraw = true;
            }
            else
                running = false;
            break;
        case PLA_EXIT:
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
