#include "plugin.h"

enum plugin_status plugin_start(const void* parameter)
{
    (void)parameter;
    rb->splash(HZ*2, "PocketCatch MVP");
    return PLUGIN_OK;
}
