/***************************************************************************
 * Animal Crossing overlay loader.
 *
 * Game data is never linked into this loader or the overlay. The user-created
 * asset pack remains a separate, ignored runtime file.
 ****************************************************************************/
#include "plugin.h"
#include "lib/overlay.h"

enum plugin_status plugin_start(const void *parameter)
{
    return run_overlay(parameter,
                       PLUGIN_GAMES_DIR "/animalcrossing.ovl",
                       "Animal Crossing");
}
