#ifndef SMSGG_ROM_H
#define SMSGG_ROM_H

#include "plugin.h"

#define SMSGG_MAX_ROMS 128

struct smsgg_rom_entry
{
    char name[MAX_PATH];
    char path[MAX_PATH];
    bool has_state;
};

struct smsgg_rom_list
{
    struct smsgg_rom_entry entries[SMSGG_MAX_ROMS];
    int count;
};

bool smsgg_rom_scan(struct smsgg_rom_list *list);
int smsgg_rom_select(struct smsgg_rom_list *list, const char *last_rom);
void smsgg_rom_build_save_paths(const char *rom_path, uint32_t crc,
                                char *sram_path, size_t sram_size,
                                char *state_path, size_t state_size);

#endif
