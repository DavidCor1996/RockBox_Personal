/***************************************************************************
 * nightcity - UI rendering and interaction
 ***************************************************************************/

#include "nightcity.h"
#include "nc_audio.h"

static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)
#include "pluginbitmaps/nightcity_title.h"
#include "pluginbitmaps/nightcity_citycard.h"
#include "pluginbitmaps/nightcity_ghostcard.h"
#include "pluginbitmaps/nightcity_cliniccard.h"
#include "pluginbitmaps/nightcity_boardcard.h"
#include "pluginbitmaps/nightcity_convoycard.h"
#include "pluginbitmaps/nightcity_relaycard.h"
#include "pluginbitmaps/nightcity_afterglowcard.h"
#include "pluginbitmaps/nightcity_panorama.h"
#include "pluginbitmaps/nightcity_glitch.h"
#include "pluginbitmaps/nightcity_portraits.h"
#include "pluginbitmaps/nightcity_sentinel.h"
#include "pluginbitmaps/nightcity_aegis.h"
#include "pluginbitmaps/nightcity_modules.h"
#include "pluginbitmaps/nightcity_endings.h"
#define NIGHTCITY_USE_BITMAP_ASSETS 1
#else
#define NIGHTCITY_USE_BITMAP_ASSETS 0
#endif

#if LCD_DEPTH > 1
#define NC_BG LCD_RGBPACK(0x08, 0x0b, 0x12)
#define NC_PANEL LCD_RGBPACK(0x11, 0x17, 0x23)
#define NC_PANEL_ALT LCD_RGBPACK(0x16, 0x1e, 0x2e)
#define NC_CYAN LCD_RGBPACK(0x23, 0xcf, 0xe1)
#define NC_MAGENTA LCD_RGBPACK(0xff, 0x46, 0xa7)
#define NC_RED LCD_RGBPACK(0xf2, 0x5f, 0x5c)
#define NC_TEXT LCD_RGBPACK(0xe8, 0xef, 0xf7)
#define NC_MUTED LCD_RGBPACK(0x7f, 0x8b, 0x9b)
#define NC_GREEN LCD_RGBPACK(0x57, 0xd6, 0x9a)
#else
#define NC_BG 0
#define NC_PANEL 0
#define NC_PANEL_ALT 0
#define NC_CYAN 0
#define NC_MAGENTA 0
#define NC_RED 0
#define NC_TEXT 0
#define NC_MUTED 0
#define NC_GREEN 0
#endif

static int font_height;
static int font_width;

static void fit_text_to_width(const char *text, int max_width,
                              char *out, size_t out_size);
static void put_text_box_left(int x, int y, int max_width,
                              const char *text, long color);
static void put_text_box_right(int right_x, int y, int max_width,
                               const char *text, long color);
static void put_text_box_center(int center_x, int y, int max_width,
                                const char *text, long color);

#define NC_PORTRAIT_W 72
#define NC_PORTRAIT_H 96
#define NC_SCENE_PORTRAIT_H 72
#define NC_PANORAMA_W 240
#define NC_PANORAMA_VIEW_W 206
#define NC_PANORAMA_H 66
#define NC_GLITCH_W 320
#define NC_GLITCH_H 32

struct menu_screen
{
    const char *title;
    const char *subtitle;
    const char *const *items;
    const char *const *descriptions;
    int count;
    bool allow_cancel;
};

enum scene_theme
{
    SCENE_THEME_CITY = 0,
    SCENE_THEME_GHOST,
    SCENE_THEME_CLINIC,
    SCENE_THEME_BOARDROOM,
    SCENE_THEME_CONVOY,
    SCENE_THEME_RELAY,
    SCENE_THEME_AFTERGLOW,
};

enum speaker_glyph
{
    GLYPH_VESPER = 0,
    GLYPH_JUNO,
    GLYPH_MIRA,
    GLYPH_SABLE,
    GLYPH_ROOK,
    GLYPH_KADE,
    GLYPH_HOSTILE,
    GLYPH_NYRA,
};

enum ending_card
{
    ENDING_CARD_REBEL = 0,
    ENDING_CARD_CORP,
    ENDING_CARD_GHOST,
    ENDING_CARD_COST,
};

enum portrait_id
{
    PORTRAIT_VESPER = 0,
    PORTRAIT_JUNO,
    PORTRAIT_MIRA,
    PORTRAIT_SABLE,
    PORTRAIT_ROOK,
    PORTRAIT_KADE,
    PORTRAIT_HOSTILE,
    PORTRAIT_NYRA,
    PORTRAIT_PROFILE_RAZOR,
    PORTRAIT_PROFILE_VELVET,
    PORTRAIT_PROFILE_DRIFT,
};

static void set_colors(void)
{
#if LCD_DEPTH > 1
    rb->lcd_set_background(NC_BG);
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

void nc_ui_init(void)
{
    rb->lcd_setfont(FONT_UI);
    rb->font_getstringsize("M", &font_width, &font_height, FONT_UI);
    set_colors();
}

int nc_ui_input(int timeout)
{
    return pluginlib_getaction(timeout, plugin_contexts, ARRAYLEN(plugin_contexts));
}

void nc_ui_fill_background(void)
{
    int i;
    int tick = *rb->current_tick;

    rb->lcd_clear_display();
#if LCD_DEPTH > 1
    rb->lcd_set_background(NC_BG);
    rb->lcd_set_foreground(NC_PANEL_ALT);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_set_foreground(NC_PANEL);
    for (i = (tick / 2) % 18; i < LCD_WIDTH; i += 18)
        rb->lcd_vline(i, 0, LCD_HEIGHT);
    rb->lcd_set_foreground(NC_PANEL);
    for (i = (tick / 3) % 16; i < LCD_HEIGHT; i += 16)
        rb->lcd_hline(0, LCD_WIDTH, i);
    rb->lcd_set_foreground(NC_BG);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

void nc_ui_frame(const char *title, const char *subtitle)
{
    char subtitle_buf[NC_MAX_LINE_CHARS];
    int subtitle_w = 0;
    int title_max_w;

    nc_ui_fill_background();

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_drawline(10, 10, LCD_WIDTH - 10, 10);
    rb->lcd_drawline(10, 34, LCD_WIDTH - 10, 34);
    rb->lcd_set_foreground(NC_MAGENTA);
    rb->lcd_drawline(10, 35, 68, 35);
    rb->lcd_set_foreground(NC_TEXT);
#endif

    if (subtitle != NULL && subtitle[0] != '\0')
    {
        fit_text_to_width(subtitle, LCD_WIDTH / 2 - 24, subtitle_buf, sizeof(subtitle_buf));
        rb->font_getstringsize(subtitle_buf, &subtitle_w, NULL, FONT_UI);
        put_text_box_right(LCD_WIDTH - 12, 15, LCD_WIDTH / 2 - 24, subtitle_buf, NC_MUTED);
    }

    title_max_w = LCD_WIDTH - 24 - (subtitle_w > 0 ? subtitle_w + 16 : 0);
    put_text_box_left(12, 15, title_max_w, title, NC_TEXT);
}

void nc_ui_box(int x, int y, int w, int h, bool selected)
{
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(selected ? NC_MAGENTA : NC_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(selected ? NC_CYAN : NC_MUTED);
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_set_foreground(NC_TEXT);
#else
    (void)selected;
    rb->lcd_drawrect(x, y, w, h);
#endif
}

void nc_ui_meter(int x, int y, int w, int value, int max_value, bool danger)
{
    int fill = (w - 2) * value / MAX(1, max_value);

    rb->lcd_drawrect(x, y, w, 8);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(danger ? NC_RED : NC_GREEN);
#endif
    rb->lcd_fillrect(x + 1, y + 1, fill, 6);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

void nc_ui_footer(const char *left, const char *center, const char *right)
{
    int y = LCD_HEIGHT - font_height - 8;

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(0, LCD_HEIGHT - font_height - 12, LCD_WIDTH, font_height + 12);
    rb->lcd_set_foreground(NC_MUTED);
    rb->lcd_drawline(0, LCD_HEIGHT - font_height - 13, LCD_WIDTH, LCD_HEIGHT - font_height - 13);
    rb->lcd_set_foreground(NC_TEXT);
#endif

    if (left != NULL)
        put_text_box_left(8, y, 92, left, NC_TEXT);
    if (center != NULL)
        put_text_box_center(LCD_WIDTH / 2, y, 104, center, NC_TEXT);
    if (right != NULL)
        put_text_box_right(LCD_WIDTH - 8, y, 92, right, NC_TEXT);
}

void nc_ui_update(void)
{
    rb->lcd_update();
}

static int append_line(char lines[][NC_MAX_LINE_CHARS], int max_lines,
                       int count, const char *line)
{
    if (count >= max_lines)
        return count;
    rb->strlcpy(lines[count], line, NC_MAX_LINE_CHARS);
    return count + 1;
}

static void fit_text_to_width(const char *text, int max_width,
                              char *out, size_t out_size)
{
    int width = 0;
    size_t len;

    if (text == NULL || out_size == 0)
        return;

    rb->strlcpy(out, text, out_size);
    rb->font_getstringsize(out, &width, NULL, FONT_UI);
    if (width <= max_width)
        return;

    rb->strlcpy(out, text, out_size);
    len = rb->strlen(out);
    while (len > 3)
    {
        out[len - 3] = '.';
        out[len - 2] = '.';
        out[len - 1] = '.';
        out[len] = '\0';
        rb->font_getstringsize(out, &width, NULL, FONT_UI);
        if (width <= max_width)
            return;
        --len;
        out[len] = '\0';
    }

    rb->strlcpy(out, "...", out_size);
}

static void draw_text_backdrop(int x, int y, int width)
{
#if LCD_DEPTH > 1
    int box_x = MAX(0, x - 4);
    int box_y = MAX(0, y - 2);
    int box_w = MIN(LCD_WIDTH - box_x, width + 8);
    int box_h = font_height + 4;

    if (box_w <= 0 || box_h <= 0)
        return;

    rb->lcd_set_foreground(NC_BG);
    rb->lcd_fillrect(box_x, box_y, box_w, box_h);
    rb->lcd_set_foreground(NC_PANEL_ALT);
    rb->lcd_drawrect(box_x, box_y, box_w, box_h);
    rb->lcd_set_foreground(NC_TEXT);
#else
    (void)x;
    (void)y;
    (void)width;
#endif
}

static void put_text_box_left(int x, int y, int max_width, const char *text, long color)
{
    char fitted[NC_MAX_LINE_CHARS];
    int width = 0;

    fit_text_to_width(text, max_width, fitted, sizeof(fitted));
    rb->font_getstringsize(fitted, &width, NULL, FONT_UI);
    draw_text_backdrop(x, y, width);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(color);
#else
    (void)color;
#endif
    rb->lcd_putsxy(x, y, fitted);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

static void put_text_box_right(int right_x, int y, int max_width, const char *text, long color)
{
    char fitted[NC_MAX_LINE_CHARS];
    int width = 0;
    int x;

    fit_text_to_width(text, max_width, fitted, sizeof(fitted));
    rb->font_getstringsize(fitted, &width, NULL, FONT_UI);
    x = MAX(0, right_x - width);
    draw_text_backdrop(x, y, width);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(color);
#else
    (void)color;
#endif
    rb->lcd_putsxy(x, y, fitted);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

static void put_text_box_center(int center_x, int y, int max_width, const char *text, long color)
{
    char fitted[NC_MAX_LINE_CHARS];
    int width = 0;
    int x;

    fit_text_to_width(text, max_width, fitted, sizeof(fitted));
    rb->font_getstringsize(fitted, &width, NULL, FONT_UI);
    x = MAX(0, center_x - width / 2);
    draw_text_backdrop(x, y, width);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(color);
#else
    (void)color;
#endif
    rb->lcd_putsxy(x, y, fitted);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

int nc_ui_wrap_text(const char *text, int width,
                    char lines[][NC_MAX_LINE_CHARS], int max_lines)
{
    char current[NC_MAX_LINE_CHARS];
    char word[NC_MAX_LINE_CHARS];
    int count = 0;
    int current_len = 0;
    const char *p = text;

    current[0] = '\0';
    while (*p != '\0')
    {
        int word_len = 0;
        int candidate_w;

        if (*p == '\n')
        {
            if (current_len > 0)
            {
                count = append_line(lines, max_lines, count, current);
                current[0] = '\0';
                current_len = 0;
            }
            else
            {
                count = append_line(lines, max_lines, count, "");
            }
            ++p;
            continue;
        }

        while (*p == ' ')
            ++p;
        if (*p == '\0')
            break;
        if (*p == '\n')
            continue;

        while (*p != '\0' && *p != ' ' && *p != '\n' &&
               word_len < NC_MAX_LINE_CHARS - 1)
        {
            word[word_len++] = *p++;
        }
        word[word_len] = '\0';
        if (word_len == 0)
            continue;

        if (current_len == 0)
        {
            rb->strlcpy(current, word, sizeof(current));
            current_len = rb->strlen(current);
            continue;
        }

        {
            char candidate[NC_MAX_LINE_CHARS];
            rb->snprintf(candidate, sizeof(candidate), "%s %s", current, word);
            rb->font_getstringsize(candidate, &candidate_w, NULL, FONT_UI);
            if (candidate_w <= width)
            {
                rb->strlcpy(current, candidate, sizeof(current));
                current_len = rb->strlen(current);
            }
            else
            {
                count = append_line(lines, max_lines, count, current);
                rb->strlcpy(current, word, sizeof(current));
                current_len = rb->strlen(current);
            }
        }
    }

    if (current_len > 0)
        count = append_line(lines, max_lines, count, current);

    if (count == 0)
        count = append_line(lines, max_lines, count, "");
    return count;
}

static void draw_stat_line(int x, int y, const char *label, int value)
{
    char buf[40];
    rb->snprintf(buf, sizeof(buf), "%s %d", label, value);
    rb->lcd_putsxy(x, y, buf);
}

static void draw_title_background(void)
{
    int i;
    int tick = *rb->current_tick;

    nc_ui_fill_background();

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_PANEL_ALT);
    for (i = 0; i < 18; ++i)
    {
        int x = 14 + i * 16;
        int height = 18 + ((i * 13 + tick / 4) % 52);
        rb->lcd_fillrect(x, 138 - height, 10, height);
        if (((tick / 6) + i) % 3 == 0)
        {
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_fillrect(x + 2, 138 - height + 6, 2, 3);
            rb->lcd_set_foreground(NC_PANEL_ALT);
        }
    }
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_drawline(0, 144, LCD_WIDTH, 144);
    rb->lcd_set_foreground(NC_MAGENTA);
    rb->lcd_drawline(0, 148, LCD_WIDTH, 148);
    rb->lcd_set_foreground(NC_MAGENTA);
    rb->lcd_fillrect(212, 44, 74, 8);
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_fillrect(212, 56, 46 + ((tick / 3) % 24), 4);
    rb->lcd_set_foreground(NC_TEXT);
#endif

#if NIGHTCITY_USE_BITMAP_ASSETS
    rb->lcd_bitmap(nightcity_title, 0, 18, BMPWIDTH_nightcity_title, BMPHEIGHT_nightcity_title);
#endif

    put_text_box_left(18, 26, 180, "NIGHTCITY", NC_TEXT);
    put_text_box_left(18, 42, 220, "portable mercenary fiction", NC_CYAN);
    put_text_box_left(18, 58, 220, "for iPod Video / 5G clickwheel", NC_MUTED);
}

static enum scene_theme detect_scene_theme(const struct nc_node *node)
{
    if (rb->strcasestr(node->title, "Finale") != NULL ||
        rb->strcasestr(node->title, "Relay") != NULL)
        return SCENE_THEME_RELAY;
    if (rb->strcasestr(node->speaker, "Sable") != NULL)
        return SCENE_THEME_GHOST;
    if (rb->strcasestr(node->speaker, "Mira") != NULL ||
        rb->strcasestr(node->title, "Clinic") != NULL ||
        rb->strcasestr(node->title, "Surgery") != NULL)
        return SCENE_THEME_CLINIC;
    if (rb->strcasestr(node->speaker, "Kade") != NULL ||
        rb->strcasestr(node->title, "Private") != NULL)
        return SCENE_THEME_BOARDROOM;
    if (rb->strcasestr(node->speaker, "Nyra") != NULL ||
        rb->strcasestr(node->title, "Afterhours") != NULL ||
        rb->strcasestr(node->title, "Broadcast") != NULL)
        return SCENE_THEME_AFTERGLOW;
    if (rb->strcasestr(node->title, "Salt") != NULL ||
        rb->strcasestr(node->title, "Convoy") != NULL)
        return SCENE_THEME_CONVOY;
    if (rb->strcasestr(node->title, "Ghost") != NULL)
        return SCENE_THEME_GHOST;
    return SCENE_THEME_CITY;
}

static enum portrait_id detect_profile_portrait(const struct nc_game_state *state)
{
    if (state == NULL)
        return PORTRAIT_VESPER;

    switch (state->profile)
    {
        case NC_PROFILE_RAZOR:
            return PORTRAIT_PROFILE_RAZOR;
        case NC_PROFILE_VELVET:
            return PORTRAIT_PROFILE_VELVET;
        case NC_PROFILE_DRIFT:
            return PORTRAIT_PROFILE_DRIFT;
        default:
            return PORTRAIT_VESPER;
    }
}

static enum portrait_id detect_portrait(const struct nc_game_state *state,
                                        const char *speaker)
{
    if (speaker == NULL)
        return detect_profile_portrait(state);
    if (rb->strcasestr(speaker, "Juno") != NULL)
        return PORTRAIT_JUNO;
    if (rb->strcasestr(speaker, "Mira") != NULL)
        return PORTRAIT_MIRA;
    if (rb->strcasestr(speaker, "Sable") != NULL)
        return PORTRAIT_SABLE;
    if (rb->strcasestr(speaker, "Rook") != NULL)
        return PORTRAIT_ROOK;
    if (rb->strcasestr(speaker, "Kade") != NULL)
        return PORTRAIT_KADE;
    if (rb->strcasestr(speaker, "Nyra") != NULL)
        return PORTRAIT_NYRA;
    if (rb->strcasestr(speaker, "Sentinel") != NULL || rb->strcasestr(speaker, "Unknown") != NULL)
        return PORTRAIT_HOSTILE;
    return detect_profile_portrait(state);
}

static enum ending_card detect_ending_card(const struct nc_node *node)
{
    if (node == NULL || node->title == NULL)
        return ENDING_CARD_COST;
    if (rb->strcasestr(node->title, "Neon Rebellion") != NULL)
        return ENDING_CARD_REBEL;
    if (rb->strcasestr(node->title, "Boardroom Truce") != NULL)
        return ENDING_CARD_CORP;
    if (rb->strcasestr(node->title, "Ghostfire") != NULL)
        return ENDING_CARD_GHOST;
    return ENDING_CARD_COST;
}

static bool is_aegis_enemy(const struct nc_enemy *enemy)
{
    return enemy != NULL &&
           enemy->name != NULL &&
           rb->strcasestr(enemy->name, "Aegis") != NULL;
}

#if !NIGHTCITY_USE_BITMAP_ASSETS
static void draw_city_asset(int x, int y, int w, int h, int tick)
{
    int i;

    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(NC_MUTED);
    for (i = 0; i < 5; ++i)
    {
        int bx = x + 6 + i * 10;
        int bh = 10 + ((i * 7 + tick / 3) % 24);
        rb->lcd_fillrect(bx, y + h - bh - 8, 6, bh);
    }
    rb->lcd_set_foreground(NC_CYAN);
    for (i = 0; i < 4; ++i)
        rb->lcd_fillrect(x + 8 + ((tick / 2) + i * 12) % (w - 12), y + 8 + i * 10, 2, 2);
    rb->lcd_set_foreground(NC_MAGENTA);
    for (i = 0; i < 5; ++i)
        rb->lcd_drawline(x + ((tick / 2) + i * 9) % w, y + 4, x + ((tick / 2) + i * 9) % w - 6, y + h - 4);
    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_ghost_asset(int x, int y, int w, int h, int tick)
{
    int i;
    int mid = y + h / 2;

    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(NC_MAGENTA);
    for (i = 0; i < w - 8; i += 4)
    {
        int amp = ((i + tick / 2) % 14) - 7;
        rb->lcd_drawline(x + 4 + i, mid, x + 6 + i, mid + amp);
    }
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_drawrect(x + 8, y + 10, w - 16, h - 20);
    rb->lcd_drawline(x + 12, y + 18 + (tick / 3) % 12, x + w - 12, y + 18 + (tick / 3) % 12);
    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_clinic_asset(int x, int y, int w, int h, int tick)
{
    int pulse = (tick / 2) % (h - 16);

    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_drawrect(x + 8, y + 8, w - 16, h - 16);
    rb->lcd_drawline(x + w / 2, y + 14, x + w / 2, y + h - 14);
    rb->lcd_drawline(x + 14, y + h / 2, x + w - 14, y + h / 2);
    rb->lcd_set_foreground(NC_GREEN);
    rb->lcd_fillrect(x + 12, y + 8 + pulse, w - 24, 3);
    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_boardroom_asset(int x, int y, int w, int h, int tick)
{
    int i;

    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(NC_MUTED);
    for (i = 0; i < 4; ++i)
    {
        int bx = x + 6 + i * 12;
        rb->lcd_fillrect(bx, y + 8, 8, h - 16);
    }
    rb->lcd_set_foreground(NC_MAGENTA);
    rb->lcd_fillrect(x + 8, y + 14, w - 16, 6);
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_fillrect(x + 10 + (tick / 3) % (w - 24), y + 32, 10, 3);
    rb->lcd_fillrect(x + 10 + (tick / 4) % (w - 24), y + 48, 18, 3);
    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_convoy_asset(int x, int y, int w, int h, int tick)
{
    int i;
    int shift = (tick / 2) % 12;

    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(NC_MUTED);
    rb->lcd_drawline(x + 8, y + h - 12, x + w / 2, y + 20);
    rb->lcd_drawline(x + w - 8, y + h - 12, x + w / 2, y + 20);
    rb->lcd_set_foreground(NC_CYAN);
    for (i = 0; i < 4; ++i)
        rb->lcd_fillrect(x + w / 2 - 2, y + 26 + i * 10 + shift % 6, 4, 6);
    rb->lcd_set_foreground(NC_MAGENTA);
    rb->lcd_fillrect(x + 12, y + h - 22, 14, 8);
    rb->lcd_fillrect(x + w - 26, y + h - 22, 14, 8);
    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_relay_asset(int x, int y, int w, int h, int tick)
{
    int cx = x + w / 2;
    int cy = y + h / 2;
    int i;

    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(NC_MAGENTA);
    for (i = 0; i < 3; ++i)
    {
        int radius = 8 + i * 8 + ((tick / 4 + i) % 3);
        rb->lcd_drawrect(cx - radius, cy - radius, radius * 2, radius * 2);
    }
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_fillrect(cx - 2, cy - 2, 4, 4);
    rb->lcd_fillrect(cx + ((tick / 2) % 20) - 10, y + 10, 4, 4);
    rb->lcd_fillrect(x + 10, cy + ((tick / 3) % 16) - 8, 4, 4);
    rb->lcd_fillrect(x + w - 14, cy - ((tick / 3) % 16) + 8, 4, 4);
    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_speaker_glyph(int x, int y, enum speaker_glyph glyph, int tick)
{
    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, 34, 34);
    rb->lcd_set_foreground(NC_CYAN);
    rb->lcd_drawrect(x, y, 34, 34);

    switch (glyph)
    {
        case GLYPH_JUNO:
            rb->lcd_drawline(x + 8, y + 24, x + 17, y + 8);
            rb->lcd_drawline(x + 26, y + 24, x + 17, y + 8);
            rb->lcd_drawline(x + 11, y + 16, x + 23, y + 16);
            break;
        case GLYPH_MIRA:
            rb->lcd_drawline(x + 17, y + 8, x + 17, y + 26);
            rb->lcd_drawline(x + 8, y + 17, x + 26, y + 17);
            rb->lcd_set_foreground(NC_GREEN);
            rb->lcd_fillrect(x + 15, y + 10 + (tick / 3) % 10, 4, 4);
            break;
        case GLYPH_SABLE:
            rb->lcd_drawline(x + 8, y + 9, x + 14, y + 17);
            rb->lcd_drawline(x + 14, y + 17, x + 20, y + 11);
            rb->lcd_drawline(x + 20, y + 11, x + 26, y + 25);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 10 + (tick / 3) % 12, y + 24, 5, 3);
            break;
        case GLYPH_ROOK:
            rb->lcd_drawrect(x + 9, y + 10, 16, 12);
            rb->lcd_drawline(x + 8, y + 25, x + 17, y + 18);
            rb->lcd_drawline(x + 26, y + 25, x + 17, y + 18);
            break;
        case GLYPH_KADE:
            rb->lcd_drawrect(x + 9, y + 9, 16, 16);
            rb->lcd_drawline(x + 12, y + 13, x + 22, y + 13);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 13, y + 18, 8, 3);
            break;
        case GLYPH_HOSTILE:
            rb->lcd_drawline(x + 8, y + 24, x + 17, y + 8);
            rb->lcd_drawline(x + 26, y + 24, x + 17, y + 8);
            rb->lcd_drawline(x + 8, y + 24, x + 26, y + 24);
            rb->lcd_set_foreground(NC_RED);
            rb->lcd_fillrect(x + 15, y + 15, 4, 4);
            break;
        case GLYPH_NYRA:
            rb->lcd_drawline(x + 9, y + 24, x + 15, y + 9);
            rb->lcd_drawline(x + 15, y + 9, x + 25, y + 12);
            rb->lcd_drawline(x + 11, y + 24, x + 24, y + 24);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 19, y + 7, 7, 4);
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_fillrect(x + 8 + (tick / 4) % 10, y + 27, 6, 3);
            break;
        default:
            rb->lcd_drawrect(x + 10, y + 8, 14, 12);
            rb->lcd_drawline(x + 8, y + 26, x + 17, y + 20);
            rb->lcd_drawline(x + 26, y + 26, x + 17, y + 20);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 12, y + 12, 10, 3);
            break;
    }

    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_threat_asset(int x, int y, bool aegis, int tick)
{
    rb->lcd_set_foreground(NC_PANEL);
    rb->lcd_fillrect(x, y, 106, 64);
    rb->lcd_set_foreground(aegis ? NC_MAGENTA : NC_CYAN);
    rb->lcd_drawrect(x, y, 106, 64);
    rb->lcd_drawrect(x + 4, y + 4, 98, 56);
    if (aegis)
    {
        rb->lcd_set_foreground(NC_MUTED);
        rb->lcd_fillrect(x + 40, y + 16, 26, 20);
        rb->lcd_fillrect(x + 46, y + 38, 14, 12);
        rb->lcd_set_foreground(NC_CYAN);
        rb->lcd_drawline(x + 34, y + 56, x + 48, y + 40);
        rb->lcd_drawline(x + 72, y + 56, x + 58, y + 40);
        rb->lcd_drawline(x + 44, y + 16, x + 36, y + 8);
        rb->lcd_drawline(x + 62, y + 16, x + 70, y + 8);
        rb->lcd_set_foreground(NC_RED);
        rb->lcd_fillrect(x + 45 + (tick / 4) % 8, y + 27, 12, 3);
    }
    else
    {
        rb->lcd_set_foreground(NC_CYAN);
        rb->lcd_drawline(x + 18, y + 44, x + 50, y + 20);
        rb->lcd_drawline(x + 88, y + 44, x + 56, y + 20);
        rb->lcd_drawellipse(x + 53, y + 30, 18, 10);
        rb->lcd_set_foreground(NC_MAGENTA);
        rb->lcd_drawline(x + 20 + (tick / 3) % 56, y + 14, x + 12 + (tick / 3) % 56, y + 52);
        rb->lcd_set_foreground(NC_GREEN);
        rb->lcd_fillrect(x + 32, y + 48, 42, 3);
    }
    rb->lcd_set_foreground(NC_TEXT);
}

static void draw_deck_modules(int x, int y, const struct nc_game_state *state)
{
    int i;
    const unsigned items[] =
    {
        NC_CYBER_COMBAT_RIG,
        NC_CYBER_GHOSTWALL,
        NC_CYBER_SOCIAL,
    };

    for (i = 0; i < 3; ++i)
    {
        nc_ui_box(x + i * 32, y, 30, 34, (state->cyberware & items[i]) != 0);
        if ((state->cyberware & items[i]) == 0)
        {
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_BG);
            rb->lcd_fillrect(x + i * 32 + 4, y + 4, 22, 26);
            rb->lcd_set_foreground(NC_MUTED);
            rb->lcd_drawline(x + i * 32 + 4, y + 30, x + i * 32 + 26, y + 4);
            rb->lcd_set_foreground(NC_TEXT);
#endif
        }
    }
}

static void draw_ending_banner(int x, int y, enum ending_card card, int tick)
{
    nc_ui_box(x, y, 68, 56, false);
    switch (card)
    {
        case ENDING_CARD_REBEL:
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_fillrect(x + 10, y + 32, 10, 14);
            rb->lcd_fillrect(x + 25, y + 22, 10, 24);
            rb->lcd_fillrect(x + 40, y + 14, 10, 32);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_drawline(x + 8, y + 18, x + 58, y + 18);
            break;
        case ENDING_CARD_CORP:
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 12, y + 14, 44, 22);
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_drawrect(x + 16, y + 18, 36, 14);
            break;
        case ENDING_CARD_GHOST:
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_drawellipse(x + 34, y + 28, 16, 16);
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_drawline(x + 12, y + 42, x + 56, y + 14);
            rb->lcd_drawline(x + 12, y + 14, x + 56, y + 42);
            rb->lcd_set_foreground(NC_GREEN);
            rb->lcd_fillrect(x + 32, y + 24 + (tick / 4) % 6, 4, 4);
            break;
        default:
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_drawline(x + 10, y + 42, x + 34, y + 16);
            rb->lcd_drawline(x + 58, y + 42, x + 34, y + 16);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 24, y + 34, 20, 4);
            rb->lcd_set_foreground(NC_GREEN);
            rb->lcd_fillrect(x + 18, y + 44, 32, 3);
            break;
    }
    rb->lcd_set_foreground(NC_TEXT);
}

#endif

static const char *ending_relationship_tag(const struct nc_game_state *state)
{
    if (state->flags & NC_FLAG_ROMANCE_NYRA)
        return "Nyra kept the rooftop frequency alive.";
    if (state->flags & NC_FLAG_ROMANCE_MIRA)
        return "Mira kept your scar human.";
    if (state->flags & NC_FLAG_ROMANCE_ROOK)
        return "Rook kept a seat warm past the city.";
    if (state->flags & NC_FLAG_ROMANCE_JUNO)
        return "Juno left you one honest lie.";
    return NULL;
}

static const char *scene_theme_label(enum scene_theme theme)
{
    switch (theme)
    {
        case SCENE_THEME_GHOST:
            return "Ghost bleed";
        case SCENE_THEME_CLINIC:
            return "Clinic feed";
        case SCENE_THEME_BOARDROOM:
            return "Board trace";
        case SCENE_THEME_CONVOY:
            return "Salt lane";
        case SCENE_THEME_RELAY:
            return "Relay surge";
        case SCENE_THEME_AFTERGLOW:
            return "Afterglow";
        default:
            return "Night grid";
    }
}

static int ping_pong_offset(int tick, int range)
{
    int period;
    int phase;

    if (range <= 0)
        return 0;

    period = range * 2;
    if (period <= 0)
        return 0;

    phase = tick % period;
    return phase <= range ? phase : period - phase;
}

static void draw_scene_panorama(enum scene_theme theme, int x, int y, int w, int h, int tick)
{
    int i;

    nc_ui_box(x, y, w, h, false);
#if LCD_DEPTH > 1
#if NIGHTCITY_USE_BITMAP_ASSETS
    if (w == NC_PANORAMA_VIEW_W && h == NC_PANORAMA_H)
    {
        int scroll = ping_pong_offset(tick / 3, NC_PANORAMA_W - NC_PANORAMA_VIEW_W);
        rb->lcd_bitmap_part(nightcity_panorama,
                            theme * NC_PANORAMA_W + scroll, 0,
                            BMPWIDTH_nightcity_panorama,
                            x, y, NC_PANORAMA_VIEW_W, NC_PANORAMA_H);
        rb->lcd_set_foreground(theme == SCENE_THEME_AFTERGLOW ? NC_MAGENTA : NC_CYAN);
        rb->lcd_drawrect(x + 1, y + 1, w - 2, h - 2);
        rb->lcd_set_foreground(NC_TEXT);
        return;
    }
#endif
    rb->lcd_set_foreground(NC_PANEL_ALT);
    rb->lcd_fillrect(x + 1, y + 1, w - 2, h - 2);
    switch (theme)
    {
        case SCENE_THEME_GHOST:
            rb->lcd_set_foreground(NC_MAGENTA);
            for (i = 0; i < h - 8; i += 6)
                rb->lcd_fillrect(x + 8 + ((tick / 2) + i * 7) % (w - 24), y + 4 + i, 28, 2);
            rb->lcd_set_foreground(NC_CYAN);
            for (i = 0; i < w - 20; i += 12)
                rb->lcd_drawline(x + i, y + h / 2, x + i + 10, y + h / 2 + (((tick / 2) + i) % 14) - 7);
            break;
        case SCENE_THEME_CLINIC:
            rb->lcd_set_foreground(NC_MUTED);
            for (i = 0; i < w; i += 16)
                rb->lcd_vline(x + i, y + 2, y + h - 4);
            for (i = 0; i < h; i += 14)
                rb->lcd_hline(x + 2, x + w - 4, y + i);
            rb->lcd_set_foreground(NC_GREEN);
            rb->lcd_fillrect(x + 6, y + 10 + (tick / 2) % (h - 20), w - 12, 4);
            rb->lcd_set_foreground(NC_CYAN);
            for (i = 0; i < w - 20; i += 18)
                rb->lcd_drawline(x + i, y + h / 2, x + i + 6, y + h / 2 - 6);
            break;
        case SCENE_THEME_BOARDROOM:
            rb->lcd_set_foreground(NC_MUTED);
            for (i = 0; i < 6; ++i)
                rb->lcd_fillrect(x + 18 + i * 42, y + 4, 16, h - 8);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 12, y + 16, w - 24, 6);
            rb->lcd_set_foreground(NC_CYAN);
            for (i = 0; i < 4; ++i)
                rb->lcd_fillrect(x + 22 + ((tick / 3) + i * 58) % (w - 44), y + 36 + (i & 1) * 12, 24, 4);
            break;
        case SCENE_THEME_CONVOY:
            rb->lcd_set_foreground(NC_MUTED);
            rb->lcd_drawline(x + 24, y + h - 6, x + w / 2, y + 8);
            rb->lcd_drawline(x + w - 24, y + h - 6, x + w / 2, y + 8);
            rb->lcd_set_foreground(NC_CYAN);
            for (i = 0; i < 5; ++i)
            {
                int dash_y = y + 14 + ((tick / 2) + i * 12) % (h - 22);
                rb->lcd_fillrect(x + w / 2 - 3, dash_y, 6, 6);
            }
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 36, y + h - 22, 26, 10);
            rb->lcd_fillrect(x + w - 62, y + h - 22, 26, 10);
            break;
        case SCENE_THEME_RELAY:
            rb->lcd_set_foreground(NC_MAGENTA);
            for (i = 0; i < 4; ++i)
            {
                int pad = 12 + i * 10 + ((tick / 5 + i) % 4);
                rb->lcd_drawrect(x + pad, y + pad / 2, w - pad * 2, h - pad);
            }
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_fillrect(x + w / 2 - 3, y + h / 2 - 3, 6, 6);
            for (i = 0; i < 5; ++i)
                rb->lcd_fillrect(x + 28 + ((tick / 2) + i * 52) % (w - 56), y + 10 + (i * 9) % (h - 20), 4, 4);
            break;
        case SCENE_THEME_AFTERGLOW:
            rb->lcd_set_foreground(NC_CYAN);
            rb->lcd_drawrect(x + w - 66, y + 8, 20, 20);
            rb->lcd_set_foreground(NC_MUTED);
            rb->lcd_drawline(x + 24, y + h - 8, x + w / 2, y + 18);
            rb->lcd_drawline(x + w - 24, y + h - 8, x + w / 2, y + 18);
            rb->lcd_set_foreground(NC_MAGENTA);
            for (i = 0; i < 5; ++i)
            {
                int glow_y = y + 10 + ((tick / 2) + i * 9) % (h - 18);
                rb->lcd_fillrect(x + 18 + i * 34, glow_y, 20, 3);
            }
            rb->lcd_set_foreground(NC_CYAN);
            for (i = 0; i < 6; ++i)
                rb->lcd_fillrect(x + 30 + ((tick / 3) + i * 44) % (w - 60),
                                 y + h - 20 - (i % 3) * 10, 6, 6);
            break;
        default:
            rb->lcd_set_foreground(NC_MUTED);
            for (i = 0; i < 15; ++i)
            {
                int bx = x + 10 + i * 18;
                int bh = 14 + ((i * 9 + tick / 4) % 28);
                rb->lcd_fillrect(bx, y + h - bh - 4, 10, bh);
            }
            rb->lcd_set_foreground(NC_CYAN);
            for (i = 0; i < 12; ++i)
                rb->lcd_drawline(x + 8 + ((tick / 2) + i * 24) % (w - 16), y + 6,
                                 x + 2 + ((tick / 2) + i * 24) % (w - 16), y + h - 8);
            rb->lcd_set_foreground(NC_MAGENTA);
            rb->lcd_fillrect(x + 40, y + 12, 66, 6);
            rb->lcd_fillrect(x + w - 116, y + 28, 52, 5);
            break;
    }
    rb->lcd_set_foreground(NC_TEXT);
#else
    (void)theme;
    (void)tick;
#endif
}

static int wrap_choice_label(const char *label,
                             char lines[][NC_MAX_LINE_CHARS], int max_lines)
{
    int count = nc_ui_wrap_text(label, LCD_WIDTH - 52, lines, max_lines);

    if (count == max_lines && lines[max_lines - 1][0] != '\0')
    {
        int len = rb->strlen(lines[max_lines - 1]);
        if (len > 3)
        {
            lines[max_lines - 1][len - 3] = '.';
            lines[max_lines - 1][len - 2] = '.';
            lines[max_lines - 1][len - 1] = '.';
        }
    }
    return count;
}

static int draw_choice_row(int y, const char *label, bool selected)
{
    char lines[2][NC_MAX_LINE_CHARS];
    int wrapped = wrap_choice_label(label, lines, ARRAYLEN(lines));
    int height = 8 + wrapped * (font_height + 1);
    int i;

    if (selected)
        nc_ui_box(18, y - 2, LCD_WIDTH - 36, height, true);

    for (i = 0; i < wrapped; ++i)
        put_text_box_left(24, y + i * (font_height + 1), LCD_WIDTH - 56, lines[i], NC_TEXT);

    return height;
}

static void draw_choice_browser(const struct nc_visible_choice *choices,
                                int choice_count, int choice_index)
{
    int panel_y = 116;
    int panel_h = 98;
    int counter_y = panel_y + 6;
    int first_choice = MAX(0, choice_index - 1);
    int y;
    int shown = 0;
    char counter[12];
    int i;

    if (choice_count <= 0)
        return;

    if (first_choice > MAX(0, choice_count - 2))
        first_choice = MAX(0, choice_count - 2);

    nc_ui_box(12, panel_y, LCD_WIDTH - 24, panel_h, false);
    put_text_box_left(18, counter_y, 120, "Choose your move", NC_MUTED);

    rb->snprintf(counter, sizeof(counter), "%d/%d", choice_index + 1, choice_count);
    put_text_box_right(LCD_WIDTH - 18, counter_y, 40, counter, NC_MUTED);

    y = panel_y + 24;
    for (i = first_choice; i < choice_count && shown < 2; ++i, ++shown)
        y += draw_choice_row(y, choices[i].choice->label, i == choice_index) + 6;
}

static bool get_now_playing_lines(char *title, size_t title_size,
                                  char *artist, size_t artist_size)
{
    const struct mp3entry *track;
    const char *base;

    if ((rb->audio_status() & AUDIO_STATUS_PLAY) != AUDIO_STATUS_PLAY)
        return false;

    track = rb->audio_current_track();
    if (track == NULL)
        return false;

    if (track->title != NULL && track->title[0] != '\0')
        rb->strlcpy(title, track->title, title_size);
    else
    {
        base = rb->strrchr(track->path, '/');
        rb->strlcpy(title, base != NULL ? base + 1 : track->path, title_size);
    }

    if (track->artist != NULL && track->artist[0] != '\0')
        rb->strlcpy(artist, track->artist, artist_size);
    else
        rb->strlcpy(artist, "Local signal", artist_size);

    return true;
}

static bool scene_uses_radio(enum scene_theme theme)
{
    return theme == SCENE_THEME_CITY ||
           theme == SCENE_THEME_AFTERGLOW ||
           theme == SCENE_THEME_CONVOY ||
           theme == SCENE_THEME_BOARDROOM;
}

static void play_menu_move_sfx(void)
{
    static long last_tick = 0;

    if ((*rb->current_tick - last_tick) < HZ / 18)
        return;

    last_tick = *rb->current_tick;
    nc_audio_play(NC_SOUND_MOVE);
}

static void play_confirm_sfx(void)
{
    nc_audio_play(NC_SOUND_CONFIRM);
}

static void play_back_sfx(void)
{
    nc_audio_play(NC_SOUND_BACK);
}

static void play_transition_sfx(void)
{
    nc_audio_play(NC_SOUND_TRANSITION);
}

static void play_title_sting(void)
{
    nc_audio_play(NC_SOUND_TITLE);
}

static void play_afterglow_sting(void)
{
    nc_audio_play(NC_SOUND_AFTERGLOW);
}

static void draw_portrait_frame(int x, int y, int w, int h, bool accent_magenta)
{
    nc_ui_box(x, y, w, h, false);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(accent_magenta ? NC_MAGENTA : NC_CYAN);
    rb->lcd_drawrect(x + 3, y + 3, w - 6, h - 6);
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

static void draw_bitmap_portrait(int x, int y,
                                 enum portrait_id portrait,
                                 int src_y,
                                 int height)
{
#if NIGHTCITY_USE_BITMAP_ASSETS
    rb->lcd_bitmap_part(nightcity_portraits, portrait * NC_PORTRAIT_W, src_y,
                        BMPWIDTH_nightcity_portraits, x, y, NC_PORTRAIT_W, height);
#else
    enum speaker_glyph glyph = (portrait == PORTRAIT_HOSTILE) ? GLYPH_HOSTILE :
                               (portrait == PORTRAIT_NYRA) ? GLYPH_NYRA :
                               (portrait == PORTRAIT_JUNO) ? GLYPH_JUNO :
                               (portrait == PORTRAIT_MIRA) ? GLYPH_MIRA :
                               (portrait == PORTRAIT_SABLE) ? GLYPH_SABLE :
                               (portrait == PORTRAIT_ROOK) ? GLYPH_ROOK :
                               (portrait == PORTRAIT_KADE) ? GLYPH_KADE :
                               GLYPH_VESPER;
    draw_speaker_glyph(x + (NC_PORTRAIT_W - 34) / 2,
                       y + (height - 34) / 2,
                       glyph,
                       *rb->current_tick);
#endif
}

static void draw_scene_asset_panel(const struct nc_game_state *state,
                                   const struct nc_node *node)
{
    int portrait_x = LCD_WIDTH - 86;
    int portrait_y = 44;
    int label_x = 20;
    int label_y = 56;
    enum scene_theme theme = detect_scene_theme(node);
    enum portrait_id portrait = detect_portrait(state, node->speaker);

    draw_scene_panorama(theme, 12, 44, 206, 66, *rb->current_tick);
    draw_portrait_frame(portrait_x, portrait_y, 80, 80, theme == SCENE_THEME_AFTERGLOW);
    draw_bitmap_portrait(portrait_x + 4, portrait_y + 4, portrait, 12, NC_SCENE_PORTRAIT_H);
    nc_ui_box(label_x, label_y, 126, 18, false);
    nc_ui_box(label_x, label_y + 22, 104, 18, false);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_MUTED);
#endif
    put_text_box_left(label_x + 6, label_y + 4, 114, node->speaker, NC_MUTED);
    put_text_box_left(label_x + 6, label_y + 26, 92, scene_theme_label(theme), NC_MUTED);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

static void draw_deck_asset_panel(const struct nc_game_state *state)
{
    int x = LCD_WIDTH - 110;
    int y = 52;

    nc_ui_box(x, y, 98, 82, false);
#if NIGHTCITY_USE_BITMAP_ASSETS
    rb->lcd_bitmap(nightcity_modules, x + 1, y + 6,
                   BMPWIDTH_nightcity_modules, BMPHEIGHT_nightcity_modules);
#else
    draw_deck_modules(x + 2, y + 6, state);
#endif

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_MUTED);
#endif
    put_text_box_left(x + 8, y + 46, 82, "Chrome", NC_MUTED);
    put_text_box_left(x + 8, y + 58, 82,
                      (state->cyberware & NC_CYBER_COMBAT_RIG) ? "Rig online" : "Rig dark",
                      NC_MUTED);
    put_text_box_left(x + 8, y + 68, 82,
                      (state->cyberware & NC_CYBER_GHOSTWALL) ? "Ghost online" : "Ghost dark",
                      NC_MUTED);
    put_text_box_left(x + 8, y + 78, 82,
                      (state->cyberware & NC_CYBER_SOCIAL) ? "Spoof online" : "Spoof dark",
                      NC_MUTED);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_TEXT);
#endif
}

void nc_ui_draw_encounter(const struct nc_game_state *state,
                          const struct nc_enemy *enemy,
                          int player_hp,
                          int enemy_hp,
                          const char log_lines[][NC_MAX_LINE_CHARS],
                          int selection,
                          bool defending,
                          int stim_turns)
{
    static const char *actions[] =
    {
        "Attack",
        "Hack",
        "Defend",
        "Medkit",
        "Stim",
    };
    int i;
    int line_height;
    int y = 126;
    char buf[64];
    int asset_x = LCD_WIDTH - 122;
    int asset_y = 48;

    rb->font_getstringsize("M", NULL, &line_height, FONT_UI);
    nc_ui_frame("Encounter", enemy->name);

    nc_ui_box(12, 48, 166, 28, false);
    rb->snprintf(buf, sizeof(buf), "Vesper %d/%d", player_hp, state->max_health);
    put_text_box_left(18, 54, 70, buf, NC_TEXT);
    nc_ui_meter(94, 56, 76, player_hp, state->max_health, player_hp < state->max_health / 3);

    nc_ui_box(12, 80, 166, 28, false);
    rb->snprintf(buf, sizeof(buf), "%s %d/%d", enemy->name, enemy_hp, enemy->max_health);
    put_text_box_left(18, 86, 70, buf, NC_TEXT);
    nc_ui_meter(94, 88, 76, enemy_hp, enemy->max_health, enemy_hp <= enemy->max_health / 3);

#if NIGHTCITY_USE_BITMAP_ASSETS
    rb->lcd_bitmap(is_aegis_enemy(enemy) ? nightcity_aegis : nightcity_sentinel,
                   asset_x, asset_y,
                   is_aegis_enemy(enemy) ? BMPWIDTH_nightcity_aegis : BMPWIDTH_nightcity_sentinel,
                   is_aegis_enemy(enemy) ? BMPHEIGHT_nightcity_aegis : BMPHEIGHT_nightcity_sentinel);
#else
    draw_threat_asset(asset_x, asset_y, is_aegis_enemy(enemy), *rb->current_tick);
#endif
    nc_ui_box(asset_x, asset_y + 68, 106, 40, false);
#if NIGHTCITY_USE_BITMAP_ASSETS
    rb->lcd_bitmap_part(nightcity_portraits,
                        PORTRAIT_HOSTILE * NC_PORTRAIT_W + 19, 16,
                        BMPWIDTH_nightcity_portraits,
                        asset_x + 8, asset_y + 14, 34, 34);
#endif
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_MUTED);
#endif
    put_text_box_left(asset_x + 48, asset_y + 76, 50, is_aegis_enemy(enemy) ? "FRAME" : "DRONE", NC_MUTED);
    put_text_box_left(asset_x + 8, asset_y + 90, 90, defending ? "Shielded" : "Open lane", NC_MUTED);
    if (stim_turns > 0)
        rb->snprintf(buf, sizeof(buf), "Stim x%d", stim_turns);
    else
        rb->strlcpy(buf, "Stim cold", sizeof(buf));
    put_text_box_left(asset_x + 8, asset_y + 100, 90, buf, NC_MUTED);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(NC_TEXT);
#endif

    nc_ui_box(12, 116, LCD_WIDTH - 146, 52, false);
    for (i = 0; i < NC_MAX_LOG_LINES; ++i)
    {
        if (log_lines[i][0] != '\0')
            put_text_box_left(18, y + i * (line_height + 2), LCD_WIDTH - 152, log_lines[i], NC_TEXT);
    }

    nc_ui_box(12, LCD_HEIGHT - 104, LCD_WIDTH - 24, 78, false);
    for (i = 0; i < (int)ARRAYLEN(actions); ++i)
    {
        bool selected = (i == selection);
        int row_y = LCD_HEIGHT - 96 + i * (line_height + 4);
        if (selected)
            nc_ui_box(18, row_y - 2, LCD_WIDTH - 36, line_height + 6, true);
        put_text_box_left(26, row_y, LCD_WIDTH - 52, actions[i], NC_TEXT);
    }

    nc_ui_footer("Deck", "Act", "Menu");
    nc_ui_update();
}

static int run_menu(const struct menu_screen *screen)
{
    int selection = 0;
    int action;

    while (1)
    {
        int i;
        int y = 86;

        if (screen->title == NULL)
            draw_title_background();
        else
            nc_ui_frame(screen->title, screen->subtitle);

        for (i = 0; i < screen->count; ++i)
        {
            nc_ui_box(18, y - 3, LCD_WIDTH - 36, font_height + 8, i == selection);
            put_text_box_left(28, y, LCD_WIDTH - 56, screen->items[i], NC_TEXT);
            y += font_height + 14;
        }

        if (screen->descriptions != NULL && screen->descriptions[selection] != NULL)
        {
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_MUTED);
#endif
            put_text_box_left(18, LCD_HEIGHT - 56, LCD_WIDTH - 36,
                              screen->descriptions[selection], NC_MUTED);
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_TEXT);
#endif
        }

        nc_ui_footer("Wheel", "Select", screen->allow_cancel ? "Menu" : "Quit");
        nc_ui_update();

        action = nc_ui_input(HZ / 8);
        switch (action)
        {
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                selection = (selection + screen->count - 1) % screen->count;
                play_menu_move_sfx();
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                selection = (selection + 1) % screen->count;
                play_menu_move_sfx();
                break;
            case PLA_SELECT_REL:
                play_confirm_sfx();
                return selection;
            case PLA_UP:
            case PLA_CANCEL:
            case PLA_EXIT:
                if (screen->allow_cancel)
                {
                    play_back_sfx();
                    return -1;
                }
                break;
        }
    }
}

int nc_ui_run_title(bool has_continue)
{
    static const char *items_with_continue[] =
    {
        "New Game",
        "Continue",
        "Help",
        "Quit",
    };
    static const char *items_no_continue[] =
    {
        "New Game",
        "Help",
        "Quit",
    };
    static const char *descs_with_continue[] =
    {
        "Start a fresh run through Crown Meridian.",
        "Resume your last checkpoint.",
        "Read controls, systems, and survival notes.",
        "Leave the plugin.",
    };
    static const char *descs_no_continue[] =
    {
        "Start a fresh run through Crown Meridian.",
        "Read controls, systems, and survival notes.",
        "Leave the plugin.",
    };
    struct menu_screen screen;
    int result;

    play_title_sting();

    screen.title = NULL;
    screen.subtitle = NULL;
    screen.allow_cancel = false;
    if (has_continue)
    {
        screen.items = items_with_continue;
        screen.descriptions = descs_with_continue;
        screen.count = ARRAYLEN(items_with_continue);
        return run_menu(&screen);
    }

    screen.items = items_no_continue;
    screen.descriptions = descs_no_continue;
    screen.count = ARRAYLEN(items_no_continue);
    result = run_menu(&screen);
    if (result == 1)
        return 2;
    if (result == 2)
        return 3;
    return result;
}

enum nc_lifepath nc_ui_choose_lifepath(void)
{
    static const char *items[] =
    {
        "Streetkid",
        "Corpo",
        "Nomad",
    };
    static const char *descriptions[] =
    {
        "Higher street cred. Better alley leverage.",
        "More credits. Corporate contact, higher risk.",
        "Extra toughness. Scrap and an escape route.",
    };
    struct menu_screen screen;
    int result;

    screen.title = "New Game";
    screen.subtitle = "Choose a lifepath";
    screen.items = items;
    screen.descriptions = descriptions;
    screen.count = ARRAYLEN(items);
    screen.allow_cancel = true;

    result = run_menu(&screen);
    switch (result)
    {
        case 0:
            return NC_LIFEPATH_STREETKID;
        case 1:
            return NC_LIFEPATH_CORPO;
        case 2:
            return NC_LIFEPATH_NOMAD;
        default:
            return NC_LIFEPATH_NONE;
    }
}

enum nc_gender nc_ui_choose_gender(void)
{
    static const char *items[] =
    {
        "Woman",
        "Man",
        "Nonbinary",
    };
    static const char *descriptions[] =
    {
        "Unlocks Nyra's punk rooftop romance route.",
        "Runs the same core story with different deck identity.",
        "Runs the same core story with different deck identity.",
    };
    struct menu_screen screen;
    int result;

    screen.title = "New Game";
    screen.subtitle = "Choose your gender";
    screen.items = items;
    screen.descriptions = descriptions;
    screen.count = ARRAYLEN(items);
    screen.allow_cancel = true;

    result = run_menu(&screen);
    switch (result)
    {
        case 0:
            return NC_GENDER_FEMME;
        case 1:
            return NC_GENDER_MASC;
        case 2:
            return NC_GENDER_NONBINARY;
        default:
            return NC_GENDER_NONE;
    }
}

enum nc_profile nc_ui_choose_profile(void)
{
    static const char *items[] =
    {
        "Razor",
        "Velvet",
        "Drift",
    };
    static const char *descriptions[] =
    {
        "Punk cut. +1 Street Cred, +2 Health.",
        "Polished lie. +20 Credits, +1 Humanity, +1 Heat.",
        "Runner hood. +1 Ghost Sync, +1 Scrap, +1 Stim.",
    };
    static const enum portrait_id portraits[] =
    {
        PORTRAIT_PROFILE_RAZOR,
        PORTRAIT_PROFILE_VELVET,
        PORTRAIT_PROFILE_DRIFT,
    };
    unsigned selection = 0;

    while (1)
    {
        unsigned i;
        int y = 62;

        nc_ui_frame("New Game", "Build your operator");
        draw_portrait_frame(214, 42, 92, 110, selection == 1);
        draw_bitmap_portrait(224, 49, portraits[selection], 0, NC_PORTRAIT_H);

        for (i = 0; i < ARRAYLEN(items); ++i)
        {
            nc_ui_box(16, y - 3, 176, font_height + 8, i == selection);
            put_text_box_left(26, y, 156, items[i], NC_TEXT);
            y += font_height + 14;
        }

#if LCD_DEPTH > 1
        rb->lcd_set_foreground(NC_MUTED);
#endif
        put_text_box_left(214, 158, 92, "Operator profile", NC_MUTED);
        put_text_box_left(16, LCD_HEIGHT - 56, LCD_WIDTH - 32, descriptions[selection], NC_MUTED);
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(NC_TEXT);
#endif
        nc_ui_footer("Wheel", "Select", "Menu");
        nc_ui_update();

        switch (nc_ui_input(HZ / 8))
        {
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                selection = (selection + ARRAYLEN(items) - 1) % ARRAYLEN(items);
                play_menu_move_sfx();
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                selection = (selection + 1) % ARRAYLEN(items);
                play_menu_move_sfx();
                break;
            case PLA_SELECT_REL:
                play_confirm_sfx();
                return (enum nc_profile)(selection + 1);
            case PLA_UP:
            case PLA_CANCEL:
            case PLA_EXIT:
                play_back_sfx();
                return NC_PROFILE_NONE;
        }
    }
}

static void show_text_screen(const char *title, const char *subtitle, const char *text)
{
    char lines[NC_MAX_WRAP_LINES][NC_MAX_LINE_CHARS];
    int line_count = nc_ui_wrap_text(text, LCD_WIDTH - 28, lines, ARRAYLEN(lines));
    int page = 0;
    int page_count = (line_count + NC_TEXT_PAGE_LINES - 1) / NC_TEXT_PAGE_LINES;

    while (1)
    {
        int i;
        int start = page * NC_TEXT_PAGE_LINES;
        int y = 52;

        nc_ui_frame(title, subtitle);
        for (i = 0; i < NC_TEXT_PAGE_LINES && (start + i) < line_count; ++i)
        {
            put_text_box_left(14, y, LCD_WIDTH - 28, lines[start + i], NC_TEXT);
            y += font_height + 4;
        }

        if (page_count > 1)
        {
            char counter[16];
            rb->snprintf(counter, sizeof(counter), "%d/%d", page + 1, page_count);
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_MUTED);
#endif
            put_text_box_right(LCD_WIDTH - 12, y + 4, 40, counter, NC_MUTED);
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_TEXT);
#endif
        }

        nc_ui_footer("Wheel", page + 1 < page_count ? "More" : "Back", "Menu");
        nc_ui_update();

        switch (nc_ui_input(HZ / 8))
        {
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                if (page > 0)
                {
                    --page;
                    play_menu_move_sfx();
                }
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                if (page + 1 < page_count)
                {
                    ++page;
                    play_menu_move_sfx();
                }
                break;
            case PLA_UP:
            case PLA_SELECT_REL:
            case PLA_CANCEL:
            case PLA_EXIT:
                if (page + 1 < page_count)
                {
                    ++page;
                    play_confirm_sfx();
                }
                else
                {
                    play_back_sfx();
                    return;
                }
                break;
        }
    }
}

void nc_ui_show_help(void)
{
    show_text_screen(
        "Help",
        "Controls and systems",
        "Controls\n"
        "Scroll wheel / up/down: move through menus and choices.\n"
        "Center: confirm, advance dialogue, pick actions.\n"
        "Left: open stats and inventory during story or encounters.\n"
        "Menu / cancel: pause menu or back out of info screens.\n\n"
        "Creator\n"
        "New Game lets you choose a lifepath, gender, and operator profile.\n"
        "Operator profiles add small stat bonuses and change Vesper's portrait.\n\n"
        "Systems\n"
        "Street Cred unlocks bolder options.\n"
        "Corp Heat raises corporate pressure and some encounter damage.\n"
        "Humanity keeps you grounded for cleaner endings.\n"
        "Ghost Sync measures how deeply Sable is fused into your head.\n"
        "Credits buy prep. Medkits and stims keep you alive.\n\n"
        "Save\n"
        "nightcity writes a checkpoint whenever you enter a new story node. "
        "Continue restarts from that checkpoint, including before encounters.");
}

void nc_ui_show_panel(const struct nc_game_state *state)
{
    char buf[48];
    char subtitle[64];

    while (1)
    {
        int y = 52;

        rb->snprintf(subtitle, sizeof(subtitle), "%s | %s | %s",
                     nc_lifepath_name(state->lifepath),
                     nc_gender_name(state->gender),
                     nc_profile_name(state->profile));
        nc_ui_frame("Deck", subtitle);
        draw_deck_asset_panel(state);
        draw_stat_line(14, y, "Street Cred:", state->street_cred); y += font_height + 6;
        nc_ui_meter(14, y, 84, state->street_cred, NC_STAT_MAX, false); y += 14;
        draw_stat_line(14, y, "Corp Heat:", state->corp_heat); y += font_height + 6;
        nc_ui_meter(14, y, 84, state->corp_heat, NC_STAT_MAX, state->corp_heat >= 6); y += 14;
        draw_stat_line(14, y, "Humanity:", state->humanity); y += font_height + 6;
        nc_ui_meter(14, y, 84, state->humanity, NC_STAT_MAX, state->humanity <= 2); y += 14;
        draw_stat_line(14, y, "Ghost Sync:", state->ghost_sync); y += font_height + 6;
        nc_ui_meter(14, y, 84, state->ghost_sync, NC_STAT_MAX, state->ghost_sync >= 6); y += 14;
        draw_stat_line(14, y, "Credits:", state->credits); y += font_height + 6;
        draw_stat_line(14, y, "Health:", state->health); y += font_height + 12;

        rb->snprintf(buf, sizeof(buf), "Medkits %d  Stims %d  Scrap %d",
                     state->medkits, state->stims, state->scrap);
        put_text_box_left(14, y, LCD_WIDTH - 132, buf, NC_TEXT);
        y += font_height + 10;

        put_text_box_left(14, y, LCD_WIDTH - 28, "Auto-checkpointed at each story node.", NC_TEXT);
        y += font_height + 4;
        put_text_box_left(14, y, LCD_WIDTH - 28, "Left opens this deck during scenes or fights.", NC_TEXT);

        nc_ui_footer(NULL, "Back", "Menu");
        nc_ui_update();

        switch (nc_ui_input(HZ / 8))
        {
            case PLA_UP:
            case PLA_SELECT_REL:
            case PLA_CANCEL:
            case PLA_EXIT:
            case PLA_LEFT:
                play_back_sfx();
                return;
        }
    }
}

enum nc_pause_result nc_ui_run_pause_menu(void)
{
    static const char *items[] =
    {
        "Resume",
        "Stats / Inventory",
        "Help",
        "Return to Title",
        "Exit Plugin",
    };
    static const char *descriptions[] =
    {
        "Jump back into the current scene.",
        "Review stats, inventory, and cyberware.",
        "Show controls and system notes.",
        "Leave the run but keep the checkpoint save.",
        "Close the plugin immediately.",
    };
    struct menu_screen screen;
    int result;

    screen.title = "Pause";
    screen.subtitle = "Run state saved";
    screen.items = items;
    screen.descriptions = descriptions;
    screen.count = ARRAYLEN(items);
    screen.allow_cancel = true;

    result = run_menu(&screen);
    if (result < 0)
        return NC_PAUSE_RESUME;
    return (enum nc_pause_result)result;
}

int nc_ui_run_scene(const struct nc_game_state *state,
                    const struct nc_node *node,
                    const struct nc_visible_choice *choices,
                    int choice_count)
{
    char lines[NC_MAX_WRAP_LINES][NC_MAX_LINE_CHARS];
    enum scene_view_mode
    {
        SCENE_VIEW_TEXT = 0,
        SCENE_VIEW_CHOICES,
    };
    int lines_per_page = 6;
    int line_count = nc_ui_wrap_text(node->text, LCD_WIDTH - 28, lines, ARRAYLEN(lines));
    int page = 0;
    int page_count = (line_count + lines_per_page - 1) / lines_per_page;
    int choice_index = 0;
    enum scene_view_mode view_mode = SCENE_VIEW_TEXT;

    if (page_count < 1)
        page_count = 1;

    while (1)
    {
        nc_ui_frame(node->title, node->speaker);
        draw_scene_asset_panel(state, node);
        if (view_mode == SCENE_VIEW_TEXT)
        {
            int i;
            int start = page * lines_per_page;
            int y = 122;
            char counter[16];

            nc_ui_box(12, 116, LCD_WIDTH - 24, 88, false);

            for (i = 0; i < lines_per_page && (start + i) < line_count; ++i)
            {
                put_text_box_left(14, y, LCD_WIDTH - 28, lines[start + i], NC_TEXT);
                y += font_height + 4;
            }

            rb->snprintf(counter, sizeof(counter), "%d/%d", page + 1, page_count);
            put_text_box_right(LCD_WIDTH - 18, 120, 40, counter, NC_MUTED);

            if (page + 1 == page_count && choice_count > 0)
                put_text_box_left(18, 190, LCD_WIDTH - 36, "Center opens choices.", NC_MUTED);

            nc_ui_footer("Deck", (page + 1 < page_count) ? "More" :
                         (choice_count > 0 ? "Choices" : "Advance"), "Menu");
        }
        else
        {
            draw_choice_browser(choices, choice_count, choice_index);
            nc_ui_footer("Story", "Confirm", "Menu");
        }

        nc_ui_update();

        switch (nc_ui_input(HZ / 8))
        {
            case PLA_LEFT:
                play_back_sfx();
                if (view_mode == SCENE_VIEW_CHOICES)
                {
                    view_mode = SCENE_VIEW_TEXT;
                    break;
                }
                return NC_SCENE_PANEL;
            case PLA_UP:
            case PLA_CANCEL:
            case PLA_EXIT:
                play_back_sfx();
                return NC_SCENE_MENU;
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                if (view_mode == SCENE_VIEW_CHOICES && choice_count > 0)
                {
                    choice_index = (choice_index + choice_count - 1) % choice_count;
                    play_menu_move_sfx();
                }
                else if (page > 0)
                {
                    --page;
                    play_menu_move_sfx();
                }
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                if (view_mode == SCENE_VIEW_CHOICES && choice_count > 0)
                {
                    choice_index = (choice_index + 1) % choice_count;
                    play_menu_move_sfx();
                }
                else if (page + 1 < page_count)
                {
                    ++page;
                    play_menu_move_sfx();
                }
                break;
            case PLA_SELECT_REL:
                if (view_mode == SCENE_VIEW_CHOICES && choice_count > 0)
                {
                    play_confirm_sfx();
                    return choice_index;
                }
                if (page + 1 < page_count)
                {
                    ++page;
                    play_confirm_sfx();
                }
                else if (choice_count > 0)
                {
                    play_confirm_sfx();
                    view_mode = SCENE_VIEW_CHOICES;
                }
                else
                {
                    play_confirm_sfx();
                    return NC_SCENE_NEXT;
                }
                break;
        }
    }
}

void nc_ui_transition(void)
{
    int frame;

    play_transition_sfx();

    for (frame = 0; frame < 4; ++frame)
    {
        int i;

        nc_ui_fill_background();
#if LCD_DEPTH > 1
#if NIGHTCITY_USE_BITMAP_ASSETS
        for (i = 0; i < 7; ++i)
        {
            int y = (frame * 16 + i * 34) % LCD_HEIGHT;
            rb->lcd_bitmap_part(nightcity_glitch, frame * NC_GLITCH_W, 0,
                                BMPWIDTH_nightcity_glitch,
                                0, y, NC_GLITCH_W, MIN(NC_GLITCH_H, LCD_HEIGHT - y));
        }
#else
        rb->lcd_set_foreground((frame & 1) ? NC_MAGENTA : NC_CYAN);
        for (i = 0; i < 8; ++i)
        {
            int y = (frame * 18 + i * 26) % LCD_HEIGHT;
            rb->lcd_fillrect(0, y, LCD_WIDTH, 5);
        }
#endif
        rb->lcd_set_foreground(NC_TEXT);
#endif
        rb->lcd_update();
        rb->sleep(HZ / 40);
    }
}

void nc_ui_story_intro(const struct nc_game_state *state,
                       const struct nc_node *node)
{
    enum scene_theme theme = detect_scene_theme(node);
    enum portrait_id portrait = detect_portrait(state, node->speaker);
    char title[NC_MAX_LINE_CHARS];
    char artist[NC_MAX_LINE_CHARS];
    char radio_line[NC_MAX_LINE_CHARS];
    bool show_radio = scene_uses_radio(theme) &&
                      get_now_playing_lines(title, sizeof(title), artist, sizeof(artist));
    int frame;

    if (show_radio)
        rb->snprintf(radio_line, sizeof(radio_line), "%s / %s", title, artist);

    if (theme == SCENE_THEME_AFTERGLOW)
        play_afterglow_sting();

    for (frame = 0; frame < 6; ++frame)
    {
        int matte = ((5 - frame) * LCD_HEIGHT) / 18;
        int pulse = 14 + frame * 8;
        int portrait_x = 222 + (5 - frame) * 10;

        draw_scene_panorama(theme, 0, 0, LCD_WIDTH, LCD_HEIGHT, *rb->current_tick + frame * 3);

        draw_portrait_frame(portrait_x - 8, 16, 90, 110, theme == SCENE_THEME_AFTERGLOW);
        draw_bitmap_portrait(portrait_x, 23, portrait, 0, NC_PORTRAIT_H);

        nc_ui_box(14, 20, 194, 20, false);
        nc_ui_box(14, 44, 194, 20, false);
        put_text_box_left(20, 26, 182, node->title, NC_TEXT);
        put_text_box_left(20, 50, 182, node->speaker, NC_MUTED);
#if LCD_DEPTH > 1
        rb->lcd_set_foreground((frame & 1) ? NC_MAGENTA : NC_CYAN);
        rb->lcd_fillrect(14, LCD_HEIGHT - 48, pulse, 3);
        rb->lcd_set_foreground(NC_TEXT);
#endif

        if (show_radio)
        {
            nc_ui_box(14, LCD_HEIGHT - 78, LCD_WIDTH - 28, 20, false);
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_MUTED);
#endif
            put_text_box_left(20, LCD_HEIGHT - 74, 38, "Radio", NC_MUTED);
            put_text_box_left(64, LCD_HEIGHT - 74, LCD_WIDTH - 98, radio_line, NC_TEXT);
        }

#if LCD_DEPTH > 1
        rb->lcd_set_foreground(NC_BG);
        rb->lcd_fillrect(0, 0, LCD_WIDTH, matte);
        rb->lcd_fillrect(0, LCD_HEIGHT - matte, LCD_WIDTH, matte);
        rb->lcd_set_foreground(NC_TEXT);
#endif
        rb->lcd_update();
        rb->sleep(HZ / 28);
    }

    rb->sleep(HZ / 12);
}

void nc_ui_show_ending(const struct nc_game_state *state,
                       const struct nc_node *node)
{
    char lines[NC_MAX_WRAP_LINES][NC_MAX_LINE_CHARS];
    char subtitle[48];
    char tag_line[64];
    int line_count;
    int page = 0;
    int page_count;
    enum ending_card card = detect_ending_card(node);

    rb->snprintf(subtitle, sizeof(subtitle), "%s | Humanity %d | Ghost %d",
                 nc_lifepath_name(state->lifepath), state->humanity, state->ghost_sync);
    if (ending_relationship_tag(state) != NULL)
        rb->snprintf(tag_line, sizeof(tag_line), "%s", ending_relationship_tag(state));
    else
        tag_line[0] = '\0';
    line_count = nc_ui_wrap_text(node->text, LCD_WIDTH - 28, lines, ARRAYLEN(lines));
    page_count = (line_count + 5) / 6;

    while (1)
    {
        int i;
        int start = page * 6;
        int y = 112;

        nc_ui_frame(node->title, subtitle);
        nc_ui_box(22, 48, 276, 60, false);
#if NIGHTCITY_USE_BITMAP_ASSETS
        rb->lcd_bitmap_part(nightcity_endings, card * 68, 0, BMPWIDTH_nightcity_endings,
                            24, 50, 68, 56);
#else
        draw_ending_banner(24, 50, card, *rb->current_tick);
#endif
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(NC_MUTED);
#endif
        put_text_box_left(104, 60, 180, "Final Signal", NC_MUTED);
        put_text_box_left(104, 74, 180,
                          card == ENDING_CARD_REBEL ? "City turned against the tower." :
                          card == ENDING_CARD_CORP ? "A cleaner lie won the board." :
                          card == ENDING_CARD_GHOST ? "You became the last transmission." :
                          "You lived, but the night stayed in you.",
                          NC_MUTED);
        if (tag_line[0] != '\0')
            put_text_box_left(104, 88, 180, tag_line, NC_MUTED);
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(NC_TEXT);
#endif

        for (i = 0; i < 6 && (start + i) < line_count; ++i)
        {
            put_text_box_left(14, y, LCD_WIDTH - 28, lines[start + i], NC_TEXT);
            y += font_height + 4;
        }

        if (page_count > 1)
        {
            char counter[16];
            rb->snprintf(counter, sizeof(counter), "%d/%d", page + 1, page_count);
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_MUTED);
#endif
            put_text_box_right(LCD_WIDTH - 12, y + 2, 40, counter, NC_MUTED);
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(NC_TEXT);
#endif
        }

        nc_ui_footer("Wheel", page + 1 < page_count ? "More" : "Back", "Menu");
        nc_ui_update();

        switch (nc_ui_input(HZ / 8))
        {
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                if (page > 0)
                {
                    --page;
                    play_menu_move_sfx();
                }
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                if (page + 1 < page_count)
                {
                    ++page;
                    play_menu_move_sfx();
                }
                break;
            case PLA_UP:
            case PLA_SELECT_REL:
            case PLA_CANCEL:
            case PLA_EXIT:
                if (page + 1 < page_count)
                {
                    ++page;
                    play_confirm_sfx();
                }
                else
                {
                    play_back_sfx();
                    return;
                }
                break;
        }
    }
}

void nc_ui_flash_message(const char *title, const char *text, int ticks)
{
    nc_ui_frame(title, NULL);
    rb->lcd_putsxy(14, 72, text);
    nc_ui_footer(NULL, NULL, NULL);
    nc_ui_update();
    rb->sleep(ticks);
}
