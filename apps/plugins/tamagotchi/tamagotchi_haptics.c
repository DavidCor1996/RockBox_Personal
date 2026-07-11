#include "tamagotchi_haptics.h"

static const struct tamagotchi_settings *haptic_settings;
static long last_attention_tick;

void tamagotchi_haptics_set_settings(const struct tamagotchi_settings *settings)
{
    haptic_settings = settings;
}

static void pulse(int duration, int strength, bool attention)
{
    long now = *rb->current_tick;

    if (haptic_settings == NULL || !haptic_settings->haptics_enabled ||
        haptic_settings->quiet_mode)
        return;

    if (attention && !haptic_settings->attention_haptics)
        return;

    if (attention && TIME_BEFORE(now, last_attention_tick + HZ * 3))
        return;

    if (rb->haptic_feedback_enabled == NULL || rb->haptic_feedback == NULL)
        return;

    if (!rb->haptic_feedback_enabled())
        return;

    rb->haptic_feedback(duration, strength);

    if (attention)
        last_attention_tick = now;
}

void tamagotchi_haptic_button(void) { pulse(10, 30, false); }
void tamagotchi_haptic_wheel(void) { pulse(7, 22, false); }
void tamagotchi_haptic_attention(void) { pulse(55, 75, true); }
void tamagotchi_haptic_save(void) { pulse(18, 45, false); pulse(18, 45, false); }
void tamagotchi_haptic_load(void) { pulse(18, 40, false); pulse(18, 40, false); }
void tamagotchi_haptic_error(void) { pulse(70, 80, false); }
