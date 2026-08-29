/***************************************************************************
 * iPod Classic 6G retained-payload handoff
 *
 * Copyright (C) 2026 by David Cormier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 ****************************************************************************/

#define ASM
#include "config.h"
#include "hibernate-6g.h"

#if defined(IPOD_6G) && !defined(BOOTLOADER) && \
        IPOD6G_HIBERNATE_STAGE3
    .section .hibernate_payload, "ax", %progbits
    .align  2
    .global ipod6g_hibernate_stage3_checkpoint
    .type   ipod6g_hibernate_stage3_checkpoint, %function

/*
 * uint32_t ipod6g_hibernate_stage3_checkpoint(struct cpu_context *context)
 *
 * This is a setjmp-style boundary. Save the live system-mode frame, every
 * exception-mode stack that the bootloader CRT replaces, and the MMU
 * contract. Return zero while entering standby. The bootloader later branches
 * to the local continuation label and it returns one to the original caller.
 */
ipod6g_hibernate_stage3_checkpoint:
    mrs     r2, cpsr
    orr     r3, r2, #0xc0
    msr     cpsr_c, r3

    stmia   r0, {r4-r11}
    str     sp, [r0, #IPOD6G_HIBERNATE_CPU_SP_OFFSET]
    str     lr, [r0, #IPOD6G_HIBERNATE_CPU_LR_OFFSET]
    adr     r3, .Lstage3_app_continue
    str     r3, [r0, #IPOD6G_HIBERNATE_CPU_PC_OFFSET]

    str     r2, [r0, #IPOD6G_HIBERNATE_CPU_CPSR_OFFSET]
    mrc     p15, 0, r3, c1, c0, 0
    str     r3, [r0, #IPOD6G_HIBERNATE_CPU_CONTROL_OFFSET]
    mrc     p15, 0, r3, c2, c0, 0
    str     r3, [r0, #IPOD6G_HIBERNATE_CPU_TTB_OFFSET]
    mrc     p15, 0, r3, c3, c0, 0
    str     r3, [r0, #IPOD6G_HIBERNATE_CPU_DOMAIN_OFFSET]

    msr     cpsr_c, #0xd2
    str     sp, [r0, #IPOD6G_HIBERNATE_CPU_IRQ_SP_OFFSET]
    msr     cpsr_c, #0xd1
    str     sp, [r0, #IPOD6G_HIBERNATE_CPU_FIQ_SP_OFFSET]
    msr     cpsr_c, #0xd3
    str     sp, [r0, #IPOD6G_HIBERNATE_CPU_SVC_SP_OFFSET]
    msr     cpsr_c, #0xd7
    str     sp, [r0, #IPOD6G_HIBERNATE_CPU_ABT_SP_OFFSET]
    msr     cpsr_c, #0xdb
    str     sp, [r0, #IPOD6G_HIBERNATE_CPU_UND_SP_OFFSET]
    msr     cpsr_c, #0xdf

    mov     r0, #0
    bx      lr

.Lstage3_app_continue:
    mov     r0, #1
    bx      lr

    .size ipod6g_hibernate_stage3_checkpoint, \
          .-ipod6g_hibernate_stage3_checkpoint
#endif

#if defined(IPOD_6G) && defined(BOOTLOADER) && IPOD6G_HIBERNATE_STAGE2
    .section .icode.ipod6g_hibernate_stage2_call, "ax", %progbits
    .align  2
    .global ipod6g_hibernate_stage2_call
    .type   ipod6g_hibernate_stage2_call, %function

/*
 * uint32_t ipod6g_hibernate_stage2_call(uintptr_t entry,
 *                                      uintptr_t stack_top,
 *                                      volatile struct record *record)
 *
 * The bootloader stack lives in IRAM1. Preserve it in callee-saved r4, move
 * onto the fixed retained-DRAM stack, and call exactly one ARM payload. The
 * payload follows AAPCS and therefore preserves r4-r11 before returning.
 */
ipod6g_hibernate_stage2_call:
    stmfd   sp!, {r4-r8, lr}
    mov     r4, sp
    mov     r5, r0
    mov     r6, r1
    mov     r0, r2
    mov     r1, r6
    mov     sp, r6
    blx     r5
    mov     sp, r4
    ldmfd   sp!, {r4-r8, pc}

    .size ipod6g_hibernate_stage2_call, \
          .-ipod6g_hibernate_stage2_call
#endif

#if defined(IPOD_6G) && defined(BOOTLOADER) && \
        IPOD6G_HIBERNATE_STAGE3
    .section .icode.ipod6g_hibernate_stage3_resume, "ax", %progbits
    .align  2
    .global ipod6g_hibernate_stage3_resume
    .type   ipod6g_hibernate_stage3_resume, %function

/*
 * void ipod6g_hibernate_stage3_resume(const struct cpu_context *context)
 *
 * Non-returning stock-style handoff. Restore the application exception stacks
 * and system frame, then branch directly to the CRC-validated setjmp
 * continuation. There is deliberately no bootloader return frame.
 */
ipod6g_hibernate_stage3_resume:
    mov     r3, r0

    msr     cpsr_c, #0xd2
    ldr     sp, [r3, #IPOD6G_HIBERNATE_CPU_IRQ_SP_OFFSET]
    msr     cpsr_c, #0xd1
    ldr     sp, [r3, #IPOD6G_HIBERNATE_CPU_FIQ_SP_OFFSET]
    msr     cpsr_c, #0xd3
    ldr     sp, [r3, #IPOD6G_HIBERNATE_CPU_SVC_SP_OFFSET]
    msr     cpsr_c, #0xd7
    ldr     sp, [r3, #IPOD6G_HIBERNATE_CPU_ABT_SP_OFFSET]
    msr     cpsr_c, #0xdb
    ldr     sp, [r3, #IPOD6G_HIBERNATE_CPU_UND_SP_OFFSET]

    /* Enter system mode with IRQ/FIQ masked until C restores all hardware. */
    msr     cpsr_c, #0xdf
    ldr     sp, [r3, #IPOD6G_HIBERNATE_CPU_SP_OFFSET]
    ldr     lr, [r3, #IPOD6G_HIBERNATE_CPU_LR_OFFSET]
    ldr     r2, [r3, #IPOD6G_HIBERNATE_CPU_PC_OFFSET]
    ldmia   r3, {r4-r11}
    mov     r0, #1
    bx      r2

    .size ipod6g_hibernate_stage3_resume, \
          .-ipod6g_hibernate_stage3_resume
#endif
