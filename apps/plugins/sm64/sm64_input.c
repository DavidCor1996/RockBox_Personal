#include "sm64_rockbox.h"
#include "upstream/include/PR/os_cont.h"

static bool hold_initialized;
static bool hold_was_on;
static int previous_held;
static unsigned transient_buttons;

static bool hold_exit_requested(void)
{
#ifdef HAS_BUTTON_HOLD
    bool hold_now = rb->button_hold();
    if (!hold_initialized)
    {
        hold_initialized = true;
        hold_was_on = hold_now;
        return false;
    }
    if (hold_now && !hold_was_on)
    {
        hold_was_on = true;
        return true;
    }
    hold_was_on = hold_now;
#endif
    return false;
}

static void haptic_press(int held)
{
    int newly_pressed = held & ~previous_held;
    previous_held = held;
    if (newly_pressed && rb->haptic_feedback &&
        rb->haptic_feedback_enabled && rb->haptic_feedback_enabled())
        rb->haptic_feedback(22, 34);
}

static void read_events(void)
{
    int event;

    transient_buttons = 0;
    while ((event = rb->button_get(false)) != BUTTON_NONE)
    {
        if (event == SYS_USB_CONNECTED)
        {
            sm64_rb.usb_connected = true;
            sm64_rb.quit = true;
            break;
        }
#ifdef BUTTON_SCROLL_FWD
        if ((event & BUTTON_SCROLL_FWD) != 0)
            transient_buttons |= R_CBUTTONS;
#endif
#ifdef BUTTON_SCROLL_BACK
        if ((event & BUTTON_SCROLL_BACK) != 0)
            transient_buttons |= L_CBUTTONS;
#endif
#ifdef BUTTON_MENU
        if ((event & BUTTON_MENU) != 0 && (event & BUTTON_REPEAT) != 0)
            sm64_rb.quit = true;
#endif
    }
}

void sm64_input_init(void)
{
    hold_initialized = false;
    hold_was_on = false;
    previous_held = 0;
    transient_buttons = 0;
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
}

static void analog_wheel(OSContPad *pad)
{
#ifdef HAVE_WHEEL_POSITION
    static const int8_t axis[16][2] = {
        {  0, 80}, { 31, 74}, { 57, 57}, { 74, 31},
        { 80,  0}, { 74,-31}, { 57,-57}, { 31,-74},
        {  0,-80}, {-31,-74}, {-57,-57}, {-74,-31},
        {-80,  0}, {-74, 31}, {-57, 57}, {-31, 74},
    };
    int position = rb->wheel_status();
    if (position >= 0)
    {
        int zone = ((position + 3) / 6) & 15;
        pad->stick_x = axis[zone][0];
        pad->stick_y = axis[zone][1];
    }
#else
#if defined(BUTTON_UP) || defined(BUTTON_DOWN)
    int held = rb->button_status();
#ifdef BUTTON_UP
    if (held & BUTTON_UP) pad->stick_y = 80;
#endif
#ifdef BUTTON_DOWN
    if (held & BUTTON_DOWN) pad->stick_y = -80;
#endif
#endif
#endif
}

void sm64_input_read(void *opaque_pad)
{
    OSContPad *pad = opaque_pad;
    int held = rb->button_status();

    rb->memset(pad, 0, sizeof(*pad));
    read_events();
    if (hold_exit_requested())
        sm64_rb.quit = true;
    haptic_press(held);
    analog_wheel(pad);

#ifdef BUTTON_SELECT
    if (held & BUTTON_SELECT) pad->button |= A_BUTTON;
#endif
#ifdef BUTTON_PLAY
    if (held & BUTTON_PLAY) pad->button |= B_BUTTON;
#endif
#ifdef BUTTON_LEFT
    if (held & BUTTON_LEFT) pad->button |= Z_TRIG;
#endif
#ifdef BUTTON_RIGHT
    if (held & BUTTON_RIGHT) pad->button |= R_TRIG;
#endif
#ifdef BUTTON_MENU
    if (held & BUTTON_MENU) pad->button |= START_BUTTON;
#endif
    pad->button |= transient_buttons;
    pad->errnum = 0;

#ifdef SIMULATOR
    if (getenv("SM64_TEST_AUTOPLAY"))
    {
        /* The title head ignores input during its opening animation. */
        if (sm64_rb.frames < 420 && (sm64_rb.frames % 120) >= 60 &&
            (sm64_rb.frames % 120) < 63)
            pad->button |= START_BUTTON;
        /* Move the file-select hand from centre to File A, then choose it. */
        if (sm64_rb.frames >= 620 && sm64_rb.frames < 633)
        {
            pad->stick_x = -64;
            pad->stick_y = 32;
        }
        if (sm64_rb.frames >= 700 && sm64_rb.frames < 703)
            pad->button |= A_BUTTON;
        /* Once the castle loads, exercise movement and repeated jumps. */
        if (sm64_rb.frames >= 1100)
        {
            pad->stick_y = 64;
            if ((sm64_rb.frames % 90) < 3)
                pad->button |= A_BUTTON;
        }
    }
#endif
}

s32 osContInit(OSMesgQueue *mq, u8 *controller_bits, OSContStatus *status)
{
    (void)mq;
    sm64_input_init();
    *controller_bits = 1;
    if (status)
        rb->memset(status, 0, sizeof(*status));
    return 0;
}

s32 osContStartReadData(OSMesgQueue *mesg)
{
    (void)mesg;
    return 0;
}

void osContGetReadData(OSContPad *pad)
{
    sm64_input_read(pad);
}
