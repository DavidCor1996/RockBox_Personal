    .arm
    .section .text
    .global _start

_start:
    ldr     r2, =msg_go
    bl      show

    /* Copy vectors, then stop using loader LCD. */
    ldr     r0, =0x08000000
    mov     r1, #0
    mov     r2, #0x3c
1:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    subs    r2, r2, #4
    bne     1b

    /* Keep main's IRQ/FIQ enable block patched as NOPs. */
    ldr     r0, =0x08005db4
    ldr     r1, =0xe1a00000
    mov     r2, #6
2:
    str     r1, [r0], #4
    subs    r2, r2, #1
    bne     2b

    ldr     r12, =0x2201d18c
    blx     r12

    msr     CPSR_c, #0xd2
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xd1
    ldr     sp, =0x0000a550
    msr     CPSR_c, #0xd3
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xd7
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xdb
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xdf
    ldr     sp, =0x00009d50

    ldr     r3, =0x08005d7c
    blx     r3
3:
    b       3b

show:
    push    {r2, lr}
    ldr     r12, =0x2200c8cc
    blx     r12
    mov     r0, #0
    mov     r1, #0
    pop     {r2, lr}
    push    {lr}
    ldr     r12, =0x2200c738
    blx     r12
    ldr     r12, =0x2201d658
    blx     r12
    pop     {pc}

msg_go:
    .asciz "NQ0 vec noirq"
    .align 2
