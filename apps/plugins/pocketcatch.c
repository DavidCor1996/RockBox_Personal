/* PocketCatch: MVP skeleton plugin for Rockbox 5G iPod */
/* PocketCatch MVP skeleton with staged hooks for future implementation */
#include "plugin.h"

/* Include MVP module interfaces (to be fleshed out) */
#include "pc_input.h"
#include "pc_physics.h"
#include "pc_state.h"
#include "pc_render.h"
#include "pc_assets.h"

/* Minimal MVP initializer that wires in future modules (no-op for now) */
static void pocketcatch_mvp_init(void)
{
    pocketcatch_input_init();
    pocketcatch_physics_init();
    pocketcatch_state_init();
    pocketcatch_render();
    pocketcatch_load_assets();
}

/* Plugin entry point */
enum plugin_status plugin_start(const void* parameter)
{
    (void)parameter;
    /* Initialize MVP hooks (no-ops for now) */
    pocketcatch_mvp_init();
    /* Simple debug/proof-of-life splash */
    rb->splash(HZ*2, "PocketCatch MVP");
    return PLUGIN_OK;
}
