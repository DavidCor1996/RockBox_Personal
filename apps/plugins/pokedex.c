/***************************************************************************
 * Pokedex -- browse the National Dex (Gen 1) with real species data and
 * real Game Boy sprites fetched by tools/pokedex_fetch_pokeapi.py.
 *
 * See docs/pokedex-spec.md for the on-device data model, the sampled
 * colour palette (taken from a real screencap of the original-series
 * anime Pokedex actively scanning a Pokemon), and the asset/legal policy
 * this plugin follows.
 ****************************************************************************/
#include "plugin.h"

#define PD_MAX_ENTRIES  151
#define PD_DIR          "/.rockbox/pokedex"
#define PD_TSV_FILE     PD_DIR "/pokedex.v1.tsv"
#define PD_SPRITE_DIR   PD_DIR "/sprites"

#define PD_LINE_MAX     400
#define PD_FIELD_COUNT  14

#define PD_NAME_MAX     20
#define PD_GENUS_MAX    28
#define PD_TYPE_MAX     12
#define PD_HEIGHT_MAX   10
#define PD_WEIGHT_MAX   14
#define PD_FLAVOR_MAX   208

#define PD_SPRITE_BYTES (48 * 1024)
#define PD_WRAP_MAX_LINES 3
#define PD_WRAP_LINE_MAX   64

/* Sampled from a real screencap of the original-series anime Pokedex
 * ("Dexter") actively scanning a Pokemon (Bulbapedia Archives,
 * File:Ash Original Pokedex scan.png) -- see docs/pokedex-spec.md
 * section 6. Unlike the closed-shell photo used for an earlier revision
 * of this palette, this frame shows the screen switched on: a solid pale
 * sage-green display with the scanned species rendered directly on it,
 * no card-within-card chrome -- so the plugin now draws one continuous
 * green "screen" instead of a light/dark two-panel layout. */
#define PD_COLOR_FRAME       LCD_RGBPACK(210,  28,  46) /* shell red */
#define PD_COLOR_CARD        LCD_RGBPACK(188, 206, 131) /* screen sage green */
#define PD_COLOR_BORDER      LCD_RGBPACK(217, 219, 221) /* bezel white */
#define PD_COLOR_DESC        PD_COLOR_CARD
#define PD_COLOR_INK         LCD_RGBPACK( 39,  55,  43) /* dark indicator green */
#define PD_COLOR_TITLE_TEXT  LCD_RGBPACK(217, 219, 221) /* bezel white */
#define PD_COLOR_BAR_FILL    LCD_RGBPACK( 39, 178, 229) /* lens blue */
#define PD_COLOR_BAR_TRACK   LCD_RGBPACK( 74,  81, 104) /* status-strip blue-grey */
#define PD_COLOR_DOT_RED     LCD_RGBPACK(192,  30,  56) /* shell indicator dot */
#define PD_COLOR_DOT_ORANGE  LCD_RGBPACK(233, 151,  57) /* shell indicator dot */

#define PD_MARGIN     4
#define PD_TITLE_Y    0
#define PD_TITLE_H    22
#define PD_CARD_Y     24
#define PD_CARD_H     194
#define PD_SPRITE_TARGET 128
#define PD_BOTTOM_Y   220
#define PD_BOTTOM_H   (LCD_HEIGHT - PD_BOTTOM_Y)

struct pokedex_entry
{
    int dex_id;
    unsigned char hp, atk, def, spa, spd, spe;
    char name[PD_NAME_MAX];
    char genus[PD_GENUS_MAX];
    char type1[PD_TYPE_MAX];
    char type2[PD_TYPE_MAX];
    char height_display[PD_HEIGHT_MAX];
    char weight_display[PD_WEIGHT_MAX];
    char flavor_text[PD_FLAVOR_MAX];
};

enum pd_view
{
    PD_VIEW_LIST = 0,
    PD_VIEW_INFO,
    PD_VIEW_STATS,
};

static struct pokedex_entry pd_entries[PD_MAX_ENTRIES];
static int pd_count = 0;

static unsigned char pd_sprite_data[PD_SPRITE_BYTES] CACHEALIGN_ATTR;
static struct bitmap pd_sprite_bitmap;
static int pd_sprite_loaded_id = -1;
static bool pd_sprite_ok = false;

static char pd_wrap_lines[PD_WRAP_MAX_LINES][PD_WRAP_LINE_MAX];
static int pd_wrap_count = 0;
static int pd_wrapped_for_id = -1;

/* ------------------------------------------------------------------ */
/* TSV loading                                                         */
/* ------------------------------------------------------------------ */

static int pd_split(char *line, char *fields[], int max_fields)
{
    int n = 0;
    char *p = line;

    fields[n++] = p;
    while (*p != '\0' && n < max_fields)
    {
        if (*p == '\t')
        {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    return n;
}

static void pd_load_tsv(void)
{
    int fd;
    char line[PD_LINE_MAX];
    bool first = true;

    pd_count = 0;
    fd = rb->open(PD_TSV_FILE, O_RDONLY);
    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) && pd_count < PD_MAX_ENTRIES)
    {
        char *fields[PD_FIELD_COUNT];
        struct pokedex_entry *e;
        int n;

        if (first)
        {
            first = false;
            continue;
        }
        if (line[0] == '\0')
            continue;

        n = pd_split(line, fields, PD_FIELD_COUNT);
        if (n < PD_FIELD_COUNT)
            continue;

        e = &pd_entries[pd_count];
        e->dex_id = rb->atoi(fields[0]);
        rb->strlcpy(e->name, fields[1], sizeof(e->name));
        rb->strlcpy(e->genus, fields[2], sizeof(e->genus));
        rb->strlcpy(e->type1, fields[3], sizeof(e->type1));
        rb->strlcpy(e->type2, fields[4], sizeof(e->type2));
        rb->strlcpy(e->height_display, fields[5], sizeof(e->height_display));
        rb->strlcpy(e->weight_display, fields[6], sizeof(e->weight_display));
        e->hp  = (unsigned char)rb->atoi(fields[7]);
        e->atk = (unsigned char)rb->atoi(fields[8]);
        e->def = (unsigned char)rb->atoi(fields[9]);
        e->spa = (unsigned char)rb->atoi(fields[10]);
        e->spd = (unsigned char)rb->atoi(fields[11]);
        e->spe = (unsigned char)rb->atoi(fields[12]);
        rb->strlcpy(e->flavor_text, fields[13], sizeof(e->flavor_text));
        pd_count++;
    }
    rb->close(fd);
}

/* ------------------------------------------------------------------ */
/* Sprite loading -- one species resident at a time, never the whole   */
/* set (docs/pokedex-spec.md section 4).                               */
/* ------------------------------------------------------------------ */

static bool pd_load_sprite(int dex_id)
{
    char path[MAX_PATH];
    int rc;

    if (pd_sprite_loaded_id == dex_id)
        return pd_sprite_ok;

    rb->snprintf(path, sizeof(path), "%s/%03d.bmp", PD_SPRITE_DIR, dex_id);
    rb->memset(&pd_sprite_bitmap, 0, sizeof(pd_sprite_bitmap));
    pd_sprite_bitmap.data = pd_sprite_data;
    /* Native size only: combining FORMAT_RESIZE with FORMAT_TRANSPARENT
     * corrupts the alpha compositing on this target (the scaled sprite
     * comes out as a solid black silhouette), so the real sprite is
     * drawn at its real 96x96 and made prominent through layout instead. */
    rc = rb->read_bmp_file(path, &pd_sprite_bitmap, sizeof(pd_sprite_data),
                            FORMAT_NATIVE | FORMAT_TRANSPARENT, NULL);
    pd_sprite_loaded_id = dex_id;
    pd_sprite_ok = (rc >= 0);
    return pd_sprite_ok;
}

/* ------------------------------------------------------------------ */
/* Word wrap for the real flavor text                                  */
/* ------------------------------------------------------------------ */

static void pd_wrap_flavor(const char *text, int max_width)
{
    int len = rb->strlen(text);
    int pos = 0;
    char line[PD_WRAP_LINE_MAX];

    pd_wrap_count = 0;
    line[0] = '\0';

    while (pos <= len && pd_wrap_count < PD_WRAP_MAX_LINES)
    {
        int wstart = pos;
        int wlen = 0;
        char word[32];
        char candidate[PD_WRAP_LINE_MAX];
        int w, h;

        while (pos < len && text[pos] != ' ')
        {
            pos++;
            wlen++;
        }
        if (wlen > (int)sizeof(word) - 1)
            wlen = (int)sizeof(word) - 1;
        rb->strlcpy(word, text + wstart, (size_t)wlen + 1);

        if (line[0] != '\0')
            rb->snprintf(candidate, sizeof(candidate), "%s %s", line, word);
        else
            rb->strlcpy(candidate, word, sizeof(candidate));

        rb->font_getstringsize((unsigned char *)candidate, &w, &h, FONT_UI);
        if (w > max_width && line[0] != '\0')
        {
            rb->strlcpy(pd_wrap_lines[pd_wrap_count], line,
                        sizeof(pd_wrap_lines[pd_wrap_count]));
            pd_wrap_count++;
            rb->strlcpy(line, word, sizeof(line));
        }
        else
        {
            rb->strlcpy(line, candidate, sizeof(line));
        }
        pos++;
    }
    if (line[0] != '\0' && pd_wrap_count < PD_WRAP_MAX_LINES)
    {
        rb->strlcpy(pd_wrap_lines[pd_wrap_count], line,
                    sizeof(pd_wrap_lines[pd_wrap_count]));
        pd_wrap_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Sound -- rb->beep_play() is Rockbox's own real square-wave generator */
/* (apps/beep.c), already used for keyclicks throughout the firmware.   */
/* It runs on the dedicated PCM_MIXER_CHAN_BEEP channel, never touching */
/* PCM_MIXER_CHAN_PLAYBACK or the shared audio buffer, so it needs none */
/* of the precautions in docs/plugin-audio-lifecycle-steering.md. A     */
/* two-tone "scan" chirp plays on opening a dex entry (echoing the      */
/* real device's scanning sound without redistributing any audio       */
/* actually ripped from the show) and a short tick plays on paging.     */
/* ------------------------------------------------------------------ */

static void pd_beep_scan(void)
{
    rb->beep_play(1046, 70, 4000);
    rb->sleep(HZ / 14);
    rb->beep_play(1568, 90, 4000);
}

static void pd_beep_tick(void)
{
    rb->beep_play(1568, 25, 3000);
}

static void pd_prepare_detail(int index)
{
    struct pokedex_entry *e = &pd_entries[index];

    pd_load_sprite(e->dex_id);
    if (pd_wrapped_for_id != e->dex_id)
    {
        pd_wrap_flavor(e->flavor_text,
                       LCD_WIDTH - 2 * (PD_MARGIN + 6));
        pd_wrapped_for_id = e->dex_id;
    }
}

/* ------------------------------------------------------------------ */
/* Type colours -- Bulbapedia's published type-colour chart             */
/* ------------------------------------------------------------------ */

static unsigned pd_type_color(const char *type)
{
    static const struct { const char *name; unsigned color; } table[] = {
        { "normal",   LCD_RGBPACK(168, 168, 120) },
        { "fire",     LCD_RGBPACK(240, 128,  48) },
        { "water",    LCD_RGBPACK(104, 144, 240) },
        { "electric", LCD_RGBPACK(248, 208,  48) },
        { "grass",    LCD_RGBPACK(120, 200,  80) },
        { "ice",      LCD_RGBPACK(152, 216, 216) },
        { "fighting", LCD_RGBPACK(192,  48,  40) },
        { "poison",   LCD_RGBPACK(160,  64, 160) },
        { "ground",   LCD_RGBPACK(224, 192, 104) },
        { "flying",   LCD_RGBPACK(168, 144, 240) },
        { "psychic",  LCD_RGBPACK(248,  88, 136) },
        { "bug",      LCD_RGBPACK(168, 184,  32) },
        { "rock",     LCD_RGBPACK(184, 160,  56) },
        { "ghost",    LCD_RGBPACK(112,  88, 152) },
        { "dragon",   LCD_RGBPACK(112,  56, 248) },
        { "dark",     LCD_RGBPACK(112,  88,  72) },
        { "steel",    LCD_RGBPACK(184, 184, 208) },
        { "fairy",    LCD_RGBPACK(238, 153, 172) },
    };
    unsigned i;

    for (i = 0; i < ARRAYLEN(table); i++)
    {
        if (rb->strcmp(type, table[i].name) == 0)
            return table[i].color;
    }
    return PD_COLOR_BORDER;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                              */
/* ------------------------------------------------------------------ */

/* Midpoint circle algorithm -- Rockbox's plugin API has no circle
 * primitive, only rects/lines/pixels, so the lens-style reticle and the
 * shell's indicator dots are plotted a pixel at a time. */
static void pd_draw_circle(int cx, int cy, int r)
{
    int x = r;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
        rb->lcd_drawpixel(cx + x, cy + y);
        rb->lcd_drawpixel(cx + y, cy + x);
        rb->lcd_drawpixel(cx - y, cy + x);
        rb->lcd_drawpixel(cx - x, cy + y);
        rb->lcd_drawpixel(cx - x, cy - y);
        rb->lcd_drawpixel(cx - y, cy - x);
        rb->lcd_drawpixel(cx + y, cy - x);
        rb->lcd_drawpixel(cx + x, cy - y);

        y++;
        if (err <= 0)
            err += 2 * y + 1;
        if (err > 0)
        {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static void pd_fill_dot(int cx, int cy, int r)
{
    int dx, dy;

    for (dy = -r; dy <= r; dy++)
    {
        for (dx = -r; dx <= r; dx++)
        {
            if (dx * dx + dy * dy <= r * r)
                rb->lcd_drawpixel(cx + dx, cy + dy);
        }
    }
}

static void pd_draw_centered(int y, const char *text, unsigned color)
{
    int w, h;

    rb->font_getstringsize((unsigned char *)text, &w, &h, FONT_UI);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy((LCD_WIDTH - w) / 2, y, (unsigned char *)text);
}

static void pd_draw_chrome(const struct pokedex_entry *e)
{
    /* PokeAPI's genus already reads e.g. "Seed Pokemon" -- it is the full
     * category name, not a noun that needs "POKEMON" appended. */
    rb->lcd_set_foreground(PD_COLOR_FRAME);
    rb->lcd_fillrect(0, PD_TITLE_Y, LCD_WIDTH, PD_TITLE_H);
    rb->lcd_fillrect(0, PD_BOTTOM_Y, LCD_WIDTH, PD_BOTTOM_H);

    pd_draw_centered(6, e->genus, PD_COLOR_TITLE_TEXT);

    /* Three small indicator dots on the shell, echoing the real device's
     * lens-side status lights in the reference screencap. */
    rb->lcd_set_foreground(PD_COLOR_DOT_RED);
    pd_fill_dot(LCD_WIDTH - 46, 11, 3);
    rb->lcd_set_foreground(PD_COLOR_DOT_ORANGE);
    pd_fill_dot(LCD_WIDTH - 34, 11, 3);
    rb->lcd_set_foreground(PD_COLOR_INK);
    pd_fill_dot(LCD_WIDTH - 22, 11, 3);
}

static void pd_draw_info(const struct pokedex_entry *e)
{
    char line[64];
    int i;
    int sprite_x, sprite_y, sprite_top, flavor_top;

    pd_draw_chrome(e);

    /* One continuous green "screen", the way the real device shows the
     * scanned species directly on the display rather than inside a
     * second light-coloured card. */
    rb->lcd_set_foreground(PD_COLOR_CARD);
    rb->lcd_fillrect(PD_MARGIN, PD_CARD_Y, LCD_WIDTH - 2 * PD_MARGIN,
                      PD_CARD_H);
    rb->lcd_set_foreground(PD_COLOR_BORDER);
    rb->lcd_drawrect(PD_MARGIN, PD_CARD_Y, LCD_WIDTH - 2 * PD_MARGIN,
                      PD_CARD_H);

    rb->lcd_set_foreground(PD_COLOR_INK);
    rb->snprintf(line, sizeof(line), "No.%03d %s", e->dex_id, e->name);
    rb->lcd_putsxy(PD_MARGIN + 6, PD_CARD_Y + 4, (unsigned char *)line);

    rb->snprintf(line, sizeof(line), "HT %-9s WT %s",
                 e->height_display, e->weight_display);
    rb->lcd_putsxy(PD_MARGIN + 6, PD_CARD_Y + 18, (unsigned char *)line);

    sprite_top = PD_CARD_Y + 36;
    flavor_top = PD_CARD_Y + PD_CARD_H - (pd_wrap_count * 13) - 6;

    {
        /* A round lens-style viewfinder behind the sprite, echoing the
         * device's real camera-lens ring (lens blue) so a small
         * native-resolution sprite reads as a deliberately framed scan
         * target rather than an icon adrift in empty screen space. */
        int center_x = LCD_WIDTH / 2;
        int center_y = (sprite_top + flavor_top) / 2;

        rb->lcd_set_foreground(PD_COLOR_BORDER);
        pd_draw_circle(center_x, center_y, 62);
        pd_draw_circle(center_x, center_y, 61);
        rb->lcd_set_foreground(PD_COLOR_BAR_FILL);
        pd_draw_circle(center_x, center_y, 54);
        pd_draw_circle(center_x, center_y, 53);
    }

    if (pd_load_sprite(e->dex_id))
    {
        sprite_x = LCD_WIDTH / 2 - pd_sprite_bitmap.width / 2;
        sprite_y = (sprite_top + flavor_top) / 2 - pd_sprite_bitmap.height / 2;
        rb->lcd_bmp_part(&pd_sprite_bitmap, 0, 0, sprite_x, sprite_y,
                          pd_sprite_bitmap.width, pd_sprite_bitmap.height);
    }

    rb->lcd_set_foreground(PD_COLOR_INK);
    for (i = 0; i < pd_wrap_count; i++)
    {
        rb->lcd_putsxy(PD_MARGIN + 6, flavor_top + i * 13,
                       (unsigned char *)pd_wrap_lines[i]);
    }

    rb->lcd_set_foreground(PD_COLOR_TITLE_TEXT);
    rb->lcd_putsxy(PD_MARGIN + 4, PD_BOTTOM_Y + 4,
                   (unsigned char *)"<PREV MENU:STATS NEXT> LEFT:LIST");
}

static void pd_draw_stat_bar(int y, const char *label, unsigned char value)
{
    int track_x = PD_MARGIN + 60;
    int track_w = LCD_WIDTH - 2 * PD_MARGIN - 60 - 30;
    int fill_w = (track_w * value) / 255;
    char num[8];

    rb->lcd_set_foreground(PD_COLOR_INK);
    rb->lcd_putsxy(PD_MARGIN + 6, y, (unsigned char *)label);

    rb->lcd_set_foreground(PD_COLOR_BAR_TRACK);
    rb->lcd_fillrect(track_x, y, track_w, 10);
    rb->lcd_set_foreground(PD_COLOR_BAR_FILL);
    rb->lcd_fillrect(track_x, y, fill_w, 10);
    rb->lcd_set_foreground(PD_COLOR_BORDER);
    rb->lcd_drawrect(track_x, y, track_w, 10);

    rb->snprintf(num, sizeof(num), "%d", value);
    rb->lcd_set_foreground(PD_COLOR_INK);
    rb->lcd_putsxy(track_x + track_w + 4, y, (unsigned char *)num);
}

static void pd_draw_stats(const struct pokedex_entry *e)
{
    int chip_x = PD_MARGIN + 6;
    int chip_y = PD_CARD_Y + 10;
    char chip[16];
    int w, h;

    pd_draw_chrome(e);

    rb->lcd_set_foreground(PD_COLOR_CARD);
    rb->lcd_fillrect(PD_MARGIN, PD_CARD_Y, LCD_WIDTH - 2 * PD_MARGIN,
                      PD_CARD_H);
    rb->lcd_set_foreground(PD_COLOR_BORDER);
    rb->lcd_drawrect(PD_MARGIN, PD_CARD_Y, LCD_WIDTH - 2 * PD_MARGIN,
                      PD_CARD_H);

    rb->snprintf(chip, sizeof(chip), " %s ", e->type1);
    rb->font_getstringsize((unsigned char *)chip, &w, &h, FONT_UI);
    rb->lcd_set_foreground(pd_type_color(e->type1));
    rb->lcd_fillrect(chip_x, chip_y, w, h + 2);
    rb->lcd_set_foreground(PD_COLOR_TITLE_TEXT);
    rb->lcd_putsxy(chip_x, chip_y + 1, (unsigned char *)chip);
    chip_x += w + 6;

    if (e->type2[0] != '\0')
    {
        rb->snprintf(chip, sizeof(chip), " %s ", e->type2);
        rb->font_getstringsize((unsigned char *)chip, &w, &h, FONT_UI);
        rb->lcd_set_foreground(pd_type_color(e->type2));
        rb->lcd_fillrect(chip_x, chip_y, w, h + 2);
        rb->lcd_set_foreground(PD_COLOR_TITLE_TEXT);
        rb->lcd_putsxy(chip_x, chip_y + 1, (unsigned char *)chip);
    }

    pd_draw_stat_bar(PD_CARD_Y + 40,  "HP ", e->hp);
    pd_draw_stat_bar(PD_CARD_Y + 62,  "ATK", e->atk);
    pd_draw_stat_bar(PD_CARD_Y + 84,  "DEF", e->def);
    pd_draw_stat_bar(PD_CARD_Y + 106, "SPA", e->spa);
    pd_draw_stat_bar(PD_CARD_Y + 128, "SPD", e->spd);
    pd_draw_stat_bar(PD_CARD_Y + 150, "SPE", e->spe);

    rb->lcd_set_foreground(PD_COLOR_TITLE_TEXT);
    rb->lcd_putsxy(PD_MARGIN + 4, PD_BOTTOM_Y + 4,
                   (unsigned char *)"<PREV MENU:INFO NEXT> LEFT:LIST");
}

static void pd_draw_missing_pack(void)
{
    rb->lcd_clear_display();
    rb->lcd_set_foreground(PD_COLOR_INK);
    pd_draw_centered(60, "Pokedex data not found", PD_COLOR_INK);
    pd_draw_centered(90, "Run on a computer:", PD_COLOR_INK);
    pd_draw_centered(110, "tools/pokedex_fetch_pokeapi.py", PD_COLOR_INK);
    pd_draw_centered(130, "--deploy-root <device>", PD_COLOR_INK);
    pd_draw_centered(160, "Expected at:", PD_COLOR_INK);
    pd_draw_centered(180, PD_TSV_FILE, PD_COLOR_INK);
}

/* ------------------------------------------------------------------ */
/* List screen                                                         */
/* ------------------------------------------------------------------ */

static const char *pd_list_name_cb(int selected_item, void *data,
                                    char *buf, size_t buf_len)
{
    struct pokedex_entry *e = &pd_entries[selected_item];

    (void)data;
    rb->snprintf(buf, buf_len, "#%03d %s", e->dex_id, e->name);
    return buf;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

enum plugin_status plugin_start(const void *parameter)
{
    struct gui_synclist lists;
    enum pd_view view = PD_VIEW_LIST;
    int cur = 0;
    bool quit = false;

    (void)parameter;

#if LCD_DEPTH > 1
    rb->lcd_set_backdrop(NULL);
#endif
    rb->lcd_setfont(FONT_UI);
    /* Text draws only foreground pixels, leaving each region's fillrect
     * background intact underneath (default SOLID mode would paint an
     * opaque box behind every string). */
    rb->lcd_set_drawmode(DRMODE_FG);

    pd_load_tsv();
    if (pd_count == 0)
    {
        pd_draw_missing_pack();
        rb->lcd_update();
        while (true)
        {
            int action = rb->get_action(CONTEXT_STD, TIMEOUT_BLOCK);

            if (action == ACTION_STD_CANCEL || action == ACTION_STD_OK)
                break;
            if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                return PLUGIN_USB_CONNECTED;
        }
        return PLUGIN_OK;
    }

    rb->gui_synclist_init(&lists, pd_list_name_cb, NULL, false, 1, NULL);
    rb->gui_synclist_set_title(&lists, "Pokedex", Icon_NOICON);
    rb->gui_synclist_set_nb_items(&lists, pd_count);
    rb->gui_synclist_select_item(&lists, 0);

    while (!quit)
    {
        if (view == PD_VIEW_LIST)
        {
            int button;

            rb->gui_synclist_draw(&lists);
            button = rb->get_action(CONTEXT_LIST, TIMEOUT_BLOCK);
            if (rb->gui_synclist_do_button(&lists, &button))
                continue;

            switch (button)
            {
                case ACTION_STD_OK:
                    cur = rb->gui_synclist_get_sel_pos(&lists);
                    pd_prepare_detail(cur);
                    pd_beep_scan();
                    view = PD_VIEW_INFO;
                    break;
                case ACTION_STD_CANCEL:
                    quit = true;
                    break;
                default:
                    if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                        return PLUGIN_USB_CONNECTED;
                    break;
            }
        }
        else
        {
            int action;

            if (view == PD_VIEW_INFO)
                pd_draw_info(&pd_entries[cur]);
            else
                pd_draw_stats(&pd_entries[cur]);
            rb->lcd_update();

            action = rb->get_action(CONTEXT_STD, TIMEOUT_BLOCK);
            switch (action)
            {
                case ACTION_STD_NEXT:
                case ACTION_STD_NEXTREPEAT:
                    if (cur < pd_count - 1)
                    {
                        cur++;
                        pd_prepare_detail(cur);
                        pd_beep_tick();
                    }
                    break;
                case ACTION_STD_PREV:
                case ACTION_STD_PREVREPEAT:
                    if (cur > 0)
                    {
                        cur--;
                        pd_prepare_detail(cur);
                        pd_beep_tick();
                    }
                    break;
                case ACTION_STD_MENU:
                    view = (view == PD_VIEW_INFO) ? PD_VIEW_STATS
                                                   : PD_VIEW_INFO;
                    pd_beep_tick();
                    break;
                case ACTION_STD_CANCEL:
                    view = PD_VIEW_LIST;
                    rb->gui_synclist_select_item(&lists, cur);
                    break;
                default:
                    if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                        return PLUGIN_USB_CONNECTED;
                    break;
            }
        }
    }
    return PLUGIN_OK;
}
