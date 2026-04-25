#include "pocketcatch.h"

void pc_world_input_handle_event(long event, struct pc_world_command *command)
{
    rb->memset(command, 0, sizeof(*command));

    if ((event & BUTTON_MENU) && (event & BUTTON_SELECT))
    {
        command->exit_requested = true;
        return;
    }

    if ((event & BUTTON_SELECT) && (event & BUTTON_REPEAT))
    {
        command->menu_requested = true;
        return;
    }

    if ((event & BUTTON_SELECT) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
    {
        command->confirm = true;
        return;
    }

    if ((event & BUTTON_LEFT) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
    {
        command->nav_x = -1;
        command->back = true;
    }
    else if ((event & BUTTON_RIGHT) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
    {
        command->nav_x = 1;
    }
    else if ((event & BUTTON_MENU) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
    {
        command->nav_y = -1;
    }
    else if ((event & BUTTON_PLAY) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
    {
        command->nav_y = 1;
    }
}
