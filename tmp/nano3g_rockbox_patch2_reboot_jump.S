.arm
.global _start

.ifndef PATCH1_ADDR
.error "PATCH1_ADDR must be supplied with --defsym"
.endif
.ifndef PATCH1_WORD
.error "PATCH1_WORD must be supplied with --defsym"
.endif
.ifndef PATCH2_ADDR
.error "PATCH2_ADDR must be supplied with --defsym"
.endif
.ifndef PATCH2_WORD
.error "PATCH2_WORD must be supplied with --defsym"
.endif
.ifndef JUMP_ADDR
.error "JUMP_ADDR must be supplied with --defsym"
.endif

_start:
    ldr sp, =0x22000ff0
    ldr r0, =0x22001000
    ldr r1, =0x22020800
    ldr r2, =78240

copy_loop:
    ldmia r0!, {r3-r10}
    stmia r1!, {r3-r10}
    subs r2, r2, #32
    bhi copy_loop

    ldr r11, =PATCH1_ADDR
    ldr r12, =PATCH1_WORD
    str r12, [r11]
    ldr r11, =PATCH2_ADDR
    ldr r12, =PATCH2_WORD
    str r12, [r11]

    ldr pc, =JUMP_ADDR
