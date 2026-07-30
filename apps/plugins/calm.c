/***************************************************************************
 * Calm: personal offline meditation and sound player for 320x240 iPods.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"

#define CALM_ROOT       ROCKBOX_DIR "/rockpod/calm"
#define CALM_MANIFEST   CALM_ROOT "/library.tsv"
#define CALM_ICON       CALM_ROOT "/assets/calm-icon.64x64x24.bmp"
#define CALM_VERSION    "rockpod_calm_v1"
#define CALM_MAX_ITEMS  96
#define CALM_ICON_W     64
#define CALM_ICON_H     64

#define CALM_CYAN       LCD_RGBPACK(59, 190, 236)
#define CALM_BLUE       LCD_RGBPACK(80, 154, 231)
#define CALM_INDIGO     LCD_RGBPACK(78, 96, 226)
#define CALM_NAVY       LCD_RGBPACK(13, 28, 68)
#define CALM_CARD       LCD_RGBPACK(27, 55, 111)
#define CALM_CARD_SOFT  LCD_RGBPACK(42, 72, 132)
#define CALM_LAVENDER   LCD_RGBPACK(207, 218, 255)
#define CALM_DIM        LCD_RGBPACK(190, 215, 245)
#define CALM_WHITE      LCD_RGBPACK(255, 255, 255)

struct calm_item
{
    char title[96];
    char category[16];
    char path[MAX_PATH];
    long duration;
};

enum calm_screen
{
    CALM_HOME = 0,
    CALM_LIBRARY,
    CALM_PLAYER
};

static const char * const calm_categories[] = {
    "For You", "Sleep", "Meditate", "Music", "Sounds"
};

static struct calm_item calm_items[CALM_MAX_ITEMS];
static int calm_item_count;
static int calm_category;
static int calm_selected;
static int calm_visible[CALM_MAX_ITEMS];
static int calm_visible_count;
static struct bitmap calm_icon;
static unsigned char calm_icon_data[
    BM_SIZE(CALM_ICON_W, CALM_ICON_H, FORMAT_NATIVE, false)];
static bool calm_icon_valid;

static unsigned calm_blend(int tr, int tg, int tb,
                           int br, int bg, int bb,
                           int pos, int span)
{
    return LCD_RGBPACK(tr + (br - tr) * pos / span,
                       tg + (bg - tg) * pos / span,
                       tb + (bb - tb) * pos / span);
}

static void calm_gradient(void)
{
    int y;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    for (y = 0; y < LCD_HEIGHT; y++)
    {
        unsigned color;

        if (y < LCD_HEIGHT / 2)
            color = calm_blend(59, 190, 236, 80, 154, 231,
                               y, LCD_HEIGHT / 2);
        else
            color = calm_blend(80, 154, 231, 78, 96, 226,
                               y - LCD_HEIGHT / 2, LCD_HEIGHT / 2);
        rb->lcd_set_foreground(color);
        rb->lcd_hline(0, LCD_WIDTH - 1, y);
    }
}

static void calm_center_text_bg(int y, const char *text, unsigned color,
                                unsigned background)
{
    int width;
    int height;

    rb->lcd_getstringsize((const unsigned char *)text, &width, &height);
    rb->lcd_set_foreground(color);
    rb->lcd_set_background(background);
    rb->lcd_putsxy(MAX(2, (LCD_WIDTH - width) / 2), y,
                   (const unsigned char *)text);
}

static void calm_center_text(int y, const char *text, unsigned color)
{
    calm_center_text_bg(y, text, color, CALM_BLUE);
}

static void calm_rounded_fill(int x, int y, int width, int height,
                              unsigned color)
{
    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x + 3, y, width - 6, height);
    rb->lcd_fillrect(x + 1, y + 2, width - 2, height - 4);
    rb->lcd_drawpixel(x + 2, y + 1);
    rb->lcd_drawpixel(x + width - 3, y + 1);
    rb->lcd_drawpixel(x + 2, y + height - 2);
    rb->lcd_drawpixel(x + width - 3, y + height - 2);
}

static void calm_fit_text(int x, int y, int max_width, const char *text)
{
    char buffer[100];
    int width;
    int height;
    int length;

    rb->strlcpy(buffer, text, sizeof(buffer));
    length = rb->strlen(buffer);
    while (length > 1)
    {
        rb->lcd_getstringsize((const unsigned char *)buffer, &width, &height);
        if (width <= max_width)
            break;
        buffer[--length] = '\0';
    }
    if (rb->strcmp(buffer, text) && length > 3)
    {
        buffer[length - 1] = '.';
        buffer[length - 2] = '.';
        buffer[length - 3] = '.';
    }
    rb->lcd_putsxy(x, y, (const unsigned char *)buffer);
}

static void calm_header(const char *title)
{
    rb->lcd_set_foreground(CALM_NAVY);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 22);
    rb->lcd_set_foreground(CALM_WHITE);
    rb->lcd_set_background(CALM_NAVY);
    rb->lcd_putsxy(7, 5, (const unsigned char *)title);
}

static bool calm_split_row(char *line, char **fields, int count)
{
    int index;
    char *cursor = line;

    for (index = 0; index < count; index++)
    {
        char *tab;

        fields[index] = cursor;
        if (index == count - 1)
            return true;
        tab = rb->strchr(cursor, '\t');
        if (!tab)
            return false;
        *tab = '\0';
        cursor = tab + 1;
    }
    return true;
}

static bool calm_load_library(void)
{
    char line[512];
    int fd;
    int length;

    calm_item_count = 0;
    fd = rb->open(CALM_MANIFEST, O_RDONLY);
    if (fd < 0)
        return false;
    length = rb->read_line(fd, line, sizeof(line));
    if (length <= 0 || rb->strcmp(line, CALM_VERSION))
    {
        rb->close(fd);
        return false;
    }
    while (calm_item_count < CALM_MAX_ITEMS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[5];
        struct calm_item *item;

        if (!calm_split_row(line, fields, 5))
            continue;
        item = &calm_items[calm_item_count];
        rb->strlcpy(item->title, fields[0], sizeof(item->title));
        rb->strlcpy(item->category, fields[1], sizeof(item->category));
        item->duration = rb->atoi(fields[2]);
        rb->strlcpy(item->path, fields[3], sizeof(item->path));
        if (!item->title[0] || !item->path[0])
            continue;
        calm_item_count++;
    }
    rb->close(fd);
    return true;
}

static void calm_load_icon(void)
{
    calm_icon.data = calm_icon_data;
    calm_icon_valid =
        rb->read_bmp_file(CALM_ICON, &calm_icon, sizeof(calm_icon_data),
                          FORMAT_NATIVE, NULL) > 0 &&
        calm_icon.width == CALM_ICON_W && calm_icon.height == CALM_ICON_H;
}

static int calm_category_count(int category)
{
    int index;
    int count = 0;

    for (index = 0; index < calm_item_count; index++)
    {
        if (!rb->strcmp(calm_items[index].category,
                        calm_categories[category]))
            count++;
    }
    return count;
}

static void calm_build_visible(void)
{
    int index;

    calm_visible_count = 0;
    for (index = 0; index < calm_item_count; index++)
    {
        if (!rb->strcmp(calm_items[index].category,
                        calm_categories[calm_category]))
            calm_visible[calm_visible_count++] = index;
    }
    if (calm_selected >= calm_visible_count)
        calm_selected = MAX(0, calm_visible_count - 1);
}

static void calm_draw_home(void)
{
    int index;

    calm_gradient();
    rb->lcd_setfont(FONT_UI);
    if (calm_icon_valid)
        rb->lcd_bmp_part(&calm_icon, 0, 0, 136, 8, 48, 48);
    else
        calm_center_text(17, "Calm", CALM_WHITE);
    calm_center_text(61, "Take a deep breath", CALM_WHITE);
    calm_center_text(79, "What do you need right now?", CALM_DIM);

    calm_rounded_fill(8, 100, LCD_WIDTH - 16, LCD_HEIGHT - 105,
                      CALM_NAVY);

    for (index = 0; index < (int)ARRAYLEN(calm_categories); index++)
    {
        int y = 108 + index * 25;
        char count[12];

        if (index == calm_category)
        {
            calm_rounded_fill(17, y, LCD_WIDTH - 34, 22,
                              CALM_LAVENDER);
            rb->lcd_set_foreground(CALM_NAVY);
            rb->lcd_set_background(CALM_LAVENDER);
        }
        else
        {
            calm_rounded_fill(17, y, LCD_WIDTH - 34, 22,
                              CALM_CARD);
            rb->lcd_set_foreground(CALM_WHITE);
            rb->lcd_set_background(CALM_CARD);
        }
        rb->lcd_putsxy(29, y + 4,
                       (const unsigned char *)calm_categories[index]);
        rb->snprintf(count, sizeof(count), "%d",
                     calm_category_count(index));
        rb->lcd_putsxy(LCD_WIDTH - 54, y + 4,
                       (const unsigned char *)count);
        if (index == calm_category)
            rb->lcd_putsxy(LCD_WIDTH - 36, y + 4,
                           (const unsigned char *)">");
    }
    rb->lcd_update();
}

static void calm_draw_empty(void)
{
    calm_gradient();
    calm_header(calm_categories[calm_category]);
    calm_center_text(82, "Nothing here yet", CALM_WHITE);
    calm_center_text(108, "Sync sounds with Rockpod", CALM_DIM);
    calm_center_text(180, "MENU  Back", CALM_WHITE);
    rb->lcd_update();
}

static void calm_draw_library(void)
{
    int visible_rows = 8;
    int top;
    int row;

    if (calm_visible_count == 0)
    {
        calm_draw_empty();
        return;
    }
    top = calm_selected >= visible_rows ?
          calm_selected - visible_rows + 1 : 0;
    calm_gradient();
    calm_header("Calm");
    rb->lcd_set_background(CALM_BLUE);
    rb->lcd_set_foreground(CALM_WHITE);
    rb->lcd_putsxy(12, 29,
                   (const unsigned char *)calm_categories[calm_category]);
    rb->lcd_set_foreground(CALM_DIM);
    rb->lcd_set_background(CALM_BLUE);
    rb->lcd_putsxy(12, 46, (const unsigned char *)"Your offline library");
    calm_rounded_fill(6, 65, LCD_WIDTH - 12, LCD_HEIGHT - 70,
                      CALM_NAVY);
    for (row = 0; row < visible_rows && top + row < calm_visible_count; row++)
    {
        int list_index = top + row;
        int item_index = calm_visible[list_index];
        int y = 72 + row * 20;
        bool active = list_index == calm_selected;
        char duration[12];

        calm_rounded_fill(12, y, LCD_WIDTH - 24, 18,
                          active ? CALM_LAVENDER : CALM_CARD);
        rb->lcd_set_background(active ? CALM_LAVENDER : CALM_CARD);
        rb->lcd_set_foreground(active ? CALM_NAVY : CALM_WHITE);
        calm_fit_text(20, y + 2, LCD_WIDTH - 84,
                      calm_items[item_index].title);
        rb->snprintf(duration, sizeof(duration), "%ld:%02ld",
                     calm_items[item_index].duration / 60,
                     calm_items[item_index].duration % 60);
        rb->lcd_putsxy(LCD_WIDTH - 58, y + 2,
                       (const unsigned char *)duration);
        if (active)
            rb->lcd_putsxy(LCD_WIDTH - 28, y + 2,
                           (const unsigned char *)">");
    }
    rb->lcd_update();
}

static bool calm_is_selected_playing(void)
{
    struct mp3entry *id3 = rb->audio_current_track();
    int item_index;

    if (calm_visible_count <= 0)
        return false;
    item_index = calm_visible[calm_selected];
    return id3 && !rb->strcmp(id3->path, calm_items[item_index].path) &&
           (rb->audio_status() & AUDIO_STATUS_PLAY);
}

static bool calm_start_selected(void)
{
    int item_index;

    if (calm_visible_count <= 0)
        return false;
    item_index = calm_visible[calm_selected];
    if (calm_is_selected_playing())
        return true;
    if (!rb->file_exists(calm_items[item_index].path))
    {
        rb->splash(HZ * 2, "Sound missing\nSync Calm again");
        return false;
    }
    /* This playlist takeover follows an explicit primary-media selection.
     * Rockbox owns decoding and PCM; the plugin takes no audio buffer. */
    if (rb->playlist_create(NULL, NULL) < 0 ||
        rb->playlist_insert_track(NULL, calm_items[item_index].path,
                                  PLAYLIST_INSERT_LAST,
                                  false, false) < 0)
    {
        rb->splash(HZ * 2, "Could not start sound");
        return false;
    }
    rb->playlist_sync(NULL);
    rb->playlist_start(0, 0, 0);
    return true;
}

static void calm_draw_mountains(void)
{
    int y;

    rb->lcd_set_foreground(LCD_RGBPACK(33, 75, 137));
    rb->lcd_fillrect(0, 142, LCD_WIDTH, 98);
    rb->lcd_set_foreground(LCD_RGBPACK(52, 94, 153));
    for (y = 112; y <= 178; y++)
    {
        int half = (y - 112) * 82 / 66;
        rb->lcd_hline(83 - half, 83 + half, y);
    }
    rb->lcd_set_foreground(LCD_RGBPACK(39, 79, 141));
    for (y = 98; y <= 178; y++)
    {
        int left = 216 - (y - 98) * 113 / 80;
        int right = 216 + (y - 98) * 103 / 80;
        rb->lcd_hline(left, right, y);
    }
    rb->lcd_set_foreground(LCD_RGBPACK(96, 146, 198));
    rb->lcd_hline(0, LCD_WIDTH - 1, 181);
    rb->lcd_hline(0, LCD_WIDTH - 1, 188);
    rb->lcd_hline(0, LCD_WIDTH - 1, 197);
}

static void calm_draw_player(void)
{
    struct mp3entry *id3 = rb->audio_current_track();
    int item_index = calm_visible[calm_selected];
    long elapsed = id3 ? id3->elapsed : 0;
    long length = id3 && id3->length > 0 ?
                  (long)id3->length :
                  calm_items[item_index].duration * 1000;
    int progress = length > 0 ?
                   MIN(LCD_WIDTH - 40, elapsed * (LCD_WIDTH - 40) / length) : 0;
    char time_text[32];
    bool paused = (rb->audio_status() & AUDIO_STATUS_PAUSE) != 0;

    calm_gradient();
    calm_header("Calm");
    calm_center_text(33, calm_items[item_index].category, CALM_DIM);
    rb->lcd_set_background(CALM_BLUE);
    rb->lcd_set_foreground(CALM_WHITE);
    calm_fit_text(18, 52, LCD_WIDTH - 36, calm_items[item_index].title);
    calm_draw_mountains();
    calm_rounded_fill(14, 141, LCD_WIDTH - 28, 91, CALM_NAVY);
    rb->lcd_set_foreground(CALM_CARD_SOFT);
    rb->lcd_fillrect(30, 158, LCD_WIDTH - 60, 5);
    rb->lcd_set_foreground(CALM_LAVENDER);
    rb->lcd_fillrect(30, 158, progress * (LCD_WIDTH - 60) /
                     (LCD_WIDTH - 40), 5);
    rb->snprintf(time_text, sizeof(time_text), "%ld:%02ld  /  %ld:%02ld",
                 elapsed / 60000, (elapsed / 1000) % 60,
                 length / 60000, (length / 1000) % 60);
    calm_center_text_bg(169, time_text, CALM_DIM, CALM_NAVY);
    calm_rounded_fill(129, 187, 62, 27, CALM_LAVENDER);
    calm_center_text_bg(193, paused ? "PLAY" : "PAUSE",
                        CALM_NAVY, CALM_LAVENDER);
    calm_center_text_bg(217, "Wheel Volume  MENU Back",
                        CALM_DIM, CALM_NAVY);
    rb->lcd_update();
}

static void calm_toggle_pause(void)
{
    if (!(rb->audio_status() & AUDIO_STATUS_PLAY))
    {
        calm_start_selected();
        return;
    }
    if (rb->audio_status() & AUDIO_STATUS_PAUSE)
        rb->audio_resume();
    else
        rb->audio_pause();
}

static void calm_stop_owned_playback(void)
{
    static const char sounds_root[] = CALM_ROOT "/sounds/";
    struct mp3entry *id3 = rb->audio_current_track();
    long deadline;

    if (!id3 ||
        rb->strncmp(id3->path, sounds_root, sizeof(sounds_root) - 1))
        return;

    rb->audio_stop();
    deadline = *rb->current_tick + HZ * 3;
    while ((rb->audio_status() & AUDIO_STATUS_PLAY) &&
           TIME_BEFORE(*rb->current_tick, deadline))
    {
        rb->sleep(1);
    }
}

static enum plugin_status calm_loop(void)
{
    static const struct button_mapping *contexts[] = { pla_main_ctx };
    enum calm_screen screen = CALM_HOME;
    enum plugin_status status = PLUGIN_OK;
    bool redraw = true;

    while (true)
    {
        int action;

        if (redraw || screen == CALM_PLAYER)
        {
            if (screen == CALM_HOME)
                calm_draw_home();
            else if (screen == CALM_LIBRARY)
                calm_draw_library();
            else
                calm_draw_player();
            redraw = false;
        }
        action = pluginlib_getaction(HZ / 10, contexts,
                                     ARRAYLEN(contexts));
        if (action == PLA_SCROLL_BACK ||
            action == PLA_SCROLL_BACK_REPEAT)
        {
            if (screen == CALM_HOME)
                calm_category =
                    (calm_category + ARRAYLEN(calm_categories) - 1) %
                    ARRAYLEN(calm_categories);
            else if (screen == CALM_LIBRARY && calm_visible_count > 0)
                calm_selected =
                    (calm_selected + calm_visible_count - 1) %
                    calm_visible_count;
            else if (screen == CALM_PLAYER)
                rb->adjust_volume(1);
            redraw = true;
        }
        else if (action == PLA_SCROLL_FWD ||
                 action == PLA_SCROLL_FWD_REPEAT)
        {
            if (screen == CALM_HOME)
                calm_category =
                    (calm_category + 1) % ARRAYLEN(calm_categories);
            else if (screen == CALM_LIBRARY && calm_visible_count > 0)
                calm_selected =
                    (calm_selected + 1) % calm_visible_count;
            else if (screen == CALM_PLAYER)
                rb->adjust_volume(-1);
            redraw = true;
        }
        else if (action == PLA_SELECT || action == PLA_SELECT_REL)
        {
            if (screen == CALM_HOME)
            {
                calm_selected = 0;
                calm_build_visible();
                screen = CALM_LIBRARY;
            }
            else if (screen == CALM_LIBRARY && calm_start_selected())
                screen = CALM_PLAYER;
            else if (screen == CALM_PLAYER)
                calm_toggle_pause();
            redraw = true;
        }
        else if (action == PLA_DOWN || action == PLA_DOWN_REPEAT)
        {
            if (screen == CALM_PLAYER)
            {
                calm_toggle_pause();
                redraw = true;
            }
        }
        else if (action == PLA_UP || action == PLA_CANCEL)
        {
            if (screen == CALM_PLAYER)
                screen = CALM_LIBRARY;
            else if (screen == CALM_LIBRARY)
                screen = CALM_HOME;
            else
                break;
            redraw = true;
        }
        else if (action == PLA_EXIT)
            break;

        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }
    }

    calm_stop_owned_playback();
    return status;
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    rb->lcd_set_viewport(NULL);
    rb->lcd_setfont(FONT_UI);
    calm_load_icon();
    if (!calm_load_library())
    {
        calm_gradient();
        calm_center_text(65, "Calm Library Unavailable", CALM_WHITE);
        calm_center_text(95, "Sync Calm with Rockpod", CALM_DIM);
        calm_center_text(178, "MENU  Back", CALM_WHITE);
        rb->lcd_update();
        rb->button_get(true);
        return PLUGIN_OK;
    }
    return calm_loop();
}
