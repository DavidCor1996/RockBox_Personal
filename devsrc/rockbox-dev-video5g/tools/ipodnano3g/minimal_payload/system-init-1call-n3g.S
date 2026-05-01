.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP, 0x2200fff0

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Control probe that isolates the first ROM/service import only.
 *
 * Behavior:
 *   1. set a local stack
 *   2. call OSOS veneer 0x22003414 -> 0x08016234
 *   3. run two software-only delay phases
 *   4. branch to an invalid address
 *
 * No additional MMIO is introduced beyond whatever the ROM target itself does.
 */
_start:
    ldr sp, =STACK_TOP
    bl apple_03414
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

.section .apple_033f0_03580, "ax"
.global apple_033f0
.type apple_033f0, %function
apple_033f0:
    .incbin "/tmp/n3g-osos-work/n3g-osos-decrypted.body.bin", 0x33f0, 0x190

    .set apple_03414, apple_033f0 + (0x03414 - 0x033f0)
