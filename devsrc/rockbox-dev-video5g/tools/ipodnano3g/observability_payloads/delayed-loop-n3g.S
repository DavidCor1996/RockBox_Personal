.syntax unified
.cpu arm926ej-s
.arm

.section .text
.global _start
.type _start, %function

/* Payload B: register-only busy wait, then infinite loop. */
_start:
    ldr r2, =0x300

1:
    ldr r3, =0x80000
2:
    subs r3, r3, #1
    bne 2b
    subs r2, r2, #1
    bne 1b

3:
    b 3b

.size _start, . - _start
