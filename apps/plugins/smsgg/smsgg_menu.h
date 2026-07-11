#ifndef SMSGG_MENU_H
#define SMSGG_MENU_H

#include "plugin.h"
#include "smsgg_state.h"

enum smsgg_menu_action
{
    SMSGG_MENU_RESUME = 0,
    SMSGG_MENU_SELECT_ROM,
    SMSGG_MENU_RESET,
    SMSGG_MENU_SAVE_STATE,
    SMSGG_MENU_LOAD_STATE,
    SMSGG_MENU_SEND_START,
    SMSGG_MENU_SEND_PAUSE,
    SMSGG_MENU_QUIT
};

enum smsgg_menu_action smsgg_menu_run(struct smsgg_settings *settings);

#endif
