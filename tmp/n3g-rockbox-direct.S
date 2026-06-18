.syntax unified
.cpu arm926ej-s
.arm

.section .text
.global _start
_start:
    ldr r0, =rockbox_start
    ldr r1, =0x48000000
    ldr r2, =rockbox_start + 861084

copy_loop:
    cmp r0, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo copy_loop

    ldr r0, =0x08000000
    bx r0

.ltorg
.align 4
rockbox_start:
