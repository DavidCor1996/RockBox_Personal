#ifndef POCKETSKY_H
#define POCKETSKY_H

#include "plugin.h"
#include <tlsf.h>
#include "math.h"
#include "upstream/astronomy_engine/astronomy.h"

#define PS_RESOURCE_DIR ROCKBOX_DIR "/apps/pocketsky"
#define PS_RESOURCE_DIR_ALT PLUGIN_APPS_DATA_DIR "/pocketsky"
#define PS_SETTINGS_FILE "pocketsky.cfg"

#define PS_HEADER_HEIGHT 18
#define PS_FOOTER_HEIGHT 18
#define PS_VIEW_TOP PS_HEADER_HEIGHT
#define PS_VIEW_HEIGHT (LCD_HEIGHT - PS_HEADER_HEIGHT - PS_FOOTER_HEIGHT)
#define PS_MAX_LABELS 12
#define PS_BODY_COUNT 11
#define PS_MAX_SEARCH_RESULTS 64
#define PS_MAX_TONIGHT_LINES 24

#define PS_STAR_RECORD_SIZE 52
#define PS_NAME_RECORD_SIZE 6
#define PS_CITY_RECORD_SIZE 24

enum ps_resource_kind
{
    PS_RESOURCE_CATALOG = 1,
    PS_RESOURCE_NAMES = 2,
    PS_RESOURCE_CONSTELLATIONS = 3,
    PS_RESOURCE_CITIES = 4
};

enum ps_selected_kind
{
    PS_SELECTED_NONE = 0,
    PS_SELECTED_STAR,
    PS_SELECTED_BODY,
    PS_SELECTED_CONSTELLATION
};

struct ps_settings
{
    int latitude_microdeg;
    int longitude_microdeg;
    int elevation_m;
    int utc_offset_min;
    int magnitude_tenths;
    int label_tenths;
    int fov_index;
    bool show_labels;
    bool show_constellations;
    bool show_constellation_names;
    bool show_grid;
    bool show_horizon;
    bool night_mode;
    bool location_set;
    char location_name[40];
};

struct ps_star_cache
{
    float east;
    float north;
    float up;
    float azimuth;
    float altitude;
    short x;
    short y;
    bool visible;
};

struct ps_body
{
    astro_body_t code;
    const char *name;
    float azimuth;
    float altitude;
    float magnitude;
    float phase_fraction;
    short x;
    short y;
    bool visible;
};

struct ps_data
{
    unsigned char *catalog;
    size_t catalog_bytes;
    unsigned int star_count;
    unsigned char *names;
    size_t names_bytes;
    unsigned int name_count;
    const char *name_pool;
    size_t name_pool_bytes;
    unsigned char *constellations;
    size_t constellation_bytes;
    unsigned int constellation_count;
    unsigned char *cities;
    size_t city_bytes;
    unsigned int city_count;
    const char *city_pool;
    size_t city_pool_bytes;
    unsigned short *hr_index;
};

struct ps_scene
{
    struct ps_star_cache *stars;
    unsigned int active_stars;
    struct ps_body bodies[PS_BODY_COUNT];
    astro_time_t time;
    double julian_date;
    double moon_phase;
    float forward_x;
    float forward_y;
    float forward_z;
    float right_x;
    float right_y;
    float up_x;
    float up_y;
    float up_z;
    float projection_scale;
    bool horizontal_dirty;
    bool projection_dirty;
};

struct ps_app
{
    struct ps_settings settings;
    struct ps_data data;
    struct ps_scene scene;
    float center_azimuth;
    float center_altitude;
    bool live;
    astro_time_t simulated_time;
    int time_step;
    enum ps_selected_kind selected_kind;
    int selected_index;
    bool running;
    bool usb;
    size_t pool_size;
    void *pool;
};

unsigned short ps_get_u16(const unsigned char *data);
unsigned int ps_get_u32(const unsigned char *data);
float ps_get_float(const unsigned char *data);
void ps_put_error(const char *title, const char *detail);
bool ps_ascii_contains(const char *haystack, const char *needle);
int ps_decimal_to_microdegrees(const char *text, bool *ok);
void ps_format_coordinate(char *buffer, size_t size, int microdegrees);

bool ps_data_load(struct ps_app *app, char *error, size_t error_size);
const unsigned char *ps_star_record(const struct ps_app *app, unsigned int index);
const char *ps_star_name(const struct ps_app *app, unsigned int index);
int ps_star_index_by_hr(const struct ps_app *app, unsigned int hr);
const char *ps_city_name(const struct ps_app *app, unsigned int index);
const char *ps_city_timezone(const struct ps_app *app, unsigned int index);
void ps_city_position(const struct ps_app *app, unsigned int index,
                      float *latitude, float *longitude);
bool ps_use_default_location(struct ps_app *app);

void ps_settings_defaults(struct ps_settings *settings);
void ps_settings_load(struct ps_settings *settings);
void ps_settings_save(const struct ps_settings *settings);

astro_time_t ps_current_time(struct ps_app *app);
void ps_scene_init(struct ps_app *app);
void ps_scene_mark_time_dirty(struct ps_app *app);
void ps_scene_mark_view_dirty(struct ps_app *app);
bool ps_scene_update(struct ps_app *app);
bool ps_project(const struct ps_app *app, float azimuth, float altitude,
                short *x, short *y);
bool ps_project_vector(const struct ps_app *app, float east, float north,
                       float up, short *x, short *y);
void ps_center_on_star(struct ps_app *app, int index);
void ps_center_on_body(struct ps_app *app, int index);
void ps_select_nearest(struct ps_app *app);

void ps_render_sky(struct ps_app *app);
void ps_show_object_card(struct ps_app *app);
void ps_show_search(struct ps_app *app);
void ps_show_tonight(struct ps_app *app);
void ps_show_location(struct ps_app *app);
void ps_show_time_machine(struct ps_app *app);
void ps_show_display_settings(struct ps_app *app);
void ps_show_about(struct ps_app *app);
int ps_show_main_menu(struct ps_app *app);

#endif
