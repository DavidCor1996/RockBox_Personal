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

#include "queen_loader.h"
#include "rbfile.h"

struct queen_version {
    long data_size;
    const char *version;
    const char *variant;
    uint8_t table_version;
    uint32_t table_offset;
};

struct queen_resource_entry {
    char filename[13];
    uint8_t bundle;
    uint32_t offset;
    uint32_t size;
};

static const struct queen_version queen_versions[] = {
    { 351775, "aEM10", "Amiga floppy", 2, 0x00103f1e },
    { 344575, "aEM10", "Amiga floppy", 2, 0x00103f1e },
    { 22677657, "PEM10", "DOS floppy", 1, 0x00000008 },
    { 22157304, "PFM10", "DOS floppy", 1, 0x0002cd93 },
    { 22240013, "PGM10", "DOS floppy", 1, 0x00059aca },
    { 190787021, "CEM10", "DOS CD", 1, 0x0000584e },
    { 186689095, "CFM10", "DOS CD", 1, 0x00032585 },
    { 217648975, "CGM10", "DOS CD", 1, 0x0005f2a7 },
    { 190705558, "CHM10", "DOS CD", 1, 0x000da981 },
    { 3732177, "PE100", "DOS demo", 1, 0x00102b7f },
    { 3735447, "PE100", "DOS demo", 1, 0x00102b7f },
    { 3724538, "PE100", "DOS demo", 1, 0x00101ec6 },
    { 1915913, "PEint", "DOS interview", 1, 0x00103838 },
    { 1889658, "PEint", "DOS interview", 1, 0x00103838 },
    { 563335, "CE101", "Amiga demo", 2, 0x00107d8d },
    { 597032, "PE100", "Amiga interview", 2, 0x001086d4 },
    { 0, NULL, NULL, 0, 0 }
};

static const struct queen_resource_entry pem10_builtin_entries[] = {
    { "QUEEN.JAS", 1, 0x00d1247e, 0x0001371a },
    { "QUEEN2.JAS", 1, 0x00d25b98, 0x00008c00 },
    { "", 0, 0, 0 }
};

static uint16_t read_be16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint16_t read_le16(const unsigned char *p)
{
    return ((uint16_t)p[1] << 8) | p[0];
}

static uint32_t read_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static char upper_ascii(char c)
{
    if (c >= 'a' && c <= 'z')
        return (char)(c - 'a' + 'A');

    return c;
}

static bool name_equals_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (upper_ascii(*a) != upper_ascii(*b))
            return false;
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static bool make_queen_path(const char *dir, const char *file,
                            char *out, size_t out_size)
{
    return scummvm_make_path(out, out_size, dir, file);
}

static bool open_resource_file(const struct scummvm_target *target,
                               char *path,
                               size_t path_size,
                               int *fd)
{
    if (make_queen_path(target->path, "queen.1c", path, path_size)) {
        *fd = rb->open(path, O_RDONLY);
        if (*fd >= 0)
            return true;
    }

    if (make_queen_path(target->path, "queen.1", path, path_size)) {
        *fd = rb->open(path, O_RDONLY);
        if (*fd >= 0)
            return true;
    }

    return false;
}

bool scummvm_queen_loader_probe(const struct scummvm_target *target,
                                char *status,
                                size_t status_size)
{
    char path[MAX_PATH];
    unsigned char header[16];
    int fd = -1;
    long size;
    const struct queen_version *version;

    if (!open_resource_file(target, path, sizeof(path), &fd)) {
        rb->strlcpy(status, "Unable to open queen.1/queen.1c", status_size);
        return false;
    }

    size = rb->filesize(fd);
    if (size < 0) {
        rb->close(fd);
        rb->strlcpy(status, "Unable to size Queen resource", status_size);
        return false;
    }

    if (rb->read(fd, header, sizeof(header)) == (ssize_t)sizeof(header) &&
        header[0] == 'Q' && header[1] == 'T' &&
        header[2] == 'B' && header[3] == 'L') {
        char version_str[7];
        rb->memcpy(version_str, header + 4, 6);
        version_str[6] = '\0';
        rb->snprintf(status, status_size,
                     "Queen rebuilt resource %s (%ld bytes)",
                     version_str, size);
        rb->close(fd);
        return true;
    }

    rb->close(fd);

    for (version = queen_versions; version->version; version++) {
        if (version->data_size == size) {
            rb->snprintf(status, status_size,
                         "Queen %s %s (%ld bytes)",
                         version->version, version->variant, size);
            return true;
        }
    }

    rb->snprintf(status, status_size,
                 "Queen resource opened, unknown size %ld", size);
    return true;
}

static const struct queen_version *version_from_size(long size)
{
    const struct queen_version *version;

    for (version = queen_versions; version->version; version++) {
        if (version->data_size == size)
            return version;
    }

    return NULL;
}

static bool count_table_entries_in_fd(int fd,
                                      uint32_t offset,
                                      uint32_t *entries,
                                      char *status,
                                      size_t status_size)
{
    unsigned char buf[2];

    if (rb->lseek(fd, offset, SEEK_SET) != (off_t)offset ||
        rb->read(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        rb->strlcpy(status, "Queen table offset is unreadable",
                    status_size);
        return false;
    }

    *entries = read_be16(buf);
    if (*entries == 0 || *entries > 4096) {
        rb->snprintf(status, status_size,
                     "Queen table has invalid count %lu",
                     (unsigned long)*entries);
        return false;
    }

    return true;
}

static bool read_table_entry(int fd,
                             uint32_t table_offset,
                             uint32_t index,
                             struct queen_resource_entry *entry)
{
    unsigned char buf[21];
    uint32_t offset = table_offset + 2 + index * sizeof(buf);

    if (rb->lseek(fd, offset, SEEK_SET) != (off_t)offset ||
        rb->read(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf))
        return false;

    rb->memcpy(entry->filename, buf, 12);
    entry->filename[12] = '\0';
    entry->bundle = buf[12];
    entry->offset = read_be32(buf + 13);
    entry->size = read_be32(buf + 17);
    return true;
}

static bool find_table_entry_in_fd(int fd,
                                   uint32_t table_offset,
                                   const char *filename,
                                   struct queen_resource_entry *entry,
                                   uint32_t *entries,
                                   char *status,
                                   size_t status_size)
{
    uint32_t i;

    if (!count_table_entries_in_fd(fd, table_offset, entries,
                                   status, status_size))
        return false;

    for (i = 0; i < *entries; i++) {
        if (!read_table_entry(fd, table_offset, i, entry)) {
            rb->strlcpy(status, "Queen table entry is unreadable",
                        status_size);
            return false;
        }
        if (name_equals_ci(entry->filename, filename))
            return true;
    }

    rb->snprintf(status, status_size, "Queen resource missing %s",
                 filename);
    return false;
}

static bool count_rebuilt_table(int fd,
                                uint32_t *entries,
                                char *status,
                                size_t status_size)
{
    unsigned char header[16];

    if (rb->lseek(fd, 0, SEEK_SET) != 0 ||
        rb->read(fd, header, 13) != 13 ||
        header[0] != 'Q' || header[1] != 'T' ||
        header[2] != 'B' || header[3] != 'L')
        return false;

    return count_table_entries_in_fd(fd, 13, entries, status, status_size);
}

static bool count_external_table(const struct scummvm_target *target,
                                 const struct queen_version *version,
                                 uint32_t *entries,
                                 char *status,
                                 size_t status_size)
{
    char path[MAX_PATH];
    unsigned char header[8];
    int fd;
    uint32_t table_version;
    bool ok;

    if (!make_queen_path(target->path, "queen.tbl", path, sizeof(path))) {
        rb->strlcpy(status, "Queen table path is too long", status_size);
        return false;
    }

    fd = rb->open(path, O_RDONLY);
    if (fd < 0) {
        rb->strlcpy(status, "Queen needs queen.tbl for this version",
                    status_size);
        return false;
    }

    if (rb->read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
        header[0] != 'Q' || header[1] != 'T' ||
        header[2] != 'B' || header[3] != 'L') {
        rb->close(fd);
        rb->strlcpy(status, "queen.tbl has no QTBL header", status_size);
        return false;
    }

    table_version = read_be32(header + 4);
    if (table_version < version->table_version) {
        rb->close(fd);
        rb->snprintf(status, status_size,
                     "queen.tbl v%lu too old for v%u",
                     (unsigned long)table_version,
                     (unsigned)version->table_version);
        return false;
    }

    ok = count_table_entries_in_fd(fd, version->table_offset, entries,
                                   status, status_size);
    rb->close(fd);
    return ok;
}

bool scummvm_queen_loader_table_info(const struct scummvm_target *target,
                                     uint32_t *entries,
                                     char *status,
                                     size_t status_size)
{
    char path[MAX_PATH];
    int fd = -1;
    long size;
    const struct queen_version *version;

    *entries = 0;
    if (!open_resource_file(target, path, sizeof(path), &fd)) {
        rb->strlcpy(status, "Unable to open queen.1/queen.1c", status_size);
        return false;
    }

    if (count_rebuilt_table(fd, entries, status, status_size)) {
        rb->snprintf(status, status_size,
                     "Queen rebuilt table: %lu resources",
                     (unsigned long)*entries);
        rb->close(fd);
        return true;
    }

    size = rb->filesize(fd);
    rb->close(fd);
    version = version_from_size(size);
    if (!version) {
        rb->snprintf(status, status_size,
                     "Queen table unknown for size %ld", size);
        return false;
    }

    if (!rb->strcmp(version->version, "PEM10")) {
        *entries = 1076;
        rb->snprintf(status, status_size,
                     "Queen built-in PEM10 table: %lu resources",
                     (unsigned long)*entries);
        return true;
    }

    return count_external_table(target, version, entries,
                                status, status_size);
}

static uint32_t jas_version_offset(const struct queen_version *version)
{
    if (!version)
        return 0;
    if (!rb->strcmp(version->variant, "DOS demo"))
        return 0x119a8;
    if (!rb->strcmp(version->variant, "DOS interview"))
        return 0x0cf8;
    if (version->version[0] == 'P' || version->version[0] == 'C')
        return 0x12484;
    return 0;
}

static bool open_bundle_file(const struct scummvm_target *target,
                             uint8_t bundle,
                             int *fd)
{
    char name[16];
    char path[MAX_PATH];

    if (bundle <= 1)
        return open_resource_file(target, path, sizeof(path), fd);

    rb->snprintf(name, sizeof(name), "queen.%u", (unsigned)bundle);
    if (!make_queen_path(target->path, name, path, sizeof(path)))
        return false;

    *fd = rb->open(path, O_RDONLY);
    return *fd >= 0;
}

static bool verify_jas_entry(const struct scummvm_target *target,
                             const struct queen_version *version,
                             const struct queen_resource_entry *entry,
                             char *status,
                             size_t status_size)
{
    uint32_t jas_offset = jas_version_offset(version);
    char actual[7];
    size_t expected_len;
    int fd = -1;

    if (jas_offset == 0) {
        rb->snprintf(status, status_size,
                     "Queen JAS present: %lu bytes",
                     (unsigned long)entry->size);
        return true;
    }

    if (entry->size <= jas_offset + 6) {
        rb->strlcpy(status, "Queen JAS is too small", status_size);
        return false;
    }

    if (!open_bundle_file(target, entry->bundle, &fd)) {
        rb->snprintf(status, status_size,
                     "Queen bundle %u is missing",
                     (unsigned)entry->bundle);
        return false;
    }

    if (rb->lseek(fd, entry->offset + jas_offset, SEEK_SET) <
            (off_t)(entry->offset + jas_offset) ||
        rb->read(fd, actual, 6) != 6) {
        rb->close(fd);
        rb->strlcpy(status, "Queen JAS version unreadable", status_size);
        return false;
    }
    rb->close(fd);

    actual[6] = '\0';
    expected_len = rb->strlen(version->version);
    if (expected_len > 6)
        expected_len = 6;
    if (rb->strncmp(actual, version->version, expected_len)) {
        rb->snprintf(status, status_size,
                     "Queen JAS version %.6s != %s",
                     actual,
                     version->version);
        return false;
    }

    rb->snprintf(status, status_size,
                 "Queen JAS verified: %.6s",
                 actual);
    return true;
}

static bool find_rebuilt_jas(int fd,
                             struct queen_resource_entry *entry,
                             uint32_t *entries,
                             char *status,
                             size_t status_size)
{
    unsigned char header[13];

    if (rb->lseek(fd, 0, SEEK_SET) != 0 ||
        rb->read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
        header[0] != 'Q' || header[1] != 'T' ||
        header[2] != 'B' || header[3] != 'L')
        return false;

    return find_table_entry_in_fd(fd, 13, "QUEEN.JAS", entry, entries,
                                  status, status_size);
}

static bool find_rebuilt_entry(int fd,
                               const char *filename,
                               struct queen_resource_entry *entry,
                               uint32_t *entries,
                               char *status,
                               size_t status_size)
{
    unsigned char header[13];

    if (rb->lseek(fd, 0, SEEK_SET) != 0 ||
        rb->read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
        header[0] != 'Q' || header[1] != 'T' ||
        header[2] != 'B' || header[3] != 'L')
        return false;

    return find_table_entry_in_fd(fd, 13, filename, entry, entries,
                                  status, status_size);
}

static bool find_external_jas(const struct scummvm_target *target,
                              const struct queen_version *version,
                              struct queen_resource_entry *entry,
                              uint32_t *entries,
                              char *status,
                              size_t status_size)
{
    char path[MAX_PATH];
    unsigned char header[8];
    int fd;
    uint32_t table_version;
    bool ok;

    if (!make_queen_path(target->path, "queen.tbl", path, sizeof(path))) {
        rb->strlcpy(status, "Queen table path is too long", status_size);
        return false;
    }

    fd = rb->open(path, O_RDONLY);
    if (fd < 0) {
        rb->strlcpy(status, "Queen needs queen.tbl for this version",
                    status_size);
        return false;
    }

    if (rb->read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
        header[0] != 'Q' || header[1] != 'T' ||
        header[2] != 'B' || header[3] != 'L') {
        rb->close(fd);
        rb->strlcpy(status, "queen.tbl has no QTBL header", status_size);
        return false;
    }

    table_version = read_be32(header + 4);
    if (table_version < version->table_version) {
        rb->close(fd);
        rb->snprintf(status, status_size,
                     "queen.tbl v%lu too old for v%u",
                     (unsigned long)table_version,
                     (unsigned)version->table_version);
        return false;
    }

    ok = find_table_entry_in_fd(fd, version->table_offset, "QUEEN.JAS",
                                entry, entries, status, status_size);
    rb->close(fd);
    return ok;
}

static bool find_external_entry(const struct scummvm_target *target,
                                const struct queen_version *version,
                                const char *filename,
                                struct queen_resource_entry *entry,
                                uint32_t *entries,
                                char *status,
                                size_t status_size)
{
    char path[MAX_PATH];
    unsigned char header[8];
    int fd;
    uint32_t table_version;
    bool ok;

    if (!make_queen_path(target->path, "queen.tbl", path, sizeof(path))) {
        rb->strlcpy(status, "Queen table path is too long", status_size);
        return false;
    }

    fd = rb->open(path, O_RDONLY);
    if (fd < 0) {
        rb->strlcpy(status, "Queen needs queen.tbl for this version",
                    status_size);
        return false;
    }

    if (rb->read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
        header[0] != 'Q' || header[1] != 'T' ||
        header[2] != 'B' || header[3] != 'L') {
        rb->close(fd);
        rb->strlcpy(status, "queen.tbl has no QTBL header", status_size);
        return false;
    }

    table_version = read_be32(header + 4);
    if (table_version < version->table_version) {
        rb->close(fd);
        rb->snprintf(status, status_size,
                     "queen.tbl v%lu too old for v%u",
                     (unsigned long)table_version,
                     (unsigned)version->table_version);
        return false;
    }

    ok = find_table_entry_in_fd(fd, version->table_offset, filename,
                                entry, entries, status, status_size);
    rb->close(fd);
    return ok;
}

bool scummvm_queen_loader_verify_jas(const struct scummvm_target *target,
                                     char *status,
                                     size_t status_size)
{
    struct queen_resource_entry entry;
    char path[MAX_PATH];
    int fd = -1;
    long size;
    uint32_t entries = 0;
    const struct queen_version *version;

    if (!open_resource_file(target, path, sizeof(path), &fd)) {
        rb->strlcpy(status, "Unable to open queen.1/queen.1c", status_size);
        return false;
    }

    if (find_rebuilt_jas(fd, &entry, &entries, status, status_size)) {
        rb->close(fd);
        rb->snprintf(status, status_size,
                     "Queen rebuilt JAS present: %lu resources",
                     (unsigned long)entries);
        return true;
    }

    size = rb->filesize(fd);
    rb->close(fd);
    version = version_from_size(size);
    if (!version) {
        rb->snprintf(status, status_size,
                     "Queen JAS unknown for size %ld", size);
        return false;
    }

    if (!rb->strcmp(version->version, "PEM10")) {
        rb->strlcpy(status,
                    "Queen PEM10 JAS covered by built-in table",
                    status_size);
        return true;
    }

    if (!find_external_jas(target, version, &entry, &entries,
                           status, status_size))
        return false;

    return verify_jas_entry(target, version, &entry,
                            status, status_size);
}

static bool queen_resource_entry_lookup(const struct scummvm_target *target,
                                        const char *filename,
                                        struct queen_resource_entry *entry,
                                        uint32_t *entries,
                                        char *status,
                                        size_t status_size)
{
    char path[MAX_PATH];
    int fd = -1;
    long size;
    const struct queen_version *version;

    *entries = 0;
    if (!open_resource_file(target, path, sizeof(path), &fd)) {
        rb->strlcpy(status, "Unable to open queen.1/queen.1c", status_size);
        return false;
    }

    if (find_rebuilt_entry(fd, filename, entry, entries,
                           status, status_size)) {
        rb->close(fd);
        return true;
    }

    size = rb->filesize(fd);
    rb->close(fd);
    version = version_from_size(size);
    if (!version) {
        rb->snprintf(status, status_size,
                     "Queen resource table unknown for size %ld", size);
        return false;
    }

    if (!rb->strcmp(version->version, "PEM10")) {
        const struct queen_resource_entry *pem_entry;
        char table_status[96];

        if (find_external_entry(target, version, filename, entry, entries,
                                table_status, sizeof(table_status)))
            return true;

        for (pem_entry = pem10_builtin_entries; pem_entry->filename[0];
             pem_entry++) {
            if (name_equals_ci(pem_entry->filename, filename)) {
                *entry = *pem_entry;
                *entries = 1076;
                return true;
            }
        }

        rb->snprintf(status, status_size,
                     "Queen PEM10 built-in entry missing %s",
                     filename);
        return false;
    }

    return find_external_entry(target, version, filename, entry, entries,
                               status, status_size);
}

bool scummvm_queen_loader_resource_info(const struct scummvm_target *target,
                                        const char *filename,
                                        struct scummvm_queen_resource_info *info,
                                        uint32_t *entries,
                                        char *status,
                                        size_t status_size)
{
    struct queen_resource_entry entry;

    if (!info || !filename || !entries) {
        rb->strlcpy(status, "Queen resource query is invalid",
                    status_size);
        return false;
    }

    if (!queen_resource_entry_lookup(target, filename, &entry, entries,
                                     status, status_size))
        return false;

    rb->memcpy(info->filename, entry.filename, sizeof(info->filename));
    info->bundle = entry.bundle;
    info->offset = entry.offset;
    info->size = entry.size;
    rb->snprintf(status, status_size,
                 "Queen resource %s: %lu bytes",
                 info->filename,
                 (unsigned long)info->size);
    return true;
}

bool scummvm_queen_loader_read_resource(const struct scummvm_target *target,
                                        const char *filename,
                                        uint32_t skip,
                                        void *dst,
                                        size_t dst_size,
                                        uint32_t *resource_size,
                                        char *status,
                                        size_t status_size)
{
    struct queen_resource_entry entry;
    uint32_t entries = 0;
    int fd = -1;
    size_t to_read;

    if (!dst || !resource_size) {
        rb->strlcpy(status, "Queen read target is invalid", status_size);
        return false;
    }

    if (!queen_resource_entry_lookup(target, filename, &entry, &entries,
                                     status, status_size))
        return false;

    *resource_size = entry.size;
    if (skip > entry.size) {
        rb->snprintf(status, status_size,
                     "Queen resource %s skip is out of range",
                     filename);
        return false;
    }

    to_read = entry.size - skip;
    if (to_read > dst_size)
        to_read = dst_size;

    if (!open_bundle_file(target, entry.bundle, &fd)) {
        rb->snprintf(status, status_size,
                     "Queen bundle %u is missing",
                     (unsigned)entry.bundle);
        return false;
    }

    if (rb->lseek(fd, entry.offset + skip, SEEK_SET) !=
            (off_t)(entry.offset + skip) ||
        rb->read(fd, dst, to_read) != (ssize_t)to_read) {
        rb->close(fd);
        rb->strlcpy(status, "Queen resource read failed", status_size);
        return false;
    }

    rb->close(fd);
    rb->snprintf(status, status_size,
                 "Queen loaded %lu/%lu bytes from %s",
                 (unsigned long)to_read,
                 (unsigned long)entry.size,
                 filename);
    return true;
}

static bool queen_jas_read_at(const struct scummvm_target *target,
                              uint32_t cursor,
                              unsigned char *dst,
                              size_t dst_size,
                              uint32_t *jas_size,
                              char *status,
                              size_t status_size)
{
    uint32_t resource_size = 0;

    if (!scummvm_queen_loader_read_resource(target, "QUEEN.JAS",
                                            20 + cursor, dst, dst_size,
                                            &resource_size, status,
                                            status_size))
        return false;

    if (resource_size < 20) {
        rb->strlcpy(status, "Queen JAS resource is too small",
                    status_size);
        return false;
    }

    *jas_size = resource_size - 20;
    if (cursor > *jas_size || dst_size > *jas_size - cursor) {
        rb->strlcpy(status, "Queen JAS read is out of range",
                    status_size);
        return false;
    }

    return true;
}

static bool queen_jas_read_u16(const struct scummvm_target *target,
                               uint32_t *cursor,
                               uint16_t *value,
                               uint32_t *jas_size,
                               char *status,
                               size_t status_size)
{
    unsigned char buf[2];

    if (!queen_jas_read_at(target, *cursor, buf, sizeof(buf), jas_size,
                           status, status_size))
        return false;

    *value = read_be16(buf);
    *cursor += 2;
    return true;
}

static bool queen_jas_skip(uint32_t *cursor,
                           uint32_t jas_size,
                           uint32_t bytes,
                           const char *section,
                           char *status,
                           size_t status_size)
{
    if (*cursor > jas_size || bytes > jas_size - *cursor) {
        rb->snprintf(status, status_size,
                     "Queen JAS %s section is truncated",
                     section);
        return false;
    }

    *cursor += bytes;
    return true;
}

static uint32_t queen_counted_bytes(uint16_t count, uint32_t entry_size)
{
    return (uint32_t)(count ? count : 1) * entry_size;
}

static int16_t read_be16s(const unsigned char *p)
{
    return (int16_t)read_be16(p);
}

static bool queen_jas_scan_grid(const struct scummvm_target *target,
                                uint32_t *cursor,
                                uint32_t *jas_size,
                                uint16_t rooms,
                                uint16_t objects,
                                uint32_t *object_box_offset,
                                char *status,
                                size_t status_size)
{
    uint16_t room;

    for (room = 1; room <= rooms; room++) {
        unsigned char header[4];
        uint16_t area_count;

        if (!queen_jas_read_at(target, *cursor, header, sizeof(header),
                               jas_size, status, status_size))
            return false;

        area_count = read_be16(header + 2);
        if (area_count > 32) {
            rb->snprintf(status, status_size,
                         "Queen JAS room %u has %u areas",
                         (unsigned)room,
                         (unsigned)area_count);
            return false;
        }

        if (!queen_jas_skip(cursor, *jas_size,
                            4 + (uint32_t)area_count * 16,
                            "grid-area", status, status_size))
            return false;
    }

    if (object_box_offset)
        *object_box_offset = *cursor;

    return queen_jas_skip(cursor, *jas_size, (uint32_t)objects * 8,
                          "grid-object-box", status, status_size);
}

static bool queen_read_box(const unsigned char *buf,
                           struct scummvm_queen_box *box)
{
    if (!buf || !box)
        return false;

    box->x1 = read_be16s(buf);
    box->y1 = read_be16s(buf + 2);
    box->x2 = read_be16s(buf + 4);
    box->y2 = read_be16s(buf + 6);
    return true;
}

static bool queen_jas_has_sfx_names(const struct scummvm_target *target)
{
    char path[MAX_PATH];
    int fd = -1;
    long size;
    const struct queen_version *version;

    if (!open_resource_file(target, path, sizeof(path), &fd))
        return true;

    size = rb->filesize(fd);
    rb->close(fd);
    version = version_from_size(size);
    if (!version)
        return true;

    if (!rb->strcmp(version->variant, "DOS demo") ||
        !rb->strcmp(version->variant, "Amiga interview"))
        return false;

    return true;
}

bool scummvm_queen_loader_read_object(const struct scummvm_target *target,
                                      const struct scummvm_queen_jas_info *info,
                                      uint16_t index,
                                      struct scummvm_queen_object_data *object,
                                      char *status,
                                      size_t status_size)
{
    unsigned char buf[16];
    uint32_t jas_size = 0;
    uint32_t offset;

    if (!info || !object) {
        rb->strlcpy(status, "Queen object read target is invalid",
                    status_size);
        return false;
    }

    if (index == 0 || index > info->objects) {
        rb->snprintf(status, status_size,
                     "Queen object %u is out of range",
                     (unsigned)index);
        return false;
    }

    offset = info->object_data_offset + (uint32_t)(index - 1) * sizeof(buf);
    if (!queen_jas_read_at(target, offset, buf, sizeof(buf), &jas_size,
                           status, status_size))
        return false;

    object->name = read_be16s(buf);
    object->x = read_be16(buf + 2);
    object->y = read_be16(buf + 4);
    object->description = read_be16(buf + 6);
    object->entry_object = read_be16s(buf + 8);
    object->room = read_be16(buf + 10);
    object->state = read_be16(buf + 12);
    object->image = read_be16s(buf + 14);
    rb->snprintf(status, status_size,
                 "Queen object %u: room %u image %d",
                 (unsigned)index,
                 (unsigned)object->room,
                 (int)object->image);
    return true;
}

static bool queen_jas_read_row(const struct scummvm_target *target,
                               uint32_t table_offset,
                               uint16_t count,
                               uint16_t index,
                               uint32_t row_size,
                               unsigned char *buf,
                               const char *table_name,
                               char *status,
                               size_t status_size)
{
    uint32_t jas_size = 0;
    uint32_t offset;

    if (!buf) {
        rb->snprintf(status, status_size,
                     "Queen %s read target is invalid", table_name);
        return false;
    }

    if (index == 0 || index > count) {
        rb->snprintf(status, status_size,
                     "Queen %s %u is out of range",
                     table_name,
                     (unsigned)index);
        return false;
    }

    offset = table_offset + (uint32_t)(index - 1) * row_size;
    return queen_jas_read_at(target, offset, buf, row_size, &jas_size,
                             status, status_size);
}

bool scummvm_queen_loader_read_item(const struct scummvm_target *target,
                                    const struct scummvm_queen_jas_info *info,
                                    uint16_t index,
                                    struct scummvm_queen_item_data *item,
                                    char *status,
                                    size_t status_size)
{
    unsigned char buf[10];

    if (!info || !item) {
        rb->strlcpy(status, "Queen item read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->item_data_offset, info->items,
                            index, sizeof(buf), buf, "item",
                            status, status_size))
        return false;

    item->name = read_be16s(buf);
    item->description = read_be16(buf + 2);
    item->state = read_be16(buf + 4);
    item->frame = read_be16(buf + 6);
    item->sfx_description = read_be16s(buf + 8);
    rb->snprintf(status, status_size,
                 "Queen item %u: frame %u state %u",
                 (unsigned)index,
                 (unsigned)item->frame,
                 (unsigned)item->state);
    return true;
}

bool scummvm_queen_loader_read_graphic(const struct scummvm_target *target,
                                       const struct scummvm_queen_jas_info *info,
                                       uint16_t index,
                                       struct scummvm_queen_graphic_data *graphic,
                                       char *status,
                                       size_t status_size)
{
    unsigned char buf[10];

    if (!info || !graphic) {
        rb->strlcpy(status, "Queen graphic read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->graphic_data_offset, info->graphics,
                            index, sizeof(buf), buf, "graphic",
                            status, status_size))
        return false;

    graphic->x = read_be16(buf);
    graphic->y = read_be16(buf + 2);
    graphic->first_frame = read_be16s(buf + 4);
    graphic->last_frame = read_be16s(buf + 6);
    graphic->speed = read_be16(buf + 8);
    rb->snprintf(status, status_size,
                 "Queen graphic %u: frame %d-%d",
                 (unsigned)index,
                 (int)graphic->first_frame,
                 (int)graphic->last_frame);
    return true;
}

bool scummvm_queen_loader_read_walk_off(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t index,
                                        struct scummvm_queen_walk_off_data *walk_off,
                                        char *status,
                                        size_t status_size)
{
    unsigned char buf[6];

    if (!info || !walk_off) {
        rb->strlcpy(status, "Queen walk-off read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->walk_off_data_offset,
                            info->walk_offs, index, sizeof(buf), buf,
                            "walk-off", status, status_size))
        return false;

    walk_off->entry_object = read_be16s(buf);
    walk_off->x = read_be16(buf + 2);
    walk_off->y = read_be16(buf + 4);
    rb->snprintf(status, status_size,
                 "Queen walk-off %u: obj %d at %u,%u",
                 (unsigned)index,
                 (int)walk_off->entry_object,
                 (unsigned)walk_off->x,
                 (unsigned)walk_off->y);
    return true;
}

bool scummvm_queen_loader_read_object_description(const struct scummvm_target *target,
                                                 const struct scummvm_queen_jas_info *info,
                                                 uint16_t index,
                                                 struct scummvm_queen_object_description *description,
                                                 char *status,
                                                 size_t status_size)
{
    unsigned char buf[8];

    if (!info || !description) {
        rb->strlcpy(status, "Queen description read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->object_description_offset,
                            info->object_descriptions, index, sizeof(buf),
                            buf, "description", status, status_size))
        return false;

    description->object = read_be16(buf);
    description->type = read_be16(buf + 2);
    description->last_description = read_be16(buf + 4);
    description->last_seen_number = read_be16(buf + 6);
    rb->snprintf(status, status_size,
                 "Queen description %u: object %u type %u",
                 (unsigned)index,
                 (unsigned)description->object,
                 (unsigned)description->type);
    return true;
}

bool scummvm_queen_loader_read_furniture(const struct scummvm_target *target,
                                         const struct scummvm_queen_jas_info *info,
                                         uint16_t index,
                                         struct scummvm_queen_furniture_data *furniture,
                                         char *status,
                                         size_t status_size)
{
    unsigned char buf[4];

    if (!info || !furniture) {
        rb->strlcpy(status, "Queen furniture read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->furniture_data_offset,
                            info->furniture, index, sizeof(buf), buf,
                            "furniture", status, status_size))
        return false;

    furniture->room = read_be16s(buf);
    furniture->object_number = read_be16s(buf + 2);
    rb->snprintf(status, status_size,
                 "Queen furniture %u: room %d object %d",
                 (unsigned)index,
                 (int)furniture->room,
                 (int)furniture->object_number);
    return true;
}

bool scummvm_queen_loader_read_actor(const struct scummvm_target *target,
                                     const struct scummvm_queen_jas_info *info,
                                     uint16_t index,
                                     struct scummvm_queen_actor_data *actor,
                                     char *status,
                                     size_t status_size)
{
    unsigned char buf[24];

    if (!info || !actor) {
        rb->strlcpy(status, "Queen actor read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->actor_data_offset, info->actors,
                            index, sizeof(buf), buf, "actor",
                            status, status_size))
        return false;

    actor->room = read_be16s(buf);
    actor->bob_number = read_be16s(buf + 2);
    actor->name = read_be16(buf + 4);
    actor->game_state_slot = read_be16s(buf + 6);
    actor->game_state_value = read_be16s(buf + 8);
    actor->color = read_be16(buf + 10);
    actor->standing_frame = read_be16(buf + 12);
    actor->x = read_be16(buf + 14);
    actor->y = read_be16(buf + 16);
    actor->anim = read_be16(buf + 18);
    actor->bank_number = read_be16(buf + 20);
    actor->file = read_be16(buf + 22);
    if (actor->file == 0)
        actor->bank_number = 15;
    rb->snprintf(status, status_size,
                 "Queen actor %u: room %d at %u,%u",
                 (unsigned)index,
                 (int)actor->room,
                 (unsigned)actor->x,
                 (unsigned)actor->y);
    return true;
}

bool scummvm_queen_loader_read_graphic_anim(const struct scummvm_target *target,
                                           const struct scummvm_queen_jas_info *info,
                                           uint16_t index,
                                           struct scummvm_queen_graphic_anim *anim,
                                           char *status,
                                           size_t status_size)
{
    unsigned char buf[6];

    if (!info || !anim) {
        rb->strlcpy(status, "Queen graphic-anim read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->graphic_anim_offset,
                            info->graphic_anims, index, sizeof(buf), buf,
                            "graphic-anim", status, status_size))
        return false;

    anim->key_frame = read_be16s(buf);
    anim->frame = read_be16s(buf + 2);
    anim->speed = read_be16(buf + 4);
    rb->snprintf(status, status_size,
                 "Queen graphic-anim %u: key %d frame %d",
                 (unsigned)index,
                 (int)anim->key_frame,
                 (int)anim->frame);
    return true;
}

bool scummvm_queen_loader_read_command_list(const struct scummvm_target *target,
                                           const struct scummvm_queen_jas_info *info,
                                           uint16_t index,
                                           struct scummvm_queen_command_list_data *command,
                                           char *status,
                                           size_t status_size)
{
    unsigned char buf[20];

    if (!info || !command) {
        rb->strlcpy(status, "Queen command-list read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->command_list_offset,
                            info->command_lists, index, sizeof(buf), buf,
                            "command-list", status, status_size))
        return false;

    command->verb = read_be16(buf);
    command->noun_object_1 = read_be16s(buf + 2);
    command->noun_object_2 = read_be16s(buf + 4);
    command->song = read_be16s(buf + 6);
    command->set_areas = read_be16(buf + 8) != 0;
    command->set_objects = read_be16(buf + 10) != 0;
    command->set_items = read_be16(buf + 12) != 0;
    command->set_conditions = read_be16(buf + 14) != 0;
    command->image_order = read_be16s(buf + 16);
    command->special_section = read_be16s(buf + 18);
    rb->snprintf(status, status_size,
                 "Queen command-list %u: verb %u",
                 (unsigned)index,
                 (unsigned)command->verb);
    return true;
}

bool scummvm_queen_loader_read_command_area(const struct scummvm_target *target,
                                           const struct scummvm_queen_jas_info *info,
                                           uint16_t index,
                                           struct scummvm_queen_command_area *area,
                                           char *status,
                                           size_t status_size)
{
    unsigned char buf[6];

    if (!info || !area) {
        rb->strlcpy(status, "Queen command-area read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->command_area_offset,
                            info->command_areas, index, sizeof(buf), buf,
                            "command-area", status, status_size))
        return false;

    area->id = read_be16s(buf);
    area->area = read_be16s(buf + 2);
    area->room = read_be16(buf + 4);
    rb->snprintf(status, status_size,
                 "Queen command-area %u: id %d room %u",
                 (unsigned)index,
                 (int)area->id,
                 (unsigned)area->room);
    return true;
}

bool scummvm_queen_loader_read_command_object(const struct scummvm_target *target,
                                             const struct scummvm_queen_jas_info *info,
                                             uint16_t index,
                                             struct scummvm_queen_command_object *object,
                                             char *status,
                                             size_t status_size)
{
    unsigned char buf[6];

    if (!info || !object) {
        rb->strlcpy(status, "Queen command-object read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->command_object_offset,
                            info->command_objects, index, sizeof(buf), buf,
                            "command-object", status, status_size))
        return false;

    object->id = read_be16s(buf);
    object->destination_object = read_be16s(buf + 2);
    object->source_object = read_be16s(buf + 4);
    if (index == 175 && object->id == 320 &&
        object->destination_object == 307 &&
        object->source_object == 309)
        object->destination_object = 308;
    rb->snprintf(status, status_size,
                 "Queen command-object %u: id %d",
                 (unsigned)index,
                 (int)object->id);
    return true;
}

bool scummvm_queen_loader_read_command_inventory(const struct scummvm_target *target,
                                                const struct scummvm_queen_jas_info *info,
                                                uint16_t index,
                                                struct scummvm_queen_command_inventory *inventory,
                                                char *status,
                                                size_t status_size)
{
    unsigned char buf[6];

    if (!info || !inventory) {
        rb->strlcpy(status, "Queen command-inventory read target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->command_inventory_offset,
                            info->command_inventory, index, sizeof(buf),
                            buf, "command-inventory", status, status_size))
        return false;

    inventory->id = read_be16s(buf);
    inventory->destination_item = read_be16s(buf + 2);
    inventory->source_item = read_be16s(buf + 4);
    rb->snprintf(status, status_size,
                 "Queen command-inventory %u: id %d",
                 (unsigned)index,
                 (int)inventory->id);
    return true;
}

bool scummvm_queen_loader_read_command_game_state(const struct scummvm_target *target,
                                                 const struct scummvm_queen_jas_info *info,
                                                 uint16_t index,
                                                 struct scummvm_queen_command_game_state *game_state,
                                                 char *status,
                                                 size_t status_size)
{
    unsigned char buf[8];

    if (!info || !game_state) {
        rb->strlcpy(status, "Queen command-game-state target is invalid",
                    status_size);
        return false;
    }

    if (!queen_jas_read_row(target, info->command_game_state_offset,
                            info->command_game_state, index, sizeof(buf),
                            buf, "command-game-state", status, status_size))
        return false;

    game_state->id = read_be16s(buf);
    game_state->slot = read_be16s(buf + 2);
    game_state->value = read_be16s(buf + 4);
    game_state->speak_value = read_be16(buf + 6);
    rb->snprintf(status, status_size,
                 "Queen command-game-state %u: id %d",
                 (unsigned)index,
                 (int)game_state->id);
    return true;
}

bool scummvm_queen_loader_grid_room(const struct scummvm_target *target,
                                    const struct scummvm_queen_jas_info *info,
                                    uint16_t room,
                                    struct scummvm_queen_grid_room *grid_room,
                                    char *status,
                                    size_t status_size)
{
    uint32_t cursor;
    uint32_t jas_size = 0;
    uint16_t current;

    if (!info || !grid_room) {
        rb->strlcpy(status, "Queen grid room target is invalid",
                    status_size);
        return false;
    }

    if (room == 0 || room > info->rooms) {
        rb->snprintf(status, status_size,
                     "Queen grid room %u is out of range",
                     (unsigned)room);
        return false;
    }

    cursor = info->grid_data_offset;
    for (current = 1; current <= room; current++) {
        unsigned char header[4];
        int16_t object_max;
        int16_t area_max;
        uint32_t area_offset;

        if (!queen_jas_read_at(target, cursor, header, sizeof(header),
                               &jas_size, status, status_size))
            return false;

        object_max = read_be16s(header);
        area_max = read_be16s(header + 2);
        if (area_max < 0 || area_max > 32) {
            rb->snprintf(status, status_size,
                         "Queen grid room %u has %d areas",
                         (unsigned)current,
                         (int)area_max);
            return false;
        }

        area_offset = cursor + 4;
        if (current == room) {
            grid_room->room = room;
            grid_room->object_max = object_max;
            grid_room->area_max = area_max;
            grid_room->area_offset = area_offset;
            rb->snprintf(status, status_size,
                         "Queen grid room %u: %d areas",
                         (unsigned)room,
                         (int)area_max);
            return true;
        }

        cursor = area_offset + (uint32_t)area_max * 16;
    }

    rb->strlcpy(status, "Queen grid room scan failed", status_size);
    return false;
}

bool scummvm_queen_loader_read_grid_area(const struct scummvm_target *target,
                                         const struct scummvm_queen_jas_info *info,
                                         uint16_t room,
                                         uint16_t area,
                                         struct scummvm_queen_grid_area *grid_area,
                                         char *status,
                                         size_t status_size)
{
    struct scummvm_queen_grid_room grid_room;
    unsigned char buf[16];
    uint32_t jas_size = 0;
    uint32_t offset;

    if (!grid_area) {
        rb->strlcpy(status, "Queen grid area target is invalid",
                    status_size);
        return false;
    }

    if (!scummvm_queen_loader_grid_room(target, info, room, &grid_room,
                                        status, status_size))
        return false;

    if (area == 0 || area > (uint16_t)grid_room.area_max) {
        rb->snprintf(status, status_size,
                     "Queen grid area %u/%u is out of range",
                     (unsigned)room,
                     (unsigned)area);
        return false;
    }

    offset = grid_room.area_offset + (uint32_t)(area - 1) * sizeof(buf);
    if (!queen_jas_read_at(target, offset, buf, sizeof(buf), &jas_size,
                           status, status_size))
        return false;

    grid_area->neighbors = read_be16s(buf);
    queen_read_box(buf + 2, &grid_area->box);
    grid_area->bottom_scale = read_be16(buf + 10);
    grid_area->top_scale = read_be16(buf + 12);
    grid_area->object = read_be16(buf + 14);
    rb->snprintf(status, status_size,
                 "Queen grid room %u area %u object %u",
                 (unsigned)room,
                 (unsigned)area,
                 (unsigned)grid_area->object);
    return true;
}

bool scummvm_queen_loader_read_object_box(const struct scummvm_target *target,
                                          const struct scummvm_queen_jas_info *info,
                                          uint16_t object,
                                          struct scummvm_queen_box *box,
                                          char *status,
                                          size_t status_size)
{
    unsigned char buf[8];
    uint32_t jas_size = 0;
    uint32_t offset;

    if (!info || !box) {
        rb->strlcpy(status, "Queen object box target is invalid",
                    status_size);
        return false;
    }

    if (object == 0 || object > info->objects) {
        rb->snprintf(status, status_size,
                     "Queen object box %u is out of range",
                     (unsigned)object);
        return false;
    }

    offset = info->grid_object_box_offset + (uint32_t)(object - 1) * sizeof(buf);
    if (!queen_jas_read_at(target, offset, buf, sizeof(buf), &jas_size,
                           status, status_size))
        return false;

    queen_read_box(buf, box);
    rb->snprintf(status, status_size,
                 "Queen object box %u: %d,%d-%d,%d",
                 (unsigned)object,
                 (int)box->x1,
                 (int)box->y1,
                 (int)box->x2,
                 (int)box->y2);
    return true;
}

bool scummvm_queen_loader_room_range(const struct scummvm_target *target,
                                     const struct scummvm_queen_jas_info *info,
                                     uint16_t room,
                                     struct scummvm_queen_room_object_range *range,
                                     char *status,
                                     size_t status_size)
{
    unsigned char buf[4];
    uint32_t jas_size = 0;
    uint32_t offset;
    uint16_t base_object;
    uint16_t next_base_object;

    if (!info || !range) {
        rb->strlcpy(status, "Queen room range target is invalid",
                    status_size);
        return false;
    }

    if (room == 0 || room > info->rooms) {
        rb->snprintf(status, status_size,
                     "Queen room %u is out of range",
                     (unsigned)room);
        return false;
    }

    offset = info->room_data_offset + (uint32_t)(room - 1) * 2;
    if (!queen_jas_read_at(target, offset, buf, sizeof(buf), &jas_size,
                           status, status_size))
        return false;

    base_object = read_be16(buf);
    next_base_object = read_be16(buf + 2);
    if (next_base_object < base_object || next_base_object > info->objects) {
        rb->snprintf(status, status_size,
                     "Queen room %u object range is invalid",
                     (unsigned)room);
        return false;
    }

    range->room = room;
    range->base_object = base_object;
    range->first_object = base_object + 1;
    range->last_object = next_base_object;
    rb->snprintf(status, status_size,
                 "Queen room %u objects %u-%u",
                 (unsigned)room,
                 (unsigned)range->first_object,
                 (unsigned)range->last_object);
    return true;
}

static bool queen_loader_read_graphic(const struct scummvm_target *target,
                                      const struct scummvm_queen_jas_info *info,
                                      uint16_t index,
                                      int16_t *last_frame,
                                      char *status,
                                      size_t status_size)
{
    unsigned char buf[10];
    uint32_t jas_size = 0;
    uint32_t offset;

    if (!info || !last_frame) {
        rb->strlcpy(status, "Queen graphic read target is invalid",
                    status_size);
        return false;
    }

    if (index == 0 || index > info->graphics) {
        rb->snprintf(status, status_size,
                     "Queen graphic %u is out of range",
                     (unsigned)index);
        return false;
    }

    offset = info->graphic_data_offset + (uint32_t)(index - 1) * sizeof(buf);
    if (!queen_jas_read_at(target, offset, buf, sizeof(buf), &jas_size,
                           status, status_size))
        return false;

    *last_frame = read_be16s(buf + 6);
    return true;
}

bool scummvm_queen_loader_room_summary(const struct scummvm_target *target,
                                       const struct scummvm_queen_jas_info *info,
                                       uint16_t room,
                                       struct scummvm_queen_room_object_summary *summary,
                                       char *status,
                                       size_t status_size)
{
    struct scummvm_queen_room_object_range range;
    uint16_t index;

    if (!summary) {
        rb->strlcpy(status, "Queen room summary target is invalid",
                    status_size);
        return false;
    }

    rb->memset(summary, 0, sizeof(*summary));
    if (!scummvm_queen_loader_room_range(target, info, room, &range,
                                         status, status_size))
        return false;

    summary->room = room;
    for (index = range.first_object; index <= range.last_object; index++) {
        struct scummvm_queen_object_data object;

        if (!scummvm_queen_loader_read_object(target, info, index, &object,
                                              status, status_size))
            return false;

        summary->objects++;
        if (object.name < 0 || object.image < 0)
            summary->hidden_objects++;
        else
            summary->visible_objects++;

        if (object.image == -1)
            summary->static_off_bobs++;
        else if (object.image == -2)
            summary->animated_off_bobs++;
        else if (object.image == -3 || object.image == -4)
            summary->person_objects++;
        else if (object.image > 0 && object.image < 5000) {
            int16_t last_frame;

            if (!queen_loader_read_graphic(target, info,
                                           (uint16_t)object.image,
                                           &last_frame,
                                           status, status_size))
                return false;
            if (last_frame == 0)
                summary->static_bobs++;
            else
                summary->animated_bobs++;
        }
        else if (object.image > 5000)
            summary->paste_downs++;
    }

    rb->snprintf(status, status_size,
                 "Queen room %u: %u objects, %u visible",
                 (unsigned)summary->room,
                 (unsigned)summary->objects,
                 (unsigned)summary->visible_objects);
    return true;
}

bool scummvm_queen_loader_room_entities(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t room,
                                        struct scummvm_queen_room_entity_summary *summary,
                                        char *status,
                                        size_t status_size)
{
    uint16_t index;

    if (!info || !summary) {
        rb->strlcpy(status, "Queen room entity target is invalid",
                    status_size);
        return false;
    }

    if (room == 0 || room > info->rooms) {
        rb->snprintf(status, status_size,
                     "Queen room %u entities are out of range",
                     (unsigned)room);
        return false;
    }

    rb->memset(summary, 0, sizeof(*summary));
    summary->room = room;

    for (index = 1; index <= info->furniture; index++) {
        struct scummvm_queen_furniture_data furniture;
        int16_t object_number;

        if (!scummvm_queen_loader_read_furniture(target, info, index,
                                                 &furniture, status,
                                                 status_size))
            return false;
        if (furniture.room != (int16_t)room)
            continue;

        object_number = furniture.object_number;
        summary->furniture++;
        if (object_number < 0)
            summary->furniture_disabled++;
        else if (object_number > 5000)
            summary->furniture_paste_downs++;
        else if (object_number > 0)
            summary->furniture_objects++;
    }

    for (index = 1; index <= info->actors; index++) {
        struct scummvm_queen_actor_data actor;

        if (!scummvm_queen_loader_read_actor(target, info, index, &actor,
                                             status, status_size))
            return false;
        if (actor.room != (int16_t)room)
            continue;

        summary->actors++;
        if (actor.game_state_slot == 0)
            summary->unconditional_actors++;
        else
            summary->conditional_actors++;
    }

    rb->snprintf(status, status_size,
                 "Queen room %u: %u furniture, %u actors",
                 (unsigned)summary->room,
                 (unsigned)summary->furniture,
                 (unsigned)summary->actors);
    return true;
}

bool scummvm_queen_loader_find_commands(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t verb,
                                        int16_t noun_object_1,
                                        int16_t noun_object_2,
                                        struct scummvm_queen_command_match_summary *summary,
                                        char *status,
                                        size_t status_size)
{
    uint16_t index;

    if (!info || !summary) {
        rb->strlcpy(status, "Queen command match target is invalid",
                    status_size);
        return false;
    }

    rb->memset(summary, 0, sizeof(*summary));
    for (index = 1; index <= info->command_lists; index++) {
        struct scummvm_queen_command_list_data command;

        if (!scummvm_queen_loader_read_command_list(target, info, index,
                                                    &command, status,
                                                    status_size))
            return false;

        if (command.verb != verb ||
            command.noun_object_1 != noun_object_1 ||
            command.noun_object_2 != noun_object_2)
            continue;

        if (summary->matches == 0)
            summary->first_match = index;
        summary->matches++;
        summary->last_match = index;
    }

    rb->snprintf(status, status_size,
                 "Queen command match verb %u obj %d/%d: %u",
                 (unsigned)verb,
                 (int)noun_object_1,
                 (int)noun_object_2,
                 (unsigned)summary->matches);
    return true;
}

static uint16_t queen_abs16(int16_t value)
{
    if (value < 0)
        return (uint16_t)-value;

    return (uint16_t)value;
}

static void queen_upper_name(char *name)
{
    while (*name) {
        if (*name >= 'a' && *name <= 'z')
            *name = (char)(*name - 'a' + 'A');
        name++;
    }
}

static bool queen_make_resource_name(const char *stem,
                                     const char *extension,
                                     char *out,
                                     size_t out_size)
{
    if (rb->snprintf(out, out_size, "%s.%s", stem, extension) >=
        (int)out_size)
        return false;

    queen_upper_name(out);
    return true;
}

bool scummvm_queen_loader_command_batch(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t command,
                                        struct scummvm_queen_command_batch_summary *summary,
                                        char *status,
                                        size_t status_size)
{
    uint16_t index;

    if (!info || !summary) {
        rb->strlcpy(status, "Queen command batch target is invalid",
                    status_size);
        return false;
    }

    rb->memset(summary, 0, sizeof(*summary));
    summary->command = command;

    for (index = 1; index <= info->command_areas; index++) {
        struct scummvm_queen_command_area area;
        struct scummvm_queen_grid_room grid_room;
        uint16_t area_number;

        if (!scummvm_queen_loader_read_command_area(target, info, index,
                                                    &area, status,
                                                    status_size))
            return false;
        if (area.id != (int16_t)command)
            continue;

        area_number = queen_abs16(area.area);
        summary->areas++;
        if (area.area > 0)
            summary->areas_on++;
        else if (area.area < 0)
            summary->areas_off++;

        if (area.room == 0 || area.room > info->rooms ||
            !scummvm_queen_loader_grid_room(target, info, area.room,
                                            &grid_room, status,
                                            status_size))
            return false;
        if (area_number == 0 ||
            area_number > (uint16_t)grid_room.area_max)
            summary->invalid_references++;
    }

    for (index = 1; index <= info->command_objects; index++) {
        struct scummvm_queen_command_object object;
        uint16_t destination;
        uint16_t source;

        if (!scummvm_queen_loader_read_command_object(target, info, index,
                                                      &object, status,
                                                      status_size))
            return false;
        if (object.id != (int16_t)command)
            continue;

        destination = queen_abs16(object.destination_object);
        source = queen_abs16(object.source_object);
        summary->objects++;
        if (object.destination_object > 0)
            summary->object_shows++;
        else if (object.destination_object < 0)
            summary->object_hides++;
        if (object.source_object > 0)
            summary->object_copies++;
        else if (object.source_object == -1)
            summary->object_deletes++;

        if (destination == 0 || destination > info->objects)
            summary->invalid_references++;
        if (object.source_object > 0 && source > info->objects)
            summary->invalid_references++;
    }

    for (index = 1; index <= info->command_inventory; index++) {
        struct scummvm_queen_command_inventory inventory;
        uint16_t destination;
        uint16_t source;

        if (!scummvm_queen_loader_read_command_inventory(target, info, index,
                                                         &inventory, status,
                                                         status_size))
            return false;
        if (inventory.id != (int16_t)command)
            continue;

        destination = queen_abs16(inventory.destination_item);
        source = queen_abs16(inventory.source_item);
        summary->inventory++;
        if (inventory.destination_item > 0)
            summary->inventory_adds++;
        else if (inventory.destination_item < 0)
            summary->inventory_deletes++;

        if (destination == 0 || destination > info->items)
            summary->invalid_references++;
        if (inventory.source_item > 0 && source > info->items)
            summary->invalid_references++;
    }

    for (index = 1; index <= info->command_game_state; index++) {
        struct scummvm_queen_command_game_state game_state;

        if (!scummvm_queen_loader_read_command_game_state(target, info,
                                                          index, &game_state,
                                                          status,
                                                          status_size))
            return false;
        if (game_state.id != (int16_t)command)
            continue;

        if (game_state.slot > 0)
            summary->game_state_tests++;
        else
            summary->game_state_sets++;
    }

    if (summary->invalid_references > 0) {
        rb->snprintf(status, status_size,
                     "Queen command %u has %u invalid references",
                     (unsigned)summary->command,
                     (unsigned)summary->invalid_references);
        return false;
    }

    rb->snprintf(status, status_size,
                 "Queen command %u batch: %u area, %u object, %u bad",
                 (unsigned)summary->command,
                 (unsigned)summary->areas,
                 (unsigned)summary->objects,
                 (unsigned)summary->invalid_references);
    return true;
}

bool scummvm_queen_loader_room_assets(const struct scummvm_target *target,
                                      const struct scummvm_queen_jas_info *info,
                                      const struct scummvm_queen_text_info *text,
                                      uint16_t room,
                                      struct scummvm_queen_room_asset_info *assets,
                                      char *status,
                                      size_t status_size)
{
    struct scummvm_queen_resource_info resource;
    uint32_t entries = 0;
    char filename[13];

    if (!info || !text || !assets) {
        rb->strlcpy(status, "Queen room asset target is invalid",
                    status_size);
        return false;
    }

    if (room == 0 || room > info->rooms) {
        rb->snprintf(status, status_size,
                     "Queen room %u assets are out of range",
                     (unsigned)room);
        return false;
    }

    rb->memset(assets, 0, sizeof(*assets));
    assets->room = room;
    if (!scummvm_queen_loader_read_text_line(
            target, text->room_name_offset + room - 1,
            assets->room_name, sizeof(assets->room_name),
            status, status_size))
        return false;
    queen_upper_name(assets->room_name);

    if (!queen_make_resource_name(assets->room_name, "PCX",
                                  filename, sizeof(filename))) {
        rb->strlcpy(status, "Queen room PCX name is too long", status_size);
        return false;
    }

    if (!scummvm_queen_loader_resource_info(target, filename, &resource,
                                            &entries, status, status_size)) {
        if (!queen_make_resource_name(assets->room_name, "LBM",
                                      filename, sizeof(filename))) {
            rb->strlcpy(status, "Queen room LBM name is too long",
                        status_size);
            return false;
        }
        if (!scummvm_queen_loader_resource_info(target, filename, &resource,
                                                &entries, status,
                                                status_size))
            return false;
    }
    rb->strlcpy(assets->backdrop_name, resource.filename,
                sizeof(assets->backdrop_name));
    assets->backdrop_size = resource.size;

    if (!queen_make_resource_name(assets->room_name, "BBK",
                                  filename, sizeof(filename))) {
        rb->strlcpy(status, "Queen room BBK name is too long", status_size);
        return false;
    }
    if (!scummvm_queen_loader_resource_info(target, filename, &resource,
                                            &entries, status, status_size))
        return false;
    rb->strlcpy(assets->bank_name, resource.filename,
                sizeof(assets->bank_name));
    assets->bank_size = resource.size;

    if (queen_make_resource_name(assets->room_name, "MSK",
                                 filename, sizeof(filename)) &&
        scummvm_queen_loader_resource_info(target, filename, &resource,
                                           &entries, status, status_size)) {
        assets->mask_present = true;
        assets->mask_size = resource.size;
    }

    if (queen_make_resource_name(assets->room_name, "LUM",
                                 filename, sizeof(filename)) &&
        scummvm_queen_loader_resource_info(target, filename, &resource,
                                           &entries, status, status_size)) {
        assets->lum_present = true;
        assets->lum_size = resource.size;
    }

    rb->snprintf(status, status_size,
                 "Queen room %u assets: %s %s",
                 (unsigned)room,
                 assets->backdrop_name,
                 assets->bank_name);
    return true;
}

bool scummvm_queen_loader_pcx_info(const struct scummvm_target *target,
                                   const char *filename,
                                   struct scummvm_queen_pcx_info *pcx,
                                   char *status,
                                   size_t status_size)
{
    unsigned char header[128];
    unsigned char palette_marker = 0;
    uint32_t resource_size = 0;
    uint16_t xmin;
    uint16_t ymin;
    uint16_t xmax;
    uint16_t ymax;

    if (!filename || !pcx) {
        rb->strlcpy(status, "Queen PCX target is invalid", status_size);
        return false;
    }

    rb->memset(pcx, 0, sizeof(*pcx));
    if (!scummvm_queen_loader_read_resource(target, filename, 0,
                                            header, sizeof(header),
                                            &resource_size, status,
                                            status_size))
        return false;

    if (resource_size < sizeof(header)) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s is too small", filename);
        return false;
    }

    if (header[0] != 0x0a) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s has no ZSoft marker", filename);
        return false;
    }

    pcx->version = header[1];
    pcx->encoding = header[2];
    pcx->bits_per_pixel = header[3];
    xmin = read_le16(header + 4);
    ymin = read_le16(header + 6);
    xmax = read_le16(header + 8);
    ymax = read_le16(header + 10);
    pcx->planes = header[65];
    pcx->bytes_per_line = read_le16(header + 66);
    pcx->has_ega_palette = true;

    if (xmax < xmin || ymax < ymin) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s dimensions are invalid", filename);
        return false;
    }

    pcx->width = xmax - xmin + 1;
    pcx->height = ymax - ymin + 1;
    if (pcx->encoding > 1 || pcx->bits_per_pixel != 8 ||
        pcx->planes != 1 || pcx->bytes_per_line < pcx->width) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s format is unsupported", filename);
        return false;
    }

    if (resource_size > 769 &&
        scummvm_queen_loader_read_resource(target, filename,
                                           resource_size - 769,
                                           &palette_marker,
                                           sizeof(palette_marker),
                                           &resource_size, status,
                                           status_size))
        pcx->has_vga_palette = palette_marker == 0x0c;

    rb->snprintf(status, status_size,
                 "Queen PCX %s: %ux%u bpl %u",
                 filename,
                 (unsigned)pcx->width,
                 (unsigned)pcx->height,
                 (unsigned)pcx->bytes_per_line);
    return true;
}

static bool queen_loader_count_text_lines(const struct scummvm_target *target,
                                          const char *filename,
                                          uint32_t *resource_size,
                                          uint32_t *lines,
                                          char *status,
                                          size_t status_size)
{
    unsigned char buf[256];
    uint32_t size = 0;
    uint32_t offset = 0;
    uint32_t count = 0;
    unsigned char last = '\n';

    if (!resource_size || !lines) {
        rb->strlcpy(status, "Queen text count target is invalid",
                    status_size);
        return false;
    }

    while (true) {
        uint32_t remaining;
        size_t to_read;
        uint32_t i;

        if (!scummvm_queen_loader_read_resource(target, filename, offset,
                                                buf, sizeof(buf), &size,
                                                status, status_size))
            return false;

        if (offset >= size)
            break;

        remaining = size - offset;
        to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        for (i = 0; i < (uint32_t)to_read; i++) {
            last = buf[i];
            if (buf[i] == '\n')
                count++;
        }

        offset += to_read;
    }

    if (size > 0 && last != '\n')
        count++;

    *resource_size = size;
    *lines = count;
    return true;
}

bool scummvm_queen_loader_text_info(const struct scummvm_target *target,
                                    const struct scummvm_queen_jas_info *jas,
                                    struct scummvm_queen_text_info *text,
                                    char *status,
                                    size_t status_size)
{
    if (!jas || !text) {
        rb->strlcpy(status, "Queen text info target is invalid",
                    status_size);
        return false;
    }

    rb->memset(text, 0, sizeof(*text));
    text->object_description_offset = 0;
    text->object_name_offset =
        text->object_description_offset + jas->descriptions;
    text->room_name_offset = text->object_name_offset + jas->names;
    text->verb_name_offset = text->room_name_offset + jas->rooms;
    text->joe_response_offset = text->verb_name_offset + 12;
    text->actor_anim_offset = text->joe_response_offset + 40;
    text->actor_name_offset = text->actor_anim_offset + jas->actor_anims;
    text->actor_file_offset = text->actor_name_offset + jas->actor_names;
    text->expected_lines = text->actor_file_offset + jas->actor_files;

    if (!queen_loader_count_text_lines(target, "QUEEN2.JAS",
                                       &text->size, &text->lines,
                                       status, status_size))
        return false;

    if (text->lines < text->expected_lines) {
        rb->snprintf(status, status_size,
                     "Queen text has %lu/%lu lines",
                     (unsigned long)text->lines,
                     (unsigned long)text->expected_lines);
        return false;
    }

    rb->snprintf(status, status_size,
                 "Queen text: %lu lines, %lu bytes",
                 (unsigned long)text->lines,
                 (unsigned long)text->size);
    return true;
}

bool scummvm_queen_loader_read_text_line(const struct scummvm_target *target,
                                         uint32_t line,
                                         char *out,
                                         size_t out_size,
                                         char *status,
                                         size_t status_size)
{
    unsigned char buf[256];
    uint32_t size = 0;
    uint32_t offset = 0;
    uint32_t current = 0;
    size_t out_pos = 0;
    bool capturing = line == 0;

    if (!out || out_size == 0) {
        rb->strlcpy(status, "Queen text line target is invalid",
                    status_size);
        return false;
    }

    out[0] = '\0';
    while (true) {
        uint32_t remaining;
        size_t to_read;
        uint32_t i;

        if (!scummvm_queen_loader_read_resource(target, "QUEEN2.JAS",
                                                offset, buf, sizeof(buf),
                                                &size, status, status_size))
            return false;

        if (offset >= size)
            break;

        remaining = size - offset;
        to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        for (i = 0; i < (uint32_t)to_read; i++) {
            unsigned char c = buf[i];

            if (c == '\n') {
                if (capturing) {
                    if (out_pos > 0 && out[out_pos - 1] == '\r')
                        out[--out_pos] = '\0';
                    rb->snprintf(status, status_size,
                                 "Queen text line %lu: %s",
                                 (unsigned long)line,
                                 out);
                    return true;
                }
                current++;
                capturing = current == line;
                continue;
            }

            if (capturing && out_pos + 1 < out_size) {
                out[out_pos++] = (char)c;
                out[out_pos] = '\0';
            }
        }

        offset += to_read;
    }

    if (capturing && (out_pos > 0 || line == current)) {
        if (out_pos > 0 && out[out_pos - 1] == '\r')
            out[--out_pos] = '\0';
        rb->snprintf(status, status_size,
                     "Queen text line %lu: %s",
                     (unsigned long)line,
                     out);
        return true;
    }

    rb->snprintf(status, status_size,
                 "Queen text line %lu is missing",
                 (unsigned long)line);
    return false;
}

bool scummvm_queen_loader_jas_info(const struct scummvm_target *target,
                                   struct scummvm_queen_jas_info *info,
                                   char *status,
                                   size_t status_size)
{
    unsigned char header[8];
    uint32_t size = 0;
    uint32_t cursor = 8;

    if (!info) {
        rb->strlcpy(status, "Queen JAS info target is invalid",
                    status_size);
        return false;
    }

    rb->memset(info, 0, sizeof(*info));
    if (!scummvm_queen_loader_read_resource(target, "QUEEN.JAS", 20,
                                            header, sizeof(header),
                                            &size, status, status_size))
        return false;

    if (size < 28) {
        rb->strlcpy(status, "Queen JAS header is too small", status_size);
        return false;
    }

    info->rooms = read_be16(header);
    info->names = read_be16(header + 2);
    info->objects = read_be16(header + 4);
    info->descriptions = read_be16(header + 6);
    info->size = size - 20;

    if (info->rooms == 0 || info->rooms > 1024 ||
        info->objects == 0 || info->objects > 8192 ||
        info->names > 8192 || info->descriptions > 8192) {
        rb->snprintf(status, status_size,
                     "Queen JAS counts invalid: r%u n%u o%u d%u",
                     (unsigned)info->rooms,
                     (unsigned)info->names,
                     (unsigned)info->objects,
                     (unsigned)info->descriptions);
        return false;
    }

    info->object_data_offset = cursor;
    if (!queen_jas_skip(&cursor, info->size,
                        (uint32_t)info->objects * 16,
                        "object-data", status, status_size) ||
        (info->room_data_offset = cursor,
         !queen_jas_skip(&cursor, info->size,
                        (uint32_t)(info->rooms + 1) * 2,
                        "room-data", status, status_size)))
        return false;

    if (queen_jas_has_sfx_names(target)) {
        info->sfx_name_offset = cursor;
        if (!queen_jas_skip(&cursor, info->size, (uint32_t)info->rooms * 2,
                            "sfx-name", status, status_size))
            return false;
    } else {
        info->sfx_name_offset = 0;
    }

    if (!queen_jas_read_u16(target, &cursor, &info->items, &info->size,
                            status, status_size) ||
        (info->item_data_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        (uint32_t)info->items * 10,
                        "item-data", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->graphics, &info->size,
                            status, status_size) ||
        (info->graphic_data_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        (uint32_t)info->graphics * 10,
                        "graphic-data", status, status_size)) ||
        (info->grid_data_offset = cursor,
        !queen_jas_scan_grid(target, &cursor, &info->size,
                             info->rooms, info->objects,
                             &info->grid_object_box_offset,
                             status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->walk_offs, &info->size,
                            status, status_size) ||
        (info->walk_off_data_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        (uint32_t)info->walk_offs * 6,
                        "walk-off", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->object_descriptions,
                            &info->size, status, status_size) ||
        (info->object_description_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        (uint32_t)info->object_descriptions * 8,
                        "object-description", status, status_size)) ||
        (info->command_data_offset = cursor,
        !queen_jas_read_u16(target, &cursor, &info->command_lists,
                            &info->size, status, status_size) ||
        (info->command_list_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        queen_counted_bytes(info->command_lists, 20),
                        "command-list", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->command_areas,
                            &info->size, status, status_size) ||
        (info->command_area_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        queen_counted_bytes(info->command_areas, 6),
                        "command-area", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->command_objects,
                            &info->size, status, status_size) ||
        (info->command_object_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        queen_counted_bytes(info->command_objects, 6),
                        "command-object", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->command_inventory,
                            &info->size, status, status_size) ||
        (info->command_inventory_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        queen_counted_bytes(info->command_inventory, 6),
                        "command-inventory", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->command_game_state,
                            &info->size, status, status_size) ||
        (info->command_game_state_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        queen_counted_bytes(info->command_game_state, 8),
                        "command-game-state", status, status_size))) ||
        !queen_jas_read_u16(target, &cursor, &info->entry_object,
                            &info->size, status, status_size) ||
        !queen_jas_read_u16(target, &cursor, &info->furniture,
                            &info->size, status, status_size) ||
        (info->furniture_data_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        (uint32_t)info->furniture * 4,
                        "furniture", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->actors, &info->size,
                            status, status_size) ||
        !queen_jas_read_u16(target, &cursor, &info->actor_anims,
                            &info->size, status, status_size) ||
        !queen_jas_read_u16(target, &cursor, &info->actor_names,
                            &info->size, status, status_size) ||
        !queen_jas_read_u16(target, &cursor, &info->actor_files,
                            &info->size, status, status_size) ||
        (info->actor_data_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        (uint32_t)info->actors * 24,
                        "actor-data", status, status_size)) ||
        !queen_jas_read_u16(target, &cursor, &info->graphic_anims,
                            &info->size, status, status_size) ||
        (info->graphic_anim_offset = cursor,
        !queen_jas_skip(&cursor, info->size,
                        queen_counted_bytes(info->graphic_anims, 6),
                        "graphic-anim", status, status_size)))
        return false;

    info->version_offset = cursor;
    if (!queen_jas_skip(&cursor, info->size, 5, "version",
                        status, status_size))
        return false;

    if (info->entry_object == 0 || info->entry_object > info->objects) {
        rb->snprintf(status, status_size,
                     "Queen entry object %u is out of range",
                     (unsigned)info->entry_object);
        return false;
    } else {
        struct scummvm_queen_object_data entry;
        struct scummvm_queen_room_object_range range;

        if (!scummvm_queen_loader_read_object(target, info,
                                              info->entry_object,
                                              &entry,
                                              status, status_size))
            return false;
        info->current_room = entry.room;
        if (!scummvm_queen_loader_room_range(target, info,
                                             info->current_room,
                                             &range,
                                             status, status_size))
            return false;
        info->current_room_first_object = range.first_object;
        info->current_room_last_object = range.last_object;
    }

    rb->snprintf(status, status_size,
                 "Queen JAS: room %u objects %u-%u, %u commands",
                 (unsigned)info->current_room,
                 (unsigned)info->current_room_first_object,
                 (unsigned)info->current_room_last_object,
                 (unsigned)info->command_lists);
    return true;
}
