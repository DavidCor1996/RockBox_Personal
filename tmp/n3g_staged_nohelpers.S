    .arm
    .section .text
    .global _start

_start:
    ldr     r2, =msg_start
    bl      show

    ldr     r0, =0x08000000
    mov     r1, #0
    mov     r2, #0x3c
1:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    subs    r2, r2, #4
    bne     1b
    ldr     r2, =msg_vec
    bl      show

    ldr     r2, =msg_skip
    bl      show

    ldr     r0, =0x08005db4
    ldr     r1, =0xe10f3000
    str     r1, [r0], #4
    ldr     r1, =0xe3c33080
    str     r1, [r0], #4
    ldr     r1, =0xe121f003
    str     r1, [r0], #4
    ldr     r1, =0xe10f3000
    str     r1, [r0], #4
    ldr     r1, =0xe3c33040
    str     r1, [r0], #4
    ldr     r1, =0xe121f003
    str     r1, [r0], #4
    ldr     r2, =msg_irq
    bl      show

    ldr     r12, =0x2201d18c
    blx     r12
    ldr     r2, =msg_commit
    bl      show

    ldr     r2, =msg_stack
    bl      show
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

    ldr     r2, =msg_main
    bl      show
    ldr     r3, =0x08005d7c
    blx     r3

    ldr     r2, =msg_return
    bl      show
2:
    b       2b

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

msg_start:   .asciz "NH0 start"
msg_vec:     .asciz "NH1 vec"
msg_skip:    .asciz "NH2 skip helpers"
msg_irq:     .asciz "NH3 irq"
msg_commit:  .asciz "NH4 commit"
msg_stack:   .asciz "NH5 stack"
msg_main:    .asciz "NH6 main"
msg_return:  .asciz "NH7 return"
    .align 2
