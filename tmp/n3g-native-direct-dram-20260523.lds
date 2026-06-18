ENTRY(_start)
OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

MEMORY
{
    DRAM (rwx) : ORIGIN = 0x09000000, LENGTH = 0x200000
}

SECTIONS
{
    . = ORIGIN(DRAM);
    .text : {
        *(.text*)
    } > DRAM
}
