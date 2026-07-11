/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____ ____  |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \\__  \ |  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )/ __ \|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/(____  /__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/
 ****************************************************************************/

#ifndef PLUGIN_NOTIFICATIONS_H
#define PLUGIN_NOTIFICATIONS_H

#include "plugin.h"

#define PLUGIN_NOTIFY_TITLE_LEN 32
#define PLUGIN_NOTIFY_BODY_LEN 96
#define PLUGIN_NOTIFY_DEFAULT_TTL_MS 5000

enum plugin_notify_priority
{
    PLUGIN_NOTIFY_LOW = 0,
    PLUGIN_NOTIFY_NORMAL,
    PLUGIN_NOTIFY_HIGH,
    PLUGIN_NOTIFY_CRITICAL,
};

enum plugin_notify_flags
{
    PLUGIN_NOTIFY_SHOW_BANNER = 0x0001,
    PLUGIN_NOTIFY_VIBRATE = 0x0002,
    PLUGIN_NOTIFY_DISMISS_ON_SELECT = 0x0004,
};

struct plugin_notification
{
    int id;
    char source[24];
    char title[PLUGIN_NOTIFY_TITLE_LEN];
    char body[PLUGIN_NOTIFY_BODY_LEN];
    enum plugin_notify_priority priority;
    unsigned flags;
    long posted_tick;
    long expire_tick;
    bool active;
};

void plugin_notify_init(void);
int plugin_notify_post(const char *source, const char *title, const char *body,
                       enum plugin_notify_priority priority, unsigned flags,
                       int ttl_ms);
int plugin_notify_post_simple(const char *source, const char *body);
void plugin_notify_clear_source(const char *source);
void plugin_notify_dismiss(int id);
bool plugin_notify_handle_button(int button);
void plugin_notify_render_overlay(void);
bool plugin_notify_active(void);

#endif
