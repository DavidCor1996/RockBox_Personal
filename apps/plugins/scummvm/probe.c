/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "probe.h"
#include "agds_loader.h"
#include "rbfile.h"

struct sky_version {
    long dinner_entries;
    long data_size;
    const char *variant;
};

struct queen_version {
    long data_size;
    const char *variant;
};

#define SKY_CPT_SIZE 419427

static const struct sky_version sky_versions[] = {
    { 232, 734425, "Floppy demo" },
    { 243, 1328979, "PC Gamer demo" },
    { 247, 814147, "Floppy demo" },
    { 1404, 8252443, "Floppy" },
    { 1413, 8387069, "Floppy" },
    { 1445, 8830435, "Floppy" },
    { 1445, -1, "Floppy" },
    { 1711, 26623798, "CD demo" },
    { 5099, 72429382, "CD" },
    { 5097, 72395713, "CD" },
    { 5097, 73123264, "CD" },
    { 0, 0, NULL }
};

static const struct queen_version queen_versions[] = {
    { 351775, "Amiga floppy" },
    { 344575, "Amiga floppy" },
    { 22677657, "DOS floppy" },
    { 22157304, "DOS floppy" },
    { 22240013, "DOS floppy" },
    { 190787021, "DOS CD" },
    { 186689095, "DOS CD" },
    { 217648975, "DOS CD" },
    { 190705558, "DOS CD" },
    { 3732177, "DOS demo" },
    { 3735447, "DOS demo" },
    { 3724538, "DOS demo" },
    { 1915913, "DOS interview" },
    { 1889658, "DOS interview" },
    { 563335, "Amiga demo" },
    { 597032, "Amiga interview" },
    { 0, NULL }
};

static long file_size_in_dir(const char *dir, const char *name)
{
    char path[MAX_PATH];
    int fd;
    long size;

    if (!scummvm_make_path(path, sizeof(path), dir, name))
        return -1;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    size = rb->filesize(fd);
    rb->close(fd);
    return size;
}

static bool read_u32le_in_dir(const char *dir, const char *name,
                              unsigned long *value)
{
    char path[MAX_PATH];
    unsigned char buf[4];
    int fd;
    bool ok = false;

    if (!scummvm_make_path(path, sizeof(path), dir, name))
        return false;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    if (rb->read(fd, buf, sizeof(buf)) == (ssize_t)sizeof(buf)) {
        *value = (unsigned long)buf[0] |
                 ((unsigned long)buf[1] << 8) |
                 ((unsigned long)buf[2] << 16) |
                 ((unsigned long)buf[3] << 24);
        ok = true;
    }

    rb->close(fd);
    return ok;
}

static void set_result(struct scummvm_probe_result *result, bool data_found,
                       const char *engine_name, const char *variant,
                       const char *detail)
{
    result->data_found = data_found;
    rb->strlcpy(result->engine_name, engine_name, sizeof(result->engine_name));
    rb->strlcpy(result->variant, variant, sizeof(result->variant));
    rb->strlcpy(result->detail, detail, sizeof(result->detail));
}

static bool probe_engine_data_file(const char *game_dir,
                                   const char *name,
                                   long expected_size,
                                   char *detail,
                                   size_t detail_size)
{
    long size = file_size_in_dir(game_dir, name);

    if (size < 0)
        size = file_size_in_dir(SCUMMVM_ENGINE_DATA_DIR, name);

    if (size < 0) {
        rb->snprintf(detail, detail_size,
                     "Need %s in game dir or %s",
                     name, SCUMMVM_ENGINE_DATA_DIR);
        return false;
    }

    if (expected_size >= 0 && size != expected_size) {
        rb->snprintf(detail, detail_size,
                     "%s has wrong size: %ld",
                     name, size);
        return false;
    }

    rb->snprintf(detail, detail_size, "%s found", name);
    return true;
}

static void probe_sky(const struct scummvm_target *target,
                      struct scummvm_probe_result *result)
{
    long dsk_size = file_size_in_dir(target->path, "sky.dsk");
    unsigned long dinner_entries = 0;
    const struct sky_version *version;

    result->supported = true;

    if (dsk_size < 0 ||
        !read_u32le_in_dir(target->path, "sky.dnr", &dinner_entries)) {
        set_result(result, false, "sky", "", "Need sky.dsk and sky.dnr");
        result->engine_data_found = false;
        rb->strlcpy(result->engine_data_detail, "Sky data not checked",
                    sizeof(result->engine_data_detail));
        return;
    }

    result->engine_data_found =
        probe_engine_data_file(target->path, "sky.cpt", SKY_CPT_SIZE,
                               result->engine_data_detail,
                               sizeof(result->engine_data_detail));

    for (version = sky_versions; version->variant; version++) {
        if ((long)dinner_entries == version->dinner_entries &&
            (version->data_size < 0 || dsk_size == version->data_size)) {
            set_result(result, true, "sky", version->variant,
                       "Beneath a Steel Sky data found");
            return;
        }
    }

    set_result(result, true, "sky", "Unknown",
               "Sky files found, version not matched");
}

static void probe_queen(const struct scummvm_target *target,
                        struct scummvm_probe_result *result)
{
    long data_size = file_size_in_dir(target->path, "queen.1");
    const struct queen_version *version;

    result->supported = true;
    result->engine_data_found = true;
    rb->strlcpy(result->engine_data_detail, "No engine data required",
                sizeof(result->engine_data_detail));

    if (data_size < 0)
        data_size = file_size_in_dir(target->path, "queen.1c");

    if (data_size < 0) {
        set_result(result, false, "queen", "", "Need queen.1 or queen.1c");
        return;
    }

    for (version = queen_versions; version->variant; version++) {
        if (data_size == version->data_size) {
            set_result(result, true, "queen", version->variant,
                       "Flight of the Amazon Queen data found");
            return;
        }
    }

    set_result(result, true, "queen", "Unknown",
               "Queen data file found, version not matched");
}

static void probe_nibiru(const struct scummvm_target *target,
                         struct scummvm_probe_result *result)
{
    struct scummvm_agds_info info;
    char detail[96];

    result->supported = true;
    result->engine_data_found = true;
    rb->strlcpy(result->engine_data_detail,
                "Uses data from the owned AGDS installation",
                sizeof(result->engine_data_detail));
    if (!scummvm_agds_probe(target, &info, detail, sizeof(detail))) {
        set_result(result, false, "agds", "NiBiRu", detail);
        return;
    }
    set_result(result, true, "agds", "NiBiRu AGDS 2.5",
               "NiBiRu AGDS archives validated");
}

void scummvm_probe_game(const struct scummvm_target *target,
                       struct scummvm_probe_result *result)
{
    rb->memset(result, 0, sizeof(*result));

    if (!rb->strcasecmp(target->engine, "sky")) {
        probe_sky(target, result);
    } else if (!rb->strcasecmp(target->engine, "queen")) {
        probe_queen(target, result);
    } else if (!rb->strcasecmp(target->engine, "agds") ||
               !rb->strcasecmp(target->engine, "nibiru")) {
        probe_nibiru(target, result);
    } else {
        rb->strlcpy(result->engine_name, target->engine,
                    sizeof(result->engine_name));
        rb->strlcpy(result->detail, "Engine not in Rockbox allowlist",
                    sizeof(result->detail));
    }
}
