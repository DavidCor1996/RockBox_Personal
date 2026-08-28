/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Syscall-only PID 1 for the N81 volatile diagnostic initramfs.
 *
 * There is no libc, shell, storage discovery, filesystem helper, or writable
 * filesystem path.  PID 1 reports through the inherited kernel consoles,
 * paints three 32-bit colour bands through /dev/fb0, emits a timed heartbeat,
 * and asks the N81 reset path to restart after 30 seconds.
 */

    .syntax unified
    .arch armv7-a
    .arm

    .equ SYS_WRITE,       4
    .equ SYS_OPEN,        5
    .equ SYS_CLOSE,       6
    .equ SYS_REBOOT,     88
    .equ SYS_NANOSLEEP, 162

    .equ O_WRONLY,        1
    .equ LINUX_REBOOT_MAGIC1, 0xfee1dead
    .equ LINUX_REBOOT_MAGIC2, 0x28121969
    .equ LINUX_REBOOT_CMD_RESTART, 0x01234567
    .equ BAND_PIXELS,     204800
    .equ FRAME_BYTES,     2457600

    .section .text
    .global _start
    .type _start, %function
_start:
    mov     r0, #1
    ldr     r1, =startup_message
    ldr     r2, =startup_message_len
    mov     r7, #SYS_WRITE
    svc     #0

    bl      paint_pid1_bands

    mov     r4, #30
heartbeat_loop:
    mov     r0, #1
    ldr     r1, =heartbeat_message
    ldr     r2, =heartbeat_message_len
    mov     r7, #SYS_WRITE
    svc     #0
    bl      sleep_one_second
    subs    r4, r4, #1
    bne     heartbeat_loop

    mov     r0, #1
    ldr     r1, =reboot_message
    ldr     r2, =reboot_message_len
    mov     r7, #SYS_WRITE
    svc     #0

    ldr     r0, =LINUX_REBOOT_MAGIC1
    ldr     r1, =LINUX_REBOOT_MAGIC2
    ldr     r2, =LINUX_REBOOT_CMD_RESTART
    mov     r3, #0
    mov     r7, #SYS_REBOOT
    svc     #0

reboot_failed:
    mov     r0, #1
    ldr     r1, =reboot_failed_message
    ldr     r2, =reboot_failed_message_len
    mov     r7, #SYS_WRITE
    svc     #0
    bl      sleep_one_second
    b       reboot_failed

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
    ldr     r5, =BAND_PIXELS
    ldr     r3, =0x0024c875
paint_green:
    str     r3, [r4], #4
    subs    r5, r5, #1
    bne     paint_green

    ldr     r5, =BAND_PIXELS
    ldr     r3, =0x001b4fff
paint_blue:
    str     r3, [r4], #4
    subs    r5, r5, #1
    bne     paint_blue

    ldr     r5, =BAND_PIXELS
    ldr     r3, =0x00ffffff
paint_white:
    str     r3, [r4], #4
    subs    r5, r5, #1
    bne     paint_white

    mov     r0, r6
    ldr     r1, =framebuffer
    ldr     r2, =FRAME_BYTES
    mov     r7, #SYS_WRITE
    svc     #0

    mov     r0, r6
    mov     r7, #SYS_CLOSE
    svc     #0
paint_done:
    pop     {r4-r7, pc}

    .size _start, . - _start

    .section .rodata
startup_message:
    .ascii  "ROCKPOD N81 RAMDIAG: PID1 started; storage disabled; display bands requested\n"
    .equ startup_message_len, . - startup_message
heartbeat_message:
    .ascii  "ROCKPOD N81 RAMDIAG HEARTBEAT; automatic reboot pending\n"
    .equ heartbeat_message_len, . - heartbeat_message
reboot_message:
    .ascii  "ROCKPOD N81 RAMDIAG: requesting reboot now\n"
    .equ reboot_message_len, . - reboot_message
reboot_failed_message:
    .ascii  "ROCKPOD N81 RAMDIAG: reboot syscall returned; use hardware reset\n"
    .equ reboot_failed_message_len, . - reboot_failed_message
fb_path:
    .asciz  "/dev/fb0"
    .align  2
one_second:
    .word   1
    .word   0

    .section .bss
    .align  5
framebuffer:
    .space  FRAME_BYTES
