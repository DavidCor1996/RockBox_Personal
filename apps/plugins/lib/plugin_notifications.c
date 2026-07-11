/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____ ____  |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \\__  \ |  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )/ __ \|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/(____  /__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/
 ****************************************************************************/

#include "plugin_notifications.h"

#define NOTIFY_HAPTIC_COOLDOWN (HZ * 3)

static struct plugin_notification current;
static int next_id = 1;
static long last_haptic_tick;

static long ms_to_ticks(int ttl_ms)
{
    long ticks;

    if (ttl_ms <= 0)
        ttl_ms = PLUGIN_NOTIFY_DEFAULT_TTL_MS;

    ticks = (long)(((long long)ttl_ms * HZ + 999) / 1000);
    return ticks > 0 ? ticks : 1;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0)
        return;

    if (src == NULL)
        src = "";

    rb->strlcpy(dst, src, dst_size);
}

static void notify_haptic(enum plugin_notify_priority priority)
{
    int duration = 10;
    int strength = 25;
    long now = *rb->current_tick;

    if (TIME_BEFORE(now, last_haptic_tick + NOTIFY_HAPTIC_COOLDOWN))
        return;

    if (rb->haptic_feedback_enabled == NULL || rb->haptic_feedback == NULL)
        return;

    if (!rb->haptic_feedback_enabled())
        return;

    switch (priority)
    {
        case PLUGIN_NOTIFY_LOW:
            duration = 8;
            strength = 25;
            break;
        case PLUGIN_NOTIFY_NORMAL:
            duration = 18;
            strength = 45;
            break;
        case PLUGIN_NOTIFY_HIGH:
            duration = 45;
            strength = 70;
            break;
        case PLUGIN_NOTIFY_CRITICAL:
        default:
            duration = 80;
            strength = 90;
            break;
    }

    rb->haptic_feedback(duration, strength);
    last_haptic_tick = now;
}

void plugin_notify_init(void)
{
    rb->memset(&current, 0, sizeof(current));
    next_id = 1;
    last_haptic_tick = 0;
}

int plugin_notify_post(const char *source, const char *title, const char *body,
                       enum plugin_notify_priority priority, unsigned flags,
                       int ttl_ms)
{
    long now = *rb->current_tick;

    rb->memset(&current, 0, sizeof(current));
    current.id = next_id++;
    current.priority = priority;
    current.flags = flags | PLUGIN_NOTIFY_SHOW_BANNER;
    current.posted_tick = now;
    current.expire_tick = now + ms_to_ticks(ttl_ms);
    current.active = true;

    copy_text(current.source, sizeof(current.source), source);
    copy_text(current.title, sizeof(current.title), title);
    copy_text(current.body, sizeof(current.body), body);

    if (flags & PLUGIN_NOTIFY_VIBRATE)
        notify_haptic(priority);

    return current.id;
}

int plugin_notify_post_simple(const char *source, const char *body)
{
    return plugin_notify_post(source, source, body, PLUGIN_NOTIFY_NORMAL,
                              PLUGIN_NOTIFY_SHOW_BANNER,
                              PLUGIN_NOTIFY_DEFAULT_TTL_MS);
}

void plugin_notify_clear_source(const char *source)
{
    if (current.active && source != NULL &&
        rb->strcmp(current.source, source) == 0)
        current.active = false;
}

void plugin_notify_dismiss(int id)
{
    if (id == 0 || current.id == id)
        current.active = false;
}

bool plugin_notify_active(void)
{
    if (current.active && TIME_AFTER(*rb->current_tick, current.expire_tick))
        current.active = false;

    return current.active;
}

bool plugin_notify_handle_button(int button)
{
    int bare = button & ~(BUTTON_REL | BUTTON_REPEAT);

    if (!plugin_notify_active())
        return false;

#ifdef BUTTON_MENU
    if (bare == BUTTON_MENU)
    {
        current.active = false;
        return true;
    }
#endif
#ifdef BUTTON_SELECT
    if (bare == BUTTON_SELECT &&
        (current.flags & PLUGIN_NOTIFY_DISMISS_ON_SELECT))
    {
        current.active = false;
        return true;
    }
#endif

    return false;
}

static void draw_banner(int x, int y, int w, int h)
{
#if LCD_DEPTH > 1
    unsigned fg = rb->lcd_get_foreground();
    unsigned bg = rb->lcd_get_background();

    rb->lcd_set_foreground(LCD_RGBPACK(116, 116, 116));
    rb->lcd_fillrect(x + 2, y + 2, w, h);
    rb->lcd_set_foreground(LCD_RGBPACK(238, 238, 238));
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(LCD_RGBPACK(250, 250, 250));
    rb->lcd_fillrect(x + 1, y + 1, w - 2, h / 2);
    rb->lcd_set_foreground(LCD_RGBPACK(84, 84, 84));
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_set_foreground(LCD_RGBPACK(0, 0, 0));
#else
    rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_drawrect(x, y, w, h);
#endif

    rb->lcd_drawrect(x + 8, y + 10, 12, 12);
    rb->lcd_fillrect(x + 11, y + 14, 2, 2);
    rb->lcd_fillrect(x + 16, y + 14, 2, 2);
    rb->lcd_hline(x + 12, x + 17, y + 19);

    rb->lcd_putsxy(x + 28, y + 6, (const unsigned char *)current.title);
    rb->lcd_putsxy(x + 28, y + 21, (const unsigned char *)current.body);

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(fg);
    rb->lcd_set_background(bg);
#endif
}

void plugin_notify_render_overlay(void)
{
    int x = 8;
    int y = 7;
    int w = LCD_WIDTH - 16;
    int h = 42;

    if (!plugin_notify_active())
        return;

    if (w < 80 || h > LCD_HEIGHT)
        return;

    draw_banner(x, y, w, h);
}
