.syntax unified
.arm
.global _start
_start:
    bl 0x2200c8cc
    mov r0, #0
    mov r1, #0
    add r2, pc, #64
    bl 0x2200c738
    bl 0x2201d658
    mov r0, #50
    bl 0x22011488

    ldr r0, f28
    ldr r1, mov1
    ldr r2, bx_lr
    str r1, [r0], #4
    str r2, [r0]

    push {r7, fp, lr}
    sub sp, sp, #0x4c
    ldr r4, r4v
    ldr r5, r5v
    ldr r3, cont
    bx r3

f28:   .word 0x08050f28
mov1:  .word 0xe3a00001
bx_lr: .word 0xe12fff1e
r4v:   .word 0x080f0b34
r5v:   .word 0x080f12b0
cont:  .word 0x0800600c
msg:   .asciz "N3G_F28_GO"
       .align 2
