#include "ipodgames.h"

static bool ig_copy_value(char *destination, size_t size, const char *value)
{
    size_t length = rb->strlen(value);

    if (length == 0 || length >= size)
        return false;
    rb->strlcpy(destination, value, size);
    return true;
}

static bool ig_parse_unsigned(const char *value, unsigned long *result)
{
    char *end;
    unsigned long parsed;

    if (!value[0] || value[0] == '-')
        return false;
    parsed = rb->strtoul(value, &end, 10);
    if (*end != '\0')
        return false;
    *result = parsed;
    return true;
}

static void ig_set_metadata_value(struct ig_game *game, const char *key,
                                  const char *value)
{
    unsigned long number;

    if (!rb->strcmp(key, "name"))
        ig_copy_value(game->name, sizeof(game->name), value);
    else if (!rb->strcmp(key, "guid"))
        ig_copy_value(game->guid, sizeof(game->guid), value);
    else if (!rb->strcmp(key, "version"))
        ig_copy_value(game->version, sizeof(game->version), value);
    else if (!rb->strcmp(key, "executable_state"))
        ig_copy_value(game->executable_state,
                      sizeof(game->executable_state), value);
    else if (!rb->strcmp(key, "executable_sha256"))
        ig_copy_value(game->executable_sha256,
                      sizeof(game->executable_sha256), value);
    else if (!rb->strcmp(key, "local_executable"))
        ig_copy_value(game->executable_path,
                      sizeof(game->executable_path), value);
    else if (!rb->strcmp(key, "platform_id") &&
             ig_parse_unsigned(value, &number) && number <= INT_MAX)
        game->platform_id = (int)number;
    else if (!rb->strcmp(key, "build_id") &&
             ig_parse_unsigned(value, &number))
        game->build_id = number;
    else if (!rb->strcmp(key, "executable_size") &&
             ig_parse_unsigned(value, &number))
        game->executable_size = number;
}

bool ig_game_load(const char *path, struct ig_game *game)
{
    char line[256];
    int fd;
    int line_number = 0;

    rb->memset(game, 0, sizeof(*game));
    rb->strlcpy(game->metadata_path, path, sizeof(game->metadata_path));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *separator;

        ++line_number;
        if (line_number == 1)
        {
            if (rb->strcmp(line, "IPODGAMES/1"))
            {
                rb->close(fd);
                return false;
            }
            continue;
        }

        separator = rb->strchr(line, '=');
        if (!separator)
            continue;
        *separator = '\0';
        ig_set_metadata_value(game, line, separator + 1);
    }
    rb->close(fd);

    game->metadata_valid = game->name[0] && game->guid[0] &&
                           game->executable_state[0] &&
                           game->platform_id > 0;
    return game->metadata_valid;
}

static int ig_compare_games(const void *left, const void *right)
{
    const struct ig_game *a = left;
    const struct ig_game *b = right;

    return rb->strcasecmp(a->name, b->name);
}

void ig_catalog_scan(struct ig_catalog *catalog)
{
    DIR *directory;
    struct dirent *entry;

    rb->memset(catalog, 0, sizeof(*catalog));
    directory = rb->opendir(IG_GAMES_DIR);
    if (!directory)
        return;

    while (catalog->count < IG_MAX_GAMES &&
           (entry = rb->readdir(directory)) != NULL)
    {
        char path[MAX_PATH];
        struct ig_game game;

        if (!rb->strcmp(entry->d_name, ".") ||
            !rb->strcmp(entry->d_name, ".."))
            continue;

        rb->snprintf(path, sizeof(path), "%s/%s/%s", IG_GAMES_DIR,
                     entry->d_name, IG_METADATA_FILE);
        if (ig_game_load(path, &game))
            catalog->games[catalog->count++] = game;
        else if (rb->file_exists(path))
            ++catalog->invalid_count;
    }
    rb->closedir(directory);

    rb->qsort(catalog->games, catalog->count, sizeof(catalog->games[0]),
              ig_compare_games);
}
