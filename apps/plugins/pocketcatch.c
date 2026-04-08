/* PocketCatch: MVP skeleton plugin for Rockbox 5G iPod */
#include "plugin.h"

/* Plugin entry point */
enum plugin_status plugin_start(const void* parameter)
{
    (void)parameter;
    /* Simple debug/proof-of-life splash */
    rb->splash(HZ*2, "PocketCatch MVP");
    return PLUGIN_OK;
}
