/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "sky_loader.h"
#include "sky_cpt_loader.h"

#include "upstream-1.9.0/engines/sky/rnc_deco.h"

#define SKY_DNR_ENTRY_SIZE 8
#define SKY_HEADER_SIZE 22
#define SKY_MAX_DNR_ENTRIES 4096
#define SKY_MAX_TEST_FILE_SIZE (2U * 1024U * 1024U)
#define SKY_MAX_TEST_UNPACKED_SIZE (3U * 1024U * 1024U)
#define SKY_STARTUP_SCREEN_FILE 60110
#define SKY_STARTUP_PALETTE_FILE 60111
#define SKY_GAME_SCREEN_H 192
#define SKY_GRID_FILE_START 60000
#define SKY_GRID_COUNT 70

static struct scummvm_sky_resource current_screen;
static struct scummvm_sky_resource current_palette;
static struct scummvm_sky_resource current_sequence;
static uint32_t current_sequence_pos;
static uint32_t current_sequence_frames_left;

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void set_status(char *status, size_t status_size, const char *message)
{
    rb->strlcpy(status, message, status_size);
}

static bool make_sky_path(const char *dir, const char *file,
                          char *out, size_t out_size)
{
    int len = rb->snprintf(out, out_size, "%s/%s", dir, file);
    return len > 0 && (size_t)len < out_size;
}

static bool read_all(int fd, uint8_t *dst, size_t size)
{
    size_t done = 0;

    while (done < size) {
        ssize_t got = rb->read(fd, dst + done, size - done);
        if (got <= 0)
            return false;
        done += (size_t)got;
    }

    return true;
}

void scummvm_sky_loader_release_resource(
    struct scummvm_sky_resource *resource)
{
    if (resource->data)
        delete[] resource->data;
    resource->data = NULL;
    resource->size = 0;
    resource->file_nr = 0;
}

void scummvm_sky_loader_reset(void)
{
    scummvm_sky_loader_release_resource(&current_screen);
    scummvm_sky_loader_release_resource(&current_palette);
    scummvm_sky_loader_release_resource(&current_sequence);
    current_sequence_pos = 0;
    current_sequence_frames_left = 0;
}

static bool unpack_if_needed(uint8_t *packed,
                             uint32_t packed_size,
                             uint32_t file_flags,
                             struct scummvm_sky_resource *out)
{
    bool uncompressed = ((file_flags >> 23) & 1) != 0;
    uint16_t header_flags;
    uint32_t unpacked_size;
    uint8_t *unpacked;
    int32_t unpacked_len;
    Sky::RncDecoder decoder;

    out->data = packed;
    out->size = packed_size;

    if (packed_size < SKY_HEADER_SIZE || uncompressed)
        return true;

    header_flags = read_le16(packed);
    if (((header_flags >> 7) & 1) == 0)
        return true;

    unpacked_size = (uint32_t)(header_flags & ~0xffU) << 8;
    unpacked_size |= read_le16(packed + 12);
    if (unpacked_size < SKY_HEADER_SIZE ||
        unpacked_size > SKY_MAX_TEST_UNPACKED_SIZE)
        return false;

    unpacked = new uint8_t[unpacked_size];
    if (!unpacked)
        return false;

    if ((file_flags >> 22) & 1) {
        unpacked_len = decoder.unpackM1(packed + SKY_HEADER_SIZE, unpacked, 0);
    } else {
        rb->memcpy(unpacked, packed, SKY_HEADER_SIZE);
        unpacked_len = decoder.unpackM1(packed + SKY_HEADER_SIZE,
                                        unpacked + SKY_HEADER_SIZE, 0);
        if (unpacked_len)
            unpacked_len += SKY_HEADER_SIZE;
    }

    if (unpacked_len == 0) {
        delete[] unpacked;
        return true;
    }

    if ((uint32_t)unpacked_len != unpacked_size) {
        delete[] unpacked;
        return false;
    }

    delete[] packed;
    out->data = unpacked;
    out->size = unpacked_size;
    return true;
}

static uint32_t sky_offset_shift(uint32_t entries, long dsk_size)
{
    if (entries == 1445 && dsk_size != 8830435)
        return 3;
    return 4;
}

static bool load_resource_from_open_files(int dsk_fd,
                                          long dsk_size,
                                          const uint8_t *dnr,
                                          uint32_t entries,
                                          uint16_t wanted_file,
                                          struct scummvm_sky_resource *out)
{
    uint32_t index;
    uint32_t shift = sky_offset_shift(entries, dsk_size);

    scummvm_sky_loader_release_resource(out);

    for (index = 0; index < entries; index++) {
        const uint8_t *entry = dnr + index * SKY_DNR_ENTRY_SIZE;
        uint16_t file_nr = read_le16(entry);
        uint32_t raw_offset;
        uint32_t file_flags;
        uint32_t file_size;
        uint32_t file_offset;
        uint8_t *file_data;

        if (file_nr != wanted_file)
            continue;

        raw_offset = read_le32(entry + 2) & 0x0ffffffU;
        file_flags = read_le24(entry + 5);
        file_size = file_flags & 0x03fffffU;
        file_offset = raw_offset & 0x7fffffU;

        if (file_size == 0 || file_size > SKY_MAX_TEST_FILE_SIZE)
            return false;

        if ((raw_offset >> 23) & 1)
            file_offset <<= shift;

        file_data = new uint8_t[file_size];
        if (!file_data)
            return false;

        if (rb->lseek(dsk_fd, file_offset, SEEK_SET) < 0 ||
            !read_all(dsk_fd, file_data, file_size)) {
            delete[] file_data;
            return false;
        }

        out->file_nr = wanted_file;
        if (!unpack_if_needed(file_data, file_size, file_flags, out)) {
            scummvm_sky_loader_release_resource(out);
            return false;
        }

        return true;
    }

    return false;
}

static bool open_sky_tables(const struct scummvm_target *target,
                            int *dnr_fd,
                            int *dsk_fd,
                            uint8_t **dnr,
                            uint32_t *entries,
                            long *dsk_size,
                            char *status,
                            size_t status_size)
{
    char path[MAX_PATH];
    uint8_t count_buf[4];

    *dnr_fd = -1;
    *dsk_fd = -1;
    *dnr = NULL;
    *entries = 0;
    *dsk_size = -1;

    if (!make_sky_path(target->path, "sky.dnr", path, sizeof(path))) {
        set_status(status, status_size, "Sky path is too long");
        return false;
    }

    *dnr_fd = rb->open(path, O_RDONLY);
    if (*dnr_fd < 0) {
        set_status(status, status_size, "Unable to open sky.dnr");
        return false;
    }

    if (!read_all(*dnr_fd, count_buf, sizeof(count_buf))) {
        set_status(status, status_size, "Unable to read sky.dnr header");
        return false;
    }

    *entries = read_le32(count_buf);
    if (*entries == 0 || *entries > SKY_MAX_DNR_ENTRIES) {
        set_status(status, status_size, "Unsupported sky.dnr table size");
        return false;
    }

    *dnr = new uint8_t[*entries * SKY_DNR_ENTRY_SIZE];
    if (!*dnr) {
        set_status(status, status_size, "Not enough memory for sky.dnr");
        return false;
    }

    if (!read_all(*dnr_fd, *dnr, *entries * SKY_DNR_ENTRY_SIZE)) {
        set_status(status, status_size, "Unable to read sky.dnr table");
        return false;
    }

    if (!make_sky_path(target->path, "sky.dsk", path, sizeof(path))) {
        set_status(status, status_size, "Sky path is too long");
        return false;
    }

    *dsk_fd = rb->open(path, O_RDONLY);
    if (*dsk_fd < 0) {
        set_status(status, status_size, "Unable to open sky.dsk");
        return false;
    }

    *dsk_size = rb->filesize(*dsk_fd);
    return *dsk_size >= 0;
}

static void close_sky_tables(int dnr_fd, int dsk_fd, uint8_t *dnr)
{
    if (dsk_fd >= 0)
        rb->close(dsk_fd);
    if (dnr)
        delete[] dnr;
    if (dnr_fd >= 0)
        rb->close(dnr_fd);
}

bool scummvm_sky_loader_probe(const struct scummvm_target *target,
                              char *status,
                              size_t status_size)
{
    int dnr_fd = -1;
    int dsk_fd = -1;
    uint8_t *dnr = NULL;
    uint32_t entries;
    long dsk_size;
    uint32_t index;
    bool ok = false;

    if (!open_sky_tables(target, &dnr_fd, &dsk_fd, &dnr, &entries, &dsk_size,
                         status, status_size))
        goto out;

    for (index = 0; index < entries; index++) {
        const uint8_t *entry = dnr + index * SKY_DNR_ENTRY_SIZE;
        uint16_t file_nr = read_le16(entry);
        uint32_t raw_offset = read_le32(entry + 2) & 0x0ffffffU;
        uint32_t file_flags = read_le24(entry + 5);
        uint32_t file_size = file_flags & 0x03fffffU;
        uint32_t file_offset = raw_offset & 0x7fffffU;
        struct scummvm_sky_resource test_resource;

        if (file_size == 0 || file_size > SKY_MAX_TEST_FILE_SIZE)
            continue;

        if ((raw_offset >> 23) & 1)
            file_offset <<= sky_offset_shift(entries, dsk_size);

        rb->memset(&test_resource, 0, sizeof(test_resource));
        ok = load_resource_from_open_files(dsk_fd, dsk_size, dnr, entries,
                                           file_nr, &test_resource);
        scummvm_sky_loader_release_resource(&test_resource);

        if (ok) {
            rb->snprintf(status, status_size,
                         "Sky resource %u loaded (%lu bytes)",
                         (unsigned)file_nr, (unsigned long)file_size);
            goto out;
        }

        (void)file_offset;
    }

    set_status(status, status_size, "No loadable Sky resource found");

out:
    close_sky_tables(dnr_fd, dsk_fd, dnr);

    return ok;
}

bool scummvm_sky_loader_load_startup(const struct scummvm_target *target,
                                     char *status,
                                     size_t status_size)
{
    return scummvm_sky_loader_load_screen(target,
                                          SKY_STARTUP_SCREEN_FILE,
                                          SKY_STARTUP_PALETTE_FILE,
                                          status,
                                          status_size);
}

bool scummvm_sky_loader_load_screen(const struct scummvm_target *target,
                                    uint16_t screen_file,
                                    uint16_t palette_file,
                                    char *status,
                                    size_t status_size)
{
    int dnr_fd = -1;
    int dsk_fd = -1;
    uint8_t *dnr = NULL;
    uint32_t entries;
    long dsk_size;
    bool ok = false;

    scummvm_sky_loader_reset();

    if (!open_sky_tables(target, &dnr_fd, &dsk_fd, &dnr, &entries, &dsk_size,
                         status, status_size))
        goto out;

    if (!load_resource_from_open_files(dsk_fd, dsk_size, dnr, entries,
                                       screen_file,
                                       &current_screen)) {
        rb->snprintf(status, status_size, "Unable to load Sky screen %u",
                     (unsigned)screen_file);
        goto out;
    }

    if (current_screen.size >= SCUMMVM_SURFACE_W * SCUMMVM_SURFACE_H) {
        rb->memset(current_screen.data + SCUMMVM_SURFACE_W * SKY_GAME_SCREEN_H,
                   0,
                   SCUMMVM_SURFACE_W *
                   (SCUMMVM_SURFACE_H - SKY_GAME_SCREEN_H));
    }

    if (!load_resource_from_open_files(dsk_fd, dsk_size, dnr, entries,
                                       palette_file,
                                       &current_palette)) {
        rb->snprintf(status, status_size, "Unable to load Sky palette %u",
                     (unsigned)palette_file);
        goto out;
    }

    rb->snprintf(status, status_size, "Sky screen %u loaded",
                 (unsigned)screen_file);
    ok = true;

out:
    close_sky_tables(dnr_fd, dsk_fd, dnr);
    if (!ok)
        scummvm_sky_loader_reset();
    return ok;
}

bool scummvm_sky_loader_load_resource(const struct scummvm_target *target,
                                      uint16_t file_nr,
                                      struct scummvm_sky_resource *resource,
                                      char *status,
                                      size_t status_size)
{
    int dnr_fd = -1;
    int dsk_fd = -1;
    uint8_t *dnr = NULL;
    uint32_t entries;
    long dsk_size;
    bool ok = false;

    rb->memset(resource, 0, sizeof(*resource));

    if (!open_sky_tables(target, &dnr_fd, &dsk_fd, &dnr, &entries, &dsk_size,
                         status, status_size))
        goto out;

    if (!load_resource_from_open_files(dsk_fd, dsk_size, dnr, entries,
                                       file_nr, resource)) {
        rb->snprintf(status, status_size, "Unable to load Sky file %u",
                     (unsigned)file_nr);
        goto out;
    }

    rb->snprintf(status, status_size, "Sky file %u loaded (%lu bytes)",
                 (unsigned)file_nr, (unsigned long)resource->size);
    ok = true;

out:
    close_sky_tables(dnr_fd, dsk_fd, dnr);
    if (!ok)
        scummvm_sky_loader_release_resource(resource);
    return ok;
}

bool scummvm_sky_loader_load_sequence(const struct scummvm_target *target,
                                      uint16_t sequence_file,
                                      char *status,
                                      size_t status_size)
{
    int dnr_fd = -1;
    int dsk_fd = -1;
    uint8_t *dnr = NULL;
    uint32_t entries;
    long dsk_size;
    bool ok = false;

    scummvm_sky_loader_release_resource(&current_sequence);
    current_sequence_pos = 0;
    current_sequence_frames_left = 0;

    if (!current_screen.data) {
        set_status(status, status_size, "Sky sequence needs a screen");
        return false;
    }

    if (!open_sky_tables(target, &dnr_fd, &dsk_fd, &dnr, &entries, &dsk_size,
                         status, status_size))
        goto out;

    if (!load_resource_from_open_files(dsk_fd, dsk_size, dnr, entries,
                                       sequence_file,
                                       &current_sequence)) {
        rb->snprintf(status, status_size, "Unable to load Sky sequence %u",
                     (unsigned)sequence_file);
        goto out;
    }

    if (current_sequence.size < 2) {
        set_status(status, status_size, "Sky sequence is too short");
        goto out;
    }

    current_sequence_frames_left = current_sequence.data[0];
    current_sequence_pos = 1;
    if (current_sequence_frames_left == 0) {
        set_status(status, status_size, "Sky sequence has no frames");
        goto out;
    }

    rb->snprintf(status, status_size, "Sky sequence %u loaded",
                 (unsigned)sequence_file);
    ok = true;

out:
    close_sky_tables(dnr_fd, dsk_fd, dnr);
    if (!ok) {
        scummvm_sky_loader_release_resource(&current_sequence);
        current_sequence_pos = 0;
        current_sequence_frames_left = 0;
    }
    return ok;
}

bool scummvm_sky_loader_sequence_running(void)
{
    return current_sequence.data && current_sequence_frames_left > 0;
}

bool scummvm_sky_loader_step_sequence(void)
{
    uint32_t screen_pos = 0;
    uint32_t screen_size;

    if (!scummvm_sky_loader_sequence_running())
        return false;

    if (!current_screen.data)
        return false;

    screen_size = SCUMMVM_SURFACE_W * SKY_GAME_SCREEN_H;
    if (screen_size > current_screen.size)
        screen_size = current_screen.size;

    while (screen_pos < screen_size) {
        uint8_t nr_to_skip;
        uint8_t nr_to_do;

        do {
            if (current_sequence_pos >= current_sequence.size)
                goto bad_sequence;
            nr_to_skip = current_sequence.data[current_sequence_pos++];
            screen_pos += nr_to_skip;
            if (screen_pos > screen_size)
                goto bad_sequence;
        } while (nr_to_skip == 0xff);

        do {
            if (current_sequence_pos >= current_sequence.size)
                goto bad_sequence;
            nr_to_do = current_sequence.data[current_sequence_pos++];
            if (screen_pos + nr_to_do > screen_size ||
                current_sequence_pos + nr_to_do > current_sequence.size)
                goto bad_sequence;

            rb->memcpy(current_screen.data + screen_pos,
                       current_sequence.data + current_sequence_pos,
                       nr_to_do);
            current_sequence_pos += nr_to_do;
            screen_pos += nr_to_do;
        } while (nr_to_do == 0xff);
    }

    current_sequence_frames_left--;
    if (current_sequence_frames_left == 0) {
        scummvm_sky_loader_release_resource(&current_sequence);
        current_sequence_pos = 0;
    }

    return true;

bad_sequence:
    scummvm_sky_loader_release_resource(&current_sequence);
    current_sequence_pos = 0;
    current_sequence_frames_left = 0;
    return false;
}

bool scummvm_sky_loader_bootstrap_section0(const struct scummvm_target *target,
                                           char *status,
                                           size_t status_size)
{
    static const uint16_t fixed_items[] = {
        49, 50, 73, 262, 36, 263, 264, 265, 266, 267, 269, 271, 272
    };
    int dnr_fd = -1;
    int dsk_fd = -1;
    uint8_t *dnr = NULL;
    uint32_t entries;
    long dsk_size;
    uint32_t fixed_count = 0;
    uint32_t grid_count = 0;
    uint32_t i;
    struct scummvm_sky_cpt_info cpt_info;
    bool ok = false;

    if (!open_sky_tables(target, &dnr_fd, &dsk_fd, &dnr, &entries, &dsk_size,
                         status, status_size))
        goto out;

    for (i = 0; i < ARRAYLEN(fixed_items); i++) {
        struct scummvm_sky_resource resource;

        rb->memset(&resource, 0, sizeof(resource));
        if (!load_resource_from_open_files(dsk_fd, dsk_size, dnr, entries,
                                           fixed_items[i], &resource)) {
            rb->snprintf(status, status_size,
                         "Missing Sky fixed item %u",
                         (unsigned)fixed_items[i]);
            scummvm_sky_loader_release_resource(&resource);
            goto out;
        }

        fixed_count++;
        scummvm_sky_loader_release_resource(&resource);
    }

    for (i = 0; i < SKY_GRID_COUNT; i++) {
        struct scummvm_sky_resource resource;
        uint16_t file_nr = (uint16_t)(SKY_GRID_FILE_START + i);

        rb->memset(&resource, 0, sizeof(resource));
        if (!load_resource_from_open_files(dsk_fd, dsk_size, dnr, entries,
                                           file_nr, &resource)) {
            rb->snprintf(status, status_size, "Missing Sky grid %u",
                         (unsigned)file_nr);
            scummvm_sky_loader_release_resource(&resource);
            goto out;
        }

        grid_count++;
        scummvm_sky_loader_release_resource(&resource);
    }

    if (!scummvm_sky_cpt_load(target, &cpt_info, status, status_size))
        goto out;

    rb->snprintf(status, status_size,
                 "Sky section 0 bootstrap: %lu fixed, %lu grids, %lu compacts",
                 (unsigned long)fixed_count,
                 (unsigned long)grid_count,
                 (unsigned long)cpt_info.compact_entries);
    ok = true;

out:
    close_sky_tables(dnr_fd, dsk_fd, dnr);
    return ok;
}

bool scummvm_sky_loader_render_current(struct scummvm_video *video)
{
    fb_data palette[256];
    uint32_t screen_pixels;
    uint32_t i;

    if (!current_screen.data || !current_palette.data)
        return false;

    if (current_palette.size < 256 * 3)
        return false;

    for (i = 0; i < 256; i++) {
        uint8_t r = current_palette.data[i * 3 + 0];
        uint8_t g = current_palette.data[i * 3 + 1];
        uint8_t b = current_palette.data[i * 3 + 2];
        r = (uint8_t)((r << 2) | (r >> 4));
        g = (uint8_t)((g << 2) | (g >> 4));
        b = (uint8_t)((b << 2) | (b >> 4));
        palette[i] = LCD_RGBPACK(r, g, b);
    }

    screen_pixels = current_screen.size;
    if (screen_pixels > (uint32_t)(video->width * video->height))
        screen_pixels = video->width * video->height;

    for (i = 0; i < screen_pixels; i++)
        video->pixels[i] = palette[current_screen.data[i]];

    for (i = screen_pixels; i < (uint32_t)(video->width * video->height); i++)
        video->pixels[i] = LCD_RGBPACK(0, 0, 0);

    return true;
}
