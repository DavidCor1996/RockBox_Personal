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

    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0x1000
    bic r0, r0, #0x5
    mcr p15, 0, r0, c1, c0, 0

clean_cache:
    mrc p15, 0, r15, c7, c10, 3
    bne clean_cache
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
    mcr p15, 0, r0, c7, c5, 0

    ldr r1, =0x38e00000
    add r2, r1, #0x00001000
    add r3, r1, #0x00002000
    sub r4, r0, #1
    str r4, [r1,#0x14]
    str r4, [r2,#0x14]
    str r4, [r1,#0xf00]
    str r4, [r2,#0xf00]
    str r4, [r3,#0x08]
    str r4, [r3,#0x0c]
    str r0, [r1,#0x14]
    str r0, [r2,#0x14]

    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #0x1000
    mcr p15, 0, r0, c1, c0, 0

    msr cpsr_c, #0xd2
    ldr sp, =0x22035da0

    msr cpsr_c, #0xd1
    ldr sp, =0x220361a0

    msr cpsr_c, #0xd3
    ldr sp, =0x22035da0

    msr cpsr_c, #0xd7
    ldr sp, =0x22035da0

    msr cpsr_c, #0xdb
    ldr sp, =0x22035da0

    msr cpsr_c, #0xdf
    ldr sp, =0x220359a0
    ldr r2, =0x220339a0
    ldr r3, =0xdeadbeef

stack_fill:
    cmp sp, r2
    strhi r3, [r2], #4
    bhi stack_fill

    ldr pc, =0x22025c74
