.syntax unified
.cpu arm926ej-s
.arm

#ifndef TARGET_ADDR
#define TARGET_ADDR 0x39200000
#endif

#ifndef BIT_INDEX
#define BIT_INDEX 0
#endif

.equ STACK_TOP,       0x22000ff0
.equ WDT_BASE,        0x3c800000
.equ WDT_RESET_VALUE, 0x00100000

.section .text
.global _start
.type _start, %function

/* Generic Nano 3G one-bit memory readback payload.
 *
 * Input:
 *   TARGET_ADDR - 32-bit word address to read
 *   BIT_INDEX   - bit position 0..31
 *
 * Output channel:
 *   bit == 1 -> loop forever (DFU stuck / LIBUSB_ERROR_OTHER)
 *   bit == 0 -> watchdog reboot (USB drop / stock OS boot)
 */
_start:
    ldr sp, =STACK_TOP
    ldr r0, =TARGET_ADDR
    ldr r1, [r0]
    mov r2, #BIT_INDEX
    lsr r1, r1, r2
    tst r1, #1
    bne bit_is_one

bit_is_zero:
    msr CPSR_c, #0xd3
    mov r3, #0x100000
    ldr r4, =WDT_BASE
    str r3, [r4]
    bx lr

1:
    b 1b

bit_is_one:
2:
    b 2b

.size _start, . - _start
