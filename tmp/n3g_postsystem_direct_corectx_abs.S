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

    MARK    n0
    bl      seed_core_ctx
    MARK    n1
    CALL    0x08083d48
    MARK    n2
    CALL    0x08082dc0
    MARK    n3
    CALL    0x08084170
    MARK    n4
    CALL    0x0806a878
    MARK    n5
    ldr     r0, =0x0337f980
    CALL    0x080780e8
    MARK    n6
    mov     r0, #1
    CALL    0x08067800
    MARK    n7
    CALL    0x08077754
    MARK    n8
    CALL    0x0807faf4
    MARK    n9
6:  b       6b

seed_core_ctx:
    ldr     r0, =0x0812215c
    ldr     r1, =0x081c553c
    ldr     r2, =0x09cfc000
    str     r2, [r0, #0]      @ handle_table
    str     r2, [r0, #8]      @ last_handle
    sub     r3, r2, #4
    str     r3, [r0, #4]      @ first_free_handle
    str     r1, [r0, #12]     @ buf_start
    str     r1, [r0, #16]     @ alloc_end
    mov     r3, #1
    strb    r3, [r0, #20]     @ compact
    bx      lr

show:
    push    {r0-r6, lr}
    ldr     r12, =0x2200c8cc
    blx     r12
    mov     r0, #0
    mov     r1, #0
    ldr     r2, [sp, #8]
    ldr     r12, =0x2200c738
    blx     r12
    ldr     r12, =0x2201d658
    blx     r12
    pop     {r0-r6, pc}

n0: .asciz "SK0 seedcore"
    .balign 4
n1: .asciz "SK1 kernel"
    .balign 4
n2: .asciz "SK2 82dc0"
    .balign 4
n3: .asciz "SK3 84170"
    .balign 4
n4: .asciz "SK4 a878"
    .balign 4
n5: .asciz "SK5 freq"
    .balign 4
n6: .asciz "SK6 c1"
    .balign 4
n7: .asciz "SK7 7754"
    .balign 4
n8: .asciz "SK8 faf4"
    .balign 4
n9: .asciz "SK9 done"
    .balign 4
    .ltorg
