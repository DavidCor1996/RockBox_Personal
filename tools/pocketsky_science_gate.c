/* Host harness used by pocketsky_science_gate.py. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astronomy.h"
#include "astro.h"
#include "coord.h"

#define RESOURCE_HEADER 64
#define STAR_SIZE 52
#define DEG (180.0 / 3.14159265358979323846)
#define RAD (3.14159265358979323846 / 180.0)

struct fixed_case {
    int year, month, day, hour, minute;
    double latitude, longitude;
};

static const struct fixed_case cases[] = {
    {1900, 1, 1, 0, 0, 0.0, 0.0},
    {1900, 6, 21, 12, 0, 45.0, -64.0},
    {1950, 3, 20, 6, 0, -45.0, 18.0},
    {1969, 7, 20, 20, 17, 28.6, -80.6},
    {1999, 12, 31, 23, 59, 80.0, 179.0},
    {2000, 2, 29, 12, 0, -80.0, -179.0},
    {2000, 6, 21, 0, 0, 0.0, 180.0},
    {2000, 12, 21, 0, 0, 0.0, -180.0},
    {2010, 1, 15, 3, 30, 45.0, 0.0},
    {2010, 7, 15, 15, 45, -45.0, 0.0},
    {2020, 2, 29, 23, 0, 80.0, 90.0},
    {2020, 2, 29, 1, 0, -80.0, -90.0},
    {2024, 3, 10, 6, 59, 46.09454, -64.7965},
    {2024, 3, 10, 7, 1, 46.09454, -64.7965},
    {2024, 11, 3, 4, 59, 46.09454, -64.7965},
    {2024, 11, 3, 6, 1, 46.09454, -64.7965},
    {2026, 7, 14, 12, 0, 46.09454, -64.7965},
    {2030, 9, 22, 18, 0, 0.0, 120.0},
    {2040, 6, 21, 12, 0, 45.0, 45.0},
    {2050, 12, 21, 12, 0, -45.0, -45.0},
    {2075, 4, 1, 9, 15, 80.0, -135.0},
    {2099, 1, 1, 0, 0, -80.0, 135.0},
    {2099, 6, 30, 23, 59, 45.0, 179.9},
    {2099, 12, 31, 23, 59, -45.0, -179.9},
};

static const astro_body_t bodies[] = {
    BODY_SUN, BODY_MOON, BODY_MERCURY, BODY_VENUS, BODY_MARS,
    BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE, BODY_PLUTO,
};

static const unsigned star_hrs[] = {15, 2491, 2943, 3982, 7001, 7557, 8728};

static uint16_t get_u16(const unsigned char *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static float get_float(const unsigned char *p)
{
    uint32_t bits = get_u32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static unsigned char *load_catalog(const char *root, size_t *size)
{
    char path[1024];
    FILE *file;
    long length;
    unsigned char *data;

    snprintf(path, sizeof(path), "%s/catalog.psc", root);
    file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) < RESOURCE_HEADER || fseek(file, 0, SEEK_SET) != 0)
        return NULL;
    data = malloc((size_t)length);
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (size_t)length;
    return data;
}

static const unsigned char *find_star(const unsigned char *data, size_t size,
                                      unsigned hr)
{
    uint32_t count;
    uint32_t i;
    const unsigned char *payload;

    if (size < RESOURCE_HEADER || memcmp(data, "PSKYRSC1", 8) != 0)
        return NULL;
    count = get_u32(data + 12);
    if (count != 9110 || size != RESOURCE_HEADER + count * STAR_SIZE)
        return NULL;
    payload = data + RESOURCE_HEADER;
    for (i = 0; i < count; ++i)
        if (get_u16(payload + i * STAR_SIZE) == hr)
            return payload + i * STAR_SIZE;
    return NULL;
}

int main(int argc, char **argv)
{
    unsigned char *catalog;
    size_t catalog_size;
    size_t c;

    if (argc != 2 || !(catalog = load_catalog(argv[1], &catalog_size)))
        return 2;
    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c)
    {
        const struct fixed_case *test = &cases[c];
        astro_time_t time = Astronomy_MakeTime(
            test->year, test->month, test->day, test->hour, test->minute, 0.0);
        astro_observer_t observer = Astronomy_MakeObserver(
            test->latitude, test->longitude, 0.0);
        double jd = time.ut + 2451545.0;
        double gmst = greenwich_mean_sidereal_time_rad(jd);
        size_t i;

        for (i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i)
        {
            astro_equatorial_t equ = Astronomy_Equator(
                bodies[i], &time, observer, EQUATOR_OF_DATE, ABERRATION);
            astro_horizon_t hor = Astronomy_Horizon(
                &time, observer, equ.ra, equ.dec, REFRACTION_NORMAL);
            astro_illum_t illum = Astronomy_Illumination(bodies[i], time);
            printf("C%02u B%02u %.12f %.12f %.12f %.12f\n",
                   (unsigned)c, (unsigned)bodies[i], hor.azimuth, hor.altitude,
                   illum.mag, illum.phase_fraction);
        }
        for (i = 0; i < sizeof(star_hrs) / sizeof(star_hrs[0]); ++i)
        {
            const unsigned char *star = find_star(
                catalog, catalog_size, star_hrs[i]);
            double ra, dec, az, alt, fast_az, fast_alt;
            double years, lst, sin_lat, cos_lat, sin_lst, cos_lst;
            double x, y, z, meridian, east, north, up, az_delta;
            if (!star)
            {
                free(catalog);
                return 3;
            }
            calc_star_position(get_float(star + 2), get_float(star + 10),
                               get_float(star + 6), get_float(star + 14),
                               jd, &ra, &dec);
            equatorial_to_horizontal(ra, dec, gmst, test->latitude * RAD,
                                     test->longitude * RAD, &az, &alt);
            years = (jd - 2451545.0) / 365.2425;
            x = get_float(star + 28) + years * get_float(star + 40);
            y = get_float(star + 32) + years * get_float(star + 44);
            z = get_float(star + 36) + years * get_float(star + 48);
            lst = gmst + test->longitude * RAD;
            sin_lat = sin(test->latitude * RAD);
            cos_lat = cos(test->latitude * RAD);
            sin_lst = sin(lst);
            cos_lst = cos(lst);
            meridian = cos_lst * x + sin_lst * y;
            east = -sin_lst * x + cos_lst * y;
            north = -sin_lat * meridian + cos_lat * z;
            up = cos_lat * meridian + sin_lat * z;
            if (up > 1.0)
                up = 1.0;
            else if (up < -1.0)
                up = -1.0;
            fast_alt = asin(up);
            fast_az = atan2(east, north);
            if (fast_az < 0.0)
                fast_az += 2.0 * 3.14159265358979323846;
            az_delta = fabs(fast_az - az);
            if (az_delta > 3.14159265358979323846)
                az_delta = 2.0 * 3.14159265358979323846 - az_delta;
            if (az_delta * DEG > 0.02 || fabs(fast_alt - alt) * DEG > 0.02)
            {
                free(catalog);
                return 4;
            }
            printf("C%02u S%04u %.12f %.12f\n", (unsigned)c, star_hrs[i],
                   fast_az * DEG, fast_alt * DEG);
        }
    }
    free(catalog);
    return 0;
}
