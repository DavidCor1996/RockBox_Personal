/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__\/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 by David Cormier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef __HIBERNATE_6G_H__
#define __HIBERNATE_6G_H__

/*
 * Stage 1 is deliberately compile-time gated.  A normal build reserves the
 * snapshot area and contains the record helpers, but follows the established
 * RetailOS/ONB hibernation path byte-for-byte at boot.
 */
#ifndef IPOD6G_HIBERNATE_STAGE1
#define IPOD6G_HIBERNATE_STAGE1 0
#endif

#ifndef IPOD6G_HIBERNATE_STAGE2
#define IPOD6G_HIBERNATE_STAGE2 0
#endif

#ifndef IPOD6G_HIBERNATE_STAGE3
#define IPOD6G_HIBERNATE_STAGE3 0
#endif

#if IPOD6G_HIBERNATE_STAGE2 && !IPOD6G_HIBERNATE_STAGE1
#error iPod 6G hibernate Stage 2 requires Stage 1
#endif

#if IPOD6G_HIBERNATE_STAGE3 && !IPOD6G_HIBERNATE_STAGE2
#error iPod 6G hibernate Stage 3 requires Stages 1 and 2
#endif

#define IPOD6G_HIBERNATE_AREA_ADDR          0x0bfec000
#define IPOD6G_HIBERNATE_AREA_SIZE          0x00010000
#define IPOD6G_HIBERNATE_CONTROL_ADDR       IPOD6G_HIBERNATE_AREA_ADDR
#define IPOD6G_HIBERNATE_CONTROL_SIZE       0x00001000
#define IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR   0x0bfed000
#define IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE   0x0000c000
#define IPOD6G_HIBERNATE_PROBE_ADDR         0x0bff9000
#define IPOD6G_HIBERNATE_PROBE_SIZE         0x00003000

/* Stage 2 uses the upper half of the control page as a dedicated stack. */
#define IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM \
        (IPOD6G_HIBERNATE_CONTROL_ADDR + 0x00000800)
#define IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP  \
        (IPOD6G_HIBERNATE_CONTROL_ADDR + IPOD6G_HIBERNATE_CONTROL_SIZE)
#define IPOD6G_HIBERNATE_PAYLOAD_STACK_SIZE 0x00000800
#define IPOD6G_HIBERNATE_PAYLOAD_MAX_SIZE   0x00001000

#define IPOD6G_HIBERNATE_PAYLOAD_COOKIE     0x48365032 /* "H6P2" */
#define IPOD6G_HIBERNATE_CONTEXT_COOKIE     0x48365033 /* "H6P3" */
#define IPOD6G_HIBERNATE_STACK_GUARD         0x5354414b /* "STAK" */
#define IPOD6G_HIBERNATE_APP_IRAM_TOP        0x0000c000

/*
 * Keep the ordinary power-off wake mask unchanged.  An explicitly armed
 * retained-context test additionally enables the PMU's dedicated USB and
 * adapter insertion inputs.  The PCF50633 manual identifies these five bits
 * as independent Standby wake enables; EXTON2 is also the iPod's observed
 * USB-VBUS input.
 */
#define IPOD6G_HIBERNATE_WAKE_MASK           0x000000c7

/*
 * OOCMODE bits 3:2 select EXTON2 behavior.  The PCF50633 manual defines
 * 01 as immediate wake on a rising edge; the R4 hardware capture instead
 * observed mode 10, where a rising edge merely starts an eight-second
 * shutdown timer.  Preserve every unrelated mode bit when correcting it.
 */
#define IPOD6G_HIBERNATE_EXTON2_MODE_MASK     0x0c
#define IPOD6G_HIBERNATE_EXTON2_MODE_RISING   0x04

/*
 * Stage 3 breadcrumbs deliberately occupy the existing reserved record tail.
 * Before the direct handoff they are raw post-reset evidence.  Once the live
 * application has restored its hardware, it commits the final breadcrumb and
 * record CRC itself.  Values identify the last boundary crossed if resume is
 * interrupted by a hard reset.
 */
#define IPOD6G_HIBERNATE_DIAGNOSTIC_OFFSET   268
#define IPOD6G_HIBERNATE_DIAG_NONE           0x00000000
#define IPOD6G_HIBERNATE_DIAG_BOOT_DIRECT    0x48334231 /* "H3B1" */
#define IPOD6G_HIBERNATE_DIAG_APP_CONTINUE   0x48334132 /* "H3A2" */
#define IPOD6G_HIBERNATE_DIAG_HW_RESTORED    0x48334133 /* "H3A3" */
#define IPOD6G_HIBERNATE_DIAG_I2C_RELEASED   0x48334134 /* "H3A4" */
#define IPOD6G_HIBERNATE_DIAG_CORE_IRQ_ARMED 0x48334135 /* "H3A5" */
#define IPOD6G_HIBERNATE_DIAG_CORE_IRQ_LIVE  0x48334136 /* "H3A6" */
#define IPOD6G_HIBERNATE_DIAG_DISPLAY_READY  0x48334137 /* "H3A7" */
#define IPOD6G_HIBERNATE_DIAG_ALL_IRQ_ARMED  0x48334138 /* "H3A8" */
#define IPOD6G_HIBERNATE_DIAG_APP_COMPLETE   0x48334139 /* "H3A9" */

#if IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP - \
        IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM != \
        IPOD6G_HIBERNATE_PAYLOAD_STACK_SIZE
#error The iPod 6G hibernate payload stack layout is inconsistent
#endif

#if IPOD6G_HIBERNATE_STAGE3
#define IPOD6G_HIBERNATE_TOKEN_VERSION      9
#define IPOD6G_HIBERNATE_RESUME_ABI         9
#define IPOD6G_HIBERNATE_RECORD_VERSION     9
#else
#define IPOD6G_HIBERNATE_TOKEN_VERSION      2
#define IPOD6G_HIBERNATE_RESUME_ABI         2
#define IPOD6G_HIBERNATE_RECORD_VERSION     2
#endif

/* Assembly-visible offsets in struct ipod6g_hibernate_cpu_context. */
#define IPOD6G_HIBERNATE_CPU_R4_OFFSET       0
#define IPOD6G_HIBERNATE_CPU_SP_OFFSET       32
#define IPOD6G_HIBERNATE_CPU_LR_OFFSET       36
#define IPOD6G_HIBERNATE_CPU_PC_OFFSET       40
#define IPOD6G_HIBERNATE_CPU_CPSR_OFFSET     44
#define IPOD6G_HIBERNATE_CPU_CONTROL_OFFSET  48
#define IPOD6G_HIBERNATE_CPU_TTB_OFFSET      52
#define IPOD6G_HIBERNATE_CPU_DOMAIN_OFFSET   56
#define IPOD6G_HIBERNATE_CPU_IRQ_SP_OFFSET   60
#define IPOD6G_HIBERNATE_CPU_FIQ_SP_OFFSET   64
#define IPOD6G_HIBERNATE_CPU_SVC_SP_OFFSET   68
#define IPOD6G_HIBERNATE_CPU_ABT_SP_OFFSET   72
#define IPOD6G_HIBERNATE_CPU_UND_SP_OFFSET   76

#if IPOD6G_HIBERNATE_CONTROL_SIZE + \
        IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE + \
        IPOD6G_HIBERNATE_PROBE_SIZE != IPOD6G_HIBERNATE_AREA_SIZE
#error The iPod 6G hibernate subregions do not fill the reserved area
#endif

#ifndef ASM

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum ipod6g_hibernate_token_state
{
    IPOD6G_HIBERNATE_TOKEN_EMPTY       = 0x00,
    IPOD6G_HIBERNATE_TOKEN_ARMED       = 0xa1,
    IPOD6G_HIBERNATE_TOKEN_RESUMING    = 0xa2,
    IPOD6G_HIBERNATE_TOKEN_PASSED      = 0xa3,
    IPOD6G_HIBERNATE_TOKEN_ENTRY_STALLED = 0xae,
    IPOD6G_HIBERNATE_TOKEN_FAILED      = 0xaf,
};

enum ipod6g_hibernate_record_state
{
    IPOD6G_HIBERNATE_RECORD_EMPTY      = 0,
    IPOD6G_HIBERNATE_RECORD_CAPABLE    = 1,
    IPOD6G_HIBERNATE_RECORD_PREPARED   = 2,
    IPOD6G_HIBERNATE_RECORD_ARMED      = 3,
    IPOD6G_HIBERNATE_RECORD_VALIDATING = 4,
    IPOD6G_HIBERNATE_RECORD_PASSED     = 5,
    IPOD6G_HIBERNATE_RECORD_FAILED     = 6,
    IPOD6G_HIBERNATE_RECORD_PAYLOAD_ENTERING = 7,
    IPOD6G_HIBERNATE_RECORD_PAYLOAD_RETURNED = 8,
    IPOD6G_HIBERNATE_RECORD_CONTEXT_ENTERING = 9,
    IPOD6G_HIBERNATE_RECORD_CONTEXT_RETURNED = 10,
};

enum ipod6g_hibernate_mode
{
    IPOD6G_HIBERNATE_MODE_NONE               = 0,
    IPOD6G_HIBERNATE_MODE_RETENTION          = 1,
    IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD = 2,
    IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT = 3,
};

enum ipod6g_hibernate_capability
{
    IPOD6G_HIBERNATE_CAP_RETENTION          = 1u << 0,
    IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD = 1u << 1,
    IPOD6G_HIBERNATE_CAP_CONTROLLED_CONTEXT = 1u << 2,
};

enum ipod6g_hibernate_phase
{
    IPOD6G_HIBERNATE_PHASE_NONE          = 0,
    IPOD6G_HIBERNATE_PHASE_RECORD_READY  = 1,
    IPOD6G_HIBERNATE_PHASE_TOKEN_ARMED   = 2,
    IPOD6G_HIBERNATE_PHASE_ENTRY_READY   = 3,
    IPOD6G_HIBERNATE_PHASE_BOOT_CLAIMED  = 4,
    IPOD6G_HIBERNATE_PHASE_MIU_RESTORED  = 5,
    IPOD6G_HIBERNATE_PHASE_DATA_VERIFIED = 6,
    IPOD6G_HIBERNATE_PHASE_PAYLOAD_ENTERED  = 7,
    IPOD6G_HIBERNATE_PHASE_PAYLOAD_RETURNED = 8,
    IPOD6G_HIBERNATE_PHASE_CONTEXT_SAVED    = 9,
    IPOD6G_HIBERNATE_PHASE_IRAM_RESTORED    = 10,
    IPOD6G_HIBERNATE_PHASE_CONTEXT_ENTERED  = 11,
    IPOD6G_HIBERNATE_PHASE_CONTEXT_RETURNED = 12,
    IPOD6G_HIBERNATE_PHASE_HARDWARE_RESTORED = 13,
    IPOD6G_HIBERNATE_PHASE_I2C_RELEASED       = 14,
    IPOD6G_HIBERNATE_PHASE_CORE_IRQ_ARMED     = 15,
    IPOD6G_HIBERNATE_PHASE_CORE_IRQ_LIVE      = 16,
    IPOD6G_HIBERNATE_PHASE_DISPLAY_RESTORED   = 17,
    IPOD6G_HIBERNATE_PHASE_ALL_IRQ_ARMED      = 18,
    IPOD6G_HIBERNATE_PHASE_RESUME_COMPLETE    = 19,
};

enum ipod6g_hibernate_failure
{
    IPOD6G_HIBERNATE_FAILURE_NONE          = 0,
    IPOD6G_HIBERNATE_FAILURE_TOKEN_IO      = 1,
    IPOD6G_HIBERNATE_FAILURE_TOKEN_INVALID = 2,
    IPOD6G_HIBERNATE_FAILURE_RECORD        = 3,
    IPOD6G_HIBERNATE_FAILURE_IRAM_CRC      = 4,
    IPOD6G_HIBERNATE_FAILURE_PROBE_CRC     = 5,
    IPOD6G_HIBERNATE_FAILURE_I2C_PREFLIGHT = 6,
    IPOD6G_HIBERNATE_FAILURE_PAYLOAD_METADATA = 7,
    IPOD6G_HIBERNATE_FAILURE_PAYLOAD_CRC      = 8,
    IPOD6G_HIBERNATE_FAILURE_PAYLOAD_RETURN   = 9,
    IPOD6G_HIBERNATE_FAILURE_PAYLOAD_STACK    = 10,
    IPOD6G_HIBERNATE_FAILURE_TTB_CRC          = 11,
    IPOD6G_HIBERNATE_FAILURE_CONTEXT_METADATA = 12,
    IPOD6G_HIBERNATE_FAILURE_IRAM_RESTORE     = 13,
    IPOD6G_HIBERNATE_FAILURE_CONTEXT_RETURN   = 14,
    IPOD6G_HIBERNATE_FAILURE_CONTEXT_INTERRUPTED = 15,
    IPOD6G_HIBERNATE_FAILURE_STANDBY_NOT_ENTERED = 16,
    IPOD6G_HIBERNATE_FAILURE_HARDWARE_RESUME = 17,
};

struct ipod6g_hibernate_pmu_snapshot
{
    uint32_t wake_reason;
    uint32_t control;
    uint32_t interrupts;
    uint32_t power;
};

struct ipod6g_hibernate_status
{
    bool valid;
    uint32_t state;
    uint32_t sequence;
    uint32_t attempt_count;
    uint32_t capabilities;
    uint32_t mode;
    uint32_t last_phase;
    uint32_t failure;
    uint32_t observed_wake_reason;
    uint32_t pmu_control;
    uint32_t pmu_interrupts;
    uint32_t pmu_power;
    uint32_t pmu_entry_before;
    uint32_t pmu_entry_after;
    uint32_t payload_expected_cookie;
    uint32_t payload_observed_cookie;
    uint32_t payload_observed_sp;
    uint32_t payload_return_value;
    uint32_t context_pc;
    uint32_t context_sp;
    uint32_t context_cpsr;
    uint32_t context_ttb_crc32;
    uint32_t context_observed_ttb_crc32;
    uint32_t diagnostic_breadcrumb;
    uint32_t vic0_enable;
    uint32_t vic1_enable;
    uint32_t vic0_raw_before_core;
    uint32_t vic1_raw_before_core;
    uint32_t vic0_raw_before_full;
    uint32_t vic1_raw_before_full;
};

struct ipod6g_hibernate_token
{
    uint8_t magic[4];
    uint8_t version;
    uint8_t state;
    uint8_t resume_abi;
    uint8_t crc8;
};

struct ipod6g_hibernate_cpu_context
{
    uint32_t r4_r11[8];
    uint32_t sp;
    uint32_t lr;
    uint32_t pc;
    uint32_t cpsr;
    uint32_t cp15_control;
    uint32_t cp15_ttb;
    uint32_t cp15_domain;
    uint32_t irq_sp;
    uint32_t fiq_sp;
    uint32_t svc_sp;
    uint32_t abt_sp;
    uint32_t und_sp;
};

struct ipod6g_hibernate_irq_context
{
    uint32_t vic0_select;
    uint32_t vic1_select;
    uint32_t vic0_enable;
    uint32_t vic1_enable;
};

struct ipod6g_hibernate_record
{
    uint8_t magic[8];
    uint32_t version;
    uint32_t record_size;
    uint32_t target_id;
    uint32_t resume_abi;
    uint32_t state;
    uint32_t sequence;
    uint32_t attempt_count;
    uint32_t capabilities;
    uint32_t mode;
    uint32_t build_fingerprint[4];
    struct ipod6g_hibernate_cpu_context cpu;
    struct ipod6g_hibernate_irq_context irq;
    uint32_t area_addr;
    uint32_t area_size;
    uint32_t iram_shadow_addr;
    uint32_t iram_shadow_size;
    uint32_t iram_shadow_crc32;
    uint32_t probe_addr;
    uint32_t probe_size;
    uint32_t probe_seed;
    uint32_t probe_crc32;
    uint32_t requested_wake_mask;
    uint32_t observed_wake_reason;
    uint32_t payload_start;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t payload_entry;
    uint32_t payload_stack_bottom;
    uint32_t payload_stack_top;
    uint32_t payload_expected_cookie;
    uint32_t payload_observed_cookie;
    uint32_t payload_observed_sp;
    uint32_t payload_return_value;
#if IPOD6G_HIBERNATE_STAGE3
    uint32_t ttb_addr;
    uint32_t ttb_size;
    uint32_t ttb_crc32;
    uint32_t observed_ttb_crc32;
#endif
    uint32_t last_phase;
    uint32_t failure;
    uint32_t record_crc32;
#if IPOD6G_HIBERNATE_STAGE3
    uint32_t reserved[4];
    uint32_t pmu_entry_before;
    uint32_t pmu_entry_after;
    uint32_t vic0_raw_before_core;
    uint32_t vic1_raw_before_core;
    uint32_t vic0_raw_before_full;
    uint32_t vic1_raw_before_full;
#else
    uint32_t reserved[8];
#endif
};

bool ipod6g_hibernate_token_read(struct ipod6g_hibernate_token *token);
bool ipod6g_hibernate_token_owned(
        const struct ipod6g_hibernate_token *token);
bool ipod6g_hibernate_token_valid(
        const struct ipod6g_hibernate_token *token);
int ipod6g_hibernate_token_arm(enum ipod6g_hibernate_token_state state);
int ipod6g_hibernate_token_set_state(
        enum ipod6g_hibernate_token_state state);
int ipod6g_hibernate_token_clear(void);

bool ipod6g_hibernate_record_valid(
        const volatile struct ipod6g_hibernate_record *record);

#ifndef BOOTLOADER
/* Stage 1 remains an explicit, one-shot retention test, not UI resume. */
bool ipod6g_hibernate_stage1_get_status(
        struct ipod6g_hibernate_status *status);
bool ipod6g_hibernate_stage1_request(uint32_t sequence);
bool ipod6g_hibernate_stage2_request(uint32_t sequence);
bool ipod6g_hibernate_stage3_request(uint32_t sequence);
bool ipod6g_hibernate_consume_request(uint32_t *sequence, uint32_t *mode);
bool ipod6g_hibernate_prepare(uint32_t sequence, uint32_t mode);
bool ipod6g_hibernate_stage1_i2c_preflight(void);
bool ipod6g_hibernate_stage1_arm(void);
void ipod6g_hibernate_stage1_fail(
        enum ipod6g_hibernate_failure failure);
void ipod6g_hibernate_stage1_enter(void)
        __attribute__((noreturn));
bool ipod6g_hibernate_stage3_enter(void);
bool ipod6g_hibernate_stage3_suspend(uint32_t sequence);
#else
enum ipod6g_hibernate_boot_action
{
    IPOD6G_HIBERNATE_BOOT_RETAIL = 0,
    IPOD6G_HIBERNATE_BOOT_ROCKBOX,
    IPOD6G_HIBERNATE_BOOT_STANDBY_STALLED,
    IPOD6G_HIBERNATE_BOOT_RECOVER,
    IPOD6G_HIBERNATE_BOOT_TOKEN_IO_ERROR,
};

enum ipod6g_hibernate_boot_action ipod6g_hibernate_boot_action(void);
void ipod6g_hibernate_capture_pmu_snapshot(
        struct ipod6g_hibernate_pmu_snapshot *snapshot);
bool ipod6g_hibernate_validate_after_wake(
        const struct ipod6g_hibernate_pmu_snapshot *snapshot);
#if IPOD6G_HIBERNATE_STAGE2
uint32_t ipod6g_hibernate_stage2_call(
        uintptr_t entry, uintptr_t stack_top,
        volatile struct ipod6g_hibernate_record *record);
#endif
#if IPOD6G_HIBERNATE_STAGE3
void ipod6g_hibernate_stage3_resume(
        const struct ipod6g_hibernate_cpu_context *context)
        __attribute__((noreturn));
#endif
void ipod6g_hibernate_stage1_mark_recovery(
        enum ipod6g_hibernate_failure failure,
        const struct ipod6g_hibernate_pmu_snapshot *snapshot);
void ipod6g_hibernate_clear_stale_token(void);
void ipod6g_hibernate_stage1_publish_capability(void);
#endif /* BOOTLOADER */

typedef char ipod6g_hibernate_token_must_be_eight_bytes
        [(sizeof(struct ipod6g_hibernate_token) == 8) ? 1 : -1];
typedef char ipod6g_hibernate_record_must_fit_control_page
        [(sizeof(struct ipod6g_hibernate_record) <=
          IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM -
          IPOD6G_HIBERNATE_CONTROL_ADDR) ? 1 : -1];
typedef char ipod6g_hibernate_cpu_sp_offset_must_match
        [(offsetof(struct ipod6g_hibernate_cpu_context, sp) ==
          IPOD6G_HIBERNATE_CPU_SP_OFFSET) ? 1 : -1];
typedef char ipod6g_hibernate_cpu_pc_offset_must_match
        [(offsetof(struct ipod6g_hibernate_cpu_context, pc) ==
          IPOD6G_HIBERNATE_CPU_PC_OFFSET) ? 1 : -1];
typedef char ipod6g_hibernate_cpu_domain_offset_must_match
        [(offsetof(struct ipod6g_hibernate_cpu_context, cp15_domain) ==
          IPOD6G_HIBERNATE_CPU_DOMAIN_OFFSET) ? 1 : -1];
typedef char ipod6g_hibernate_cpu_irq_sp_offset_must_match
        [(offsetof(struct ipod6g_hibernate_cpu_context, irq_sp) ==
          IPOD6G_HIBERNATE_CPU_IRQ_SP_OFFSET) ? 1 : -1];
typedef char ipod6g_hibernate_cpu_und_sp_offset_must_match
        [(offsetof(struct ipod6g_hibernate_cpu_context, und_sp) ==
          IPOD6G_HIBERNATE_CPU_UND_SP_OFFSET) ? 1 : -1];
#if IPOD6G_HIBERNATE_STAGE3
typedef char ipod6g_hibernate_diagnostic_offset_must_match
        [(offsetof(struct ipod6g_hibernate_record, reserved[0]) ==
          IPOD6G_HIBERNATE_DIAGNOSTIC_OFFSET) ? 1 : -1];
#endif

#endif /* !ASM */
#endif /* __HIBERNATE_6G_H__ */
