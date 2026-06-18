.syntax unified
.arm
.global _start
_start:
    push {r7, fp, lr}
    sub sp, sp, #0x4c
    ldr r3, lcd_init_exact
    blx r3
    ldr r3, font_init_exact
    blx r3
    ldr r3, show_logo
    blx r3
1:
    b 1b
lcd_init_exact:  .word 0x080743e4
font_init_exact: .word 0x080723a4
show_logo:       .word 0x08005d0c
