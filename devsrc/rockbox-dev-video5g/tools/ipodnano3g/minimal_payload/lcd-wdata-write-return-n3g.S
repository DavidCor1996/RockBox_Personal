.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP, 0x22000ff0
.equ MARK_ADDR, 0x22000f00
.equ LCD_WDATA, 0x38300040

/* Single MMIO write probe: write LCD_WDATA once, store marker, return. */
.section .text
.global _start
.type _start, %function

_start:
    ldr sp, =STACK_TOP
    ldr r0, =0x4c445354 /* LDST */
    ldr r1, =MARK_ADDR
    str r0, [r1]

    ldr r0, =0x0000ffff
    ldr r2, =LCD_WDATA
    str r0, [r2]

    ldr r0, =0x4c444f4b /* LDOK */
    str r0, [r1]
    bx lr

.size _start, . - _start
