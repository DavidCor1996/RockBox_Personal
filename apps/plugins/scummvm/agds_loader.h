/***************************************************************************
 * Minimal AGDS data probe for the Rockbox ScummVM plugin.
 *
 * This reads only archive/database metadata. Copyrighted NiBiRu data stays
 * outside the firmware tree and is supplied by the user's own installation.
 ****************************************************************************/

#ifndef SCUMMVM_AGDS_LOADER_H
#define SCUMMVM_AGDS_LOADER_H

#include "scummvm.h"

#define AGDS_RESOURCE_NAME_SIZE 34
#define AGDS_ADB_NAME_SIZE 64
#define AGDS_ARCHIVE_NAME_SIZE 16
#define AGDS_MAX_ARCHIVES 12

struct scummvm_agds_config {
    uint16_t video_width;
    uint16_t video_height;
    uint8_t video_depth;
    uint8_t archive_count;
    char archives[AGDS_MAX_ARCHIVES][AGDS_ARCHIVE_NAME_SIZE];
};

struct scummvm_agds_info {
    uint32_t adb_entries;
    uint32_t grp_entries;
    uint32_t grp_files;
    uint32_t pictures;
    uint32_t audio;
    uint32_t video;
    bool encrypted_groups;
    char first_adb_entry[40];
    char first_resource[40];
};

struct scummvm_agds_resource {
    char archive[16];
    char name[AGDS_RESOURCE_NAME_SIZE];
    uint32_t offset;
    uint32_t size;
};

struct scummvm_agds_adb_entry {
    char archive[AGDS_ARCHIVE_NAME_SIZE];
    char name[AGDS_ADB_NAME_SIZE];
    uint32_t offset;
    uint32_t size;
};

bool scummvm_agds_probe(const struct scummvm_target *target,
                        struct scummvm_agds_info *info,
                        char *status, size_t status_size);
bool scummvm_agds_read_config(const struct scummvm_target *target,
                              struct scummvm_agds_config *config,
                              char *status, size_t status_size);
bool scummvm_agds_find_resource(const struct scummvm_target *target,
                                const char *name,
                                struct scummvm_agds_resource *resource,
                                char *status, size_t status_size);
bool scummvm_agds_find_first_picture(
    const struct scummvm_target *target,
    struct scummvm_agds_resource *resource,
    char *status, size_t status_size);
long scummvm_agds_read_resource(
    const struct scummvm_target *target,
    const struct scummvm_agds_resource *resource,
    uint32_t relative_offset, void *buffer, uint32_t size);
bool scummvm_agds_find_adb_entry(
    const struct scummvm_target *target, const char *name,
    struct scummvm_agds_adb_entry *entry,
    char *status, size_t status_size);
bool scummvm_agds_find_named_adb_entry(
    const struct scummvm_target *target, const char *archive,
    const char *name, struct scummvm_agds_adb_entry *entry,
    char *status, size_t status_size);
long scummvm_agds_read_adb_entry(
    const struct scummvm_target *target,
    const struct scummvm_agds_adb_entry *entry,
    uint32_t relative_offset, void *buffer, uint32_t size);
bool scummvm_agds_read_text(const struct scummvm_target *target,
                            const char *entry_name,
                            char *text, size_t text_size,
                            char *status, size_t status_size);

#endif
