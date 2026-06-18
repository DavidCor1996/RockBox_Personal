.arm
.global _start

_start:
    ldr sp, =0x22000ff0

    ldr r0, =0x22001000
    ldr r1, =0x22020800
    ldr r2, =78240

copy_loop:
    ldmia r0!, {r3-r10}
    stmia r1!, {r3-r10}
    subs r2, r2, #32
    bhi copy_loop

    ldr r1, =0x22000fe0
    ldr r2, =0x3147334e
    str r2, [r1]

    msr CPSR_c, #0xd3
    mov r0, #0x100000
    ldr r1, =0x3c800000
    str r0, [r1]
    bx lr
