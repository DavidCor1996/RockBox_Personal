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
