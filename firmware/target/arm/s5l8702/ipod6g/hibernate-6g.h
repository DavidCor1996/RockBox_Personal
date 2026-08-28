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

#define IPOD6G_HIBERNATE_AREA_ADDR          0x0bfec000
#define IPOD6G_HIBERNATE_AREA_SIZE          0x00010000
#define IPOD6G_HIBERNATE_CONTROL_ADDR       IPOD6G_HIBERNATE_AREA_ADDR
#define IPOD6G_HIBERNATE_CONTROL_SIZE       0x00001000
#define IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR   0x0bfed000
#define IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE   0x0000c000
#define IPOD6G_HIBERNATE_PROBE_ADDR         0x0bff9000
#define IPOD6G_HIBERNATE_PROBE_SIZE         0x00003000

#define IPOD6G_HIBERNATE_TOKEN_VERSION      1
#define IPOD6G_HIBERNATE_RESUME_ABI         1
#define IPOD6G_HIBERNATE_RECORD_VERSION     1

#if IPOD6G_HIBERNATE_CONTROL_SIZE + \
        IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE + \
        IPOD6G_HIBERNATE_PROBE_SIZE != IPOD6G_HIBERNATE_AREA_SIZE
#error The iPod 6G hibernate subregions do not fill the reserved area
#endif

#ifndef ASM

#include <stdbool.h>
#include <stdint.h>

enum ipod6g_hibernate_token_state
{
    IPOD6G_HIBERNATE_TOKEN_EMPTY       = 0x00,
    IPOD6G_HIBERNATE_TOKEN_ARMED       = 0xa1,
    IPOD6G_HIBERNATE_TOKEN_RESUMING    = 0xa2,
    IPOD6G_HIBERNATE_TOKEN_PASSED      = 0xa3,
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
};

struct ipod6g_hibernate_status
{
    bool valid;
    uint32_t state;
    uint32_t sequence;
    uint32_t attempt_count;
    uint32_t last_phase;
    uint32_t failure;
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
    uint32_t build_fingerprint[4];
    struct ipod6g_hibernate_cpu_context cpu;
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
    uint32_t last_phase;
    uint32_t failure;
    uint32_t record_crc32;
    uint32_t reserved[8];
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
bool ipod6g_hibernate_stage1_consume_request(uint32_t *sequence);
bool ipod6g_hibernate_stage1_prepare(uint32_t sequence);
bool ipod6g_hibernate_stage1_i2c_preflight(void);
bool ipod6g_hibernate_stage1_arm(void);
void ipod6g_hibernate_stage1_fail(
        enum ipod6g_hibernate_failure failure);
void ipod6g_hibernate_stage1_enter(void)
        __attribute__((noreturn));
#else
enum ipod6g_hibernate_boot_action
{
    IPOD6G_HIBERNATE_BOOT_RETAIL = 0,
    IPOD6G_HIBERNATE_BOOT_STAGE1,
    IPOD6G_HIBERNATE_BOOT_RECOVER,
    IPOD6G_HIBERNATE_BOOT_TOKEN_IO_ERROR,
};

enum ipod6g_hibernate_boot_action ipod6g_hibernate_boot_action(void);
bool ipod6g_hibernate_stage1_validate_after_wake(void);
void ipod6g_hibernate_stage1_mark_recovery(
        enum ipod6g_hibernate_failure failure);
void ipod6g_hibernate_clear_stale_token(void);
void ipod6g_hibernate_stage1_publish_capability(void);
#endif /* BOOTLOADER */

typedef char ipod6g_hibernate_token_must_be_eight_bytes
        [(sizeof(struct ipod6g_hibernate_token) == 8) ? 1 : -1];
typedef char ipod6g_hibernate_record_must_fit_control_page
        [(sizeof(struct ipod6g_hibernate_record) <=
          IPOD6G_HIBERNATE_CONTROL_SIZE) ? 1 : -1];

#endif /* !ASM */
#endif /* __HIBERNATE_6G_H__ */
