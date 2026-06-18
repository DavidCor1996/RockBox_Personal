.syntax unified
.arm

.global _start
_start:
    push {lr}

    ldr r2, =msg_go
    bl show

    ldr r3, =0x08005d7c
    blx r3

    ldr r2, =msg_ret
    bl show

1:
    ldr r3, =0x22011488
    mov r0, #100
    blx r3
    b 1b

show:
    push {r4, lr}
    mov r4, r2
    ldr r3, =0x2200c8cc
    blx r3
    mov r0, #0
    mov r1, #0
    mov r2, r4
    ldr r3, =0x2200c738
    blx r3
    ldr r3, =0x2201d658
    blx r3
    pop {r4, pc}

msg_go:
    .asciz "N3G_MAIN_GO"
msg_ret:
    .asciz "N3G_MAIN_RET"
    .balign 4
