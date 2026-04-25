.syntax unified
.cpu arm926ej-s
.arm

.section .text
.global _start
.type _start, %function

/* Payload C: mutate only a word inside the loaded payload image. */
_start:
    ldr r0, =counter
    mov r1, #0

1:
    add r1, r1, #1
    str r1, [r0]
    b 1b

.size _start, . - _start

.section .data
.align 2
counter:
    .word 0
