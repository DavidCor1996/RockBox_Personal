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

.ifndef BOOT_COPY_LEN
.error "BOOT_COPY_LEN must be supplied with --defsym"
.endif

.ifndef ROCK_SRC
.error "ROCK_SRC must be supplied with --defsym"
.endif

.ifndef ROCK_DST
.error "ROCK_DST must be supplied with --defsym"
.endif

.ifndef ROCK_COPY_LEN
.error "ROCK_COPY_LEN must be supplied with --defsym"
.endif

_start:
    ldr sp, =0x22000ff0

    ldr r0, =0x22001000
    ldr r1, =0x22020800
    ldr r2, =BOOT_COPY_LEN
    bl copy_bytes

    ldr r0, =ROCK_SRC
    ldr r1, =ROCK_DST
    ldr r2, =ROCK_COPY_LEN
    bl copy_bytes

    ldr r11, =PATCH_ADDR
    ldr r12, =PATCH_WORD
    str r12, [r11]

    ldr pc, =JUMP_ADDR

copy_bytes:
    cmp r2, #32
    blo copy_tail

copy_words:
    ldmia r0!, {r3-r10}
    stmia r1!, {r3-r10}
    subs r2, r2, #32
    cmp r2, #32
    bhs copy_words

copy_tail:
    cmp r2, #0
    bxeq lr
    ldrb r3, [r0], #1
    strb r3, [r1], #1
    subs r2, r2, #1
    b copy_tail
