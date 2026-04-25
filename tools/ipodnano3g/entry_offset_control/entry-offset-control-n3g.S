.syntax unified
.cpu arm926ej-s
.arm

.section .text
.global _start
.type _start, %function

/* Offset 0x0: immediate infinite loop.
 * Offset 0x20: delayed-fault routine, reachable only if entry offset 0x20
 * is honored at runtime.
 */
_start:
0:
    b 0b

    /* Deterministic padding so the alternate routine starts at body offset 0x20. */
    .space 0x20 - (. - _start), 0

.global entry_offset_0x20
entry_offset_0x20:
    ldr r2, =0x300

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
