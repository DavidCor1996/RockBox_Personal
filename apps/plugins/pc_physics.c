#include "pc_physics.h"
// Lightweight placeholder implementation
static float g_spin = 0.0f;

void pc_physics_init(void)
{
    g_spin = 0.0f;
}

float pc_physics_step(float dt)
{
    (void)dt;
    // No real physics yet; advance spin trivially
    g_spin += 0.0f;
    return g_spin;
}
