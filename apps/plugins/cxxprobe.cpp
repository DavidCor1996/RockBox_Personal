/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * C++ plugin support probe.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"

extern "C" {
#include "lib/plugin_cxx.h"
}

class ProbeCounter {
public:
    explicit ProbeCounter(int value) : value_(value) {}
    ~ProbeCounter() { value_ = 0; }

    int tick(int delta)
    {
        value_ += delta;
        return value_;
    }

private:
    int value_;
};

extern "C" enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;

    size_t buffer_size = 0;
    void *buffer = rb->plugin_get_buffer(&buffer_size);
    plugin_cxx_init(buffer, buffer_size / 4);

    ProbeCounter stack_counter(7);
    ProbeCounter *heap_counter = new ProbeCounter(11);
    int value = stack_counter.tick(5);

    if (!heap_counter) {
        rb->splash(HZ, "C++ new failed");
        return PLUGIN_ERROR;
    }

    value += heap_counter->tick(9);
    delete heap_counter;

    rb->lcd_clear_display();
    rb->lcd_putsxy(4, 4, "C++ plugin probe");
    rb->lcd_putsxyf(4, 22, "constructors: %d", value);
    rb->lcd_putsxyf(4, 40, "heap free: %luK",
                    (unsigned long)(plugin_cxx_available() / 1024));
    rb->lcd_putsxy(4, 58, "Select/Menu exits");
    rb->lcd_update();

    while (true) {
        int button = rb->button_get(true);

        if (button == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
        if ((button & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SELECT ||
            (button & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_MENU)
            break;
    }

    return PLUGIN_OK;
}
