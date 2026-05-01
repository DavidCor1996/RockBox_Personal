.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,            0x2200fff0
.equ SERVICE_PTR_ADDR,     0x22007398
.equ SERVICE_CTX_ADDR,     0x22007338
.equ PANEL_STATE_ADDR,     0x2200739c
.equ PANEL_SELECTOR_ADDR,  0x2200700c

.section .text.entry, "ax"
.global _start
.type _start, %function

/* Apple-backed Nano 3G LCD full-init payload.
 *
 * Strategy:
 *   - keep Apple LCD code and tables at their original decrypted WTF VMAs
 *   - install a minimal runtime service table at 0x22007398
 *   - call the Apple wrappers and LCD init helpers from body offset 0
 *
 * This payload is prepared for review only and must not be run automatically.
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

    /* Drive the Apple wrapper sequence using the reduced no-op service table. */
    ldr r0, =SERVICE_CTX_ADDR
    bl apple_03c98

    mov r0, #1
    bl apple_03ce0

    mov r0, #4
    bl apple_03ce0

    ldr r0, =SERVICE_CTX_ADDR
    bl apple_03d14

    bl apple_03cc0

    /* Direct Apple LCD path: preamble + panel-group cache. */
    bl apple_0455c

    /* Minimal conservative init sequence:
     *   mode 1 -> full panel init table
     *   mode 4 -> awake tail where the selected panel helper implements it
     *
     * For groups without a mode-4 tail, the Apple helpers return immediately.
     */
    mov r0, #1
    mov r1, #0
    bl apple_048bc

    mov r0, #4
    mov r1, #0
    bl apple_048bc

1:
    b 1b

stub_ret0:
    mov r0, #0
    bx lr

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
/* The original 0x22002f04 path is only used for an error print in 0x22004bf8.
 * We replace it with a quiet no-op to keep the payload self-contained.
 */
apple_02f04:
    bx lr

.section .apple_00664, "ax"
.global apple_00664
.type apple_00664, %function
apple_00664:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x664, 0x38

.section .apple_00828, "ax"
.global apple_00828
.type apple_00828, %function
apple_00828:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x828, 0xc8

.section .apple_03b18, "ax"
.global apple_03b18
.type apple_03b18, %function
apple_03b18:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x3b18, 0x12c4

    .set apple_03c98, apple_03b18 + (0x03c98 - 0x03b18)
    .set apple_03cc0, apple_03b18 + (0x03cc0 - 0x03b18)
    .set apple_03ce0, apple_03b18 + (0x03ce0 - 0x03b18)
    .set apple_03d14, apple_03b18 + (0x03d14 - 0x03b18)
    .set apple_0428c, apple_03b18 + (0x0428c - 0x03b18)
    .set apple_042b8, apple_03b18 + (0x042b8 - 0x03b18)
    .set apple_042c8, apple_03b18 + (0x042c8 - 0x03b18)
    .set apple_04438, apple_03b18 + (0x04438 - 0x03b18)
    .set apple_04500, apple_03b18 + (0x04500 - 0x03b18)
    .set apple_04540, apple_03b18 + (0x04540 - 0x03b18)
    .set apple_0455c, apple_03b18 + (0x0455c - 0x03b18)
    .set apple_04598, apple_03b18 + (0x04598 - 0x03b18)
    .set apple_045bc, apple_03b18 + (0x045bc - 0x03b18)
    .set apple_045f8, apple_03b18 + (0x045f8 - 0x03b18)
    .set apple_04624, apple_03b18 + (0x04624 - 0x03b18)
    .set apple_0475c, apple_03b18 + (0x0475c - 0x03b18)
    .set apple_04778, apple_03b18 + (0x04778 - 0x03b18)
    .set apple_04794, apple_03b18 + (0x04794 - 0x03b18)
    .set apple_048bc, apple_03b18 + (0x048bc - 0x03b18)
    .set apple_04968, apple_03b18 + (0x04968 - 0x03b18)
    .set apple_049a8, apple_03b18 + (0x049a8 - 0x03b18)
    .set apple_04a50, apple_03b18 + (0x04a50 - 0x03b18)
    .set apple_04bf8, apple_03b18 + (0x04bf8 - 0x03b18)
    .set apple_04d74, apple_03b18 + (0x04d74 - 0x03b18)
    .set apple_04d8c, apple_03b18 + (0x04d8c - 0x03b18)
    .set apple_04da8, apple_03b18 + (0x04da8 - 0x03b18)
    .set apple_04dc4, apple_03b18 + (0x04dc4 - 0x03b18)
    .set apple_04f58, apple_03b18 + (0x04f58 - 0x03b18)
    .set apple_04f88, apple_03b18 + (0x04f88 - 0x03b18)

.section .apple_data_07000, "a"
.global apple_data_07000
apple_data_07000:
    .incbin "/tmp/n3g-wtf-decrypted.body.bin", 0x7000, 0x830

.section .apple_zero_07830, "a"
/* The decrypted body ends at 0x22007830, but the Apple LCD helper code also
 * uses work buffers rooted at 0x22007c74 / 0x22007c84. Pad this range with
 * explicit zeros so the flat binary stays self-contained.
 */
    .fill 0x7d0, 1, 0
