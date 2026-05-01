ENTRY(_start)

OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

MEMORY
{
    IRAM (rwx) : ORIGIN = 0x22000000, LENGTH = 0x1000
}

SECTIONS
{
    . = ORIGIN(IRAM);

    .text : ALIGN(4)
    {
        KEEP(*(.text))
        *(.text*)
        *(.rodata*)
    } > IRAM

    .data : ALIGN(4)
    {
        *(.data*)
    } > IRAM

    .bss (NOLOAD) : ALIGN(4)
    {
        *(.bss*)
        *(COMMON)
    } > IRAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
