.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,      0x22000ff0
.equ MARK_ADDR,      0x22000f00
.equ LCD_CONFIG,     0x38300000
.equ LCD_CMD,        0x38300004
.equ LCD_WDATA,      0x38300040

/* Preamble plus command sequence plus one data write, no wait loops. */
.section .text
.global _start
.type _start, %function

_start:
    ldr sp, =STACK_TOP
    ldr r1, =MARK_ADDR
    ldr r0, =0x4c443030 /* LD00 */
    str r0, [r1]

    ldr r2, =LCD_CONFIG
    ldr r0, =0x80000000
    str r0, [r2]
    ldr r0, =0x80100db1
    str r0, [r2]
    ldr r0, =0x01000000
    str r0, [r2, #0x88]
    mov r0, #0x33
    str r0, [r2, #0x20]
    ldr r0, =0x00000804
    str r0, [r2, #0x7c]

    ldr r2, =LCD_CMD
    mov r0, #0x11
    str r0, [r2]
    mov r0, #0x13
    str r0, [r2]
    mov r0, #0x29
    str r0, [r2]
    mov r0, #0x2c
    str r0, [r2]

    ldr r2, =LCD_WDATA
    ldr r0, =0x0000ffff
    str r0, [r2]

    ldr r0, =0x4c444f4b /* LDOK */
    str r0, [r1]
    bx lr

.size _start, . - _start
