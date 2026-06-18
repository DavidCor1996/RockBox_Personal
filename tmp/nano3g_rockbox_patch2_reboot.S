.arm
.global _start

.ifndef PATCH_ADDR1
.error "PATCH_ADDR1 must be supplied with --defsym"
.endif

.ifndef PATCH_WORD1
.error "PATCH_WORD1 must be supplied with --defsym"
.endif

.ifndef PATCH_ADDR2
.error "PATCH_ADDR2 must be supplied with --defsym"
.endif

.ifndef PATCH_WORD2
.error "PATCH_WORD2 must be supplied with --defsym"
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

    ldr r11, =PATCH_ADDR1
    ldr r12, =PATCH_WORD1
    str r12, [r11]

    ldr r11, =PATCH_ADDR2
    ldr r12, =PATCH_WORD2
    str r12, [r11]

    ldr pc, =0x220208a0
