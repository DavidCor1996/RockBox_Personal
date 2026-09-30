#include "armemu.h"

/* The original standalone emulator had instruction-level stdio tracing.
 * Register and condition lookups are header-inlined for this hot decoder. */
#define printf(...) do { } while (0)
#define dprintf(...) do { } while (0)

static inline __attribute__((always_inline))
u8 execute_ldb(machine_t *mach, cpu_t *cpu, u32 address)
{
    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset < (u32)mach->dramsize)
            return ((u8 *)mach->dram)[offset];
    }
    return ldb(mach, cpu, address);
}

static inline __attribute__((always_inline))
u16 execute_ldh(machine_t *mach, cpu_t *cpu, u32 address)
{
    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            2 <= (u32)mach->dramsize - offset)
            return load_le16_aligned((u8 *)mach->dram + offset);
    }
    return ldh(mach, cpu, address);
}

static inline __attribute__((always_inline))
u32 execute_ldw(machine_t *mach, cpu_t *cpu, u32 address)
{
    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            4 <= (u32)mach->dramsize - offset)
            return load_le32_aligned((u8 *)mach->dram + offset);
    }
    return ldw(mach, cpu, address);
}

static inline __attribute__((always_inline))
u32 execute_ldwi(machine_t *mach, cpu_t *cpu, u32 address)
{
    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            4 <= (u32)mach->dramsize - offset)
            return load_le32_aligned((u8 *)mach->dram + offset);
    }
    return ldwi(mach, cpu, address);
}

static inline __attribute__((always_inline))
void execute_stb(machine_t *mach, cpu_t *cpu, u32 address, u8 value)
{
    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset < (u32)mach->dramsize)
        {
            ((u8 *)mach->dram)[offset] = value;
            return;
        }
    }
    stb(mach, cpu, address, value);
}

static inline __attribute__((always_inline))
void execute_sth(machine_t *mach, cpu_t *cpu, u32 address, u16 value)
{
    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            2 <= (u32)mach->dramsize - offset)
        {
            store_le16_aligned((u8 *)mach->dram + offset, value);
            return;
        }
    }
    sth(mach, cpu, address, value);
}

static inline __attribute__((always_inline))
void execute_stw(machine_t *mach, cpu_t *cpu, u32 address, u32 value)
{
    if (address >= mach->drambase)
    {
        u32 offset = address - mach->drambase;

        if (offset <= (u32)mach->dramsize &&
            4 <= (u32)mach->dramsize - offset)
        {
            store_le32_aligned((u8 *)mach->dram + offset, value);
            return;
        }
    }
    stw(mach, cpu, address, value);
}

static inline __attribute__((always_inline))
int execute_reg(cpu_t *cpu, int number)
{
    /* Decrypted eApps remain in ARM user mode while the host handles their
     * semihosting calls. Avoid re-reading and testing CPSR for every source
     * and destination register in this hot interpreter. SPSR still uses the
     * banked lookup for the exception-only fallback path. */
    if (number >= 0)
        return number;
    return reg_banked(cpu, number);
}

static inline __attribute__((always_inline))
u32 execute_barrel_shift(cpu_t *cpu, u32 descriptor, int *carry_out)
{
    int rm = descriptor & 15;
    bool by_register = (descriptor & (1 << 4)) != 0;
    int type = (descriptor >> 5) & 3;
    unsigned int shift;
    u32 value = cpu->r[execute_reg(cpu, rm)];
    int ignored_carry;

    if (!carry_out)
        carry_out = &ignored_carry;
    *carry_out = -1;
    shift = by_register ?
        (cpu->r[execute_reg(cpu, (descriptor >> 8) & 15)] & 0xff) :
        ((descriptor >> 7) & 31);
    if (by_register && shift == 0)
        return value;

    switch (type)
    {
        case 0:
            if (shift == 0)
                return value;
            *carry_out = shift <= 32 ? (value >> (32 - shift)) & 1 : 0;
            return shift < 32 ? value << shift : 0;
        case 1:
            if (!by_register && shift == 0)
                shift = 32;
            *carry_out = shift <= 32 ? (value >> (shift - 1)) & 1 : 0;
            return shift < 32 ? value >> shift : 0;
        case 2:
            if (!by_register && shift == 0)
                shift = 32;
            if (shift >= 32)
            {
                *carry_out = value >> 31;
                return (value & 0x80000000) ? 0xffffffff : 0;
            }
            *carry_out = (value >> (shift - 1)) & 1;
            return (u32)((s32)value >> shift);
        default:
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

#define ldb execute_ldb
#define ldh execute_ldh
#define ldw execute_ldw
#define ldwi execute_ldwi
#define stb execute_stb
#define sth execute_sth
#define stw execute_stw
#define barrel_shift execute_barrel_shift
#define reg execute_reg

static inline __attribute__((always_inline))
bool execute_one(machine_t *mach, cpu_t *cpu)
{
    u32 instr;
    u32 mode;
    int PCup = 0, PCmod = 0;

    u32 address = cpu->r[PC] - 8;
    u32 offset = address - mach->drambase;

    /* Loaded eApp code lives in the contiguous guest DRAM window. Avoid the
     * generic MMIO/page-abort path and its CPSR-mode check for every normal
     * instruction fetch. */
    if (address >= mach->drambase &&
        offset <= (u32)mach->dramsize &&
        4 <= (u32)mach->dramsize - offset)
        instr = load_le32_aligned((u8 *)mach->dram + offset);
    else
    {
        mode = cpu->r[CPSR] & CPSR_mode;
        instr = ldwi(mach, cpu, address);
        if ((cpu->r[CPSR] & CPSR_mode) != mode) /* caused a pabt */
            return false;
    }

#ifdef SIMULATOR
    ++mach->profile_condition[instr >> 28];
    ++mach->profile_top[(instr >> 25) & 7];
#endif

    /* Most retail eApp instructions are unconditional.  Avoid decoding all
     * four CPSR flags and entering the condition switch for ARM's AL case;
     * this is on the hottest path in the interpreter. */
    {
        unsigned int condition = instr >> 28;

        if (condition == 0xe) /* AL */
        {
            /* Vortex executes nearly four fifths of its instructions with
             * this condition.  Keep its path to the encoding decoder to one
             * comparison. */
        }
        else if (condition == 0) /* EQ */
        {
            if (!(cpu->r[CPSR] & CPSR_Z))
                goto skip;
        }
        else if (condition == 1) /* NE */
        {
            if (cpu->r[CPSR] & CPSR_Z)
                goto skip;
        }
        else if (!cond(cpu, condition))
            goto skip; /* instr not to be executed */
    }

    /* Route the three large, unambiguous ARM encoding classes directly to
     * their handlers.  Retail eApps spend roughly two fifths of execution
     * in loads/stores, branches and block transfers; making those walk the
     * rare BX/SWI/status/multiply recognizers wastes several host branches
     * for every guest instruction. */
    switch ((instr >> 25) & 7)
    {
        case 0:
            /* Multiply, swap and halfword transfers share bits 7/4; BX and
             * status-register operations occupy the no-S test/compare
             * encoding.  Ordinary ALU instructions can reject both groups
             * with two tests instead of walking every rare exact pattern. */
            if ((instr & 0x0e000090) != 0x00000090 &&
                (instr & 0x01900000) != 0x01000000)
                goto alu_instruction;
            break;
        case 1:
            /* The only status-register immediate encoding in this class is
             * MSR_f. Everything else follows the common ALU path. */
            if ((instr & 0x0dbff000) != 0x0128f000)
                goto alu_instruction;
            break;
        case 2:
        case 3:
            goto load_store_instruction;
        case 4:
            goto load_store_multiple_instruction;
        case 5:
            goto branch_instruction;
        default:
            break;
    }

    // BX
    if ((instr & 0x0ffffff0) == 0x012fff10) {
        cpu->r[PC] = cpu->r[reg (cpu, instr & 0xf)];
        if (cpu->r[PC] & 1) { // Thumb
            cpu->r[CPSR] |= CPSR_T;
            cpu->r[PC]   &= ~1;
        } else {
            cpu->r[CPSR] &= ~CPSR_T;
        }
        PCup = 1;
        if (mach->verbose >= 3) printf ("\t\tPC = %08x\n", cpu->r[PC]);
    }

    // SWI
    else if ((instr & 0x0f000000) == 0x0f000000) {
        if (!mach->swi || !mach->swi(mach, cpu, instr & 0x00ffffff)) {
            interrupt (cpu, IVEC_swi, CPSR_mode_svc);
            PCmod = 1;
        }
    }

    // B, BL
    else if ((instr & 0x0e000000) == 0x0a000000) {
branch_instruction:
        if (instr & (1 << 24)) // L
            cpu->r[reg (cpu, LR)] = cpu->r[PC] - 4;

        u32 off = (instr & 0x00ffffff) << 2;
        if (off & (1 << 25)) off |= 0xfc000000; // sign-extend
        s32 soff = (s32)off;
        
        cpu->r[PC] += soff;
        PCup = 1;
        if (mach->verbose >= 3) printf ("\t\tPC = %08x\n", cpu->r[PC]);
    }

    // MSR, MRS
    else if (((instr & 0x0fbf0fff) == 0x010f0000) || // MRS
             ((instr & 0x0fb1fff0) == 0x0121f000) || // MSR_cxsf
             ((instr & 0x0db1f000) == 0x0120f000)) { // MSR_f
        if ((instr & 0x0fbf0fff) == 0x010f0000) { // MRS
            int src;
            if (instr & (1 << 22))
                src = SPSR;
            else
                src = CPSR;
            cpu->r[reg (cpu, (instr >> 12) & 0xf)] = cpu->r[reg (cpu, src)];
            if (mach->verbose >= 3) printf ("\t\tr%d = %08x\n", (instr >> 12) & 0xf, cpu->r[reg (cpu, (instr >> 12) & 0xf)]);
        } else if ((instr & 0x0fbffff0) == 0x0129f000) { // MSR_cxsf
            int dest;
            if (instr & (1 << 22))
                dest = SPSR;
            else
                dest = CPSR;

            u32 val = cpu->r[reg (cpu, instr & 0xf)];
            
            if (!(cpu->r[CPSR] & 0xf)) { // not privileged
                val &= 0xf0000000;
                cpu->r[reg (cpu, dest)] = (cpu->r[reg (cpu, dest)] & ~0xf0000000) | val;
            } else {
                cpu->r[reg (cpu, dest)] = val;
            }
            if (mach->verbose >= 3) printf ("\t\tCPSR = %08x\n", cpu->r[reg (cpu, dest)]);
        } else if ((instr & 0x0dbff000) == 0x0128f000) { // MSR_f
            int dest;
            if (instr & (1 << 22))
                dest = SPSR;
            else
                dest = CPSR;
            
            u32 val;
            if (instr & (1 << 25)) {
                int ror = (instr & 0xf00) >> 7; // >>8 *2
                val = instr & 0xff;
                if (ror)
                    val = (val >> ror) | (val << (32 - ror));
            } else {
                val = cpu->r[reg (cpu, instr & 0xf)];
            }
            val &= 0xf0000000;
            cpu->r[reg (cpu, dest)] = (cpu->r[reg (cpu, dest)] & ~0xf0000000) | val;
            if (mach->verbose >= 3) printf ("\t\tCPSR = %08x\n", cpu->r[reg (cpu, dest)]);
        }
    }

    // MUL, MLA
    else if ((instr & 0x0fc000f0) == 0x00000090) {
        int regD   = (instr >> 16) & 0xf;
        int regN   = (instr >> 12) & 0xf;
        int regS   = (instr >>  8) & 0xf;
        int regM   = (instr >>  0) & 0xf;
        u32 valN   = cpu->r[reg (cpu, regN)];
        u32 valS   = cpu->r[reg (cpu, regS)];
        u32 valM   = cpu->r[reg (cpu, regM)];
        u32 result = 0;

        if (instr & (1 << 21)) { // MLA
            result = valM * valS + valN;
        } else { // MUL
            result = valM * valS;
        }

        if (instr & (1 << 20)) { // S
            cpu->r[CPSR] = (cpu->r[CPSR] & ~(CPSR_N | CPSR_Z)) |
                (result & CPSR_N) | (result == 0 ? CPSR_Z : 0);
        }

        cpu->r[reg (cpu, regD)] = result;
        if (regD == PC)
            PCup = 1;
        if (mach->verbose >= 3) printf ("\t\tr%d = %08x * %08x + %08x = %08x\n", regD, valM, valS, ((instr & (1 << 21))? valN : 0),
                cpu->r[reg (cpu, regD)]);
    }

    // UMULL, UMLAL, SMULL, SMLAL
    else if ((instr & 0x0f8000f0) == 0x00800090) {
        int regDhi = (instr >> 16) & 0xf;
        int regDlo = (instr >> 12) & 0xf;
        int regS   = (instr >>  8) & 0xf;
        int regM   = (instr >>  0) & 0xf;
        u32 valDhi = cpu->r[reg (cpu, regDhi)];
        u32 valDlo = cpu->r[reg (cpu, regDlo)];
        u32 valS   = cpu->r[reg (cpu, regS)];
        u32 valM   = cpu->r[reg (cpu, regM)];
        u64 result = 0;

        if (instr & (1 << 21)) // UMLAL/SMLAL
            result = ((u64)valDhi << 32) | valDlo;
        
        if (instr & (1 << 22)) { // signed
            /* Sign-extend each 32-bit operand before multiplying. Keep
             * accumulation unsigned so overflow wraps modulo 2^64. */
            result += (u64)((s64)(s32)valS * (s64)(s32)valM);
        } else {
            result += (u64)valS * (u64)valM;
        }

        if (instr & (1 << 20)) { // S
            /* ARMv5 preserves C/V; N describes bit 63, not bit 31. */
            cpu->r[CPSR] = (cpu->r[CPSR] & ~(CPSR_N | CPSR_Z)) |
                ((u32)(result >> 32) & CPSR_N) |
                (result == 0 ? CPSR_Z : 0);
        }

        cpu->r[reg (cpu, regDhi)] = (result >> 32) & 0xffffffff;
        cpu->r[reg (cpu, regDlo)] = result & 0xffffffff;
        if (regDhi == PC || regDlo == PC)
            PCup = 1;
    }

    // LDR, STR, LDRB, STRB
    else if ((instr & 0x0c000000) == 0x04000000) {
load_store_instruction:
        ;
        int regN   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        u32 offset = 0;
        int IPUBWL = (instr >> 20) & 0x3f;

#ifdef SIMULATOR
        ++mach->profile_load_store[
            ((IPUBWL & 1) ? 1 : 0) |
            ((IPUBWL & 4) ? 2 : 0) |
            ((IPUBWL & 32) ? 4 : 0)];
#endif

        if (IPUBWL & (1 << 5)) { // I -> not an immediate
            offset = barrel_shift (cpu, instr & 0xfff, 0);
        } else {
            offset = instr & 0xfff;
        }

        u32 base   = cpu->r[reg (cpu, regN)];
        u32 taddr;
        u32 postinc;

        if (IPUBWL & (1 << 4)) { // P
            if (IPUBWL & (1 << 3)) // U
                taddr = base + offset;
            else
                taddr = base - offset;
            postinc = 0;
        } else {
            taddr = base;
            postinc = offset;
        }

        if (IPUBWL & (1 << 0)) { // ldr
            u32 memory_address = (IPUBWL & (1 << 2)) ? taddr : taddr & ~3;
            u32 memory_offset = memory_address - mach->drambase;
            unsigned int memory_size = (IPUBWL & (1 << 2)) ? 1 : 4;

            if (memory_address >= mach->drambase &&
                memory_offset <= (u32)mach->dramsize &&
                memory_size <= (u32)mach->dramsize - memory_offset)
            {
                if (memory_size == 1)
                    cpu->r[reg (cpu, regD)] =
                        ((u8 *)mach->dram)[memory_offset];
                else
                    cpu->r[reg (cpu, regD)] = load_le32_aligned(
                        (u8 *)mach->dram + memory_offset);
            }
            else
            {
                mode = cpu->r[CPSR] & CPSR_mode;
                if (memory_size == 1)
                    cpu->r[reg (cpu, regD)] = ldb (mach, cpu, taddr);
                else
                    cpu->r[reg (cpu, regD)] =
                        ldw (mach, cpu, memory_address);
                if ((cpu->r[CPSR] & CPSR_mode) != mode)
                    PCmod = 1;
            }

            if (regD == PC)
                PCup = 1;
            if (mach->verbose >= 3) printf ("\t\tr%d = [%08x] = %08x\n", regD, taddr, cpu->r[reg (cpu, regD)]);
        } else { // str
            u32 memory_address = (IPUBWL & (1 << 2)) ? taddr : taddr & ~3;
            u32 memory_offset = memory_address - mach->drambase;
            unsigned int memory_size = (IPUBWL & (1 << 2)) ? 1 : 4;

            if (memory_address >= mach->drambase &&
                memory_offset <= (u32)mach->dramsize &&
                memory_size <= (u32)mach->dramsize - memory_offset)
            {
                if (memory_size == 1)
                    ((u8 *)mach->dram)[memory_offset] =
                        cpu->r[reg (cpu, regD)] & 0xff;
                else
                    store_le32_aligned((u8 *)mach->dram + memory_offset,
                                       cpu->r[reg (cpu, regD)]);
            }
            else
            {
                mode = cpu->r[CPSR] & CPSR_mode;
                if (memory_size == 1)
                    stb (mach, cpu, taddr,
                         cpu->r[reg (cpu, regD)] & 0xff);
                else
                    stw (mach, cpu, memory_address,
                         cpu->r[reg (cpu, regD)]);
                if ((cpu->r[CPSR] & CPSR_mode) != mode)
                    PCmod = 1;
            }
            if (mach->verbose >= 3) printf ("\t\t[%08x] = r%d = %08x\n", taddr, regD, cpu->r[reg (cpu, regD)]);
        }

        if ((IPUBWL & 0x12) != 0x10) { // write back
            if (IPUBWL & (1 << 4)) { // P (re-increment)
                cpu->r[reg (cpu, regN)] = taddr;
            } else {
                if (IPUBWL & (1 << 3)) { // U (p)
                    cpu->r[reg (cpu, regN)] = taddr + postinc;
                } else {
                    cpu->r[reg (cpu, regN)] = taddr - postinc;
                }
            }
            if (mach->verbose >= 3) printf ("\t\tr%d = %08x\n", regN, cpu->r[reg (cpu, regN)]);
        }
    }

    // SWP
    else if ((instr & 0x0fb00ff0) == 0x01000090) {
        int regN   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        int regM   =  instr        & 0xf;
        int byte   =  instr        & (1 << 22);
        
        mode = cpu->r[CPSR] & CPSR_mode;

        if (byte) {
            cpu->r[reg (cpu, regD)] = ldb (mach, cpu, cpu->r[reg (cpu, regN)]);
            stb (mach, cpu, cpu->r[reg (cpu, regN)], cpu->r[reg (cpu, regM)]);
        } else {
            cpu->r[reg (cpu, regM)] = ldw (mach, cpu, cpu->r[reg (cpu, regN)] & ~3);
            stw (mach, cpu, cpu->r[reg (cpu, regN)] & ~3, cpu->r[reg (cpu, regM)]);
        }

        if (regD == PC)
            PCup = 1;
        if (mode != (cpu->r[CPSR] & CPSR_mode))
            PCmod = 1;
    }

    // LDRH, STRH, LDRSB, LDRSH
    else if ((instr & 0x0e000090) == 0x00000090) {
        int regN   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        u32 offset = 0;
        int PUIWL  = (instr >> 20) & 0x1f;
        int SH     = (instr >> 5) & 3;
        
        if (PUIWL & (1 << 2)) { // I
            offset = ((instr >> 4) & 0xf0) | (instr & 0xf);
        } else {
            offset = cpu->r[reg (cpu, instr & 0xf)];
        }

        u32 base   = cpu->r[reg (cpu, regN)];
        u32 taddr, postinc;

        if (PUIWL & (1 << 4)) { // P
            if (PUIWL & (1 << 3)) // U
                taddr = base + offset;
            else
                taddr = base - offset;
            postinc = 0;
        } else {
            taddr = base;
            postinc = offset;
        }

        mode = cpu->r[CPSR] & CPSR_mode;

        switch (SH) {
        case 3: // LDRSH
        case 1: // <LD|ST>RH
            if (PUIWL & (1 << 0)) { // ldrh
                cpu->r[reg (cpu, regD)] = ldh (mach, cpu, taddr & ~1);

                if ((SH & 2) && (cpu->r[reg (cpu, regD)] & 0x8000))
                    cpu->r[reg (cpu, regD)] |= 0xffff0000;
                
                if (regD == PC) PCup = 1;
            } else {
                sth (mach, cpu, taddr & ~1, cpu->r[reg (cpu, regD)]);
            }
            break;

        case 2: // LDRSB
            /* Byte loads are never word-aligned.  The imported iPodLinux
             * core rounded this address down, so every four entries in a
             * signed-byte table read the same value. */
            cpu->r[reg (cpu, regD)] = ldb (mach, cpu, taddr);
            
            if (cpu->r[reg (cpu, regD)] & 0x80)
                cpu->r[reg (cpu, regD)] |= 0xffffff00;

            if (regD == PC) PCup = 1;
            break;
        }

        if ((cpu->r[CPSR] & CPSR_mode) != mode) // caused a fault
            PCmod = 1;

        if ((PUIWL & 0x12) != 0x10) { // write back
            if (PUIWL & (1 << 4)) { // P (re-increment)
                cpu->r[reg (cpu, regN)] = taddr;
            } else {
                if (PUIWL & (1 << 3)) { // U (p)
                    cpu->r[reg (cpu, regN)] = taddr + postinc;
                } else {
                    cpu->r[reg (cpu, regN)] = taddr - postinc;
                }
            }
        }
    }

    // LDM, STM
    else if ((instr & 0x0e000000) == 0x08000000) {
load_store_multiple_instruction:
        ;
        int regN   = (instr >> 16) & 0xf;
        int PUSWL  = (instr >> 20) & 0x1f;
        int reglist = instr & 0xffff;
        u32 base   = cpu->r[reg (cpu, regN)];
        u32 taddr;
        int nregs = 0;
        int r;

        for (r = 0; r < 16; r++) {
            if (reglist & (1 << r))
                nregs++;
        }
        
        // Here, `base' is updated to what will be written back
        // if W is set, and `taddr' is the actual base address
        // for the transfer.

        switch (PUSWL >> 3) {
        case 0: // LDMFA, LDMDA, STMED, STMDA - post-decrement
            base -= (nregs << 2);
            taddr = (base + 4) & ~3;
            break;
        case 1: // LDMFD, LDMIA, STMEA, STMIA - post-increment
            taddr = base & ~3;
            base += (nregs << 2);
            break;
        case 2: // LDMEA, LDMDB, STMFD, STMDB - pre-decrement
            base -= (nregs << 2);
            taddr = base & ~3;
            break;
        case 3: // LDMED, LDMIB, STMFA, STMIB - pre-increment
            taddr = (base + 4) & ~3;
            base += (nregs << 2);
            break;
        }

        mode = cpu->r[CPSR] & CPSR_mode;

        int transferring_PC = (reglist & (1 << 15));
        for (r = 0; r < 16; r++) {
            if (reglist & (1 << r)) {
                if (PUSWL & 1) { // load
                    if ((PUSWL & (1 << 2)) && !transferring_PC) // user bank transfer
                        cpu->r[r] = ldw (mach, cpu, taddr);
                    else {
                        cpu->r[reg (cpu, r)] = ldw (mach, cpu, taddr);
                        dprintf ("\t\tr%d = [%08x] = %08x\n", r, taddr, cpu->r[reg (cpu, r)]);
                        if ((PUSWL & (1 << 2)) && (r == PC)) {
                            cpu->r[CPSR] = cpu->r[reg (cpu, SPSR)];
                            dprintf ("\t\tCPSR = %08x\n", cpu->r[CPSR]);
                        }
                    }
                } else { // store
                    if (r == PC) // PC is stored as this instr's addr + 12
                        stw (mach, cpu, taddr, cpu->r[PC] + 4);
                    else if (PUSWL & (1 << 2)) // user bank transfer
                        stw (mach, cpu, taddr, cpu->r[r]);
                    else
                        stw (mach, cpu, taddr, cpu->r[reg (cpu, r)]);
                    if (mach->verbose >= 3) printf ("\t\t[%08x] = r%d = %08x\n", taddr, r, cpu->r[reg (cpu, r)]);
                }

                if (r == PC)
                    PCup = 1;
                
                taddr += 4;
            }
        }

        if ((cpu->r[CPSR] & CPSR_mode) != mode)
            PCmod = 1;

        if (PUSWL & (1 << 1)) { // (W)rite-back
            cpu->r[reg (cpu, regN)] = base;
            if (mach->verbose >= 3) printf ("\t\tr%d = %08x\n", regN, base);
        }
    }

    // ALU instrs
    else if ((instr & 0x0c000000) == 0) {
alu_instruction:
        ;
        int is_imm = !!(instr & (1 << 25));
        int setflg = !!(instr & (1 << 20));
        int opcode = (instr >> 21) & 0xf;
        int regA   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        u32 opB    = 0;
        int LCflag = -1;
        int ACflag = -1;
        int Zflag  = -1;
        int Vflag  = -1;
        int Nflag  = -1;
        u32 result = 0;

#ifdef SIMULATOR
        ++mach->profile_alu[opcode];
        ++mach->profile_alu_shape[
            (is_imm ? 1 : 0) |
            (setflg ? 2 : 0) |
            ((!is_imm && (instr & 0xff0)) ? 4 : 0)];
#endif

        if (!setflg && (opcode < 8 || opcode >= 12))
        {
            u32 fast_op_b;
            u32 fast_op_a = cpu->r[reg (cpu, regA)];

            if (is_imm)
            {
                u32 immediate = instr & 0xff;
                unsigned int rotate = (instr & 0xf00) >> 7;

                fast_op_b = rotate ?
                    (immediate >> rotate) |
                    (immediate << (32 - rotate)) : immediate;
            }
            else
                fast_op_b = barrel_shift(cpu, instr & 0xfff, NULL);

            switch (opcode)
            {
                case 0: result = fast_op_a & fast_op_b; break;
                case 1: result = fast_op_a ^ fast_op_b; break;
                case 2: result = fast_op_a - fast_op_b; break;
                case 3: result = fast_op_b - fast_op_a; break;
                case 4: result = fast_op_a + fast_op_b; break;
                case 5:
                    result = fast_op_a + fast_op_b +
                        !!(cpu->r[CPSR] & CPSR_C);
                    break;
                case 6:
                    result = fast_op_a - fast_op_b -
                        !(cpu->r[CPSR] & CPSR_C);
                    break;
                case 7:
                    result = fast_op_b - fast_op_a -
                        !(cpu->r[CPSR] & CPSR_C);
                    break;
                case 12: result = fast_op_a | fast_op_b; break;
                case 13: result = fast_op_b; break;
                case 14: result = fast_op_a & ~fast_op_b; break;
                default: result = ~fast_op_b; break;
            }
            cpu->r[reg (cpu, regD)] = result;
            if (regD == PC)
                PCup = 1;
            goto skip;
        }
        if (setflg && opcode == 10 && regD != PC)
        {
            u32 compare_op_b;
            u32 compare_op_a = cpu->r[reg (cpu, regA)];
            u32 compare_result;
            u32 compare_flags;

            if (is_imm)
            {
                u32 immediate = instr & 0xff;
                unsigned int rotate = (instr & 0xf00) >> 7;

                compare_op_b = rotate ?
                    (immediate >> rotate) |
                    (immediate << (32 - rotate)) : immediate;
            }
            else
                compare_op_b = barrel_shift(cpu, instr & 0xfff, NULL);
            compare_result = compare_op_a - compare_op_b;
            compare_flags = compare_result & CPSR_N;
            if (!compare_result)
                compare_flags |= CPSR_Z;
            if (compare_op_a >= compare_op_b)
                compare_flags |= CPSR_C;
            if ((compare_op_a ^ compare_op_b) &
                (compare_op_a ^ compare_result) & CPSR_N)
                compare_flags |= CPSR_V;
            cpu->r[CPSR] =
                (cpu->r[CPSR] & ~CPSR_flags) | compare_flags;
            goto skip;
        }
        
        if (is_imm) {
            u32 imm = (instr & 0xff);
            int ror = (instr & 0xf00) >> 7; // >>8 *2
            opB = ror ? (imm >> ror) | (imm << (32 - ror)) : imm;
            /* ARM data-processing immediates update C from bit 31 of the
             * rotated operand. With a zero rotation C is preserved. This is
             * observable in the eApp's optimized strcmp: a following RRX
             * depends on TST's immediate-shifter carry. */
            if (ror)
                LCflag = (opB >> 31) & 1;
        } else {
            opB = barrel_shift (cpu, instr & 0xfff, &LCflag);
        }

        u32 opA = cpu->r[reg (cpu, regA)];
        int writeres = 1;
        int arith = 0;

        // do the op
        switch (opcode) {
        case 8: /* TST */
            writeres = 0;
        case 0: /* AND */
            result = opA & opB;
            break;
            
        case 9: /* TEQ */
            writeres = 0;
        case 1: /* EOR */
            result = opA ^ opB;
            break;
            
        case 0xC: /* ORR */
            result = opA | opB;
            break;

        case 0xA: /* CMP */
            writeres = 0;
        case 6: /* SBC */
        case 2: /* SUB */
        {
            u32 borrow = opcode == 6 && !(cpu->r[CPSR] & CPSR_C);
            result = opA - opB - borrow; arith = 1;
            ACflag = opA > opB || (opA == opB && !borrow);
            Vflag  = !!((opA ^ opB) & (opA ^ result) & 0x80000000);
            break;
        }
        case 7: /* RSC */
        case 3: /* RSB */
        {
            u32 borrow = opcode == 7 && !(cpu->r[CPSR] & CPSR_C);
            result = opB - opA - borrow; arith = 1;
            ACflag = opB > opA || (opB == opA && !borrow);
            Vflag  = !!((opB ^ opA) & (opB ^ result) & 0x80000000);
            break;
        }

        case 0xB: /* CMN */
            writeres = 0;
        case 5: /* ADC */
        case 4: /* ADD */
        {
            u32 carry = opcode == 5 && (cpu->r[CPSR] & CPSR_C);
            result = opA + opB + carry; arith = 1;
            ACflag = result < opA || (carry && result == opA);
            Vflag = !!(~(opA ^ opB) & (opA ^ result) & 0x80000000);
            break;
        }

        case 0xD: /* MOV */
            result = opB;
            break;
        case 0xE: /* BIC */
            result = opA & ~opB;
            break;
        case 0xF: /* MVN */
            result = ~opB;
            break;
        }

        // update flags
        if (setflg) {
            if (regD == PC) {
                cpu->r[CPSR] = cpu->r[reg (cpu, SPSR)];
            } else {
                int Cflag = (arith? ACflag : LCflag);
                int flgmask;
                
                Nflag = !!(result & 0x80000000);
                Zflag = !result;
                
                flgmask = (((Cflag != -1) << CPSR_Cbits) |
                           ((Vflag != -1) << CPSR_Vbits) |
                           ((u32)(Zflag != -1) << CPSR_Zbits) |
                           ((u32)(Nflag != -1) << CPSR_Nbits));
                
                cpu->r[CPSR] = ((cpu->r[CPSR] & ~flgmask) |
                                ((((Cflag & 1) << CPSR_Cbits) |
                                  ((Vflag & 1) << CPSR_Vbits) |
                                  ((u32)(Zflag & 1) << CPSR_Zbits) |
                                  ((u32)(Nflag & 1) << CPSR_Nbits)) & flgmask));
            }
        }
        
        // write result
        if (writeres) {
            cpu->r[reg (cpu, regD)] = result;
            if (regD == PC)
                PCup = 1;
        }
    }
    
    // undefined instr
    else {
        mach->fault = 1;
        mach->fault_address = cpu->r[PC] - 8;
        mach->fault_instruction = cpu->r[PC] - 8;
        mach->stopped = 1;
        return false;
    }

 skip:
    if (PCup)
        fill_pipeline (cpu);
    else if (!PCmod)
        cpu->r[PC] += 4;

    return true;
}

#ifdef SIMULATOR
void execute(machine_t *mach, cpu_t *cpu)
{
    if (execute_one(mach, cpu))
        ++mach->instructions;
}
#endif

void execute_until(machine_t *mach, cpu_t *cpu, u32 stop_address,
                   unsigned long stop_instruction)
{
    unsigned long instructions = mach->instructions;

    while (!mach->stopped && instructions < stop_instruction &&
           cpu->r[PC] - 8 < stop_address)
    {
        if (!execute_one(mach, cpu))
            break;
        ++instructions;
    }
    mach->instructions = instructions;
}
