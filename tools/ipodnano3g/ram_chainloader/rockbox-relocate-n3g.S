.syntax unified
.cpu arm926ej-s
.arm

#ifndef ROCKBOX_BIN
#error "Define ROCKBOX_BIN as the quoted path to rockbox.bin"
#endif

#ifndef ROCKBOX_CHECKSUM
#error "Define ROCKBOX_CHECKSUM as the Rockbox add-up checksum"
#endif

.equ ROCKBOX_DEST, 0x08000000

.section .text
.global _start
.type _start, %function

/*
 * The Nano 3G haxed-DFU path places the IMG1 body at 0x08000800 and
 * enters its first word.  Keep the relocation routine after the embedded
 * Rockbox image so copying the image down to 0x08000000 cannot overwrite
 * code that is still executing.
 */
_start:
    b relocate_rockbox

rockbox_image:
    .incbin ROCKBOX_BIN
rockbox_image_end:
    .align 5

relocate_rockbox:
    msr cpsr_c, #0xd3

    /* Match the S5L8702 crt0 cache/protection handoff. */
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0x1000
    bic r0, r0, #0x5
    mcr p15, 0, r0, c1, c0, 0
1:
    mrc p15, 0, r15, c7, c10, 3
    bne 1b
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
    mcr p15, 0, r0, c7, c5, 0

    ldr r0, =rockbox_image
    ldr r1, =ROCKBOX_DEST
    ldr r2, =rockbox_image_end - rockbox_image
    ldr r3, =ROCKBOX_CHECKSUM
    mov r5, #117

2:
    ldrb r4, [r0], #1
    strb r4, [r1], #1
    add r5, r5, r4
    subs r2, r2, #1
    bne 2b

    cmp r5, r3
    bne checksum_failed

    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
    mcr p15, 0, r0, c7, c5, 0
    ldr r0, =ROCKBOX_DEST
    bx r0

checksum_failed:
    b checksum_failed

.ltorg
.size _start, . - _start
