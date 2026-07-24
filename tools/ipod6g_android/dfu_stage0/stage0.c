/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal Classic 6G/7G DRAM bootstrap.  It has no storage, USB, filesystem,
 * flash, partition, PMU, or charger code.  The register sequence is the
 * Rockbox ipod6g miu_preinit(false) sequence used by the qualified U-Boot
 * board port.
 */

#include <stddef.h>
#include <stdint.h>

#define CLK_BASE              0x3c500000u
#define CLKCON0               (CLK_BASE + 0x00u)
#define CG16_RTIME_WORD       (CLK_BASE + 0x10u)
#define PLLMODE               (CLK_BASE + 0x44u)
#define PWRCON0               (CLK_BASE + 0x48u)

#define MIU_BASE              0x38100000u
#define MIU_REG(offset)       (MIU_BASE + (offset))
#define MIUCON                MIU_REG(0x00u)
#define MIUCOM                MIU_REG(0x04u)
#define MIUAREF               MIU_REG(0x08u)
#define MIUMRS                MIU_REG(0x0cu)
#define MIUSDPARA             MIU_REG(0x10u)

#define DRAM_STAGING          0x08010000u
#define RELOCATOR_ADDRESS     0x2203c000u
#define WDTCON                0x3c800000u
#define SWRCON                0x3c500050u

extern const unsigned char relocator_blob_start[];
extern const unsigned char relocator_blob_end[];
extern const unsigned char compressed_blob_start[];
extern const unsigned char compressed_blob_end[];

static inline uint32_t read32(uintptr_t address)
{
    return *(volatile uint32_t *)address;
}

static inline void write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static void __attribute__((noreturn, unused)) reset_to_installed_boot(void)
{
    write32(SWRCON, 0x000000a5u);
    write32(WDTCON, 0x00100000u);
    for (;;)
        ;
}

static void copy_bytes(unsigned char *destination,
                       const unsigned char *source, size_t length)
{
    while (length-- != 0)
        *destination++ = *source++;
}

static void n25_dram_cold_init(void)
{
    uint32_t value;
    unsigned int index;

    write32(CLKCON0, read32(CLKCON0) & ~(1u << 31));
    write32(PLLMODE, read32(PLLMODE) & ~(1u << 8));
    value = read32(CG16_RTIME_WORD) & 0x0000ffffu;
    write32(CG16_RTIME_WORD, value);
    write32(PWRCON0, read32(PWRCON0) & ~((1u << 3) | (1u << 4)));

    write32(MIUCON, 0x0000080du);
    write32(MIU_REG(0xf0), 0);
    write32(MIUAREF, 0x0006105du);
    write32(MIUSDPARA, 0x001fb621u);
    write32(MIU_REG(0x200), 0x1845);
    write32(MIU_REG(0x204), 0x1845);
    write32(MIU_REG(0x210), 0x1800);
    write32(MIU_REG(0x214), 0x1800);
    write32(MIU_REG(0x220), 0x1845);
    write32(MIU_REG(0x224), 0x1845);
    write32(MIU_REG(0x230), 0x1885);
    write32(MIU_REG(0x234), 0x1885);
    write32(MIU_REG(0x14), 0x19);
    write32(MIU_REG(0x18), 0x19);
    write32(MIU_REG(0x1c), 0x0790682bu);
    write32(MIU_REG(0x314), read32(MIU_REG(0x314)) & ~0x10u);

    for (index = 0; index < 0x24; index++)
    {
        uintptr_t address = MIU_REG(0x2c + index * 4);
        write32(address, read32(address) & ~(1u << 24));
    }

    write32(MIU_REG(0x1cc), 0x540);
    write32(MIU_REG(0x1d4), read32(MIU_REG(0x1d4)) | 0x80u);

    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x233);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x333);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x333);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);

    write32(MIUMRS, 0x33);
    write32(MIUCOM, 0x133);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUMRS, 0x8040);
    write32(MIUCOM, 0x133);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUCOM, 0x33);
    write32(MIUAREF, read32(MIUAREF) | 0x61000u);
}

static int n25_dram_probe(void)
{
    static const uintptr_t addresses[] = {
        0x08000000u, 0x09fffffcu, 0x0a000000u, 0x0bfffffcu,
    };
    static const uint32_t patterns[] = {
        0x55aa00ffu, 0xaa55ff00u, 0x0ff033ccu, 0xf00fcc33u,
    };
    unsigned int index;

    for (index = 0; index < 4; index++)
        write32(addresses[index], patterns[index]);
    for (index = 0; index < 4; index++)
    {
        if (read32(addresses[index]) != patterns[index])
            return -1;
    }
    for (index = 0; index < 4; index++)
        write32(addresses[index], 0);
    return 0;
}

void __attribute__((noreturn)) stage0_main(void)
{
#ifdef N25_STAGE0_CHECKPOINT_ENTRY
    reset_to_installed_boot();
#else
    size_t compressed_size;
    size_t relocator_size;
    void (*relocator)(void);

    n25_dram_cold_init();
    if (n25_dram_probe() != 0)
        for (;;)
            ;
#ifdef N25_STAGE0_CHECKPOINT_DRAM
    reset_to_installed_boot();
#endif

    compressed_size = (size_t)(compressed_blob_end - compressed_blob_start);
    relocator_size = (size_t)(relocator_blob_end - relocator_blob_start);
    copy_bytes((unsigned char *)DRAM_STAGING, compressed_blob_start,
               compressed_size);
    copy_bytes((unsigned char *)RELOCATOR_ADDRESS, relocator_blob_start,
               relocator_size);

    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c10, 4\n"
        "mcr p15, 0, r0, c7, c5, 0\n"
        "mcr p15, 0, r0, c7, c5, 4\n"
        : : : "r0", "memory");

    relocator = (void (*)(void))RELOCATOR_ADDRESS;
    relocator();
    for (;;)
        ;
#endif
}
