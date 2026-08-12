#include "plugin.h"
#include "lib/helper.h"
#include "uxn_audio.h"
#include "devices.h"
#include "file.h"
#include "screen.h"
#include "uxn.h"

#define UXN_ROM_DIR "/Uxn"
#define UXN_MEMORY_REQUIRED \
    (UXN_RAM_SIZE + UXN_SCREEN_LAYER_SIZE * 2 + UXN_SCREEN_OUTPUT_SIZE)
#define UXN_FRAME_RATE 60

struct uxn_vm uxn;

static bool pointer_mode;
static int pointer_x;
static int pointer_y;

uint8_t uxn_dei(uint8_t addr)
{
    uint8_t page = addr & 0xf0;
    uint8_t port = addr & 0x0f;

    switch (page)
    {
    case 0x00: return system_dei(addr);
    case 0x20: return screen_dei(addr);
    case 0x30: return uxn_audio_dei(0, port);
    case 0x40: return uxn_audio_dei(1, port);
    case 0x50: return uxn_audio_dei(2, port);
    case 0x60: return uxn_audio_dei(3, port);
    case 0xc0: return datetime_dei(addr);
    default: return uxn.dev[addr];
    }
}

void uxn_deo(uint8_t addr, uint8_t value)
{
    uint8_t page = addr & 0xf0;
    uint8_t port = addr & 0x0f;

    uxn.dev[addr] = value;
    switch (page)
    {
    case 0x00:
        system_deo(addr);
        if (port > 0x7 && port < 0xe)
            screen_palette();
        break;
    case 0x10: console_deo(addr); break;
    case 0x20: screen_deo(addr); break;
    case 0x30: uxn_audio_deo(0, port); break;
    case 0x40: uxn_audio_deo(1, port); break;
    case 0x50: uxn_audio_deo(2, port); break;
    case 0x60: uxn_audio_deo(3, port); break;
    case 0x80: controller_deo(addr); break;
    case 0x90: mouse_deo(addr); break;
    case 0xa0:
    case 0xb0: uxn_file_deo(addr); break;
    }
}

static bool is_rom(char *name, int attr, struct tree_context *context)
{
    const char *extension;

    (void)context;
    if (attr & ATTR_DIRECTORY)
        return true;
    extension = rb->strrchr(name, '.');
    return extension && !rb->strcasecmp(extension, ".rom");
}

static bool browse_rom(char *path, size_t path_size)
{
    struct browse_context browse = {
        .dirfilter = SHOW_ALL,
        .flags = BROWSE_SELECTONLY | BROWSE_NO_CONTEXT_MENU,
        .title = "Uxn ROM",
        .icon = Icon_Plugin,
        .root = UXN_ROM_DIR,
        .buf = path,
        .bufsize = path_size,
        .callback_show_item = is_rom,
    };

    rb->mkdir(UXN_ROM_DIR);
    rb->rockbox_browse(&browse);
    return browse.flags & BROWSE_SELECTED;
}

static bool load_rom(const char *path)
{
    int fd = rb->open(path, O_RDONLY);
    size_t room = UXN_RAM_SIZE - UXN_PAGE_PROGRAM;
    size_t loaded = 0;

    if (fd < 0)
        return false;
    while (loaded < room)
    {
        ssize_t amount = rb->read(fd, uxn.ram + UXN_PAGE_PROGRAM + loaded,
                                  room - loaded);

        if (amount < 0)
        {
            rb->close(fd);
            return false;
        }
        if (!amount)
            break;
        loaded += amount;
    }
    if (loaded == room)
    {
        uint8_t extra;

        if (rb->read(fd, &extra, 1) > 0)
        {
            rb->close(fd);
            return false;
        }
    }
    rb->close(fd);
    return loaded > 0;
}

static uint8_t controller_mask(long button)
{
    switch (button)
    {
    case BUTTON_SELECT: return 0x01;
    case BUTTON_MENU: return 0x10;
    case BUTTON_PLAY: return 0x20;
    case BUTTON_LEFT: return 0x40;
    case BUTTON_RIGHT: return 0x80;
    default: return 0;
    }
}

static void controller_pulse(uint8_t mask)
{
    controller_down(mask);
    controller_up(mask);
}

static void move_pointer(int dx, int dy)
{
    pointer_x += dx;
    pointer_y += dy;
    if (pointer_x < 0)
        pointer_x = 0;
    if (pointer_y < 0)
        pointer_y = 0;
    if (pointer_x >= uxn_screen.width)
        pointer_x = uxn_screen.width - 1;
    if (pointer_y >= uxn_screen.height)
        pointer_y = uxn_screen.height - 1;
    mouse_pos(pointer_x, pointer_y);
}

static bool handle_pointer(long base, bool release, bool repeat)
{
    if (base == BUTTON_SELECT)
    {
        if (release)
            mouse_up(0x01);
        else if (!repeat)
            mouse_down(0x01);
    }
    else if (!release)
    {
        int step = repeat ? 8 : 4;

        if (base == BUTTON_LEFT)
            move_pointer(-step, 0);
        else if (base == BUTTON_RIGHT)
            move_pointer(step, 0);
        else if (base == BUTTON_MENU)
            move_pointer(0, -step);
        else if (base == BUTTON_PLAY)
            move_pointer(0, step);
        else if (base == BUTTON_SCROLL_BACK)
            mouse_scroll(0, 1);
        else if (base == BUTTON_SCROLL_FWD)
            mouse_scroll(0, -1);
    }
    return true;
}

static bool handle_button(long button)
{
    bool release = button & BUTTON_REL;
    bool repeat = button & BUTTON_REPEAT;
    long base = button & ~(BUTTON_REL | BUTTON_REPEAT);
    uint8_t mask;

    if (!release && base == (BUTTON_MENU | BUTTON_PLAY))
        return false;
    if (!release && !repeat && base == (BUTTON_SELECT | BUTTON_PLAY))
    {
        controller_up(0xff);
        mouse_up(0x07);
        pointer_mode = !pointer_mode;
        pointer_x = uxn_screen.width / 2;
        pointer_y = uxn_screen.height / 2;
        mouse_pos(pointer_x, pointer_y);
        rb->splash(HZ / 2, pointer_mode ? "Pointer mode" : "Controller mode");
        uxn_screen.dirty = true;
        return true;
    }
    if (pointer_mode)
        return handle_pointer(base, release, repeat);
    if (!release && base == (BUTTON_SELECT | BUTTON_MENU))
    {
        controller_pulse(0x04);
        return true;
    }
    if (!release && (base == BUTTON_SCROLL_BACK ||
                     base == BUTTON_SCROLL_FWD))
    {
        controller_pulse(base == BUTTON_SCROLL_BACK ? 0x02 : 0x08);
        return true;
    }

    mask = controller_mask(base);
    if (release)
        controller_up(mask);
    else if (!repeat)
        controller_down(mask);
    return true;
}

static enum plugin_status run_vm(void)
{
    long next_frame = *rb->current_tick;
    long frame_tick_accum = 0;
#ifdef SIMULATOR
    const char *test_frames = getenv("UXN_TEST_FRAMES");
    int test_limit = test_frames ? atoi(test_frames) : 0;
    int frames = 0;
#endif

    pointer_mode = false;
    pointer_x = uxn_screen.width / 2;
    pointer_y = uxn_screen.height / 2;
    rb->button_clear_queue();
    if (!uxn_eval(UXN_PAGE_PROGRAM))
        return PLUGIN_ERROR;
    screen_redraw();

    while (!uxn.dev[0x0f])
    {
        long now = *rb->current_tick;
        int timeout = TIME_BEFORE(now, next_frame) ? next_frame - now : 0;
        long button = rb->button_get_w_tmo(timeout);

        if (button != BUTTON_NONE)
        {
            if (IS_SYSEVENT(button))
            {
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
            }
            else if (!handle_button(button))
                return PLUGIN_OK;
        }
        now = *rb->current_tick;
        if (!TIME_BEFORE(now, next_frame))
        {
            uxn_eval(uxn_screen.vector);
            uxn_audio_poll();
            screen_redraw();
            frame_tick_accum += HZ;
            next_frame += frame_tick_accum / UXN_FRAME_RATE;
            frame_tick_accum %= UXN_FRAME_RATE;
            if (TIME_AFTER(now, next_frame + HZ))
                next_frame = now;
#ifdef SIMULATOR
            if (test_limit > 0 && ++frames >= test_limit)
                return PLUGIN_OK;
#endif
        }
        rb->yield();
    }
    return PLUGIN_OK;
}

enum plugin_status plugin_start(const void *parameter)
{
    char selected_path[MAX_PATH];
    const char *path = parameter;
    uint8_t *memory;
    size_t memory_size;
    enum plugin_status status = PLUGIN_ERROR;

    rb->lcd_setfont(FONT_UI);
    if (!path || !path[0])
    {
        if (!browse_rom(selected_path, sizeof(selected_path)))
        {
            rb->splash(HZ * 3, "Put .rom files in /Uxn");
            return PLUGIN_OK;
        }
        path = selected_path;
    }

    memory = rb->plugin_get_buffer(&memory_size);
    if (memory_size < UXN_MEMORY_REQUIRED)
    {
        rb->splashf(HZ * 3, "Uxn needs %lu KiB",
                    (unsigned long)(UXN_MEMORY_REQUIRED / 1024));
        return PLUGIN_ERROR;
    }
    rb->memset(memory, 0, UXN_MEMORY_REQUIRED);
    rb->memset(&uxn, 0, sizeof(uxn));
    uxn.ram = memory;
    screen_init(memory + UXN_RAM_SIZE);
    uxn_file_init();

    if (!load_rom(path))
    {
        rb->splash(HZ * 2, "Could not load Uxn ROM");
        goto cleanup_files;
    }

    backlight_ignore_timeout();
#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
    rb->cpu_boost(true);
#endif
    uxn_audio_init();
    status = run_vm();
    uxn_audio_shutdown();
#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
    rb->cpu_boost(false);
#endif
    backlight_use_settings();

cleanup_files:
    uxn_file_shutdown();
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_update();
    return status;
}
