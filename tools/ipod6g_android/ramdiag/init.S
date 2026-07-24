/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Syscall-only PID 1 for the N25 volatile diagnostic initramfs.
 *
 * It has no libc, shell, filesystem writer, or storage discovery logic.  The
 * kernel opens stdin/stdout/stderr on the configured console before execing
 * PID 1.  On the visible N25 profile it paints three RGB565 bands through
 * /dev/fb0 before waiting for USB.  This proves that the kernel reached PID 1
 * independently of USB enumeration.  The program then retries /dev/ttyGS0
 * until the USB serial gadget is ready and emits a heartbeat there.  Sending
 * a lower-case 'r' over ttyGS0 requests the Linux reboot syscall.
 */

    .syntax unified
    .arch armv5te
    .arm

    .equ SYS_READ,       3
    .equ SYS_WRITE,      4
    .equ SYS_OPEN,       5
    .equ SYS_CLOSE,      6
    .equ SYS_REBOOT,    88
    .equ SYS_NANOSLEEP, 162

    .equ O_WRONLY,       1
    .equ O_RDWR_NONBLOCK, 0x802
    .equ LINUX_REBOOT_MAGIC1, 0xfee1dead
    .equ LINUX_REBOOT_MAGIC2, 0x28121969
    .equ LINUX_REBOOT_CMD_RESTART, 0x01234567

    .section .text
    .global _start
    .type _start, %function
_start:
    ldr     r10, [sp]
    mov     r0, #1
    ldr     r1, =startup_message
    ldr     r2, =startup_message_len
    mov     r7, #SYS_WRITE
    svc     #0

    /*
     * The framebuffer is optional in host/QEMU tests and mandatory in the
     * visible N25 hardware profile.  One full write avoids deferred-I/O races
     * and produces green, blue, and white thirds when PID 1 is alive.
     */
    bl      paint_pid1_bands

    /* Any explicit argument selects stdin/stdout as a QEMU test transport. */
    cmp     r10, #1
    bgt     emulated_serial_ready

open_usb_serial:
    ldr     r0, =tty_path
    ldr     r1, =O_RDWR_NONBLOCK
    mov     r2, #0
    mov     r7, #SYS_OPEN
    svc     #0
    cmp     r0, #0
    bge     usb_serial_ready

    bl      sleep_one_second
    b       open_usb_serial

usb_serial_ready:
    mov     r8, r0
    mov     r9, r0
    b       heartbeat_loop

emulated_serial_ready:
    mov     r8, #0
    mov     r9, #1

heartbeat_loop:
    mov     r0, #1
    ldr     r1, =heartbeat_message
    ldr     r2, =heartbeat_message_len
    mov     r7, #SYS_WRITE
    svc     #0

    mov     r0, r9
    ldr     r1, =heartbeat_message
    ldr     r2, =heartbeat_message_len
    mov     r7, #SYS_WRITE
    svc     #0

    mov     r0, r8
    ldr     r1, =command_byte
    mov     r2, #1
    mov     r7, #SYS_READ
    svc     #0
    cmp     r0, #1
    bne     heartbeat_sleep
    ldr     r1, =command_byte
    ldrb    r0, [r1]
    cmp     r0, #'r'
    beq     restart

heartbeat_sleep:
    bl      sleep_one_second
    b       heartbeat_loop

sleep_one_second:
    ldr     r0, =one_second
    mov     r1, #0
    mov     r7, #SYS_NANOSLEEP
    svc     #0
    bx      lr

paint_pid1_bands:
    push    {r4-r7, lr}
    ldr     r0, =fb_path
    mov     r1, #O_WRONLY
    mov     r2, #0
    mov     r7, #SYS_OPEN
    svc     #0
    cmp     r0, #0
    blt     paint_done
    mov     r6, r0

    ldr     r4, =framebuffer
    ldr     r5, =25600
    ldr     r3, =0x07e0
paint_green:
    strh    r3, [r4], #2
    subs    r5, r5, #1
    bne     paint_green

    ldr     r5, =25600
    mov     r3, #0x1f
paint_blue:
    strh    r3, [r4], #2
    subs    r5, r5, #1
    bne     paint_blue

    ldr     r5, =25600
    ldr     r3, =0xffff
paint_white:
    strh    r3, [r4], #2
    subs    r5, r5, #1
    bne     paint_white

    mov     r0, r6
    ldr     r1, =framebuffer
    ldr     r2, =153600
    mov     r7, #SYS_WRITE
    svc     #0

    mov     r0, r6
    mov     r7, #SYS_CLOSE
    svc     #0
paint_done:
    pop     {r4-r7, pc}

restart:
    ldr     r0, =LINUX_REBOOT_MAGIC1
    ldr     r1, =LINUX_REBOOT_MAGIC2
    ldr     r2, =LINUX_REBOOT_CMD_RESTART
    mov     r3, #0
    mov     r7, #SYS_REBOOT
    svc     #0
1:
    b       1b

    .size _start, . - _start

    .section .rodata
startup_message:
    .ascii  "ROCKPOD N25 RAMDIAG: PID1 started; no storage drivers; display bands requested\n"
    .equ startup_message_len, . - startup_message
heartbeat_message:
    .ascii  "ROCKPOD N25 RAMDIAG HEARTBEAT; send r to reboot\n"
    .equ heartbeat_message_len, . - heartbeat_message
tty_path:
    .asciz  "/dev/ttyGS0"
fb_path:
    .asciz  "/dev/fb0"
    .align  2
one_second:
    .word   1
    .word   0

    .section .bss
    .align  2
command_byte:
    .space  1
    .align  5
framebuffer:
    .space  153600
