.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP, 0x2200fff0

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Minimal Nano 3G early-system-init probe.
 *
 * This replays the bounded OSOS wrapper at 0x22002770, which is the best
 * current shared global-init candidate below the reset vector:
 *   - imported front-edge service call
 *   - CP15 bit clear
 *   - MIU/global memory configuration
 *   - clock/reset-style configuration
 *   - CP15 bit restore
 *
 * It intentionally excludes LCD, audio, NAND, and USB PHY logic.
 * Prepared for review only; do not run automatically.
 */
_start:
    ldr sp, =STACK_TOP
    bl apple_02770
1:
    b 1b

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
