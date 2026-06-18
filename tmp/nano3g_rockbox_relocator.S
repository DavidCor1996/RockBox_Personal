.arm
.global _start

_start:
    ldr r0, =0x22001000
    ldr r1, =0x22020800
    ldr r2, =78240

copy_loop:
    ldmia r0!, {r3-r10}
    stmia r1!, {r3-r10}
    subs r2, r2, #32
    bhi copy_loop

    ldr pc, =0x22020800
