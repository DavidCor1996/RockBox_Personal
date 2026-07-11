#ifndef TAMAGOTCHI_ROM_H
#define TAMAGOTCHI_ROM_H

#include "tamagotchi.h"
#include "upstream/tamalib/hal_types.h"

bool tamagotchi_rom_load(u12_t *program, size_t max_words, size_t *out_words);
const char *tamagotchi_rom_error(void);

#endif
