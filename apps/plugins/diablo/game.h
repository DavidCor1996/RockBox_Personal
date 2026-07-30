#ifndef DIABLO_GAME_H
#define DIABLO_GAME_H

#include "plugin.h"

/* Runs the whole gameplay loop (town + level-1 dungeon, monsters,
 * inventory, save/load) until the player exits. Loads a save file if
 * one exists, otherwise starts a fresh game in town. Returns false only
 * on an unrecoverable asset-loading failure. */
bool game_run(void);

#endif
