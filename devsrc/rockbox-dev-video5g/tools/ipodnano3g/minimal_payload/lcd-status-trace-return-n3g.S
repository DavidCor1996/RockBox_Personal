.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,      0x22000ff0
.equ MARK_ADDR,      0x22000f00
.equ LCD_CONFIG,     0x38300000
.equ LCD_CMD,        0x38300004
.equ LCD_STATUS,     0x3830001c
.equ LCD_WDATA,      0x38300040

.macro write_marker imm
    ldr r10, =MARK_ADDR
    ldr r11, =\imm
    str r11, [r10]
.endm

.section .text
.global _start
.type _start, %function

_start:
    ldr sp, =STACK_TOP
    write_marker 0x4c533030 /* LS00 */

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

    ldr r4, =MARK_ADDR
    ldr r5, =LCD_STATUS
    ldr r0, [r5]
    str r0, [r4, #4]

    ldr r2, =LCD_CMD
    mov r0, #0x11
    str r0, [r2]
    mov r0, #0x13
    str r0, [r2]
    mov r0, #0x29
    str r0, [r2]
    mov r0, #0x2c
    str r0, [r2]

    ldr r0, [r5]
    str r0, [r4, #8]

    ldr r2, =LCD_WDATA
    ldr r0, =0x0000ffff
    str r0, [r2]
    ldr r0, [r5]
    str r0, [r4, #12]

    ldr r0, =0x0000ffff
    mov r3, #63
1:
    str r0, [r2]
    subs r3, r3, #1
    bne 1b

    ldr r0, [r5]
    str r0, [r4, #16]

    write_marker 0x4c534f4b /* LSOK */
    bx lr

.size _start, . - _start
