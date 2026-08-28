/***************************************************************************
 * Zelda3 overlay loader.  Copyrighted game data is never linked into the
 * loader or overlay; zelda3_assets.dat must be extracted by the user.
 ****************************************************************************/
#include "plugin.h"
#include "lib/overlay.h"

enum plugin_status plugin_start(const void *parameter)
{
    return run_overlay(parameter, PLUGIN_GAMES_DIR "/zelda3.ovl",
                       "A Link to the Past");
}
