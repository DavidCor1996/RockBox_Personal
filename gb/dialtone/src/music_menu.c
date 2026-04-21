#pragma bank 255

#include "audio.h"
#include <stdint.h>

extern void show_message(const char *title, const char *body, const char *footer);
extern uint8_t menu_screen(const char *title, const char *subtitle, const char *const *items, uint8_t count, uint8_t selected);

uint8_t dt_record_from_media(uint8_t media_flag) BANKED {
    if (media_flag == 0x01u) return RECORD_RAIN_LOOP;
    if (media_flag == 0x02u) return RECORD_VENDING_DREAMS;
    if (media_flag == 0x04u) return RECORD_ROOFTOP_SET;
    return RECORD_NONE;
}

const char *dt_record_name(uint8_t record_id) BANKED {
    switch (record_id) {
        case RECORD_RAIN_LOOP: return "Rain Loop";
        case RECORD_VENDING_DREAMS: return "Vending Dreams";
        case RECORD_ROOFTOP_SET: return "Rooftop Set";
        default: return "Silent";
    }
}

void dt_home_record_menu(uint8_t media, uint8_t room_id) BANKED {
    const char *items[5];
    uint8_t values[5];
    uint8_t count = 0u;
    uint8_t selected = 0u;
    uint8_t choice;

    if (media == 0u) {
        show_message("HOME DECK", "No records yet.\nFinish favors to bring home tiny songs.", "SELECT CLOSE");
        return;
    }

    if (media & 0x01u) {
        items[count] = "Rain Loop";
        values[count] = RECORD_RAIN_LOOP;
        if (dt_audio_get_home_record() == RECORD_RAIN_LOOP) selected = count;
        ++count;
    }
    if (media & 0x02u) {
        items[count] = "Vending Dreams";
        values[count] = RECORD_VENDING_DREAMS;
        if (dt_audio_get_home_record() == RECORD_VENDING_DREAMS) selected = count;
        ++count;
    }
    if (media & 0x04u) {
        items[count] = "Rooftop Set";
        values[count] = RECORD_ROOFTOP_SET;
        if (dt_audio_get_home_record() == RECORD_ROOFTOP_SET) selected = count;
        ++count;
    }

    items[count] = "Stop Deck";
    values[count] = RECORD_NONE;
    if (dt_audio_get_home_record() == RECORD_NONE) selected = count;
    ++count;

    items[count] = "Back";
    values[count] = 0xFFu;
    ++count;

    choice = menu_screen("HOME DECK", "Load a found record", items, count, selected);
    if (values[choice] == 0xFFu) {
        return;
    }

    dt_audio_set_home_record(values[choice]);
    dt_audio_sync(room_id);
    dt_audio_play_sfx((values[choice] == RECORD_NONE) ? SFX_CANCEL : SFX_CONFIRM);
}
