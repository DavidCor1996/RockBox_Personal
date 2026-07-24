/* SPDX-License-Identifier: GPL-2.0-only */
ENTRY(_start)
OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

SECTIONS
{
    /* wInd3x CopyHeaderBody places the unsigned IMG1 body at IRAM0. */
    . = 0x22000000;
    .entry : { KEEP(*(.text.entry)) }
    . = ALIGN(4);
    __relocated_load_start = .;

    /* Rockbox's proven S5L8702 bootloader execution address. */
    . = 0x22020800;
    .relocated : AT(__relocated_load_start)
    {
        __relocated_start = .;
        *(.text.start)
        *(.text*)
        *(.rodata*)
        *(.data*)
        . = ALIGN(4);
        __relocated_end = .;
    }
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment*) *(.ARM.attributes*) *(.note*) }
}

ASSERT(SIZEOF(.bss) == 0, "stage zero may not require BSS")
ASSERT(LOADADDR(.relocated) + SIZEOF(.relocated) <= 0x2201c000,
       "stage zero source enters Boot ROM live workspace")
ASSERT(__relocated_end <= 0x22039800,
       "relocated stage zero leaves less than 6 KiB of stack")
