#include "tamagotchi_rom.h"

static char rom_error[96];

const char *tamagotchi_rom_error(void)
{
    return rom_error;
}

static bool read_exact(int fd, unsigned char *buf, size_t size)
{
    size_t done = 0;

    while (done < size)
    {
        ssize_t got = rb->read(fd, buf + done, size - done);

        if (got <= 0)
            return false;
        done += got;
    }

    return true;
}

bool tamagotchi_rom_load(u12_t *program, size_t max_words, size_t *out_words)
{
    int fd;
    off_t size;
    unsigned char pair[2];
    size_t i;
    const char *path = TAMAGOTCHI_GAME_ROM_PATH;

    rb->snprintf(rom_error, sizeof(rom_error),
                 "Missing tama.b. Put your legally obtained Tamagotchi P1 "
                 "ROM at .rockbox/games/tamagotchi/roms/tama.b");

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        path = TAMAGOTCHI_ROM_PATH;
        fd = rb->open(path, O_RDONLY);
        rb->snprintf(rom_error, sizeof(rom_error),
                     "Missing tama.b. Put your legally obtained Tamagotchi P1 "
                     "ROM at .rockbox/games/tamagotchi/roms/tama.b or "
                     ".rockbox/apps/tamagotchi/roms/tama.b");
    }
    if (fd < 0)
        return false;

    size = rb->filesize(fd);
    if (size != TAMA_ROM_BYTES || max_words < TAMA_ROM_WORDS)
    {
        rb->close(fd);
        rb->snprintf(rom_error, sizeof(rom_error),
                     "Invalid tama.b: expected %d bytes", TAMA_ROM_BYTES);
        return false;
    }

    for (i = 0; i < TAMA_ROM_WORDS; i++)
    {
        if (!read_exact(fd, pair, sizeof(pair)))
        {
            rb->close(fd);
            rb->snprintf(rom_error, sizeof(rom_error), "Could not read tama.b");
            return false;
        }
        program[i] = (u12_t)(pair[1] | ((pair[0] & 0x0f) << 8));
    }

    rb->close(fd);
    *out_words = TAMA_ROM_WORDS;
    return true;
}
