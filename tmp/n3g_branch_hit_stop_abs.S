    .arm
    .section .text
    .global _start

_start:
    ldr     r12, =0x2201d18c
    blx     r12
    mov     r0, #0
    mov     r1, #0
    ldr     r2, =msg
    ldr     r12, =0x2200c738
    blx     r12
    ldr     r12, =0x2201d658
    blx     r12
1:  b       1b

msg:
    .asciz "N3G_BRANCH_HIT"
    .balign 4
    .ltorg
