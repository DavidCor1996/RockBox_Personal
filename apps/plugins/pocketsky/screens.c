#include "pocketsky.h"

struct ps_search_result
{
    enum ps_selected_kind kind;
    int index;
    const char *name;
};

static int ps_list_select(struct ps_app *app, const char *title,
                          const char * const *items, int count)
{
    int selected = 0;
    int top = 0;
    int line_width;
    int line_height;
    int visible;

    rb->lcd_getstringsize("Ag", &line_width, &line_height);
    if (line_height < 12)
        line_height = 12;
    visible = (LCD_HEIGHT - 36) / line_height;
    while (app->running)
    {
        int i;
        int button;
        int clean;

        if (selected < top)
            top = selected;
        if (selected >= top + visible)
            top = selected - visible + 1;
        rb->lcd_set_background(app->settings.night_mode ? LCD_BLACK :
                               LCD_RGBPACK(9, 15, 25));
        rb->lcd_set_foreground(app->settings.night_mode ?
                               LCD_RGBPACK(220, 0, 0) : LCD_WHITE);
        rb->lcd_clear_display();
        rb->lcd_putsxy(4, 4, title);
        for (i = 0; i < visible && top + i < count; ++i)
        {
            int y = 25 + i * line_height;
            if (top + i == selected)
            {
                rb->lcd_set_foreground(app->settings.night_mode ?
                                       LCD_RGBPACK(85, 0, 0) :
                                       LCD_RGBPACK(35, 75, 108));
                rb->lcd_fillrect(2, y - 1, LCD_WIDTH - 4, line_height);
                rb->lcd_set_foreground(app->settings.night_mode ?
                                       LCD_RGBPACK(255, 15, 15) : LCD_WHITE);
            }
            rb->lcd_putsxy(7, y, items[top + i]);
        }
        rb->lcd_putsxy(4, LCD_HEIGHT - 14, "SELECT open   MENU back");
        rb->lcd_update();

        button = rb->button_get(true);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if ((clean == BUTTON_SCROLL_FWD || clean == BUTTON_RIGHT) && count > 0)
            selected = (selected + 1) % count;
        else if ((clean == BUTTON_SCROLL_BACK || clean == BUTTON_LEFT) && count > 0)
            selected = (selected + count - 1) % count;
        else if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
            return selected;
        else if (clean == BUTTON_MENU)
            return -1;
        else if (button == SYS_USB_CONNECTED)
        {
            app->usb = true;
            app->running = false;
            return -1;
        }
    }
    return -1;
}

static const unsigned char *ps_constellation_at(const struct ps_app *app,
                                                 unsigned int wanted,
                                                 const char **name,
                                                 unsigned int *name_length,
                                                 unsigned int *segments)
{
    const unsigned char *cursor = app->data.constellations;
    const unsigned char *end = cursor + app->data.constellation_bytes;
    unsigned int index;

    for (index = 0; index < app->data.constellation_count; ++index)
    {
        unsigned int encoded_name_length;
        unsigned int segment_count;
        if (cursor + 8 > end)
            return NULL;
        encoded_name_length = cursor[4];
        segment_count = ps_get_u16(cursor + 6);
        if (cursor + 8 + encoded_name_length + segment_count * 4 > end)
            return NULL;
        if (index == wanted)
        {
            *name = (const char *)(cursor + 8);
            *name_length = encoded_name_length;
            *segments = segment_count;
            return cursor + 8 + encoded_name_length;
        }
        cursor += 8 + encoded_name_length + segment_count * 4;
    }
    return NULL;
}

static void ps_center_constellation(struct ps_app *app, int index)
{
    const char *name;
    const unsigned char *pairs;
    unsigned int name_length;
    unsigned int segments;
    unsigned int i;
    double east = 0.0;
    double north = 0.0;
    double altitude = 0.0;
    int count = 0;

    pairs = ps_constellation_at(app, index, &name, &name_length, &segments);
    if (!pairs)
        return;
    (void)name;
    (void)name_length;
    for (i = 0; i < segments * 2; ++i)
    {
        int star = ps_star_index_by_hr(app, ps_get_u16(pairs + i * 2));
        double azimuth;
        if (star < 0 || (unsigned int)star >= app->scene.active_stars)
            continue;
        azimuth = app->scene.stars[star].azimuth * M_PI / 180.0;
        east += sin(azimuth);
        north += cos(azimuth);
        altitude += app->scene.stars[star].altitude;
        ++count;
    }
    if (count > 0)
    {
        app->center_azimuth = atan2(east, north) * 180.0 / M_PI;
        if (app->center_azimuth < 0)
            app->center_azimuth += 360.0f;
        app->center_altitude = altitude / count;
        app->selected_kind = PS_SELECTED_CONSTELLATION;
        app->selected_index = index;
        ps_scene_mark_view_dirty(app);
    }
}

void ps_show_search(struct ps_app *app)
{
    char query[40] = "";
    struct ps_search_result results[PS_MAX_SEARCH_RESULTS];
    const char *items[PS_MAX_SEARCH_RESULTS];
    char constellation_names[88][32];
    int count = 0;
    int choice;
    unsigned int i;

    if (rb->kbd_input(query, sizeof(query), NULL) < 0 || !query[0])
        return;
    for (i = 0; i < PS_BODY_COUNT - 1 && count < PS_MAX_SEARCH_RESULTS; ++i)
    {
        if (ps_ascii_contains(app->scene.bodies[i].name, query))
        {
            results[count].kind = PS_SELECTED_BODY;
            results[count].index = i;
            results[count].name = app->scene.bodies[i].name;
            items[count] = results[count].name;
            ++count;
        }
    }
    for (i = 0; i < app->data.star_count && count < PS_MAX_SEARCH_RESULTS; ++i)
    {
        const char *name = ps_star_name(app, i);
        if (name && ps_ascii_contains(name, query))
        {
            results[count].kind = PS_SELECTED_STAR;
            results[count].index = i;
            results[count].name = name;
            items[count] = name;
            ++count;
        }
    }
    for (i = 0; i < app->data.constellation_count &&
                count < PS_MAX_SEARCH_RESULTS; ++i)
    {
        const char *name;
        unsigned int name_length;
        unsigned int segments;
        if (!ps_constellation_at(app, i, &name, &name_length, &segments))
            break;
        (void)segments;
        if (name_length >= sizeof(constellation_names[i]))
            name_length = sizeof(constellation_names[i]) - 1;
        rb->memcpy(constellation_names[i], name, name_length);
        constellation_names[i][name_length] = '\0';
        if (ps_ascii_contains(constellation_names[i], query))
        {
            results[count].kind = PS_SELECTED_CONSTELLATION;
            results[count].index = i;
            results[count].name = constellation_names[i];
            items[count] = results[count].name;
            ++count;
        }
    }
    if (count == 0)
    {
        rb->splash(HZ * 2, "No matching sky objects");
        return;
    }
    choice = ps_list_select(app, "Search results", items, count);
    if (choice < 0)
        return;
    if (results[choice].kind == PS_SELECTED_STAR)
    {
        int index = results[choice].index;
        if ((unsigned int)index >= app->scene.active_stars)
        {
            int needed = (int)(ps_get_float(ps_star_record(app, index) + 18) * 10.0f + 1.0f);
            if (needed <= 80)
            {
                app->settings.magnitude_tenths = needed;
                ps_scene_mark_time_dirty(app);
                ps_scene_update(app);
            }
        }
        ps_center_on_star(app, index);
    }
    else if (results[choice].kind == PS_SELECTED_BODY)
        ps_center_on_body(app, results[choice].index);
    else
        ps_center_constellation(app, results[choice].index);
}

static void ps_format_local_event(struct ps_app *app,
                                  astro_search_result_t event,
                                  char *buffer, size_t size)
{
    astro_utc_t local;
    if (event.status != ASTRO_SUCCESS)
    {
        rb->strlcpy(buffer, "--", size);
        return;
    }
    local = Astronomy_UtcFromTime(Astronomy_AddDays(
        event.time, app->settings.utc_offset_min / (24.0 * 60.0)));
    rb->snprintf(buffer, size, "%02d:%02d", local.hour, local.minute);
}

static void ps_add_tonight_line(char lines[][72], int *count,
                                const char *label, const char *value)
{
    if (*count >= PS_MAX_TONIGHT_LINES)
        return;
    rb->snprintf(lines[*count], 72, "%-22s %s", label, value);
    ++*count;
}

void ps_show_tonight(struct ps_app *app)
{
    char lines[PS_MAX_TONIGHT_LINES][72];
    const char *items[PS_MAX_TONIGHT_LINES];
    char value[32];
    astro_observer_t observer;
    astro_time_t start = ps_current_time(app);
    astro_search_result_t event;
    int count = 0;
    int i;
    static const char * const phase_names[8] =
    {
        "New Moon", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
        "Full Moon", "Waning Gibbous", "Last Quarter", "Waning Crescent"
    };

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true);
#endif
    observer = Astronomy_MakeObserver(
        app->settings.latitude_microdeg / 1000000.0,
        app->settings.longitude_microdeg / 1000000.0,
        app->settings.elevation_m);
    event = Astronomy_SearchRiseSet(BODY_SUN, observer, DIRECTION_SET,
                                    start, 1.5);
    ps_format_local_event(app, event, value, sizeof(value));
    ps_add_tonight_line(lines, &count, "Sunset", value);

    event = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_SET,
                                     start, 1.5, -6.0);
    ps_format_local_event(app, event, value, sizeof(value));
    ps_add_tonight_line(lines, &count, "Civil twilight ends", value);
    event = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_SET,
                                     start, 1.5, -12.0);
    ps_format_local_event(app, event, value, sizeof(value));
    ps_add_tonight_line(lines, &count, "Nautical twilight", value);
    event = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_SET,
                                     start, 1.5, -18.0);
    ps_format_local_event(app, event, value, sizeof(value));
    ps_add_tonight_line(lines, &count, "Astronomical dark", value);

    i = ((int)floor((app->scene.moon_phase + 22.5) / 45.0)) & 7;
    rb->snprintf(value, sizeof(value), "%s, %d%%", phase_names[i],
                 (int)(app->scene.bodies[1].phase_fraction * 100.0f + 0.5f));
    ps_add_tonight_line(lines, &count, "Moon", value);

    for (i = 1; i < PS_BODY_COUNT - 1; ++i)
    {
        char rise[8];
        char set[8];
        char label[32];
        event = Astronomy_SearchRiseSet(app->scene.bodies[i].code, observer,
                                        DIRECTION_RISE, start, 1.5);
        ps_format_local_event(app, event, rise, sizeof(rise));
        event = Astronomy_SearchRiseSet(app->scene.bodies[i].code, observer,
                                        DIRECTION_SET, start, 1.5);
        ps_format_local_event(app, event, set, sizeof(set));
        rb->snprintf(label, sizeof(label), "%s rise/set",
                     app->scene.bodies[i].name);
        rb->snprintf(value, sizeof(value), "%s / %s", rise, set);
        ps_add_tonight_line(lines, &count, label, value);
    }
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
    for (i = 0; i < count; ++i)
        items[i] = lines[i];
    ps_list_select(app, "Tonight (local time)", items, count);
}

static void ps_search_city(struct ps_app *app)
{
    char query[40] = "";
    int matches[PS_MAX_SEARCH_RESULTS];
    const char *items[PS_MAX_SEARCH_RESULTS];
    int count = 0;
    int choice;
    unsigned int i;

    if (rb->kbd_input(query, sizeof(query), NULL) < 0 || !query[0])
        return;
    for (i = 0; i < app->data.city_count && count < PS_MAX_SEARCH_RESULTS; ++i)
    {
        const char *name = ps_city_name(app, i);
        if (name && ps_ascii_contains(name, query))
        {
            matches[count] = i;
            items[count] = name;
            ++count;
        }
    }
    if (count == 0)
    {
        rb->splash(HZ * 2, "No matching cities");
        return;
    }
    choice = ps_list_select(app, "Choose city", items, count);
    if (choice >= 0)
    {
        float latitude;
        float longitude;
        int index = matches[choice];
        ps_city_position(app, index, &latitude, &longitude);
        app->settings.latitude_microdeg = latitude * 1000000.0f;
        app->settings.longitude_microdeg = longitude * 1000000.0f;
        rb->strlcpy(app->settings.location_name, ps_city_name(app, index),
                    sizeof(app->settings.location_name));
        app->settings.location_set = true;
        rb->splashf(HZ * 2, "%s\nTimezone: %s\nSet UTC offset next",
                    ps_city_name(app, index), ps_city_timezone(app, index));
        rb->set_int("UTC offset (minutes)", "min", UNIT_INT,
                    &app->settings.utc_offset_min, NULL, 15, -720, 840, NULL);
        ps_scene_mark_time_dirty(app);
        ps_settings_save(&app->settings);
    }
}

static void ps_manual_coordinate(struct ps_app *app, bool latitude)
{
    char value[32];
    bool ok;
    int microdegrees;
    int limit = latitude ? 90000000 : 180000000;

    ps_format_coordinate(value, sizeof(value), latitude ?
                         app->settings.latitude_microdeg :
                         app->settings.longitude_microdeg);
    if (rb->kbd_input(value, sizeof(value), NULL) < 0)
        return;
    microdegrees = ps_decimal_to_microdegrees(value, &ok);
    if (!ok || microdegrees < -limit || microdegrees > limit)
    {
        rb->splash(HZ * 2, "Invalid coordinate");
        return;
    }
    if (latitude)
        app->settings.latitude_microdeg = microdegrees;
    else
        app->settings.longitude_microdeg = microdegrees;
    rb->strlcpy(app->settings.location_name, "Manual",
                sizeof(app->settings.location_name));
    app->settings.location_set = true;
    ps_scene_mark_time_dirty(app);
}

void ps_show_location(struct ps_app *app)
{
    MENUITEM_STRINGLIST(menu, "Location", NULL,
                        "Search cities", "Manual latitude",
                        "Manual longitude", "Elevation", "UTC offset", "Back");
    int selected = 0;
    bool done = false;

    while (!done && app->running)
    {
        switch (rb->do_menu(&menu, &selected, NULL, false))
        {
            case 0:
                ps_search_city(app);
                break;
            case 1:
                ps_manual_coordinate(app, true);
                break;
            case 2:
                ps_manual_coordinate(app, false);
                break;
            case 3:
                rb->set_int("Elevation", "m", UNIT_INT,
                            &app->settings.elevation_m, NULL, 10, -500, 9000, NULL);
                ps_scene_mark_time_dirty(app);
                break;
            case 4:
                rb->set_int("UTC offset", "min", UNIT_INT,
                            &app->settings.utc_offset_min, NULL, 15, -720, 840, NULL);
                ps_scene_mark_time_dirty(app);
                break;
            default:
                done = true;
                break;
        }
    }
    ps_settings_save(&app->settings);
}

void ps_show_time_machine(struct ps_app *app)
{
    static const char * const step_names[] =
        {"minute", "hour", "day", "month", "year"};
    static const double step_days[] =
        {1.0 / 1440.0, 1.0 / 24.0, 1.0, 30.0, 365.2425};
    astro_time_t original = ps_current_time(app);
    astro_time_t edited = original;
    int step = app->time_step;

    while (app->running)
    {
        astro_utc_t utc = Astronomy_UtcFromTime(edited);
        char line[80];
        int button;
        int clean;

        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(app->settings.night_mode ?
                               LCD_RGBPACK(220, 0, 0) : LCD_WHITE);
        rb->lcd_clear_display();
        rb->lcd_putsxy(6, 8, "Time Machine (UTC)");
        rb->snprintf(line, sizeof(line), "%04d-%02d-%02d  %02d:%02d:%02d",
                     utc.year, utc.month, utc.day, utc.hour, utc.minute,
                     (int)utc.second);
        rb->lcd_putsxy(35, 72, line);
        rb->snprintf(line, sizeof(line), "Wheel changes one %s", step_names[step]);
        rb->lcd_putsxy(25, 112, line);
        rb->lcd_putsxy(12, 160, "LEFT/RIGHT step   SELECT accept");
        rb->lcd_putsxy(12, 180, "PLAY live now     MENU cancel");
        rb->lcd_update();

        button = rb->button_get(true);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_SCROLL_FWD)
            edited = Astronomy_AddDays(edited, step_days[step]);
        else if (clean == BUTTON_SCROLL_BACK)
            edited = Astronomy_AddDays(edited, -step_days[step]);
        else if (clean == BUTTON_RIGHT)
            step = (step + 1) % ARRAYLEN(step_days);
        else if (clean == BUTTON_LEFT)
            step = (step + ARRAYLEN(step_days) - 1) % ARRAYLEN(step_days);
        else if (clean == BUTTON_SELECT)
        {
            app->simulated_time = edited;
            app->live = false;
            app->time_step = step;
            ps_scene_mark_time_dirty(app);
            return;
        }
        else if (clean == BUTTON_PLAY &&
                 !(button & (BUTTON_REPEAT | BUTTON_REL)))
        {
            app->live = true;
            ps_scene_mark_time_dirty(app);
            return;
        }
        else if (clean == BUTTON_MENU)
            return;
        else if (button == SYS_USB_CONNECTED)
        {
            app->usb = true;
            app->running = false;
            return;
        }
    }
}

void ps_show_display_settings(struct ps_app *app)
{
    MENUITEM_STRINGLIST(menu, "Display", NULL,
                        "Star magnitude", "Label magnitude", "Star labels",
                        "Constellation figures", "Constellation names",
                        "Az/alt grid", "Horizon", "Red night mode", "Back");
    int selected = 0;
    bool done = false;

    while (!done && app->running)
    {
        switch (rb->do_menu(&menu, &selected, NULL, false))
        {
            case 0:
                rb->set_int("Magnitude x10", "", UNIT_INT,
                            &app->settings.magnitude_tenths, NULL, 5, -10, 80, NULL);
                ps_scene_mark_time_dirty(app);
                break;
            case 1:
                rb->set_int("Label magnitude x10", "", UNIT_INT,
                            &app->settings.label_tenths, NULL, 5, -10, 60, NULL);
                break;
            case 2:
                rb->set_bool("Star labels", &app->settings.show_labels);
                break;
            case 3:
                rb->set_bool("Constellation figures",
                             &app->settings.show_constellations);
                break;
            case 4:
                rb->set_bool("Constellation names",
                             &app->settings.show_constellation_names);
                break;
            case 5:
                rb->set_bool("Az/alt grid", &app->settings.show_grid);
                break;
            case 6:
                rb->set_bool("Horizon", &app->settings.show_horizon);
                break;
            case 7:
                rb->set_bool("Red night mode", &app->settings.night_mode);
                break;
            default:
                done = true;
                break;
        }
    }
    ps_settings_save(&app->settings);
}

void ps_show_about(struct ps_app *app)
{
    static const char * const lines[] =
    {
        "Pocket Sky for Rockbox",
        "",
        "Stars/projection: Astroterm v1.2.0 (MIT)",
        "Ephemeris: Astronomy Engine C (MIT)",
        "Math: musl 1.2.5 (MIT/BSD)",
        "Stars: Yale Bright Star Catalogue 5",
        "Names: IAU WGSN via Astroterm",
        "Figures: Stellarium/HYG via Astroterm",
        "Cities: GeoNames (CC BY 3.0)",
        "",
        "Installed resource hashes and complete credits:",
        PS_RESOURCE_DIR "/PROVENANCE.txt"
    };
    (void)ps_list_select(app, "About / Data credits", lines, ARRAYLEN(lines));
}

int ps_show_main_menu(struct ps_app *app)
{
    MENUITEM_STRINGLIST(menu, "Pocket Sky", NULL,
                        "Return to sky", "Search", "Tonight", "Location",
                        "Time Machine", "Display settings", "About", "Exit");
    int selected = 0;

    switch (rb->do_menu(&menu, &selected, NULL, false))
    {
        case 1:
            ps_show_search(app);
            break;
        case 2:
            ps_show_tonight(app);
            break;
        case 3:
            ps_show_location(app);
            break;
        case 4:
            ps_show_time_machine(app);
            break;
        case 5:
            ps_show_display_settings(app);
            break;
        case 6:
            ps_show_about(app);
            break;
        case 7:
            return 1;
        case MENU_ATTACHED_USB:
            app->usb = true;
            app->running = false;
            break;
        default:
            break;
    }
    return 0;
}
