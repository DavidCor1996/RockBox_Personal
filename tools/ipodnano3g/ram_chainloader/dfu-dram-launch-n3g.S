.syntax unified
.cpu arm926ej-s
.arm

#ifndef IMAGE_SIZE
#error "Define IMAGE_SIZE"
#endif
#ifndef IMAGE_CHECKSUM
#error "Define IMAGE_CHECKSUM as the Rockbox checksum including model seed"
#endif

.equ IMAGE_ADDR,  0x08000000
.equ STATUS_ADDR, 0x087f0000

.section .text
.global _start
.type _start, %function

_start:
    msr cpsr_c, #0xd3
    ldr r0, =IMAGE_ADDR
    ldr r1, =IMAGE_SIZE
    mov r2, #117

1:
    ldrb r3, [r0], #1
    add r2, r2, r3
    subs r1, r1, #1
    bne 1b

    ldr r0, =STATUS_ADDR
    ldr r1, =0x4c33474e /* "NG3L" */
    str r1, [r0, #0]
    str r2, [r0, #4]
    ldr r1, =IMAGE_CHECKSUM
    str r1, [r0, #8]
    cmp r2, r1
    bne 2f

    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
    mcr p15, 0, r0, c7, c5, 0
    ldr r0, =IMAGE_ADDR
    bx r0

2:
    b 2b

.ltorg
.size _start, . - _start
