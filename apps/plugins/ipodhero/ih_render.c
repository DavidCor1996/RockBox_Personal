/***************************************************************************
 * iPod Hero cached-pixel renderer
 ****************************************************************************/

#include "ipodhero.h"

#define IH_HEADER_H 20
/* Slightly tighter than a stock Rockbox list so six fret bars, a two line
 * song billing and the control footer all fit a 240 pixel tall panel. */
#define IH_MENU_ROW_H 20
#define IH_HIGHWAY_X 60
#define IH_HIGHWAY_Y 20
#define IH_RECEPTOR_Y 207
#define IH_GEM_SIZE 32
#define IH_STAGE_CUT_MS 7000
#define IH_STAGE_WIPE_MS 700

static void ih_draw_background(const struct ih_app *app)
{
    rb->lcd_bitmap((const fb_data *)app->skin.background.bitmap.data,
                   0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static void ih_draw_text_center(int y, const char *text, unsigned color)
{
    int width;
    rb->lcd_getstringsize(text, &width, NULL);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2 + 1, y + 1, text);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2, y, text);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

/* Fret colours in the conventional five-lane order.  The menu chrome reuses
 * them so every screen reads as part of the same instrument.  The component
 * table exists because the selection bars shade towards each fret colour and
 * the LCD driver only takes packed values. */
static const unsigned char ih_fret_rgb[IH_LANE_COUNT][3] =
{
    {  45, 205,  72 },
    { 235,  55,  62 },
    { 245, 205,  45 },
    {  60, 120, 235 },
    { 245, 135,  35 }
};

static const unsigned ih_fret_colors[IH_LANE_COUNT] =
{
    LCD_RGBPACK(45, 205, 72),
    LCD_RGBPACK(235, 55, 62),
    LCD_RGBPACK(245, 205, 45),
    LCD_RGBPACK(60, 120, 235),
    LCD_RGBPACK(245, 135, 35)
};

static void ih_draw_vgradient(int x, int y, int width, int height,
                              int r0, int g0, int b0,
                              int r1, int g1, int b1)
{
    int span = MAX(1, height - 1);
    int row;

    for (row = 0; row < height; ++row)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(r0 + (r1 - r0) * row / span,
                                           g0 + (g1 - g0) * row / span,
                                           b0 + (b1 - b0) * row / span));
        rb->lcd_hline(x, x + width - 1, y + row);
    }
}

/* The five-colour rule that separates the stage from the chrome. */
static void ih_draw_fret_rule(int x, int y, int width, int height)
{
    int column;

    for (column = 0; column < width; ++column)
    {
        rb->lcd_set_foreground(
            ih_fret_colors[column * IH_LANE_COUNT / MAX(1, width)]);
        rb->lcd_vline(x + column, y, y + height - 1);
    }
}

/* Darken the stage photograph behind a block of text without hiding it.  The
 * LCD driver has no alpha blend, so this lays down black scanlines: it reads
 * as stage lighting rather than as a dither, and costs one hline per pair of
 * rows instead of a pixel call per pixel. */
static void ih_draw_scrim(int x, int y, int width, int height)
{
    int row;

    rb->lcd_set_foreground(LCD_BLACK);
    for (row = 0; row < height; row += 2)
        rb->lcd_hline(x, x + width - 1, y + row);
}

static void ih_draw_panel(int x, int y, int width, int height)
{
    ih_draw_scrim(x, y, width, height);
    rb->lcd_set_foreground(LCD_RGBPACK(24, 24, 30));
    rb->lcd_fillrect(x + 2, y + 2, width - 4, height - 4);
    rb->lcd_set_foreground(LCD_RGBPACK(96, 96, 108));
    rb->lcd_drawrect(x, y, width, height);
    rb->lcd_set_foreground(LCD_RGBPACK(148, 148, 160));
    rb->lcd_hline(x + 1, x + width - 2, y + 1);
}

static void ih_draw_screen_header(const char *title)
{
    int width;

    ih_draw_vgradient(0, 0, LCD_WIDTH, IH_HEADER_H - 2,
                      34, 34, 42, 10, 10, 14);
    ih_draw_fret_rule(0, IH_HEADER_H - 2, LCD_WIDTH, 2);
    rb->lcd_getstringsize(title, &width, NULL);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2 + 1, 4, title);
    rb->lcd_set_foreground(LCD_RGBPACK(248, 248, 252));
    rb->lcd_putsxy((LCD_WIDTH - width) / 2, 3, title);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void ih_draw_control_footer(const char *menu_action,
                                   const char *select_action)
{
    char left[48];
    char right[48];
    int width;
    int top = LCD_HEIGHT - 21;

    ih_draw_fret_rule(0, top, LCD_WIDTH, 2);
    ih_draw_vgradient(0, top + 2, LCD_WIDTH, 19, 14, 14, 18, 34, 34, 42);
    rb->snprintf(left, sizeof(left), "MENU  %s", menu_action);
    rb->snprintf(right, sizeof(right), "CENTER  %s", select_action);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_RGBPACK(214, 214, 222));
    rb->lcd_putsxy(10, LCD_HEIGHT - 16, left);
    rb->lcd_getstringsize(right, &width, NULL);
    rb->lcd_putsxy(LCD_WIDTH - width - 10, LCD_HEIGHT - 16, right);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

/* Guitar Hero style selection bars: slanted, fret coloured, and lit from the
 * top.  The geometry matches the old stock rows exactly so every caller's
 * layout and selection indices are unchanged. */
static void ih_draw_menu_rows(const char *const *items, int count,
                              int selected, int top)
{
    const int row_h = IH_MENU_ROW_H;
    const int x = 16;
    const int width = LCD_WIDTH - x * 2;
    const int slant = 7;
    int i;

    for (i = 0; i < count; ++i)
    {
        int y = top + i * row_h;
        unsigned accent = ih_fret_colors[i % IH_LANE_COUNT];
        int bar_h = row_h - 3;
        int span = MAX(1, bar_h - 1);
        int row;

        for (row = 0; row < bar_h; ++row)
        {
            int shift = (bar_h - 1 - row) * slant / bar_h;
            int left = x + shift;
            int right = x + width - 1 - shift / 2;

            if (i == selected)
            {
                /* Lit fret bar: full colour at the top edge falling to a
                 * third of it at the bottom, so white text stays legible. */
                int lit = 100 + 155 * (span - row) / span;
                rb->lcd_set_foreground(
                    LCD_RGBPACK(ih_fret_rgb[i % IH_LANE_COUNT][0] * lit / 255,
                                ih_fret_rgb[i % IH_LANE_COUNT][1] * lit / 255,
                                ih_fret_rgb[i % IH_LANE_COUNT][2] * lit / 255));
            }
            else
            {
                rb->lcd_set_foreground(
                    LCD_RGBPACK(20 + 26 * (span - row) / span,
                                20 + 26 * (span - row) / span,
                                26 + 30 * (span - row) / span));
            }
            rb->lcd_hline(left, right, y + row);
        }

        /* Fret cap on the leading edge, brightened for the active row. */
        rb->lcd_set_foreground(accent);
        for (row = 0; row < bar_h; ++row)
        {
            int shift = (bar_h - 1 - row) * slant / bar_h;
            rb->lcd_hline(x + shift, x + shift + (i == selected ? 5 : 3),
                          y + row);
        }
        if (i == selected)
        {
            rb->lcd_set_foreground(LCD_RGBPACK(252, 252, 255));
            rb->lcd_hline(x + slant, x + width - 1, y);
        }

        rb->lcd_set_drawmode(DRMODE_FG);
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(x + 15, y + 4, items[i]);
        rb->lcd_set_foreground(i == selected ? LCD_RGBPACK(255, 255, 255) :
                               LCD_RGBPACK(186, 186, 196));
        rb->lcd_putsxy(x + 14, y + 3, items[i]);
        if (i == selected)
        {
            rb->lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
            rb->lcd_putsxy(x + width - 16, y + 3, ">");
        }
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
}

void ih_render_song_library(const struct ih_song_library *library,
                            int selected)
{
    enum { visible_rows = 7 };
    const char *items[visible_rows];
    char labels[visible_rows][96];
    char count_text[32];
    int first;
    int count;
    int row;

    selected = MAX(0, MIN(selected, library->count - 1));
    first = selected / visible_rows * visible_rows;
    count = MIN(visible_rows, library->count - first);
    for (row = 0; row < count; row++)
    {
        const struct ih_index_entry *entry =
            &library->entries[first + row];
        rb->snprintf(labels[row], sizeof(labels[row]), "%.18s - %.20s",
                     entry->artist, entry->title);
        items[row] = labels[row];
    }

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    ih_draw_vgradient(0, IH_HEADER_H, LCD_WIDTH, LCD_HEIGHT - IH_HEADER_H,
                      18, 18, 24, 6, 6, 9);
    rb->snprintf(count_text, sizeof(count_text), "Songs  %d / %d",
                 selected + 1, library->count);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_RGBPACK(190, 190, 200));
    rb->lcd_putsxy(20, 24, count_text);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    ih_draw_menu_rows(items, count, selected - first, 43);
    ih_draw_control_footer("Exit", "Open");
    ih_draw_screen_header("iPod Hero Songs");
    rb->lcd_update();
}

void ih_render_menu(const struct ih_app *app, int selected,
                    const char *message)
{
    const char *items[6];
    char difficulty[32];
    char detail[96];
    char best[64];
    static const char *const difficulty_name[] =
    {
        "Easy", "Medium", "Hard", "Expert"
    };
    static const char *const origin_name[] =
    {
        "AUTHORED", "IMPORTED", "GENERATED"
    };

    (void)message;
    rb->snprintf(difficulty, sizeof(difficulty), "Difficulty: %s",
                 difficulty_name[app->selected_difficulty]);
    items[0] = "Start Song";
    items[1] = "Practice Mode";
    items[2] = difficulty;
    items[3] = "Calibrate Timing";
    items[4] = "Game Settings";
    items[5] = "Choose Song";

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    ih_draw_background(app);
    /* Stage banner: a darkened strip of the concert photograph billing the
     * track, closed by a fret rule.  The logo lives on the Settings screen,
     * which is the only one with room for the full stacked wordmark. */
    ih_draw_scrim(0, IH_HEADER_H, LCD_WIDTH, 40);
    ih_draw_fret_rule(0, 60, LCD_WIDTH, 2);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, 62, LCD_WIDTH, 34);
    rb->snprintf(detail, sizeof(detail), "%s - %s",
                 app->index.artist, app->index.title);
    ih_draw_text_center(63, detail, LCD_WHITE);
    if (app->best_score > 0)
        rb->snprintf(best, sizeof(best), "%s  Best %08lu  %lu.%02lu%%",
                     origin_name[app->chart.origin],
                     (unsigned long)app->best_score,
                     (unsigned long)(app->best_accuracy_bp / 100),
                     (unsigned long)(app->best_accuracy_bp % 100));
    else
        rb->snprintf(best, sizeof(best), "%s  No ranked score  %+d ms",
                     origin_name[app->chart.origin],
                     app->game.calibration_ms);
    ih_draw_text_center(79, best, LCD_LIGHTGRAY);
    ih_draw_menu_rows(items, ARRAYLEN(items), selected, 98);
    ih_draw_control_footer("Songs", "Choose");
    ih_draw_screen_header("iPod Hero");
    rb->lcd_update();
}

static void ih_draw_game_background(const struct ih_app *app)
{
    const struct ih_game *game = &app->game;
    const struct ih_image *current;
    const struct ih_image *next;
    int song_time = MAX(0, game->song_time_ms);
    int cut = song_time / IH_STAGE_CUT_MS;
    int phase = song_time % IH_STAGE_CUT_MS;

    if (cut & 1)
    {
        current = &app->skin.background_alt;
        next = &app->skin.background;
    }
    else
    {
        current = &app->skin.background;
        next = &app->skin.background_alt;
    }
    rb->lcd_bitmap((const fb_data *)current->bitmap.data,
                   0, 0, LCD_WIDTH, LCD_HEIGHT);
    if (phase >= IH_STAGE_CUT_MS - IH_STAGE_WIPE_MS)
    {
        int transition = phase - (IH_STAGE_CUT_MS - IH_STAGE_WIPE_MS);
        int strip;

        for (strip = 0; strip < 8; ++strip)
        {
            int y = strip * (LCD_HEIGHT / 8);
            int width = transition * (LCD_WIDTH + 56) /
                        IH_STAGE_WIPE_MS - strip * 8;

            width = MAX(0, MIN(width, LCD_WIDTH));
            if (width > 0)
                rb->lcd_bitmap_part(
                    (const fb_data *)next->bitmap.data,
                    0, y, next->bitmap.width, 0, y,
                    width, LCD_HEIGHT / 8);
        }
    }
}

static int ih_lane_x(int lane, int y)
{
    int travel = y - IH_HIGHWAY_Y;
    int top_x = 140 + lane * 10;
    int bottom_x = 80 + lane * 40;

    if (travel < 0)
        travel = 0;
    if (travel > 190)
        travel = 190;
    return top_x + (bottom_x - top_x) * travel / 190;
}

static int ih_note_y(int delta_ms)
{
    int progress = IH_APPROACH_MS - delta_ms;
    if (progress < 0)
        progress = 0;
    if (progress > IH_APPROACH_MS)
        progress = IH_APPROACH_MS;
    return IH_HIGHWAY_Y + progress * 190 / IH_APPROACH_MS;
}

static void ih_draw_atlas_lane(const struct ih_image *image, int lane,
                               int band, int x, int y)
{
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)image->bitmap.data, lane * IH_GEM_SIZE,
        band * IH_GEM_SIZE,
        image->bitmap.width, x - IH_GEM_SIZE / 2,
        y - IH_GEM_SIZE / 2, IH_GEM_SIZE, IH_GEM_SIZE);
}

static void ih_draw_sustain(const struct ih_app *app,
                            const struct ih_note_event *event, int lane,
                            int song_time)
{
    int start_y = ih_note_y((int)event->time_ms - song_time);
    int end_y = ih_note_y((int)(event->time_ms + event->duration_ms) -
                          song_time);
    int first = MIN(start_y, end_y);
    int last = MAX(start_y, end_y);
    int y;

    for (y = first; y <= last; y += 8)
        rb->lcd_bitmap_transparent_part(
            (const fb_data *)app->skin.sustains.bitmap.data,
            lane * 16, 0, app->skin.sustains.bitmap.width,
            ih_lane_x(lane, y) - 8, y - 8, 16, 16);
}

static void ih_draw_notes(const struct ih_app *app)
{
    const struct ih_game *game = &app->game;
    uint32_t begin = game->judge_cursor > 64 ? game->judge_cursor - 64 : 0;
    uint32_t i;

    for (i = begin; i < game->chart.event_count; ++i)
    {
        const struct ih_note_event *event = &game->chart.events[i];
        int delta = (int)event->time_ms - game->song_time_ms;
        int lane;
        if (delta > IH_APPROACH_MS)
            break;
        if (event->state != IH_EVENT_WAITING ||
            delta < -game->hit_window_ms)
            continue;
        if (event->duration_ms > 0)
            for (lane = 0; lane < IH_LANE_COUNT; ++lane)
                if (event->lane_mask & (1u << lane))
                    ih_draw_sustain(app, event, lane, game->song_time_ms);
    }

    for (i = begin; i < game->chart.event_count; ++i)
    {
        const struct ih_note_event *event = &game->chart.events[i];
        const struct ih_image *atlas = &app->skin.gems;
        int delta = (int)event->time_ms - game->song_time_ms;
        int y;
        int band;
        int lane;
        if (delta > IH_APPROACH_MS)
            break;
        if (event->state != IH_EVENT_WAITING ||
            delta < -game->hit_window_ms)
            continue;
        if (event->flags & IH_NOTE_STAR)
            atlas = &app->skin.star;
        else if (event->flags & (IH_NOTE_HOPO | IH_NOTE_TAP))
            atlas = &app->skin.hopo;
        y = ih_note_y(delta);
        band = (IH_APPROACH_MS - delta) * IH_GEM_BANDS /
               (IH_APPROACH_MS + 1);
        if (band < 0)
            band = 0;
        if (band >= IH_GEM_BANDS)
            band = IH_GEM_BANDS - 1;
        for (lane = 0; lane < IH_LANE_COUNT; ++lane)
            if (event->lane_mask & (1u << lane))
                ih_draw_atlas_lane(atlas, lane, band,
                                   ih_lane_x(lane, y), y);
    }
}

uint32_t ih_render_active_sprites(const struct ih_game *game)
{
    uint32_t count = IH_LANE_COUNT;
    uint32_t begin = game->judge_cursor > 64 ? game->judge_cursor - 64 : 0;
    uint32_t i;

    for (i = begin; i < game->chart.event_count; ++i)
    {
        const struct ih_note_event *event = &game->chart.events[i];
        int delta = (int)event->time_ms - game->song_time_ms;
        int lane;

        if (delta > IH_APPROACH_MS)
            break;
        if (event->state != IH_EVENT_WAITING ||
            delta < -game->hit_window_ms)
            continue;
        for (lane = 0; lane < IH_LANE_COUNT; ++lane)
            if (event->lane_mask & (1u << lane))
            {
                count++;
                if (event->duration_ms > 0)
                    count += event->duration_ms / 80 + 1;
            }
    }
    return count;
}

static const char *ih_judge_name(enum ih_judgement judgement)
{
    switch (judgement)
    {
        case IH_JUDGE_PERFECT: return "PERFECT";
        case IH_JUDGE_GREAT: return "GREAT";
        case IH_JUDGE_GOOD: return "GOOD";
        case IH_JUDGE_GRACE: return "HIT";
        case IH_JUDGE_MISS: return "MISS";
        default: return "";
    }
}

static void ih_draw_lane_labels(void)
{
    static const char *const labels[IH_LANE_COUNT] =
    {
        "LEFT", "MENU", "SELECT", "PLAY", "RIGHT"
    };
    int lane;

    rb->lcd_setfont(FONT_SYSFIXED);
    for (lane = 0; lane < IH_LANE_COUNT; ++lane)
    {
        int x = 61 + lane * 40;
        int width;

        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_fillrect(x, 221, 38, 17);
        rb->lcd_set_foreground(ih_fret_colors[lane]);
        rb->lcd_drawrect(x, 221, 38, 17);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_getstringsize(labels[lane], &width, NULL);
        rb->lcd_putsxy(x + (38 - width) / 2, 225, labels[lane]);
    }
    rb->lcd_setfont(FONT_UI);
}

void ih_render_game(const struct ih_app *app)
{
    const struct ih_game *game = &app->game;
    int lane;
    int progress;
    int star_height;
    char text[64];

    ih_draw_game_background(app);
    rb->lcd_bitmap_transparent((const fb_data *)app->skin.highway.bitmap.data,
                               IH_HIGHWAY_X, IH_HIGHWAY_Y, 200, 220);
    ih_draw_notes(app);
    for (lane = 0; lane < IH_LANE_COUNT; ++lane)
    {
        ih_draw_atlas_lane(&app->skin.rings, lane,
                           game->held_mask & (1u << lane) ? 1 : 0,
                           ih_lane_x(lane, IH_RECEPTOR_Y), IH_RECEPTOR_Y);
        if (*rb->current_tick < game->score.feedback_until &&
            game->score.last_judgement != IH_JUDGE_MISS &&
            (game->score.last_hit_mask & (1u << lane)))
            ih_draw_atlas_lane(&app->skin.flames, lane, 0,
                               ih_lane_x(lane, IH_RECEPTOR_Y),
                               IH_RECEPTOR_Y - 22);
    }

    rb->lcd_bitmap_transparent((const fb_data *)app->skin.hud.bitmap.data,
                               0, 0, 128, 28);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->snprintf(text, sizeof(text), "%08lu   %dx   %lu streak",
                 (unsigned long)game->score.points,
                 ih_score_multiplier(&game->score),
                 (unsigned long)game->score.streak);
    rb->lcd_putsxy(5, 3, text);
    rb->lcd_bitmap_transparent(
        (const fb_data *)app->skin.rock_meter.bitmap.data,
        252, 22, 64, 44);
    rb->lcd_set_foreground(LCD_DARKGRAY);
    rb->lcd_fillrect(265, 26, 49, 7);
    rb->lcd_set_foreground(game->score.rock < 25 ?
                           LCD_RGBPACK(235, 47, 53) :
                           LCD_RGBPACK(49, 210, 78));
    rb->lcd_fillrect(265, 26, game->score.rock * 49 / 100, 7);
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)app->skin.star_meter.bitmap.data,
        0, 0, app->skin.star_meter.bitmap.width, 4, 26, 40, 40);
    star_height = (int)(game->score.star_ms * 40 / 10000);
    if (star_height > 0)
        rb->lcd_bitmap_transparent_part(
            (const fb_data *)app->skin.star_meter.bitmap.data,
            40, 40 - star_height, app->skin.star_meter.bitmap.width,
            4, 26 + 40 - star_height, 40, star_height);
    if (game->score.star_active)
        ih_draw_text_center(68, "STAR POWER", LCD_RGBPACK(80, 210, 255));
    else if (game->chart.section_count > 0 &&
             game->section_cursor < game->chart.section_count &&
             (int)game->chart.sections[game->section_cursor].time_ms <=
                 game->song_time_ms)
        ih_draw_text_center(68,
            game->chart.section_strings +
            game->chart.sections[game->section_cursor].name_offset,
            LCD_WHITE);
    if (game->chart.song_length_ms > 0)
    {
        progress = game->song_time_ms * 318 /
                   (int)game->chart.song_length_ms;
        if (progress < 0)
            progress = 0;
        if (progress > 318)
            progress = 318;
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_fillrect(1, LCD_HEIGHT - 3, progress, 2);
    }
    ih_draw_lane_labels();
    if (*rb->current_tick < game->score.feedback_until)
        ih_draw_text_center(43, ih_judge_name(game->score.last_judgement),
                            game->score.last_judgement == IH_JUDGE_MISS ?
                            LCD_RGBPACK(245, 70, 70) : LCD_WHITE);
    rb->lcd_update();
}

void ih_render_pause(const struct ih_app *app, int selected)
{
    static const char *const items[] =
    {
        "Resume", "Restart", "Calibration", "No-Fail", "Quit Song"
    };
    ih_render_game(app);
    /* Scrim the frozen stage so the pause list reads over live gameplay. */
    ih_draw_scrim(0, IH_HEADER_H, LCD_WIDTH, LCD_HEIGHT - IH_HEADER_H - 21);
    ih_draw_panel(12, 42, LCD_WIDTH - 24, 150);
    ih_draw_menu_rows(items, ARRAYLEN(items), selected, 62);
    ih_draw_control_footer("Resume", "Choose");
    ih_draw_screen_header("Paused");
    rb->lcd_update();
}

void ih_render_results(const struct ih_app *app, int selected)
{
    static const char *const items[] = { "Retry", "Main Menu" };
    char text[80];
    uint32_t total = app->game.score.hit_count + app->game.score.miss_count;
    uint32_t accuracy = total ? app->game.score.hit_count * 1000 / total : 0;
    int stars;

    ih_draw_background(app);
    ih_draw_panel(12, 26, LCD_WIDTH - 24, 196);
    ih_draw_fret_rule(14, 60, LCD_WIDTH - 28, 2);
    ih_draw_text_center(40, app->game.failed ? "SONG FAILED" :
                        app->game.assist ? "SONG COMPLETE - ASSIST" :
                        app->game.no_fail ? "SONG COMPLETE - NO FAIL" :
                        "SONG COMPLETE",
                        app->game.failed ? LCD_RGBPACK(245, 80, 80) :
                        LCD_RGBPACK(250, 215, 90));
    rb->snprintf(text, sizeof(text), "Score  %08lu",
                 (unsigned long)app->game.score.points);
    ih_draw_text_center(70, text, LCD_WHITE);
    rb->snprintf(text, sizeof(text), "Accuracy  %lu.%lu%%",
                 (unsigned long)(accuracy / 10),
                 (unsigned long)(accuracy % 10));
    ih_draw_text_center(91, text, LCD_WHITE);
    rb->snprintf(text, sizeof(text), "Best streak  %lu",
                 (unsigned long)app->game.score.max_streak);
    ih_draw_text_center(112, text, LCD_WHITE);
    stars = accuracy >= 900 ? 5 : accuracy >= 800 ? 4 :
            accuracy >= 700 ? 3 : accuracy >= 600 ? 2 : 1;
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)app->skin.results_stars.bitmap.data,
        0, 0, app->skin.results_stars.bitmap.width,
        85, 125, 150, 30);
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)app->skin.results_stars.bitmap.data,
        0, 30, app->skin.results_stars.bitmap.width,
        85, 125, stars * 30, 30);
    ih_draw_menu_rows(items, ARRAYLEN(items), selected, 169);
    ih_draw_control_footer("Menu", "Choose");
    ih_draw_screen_header("Results");
    rb->lcd_update();
}

void ih_render_calibration(const struct ih_app *app, int proposed,
                           bool allow_tap)
{
    char text[64];
    int marker = 160 + proposed / 4;

    ih_draw_background(app);
    ih_draw_panel(16, 34, LCD_WIDTH - 32, 172);
    ih_draw_text_center(50, "Audio Calibration", LCD_RGBPACK(250, 215, 90));
    rb->snprintf(text, sizeof(text), "%+d ms", proposed);
    ih_draw_text_center(82, text, LCD_WHITE);
    rb->lcd_set_foreground(LCD_RGBPACK(52, 52, 62));
    rb->lcd_fillrect(60, 119, 200, 8);
    ih_draw_fret_rule(60, 121, 200, 4);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(marker - 3, 111, 7, 24);
    rb->lcd_set_foreground(LCD_RGBPACK(252, 252, 255));
    rb->lcd_fillrect(marker - 2, 111, 5, 24);
    ih_draw_text_center(145, "Wheel adjusts in 5 ms steps", LCD_WHITE);
    ih_draw_text_center(164, allow_tap ? "Play starts an 8-tap test" :
                        "Tap test is available before play", LCD_WHITE);
    ih_draw_text_center(183, "Select saves  |  Menu cancels", LCD_WHITE);
    ih_draw_control_footer("Cancel", "Save");
    ih_draw_screen_header("Calibration");
    rb->lcd_update();
}

void ih_render_calibration_tap(const struct ih_app *app, int tap_count)
{
    char text[40];

    ih_render_game(app);
    ih_draw_panel(58, 38, 204, 48);
    rb->snprintf(text, sizeof(text), "TAP SELECT  %d / 8", tap_count);
    ih_draw_text_center(49, text, LCD_RGBPACK(250, 215, 90));
    ih_draw_text_center(66, "Menu cancels", LCD_LIGHTGRAY);
    rb->lcd_update();
}

void ih_render_settings(const struct ih_app *app, int selected)
{
    const char *items[4];
    char no_fail[32];
    char hit_window[40];

    rb->snprintf(no_fail, sizeof(no_fail), "No-Fail: %s",
                 app->game.no_fail ? "On (unranked)" : "Off");
    rb->snprintf(hit_window, sizeof(hit_window), "Hit Window: %d ms%s",
                 app->game.hit_window_ms,
                 app->game.hit_window_ms == IH_HIT_WINDOW_MS ? "" :
                 " (Assist)");
    items[0] = no_fail;
    items[1] = hit_window;
    items[2] = "Input Test";
    items[3] = "Done";
    ih_draw_background(app);
    ih_draw_scrim(0, IH_HEADER_H, LCD_WIDTH, 86);
    rb->lcd_bitmap_transparent((const fb_data *)app->skin.logo.bitmap.data,
                               40, 24, 240, 80);
    ih_draw_fret_rule(0, 104, LCD_WIDTH, 2);
    ih_draw_menu_rows(items, ARRAYLEN(items), selected, 106);
    ih_draw_control_footer("Back", "Choose");
    ih_draw_screen_header("Settings");
    rb->lcd_update();
}

void ih_render_input_test(uint32_t count, const char *last_event,
                          bool hold, bool full)
{
    char text[64];

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    ih_draw_vgradient(0, IH_HEADER_H, LCD_WIDTH, LCD_HEIGHT - IH_HEADER_H,
                      18, 18, 24, 6, 6, 9);
    ih_draw_screen_header("Input Test");
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_RGBPACK(214, 214, 222));
    rb->lcd_putsxy(12, 35, "Press every button and combination");
    rb->lcd_putsxy(12, 57, "Roll all five lanes both directions");
    rb->lcd_putsxy(12, 79, "Turn wheel while holding a lane");
    rb->lcd_putsxy(12, 101, "Move Hold on and off");
    rb->lcd_set_drawmode(DRMODE_SOLID);
    ih_draw_fret_rule(12, 128, LCD_WIDTH - 24, 2);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_RGBPACK(250, 215, 90));
    rb->snprintf(text, sizeof(text), "Events: %lu / 512%s",
                 (unsigned long)count, full ? "  FULL" : "");
    rb->lcd_putsxy(12, 140, text);
    rb->snprintf(text, sizeof(text), "Hold: %s", hold ? "ON" : "off");
    rb->lcd_putsxy(210, 140, text);
    rb->lcd_set_foreground(LCD_RGBPACK(214, 214, 222));
    rb->lcd_putsxy(12, 164, "Last:");
    rb->lcd_putsxy(52, 164, last_event);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    ih_draw_fret_rule(0, 202, LCD_WIDTH, 2);
    ih_draw_vgradient(0, 204, LCD_WIDTH, 36, 34, 34, 42, 12, 12, 16);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_RGBPACK(248, 248, 252));
    rb->lcd_putsxy(12, 209, "Select + Menu exits");
    rb->lcd_putsxy(12, 225, "Hold for 2 seconds also exits");
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_update();
}

void ih_render_error(const char *title, const char *detail)
{
    char line[MAX_PATH];
    const char *cursor = detail;
    int y = 62;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    ih_draw_vgradient(0, IH_HEADER_H, LCD_WIDTH, LCD_HEIGHT - IH_HEADER_H,
                      18, 18, 24, 6, 6, 9);
    ih_draw_screen_header(title);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_RGBPACK(245, 90, 90));
    rb->lcd_putsxy(14, 42, "iPod Hero could not continue:");
    rb->lcd_set_foreground(LCD_RGBPACK(214, 214, 222));
    while (cursor[0] != '\0' && y <= 182)
    {
        const char *newline = rb->strchr(cursor, '\n');
        size_t length = newline == NULL ? rb->strlen(cursor) :
                        (size_t)(newline - cursor);
        if (length >= sizeof(line))
            length = sizeof(line) - 1;
        rb->memcpy(line, cursor, length);
        line[length] = '\0';
        rb->lcd_putsxy(14, y, line);
        y += 20;
        if (newline == NULL)
            break;
        cursor = newline + 1;
    }
    rb->lcd_set_foreground(LCD_RGBPACK(250, 215, 90));
    rb->lcd_putsxy(14, 205, "Press Menu to return");
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_update();
}
