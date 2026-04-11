#include "pc_state.h"
static int g_state = 0;
void pc_state_init(void)
{
    g_state = 0;
}
void pc_state_step(float dt)
{
    (void)dt;
    // Placeholder state progression
    g_state = (g_state + 1) % 4;
}
