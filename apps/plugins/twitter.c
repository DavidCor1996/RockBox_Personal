/***************************************************************************
 * Twitter: offline classic timeline built from RockPod's synced X posts.
 * The wordmark and application icon come from the original Twitter vector.
 ***************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/video_player.h"

#if !defined(HAVE_LCD_COLOR) || LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error The Twitter application requires a 320x240 colour display
#endif

#define TW_ROOT       ROCKBOX_DIR "/twitter"
#define TW_LIBRARY    TW_ROOT "/library.tsv"
#define TW_ACCOUNTS   TW_ROOT "/accounts.tsv"
#define TW_LOGO       TW_ROOT "/assets/twitter-logo.bmp"
#define TW_MAX_POSTS  500
#define TW_MAX_USERS  24
#define TW_LINE_SIZE  4096
#define TW_MEDIA_MAX  4
#define TW_BLUE       LCD_RGBPACK(0x3d, 0xa4, 0xd9)
#define TW_DARK_BLUE  LCD_RGBPACK(0x19, 0x6f, 0xa3)
#define TW_PALE       LCD_RGBPACK(0xe8, 0xf5, 0xfb)
#define TW_DARK       LCD_RGBPACK(0x22, 0x27, 0x2a)
#define TW_GRAY       LCD_RGBPACK(0x78, 0x82, 0x88)
#define TW_BORDER     LCD_RGBPACK(0xc9, 0xd4, 0xd9)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping tw_context[] = {
    { PLA_SCROLL_BACK, BUTTON_SCROLL_BACK, BUTTON_NONE },
    { PLA_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT, BUTTON_SCROLL_FWD|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SELECT_REL, BUTTON_SELECT|BUTTON_REL, BUTTON_NONE },
    { PLA_SELECT_REPEAT, BUTTON_SELECT|BUTTON_REPEAT, BUTTON_SELECT },
    { PLA_LEFT, BUTTON_LEFT, BUTTON_NONE },
    { PLA_RIGHT, BUTTON_RIGHT, BUTTON_NONE },
    { PLA_CANCEL, BUTTON_MENU, BUTTON_NONE },
    { PLA_EXIT, BUTTON_PLAY|BUTTON_REL, BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *contexts[] = { tw_context };
#else
static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

struct tw_account {
    char handle[20], name[72], avatar[MAX_PATH];
    int posts;
};

struct tw_post {
    char id[32], handle[20], name[72], date[24], text[2048];
    int replies, retweets, likes;
    char avatar[MAX_PATH], thumbnail[MAX_PATH];
    char media[TW_MEDIA_MAX][MAX_PATH];
    char reply_id[32], quote_id[32];
};

static struct tw_account accounts[TW_MAX_USERS];
static struct tw_post posts[TW_MAX_POSTS];
static int account_count, post_count, account_selection, post_selection;
static int active_account = -1, detail_scroll;
static fb_data avatar_pixels[2][40 * 40];
static struct bitmap avatar_bm[2];
static int avatar_index[2] = {-1, -1};
static fb_data thumb_pixels[2][296 * 84];
static struct bitmap thumb_bm[2];
static int thumb_index[2] = {-1, -1};
static fb_data viewer_pixels[320 * 180];
static struct bitmap viewer_bm;
static fb_data logo_pixels[90 * 16];
static struct bitmap logo_bm;
static bool logo_loaded;
static int feed_filter, visible[TW_MAX_POSTS], visible_count, feed_selection;
static char saved_ids[TW_MAX_POSTS][32];
static int saved_count;
static int text_height;
static int tw_font = FONT_UI;

static void tw_split(char *line, char **fields, int count)
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
}

static bool tw_bmp(const char *path, struct bitmap *bm, fb_data *pixels,
                   size_t bytes)
{
    bm->data = (unsigned char *)pixels;
    return path[0] && rb->read_bmp_file(path, bm, bytes,
                                         FORMAT_NATIVE, NULL) > 0;
}

static void tw_accounts_load(void)
{
    int fd = rb->open(TW_ACCOUNTS, O_RDONLY);
    char line[TW_LINE_SIZE];
    account_count = 0;
    if (fd < 0)
        return;
    rb->read_line(fd, line, sizeof(line));
    while (account_count < TW_MAX_USERS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *f[4];
        struct tw_account *a = &accounts[account_count];
        tw_split(line, f, 4);
        rb->memset(a, 0, sizeof(*a));
        rb->strlcpy(a->handle, f[0], sizeof(a->handle));
        rb->strlcpy(a->name, f[1], sizeof(a->name));
        a->posts = rb->atoi(f[2]);
        rb->strlcpy(a->avatar, f[3], sizeof(a->avatar));
        account_count++;
    }
    rb->close(fd);
}

static void tw_posts_load(int account)
{
    int fd = rb->open(TW_LIBRARY, O_RDONLY);
    char line[TW_LINE_SIZE];
    post_count = 0;
    avatar_index[0] = avatar_index[1] = -1;
    thumb_index[0] = thumb_index[1] = -1;
    if (fd < 0)
        return;
    if (account < 0 || account >= account_count)
    {
        rb->close(fd);
        return;
    }
    rb->read_line(fd, line, sizeof(line));
    while (post_count < TW_MAX_POSTS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *f[17];
        struct tw_post *p;
        tw_split(line, f, 17);
        if (rb->strcmp(f[1], accounts[account].handle))
            continue;
        p = &posts[post_count++];
        rb->memset(p, 0, sizeof(*p));
        rb->strlcpy(p->id, f[0], sizeof(p->id));
        rb->strlcpy(p->handle, f[16][0] ? f[16] : f[1], sizeof(p->handle));
        rb->strlcpy(p->name, f[2], sizeof(p->name));
        rb->strlcpy(p->date, f[3], sizeof(p->date));
        rb->strlcpy(p->text, f[4], sizeof(p->text));
        p->replies = rb->atoi(f[5]);
        p->retweets = rb->atoi(f[6]);
        p->likes = rb->atoi(f[7]);
        rb->strlcpy(p->avatar, f[8], sizeof(p->avatar));
        rb->strlcpy(p->thumbnail, f[9], sizeof(p->thumbnail));
        for (int i = 0; i < TW_MEDIA_MAX; i++)
            rb->strlcpy(p->media[i], f[10 + i], sizeof(p->media[i]));
        rb->strlcpy(p->reply_id, f[14], sizeof(p->reply_id));
        rb->strlcpy(p->quote_id, f[15], sizeof(p->quote_id));
    }
    rb->close(fd);
}

/* All glyphs are transparent foreground pixels. Inherited LCD backdrops and
 * solid text backgrounds otherwise cover the blue bars and selected cards. */
static void tw_text(const char *value, int x, int y, int width, fb_data color)
{
    char shown[256];
    int w, h, length;
    rb->strlcpy(shown, value ? value : "", sizeof(shown));
    length = rb->strlen(shown);
    rb->lcd_getstringsize(shown, &w, &h);
    while (w > width && length > 0)
    {
        shown[--length] = '\0';
        rb->lcd_getstringsize(shown, &w, &h);
    }
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy(x, y, shown);
}

static int tw_wrap(const char *text, int skip, int x, int y,
                   int width, int lines)
{
    char line[256];
    int offset = 0, row = 0, drawn = 0;
    while (text[offset])
    {
        int n = 0, space = -1, w = 0, h;
        while (text[offset + n] && n < (int)sizeof(line) - 1)
        {
            line[n] = text[offset + n];
            line[n + 1] = '\0';
            rb->lcd_getstringsize(line, &w, &h);
            if (w > width && n > 0)
                break;
            if (line[n] == ' ')
                space = n;
            n++;
        }
        if (text[offset + n] && space > 0)
            n = space;
        if (!n)
            n = 1;
        line[n] = '\0';
        if (row >= skip && drawn < lines)
        {
            tw_text(line, x, y + drawn * text_height, width, TW_DARK);
            drawn++;
        }
        offset += n;
        while (text[offset] == ' ')
            offset++;
        row++;
    }
    return row;
}

static void tw_header(const char *title, bool back)
{
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_set_foreground(TW_BLUE);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 30);
    rb->lcd_set_foreground(TW_DARK_BLUE);
    rb->lcd_hline(0, LCD_WIDTH - 1, 29);
    tw_text(title, back ? 26 : 10, 6, 184, LCD_WHITE);
    if (back)
        tw_text("<", 8, 6, 14, LCD_WHITE);
    if (logo_loaded)
        rb->lcd_bitmap((fb_data *)logo_bm.data, 220, 7, 90, 16);
}

static void tw_footer(const char *hint)
{
    rb->lcd_set_foreground(TW_DARK);
    rb->lcd_fillrect(0, 220, LCD_WIDTH, 20);
    rb->lcd_setfont(FONT_SYSFIXED);
    tw_text(hint, 7, 226, 306, LCD_WHITE);
    rb->lcd_setfont(tw_font);
    rb->lcd_update();
}

static bool tw_saved(const char *id)
{
    for (int i = 0; i < saved_count; i++)
        if (!rb->strcmp(id, saved_ids[i]))
            return true;
    return false;
}

static void tw_saved_load(void)
{
    int fd = rb->open(TW_ROOT "/saved.txt", O_RDONLY);
    saved_count = 0;
    if (fd < 0)
        return;
    while (saved_count < TW_MAX_POSTS && rb->read_line(fd,
           saved_ids[saved_count], sizeof(saved_ids[0])) > 0)
        saved_count++;
    rb->close(fd);
}

static void tw_toggle_saved(void)
{
    const char *id = posts[post_selection].id;
    int i, fd;
    for (i = 0; i < saved_count; i++)
        if (!rb->strcmp(id, saved_ids[i]))
            break;
    if (i < saved_count)
    {
        for (; i + 1 < saved_count; i++)
            rb->strlcpy(saved_ids[i], saved_ids[i + 1], sizeof(saved_ids[i]));
        saved_count--;
    }
    else if (saved_count < TW_MAX_POSTS)
        rb->strlcpy(saved_ids[saved_count++], id, sizeof(saved_ids[0]));
    fd = rb->open(TW_ROOT "/saved.tmp", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd < 0)
    {
        rb->splash(HZ, "Could not save tweet");
        tw_saved_load();
        return;
    }
    for (i = 0; i < saved_count; i++)
        rb->fdprintf(fd, "%s\n", saved_ids[i]);
    rb->close(fd);
    rb->rename(TW_ROOT "/saved.tmp", TW_ROOT "/saved.txt");
}

static void tw_filter(void)
{
    visible_count = 0;
    for (int i = 0; i < post_count; i++)
        if (feed_filter == 0 ||
            (feed_filter == 1 && posts[i].media[0][0]) ||
            (feed_filter == 2 && tw_saved(posts[i].id)))
            visible[visible_count++] = i;
    feed_selection = MIN(feed_selection, MAX(0, visible_count - 1));
    if (visible_count)
        post_selection = visible[feed_selection];
}

static void tw_prepare_post(void)
{
    int i = post_selection;
    if (!visible_count)
        return;
    if (avatar_index[0] != i)
        avatar_index[0] = tw_bmp(posts[i].avatar, &avatar_bm[0],
            avatar_pixels[0], sizeof(avatar_pixels[0])) ? i : -2;
    if (thumb_index[0] != i)
        thumb_index[0] = tw_bmp(posts[i].thumbnail, &thumb_bm[0],
            thumb_pixels[0], sizeof(thumb_pixels[0])) ? i : -2;
}

static void tw_draw_accounts(void)
{
    int first = account_selection > 1 ? account_selection - 1 : 0;
    tw_header("Profiles", false);
    if (!account_count)
        tw_text("Import a profile in RockPod", 18, 100, 285, TW_DARK);
    for (int row = 0; row < 3 && first + row < account_count; row++)
    {
        int i = first + row, y = 34 + row * 60;
        char info[80];
        if (i == account_selection)
        {
            rb->lcd_set_foreground(TW_PALE);
            rb->lcd_fillrect(0, y, 320, 59);
        }
        tw_text(accounts[i].name, 12, y + 6, 290, TW_DARK);
        rb->snprintf(info, sizeof(info), "@%s  /  %d tweets",
                     accounts[i].handle, accounts[i].posts);
        tw_text(info, 12, y + 30, 290, TW_DARK_BLUE);
    }
    tw_footer("MENU Exit                  SELECT Open profile");
}

static void tw_author(struct tw_post *p, int y)
{
    char by[80];
    if (avatar_index[0] == post_selection)
        rb->lcd_bitmap((fb_data *)avatar_bm[0].data, 10, y, 40, 40);
    tw_text(p->name, 58, y, 248, TW_DARK);
    rb->snprintf(by, sizeof(by), "@%s", p->handle);
    tw_text(by, 58, y + text_height, 248, TW_GRAY);
}

static void tw_draw_timeline(void)
{
    static const char *tabs[] = {"Tweets", "Media", "Saved"};
    char stats[96], title[80];
    rb->snprintf(title, sizeof(title), "@%s", accounts[active_account].handle);
    tw_header(title, true);
    for (int i = 0; i < 3; i++)
    {
        tw_text(tabs[i], 12 + i * 104, 33, 94,
                i == feed_filter ? TW_DARK_BLUE : TW_GRAY);
        if (i == feed_filter)
        {
            rb->lcd_set_foreground(TW_BLUE);
            rb->lcd_fillrect(i * 104 + 7, 51, 94, 2);
        }
    }
    if (!visible_count)
    {
        tw_text("No tweets in this view", 20, 106, 280, TW_GRAY);
        tw_footer("LEFT/RIGHT Tabs                MENU Profiles");
        return;
    }
    struct tw_post *p = &posts[post_selection];
    bool preview = thumb_index[0] == post_selection;
    tw_author(p, 60);
    int lines = preview ? MAX(1, 36 / text_height) : MAX(1, 93 / text_height);
    tw_wrap(p->text, 0, 12, 103, 296, lines);
    if (preview)
        rb->lcd_bitmap((fb_data *)thumb_bm[0].data, 12, 141,
                       thumb_bm[0].width, MIN(60, thumb_bm[0].height));
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->snprintf(stats, sizeof(stats), "%d RT  %d favorites  %s  %d/%d",
                 p->retweets, p->likes, tw_saved(p->id) ? "Saved" : "",
                 feed_selection + 1, visible_count);
    tw_text(stats, 12, 207, 296, TW_GRAY);
    rb->lcd_setfont(tw_font);
    tw_footer("Wheel Browse   L/R Tabs   SELECT Read (hold options)");
}

static int tw_draw_detail(void)
{
    struct tw_post *p = &posts[post_selection];
    char info[100];
    tw_header("Tweet", true);
    tw_author(p, 37);
    rb->lcd_setfont(FONT_SYSFIXED);
    tw_text(p->date, 12, 82, 295, TW_GRAY);
    rb->lcd_setfont(tw_font);
    int lines = MAX(1, 103 / text_height);
    int total = tw_wrap(p->text, detail_scroll, 12, 97, 296, lines);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->snprintf(info, sizeof(info), "%d replies  %d retweets  %d favorites",
                 p->replies, p->retweets, p->likes);
    tw_text(info, 12, 207, 296, TW_DARK_BLUE);
    rb->lcd_setfont(tw_font);
    tw_footer(p->media[0][0] ? "MENU Feed   Wheel Scroll   SELECT Photos / Video" :
              "MENU Feed   Wheel Scroll   Hold SELECT Options");
    return MAX(0, total - lines);
}

static bool tw_video(const char *path)
{
    return plugin_video_extension_is(path, "mpg") ||
           plugin_video_extension_is(path, "m4v");
}

static enum plugin_status tw_view_media(void)
{
    struct tw_post *p = &posts[post_selection];
    int selected = 0, count = 0, loaded_index = -1;
    bool loaded = false;
    while (count < TW_MEDIA_MAX && p->media[count][0])
        count++;
    while (count)
    {
        char title[48];
        bool video = tw_video(p->media[selected]);
        if (!video && loaded_index != selected)
        {
            loaded = tw_bmp(p->media[selected], &viewer_bm,
                            viewer_pixels, sizeof(viewer_pixels));
            loaded_index = selected;
        }
        rb->snprintf(title, sizeof(title), "%s %d / %d",
                     video ? "Video" : "Photo", selected + 1, count);
        tw_header(title, true);
        if (!video && loaded)
            rb->lcd_bitmap((fb_data *)viewer_bm.data, 0, 35,
                           viewer_bm.width, viewer_bm.height);
        else if (video)
        {
            if (thumb_index[0] == post_selection)
                rb->lcd_bitmap((fb_data *)thumb_bm[0].data, 12, 70,
                               thumb_bm[0].width, thumb_bm[0].height);
            tw_text("Press SELECT to play video", 20, 178, 285, TW_DARK_BLUE);
        }
        else
            tw_text("Photo unavailable", 45, 110, 240, TW_GRAY);
        tw_footer("LEFT/RIGHT Attachments             MENU Tweet");
        int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts, ARRAYLEN(contexts));
        if (action == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
        if (action == PLA_CANCEL || action == PLA_EXIT)
            return PLUGIN_OK;
        if (action == PLA_SELECT_REL && video)
        {
            static char launch[MAX_PATH + 16];
            rb->snprintf(launch, sizeof(launch), "twitter-app:%s", p->media[selected]);
            return rb->plugin_open(plugin_video_player_for(p->media[selected]), launch);
        }
        if ((action == PLA_RIGHT || action == PLA_SCROLL_FWD) && selected + 1 < count)
            selected++;
        if ((action == PLA_LEFT || action == PLA_SCROLL_BACK) && selected > 0)
            selected--;
    }
    return PLUGIN_OK;
}

static void tw_options(void)
{
    MENUITEM_STRINGLIST(menu, "Tweet", NULL, "Save / unsave offline",
                       "Open quoted tweet", "Open replied-to tweet");
    int selected = 0;
    int choice = rb->do_menu(&menu, &selected, NULL, false);
    if (choice == 0)
        tw_toggle_saved();
    else if (choice == 1 || choice == 2)
    {
        const char *id = choice == 1 ? posts[post_selection].quote_id :
                                      posts[post_selection].reply_id;
        for (int i = 0; i < post_count; i++)
            if (!rb->strcmp(id, posts[i].id))
            {
                post_selection = i;
                detail_scroll = 0;
                return;
            }
        rb->splash(HZ, "This tweet is not in the synced library");
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    enum { VIEW_ACCOUNTS, VIEW_TIMELINE, VIEW_DETAIL } view = VIEW_ACCOUNTS;
    int w, detail_max = 0;
    rb->lcd_setfont(tw_font);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_getstringsize("Ag", &w, &text_height);
    if (text_height > 18)
    {
        tw_font = FONT_SYSFIXED;
        rb->lcd_setfont(tw_font);
        rb->lcd_getstringsize("Ag", &w, &text_height);
    }
    text_height += 2;
    logo_loaded = tw_bmp(TW_LOGO, &logo_bm, logo_pixels, sizeof(logo_pixels));
    tw_accounts_load();
    tw_saved_load();
    if (account_count == 1 || parameter)
    {
        active_account = 0;
        tw_posts_load(active_account);
        tw_filter();
        view = VIEW_TIMELINE;
    }
    if (parameter && !rb->strncmp(parameter, "return:", 7))
    {
        const char *path = (const char *)parameter + 7;
        for (int a = 0; a < account_count; a++)
        {
            tw_posts_load(a);
            for (int i = 0; i < post_count; i++)
                for (int m = 0; m < TW_MEDIA_MAX; m++)
                    if (!rb->strcmp(path, posts[i].media[m]))
                    {
                        active_account = a;
                        feed_selection = i;
                        tw_filter();
                        view = VIEW_DETAIL;
                        goto restored;
                    }
        }
    }
restored:
    while (1)
    {
        if (view == VIEW_ACCOUNTS)
            tw_draw_accounts();
        else
        {
            tw_prepare_post();
            if (view == VIEW_TIMELINE)
                tw_draw_timeline();
            else
                detail_max = tw_draw_detail();
        }
        int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts, ARRAYLEN(contexts));
        if (action == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
        if (action == PLA_EXIT)
            return PLUGIN_OK;
        if (action == PLA_SELECT_REPEAT && view != VIEW_ACCOUNTS && visible_count)
        {
            tw_options();
            view = VIEW_DETAIL;
        }
        else if (action == PLA_CANCEL)
        {
            if (view == VIEW_DETAIL)
            {
                tw_filter();
                view = VIEW_TIMELINE;
            }
            else if (view == VIEW_TIMELINE)
                view = VIEW_ACCOUNTS;
            else
                return PLUGIN_OK;
        }
        else if ((action == PLA_LEFT || action == PLA_RIGHT) && view == VIEW_TIMELINE)
        {
            feed_filter = (feed_filter + (action == PLA_RIGHT ? 1 : 2)) % 3;
            feed_selection = 0;
            tw_filter();
        }
        else if (action == PLA_SCROLL_BACK || action == PLA_SCROLL_BACK_REPEAT)
        {
            if (view == VIEW_ACCOUNTS && account_selection > 0)
                account_selection--;
            else if (view == VIEW_TIMELINE && feed_selection > 0)
            {
                feed_selection--;
                post_selection = visible[feed_selection];
            }
            else if (view == VIEW_DETAIL && detail_scroll > 0)
                detail_scroll--;
        }
        else if (action == PLA_SCROLL_FWD || action == PLA_SCROLL_FWD_REPEAT)
        {
            if (view == VIEW_ACCOUNTS && account_selection + 1 < account_count)
                account_selection++;
            else if (view == VIEW_TIMELINE && feed_selection + 1 < visible_count)
            {
                feed_selection++;
                post_selection = visible[feed_selection];
            }
            else if (view == VIEW_DETAIL && detail_scroll < detail_max)
                detail_scroll++;
        }
        else if (action == PLA_SELECT_REL)
        {
            if (view == VIEW_ACCOUNTS && account_count)
            {
                active_account = account_selection;
                tw_posts_load(active_account);
                feed_selection = 0;
                tw_filter();
                view = VIEW_TIMELINE;
            }
            else if (view == VIEW_TIMELINE && visible_count)
            {
                detail_scroll = 0;
                view = VIEW_DETAIL;
            }
            else if (view == VIEW_DETAIL && posts[post_selection].media[0][0])
            {
                enum plugin_status rc = tw_view_media();
                if (rc != PLUGIN_OK)
                    return rc;
            }
        }
    }
}
