/***************************************************************************
 * Minimal, allocation-free AGDS ADB/GRP metadata reader.
 *
 * The format contract follows the independently documented AGDS containers
 * and is intentionally bounded for malformed media. It does not extract or
 * include game content.
 ****************************************************************************/

#include "agds_loader.h"
#include "rbfile.h"

#define AGDS_ADB_MAGIC 666u
#define AGDS_ADB_HEADER 0x14u
#define AGDS_GRP_HEADER 0x2cu
#define AGDS_GRP_RECORD 0x31u
#define AGDS_GRP_NAME 0x21u
#define AGDS_GRP_MAGIC 0x1a03c9e6u
#define AGDS_GRP_VERSION1 44u
#define AGDS_GRP_VERSION2 2u
#define AGDS_CONFIG_MAX_SIZE 2048u

static const unsigned char agds_grp_signature[16] = {
    'A', 'G', 'D', 'S', ' ', 'g', 'r', 'o',
    'u', 'p', ' ', 'f', 'i', 'l', 'e', 0x1a
};

static const char agds_key[] =
    "Vyvojovy tym AGDS varuje: Hackerovani skodi obchodu!";

static unsigned char agds_config_buffer[AGDS_CONFIG_MAX_SIZE];

static uint32_t read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void decrypt(unsigned char *data, size_t size)
{
    size_t key_pos = 0;
    size_t key_size = sizeof(agds_key) - 1;

    while (size-- > 0) {
        *data++ ^= (unsigned char)(0xffu ^ (unsigned char)agds_key[key_pos]);
        key_pos++;
        if (key_pos == key_size)
            key_pos = 0;
    }
}

static bool read_exact(struct scummvm_file *file, long offset,
                       void *buffer, long size)
{
    return scummvm_file_seek(file, offset) &&
           scummvm_file_read(file, buffer, size) == size;
}

static void copy_name(char *dst, size_t dst_size,
                      const unsigned char *src, size_t src_size)
{
    size_t length = 0;

    while (length < src_size && src[length] != '\0')
        length++;
    if (length >= dst_size)
        length = dst_size - 1;
    rb->memcpy(dst, src, length);
    dst[length] = '\0';
}

static bool suffix(const char *name, const char *extension)
{
    size_t name_len = rb->strlen(name);
    size_t extension_len = rb->strlen(extension);

    return name_len >= extension_len &&
           !rb->strcasecmp(name + name_len - extension_len, extension);
}

static bool parse_uint(const char **cursor, unsigned *value)
{
    const char *p = *cursor;
    unsigned result = 0;
    bool found = false;

    while (*p >= '0' && *p <= '9') {
        unsigned digit = (unsigned)(*p - '0');

        if (result > 65535u / 10u ||
            (result == 65535u / 10u && digit > 65535u % 10u))
            return false;
        result = result * 10u + digit;
        found = true;
        p++;
    }
    if (!found)
        return false;
    *cursor = p;
    *value = result;
    return true;
}

static bool parse_videomode(const char *value,
                            struct scummvm_agds_config *config)
{
    unsigned width;
    unsigned height;
    unsigned depth;

    if (!parse_uint(&value, &width) || *value++ != 'x' ||
        !parse_uint(&value, &height) || *value++ != 'x' ||
        !parse_uint(&value, &depth) || *value != '\0' ||
        width == 0 || height == 0 || depth == 0 || depth > 32)
        return false;
    config->video_width = (uint16_t)width;
    config->video_height = (uint16_t)height;
    config->video_depth = (uint8_t)depth;
    return true;
}

bool scummvm_agds_read_config(const struct scummvm_target *target,
                              struct scummvm_agds_config *config,
                              char *status, size_t status_size)
{
    struct scummvm_file file;
    long size;
    char *line;
    char *end;

    rb->memset(config, 0, sizeof(*config));
    if (!scummvm_file_open_game(&file, target, "agds.cfg")) {
        rb->strlcpy(status, "Need agds.cfg", status_size);
        return false;
    }
    size = scummvm_file_size(&file);
    if (size <= 0 || size >= (long)sizeof(agds_config_buffer) ||
        scummvm_file_read(&file, agds_config_buffer, size) != size) {
        rb->strlcpy(status, "agds.cfg is empty or too large", status_size);
        scummvm_file_close(&file);
        return false;
    }
    scummvm_file_close(&file);
    agds_config_buffer[size] = '\0';
    line = (char *)agds_config_buffer;
    end = line + size;
    while (line < end) {
        char *next = line;
        char *after;
        char *tail;

        while (next < end && *next != '\n' && *next != '\r')
            next++;
        after = next;
        while (after < end && (*after == '\n' || *after == '\r'))
            after++;
        tail = next;
        while (tail > line && (tail[-1] == ' ' || tail[-1] == '\t'))
            tail--;
        *tail = '\0';
        while (*line == ' ' || *line == '\t')
            line++;
        if (!rb->strncasecmp(line, "videomode=", 10)) {
            if (!parse_videomode(line + 10, config)) {
                rb->strlcpy(status, "Invalid agds.cfg videomode",
                            status_size);
                return false;
            }
        } else if (!rb->strncasecmp(line, "path=", 5)) {
            const char *archive = line + 5;

            if (config->archive_count >= AGDS_MAX_ARCHIVES ||
                archive[0] == '\0' || rb->strlen(archive) >=
                    AGDS_ARCHIVE_NAME_SIZE) {
                rb->strlcpy(status, "Invalid agds.cfg archive list",
                            status_size);
                return false;
            }
            rb->strlcpy(config->archives[config->archive_count], archive,
                        AGDS_ARCHIVE_NAME_SIZE);
            config->archive_count++;
        }
        line = after;
    }
    if (config->video_width == 0 || config->video_height == 0 ||
        config->archive_count == 0) {
        rb->strlcpy(status, "agds.cfg lacks video mode or archives",
                    status_size);
        return false;
    }
    return true;
}

static unsigned archive_count(const struct scummvm_agds_config *config)
{
    return config->archive_count ? config->archive_count : 9u;
}

static const char *archive_name(const struct scummvm_agds_config *config,
                                unsigned index, char *fallback,
                                size_t fallback_size)
{
    if (config->archive_count)
        return config->archives[index];
    rb->snprintf(fallback, fallback_size, "gfx%u.grp", index + 1u);
    return fallback;
}

static bool probe_adb(const struct scummvm_target *target,
                      struct scummvm_agds_info *info,
                      char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char header[AGDS_ADB_HEADER];
    uint32_t total;
    uint32_t used;
    uint32_t name_size;
    uint32_t record_size;
    uint64_t data_offset;
    uint32_t index;
    unsigned char record[4 + 256 + 4];

    if (!scummvm_file_open_game(&file, target, "data.adb")) {
        rb->strlcpy(status, "Need data.adb", status_size);
        return false;
    }
    if (!read_exact(&file, 0, header, sizeof(header)) ||
        read_u32le(header) != AGDS_ADB_MAGIC) {
        rb->strlcpy(status, "Invalid data.adb header", status_size);
        scummvm_file_close(&file);
        return false;
    }

    total = read_u32le(header + 8);
    used = read_u32le(header + 12);
    name_size = read_u32le(header + 16);
    if (used > total || total > 200000u || name_size == 0 || name_size > 255u) {
        rb->strlcpy(status, "Unsafe data.adb index", status_size);
        scummvm_file_close(&file);
        return false;
    }
    record_size = name_size + 9u;
    data_offset = AGDS_ADB_HEADER + (uint64_t)record_size * total;
    if (data_offset > (uint64_t)scummvm_file_size(&file)) {
        rb->strlcpy(status, "Truncated data.adb index", status_size);
        scummvm_file_close(&file);
        return false;
    }

    for (index = 0; index < used; index++) {
        uint32_t relative;
        uint32_t size;
        uint64_t end;

        if (!read_exact(&file, AGDS_ADB_HEADER + (long)index * record_size,
                        record, record_size)) {
            rb->strlcpy(status, "Short data.adb record", status_size);
            scummvm_file_close(&file);
            return false;
        }
        relative = read_u32le(record);
        size = read_u32le(record + 4 + name_size + 1);
        end = data_offset + relative + size;
        if (end > (uint64_t)scummvm_file_size(&file)) {
            rb->strlcpy(status, "data.adb entry outside file", status_size);
            scummvm_file_close(&file);
            return false;
        }
        if (index == 0)
            copy_name(info->first_adb_entry,
                      sizeof(info->first_adb_entry), record + 4,
                      name_size + 1);
    }

    info->adb_entries = used;
    scummvm_file_close(&file);
    return true;
}

static bool probe_grp(const struct scummvm_target *target, const char *name,
                      struct scummvm_agds_info *info,
                      char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char header[AGDS_GRP_HEADER];
    uint32_t count;
    uint32_t index;
    bool encrypted = false;

    if (!scummvm_file_open_game(&file, target, name))
        return false;
    if (!read_exact(&file, 0, header, sizeof(header))) {
        rb->snprintf(status, status_size, "%s header truncated", name);
        scummvm_file_close(&file);
        return false;
    }
    if (rb->memcmp(header, agds_grp_signature,
                   sizeof(agds_grp_signature)) != 0) {
        decrypt(header, sizeof(agds_grp_signature));
        if (rb->memcmp(header, agds_grp_signature,
                       sizeof(agds_grp_signature)) != 0) {
            rb->snprintf(status, status_size, "%s signature invalid", name);
            scummvm_file_close(&file);
            return false;
        }
        encrypted = true;
    }
    if (read_u32le(header + 0x10) != AGDS_GRP_VERSION1 ||
        read_u32le(header + 0x14) != AGDS_GRP_MAGIC ||
        read_u32le(header + 0x18) != AGDS_GRP_VERSION2) {
        rb->snprintf(status, status_size, "%s version invalid", name);
        scummvm_file_close(&file);
        return false;
    }
    count = read_u32le(header + 0x1c);
    if (count > 500000u ||
        (uint64_t)AGDS_GRP_HEADER + (uint64_t)count * AGDS_GRP_RECORD >
            (uint64_t)scummvm_file_size(&file)) {
        rb->snprintf(status, status_size, "%s index truncated", name);
        scummvm_file_close(&file);
        return false;
    }

    for (index = 0; index < count; index++) {
        unsigned char record[AGDS_GRP_RECORD];
        char member[AGDS_GRP_NAME + 1];
        size_t name_len = 0;
        uint32_t offset;
        uint32_t size;

        if (!read_exact(&file, AGDS_GRP_HEADER +
                        (long)index * AGDS_GRP_RECORD,
                        record, sizeof(record))) {
            rb->snprintf(status, status_size, "%s record truncated", name);
            scummvm_file_close(&file);
            return false;
        }
        while (name_len < AGDS_GRP_NAME && record[name_len] != '\0')
            name_len++;
        if (name_len == AGDS_GRP_NAME) {
            rb->snprintf(status, status_size, "%s name unterminated", name);
            scummvm_file_close(&file);
            return false;
        }
        if (encrypted)
            decrypt(record, name_len);
        copy_name(member, sizeof(member), record, name_len);
        offset = read_u32le(record + 0x21);
        size = read_u32le(record + 0x25);
        if ((uint64_t)offset + size >
            (uint64_t)scummvm_file_size(&file)) {
            rb->snprintf(status, status_size, "%s member outside file", name);
            scummvm_file_close(&file);
            return false;
        }
        if (info->first_resource[0] == '\0')
            rb->strlcpy(info->first_resource, member,
                        sizeof(info->first_resource));
        if (suffix(member, ".bmp") || suffix(member, ".pcx"))
            info->pictures++;
        else if (suffix(member, ".wav") || suffix(member, ".ogg"))
            info->audio++;
        else if (suffix(member, ".flc") || suffix(member, ".avi") ||
                 suffix(member, ".mpg") || suffix(member, ".mpeg"))
            info->video++;
    }

    info->grp_entries += count;
    info->grp_files++;
    info->encrypted_groups |= encrypted;
    scummvm_file_close(&file);
    return true;
}

static bool find_in_grp(const struct scummvm_target *target,
                        const char *archive, const char *wanted,
                        bool picture_only,
                        struct scummvm_agds_resource *resource,
                        bool *archive_present,
                        char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char header[AGDS_GRP_HEADER];
    uint32_t count;
    uint32_t index;
    bool encrypted = false;

    *archive_present = false;
    if (!scummvm_file_open_game(&file, target, archive))
        return false;
    *archive_present = true;
    if (!read_exact(&file, 0, header, sizeof(header))) {
        rb->snprintf(status, status_size, "%s header truncated", archive);
        scummvm_file_close(&file);
        return false;
    }
    if (rb->memcmp(header, agds_grp_signature,
                   sizeof(agds_grp_signature)) != 0) {
        decrypt(header, sizeof(agds_grp_signature));
        if (rb->memcmp(header, agds_grp_signature,
                       sizeof(agds_grp_signature)) != 0) {
            rb->snprintf(status, status_size, "%s signature invalid", archive);
            scummvm_file_close(&file);
            return false;
        }
        encrypted = true;
    }
    if (read_u32le(header + 0x10) != AGDS_GRP_VERSION1 ||
        read_u32le(header + 0x14) != AGDS_GRP_MAGIC ||
        read_u32le(header + 0x18) != AGDS_GRP_VERSION2) {
        rb->snprintf(status, status_size, "%s version invalid", archive);
        scummvm_file_close(&file);
        return false;
    }
    count = read_u32le(header + 0x1c);
    if (count > 500000u ||
        (uint64_t)AGDS_GRP_HEADER + (uint64_t)count * AGDS_GRP_RECORD >
            (uint64_t)scummvm_file_size(&file)) {
        rb->snprintf(status, status_size, "%s index truncated", archive);
        scummvm_file_close(&file);
        return false;
    }

    for (index = 0; index < count; index++) {
        unsigned char record[AGDS_GRP_RECORD];
        char member[AGDS_RESOURCE_NAME_SIZE];
        size_t name_len = 0;
        uint32_t offset;
        uint32_t size;
        bool matches;

        if (!read_exact(&file, AGDS_GRP_HEADER +
                        (long)index * AGDS_GRP_RECORD,
                        record, sizeof(record))) {
            rb->snprintf(status, status_size, "%s record truncated", archive);
            scummvm_file_close(&file);
            return false;
        }
        while (name_len < AGDS_GRP_NAME && record[name_len] != '\0')
            name_len++;
        if (name_len == AGDS_GRP_NAME) {
            rb->snprintf(status, status_size, "%s name unterminated", archive);
            scummvm_file_close(&file);
            return false;
        }
        if (encrypted)
            decrypt(record, name_len);
        copy_name(member, sizeof(member), record, name_len);
        offset = read_u32le(record + 0x21);
        size = read_u32le(record + 0x25);
        if ((uint64_t)offset + size >
            (uint64_t)scummvm_file_size(&file)) {
            rb->snprintf(status, status_size,
                         "%s member outside file", archive);
            scummvm_file_close(&file);
            return false;
        }
        matches = wanted ? !rb->strcasecmp(member, wanted) :
            (picture_only &&
             (suffix(member, ".bmp") || suffix(member, ".pcx")));
        if (!matches)
            continue;
        rb->strlcpy(resource->archive, archive, sizeof(resource->archive));
        rb->strlcpy(resource->name, member, sizeof(resource->name));
        resource->offset = offset;
        resource->size = size;
        scummvm_file_close(&file);
        return true;
    }

    scummvm_file_close(&file);
    return false;
}

bool scummvm_agds_probe(const struct scummvm_target *target,
                        struct scummvm_agds_info *info,
                        char *status, size_t status_size)
{
    struct scummvm_agds_config config;
    unsigned index;
    char name[16];

    rb->memset(info, 0, sizeof(*info));
    if (status_size > 0)
        status[0] = '\0';
    if (!probe_adb(target, info, status, status_size))
        return false;

    if (!scummvm_agds_read_config(target, &config, status, status_size))
        return false;

    for (index = 0; index < archive_count(&config); index++) {
        bool loaded;
        const char *archive = archive_name(&config, index, name,
                                           sizeof(name));

        if (status_size > 0)
            status[0] = '\0';
        loaded = probe_grp(target, archive, info, status, status_size);
        if (!loaded) {
            if (status_size > 0 && status[0] == '\0')
                rb->snprintf(status, status_size, "Need %s", archive);
            return false;
        }
    }
    rb->snprintf(status, status_size,
                 "AGDS indexed: %lu scripts, %lu assets in %lu groups",
                 (unsigned long)info->adb_entries,
                 (unsigned long)info->grp_entries,
                 (unsigned long)info->grp_files);
    return true;
}

static bool find_resource(const struct scummvm_target *target,
                          const char *name, bool picture_only,
                          struct scummvm_agds_resource *resource,
                          char *status, size_t status_size)
{
    struct scummvm_agds_config config;
    unsigned index;
    char archive[16];

    rb->memset(resource, 0, sizeof(*resource));
    if (status_size > 0)
        status[0] = '\0';
    if (!scummvm_agds_read_config(target, &config, status, status_size))
        return false;
    for (index = 0; index < archive_count(&config); index++) {
        bool present;
        const char *archive_path = archive_name(&config, index, archive,
                                                sizeof(archive));

        if (find_in_grp(target, archive_path, name, picture_only, resource,
                        &present, status, status_size))
            return true;
        if (present && status_size > 0 && status[0] != '\0')
            return false;
    }
    if (name)
        rb->snprintf(status, status_size, "Missing AGDS resource %.40s", name);
    else
        rb->strlcpy(status, "No BMP/PCX resource found", status_size);
    return false;
}

bool scummvm_agds_find_resource(const struct scummvm_target *target,
                                const char *name,
                                struct scummvm_agds_resource *resource,
                                char *status, size_t status_size)
{
    if (!name || name[0] == '\0') {
        rb->strlcpy(status, "AGDS resource name is empty", status_size);
        return false;
    }
    return find_resource(target, name, false, resource, status, status_size);
}

bool scummvm_agds_find_first_picture(
    const struct scummvm_target *target,
    struct scummvm_agds_resource *resource,
    char *status, size_t status_size)
{
    return find_resource(target, NULL, true, resource, status, status_size);
}

long scummvm_agds_read_resource(
    const struct scummvm_target *target,
    const struct scummvm_agds_resource *resource,
    uint32_t relative_offset, void *buffer, uint32_t size)
{
    struct scummvm_file file;
    long result;

    if (!resource || relative_offset > resource->size ||
        size > resource->size - relative_offset ||
        (uint64_t)resource->offset + relative_offset + size > 0x7fffffffu)
        return -1;
    if (!scummvm_file_open_game(&file, target, resource->archive))
        return -1;
    if (!scummvm_file_seek(&file,
                           (long)(resource->offset + relative_offset))) {
        scummvm_file_close(&file);
        return -1;
    }
    result = scummvm_file_read(&file, buffer, (long)size);
    scummvm_file_close(&file);
    return result;
}

bool scummvm_agds_find_named_adb_entry(
    const struct scummvm_target *target, const char *archive,
    const char *name,
    struct scummvm_agds_adb_entry *entry,
    char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char header[AGDS_ADB_HEADER];
    uint32_t total;
    uint32_t used;
    uint32_t name_size;
    uint32_t record_size;
    uint64_t data_offset;
    uint32_t index;
    unsigned char record[4 + 256 + 4];

    rb->memset(entry, 0, sizeof(*entry));
    if (!archive || archive[0] == '\0' || !name || name[0] == '\0') {
        rb->strlcpy(status, "ADB entry name is empty", status_size);
        return false;
    }
    if (!scummvm_file_open_game(&file, target, archive)) {
        rb->snprintf(status, status_size, "Need %.15s", archive);
        return false;
    }
    if (!read_exact(&file, 0, header, sizeof(header)) ||
        read_u32le(header) != AGDS_ADB_MAGIC) {
        rb->snprintf(status, status_size,
                     "Invalid %.15s header", archive);
        scummvm_file_close(&file);
        return false;
    }
    total = read_u32le(header + 8);
    used = read_u32le(header + 12);
    name_size = read_u32le(header + 16);
    if (used > total || total > 200000u ||
        name_size == 0 || name_size > 255u) {
        rb->snprintf(status, status_size,
                     "Unsafe %.15s index", archive);
        scummvm_file_close(&file);
        return false;
    }
    record_size = name_size + 9u;
    data_offset = AGDS_ADB_HEADER + (uint64_t)record_size * total;
    if (data_offset > (uint64_t)scummvm_file_size(&file)) {
        rb->snprintf(status, status_size,
                     "Truncated %.15s index", archive);
        scummvm_file_close(&file);
        return false;
    }
    for (index = 0; index < used; index++) {
        char entry_name[AGDS_ADB_NAME_SIZE];
        uint32_t relative;
        uint32_t size;

        if (!read_exact(&file, AGDS_ADB_HEADER +
                        (long)index * record_size,
                        record, record_size)) {
            rb->snprintf(status, status_size,
                         "Short %.15s record", archive);
            scummvm_file_close(&file);
            return false;
        }
        copy_name(entry_name, sizeof(entry_name), record + 4,
                  name_size + 1);
        relative = read_u32le(record);
        size = read_u32le(record + 4 + name_size + 1);
        if (data_offset + relative + size >
            (uint64_t)scummvm_file_size(&file)) {
            rb->snprintf(status, status_size,
                         "%.15s entry outside file", archive);
            scummvm_file_close(&file);
            return false;
        }
        if (rb->strcasecmp(entry_name, name))
            continue;
        rb->strlcpy(entry->archive, archive, sizeof(entry->archive));
        rb->strlcpy(entry->name, entry_name, sizeof(entry->name));
        entry->offset = (uint32_t)(data_offset + relative);
        entry->size = size;
        scummvm_file_close(&file);
        return true;
    }
    rb->snprintf(status, status_size, "Missing ADB entry %.40s", name);
    scummvm_file_close(&file);
    return false;
}

bool scummvm_agds_find_adb_entry(
    const struct scummvm_target *target, const char *name,
    struct scummvm_agds_adb_entry *entry,
    char *status, size_t status_size)
{
    return scummvm_agds_find_named_adb_entry(
        target, "data.adb", name, entry, status, status_size);
}

long scummvm_agds_read_adb_entry(
    const struct scummvm_target *target,
    const struct scummvm_agds_adb_entry *entry,
    uint32_t relative_offset, void *buffer, uint32_t size)
{
    struct scummvm_file file;
    long result;

    if (!entry || relative_offset > entry->size ||
        size > entry->size - relative_offset ||
        (uint64_t)entry->offset + relative_offset + size > 0x7fffffffu)
        return -1;
    if (!scummvm_file_open_game(&file, target,
                                entry->archive[0] != '\0' ?
                                entry->archive : "data.adb"))
        return -1;
    if (!scummvm_file_seek(&file,
                           (long)(entry->offset + relative_offset))) {
        scummvm_file_close(&file);
        return -1;
    }
    result = scummvm_file_read(&file, buffer, (long)size);
    scummvm_file_close(&file);
    return result;
}

bool scummvm_agds_read_text(const struct scummvm_target *target,
                            const char *entry_name,
                            char *text, size_t text_size,
                            char *status, size_t status_size)
{
    struct scummvm_agds_adb_entry entry;
    size_t length;

    if (!text || text_size < 2) {
        rb->strlcpy(status, "AGDS text buffer is too small", status_size);
        return false;
    }
    text[0] = '\0';
    if (!scummvm_agds_find_adb_entry(target, entry_name, &entry,
                                     status, status_size))
        return false;
    if (entry.size == 0 || entry.size >= text_size ||
        scummvm_agds_read_adb_entry(target, &entry, 0, text,
                                    entry.size) != (long)entry.size) {
        rb->snprintf(status, status_size,
                     "AGDS text %.32s is invalid", entry_name);
        return false;
    }
    length = entry.size;
    while (length > 0 && text[length - 1] == '\0')
        length--;
    if (length == 0) {
        rb->snprintf(status, status_size,
                     "AGDS text %.32s is empty", entry_name);
        return false;
    }
    if ((unsigned char)text[0] < ' ' || (unsigned char)text[0] >= 0x7f)
        decrypt((unsigned char *)text, length);
    /* Encrypted AGDS text can include its C terminator in the encrypted
     * payload, so it only becomes visible after decryption. */
    while (length > 0 && text[length - 1] == '\0')
        length--;
    if (length == 0) {
        rb->snprintf(status, status_size,
                     "AGDS text %.32s is empty", entry_name);
        return false;
    }
    if (rb->memchr(text, '\0', length) != NULL) {
        rb->snprintf(status, status_size,
                     "AGDS text %.32s contains NUL", entry_name);
        return false;
    }
    text[length] = '\0';
    return true;
}
