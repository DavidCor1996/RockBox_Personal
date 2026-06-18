.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,      0x22000ff0
.equ MARK_ADDR,      0x22000f00
.equ LCD_CONFIG,     0x38300000
.equ LCD_CMD,        0x38300004
.equ LCD_STATUS,     0x3830001c
.equ LCD_WDATA,      0x38300040
.equ WAIT_LIMIT,     0x00100000

.macro mark imm
    ldr r10, =MARK_ADDR
    ldr r11, =\imm
    str r11, [r10]
.endm

.section .text
.global _start
.type _start, %function

/* Single-chunk bounded LCD probe.
 *
 * Host readback from MARK_ADDR:
 *   LT00 start
 *   LT01 LCD config written
 *   LT02 command sequence written
 *   LDON pixel writes completed
 *   LERR LCD ready timeout
 */
_start:
    ldr sp, =STACK_TOP
    mark 0x4c543030 /* LT00 */

    ldr r1, =LCD_CONFIG
    ldr r0, =0x80000000
    str r0, [r1]
    ldr r0, =0x80100db1
    str r0, [r1]
    ldr r0, =0x01000000
    str r0, [r1, #0x88]
    mov r0, #0x33
    str r0, [r1, #0x20]
    ldr r0, =0x00000804
    str r0, [r1, #0x7c]
    mark 0x4c543031 /* LT01 */

    mov r0, #0x11
    bl send_cmd
    cmp r0, #0
    bne done
    mov r0, #0x13
    bl send_cmd
    cmp r0, #0
    bne done
    mov r0, #0x29
    bl send_cmd
    cmp r0, #0
    bne done
    mov r0, #0x2c
    bl send_cmd
    cmp r0, #0
    bne done
    mark 0x4c543032 /* LT02 */

    ldr r4, =LCD_WDATA
    ldr r5, =0x0000ffff
    mov r6, #64
1:
    bl wait_ready
    cmp r0, #0
    bne done
    str r5, [r4]
    subs r6, r6, #1
    bne 1b
    mark 0x4c444f4e /* LDON */

done:
    bx lr

send_cmd:
    mov r3, r0
    bl wait_ready
    cmp r0, #0
    bxne lr
    ldr r1, =LCD_CMD
    str r3, [r1]
    mov r0, #0
    bx lr

wait_ready:
    ldr r1, =LCD_STATUS
    ldr r2, =WAIT_LIMIT
2:
    ldr r0, [r1]
    tst r0, #0x10
    moveq r0, #0
    bxeq lr
    subs r2, r2, #1
    bne 2b
    mark 0x4c455252 /* LERR */
    mov r0, #1
    bx lr

.size _start, . - _start
