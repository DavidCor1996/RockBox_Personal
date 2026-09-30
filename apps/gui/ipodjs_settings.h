#ifndef IPODJS_SETTINGS_H
#define IPODJS_SETTINGS_H

#include "screen_access.h"

struct ipodjs_about_data {
    char used[24], free[24];
    char serial[40], model[40], version[64];
    char count[7][16], space[4][24];
    unsigned percent[4]; /* hundredths of one percent of the volume */
    unsigned app_count;
    bool inventory;
};

void ipodjs_settings_prepare(void);
bool ipodjs_settings_dialog_available(struct screen *display);
void ipodjs_about_snapshot(struct ipodjs_about_data *data);
void ipodjs_about_draw(struct screen *display,
    const struct ipodjs_about_data *data, int page, int app_top);
void ipodjs_about_scroll(struct screen *display);
bool ipodjs_settings_draw_button(struct screen *display,
    int x, int y, int width, bool active, const char *label);
bool ipodjs_settings_draw_notice(struct screen *display,
    int x, int y, int width, int height);
bool ipodjs_settings_draw_confirmation(struct screen *display,
    const char *const *lines, int count, const char *accept,
    const char *cancel, int default_button, int seconds);
bool ipodjs_settings_draw_message(struct screen *display,
    const char *const *lines, int count);

#endif
