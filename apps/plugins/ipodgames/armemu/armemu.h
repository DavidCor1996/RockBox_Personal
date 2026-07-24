#ifndef _armemu_h_
#define _armemu_h_

#include "plugin.h"

/*
 * CPU execution core adapted from iPodLinux armemu (GPL-2.0). The original
 * project is archived at https://github.com/iPodLinux/armemu. Rockbox-specific
 * memory and framework handling live outside this directory.
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long ulong;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

#define  R_usr(x)  (x)
#define  FP        11
#define  IP        12
#define  SP        13
#define  LR        14
#define  PC        15
#define  R_fiq(x)  (((x) >= 8) && ((x) < 15)? 16+((x)-8) : (x))
#define  R8_fiq    16
#define  R9_fiq    17
#define  R10_fiq   18
#define  R11_fiq   19
#define  R12_fiq   20
#define  R13_fiq   21
#define  R14_fiq   22
#define  R_svc(x)  (((x) == 13) || ((x) == 14)? 23+((x)-13) : (x))
#define  R13_svc   23
#define  R14_svc   24
#define  R_abt(x)  (((x) == 13) || ((x) == 14)? 25+((x)-13) : (x))
#define  R13_abt   25
#define  R14_abt   26
#define  R_irq(x)  (((x) == 13) || ((x) == 14)? 27+((x)-13) : (x))
#define  R13_irq   27
#define  R14_irq   28
#define  R_und(x)  (((x) == 13) || ((x) == 14)? 29+((x)-13) : (x))
#define  R13_und   29
#define  R14_und   30

#define  CPSR      31
#define  SPSR_fiq  32
#define  SPSR_svc  33
#define  SPSR_abt  34
#define  SPSR_irq  35
#define  SPSR_und  36

#define  CPSR_control   0xff
#define  CPSR_mode      0x1f
#define  CPSR_mode_usr  0x10
#define  CPSR_mode_fiq  0x11
#define  CPSR_mode_irq  0x12
#define  CPSR_mode_svc  0x13
#define  CPSR_mode_abt  0x17
#define  CPSR_mode_und  0x1b
#define  CPSR_mode_sys  0x1f
#define  CPSR_T         0x20
#define  CPSR_F         0x40
#define  CPSR_I         0x80
#define  CPSR_flags     0xf0000000
#define  CPSR_V         0x10000000
#define  CPSR_C         0x20000000
#define  CPSR_Z         0x40000000
#define  CPSR_N         0x80000000
#define  CPSR_Vbits     28
#define  CPSR_Cbits     29
#define  CPSR_Zbits     30
#define  CPSR_Nbits     31

#define  IVEC_reset     0x00
#define  IVEC_und       0x04
#define  IVEC_swi       0x08
#define  IVEC_pabt      0x0C
#define  IVEC_dabt      0x10
#define  IVEC_irq       0x18
#define  IVEC_fiq       0x1C

typedef struct cpu
{
    u32 r[37];
} cpu_t;

typedef struct machine
{
    cpu_t *cpu;

    int iramsize;
    u32 irambase;
    void *iram;
    int dramsize;
    u32 drambase;
    void *dram;
    int verbose;
    int stopped;
    int fault;
    u32 fault_address;
    u32 fault_instruction;
    unsigned long instructions;
#ifdef SIMULATOR
    unsigned long profile_condition[16];
    unsigned long profile_top[8];
    unsigned long profile_alu[16];
    unsigned long profile_alu_shape[8];
    unsigned long profile_load_store[8];
#endif
    bool (*swi)(struct machine *mach, cpu_t *cpu, u32 immediate);
} machine_t;

void fatal (const char *err);

/* Magic to get the current SPSR */
#define SPSR -1
int reg_banked(cpu_t *cpu, int r);

/* Retail click-wheel games execute almost entirely in ARM user mode.  Keep
 * the common register lookup in the instruction decoder and fall back to the
 * banked-mode implementation only for exceptions/SPSR access. */
static inline __attribute__((always_inline))
int reg(cpu_t *cpu, int number)
{
    if (number >= 0 &&
        (cpu->r[CPSR] & CPSR_mode) == CPSR_mode_usr)
        return number;
    return reg_banked(cpu, number);
}

void fill_pipeline (cpu_t *cpu);
void reset (cpu_t *cpu);
u32 mapaddr (machine_t *mach, u32 vaddr);
int is_mmio (machine_t *mach, u32 paddr);
void *resolve_addr (machine_t *mach, u32 vaddr);
void interrupt (cpu_t *cpu, int vec, int mode);
u32 ldwi(machine_t *mach, cpu_t *cpu, u32 vaddr);
u32 ldw (machine_t *mach, cpu_t *cpu, u32 vaddr);
u16 ldh (machine_t *mach, cpu_t *cpu, u32 vaddr);
u8  ldb (machine_t *mach, cpu_t *cpu, u32 vaddr);
void stw (machine_t *mach, cpu_t *cpu, u32 vaddr, u32 value);
void sth (machine_t *mach, cpu_t *cpu, u32 vaddr, u16 value);
void stb (machine_t *mach, cpu_t *cpu, u32 vaddr, u8  value);
static inline int cond(cpu_t *cpu, int code)
{
    bool z = (cpu->r[CPSR] & CPSR_Z) != 0;
    bool c = (cpu->r[CPSR] & CPSR_C) != 0;
    bool n = (cpu->r[CPSR] & CPSR_N) != 0;
    bool v = (cpu->r[CPSR] & CPSR_V) != 0;

    switch (code & 15)
    {
        case 0: return z;
        case 1: return !z;
        case 2: return c;
        case 3: return !c;
        case 4: return n;
        case 5: return !n;
        case 6: return v;
        case 7: return !v;
        case 8: return c && !z;
        case 9: return !c || z;
        case 10: return n == v;
        case 11: return n != v;
        case 12: return !z && n == v;
        case 13: return z || n != v;
        case 14: return true;
        default: return false;
    }
}
u32 barrel_shift (cpu_t *cpu, u32 shdesc, int *cflagp);
#ifdef SIMULATOR
void execute(machine_t *mach, cpu_t *cpu);
#endif
void execute_until(machine_t *mach, cpu_t *cpu, u32 stop_address,
                   unsigned long stop_instruction) ICODE_ATTR;
#endif
