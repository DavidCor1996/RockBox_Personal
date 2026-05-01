.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,            0x2200fff0
.equ PMU_REG_LEDCTL,       0x20
.equ PMU_LEDCTL_UNKNOWN,   0x40
.equ PMU_LEDCTL_ENABLE,    0x80
.equ PMU_LEDCTL_OUT_MASK,  0x1f

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Nano 3G PMU backlight-only payload.
 *
 * Purpose:
 *   - touch only the PMU backlight register
 *   - preserve the unknown 0x40 bit if already set
 *   - force enable plus maximum output bits
 *
 * Observable signal:
 *   - any backlight glow means panel/backlight power path is present
 */
_start:
    ldr sp, =STACK_TOP

    mov r0, #PMU_REG_LEDCTL
    bl pmu_read1

    and r0, r0, #PMU_LEDCTL_UNKNOWN
    orr r1, r0, #(PMU_LEDCTL_ENABLE | PMU_LEDCTL_OUT_MASK)
    mov r0, #PMU_REG_LEDCTL
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

pmu_read1:
    push {lr}
    sub sp, sp, #4
    mov r1, #1
    mov r2, sp
    bl apple_05420
    ldrb r0, [sp]
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
