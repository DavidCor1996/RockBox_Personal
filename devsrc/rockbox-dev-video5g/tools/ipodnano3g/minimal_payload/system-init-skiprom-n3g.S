.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP, 0x2200fff0

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Control probe with no ROM/service calls.
 *
 * Behavior:
 *   1. set a local stack
 *   2. run two software-only delay phases
 *   3. branch to an invalid address
 *
 * Used to establish whether the deliberate delayed-fault path is itself
 * observable on Nano 3G, independent of the early-init ROM imports.
 */
_start:
    ldr sp, =STACK_TOP
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
