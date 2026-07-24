/***************************************************************************
 * iPod Hero external skin loader
 ****************************************************************************/

#include "ipodhero.h"

#include <fcntl.h>

struct ih_skin_paths
{
    char background[64];
    char background_alt[64];
    char highway[64];
    char logo[64];
    char gems[64];
    char hopo[64];
    char star[64];
    char rings[64];
    char flames[64];
    char hud[64];
    char rock_meter[64];
    char star_meter[64];
    char sustains[64];
    char results_stars[64];
    uint32_t background_crc;
    uint32_t background_alt_crc;
    uint32_t highway_crc;
    uint32_t logo_crc;
    uint32_t gems_crc;
    uint32_t hopo_crc;
    uint32_t star_crc;
    uint32_t rings_crc;
    uint32_t flames_crc;
    uint32_t hud_crc;
    uint32_t rock_meter_crc;
    uint32_t star_meter_crc;
    uint32_t sustains_crc;
    uint32_t results_stars_crc;
};

static bool ih_safe_leaf(const char *name)
{
    const char *p;

    if (name[0] == '\0' || name[0] == '/' || rb->strchr(name, '\\') != NULL)
        return false;
    for (p = name; p[0] != '\0'; ++p)
        if (p[0] == '.' && p[1] == '.')
            return false;
    return true;
}

static void ih_assign_path(struct ih_skin_paths *paths, const char *key,
                           const char *value)
{
    char *dest = NULL;
    size_t size = 0;

    if (!rb->strcmp(key, "background"))
    {
        dest = paths->background;
        size = sizeof(paths->background);
    }
    else if (!rb->strcmp(key, "background_alt"))
    {
        dest = paths->background_alt;
        size = sizeof(paths->background_alt);
    }
    else if (!rb->strcmp(key, "highway"))
    {
        dest = paths->highway;
        size = sizeof(paths->highway);
    }
    else if (!rb->strcmp(key, "logo"))
    {
        dest = paths->logo;
        size = sizeof(paths->logo);
    }
    else if (!rb->strcmp(key, "gems"))
    {
        dest = paths->gems;
        size = sizeof(paths->gems);
    }
    else if (!rb->strcmp(key, "hopo"))
    {
        dest = paths->hopo;
        size = sizeof(paths->hopo);
    }
    else if (!rb->strcmp(key, "star"))
    {
        dest = paths->star;
        size = sizeof(paths->star);
    }
    else if (!rb->strcmp(key, "rings"))
    {
        dest = paths->rings;
        size = sizeof(paths->rings);
    }
    else if (!rb->strcmp(key, "flames"))
    {
        dest = paths->flames;
        size = sizeof(paths->flames);
    }
    else if (!rb->strcmp(key, "hud"))
    {
        dest = paths->hud;
        size = sizeof(paths->hud);
    }
    else if (!rb->strcmp(key, "rock_meter"))
    {
        dest = paths->rock_meter;
        size = sizeof(paths->rock_meter);
    }
    else if (!rb->strcmp(key, "star_meter"))
    {
        dest = paths->star_meter;
        size = sizeof(paths->star_meter);
    }
    else if (!rb->strcmp(key, "sustains"))
    {
        dest = paths->sustains;
        size = sizeof(paths->sustains);
    }
    else if (!rb->strcmp(key, "results_stars"))
    {
        dest = paths->results_stars;
        size = sizeof(paths->results_stars);
    }
    if (dest != NULL && ih_safe_leaf(value))
        rb->strlcpy(dest, value, size);
}

static void ih_assign_crc(struct ih_skin_paths *paths, const char *key,
                          const char *value)
{
    uint32_t *dest = NULL;

    if (!rb->strcmp(key, "background_crc32"))
        dest = &paths->background_crc;
    else if (!rb->strcmp(key, "background_alt_crc32"))
        dest = &paths->background_alt_crc;
    else if (!rb->strcmp(key, "highway_crc32"))
        dest = &paths->highway_crc;
    else if (!rb->strcmp(key, "logo_crc32"))
        dest = &paths->logo_crc;
    else if (!rb->strcmp(key, "gems_crc32"))
        dest = &paths->gems_crc;
    else if (!rb->strcmp(key, "hopo_crc32"))
        dest = &paths->hopo_crc;
    else if (!rb->strcmp(key, "star_crc32"))
        dest = &paths->star_crc;
    else if (!rb->strcmp(key, "rings_crc32"))
        dest = &paths->rings_crc;
    else if (!rb->strcmp(key, "flames_crc32"))
        dest = &paths->flames_crc;
    else if (!rb->strcmp(key, "hud_crc32"))
        dest = &paths->hud_crc;
    else if (!rb->strcmp(key, "rock_meter_crc32"))
        dest = &paths->rock_meter_crc;
    else if (!rb->strcmp(key, "star_meter_crc32"))
        dest = &paths->star_meter_crc;
    else if (!rb->strcmp(key, "sustains_crc32"))
        dest = &paths->sustains_crc;
    else if (!rb->strcmp(key, "results_stars_crc32"))
        dest = &paths->results_stars_crc;
    if (dest != NULL)
        *dest = rb->strtoul(value, NULL, 16);
}

static bool ih_file_crc(const char *path, uint32_t *result)
{
    /* The skin loader is single-threaded.  Avoid stacking this scratch
     * buffer underneath the native BMP decoder. */
    static unsigned char buffer[512];
    uint32_t crc = 0xffffffffu;
    int fd = rb->open(path, O_RDONLY);

    if (fd < 0)
        return false;
    while (true)
    {
        ssize_t count = rb->read(fd, buffer, sizeof(buffer));
        if (count < 0)
        {
            rb->close(fd);
            return false;
        }
        if (count == 0)
            break;
        crc = rb->crc_32(buffer, (size_t)count, crc);
    }
    rb->close(fd);
    *result = crc;
    return true;
}

static bool ih_load_image(struct ih_skin *skin, struct ih_arena *arena,
                          struct ih_image *image, const char *leaf,
                          uint32_t expected_crc, int width, int height,
                          char *error,
                          size_t error_size)
{
    char path[MAX_PATH];
    int needed;
    int result;
    uint32_t actual_crc;

    rb->memset(image, 0, sizeof(*image));
    if (!ih_safe_leaf(leaf))
    {
        rb->snprintf(error, error_size, "Invalid skin asset path");
        return false;
    }
    rb->snprintf(path, sizeof(path), "%s/%s", skin->root, leaf);
    if (expected_crc == 0 || !ih_file_crc(path, &actual_crc) ||
        actual_crc != expected_crc)
    {
        rb->snprintf(error, error_size, "Skin asset CRC mismatch\n%s", leaf);
        return false;
    }
    needed = rb->read_bmp_file(path, &image->bitmap, 0,
                               FORMAT_NATIVE | FORMAT_RETURN_SIZE, NULL);
    if (needed <= 0 || skin->decoded_bytes + (size_t)needed > IH_SKIN_LIMIT)
    {
        rb->snprintf(error, error_size, "Missing or oversized asset\n%s",
                     leaf);
        return false;
    }
    image->bitmap.data = ih_arena_alloc(arena, (size_t)needed);
    if (image->bitmap.data == NULL)
    {
        rb->snprintf(error, error_size, "Not enough memory for\n%s", leaf);
        return false;
    }
    result = rb->read_bmp_file(path, &image->bitmap, needed,
                               FORMAT_NATIVE, NULL);
    if (result <= 0 || (width > 0 && image->bitmap.width != width) ||
        (height > 0 && image->bitmap.height != height))
    {
        rb->snprintf(error, error_size, "Wrong asset dimensions\n%s", leaf);
        return false;
    }
    image->loaded = true;
    skin->decoded_bytes += (size_t)needed;
    return true;
}

bool ih_skin_load(struct ih_skin *skin, struct ih_arena *arena,
                  const char *skin_id, char *error, size_t error_size)
{
    /* These buffers are temporary but large enough to overflow the native
     * firmware stack when combined with read_bmp_file().  A plugin instance
     * loads one skin at a time, so BSS-backed scratch storage is sufficient. */
    static struct ih_skin_paths paths;
    static char manifest[MAX_PATH];
    static char line[192];
    int fd;

    rb->memset(skin, 0, sizeof(*skin));
    rb->memset(&paths, 0, sizeof(paths));
    if (!ih_safe_leaf(skin_id))
    {
        rb->snprintf(error, error_size, "Invalid skin id");
        return false;
    }
    rb->snprintf(skin->root, sizeof(skin->root), "%s/skins/%s",
                 IH_DATA_DIR, skin_id);
    rb->snprintf(manifest, sizeof(manifest), "%s/skin.ihs", skin->root);
    fd = rb->open(manifest, O_RDONLY);
    if (fd < 0)
    {
        rb->snprintf(error, error_size,
                     "iPod Hero assets are not installed\n"
                     "Run tools/ipodhero_prepare_assets.py");
        return false;
    }
    if (rb->read_line(fd, line, sizeof(line)) <= 0 ||
        rb->strcmp(line, "IHS1"))
    {
        rb->close(fd);
        rb->snprintf(error, error_size, "Unsupported skin manifest");
        return false;
    }
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *value;
        if (line[0] == '#' || line[0] == '\0')
            continue;
        value = rb->strchr(line, '=');
        if (value == NULL)
            continue;
        *value++ = '\0';
        if (!rb->strcmp(line, "name"))
            rb->strlcpy(skin->name, value, sizeof(skin->name));
        else if (rb->strstr(line, "_crc32") != NULL)
            ih_assign_crc(&paths, line, value);
        else
            ih_assign_path(&paths, line, value);
    }
    rb->close(fd);

    if (!ih_load_image(skin, arena, &skin->background, paths.background,
                       paths.background_crc, 320, 240, error, error_size) ||
        !ih_load_image(skin, arena, &skin->background_alt,
                       paths.background_alt, paths.background_alt_crc,
                       320, 240, error, error_size) ||
        !ih_load_image(skin, arena, &skin->highway, paths.highway,
                       paths.highway_crc, 200, 220, error, error_size) ||
        !ih_load_image(skin, arena, &skin->logo, paths.logo,
                       paths.logo_crc, 240, 80, error, error_size) ||
        !ih_load_image(skin, arena, &skin->gems, paths.gems,
                       paths.gems_crc, 160, 128, error, error_size) ||
        !ih_load_image(skin, arena, &skin->hopo, paths.hopo,
                       paths.hopo_crc, 160, 128, error, error_size) ||
        !ih_load_image(skin, arena, &skin->star, paths.star,
                       paths.star_crc, 160, 128, error, error_size) ||
        !ih_load_image(skin, arena, &skin->rings, paths.rings,
                       paths.rings_crc, 160, 64, error, error_size) ||
        !ih_load_image(skin, arena, &skin->flames, paths.flames,
                       paths.flames_crc, 160, 32, error, error_size) ||
        !ih_load_image(skin, arena, &skin->hud, paths.hud,
                       paths.hud_crc, 128, 28, error, error_size) ||
        !ih_load_image(skin, arena, &skin->rock_meter, paths.rock_meter,
                       paths.rock_meter_crc, 64, 44, error, error_size) ||
        !ih_load_image(skin, arena, &skin->star_meter, paths.star_meter,
                       paths.star_meter_crc, 80, 40, error, error_size) ||
        !ih_load_image(skin, arena, &skin->sustains, paths.sustains,
                       paths.sustains_crc, 80, 16, error, error_size) ||
        !ih_load_image(skin, arena, &skin->results_stars,
                       paths.results_stars, paths.results_stars_crc,
                       150, 60, error, error_size))
        return false;
    return true;
}
