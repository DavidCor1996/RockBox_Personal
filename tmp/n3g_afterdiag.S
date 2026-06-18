.syntax unified
.arm

.global _start
_start:
    push {r4-r8, lr}

    ldr r3, =0x22011488
    mov r0, #100
    blx r3

    ldr r3, =0x0811bd4c
    ldr r4, =line_flags
    ldrb r0, [r3, #0x59]
    add r1, r4, #3
    bl hex8
    ldrb r0, [r3, #0x5a]
    add r1, r4, #9
    bl hex8
    ldrb r0, [r3, #0x5b]
    add r1, r4, #15
    bl hex8
    ldrb r0, [r3, #0x5c]
    add r1, r4, #21
    bl hex8

    ldr r4, =line_w64
    ldr r0, [r3, #0x64]
    add r1, r4, #4
    bl hex32

    ldr r4, =line_w36c
    ldr r0, [r3, #0x36c]
    add r1, r4, #5
    bl hex32

    ldr r4, =line_w370
    ldr r0, [r3, #0x370]
    add r1, r4, #5
    bl hex32

    ldr r3, =0x2200c8cc
    blx r3

    mov r0, #0
    mov r1, #0
    ldr r2, =title
    bl puts
    mov r0, #0
    mov r1, #1
    ldr r2, =line_flags
    bl puts
    mov r0, #0
    mov r1, #2
    ldr r2, =line_w64
    bl puts
    mov r0, #0
    mov r1, #3
    ldr r2, =line_w36c
    bl puts
    mov r0, #0
    mov r1, #4
    ldr r2, =line_w370
    bl puts

    ldr r3, =0x2201d658
    blx r3

1:
    ldr r3, =0x22011488
    mov r0, #100
    blx r3
    b 1b

puts:
    ldr r3, =0x2200c738
    bx r3

hex8:
    push {r2-r5, lr}
    ldr r5, =digits
    mov r2, r0, lsr #4
    and r2, r2, #0xf
    ldrb r2, [r5, r2]
    strb r2, [r1]
    and r2, r0, #0xf
    ldrb r2, [r5, r2]
    strb r2, [r1, #1]
    pop {r2-r5, pc}

hex32:
    push {r2-r7, lr}
    ldr r5, =digits
    mov r6, #28
2:
    mov r2, r0, lsr r6
    and r2, r2, #0xf
    ldrb r2, [r5, r2]
    strb r2, [r1], #1
    subs r6, r6, #4
    bpl 2b
    pop {r2-r7, pc}

digits:
    .ascii "0123456789ABCDEF"
title:
    .asciz "N3G_RAWSTATE"
line_flags:
    .asciz "59=00 5A=00 5B=00 5C=00"
line_w64:
    .asciz "064=00000000"
line_w36c:
    .asciz "36C=00000000"
line_w370:
    .asciz "370=00000000"

