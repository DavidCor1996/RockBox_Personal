    .arm
    .section .text
    .global _start

_start:
    /* Visible proof that the confirmed SLEEPRET hook reached this stub. */
    bl      loader_clear
    mov     r0, #0
    mov     r1, #0
    ldr     r2, =msg0
    bl      loader_puts
    mov     r0, #0
    mov     r1, #1
    ldr     r2, =msg1
    bl      loader_puts
    bl      loader_update

    /* Restore the exception vectors that crt0 would normally copy. */
    ldr     r0, =0x08000000
    mov     r1, #0
    mov     r2, #0x3c
1:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    subs    r2, r2, #4
    bne     1b

    /* Restore first words of init helpers that the diagnostic loader patched. */
    ldr     r0, =0x08077fac
    ldr     r1, =0xe92d4010
    str     r1, [r0]
    ldr     r0, =0x080780e8
    ldr     r1, =0xe59f3064
    str     r1, [r0]
    ldr     r0, =0x08077dec
    ldr     r1, =0xe92d4010
    str     r1, [r0]
    ldr     r0, =0x08083f48
    ldr     r1, =0xe92d4010
    str     r1, [r0]

    /* Restore main's IRQ/FIQ enable sequence. */
    ldr     r0, =0x08005db4
    ldr     r1, =0xe10f3000
    str     r1, [r0], #4
    ldr     r1, =0xe3c33080
    str     r1, [r0], #4
    ldr     r1, =0xe121f003
    str     r1, [r0], #4
    ldr     r1, =0xe10f3000
    str     r1, [r0], #4
    ldr     r1, =0xe3c33040
    str     r1, [r0], #4
    ldr     r1, =0xe121f003
    str     r1, [r0], #4

    bl      loader_commit

    /* Match crt0 mode stacks before entering main. */
    msr     CPSR_c, #0xd2
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xd1
    ldr     sp, =0x0000a550
    msr     CPSR_c, #0xd3
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xd7
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xdb
    ldr     sp, =0x0000a150
    msr     CPSR_c, #0xdf
    ldr     sp, =0x00009d50

    ldr     r3, =0x08005d7c
    blx     r3

    /* If main returns, make that visible and stop. */
    bl      loader_clear
    mov     r0, #0
    mov     r1, #0
    ldr     r2, =msg2
    bl      loader_puts
    bl      loader_update
2:
    b       2b

loader_clear:
    ldr     r12, =0x2200c8cc
    bx      r12
loader_puts:
    ldr     r12, =0x2200c738
    bx      r12
loader_update:
    ldr     r12, =0x2201d658
    bx      r12
loader_commit:
    ldr     r12, =0x2201d18c
    bx      r12

msg0:
    .asciz "N3G_REALMAIN"
msg1:
    .asciz "vectors+irq"
msg2:
    .asciz "MAIN RETURN"
    .align 2
