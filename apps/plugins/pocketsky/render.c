#include "pocketsky.h"

struct ps_label_box
{
    short x;
    short y;
    short width;
    short height;
};

struct ps_palette
{
    unsigned background;
    unsigned below_horizon;
    unsigned grid;
    unsigned horizon;
    unsigned constellation;
    unsigned star;
    unsigned body;
    unsigned selected;
    unsigned ui_background;
    unsigned text;
    unsigned muted;
};

static const struct ps_palette ps_normal_palette =
{
    LCD_RGBPACK(2, 5, 14),
    LCD_RGBPACK(8, 10, 12),
    LCD_RGBPACK(28, 45, 66),
    LCD_RGBPACK(80, 112, 136),
    LCD_RGBPACK(46, 73, 101),
    LCD_RGBPACK(236, 240, 242),
    LCD_RGBPACK(246, 203, 92),
    LCD_RGBPACK(112, 207, 255),
    LCD_RGBPACK(7, 13, 24),
    LCD_RGBPACK(238, 242, 245),
    LCD_RGBPACK(139, 154, 168)
};

static const struct ps_palette ps_night_palette =
{
    LCD_RGBPACK(0, 0, 0),
    LCD_RGBPACK(5, 0, 0),
    LCD_RGBPACK(45, 0, 0),
    LCD_RGBPACK(100, 0, 0),
    LCD_RGBPACK(62, 0, 0),
    LCD_RGBPACK(180, 0, 0),
    LCD_RGBPACK(225, 0, 0),
    LCD_RGBPACK(255, 20, 20),
    LCD_RGBPACK(8, 0, 0),
    LCD_RGBPACK(225, 0, 0),
    LCD_RGBPACK(125, 0, 0)
};

static const struct ps_palette *ps_palette(const struct ps_app *app)
{
    return app->settings.night_mode ? &ps_night_palette : &ps_normal_palette;
}

static bool ps_inside_view(short x, short y)
{
    return x >= 0 && x < LCD_WIDTH && y >= PS_VIEW_TOP &&
           y < PS_VIEW_TOP + PS_VIEW_HEIGHT;
}

static void ps_draw_point(short x, short y, int radius, unsigned color)
{
    rb->lcd_set_foreground(color);
    if (radius <= 1)
        rb->lcd_fillrect(x, y, 2, 2);
    else
        rb->lcd_fillrect(x - radius, y - radius,
                         radius * 2 + 1, radius * 2 + 1);
}

static bool ps_boxes_overlap(const struct ps_label_box *a,
                             const struct ps_label_box *b)
{
    return a->x < b->x + b->width && a->x + a->width > b->x &&
           a->y < b->y + b->height && a->y + a->height > b->y;
}

static bool ps_draw_label(const char *text, int x, int y,
                          struct ps_label_box *boxes, int *box_count,
                          unsigned color)
{
    struct ps_label_box candidate;
    int width;
    int height;
    int i;

    if (!text || !*text || *box_count >= PS_MAX_LABELS)
        return false;
    rb->lcd_getstringsize(text, &width, &height);
    candidate.x = x + 3;
    candidate.y = y - height - 1;
    candidate.width = width + 2;
    candidate.height = height + 1;
    if (candidate.x < 0 || candidate.y < PS_VIEW_TOP ||
        candidate.x + candidate.width >= LCD_WIDTH ||
        candidate.y + candidate.height >= PS_VIEW_TOP + PS_VIEW_HEIGHT)
        return false;
    for (i = 0; i < *box_count; ++i)
    {
        if (ps_boxes_overlap(&candidate, &boxes[i]))
            return false;
    }
    boxes[*box_count] = candidate;
    ++*box_count;
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy(candidate.x + 1, candidate.y, text);
    return true;
}

static void ps_draw_altitude_grid(struct ps_app *app,
                                  const struct ps_palette *palette)
{
    int altitude;
    int azimuth;
    float sin_azimuth_step = sin(5.0 * M_PI / 180.0);
    float cos_azimuth_step = cos(5.0 * M_PI / 180.0);
    float sin_altitude_step = sin(5.0 * M_PI / 180.0);
    float cos_altitude_step = cos(5.0 * M_PI / 180.0);

    rb->lcd_set_foreground(palette->grid);
    for (altitude = 0; altitude <= 60; altitude += 30)
    {
        bool have_previous = false;
        short previous_x = 0;
        short previous_y = 0;
        float altitude_rad = altitude * M_PI / 180.0;
        float sin_altitude = sin(altitude_rad);
        float cos_altitude = cos(altitude_rad);
        float sin_azimuth = 0.0f;
        float cos_azimuth = 1.0f;
        for (azimuth = 0; azimuth <= 360; azimuth += 5)
        {
            short x;
            short y;
            float next_sin;
            bool visible = ps_project_vector(
                app, cos_altitude * sin_azimuth,
                cos_altitude * cos_azimuth, sin_altitude, &x, &y);
            if (visible && have_previous && ps_inside_view(previous_x, previous_y))
                rb->lcd_drawline(previous_x, previous_y, x, y);
            have_previous = visible;
            if (visible)
            {
                previous_x = x;
                previous_y = y;
            }
            next_sin = sin_azimuth * cos_azimuth_step +
                       cos_azimuth * sin_azimuth_step;
            cos_azimuth = cos_azimuth * cos_azimuth_step -
                          sin_azimuth * sin_azimuth_step;
            sin_azimuth = next_sin;
        }
    }
    for (azimuth = 0; azimuth < 360; azimuth += 45)
    {
        bool have_previous = false;
        short previous_x = 0;
        short previous_y = 0;
        float azimuth_rad = azimuth * M_PI / 180.0;
        float sin_azimuth = sin(azimuth_rad);
        float cos_azimuth = cos(azimuth_rad);
        float sin_altitude = sin(-10.0 * M_PI / 180.0);
        float cos_altitude = cos(-10.0 * M_PI / 180.0);
        for (altitude = -10; altitude <= 90; altitude += 5)
        {
            short x;
            short y;
            float next_sin;
            bool visible = ps_project_vector(
                app, cos_altitude * sin_azimuth,
                cos_altitude * cos_azimuth, sin_altitude, &x, &y);
            if (visible && have_previous && ps_inside_view(previous_x, previous_y))
                rb->lcd_drawline(previous_x, previous_y, x, y);
            have_previous = visible;
            if (visible)
            {
                previous_x = x;
                previous_y = y;
            }
            next_sin = sin_altitude * cos_altitude_step +
                       cos_altitude * sin_altitude_step;
            cos_altitude = cos_altitude * cos_altitude_step -
                           sin_altitude * sin_altitude_step;
            sin_altitude = next_sin;
        }
    }
}

static void ps_draw_horizon(struct ps_app *app,
                            const struct ps_palette *palette)
{
    int azimuth;
    bool have_previous = false;
    short previous_x = 0;
    short previous_y = 0;
    float sin_step = sin(3.0 * M_PI / 180.0);
    float cos_step = cos(3.0 * M_PI / 180.0);
    float sin_azimuth = 0.0f;
    float cos_azimuth = 1.0f;

    rb->lcd_set_foreground(palette->horizon);
    for (azimuth = 0; azimuth <= 360; azimuth += 3)
    {
        short x;
        short y;
        float next_sin;
        bool visible = ps_project_vector(app, sin_azimuth, cos_azimuth,
                                         0.0f, &x, &y);
        if (visible && have_previous && ps_inside_view(previous_x, previous_y))
            rb->lcd_drawline(previous_x, previous_y, x, y);
        have_previous = visible;
        if (visible)
        {
            previous_x = x;
            previous_y = y;
        }
        next_sin = sin_azimuth * cos_step + cos_azimuth * sin_step;
        cos_azimuth = cos_azimuth * cos_step - sin_azimuth * sin_step;
        sin_azimuth = next_sin;
    }
}

static void ps_draw_constellations(struct ps_app *app,
                                   struct ps_label_box *boxes,
                                   int *box_count,
                                   const struct ps_palette *palette)
{
    const unsigned char *cursor = app->data.constellations;
    const unsigned char *end = cursor + app->data.constellation_bytes;
    unsigned int constellation;

    rb->lcd_set_foreground(palette->constellation);
    for (constellation = 0;
         constellation < app->data.constellation_count && cursor + 8 <= end;
         ++constellation)
    {
        char name[32];
        unsigned int encoded_name_length = cursor[4];
        unsigned int name_length = encoded_name_length;
        unsigned int segments = ps_get_u16(cursor + 6);
        const unsigned char *pairs;
        unsigned int segment;
        int sum_x = 0;
        int sum_y = 0;
        int visible_points = 0;

        cursor += 8;
        if (cursor + name_length + segments * 4 > end)
            return;
        if (name_length >= sizeof(name))
            name_length = sizeof(name) - 1;
        rb->memcpy(name, cursor, name_length);
        name[name_length] = '\0';
        cursor += encoded_name_length;
        pairs = cursor;

        for (segment = 0; segment < segments; ++segment)
        {
            int first = ps_star_index_by_hr(app, ps_get_u16(pairs + segment * 4));
            int second = ps_star_index_by_hr(app, ps_get_u16(pairs + segment * 4 + 2));
            if (first < 0 || second < 0 ||
                (unsigned int)first >= app->scene.active_stars ||
                (unsigned int)second >= app->scene.active_stars)
                continue;
            if (app->scene.stars[first].visible && app->scene.stars[second].visible)
            {
                rb->lcd_drawline(app->scene.stars[first].x,
                                 app->scene.stars[first].y,
                                 app->scene.stars[second].x,
                                 app->scene.stars[second].y);
                sum_x += app->scene.stars[first].x + app->scene.stars[second].x;
                sum_y += app->scene.stars[first].y + app->scene.stars[second].y;
                visible_points += 2;
            }
        }
        if (app->settings.show_constellation_names && visible_points > 0)
            ps_draw_label(name, sum_x / visible_points, sum_y / visible_points,
                          boxes, box_count, palette->muted);
        cursor = pairs + segments * 4;
    }
}

static void ps_draw_moon(const struct ps_body *moon,
                         const struct ps_palette *palette,
                         double phase)
{
    int radius = 5;
    int dx;
    int dy;
    bool waxing = phase < 180.0;
    double fraction = moon->phase_fraction;

    rb->lcd_set_foreground(palette->grid);
    rb->lcd_fillrect(moon->x - radius, moon->y - radius,
                     radius * 2 + 1, radius * 2 + 1);
    rb->lcd_set_foreground(palette->body);
    for (dx = -radius; dx <= radius; ++dx)
    {
        int height = (int)sqrt(radius * radius - dx * dx);
        double normalized = (dx + radius) / (2.0 * radius);
        bool lit = waxing ? normalized >= 1.0 - fraction :
                            normalized <= fraction;
        if (lit)
        {
            for (dy = -height; dy <= height; ++dy)
                rb->lcd_drawpixel(moon->x + dx, moon->y + dy);
        }
    }
    rb->lcd_set_foreground(palette->horizon);
    rb->lcd_drawrect(moon->x - radius, moon->y - radius,
                     radius * 2 + 1, radius * 2 + 1);
}

static void ps_draw_scene_objects(struct ps_app *app,
                                  struct ps_label_box *boxes,
                                  int *box_count,
                                  const struct ps_palette *palette)
{
    int i;

    for (i = (int)app->scene.active_stars - 1; i >= 0; --i)
    {
        const unsigned char *record;
        float magnitude;
        int radius;
        const char *name;

        if (!app->scene.stars[i].visible)
            continue;
        record = ps_star_record(app, i);
        magnitude = ps_get_float(record + 18);
        radius = magnitude < 0.0f ? 3 : magnitude < 2.0f ? 2 : 1;
        ps_draw_point(app->scene.stars[i].x, app->scene.stars[i].y,
                      radius, palette->star);
        name = ps_star_name(app, i);
        if (app->settings.show_labels && name &&
            magnitude <= app->settings.label_tenths / 10.0f)
            ps_draw_label(name, app->scene.stars[i].x,
                          app->scene.stars[i].y, boxes, box_count,
                          palette->text);
    }

    for (i = 0; i < PS_BODY_COUNT - 1; ++i)
    {
        struct ps_body *body = &app->scene.bodies[i];
        if (!body->visible || (body->code == BODY_PLUTO && body->magnitude >
                               app->settings.magnitude_tenths / 10.0f))
            continue;
        if (body->code == BODY_MOON)
            ps_draw_moon(body, palette, app->scene.moon_phase);
        else
            ps_draw_point(body->x, body->y,
                          body->code == BODY_SUN ? 5 : 3, palette->body);
        if (app->settings.show_labels)
            ps_draw_label(body->name, body->x, body->y,
                          boxes, box_count, palette->body);
    }
}

static void ps_draw_ui(struct ps_app *app, const struct ps_palette *palette)
{
    astro_utc_t utc = Astronomy_UtcFromTime(app->scene.time);
    char top[96];
    char bottom[96];
    int center_x = LCD_WIDTH / 2;
    int center_y = PS_VIEW_TOP + PS_VIEW_HEIGHT / 2;

    rb->lcd_set_foreground(palette->ui_background);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, PS_HEADER_HEIGHT);
    rb->lcd_fillrect(0, LCD_HEIGHT - PS_FOOTER_HEIGHT,
                     LCD_WIDTH, PS_FOOTER_HEIGHT);
    rb->snprintf(top, sizeof(top), "%s  %s  %04d-%02d-%02d %02d:%02dZ",
                 app->settings.location_name,
                 app->live ? "LIVE" : "SIM",
                 utc.year, utc.month, utc.day, utc.hour, utc.minute);
    rb->snprintf(bottom, sizeof(bottom), "Az %03d  Alt %+03d   SELECT identify",
                 (int)app->center_azimuth, (int)app->center_altitude);
    rb->lcd_set_foreground(palette->text);
    rb->lcd_putsxy(3, 3, top);
    rb->lcd_putsxy(3, LCD_HEIGHT - PS_FOOTER_HEIGHT + 3, bottom);

    rb->lcd_set_foreground(palette->selected);
    rb->lcd_hline(center_x - 5, center_x + 5, center_y);
    rb->lcd_vline(center_x, center_y - 5, center_y + 5);
}

void ps_render_sky(struct ps_app *app)
{
    struct ps_label_box boxes[PS_MAX_LABELS];
    int box_count = 0;
    const struct ps_palette *palette = ps_palette(app);

    rb->lcd_set_background(palette->background);
    rb->lcd_set_foreground(palette->background);
    rb->lcd_clear_display();
    if (app->settings.show_grid)
        ps_draw_altitude_grid(app, palette);
    if (app->settings.show_horizon)
        ps_draw_horizon(app, palette);
    if (app->settings.show_constellations)
        ps_draw_constellations(app, boxes, &box_count, palette);
    ps_draw_scene_objects(app, boxes, &box_count, palette);
    ps_draw_ui(app, palette);
    rb->lcd_update();
}

static void ps_wait_for_back(struct ps_app *app)
{
    while (true)
    {
        int button = rb->button_get(true);
        int clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_MENU || clean == BUTTON_SELECT ||
            clean == BUTTON_LEFT)
            return;
        if (button == SYS_USB_CONNECTED)
        {
            app->usb = true;
            app->running = false;
            return;
        }
    }
}

void ps_show_object_card(struct ps_app *app)
{
    char line[96];
    int y = 10;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(app->settings.night_mode ?
                           LCD_RGBPACK(220, 0, 0) : LCD_WHITE);
    rb->lcd_clear_display();
    if (app->selected_kind == PS_SELECTED_STAR && app->selected_index >= 0)
    {
        const unsigned char *record = ps_star_record(app, app->selected_index);
        const char *name = ps_star_name(app, app->selected_index);
        astro_constellation_t constellation = Astronomy_Constellation(
            ps_get_float(record + 2) * 12.0 / M_PI,
            ps_get_float(record + 6) * 180.0 / M_PI);
        rb->snprintf(line, sizeof(line), "%s", name ? name : "Catalog star");
        rb->lcd_putsxy(6, y, line);
        y += 24;
        rb->snprintf(line, sizeof(line), "HR %u   Mag %.2f",
                     ps_get_u16(record), ps_get_float(record + 18));
        rb->lcd_putsxy(6, y, line);
        y += 18;
        rb->snprintf(line, sizeof(line), "Altitude %+06.2f deg",
                     app->scene.stars[app->selected_index].altitude);
        rb->lcd_putsxy(6, y, line);
        y += 18;
        rb->snprintf(line, sizeof(line), "Azimuth %06.2f deg",
                     app->scene.stars[app->selected_index].azimuth);
        rb->lcd_putsxy(6, y, line);
        y += 18;
        if (constellation.status == ASTRO_SUCCESS)
        {
            rb->snprintf(line, sizeof(line), "Constellation %s",
                         constellation.name);
            rb->lcd_putsxy(6, y, line);
        }
    }
    else if (app->selected_kind == PS_SELECTED_BODY &&
             app->selected_index >= 0)
    {
        struct ps_body *body = &app->scene.bodies[app->selected_index];
        rb->lcd_putsxy(6, y, body->name);
        y += 24;
        rb->snprintf(line, sizeof(line), "Altitude %+06.2f deg", body->altitude);
        rb->lcd_putsxy(6, y, line);
        y += 18;
        rb->snprintf(line, sizeof(line), "Azimuth %06.2f deg", body->azimuth);
        rb->lcd_putsxy(6, y, line);
        y += 18;
        rb->snprintf(line, sizeof(line), "Magnitude %.2f", body->magnitude);
        rb->lcd_putsxy(6, y, line);
        y += 18;
        if (body->code == BODY_MOON)
        {
            rb->snprintf(line, sizeof(line), "Illuminated %d%%",
                         (int)(body->phase_fraction * 100.0f + 0.5f));
            rb->lcd_putsxy(6, y, line);
        }
    }
    else
        rb->lcd_putsxy(6, y, "No object near the reticle");
    rb->lcd_putsxy(6, LCD_HEIGHT - 22, "MENU/SELECT: back");
    rb->lcd_update();
    ps_wait_for_back(app);
}
