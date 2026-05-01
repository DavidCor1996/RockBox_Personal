.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,       0x22000ff0
.equ PATTERN_ADDR,    0x22000fe0
.equ PATTERN_WORD,    0x3147334e /* "N3G1" in little-endian memory */
.equ WDT_BASE,        0x3c800000
.equ WDT_RESET_VALUE, 0x00100000

.section .text
.global _start
.type _start, %function

/* Minimal direct Nano 3G terminal-state differentiation payload.
 *
 * Behavior:
 *   1. set a local IRAM stack
 *   2. write one marker word into IRAM
 *   3. arm the SoC watchdog reset using the exact in-tree S5L8702 reboot write
 *   4. return with bx lr instead of looping or faulting
 *
 * This avoids OSOS, display, NAND, PMU, clocks, GPIO, and USB PHY logic.
 */
_start:
    ldr sp, =STACK_TOP
    ldr r1, =PATTERN_ADDR
    ldr r2, =PATTERN_WORD
    str r2, [r1]

    msr CPSR_c, #0xd3
    mov r0, #0x100000
    ldr r1, =WDT_BASE
    str r0, [r1]
    bx lr

.size _start, . - _start
