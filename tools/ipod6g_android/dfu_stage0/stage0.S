/* SPDX-License-Identifier: GPL-2.0-only */
/* S5L8702 DFU stage zero, entered at the Nano3/Classic IMG1 body address. */

    .syntax unified
    .arch armv5te
    .arm

    .section .text.entry, "ax", %progbits
    .global _start
    .type _start, %function
_start:
    /*
     * Match the physically proven Rockbox N25 DFU contract: CopyHeaderBody
     * stages the flat image at IRAM0, then this position-independent stub
     * relocates the executable body to IRAM1 + the 0x800 IMG1 reservation.
     * Cache shutdown and all C code execute only from the relocated copy.
     */
    ldr     r0, =__relocated_load_start
    ldr     r1, =__relocated_start
    ldr     r2, =__relocated_end
1:
    cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     1b
    ldr     pc, =relocated_start
    .ltorg
    .size _start, . - _start

    .section .text.start, "ax", %progbits
    .global relocated_start
    .type relocated_start, %function
relocated_start:
    msr     cpsr_c, #0xd3

    /* Match Rockbox's proven ARM926 cache shutdown before touching DRAM. */
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #0x1000
    bic     r0, r0, #0x5
    mcr     p15, 0, r0, c1, c0, 0
2:
    mrc     p15, 0, r15, c7, c10, 3
    bne     2b
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 0
    mcr     p15, 0, r0, c7, c5, 4

    ldr     sp, =0x2203b000
    bl      stage0_main
3:
    b       3b
    .size relocated_start, . - relocated_start
