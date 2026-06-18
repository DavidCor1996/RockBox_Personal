.syntax unified
.arm
.global _start
_start:
    push {r7, fp, lr}
    sub sp, sp, #0x4c
    ldr r3, init_state
    blx r3
    ldr r3, state_base
    mov r0, #1
    strb r0, [r3, #0x59]
    str r0, [r3, #0x64]
    ldr r4, r4v
    ldr r5, r5v
    ldr r3, cont
    bx r3
init_state: .word 0x08050e84
state_base: .word 0x0811bd4c
r4v:        .word 0x080f0b34
r5v:        .word 0x080f12b0
cont:       .word 0x08006010
