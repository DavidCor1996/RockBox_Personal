#include "plugin.h"
#include "rockachievements.h"
#include "rockachievements_allocator.h"
#include "../../../lib/rcheevos/include/rc_runtime.h"

#define RA_ROOT ROCKBOX_DIR "/achievements"
#define RA_CURRENT RA_ROOT "/current"
#define RA_UNLOCKS RA_ROOT "/state/unlocks.v1.tsv"
#define RA_EVENTS RA_ROOT "/state/events.v1.tsv"
#define RA_RUNTIME_DIR RA_ROOT "/state/runtime"
#define RA_MAX_UNLOCKS 384

static struct rockachievements_runtime *active_runtime;
static uint32_t known_unlocks[RA_MAX_UNLOCKS];
static unsigned known_unlock_count;

static int split_tsv(char *line, char **fields, int count)
{
    int index;
    char *cursor = line;

    for (index = 0; index < count; ++index)
    {
        char *tab;

        fields[index] = cursor;
        tab = rb->strchr(cursor, '\t');
        if (!tab)
            return index + 1;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static bool unlock_known(uint32_t id)
{
    unsigned index;

    for (index = 0; index < known_unlock_count; ++index)
        if (known_unlocks[index] == id)
            return true;
    return false;
}

static void load_known_unlocks(const char *game_key)
{
    char line[256];
    int fd = rb->open(RA_UNLOCKS, O_RDONLY);

    known_unlock_count = 0;
    if (fd < 0)
        return;
    rb->read_line(fd, line, sizeof(line));
    while (known_unlock_count < RA_MAX_UNLOCKS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[4];

        if (split_tsv(line, fields, 4) >= 2 &&
            !rb->strcmp(fields[0], game_key))
            known_unlocks[known_unlock_count++] =
                rb->strtoul(fields[1], NULL, 10);
    }
    rb->close(fd);
}

static void append_header_if_empty(int fd, const char *header)
{
    if (rb->lseek(fd, 0, SEEK_END) == 0)
        rb->fdprintf(fd, "%s\n", header);
}

static unsigned long next_event_sequence(void)
{
    char line[256];
    unsigned long sequence = 0;
    int fd = rb->open(RA_EVENTS, O_RDONLY);

    if (fd < 0)
        return 1;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        unsigned long value = rb->strtoul(line, NULL, 10);
        if (value > sequence)
            sequence = value;
    }
    rb->close(fd);
    return sequence + 1;
}

static bool save_progress(struct rockachievements_runtime *runtime)
{
    rc_runtime_t *rc_runtime = (rc_runtime_t *)runtime->runtime;
    uint32_t size;
    uint8_t *buffer;
    char temporary[MAX_PATH];
    int fd;
    bool saved = false;

    if (!runtime->enabled || runtime->hardcore || !rc_runtime)
        return false;
    size = rc_runtime_progress_size(rc_runtime, NULL);
    if (!size)
        return true;
    buffer = rockachievements_malloc(size);
    if (!buffer)
        return false;
    if (rc_runtime_serialize_progress_sized(buffer, size, rc_runtime, NULL) !=
        RC_OK)
        goto done;
    rb->snprintf(temporary, sizeof(temporary), "%s.new",
                 runtime->progress_path);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        goto done;
    saved = rb->write(fd, buffer, size) == (ssize_t)size;
    rb->close(fd);
    if (saved)
        saved = rb->rename(temporary, runtime->progress_path) == 0;
    else
        rb->remove(temporary);

done:
    rockachievements_free(buffer);
    return saved;
}

static void record_unlock(uint32_t id)
{
    long timestamp;
    unsigned long sequence;
    int fd;

    if (!active_runtime || unlock_known(id))
        return;
    timestamp = (long)rb->mktime(rb->get_time());
    sequence = next_event_sequence();
    fd = rb->open(RA_UNLOCKS, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        append_header_if_empty(
            fd, "game_key\tachievement_id\tunlock_time\tsource");
        rb->fdprintf(fd, "%s\t%lu\t%ld\tipod-hardcore\n",
                     active_runtime->game_key, (unsigned long)id, timestamp);
        rb->close(fd);
    }
    fd = rb->open(RA_EVENTS, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        append_header_if_empty(
            fd, "sequence\tgame_key\tachievement_id\tevent_time\tmode\tclient");
        rb->fdprintf(fd, "%lu\t%s\t%lu\t%ld\thardcore\tipod\n",
                     sequence, active_runtime->game_key,
                     (unsigned long)id, timestamp);
        rb->close(fd);
    }
    if (known_unlock_count < RA_MAX_UNLOCKS)
        known_unlocks[known_unlock_count++] = id;
    save_progress(active_runtime);
}

static void runtime_event(const rc_runtime_event_t *event)
{
    if (event->type == RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED)
        record_unlock(event->id);
}

static uint32_t runtime_peek(uint32_t address, uint32_t num_bytes,
                             void *userdata)
{
    struct rockachievements_runtime *runtime = userdata;
    return runtime->peek(address, num_bytes, runtime->peek_userdata);
}

static bool find_game(const char *launch_target, char *game_key,
                      size_t game_key_size, char *achievement_path,
                      size_t achievement_path_size)
{
    char normalized_target[MAX_PATH];
    char generation[40];
    char path[MAX_PATH];
    char line[1024];
    int fd = rb->open(RA_CURRENT, O_RDONLY);

    if (launch_target[0] == '@')
        rb->snprintf(normalized_target, sizeof(normalized_target), "/%s",
                     launch_target + 1);
    else
        rb->strlcpy(normalized_target, launch_target,
                    sizeof(normalized_target));

    if (fd < 0 || rb->read_line(fd, generation, sizeof(generation)) <= 0)
    {
        if (fd >= 0)
            rb->close(fd);
        return false;
    }
    rb->close(fd);
    rb->snprintf(path, sizeof(path), RA_ROOT "/generations/%s/catalog.tsv",
                 generation);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[14];

        if (split_tsv(line, fields, 14) < 12 ||
            rb->strcmp(fields[11], normalized_target) ||
            rb->strcmp(fields[3], "retroachievements"))
            continue;
        rb->strlcpy(game_key, fields[0], game_key_size);
        rb->strlcpy(achievement_path, fields[5], achievement_path_size);
        rb->close(fd);
        return true;
    }
    rb->close(fd);
    return false;
}

static bool valid_runtime_definition(const char *definition)
{
    const char *cursor;

    if (!definition || !definition[0])
        return false;
    cursor = definition;
    while (*cursor)
    {
        if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X'))
            return true;
        ++cursor;
    }
    return false;
}

static bool achievement_file_has_runtime_definition(const char *path)
{
    char line[4096];
    int fd = rb->open(path, O_RDONLY);

    if (fd < 0)
        return false;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[11];

        if (split_tsv(line, fields, 11) >= 11 &&
            !rb->strcmp(fields[9], "retroachievements") &&
            valid_runtime_definition(fields[10]))
        {
            rb->close(fd);
            return true;
        }
    }
    rb->close(fd);
    return false;
}

bool rockachievements_available(const char *launch_target)
{
    char game_key[21];
    char achievement_path[MAX_PATH];

    return launch_target &&
        find_game(launch_target, game_key, sizeof(game_key),
                  achievement_path, sizeof(achievement_path)) &&
        achievement_file_has_runtime_definition(achievement_path);
}

static unsigned activate_achievements(struct rockachievements_runtime *runtime,
                                      const char *path)
{
    char line[4096];
    int fd = rb->open(path, O_RDONLY);
    unsigned count = 0;

    if (fd < 0)
        return 0;
    rb->read_line(fd, line, sizeof(line));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[11];
        uint32_t id;

        if (split_tsv(line, fields, 11) < 11 ||
            rb->strcmp(fields[9], "retroachievements") ||
            !valid_runtime_definition(fields[10]))
            continue;
        id = rb->strtoul(fields[0], NULL, 10);
        if (!id || !rb->strcmp(fields[6], "unlocked") || unlock_known(id))
            continue;
        if (rc_runtime_activate_achievement(
                (rc_runtime_t *)runtime->runtime, id, fields[10], NULL, 0) ==
            RC_OK)
            ++count;
    }
    rb->close(fd);
    return count;
}

static void load_progress(struct rockachievements_runtime *runtime)
{
    rc_runtime_t *rc_runtime = (rc_runtime_t *)runtime->runtime;
    off_t size;
    uint8_t *buffer;
    int fd = rb->open(runtime->progress_path, O_RDONLY);

    if (fd < 0)
        return;
    size = rb->filesize(fd);
    if (size <= 0 || size > ROCKACHIEVEMENTS_WORKSPACE_TARGET)
    {
        rb->close(fd);
        return;
    }
    buffer = rockachievements_malloc(size);
    if (buffer && rb->read(fd, buffer, size) == size)
        rc_runtime_deserialize_progress_sized(rc_runtime, buffer, size, NULL);
    rockachievements_free(buffer);
    rb->close(fd);
}

bool rockachievements_init(struct rockachievements_runtime *runtime,
                           const char *launch_target,
                           rockachievements_peek_t peek,
                           void *peek_userdata,
                           void *workspace,
                           size_t workspace_size)
{
    char achievement_path[MAX_PATH];

    rb->memset(runtime, 0, sizeof(*runtime));
    if (!launch_target || !peek || !workspace ||
        workspace_size < ROCKACHIEVEMENTS_WORKSPACE_MIN)
        return false;
    if (!find_game(launch_target, runtime->game_key,
                   sizeof(runtime->game_key), achievement_path,
                   sizeof(achievement_path)))
        return false;
    rockachievements_allocator_reset(workspace, workspace_size);
    runtime->runtime = rc_runtime_alloc();
    if (!runtime->runtime)
        return false;
    runtime->peek = peek;
    runtime->peek_userdata = peek_userdata;
    runtime->hardcore = true;
    load_known_unlocks(runtime->game_key);
    runtime->active_count = activate_achievements(runtime, achievement_path);
    if (!runtime->active_count)
    {
        rc_runtime_destroy((rc_runtime_t *)runtime->runtime);
        runtime->runtime = NULL;
        return false;
    }
    rb->mkdir(RA_ROOT "/state");
    rb->mkdir(RA_RUNTIME_DIR);
    rb->snprintf(runtime->progress_path, sizeof(runtime->progress_path),
                 RA_RUNTIME_DIR "/%s.bin", runtime->game_key);
    runtime->enabled = true;
    active_runtime = runtime;
    if (!runtime->hardcore)
        load_progress(runtime);
    return true;
}

void rockachievements_do_frame(struct rockachievements_runtime *runtime)
{
    if (runtime && runtime->enabled)
        rc_runtime_do_frame((rc_runtime_t *)runtime->runtime, runtime_event,
                            runtime_peek, runtime, NULL);
}

void rockachievements_reset(struct rockachievements_runtime *runtime)
{
    if (runtime && runtime->enabled)
        rc_runtime_reset((rc_runtime_t *)runtime->runtime);
}

void rockachievements_shutdown(struct rockachievements_runtime *runtime)
{
    if (!runtime || !runtime->runtime)
        return;
    save_progress(runtime);
    rc_runtime_destroy((rc_runtime_t *)runtime->runtime);
    if (active_runtime == runtime)
        active_runtime = NULL;
    runtime->runtime = NULL;
    runtime->enabled = false;
    runtime->hardcore = false;
}

bool rockachievements_hardcore_active(
    const struct rockachievements_runtime *runtime)
{
    return runtime && runtime->enabled && runtime->hardcore;
}

bool rockachievements_any_hardcore_active(void)
{
    return rockachievements_hardcore_active(active_runtime);
}
