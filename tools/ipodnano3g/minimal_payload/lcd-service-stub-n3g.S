.syntax unified
.cpu arm926ej-s
.arm

.equ RUNTIME_SERVICE_PTR, 0x22007398

.section .text
.global _start
.type _start, %function

/* Minimal Nano 3G runtime-service stub bootstrap.
 *
 * Purpose:
 *   - install a valid callback table pointer at 0x22007398
 *   - provide the four observed callback slots used by the WTF LCD path
 *   - avoid all MMIO and leave the device in a stable loop
 *
 * This payload is host-buildable for review only. It is not executed
 * automatically and does not attempt LCD init by itself.
 */
_start:
    ldr r0, =RUNTIME_SERVICE_PTR
    ldr r1, =service_table
    str r1, [r0]

1:
    b 1b

stub_ret0:
    mov r0, #0
    bx lr

    .align 4
service_table:
    .word 0              /* +0x00 reserved / unknown */
    .word stub_ret0      /* +0x04 mode/state hook */
    .word stub_ret0      /* +0x08 context bind/handoff */
    .space 0x1c          /* +0x0c .. +0x27 unused by observed wrappers */
    .word stub_ret0      /* +0x28 setup/open/query */
    .word stub_ret0      /* +0x2c finalize/commit/start */

.size _start, . - _start
