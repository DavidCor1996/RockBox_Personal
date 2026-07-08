.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,    0x22000ff0
.equ PATTERN_ADDR, 0x22000f00
.equ PATTERN_WORD, 0x5247334e /* "N3GR" in little-endian memory */

.section .text
.global _start
.type _start, %function

/* Minimal direct-call Nano 3G payload.
 *
 * Behavior:
 *   1. set a local IRAM stack
 *   2. write one marker word into IRAM
 *   3. return to the caller
 *
 * This lets the host prove that uploaded code executed by reading PATTERN_ADDR
 * from the same RCE transaction, without touching LCD, PMU, NAND, clocks, or
 * USB PHY state.
 */
_start:
    ldr sp, =STACK_TOP
    ldr r0, =PATTERN_ADDR
    ldr r1, =PATTERN_WORD
    str r1, [r0]
    bx lr

.size _start, . - _start
