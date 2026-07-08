.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,             0x22000ff0
.equ MARK_ADDR,             0x22000f00

.equ LCD_BASE,              0x38300000
.equ LCD_CONFIG,            (LCD_BASE + 0x00)
.equ LCD_CMD,               (LCD_BASE + 0x04)
.equ LCD_STATUS,            (LCD_BASE + 0x1c)
.equ LCD_PHTIME,            (LCD_BASE + 0x20)
.equ LCD_WDATA,             (LCD_BASE + 0x40)
.equ LCD_UNKNOWN_7C,        (LCD_BASE + 0x7c)
.equ LCD_UNKNOWN_88,        (LCD_BASE + 0x88)

.equ GPIO_BASE,             0x3cf00000
.equ GPIO_BANK6_DATA,       (GPIO_BASE + 0xc4)

.equ STREAM_COLOR,          0x0000ffff
.equ WAIT_LIMIT,            0x00100000

.macro mark imm
    ldr r10, =MARK_ADDR
    ldr r11, =\imm
    str r11, [r10]
.endm

.section .text
.global _start
.type _start, %function

/* Bounded LCD_WDATA probe for Nano 3G.
 *
 * Writes stage markers to MARK_ADDR and returns to the caller so the host can
 * read progress via pokecall. It uses the same direct LCD register path as the
 * streaming payload, but every LCD ready wait has a timeout.
 */
_start:
    ldr sp, =STACK_TOP
    mark 0x4c303030 /* L000 */

    bl lcd_preamble
    mark 0x4c303031 /* L001 */

    bl lcd_enter_command_mode
    cmp r0, #0
    bne done
    mark 0x4c303032 /* L002 */

    mov r0, #0x11
    bl lcd_send_cmd8
    cmp r0, #0
    bne done
    mark 0x4c303033 /* L003 */

    ldr r0, =60000
    bl delay_ticks
    mark 0x4c303034 /* L004 */

    mov r0, #0x13
    bl lcd_send_cmd8
    cmp r0, #0
    bne done
    mark 0x4c303035 /* L005 */

    mov r0, #0x29
    bl lcd_send_cmd8
    cmp r0, #0
    bne done
    mark 0x4c303036 /* L006 */

    mov r0, #0x2c
    bl lcd_send_cmd8
    cmp r0, #0
    bne done
    mark 0x4c303037 /* L007 */

    ldr r4, =LCD_WDATA
    ldr r5, =STREAM_COLOR
    mov r6, #1024
1:
    bl lcd_wait_ready
    cmp r0, #0
    bne done
    str r5, [r4]
    subs r6, r6, #1
    bne 1b

    mark 0x4c444f4e /* LDON */
    mov r0, #0

done:
    bx lr

lcd_preamble:
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
    bx lr

lcd_enter_command_mode:
    bl lcd_wait_ready
    cmp r0, #0
    bxne lr

    ldr r3, =GPIO_BANK6_DATA
    ldr r2, [r3]
    mov r0, r2, lsr #4
    and r0, r0, #1
    mov r1, r2, lsr #5
    and r1, r1, #1
    orr r0, r0, r1, lsl #1

    ldr r2, =LCD_CONFIG
    ldr r3, [r2]
    ldr r1, =0x80000007
    and r3, r3, r1
    cmp r0, #2
    ldrlt r1, =0x00000c20
    ldrge r1, =0x00000da8
    orr r3, r3, r1
    str r3, [r2]
    mov r0, #0
    bx lr

lcd_send_cmd8:
    mov r3, r0
    bl lcd_wait_ready
    cmp r0, #0
    bxne lr
    ldr r1, =LCD_CMD
    str r3, [r1]
    mov r0, #0
    bx lr

lcd_wait_ready:
    ldr r1, =LCD_STATUS
    ldr r2, =WAIT_LIMIT
2:
    ldr r0, [r1]
    tst r0, #2
    movne r0, #0
    bxne lr
    subs r2, r2, #1
    bne 2b
    mark 0x4c455252 /* LERR */
    mov r0, #1
    bx lr

delay_ticks:
    ldr r2, =0x00040000
3:
    subs r2, r2, #1
    bne 3b
    bx lr

.size _start, . - _start
