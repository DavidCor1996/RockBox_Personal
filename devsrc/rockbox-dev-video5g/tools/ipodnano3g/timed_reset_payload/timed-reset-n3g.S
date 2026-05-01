.syntax unified
.cpu arm926ej-s
.arm

.section .text
.global _start
.type _start, %function

/* Timed reset proof payload for Nano 3G haxed DFU.
 * Busy-waits using registers only, then triggers a deliberate invalid write.
 * No MMIO, no stack setup, no runtime, no RAM writes before the fault.
 */
_start:
    ldr r2, =0x500

1:
    ldr r3, =0x80000
2:
    subs r3, r3, #1
    bne 2b
    subs r2, r2, #1
    bne 1b

    mvn r0, #0
    mov r1, #0
    strb r1, [r0]

3:
    b 3b

.size _start, . - _start
