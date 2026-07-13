/***************************************************************************
 * ROM inspection and SRAM persistence.
 ***************************************************************************/

#include "snes_lite.h"

static bool has_rom_extension(const char *path)
{
    const char *extension = rb->strrchr(path, '.');

    return extension && (!rb->strcasecmp(extension, ".sfc") ||
                         !rb->strcasecmp(extension, ".smc"));
}

static int header_score(const unsigned char *header)
{
    int score = 0;
    unsigned checksum = header[0x1e] | (header[0x1f] << 8);
    unsigned inverse = header[0x1c] | (header[0x1d] << 8);

    if ((checksum ^ inverse) == 0xffff)
        score += 4;
    if ((header[0x3d] & 0x80) != 0)
        score += 2;
    if (header[0x15] < 0x40)
        score++;
    return score;
}

static const char *detect_special_chip(unsigned char map,
                                       unsigned char type)
{
    if ((map & 0x2f) == 0x23 || type == 0x34 || type == 0x35)
        return "SA-1";
    if ((map & 0x3f) == 0x32 || type == 0x43 || type == 0x45)
        return "S-DD1";
    if (type >= 0x13 && type <= 0x1a)
        return "SuperFX";
    if ((type & 0xf0) == 0xf0 &&
        (!rb->strncasecmp(snes_lite.rom.title, "MEGAMAN X", 9) ||
         !rb->strncasecmp(snes_lite.rom.title, "ROCKMAN X", 9)))
        return "C4";
    return NULL;
}

static void make_save_path(const char *rom_path)
{
    const char *name = rb->strrchr(rom_path, '/');
    char *extension;

    name = name ? name + 1 : rom_path;
    rb->strlcpy(snes_lite.rom.basename, name,
                sizeof(snes_lite.rom.basename));
    extension = rb->strrchr(snes_lite.rom.basename, '.');
    if (extension)
        *extension = '\0';
    rb->snprintf(snes_lite.rom.save_path,
                 sizeof(snes_lite.rom.save_path), "%s/%s.srm",
                 SNES_LITE_SAVE_DIR, snes_lite.rom.basename);
}

bool snes_lite_rom_load(const char *path)
{
    int fd;
    off_t size;
    ssize_t read_size;
    size_t header_offset;
    const unsigned char *header;
    int lo_score = -1;
    int hi_score = -1;
    int i;

    if (!path || !path[0] || !has_rom_extension(path))
    {
        rb->splash(HZ * 2, "Open an uncompressed .sfc or .smc ROM");
        return false;
    }
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        rb->splash(HZ * 2, "ROM not found");
        return false;
    }
    size = rb->filesize(fd);
    if (size < 0x8000 || size > SNES_LITE_MAX_ROM_SIZE)
    {
        rb->close(fd);
        rb->splash(HZ * 2, "ROM is empty or too large");
        return false;
    }
    snes_lite.rom_data = snes_lite_malloc((size_t)size);
    if (!snes_lite.rom_data)
    {
        rb->close(fd);
        rb->splash(HZ * 2, "Insufficient memory for ROM");
        return false;
    }
    read_size = rb->read(fd, snes_lite.rom_data, (size_t)size);
    rb->close(fd);
    if (read_size != size)
    {
        rb->splash(HZ * 2, "Could not read complete ROM");
        return false;
    }
    snes_lite.rom.size = (size_t)size;
    header_offset = (snes_lite.rom.size & 0x3ff) == 512 ? 512 : 0;
    if (snes_lite.rom.size >= header_offset + 0x8000)
        lo_score = header_score(snes_lite.rom_data + header_offset + 0x7fc0);
    if (snes_lite.rom.size >= header_offset + 0x10000)
        hi_score = header_score(snes_lite.rom_data + header_offset + 0xffc0);
    snes_lite.rom.hirom = hi_score > lo_score;
    if (snes_lite.rom.hirom &&
        snes_lite.rom.size < header_offset + 0x10000)
        snes_lite.rom.hirom = false;
    header = snes_lite.rom_data + header_offset +
             (snes_lite.rom.hirom ? 0xffc0 : 0x7fc0);
    for (i = 0; i < 21; i++)
    {
        unsigned char c = header[i];
        snes_lite.rom.title[i] = c >= 32 && c < 127 ? (char)c : ' ';
    }
    snes_lite.rom.title[21] = '\0';
    for (i = 20; i >= 0 && snes_lite.rom.title[i] == ' '; i--)
        snes_lite.rom.title[i] = '\0';
    snes_lite.rom.map_mode = header[0x15];
    snes_lite.rom.cartridge_type = header[0x16];
    snes_lite.rom.unsupported_chip = detect_special_chip(
        snes_lite.rom.map_mode, snes_lite.rom.cartridge_type);
    make_save_path(path);
    return true;
}

static void ensure_save_dirs(void)
{
    if (!rb->dir_exists(ROCKBOX_DIR "/saves"))
        rb->mkdir(ROCKBOX_DIR "/saves");
    if (!rb->dir_exists(SNES_LITE_SAVE_DIR))
        rb->mkdir(SNES_LITE_SAVE_DIR);
}

bool snes_lite_sram_load(void)
{
    size_t size;
    void *data = snes_lite_core_sram(&size);
    int fd;
    off_t file_size;

    if (!data || size == 0)
        return true;
    fd = rb->open(snes_lite.rom.save_path, O_RDONLY);
    if (fd < 0)
        return true;
    file_size = rb->filesize(fd);
    if (file_size > 0)
        rb->read(fd, data, MIN((size_t)file_size, size));
    rb->close(fd);
    return true;
}

bool snes_lite_sram_save(void)
{
    size_t size;
    void *data = snes_lite_core_sram(&size);
    int fd;
    bool ok;

    if (!data || size == 0)
        return true;
    ensure_save_dirs();
    fd = rb->open(snes_lite.rom.save_path,
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        rb->splash(HZ * 2, "Save path unavailable");
        return false;
    }
    ok = rb->write(fd, data, size) == (ssize_t)size;
    rb->close(fd);
    if (!ok)
        rb->splash(HZ * 2, "Could not write complete SRAM");
    return ok;
}
