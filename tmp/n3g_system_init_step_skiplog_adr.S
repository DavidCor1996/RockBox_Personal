    .arm
    .section .text
    .global _start

_start:
    ldr     r12, =0x2201d18c
    blx     r12
    bl      showi
    .asciz "SA prep"
    .align 2

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
4:  str     r1, [r0], #4
    subs    r2, r2, #1
    bne     4b

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

    bl      showi
    .asciz "SB0 skip"
    .align 2

    bl      showi
    .asciz "SB1 cg"
    .align 2
    mov     r0, #4
    mov     r1, #0
    ldr     r3, =0x08078900
    blx     r3

    bl      showi
    .asciz "SB2 clk"
    .align 2
    ldr     r0, =0x080cb3c0
    mov     r1, #0
    ldr     r3, =0x08078804
    blx     r3

    bl      showi
    .asciz "SB3 gpre"
    .align 2
    ldr     r3, =0x08078264
    blx     r3

    bl      showi
    .asciz "SB4 i2c"
    .align 2
    mov     r0, #0
    ldr     r3, =0x08077948
    blx     r3

    bl      showi
    .asciz "SB5 pmu"
    .align 2
    ldr     r3, =0x0807f9d8
    blx     r3

    bl      showi
    .asciz "SB6 gpio"
    .align 2
    ldr     r3, =0x080782a0
    blx     r3

    bl      showi
    .asciz "SB7 eint"
    .align 2
    ldr     r3, =0x080782b4
    blx     r3

    bl      showi
    .asciz "SB8 vic"
    .align 2
    ldr     r2, =0x38e01000
    mvn     r3, #0
    str     r3, [r2, #0x14]
    str     r3, [r2, #0x34]
    str     r3, [r2, #0x54]
    str     r3, [r2, #0x74]
    ldr     r2, =0x38e00000
    str     r3, [r2, #0x14]
    str     r3, [r2, #0x34]
    str     r3, [r2, #0x54]
    str     r3, [r2, #0x74]

    bl      showi
    .asciz "SB9 dma"
    .align 2
    ldr     r3, =0x080784a4
    blx     r3

    bl      showi
    .asciz "SBA uart"
    .align 2
    ldr     r3, =0x08079918
    blx     r3

    bl      showi
    .asciz "SBB irq"
    .align 2
    ldr     r2, =0x38e00000
    ldr     r1, [r2]
    orr     r1, r1, #0x40000000
    str     r1, [r2]
    ldr     r2, =0x38e01000
    ldr     r1, [r2]
    orr     r1, r1, #0x40
    orr     r1, r1, #0x800000
    str     r1, [r2]

    bl      showi
    .asciz "SBC done"
    .align 2
6:  b       6b

showi:
    push    {r4, lr}
    mov     r2, lr
    mov     r4, lr
7:  ldrb    r0, [r4], #1
    cmp     r0, #0
    bne     7b
    add     r4, r4, #3
    bic     r4, r4, #3
    bl      show
    mov     r0, r4
    pop     {r4, lr}
    bx      r0

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

    .align 2
    .ltorg
