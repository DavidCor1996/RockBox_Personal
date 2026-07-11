#ifndef ARDUBOY_AVR_H
#define ARDUBOY_AVR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ARDUBOY_AVR_SCREEN_W 128
#define ARDUBOY_AVR_SCREEN_H 64
#define ARDUBOY_AVR_SCREEN_BYTES \
    (ARDUBOY_AVR_SCREEN_W * ARDUBOY_AVR_SCREEN_H / 8)

#define ARDUBOY_AVR_BUTTON_UP    0x01
#define ARDUBOY_AVR_BUTTON_DOWN  0x02
#define ARDUBOY_AVR_BUTTON_LEFT  0x04
#define ARDUBOY_AVR_BUTTON_RIGHT 0x08
#define ARDUBOY_AVR_BUTTON_A     0x10
#define ARDUBOY_AVR_BUTTON_B     0x20

struct arduboy_avr {
    uint8_t display[ARDUBOY_AVR_SCREEN_BYTES];
    uint8_t eeprom[1024];
    uint32_t cycles;
    uint32_t timer0_cycles;
    uint32_t timer3_cycles;
    uint16_t frame_count;
    uint32_t spi_writes;
    uint32_t data_writes;
    uint32_t cmd_writes;
    uint32_t eeprom_reads;
    uint32_t pinf_reads;
    uint32_t timer0_interrupts;
    uint32_t sleep_count;
    uint32_t sleep_fast_forwards;
    uint32_t last_pc;
    uint32_t pc_history[8];
    uint16_t instr_history[8];
    uint8_t pc_history_pos;
    uint16_t last_eeprom_addr;
    uint8_t last_eeprom_value;
    uint8_t last_pinf_value;
    uint8_t last_ddrf_value;
    uint8_t last_portf_value;
    uint8_t last_sreg_value;
    uint8_t last_tccr0b_value;
    uint8_t last_timsk0_value;
    uint8_t last_tifr0_value;
    uint8_t last_tcnt0_value;
    uint8_t last_wdtcsr_value;
    uint16_t audio_frequency;
    int8_t speaker_level;
    uint8_t buttons;
    uint8_t page;
    uint8_t column;
    uint8_t cmd;
    uint8_t cmd_args[2];
    uint8_t cmd_arg_count;
    uint8_t cmd_arg_need;
    bool halted;
    bool display_dirty;
    bool quit_requested;
};

void arduboy_avr_reset(struct arduboy_avr *avr, const uint8_t *flash_bytes,
                       size_t flash_size);
void arduboy_avr_set_buttons(struct arduboy_avr *avr, uint8_t buttons);
void arduboy_avr_run_frame(struct arduboy_avr *avr);
bool arduboy_avr_run_display_frame(struct arduboy_avr *avr);
const char *arduboy_avr_error(void);

#endif
