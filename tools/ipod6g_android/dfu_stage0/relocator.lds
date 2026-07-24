/* SPDX-License-Identifier: GPL-2.0-only */
ENTRY(_relocator_start)
OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

SECTIONS
{
    . = 0x2203c000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment*) *(.ARM.attributes*) *(.note*) }
}

ASSERT(SIZEOF(.bss) == 0, "relocator may not require BSS")
ASSERT(. <= 0x2203d800, "relocator exceeds reserved execution window")

