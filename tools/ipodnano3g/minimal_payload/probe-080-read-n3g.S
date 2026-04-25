.syntax unified
.cpu arm926ej-s
.arm

.equ PROBE_SRC_ADDR,   0x08064000
.equ RESULT_BASE_ADDR, 0x22000100
.equ MARKER_BEFORE,    0x11111111
.equ MARKER_AFTER,     0x22222222

.section .text
.global _start
.type _start, %function

/* Minimal Nano 3G runtime-read probe.
 *
 * Behavior:
 *   - write marker 0x11111111 at 0x22000100
 *   - read one word from 0x08064000
 *   - store it at 0x22000104
 *   - write marker 0x22222222 at 0x22000108
 *   - loop forever
 *
 * No MMIO writes, no stack setup, no runtime, no peripheral access.
 */
_start:
    ldr r0, =RESULT_BASE_ADDR
    ldr r1, =MARKER_BEFORE
    str r1, [r0]

    ldr r2, =PROBE_SRC_ADDR
    ldr r3, [r2]
    str r3, [r0, #4]

    ldr r1, =MARKER_AFTER
    str r1, [r0, #8]

1:
    b 1b

.size _start, . - _start
