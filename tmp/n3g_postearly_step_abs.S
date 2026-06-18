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

    .macro PATCH addr, value
    ldr     r0, =\addr
    ldr     r1, =\value
    str     r1, [r0]
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

    PATCH   0x080780e8, 0xe12fff1e
    PATCH   0x0807f134, 0xe12fff1e
    PATCH   0x08005e2c, 0xe3a00000
    PATCH   0x08005e84, 0xe3a00001
    PATCH   0x080743f4, 0xe1a00000

    msr     CPSR_c, #0xdf
    ldr     sp, =0x00009d50

    ldr     r4, =0x0812215c
    ldr     r5, =0x081c553c
    ldr     r6, =0x09cfc000
    str     r6, [r4, #0]
    str     r6, [r4, #8]
    sub     r3, r6, #4
    str     r3, [r4, #4]
    str     r5, [r4, #12]
    str     r5, [r4, #16]
    mov     r3, #1
    strb    r3, [r4, #20]

    CALL    0x08083d48
    CALL    0x08082dc0
    CALL    0x08084170
    CALL    0x0806a878
    ldr     r0, =0x0337f980
    ldr     r1, =0x080cfa9c
    str     r0, [r1]
    mov     r0, #1
    CALL    0x08067800
    CALL    0x08077754
    CALL    0x0807faf4

    MARK    p0
    CALL    0x080743e4
    MARK    p1
    ldr     r0, =0x080f12e4
    mvn     r1, #0
    str     r1, [r0]
    MARK    p2
    CALL    0x080723a4
    MARK    p3
    CALL    0x0801af48
    MARK    p4
    CALL    0x08005d0c
    MARK    p5
    ldr     r0, =0x080c31dc
    ldr     r1, =0x0812098c
    ldr     r2, =884
    CALL    0x08005a84
    MARK    p9
4:  b       4b

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

p0: .asciz "P0 lcd"
    .balign 4
p1: .asciz "P1 lcd"
    .balign 4
p2: .asciz "P2 font"
    .balign 4
p3: .asciz "P3 set"
    .balign 4
p4: .asciz "P4 logo"
    .balign 4
p5: .asciz "P5 lang"
    .balign 4
p9: .asciz "P9 done"
    .balign 4
    .ltorg
