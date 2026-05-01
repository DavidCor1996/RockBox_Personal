.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,    0x22000ff0
.equ PATTERN_ADDR, 0x22000fe0
.equ PATTERN_WORD, 0x3147334e /* "N3G1" in little-endian memory */
.equ WDT_BASE,        0x3c800000
.equ WDT_RESET_VALUE, 0x00100000

.section .text
.global _start
.type _start, %function

/* Minimal direct Nano 3G payload with in-band RAM write verification.
 *
 * Behavior:
 *   1. set a local IRAM stack
 *   2. write one marker word to IRAM
 *   3. read it back and compare
 *   4. if equal, loop forever
 *   5. if different, arm watchdog reset and return
 *
 * No OSOS, display, NAND, GPIO, clock, or USB writes are used.
 * The only peripheral write is the evidence-backed watchdog reboot primitive
 * already used by the S5L8702 target.
 */
_start:
    ldr sp, =STACK_TOP
    ldr r0, =PATTERN_ADDR
    ldr r1, =PATTERN_WORD
    str r1, [r0]
    ldr r2, [r0]
    cmp r2, r1
    beq success_loop

fault_path:
    msr CPSR_c, #0xd3
    mov r3, #0x100000
    ldr r4, =WDT_BASE
    str r3, [r4]
    bx lr

1:
    b 1b

success_loop:
2:
    b 2b

.size _start, . - _start
