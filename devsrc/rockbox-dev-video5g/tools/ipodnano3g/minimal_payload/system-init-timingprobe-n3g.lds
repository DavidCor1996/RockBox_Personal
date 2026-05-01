ENTRY(_start)

OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

MEMORY
{
    IRAM (rwx) : ORIGIN = 0x22000000, LENGTH = 0x10000
}

SECTIONS
{
    .text.entry 0x22000000 : ALIGN(4)
    {
        KEEP(*(.text.entry))
    } > IRAM

    .apple_01f00_025b4 0x22001f00 : ALIGN(4)
    {
        KEEP(*(.apple_01f00_025b4))
    } > IRAM

    .apple_02770_02e8c 0x22002770 : ALIGN(4)
    {
        KEEP(*(.apple_02770_02e8c))
    } > IRAM

    .apple_030d4_03160 0x220030d4 : ALIGN(4)
    {
        KEEP(*(.apple_030d4_03160))
    } > IRAM

    .apple_033f0_03580 0x220033f0 : ALIGN(4)
    {
        KEEP(*(.apple_033f0_03580))
    } > IRAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
