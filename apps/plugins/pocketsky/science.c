#include "pocketsky.h"
#include "upstream/astroterm/astro.h"
#include "upstream/astroterm/coord.h"

#define PS_RAD (M_PI / 180.0)
#define PS_DEG (180.0 / M_PI)

static const float ps_fov_degrees[] = {120.0f, 90.0f, 60.0f, 40.0f, 25.0f};

static const astro_body_t ps_body_codes[PS_BODY_COUNT] =
{
    BODY_SUN, BODY_MOON, BODY_MERCURY, BODY_VENUS, BODY_MARS,
    BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE, BODY_PLUTO,
    BODY_EARTH
};

static const char * const ps_body_names[PS_BODY_COUNT] =
{
    "Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter",
    "Saturn", "Uranus", "Neptune", "Pluto", "Earth"
};

astro_time_t ps_current_time(struct ps_app *app)
{
    struct tm *clock;
    astro_time_t local;

    if (!app->live)
        return app->simulated_time;
    clock = rb->get_time();
    if (!clock || clock->tm_year + 1900 < 1900 ||
        clock->tm_year + 1900 > 2099)
        return Astronomy_MakeTime(2000, 1, 1, 0, 0, 0.0);
    local = Astronomy_MakeTime(clock->tm_year + 1900, clock->tm_mon + 1,
                               clock->tm_mday, clock->tm_hour,
                               clock->tm_min, clock->tm_sec);
    return Astronomy_AddDays(local,
        -(double)app->settings.utc_offset_min / (24.0 * 60.0));
}

void ps_scene_init(struct ps_app *app)
{
    int i;

    app->center_azimuth = 180.0f;
    app->center_altitude = 35.0f;
    app->live = true;
    app->time_step = 1;
    app->selected_kind = PS_SELECTED_NONE;
    app->selected_index = -1;
    app->scene.horizontal_dirty = true;
    app->scene.projection_dirty = true;
    for (i = 0; i < PS_BODY_COUNT; ++i)
    {
        app->scene.bodies[i].code = ps_body_codes[i];
        app->scene.bodies[i].name = ps_body_names[i];
    }
}

void ps_scene_mark_time_dirty(struct ps_app *app)
{
    app->scene.horizontal_dirty = true;
    app->scene.projection_dirty = true;
}

void ps_scene_mark_view_dirty(struct ps_app *app)
{
    app->scene.projection_dirty = true;
}

bool ps_project_vector(const struct ps_app *app, float point_x,
                       float point_y, float point_z, short *x, short *y)
{
    float forward = point_x * app->scene.forward_x +
                    point_y * app->scene.forward_y +
                    point_z * app->scene.forward_z;
    float denominator;
    float plane_x;
    float plane_y;
    float projected_x;
    float projected_y;
    int screen_x;
    int screen_y;

    if (forward <= -0.99)
        return false;
    denominator = 1.0 + forward;
    plane_x = 2.0 * (point_x * app->scene.right_x +
                     point_y * app->scene.right_y) / denominator;
    plane_y = 2.0 * (point_x * app->scene.up_x +
                     point_y * app->scene.up_y +
                     point_z * app->scene.up_z) / denominator;
    projected_x = plane_x * app->scene.projection_scale;
    projected_y = plane_y * app->scene.projection_scale;
    screen_x = LCD_WIDTH / 2 +
               (int)(projected_x + (projected_x < 0.0f ? -0.5f : 0.5f));
    screen_y = PS_VIEW_TOP + PS_VIEW_HEIGHT / 2 -
               (int)(projected_y + (projected_y < 0.0f ? -0.5f : 0.5f));
    if (screen_x < -512 || screen_x > LCD_WIDTH + 512 ||
        screen_y < PS_VIEW_TOP - 512 || screen_y > LCD_HEIGHT + 512)
        return false;
    *x = screen_x;
    *y = screen_y;
    return screen_x >= 0 && screen_x < LCD_WIDTH &&
           screen_y >= PS_VIEW_TOP && screen_y < PS_VIEW_TOP + PS_VIEW_HEIGHT;
}

bool ps_project(const struct ps_app *app, float azimuth, float altitude,
                short *x, short *y)
{
    float az = azimuth * PS_RAD;
    float alt = altitude * PS_RAD;
    float cos_alt = cos(alt);

    return ps_project_vector(app, cos_alt * sin(az), cos_alt * cos(az),
                             sin(alt), x, y);
}

static void ps_star_angles(struct ps_star_cache *star)
{
    double up = star->up;
    double azimuth;

    if (up > 1.0)
        up = 1.0;
    else if (up < -1.0)
        up = -1.0;
    star->altitude = asin(up) * PS_DEG;
    azimuth = atan2(star->east, star->north) * PS_DEG;
    if (azimuth < 0.0)
        azimuth += 360.0;
    star->azimuth = azimuth;
}

static bool ps_update_horizontal(struct ps_app *app)
{
    astro_observer_t observer;
    double latitude;
    double longitude;
    double gmst;
    double local_sidereal;
    float sin_latitude;
    float cos_latitude;
    float sin_sidereal;
    float cos_sidereal;
    float years_from_epoch;
    float threshold;
    unsigned int i;

    app->scene.time = ps_current_time(app);
    app->scene.julian_date = app->scene.time.ut + 2451545.0;
    latitude = app->settings.latitude_microdeg / 1000000.0 * PS_RAD;
    longitude = app->settings.longitude_microdeg / 1000000.0 * PS_RAD;
    threshold = app->settings.magnitude_tenths / 10.0f;
    gmst = greenwich_mean_sidereal_time_rad(app->scene.julian_date);
    local_sidereal = gmst + longitude;
    sin_latitude = sin(latitude);
    cos_latitude = cos(latitude);
    sin_sidereal = sin(local_sidereal);
    cos_sidereal = cos(local_sidereal);
    years_from_epoch = (app->scene.julian_date - 2451545.0) / 365.2425;
    app->scene.active_stars = 0;

    for (i = 0; i < app->data.star_count; ++i)
    {
        const unsigned char *record = ps_star_record(app, i);
        struct ps_star_cache *star = &app->scene.stars[i];
        float eq_x;
        float eq_y;
        float eq_z;
        float meridian;

        if (ps_get_float(record + 18) > threshold)
            break;
        eq_x = ps_get_float(record + 28) +
               years_from_epoch * ps_get_float(record + 40);
        eq_y = ps_get_float(record + 32) +
               years_from_epoch * ps_get_float(record + 44);
        eq_z = ps_get_float(record + 36) +
               years_from_epoch * ps_get_float(record + 48);
        meridian = cos_sidereal * eq_x + sin_sidereal * eq_y;
        star->east = -sin_sidereal * eq_x + cos_sidereal * eq_y;
        star->north = -sin_latitude * meridian + cos_latitude * eq_z;
        star->up = cos_latitude * meridian + sin_latitude * eq_z;
        ++app->scene.active_stars;
        if ((i & 0xff) == 0xff)
            rb->yield();
    }

    observer = Astronomy_MakeObserver(
        app->settings.latitude_microdeg / 1000000.0,
        app->settings.longitude_microdeg / 1000000.0,
        app->settings.elevation_m);
    for (i = 0; i < PS_BODY_COUNT - 1; ++i)
    {
        struct ps_body *body = &app->scene.bodies[i];
        astro_equatorial_t equatorial = Astronomy_Equator(
            body->code, &app->scene.time, observer, EQUATOR_OF_DATE, ABERRATION);
        astro_horizon_t horizon;
        astro_illum_t illumination;

        if (equatorial.status != ASTRO_SUCCESS)
            return false;
        horizon = Astronomy_Horizon(&app->scene.time, observer,
                                    equatorial.ra, equatorial.dec,
                                    REFRACTION_NORMAL);
        body->azimuth = horizon.azimuth;
        body->altitude = horizon.altitude;
        body->magnitude = body->code == BODY_SUN ? -26.74f : 0.0f;
        body->phase_fraction = 1.0f;
        if (body->code != BODY_SUN)
        {
            illumination = Astronomy_Illumination(body->code, app->scene.time);
            if (illumination.status == ASTRO_SUCCESS)
            {
                body->magnitude = illumination.mag;
                body->phase_fraction = illumination.phase_fraction;
            }
        }
    }
    app->scene.moon_phase = Astronomy_MoonPhase(app->scene.time).angle;
    app->scene.horizontal_dirty = false;
    app->scene.projection_dirty = true;
    return true;
}

static void ps_update_projection(struct ps_app *app)
{
    float center_az = app->center_azimuth * PS_RAD;
    float center_alt = app->center_altitude * PS_RAD;
    float sin_az = sin(center_az);
    float cos_az = cos(center_az);
    float sin_alt = sin(center_alt);
    float cos_alt = cos(center_alt);
    float edge;
    unsigned int i;

    app->scene.forward_x = cos_alt * sin_az;
    app->scene.forward_y = cos_alt * cos_az;
    app->scene.forward_z = sin_alt;
    app->scene.right_x = cos_az;
    app->scene.right_y = -sin_az;
    app->scene.up_x = -sin_az * sin_alt;
    app->scene.up_y = -cos_az * sin_alt;
    app->scene.up_z = cos_alt;
    edge = 2.0 * tan(ps_fov_degrees[app->settings.fov_index] *
                     PS_RAD / 4.0);
    app->scene.projection_scale = (PS_VIEW_HEIGHT / 2.0) / edge;

    for (i = 0; i < app->scene.active_stars; ++i)
    {
        struct ps_star_cache *star = &app->scene.stars[i];
        star->visible = ps_project_vector(app, star->east, star->north,
                                          star->up, &star->x, &star->y);
    }
    for (i = 0; i < PS_BODY_COUNT - 1; ++i)
    {
        struct ps_body *body = &app->scene.bodies[i];
        body->visible = ps_project(app, body->azimuth, body->altitude,
                                   &body->x, &body->y);
    }
    app->scene.projection_dirty = false;
}

bool ps_scene_update(struct ps_app *app)
{
    if (app->scene.horizontal_dirty && !ps_update_horizontal(app))
        return false;
    if (app->scene.projection_dirty)
        ps_update_projection(app);
    return true;
}

void ps_center_on_star(struct ps_app *app, int index)
{
    if (index < 0 || (unsigned int)index >= app->scene.active_stars)
        return;
    ps_star_angles(&app->scene.stars[index]);
    app->center_azimuth = app->scene.stars[index].azimuth;
    app->center_altitude = app->scene.stars[index].altitude;
    app->selected_kind = PS_SELECTED_STAR;
    app->selected_index = index;
    ps_scene_mark_view_dirty(app);
}

void ps_center_on_body(struct ps_app *app, int index)
{
    if (index < 0 || index >= PS_BODY_COUNT - 1)
        return;
    app->center_azimuth = app->scene.bodies[index].azimuth;
    app->center_altitude = app->scene.bodies[index].altitude;
    app->selected_kind = PS_SELECTED_BODY;
    app->selected_index = index;
    ps_scene_mark_view_dirty(app);
}

void ps_select_nearest(struct ps_app *app)
{
    int center_x = LCD_WIDTH / 2;
    int center_y = PS_VIEW_TOP + PS_VIEW_HEIGHT / 2;
    int best_distance = 16 * 16 + 1;
    int best_index = -1;
    enum ps_selected_kind best_kind = PS_SELECTED_NONE;
    unsigned int i;

    for (i = 0; i < app->scene.active_stars; ++i)
    {
        int dx;
        int dy;
        int distance;
        if (!app->scene.stars[i].visible)
            continue;
        dx = app->scene.stars[i].x - center_x;
        dy = app->scene.stars[i].y - center_y;
        distance = dx * dx + dy * dy;
        if (distance < best_distance)
        {
            best_distance = distance;
            best_index = i;
            best_kind = PS_SELECTED_STAR;
        }
    }
    for (i = 0; i < PS_BODY_COUNT - 1; ++i)
    {
        int dx;
        int dy;
        int distance;
        if (!app->scene.bodies[i].visible)
            continue;
        dx = app->scene.bodies[i].x - center_x;
        dy = app->scene.bodies[i].y - center_y;
        distance = dx * dx + dy * dy;
        if (distance < best_distance)
        {
            best_distance = distance;
            best_index = i;
            best_kind = PS_SELECTED_BODY;
        }
    }
    app->selected_kind = best_kind;
    app->selected_index = best_index;
    if (best_kind == PS_SELECTED_STAR)
        ps_star_angles(&app->scene.stars[best_index]);
}
