/***************************************************************************
 * iPod Hero physical click-wheel input qualification
 ****************************************************************************/

#include "ipodhero.h"

#include <fcntl.h>

#define IH_INPUT_TRACE_LIMIT 512u

static void ih_probe_append(char *buffer, size_t size, const char *text)
{
    if (buffer[0] != '\0')
        rb->strlcat(buffer, "+", size);
    rb->strlcat(buffer, text, size);
}

static void ih_probe_describe(long button, char *buffer, size_t size)
{
    int clean = button & ~(BUTTON_REPEAT | BUTTON_REL);

    buffer[0] = '\0';
    if (clean & BUTTON_LEFT)
        ih_probe_append(buffer, size, "Left");
    if (clean & BUTTON_MENU)
        ih_probe_append(buffer, size, "Menu");
    if (clean & BUTTON_SELECT)
        ih_probe_append(buffer, size, "Select");
    if (clean & BUTTON_PLAY)
        ih_probe_append(buffer, size, "Play");
    if (clean & BUTTON_RIGHT)
        ih_probe_append(buffer, size, "Right");
    if (clean == BUTTON_SCROLL_FWD)
        ih_probe_append(buffer, size, "WheelCW");
    else if (clean == BUTTON_SCROLL_BACK)
        ih_probe_append(buffer, size, "WheelCCW");
    if (buffer[0] == '\0')
        rb->snprintf(buffer, size, "Raw-%08lx", (unsigned long)clean);
    if (button & BUTTON_REPEAT)
        rb->strlcat(buffer, " repeat", size);
    if (button & BUTTON_REL)
        rb->strlcat(buffer, " release", size);
    else if (!(button & BUTTON_REPEAT))
        rb->strlcat(buffer, " press", size);
}

static void ih_probe_log(int fd, uint32_t count, long button,
                         const char *description)
{
    rb->fdprintf(fd, "%lu\ttick=%ld\traw=%08lx\t%s\n",
                 (unsigned long)count, *rb->current_tick,
                 (unsigned long)button, description);
}

bool ih_input_probe_run(struct ih_app *app, char *error, size_t error_size)
{
    char temporary[MAX_PATH];
    char last_event[80] = "Waiting for input";
    uint32_t count = 0;
    int fd;
    bool hold = false;
    bool previous_hold = false;
    long hold_since = 0;

#ifdef HAS_BUTTON_HOLD
    hold = rb->button_hold();
    previous_hold = hold;
    if (hold)
        hold_since = *rb->current_tick;
#endif
    rb->snprintf(temporary, sizeof(temporary), "%s.tmp",
                 IH_INPUT_TRACE_FILE);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        rb->snprintf(error, error_size,
                     "Could not create input-trace.log");
        return false;
    }
    rb->fdprintf(fd,
        "IHT1\nversion=1\nhz=%d\nkeypad=%d\ntrack=%s\n"
        "columns=count,tick,raw,event\n",
        HZ, CONFIG_KEYPAD, app->index.audio_path);
    rb->button_clear_queue();
#ifdef SIMULATOR
    if (app->sim_test_mode == 4)
    {
        static const long events[] =
        {
            BUTTON_LEFT, BUTTON_LEFT | BUTTON_REPEAT,
            BUTTON_LEFT | BUTTON_REL, BUTTON_MENU, BUTTON_SELECT,
            BUTTON_PLAY, BUTTON_RIGHT, BUTTON_SCROLL_FWD,
            BUTTON_SCROLL_BACK, BUTTON_LEFT | BUTTON_SELECT,
            BUTTON_SELECT | BUTTON_MENU
        };
        size_t event_index;

        for (event_index = 0; event_index < ARRAYLEN(events); ++event_index)
        {
            ih_probe_describe(events[event_index], last_event,
                              sizeof(last_event));
            count++;
            ih_probe_log(fd, count, events[event_index], last_event);
            ih_render_input_test(count, last_event, false, false);
            rb->sleep(MAX(1, HZ / 20));
        }
        rb->strlcpy(last_event, "Hold ON", sizeof(last_event));
        count++;
        ih_probe_log(fd, count, 0, last_event);
        rb->strlcpy(last_event, "Hold off", sizeof(last_event));
        count++;
        ih_probe_log(fd, count, 0, last_event);
        goto complete;
    }
#endif
    while (true)
    {
        long button;
        int clean;
        int system_event;

#ifdef HAS_BUTTON_HOLD
        hold = rb->button_hold();
        if (hold != previous_hold && count < IH_INPUT_TRACE_LIMIT)
        {
            rb->snprintf(last_event, sizeof(last_event), "Hold %s",
                         hold ? "ON" : "off");
            count++;
            ih_probe_log(fd, count, 0, last_event);
            previous_hold = hold;
            hold_since = hold ? *rb->current_tick : 0;
        }
#endif
        ih_render_input_test(count, last_event, hold,
                             count >= IH_INPUT_TRACE_LIMIT);
        if (hold && hold_since != 0 &&
            TIME_AFTER(*rb->current_tick, hold_since + HZ * 2))
            break;
        button = rb->button_get_w_tmo(MAX(1, HZ / 20));
        if (button == BUTTON_NONE)
            continue;
        if (button == SYS_USB_CONNECTED)
        {
            app->usb = true;
            break;
        }
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (count < IH_INPUT_TRACE_LIMIT)
        {
            ih_probe_describe(button, last_event, sizeof(last_event));
            count++;
            ih_probe_log(fd, count, button, last_event);
        }
        if ((clean & (BUTTON_SELECT | BUTTON_MENU)) ==
            (BUTTON_SELECT | BUTTON_MENU) &&
            !(button & (BUTTON_REPEAT | BUTTON_REL)))
            break;
        system_event = rb->default_event_handler(button);
        if (system_event == SYS_USB_CONNECTED)
        {
            app->usb = true;
            break;
        }
        if (system_event == SYS_POWEROFF || system_event == SYS_REBOOT)
            break;
    }
#ifdef SIMULATOR
complete:
#endif
    rb->fdprintf(fd, "complete=1\nevents=%lu\n",
                 (unsigned long)count);
    rb->close(fd);
    rb->remove(IH_INPUT_TRACE_FILE);
    if (rb->rename(temporary, IH_INPUT_TRACE_FILE) < 0)
    {
        rb->snprintf(error, error_size,
                     "Could not install input-trace.log");
        rb->remove(temporary);
        return false;
    }
#ifdef SIMULATOR
    if (app->sim_test_mode == 4)
    {
        ih_render_input_test(count, "Trace saved", false, false);
        rb->sleep(HZ * 8);
    }
#endif
    return !app->usb;
}
