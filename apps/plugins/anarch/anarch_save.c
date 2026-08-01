#include "anarch_platform.h"

#include <fcntl.h>

#define SAVE_DIR ROCKBOX_DIR "/games/anarch"
#define SAVE_FILE SAVE_DIR "/anarch.sav"
#define SAVE_TEMP SAVE_DIR "/anarch.sav.tmp"
#define SAVE_MAGIC 0x31524e41u
#define SAVE_VERSION 1

struct anarch_save_file {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_length;
    uint8_t upstream_commit[20];
    uint32_t crc32;
    uint8_t payload[ANARCH_SAVE_SIZE];
} __attribute__((packed));

static const uint8_t upstream_commit[20] = {
    0x6f, 0x90, 0x56, 0x21, 0x61, 0x20, 0x06, 0x82, 0x45, 0x9e,
    0x77, 0x2f, 0x1d, 0xac, 0xb7, 0x47, 0xf2, 0x3c, 0x5f, 0x95
};
static bool corrupt_warning;

void anarch_save_init(void)
{
    rb->mkdir(ROCKBOX_DIR "/games");
    rb->mkdir(SAVE_DIR);
    corrupt_warning = false;
}

void anarch_save_write(const uint8_t data[ANARCH_SAVE_SIZE])
{
    struct anarch_save_file save;
    struct anarch_save_file verify;
    int fd;

    rb->memset(&save, 0, sizeof(save));
    save.magic = SAVE_MAGIC;
    save.version = SAVE_VERSION;
    save.payload_length = ANARCH_SAVE_SIZE;
    rb->memcpy(save.upstream_commit, upstream_commit,
               sizeof(save.upstream_commit));
    rb->memcpy(save.payload, data, ANARCH_SAVE_SIZE);
    save.crc32 = rb->crc_32(save.payload, ANARCH_SAVE_SIZE, 0xffffffffu);

    fd = rb->open(SAVE_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    if (rb->write(fd, &save, sizeof(save)) != (ssize_t)sizeof(save))
    {
        rb->close(fd);
        rb->remove(SAVE_TEMP);
        return;
    }
    rb->close(fd);
    fd = rb->open(SAVE_TEMP, O_RDONLY);
    if (fd < 0 || rb->read(fd, &verify, sizeof(verify)) !=
        (ssize_t)sizeof(verify) ||
        rb->memcmp(&save, &verify, sizeof(save)) != 0)
    {
        if (fd >= 0)
            rb->close(fd);
        rb->remove(SAVE_TEMP);
        return;
    }
    rb->close(fd);
    if (rb->rename(SAVE_TEMP, SAVE_FILE) < 0)
        rb->remove(SAVE_TEMP);
    else
        corrupt_warning = false;
}

uint8_t anarch_save_read(uint8_t data[ANARCH_SAVE_SIZE])
{
    struct anarch_save_file save;
    int fd = rb->open(SAVE_FILE, O_RDONLY);

    if (fd < 0)
        return 1;
    if (rb->read(fd, &save, sizeof(save)) != (ssize_t)sizeof(save))
        corrupt_warning = true;
    rb->close(fd);
    if (corrupt_warning || save.magic != SAVE_MAGIC ||
        save.version != SAVE_VERSION ||
        save.payload_length != ANARCH_SAVE_SIZE ||
        rb->memcmp(save.upstream_commit, upstream_commit,
                   sizeof(upstream_commit)) != 0 ||
        save.crc32 != rb->crc_32(save.payload, ANARCH_SAVE_SIZE, 0xffffffffu))
    {
        corrupt_warning = true;
        return 1;
    }
    rb->memcpy(data, save.payload, ANARCH_SAVE_SIZE);
    return 1;
}

bool anarch_save_warning(void)
{
    return corrupt_warning;
}

#ifdef SIMULATOR
bool anarch_save_selftest(void)
{
    uint8_t payload[ANARCH_SAVE_SIZE];
    uint8_t loaded[ANARCH_SAVE_SIZE];
    uint8_t corrupt = 0xff;
    int fd;
    int i;

    rb->remove(SAVE_TEMP);
    rb->remove(SAVE_FILE);
    anarch_save_init();
    for (i = 0; i < ANARCH_SAVE_SIZE; ++i)
        payload[i] = (uint8_t)(i * 17 + 3);
    rb->memset(loaded, 0, sizeof(loaded));
    anarch_save_write(payload);
    if (anarch_save_warning() || !anarch_save_read(loaded) ||
        rb->memcmp(payload, loaded, sizeof(payload)) != 0 ||
        rb->file_exists(SAVE_TEMP))
        return false;

    fd = rb->open(SAVE_FILE, O_WRONLY);
    if (fd < 0 || rb->lseek(fd, -1, SEEK_END) < 0 ||
        rb->write(fd, &corrupt, 1) != 1)
    {
        if (fd >= 0)
            rb->close(fd);
        return false;
    }
    rb->close(fd);
    anarch_save_init();
    rb->memset(loaded, 0xa5, sizeof(loaded));
    if (!anarch_save_read(loaded) || !anarch_save_warning())
        return false;
    for (i = 0; i < ANARCH_SAVE_SIZE; ++i)
        if (loaded[i] != 0xa5)
            return false;
    return rb->file_exists(SAVE_FILE) && !rb->file_exists(SAVE_TEMP);
}
#endif
