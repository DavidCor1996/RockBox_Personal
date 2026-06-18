.syntax unified
.arm
.global _start
_start:
    ldr r3, lcd_init_exact
    blx r3
    ldr r3, lcd_clear_exact
    blx r3
    mov r0, #0
    mov r1, #0
    ldr r2, msg
    ldr r3, lcd_puts_exact
    blx r3
    ldr r3, lcd_update_exact
    blx r3
1:
    b 1b
lcd_init_exact:   .word 0x080743e4
lcd_clear_exact:  .word 0x080743bc
msg:              .word 0x22017ebe
lcd_puts_exact:   .word 0x08074120
lcd_update_exact: .word 0x0808f2a8
