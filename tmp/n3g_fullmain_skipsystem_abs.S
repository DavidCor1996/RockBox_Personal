    .arm
    .section .text
    .global _start

    .macro MARK name
    ldr     r2, =\name
    bl      show
    .endm

_start:
    ldr     r12, =0x2201d18c
    blx     r12
    MARK    msg0

    ldr     r3, =0x08078160
    blx     r3

    ldr     r0, =0x080d23a0
    ldr     r1, =0x081c5538
    mov     r2, #0
1:  cmp     r0, r1
    strlo   r2, [r0], #4
    blo     1b

    ldr     r0, =0x080cff68
    ldr     r1, =0x00000060
    ldr     r2, =0x00002494
2:  cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     2b

    ldr     r0, =0x00002494
    ldr     r1, =0x00007d50
    mov     r2, #0
3:  cmp     r0, r1
    strlo   r2, [r0], #4
    blo     3b

    MARK    msg1

    ldr     r1, =0xe1a00000
    ldr     r0, =0x08005d84
    str     r1, [r0]
    ldr     r0, =0x080743f4
    str     r1, [r0]
    ldr     r0, =0x08005db4
    mov     r2, #6
4:  str     r1, [r0], #4
    subs    r2, r2, #1
    bne     4b

    ldr     r0, =0x0807f134
    ldr     r1, =0xe12fff1e
    str     r1, [r0]

    ldr     r0, =0x08005e2c
    ldr     r1, =0xe3a00000
    str     r1, [r0]
    ldr     r0, =0x08005e84
    ldr     r1, =0xe3a00001
    str     r1, [r0]

    MARK    msg2

    ldr     r0, =0x08000000
    mov     r1, #0
    mov     r2, #0x3c
5:  ldr     r3, [r0], #4
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

    MARK    msg3
    ldr     r3, =0x08005d7c
    blx     r3

    MARK    msg4
6:  b       6b

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

msg0: .asciz "SH0 prep"
    .balign 4
msg1: .asciz "SH1 patch"
    .balign 4
msg2: .asciz "SH2 vec"
    .balign 4
msg3: .asciz "SH3 main"
    .balign 4
msg4: .asciz "SH4 ret"
    .balign 4
    .ltorg
