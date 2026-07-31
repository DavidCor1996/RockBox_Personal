/***************************************************************************
 * Animal Crossing for Rockbox: Phase 0 target shell and asset verifier.
 *
 * This source contains no Nintendo data. Runtime assets must be prepared from
 * a user-owned GAFE01 revision 0 disc with animalcrossing_prepare_assets.py.
 ****************************************************************************/
#include "plugin.h"
#include "lib/helper.h"
#include "ac_demake.h"

#include <stdint.h>
#include <stdbool.h>

#define AC_PACK_PATH AC_DATA_DIR "/assets.pack"
#define AC_PACK_MAGIC "ACIPACK"
#define AC_PACK_VERSION 1
#define AC_PACK_HEADER_SIZE 64
#define AC_PACK_ENTRY_SIZE 128
#define AC_PACK_MAX_ENTRIES 16384
#define AC_IO_CHUNK 4096

#define AC_COLOR_HEADER LCD_RGBPACK(196, 196, 196)
#define AC_COLOR_SELECT LCD_RGBPACK(47, 94, 173)
#define AC_COLOR_PANEL LCD_RGBPACK(238, 238, 238)
#define AC_COLOR_TEXT LCD_RGBPACK(24, 24, 24)

struct ac_pack_header {
    unsigned char magic[8];
    uint32_t version;
    uint32_t entry_count;
    uint32_t entry_size;
    uint32_t reserved;
    uint64_t index_offset;
    uint64_t data_offset;
    unsigned char identity[8];
    unsigned char padding[16];
} __attribute__((packed));

struct ac_pack_entry {
    char name[96];
    uint64_t offset;
    uint64_t size;
    uint32_t crc32;
    uint32_t flags;
    uint64_t source_offset;
} __attribute__((packed));

struct ac_pack_summary {
    bool present;
    bool layout_valid;
    bool crc_valid;
    uint32_t entries;
    uint64_t bytes;
    char error[80];
};

static unsigned char io_buffer[AC_IO_CHUNK];
static struct ac_pack_summary pack;
static size_t shared_bytes;
static bool quit;
static bool usb_connected;

static uint32_t ac_crc32(uint32_t crc, const unsigned char *data, size_t size)
{
    size_t index;
    int bit;

    crc ^= 0xffffffffu;
    for (index = 0; index < size; ++index)
    {
        crc ^= data[index];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

static bool ac_read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *next = buffer;

    while (size > 0)
    {
        ssize_t count = rb->read(fd, next, size);
        if (count <= 0)
            return false;
        next += count;
        size -= (size_t)count;
    }
    return true;
}

static void ac_pack_error(const char *message)
{
    rb->strlcpy(pack.error, message, sizeof(pack.error));
    pack.layout_valid = false;
}

static bool ac_valid_name(const char name[96])
{
    size_t index;

    if (name[0] == '\0' || name[0] == '/')
        return false;
    for (index = 0; index < 96; ++index)
    {
        if (name[index] == '\0')
            return true;
        if (name[index] == '\\')
            return false;
        if (name[index] == '.' && index + 1 < 96 &&
            name[index + 1] == '.' &&
            (index == 0 || name[index - 1] == '/') &&
            (index + 2 == 96 || name[index + 2] == '/' ||
             name[index + 2] == '\0'))
            return false;
    }
    return false;
}

static bool ac_inspect_pack(bool verify_crc)
{
    struct ac_pack_header header;
    struct ac_pack_entry entry;
    off_t file_size;
    uint64_t previous_end = 0;
    uint32_t index;
    int fd;

    rb->memset(&pack, 0, sizeof(pack));
    fd = rb->open(AC_PACK_PATH, O_RDONLY);
    if (fd < 0)
    {
        ac_pack_error("assets.pack not found");
        return false;
    }
    pack.present = true;
    file_size = rb->filesize(fd);
    if (file_size < AC_PACK_HEADER_SIZE ||
        !ac_read_exact(fd, &header, sizeof(header)))
    {
        ac_pack_error("truncated pack header");
        goto fail;
    }
    if (rb->memcmp(header.magic, AC_PACK_MAGIC, 7) != 0 ||
        header.magic[7] != '\0')
    {
        ac_pack_error("invalid pack magic");
        goto fail;
    }
    if (header.version != AC_PACK_VERSION ||
        header.entry_size != AC_PACK_ENTRY_SIZE)
    {
        ac_pack_error("unsupported pack version");
        goto fail;
    }
    if (header.entry_count < 2 ||
        header.entry_count > AC_PACK_MAX_ENTRIES ||
        header.index_offset != AC_PACK_HEADER_SIZE ||
        header.index_offset +
            (uint64_t)header.entry_count * header.entry_size >
            header.data_offset ||
        header.data_offset > (uint64_t)file_size)
    {
        ac_pack_error("invalid pack index bounds");
        goto fail;
    }
    if (rb->memcmp(header.identity, "GAFE01", 6) != 0 ||
        header.identity[6] != 0)
    {
        ac_pack_error("pack is not GAFE01 rev 0");
        goto fail;
    }

    pack.entries = header.entry_count;
    pack.bytes = (uint64_t)file_size;
    previous_end = header.data_offset;
    for (index = 0; index < header.entry_count; ++index)
    {
        uint64_t remaining;
        uint32_t crc = 0;

        if (rb->lseek(fd, header.index_offset +
                     (uint64_t)index * header.entry_size, SEEK_SET) < 0 ||
            !ac_read_exact(fd, &entry, sizeof(entry)))
        {
            ac_pack_error("cannot read pack index");
            goto fail;
        }
        if (!ac_valid_name(entry.name) ||
            entry.offset < header.data_offset ||
            entry.offset < previous_end ||
            entry.size > (uint64_t)file_size ||
            entry.offset > (uint64_t)file_size - entry.size)
        {
            ac_pack_error("invalid pack entry bounds");
            goto fail;
        }
        previous_end = entry.offset + entry.size;
        if (!verify_crc)
            continue;

        if (rb->lseek(fd, entry.offset, SEEK_SET) < 0)
        {
            ac_pack_error("cannot seek to pack data");
            goto fail;
        }
        remaining = entry.size;
        while (remaining > 0)
        {
            size_t chunk = remaining > sizeof(io_buffer) ?
                           sizeof(io_buffer) : (size_t)remaining;
            if (!ac_read_exact(fd, io_buffer, chunk))
            {
                ac_pack_error("truncated pack data");
                goto fail;
            }
            crc = ac_crc32(crc, io_buffer, chunk);
            remaining -= chunk;
        }
        if (crc != entry.crc32)
        {
            ac_pack_error("asset checksum mismatch");
            goto fail;
        }
        rb->splashf(1, "Verifying assets %lu/%lu",
                    (unsigned long)index + 1,
                    (unsigned long)header.entry_count);
    }
    rb->close(fd);
    pack.layout_valid = true;
    pack.crc_valid = verify_crc;
    pack.error[0] = '\0';
    return true;

fail:
    rb->close(fd);
    return false;
}

static void ac_draw_header(const char *title)
{
    int width;
    int height;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(AC_COLOR_TEXT);
    rb->lcd_clear_display();
    rb->lcd_set_foreground(AC_COLOR_HEADER);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 25);
    rb->lcd_set_foreground(AC_COLOR_TEXT);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_getstringsize(title, &width, &height);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2, (25 - height) / 2, title);
    rb->lcd_hline(0, LCD_WIDTH - 1, 25);
}

static void ac_draw_footer(const char *text)
{
    int width;
    int height;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_foreground(AC_COLOR_PANEL);
    rb->lcd_fillrect(0, LCD_HEIGHT - 23, LCD_WIDTH, 23);
    rb->lcd_set_foreground(AC_COLOR_TEXT);
    rb->lcd_hline(0, LCD_WIDTH - 1, LCD_HEIGHT - 23);
    rb->lcd_getstringsize(text, &width, &height);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2,
                   LCD_HEIGHT - 12 - height / 2, text);
}

static void ac_draw_menu(int selected)
{
    static const char *items[] = {
        "Start Game",
        "Verify Assets",
        "Memory Report",
        "Controls",
        "Exit"
    };
    int index;
    int line_height;
    int y = 43;

    ac_draw_header("Animal Crossing");
    rb->lcd_getstringsize("Ag", NULL, &line_height);
    for (index = 0; index < (int)ARRAYLEN(items); ++index)
    {
        int text_width;
        int row_y = y + index * (line_height + 8);

        rb->lcd_getstringsize(items[index], &text_width, NULL);
        if (index == selected)
        {
            rb->lcd_set_drawmode(DRMODE_SOLID);
            rb->lcd_set_foreground(AC_COLOR_SELECT);
            rb->lcd_fillrect(22, row_y - 3, LCD_WIDTH - 44,
                             line_height + 6);
            rb->lcd_set_foreground(LCD_WHITE);
        }
        else
            rb->lcd_set_foreground(AC_COLOR_TEXT);
        rb->lcd_set_drawmode(DRMODE_FG);
        rb->lcd_putsxy((LCD_WIDTH - text_width) / 2, row_y, items[index]);
    }

    if (pack.layout_valid)
        ac_draw_footer("GAFE01 assets ready");
    else
        ac_draw_footer(pack.error);
    rb->lcd_update();
}

static void ac_info_screen(const char *title, const char **lines, int count)
{
    int line_height;
    int index;

    ac_draw_header(title);
    rb->lcd_getstringsize("Ag", NULL, &line_height);
    for (index = 0; index < count; ++index)
    {
        int width;
        rb->lcd_getstringsize(lines[index], &width, NULL);
        rb->lcd_putsxy((LCD_WIDTH - width) / 2,
                       48 + index * (line_height + 7), lines[index]);
    }
    ac_draw_footer("MENU to return");
    rb->lcd_update();
    while (1)
    {
        int button = rb->button_get(true);
        if (button == BUTTON_MENU || button == (BUTTON_MENU | BUTTON_REL) ||
            button == BUTTON_LEFT)
            return;
        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
        {
            quit = true;
            return;
        }
    }
}

static void ac_show_memory(void)
{
    char shared[48];
    char pack_size[48];
    char entries[48];
    const char *lines[] = { shared, pack_size, entries,
                            "Target cap: 52 MiB",
                            "Required headroom: 2 MiB" };

    rb->snprintf(shared, sizeof(shared), "Shared arena: %lu KiB",
                 (unsigned long)(shared_bytes / 1024));
    rb->snprintf(pack_size, sizeof(pack_size), "Asset pack: %lu KiB",
                 (unsigned long)(pack.bytes / 1024));
    rb->snprintf(entries, sizeof(entries), "Pack entries: %lu",
                 (unsigned long)pack.entries);
    ac_info_screen("Memory Report", lines, ARRAYLEN(lines));
}

static void ac_show_controls(void)
{
    const char *lines[] = {
        "Wheel: move / select",
        "Center: A / confirm",
        "Play: B / cancel",
        "Previous / Next: X / Y",
        "MENU: Start / pause"
    };
    ac_info_screen("Controls", lines, ARRAYLEN(lines));
}

static void ac_start_gate(void)
{
    const char *missing[] = {
        "Prepare assets.pack",
        "from a user-owned",
        "GAFE01 revision 0 disc."
    };

    if (pack.layout_valid)
    {
        if (ac_demake_run() == PLUGIN_USB_CONNECTED)
        {
            usb_connected = true;
            quit = true;
        }
    }
    else
        ac_info_screen("Assets Required", missing, ARRAYLEN(missing));
}

static enum plugin_status ac_menu(void)
{
    int selected = 0;

    while (!quit)
    {
        int button;
        ac_draw_menu(selected);
        button = rb->button_get(true);
        switch (button)
        {
            case BUTTON_SCROLL_BACK:
                selected = (selected + 4) % 5;
                break;
            case BUTTON_SCROLL_FWD:
                selected = (selected + 1) % 5;
                break;
            case BUTTON_SELECT:
            case BUTTON_RIGHT:
                if (selected == 0)
                    ac_start_gate();
                else if (selected == 1)
                {
                    rb->splash(0, "Verifying asset checksums...");
                    if (ac_inspect_pack(true))
                        rb->splash(HZ, "All asset checksums passed");
                    else
                        rb->splashf(HZ * 2, "Asset error: %s", pack.error);
                }
                else if (selected == 2)
                    ac_show_memory();
                else if (selected == 3)
                    ac_show_controls();
                else
                    quit = true;
                break;
            case BUTTON_MENU:
            case BUTTON_LEFT:
                quit = true;
                break;
            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }
    return usb_connected ? PLUGIN_USB_CONNECTED : PLUGIN_OK;
}

enum plugin_status plugin_start(const void *parameter)
{
    unsigned char *shared;
    enum plugin_status status;
    (void)parameter;

    rb->mkdir(AC_DATA_DIR);
    shared = rb->plugin_get_audio_buffer(&shared_bytes);
    if (!shared || shared_bytes < 8 * 1024 * 1024)
    {
        rb->splash(HZ * 2, "Animal Crossing: shared memory unavailable");
        return PLUGIN_ERROR;
    }
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    if ((uintptr_t)shared < (uintptr_t)plugin_start_addr)
        shared_bytes = MIN(shared_bytes,
                           (size_t)((uintptr_t)plugin_start_addr -
                                    (uintptr_t)shared));
#endif
    quit = false;
    usb_connected = false;
    ac_inspect_pack(false);
#ifdef SIMULATOR
    if (getenv("ANIMALCROSSING_TEST_VERIFY") != NULL)
    {
        bool valid = ac_inspect_pack(true);
        DEBUGF("Animal Crossing asset verification: %s (%s)\n",
               valid ? "pass" : "fail",
               valid ? "all checksums valid" : pack.error);
        rb->plugin_release_audio_buffer();
        return valid ? PLUGIN_OK : PLUGIN_ERROR;
    }
#endif
    backlight_ignore_timeout();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    status = ac_menu();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    backlight_use_settings();
    rb->plugin_release_audio_buffer();
    return status;
}
