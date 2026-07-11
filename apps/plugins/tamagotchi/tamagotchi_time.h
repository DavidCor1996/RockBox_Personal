#ifndef TAMAGOTCHI_TIME_H
#define TAMAGOTCHI_TIME_H

#include "tamagotchi.h"

void tamagotchi_time_on_launch(void);
void tamagotchi_time_on_exit(void);
void tamagotchi_time_schedule_alert(unsigned seconds, const char *body);

#endif
