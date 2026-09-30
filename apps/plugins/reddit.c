/***************************************************************************
 * Reddit: standalone offline subreddit reader for RockPod-synchronised data.
 * Uses Reddit's official synced logo and bounded framebuffer storage.
 ***************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/video_player.h"

#if !defined(HAVE_LCD_COLOR) || LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error The Reddit application requires a 320x240 colour display
#endif

#define RD_ROOT          ROCKBOX_DIR "/reddit"
#define RD_LIBRARY       RD_ROOT "/library.tsv"
#define RD_SUBREDDITS    RD_ROOT "/subreddits.tsv"
#define RD_LOGO          RD_ROOT "/assets/reddit-logo.bmp"
#define RD_PREFIX        "reddit-app:"
#define RD_MAX_POSTS     96
#define RD_MAX_SUBS      24
#define RD_LINE_SIZE     1400
/* Backward-compatible capacity for pre-2010-pass 96x72 cache entries. */
#define RD_THUMB_W       96
#define RD_THUMB_H       72
#define RD_VIEW_W        320
#define RD_VIEW_H        200
#define RD_ORANGE        LCD_RGBPACK(0xff, 0x45, 0x00)
#define RD_DARK          LCD_RGBPACK(0x1a, 0x1a, 0x1b)
#define RD_GRAY          LCD_RGBPACK(0x78, 0x7c, 0x7e)
#define RD_BORDER        LCD_RGBPACK(0xd7, 0xda, 0xdc)
#define RD_CLASSIC_BLUE  LCD_RGBPACK(0x5f, 0x99, 0xcf)
#define RD_HEADER_BLUE   LCD_RGBPACK(0xce, 0xe3, 0xf8)
#define RD_SELECTED      LCD_RGBPACK(0xef, 0xf7, 0xff)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping rd_context[] = {
    { PLA_SCROLL_BACK, BUTTON_SCROLL_BACK, BUTTON_NONE },
    { PLA_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT, BUTTON_SCROLL_FWD|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SELECT_REL, BUTTON_SELECT|BUTTON_REL, BUTTON_NONE },
    { PLA_LEFT, BUTTON_LEFT, BUTTON_NONE },
    { PLA_RIGHT, BUTTON_RIGHT, BUTTON_NONE },
    { PLA_CANCEL, BUTTON_MENU, BUTTON_NONE },
    { PLA_EXIT, BUTTON_PLAY|BUTTON_REL, BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *contexts[] = { rd_context };
#else
static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

struct rd_post {
    char id[32], subreddit[32], type[8], title[104], body[184], author[64];
    char media[MAX_PATH], thumbnail[MAX_PATH], display[MAX_PATH];
    int score, comments, ratio;
    char created[20], flair[52];
    bool stickied;
};

struct rd_subreddit {
    char name[32], display_name[48], description[184];
    int subscribers, posts;
};

static struct rd_post posts[RD_MAX_POSTS];
static struct rd_subreddit subs[RD_MAX_SUBS];
static int post_count, sub_count, sub_selection, post_selection, active_sub = -1;
static fb_data thumb_pixels[2][RD_THUMB_W * RD_THUMB_H];
static struct bitmap thumb_bm[2];
static int thumb_index[2] = {-1, -1};
static fb_data viewer_pixels[RD_VIEW_W * RD_VIEW_H];
static struct bitmap viewer_bm;
static bool viewer_loaded;
static fb_data logo_pixels[40 * 40];
static struct bitmap logo_bm;
static bool logo_loaded;

static void rd_split(char *line, char **fields, int count)
{
    int found = 1;
    char *cursor = line;
    fields[0] = line;
    while (*cursor && found < count) {
        if (*cursor == '\t') { *cursor = '\0'; fields[found++] = cursor + 1; }
        cursor++;
    }
    while (found < count) fields[found++] = "";
    cursor = fields[count - 1];
    while (*cursor && *cursor != '\r' && *cursor != '\n') cursor++;
    *cursor = '\0';
}

static bool rd_bmp(const char *path, struct bitmap *bm, fb_data *pixels, size_t bytes)
{
    bm->data = (unsigned char *)pixels;
    return path[0] && rb->read_bmp_file(path, bm, bytes, FORMAT_NATIVE, NULL) > 0;
}

static void rd_load_subreddits(void)
{
    int fd = rb->open(RD_SUBREDDITS, O_RDONLY);
    char line[RD_LINE_SIZE];
    sub_count = 0;
    if (fd < 0) return;
    rb->read_line(fd, line, sizeof(line));
    while (sub_count < RD_MAX_SUBS && rb->read_line(fd, line, sizeof(line)) > 0) {
        char *f[5];
        struct rd_subreddit *s = &subs[sub_count];
        rd_split(line, f, 5);
        rb->memset(s, 0, sizeof(*s));
        rb->strlcpy(s->name, f[0], sizeof(s->name));
        rb->strlcpy(s->display_name, f[1], sizeof(s->display_name));
        rb->strlcpy(s->description, f[2], sizeof(s->description));
        s->subscribers = rb->atoi(f[3]); s->posts = rb->atoi(f[4]);
        sub_count++;
    }
    rb->close(fd);
}

static void rd_load_posts(int index)
{
    int fd = rb->open(RD_LIBRARY, O_RDONLY);
    char line[RD_LINE_SIZE];
    post_count = 0; thumb_index[0] = thumb_index[1] = -1;
    if (fd < 0 || index < 0 || index >= sub_count) return;
    rb->read_line(fd, line, sizeof(line));
    while (post_count < RD_MAX_POSTS && rb->read_line(fd, line, sizeof(line)) > 0) {
        char *f[15];
        struct rd_post *p;
        rd_split(line, f, 15);
        if (rb->strcmp(f[1], subs[index].name)) continue;
        p = &posts[post_count++]; rb->memset(p, 0, sizeof(*p));
        rb->strlcpy(p->id, f[0], sizeof(p->id));
        rb->strlcpy(p->subreddit, f[1], sizeof(p->subreddit));
        rb->strlcpy(p->type, f[2], sizeof(p->type));
        rb->strlcpy(p->title, f[3], sizeof(p->title));
        rb->strlcpy(p->body, f[4], sizeof(p->body));
        rb->strlcpy(p->author, f[5], sizeof(p->author));
        rb->strlcpy(p->media, f[6], sizeof(p->media));
        rb->strlcpy(p->thumbnail, f[7], sizeof(p->thumbnail));
        rb->strlcpy(p->display, f[8], sizeof(p->display));
        p->score = rb->atoi(f[9]); p->comments = rb->atoi(f[10]); p->ratio = rb->atoi(f[11]);
        rb->strlcpy(p->created, f[12], sizeof(p->created));
        rb->strlcpy(p->flair, f[13], sizeof(p->flair)); p->stickied = rb->atoi(f[14]) != 0;
    }
    rb->close(fd);
}

static void rd_text(const char *text, int x, int y, int max_w, fb_data color)
{
    char shown[128]; int width, height, len;
    rb->strlcpy(shown, text && text[0] ? text : " ", sizeof(shown));
    len = rb->strlen(shown);
    rb->lcd_getstringsize(shown, &width, &height);
    while (width > max_w && len > 3) {
        len--; shown[len] = '\0';
        rb->lcd_getstringsize(shown, &width, &height);
    }
    if (len < (int)rb->strlen(text) && len > 2) { shown[len-1] = '.'; shown[len-2] = '.'; }
    rb->lcd_set_foreground(color); rb->lcd_putsxy(x, y, shown);
}

static void rd_header(const char *title, bool back)
{
    rb->lcd_set_background(LCD_WHITE); rb->lcd_clear_display();
    rb->lcd_set_foreground(RD_HEADER_BLUE); rb->lcd_fillrect(0, 0, LCD_WIDTH, 32);
    rb->lcd_set_foreground(RD_CLASSIC_BLUE); rb->lcd_hline(0, LCD_WIDTH-1, 31);
    rb->lcd_set_foreground(RD_DARK);
    if (back) rb->lcd_putsxy(7, 9, "<");
    rb->lcd_putsxy(back ? 24 : 10, 9, title);
    if (logo_loaded) rb->lcd_bitmap((fb_data *)logo_bm.data, LCD_WIDTH-29, 3,
                                    logo_bm.width, logo_bm.height);
}

static void rd_scrollbar(int top, int height, int selected, int total)
{
    int thumb_height, thumb_y;
    if (total <= 1) return;
    rb->lcd_set_foreground(RD_BORDER); rb->lcd_fillrect(LCD_WIDTH-4, top, 2, height);
    thumb_height = MAX(14, height / MIN(total, 8));
    thumb_y = top + ((height-thumb_height) * selected) / (total-1);
    rb->lcd_set_foreground(RD_CLASSIC_BLUE); rb->lcd_fillrect(LCD_WIDTH-5, thumb_y, 3, thumb_height);
}

static void rd_draw_subreddits(void)
{
    int first, row;
    rd_header("Reddit", false);
    if (!sub_count) {
        rd_text("No communities synced", 24, 92, 272, RD_DARK);
        rd_text("Add r/ipod in RockPod", 24, 116, 272, RD_GRAY);
    } else {
        first = sub_selection > 1 ? sub_selection - 1 : 0;
        for (row = 0; row < 3 && first + row < sub_count; row++) {
            int index = first + row, y = 36 + row * 63;
            struct rd_subreddit *s = &subs[index];
            if (index == sub_selection) { rb->lcd_set_foreground(RD_SELECTED); rb->lcd_fillrect(0, y, LCD_WIDTH, 62); }
            rd_text(s->display_name, 14, y+8, 210, index == sub_selection ? RD_CLASSIC_BLUE : RD_DARK);
            {
                char meta[80]; rb->snprintf(meta, sizeof(meta), "%d readers  |  %d posts", s->subscribers, s->posts);
                rd_text(meta, 14, y+29, 285, RD_GRAY);
            }
            rb->lcd_set_foreground(RD_BORDER); rb->lcd_drawline(10, y+61, LCD_WIDTH-10, y+61);
            rd_text(">", LCD_WIDTH-20, y+20, 12, RD_GRAY);
        }
        rd_scrollbar(36, 188, sub_selection, sub_count);
    }
    rd_text("MENU Back     SELECT Open", 8, LCD_HEIGHT-15, 250, RD_GRAY); rb->lcd_update();
}

static void rd_load_thumb(int slot, int index)
{
    if (slot < 0 || slot > 1 || index < 0 || index >= post_count) return;
    if (thumb_index[slot] == index) return;
    thumb_index[slot] = rd_bmp(posts[index].thumbnail, &thumb_bm[slot], thumb_pixels[slot], sizeof(thumb_pixels[slot])) ? index : -2;
}

static void rd_draw_posts(void)
{
    int first = post_selection > 0 ? post_selection - 1 : 0, row;
    rd_header(subs[active_sub].display_name, true);
    for (row = 0; row < 2 && first + row < post_count; row++) {
        int index = first + row, y = 36 + row * 91, text_x = 38;
        struct rd_post *p = &posts[index];
        if (index == post_selection) { rb->lcd_set_foreground(RD_SELECTED); rb->lcd_fillrect(0, y, LCD_WIDTH, 90); }
        rb->lcd_set_foreground(RD_ORANGE); rb->lcd_putsxy(12, y+9, "^");
        {
            char score[16]; rb->snprintf(score, sizeof(score), "%d", p->score);
            rd_text(score, 7, y+31, 28, RD_GRAY);
        }
        rd_load_thumb(row, index);
        if (thumb_index[row] == index) {
            rb->lcd_bitmap((fb_data *)thumb_bm[row].data, 36, y+9,
                           thumb_bm[row].width, thumb_bm[row].height);
            text_x = 36 + thumb_bm[row].width + 6;
        }
        rd_text(p->title, text_x, y+7, LCD_WIDTH-text_x-10, RD_CLASSIC_BLUE);
        {
            char by[86], stats[86];
            rb->snprintf(by, sizeof(by), "u/%s%s", p->author, p->stickied ? "  [PINNED]" : "");
            rb->snprintf(stats, sizeof(stats), "%d comments  %d%%", p->comments, p->ratio);
            rd_text(by, text_x, y+31, LCD_WIDTH-text_x-8, p->stickied ? RD_ORANGE : RD_GRAY);
            rd_text(stats, text_x, y+53, LCD_WIDTH-text_x-8, RD_GRAY);
            if (p->flair[0]) rd_text(p->flair, text_x, y+70, LCD_WIDTH-text_x-8, RD_ORANGE);
        }
        rb->lcd_set_foreground(RD_BORDER); rb->lcd_drawline(6, y+89, LCD_WIDTH-6, y+89);
    }
    rd_scrollbar(36, 182, post_selection, post_count);
    rd_text("MENU Back     SELECT Open", 8, LCD_HEIGHT-15, 250, RD_GRAY); rb->lcd_update();
}

static void rd_draw_detail(void)
{
    struct rd_post *p = &posts[post_selection];
    int y = 41, offset = 0, line = 0, length = rb->strlen(p->body), chunk;
    char text[52], meta[96];
    rd_header("Post", true); rd_text(p->title, 10, y, 300, RD_DARK); y += 25;
    rb->snprintf(meta, sizeof(meta), "u/%s  ^%d  %d comments", p->author, p->score, p->comments);
    rd_text(meta, 10, y, 300, RD_CLASSIC_BLUE); y += 24;
    while (offset < length && line < 5) {
        chunk = MIN(48, length-offset); rb->memcpy(text, p->body+offset, chunk); text[chunk] = '\0';
        rd_text(text, 10, y + line*22, 300, RD_DARK); offset += chunk; line++;
    }
    if (!length) rd_text(p->type[0] ? p->type : "link", 10, y, 300, RD_GRAY);
    rd_text("MENU: posts", 8, LCD_HEIGHT-15, 150, RD_GRAY); rb->lcd_update();
}

static void rd_draw_viewer(void)
{
    rd_header("Image", true);
    if (viewer_loaded) rb->lcd_bitmap((fb_data *)viewer_bm.data, 0, 36, RD_VIEW_W, RD_VIEW_H);
    else rd_text("Image unavailable", 85, 112, 180, RD_GRAY);
    rb->lcd_update();
}

static void rd_open_post(void)
{
    struct rd_post *p = &posts[post_selection];
    if (!rb->strcmp(p->type, "video") && p->media[0]) {
        char parameter[MAX_PATH + sizeof(RD_PREFIX)];
        rb->snprintf(parameter, sizeof(parameter), RD_PREFIX "%s", p->media);
        rb->plugin_open(plugin_video_player_for(p->media), parameter);
    } else if (!rb->strcmp(p->type, "photo") && p->display[0]) {
        viewer_loaded = rd_bmp(p->display, &viewer_bm, viewer_pixels, sizeof(viewer_pixels));
        rd_draw_viewer();
        while (1) {
            int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts, ARRAYLEN(contexts));
            if (action == PLA_CANCEL || action == PLA_LEFT || action == PLA_EXIT || action == SYS_USB_CONNECTED) break;
        }
    } else {
        rd_draw_detail();
        while (1) {
            int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts, ARRAYLEN(contexts));
            if (action == PLA_CANCEL || action == PLA_LEFT || action == PLA_EXIT || action == SYS_USB_CONNECTED) break;
        }
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    bool in_posts = false;
    (void)parameter;
    rb->lcd_setfont(FONT_UI);
    logo_loaded = rd_bmp(RD_LOGO, &logo_bm, logo_pixels, sizeof(logo_pixels));
    rd_load_subreddits();
    while (1) {
        int action;
        if (in_posts) rd_draw_posts(); else rd_draw_subreddits();
        action = pluginlib_getaction(TIMEOUT_BLOCK, contexts, ARRAYLEN(contexts));
        if (action == SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        if (action == PLA_EXIT) return PLUGIN_OK;
        if (action == PLA_CANCEL || action == PLA_LEFT) {
            if (in_posts) { in_posts = false; active_sub = -1; }
            else return PLUGIN_OK;
        } else if (action == PLA_SCROLL_BACK) {
            if (in_posts && post_selection > 0) post_selection--;
            else if (!in_posts && sub_selection > 0) sub_selection--;
        } else if (action == PLA_SCROLL_FWD) {
            if (in_posts && post_selection + 1 < post_count) post_selection++;
            else if (!in_posts && sub_selection + 1 < sub_count) sub_selection++;
        } else if (action == PLA_SELECT_REL || action == PLA_RIGHT) {
            if (!in_posts && sub_count) { active_sub = sub_selection; rd_load_posts(active_sub); post_selection = 0; in_posts = true; }
            else if (in_posts && post_count) rd_open_post();
        }
    }
}
