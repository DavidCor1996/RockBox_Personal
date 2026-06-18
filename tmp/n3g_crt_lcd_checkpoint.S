    .arm
    .section .text
    .global _start

_start:
    ldr     r2, =msg0
    bl      show

    ldr     r12, =0x2201d18c
    blx     r12

    /* Same manual crt0 memory prep used by the loader path that reaches main. */
    ldr     r3, =0x08078160
    blx     r3
    ldr     r2, =msg1
    bl      show

    ldr     r0, =0x080d23a0
    ldr     r1, =0x081c5538
    mov     r2, #0
1:
    cmp     r0, r1
    strlo   r2, [r0], #4
    blo     1b
    ldr     r2, =msg2
    bl      show

    ldr     r0, =0x080cff68
    ldr     r1, =0x00000060
    ldr     r2, =0x00002494
2:
    cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     2b
    ldr     r2, =msg3
    bl      show

    ldr     r0, =0x00002494
    ldr     r1, =0x00007d50
    mov     r2, #0
3:
    cmp     r0, r1
    strlo   r2, [r0], #4
    blo     3b
    ldr     r2, =msg4
    bl      show

    /* Do not rerun panel hardware init yet; use loader-initialized display. */
    ldr     r0, =0x080743f4
    ldr     r1, =0xe1a00000
    str     r1, [r0]
    ldr     r12, =0x2201d18c
    blx     r12
    ldr     r2, =msg5
    bl      show

    ldr     r3, =0x080743e4
    blx     r3
    ldr     r2, =msg6
    bl      show

    ldr     r3, =0x080723a4
    blx     r3
    ldr     r2, =msg7
    bl      show

    ldr     r3, =0x080743bc
    blx     r3
    ldr     r2, =msg8
    bl      show

    mov     r0, #0
    mov     r1, #0
    ldr     r2, =msg_rb
    ldr     r3, =0x08074120
    blx     r3
    ldr     r2, =msg9
    bl      show

    ldr     r3, =0x0808f2a8
    blx     r3
    ldr     r2, =msg10
    bl      show
4:
    b       4b

show:
    push    {r2, lr}
    ldr     r12, =0x2200c8cc
    blx     r12
    mov     r0, #0
    mov     r1, #0
    pop     {r2, lr}
    push    {lr}
    ldr     r12, =0x2200c738
    blx     r12
    ldr     r12, =0x2201d658
    blx     r12
    pop     {pc}

msg0:   .asciz "NT0 crt lcd"
msg1:   .asciz "NT1 memret"
msg2:   .asciz "NT2 bss"
msg3:   .asciz "NT3 iram"
msg4:   .asciz "NT4 ibss"
msg5:   .asciz "NT5 patchlcd"
msg6:   .asciz "NT6 lcdret"
msg7:   .asciz "NT7 fontret"
msg8:   .asciz "NT8 clearret"
msg9:   .asciz "NT9 putsret"
msg10:  .asciz "NTA updateret"
msg_rb: .asciz "Rockbox LCD OK"
    .align 2
