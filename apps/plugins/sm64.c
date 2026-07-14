/***************************************************************************
 * SM64 overlay loader.  The game image is intentionally kept out of the
 * three-megabyte plugin buffer and loaded into the shared game/audio arena.
 ****************************************************************************/
#include "plugin.h"
#include "lib/overlay.h"

#define SM64_LOADER_DIR PLUGIN_GAMES_DATA_DIR "/sm64"
#define SM64_LOADER_LOG SM64_LOADER_DIR "/loader.log"

static void loader_mark(const char *message, int status)
{
    int fd = rb->open(SM64_LOADER_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd, "%s status=%d\n", message, status);
        rb->close(fd);
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status;

    rb->mkdir(SM64_LOADER_DIR);
    rb->remove(SM64_LOADER_LOG);
    loader_mark("loader enter", 0);
    status = run_overlay(parameter, PLUGIN_GAMES_DIR "/sm64.ovl",
                         "Super Mario 64");
    loader_mark("loader return", status);
    return status;
}
