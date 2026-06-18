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
    MARK    m0

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

    MARK    m2
    mov     r0, #4
    mov     r1, #0
    ldr     r3, =0x08078900
    blx     r3

    MARK    m3
    ldr     r0, =0x080cb3c0
    mov     r1, #0
    ldr     r3, =0x08078804
    blx     r3

    MARK    m4
    ldr     r3, =0x08078264
    blx     r3

    MARK    m5
    mov     r0, #0
    ldr     r3, =0x08077948
    blx     r3

    MARK    m6
    ldr     r3, =0x0807f9d8
    blx     r3

    MARK    m7
    ldr     r3, =0x080782a0
    blx     r3

    MARK    m8
    ldr     r3, =0x080782b4
    blx     r3

    MARK    m9
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

    MARK    ma
    ldr     r3, =0x080784a4
    blx     r3

    MARK    mb
    ldr     r3, =0x08079918
    blx     r3

    MARK    mc
    ldr     r2, =0x38e00000
    ldr     r1, [r2]
    orr     r1, r1, #0x40000000
    str     r1, [r2]
    ldr     r2, =0x38e01000
    ldr     r1, [r2]
    orr     r1, r1, #0x40
    orr     r1, r1, #0x800000
    str     r1, [r2]

    MARK    md
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

m0: .asciz "SC prep"
    .balign 4
m1: .asciz "SC0 skip"
    .balign 4
m2: .asciz "SD1 cg"
    .balign 4
m3: .asciz "SD2 clk"
    .balign 4
m4: .asciz "SD3 gpre"
    .balign 4
m5: .asciz "SD4 i2c"
    .balign 4
m6: .asciz "SD5 pmu"
    .balign 4
m7: .asciz "SD6 gpio"
    .balign 4
m8: .asciz "SD7 eint"
    .balign 4
m9: .asciz "SD8 vic"
    .balign 4
ma: .asciz "SD9 dma"
    .balign 4
mb: .asciz "SDA uart"
    .balign 4
mc: .asciz "SDB irq"
    .balign 4
md: .asciz "SDC done"
    .balign 4
    .ltorg
