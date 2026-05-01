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

    .text.stub_02f04 0x22002f04 : ALIGN(4)
    {
        KEEP(*(.text.stub_02f04))
    } > IRAM

    .apple_00664 0x22000664 : ALIGN(4)
    {
        KEEP(*(.apple_00664))
    } > IRAM

    .apple_00828 0x22000828 : ALIGN(4)
    {
        KEEP(*(.apple_00828))
    } > IRAM

    .apple_03b18 0x22003b18 : ALIGN(4)
    {
        KEEP(*(.apple_03b18))
    } > IRAM

    .apple_data_07000 0x22007000 : ALIGN(4)
    {
        KEEP(*(.apple_data_07000))
    } > IRAM

    .apple_zero_07830 0x22007830 : ALIGN(4)
    {
        KEEP(*(.apple_zero_07830))
    } > IRAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
