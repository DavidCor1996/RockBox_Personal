.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,  0x22000ff0
.equ MARK_ADDR,  0x22000f00
.equ LCD_STATUS, 0x3830001c

.section .text
.global _start
.type _start, %function

/* Single MMIO read probe: read LCD_STATUS once, store marker/value, return. */
_start:
    ldr sp, =STACK_TOP
    ldr r0, =MARK_ADDR
    ldr r1, =0x4c535244 /* LSRD */
    str r1, [r0]
    ldr r2, =LCD_STATUS
    ldr r3, [r2]
    str r3, [r0, #4]
    ldr r1, =0x4c534f4b /* LSOK */
    str r1, [r0]
    bx lr

.size _start, . - _start
