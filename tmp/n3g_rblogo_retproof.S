.syntax unified
.arm
.global _start
_start:
    push {r7, fp, lr}
    sub sp, sp, #0x4c
    ldr r3, logo
    blx r3
    bl 0x2200c8cc
    mov r0, #0
    mov r1, #0
    ldr r2, msg
    bl 0x2200c738
    bl 0x2201d658
1:
    b 1b
logo: .word 0x08005d0c
msg:  .word 0x22017ebe
