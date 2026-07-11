#include "plugin.h"
#include "smsgg_haptics.h"

static bool haptics_enabled;
static int haptics_strength;

void smsgg_haptic_set_enabled(bool enabled, int strength)
{
    haptics_enabled = enabled;
    haptics_strength = strength;
}

static void pulse(int duration, int strength)
{
    if (!haptics_enabled)
        return;

    if (rb->haptic_feedback_enabled == NULL || rb->haptic_feedback == NULL)
        return;

    if (!rb->haptic_feedback_enabled())
        return;

    rb->haptic_feedback(duration, (strength * haptics_strength) / 100);
}

void smsgg_haptic_button(void) { pulse(10, 35); }
void smsgg_haptic_menu(void) { pulse(12, 25); }
void smsgg_haptic_pause(void) { pulse(30, 50); }
void smsgg_haptic_save(void) { pulse(35, 65); }
void smsgg_haptic_load(void) { pulse(35, 55); }
void smsgg_haptic_damage(void) { pulse(60, 80); }
void smsgg_haptic_collect(void) { pulse(8, 25); }
void smsgg_haptic_collision(void) { pulse(45, 70); }
