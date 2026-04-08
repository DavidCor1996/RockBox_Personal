#include "pc_input.h"
#include "plugin.h"

void pc_input_init(void)
{
    /* TODO: initialize wheel/button sampling */
}

/* Ensure a valid plugin entry point for simulator builds */
enum plugin_status plugin_start(const void* parameter) { (void)parameter; return PLUGIN_OK; }
