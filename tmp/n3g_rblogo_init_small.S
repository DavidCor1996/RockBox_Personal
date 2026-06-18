.syntax unified
.arm
.global _start
_start:
    push {r7, fp, lr}
    sub sp, sp, #0x4c
    ldr r3, lcd_init
    blx r3
    ldr r3, font_init
    blx r3
    ldr r3, logo
    blx r3
1:
    b 1b
lcd_init:  .word 0x08074064
font_init: .word 0x08072024
logo:      .word 0x08005d0c
