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
 * uint32_t ipod6g_hibernate_stage3_checkpoint(struct cpu_context *context,
 *                                             uintptr_t resume_pc)
 *
 * The caller has already disabled IRQ and FIQ. Save the non-volatile ARM
 * context plus the MMU contract, then return zero on the initial path. The
 * bootloader does not return here: it restores this context and branches to
 * the separately supplied, CRC-covered resume_pc on the original app stack.
 */
ipod6g_hibernate_stage3_checkpoint:
    stmia   r0, {r4-r11}
    str     sp, [r0, #IPOD6G_HIBERNATE_CPU_SP_OFFSET]
    str     lr, [r0, #IPOD6G_HIBERNATE_CPU_LR_OFFSET]
    str     r1, [r0, #IPOD6G_HIBERNATE_CPU_PC_OFFSET]

    mrs     r2, cpsr
    str     r2, [r0, #IPOD6G_HIBERNATE_CPU_CPSR_OFFSET]
    mrc     p15, 0, r2, c1, c0, 0
    str     r2, [r0, #IPOD6G_HIBERNATE_CPU_CONTROL_OFFSET]
    mrc     p15, 0, r2, c2, c0, 0
    str     r2, [r0, #IPOD6G_HIBERNATE_CPU_TTB_OFFSET]
    mrc     p15, 0, r2, c3, c0, 0
    str     r2, [r0, #IPOD6G_HIBERNATE_CPU_DOMAIN_OFFSET]

    mov     r0, #0
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
    .global ipod6g_hibernate_stage3_resume_call
    .type   ipod6g_hibernate_stage3_resume_call, %function

/*
 * uint32_t ipod6g_hibernate_stage3_resume_call(
 *        const struct cpu_context *context, struct record *record,
 *        uint32_t cookie, uintptr_t return_entry)
 *
 * Save a resumable bootloader frame in IRAM1, restore the Rockbox system-mode
 * stack and non-volatile context from the retained record, and branch to the
 * CRC-validated application continuation. Four volatile registers carry the
 * round-trip contract into that continuation:
 *
 *   r0 = record, r1 = cookie, r2 = saved bootloader SP, r3 = return entry.
 */
ipod6g_hibernate_stage3_resume_call:
    /* Ten saved words preserve the bootloader's 8-byte stack alignment. */
    stmfd   sp!, {r4-r12, lr}
    mov     r12, r0
    mov     r0, r1
    mov     r1, r2
    mov     r2, sp

    ldr     lr, [r12, #IPOD6G_HIBERNATE_CPU_CPSR_OFFSET]
    msr     cpsr_cxsf, lr
    ldr     sp, [r12, #IPOD6G_HIBERNATE_CPU_SP_OFFSET]
    ldr     lr, [r12, #IPOD6G_HIBERNATE_CPU_LR_OFFSET]
    ldmia   r12, {r4-r11}
    ldr     r12, [r12, #IPOD6G_HIBERNATE_CPU_PC_OFFSET]
    bx      r12

    .size ipod6g_hibernate_stage3_resume_call, \
          .-ipod6g_hibernate_stage3_resume_call

    .align  2
    .global ipod6g_hibernate_stage3_return
    .type   ipod6g_hibernate_stage3_return, %function

/* Called exactly once by the retained app continuation. */
ipod6g_hibernate_stage3_return:
    msr     cpsr_c, #0xd3
    mov     sp, r2
    mov     r0, r1
    ldmfd   sp!, {r4-r12, pc}

    .size ipod6g_hibernate_stage3_return, \
          .-ipod6g_hibernate_stage3_return
#endif
