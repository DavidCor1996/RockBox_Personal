#include "plugin.h"
#include "smsgg_platform.h"
#include "smsgg_rom.h"

static bool is_rom_name(const char *name)
{
    size_t len = rb->strlen(name);

    if (len < 4)
        return false;

    return rb->strcasecmp(name + len - 4, ".sms") == 0 ||
           rb->strcasecmp(name + len - 3, ".gg") == 0 ||
           rb->strcasecmp(name + len - 3, ".sg") == 0;
}

bool smsgg_rom_scan(struct smsgg_rom_list *list)
{
    DIR *dir;
    struct dirent *entry;

    rb->memset(list, 0, sizeof(*list));
    dir = rb->opendir(SMSGG_ROM_DIR);
    if (dir == NULL)
        return false;

    while ((entry = rb->readdir(dir)) != NULL && list->count < SMSGG_MAX_ROMS)
    {
        struct smsgg_rom_entry *rom;

        if (!is_rom_name(entry->d_name))
            continue;

        rom = &list->entries[list->count++];
        rb->strlcpy(rom->name, entry->d_name, sizeof(rom->name));
        rb->snprintf(rom->path, sizeof(rom->path), "%s/%s",
                     SMSGG_ROM_DIR, entry->d_name);
    }

    rb->closedir(dir);
    return true;
}

int smsgg_rom_select(struct smsgg_rom_list *list, const char *last_rom)
{
    int i;
    int selected = 0;

    if (list->count == 0)
    {
        rb->splash(HZ * 4, "No SMS/GG ROMs found. Put legally obtained .sms or .gg files in .rockbox/games/smsgg/roms/");
        return -1;
    }

    for (i = 0; i < list->count; i++)
        if (last_rom != NULL && rb->strcmp(last_rom, list->entries[i].path) == 0)
            selected = i;

    rb->button_clear_queue();

    while (1)
    {
        int button;
        int start = selected - 5;
        int line = 0;

        if (start < 0)
            start = 0;

        rb->lcd_clear_display();
        rb->lcd_puts(0, line++, "Sega Master System / Game Gear");

        for (i = start; i < list->count && line < 10; i++, line++)
        {
            char text[MAX_PATH + 4];
            rb->snprintf(text, sizeof(text), "%c %s",
                         i == selected ? '>' : ' ', list->entries[i].name);
            rb->lcd_puts(0, line, text);
        }

        rb->lcd_update();
        button = rb->button_get(true);

        if (IS_SYSEVENT(button))
        {
            if (button == SYS_USB_CONNECTED)
                return -1;
            continue;
        }

#ifdef BUTTON_MENU
        if ((button & ~BUTTON_REPEAT) == BUTTON_MENU && selected > 0)
            selected--;
#endif
#ifdef BUTTON_PLAY
        if ((button & ~BUTTON_REPEAT) == BUTTON_PLAY &&
            selected + 1 < list->count)
            selected++;
#endif
#ifdef BUTTON_SCROLL_BACK
        if ((button & ~BUTTON_REPEAT) == BUTTON_SCROLL_BACK && selected > 0)
            selected--;
#endif
#ifdef BUTTON_SCROLL_FWD
        if ((button & ~BUTTON_REPEAT) == BUTTON_SCROLL_FWD &&
            selected + 1 < list->count)
            selected++;
#endif
#ifdef BUTTON_SELECT
        if ((button & ~BUTTON_REPEAT) == BUTTON_SELECT)
            return selected;
#endif
    }
}

void smsgg_rom_build_save_paths(const char *rom_path, uint32_t crc,
                                char *sram_path, size_t sram_size,
                                char *state_path, size_t state_size)
{
    char safe[64];

    smsgg_make_safe_name(safe, sizeof(safe), smsgg_basename(rom_path));
    rb->snprintf(sram_path, sram_size, "%s/%s-%08lx.sav",
                 SMSGG_SAVE_DIR, safe, (unsigned long)crc);
    rb->snprintf(state_path, state_size, "%s/%s-%08lx.state",
                 SMSGG_STATE_DIR, safe, (unsigned long)crc);
}
