.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,            0x2200fff0
.equ SERVICE_PTR_ADDR,     0x22007398
.equ SERVICE_CTX_ADDR,     0x22007338
.equ PANEL_STATE_ADDR,     0x2200739c
.equ PANEL_SELECTOR_ADDR,  0x2200700c
.equ STARTUP_GATE_MASK,    0x00010000
.equ PLATFORM_MISC_BASE,   0x39300000
.equ USB_OTG_BASE,         0x38400000
.equ RESET_REG_OFF,        0x804
.equ RESET_BIT,            0x2

.equ PMU_REG_EVENTA,       0x02
.equ PMU_REG_IRQMASKA,     0x06
.equ PMU_REG_LEDCTL,       0x20
.equ PMU_LEDCTL_ENABLE,    0x80

.equ LCD_BASE,             0x38300000
.equ LCD_CONFIG,           (LCD_BASE + 0x00)
.equ LCD_CMD,              (LCD_BASE + 0x04)
.equ LCD_STATUS,           (LCD_BASE + 0x1c)
.equ LCD_WDATA,            (LCD_BASE + 0x40)

.equ LCD_MODE_P8,          0x80000c20
.equ LCD_MODE_P8B,         0x80100c20
.equ LCD_MODE_P9,          0x81100db8
.equ LCD_MODE_P16,         0x80100db0
.equ LCD_MODE_P18,         0x80000da8

.equ LCD_WIDTH,            320
.equ LCD_HEIGHT,           240
.equ LCD_PIXELS,           (LCD_WIDTH * LCD_HEIGHT)
.equ TIMER_COUNTER,        0x3c7000b4
.equ WDT_BASE,             0x3c800000

#ifndef NANO3G_COMPLETION_WDT_RESET
#define NANO3G_COMPLETION_WDT_RESET 0
#endif

#ifndef NANO3G_SOFT_DELAY
#define NANO3G_SOFT_DELAY 0
#endif

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Apple-backed Nano 3G LCD full power-preinit/resetpulse/full-init payload.
 *
 * Additions relative to lcd-resetpulse-fullinit-n3g:
 *   - mirror Nano 3G pmu_preinit() before the Apple LCD path
 *   - keep the verified 0x38400804 reset pulse
 *
 * This tests the remaining strong hypothesis: the panel is never electrically
 * powered into a usable state under our reduced payloads.
 */
_start:
    ldr sp, =STACK_TOP

    /* Recreate the minimal runtime callback table expected by the LCD path. */
    ldr r0, =SERVICE_PTR_ADDR
    ldr r1, =service_table
    str r1, [r0]

    /* Re-seed the strap selector and clear cached panel-helper state. */
    ldr r0, =PANEL_SELECTOR_ADDR
    mov r1, #4
    str r1, [r0]

    ldr r0, =PANEL_STATE_ADDR
    mov r1, #0
    str r1, [r0]
    str r1, [r0, #4]

    /* Mirror Nano 3G pmu_preinit(). */
    mov r0, #0x1b
    mov r1, #0x14
    bl pmu_write1

    mov r0, #0x16
    mov r1, #0x14
    bl pmu_write1

    mov r0, #0x15
    mov r1, #0x14
    bl pmu_write1

    mov r0, #0x18
    mov r1, #0x18
    bl pmu_write1

    mov r0, #0x10
    bl pmu_read1
    and r0, r0, #0xdb
    orr r1, r0, #0x8
    mov r0, #0x10
    bl pmu_write1

    mov r0, #0x34
    mov r1, #0x72
    bl pmu_write1

    mov r0, #0x30
    bl pmu_read1
    orr r1, r0, #0x20
    mov r0, #0x30
    bl pmu_write1

    mov r0, #0x21
    mov r1, #0x5c
    bl pmu_write1

    mov r0, #0x0b
    mov r1, #0x06
    bl pmu_write1

    mov r0, #0x1d
    mov r1, #0x00
    bl pmu_write1

    sub sp, sp, #4
    mov r0, #0x42
    strb r0, [sp]
    mov r0, #0xbe
    strb r0, [sp, #1]
    mov r0, #PMU_REG_IRQMASKA
    mov r1, #2
    mov r2, sp
    bl apple_05474
    add sp, sp, #4

    sub sp, sp, #4
    mov r0, #0xff
    strb r0, [sp]
    strb r0, [sp, #1]
    mov r0, #PMU_REG_EVENTA
    mov r1, #2
    mov r2, sp
    bl apple_05474
    add sp, sp, #4

    mov r0, #PMU_REG_LEDCTL
    bl pmu_read1
    bic r1, r0, #PMU_LEDCTL_ENABLE
    mov r0, #PMU_REG_LEDCTL
    bl pmu_write1

    /* Corrected early startup helper block from 0x22001f4c. */
    bl apple_01f4c

    /* Mirror the direct startup gates from 0x22001630..0x22001650. */
    ldr r0, =STARTUP_GATE_MASK
    mov r1, #0
    mov r2, #1
    bl apple_00664

    ldr r0, =PLATFORM_MISC_BASE
    ldr r1, [r0, #0x3c]
    bic r1, r1, #7
    str r1, [r0, #0x3c]

    ldr r0, =STARTUP_GATE_MASK
    mov r1, #0
    mov r2, #0
    bl apple_00664

    /* Reproduce the larger Apple path's pre-LCD reset pulse. */
    ldr r4, =USB_OTG_BASE
    ldr r0, [r4, #RESET_REG_OFF]
    orr r0, r0, #RESET_BIT
    str r0, [r4, #RESET_REG_OFF]

    mov r0, #500
    bl apple_00884

    ldr r0, [r4, #RESET_REG_OFF]
    bic r0, r0, #RESET_BIT
    str r0, [r4, #RESET_REG_OFF]

    mov r0, #10
    bl apple_008c4

    /* Drive the Apple LCD path using the reduced no-op service table. */
    ldr r0, =SERVICE_CTX_ADDR
    bl apple_03c98

    mov r0, #1
    bl apple_03ce0

    mov r0, #4
    bl apple_03ce0

    ldr r0, =SERVICE_CTX_ADDR
    bl apple_03d14

    bl apple_03cc0
    bl apple_0455c

    mov r0, #1
    mov r1, #0
    bl apple_048bc

    mov r0, #4
    mov r1, #0
    bl apple_048bc

    bl lcd_if_pixel_matrix

#if NANO3G_COMPLETION_WDT_RESET
    bl watchdog_reboot
#endif

1:
    b 1b

lcd_if_pixel_matrix:
    push {r4-r11, lr}
    ldr r11, =lcd_modes
    mov r10, #5

mode_loop:
    ldr r9, [r11], #4
    mov r8, #0

packing_loop:
    ldr r7, =lcd_colors
    mov r6, #5

color_loop:
    ldr r5, [r7], #4
    bl lcd_setup_full_window
    mov r0, r9
    bl lcd_write_config
    mov r0, r8
    mov r1, r5
    bl lcd_fill_color_packed
    ldr r0, =50000
    bl delay_ticks
    subs r6, r6, #1
    bne color_loop

    add r8, r8, #1
    cmp r8, #4
    blo packing_loop

    subs r10, r10, #1
    bne mode_loop
    pop {r4-r11, pc}

lcd_setup_full_window:
    push {lr}
    ldr r0, =LCD_MODE_P8
    bl lcd_write_config

    mov r0, #0x2a
    bl lcd_write_cmd
    mov r0, #0x00
    bl lcd_write_data
    mov r0, #0x00
    bl lcd_write_data
    mov r0, #0x01
    bl lcd_write_data
    mov r0, #0x3f
    bl lcd_write_data

    mov r0, #0x2b
    bl lcd_write_cmd
    mov r0, #0x00
    bl lcd_write_data
    mov r0, #0x00
    bl lcd_write_data
    mov r0, #0x00
    bl lcd_write_data
    mov r0, #0xef
    bl lcd_write_data

    mov r0, #0x2c
    bl lcd_write_cmd
    pop {pc}

lcd_fill_color_packed:
    push {r4-r8, lr}
    mov r4, r0
    mov r5, r1
    ldr r6, =LCD_PIXELS

fill_loop:
    cmp r4, #0
    beq fill_word16
    cmp r4, #1
    beq fill_hi_lo
    cmp r4, #2
    beq fill_lo_hi

fill_9bit_shift_hi_lo:
    mov r0, r5, lsr #8
    and r0, r0, #0xff
    mov r0, r0, lsl #1
    bl lcd_write_data
    and r0, r5, #0xff
    mov r0, r0, lsl #1
    bl lcd_write_data
    b fill_next

fill_word16:
    mov r0, r5
    bl lcd_write_data
    b fill_next

fill_hi_lo:
    mov r0, r5, lsr #8
    and r0, r0, #0xff
    bl lcd_write_data
    and r0, r5, #0xff
    bl lcd_write_data
    b fill_next

fill_lo_hi:
    and r0, r5, #0xff
    bl lcd_write_data
    mov r0, r5, lsr #8
    and r0, r0, #0xff
    bl lcd_write_data

fill_next:
    subs r6, r6, #1
    bne fill_loop
    pop {r4-r8, pc}

lcd_write_config:
    push {r1-r2, lr}
    ldr r1, =LCD_STATUS
2:
    ldr r2, [r1]
    tst r2, #2
    beq 2b
    ldr r1, =LCD_CONFIG
    str r0, [r1]
    pop {r1-r2, pc}

lcd_write_cmd:
    push {r1-r2, lr}
    bl lcd_wait_not_busy
    ldr r1, =LCD_CMD
    str r0, [r1]
    pop {r1-r2, pc}

lcd_write_data:
    push {r1-r2, lr}
    bl lcd_wait_not_busy
    ldr r1, =LCD_WDATA
    str r0, [r1]
    pop {r1-r2, pc}

lcd_wait_not_busy:
    ldr r1, =LCD_STATUS
3:
    ldr r2, [r1]
    tst r2, #0x10
    bne 3b
    bx lr

delay_ticks:
#if NANO3G_SOFT_DELAY
    subs r0, r0, #1
    bne delay_ticks
    bx lr
#else
    push {r1-r3, lr}
    ldr r1, =TIMER_COUNTER
    ldr r2, [r1]
4:
    ldr r3, [r1]
    sub r3, r3, r2
    cmp r3, r0
    bcc 4b
    pop {r1-r3, pc}
#endif

watchdog_reboot:
    msr CPSR_c, #0xd3
    mov r0, #0x100000
    ldr r1, =WDT_BASE
    str r0, [r1]
    bx lr

    .align 4
lcd_modes:
    .word LCD_MODE_P8
    .word LCD_MODE_P8B
    .word LCD_MODE_P9
    .word LCD_MODE_P16
    .word LCD_MODE_P18

lcd_colors:
    .word 0x0000ffff
    .word 0x00000000
    .word 0x0000f800
    .word 0x000007e0
    .word 0x0000001f

pmu_write1:
    push {lr}
    sub sp, sp, #4
    strb r1, [sp]
    mov r1, #1
    mov r2, sp
    bl apple_05474
    add sp, sp, #4
    pop {pc}

pmu_read1:
    push {lr}
    sub sp, sp, #4
    mov r1, #1
    mov r2, sp
    bl apple_05420
    ldrb r0, [sp]
    add sp, sp, #4
    pop {pc}

stub_ret0:
    mov r0, #0
    bx lr

    .align 4
service_table:
    .word 0
    .word stub_ret0
    .word stub_ret0
    .space 0x1c
    .word stub_ret0
    .word stub_ret0

.size _start, . - _start

.section .text.stub_02f04, "ax"
.global apple_02f04
.type apple_02f04, %function
apple_02f04:
    bx lr

.section .apple_00648_0208c, "ax"
.global apple_00648
.type apple_00648, %function
apple_00648:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x648, 0x1a44

    .set apple_00664, apple_00648 + (0x00664 - 0x00648)
    .set apple_00828, apple_00648 + (0x00828 - 0x00648)
    .set apple_00884, apple_00648 + (0x00884 - 0x00648)
    .set apple_008c4, apple_00648 + (0x008c4 - 0x00648)
    .set apple_008f0, apple_00648 + (0x008f0 - 0x00648)
    .set apple_00958, apple_00648 + (0x00958 - 0x00648)
    .set apple_00d6c, apple_00648 + (0x00d6c - 0x00648)
    .set apple_01f4c, apple_00648 + (0x01f4c - 0x00648)

.section .apple_03890_04ddc, "ax"
.global apple_03890
.type apple_03890, %function
apple_03890:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x3890, 0x154c

    .set apple_03b18, apple_03890 + (0x03b18 - 0x03890)
    .set apple_03bd4, apple_03890 + (0x03bd4 - 0x03890)
    .set apple_03c18, apple_03890 + (0x03c18 - 0x03890)
    .set apple_03c98, apple_03890 + (0x03c98 - 0x03890)
    .set apple_03cc0, apple_03890 + (0x03cc0 - 0x03890)
    .set apple_03ce0, apple_03890 + (0x03ce0 - 0x03890)
    .set apple_03d14, apple_03890 + (0x03d14 - 0x03890)
    .set apple_0428c, apple_03890 + (0x0428c - 0x03890)
    .set apple_042b8, apple_03890 + (0x042b8 - 0x03890)
    .set apple_042c8, apple_03890 + (0x042c8 - 0x03890)
    .set apple_04438, apple_03890 + (0x04438 - 0x03890)
    .set apple_04500, apple_03890 + (0x04500 - 0x03890)
    .set apple_04540, apple_03890 + (0x04540 - 0x03890)
    .set apple_0455c, apple_03890 + (0x0455c - 0x03890)
    .set apple_04598, apple_03890 + (0x04598 - 0x03890)
    .set apple_045bc, apple_03890 + (0x045bc - 0x03890)
    .set apple_045f8, apple_03890 + (0x045f8 - 0x03890)
    .set apple_04624, apple_03890 + (0x04624 - 0x03890)
    .set apple_0475c, apple_03890 + (0x0475c - 0x03890)
    .set apple_04778, apple_03890 + (0x04778 - 0x03890)
    .set apple_04794, apple_03890 + (0x04794 - 0x03890)
    .set apple_048bc, apple_03890 + (0x048bc - 0x03890)
    .set apple_04968, apple_03890 + (0x04968 - 0x03890)
    .set apple_049a8, apple_03890 + (0x049a8 - 0x03890)
    .set apple_04a50, apple_03890 + (0x04a50 - 0x03890)
    .set apple_04bf8, apple_03890 + (0x04bf8 - 0x03890)
    .set apple_04d74, apple_03890 + (0x04d74 - 0x03890)
    .set apple_04d8c, apple_03890 + (0x04d8c - 0x03890)
    .set apple_04da8, apple_03890 + (0x04da8 - 0x03890)
    .set apple_04dc4, apple_03890 + (0x04dc4 - 0x03890)
    .set apple_04f58, apple_03890 + (0x04f58 - 0x03890)
    .set apple_04f88, apple_03890 + (0x04f88 - 0x03890)

.section .apple_04ff8_0553c, "ax"
.global apple_04ff8
.type apple_04ff8, %function
apple_04ff8:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x4ff8, 0x544

    .set apple_05178, apple_04ff8 + (0x05178 - 0x04ff8)
    .set apple_05258, apple_04ff8 + (0x05258 - 0x04ff8)
    .set apple_05420, apple_04ff8 + (0x05420 - 0x04ff8)
    .set apple_05474, apple_04ff8 + (0x05474 - 0x04ff8)
    .set apple_054b0, apple_04ff8 + (0x054b0 - 0x04ff8)
    .set apple_054f8, apple_04ff8 + (0x054f8 - 0x04ff8)

.section .apple_data_07000, "a"
.global apple_data_07000
apple_data_07000:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x7000, 0x830

.section .apple_zero_07830, "a"
    .fill 0x7d0, 1, 0
