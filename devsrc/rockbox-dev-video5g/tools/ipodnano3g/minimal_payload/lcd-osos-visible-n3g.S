.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,            0x2200fff0
.equ SERVICE_PTR_ADDR,     0x22007398
.equ SERVICE_CTX_ADDR,     0x22007338
.equ PANEL_STATE_ADDR,     0x2200739c
.equ PANEL_SELECTOR_ADDR,  0x2200700c
.equ STARTUP_GATE_MASK,    0x00010000
.equ PLATFORM_MISC_BASE,   0x39300000

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Apple-backed Nano 3G visibility payload.
 *
 * Strategy:
 *   - keep the previously prepared Apple-backed startup/pregate/LCD init path
 *   - append the smallest OSOS-derived visibility sequence recovered from the
 *     decrypted RetailOS body:
 *       * import_0x2200374c()
 *       * apple_0x22004c5c(service)
 *       * import_0x2200374c()
 *       * import_0x22003774(service)
 *   - then loop forever
 *
 * Prepared only. Do not run automatically.
 */
_start:
    ldr sp, =STACK_TOP

    /* Recreate the minimal runtime callback table expected by the LCD path. */
    ldr r0, =SERVICE_PTR_ADDR
    ldr r1, =service_table
    str r1, [r0]

    /* Re-seed the strap selector and clear cached panel-helper state. */
    ldr r0, =PANEL_SELECTOR_ADDR
    mov r1, #4
    str r1, [r0]

    ldr r0, =PANEL_STATE_ADDR
    mov r1, #0
    str r1, [r0]
    str r1, [r0, #4]

    /* Corrected early startup helper block from 0x22001f4c. */
    bl apple_01f4c

    /* Mirror the direct startup gates from 0x22001630..0x22001650. */
    ldr r0, =STARTUP_GATE_MASK
    mov r1, #0
    mov r2, #1
    bl apple_00664

    ldr r0, =PLATFORM_MISC_BASE
    ldr r1, [r0, #0x3c]
    bic r1, r1, #7
    str r1, [r0, #0x3c]

    ldr r0, =STARTUP_GATE_MASK
    mov r1, #0
    mov r2, #0
    bl apple_00664

    /* Drive the Apple LCD path using the reduced no-op service table. */
    ldr r0, =SERVICE_CTX_ADDR
    bl apple_03c98

    mov r0, #1
    bl apple_03ce0

    mov r0, #4
    bl apple_03ce0

    ldr r0, =SERVICE_CTX_ADDR
    bl apple_03d14

    bl apple_03cc0
    bl apple_0455c

    mov r0, #1
    mov r1, #0
    bl apple_048bc

    mov r0, #4
    mov r1, #0
    bl apple_048bc

    /* Minimal OSOS-derived visibility-on sequence from 0x220057d4..0x220057e0. */
    bl osos_visibility_on_sequence

1:
    b 1b

stub_ret0:
    mov r0, #0
    bx lr

/* Exact body-visible helper from OSOS 0x22004c5c. */
osos_04c5c:
    push {r4, lr}
    mov r4, r0
    add r0, r0, #0x44
    bl osos_import_0367c
    mov r0, #0
    strb r0, [r4, #5]
    add r0, r4, #0x44
    pop {r4, lr}
    b osos_import_03684

/* Minimal extracted screen-visible sequence from OSOS:
 *   svc = import_0374c()
 *   04c5c(svc)
 *   svc = import_0374c()
 *   import_03774(svc)
 */
osos_visibility_on_sequence:
    push {lr}
    bl osos_import_0374c
    bl osos_04c5c
    bl osos_import_0374c
    bl osos_import_03774
    pop {pc}

/* Imported OSOS service veneers recreated as direct ROM jumps. */
osos_import_0367c:
    ldr pc, =0x080646b4

osos_import_03684:
    ldr pc, =0x08064790

osos_import_0374c:
    ldr pc, =0x080db704

osos_import_03774:
    ldr pc, =0x080dbe58

    .align 4
service_table:
    .word 0
    .word stub_ret0
    .word stub_ret0
    .space 0x1c
    .word stub_ret0
    .word stub_ret0

.size _start, . - _start

.section .text.stub_02f04, "ax"
.global apple_02f04
.type apple_02f04, %function
apple_02f04:
    bx lr

.section .apple_00648_0208c, "ax"
.global apple_00648
.type apple_00648, %function
apple_00648:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x648, 0x1a44

    .set apple_00664, apple_00648 + (0x00664 - 0x00648)
    .set apple_00828, apple_00648 + (0x00828 - 0x00648)
    .set apple_008f0, apple_00648 + (0x008f0 - 0x00648)
    .set apple_00958, apple_00648 + (0x00958 - 0x00648)
    .set apple_00d6c, apple_00648 + (0x00d6c - 0x00648)
    .set apple_01f4c, apple_00648 + (0x01f4c - 0x00648)

.section .apple_03890_04ddc, "ax"
.global apple_03890
.type apple_03890, %function
apple_03890:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x3890, 0x154c

    .set apple_03b18, apple_03890 + (0x03b18 - 0x03890)
    .set apple_03bd4, apple_03890 + (0x03bd4 - 0x03890)
    .set apple_03c18, apple_03890 + (0x03c18 - 0x03890)
    .set apple_03c98, apple_03890 + (0x03c98 - 0x03890)
    .set apple_03cc0, apple_03890 + (0x03cc0 - 0x03890)
    .set apple_03ce0, apple_03890 + (0x03ce0 - 0x03890)
    .set apple_03d14, apple_03890 + (0x03d14 - 0x03890)
    .set apple_0428c, apple_03890 + (0x0428c - 0x03890)
    .set apple_042b8, apple_03890 + (0x042b8 - 0x03890)
    .set apple_042c8, apple_03890 + (0x042c8 - 0x03890)
    .set apple_04438, apple_03890 + (0x04438 - 0x03890)
    .set apple_04500, apple_03890 + (0x04500 - 0x03890)
    .set apple_04540, apple_03890 + (0x04540 - 0x03890)
    .set apple_0455c, apple_03890 + (0x0455c - 0x03890)
    .set apple_04598, apple_03890 + (0x04598 - 0x03890)
    .set apple_045bc, apple_03890 + (0x045bc - 0x03890)
    .set apple_045f8, apple_03890 + (0x045f8 - 0x03890)
    .set apple_04624, apple_03890 + (0x04624 - 0x03890)
    .set apple_0475c, apple_03890 + (0x0475c - 0x03890)
    .set apple_04778, apple_03890 + (0x04778 - 0x03890)
    .set apple_04794, apple_03890 + (0x04794 - 0x03890)
    .set apple_048bc, apple_03890 + (0x048bc - 0x03890)
    .set apple_04968, apple_03890 + (0x04968 - 0x03890)
    .set apple_049a8, apple_03890 + (0x049a8 - 0x03890)
    .set apple_04a50, apple_03890 + (0x04a50 - 0x03890)
    .set apple_04bf8, apple_03890 + (0x04bf8 - 0x03890)
    .set apple_04d74, apple_03890 + (0x04d74 - 0x03890)
    .set apple_04d8c, apple_03890 + (0x04d8c - 0x03890)
    .set apple_04da8, apple_03890 + (0x04da8 - 0x03890)
    .set apple_04dc4, apple_03890 + (0x04dc4 - 0x03890)
    .set apple_04f58, apple_03890 + (0x04f58 - 0x03890)
    .set apple_04f88, apple_03890 + (0x04f88 - 0x03890)

.section .apple_data_07000, "a"
.global apple_data_07000
apple_data_07000:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x7000, 0x830

.section .apple_zero_07830, "a"
    .fill 0x7d0, 1, 0
