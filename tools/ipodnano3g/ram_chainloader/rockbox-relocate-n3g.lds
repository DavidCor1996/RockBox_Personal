ENTRY(_start)
OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

MEMORY
{
    DRAM (rwx) : ORIGIN = 0x08000800, LENGTH = 0x01000000
}

SECTIONS
{
    . = ORIGIN(DRAM);
    .text : ALIGN(4)
    {
        KEEP(*(.text))
        *(.text*)
        *(.rodata*)
    } > DRAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
