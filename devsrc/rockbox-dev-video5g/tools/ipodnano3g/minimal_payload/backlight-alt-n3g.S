.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,            0x2200fff0

.equ PMU_REG_LEDOUT,       0x28
.equ PMU_REG_LEDSTATE,     0x29
.equ PMU_REG_LEDCTL2,      0x2a
.equ PMU_REG_DIMSTEP,      0x2b

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Alternate PMU backlight-only payload using the 6g-style register block.
 *
 * Motivation:
 *   - Nano3G backlight code uses D1671 LEDCTL at 0x20, but the Nano3G debug
 *     screen reports backlight state via 0x29 and brightness via 0x28.
 *   - This payload tests that alternate PMU-controlled backlight path.
 */
_start:
    ldr sp, =STACK_TOP

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

1:
    b 1b

pmu_write1:
    push {lr}
    sub sp, sp, #4
    strb r1, [sp]
    mov r1, #1
    mov r2, sp
    bl apple_05474
    add sp, sp, #4
    pop {pc}

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
