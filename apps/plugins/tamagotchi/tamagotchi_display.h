#ifndef TAMAGOTCHI_DISPLAY_H
#define TAMAGOTCHI_DISPLAY_H

#include "tamagotchi.h"

void tamagotchi_display_init(void);
void tamagotchi_display_set_pixel(int x, int y, bool value);
void tamagotchi_display_set_icon(int icon, bool value);
void tamagotchi_display_set_selected_icon(int icon);
void tamagotchi_display_render(const struct tamagotchi_settings *settings);
void tamagotchi_display_show_loading(const char *status);
bool tamagotchi_display_is_dirty(void);
void tamagotchi_display_mark_dirty(void);

#endif
