.syntax unified
.cpu arm926ej-s
.arm

.equ PCON0_ADDR,      0x3cf00000
.equ GPIOCMD_ADDR,    0x3cf00200
.equ USEC_TIMER_ADDR, 0x3c7000b4

.equ PCON_ENABLE,     0x53000000
.equ PCON_DISABLE,    0xee000000
.equ PCON_MASK,       0xff000000

.equ GPIOCMD_BASE,    0x0000060e
.equ BEEP_PERIOD_US,  500
.equ HALF_PERIOD_US,  250
.equ BEEP_TIME_US,    100000

.section .text
.global _start
.type _start, %function

/* Minimal Nano 3G piezo proof payload.
 *
 * Source basis:
 *   - firmware/target/arm/s5l8702/ipodnano3g/piezo-nano3g.c
 *   - bootloader/ipod-s5l87xx.c (alive[] = { 500, 100, 0, 0 })
 *
 * Behavior:
 *   - switch GPIO port 0 high byte into the piezo alternate function
 *   - toggle GPIOCMD between 0x0060e and 0x0060f every 250 us
 *   - continue for 100 ms total (about a 2 kHz tone)
 *   - restore the same low-power PCON0 setting used by piezo_stop()
 *   - loop forever
 *
 * No NAND, storage, LCD, PMU, USB PHY, or codec path is touched.
 */
_start:
    ldr r0, =PCON0_ADDR
    ldr r1, [r0]
    bic r1, r1, #PCON_MASK
    ldr r2, =PCON_ENABLE
    orr r1, r1, r2
    str r1, [r0]

    ldr r3, =USEC_TIMER_ADDR
    ldr r4, [r3]
    ldr r5, =BEEP_TIME_US
    add r4, r4, r5
    mov r6, #0

1:
    ldr r7, [r3]
    cmp r7, r4
    bhs 3f

    eor r6, r6, #1
    ldr r0, =GPIOCMD_ADDR
    ldr r1, =GPIOCMD_BASE
    orr r1, r1, r6
    str r1, [r0]

    ldr r7, [r3]
    ldr r0, =HALF_PERIOD_US
    add r7, r7, r0
2:
    ldr r0, [r3]
    cmp r0, r7
    blo 2b
    b 1b

3:
    ldr r0, =PCON0_ADDR
    ldr r1, [r0]
    bic r1, r1, #PCON_MASK
    ldr r2, =PCON_DISABLE
    orr r1, r1, r2
    str r1, [r0]

4:
    b 4b

.size _start, . - _start
