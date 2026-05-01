.syntax unified
.cpu arm926ej-s
.arm

.section .text
.global _start
.type _start, %function

/* Payload A: immediate infinite loop, no writes. */
_start:
1:
    b 1b

.size _start, . - _start
