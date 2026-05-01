#pragma bank 255

#include "audio.h"
#include "dialtone.h"
#include <gb/gb.h>
#include <stdint.h>

typedef struct note_event_t {
    uint8_t note;
    uint8_t frames;
} note_event_t;

enum {
    NOTE_C3,
    NOTE_CS3,
    NOTE_D3,
    NOTE_DS3,
    NOTE_E3,
    NOTE_F3,
    NOTE_FS3,
    NOTE_G3,
    NOTE_GS3,
    NOTE_A3,
    NOTE_AS3,
    NOTE_B3,
    NOTE_C4,
    NOTE_CS4,
    NOTE_D4,
    NOTE_DS4,
    NOTE_E4,
    NOTE_F4,
    NOTE_FS4,
    NOTE_G4,
    NOTE_GS4,
    NOTE_A4,
    NOTE_AS4,
    NOTE_B4,
    NOTE_C5,
    NOTE_CS5,
    NOTE_D5,
    NOTE_DS5,
    NOTE_E5,
    NOTE_F5,
    NOTE_FS5,
    NOTE_G5,
    NOTE_REST = 0xFEu,
    NOTE_END = 0xFFu
};

static const uint16_t note_freqs[] = {
    1798u, 1812u, 1825u, 1837u, 1849u, 1860u, 1871u, 1881u,
    1890u, 1899u, 1907u, 1915u, 1923u, 1930u, 1936u, 1943u,
    1949u, 1954u, 1959u, 1964u, 1969u, 1974u, 1978u, 1982u,
    1985u, 1988u, 1992u, 1995u, 1998u, 2001u, 2004u, 2006u
};

/* Compact in-ROM arrangements based on CC0 reference tracks listed in
 * documents/gameboy/dialtone_audio_sources.md. */
static const note_event_t rain_loop_track[] = {
    { NOTE_E3, 10u }, { NOTE_REST, 2u }, { NOTE_G3, 6u }, { NOTE_A3, 10u },
    { NOTE_G3, 6u }, { NOTE_E3, 8u }, { NOTE_D3, 8u }, { NOTE_REST, 4u },
    { NOTE_E3, 10u }, { NOTE_REST, 2u }, { NOTE_G3, 6u }, { NOTE_B3, 10u },
    { NOTE_A3, 6u }, { NOTE_G3, 8u }, { NOTE_E3, 8u }, { NOTE_REST, 10u },
    { NOTE_END, 0u }
};

static const note_event_t vending_dreams_track[] = {
    { NOTE_C4, 6u }, { NOTE_E4, 6u }, { NOTE_G4, 6u }, { NOTE_A4, 6u },
    { NOTE_G4, 6u }, { NOTE_E4, 6u }, { NOTE_D4, 6u }, { NOTE_E4, 6u },
    { NOTE_C4, 8u }, { NOTE_REST, 4u }, { NOTE_E4, 6u }, { NOTE_G4, 6u },
    { NOTE_B4, 6u }, { NOTE_A4, 6u }, { NOTE_G4, 6u }, { NOTE_E4, 6u },
    { NOTE_D4, 10u }, { NOTE_REST, 8u }, { NOTE_END, 0u }
};

static const note_event_t rooftop_set_track[] = {
    { NOTE_A3, 8u }, { NOTE_C4, 8u }, { NOTE_E4, 8u }, { NOTE_G4, 8u },
    { NOTE_E4, 8u }, { NOTE_C4, 8u }, { NOTE_D4, 8u }, { NOTE_REST, 4u },
    { NOTE_A3, 8u }, { NOTE_D4, 8u }, { NOTE_F4, 8u }, { NOTE_G4, 8u },
    { NOTE_F4, 8u }, { NOTE_D4, 8u }, { NOTE_C4, 8u }, { NOTE_REST, 8u },
    { NOTE_END, 0u }
};

#define TRACK_TITLE_THEME 0xFEu

static const note_event_t title_theme_track[] = {
    { NOTE_E4, 8u }, { NOTE_G4, 8u }, { NOTE_B4, 8u }, { NOTE_DS5, 8u },
    { NOTE_B4, 6u }, { NOTE_G4, 6u }, { NOTE_FS4, 8u }, { NOTE_REST, 4u },
    { NOTE_E4, 8u }, { NOTE_A4, 8u }, { NOTE_C5, 8u }, { NOTE_E5, 8u },
    { NOTE_C5, 6u }, { NOTE_A4, 6u }, { NOTE_G4, 8u }, { NOTE_REST, 6u },
    { NOTE_D4, 8u }, { NOTE_FS4, 8u }, { NOTE_A4, 8u }, { NOTE_C5, 8u },
    { NOTE_A4, 6u }, { NOTE_FS4, 6u }, { NOTE_E4, 8u }, { NOTE_REST, 8u },
    { NOTE_END, 0u }
};

static uint8_t home_record;
static uint8_t current_track;
static uint8_t note_index;
static uint8_t note_frames_left;

static const note_event_t *track_for_record(uint8_t record_id) {
    switch (record_id) {
        case TRACK_TITLE_THEME: return title_theme_track;
        case RECORD_RAIN_LOOP: return rain_loop_track;
        case RECORD_VENDING_DREAMS: return vending_dreams_track;
        case RECORD_ROOFTOP_SET: return rooftop_set_track;
        default: return 0;
    }
}

static uint8_t track_duty(uint8_t record_id) {
    switch (record_id) {
        case TRACK_TITLE_THEME: return 0x40u;
        case RECORD_RAIN_LOOP: return 0x40u;
        case RECORD_VENDING_DREAMS: return 0x40u;
        case RECORD_ROOFTOP_SET: return 0x40u;
        default: return 0x40u;
    }
}

static uint8_t track_envelope(uint8_t record_id) {
    switch (record_id) {
        case TRACK_TITLE_THEME: return 0x52u;
        case RECORD_RAIN_LOOP: return 0x32u;
        case RECORD_VENDING_DREAMS: return 0x42u;
        case RECORD_ROOFTOP_SET: return 0x33u;
        default: return 0x30u;
    }
}

static void stop_music_channel(void) {
    NR22_REG = 0x00u;
    NR24_REG = 0x00u;
}

static void play_music_note(uint8_t record_id, uint8_t note) {
    uint16_t freq = note_freqs[note];

    NR21_REG = track_duty(record_id);
    NR22_REG = track_envelope(record_id);
    NR23_REG = (uint8_t)(freq & 0xFFu);
    NR24_REG = (uint8_t)(0x80u | ((freq >> 8u) & 0x07u));
}

static void play_pulse_sfx(uint16_t freq, uint8_t duty, uint8_t envelope, uint8_t sweep) {
    NR10_REG = sweep;
    NR11_REG = duty;
    NR12_REG = envelope;
    NR13_REG = (uint8_t)(freq & 0xFFu);
    NR14_REG = (uint8_t)(0x80u | ((freq >> 8u) & 0x07u));
}

static void play_noise_sfx(uint8_t envelope, uint8_t noise) {
    NR41_REG = 0x00u;
    NR42_REG = envelope;
    NR43_REG = noise;
    NR44_REG = 0x80u;
}

void dt_audio_init(void) BANKED {
    NR52_REG = 0x80u;
    NR50_REG = 0x33u;
    NR51_REG = 0xFFu;
    home_record = RECORD_NONE;
    current_track = RECORD_NONE;
    note_index = 0u;
    note_frames_left = 0u;
    stop_music_channel();
}

void dt_audio_update(void) BANKED {
    const note_event_t *track;
    note_event_t event;

    if (current_track == RECORD_NONE) {
        return;
    }

    if (note_frames_left != 0u) {
        --note_frames_left;
        if (note_frames_left != 0u) {
            return;
        }
    }

    track = track_for_record(current_track);
    if (track == 0) {
        current_track = RECORD_NONE;
        stop_music_channel();
        return;
    }

    event = track[note_index];
    if (event.note == NOTE_END) {
        note_index = 0u;
        event = track[0];
    }
    ++note_index;
    note_frames_left = event.frames;

    if (event.note == NOTE_REST) {
        stop_music_channel();
    } else {
        play_music_note(current_track, event.note);
    }
}

void dt_audio_play_sfx(uint8_t sfx_id) BANKED {
    switch (sfx_id) {
        case SFX_MENU_MOVE:
            play_noise_sfx(0x12u, 0x20u);
            break;
        case SFX_CONFIRM:
            play_pulse_sfx(note_freqs[NOTE_G5], 0x40u, 0x62u, 0x00u);
            break;
        case SFX_CANCEL:
            play_noise_sfx(0x31u, 0x36u);
            break;
        case SFX_DOOR:
            play_pulse_sfx(note_freqs[NOTE_D4], 0x40u, 0x54u, 0x16u);
            break;
        case SFX_REWARD:
            play_pulse_sfx(note_freqs[NOTE_B4], 0x40u, 0x72u, 0x00u);
            break;
        case SFX_FART:
            play_pulse_sfx(note_freqs[NOTE_D3], 0x40u, 0x82u, 0x1Eu);
            play_noise_sfx(0x43u, 0x5Du);
            break;
        default:
            break;
    }
}

void dt_audio_play_title_theme(void) BANKED {
    if (current_track != TRACK_TITLE_THEME) {
        current_track = TRACK_TITLE_THEME;
        note_index = 0u;
        note_frames_left = 0u;
    }
}

void dt_audio_stop_music(void) BANKED {
    current_track = RECORD_NONE;
    note_index = 0u;
    note_frames_left = 0u;
    stop_music_channel();
}

void dt_audio_set_home_record(uint8_t record_id) BANKED {
    home_record = record_id;
    note_index = 0u;
    note_frames_left = 0u;
}

uint8_t dt_audio_get_home_record(void) BANKED {
    return home_record;
}

void dt_audio_sync(uint8_t room_id) BANKED {
    if ((room_id == ROOM_APARTMENT) && (home_record != RECORD_NONE)) {
        if (current_track != home_record) {
            current_track = home_record;
            note_index = 0u;
            note_frames_left = 0u;
        }
    } else {
        dt_audio_stop_music();
    }
}
