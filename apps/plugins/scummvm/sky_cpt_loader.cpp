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
#include "sky_cpt_loader.h"
#include "rbfile.h"

#define SKY_CPT_FILE "sky.cpt"
#define SKY_CPT_SIZE 419427
#define SKY_CPT_MAX_LISTS 64
#define SKY_CPT_MAX_ENTRIES 8192
#define SKY_CPT_MAX_SOURCE_WORDS 262144
#define SKY_CPT_MAX_RAW_WORDS 262144
#define SKY_CPT_MAX_ASCII_BYTES 262144
#define SKY_CPT_MAX_TYPE 7

struct sky_cpt_entry {
    uint32_t raw_offset;
    uint32_t name_offset;
    uint16_t size;
    uint16_t type;
};

struct sky_cpt_table {
    uint16_t data_lists;
    uint16_t *list_lens;
    uint32_t *list_offsets;
    struct sky_cpt_entry *entries;
    uint16_t *raw;
    char *ascii;
    uint16_t *save_ids;
    uint32_t entry_count;
    uint32_t raw_words;
    uint32_t ascii_bytes;
    uint32_t save_id_count;
};

static struct sky_cpt_table current_cpt;

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool read_all(int fd, void *dst, size_t size)
{
    uint8_t *out = (uint8_t *)dst;
    size_t done = 0;

    while (done < size) {
        ssize_t got = rb->read(fd, out + done, size - done);
        if (got <= 0)
            return false;
        done += (size_t)got;
    }

    return true;
}

static bool open_cpt(const struct scummvm_target *target,
                     char *path,
                     size_t path_size,
                     int *fd)
{
    if (scummvm_make_path(path, path_size, target->path, SKY_CPT_FILE)) {
        *fd = rb->open(path, O_RDONLY);
        if (*fd >= 0)
            return true;
    }

    if (scummvm_make_path(path, path_size, SCUMMVM_ENGINE_DATA_DIR,
                          SKY_CPT_FILE)) {
        *fd = rb->open(path, O_RDONLY);
        if (*fd >= 0)
            return true;
    }

    return false;
}

static bool read_u16_fd(int fd, uint16_t *value)
{
    uint8_t buf[2];

    if (!read_all(fd, buf, sizeof(buf)))
        return false;

    *value = read_le16(buf);
    return true;
}

static bool read_u32_fd(int fd, uint32_t *value)
{
    uint8_t buf[4];

    if (!read_all(fd, buf, sizeof(buf)))
        return false;

    *value = read_le32(buf);
    return true;
}

static bool src_read_u16(const uint8_t *src,
                         uint32_t src_words,
                         uint32_t *src_pos,
                         uint16_t *value)
{
    if (*src_pos >= src_words)
        return false;

    *value = read_le16(src + *src_pos * 2);
    (*src_pos)++;
    return true;
}

static bool ascii_skip_name(const uint8_t *ascii,
                            uint32_t ascii_bytes,
                            uint32_t *ascii_pos)
{
    while (*ascii_pos < ascii_bytes) {
        if (ascii[*ascii_pos] == '\0') {
            (*ascii_pos)++;
            return true;
        }
        (*ascii_pos)++;
    }

    return false;
}

static bool ascii_assign_name(const uint8_t *ascii,
                              uint32_t ascii_bytes,
                              uint32_t *ascii_pos,
                              uint32_t *name_offset)
{
    *name_offset = *ascii_pos;
    return ascii_skip_name(ascii, ascii_bytes, ascii_pos);
}

void scummvm_sky_cpt_unload(void)
{
    if (current_cpt.save_ids)
        delete[] current_cpt.save_ids;
    if (current_cpt.ascii)
        delete[] current_cpt.ascii;
    if (current_cpt.raw)
        delete[] current_cpt.raw;
    if (current_cpt.entries)
        delete[] current_cpt.entries;
    if (current_cpt.list_offsets)
        delete[] current_cpt.list_offsets;
    if (current_cpt.list_lens)
        delete[] current_cpt.list_lens;

    rb->memset(&current_cpt, 0, sizeof(current_cpt));
}

const uint16_t *scummvm_sky_cpt_fetch(uint16_t cpt_id,
                                      uint16_t *size,
                                      uint16_t *type,
                                      const char **name)
{
    uint16_t list = cpt_id >> 12;
    uint16_t index = cpt_id & 0x0fff;
    struct sky_cpt_entry *entry;

    if (list >= current_cpt.data_lists ||
        index >= current_cpt.list_lens[list])
        return NULL;

    entry = &current_cpt.entries[current_cpt.list_offsets[list] + index];
    if (entry->size == 0)
        return NULL;

    if (size)
        *size = entry->size;
    if (type)
        *type = entry->type;
    if (name)
        *name = current_cpt.ascii + entry->name_offset;

    return current_cpt.raw + entry->raw_offset;
}

bool scummvm_sky_cpt_load(const struct scummvm_target *target,
                          struct scummvm_sky_cpt_info *info,
                          char *status,
                          size_t status_size)
{
    int fd = -1;
    char path[MAX_PATH];
    uint16_t version;
    uint16_t num_lists;
    uint16_t num_dlincs;
    uint16_t num_diffs;
    uint16_t diff_words;
    uint16_t num_save_ids;
    uint16_t *list_lens = NULL;
    uint32_t *list_offsets = NULL;
    struct sky_cpt_entry *entries = NULL;
    uint16_t *raw = NULL;
    uint16_t *save_ids = NULL;
    uint8_t *src = NULL;
    uint8_t *ascii = NULL;
    uint32_t src_pos = 0;
    uint32_t ascii_pos = 0;
    uint32_t compact_words = 0;
    uint32_t i;
    bool ok = false;

    rb->memset(info, 0, sizeof(*info));
    scummvm_sky_cpt_unload();

    if (!open_cpt(target, path, sizeof(path), &fd)) {
        rb->snprintf(status, status_size,
                     "Need sky.cpt in game dir or %s",
                     SCUMMVM_ENGINE_DATA_DIR);
        return false;
    }

    if (rb->filesize(fd) != SKY_CPT_SIZE) {
        rb->snprintf(status, status_size, "sky.cpt has wrong size: %ld",
                     rb->filesize(fd));
        goto out;
    }

    if (!read_u16_fd(fd, &version)) {
        rb->strlcpy(status, "Unable to read sky.cpt version", status_size);
        goto out;
    }

    if (version != 0) {
        rb->snprintf(status, status_size,
                     "Unsupported sky.cpt version %u", (unsigned)version);
        goto out;
    }

    if (!read_u16_fd(fd, &num_lists) ||
        num_lists == 0 || num_lists > SKY_CPT_MAX_LISTS) {
        rb->strlcpy(status, "Invalid sky.cpt list count", status_size);
        goto out;
    }

    list_lens = new uint16_t[num_lists];
    list_offsets = new uint32_t[num_lists];
    if (!list_lens || !list_offsets) {
        rb->strlcpy(status, "Not enough memory for sky.cpt lists",
                    status_size);
        goto out;
    }

    for (i = 0; i < num_lists; i++) {
        if (!read_u16_fd(fd, &list_lens[i])) {
            rb->strlcpy(status, "Unable to read sky.cpt lists", status_size);
            goto out;
        }

        list_offsets[i] = info->data_entries;
        info->data_entries += list_lens[i];
        if (info->data_entries > SKY_CPT_MAX_ENTRIES) {
            rb->strlcpy(status, "sky.cpt has too many entries", status_size);
            goto out;
        }
    }

    if (!read_u32_fd(fd, &info->raw_words) ||
        !read_u32_fd(fd, &info->src_words) ||
        !read_u32_fd(fd, &info->ascii_bytes)) {
        rb->strlcpy(status, "Unable to read sky.cpt section sizes",
                    status_size);
        goto out;
    }

    if (info->raw_words == 0 ||
        info->raw_words > SKY_CPT_MAX_RAW_WORDS ||
        info->src_words == 0 ||
        info->src_words > SKY_CPT_MAX_SOURCE_WORDS ||
        info->ascii_bytes == 0 ||
        info->ascii_bytes > SKY_CPT_MAX_ASCII_BYTES) {
        rb->strlcpy(status, "Invalid sky.cpt section sizes", status_size);
        goto out;
    }

    entries = new struct sky_cpt_entry[info->data_entries];
    raw = new uint16_t[info->raw_words];
    src = new uint8_t[info->src_words * 2];
    ascii = new uint8_t[info->ascii_bytes];
    if (!entries || !raw || !src || !ascii) {
        rb->strlcpy(status, "Not enough memory for sky.cpt sections",
                    status_size);
        goto out;
    }
    rb->memset(entries, 0, sizeof(*entries) * info->data_entries);

    if (!read_all(fd, src, info->src_words * 2) ||
        !read_all(fd, ascii, info->ascii_bytes)) {
        rb->strlcpy(status, "Unable to read sky.cpt sections", status_size);
        goto out;
    }

    for (i = 0; i < num_lists; i++) {
        uint32_t entry;

        for (entry = 0; entry < list_lens[i]; entry++) {
            struct sky_cpt_entry *cpt_entry =
                &entries[list_offsets[i] + entry];
            uint16_t cpt_size;
            uint16_t cpt_type;
            uint32_t word;

            if (!src_read_u16(src, info->src_words, &src_pos, &cpt_size)) {
                rb->strlcpy(status, "sky.cpt source table is truncated",
                            status_size);
                goto out;
            }

            if (cpt_size == 0)
                continue;

            if (!src_read_u16(src, info->src_words, &src_pos, &cpt_type) ||
                cpt_type == 0 || cpt_type > SKY_CPT_MAX_TYPE ||
                src_pos + cpt_size > info->src_words ||
                compact_words + cpt_size > info->raw_words) {
                rb->strlcpy(status, "sky.cpt compact data is truncated",
                            status_size);
                goto out;
            }

            cpt_entry->raw_offset = compact_words;
            cpt_entry->size = cpt_size;
            cpt_entry->type = cpt_type;
            info->compact_entries++;
            for (word = 0; word < cpt_size; word++)
                raw[compact_words + word] =
                    read_le16(src + (src_pos + word) * 2);
            src_pos += cpt_size;
            compact_words += cpt_size;

            if (!ascii_assign_name(ascii, info->ascii_bytes, &ascii_pos,
                                   &cpt_entry->name_offset)) {
                rb->strlcpy(status, "sky.cpt name table is truncated",
                            status_size);
                goto out;
            }
        }
    }

    if (!read_u16_fd(fd, &num_dlincs)) {
        rb->strlcpy(status, "Unable to read sky.cpt dlinc count",
                    status_size);
        goto out;
    }

    info->dlinc_entries = num_dlincs;
    for (i = 0; i < num_dlincs; i++) {
        uint16_t dlinc_id;
        uint16_t dest_id;
        uint16_t dlinc_list;
        uint16_t dlinc_index;
        uint16_t dest_list;
        uint16_t dest_index;

        if (!read_u16_fd(fd, &dlinc_id) || !read_u16_fd(fd, &dest_id)) {
            rb->strlcpy(status, "sky.cpt dlinc table is truncated",
                        status_size);
            goto out;
        }

        dlinc_list = dlinc_id >> 12;
        dlinc_index = dlinc_id & 0x0fff;
        dest_list = dest_id >> 12;
        dest_index = dest_id & 0x0fff;
        if (dlinc_list >= num_lists || dest_list >= num_lists ||
            dlinc_index >= list_lens[dlinc_list] ||
            dest_index >= list_lens[dest_list] ||
            entries[list_offsets[dest_list] + dest_index].size == 0) {
            rb->strlcpy(status, "sky.cpt dlinc id is out of range",
                        status_size);
            goto out;
        }

        entries[list_offsets[dlinc_list] + dlinc_index] =
            entries[list_offsets[dest_list] + dest_index];
        if (!ascii_assign_name(
                ascii, info->ascii_bytes, &ascii_pos,
                &entries[list_offsets[dlinc_list] + dlinc_index].name_offset)) {
            rb->strlcpy(status, "sky.cpt dlinc names are truncated",
                        status_size);
            goto out;
        }
    }

    if (!read_u16_fd(fd, &num_diffs) || !read_u16_fd(fd, &diff_words)) {
        rb->strlcpy(status, "Unable to read sky.cpt diff header",
                    status_size);
        goto out;
    }

    info->diff_entries = num_diffs;
    if (rb->lseek(fd, diff_words * 2, SEEK_CUR) < 0) {
        rb->strlcpy(status, "Invalid sky.cpt diff table", status_size);
        goto out;
    }

    if (!read_u16_fd(fd, &num_save_ids)) {
        rb->strlcpy(status, "Unable to read sky.cpt save id count",
                    status_size);
        goto out;
    }

    info->save_ids = num_save_ids;
    save_ids = new uint16_t[num_save_ids];
    if (!save_ids) {
        rb->strlcpy(status, "Not enough memory for sky.cpt save ids",
                    status_size);
        goto out;
    }

    for (i = 0; i < num_save_ids; i++) {
        if (!read_u16_fd(fd, &save_ids[i])) {
            rb->strlcpy(status, "sky.cpt save id table is truncated",
                        status_size);
            goto out;
        }
    }

    if (rb->lseek(fd, 0, SEEK_CUR) < 0) {
        rb->strlcpy(status, "Invalid sky.cpt save id table", status_size);
        goto out;
    }

    if (rb->lseek(fd, 0, SEEK_CUR) > rb->filesize(fd)) {
        rb->strlcpy(status, "sky.cpt save id table exceeds file size",
                    status_size);
        goto out;
    }

    info->data_lists = num_lists;
    rb->strlcpy(info->source_path, path, sizeof(info->source_path));
    current_cpt.data_lists = num_lists;
    current_cpt.list_lens = list_lens;
    current_cpt.list_offsets = list_offsets;
    current_cpt.entries = entries;
    current_cpt.raw = raw;
    current_cpt.ascii = (char *)ascii;
    current_cpt.save_ids = save_ids;
    current_cpt.entry_count = info->data_entries;
    current_cpt.raw_words = info->raw_words;
    current_cpt.ascii_bytes = info->ascii_bytes;
    current_cpt.save_id_count = num_save_ids;
    list_lens = NULL;
    list_offsets = NULL;
    entries = NULL;
    raw = NULL;
    ascii = NULL;
    save_ids = NULL;
    rb->snprintf(status, status_size,
                 "sky.cpt loaded: %u lists, %lu compacts, %lu save ids",
                 (unsigned)info->data_lists,
                 (unsigned long)info->compact_entries,
                 (unsigned long)info->save_ids);
    ok = true;

out:
    if (ascii)
        delete[] ascii;
    if (src)
        delete[] src;
    if (save_ids)
        delete[] save_ids;
    if (raw)
        delete[] raw;
    if (entries)
        delete[] entries;
    if (list_offsets)
        delete[] list_offsets;
    if (list_lens)
        delete[] list_lens;
    if (fd >= 0)
        rb->close(fd);

    return ok;
}
