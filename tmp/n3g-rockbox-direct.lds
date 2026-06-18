ENTRY(_start)
OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

MEMORY
{
    IRAM (rwx) : ORIGIN = 0x22000000, LENGTH = 0x40000
}

SECTIONS
{
    . = ORIGIN(IRAM);
    .text : {
        *(.text*)
    } > IRAM

    rockbox_end = .;
}
