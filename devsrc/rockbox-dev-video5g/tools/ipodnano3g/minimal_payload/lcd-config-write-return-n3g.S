.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,  0x22000ff0
.equ MARK_ADDR,  0x22000f00
.equ LCD_CONFIG, 0x38300000

.section .text
.global _start
.type _start, %function

/* Single MMIO write probe: write LCD_CONFIG once, store marker, return. */
_start:
    ldr sp, =STACK_TOP
    ldr r0, =MARK_ADDR
    ldr r1, =0x4c575254 /* LWRT */
    str r1, [r0]
    ldr r2, =LCD_CONFIG
    ldr r3, =0x80000000
    str r3, [r2]
    ldr r1, =0x4c574f4b /* LWOK */
    str r1, [r0]
    bx lr

.size _start, . - _start
