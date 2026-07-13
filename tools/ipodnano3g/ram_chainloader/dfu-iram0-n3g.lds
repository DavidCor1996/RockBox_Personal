ENTRY(_start)
OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

MEMORY
{
    IRAM0 (rwx) : ORIGIN = 0x22000000, LENGTH = 0x00020000
}

SECTIONS
{
    . = ORIGIN(IRAM0);
    .text : ALIGN(4)
    {
        KEEP(*(.text))
        *(.text*)
        *(.rodata*)
    } > IRAM0

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
