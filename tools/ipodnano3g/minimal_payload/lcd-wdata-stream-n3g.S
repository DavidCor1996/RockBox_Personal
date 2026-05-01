.syntax unified
.cpu arm926ej-s
.arm

.equ LCD_BASE,             0x38300000
.equ LCD_CONFIG,           (LCD_BASE + 0x00)
.equ LCD_CMD,              (LCD_BASE + 0x04)
.equ LCD_STATUS,           (LCD_BASE + 0x1c)
.equ LCD_PHTIME,           (LCD_BASE + 0x20)
.equ LCD_WDATA,            (LCD_BASE + 0x40)
.equ LCD_UNKNOWN_7C,       (LCD_BASE + 0x7c)
.equ LCD_UNKNOWN_88,       (LCD_BASE + 0x88)

.equ GPIO_BASE,            0x3cf00000
.equ GPIO_BANK6_DATA,      (GPIO_BASE + 0xc4)

.equ TIMER_COUNTER,        0x3c7000b4
.equ STREAM_COLOR,         0x0000ffff

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Nano 3G LCD_IF CPU-stream payload.
 *
 * Purpose:
 *   - use the confirmed LCD_IF command path
 *   - enter RAM-write mode with command 0x2c
 *   - stream a constant color directly into LCD_WDATA from the CPU
 *   - avoid DMA entirely
 *
 * Note:
 *   - LCD_WDATA is 0x38300040 on Nano 3G, not 0x38300008.
 */
_start:
    bl lcd_preamble
    bl lcd_enter_command_mode

    mov r0, #0x11
    bl lcd_send_cmd8

    ldr r0, =60000
    bl delay_ticks

    mov r0, #0x13
    bl lcd_send_cmd8

    mov r0, #0x29
    bl lcd_send_cmd8

    mov r0, #0x2c
    bl lcd_send_cmd8

    ldr r4, =LCD_WDATA
    ldr r5, =STREAM_COLOR
1:
    bl lcd_wait_ready
    str r5, [r4]
    b 1b

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
    mov r0, #1
    bl delay_ticks

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
    bx lr

lcd_send_cmd8:
    mov r3, r0
    bl lcd_wait_ready
    ldr r1, =LCD_CMD
    str r3, [r1]
    bx lr

lcd_wait_ready:
    ldr r1, =LCD_STATUS
2:
    ldr r0, [r1]
    tst r0, #2
    beq 2b
    bx lr

delay_ticks:
    ldr r1, =TIMER_COUNTER
    ldr r2, [r1]
3:
    ldr r3, [r1]
    sub r3, r3, r2
    cmp r3, r0
    bcc 3b
    bx lr

.size _start, . - _start
