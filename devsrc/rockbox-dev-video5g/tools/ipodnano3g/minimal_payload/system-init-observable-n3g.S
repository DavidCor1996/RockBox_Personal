.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP, 0x2200fff0

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Observable variant of the bounded early-system-init probe.
 *
 * Behavior:
 *   1. set a local stack
 *   2. call embedded OSOS wrapper 0x22002770
 *   3. run two software delay phases
 *   4. branch to an invalid address to force an observable reset/crash
 *
 * This keeps the hardware footprint identical to the bounded early-init probe
 * until the deliberate fault point. No LCD, PMU, GPIO, USB PHY, audio, NAND,
 * or storage logic is added.
 */
_start:
    ldr sp, =STACK_TOP
    bl apple_02770
    bl delay_phase
    bl delay_phase
    ldr pc, =0xdead0000

delay_phase:
    ldr r0, =0x00200000
1:
    subs r0, r0, #1
    bne 1b
    bx lr

.size _start, . - _start

.section .apple_01f00_025b4, "ax"
.global apple_01f00
.type apple_01f00, %function
apple_01f00:
    .incbin "/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin", 0x1f00, 0x6b4

    .set apple_01f0c, apple_01f00 + (0x01f0c - 0x01f00)
    .set apple_023f4, apple_01f00 + (0x023f4 - 0x01f00)
    .set apple_02420, apple_01f00 + (0x02420 - 0x01f00)

.section .apple_02770_02e8c, "ax"
.global apple_02770
.type apple_02770, %function
apple_02770:
    .incbin "/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin", 0x2770, 0x71c

    .set apple_02d78, apple_02770 + (0x02d78 - 0x02770)

.section .apple_030d4_03160, "ax"
.global apple_030d4
.type apple_030d4, %function
apple_030d4:
    .incbin "/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin", 0x30d4, 0x8c

    .set apple_030fc, apple_030d4 + (0x030fc - 0x030d4)
    .set apple_03110, apple_030d4 + (0x03110 - 0x030d4)
    .set apple_03138, apple_030d4 + (0x03138 - 0x030d4)
    .set apple_03150, apple_030d4 + (0x03150 - 0x030d4)

.section .apple_033f0_03580, "ax"
.global apple_033f0
.type apple_033f0, %function
apple_033f0:
    .incbin "/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin", 0x33f0, 0x190

    .set apple_03404, apple_033f0 + (0x03404 - 0x033f0)
    .set apple_0340c, apple_033f0 + (0x0340c - 0x033f0)
    .set apple_03414, apple_033f0 + (0x03414 - 0x033f0)
    .set apple_0341c, apple_033f0 + (0x0341c - 0x033f0)
    .set apple_034bc, apple_033f0 + (0x034bc - 0x033f0)
    .set apple_03564, apple_033f0 + (0x03564 - 0x033f0)
    .set apple_0356c, apple_033f0 + (0x0356c - 0x033f0)
