#ifndef TAMAGOTCHI_INPUT_H
#define TAMAGOTCHI_INPUT_H

#include "tamagotchi.h"

struct tamagotchi_input_state
{
    bool a;
    bool b;
    bool c;
    bool menu_requested;
    bool quit_requested;
    bool usb_requested;
    int selected_icon;
};

void tamagotchi_input_init(void);
void tamagotchi_input_poll(const struct tamagotchi_settings *settings,
                           struct tamagotchi_input_state *state);

#endif
