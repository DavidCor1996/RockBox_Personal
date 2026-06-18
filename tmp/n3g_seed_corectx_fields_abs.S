    .arm
    .section .text
    .global _start

    .macro MARK name
    ldr     r2, =\name
    bl      show
    .endm

    .macro CALL addr
    ldr     r3, =\addr
    blx     r3
    .endm

_start:
    ldr     r12, =0x2201d18c
    blx     r12

    CALL    0x08078160
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

    msr     CPSR_c, #0xdf
    ldr     sp, =0x00009d50

    ldr     r4, =0x0812215c
    ldr     r5, =0x081c553c
    ldr     r6, =0x09cfc000

    MARK    m0
    str     r6, [r4, #0]
    MARK    m1
    str     r6, [r4, #8]
    MARK    m2
    sub     r3, r6, #4
    str     r3, [r4, #4]
    MARK    m3
    str     r5, [r4, #12]
    MARK    m4
    str     r5, [r4, #16]
    MARK    m5
    mov     r3, #1
    strb    r3, [r4, #20]
    MARK    m6
5:  b       5b

show:
    push    {r0-r3, lr}
    ldr     r12, =0x2200c8cc
    blx     r12
    mov     r0, #0
    mov     r1, #0
    ldr     r2, [sp, #8]
    ldr     r12, =0x2200c738
    blx     r12
    ldr     r12, =0x2201d658
    blx     r12
    pop     {r0-r3, pc}

m0: .asciz "SL0 ctx"
    .balign 4
m1: .asciz "SL1 ht"
    .balign 4
m2: .asciz "SL2 last"
    .balign 4
m3: .asciz "SL3 free"
    .balign 4
m4: .asciz "SL4 start"
    .balign 4
m5: .asciz "SL5 end"
    .balign 4
m6: .asciz "SL6 compact"
    .balign 4
    .ltorg
