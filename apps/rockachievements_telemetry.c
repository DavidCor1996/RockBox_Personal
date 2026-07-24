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
#include "string-extra.h"
#include "timefuncs.h"

#define ACH_ROOT ROCKBOX_DIR "/achievements"
#define ACH_CURRENT ACH_ROOT "/current"
#define ACH_STATE ACH_ROOT "/state"
#define ACH_SESSIONS ACH_STATE "/sessions.v1.tsv"
#define ACH_SESSIONS_NEW ACH_STATE "/sessions.v1.new"

static bool target_known(const char *target)
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
        char *field = line;
        char *end;
        int index;

        for (index = 0; index < 11 && field; ++index)
        {
            field = strchr(field, '\t');
            if (field)
                field++;
        }
        if (!field)
            continue;
        end = strchr(field, '\t');
        if (end)
            *end = '\0';
        if (field[0] == '@')
            field++;
        if (!strcasecmp(field, target))
        {
            close(fd);
            return true;
        }
    }
    close(fd);
    return false;
}

void rockachievements_record_session(const char *target, long started)
{
    char line[MAX_PATH + 96];
    int input;
    int output;
    bool found = false;
    long elapsed = MAX(0, (current_tick - started) / HZ);
    long timestamp;

    if (!target || !target[0] || !target_known(target))
        return;
    if (target[0] == '@')
        target++;
    timestamp = (long)mktime(get_time());
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
        fdprintf(output, "%s\t%d\t%ld\t%ld\n", target,
                 atoi(sessions_text) + 1,
                 strtol(seconds_text, NULL, 10) + elapsed, timestamp);
        found = true;
    }
    if (!found)
        fdprintf(output, "%s\t1\t%ld\t%ld\n", target, elapsed,
                 timestamp);
    close(output);
    if (input >= 0)
        close(input);
    rename(ACH_SESSIONS_NEW, ACH_SESSIONS);
}
