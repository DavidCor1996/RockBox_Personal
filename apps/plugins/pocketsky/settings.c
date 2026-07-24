#include "pocketsky.h"
#include "lib/configfile.h"

static struct ps_settings *ps_config_settings;

static struct configdata ps_config[] =
{
    { TYPE_INT, -90000000, 90000000,
      { .int_p = NULL }, "latitude_microdeg", NULL },
    { TYPE_INT, -180000000, 180000000,
      { .int_p = NULL }, "longitude_microdeg", NULL },
    { TYPE_INT, -500, 9000,
      { .int_p = NULL }, "elevation_m", NULL },
    { TYPE_INT, -720, 840,
      { .int_p = NULL }, "utc_offset_min", NULL },
    { TYPE_INT, -10, 80,
      { .int_p = NULL }, "magnitude_tenths", NULL },
    { TYPE_INT, -10, 60,
      { .int_p = NULL }, "label_tenths", NULL },
    { TYPE_INT, 0, 4,
      { .int_p = NULL }, "fov_index", NULL },
    { TYPE_BOOL, 0, 1,
      { .bool_p = NULL }, "show_labels", NULL },
    { TYPE_BOOL, 0, 1,
      { .bool_p = NULL }, "show_constellations", NULL },
    { TYPE_BOOL, 0, 1,
      { .bool_p = NULL }, "show_constellation_names", NULL },
    { TYPE_BOOL, 0, 1,
      { .bool_p = NULL }, "show_grid", NULL },
    { TYPE_BOOL, 0, 1,
      { .bool_p = NULL }, "show_horizon", NULL },
    { TYPE_BOOL, 0, 1,
      { .bool_p = NULL }, "night_mode", NULL },
    { TYPE_BOOL, 0, 1,
      { .bool_p = NULL }, "location_set", NULL },
    { TYPE_STRING, 0, 40,
      { .string = NULL }, "location_name", NULL },
};

static void ps_config_bind(struct ps_settings *settings)
{
    ps_config_settings = settings;
    ps_config[0].int_p = &settings->latitude_microdeg;
    ps_config[1].int_p = &settings->longitude_microdeg;
    ps_config[2].int_p = &settings->elevation_m;
    ps_config[3].int_p = &settings->utc_offset_min;
    ps_config[4].int_p = &settings->magnitude_tenths;
    ps_config[5].int_p = &settings->label_tenths;
    ps_config[6].int_p = &settings->fov_index;
    ps_config[7].bool_p = &settings->show_labels;
    ps_config[8].bool_p = &settings->show_constellations;
    ps_config[9].bool_p = &settings->show_constellation_names;
    ps_config[10].bool_p = &settings->show_grid;
    ps_config[11].bool_p = &settings->show_horizon;
    ps_config[12].bool_p = &settings->night_mode;
    ps_config[13].bool_p = &settings->location_set;
    ps_config[14].string = settings->location_name;
}

void ps_settings_defaults(struct ps_settings *settings)
{
    rb->memset(settings, 0, sizeof(*settings));
    settings->magnitude_tenths = 50;
    settings->label_tenths = 10;
    settings->fov_index = 1;
    settings->show_labels = true;
    settings->show_constellations = true;
    settings->show_constellation_names = true;
    settings->show_horizon = true;
    rb->strlcpy(settings->location_name, "Set location",
                sizeof(settings->location_name));
}

void ps_settings_load(struct ps_settings *settings)
{
    ps_settings_defaults(settings);
    ps_config_bind(settings);
    configfile_load(PS_SETTINGS_FILE, ps_config, ARRAYLEN(ps_config), 1);

    if (settings->latitude_microdeg < -90000000 ||
        settings->latitude_microdeg > 90000000 ||
        settings->longitude_microdeg < -180000000 ||
        settings->longitude_microdeg > 180000000 ||
        settings->utc_offset_min < -720 || settings->utc_offset_min > 840 ||
        settings->magnitude_tenths < -10 || settings->magnitude_tenths > 80 ||
        settings->fov_index < 0 || settings->fov_index > 4)
    {
        ps_settings_defaults(settings);
        ps_config_bind(settings);
    }
}

void ps_settings_save(const struct ps_settings *settings)
{
    if (ps_config_settings != settings)
        ps_config_bind((struct ps_settings *)settings);
    configfile_save(PS_SETTINGS_FILE, ps_config, ARRAYLEN(ps_config), 1);
}
