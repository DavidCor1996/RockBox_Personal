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
