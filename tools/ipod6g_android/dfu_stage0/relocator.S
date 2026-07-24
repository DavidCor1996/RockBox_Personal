/* SPDX-License-Identifier: GPL-2.0-only */

    .syntax unified
    .arch armv5te
    .arm

    .section .text.start, "ax", %progbits
    .global _relocator_start
    .type _relocator_start, %function
_relocator_start:
    msr     cpsr_c, #0xd3
    ldr     sp, =0x08080000
    bl      relocator_main
1:
    b       1b
    .size _relocator_start, . - _relocator_start

