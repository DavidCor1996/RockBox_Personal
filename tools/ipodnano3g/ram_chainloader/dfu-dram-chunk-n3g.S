.syntax unified
.cpu arm926ej-s
.arm

#ifndef CHUNK_BIN
#error "Define CHUNK_BIN as the quoted path to the chunk"
#endif
#ifndef CHUNK_DEST
#error "Define CHUNK_DEST as the destination address"
#endif
#ifndef CHUNK_SIZE
#error "Define CHUNK_SIZE as the exact chunk byte count"
#endif
#ifndef CHUNK_SUM
#error "Define CHUNK_SUM as the byte-add checksum"
#endif
#ifndef CHUNK_ID
#error "Define CHUNK_ID"
#endif

.equ STATUS_ADDR, 0x087f0000

.section .text
.global _start
.type _start, %function

_start:
    msr cpsr_c, #0xd3
    ldr r0, =chunk_data
    ldr r1, =CHUNK_DEST
    ldr r2, =CHUNK_SIZE
    mov r3, #0

1:
    ldrb r4, [r0], #1
    strb r4, [r1], #1
    add r3, r3, r4
    subs r2, r2, #1
    bne 1b

    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4

    ldr r0, =STATUS_ADDR
    ldr r1, =0x4b33474e /* "NG3K" */
    str r1, [r0, #0]
    ldr r1, =CHUNK_ID
    str r1, [r0, #4]
    str r3, [r0, #8]
    ldr r1, =CHUNK_SUM
    str r1, [r0, #12]
    ldr r2, =CHUNK_DEST
    str r2, [r0, #16]
    ldr r2, =CHUNK_SIZE
    str r2, [r0, #20]
    cmp r3, r1
    moveq r2, #1
    movne r2, #0
    str r2, [r0, #24]

2:
    b 2b

.ltorg
.align 4
chunk_data:
    .incbin CHUNK_BIN
chunk_data_end:

.if (chunk_data_end - chunk_data) != CHUNK_SIZE
.error "CHUNK_SIZE does not match CHUNK_BIN"
.endif

.size _start, . - _start
