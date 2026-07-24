#include "armemu.h"

void fatal(const char *error)
{
    (void)error;
}

int reg_banked(cpu_t *cpu, int number)
{
    if (number < 0)
    {
        switch (cpu->r[CPSR] & CPSR_mode)
        {
            case CPSR_mode_fiq: return SPSR_fiq;
            case CPSR_mode_irq: return SPSR_irq;
            case CPSR_mode_svc: return SPSR_svc;
            case CPSR_mode_abt: return SPSR_abt;
            case CPSR_mode_und: return SPSR_und;
            default: return CPSR;
        }
    }

    switch (cpu->r[CPSR] & CPSR_mode)
    {
        case CPSR_mode_fiq: return R_fiq(number);
        case CPSR_mode_irq: return R_irq(number);
        case CPSR_mode_svc: return R_svc(number);
        case CPSR_mode_abt: return R_abt(number);
        case CPSR_mode_und: return R_und(number);
        default: return R_usr(number);
    }
}

void fill_pipeline(cpu_t *cpu)
{
    cpu->r[PC] += 8;
}

void reset(cpu_t *cpu)
{
    rb->memset(cpu, 0, sizeof(*cpu));
    cpu->r[CPSR] = CPSR_mode_usr;
}

u32 mapaddr(machine_t *mach, u32 address)
{
    (void)mach;
    return address;
}

int is_mmio(machine_t *mach, u32 address)
{
    (void)mach;
    (void)address;
    return 0;
}

void *resolve_addr(machine_t *mach, u32 address)
{
    if (address >= mach->drambase &&
        address - mach->drambase < (u32)mach->dramsize)
        return (u8 *)mach->dram + (address - mach->drambase);
    if (address >= mach->irambase &&
        address - mach->irambase < (u32)mach->iramsize)
        return (u8 *)mach->iram + (address - mach->irambase);
    return NULL;
}

void interrupt(cpu_t *cpu, int vector, int mode)
{
    u32 saved = cpu->r[CPSR];

    cpu->r[CPSR] = (saved & ~(CPSR_mode | CPSR_T)) | mode;
    switch (vector)
    {
        case IVEC_reset:
            reset(cpu);
            break;
        case IVEC_und:
            cpu->r[SPSR_und] = saved;
            cpu->r[R14_und] = cpu->r[PC] - 4;
            break;
        case IVEC_swi:
            cpu->r[SPSR_svc] = saved;
            cpu->r[R14_svc] = cpu->r[PC] - 4;
            break;
        case IVEC_pabt:
            cpu->r[SPSR_abt] = saved;
            cpu->r[R14_abt] = cpu->r[PC] - 4;
            break;
        case IVEC_dabt:
            cpu->r[SPSR_abt] = saved;
            cpu->r[R14_abt] = cpu->r[PC];
            break;
        case IVEC_irq:
            cpu->r[SPSR_irq] = saved;
            cpu->r[R14_irq] = cpu->r[PC] - 4;
            cpu->r[CPSR] |= CPSR_I;
            break;
        case IVEC_fiq:
            cpu->r[SPSR_fiq] = saved;
            cpu->r[R14_fiq] = cpu->r[PC] - 4;
            cpu->r[CPSR] |= CPSR_F;
            break;
        default:
            return;
    }
    cpu->r[PC] = (u32)vector;
    fill_pipeline(cpu);
}

static bool address_range(machine_t *mach, u32 address, u32 size,
                          unsigned char **pointer)
{
    u32 offset;

    if (size == 0 || address > 0xffffffffu - (size - 1))
        return false;
    if (address >= mach->drambase)
    {
        offset = address - mach->drambase;
        if (offset <= (u32)mach->dramsize &&
            size <= (u32)mach->dramsize - offset)
        {
            *pointer = (unsigned char *)mach->dram + offset;
            return true;
        }
    }
    if (address >= mach->irambase)
    {
        offset = address - mach->irambase;
        if (offset <= (u32)mach->iramsize &&
            size <= (u32)mach->iramsize - offset)
        {
            *pointer = (unsigned char *)mach->iram + offset;
            return true;
        }
    }
    return false;
}

static void memory_fault(machine_t *mach, cpu_t *cpu, u32 address)
{
    mach->fault = 2;
    mach->fault_address = address;
    mach->fault_instruction = cpu->r[PC] - 8;
    mach->stopped = 1;
}

u8 ldb(machine_t *mach, cpu_t *cpu, u32 address)
{
    unsigned char *pointer;

    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset < (u32)mach->dramsize)
            return ((unsigned char *)mach->dram)[offset];
    }
    if (!address_range(mach, address, 1, &pointer))
    {
        memory_fault(mach, cpu, address);
        return 0;
    }
    return pointer[0];
}

u16 ldh(machine_t *mach, cpu_t *cpu, u32 address)
{
    unsigned char *pointer;

    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            2 <= (u32)mach->dramsize - offset)
            return load_le16_aligned((unsigned char *)mach->dram + offset);
    }
    if (!address_range(mach, address, 2, &pointer))
    {
        memory_fault(mach, cpu, address);
        return 0;
    }
    return load_le16_aligned(pointer);
}

u32 ldw(machine_t *mach, cpu_t *cpu, u32 address)
{
    unsigned char *pointer;

    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            4 <= (u32)mach->dramsize - offset)
            return load_le32_aligned((unsigned char *)mach->dram + offset);
    }
    if (!address_range(mach, address, 4, &pointer))
    {
        memory_fault(mach, cpu, address);
        return 0;
    }
    return load_le32_aligned(pointer);
}

u32 ldwi(machine_t *mach, cpu_t *cpu, u32 address)
{
    u32 offset;
    unsigned char *pointer;

    if (address >= mach->drambase)
    {
        offset = address - mach->drambase;
        if (offset <= (u32)mach->dramsize &&
            4 <= (u32)mach->dramsize - offset)
        {
            pointer = (unsigned char *)mach->dram + offset;
            return load_le32_aligned(pointer);
        }
    }
    return ldw(mach, cpu, address);
}

void stb(machine_t *mach, cpu_t *cpu, u32 address, u8 value)
{
    unsigned char *pointer;

    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset < (u32)mach->dramsize)
        {
            ((unsigned char *)mach->dram)[offset] = value;
            return;
        }
    }
    if (!address_range(mach, address, 1, &pointer))
    {
        memory_fault(mach, cpu, address);
        return;
    }
    pointer[0] = value;
}

void sth(machine_t *mach, cpu_t *cpu, u32 address, u16 value)
{
    unsigned char *pointer;

    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            2 <= (u32)mach->dramsize - offset)
        {
            store_le16_aligned((unsigned char *)mach->dram + offset, value);
            return;
        }
    }
    if (!address_range(mach, address, 2, &pointer))
    {
        memory_fault(mach, cpu, address);
        return;
    }
    store_le16_aligned(pointer, value);
}

void stw(machine_t *mach, cpu_t *cpu, u32 address, u32 value)
{
    unsigned char *pointer;

    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            4 <= (u32)mach->dramsize - offset)
        {
            store_le32_aligned((unsigned char *)mach->dram + offset, value);
            return;
        }
    }
    if (!address_range(mach, address, 4, &pointer))
    {
        memory_fault(mach, cpu, address);
        return;
    }
    store_le32_aligned(pointer, value);
}

u32 barrel_shift(cpu_t *cpu, u32 descriptor, int *carry_out)
{
    int rm = descriptor & 15;
    bool by_register = (descriptor & (1 << 4)) != 0;
    int type = (descriptor >> 5) & 3;
    unsigned int shift;
    u32 value = cpu->r[reg(cpu, rm)];
    int ignored_carry;

    if (!carry_out)
        carry_out = &ignored_carry;
    *carry_out = -1;
    shift = by_register ?
        (cpu->r[reg(cpu, (descriptor >> 8) & 15)] & 0xff) :
        ((descriptor >> 7) & 31);
    if (by_register && shift == 0)
        return value;

    switch (type)
    {
        case 0: /* LSL */
            if (shift == 0)
                return value;
            *carry_out = shift <= 32 ? (value >> (32 - shift)) & 1 : 0;
            return shift < 32 ? value << shift : 0;
        case 1: /* LSR */
            if (!by_register && shift == 0)
                shift = 32;
            *carry_out = shift <= 32 ? (value >> (shift - 1)) & 1 : 0;
            return shift < 32 ? value >> shift : 0;
        case 2: /* ASR */
            if (!by_register && shift == 0)
                shift = 32;
            if (shift >= 32)
            {
                *carry_out = value >> 31;
                return (value & 0x80000000) ? 0xffffffff : 0;
            }
            *carry_out = (value >> (shift - 1)) & 1;
            return (u32)((s32)value >> shift);
        default: /* ROR/RRX */
            if (!by_register && shift == 0)
            {
                *carry_out = value & 1;
                return (value >> 1) |
                       ((cpu->r[CPSR] & CPSR_C) ? 0x80000000 : 0);
            }
            shift &= 31;
            if (shift == 0)
            {
                *carry_out = value >> 31;
                return value;
            }
            *carry_out = (value >> (shift - 1)) & 1;
            return (value >> shift) | (value << (32 - shift));
    }
}
