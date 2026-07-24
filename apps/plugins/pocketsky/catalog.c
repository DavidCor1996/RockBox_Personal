#include "pocketsky.h"

#include <fcntl.h>

#define PS_RESOURCE_HEADER_SIZE 64
#define PS_RESOURCE_VERSION 2

static const unsigned char ps_magic[8] =
    {'P', 'S', 'K', 'Y', 'R', 'S', 'C', '1'};

static bool ps_pool_string_valid(const char *pool, size_t pool_bytes,
                                 unsigned int offset)
{
    return offset < pool_bytes &&
           rb->memchr(pool + offset, '\0', pool_bytes - offset) != NULL;
}

static bool ps_read_exact(int file, void *buffer, size_t bytes)
{
    unsigned char *output = buffer;

    while (bytes > 0)
    {
        ssize_t count = rb->read(file, output, bytes);
        if (count <= 0)
            return false;
        output += count;
        bytes -= count;
    }
    return true;
}

static int ps_open_resource(const char *filename, char *path, size_t path_size)
{
    int file;

    rb->snprintf(path, path_size, "%s/%s", PS_RESOURCE_DIR, filename);
    file = rb->open(path, O_RDONLY);
    if (file >= 0)
        return file;
    rb->snprintf(path, path_size, "%s/%s", PS_RESOURCE_DIR_ALT, filename);
    return rb->open(path, O_RDONLY);
}

static unsigned char *ps_load_resource(const char *filename, int expected_kind,
                                       unsigned int *record_count,
                                       size_t *payload_bytes,
                                       char *error, size_t error_size)
{
    unsigned char header[PS_RESOURCE_HEADER_SIZE];
    unsigned char *payload;
    unsigned int crc;
    unsigned int stored_crc;
    char path[MAX_PATH];
    int file;

    file = ps_open_resource(filename, path, sizeof(path));
    if (file < 0)
    {
        rb->snprintf(error, error_size,
                     "Missing %s. Run tools/pocketsky_import_data.py and install "
                     "its output in %s", filename, PS_RESOURCE_DIR);
        return NULL;
    }
    if (!ps_read_exact(file, header, sizeof(header)))
    {
        rb->snprintf(error, error_size, "Truncated resource header: %s", path);
        rb->close(file);
        return NULL;
    }
    if (rb->memcmp(header, ps_magic, sizeof(ps_magic)) != 0 ||
        ps_get_u16(header + 8) != expected_kind ||
        ps_get_u16(header + 10) != PS_RESOURCE_VERSION)
    {
        rb->snprintf(error, error_size, "Wrong resource type/version: %s", path);
        rb->close(file);
        return NULL;
    }
    *record_count = ps_get_u32(header + 12);
    *payload_bytes = ps_get_u32(header + 16);
    stored_crc = ps_get_u32(header + 52);
    if (*payload_bytes == 0 || *payload_bytes > 1024 * 1024)
    {
        rb->snprintf(error, error_size, "Invalid resource size: %s", path);
        rb->close(file);
        return NULL;
    }
    payload = tlsf_malloc(*payload_bytes);
    if (!payload)
    {
        rb->snprintf(error, error_size, "Out of memory loading %s", filename);
        rb->close(file);
        return NULL;
    }
    if (!ps_read_exact(file, payload, *payload_bytes))
    {
        rb->snprintf(error, error_size, "Truncated resource payload: %s", path);
        tlsf_free(payload);
        rb->close(file);
        return NULL;
    }
    rb->close(file);
    crc = rb->crc_32r(payload, *payload_bytes, 0xffffffff) ^ 0xffffffff;
    if (crc != stored_crc)
    {
        rb->snprintf(error, error_size, "Resource CRC failed: %s", path);
        tlsf_free(payload);
        return NULL;
    }
    return payload;
}

bool ps_data_load(struct ps_app *app, char *error, size_t error_size)
{
    unsigned int i;
    size_t table_bytes;

    app->data.catalog = ps_load_resource(
        "catalog.psc", PS_RESOURCE_CATALOG, &app->data.star_count,
        &app->data.catalog_bytes, error, error_size);
    if (!app->data.catalog)
        return false;
    if (app->data.star_count != 9110 ||
        app->data.catalog_bytes != app->data.star_count * PS_STAR_RECORD_SIZE)
    {
        rb->snprintf(error, error_size, "Catalog count/size is invalid");
        return false;
    }

    app->data.names = ps_load_resource(
        "names.psc", PS_RESOURCE_NAMES, &app->data.name_count,
        &app->data.names_bytes, error, error_size);
    if (!app->data.names)
        return false;
    table_bytes = app->data.name_count * PS_NAME_RECORD_SIZE;
    if (app->data.name_count != 333 || table_bytes >= app->data.names_bytes)
    {
        rb->snprintf(error, error_size, "Named-star table is invalid");
        return false;
    }
    app->data.name_pool = (const char *)app->data.names + table_bytes;
    app->data.name_pool_bytes = app->data.names_bytes - table_bytes;
    for (i = 0; i < app->data.name_count; ++i)
    {
        const unsigned char *record = app->data.names + i * PS_NAME_RECORD_SIZE;
        unsigned int hr = ps_get_u16(record);
        unsigned int offset = ps_get_u32(record + 2);
        if (hr < 1 || hr > 9110 ||
            !ps_pool_string_valid(app->data.name_pool,
                                  app->data.name_pool_bytes, offset))
        {
            rb->snprintf(error, error_size, "Named-star string is invalid");
            return false;
        }
    }

    app->data.constellations = ps_load_resource(
        "constellations.psc", PS_RESOURCE_CONSTELLATIONS,
        &app->data.constellation_count, &app->data.constellation_bytes,
        error, error_size);
    if (!app->data.constellations || app->data.constellation_count != 88)
        return false;

    app->data.cities = ps_load_resource(
        "cities.psc", PS_RESOURCE_CITIES, &app->data.city_count,
        &app->data.city_bytes, error, error_size);
    if (!app->data.cities)
        return false;
    table_bytes = app->data.city_count * PS_CITY_RECORD_SIZE;
    if (app->data.city_count < 2900 || table_bytes >= app->data.city_bytes)
    {
        rb->snprintf(error, error_size, "City table is invalid");
        return false;
    }
    app->data.city_pool = (const char *)app->data.cities + table_bytes;
    app->data.city_pool_bytes = app->data.city_bytes - table_bytes;
    for (i = 0; i < app->data.city_count; ++i)
    {
        const unsigned char *record =
            app->data.cities + i * PS_CITY_RECORD_SIZE;
        float latitude = ps_get_float(record);
        float longitude = ps_get_float(record + 4);
        if (!isfinite(latitude) || !isfinite(longitude) ||
            latitude < -90.0f || latitude > 90.0f ||
            longitude < -180.0f || longitude > 180.0f ||
            !ps_pool_string_valid(app->data.city_pool,
                                  app->data.city_pool_bytes,
                                  ps_get_u32(record + 12)) ||
            !ps_pool_string_valid(app->data.city_pool,
                                  app->data.city_pool_bytes,
                                  ps_get_u32(record + 16)))
        {
            rb->snprintf(error, error_size, "City record/string is invalid");
            return false;
        }
    }

    app->data.hr_index = tlsf_malloc(9111 * sizeof(*app->data.hr_index));
    app->scene.stars = tlsf_calloc(app->data.star_count,
                                   sizeof(*app->scene.stars));
    if (!app->data.hr_index || !app->scene.stars)
    {
        rb->snprintf(error, error_size, "Out of memory creating sky indexes");
        return false;
    }
    for (i = 0; i <= 9110; ++i)
        app->data.hr_index[i] = 0xffff;
    for (i = 0; i < app->data.star_count; ++i)
    {
        unsigned int hr = ps_get_u16(ps_star_record(app, i));
        if (hr < 1 || hr > 9110 || app->data.hr_index[hr] != 0xffff)
        {
            rb->snprintf(error, error_size, "Catalog HR index is invalid");
            return false;
        }
        app->data.hr_index[hr] = i;
    }
    {
        const unsigned char *cursor = app->data.constellations;
        const unsigned char *end = cursor + app->data.constellation_bytes;
        for (i = 0; i < app->data.constellation_count; ++i)
        {
            unsigned int name_length;
            unsigned int segment_count;
            unsigned int reference;
            if (cursor + 8 > end)
                goto invalid_constellations;
            name_length = cursor[4];
            segment_count = ps_get_u16(cursor + 6);
            cursor += 8;
            if (name_length == 0 ||
                cursor + name_length + segment_count * 4 > end)
                goto invalid_constellations;
            cursor += name_length;
            for (reference = 0; reference < segment_count * 2; ++reference)
            {
                unsigned int hr = ps_get_u16(cursor + reference * 2);
                if (hr < 1 || hr > 9110 || app->data.hr_index[hr] == 0xffff)
                    goto invalid_constellations;
            }
            cursor += segment_count * 4;
        }
        if (cursor != end)
            goto invalid_constellations;
    }
    return true;

invalid_constellations:
    rb->snprintf(error, error_size, "Constellation references are invalid");
    return false;
}

const unsigned char *ps_star_record(const struct ps_app *app, unsigned int index)
{
    if (index >= app->data.star_count)
        return NULL;
    return app->data.catalog + index * PS_STAR_RECORD_SIZE;
}

const char *ps_star_name(const struct ps_app *app, unsigned int index)
{
    const unsigned char *record = ps_star_record(app, index);
    unsigned int offset;

    if (!record)
        return NULL;
    offset = ps_get_u32(record + 22);
    if (offset == 0xffffffff || offset >= app->data.name_pool_bytes)
        return NULL;
    if (!rb->memchr(app->data.name_pool + offset, '\0',
                    app->data.name_pool_bytes - offset))
        return NULL;
    return app->data.name_pool + offset;
}

int ps_star_index_by_hr(const struct ps_app *app, unsigned int hr)
{
    if (hr < 1 || hr > 9110 || app->data.hr_index[hr] == 0xffff)
        return -1;
    return app->data.hr_index[hr];
}

const char *ps_city_name(const struct ps_app *app, unsigned int index)
{
    unsigned int offset;

    if (index >= app->data.city_count)
        return NULL;
    offset = ps_get_u32(app->data.cities + index * PS_CITY_RECORD_SIZE + 12);
    if (!ps_pool_string_valid(app->data.city_pool,
                              app->data.city_pool_bytes, offset))
        return NULL;
    return app->data.city_pool + offset;
}

const char *ps_city_timezone(const struct ps_app *app, unsigned int index)
{
    unsigned int offset;

    if (index >= app->data.city_count)
        return NULL;
    offset = ps_get_u32(app->data.cities + index * PS_CITY_RECORD_SIZE + 16);
    if (!ps_pool_string_valid(app->data.city_pool,
                              app->data.city_pool_bytes, offset))
        return NULL;
    return app->data.city_pool + offset;
}

void ps_city_position(const struct ps_app *app, unsigned int index,
                      float *latitude, float *longitude)
{
    const unsigned char *record;

    if (index >= app->data.city_count)
    {
        *latitude = 0;
        *longitude = 0;
        return;
    }
    record = app->data.cities + index * PS_CITY_RECORD_SIZE;
    *latitude = ps_get_float(record);
    *longitude = ps_get_float(record + 4);
}

bool ps_use_default_location(struct ps_app *app)
{
    unsigned int index;

    for (index = 0; index < app->data.city_count; ++index)
    {
        const char *name = ps_city_name(app, index);
        const char *timezone = ps_city_timezone(app, index);
        float latitude;
        float longitude;

        if (!name || !timezone || rb->strcmp(name, "Moncton") != 0 ||
            rb->strcmp(timezone, "America/Moncton") != 0)
            continue;
        ps_city_position(app, index, &latitude, &longitude);
        app->settings.latitude_microdeg = latitude * 1000000.0f;
        app->settings.longitude_microdeg = longitude * 1000000.0f;
        app->settings.elevation_m = 30;
        app->settings.utc_offset_min = -180;
        app->settings.location_set = true;
        rb->strlcpy(app->settings.location_name, name,
                    sizeof(app->settings.location_name));
        return true;
    }
    return false;
}
