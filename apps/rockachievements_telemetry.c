/***************************************************************************
 * Shared local achievement session telemetry for every plugin launcher.
 ***************************************************************************/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "misc.h"
#ifdef HAVE_IPODJS_UI
#include "notification_manager.h"
#endif
#include "string-extra.h"
#include "timefuncs.h"

#define ACH_ROOT ROCKBOX_DIR "/achievements"
#define ACH_CURRENT ACH_ROOT "/current"
#define ACH_STATE ACH_ROOT "/state"
#define ACH_SESSIONS ACH_STATE "/sessions.v1.tsv"
#define ACH_SESSIONS_NEW ACH_STATE "/sessions.v1.new"
#define ACH_UNLOCKS ACH_STATE "/unlocks.v1.tsv"
#define ACH_EVENTS ACH_STATE "/events.v1.tsv"

struct achievement_target_info
{
    char key[24];
    char title[72];
    char kind[24];
};

static bool target_info(const char *target,
                        struct achievement_target_info *info)
{
    char generation[40];
    char catalog[MAX_PATH];
    char line[1024];
    int fd;

    if (!target || !target[0])
        return false;
    if (target[0] == '@')
        target++;
    fd = open(ACH_CURRENT, O_RDONLY);
    if (fd < 0 || read_line(fd, generation, sizeof(generation)) <= 0)
    {
        if (fd >= 0)
            close(fd);
        return false;
    }
    close(fd);
    snprintf(catalog, sizeof(catalog),
             ACH_ROOT "/generations/%s/catalog.tsv", generation);
    fd = open(catalog, O_RDONLY);
    if (fd < 0)
        return false;
    read_line(fd, line, sizeof(line));
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[14];
        char *cursor = line;
        int index;

        for (index = 0; index < (int)ARRAYLEN(fields); ++index)
        {
            fields[index] = cursor;
            cursor = strchr(cursor, '\t');
            if (!cursor)
                break;
            *cursor++ = '\0';
        }
        if (index < 11)
            continue;
        cursor = fields[11];
        if (cursor[0] == '@')
            cursor++;
        if (!strcasecmp(cursor, target))
        {
            if (info)
            {
                strmemccpy(info->key, fields[0], sizeof(info->key));
                strmemccpy(info->title, fields[1], sizeof(info->title));
                strmemccpy(info->kind, fields[3], sizeof(info->kind));
            }
            close(fd);
            return true;
        }
    }
    close(fd);
    return false;
}

#ifdef HAVE_IPODJS_UI
static uint32_t achievement_stable_id(const char *key, const char *id)
{
    uint32_t hash = 2166136261u;
    const unsigned char *text;

    for (text = (const unsigned char *)key; *text; ++text)
    {
        hash ^= *text;
        hash *= 16777619u;
    }
    hash ^= ':';
    hash *= 16777619u;
    for (text = (const unsigned char *)id; *text; ++text)
    {
        hash ^= *text;
        hash *= 16777619u;
    }
    return hash ? hash : 1;
}

static bool achievement_already_recorded(const char *key, const char *id)
{
    char line[256];
    int fd = open(ACH_UNLOCKS, O_RDONLY);

    if (fd < 0)
        return false;
    read_line(fd, line, sizeof(line));
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *tab = strchr(line, '\t');
        char *row_id;
        char *end;

        if (!tab)
            continue;
        *tab++ = '\0';
        row_id = tab;
        end = strchr(row_id, '\t');
        if (end)
            *end = '\0';
        if (!strcmp(line, key) && !strcmp(row_id, id))
        {
            close(fd);
            return true;
        }
    }
    close(fd);
    return false;
}

static unsigned long achievement_next_sequence(void)
{
    char line[256];
    unsigned long sequence = 0;
    int fd = open(ACH_EVENTS, O_RDONLY);

    if (fd < 0)
        return 1;
    read_line(fd, line, sizeof(line));
    while (read_line(fd, line, sizeof(line)) > 0)
        sequence = MAX(sequence, strtoul(line, NULL, 10));
    close(fd);
    return sequence + 1;
}

static void achievement_unlock(const struct achievement_target_info *info,
                               const char *id, const char *title,
                               long timestamp)
{
    struct notification_request request;
    unsigned long sequence;
    bool unlock_written = false;
    bool event_written = false;
    int fd;

    if (achievement_already_recorded(info->key, id))
        return;
    sequence = achievement_next_sequence();
    fd = open(ACH_UNLOCKS, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        if (lseek(fd, 0, SEEK_END) == 0)
            fdprintf(fd, "game_key\tachievement_id\tunlock_time\tsource\n");
        unlock_written = fdprintf(fd, "%s\t%s\t%ld\trockpod-local\n",
                                  info->key, id, timestamp) > 0;
        close(fd);
    }
    fd = open(ACH_EVENTS, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        if (lseek(fd, 0, SEEK_END) == 0)
            fdprintf(fd, "sequence\tgame_key\tachievement_id\tevent_time\tmode\tclient\n");
        event_written = fdprintf(fd, "%lu\t%s\t%s\t%ld\tlocal\tipod\n",
                                 sequence, info->key, id, timestamp) > 0;
        close(fd);
    }
    if (!unlock_written || !event_written)
        return;
    memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_ACHIEVEMENTS;
    request.kind = NOTIFICATION_ACHIEVEMENT_UNLOCKED;
    request.stable_id = achievement_stable_id(info->key, id);
    request.timestamp = timestamp;
    strmemccpy(request.title, "Achievement Unlocked",
               sizeof(request.title));
    snprintf(request.body, sizeof(request.body), "%s — %s",
             info->title, title);
    strmemccpy(request.route, "achievements", sizeof(request.route));
    notification_post(&request);
}

static void achievement_evaluate_baseline(
    const struct achievement_target_info *info, int sessions, long seconds,
    long timestamp)
{
    static const struct {
        const char *id;
        const char *title;
        int sessions;
        long seconds;
    } rules[] = {
        { "local-first-play", "First Play", 1, 0 },
        { "local-15-minutes", "Getting Started", 0, 15 * 60 },
        { "local-60-minutes", "Settled In", 0, 60 * 60 },
        { "local-10-sessions", "Regular Player", 10, 0 },
    };
    int index;

    if (strcmp(info->kind, "local-baseline"))
        return;
    for (index = 0; index < (int)ARRAYLEN(rules); ++index)
        if ((!rules[index].sessions || sessions >= rules[index].sessions) &&
            (!rules[index].seconds || seconds >= rules[index].seconds))
            achievement_unlock(info, rules[index].id, rules[index].title,
                               timestamp);
}
#endif

void rockachievements_record_session(const char *target, long started)
{
    struct achievement_target_info info;
    char line[MAX_PATH + 96];
    int input;
    int output;
    bool found = false;
    int total_sessions = 1;
    long total_seconds;
    long elapsed = MAX(0, (current_tick - started) / HZ);
    long timestamp;

    if (!target || !target[0] || !target_info(target, &info))
        return;
    if (target[0] == '@')
        target++;
    timestamp = (long)mktime(get_time());
    total_seconds = elapsed;
    mkdir(ACH_ROOT);
    mkdir(ACH_STATE);
    input = open(ACH_SESSIONS, O_RDONLY);
    output = open(ACH_SESSIONS_NEW, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output < 0)
    {
        if (input >= 0)
            close(input);
        return;
    }
    if (input < 0)
        fdprintf(output,
                 "launch_target\tsessions\tseconds\tlast_played\n");
    while (input >= 0 && read_line(input, line, sizeof(line)) > 0)
    {
        char original[MAX_PATH + 96];
        char *cursor;
        char *sessions_text;
        char *seconds_text;

        strmemccpy(original, line, sizeof(original));
        cursor = strchr(line, '\t');
        if (!cursor || strncmp(line, target, cursor - line) ||
            strlen(target) != (size_t)(cursor - line))
        {
            fdprintf(output, "%s\n", original);
            continue;
        }
        *cursor++ = '\0';
        sessions_text = cursor;
        cursor = strchr(cursor, '\t');
        if (!cursor)
        {
            fdprintf(output, "%s\n", original);
            continue;
        }
        *cursor++ = '\0';
        seconds_text = cursor;
        cursor = strchr(cursor, '\t');
        if (cursor)
            *cursor = '\0';
        total_sessions = atoi(sessions_text) + 1;
        total_seconds = strtol(seconds_text, NULL, 10) + elapsed;
        fdprintf(output, "%s\t%d\t%ld\t%ld\n", target,
                 total_sessions, total_seconds, timestamp);
        found = true;
    }
    if (!found)
        fdprintf(output, "%s\t1\t%ld\t%ld\n", target, elapsed,
                 timestamp);
    close(output);
    if (input >= 0)
        close(input);
    if (rename(ACH_SESSIONS_NEW, ACH_SESSIONS) < 0)
        return;
#ifdef HAVE_IPODJS_UI
    achievement_evaluate_baseline(&info, total_sessions, total_seconds,
                                  timestamp);
#endif
}
