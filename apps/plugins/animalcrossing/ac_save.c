#include "ac_demake.h"

#define AC_SAVE_MAGIC 0x31544341u /* ACT1 */
#define AC_SAVE_VERSION 1

struct ac_save_file {
    uint32_t magic;
    uint32_t version;
    uint32_t payload_size;
    uint32_t crc32;
    uint32_t town_seed;
    int16_t player_x;
    int16_t player_y;
    uint32_t bells;
    uint16_t inventory[AC_INVENTORY_SIZE];
    struct ac_point trees[AC_TREE_COUNT];
    struct ac_villager villagers[AC_VILLAGER_COUNT];
} __attribute__((packed));

static uint32_t ac_save_crc(const struct ac_save_file *save)
{
    return rb->crc_32(&save->town_seed,
                      sizeof(*save) - offsetof(struct ac_save_file, town_seed),
                      0xffffffffu);
}

bool ac_save_load(struct ac_state *state)
{
    struct ac_save_file save;
    int fd = rb->open(AC_SAVE_PATH, O_RDONLY);

    if (fd < 0)
        return false;
    if (rb->read(fd, &save, sizeof(save)) != (ssize_t)sizeof(save))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (save.magic != AC_SAVE_MAGIC ||
        save.version != AC_SAVE_VERSION ||
        save.payload_size != sizeof(save) ||
        save.crc32 != ac_save_crc(&save) ||
        save.player_x < 0 || save.player_x >= AC_WORLD_W ||
        save.player_y < 0 || save.player_y >= AC_WORLD_H)
        return false;

    state->town_seed = save.town_seed;
    state->player_x = save.player_x;
    state->player_y = save.player_y;
    state->bells = save.bells;
    rb->memcpy(state->inventory, save.inventory, sizeof(state->inventory));
    rb->memcpy(state->trees, save.trees, sizeof(state->trees));
    rb->memcpy(state->villagers, save.villagers,
               sizeof(state->villagers));
    state->dirty = false;
    return true;
}

bool ac_save_write(const struct ac_state *state)
{
    static const char temporary[] = AC_SAVE_PATH ".tmp";
    struct ac_save_file save;
    int fd;

    rb->memset(&save, 0, sizeof(save));
    save.magic = AC_SAVE_MAGIC;
    save.version = AC_SAVE_VERSION;
    save.payload_size = sizeof(save);
    save.town_seed = state->town_seed;
    save.player_x = state->player_x;
    save.player_y = state->player_y;
    save.bells = state->bells;
    rb->memcpy(save.inventory, state->inventory, sizeof(save.inventory));
    rb->memcpy(save.trees, state->trees, sizeof(save.trees));
    rb->memcpy(save.villagers, state->villagers,
               sizeof(save.villagers));
    save.crc32 = ac_save_crc(&save);

    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    if (rb->write(fd, &save, sizeof(save)) != (ssize_t)sizeof(save))
    {
        rb->close(fd);
        rb->remove(temporary);
        return false;
    }
    rb->close(fd);
    if (rb->rename(temporary, AC_SAVE_PATH) < 0)
    {
        rb->remove(temporary);
        return false;
    }
    return true;
}
