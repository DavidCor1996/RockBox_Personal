    .arm
    .section .text
    .global _start

_start:
    ldr     r2, =msg_go
    bl      show_loader

    /* Install Rockbox vectors, then do not call loader LCD helpers again. */
    ldr     r0, =0x08000000
    mov     r1, #0
    mov     r2, #0x3c
1:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    subs    r2, r2, #4
    bne     1b

    /* Keep main IRQ/FIQ enable block disabled for this display-only probe. */
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

    /* Skip lcd_init_device inside exact lcd_init: 080743f4 BL 08079b48. */
    ldr     r0, =0x080743f4
    ldr     r1, =0xe1a00000
    str     r1, [r0]
    ldr     r12, =0x2201d18c
    blx     r12

    /* Rockbox-native LCD path, exact-image addresses from confirmed probes. */
    ldr     r3, =0x080743e4
    blx     r3
    ldr     r3, =0x080723a4
    blx     r3
    ldr     r3, =0x08074118
    blx     r3
    mov     r0, #0
    mov     r1, #0
    ldr     r2, =msg_rb
    ldr     r3, =0x08074120
    blx     r3
    ldr     r3, =0x0808f2a8
    blx     r3
3:
    b       3b

show_loader:
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
    .asciz "NR0 rblcd next"
    .align 2
msg_rb:
    .asciz "NR1 Rockbox LCD"
    .align 2
