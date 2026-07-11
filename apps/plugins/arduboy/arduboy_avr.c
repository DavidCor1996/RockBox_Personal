#include "arduboy_avr.h"
#include "emu.h"
#include "instr.h"

#define AVR_RAMEND 0x0aff

#define AVR_PINB  0x23
#define AVR_DDRB  0x24
#define AVR_PORTB 0x25
#define AVR_PINC  0x26
#define AVR_DDRC  0x27
#define AVR_PORTC 0x28
#define AVR_PIND  0x29
#define AVR_DDRD  0x2a
#define AVR_PORTD 0x2b
#define AVR_PINE  0x2c
#define AVR_DDRE  0x2d
#define AVR_PORTE 0x2e
#define AVR_PINF  0x2f
#define AVR_DDRF  0x30
#define AVR_PORTF 0x31

#define AVR_TIFR0  0x35
#define AVR_TIFR3  0x38
#define AVR_EECR   0x3f
#define AVR_EEDR   0x40
#define AVR_EEARL  0x41
#define AVR_EEARH  0x42
#define AVR_TCCR0A 0x44
#define AVR_TCCR0B 0x45
#define AVR_TCNT0  0x46
#define AVR_OCR0A  0x47
#define AVR_PLLCSR 0x49
#define AVR_SPCR   0x4c
#define AVR_SPSR   0x4d
#define AVR_SPDR   0x4e
#define AVR_SMCR   0x53
#define AVR_WDTCSR 0x60
#define AVR_TIMSK0 0x6e
#define AVR_TIMSK3 0x71
#define AVR_ADCL   0x78
#define AVR_ADCH   0x79
#define AVR_ADCSRA 0x7a
#define AVR_USBINT 0xda
#define AVR_TCCR3A 0x90
#define AVR_TCCR3B 0x91
#define AVR_TCNT3L 0x94
#define AVR_TCNT3H 0x95
#define AVR_OCR3AL 0x98
#define AVR_OCR3AH 0x99

#define AVR_SPIF 0x80
#define AVR_SPE  0x40
#define AVR_ADSC 0x40
#define AVR_TOIE0 0x01
#define AVR_OCF3A 0x02
#define AVR_OCIE3A 0x02
#define AVR_COM3A0 0x40
#define AVR_WGM32 0x08
#define AVR_PLOCK 0x01
#define AVR_EERE  0x01
#define AVR_EEPE  0x02
#define AVR_EEMPE 0x04
#define AVR_SE    0x01

#define OLED_DC_BIT 4
#define OLED_CS_BIT 6

#define BTN_B_BIT     4
#define BTN_A_BIT     6
#define BTN_DOWN_BIT  4
#define BTN_LEFT_BIT  5
#define BTN_RIGHT_BIT 6
#define BTN_UP_BIT    7

#define TIMER0_VECTOR_WORD 46
#define TIMER3_COMPA_VECTOR_WORD 64
#define FRAME_CYCLES 533333
#define AVR_CPU_HZ 16000000UL
#define AVR_CYCLE_SCALE 64
#define DISPLAY_FRAME_BYTES ARDUBOY_AVR_SCREEN_BYTES
#define DISPLAY_FRAME_MAX_INSNS 120000UL

uint32_t pc;
uint32_t pc_start;
uint32_t instr_size;
bool skip_next_instruction;
uint8_t memory[AVR_DATA_SIZE];
uint16_t flash[AVR_FLASH_WORDS];
bool pc22;
bool pc_mem_max_64k;
bool pc_mem_max_256b;
bool off;
bool replay_mode;
bool stepone;
uint64_t insns;
uint64_t insnreplaylim;
uint64_t insnlimit;

static char avr_error[64];
static struct arduboy_avr *active_avr;
static const struct instr_decode *decode_cache[UINT16_MAX + 1];

#ifdef ARDUBOY_PROFILE
uint32_t arduboy_profile_counts[UINT16_MAX + 1];
uint32_t arduboy_profile_slow_counts[UINT16_MAX + 1];
uint32_t arduboy_profile_fast_total;
uint32_t arduboy_profile_slow_total;
#endif

static const struct instr_decode avr_instr[] = {
    { 0x0000, 0xffff, instr_nop },
    { 0x0100, 0xff00, instr_movw, .dddd74 = true, .rrrr30 = true },
    { 0x0200, 0xff00, instr_muls, .dddd74 = true, .rrrr30 = true },
    { 0x0300, 0xff88, instr_mulsu, .ddd64 = true, .rrr20 = true },
    { 0x0308, 0xff88, instr_fmul, .ddd64 = true, .rrr20 = true },
    { 0x0380, 0xff80, instr_fmulsu, .ddd64 = true, .rrr20 = true },
    { 0x0400, 0xec00, instr_cpc, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x0800, 0xec00, instr_cpc, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x0c00, 0xec00, instr_adc, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x1000, 0xfc00, instr_cpse, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x2000, 0xfc00, instr_and, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x2400, 0xfc00, instr_xor, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x2800, 0xfc00, instr_or, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x2c00, 0xfc00, instr_mov, .ddddd84 = true, .rrrrr9_30 = true },
    { 0x3000, 0xf000, instr_cpi, .dddd74 = true, .KKKK118_30 = true },
    { 0x4000, 0xe000, instr_cpi, .dddd74 = true, .KKKK118_30 = true },
    { 0x6000, 0xf000, instr_ori, .dddd74 = true, .KKKK118_30 = true },
    { 0x7000, 0xf000, instr_andi, .dddd74 = true, .KKKK118_30 = true },
    { 0x8000, 0xd200, instr_ldyz, .ddddd84 = true },
    { 0x8200, 0xd200, instr_styz, .ddddd84 = true },
    { 0x9000, 0xfe0f, instr_lds, .ddddd84 = true, .imm16 = true },
    { 0x9001, 0xfe07, instr_ldyz, .ddddd84 = true },
    { 0x9002, 0xfe07, instr_ldyz, .ddddd84 = true },
    { 0x9004, 0xfe0c, instr_elpmz, .ddddd84 = true },
    { 0x900c, 0xfe0e, instr_ldx, .ddddd84 = true },
    { 0x900e, 0xfe0f, instr_ldx, .ddddd84 = true },
    { 0x900f, 0xfe0f, instr_pop, .ddddd84 = true },
    { 0x9200, 0xfe0f, instr_sts, .ddddd84 = true, .imm16 = true },
    { 0x9201, 0xfe07, instr_styz, .ddddd84 = true },
    { 0x9202, 0xfe07, instr_styz, .ddddd84 = true },
    { 0x9204, 0xfe0c, instr_xch, .ddddd84 = true },
    { 0x920c, 0xfe0e, instr_stx, .ddddd84 = true },
    { 0x920e, 0xfe0f, instr_stx, .ddddd84 = true },
    { 0x920f, 0xfe0f, instr_push, .ddddd84 = true },
    { 0x9400, 0xfe0f, instr_com, .ddddd84 = true },
    { 0x9401, 0xfe0f, instr_neg, .ddddd84 = true },
    { 0x9402, 0xfe0f, instr_swap, .ddddd84 = true },
    { 0x9403, 0xfe0f, instr_inc, .ddddd84 = true },
    { 0x9405, 0xfe0f, instr_asr, .ddddd84 = true },
    { 0x9406, 0xfe0f, instr_lsr, .ddddd84 = true },
    { 0x9407, 0xfe0f, instr_ror, .ddddd84 = true },
    { 0x9408, 0xff0f, instr_bclrset, .ddd64 = true },
    { 0x940a, 0xfe0f, instr_dec, .ddddd84 = true },
    { 0x940b, 0xff0f, instr_des, .dddd74 = true },
    { 0x940c, 0xfe0e, instr_jmp, .imm16 = true },
    { 0x940e, 0xfe0e, instr_call, .imm16 = true },
    { 0x9508, 0xffff, instr_ret },
    { 0x9409, 0xfeef, instr_eicalljump },
    { 0x9518, 0xffff, instr_reti },
    { 0x9588, 0xffff, instr_nop },
    { 0x9598, 0xffff, instr_unimp },
    { 0x95a8, 0xffff, instr_nop },
    { 0x95c8, 0xffef, instr_elpm },
    { 0x95e8, 0xffff, instr_nop },
    { 0x95f8, 0xffff, instr_nop },
    { 0x9600, 0xff00, instr_adiw },
    { 0x9700, 0xff00, instr_sbiw },
    { 0x9800, 0xfd00, instr_cbisbi, .rrr20 = true },
    { 0x9900, 0xfd00, instr_sbics, .rrr20 = true },
    { 0x9c00, 0xfc00, instr_mul, .ddddd84 = true, .rrrrr9_30 = true },
    { 0xb000, 0xf800, instr_in, .ddddd84 = true },
    { 0xb800, 0xf800, instr_out, .ddddd84 = true },
    { 0xc000, 0xe000, instr_rcalljmp },
    { 0xe000, 0xf000, instr_ldi, .dddd74 = true, .KKKK118_30 = true },
    { 0xf000, 0xf800, instr_brb, .rrr20 = true },
    { 0xf800, 0xfe08, instr_bld, .ddddd84 = true, .rrr20 = true },
    { 0xfa00, 0xfe08, instr_bst, .ddddd84 = true, .rrr20 = true },
    { 0xfc00, 0xfc08, instr_sbrcs, .ddddd84 = true, .rrr20 = true },
};

static const struct instr_decode *find_decode(uint16_t instr)
{
    const struct instr_decode *decode = decode_cache[instr];
    size_t i;

    if (decode != NULL)
        return decode;

    for (i = 0; i < ARRAYLEN(avr_instr); i++)
    {
        if ((instr & avr_instr[i].mask) == avr_instr[i].pattern)
        {
            decode_cache[instr] = &avr_instr[i];
            return &avr_instr[i];
        }
    }

    return NULL;
}

const char *arduboy_avr_error(void)
{
    return avr_error;
}

static void set_error(const char *text, uint16_t instr)
{
    if (active_avr)
    {
        uint8_t pos = (active_avr->pc_history_pos - 1) & 7;

        rb->snprintf(avr_error, sizeof(avr_error),
                     "%s %04x @%04lx p%04lx/%04x",
                     text, instr, (unsigned long)pc_start,
                     (unsigned long)active_avr->pc_history[pos],
                     active_avr->instr_history[pos]);
    }
    else
    {
        rb->snprintf(avr_error, sizeof(avr_error), "%s %04x @%04lx",
                     text, instr, (unsigned long)pc_start);
    }
}

void _unhandled(const char *f, unsigned l, uint16_t instr)
{
    (void)f;
    (void)l;
    set_error("unhandled", instr);
    off = true;
    if (active_avr)
        active_avr->halted = true;
}

void _illins(const char *f, unsigned l, uint16_t instr)
{
    (void)f;
    (void)l;
    set_error("illegal", instr);
    off = true;
    if (active_avr)
        active_avr->halted = true;
}

void abort_nodump(void)
{
    _illins(__FILE__, __LINE__, romword(pc));
}

static void push_byte(uint8_t value)
{
    uint16_t sp = getsp();

    memory[sp] = value;
    setsp(sp - 1);
}

static void request_interrupt(uint16_t vector_word)
{
    uint32_t next_pc = pc;

    push_byte(next_pc & 0xff);
    push_byte((next_pc >> 8) & 0xff);
    memory[SREG] &= ~SREG_I;
    pc = vector_word;
}

static void oled_data(struct arduboy_avr *avr, uint8_t value)
{
    if (avr->page < 8 && avr->column < 128)
    {
        avr->display[avr->page * 128 + avr->column] = value;
        avr->display_dirty = true;
    }

    avr->column++;
    if (avr->column >= 128)
    {
        avr->column = 0;
        avr->page = (avr->page + 1) & 7;
    }
}

static uint8_t oled_command_args(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x20:
        case 0x81:
        case 0x8d:
        case 0xd3:
        case 0xd5:
        case 0xd9:
        case 0xdb:
            return 1;
        case 0x21:
        case 0x22:
            return 2;
        default:
            return 0;
    }
}

static void oled_finish_command(struct arduboy_avr *avr)
{
    switch (avr->cmd)
    {
        case 0x21:
            avr->column = avr->cmd_args[0] & 0x7f;
            break;
        case 0x22:
            avr->page = avr->cmd_args[0] & 0x07;
            break;
        default:
            break;
    }
}

static void oled_command(struct arduboy_avr *avr, uint8_t value)
{
    if (avr->cmd_arg_count < avr->cmd_arg_need)
    {
        avr->cmd_args[avr->cmd_arg_count++] = value;
        if (avr->cmd_arg_count >= avr->cmd_arg_need)
            oled_finish_command(avr);
        return;
    }

    if ((value & 0xf0) == 0xb0)
    {
        avr->page = value & 0x07;
        return;
    }
    if ((value & 0xf0) == 0x00)
    {
        avr->column = (avr->column & 0xf0) | (value & 0x0f);
        return;
    }
    if ((value & 0xf0) == 0x10)
    {
        avr->column = (avr->column & 0x0f) | ((value & 0x0f) << 4);
        return;
    }

    avr->cmd = value;
    avr->cmd_arg_count = 0;
    avr->cmd_arg_need = oled_command_args(value);
    if (avr->cmd_arg_need == 0)
        oled_finish_command(avr);
}

static void handle_spi_write(struct arduboy_avr *avr, uint8_t value)
{
    bool selected = (memory[AVR_PORTD] & (1 << OLED_CS_BIT)) == 0;
    bool data = (memory[AVR_PORTD] & (1 << OLED_DC_BIT)) != 0;

    avr->spi_writes++;
    if (selected)
    {
        if (data)
        {
            avr->data_writes++;
            oled_data(avr, value);
        }
        else
        {
            avr->cmd_writes++;
            oled_command(avr, value);
        }
    }

    memory[AVR_SPSR] |= AVR_SPIF;
}

static uint16_t eeprom_addr(void)
{
    return (uint16_t)memory[AVR_EEARL] |
           ((uint16_t)(memory[AVR_EEARH] & 0x03) << 8);
}

static void handle_eeprom_write(struct arduboy_avr *avr, uint8_t value)
{
    uint16_t addr = eeprom_addr() & 0x03ff;

    memory[AVR_EECR] = value;
    if (value & AVR_EERE)
    {
        memory[AVR_EEDR] = avr->eeprom[addr];
        avr->eeprom_reads++;
        avr->last_eeprom_addr = addr;
        avr->last_eeprom_value = memory[AVR_EEDR];
        memory[AVR_EECR] &= ~AVR_EERE;
    }
    if (value & AVR_EEPE)
    {
        avr->eeprom[addr] = memory[AVR_EEDR];
        memory[AVR_EECR] &= ~(AVR_EEPE | AVR_EEMPE);
    }
}

static uint8_t read_pin(uint16_t pin_addr, uint16_t ddr_addr, uint16_t port_addr)
{
    uint8_t ddr = memory[ddr_addr];
    uint8_t port = memory[port_addr];
    uint8_t external = memory[pin_addr];

    return (port & ddr) | (external & (uint8_t)~ddr);
}

static uint16_t mem_u16(uint16_t lo_addr)
{
    return (uint16_t)memory[lo_addr] |
           ((uint16_t)memory[(lo_addr + 1) & 0xffff] << 8);
}

static void write_mem_u16(uint16_t lo_addr, uint16_t value)
{
    memory[lo_addr] = value & 0xff;
    memory[(lo_addr + 1) & 0xffff] = value >> 8;
}

static inline uint8_t fast_io_read(uint16_t addr)
{
    if (addr >= 0x0100)
        return memory[addr];

    return arduboy_avr_io_read(addr);
}

static inline void fast_io_write(uint16_t addr, uint8_t value)
{
    if (addr >= 0x0100)
    {
        memory[addr] = value;
        return;
    }

    arduboy_avr_io_write(addr, value);
}

static inline uint8_t fast_d(uint16_t instr)
{
    return (instr >> 4) & 0x1f;
}

static inline uint8_t fast_r(uint16_t instr)
{
    return (instr & 0x0f) | ((instr >> 5) & 0x10);
}

static inline uint8_t fast_k8(uint16_t instr)
{
    return (instr & 0x0f) | ((instr >> 4) & 0xf0);
}

static inline void fast_logic_flags(uint8_t result)
{
    uint8_t sreg = memory[SREG] & (uint8_t)~(SREG_N | SREG_Z |
                                             SREG_V | SREG_S);

    if (result == 0)
        sreg |= SREG_Z;
    if (result & 0x80)
        sreg |= SREG_N | SREG_S;
    memory[SREG] = sreg;
}

static inline void fast_add_flags(uint8_t rd, uint8_t rr, uint8_t res)
{
    uint8_t sreg = memory[SREG] & (uint8_t)~(SREG_H | SREG_S | SREG_V |
                                             SREG_N | SREG_Z | SREG_C);

    if (((rd & rr) | (rr & (uint8_t)~res) |
         ((uint8_t)~res & rd)) & 0x80)
        sreg |= SREG_C;
    if (((rd & rr) | (rr & (uint8_t)~res) |
         ((uint8_t)~res & rd)) & 0x08)
        sreg |= SREG_H;
    if (((rd & rr & (uint8_t)~res) |
         ((uint8_t)~rd & (uint8_t)~rr & res)) & 0x80)
        sreg |= SREG_V;
    if (res & 0x80)
        sreg |= SREG_N;
    if (res == 0)
        sreg |= SREG_Z;
    if (((sreg & SREG_N) != 0) != ((sreg & SREG_V) != 0))
        sreg |= SREG_S;
    memory[SREG] = sreg;
}

static inline void fast_sub_flags(uint8_t rd, uint8_t rr, uint16_t subtrahend,
                                  uint8_t res, bool carry_mode, bool old_z)
{
    uint8_t sreg = memory[SREG] & (uint8_t)~(SREG_H | SREG_S | SREG_V |
                                             SREG_N | SREG_Z | SREG_C);
    uint8_t carry = subtrahend != rr ? 1 : 0;

    if (((rd & (uint8_t)~rr & (uint8_t)~res) |
         ((uint8_t)~rd & rr & res)) & 0x80)
        sreg |= SREG_V;
    if (res & 0x80)
        sreg |= SREG_N;
    if (carry_mode)
    {
        if (res == 0 && old_z)
            sreg |= SREG_Z;
    }
    else if (res == 0)
        sreg |= SREG_Z;
    if ((rd & 0x0f) < ((rr & 0x0f) + carry))
        sreg |= SREG_H;
    if (rd < subtrahend)
        sreg |= SREG_C;
    if (((sreg & SREG_N) != 0) != ((sreg & SREG_V) != 0))
        sreg |= SREG_S;
    memory[SREG] = sreg;
}

static inline void fast_adiw_flags(uint16_t rd, uint16_t res)
{
    uint8_t sreg = memory[SREG] & (uint8_t)~(SREG_S | SREG_V |
                                             SREG_N | SREG_Z | SREG_C);

    if (res & 0x8000)
        sreg |= SREG_N;
    if ((uint16_t)(~rd) & res & 0x8000)
        sreg |= SREG_V;
    if (res == 0)
        sreg |= SREG_Z;
    if ((uint16_t)(~res) & rd & 0x8000)
        sreg |= SREG_C;
    if (((sreg & SREG_N) != 0) != ((sreg & SREG_V) != 0))
        sreg |= SREG_S;
    memory[SREG] = sreg;
}

static inline void fast_sbiw_flags(uint16_t rd, uint16_t res)
{
    uint8_t sreg = memory[SREG] & (uint8_t)~(SREG_S | SREG_V |
                                             SREG_N | SREG_Z | SREG_C);

    if (res == 0)
        sreg |= SREG_Z;
    if ((uint16_t)(~res) & rd & 0x8000)
        sreg |= SREG_C;
    if (res & 0x8000)
        sreg |= SREG_N;
    if (rd & (uint16_t)(~res) & 0x8000)
        sreg |= SREG_V;
    if (((sreg & SREG_N) != 0) != ((sreg & SREG_V) != 0))
        sreg |= SREG_S;
    memory[SREG] = sreg;
}

static inline void fast_shift_flags(uint8_t rd, uint8_t res)
{
    uint8_t sreg = memory[SREG] & (uint8_t)~(SREG_S | SREG_V |
                                             SREG_N | SREG_Z | SREG_C);

    if (rd & 0x01)
        sreg |= SREG_C;
    if (res & 0x80)
        sreg |= SREG_N;
    if (res == 0)
        sreg |= SREG_Z;
    if (((sreg & SREG_N) != 0) != ((sreg & SREG_C) != 0))
        sreg |= SREG_V;
    if (((sreg & SREG_N) != 0) != ((sreg & SREG_V) != 0))
        sreg |= SREG_S;
    memory[SREG] = sreg;
}

static uint32_t timer_prescale(uint8_t cs)
{
    switch (cs & 0x07)
    {
        case 1:
            return 1;
        case 2:
            return 8;
        case 3:
            return 64;
        case 4:
            return 256;
        case 5:
            return 1024;
        default:
            return 0;
    }
}

static void update_speaker_state(struct arduboy_avr *avr)
{
    uint8_t portc = memory[AVR_PORTC] & memory[AVR_DDRC];
    uint16_t ocr3a = mem_u16(AVR_OCR3AL);
    uint32_t prescale = timer_prescale(memory[AVR_TCCR3B]);

    avr->speaker_level = 0;
    if (portc & (1 << 6))
        avr->speaker_level++;
    if (portc & (1 << 7))
        avr->speaker_level--;

    avr->audio_frequency = 0;
    if (prescale != 0 && ocr3a > 0 &&
        ((memory[AVR_TCCR3A] & AVR_COM3A0) ||
         (memory[AVR_TIMSK3] & AVR_OCIE3A)))
    {
        uint32_t freq = AVR_CPU_HZ / (2UL * prescale * ((uint32_t)ocr3a + 1));

        if (freq > 30 && freq < 20000)
            avr->audio_frequency = (uint16_t)freq;
    }
}

uint8_t arduboy_avr_io_read(uint16_t addr)
{
    addr &= 0xffff;
    if (addr >= 0x0100)
        return memory[addr];

    switch (addr)
    {
        case AVR_PINB:
            return read_pin(AVR_PINB, AVR_DDRB, AVR_PORTB);
        case AVR_PINC:
            return read_pin(AVR_PINC, AVR_DDRC, AVR_PORTC);
        case AVR_PIND:
            return read_pin(AVR_PIND, AVR_DDRD, AVR_PORTD);
        case AVR_PINE:
            return read_pin(AVR_PINE, AVR_DDRE, AVR_PORTE);
        case AVR_PINF:
            if (active_avr)
            {
                active_avr->pinf_reads++;
                active_avr->last_pinf_value =
                    read_pin(AVR_PINF, AVR_DDRF, AVR_PORTF);
                active_avr->last_ddrf_value = memory[AVR_DDRF];
                active_avr->last_portf_value = memory[AVR_PORTF];
                return active_avr->last_pinf_value;
            }
            return read_pin(AVR_PINF, AVR_DDRF, AVR_PORTF);
        default:
            return memory[addr];
    }
}

void arduboy_avr_io_write(uint16_t addr, uint8_t value)
{
    uint8_t old_value;

    addr &= 0xffff;
    if (addr >= 0x0100)
    {
        memory[addr] = value;
        return;
    }

    old_value = memory[addr];
    memory[addr] = value;

    if (!active_avr)
        return;

    if (addr == AVR_TIFR0)
        memory[AVR_TIFR0] = old_value & (uint8_t)~value;
    else if (addr == AVR_TIFR3)
        memory[AVR_TIFR3] = old_value & (uint8_t)~value;
    else if (addr == AVR_EECR)
        handle_eeprom_write(active_avr, value);
    else if (addr == AVR_PLLCSR && (value & 0x02))
        memory[AVR_PLLCSR] = value | AVR_PLOCK;
    else if (addr == AVR_SPDR && (memory[AVR_SPCR] & AVR_SPE))
        handle_spi_write(active_avr, value);
    else if (addr == AVR_ADCSRA && (value & AVR_ADSC))
    {
        memory[AVR_ADCSRA] &= ~AVR_ADSC;
        memory[AVR_ADCL] = (uint8_t)(active_avr->cycles ^ 0x5a);
        memory[AVR_ADCH] = (uint8_t)((active_avr->cycles >> 8) & 0x03);
    }
    else if (addr == AVR_WDTCSR)
        active_avr->last_wdtcsr_value = value;

    if (addr == AVR_PORTC || addr == AVR_DDRC || addr == AVR_TCCR3A ||
        addr == AVR_TCCR3B || addr == AVR_OCR3AL || addr == AVR_OCR3AH ||
        addr == AVR_TIMSK3)
        update_speaker_state(active_avr);
}

static void apply_buttons(struct arduboy_avr *avr)
{
    uint8_t pinf = 0xff;
    uint8_t pine = 0xff;
    uint8_t pinb = 0xff;

    if (avr->buttons & ARDUBOY_AVR_BUTTON_UP)
        pinf &= ~(1 << BTN_UP_BIT);
    if (avr->buttons & ARDUBOY_AVR_BUTTON_DOWN)
        pinf &= ~(1 << BTN_DOWN_BIT);
    if (avr->buttons & ARDUBOY_AVR_BUTTON_LEFT)
        pinf &= ~(1 << BTN_LEFT_BIT);
    if (avr->buttons & ARDUBOY_AVR_BUTTON_RIGHT)
        pinf &= ~(1 << BTN_RIGHT_BIT);
    if (avr->buttons & ARDUBOY_AVR_BUTTON_A)
        pine &= ~(1 << BTN_A_BIT);
    if (avr->buttons & ARDUBOY_AVR_BUTTON_B)
        pinb &= ~(1 << BTN_B_BIT);

    memory[AVR_PINF] = (memory[AVR_PINF] & 0x0f) | pinf;
    memory[AVR_PINE] = (memory[AVR_PINE] & ~0x40) | pine;
    memory[AVR_PINB] = (memory[AVR_PINB] & ~0x10) | pinb;
}

static bool fast_forward_sleep(struct arduboy_avr *avr);

static inline uint8_t fast_pop_byte(void)
{
    uint16_t sp = getsp();

    setsp(sp + 1);
    return memory[sp + 1];
}

static inline void fast_push_byte(uint8_t value)
{
    push_byte(value);
}

static inline bool fast_finish_step(struct arduboy_avr *avr)
{
    pc += instr_size;
    insns++;
    avr->cycles += AVR_CYCLE_SCALE;
    return true;
}

static bool fast_step_common(struct arduboy_avr *avr, uint16_t instr)
{
    uint8_t d;
    uint8_t r;
    uint8_t rd;
    uint8_t rr;
    uint8_t res;
    uint8_t sreg;
    uint16_t addr;
    uint16_t word;

    if ((instr & 0xf000) == 0xe000)
    {
        memory[16 + ((instr >> 4) & 0x0f)] = fast_k8(instr);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfc00) == 0x2c00)
    {
        memory[fast_d(instr)] = memory[fast_r(instr)];
        return fast_finish_step(avr);
    }

    if ((instr & 0xfc00) == 0x2400)
    {
        d = fast_d(instr);
        memory[d] ^= memory[fast_r(instr)];
        fast_logic_flags(memory[d]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfc00) == 0x2000)
    {
        d = fast_d(instr);
        memory[d] &= memory[fast_r(instr)];
        fast_logic_flags(memory[d]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfc00) == 0x2800)
    {
        d = fast_d(instr);
        memory[d] |= memory[fast_r(instr)];
        fast_logic_flags(memory[d]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xff00) == 0x0100)
    {
        uint8_t dd = ((instr >> 4) & 0x0f) << 1;
        uint8_t rr2 = (instr & 0x0f) << 1;

        write_mem_u16(dd, mem_u16(rr2));
        return fast_finish_step(avr);
    }

    if ((instr & 0xec00) == 0x0c00)
    {
        d = fast_d(instr);
        r = fast_r(instr);
        rd = memory[d];
        rr = memory[r];
        res = rd + rr;
        if ((instr & 0x1000) && (memory[SREG] & SREG_C))
            res++;
        memory[d] = res;
        fast_add_flags(rd, rr, res);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfc00) == 0x9c00)
    {
        uint16_t mul_res;
        d = fast_d(instr);
        r = fast_r(instr);
        mul_res = (uint16_t)memory[d] * (uint16_t)memory[r];
        write_mem_u16(0, mul_res);
        memory[SREG] &= (uint8_t)~(SREG_C | SREG_Z);
        if (mul_res == 0)
            memory[SREG] |= SREG_Z;
        if (mul_res & 0x8000)
            memory[SREG] |= SREG_C;
        return fast_finish_step(avr);
    }

    if ((instr & 0xec00) == 0x0400 || (instr & 0xec00) == 0x0800)
    {
        bool carry_mode = (instr & 0x1000) == 0;
        bool nostore = ((instr >> 10) & 0x03) == 1;
        bool old_z = (memory[SREG] & SREG_Z) != 0;
        uint16_t subtrahend;

        d = fast_d(instr);
        r = fast_r(instr);
        rd = memory[d];
        rr = memory[r];
        subtrahend = rr;
        if (carry_mode && (memory[SREG] & SREG_C))
            subtrahend++;
        res = rd - subtrahend;
        if (!nostore)
            memory[d] = res;
        fast_sub_flags(rd, rr, subtrahend, res, carry_mode, old_z);
        return fast_finish_step(avr);
    }

    if ((instr & 0xf000) == 0x3000 || (instr & 0xe000) == 0x4000)
    {
        bool carry_mode = (instr & 0x1000) == 0;
        bool store = ((instr >> 13) & 0x03) == 2;
        bool old_z = (memory[SREG] & SREG_Z) != 0;
        uint16_t subtrahend;

        d = 16 + ((instr >> 4) & 0x0f);
        rr = fast_k8(instr);
        rd = memory[d];
        subtrahend = rr;
        if (carry_mode && (memory[SREG] & SREG_C))
            subtrahend++;
        res = rd - subtrahend;
        if (store)
            memory[d] = res;
        fast_sub_flags(rd, rr, subtrahend, res, carry_mode, old_z);
        return fast_finish_step(avr);
    }

    if ((instr & 0xf000) == 0x6000)
    {
        d = 16 + ((instr >> 4) & 0x0f);
        memory[d] |= fast_k8(instr);
        fast_logic_flags(memory[d]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xf000) == 0x7000)
    {
        d = 16 + ((instr >> 4) & 0x0f);
        memory[d] &= fast_k8(instr);
        fast_logic_flags(memory[d]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xf800) == 0xf000)
    {
        uint8_t bit = instr & 0x07;
        bool clear = (instr & 0x0400) != 0;
        int8_t offset = (instr >> 3) & 0x7f;

        if (offset & 0x40)
            offset |= 0x80;
        if (((memory[SREG] & (1 << bit)) != 0) != clear)
            pc += offset;
        return fast_finish_step(avr);
    }

    if ((instr & 0xf800) == 0xb000)
    {
        uint8_t io = ((instr >> 5) & 0x30) | (instr & 0x0f);

        memory[fast_d(instr)] = arduboy_avr_io_read(AVR_IO_BASE + io);
        return fast_finish_step(avr);
    }

    if ((instr & 0xf800) == 0xb800)
    {
        uint8_t io = ((instr >> 5) & 0x30) | (instr & 0x0f);

        arduboy_avr_io_write(AVR_IO_BASE + io, memory[fast_d(instr)]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xe000) == 0xc000)
    {
        int16_t offset = instr & 0x0fff;

        if (offset & 0x0800)
            offset |= 0xf000;
        if (instr & 0x1000)
        {
            uint32_t ret = pc_start + 1;

            fast_push_byte(ret & 0xff);
            fast_push_byte((ret >> 8) & 0xff);
        }
        pc += offset;
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x940a)
    {
        d = fast_d(instr);
        res = memory[d] - 1;
        memory[d] = res;
        fast_logic_flags(res);
        if (res == 0x7f)
            memory[SREG] = (memory[SREG] & (uint8_t)~SREG_N) |
                           SREG_V | SREG_S;
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9403)
    {
        d = fast_d(instr);
        res = memory[d] + 1;
        memory[d] = res;
        fast_logic_flags(res);
        if (res == 0x80)
            memory[SREG] = (memory[SREG] & (uint8_t)~SREG_S) |
                           SREG_N | SREG_V;
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9402)
    {
        d = fast_d(instr);
        rd = memory[d];
        memory[d] = (rd >> 4) | (rd << 4);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9406)
    {
        d = fast_d(instr);
        rd = memory[d];
        res = rd >> 1;
        memory[d] = res;
        fast_shift_flags(rd, res);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9407)
    {
        d = fast_d(instr);
        rd = memory[d];
        res = rd >> 1;
        if (memory[SREG] & SREG_C)
            res |= 0x80;
        memory[d] = res;
        fast_shift_flags(rd, res);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9405)
    {
        d = fast_d(instr);
        rd = memory[d];
        res = (rd >> 1) | (rd & 0x80);
        memory[d] = res;
        fast_shift_flags(rd, res);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9400)
    {
        d = fast_d(instr);
        res = 0xff - memory[d];
        memory[d] = res;
        fast_logic_flags(res);
        memory[SREG] |= SREG_C;
        return fast_finish_step(avr);
    }

    if ((instr & 0xff00) == 0x9600 || (instr & 0xff00) == 0x9700)
    {
        uint8_t imm = ((instr >> 2) & 0x30) | (instr & 0x0f);
        uint16_t old_word;

        d = 24 + ((instr >> 3) & 0x06);
        old_word = mem_u16(d);
        word = old_word;
        if ((instr & 0xff00) == 0x9600)
            word += imm;
        else
            word -= imm;
        write_mem_u16(d, word);
        if ((instr & 0xff00) == 0x9600)
            fast_adiw_flags(old_word, word);
        else
            fast_sbiw_flags(old_word, word);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x900f)
    {
        memory[fast_d(instr)] = fast_pop_byte();
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x920f)
    {
        fast_push_byte(memory[fast_d(instr)]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9000)
    {
        instr_size = 2;
        memory[fast_d(instr)] = fast_io_read(romword(pc + 1));
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x9200)
    {
        instr_size = 2;
        fast_io_write(romword(pc + 1), memory[fast_d(instr)]);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0c) == 0x9004)
    {
        uint32_t prog_addr = mem_u16(REGP_Z);
        bool ext = (instr & 0x0002) != 0;
        bool postinc = (instr & 0x0001) != 0;
        uint16_t prog_word;

        if (ext)
            prog_addr |= ((uint32_t)memory[RAMPZ] << 16);
        prog_word = romword(prog_addr >> 1);
        if (prog_addr & 1)
            memory[fast_d(instr)] = prog_word >> 8;
        else
            memory[fast_d(instr)] = prog_word & 0xff;
        if (postinc)
        {
            prog_addr++;
            write_mem_u16(REGP_Z, prog_addr & 0xffff);
        }
        return fast_finish_step(avr);
    }

    if ((instr & 0xd200) == 0x8000 || (instr & 0xd200) == 0x8200)
    {
        uint8_t rp = (instr & 0x0008) ? REGP_Y : REGP_Z;
        uint8_t disp;
        bool store = (instr & 0x0200) != 0;

        if ((instr & 0x1000) == 0)
            disp = ((instr >> 8) & 0x20) | ((instr >> 7) & 0x18) |
                   (instr & 0x07);
        else
            disp = 0;
        addr = (mem_u16(rp) + disp) & 0xffff;
        if (store)
            fast_io_write(addr, memory[fast_d(instr)]);
        else
            memory[fast_d(instr)] = fast_io_read(addr);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0f) == 0x900c || (instr & 0xfe0f) == 0x920c)
    {
        uint8_t mode = instr & 0x03;
        bool store = (instr & 0x0200) != 0;

        addr = mem_u16(REGP_X);
        if (mode == 2)
            addr--;
        if (store)
            fast_io_write(addr, memory[fast_d(instr)]);
        else
            memory[fast_d(instr)] = fast_io_read(addr);
        if (mode == 1)
            addr++;
        if (mode == 1 || mode == 2)
            write_mem_u16(REGP_X, addr);
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe07) == 0x9001 || (instr & 0xfe07) == 0x9201)
    {
        uint8_t rp = (instr & 0x0008) ? REGP_Y : REGP_Z;
        uint8_t mode = instr & 0x03;
        bool store = (instr & 0x0200) != 0;

        addr = mem_u16(rp);
        if (mode == 2)
            addr--;
        if (store)
            fast_io_write(addr, memory[fast_d(instr)]);
        else
            memory[fast_d(instr)] = fast_io_read(addr);
        if (mode == 1)
            addr++;
        if (mode == 1 || mode == 2)
            write_mem_u16(rp, addr);
        return fast_finish_step(avr);
    }

    if (instr == 0x9508 || instr == 0x9518)
    {
        pc = ((uint32_t)fast_pop_byte() << 8);
        pc |= fast_pop_byte();
        if (instr == 0x9518)
            memory[SREG] |= SREG_I;
        pc -= instr_size;
        return fast_finish_step(avr);
    }

    if ((instr & 0xfe0e) == 0x940c || (instr & 0xfe0e) == 0x940e)
    {
        instr_size = 2;
        if (instr & 0x0002)
        {
            uint32_t ret = pc_start + 2;

            fast_push_byte(ret & 0xff);
            fast_push_byte((ret >> 8) & 0xff);
        }
        pc = romword(pc + 1) - instr_size;
        return fast_finish_step(avr);
    }

    (void)avr;
    (void)sreg;
    return false;
}

static bool step_one(struct arduboy_avr *avr)
{
    struct instr_decode_common idc;
    const struct instr_decode *decode;
    uint16_t instr;

restart:
    if (pc >= AVR_FLASH_WORDS)
    {
        _illins(__FILE__, __LINE__, 0xffff);
        return false;
    }

    pc_start = pc;
    avr->last_pc = pc;
    instr_size = 1;
    instr = romword(pc);
#ifdef ARDUBOY_PROFILE
    arduboy_profile_counts[instr]++;
#endif
    avr->pc_history[avr->pc_history_pos] = pc;
    avr->instr_history[avr->pc_history_pos] = instr;
    avr->pc_history_pos = (avr->pc_history_pos + 1) & 7;

    decode = find_decode(instr);
    if (decode == NULL)
    {
        _illins(__FILE__, __LINE__, instr);
        return false;
    }

    idc.instr = instr;
    idc.ddddd = 0;
    idc.rrrrr = 0;
    idc.imm_u8 = 0;
    idc.imm_u16 = 0;
    idc.setflags = 0;
    idc.clrflags = 0;

    if (decode->imm16)
    {
        idc.imm_u16 = romword(pc + 1);
        instr_size++;
    }

    if (skip_next_instruction)
    {
        skip_next_instruction = false;
        pc += instr_size;
        return true;
    }

    if (instr == 0x9588)
    {
        avr->sleep_count++;
        pc += instr_size;
        insns++;
        avr->cycles += AVR_CYCLE_SCALE;
        if (fast_forward_sleep(avr))
            avr->sleep_fast_forwards++;
        return true;
    }

    if (fast_step_common(avr, instr))
    {
#ifdef ARDUBOY_PROFILE
        arduboy_profile_fast_total++;
#endif
        return true;
    }

#ifdef ARDUBOY_PROFILE
    arduboy_profile_slow_counts[instr]++;
    arduboy_profile_slow_total++;
#endif

    if (decode->ddddd84)
        idc.ddddd = bits(instr, 8, 4) >> 4;
    if (decode->dddd74)
        idc.ddddd = 16 + (bits(instr, 7, 4) >> 4);
    if (decode->ddd64)
        idc.ddddd = 16 + (bits(instr, 6, 4) >> 4);

    if (decode->rrrrr9_30)
        idc.rrrrr = (bits(instr, 9, 9) >> 5) | bits(instr, 3, 0);
    if (decode->rrrr30)
        idc.rrrrr = 16 + bits(instr, 3, 0);
    if (decode->rrr20)
        idc.rrrrr = 16 + bits(instr, 2, 0);

    if (decode->KKKK118_30)
        idc.imm_u8 = (bits(instr, 11, 8) >> 4) | bits(instr, 3, 0);

    decode->code(&idc);
    if (avr->halted)
        return false;

    memory[SREG] &= ~idc.clrflags;
    memory[SREG] |= idc.setflags;

    pc += instr_size;
    insns++;
    avr->cycles += AVR_CYCLE_SCALE;

    if (skip_next_instruction)
        goto restart;

    return true;
}

static void maybe_timer0_interrupt(struct arduboy_avr *avr)
{
    uint32_t prescale;
    uint32_t ticks;
    uint8_t cs = memory[AVR_TCCR0B] & 0x07;

    prescale = timer_prescale(cs);
    if (prescale == 0)
        return;

    ticks = avr->timer0_cycles / prescale;
    if (ticks == 0)
        return;

    avr->timer0_cycles -= ticks * prescale;
    while (ticks-- > 0)
    {
        uint8_t old = memory[AVR_TCNT0];

        memory[AVR_TCNT0] = old + 1;
        if (old == 0xff)
        {
            memory[AVR_TIFR0] |= AVR_TOIE0;
            break;
        }
    }

    if ((memory[SREG] & SREG_I) == 0)
        return;
    if ((memory[AVR_TIMSK0] & AVR_TOIE0) == 0)
        return;
    if ((memory[AVR_TIFR0] & AVR_TOIE0) == 0)
        return;
    avr->last_sreg_value = memory[SREG];
    avr->last_tccr0b_value = memory[AVR_TCCR0B];
    avr->last_timsk0_value = memory[AVR_TIMSK0];
    avr->last_tifr0_value = memory[AVR_TIFR0];
    avr->last_tcnt0_value = memory[AVR_TCNT0];
    memory[AVR_TIFR0] &= ~AVR_TOIE0;
    avr->timer0_interrupts++;
    request_interrupt(TIMER0_VECTOR_WORD);
}

static void maybe_timer3_interrupt(struct arduboy_avr *avr)
{
    uint32_t prescale = timer_prescale(memory[AVR_TCCR3B]);
    uint32_t ticks;
    uint16_t tcnt;
    uint16_t ocr3a;
    bool matched = false;

    if (prescale == 0)
        return;

    ticks = avr->timer3_cycles / prescale;
    if (ticks == 0)
        return;

    avr->timer3_cycles -= ticks * prescale;
    tcnt = mem_u16(AVR_TCNT3L);
    ocr3a = mem_u16(AVR_OCR3AL);

    while (ticks-- > 0)
    {
        tcnt++;
        if (ocr3a > 0 && tcnt >= ocr3a)
        {
            matched = true;
            if (memory[AVR_TCCR3B] & AVR_WGM32)
                tcnt = 0;
        }
    }

    write_mem_u16(AVR_TCNT3L, tcnt);
    if (!matched)
        return;

    memory[AVR_TIFR3] |= AVR_OCF3A;
    if (memory[AVR_TCCR3A] & AVR_COM3A0)
        memory[AVR_PORTC] ^= (1 << 6);
    update_speaker_state(avr);

    if ((memory[SREG] & SREG_I) == 0)
        return;
    if ((memory[AVR_TIMSK3] & AVR_OCIE3A) == 0)
        return;

    memory[AVR_TIFR3] &= ~AVR_OCF3A;
    request_interrupt(TIMER3_COMPA_VECTOR_WORD);
}

static bool fast_forward_sleep(struct arduboy_avr *avr)
{
    uint32_t prescale;
    uint32_t ticks;
    uint32_t cycles;

    if ((memory[AVR_SMCR] & AVR_SE) == 0)
        return false;
    if ((memory[SREG] & SREG_I) == 0)
        return false;
    if ((memory[AVR_TIMSK0] & AVR_TOIE0) == 0)
        return false;

    prescale = timer_prescale(memory[AVR_TCCR0B]);
    if (prescale == 0)
        return false;

    ticks = 256U - memory[AVR_TCNT0];
    cycles = ticks * prescale;
    if (cycles <= avr->timer0_cycles)
        cycles = 1;
    else
        cycles -= avr->timer0_cycles;

    avr->cycles += cycles;
    avr->timer0_cycles += cycles;
    avr->timer3_cycles += cycles;
    maybe_timer0_interrupt(avr);
    maybe_timer3_interrupt(avr);
    return true;
}

void arduboy_avr_reset(struct arduboy_avr *avr, const uint8_t *flash_bytes,
                       size_t flash_size)
{
    size_t i;

    active_avr = avr;
    memset(avr, 0, sizeof(*avr));
    memset(avr->eeprom, 0xff, sizeof(avr->eeprom));
    avr->eeprom[1] &= (uint8_t)~0x02;
    memset(memory, 0, sizeof(memory));
    memset(flash, 0xff, sizeof(flash));
    memset(avr_error, 0, sizeof(avr_error));

    for (i = 0; i < AVR_FLASH_WORDS; i++)
    {
        size_t byte = i * 2;
        uint16_t word = 0xffff;

        if (byte < flash_size)
            word = flash_bytes[byte];
        if (byte + 1 < flash_size)
            word |= (uint16_t)flash_bytes[byte + 1] << 8;
        flash[i] = word;
    }

    pc = 0;
    pc_start = 0;
    instr_size = 1;
    skip_next_instruction = false;
    pc22 = false;
    pc_mem_max_64k = true;
    pc_mem_max_256b = false;
    off = false;
    replay_mode = false;
    stepone = false;
    insns = 0;
    insnreplaylim = 0;
    insnlimit = 0;

    setsp(AVR_RAMEND);
    memory[AVR_PORTD] = (1 << OLED_CS_BIT) | (1 << OLED_DC_BIT);
    memory[AVR_SPSR] = AVR_SPIF;
    apply_buttons(avr);
}

void arduboy_avr_set_buttons(struct arduboy_avr *avr, uint8_t buttons)
{
    avr->buttons = buttons;
    apply_buttons(avr);
}

static bool run_core(struct arduboy_avr *avr, uint32_t cycle_budget,
                     uint32_t data_budget, uint32_t insn_budget)
{
    uint32_t start = avr->cycles;
    uint32_t start_data = avr->data_writes;
    uint32_t timer_batch = 0;
    uint32_t ran = 0;
    unsigned int poll_count = 0;
    bool got_display_frame = false;

    active_avr = avr;
    avr->display_dirty = false;

    while (!avr->halted && !off)
    {
        if (cycle_budget > 0 && avr->cycles - start >= cycle_budget)
            break;
        if (data_budget > 0 && avr->data_writes - start_data >= data_budget)
        {
            got_display_frame = true;
            break;
        }
        if (insn_budget > 0 && ran >= insn_budget)
            break;

        if (!step_one(avr))
            break;
        ran++;

        timer_batch += AVR_CYCLE_SCALE;
        if (timer_batch >= 1024)
        {
            avr->timer0_cycles += timer_batch;
            avr->timer3_cycles += timer_batch;
            timer_batch = 0;
            maybe_timer0_interrupt(avr);
            maybe_timer3_interrupt(avr);
        }

        if ((++poll_count & 0x3ff) == 0)
        {
            int held = rb->button_status();

            if ((held & (BUTTON_MENU | BUTTON_SELECT)) ==
                (BUTTON_MENU | BUTTON_SELECT))
            {
                avr->quit_requested = true;
                break;
            }
            rb->yield();
        }
    }

    if (timer_batch > 0 && !avr->halted && !off)
    {
        avr->timer0_cycles += timer_batch;
        avr->timer3_cycles += timer_batch;
        maybe_timer0_interrupt(avr);
        maybe_timer3_interrupt(avr);
    }

    avr->frame_count++;
    return got_display_frame;
}

void arduboy_avr_run_frame(struct arduboy_avr *avr)
{
    run_core(avr, FRAME_CYCLES, 0, 0);
}

bool arduboy_avr_run_display_frame(struct arduboy_avr *avr)
{
    return run_core(avr, 0, DISPLAY_FRAME_BYTES, DISPLAY_FRAME_MAX_INSNS);
}
