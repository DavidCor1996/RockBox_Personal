.syntax unified
.arm
.global _start
_start:
    push {r7, fp, lr}
    sub sp, sp, #0x4c
    ldr r3, logo
    blx r3
1:
    ldr r3, sleep
    mov r0, #100
    blx r3
    b 1b
logo:  .word 0x08005d0c
sleep: .word 0x08083f48
