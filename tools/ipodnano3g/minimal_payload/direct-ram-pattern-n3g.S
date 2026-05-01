.syntax unified
.cpu arm926ej-s
.arm

.equ STACK_TOP,    0x22000ff0
.equ PATTERN_ADDR, 0x22000fe0
.equ PATTERN_WORD, 0x3147334e /* "N3G1" in little-endian memory */

.section .text
.global _start
.type _start, %function

/* Minimal direct Nano 3G payload for haxed DFU execution.
 *
 * Intended behavior:
 *   1. enter directly from WTF/haxed DFU at 0x22000000
 *   2. set a local IRAM stack
 *   3. write one known-safe marker word into IRAM
 *   4. loop forever
 *
 * This deliberately avoids LCD, PMU, NAND, clocks, GPIO, USB PHY, and any
 * ROM/OSOS helper calls. The host-side observable is the same DFU-takeover
 * state already seen on successful direct payload execution:
 *   - lsusb still shows 05ac:1223
 *   - mks5lboot --dfuscan returns LIBUSB_ERROR_OTHER
 */
_start:
    ldr sp, =STACK_TOP
    ldr r0, =PATTERN_ADDR
    ldr r1, =PATTERN_WORD
    str r1, [r0]

1:
    b 1b

.size _start, . - _start
