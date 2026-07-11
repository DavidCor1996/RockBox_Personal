#ifndef ARDUBOY_AVR_EMU_H
#define ARDUBOY_AVR_EMU_H

#include "plugin.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define likely(cond) (cond)
#define unlikely(cond) (cond)
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#ifndef __dead2
#define __dead2
#endif

#ifndef ARRAYLEN
#define ARRAYLEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#define ASSERT(cond, ...) do { if (!(cond)) abort_nodump(); } while (0)

#define memset rb->memset
#define memcpy rb->memcpy
#define printf(...) do { } while (0)

typedef unsigned int uns;

#define REGP_Z 30
#define REGP_Y 28
#define REGP_X 26

#define SREG  0x5f
#define SP_HI 0x5e
#define SP_LO 0x5d
#define EIND  0x5c
#define RAMPZ 0x5b
#define RAMPY 0x5a
#define RAMPX 0x59
#define RAMPD 0x58
#define CCP   0x54

#define AVR_IO_BASE 0x20
#define SREG_IO (SREG - AVR_IO_BASE)
#define EIO_BASE 0x60

#define SREG_C 0x01u
#define SREG_Z 0x02u
#define SREG_N 0x04u
#define SREG_V 0x08u
#define SREG_S 0x10u
#define SREG_H 0x20u
#define SREG_T 0x40u
#define SREG_I 0x80u

#define AVR_FLASH_WORDS 16384
#define AVR_DATA_SIZE   65536

extern uint32_t pc;
extern uint32_t pc_start;
extern uint32_t instr_size;
extern bool skip_next_instruction;
extern uint8_t memory[AVR_DATA_SIZE];
extern uint16_t flash[AVR_FLASH_WORDS];
extern bool pc22;
extern bool pc_mem_max_64k;
extern bool pc_mem_max_256b;
extern bool off;
extern bool replay_mode;
extern bool stepone;
extern uint64_t insns;
extern uint64_t insnreplaylim;
extern uint64_t insnlimit;

void abort_nodump(void);
void _unhandled(const char *f, unsigned l, uint16_t instr);
void _illins(const char *f, unsigned l, uint16_t instr);
uint8_t arduboy_avr_io_read(uint16_t addr);
void arduboy_avr_io_write(uint16_t addr, uint8_t value);

#define unhandled(instr) _unhandled(__FILE__, __LINE__, instr)
#define illins(instr) _illins(__FILE__, __LINE__, instr)

static inline uint16_t bits(uint16_t v, unsigned max, unsigned min)
{
    uint16_t mask = ((unsigned)1 << (max + 1)) - 1;

    if (min > 0)
        mask &= ~(((unsigned)1 << min) - 1);

    return v & mask;
}

static inline uint16_t getsp(void)
{
    return (((uint16_t)memory[SP_HI] << 8) | memory[SP_LO]);
}

static inline void setsp(uint16_t sp)
{
    memory[SP_LO] = sp & 0xff;
    memory[SP_HI] = sp >> 8;
}

static inline uint16_t membyte(uint32_t addr)
{
    return memory[addr & 0xffff];
}

static inline uint16_t memword(uint32_t addr)
{
    addr &= 0xffff;
    return memory[addr] | ((uint16_t)memory[(addr + 1) & 0xffff] << 8);
}

static inline void memwriteword(uint32_t addr, uint16_t word)
{
    addr &= 0xffff;
    memory[addr] = word & 0xff;
    memory[(addr + 1) & 0xffff] = word >> 8;
}

static inline uint16_t romword(uint32_t addr_word)
{
    if (addr_word >= AVR_FLASH_WORDS)
        return 0xffff;

    return flash[addr_word];
}

#endif
