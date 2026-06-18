    .arm
    .section .text
    .global _start

_start:
    ldr     r12, =0x2201d18c
    blx     r12

    /* Manual crt0 memory prep. */
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

    /* Keep the trace environment quiet and bypass known storage loop paths. */
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

    /* Low-memory veneer reachable from Rockbox main BL range. */
    ldr     r1, =0x081c6000
    ldr     r0, =0xe59fc000
    str     r0, [r1], #4
    ldr     r0, =0xe12fff1c
    str     r0, [r1], #4
    ldr     r0, =trace_start
    str     r0, [r1], #4

    /* Patch selected main BL sites to shared tracer. */
    adr     r7, patch_table
    mov     r8, #10
6:
    ldr     r0, [r7], #4
    ldr     r1, [r7], #4
    str     r1, [r0]
    subs    r8, r8, #1
    bne     6b

    ldr     r12, =0x2201d18c
    blx     r12
    ldr     r2, =msg1
    bl      show_loader

    /* Match the full handoff environment before entering main. */
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
    ldr     r3, =0x08005d7c
    blx     r3

8:
    b       8b

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

trace_start:
    mov     r11, lr
    stmdb   sp!, {r4-r11, lr}
    sub     sp, sp, #24
    stmia   sp, {r0-r3}
    mov     r0, r11
    bl      trace_lookup
    mov     r0, r5
    mov     r1, #0
    bl      trace_msg
    bl      trace_show
    ldmia   sp, {r0-r3}
    blx     r4
    str     r0, [sp, #16]
    mov     r0, r5
    mov     r1, #1
    bl      trace_msg
    bl      trace_show
    ldr     r0, [sp, #16]
    add     sp, sp, #24
    ldmia   sp!, {r4-r11, lr}
    bx      lr

trace_lookup:
    adr     r7, trace_table
    mov     r8, #10
7:
    ldr     r1, [r7], #4
    cmp     r0, r1
    ldreq   r4, [r7], #4
    ldreq   r5, [r7], #4
    bxeq    lr
    add     r7, r7, #8
    subs    r8, r8, #1
    bne     7b
    ldr     r4, =0x08083f48
    mov     r5, #99
    bx      lr

trace_msg:
    adr     r2, trace_buf
    mov     r3, #'V'
    strb    r3, [r2]
    mov     r3, #'0'
    cmp     r0, #10
    subhs   r0, r0, #10
    movhs   r3, #'1'
    strb    r3, [r2, #1]
    add     r3, r0, #'0'
    strb    r3, [r2, #2]
    cmp     r1, #0
    moveq   r3, #'g'
    movne   r3, #'r'
    strb    r3, [r2, #4]
    moveq   r3, #'o'
    movne   r3, #'e'
    strb    r3, [r2, #5]
    movne   r3, #'t'
    strneb  r3, [r2, #6]
    moveq   r3, #0
    streqb  r3, [r2, #6]
    movne   r3, #0
    strneb  r3, [r2, #7]
    bx      lr

trace_show:
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

trace_table:
    .word 0x08005d88, 0x08077fac, 0
    .word 0x08005d8c, 0x080667f4, 1
    .word 0x08005d90, 0x08083d48, 2
    .word 0x08005d94, 0x08082dc0, 3
    .word 0x08005d98, 0x08084170, 4
    .word 0x08005d9c, 0x0806a878, 5
    .word 0x08005da4, 0x080780e8, 6
    .word 0x08005dac, 0x08067800, 7
    .word 0x08005db0, 0x08077754, 8
    .word 0x08005db4, 0x0807faf4, 9
    /* Later LCD/logo calls are covered by separate NT checkpoint probes. */
trace_buf:
    .asciz "V00 go"
    .byte 0
    .align 2
    .ltorg
trace_end:

patch_table:
    .word 0x08005d84, 0xeb07009d
    .word 0x08005d88, 0xeb07009c
    .word 0x08005d8c, 0xeb07009b
    .word 0x08005d90, 0xeb07009a
    .word 0x08005d94, 0xeb070099
    .word 0x08005d98, 0xeb070098
    .word 0x08005da0, 0xeb070096
    .word 0x08005da8, 0xeb070094
    .word 0x08005dac, 0xeb070093
    .word 0x08005db0, 0xeb070092
    /* Keep only first ten main calls to fit the hook slot. */

msg1: .asciz "NV trace main"
    .align 2
    .ltorg
trace_copy_end:
