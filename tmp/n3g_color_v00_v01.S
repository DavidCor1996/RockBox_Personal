    .arm
    .section .text
    .global _start

_start:
    /* Bring loader LCD online once; text may be unreadable later, colors are the signal. */
    ldr     r12, =0x2201d18c
    blx     r12

    /* Manual crt0 prep. */
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

    /* Bypass storage/mount loops and skip panel reinit inside lcd_init. */
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

    /* Rockbox vectors/stacks. */
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

    /* Establish Rockbox viewport/font/display state once. */
    ldr     r3, =0x080743e4
    blx     r3
    ldr     r3, =0x080723a4
    blx     r3

    /* Red: about to enter system_init. */
    ldr     r0, =0xf800
    bl      color
    ldr     r3, =0x08077fac
    blx     r3

    /* Green: system_init returned, about to enter core_allocator_init. */
    ldr     r0, =0x07e0
    bl      color
    ldr     r3, =0x080667f4
    blx     r3

    /* Blue: core_allocator_init returned. */
    ldr     r0, =0x001f
    bl      color

6:
    b       6b

color:
    push    {r4, lr}
    ldr     r1, =0x0813d540
    ldr     r2, =76800
7:
    strh    r0, [r1], #2
    subs    r2, r2, #1
    bne     7b
    ldr     r3, =0x0808f2a8
    blx     r3
    pop     {r4, pc}

    .align 2
    .ltorg
