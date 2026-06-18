.arm
.global _start

.ifndef JUMP_ADDR
.error "JUMP_ADDR must be supplied with --defsym"
.endif

.ifndef COPY_LEN
.error "COPY_LEN must be supplied with --defsym"
.endif

_start:
    ldr sp, =0x22000ff0

    ldr r0, =0x22001000
    ldr r1, =0x22020800
    ldr r2, =COPY_LEN
    bl copy_bytes

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
