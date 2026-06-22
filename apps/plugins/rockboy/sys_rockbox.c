/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 Michiel van der Kolk, Jens Arnold
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "rockmacros.h"
#include "fb.h"
#include "input.h"
#include "lcd-gb.h"
#include "hw.h"
#include "config.h"
#include "profiler.h"

#ifdef SIMULATOR
#include <stdlib.h>
#define ROCKBOY_INPUT_SCRIPT_ENV "ROCKBOY_INPUT_SCRIPT"
#endif

#if CONFIG_KEYPAD == SANSA_E200_PAD || CONFIG_KEYPAD == SANSA_FUZE_PAD
#define ROCKBOY_SCROLLWHEEL
#define ROCKBOY_SCROLLWHEEL_CC  BUTTON_SCROLL_BACK
#define ROCKBOY_SCROLLWHEEL_CW  BUTTON_SCROLL_FWD
#endif

struct fb fb IBSS_ATTR;

extern int debug_trace;

#ifdef HAVE_WHEEL_POSITION
static int oldwheel = -1, wheel;

static const byte wheelmap_classic[8] = {
    PAD_UP,     /* Top */
    PAD_A,      /* Top-right */
    PAD_RIGHT,  /* Right */
    PAD_START,  /* Bottom-right */
    PAD_DOWN,   /* Bottom */
    PAD_SELECT, /* Bottom-left */
    PAD_LEFT,   /* Left */
    PAD_B       /* Top-left */
};

static const byte wheelmap_ipod5g[8] = {
    PAD_UP,
    PAD_UP | PAD_RIGHT,
    PAD_RIGHT,
    PAD_DOWN | PAD_RIGHT,
    PAD_DOWN,
    PAD_DOWN | PAD_LEFT,
    PAD_LEFT,
    PAD_UP | PAD_LEFT
};
#endif

#ifdef HAVE_WHEEL_POSITION
static void post_masked_event(event_t *ev, byte mask)
{
    static const byte pads[] = {
        PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
        PAD_A, PAD_B, PAD_START, PAD_SELECT
    };
    int i;

    for (i = 0; i < (int)(sizeof(pads) / sizeof(pads[0])); i++)
    {
        if (mask & pads[i])
        {
            ev->code = pads[i];
            ev_postevent(ev);
        }
    }
}
#endif

#ifdef SIMULATOR
static unsigned long scripted_input_frame;

static int scripted_input_matches(const char *name, int len, const char *literal)
{
    int i;

    for (i = 0; i < len && literal[i]; i++)
    {
        char c = name[i];
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if (c != literal[i])
            return 0;
    }

    return i == len && literal[i] == '\0';
}

static int scripted_input_pad(const char *name, int len)
{
    if (scripted_input_matches(name, len, "UP")) return PAD_UP;
    if (scripted_input_matches(name, len, "DOWN")) return PAD_DOWN;
    if (scripted_input_matches(name, len, "LEFT")) return PAD_LEFT;
    if (scripted_input_matches(name, len, "RIGHT")) return PAD_RIGHT;
    if (scripted_input_matches(name, len, "A")) return PAD_A;
    if (scripted_input_matches(name, len, "B")) return PAD_B;
    if (scripted_input_matches(name, len, "START")) return PAD_START;
    if (scripted_input_matches(name, len, "SELECT")) return PAD_SELECT;
    return -1;
}

static int scripted_input_event_type(const char *state, int len)
{
    char c;

    if (len <= 0)
        return EV_NONE;

    c = state[0];
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';

    if (c == 'P')
        return EV_PRESS;
    if (c == 'R')
        return EV_RELEASE;
    return EV_NONE;
}

static const char *scripted_input_next_token(const char *cursor)
{
    while (*cursor && *cursor != ',')
        cursor++;
    if (*cursor == ',')
        cursor++;
    return cursor;
}

static void scripted_input_post_due(void)
{
    const char *script = getenv(ROCKBOY_INPUT_SCRIPT_ENV);
    const char *cursor;
    char *end;

    scripted_input_frame++;
    if (!script || !script[0])
        return;

    cursor = script;
    while (*cursor)
    {
        unsigned long frame;
        const char *name;
        const char *state;
        int name_len;
        int state_len;
        int pad;
        int type;

        while (*cursor == ' ' || *cursor == ',')
            cursor++;
        if (!*cursor)
            break;

        frame = strtoul(cursor, &end, 10);
        if (end == cursor || *end != ':')
        {
            cursor = scripted_input_next_token(cursor);
            continue;
        }

        name = end + 1;
        cursor = name;
        while (*cursor && *cursor != ':' && *cursor != ',')
            cursor++;
        if (*cursor != ':')
        {
            cursor = scripted_input_next_token(cursor);
            continue;
        }
        name_len = cursor - name;

        state = cursor + 1;
        cursor = state;
        while (*cursor && *cursor != ',')
            cursor++;
        state_len = cursor - state;

        if (frame == scripted_input_frame)
        {
            pad = scripted_input_pad(name, name_len);
            type = scripted_input_event_type(state, state_len);
            if (pad >= 0 && type != EV_NONE)
            {
                event_t ev;
                ev.type = type;
                ev.code = pad;
                ev_postevent(&ev);
            }
        }

        if (*cursor == ',')
            cursor++;
    }
}
#endif

void ev_poll(void)
{
    event_t ev;

    static unsigned int oldbuttonstate;
    unsigned int buttons = BUTTON_NONE;
    unsigned int released, pressed;
    unsigned int btn;
    bool quit = false;

    do
    {
        btn = rb->button_get(false);
        /* BUTTON_NONE doesn't necessarily mean no button is pressed,
         * it just means the button queue became empty for this tick.
         * One can only be sure that no button is pressed by
         * calling button_status(). */
        if (btn == BUTTON_NONE)
        {
            /* loop only until all button events are popped off */
            quit = true;
            btn = rb->button_status();
        }
        buttons |= btn;
#if defined(HAVE_SCROLLWHEEL) && !defined(ROCKBOY_SCROLLWHEEL)
        /* filter out scroll wheel events if not supported */
        buttons &= ~(BUTTON_SCROLL_FWD|BUTTON_SCROLL_BACK);
#endif
    }
    while (!quit);

    released = ~buttons & oldbuttonstate;
    pressed = buttons & ~oldbuttonstate;

    oldbuttonstate = buttons;
#if (LCD_WIDTH == 160) && (LCD_HEIGHT == 128) && (LCD_DEPTH == 2)
    static unsigned int holdbutton;
    if (rb->button_hold()&~holdbutton)
        fb.mode=(fb.mode+1)%4;
    holdbutton=rb->button_hold();
#endif

#ifdef HAVE_WHEEL_POSITION
    /* Get the current wheel position - 0..95 or -1 for untouched */
    wheel = rb->wheel_status(); 

    /* Convert to number from 0 to 7 - clockwise from top */
    if ( wheel > 0 ){
        wheel += 6;
        wheel /= 12;
        if ( wheel > 7 ) wheel = 0; 
    }

    if ( wheel != oldwheel ) {
        const byte *wheelmap =
            (options.control_preset == ROCKBOY_CTRL_IPOD5G) ?
            wheelmap_ipod5g : wheelmap_classic;

        if (oldwheel >= 0) {
            ev.type = EV_RELEASE;
            post_masked_event(&ev, wheelmap[oldwheel]);
        }

        if (wheel >= 0) {
            ev.type = EV_PRESS;
            post_masked_event(&ev, wheelmap[wheel]);
        }
    }

    oldwheel = wheel;
    if(released) {
        ev.type = EV_RELEASE;
        if (options.control_preset == ROCKBOY_CTRL_IPOD5G) {
            if (released & BUTTON_SELECT) {
                ev.code = PAD_A;
                ev_postevent(&ev);
            }
            if (released & BUTTON_LEFT) {
                ev.code = PAD_B;
                ev_postevent(&ev);
            }
            if (released & BUTTON_RIGHT) {
                ev.code = PAD_SELECT;
                ev_postevent(&ev);
            }
            if (released & BUTTON_MENU) {
                ev.code = PAD_START;
                ev_postevent(&ev);
            }
        } else if (released & BUTTON_SELECT) {
            ev.code = PAD_SELECT;
            ev_postevent(&ev);
        }
    }
    if(pressed) { /* button press */
        ev.type = EV_PRESS;
        if (options.control_preset == ROCKBOY_CTRL_IPOD5G) {
            if (pressed & BUTTON_SELECT) {
                ev.code = PAD_A;
                ev_postevent(&ev);
            }
            if (pressed & BUTTON_LEFT) {
                ev.code = PAD_B;
                ev_postevent(&ev);
            }
            if (pressed & BUTTON_RIGHT) {
                ev.code = PAD_SELECT;
                ev_postevent(&ev);
            }
            if (pressed & BUTTON_MENU) {
                ev.code = PAD_START;
                ev_postevent(&ev);
            }
        } else if (pressed & BUTTON_SELECT) {
            ev.code = PAD_SELECT;
            ev_postevent(&ev);
        }
    }
#else
    if(released) {
        ev.type = EV_RELEASE;
#ifdef HAVE_LCD_COLOR
        if (options.rotate == 1) /* if screen is rotated, rotate direction keys */
        {
            if(released & options.LEFT) {ev.code=PAD_DOWN; ev_postevent(&ev);}
            if(released & options.RIGHT) {ev.code=PAD_UP; ev_postevent(&ev);}
            if(released & options.DOWN) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
            if(released & options.UP) {ev.code=PAD_LEFT; ev_postevent(&ev);}
        }
        else if (options.rotate == 2) /* if screen is rotated, rotate direction keys */
        {
            if(released & options.LEFT) {ev.code=PAD_UP; ev_postevent(&ev);}
            if(released & options.RIGHT) {ev.code=PAD_DOWN; ev_postevent(&ev);}
            if(released & options.DOWN) {ev.code=PAD_LEFT; ev_postevent(&ev);}
            if(released & options.UP) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
        }
        else /* screen is not rotated, do not rotate direction keys */
        {
            if(released & options.LEFT) {ev.code=PAD_LEFT; ev_postevent(&ev);}
            if(released & options.RIGHT) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
            if(released & options.DOWN) {ev.code=PAD_DOWN; ev_postevent(&ev);}
            if(released & options.UP) {ev.code=PAD_UP; ev_postevent(&ev);}
        }
#else
        if(released & options.LEFT) {ev.code=PAD_LEFT; ev_postevent(&ev);}
        if(released & options.RIGHT) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
        if(released & options.DOWN) {ev.code=PAD_DOWN; ev_postevent(&ev);}
        if(released & options.UP) {ev.code=PAD_UP; ev_postevent(&ev);}
#endif
        if(released & options.A) { ev.code=PAD_A; ev_postevent(&ev); }
        if(released & options.B) { ev.code=PAD_B; ev_postevent(&ev); }
        if(released & options.START) {
            ev.code=PAD_START;
            ev_postevent(&ev);
        }
        if(released & options.SELECT) {
            ev.code=PAD_SELECT;
            ev_postevent(&ev);
        }
    }
    if(pressed) { /* button press */
        ev.type = EV_PRESS;
#ifdef HAVE_LCD_COLOR
        if (options.rotate == 1) /* if screen is rotated, rotate direction keys */
        {
            if(pressed & options.LEFT) {ev.code=PAD_DOWN; ev_postevent(&ev);}
            if(pressed & options.RIGHT) {ev.code=PAD_UP; ev_postevent(&ev);}
            if(pressed & options.DOWN) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
            if(pressed & options.UP) {ev.code=PAD_LEFT; ev_postevent(&ev);}
        }
        else if (options.rotate == 2) /* if screen is rotated, rotate direction keys */
        {
            if(pressed & options.LEFT) {ev.code=PAD_UP; ev_postevent(&ev);}
            if(pressed & options.RIGHT) {ev.code=PAD_DOWN; ev_postevent(&ev);}
            if(pressed & options.DOWN) {ev.code=PAD_LEFT; ev_postevent(&ev);}
            if(pressed & options.UP) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
        }
        else /* screen is not rotated, do not rotate direction keys */
        {
            if(pressed & options.LEFT) {ev.code=PAD_LEFT; ev_postevent(&ev);}
            if(pressed & options.RIGHT) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
            if(pressed & options.DOWN) {ev.code=PAD_DOWN; ev_postevent(&ev);}
            if(pressed & options.UP) {ev.code=PAD_UP; ev_postevent(&ev);}
        }
#else
        if(pressed & options.LEFT) {ev.code=PAD_LEFT; ev_postevent(&ev);}
        if(pressed & options.RIGHT) {ev.code=PAD_RIGHT; ev_postevent(&ev);}
        if(pressed & options.DOWN) {ev.code=PAD_DOWN; ev_postevent(&ev);}
        if(pressed & options.UP) {ev.code=PAD_UP; ev_postevent(&ev);}
#endif
        if(pressed & options.A) { ev.code=PAD_A; ev_postevent(&ev); }
        if(pressed & options.B) { ev.code=PAD_B; ev_postevent(&ev); }
        if(pressed & options.START) {
            ev.code=PAD_START;
            ev_postevent(&ev); 
        }
        if(pressed & options.SELECT) {
            ev.code=PAD_SELECT;
            ev_postevent(&ev);
        }
#endif
#if CONFIG_KEYPAD == IPOD_4G_PAD
        static bool hold_to_exit_armed;
        bool hold_now = rb->button_hold();

        if (hold_now && !hold_to_exit_armed && rockboy_return_to_launcher) {
            die("");
            cleanshut=1;
        }
        hold_to_exit_armed = hold_now;

        if(hold_now && !rockboy_return_to_launcher) {
#else
        if(pressed & options.MENU) {
#endif
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(true);
#endif
            if (do_user_menu() == USER_MENU_QUIT) 
            {
                die("");
                cleanshut=1;
            }
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(false);
#endif
        }

#ifndef HAVE_WHEEL_POSITION
    }
#endif
#ifdef SIMULATOR
    scripted_input_post_due();
#endif
}

/* New frameskip, makes more sense to me and performs as well */
void vid_begin(void)
{
    static int skip = 0;
    if (skip<options.frameskip)
    {
        skip++;
        fb.enabled=0;
        rockboy_perf_frame_skipped();
        rockboy_profile_frame_skipped();
    }
    else
    {
        skip=0;
        fb.enabled=1;
        rockboy_perf_frame_rendered();
    }
}

void vid_init(void)
{
    fb.enabled=1;
    rockboy_profile_reset();

#if !defined(HAVE_LCD_COLOR)
    fb.mode=3;
#endif
}

#if !defined(HAVE_LCD_COLOR)
/* Color targets are handled in lcd.c */
fb_data *frameb;
void vid_update(int scanline) 
{ 
   register int cnt=0;
    static fb_data *lcd_fb = NULL;
    if (!lcd_fb)
    {
        struct viewport *vp_main = *(rb->screens[SCREEN_MAIN]->current_viewport);
        lcd_fb = vp_main->buffer->fb_ptr;
    }
    int scanline_remapped;
#if (LCD_HEIGHT == 64) && (LCD_DEPTH == 1) /* Archos, Clip, m200v4 */
    int balance = 0;
    if (fb.mode==1)
        scanline-=16;
    else if (fb.mode==2)
        scanline-=8;
    scanline_remapped = scanline / 16;
    frameb = lcd_fb + scanline_remapped * LCD_WIDTH;
    while (cnt < 160) {
        balance += LCD_WIDTH;
        if (balance > 0)
        {
             register unsigned scrbyte = 0;
             if (scan.buf[0][cnt] & 0x02)  scrbyte |= 0x01;
             if (scan.buf[1][cnt] & 0x02)  scrbyte |= 0x02;
             if (scan.buf[2][cnt] & 0x02)  scrbyte |= 0x04;
             if (scan.buf[3][cnt] & 0x02)  scrbyte |= 0x08;
             if (scan.buf[4][cnt] & 0x02)  scrbyte |= 0x10;
             if (scan.buf[5][cnt] & 0x02)  scrbyte |= 0x20;
             if (scan.buf[6][cnt] & 0x02)  scrbyte |= 0x40;
             if (scan.buf[7][cnt] & 0x02)  scrbyte |= 0x80;
             *(frameb++) = scrbyte;
             balance -= 160;
        }
        cnt ++;
    }
    rb->lcd_update_rect(0, (scanline/2) & ~7, LCD_WIDTH, 8);
#elif (LCD_HEIGHT == 128) && (LCD_DEPTH == 2) /* iriver H1x0, Samsung YH920 */
    if (fb.mode==1)
        scanline-=16;
    else if (fb.mode==2)
        scanline-=8;
    scanline_remapped = scanline / 4;
    frameb = lcd_fb + scanline_remapped * LCD_WIDTH;
    while (cnt < 160) {
        *(frameb++) = (scan.buf[0][cnt]&0x3) |
                      ((scan.buf[1][cnt]&0x3)<<2) |
                      ((scan.buf[2][cnt]&0x3)<<4) |
                      ((scan.buf[3][cnt]&0x3)<<6);
        cnt++;
    }
    rb->lcd_update_rect(0, scanline & ~3, LCD_WIDTH, 4);
#elif defined(HAVE_LCD_COLOR)
    /* handled in lcd.c now */
#endif /* LCD_HEIGHT */
}
#endif
