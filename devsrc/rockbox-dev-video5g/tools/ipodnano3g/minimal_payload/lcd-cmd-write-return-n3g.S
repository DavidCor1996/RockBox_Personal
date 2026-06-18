.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP, 0x22000ff0
.equ MARK_ADDR, 0x22000f00
.equ LCD_CMD,   0x38300004

/* Single MMIO write probe: write LCD_CMD once, store marker, return. */
.section .text
.global _start
.type _start, %function

_start:
    ldr sp, =STACK_TOP
    ldr r0, =0x4c435354 /* LCST */
    ldr r1, =MARK_ADDR
    str r0, [r1]

    mov r0, #0x11
    ldr r2, =LCD_CMD
    str r0, [r2]

    ldr r0, =0x4c434f4b /* LCOK */
    str r0, [r1]
    bx lr

.size _start, . - _start
