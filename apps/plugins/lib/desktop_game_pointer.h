/* Desktop Mode-compatible pointer input for explicitly desktop-launched games.
 * No audio ownership; native sampling never reads the filesystem. */
#ifndef DESKTOP_GAME_POINTER_H
#define DESKTOP_GAME_POINTER_H
#include "plugin.h"
#include "fixedpoint.h"

struct desktop_game_pointer
{
    int x, y, buttons, previous_buttons;
    int wheel, fraction_x, fraction_y, velocity, direction;
    long tick, contact_tick, host_tick;
    bool armed;
};

static bool desktop_game_pointer_poll(struct desktop_game_pointer *p)
{
    long now = *rb->current_tick;
    int old_x = p->x, old_y = p->y;
    p->previous_buttons = p->buttons;
#ifdef SIMULATOR
    if (!TIME_BEFORE(now, p->host_tick + MAX(1, HZ / 60)))
    {
        char record[14];
        p->host_tick = now;
        int fd = rb->open(ROCKBOX_DIR "/host-pointer", O_RDONLY);
        if (fd >= 0)
        {
            int got = rb->read(fd, record, 13);
            rb->close(fd);
            bool valid = got == 13 && record[4] == ' ' &&
                         record[9] == ' ' && record[12] == '\n';
            for (int i = 0; valid && i < 12; i++)
                if (i != 4 && i != 9 &&
                    (record[i] < '0' || record[i] > '9')) valid = false;
            if (valid)
            {
                record[4] = record[9] = record[12] = '\0';
                p->x = MIN(LCD_WIDTH - 1, rb->atoi(record));
                p->y = MIN(LCD_HEIGHT - 1, rb->atoi(record + 5));
                p->buttons = rb->atoi(record + 10) & 3;
                /* Do not inherit the click which launched the game. */
                if (!p->armed)
                {
                    if (!p->buttons) p->armed = true;
                    p->previous_buttons = p->buttons;
                }
                return p->x != old_x || p->y != old_y ||
                       p->buttons != p->previous_buttons;
            }
        }
    }
#endif
#ifdef HAVE_WHEEL_POSITION
    int wheel = rb->wheel_status();
    long held = rb->button_status();
    if (wheel < 0 || (held & (BUTTON_SELECT | BUTTON_MENU | BUTTON_PLAY |
                              BUTTON_LEFT | BUTTON_RIGHT)))
    {
        p->wheel = -1;
        p->fraction_x = p->fraction_y = p->velocity = 0;
        p->tick = p->contact_tick = now;
        return false;
    }
    int angle = wheel * 360 / 96 - 90;
    if (p->wheel < 0)
    {
        p->x += fp14_cos(angle) > 4096 ? 1 :
                fp14_cos(angle) < -4096 ? -1 : 0;
        p->y += fp14_sin(angle) > 4096 ? 1 :
                fp14_sin(angle) < -4096 ? -1 : 0;
        p->contact_tick = now;
    }
    else if (wheel == p->wheel)
    {
        long ticks = MAX(0, MIN(now - p->tick, HZ));
        int rate = MIN(14, 5 + (now - p->contact_tick) * 9 /
                                  MAX(1, HZ * 3 / 4));
        p->fraction_x += fp14_cos(angle) * rate * ticks / 16384;
        p->fraction_y += fp14_sin(angle) * rate * ticks / 16384;
    }
    else
    {
        int delta = wheel - p->wheel;
        if (delta > 48) delta -= 96;
        if (delta < -48) delta += 96;
        int direction = delta < 0 ? -1 : 1;
        long ticks = MAX(1, now - p->tick);
        if (direction != p->direction || ticks > HZ / 4) p->velocity = 0;
        p->velocity = MIN(4, (p->velocity * 2 +
            MIN(4, abs(delta) * HZ / ticks / 12)) / 3);
        int gain = 32 + 12 * MIN(8, MAX(0, abs(delta) - 4)) / 8 +
                   p->velocity * 6;
        int previous = p->wheel * 360 / 96 - 90;
        p->fraction_x += (fp14_cos(angle) - fp14_cos(previous)) * gain / 4096;
        p->fraction_y += (fp14_sin(angle) - fp14_sin(previous)) * gain / 4096;
        p->direction = direction;
        p->contact_tick = now;
    }
    int dx = MAX(-6, MIN(6, p->fraction_x / 16));
    int dy = MAX(-6, MIN(6, p->fraction_y / 16));
    p->fraction_x -= dx * 16;
    p->fraction_y -= dy * 16;
    p->x = MAX(0, MIN(LCD_WIDTH - 1, p->x + dx));
    p->y = MAX(0, MIN(LCD_HEIGHT - 1, p->y + dy));
    p->wheel = wheel;
    p->tick = now;
#endif
    return p->x != old_x || p->y != old_y;
}
#endif
