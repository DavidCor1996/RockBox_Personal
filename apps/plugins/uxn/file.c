#include "file.h"

#define UXN_FILE_COUNT 2

enum file_state
{
    FILE_IDLE,
    FILE_READING,
    FILE_WRITING,
    DIR_READING
};

struct uxn_file
{
    int fd;
    DIR *dir;
    enum file_state state;
    char path[MAX_PATH];
    char pending[MAX_PATH + 16];
    int pending_length;
    bool directory_path;
    bool valid;
};

static struct uxn_file files[UXN_FILE_COUNT];

static void file_reset(struct uxn_file *file)
{
    if (file->fd >= 0)
        rb->close(file->fd);
    if (file->dir)
        rb->closedir(file->dir);
    file->fd = -1;
    file->dir = NULL;
    file->state = FILE_IDLE;
    file->pending_length = 0;
}

void uxn_file_init(void)
{
    int i;

    rb->mkdir(ROCKBOX_DIR "/uxn");
    rb->mkdir(UXN_DATA_DIR);
    rb->memset(files, 0, sizeof(files));
    for (i = 0; i < UXN_FILE_COUNT; i++)
        files[i].fd = -1;
}

void uxn_file_shutdown(void)
{
    int i;

    for (i = 0; i < UXN_FILE_COUNT; i++)
        file_reset(&files[i]);
}

static bool build_path(struct uxn_file *file, const uint8_t *source,
                       size_t source_max)
{
    char relative[MAX_PATH];
    size_t in = 0;
    size_t out = 0;

    file_reset(file);
    file->valid = false;
    file->directory_path = false;
    while (in < source_max && source[in] == '/')
        in++;
    while (in < source_max && source[in])
    {
        size_t segment_start;

        while (in < source_max && source[in] == '/')
            in++;
        if (in >= source_max || !source[in])
            break;
        segment_start = in;
        while (in < source_max && source[in] && source[in] != '/')
            in++;
        if ((in - segment_start == 1 && source[segment_start] == '.') ||
            (in - segment_start == 2 && source[segment_start] == '.' &&
             source[segment_start + 1] == '.'))
            return false;
        if (out && out < sizeof(relative) - 1)
            relative[out++] = '/';
        if (out + in - segment_start >= sizeof(relative))
            return false;
        rb->memcpy(relative + out, source + segment_start,
                   in - segment_start);
        out += in - segment_start;
    }
    if (in >= source_max)
        return false;
    relative[out] = '\0';
    if (!out)
        rb->strlcpy(file->path, UXN_DATA_DIR, sizeof(file->path));
    else if (rb->snprintf(file->path, sizeof(file->path), "%s/%s",
                          UXN_DATA_DIR, relative) >= (int)sizeof(file->path))
        return false;
    file->directory_path = in > 0 && source[in - 1] == '/';
    file->valid = true;
    return true;
}

static void ensure_parents(char *path)
{
    char *cursor = path + sizeof(UXN_DATA_DIR);

    while ((cursor = rb->strchr(cursor, '/')) != NULL)
    {
        *cursor = '\0';
        rb->mkdir(path);
        *cursor++ = '/';
    }
}

static uint16_t read_directory(struct uxn_file *file, char *dest,
                               uint16_t length)
{
    uint16_t used = 0;
    struct dirent *entry;

    if (file->pending_length)
    {
        if (file->pending_length > length)
            return 0;
        rb->memcpy(dest, file->pending, file->pending_length);
        used = file->pending_length;
        file->pending_length = 0;
    }
    while ((entry = rb->readdir(file->dir)) != NULL)
    {
        struct dirinfo info;
        int line_length;

        if (!rb->strcmp(entry->d_name, ".") ||
            !rb->strcmp(entry->d_name, ".."))
            continue;
        info = rb->dir_get_info(file->dir, entry);
        if (info.attribute & ATTR_DIRECTORY)
            line_length = rb->snprintf(file->pending,
                                       sizeof(file->pending), "---- %s/\n",
                                       entry->d_name);
        else if (info.size < 0x10000)
            line_length = rb->snprintf(file->pending,
                                       sizeof(file->pending), "%04x %s\n",
                                       (unsigned int)info.size,
                                       entry->d_name);
        else
            line_length = rb->snprintf(file->pending,
                                       sizeof(file->pending), "???? %s\n",
                                       entry->d_name);
        if (line_length <= 0 || line_length >= (int)sizeof(file->pending))
        {
            file->pending_length = 0;
            continue;
        }
        file->pending_length = line_length;
        if (used + file->pending_length > length)
            break;
        rb->memcpy(dest + used, file->pending, file->pending_length);
        used += file->pending_length;
        file->pending_length = 0;
    }
    return used;
}

static uint16_t file_read(struct uxn_file *file, void *dest, uint16_t length)
{
    ssize_t result;

    if (!file->valid)
        return 0;
    if (file->state == FILE_IDLE)
    {
        file->dir = rb->opendir(file->path);
        if (file->dir)
            file->state = DIR_READING;
        else
        {
            file->fd = rb->open(file->path, O_RDONLY);
            if (file->fd >= 0)
                file->state = FILE_READING;
        }
    }
    if (file->state == DIR_READING)
        return read_directory(file, dest, length);
    if (file->state != FILE_READING)
        return 0;
    result = rb->read(file->fd, dest, length);
    return result > 0 ? result : 0;
}

static uint16_t file_write(struct uxn_file *file, const void *source,
                           uint16_t length, uint8_t flags)
{
    ssize_t result;

    if (!file->valid)
        return 0;
    ensure_parents(file->path);
    if (file->directory_path)
        return rb->dir_exists(file->path) || rb->mkdir(file->path) == 0;
    if (file->state == FILE_IDLE)
    {
        int mode = O_WRONLY | O_CREAT | (flags & 1 ? O_APPEND : O_TRUNC);

        file->fd = rb->open(file->path, mode, 0666);
        if (file->fd >= 0)
            file->state = FILE_WRITING;
    }
    if (file->state != FILE_WRITING)
        return 0;
    result = rb->write(file->fd, source, length);
    return result > 0 ? result : 0;
}

static uint16_t fill_stat(struct uxn_file *file, uint8_t *dest,
                          uint16_t length)
{
    int fd;
    off_t size;
    int i;

    if (!file->valid)
        return 0;
    if (rb->dir_exists(file->path))
    {
        rb->memset(dest, '-', length);
        return length;
    }
    fd = rb->open(file->path, O_RDONLY);
    if (fd < 0)
    {
        rb->memset(dest, '!', length);
        return length;
    }
    size = rb->filesize(fd);
    rb->close(fd);
    for (i = length - 1; i >= 0; i--)
    {
        int digit = size & 0xf;

        dest[i] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        size >>= 4;
    }
    if (size)
        rb->memset(dest, '?', length);
    return length;
}

static uint16_t file_delete(struct uxn_file *file)
{
    if (!file->valid)
        return 0;
    file_reset(file);
    if (rb->dir_exists(file->path))
        return rb->rmdir(file->path);
    return rb->remove(file->path);
}

void uxn_file_deo(uint8_t port)
{
    int index = port >= 0xb0;
    uint8_t base = index ? 0xb0 : 0xa0;
    uint8_t relative = port - base;
    uint8_t *device = &uxn.dev[base];
    struct uxn_file *file = &files[index];
    uint16_t addr;
    uint16_t length;
    uint16_t result = 0;

    switch (relative)
    {
    case 0x5:
        addr = PEEK2(device + 0x4);
        length = PEEK2(device + 0xa);
        if (length > UXN_PAGE_SIZE - addr)
            length = UXN_PAGE_SIZE - addr;
        result = fill_stat(file, &uxn.ram[addr], length);
        break;
    case 0x6:
        result = file_delete(file);
        break;
    case 0x9:
        addr = PEEK2(device + 0x8);
        build_path(file, &uxn.ram[addr], UXN_PAGE_SIZE - addr);
        break;
    case 0xd:
        addr = PEEK2(device + 0xc);
        length = PEEK2(device + 0xa);
        if (length > UXN_PAGE_SIZE - addr)
            length = UXN_PAGE_SIZE - addr;
        result = file_read(file, &uxn.ram[addr], length);
        break;
    case 0xf:
        addr = PEEK2(device + 0xe);
        length = PEEK2(device + 0xa);
        if (length > UXN_PAGE_SIZE - addr)
            length = UXN_PAGE_SIZE - addr;
        result = file_write(file, &uxn.ram[addr], length, device[0x7]);
        break;
    default:
        return;
    }
    POKE2(device + 0x2, result);
}
