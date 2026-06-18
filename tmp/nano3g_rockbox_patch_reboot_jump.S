.arm
.global _start

.ifndef PATCH_ADDR
.error "PATCH_ADDR must be supplied with --defsym"
.endif

.ifndef PATCH_WORD
.error "PATCH_WORD must be supplied with --defsym"
.endif

.ifndef JUMP_ADDR
.error "JUMP_ADDR must be supplied with --defsym"
.endif

.ifndef COPY_LEN
.set COPY_LEN, 78240
.endif

_start:
    ldr sp, =0x22000ff0

    ldr r0, =0x22001000
    ldr r1, =0x22020800
    ldr r2, =COPY_LEN

copy_loop:
    ldmia r0!, {r3-r10}
    stmia r1!, {r3-r10}
    subs r2, r2, #32
    bhi copy_loop

    ldr r11, =PATCH_ADDR
    ldr r12, =PATCH_WORD
    str r12, [r11]

    ldr pc, =JUMP_ADDR
