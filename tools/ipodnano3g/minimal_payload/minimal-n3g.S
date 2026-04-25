.syntax unified
.cpu arm926ej-s
.arm

.section .text
.global _start
.type _start, %function

/* Deliberate-fault execution proof payload for Nano 3G haxed DFU.
 * No MMIO, no stack setup, no runtime, no peripheral access.
 * The only intended side effect is a byte store to an invalid address.
 */
_start:
    ldr r0, =0xffffffff
    mov r1, #0

1:
    strb r1, [r0]
    b 1b

.size _start, . - _start
