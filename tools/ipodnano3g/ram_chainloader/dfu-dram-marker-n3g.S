.syntax unified
.cpu arm926ej-s
.arm

.equ MARKER_ADDR, 0x08100000
.equ WDT_BASE,    0x3c800000

.section .text
.global _start
.type _start, %function

_start:
    msr cpsr_c, #0xd3

    ldr r0, =MARKER_ADDR
    ldr r1, =0x33475244 /* "DRG3" in little endian memory */
    str r1, [r0], #4
    ldr r1, =0x53524550 /* "PERS" */
    str r1, [r0], #4
    ldr r1, =0x53545349 /* "ISTS" */
    str r1, [r0], #4
    ldr r1, =0x214b4f3f /* "?OK!" */
    str r1, [r0]

    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4

    ldr r0, =0x00100000
    ldr r1, =WDT_BASE
    str r0, [r1]

1:
    b 1b

.ltorg
.size _start, . - _start
