.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,            0x2200fff0
.equ TIMER_COUNTER,        0x3c7000b4
.equ WDT_BASE,             0x3c800000

#ifndef NANO3G_COMPLETION_WDT_RESET
#define NANO3G_COMPLETION_WDT_RESET 0
#endif

#ifndef NANO3G_SOFT_DELAY
#define NANO3G_SOFT_DELAY 0
#endif

.equ PMU_REG_EVENTA,       0x02
.equ PMU_REG_IRQMASKA,     0x06
.equ PMU_REG_PWRUNK10,     0x10
.equ PMU_REG_PWR15,        0x15
.equ PMU_REG_PWR16,        0x16
.equ PMU_REG_PWR18,        0x18
.equ PMU_REG_PWR1B,        0x1b
.equ PMU_REG_PWR1D,        0x1d
.equ PMU_REG_LEDCTL,       0x20
.equ PMU_REG_PWR21,        0x21
.equ PMU_REG_LEDOUT,       0x28
.equ PMU_REG_LEDSTATE,     0x29
.equ PMU_REG_LEDCTL2,      0x2a
.equ PMU_REG_DIMSTEP,      0x2b
.equ PMU_REG_PWR30,        0x30
.equ PMU_REG_PWR34,        0x34

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Nano 3G backlight power matrix payload.
 *
 * Purpose:
 *   - prove whether either known PMU LED/backlight register family can produce
 *     visible glow, independent of LCD_IF frame pixels.
 *   - loop visible on/off pulses so a momentary transition is easy to see.
 *
 * Host-side execution signature:
 *   - infinite loop leaves DFU claim in the usual execution/stall state.
 *   - with NANO3G_COMPLETION_WDT_RESET=1, one complete matrix pass ends in a
 *     watchdog reboot to prove the payload reached the end.
 */
_start:
    ldr sp, =STACK_TOP

    bl pmu_preinit_subset

blink_loop:
    bl backlight_path_20_on
    ldr r0, =120000
    bl delay_ticks

    bl backlight_path_20_off
    ldr r0, =60000
    bl delay_ticks

    bl backlight_path_28_29_on
    ldr r0, =120000
    bl delay_ticks

    bl backlight_path_28_29_off
    ldr r0, =60000
    bl delay_ticks

    bl backlight_both_on
    ldr r0, =180000
    bl delay_ticks

    bl backlight_both_off
    ldr r0, =60000
    bl delay_ticks

#if NANO3G_COMPLETION_WDT_RESET
    bl watchdog_reboot
#endif

    b blink_loop

pmu_preinit_subset:
    push {lr}

    mov r0, #PMU_REG_PWR1B
    mov r1, #0x14
    bl pmu_write1

    mov r0, #PMU_REG_PWR16
    mov r1, #0x14
    bl pmu_write1

    mov r0, #PMU_REG_PWR15
    mov r1, #0x14
    bl pmu_write1

    mov r0, #PMU_REG_PWR18
    mov r1, #0x18
    bl pmu_write1

    mov r0, #PMU_REG_PWRUNK10
    bl pmu_read1
    and r0, r0, #0xdb
    orr r1, r0, #0x08
    mov r0, #PMU_REG_PWRUNK10
    bl pmu_write1

    mov r0, #PMU_REG_PWR34
    mov r1, #0x72
    bl pmu_write1

    mov r0, #PMU_REG_PWR30
    bl pmu_read1
    orr r1, r0, #0x20
    mov r0, #PMU_REG_PWR30
    bl pmu_write1

    mov r0, #PMU_REG_PWR21
    mov r1, #0x5c
    bl pmu_write1

    mov r0, #0x0b
    mov r1, #0x06
    bl pmu_write1

    mov r0, #PMU_REG_PWR1D
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

    pop {pc}

backlight_path_20_on:
    push {lr}
    mov r0, #PMU_REG_LEDCTL
    mov r1, #0xff
    bl pmu_write1
    pop {pc}

backlight_path_20_off:
    push {lr}
    mov r0, #PMU_REG_LEDCTL
    mov r1, #0x00
    bl pmu_write1
    pop {pc}

backlight_path_28_29_on:
    push {lr}
    mov r0, #PMU_REG_LEDCTL2
    mov r1, #0x05
    bl pmu_write1
    mov r0, #PMU_REG_DIMSTEP
    mov r1, #0x14
    bl pmu_write1
    mov r0, #PMU_REG_LEDOUT
    mov r1, #0xff
    bl pmu_write1
    mov r0, #PMU_REG_LEDSTATE
    mov r1, #1
    bl pmu_write1
    pop {pc}

backlight_path_28_29_off:
    push {lr}
    mov r0, #PMU_REG_LEDSTATE
    mov r1, #0
    bl pmu_write1
    mov r0, #PMU_REG_LEDOUT
    mov r1, #0
    bl pmu_write1
    pop {pc}

backlight_both_on:
    push {lr}
    bl backlight_path_20_on
    bl backlight_path_28_29_on
    pop {pc}

backlight_both_off:
    push {lr}
    bl backlight_path_28_29_off
    bl backlight_path_20_off
    pop {pc}

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

delay_ticks:
#if NANO3G_SOFT_DELAY
    subs r0, r0, #1
    bne delay_ticks
    bx lr
#else
    push {r1-r3, lr}
    ldr r1, =TIMER_COUNTER
    ldr r2, [r1]
1:
    ldr r3, [r1]
    sub r3, r3, r2
    cmp r3, r0
    bcc 1b
    pop {r1-r3, pc}
#endif

watchdog_reboot:
    msr CPSR_c, #0xd3
    mov r0, #0x100000
    ldr r1, =WDT_BASE
    str r0, [r1]
    bx lr

.size _start, . - _start

.section .text.stub_02f04, "ax"
.global apple_02f04
.type apple_02f04, %function
apple_02f04:
    bx lr

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
