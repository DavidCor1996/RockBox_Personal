#include "ipodgames.h"

#define IG_EAPP_HEADER_SIZE 0x2c
#define IG_FRAMEWORK_NAME_SIZE 32
#define IG_FRAMEWORK_FIXED_SIZE (IG_FRAMEWORK_NAME_SIZE + 16 + 4 + 4)
#define IG_MAX_FRAMEWORKS 128
#define IG_MAX_IMPORTS 4096

static unsigned long ig_get_u32le(const unsigned char *data)
{
    return (unsigned long)data[0] |
           ((unsigned long)data[1] << 8) |
           ((unsigned long)data[2] << 16) |
           ((unsigned long)data[3] << 24);
}

static bool ig_read_at(int fd, unsigned long offset, void *buffer, size_t size)
{
    return rb->lseek(fd, (off_t)offset, SEEK_SET) == (off_t)offset &&
           rb->read(fd, buffer, size) == (ssize_t)size;
}

static bool ig_framework_name_valid(const unsigned char *record)
{
    unsigned int index;

    if (!record[0])
        return false;
    for (index = 0; index < IG_FRAMEWORK_NAME_SIZE && record[index]; ++index)
    {
        if (record[index] < 0x20 || record[index] > 0x7e)
            return false;
    }
    return index < IG_FRAMEWORK_NAME_SIZE;
}

const char *ig_probe_result_name(enum ig_probe_result result)
{
    switch (result)
    {
        case IG_PROBE_OK:
            return "eApp header valid";
        case IG_PROBE_NO_EXECUTABLE:
            return "No decrypted executable";
        case IG_PROBE_OPEN_FAILED:
            return "Cannot open executable";
        case IG_PROBE_SHORT_HEADER:
            return "Executable header is truncated";
        case IG_PROBE_BAD_MAGIC:
            return "Executable is encrypted or unsupported";
        case IG_PROBE_BAD_POINTER:
            return "Invalid framework pointer";
        case IG_PROBE_BAD_FRAMEWORK:
            return "Invalid framework table";
    }
    return "Unknown probe result";
}

void ig_runtime_probe(const struct ig_game *game, struct ig_eapp_probe *probe)
{
    unsigned char header[IG_EAPP_HEADER_SIZE];
    unsigned char framework[IG_FRAMEWORK_FIXED_SIZE];
    char directory[MAX_PATH];
    char path[MAX_PATH];
    char *separator;
    unsigned long framework_pointer;
    int fd;

    rb->memset(probe, 0, sizeof(*probe));
    if (!game->executable_path[0])
    {
        probe->result = IG_PROBE_NO_EXECUTABLE;
        return;
    }

    rb->strlcpy(directory, game->metadata_path, sizeof(directory));
    separator = rb->strrchr(directory, PATH_SEPCH);
    if (!separator)
    {
        probe->result = IG_PROBE_OPEN_FAILED;
        return;
    }
    *separator = '\0';
    rb->snprintf(path, sizeof(path), "%s/%s", directory,
                 game->executable_path);

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        probe->result = IG_PROBE_OPEN_FAILED;
        return;
    }
    probe->file_size = (unsigned long)rb->filesize(fd);
    if (rb->read(fd, header, sizeof(header)) != (ssize_t)sizeof(header))
    {
        probe->result = IG_PROBE_SHORT_HEADER;
        rb->close(fd);
        return;
    }
    if (rb->memcmp(header, "eapp", 4))
    {
        probe->result = IG_PROBE_BAD_MAGIC;
        rb->close(fd);
        return;
    }

    probe->first_record_offset = ig_get_u32le(&header[0x0c]);
    probe->first_framework_pointer = ig_get_u32le(&header[0x10]);
    probe->header_word_14 = ig_get_u32le(&header[0x14]);
    probe->header_word_18 = ig_get_u32le(&header[0x18]);
    probe->header_word_24 = ig_get_u32le(&header[0x24]);
    if (probe->first_framework_pointer < probe->first_record_offset + 4)
    {
        probe->result = IG_PROBE_BAD_POINTER;
        rb->close(fd);
        return;
    }
    probe->inferred_load_base = probe->first_framework_pointer -
                                (probe->first_record_offset + 4);
    if (probe->inferred_load_base & 3)
    {
        probe->result = IG_PROBE_BAD_POINTER;
        rb->close(fd);
        return;
    }

    framework_pointer = probe->first_framework_pointer;
    while (framework_pointer)
    {
        unsigned long offset;
        unsigned long count;
        unsigned long next_pointer;
        unsigned long table_end;

        if (probe->framework_count >= IG_MAX_FRAMEWORKS ||
            framework_pointer < probe->inferred_load_base)
        {
            probe->result = IG_PROBE_BAD_FRAMEWORK;
            rb->close(fd);
            return;
        }
        offset = framework_pointer - probe->inferred_load_base;
        if (offset > probe->file_size ||
            probe->file_size - offset < sizeof(framework) ||
            !ig_read_at(fd, offset, framework, sizeof(framework)) ||
            !ig_framework_name_valid(framework))
        {
            probe->result = IG_PROBE_BAD_FRAMEWORK;
            rb->close(fd);
            return;
        }

        count = ig_get_u32le(&framework[IG_FRAMEWORK_NAME_SIZE + 16]);
        next_pointer = ig_get_u32le(&framework[IG_FRAMEWORK_NAME_SIZE + 20]);
        if (count == 0 && next_pointer == 0)
            break;
        if (count > IG_MAX_IMPORTS ||
            count > (probe->file_size - offset - sizeof(framework)) / 8)
        {
            probe->result = IG_PROBE_BAD_FRAMEWORK;
            rb->close(fd);
            return;
        }
        table_end = offset + sizeof(framework) + count * 8;
        if (table_end > probe->file_size ||
            next_pointer <= framework_pointer ||
            next_pointer - probe->inferred_load_base < table_end ||
            next_pointer - probe->inferred_load_base >= probe->file_size)
        {
            probe->result = IG_PROBE_BAD_FRAMEWORK;
            rb->close(fd);
            return;
        }

        ++probe->framework_count;
        probe->total_imports += count;
        framework_pointer = next_pointer;
    }
    rb->close(fd);
    probe->result = IG_PROBE_OK;
}
