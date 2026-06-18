    .arm
    .global _start
_start:
    ldr r3, =0x080743e4
    blx r3
    ldr r3, =0x080723a4
    blx r3
    mov r0, #0
    mov r1, #0
    ldr r2, =0x22017ebe
    ldr r3, =0x08074120
    blx r3
    ldr r3, =0x0808f2a8
    blx r3
1:
    b 1b
