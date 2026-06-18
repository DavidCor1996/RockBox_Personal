    .arm
    .section .text
    .global _start

_start:
    ldr     r2, =msg0
    bl      lshow

    ldr     r12, =0x2201d18c
    blx     r12

    ldr     r3, =0x08078160
    blx     r3

    ldr     r0, =0x080d23a0
    ldr     r1, =0x081c5538
    mov     r2, #0
1:
    cmp     r0, r1
    strlo   r2, [r0], #4
    blo     1b
    ldr     r0, =0x080cff68
    ldr     r1, =0x00000060
    ldr     r2, =0x00002494
2:
    cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     2b
    ldr     r0, =0x00002494
    ldr     r1, =0x00007d50
    mov     r2, #0
3:
    cmp     r0, r1
    strlo   r2, [r0], #4
    blo     3b

    ldr     r0, =0x08005e2c
    ldr     r1, =0xe3a00000
    str     r1, [r0]
    ldr     r0, =0x08005e84
    ldr     r1, =0xe3a00001
    str     r1, [r0]
    ldr     r0, =0x080743f4
    ldr     r1, =0xe1a00000
    str     r1, [r0]
    ldr     r0, =0x08005db4
    mov     r2, #6
4:
    str     r1, [r0], #4
    subs    r2, r2, #1
    bne     4b

    ldr     r0, =0x08000000
    mov     r1, #0
    mov     r2, #0x3c
5:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    subs    r2, r2, #4
    bne     5b

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

    ldr     r2, =msg3
    bl      lshow
    ldr     r3, =0x08077fac
    blx     r3

    ldr     r2, =msg4
    bl      rbshow
    ldr     r2, =msg5
    bl      rbshow
    ldr     r3, =0x080667f4
    blx     r3
    ldr     r2, =msg6
    bl      rbshow

6:
    b       6b

lshow:
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

rbshow:
    push    {r2, lr}
    ldr     r3, =0x080743e4
    blx     r3
    ldr     r3, =0x080723a4
    blx     r3
    ldr     r3, =0x080743bc
    blx     r3
    mov     r0, #0
    mov     r1, #0
    pop     {r2, lr}
    push    {lr}
    ldr     r3, =0x08074120
    blx     r3
    ldr     r3, =0x0808f2a8
    blx     r3
    pop     {pc}

msg0: .asciz "NY0 direct"
msg3: .asciz "NY3 sys go"
msg4: .asciz "NY4 sys ret"
msg5: .asciz "NY5 core go"
msg6: .asciz "NY6 core ret"
    .align 2
    .ltorg
