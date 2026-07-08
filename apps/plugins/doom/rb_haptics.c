#include "plugin.h"
#include "rb_haptics.h"

static void rb_haptics_pulse(int duration_ms, int strength)
{
    if (!rb->haptic_feedback_enabled)
        return;

    if (!rb->haptic_feedback_enabled())
        return;

    rb->haptic_feedback(duration_ms, strength);
}

void rb_haptics_menu_move(void)
{
    rb_haptics_pulse(14, 22);
}

void rb_haptics_menu_select(void)
{
    rb_haptics_pulse(30, 48);
}

void rb_haptics_weapon_fire(int strength)
{
    strength = strength < 1 ? 1 : strength;
    strength = strength > 100 ? 100 : strength;

    rb_haptics_pulse(16 + strength / 4, strength);
}

void rb_haptics_player_hurt(int damage)
{
    int strength = damage * 4;

    if (strength < 20)
        strength = 20;
    if (strength > 100)
        strength = 100;

    rb_haptics_pulse(20 + damage / 2, strength);
}
