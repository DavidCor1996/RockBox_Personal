#ifndef ANIMALCROSSING_DEMAKE_H
#define ANIMALCROSSING_DEMAKE_H

#include "plugin.h"
#include <stdbool.h>
#include <stdint.h>

#define AC_DATA_DIR ROCKBOX_DIR "/animalcrossing"
#define AC_SAVE_DIR AC_DATA_DIR "/save"
#define AC_SAVE_PATH AC_SAVE_DIR "/town.dat"

#define AC_WORLD_W 48
#define AC_WORLD_H 36
#define AC_TREE_COUNT 28
#define AC_VILLAGER_COUNT 4
#define AC_INVENTORY_SIZE 8

enum ac_mode {
    AC_MODE_WORLD = 0,
    AC_MODE_DIALOGUE,
    AC_MODE_INVENTORY,
    AC_MODE_PAUSE
};

struct ac_point {
    int16_t x;
    int16_t y;
};

struct ac_villager {
    struct ac_point position;
    uint8_t kind;
};

struct ac_state {
    uint32_t town_seed;
    int16_t player_x;
    int16_t player_y;
    int16_t facing_x;
    int16_t facing_y;
    uint32_t bells;
    uint16_t inventory[AC_INVENTORY_SIZE];
    struct ac_point trees[AC_TREE_COUNT];
    struct ac_villager villagers[AC_VILLAGER_COUNT];
    enum ac_mode mode;
    int selected_item;
    int dialogue_villager;
    bool dirty;
    bool quit;
    bool usb;
    unsigned long frames;
};

void ac_state_new(struct ac_state *state);
bool ac_save_load(struct ac_state *state);
bool ac_save_write(const struct ac_state *state);
void ac_render(const struct ac_state *state);
enum plugin_status ac_demake_run(void);

#endif
