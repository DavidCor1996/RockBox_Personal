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

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include "system.h"
#ifndef BOOTLOADER
#include "kernel.h"
#endif
#include "s5l87xx.h"
#include "pmu-target.h"
#include "hibernate-6g.h"
#if IPOD6G_HIBERNATE_STAGE3
#include "version.h"
#ifndef BOOTLOADER
#include "audio.h"
#include "backlight-target.h"
#include "lcd.h"
#include "lcd-s5l8702.h"
#include "pcm.h"
#include "power.h"
#include "powermgmt.h"
#include "storage.h"
#include "usb.h"
#include "videoout-6g.h"
#include "gpio-s5l8702.h"
#include "i2c-s5l8702.h"
#include "uart-target.h"
#endif
#endif

#define IPOD6G_HIBERNATE_PROBE_SEED 0x68364731u /* "h6G1" */

#if IPOD6G_HIBERNATE_STAGE3
#define IPOD6G_HIBERNATE_COMPILED_CAPABILITIES \
        (IPOD6G_HIBERNATE_CAP_RETENTION | \
         IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD | \
         IPOD6G_HIBERNATE_CAP_CONTROLLED_CONTEXT)
#elif IPOD6G_HIBERNATE_STAGE2
#define IPOD6G_HIBERNATE_COMPILED_CAPABILITIES \
        (IPOD6G_HIBERNATE_CAP_RETENTION | \
         IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD)
#else
#define IPOD6G_HIBERNATE_COMPILED_CAPABILITIES \
        IPOD6G_HIBERNATE_CAP_RETENTION
#endif

static const uint8_t token_magic[4] = { 'R', 'B', 'H', '6' };
static const uint8_t record_magic[8] =
        { 'R', 'B', 'H', '6', 'R', 'E', 'C', '1' };

static int token_read_bytes(uint8_t *bytes)
{
#ifdef BOOTLOADER
    return pmu_rd_multiple(PCF5063X_REG_MEMBYTE0, 8, bytes);
#else
    return pmu_read_multiple(PCF5063X_REG_MEMBYTE0, 8, bytes);
#endif
}

static int token_write_bytes(int offset, int count, uint8_t *bytes)
{
#ifdef BOOTLOADER
    return pmu_wr_multiple(PCF5063X_REG_MEMBYTE0 + offset, count, bytes);
#else
    return pmu_write_multiple(PCF5063X_REG_MEMBYTE0 + offset, count, bytes);
#endif
}

#if IPOD6G_HIBERNATE_STAGE3
static uint8_t token_crc8(const uint8_t *bytes) ICODE_ATTR;
#else
static uint8_t token_crc8(const uint8_t *bytes);
#endif
static uint8_t token_crc8(const uint8_t *bytes)
{
    uint8_t crc = 0;

    for (int i = 0; i < 7; i++)
    {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
    }

    return crc;
}

static bool token_state_valid(uint8_t state)
{
    return state == IPOD6G_HIBERNATE_TOKEN_ARMED ||
           state == IPOD6G_HIBERNATE_TOKEN_RESUMING ||
           state == IPOD6G_HIBERNATE_TOKEN_PASSED ||
#if IPOD6G_HIBERNATE_STAGE3
           state == IPOD6G_HIBERNATE_TOKEN_ENTRY_STALLED ||
#endif
           state == IPOD6G_HIBERNATE_TOKEN_FAILED;
}

static void token_init(struct ipod6g_hibernate_token *token,
                       enum ipod6g_hibernate_token_state state)
{
    for (int i = 0; i < 4; i++)
        token->magic[i] = token_magic[i];

    token->version = IPOD6G_HIBERNATE_TOKEN_VERSION;
    token->state = state;
    token->resume_abi = IPOD6G_HIBERNATE_RESUME_ABI;
    token->crc8 = token_crc8((const uint8_t *)token);
}

bool ipod6g_hibernate_token_read(struct ipod6g_hibernate_token *token)
{
    uint8_t *bytes = (uint8_t *)token;

    for (int i = 0; i < 8; i++)
        bytes[i] = 0;

    return token_read_bytes(bytes) == 0;
}

bool ipod6g_hibernate_token_owned(
        const struct ipod6g_hibernate_token *token)
{
    for (int i = 0; i < 4; i++)
    {
        if (token->magic[i] != token_magic[i])
            return false;
    }

    return true;
}

bool ipod6g_hibernate_token_valid(
        const struct ipod6g_hibernate_token *token)
{
    return ipod6g_hibernate_token_owned(token) &&
           token->version == IPOD6G_HIBERNATE_TOKEN_VERSION &&
           token->resume_abi == IPOD6G_HIBERNATE_RESUME_ABI &&
           token_state_valid(token->state) &&
           token->crc8 == token_crc8((const uint8_t *)token);
}

int ipod6g_hibernate_token_clear(void)
{
    uint8_t clear_magic[4] = { 0, 0, 0, 0 };
    return token_write_bytes(0, 4, clear_magic);
}

int ipod6g_hibernate_token_arm(enum ipod6g_hibernate_token_state state)
{
    struct ipod6g_hibernate_token token;
    struct ipod6g_hibernate_token verify;
    uint8_t *bytes = (uint8_t *)&token;
    int rc;

    token_init(&token, state);

    /* Invalidate old ownership, write payload, then publish magic last. */
    rc = ipod6g_hibernate_token_clear();
    if (rc == 0)
        rc = token_write_bytes(4, 4, bytes + 4);
    if (rc == 0)
        rc = token_write_bytes(0, 4, bytes);
    if (rc != 0)
        return rc;

    if (!ipod6g_hibernate_token_read(&verify) ||
        !ipod6g_hibernate_token_valid(&verify) ||
        verify.state != (uint8_t)state)
    {
        ipod6g_hibernate_token_clear();
        return -1;
    }

    return 0;
}

int ipod6g_hibernate_token_set_state(
        enum ipod6g_hibernate_token_state state)
{
    struct ipod6g_hibernate_token token;
    struct ipod6g_hibernate_token verify;
    uint8_t *bytes = (uint8_t *)&token;
    int rc;

    if (!ipod6g_hibernate_token_read(&token) ||
        !ipod6g_hibernate_token_valid(&token))
    {
        return -1;
    }

    token.state = state;
    token.crc8 = token_crc8(bytes);

    /* Magic remains present: an interrupted update is still Rockbox-owned. */
    rc = token_write_bytes(4, 4, bytes + 4);
    if (rc != 0)
        return rc;

    if (!ipod6g_hibernate_token_read(&verify) ||
        !ipod6g_hibernate_token_valid(&verify) ||
        verify.state != (uint8_t)state)
    {
        return -1;
    }

    return 0;
}

static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (int bit = 0; bit < 8; bit++)
        crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);

    return crc;
}

#if defined(BOOTLOADER) || IPOD6G_HIBERNATE_STAGE1
static uint32_t crc32_region(uintptr_t address, uint32_t size)
{
    const volatile uint8_t *data = (const volatile uint8_t *)address;
    uint32_t crc = 0xffffffffu;

    for (uint32_t i = 0; i < size; i++)
        crc = crc32_byte(crc, data[i]);

    return ~crc;
}
#endif

#if IPOD6G_HIBERNATE_STAGE3
static uint32_t build_version_crc32(void)
{
    const uint8_t *data = (const uint8_t *)rbversion;
    uint32_t crc = 0xffffffffu;

    while (*data != '\0')
        crc = crc32_byte(crc, *data++);

    return ~crc;
}
#endif

static uint32_t record_crc32(
        const volatile struct ipod6g_hibernate_record *record)
{
    const volatile uint8_t *data = (const volatile uint8_t *)record;
    const size_t crc_offset = offsetof(struct ipod6g_hibernate_record,
                                       record_crc32);
    uint32_t crc = 0xffffffffu;

    for (size_t i = 0; i < sizeof(*record); i++)
    {
        uint8_t byte = data[i];
        if (i >= crc_offset && i < crc_offset + sizeof(record->record_crc32))
            byte = 0;
        crc = crc32_byte(crc, byte);
    }

    return ~crc;
}

static bool record_magic_valid(
        const volatile struct ipod6g_hibernate_record *record)
{
    for (int i = 0; i < 8; i++)
    {
        if (record->magic[i] != record_magic[i])
            return false;
    }

    return true;
}

static bool record_layout_valid(
        const volatile struct ipod6g_hibernate_record *record)
{
#if IPOD6G_HIBERNATE_STAGE3
    const uint32_t allowed_capabilities =
            IPOD6G_HIBERNATE_CAP_RETENTION |
            IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD |
            IPOD6G_HIBERNATE_CAP_CONTROLLED_CONTEXT;
    const uint32_t maximum_mode =
            IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT;
#else
    const uint32_t allowed_capabilities =
            IPOD6G_HIBERNATE_CAP_RETENTION |
            IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD;
    const uint32_t maximum_mode =
            IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD;
#endif
#if IPOD6G_HIBERNATE_STAGE3
    const bool stage3_layout_valid = record->ttb_addr == TTB_BASE_ADDR &&
                                     record->ttb_size == TTB_SIZE;
#else
    const bool stage3_layout_valid = true;
#endif

    return record_magic_valid(record) &&
           record->version == IPOD6G_HIBERNATE_RECORD_VERSION &&
           record->record_size == sizeof(*record) &&
           record->target_id == MODEL_NUMBER &&
           record->resume_abi == IPOD6G_HIBERNATE_RESUME_ABI &&
           (record->capabilities & IPOD6G_HIBERNATE_CAP_RETENTION) != 0 &&
           (record->capabilities & ~allowed_capabilities) == 0 &&
           record->mode <= maximum_mode &&
           record->area_addr == IPOD6G_HIBERNATE_AREA_ADDR &&
           record->area_size == IPOD6G_HIBERNATE_AREA_SIZE &&
           record->iram_shadow_addr == IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR &&
           record->iram_shadow_size == IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE &&
           record->probe_addr == IPOD6G_HIBERNATE_PROBE_ADDR &&
           record->probe_size == IPOD6G_HIBERNATE_PROBE_SIZE &&
           stage3_layout_valid;
}

bool ipod6g_hibernate_record_valid(
        const volatile struct ipod6g_hibernate_record *record)
{
    return record_layout_valid(record) &&
           record->record_crc32 == record_crc32(record);
}

#if defined(BOOTLOADER) || IPOD6G_HIBERNATE_STAGE1
static void record_clear(void)
{
    volatile uint32_t *words = (volatile uint32_t *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    for (uint32_t i = 0; i < IPOD6G_HIBERNATE_CONTROL_SIZE / 4; i++)
        words[i] = 0;
}

static void record_init(volatile struct ipod6g_hibernate_record *record,
                        enum ipod6g_hibernate_record_state state)
{
    for (int i = 0; i < 8; i++)
        record->magic[i] = record_magic[i];

    record->version = IPOD6G_HIBERNATE_RECORD_VERSION;
    record->record_size = sizeof(*record);
    record->target_id = MODEL_NUMBER;
    record->resume_abi = IPOD6G_HIBERNATE_RESUME_ABI;
    record->state = state;
    record->capabilities = IPOD6G_HIBERNATE_COMPILED_CAPABILITIES;
    record->mode = IPOD6G_HIBERNATE_MODE_NONE;
    record->area_addr = IPOD6G_HIBERNATE_AREA_ADDR;
    record->area_size = IPOD6G_HIBERNATE_AREA_SIZE;
    record->iram_shadow_addr = IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR;
    record->iram_shadow_size = IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE;
    record->probe_addr = IPOD6G_HIBERNATE_PROBE_ADDR;
    record->probe_size = IPOD6G_HIBERNATE_PROBE_SIZE;
    record->probe_seed = IPOD6G_HIBERNATE_PROBE_SEED;
#if IPOD6G_HIBERNATE_STAGE3
    record->ttb_addr = TTB_BASE_ADDR;
    record->ttb_size = TTB_SIZE;
#endif
}

static void record_commit_crc(
        volatile struct ipod6g_hibernate_record *record)
{
    record->record_crc32 = 0;
    record->record_crc32 = record_crc32(record);
}
#endif

#ifndef BOOTLOADER

static volatile bool hibernate_poweroff_enabled;

bool ipod6g_hibernate_stage1_get_status(
        struct ipod6g_hibernate_status *status)
{
    const volatile struct ipod6g_hibernate_record *record =
            (const volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    status->valid = false;
    status->state = IPOD6G_HIBERNATE_RECORD_EMPTY;
    status->sequence = 0;
    status->attempt_count = 0;
    status->capabilities = 0;
    status->mode = IPOD6G_HIBERNATE_MODE_NONE;
    status->last_phase = IPOD6G_HIBERNATE_PHASE_NONE;
    status->failure = IPOD6G_HIBERNATE_FAILURE_NONE;
    status->observed_wake_reason = 0;
    status->pmu_control = 0;
    status->pmu_interrupts = 0;
    status->pmu_power = 0;
    status->pmu_entry_before = 0;
    status->pmu_entry_after = 0;
    status->payload_expected_cookie = 0;
    status->payload_observed_cookie = 0;
    status->payload_observed_sp = 0;
    status->payload_return_value = 0;
    status->context_pc = 0;
    status->context_sp = 0;
    status->context_cpsr = 0;
    status->context_ttb_crc32 = 0;
    status->context_observed_ttb_crc32 = 0;
    status->diagnostic_breadcrumb = IPOD6G_HIBERNATE_DIAG_NONE;
    status->vic0_enable = 0;
    status->vic1_enable = 0;
    status->vic0_raw_before_core = 0;
    status->vic1_raw_before_core = 0;
    status->vic0_raw_before_full = 0;
    status->vic1_raw_before_full = 0;
    status->lcd_dma_raw_tc = 0;
    status->lcd_dma_raw_error = 0;
    status->lcd_dma_enabled_channels = 0;
    status->timer_e_count_before = 0;
    status->timer_e_count_after = 0;

    if (!ipod6g_hibernate_record_valid(record))
        return false;

    status->valid = true;
    status->state = record->state;
    status->sequence = record->sequence;
    status->attempt_count = record->attempt_count;
    status->capabilities = record->capabilities;
    status->mode = record->mode;
    status->last_phase = record->last_phase;
    status->failure = record->failure;
    status->observed_wake_reason = record->observed_wake_reason;
    status->payload_expected_cookie = record->payload_expected_cookie;
    status->payload_observed_cookie = record->payload_observed_cookie;
    status->payload_observed_sp = record->payload_observed_sp;
    status->payload_return_value = record->payload_return_value;
    status->context_pc = record->cpu.pc;
    status->context_sp = record->cpu.sp;
    status->context_cpsr = record->cpu.cpsr;
#if IPOD6G_HIBERNATE_STAGE3
    status->context_ttb_crc32 = record->ttb_crc32;
    status->context_observed_ttb_crc32 = record->observed_ttb_crc32;
    status->diagnostic_breadcrumb = record->reserved[0];
    status->pmu_control = record->reserved[1];
    status->pmu_interrupts = record->reserved[2];
    status->pmu_power = record->reserved[3];
    status->pmu_entry_before = record->pmu_entry_before;
    status->pmu_entry_after = record->pmu_entry_after;
    status->vic0_enable = record->irq.vic0_enable;
    status->vic1_enable = record->irq.vic1_enable;
    status->vic0_raw_before_core = record->vic0_raw_before_core;
    status->vic1_raw_before_core = record->vic1_raw_before_core;
    status->vic0_raw_before_full = record->vic0_raw_before_full;
    status->vic1_raw_before_full = record->vic1_raw_before_full;
    status->lcd_dma_raw_tc = record->lcd_dma_raw_tc;
    status->lcd_dma_raw_error = record->lcd_dma_raw_error;
    status->lcd_dma_enabled_channels =
            record->lcd_dma_enabled_channels;
    status->timer_e_count_before = record->timer_e_count_before;
    status->timer_e_count_after = record->timer_e_count_after;
#endif
    return true;
}

#if IPOD6G_HIBERNATE_STAGE1
#define IPOD6G_HIBERNATE_REQUEST_LIFETIME (30 * HZ)
#define IPOD6G_HIBERNATE_I2C_TIMEOUT_US    20000u

static volatile bool hibernate_requested;
static uint32_t hibernate_request_sequence;
static uint32_t hibernate_request_mode;
static long hibernate_request_deadline;

static bool hibernate_record_proves_bootloader(
        const volatile struct ipod6g_hibernate_record *record,
        uint32_t required_capability)
{
    if (!ipod6g_hibernate_record_valid(record))
        return false;

    return (record->capabilities & required_capability) != 0 &&
           (record->state == IPOD6G_HIBERNATE_RECORD_CAPABLE ||
            record->state == IPOD6G_HIBERNATE_RECORD_PASSED ||
            record->state == IPOD6G_HIBERNATE_RECORD_FAILED);
}

static bool hibernate_request(uint32_t sequence, uint32_t mode,
                              uint32_t required_capability)
{
    const volatile struct ipod6g_hibernate_record *record =
            (const volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    if (!hibernate_record_proves_bootloader(record, required_capability))
        return false;

    hibernate_request_sequence = sequence;
    hibernate_request_mode = mode;
    hibernate_request_deadline = current_tick +
            IPOD6G_HIBERNATE_REQUEST_LIFETIME;
    hibernate_requested = true;
    return true;
}

bool ipod6g_hibernate_stage1_request(uint32_t sequence)
{
    return hibernate_request(sequence, IPOD6G_HIBERNATE_MODE_RETENTION,
                             IPOD6G_HIBERNATE_CAP_RETENTION);
}

bool ipod6g_hibernate_stage2_request(uint32_t sequence)
{
#if IPOD6G_HIBERNATE_STAGE2
    return hibernate_request(sequence,
            IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD,
            IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD);
#else
    (void)sequence;
    return false;
#endif
}

bool ipod6g_hibernate_stage3_request(uint32_t sequence)
{
#if IPOD6G_HIBERNATE_STAGE3
    /* Stage 3 must use the coordinated in-thread suspend path below. */
    (void)sequence;
    return false;
#else
    (void)sequence;
    return false;
#endif
}

bool ipod6g_hibernate_consume_request(uint32_t *sequence, uint32_t *mode)
{
    bool requested = hibernate_requested;

    hibernate_requested = false;
    if (!requested || !TIME_BEFORE(current_tick, hibernate_request_deadline))
        return false;

    *sequence = hibernate_request_sequence;
    *mode = hibernate_request_mode;
    return true;
}

#if IPOD6G_HIBERNATE_STAGE2
extern unsigned char _hibernate_payload_start[];
extern unsigned char _hibernate_payload_end[];

static uint32_t ipod6g_hibernate_stage2_payload(
        volatile struct ipod6g_hibernate_record *record,
        uint32_t expected_stack_top)
        __attribute__((section(".hibernate_payload"), noinline, used));
#endif

#if IPOD6G_HIBERNATE_STAGE3
extern unsigned char _stackbegin[];
extern unsigned char _stackend[];

extern uint32_t ipod6g_hibernate_stage3_checkpoint(
        struct ipod6g_hibernate_cpu_context *context);
#ifndef BOOTLOADER
void button_hibernate_resume(void);
void pcm_hibernate_resume(void);
void power_hibernate_resume(void);
#endif
#endif

static uint32_t probe_pattern(uint32_t index, uint32_t seed)
{
    uint32_t value = seed + index * 0x9e3779b9u;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    value ^= 1u << (index & 31);
    value ^= (index & 1) ? 0xaaaaaaaau : 0x55555555u;
    return value;
}

bool ipod6g_hibernate_prepare(uint32_t sequence, uint32_t mode)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;
    const volatile uint32_t *iram =
            (const volatile uint32_t *)IRAM0_ORIG;
    volatile uint32_t *shadow = (volatile uint32_t *)
            IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR;
    volatile uint32_t *probe = (volatile uint32_t *)
            IPOD6G_HIBERNATE_PROBE_ADDR;

    uint32_t required_capability;

    if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
        required_capability = IPOD6G_HIBERNATE_CAP_CONTROLLED_CONTEXT;
    else if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD)
        required_capability = IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD;
    else
        required_capability = IPOD6G_HIBERNATE_CAP_RETENTION;

    if (mode != IPOD6G_HIBERNATE_MODE_RETENTION &&
        mode != IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD &&
        mode != IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
    {
        return false;
    }

    /* A retained result also proves that this bootloader speaks our ABI. */
    if (!hibernate_record_proves_bootloader(record, required_capability))
        return false;

    if (ipod6g_hibernate_token_clear() != 0)
        return false;

    record_clear();

    /* Make the physical IRAM alias current before snapshotting it. */
    commit_dcache();
    for (uint32_t i = 0; i < IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE / 4; i++)
        shadow[i] = iram[i];

    for (uint32_t i = 0; i < IPOD6G_HIBERNATE_PROBE_SIZE / 4; i++)
        probe[i] = probe_pattern(i, IPOD6G_HIBERNATE_PROBE_SEED);

    record_init(record, IPOD6G_HIBERNATE_RECORD_PREPARED);
    record->sequence = sequence;
    record->mode = mode;
    record->requested_wake_mask = IPOD6G_HIBERNATE_WAKE_MASK;
    record->iram_shadow_crc32 = crc32_region(
            IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR,
            IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE);
    record->probe_crc32 = crc32_region(IPOD6G_HIBERNATE_PROBE_ADDR,
                                       IPOD6G_HIBERNATE_PROBE_SIZE);
#if IPOD6G_HIBERNATE_STAGE2
    if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD ||
        mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
    {
        uintptr_t payload_start = (uintptr_t)_hibernate_payload_start;
        uintptr_t payload_end = (uintptr_t)_hibernate_payload_end;
        uintptr_t payload_entry;
        uintptr_t stack_bottom;
        uintptr_t stack_top;
        uint32_t expected_cookie;
        uint32_t payload_size = payload_end - payload_start;

#if IPOD6G_HIBERNATE_STAGE3
        if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
        {
            payload_entry =
                    (uintptr_t)ipod6g_hibernate_stage3_checkpoint;
            stack_bottom = (uintptr_t)_stackbegin;
            stack_top = (uintptr_t)_stackend;
            expected_cookie = IPOD6G_HIBERNATE_CONTEXT_COOKIE;
        }
        else
#endif
        {
            payload_entry = (uintptr_t)ipod6g_hibernate_stage2_payload;
            stack_bottom = IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM;
            stack_top = IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP;
            expected_cookie = IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
        }

        if (payload_start < DRAM_ORIG ||
            payload_end > IPOD6G_HIBERNATE_AREA_ADDR ||
            payload_end <= payload_start ||
            payload_size > IPOD6G_HIBERNATE_PAYLOAD_MAX_SIZE ||
            payload_entry < payload_start || payload_entry >= payload_end ||
            (payload_entry & 3u) != 0)
        {
            return false;
        }

        if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD)
        {
            volatile uint32_t *stack_guard = (volatile uint32_t *)
                    IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM;

            for (unsigned i = 0; i < 4; i++)
                stack_guard[i] = IPOD6G_HIBERNATE_STACK_GUARD ^ i;
        }

        record->payload_start = payload_start;
        record->payload_size = payload_size;
        record->payload_crc32 = crc32_region(payload_start, payload_size);
        record->payload_entry = payload_entry;
        record->payload_stack_bottom = stack_bottom;
        record->payload_stack_top = stack_top;
        record->payload_expected_cookie = expected_cookie;
#if IPOD6G_HIBERNATE_STAGE3
        if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
        {
            record->build_fingerprint[0] = build_version_crc32();
            record->build_fingerprint[1] = sizeof(*record);
            record->build_fingerprint[2] = record->payload_crc32;
            record->build_fingerprint[3] =
                    IPOD6G_HIBERNATE_CONTEXT_COOKIE;
            record->ttb_crc32 = crc32_region(TTB_BASE_ADDR, TTB_SIZE);
        }
#endif
    }
#else
    if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD ||
        mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
        return false;
#endif
    record->last_phase = IPOD6G_HIBERNATE_PHASE_RECORD_READY;
    record_commit_crc(record);
    commit_dcache();

    return ipod6g_hibernate_record_valid(record);
}

#if IPOD6G_HIBERNATE_STAGE2
/*
 * This is the only retained application code executed by the Stage 2 gate.
 * It deliberately has no calls, no globals, no device access, and no return
 * path except the bootloader-provided LR. The volatile locals force real use
 * of the dedicated retained stack; the linked image is disassembled before a
 * hardware build is accepted.
 */
static uint32_t ipod6g_hibernate_stage2_payload(
        volatile struct ipod6g_hibernate_record *record,
        uint32_t expected_stack_top)
{
    volatile uint32_t stack_probe[4];
    uintptr_t observed_sp;

    stack_probe[0] = IPOD6G_HIBERNATE_STACK_GUARD;
    stack_probe[1] = IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
    stack_probe[2] = (uintptr_t)record;
    stack_probe[3] = expected_stack_top;
    asm volatile("mov %0, sp" : "=r"(observed_sp));

    if (record != (volatile struct ipod6g_hibernate_record *)
                    IPOD6G_HIBERNATE_CONTROL_ADDR ||
        expected_stack_top != IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP ||
        record->state != IPOD6G_HIBERNATE_RECORD_PAYLOAD_ENTERING ||
        record->mode != IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD ||
        record->payload_stack_bottom !=
                    IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM ||
        record->payload_stack_top != IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP ||
        record->payload_expected_cookie !=
                    IPOD6G_HIBERNATE_PAYLOAD_COOKIE ||
        observed_sp < IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM ||
        observed_sp >= IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP ||
        stack_probe[0] != IPOD6G_HIBERNATE_STACK_GUARD ||
        stack_probe[1] != IPOD6G_HIBERNATE_PAYLOAD_COOKIE ||
        stack_probe[2] != (uintptr_t)record ||
        stack_probe[3] != expected_stack_top)
    {
        return ~IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
    }

    record->payload_observed_cookie = IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
    record->payload_observed_sp = observed_sp;
    record->payload_return_value = IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
    record->last_phase = IPOD6G_HIBERNATE_PHASE_PAYLOAD_RETURNED;
    record->state = IPOD6G_HIBERNATE_RECORD_PAYLOAD_RETURNED;
    return IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
}
#endif /* IPOD6G_HIBERNATE_STAGE2 */

#if IPOD6G_HIBERNATE_STAGE3
static void __attribute__((noinline, noclone)) stage3_mask_vic(
        const volatile struct ipod6g_hibernate_record *record)
{
    VIC0INTENCLEAR = 0xffffffffu;
    VIC1INTENCLEAR = 0xffffffffu;
    VIC0INTSELECT = record->irq.vic0_select;
    VIC1INTSELECT = record->irq.vic1_select;
}

static void __attribute__((noinline, noclone)) stage3_rearm_tick(void)
{
    /* Acknowledge the old Timer B edge and provide a complete interval before
     * the saved VIC mask is restored. No scheduler primitive runs here. */
    tick_start(1000 / HZ);
}

static void __attribute__((noinline, noclone)) stage3_restore_all_vic(
        const volatile struct ipod6g_hibernate_record *record)
{
    /* The bootloader may leave a 32-bit timer event latched.  IRQ_TIMER32 is
     * enabled during normal Rockbox operation even when no plugin timer owns
     * Timer F; an uncleared status bit would therefore re-enter the IRQ
     * handler forever as soon as the saved mask becomes live. */
    TSTAT = 0x07070707u;

    VIC0INTENCLEAR = 0xffffffffu;
    VIC1INTENCLEAR = 0xffffffffu;
    VIC0INTSELECT = record->irq.vic0_select;
    VIC1INTSELECT = record->irq.vic1_select;
    VIC0INTENABLE = record->irq.vic0_enable;
    VIC1INTENABLE = record->irq.vic1_enable;
}

static void stage3_commit_boundary(
        volatile struct ipod6g_hibernate_record *record,
        uint32_t breadcrumb, uint32_t phase)
{
    record->reserved[0] = breadcrumb;
    record->last_phase = phase;
    record_commit_crc(record);
    commit_dcache();
}

static void stage3_lcd_checkpoint(unsigned int checkpoint, void *context)
{
    volatile struct ipod6g_hibernate_record *record = context;
    uint32_t breadcrumb;
    uint32_t phase;

    switch (checkpoint)
    {
        case LCD_HIBERNATE_RESUME_CLOCKS:
            breadcrumb = IPOD6G_HIBERNATE_DIAG_LCD_CLOCKS;
            phase = IPOD6G_HIBERNATE_PHASE_LCD_CLOCKS;
            break;
        case LCD_HIBERNATE_RESUME_COMMAND:
            breadcrumb = IPOD6G_HIBERNATE_DIAG_LCD_COMMAND;
            phase = IPOD6G_HIBERNATE_PHASE_LCD_COMMAND;
            break;
        case LCD_HIBERNATE_RESUME_SEQUENCE:
            breadcrumb = IPOD6G_HIBERNATE_DIAG_LCD_SEQUENCE;
            phase = IPOD6G_HIBERNATE_PHASE_LCD_SEQUENCE;
            break;
        case LCD_HIBERNATE_RESUME_FRAME:
            breadcrumb = IPOD6G_HIBERNATE_DIAG_LCD_FRAME;
            phase = IPOD6G_HIBERNATE_PHASE_LCD_FRAME;
            break;
        case LCD_HIBERNATE_RESUME_DMA_DONE:
            breadcrumb = IPOD6G_HIBERNATE_DIAG_LCD_DMA_DONE;
            phase = IPOD6G_HIBERNATE_PHASE_LCD_DMA_DONE;
            break;
        default:
            return;
    }

    stage3_commit_boundary(record, breadcrumb, phase);
}

static bool __attribute__((noinline, noclone)) stage3_finish_display(
        volatile struct ipod6g_hibernate_record *record)
{
    struct lcd_hibernate_resume_status status;
    bool success = lcd_hibernate_finish_resume(stage3_lcd_checkpoint,
                                               (void *)record, &status);

    record->timer_e_count_before = status.timer_e_count_before;
    record->timer_e_count_after = status.timer_e_count_after;
    record->lcd_dma_raw_tc = status.dma_raw_tc;
    record->lcd_dma_raw_error = status.dma_raw_error;
    record->lcd_dma_enabled_channels = status.dma_enabled_channels;
    backlight_hibernate_resume();
    return success;
}

bool ipod6g_hibernate_stage3_enter(void)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;
    const volatile uint32_t *iram =
            (const volatile uint32_t *)IRAM0_ORIG;
    volatile uint32_t *shadow = (volatile uint32_t *)
            IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR;

    if (!ipod6g_hibernate_record_valid(record) ||
        record->state != IPOD6G_HIBERNATE_RECORD_ARMED ||
        record->mode != IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT ||
        record->payload_entry !=
                (uintptr_t)ipod6g_hibernate_stage3_checkpoint)
    {
        ipod6g_hibernate_stage1_fail(
                IPOD6G_HIBERNATE_FAILURE_CONTEXT_METADATA);
        return false;
    }

    if (ipod6g_hibernate_stage3_checkpoint(
                (struct ipod6g_hibernate_cpu_context *)&record->cpu) != 0)
    {
        uintptr_t observed_sp;

        record->reserved[0] = IPOD6G_HIBERNATE_DIAG_APP_CONTINUE;
        asm volatile("mov %0, sp" : "=r"(observed_sp));

        /* The bootloader CRT replaced these blocks; rebuild hardware only. */
        system_hibernate_resume();
        eint_hibernate_resume();
        button_hibernate_resume();
        uart_hibernate_resume();
        pmu_hibernate_resume();
        power_hibernate_resume();
        pcm_hibernate_resume();
        lcd_hibernate_resume();

        record->payload_observed_cookie = IPOD6G_HIBERNATE_CONTEXT_COOKIE;
        record->payload_observed_sp = observed_sp;
        record->payload_return_value = IPOD6G_HIBERNATE_CONTEXT_COOKIE;
        record->vic0_raw_before_core = VIC0RAWINTR;
        record->vic1_raw_before_core = VIC1RAWINTR;

        /* Hardware hooks may enable individual sources while the CPU remains
         * masked. Return to the suspending thread with every VIC source
         * closed; it repairs the complete display substrate before exposing
         * any retained scheduler state. */
        stage3_mask_vic(record);
        stage3_commit_boundary(record,
                IPOD6G_HIBERNATE_DIAG_HW_RESTORED,
                IPOD6G_HIBERNATE_PHASE_HARDWARE_RESTORED);
        return true;
    }

    record->irq.vic0_select = VIC0INTSELECT;
    record->irq.vic1_select = VIC1INTSELECT;
    record->irq.vic0_enable = VIC0INTENABLE;
    record->irq.vic1_enable = VIC1INTENABLE;

    /* Freeze the complete application IRAM image after context capture. */
    commit_dcache();
    for (uint32_t i = 0; i < IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE / 4; i++)
        shadow[i] = iram[i];

    record->iram_shadow_crc32 = crc32_region(
            IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR,
            IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE);
    record->last_phase = IPOD6G_HIBERNATE_PHASE_CONTEXT_SAVED;
    record_commit_crc(record);
    commit_dcache();

    ipod6g_hibernate_stage1_enter();
}

bool ipod6g_hibernate_stage3_suspend(uint32_t sequence)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;
    bool display_slept = false;
    bool display_ok = false;
    bool resumed;
    int oldlevel;

    if (!battery_level_safe()
#if CONFIG_CHARGING
        || power_input_present()
#endif
        || audio_status() != 0 || pcm_is_playing()
#ifdef HAVE_RECORDING
        || pcm_is_recording()
#endif
        || usb_detect() != USB_EXTRACTED ||
        ipod6g_videoout_active())
    {
        return false;
    }

    pmu_set_wake_condition(IPOD6G_HIBERNATE_WAKE_MASK);
    if (!ipod6g_hibernate_prepare(sequence,
                    IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT))
    {
        return false;
    }

    if (!ipod6g_hibernate_stage1_i2c_preflight())
    {
        ipod6g_hibernate_stage1_fail(
                IPOD6G_HIBERNATE_FAILURE_I2C_PREFLIGHT);
        return false;
    }

    if (storage_flush() != 0)
        return false;

    storage_sleepnow();
    uart_hibernate_suspend();
    lcd_wait_for_dma();
    backlight_hw_kill();
#ifdef HAVE_LCD_SLEEP
    lcd_sleep();
    display_slept = true;
#endif

    if (!ipod6g_hibernate_stage1_arm())
    {
        if (display_slept)
            lcd_awake();
        backlight_hw_on();
        return false;
    }

    /* Match RetailOS's coordinated power transition: no other retained
     * thread may start a PMU transaction after the context boundary. */
    i2c_bus_lock(0);
    oldlevel = disable_interrupt_save(IRQ_FIQ_STATUS);
    resumed = ipod6g_hibernate_stage3_enter();

    /* R7 proved that restoring every saved VIC source and unmasking the CPU
     * while this retained mutex was still owned deadlocks at the first IRQ. */
    i2c_bus_unlock(0);

    if (resumed)
    {
        stage3_commit_boundary(record,
                IPOD6G_HIBERNATE_DIAG_I2C_RELEASED,
                IPOD6G_HIBERNATE_PHASE_I2C_RELEASED);

        /* RetailOS initializes Timer E at the retained entry and performs its
         * controller repair with hardware polling. R9 proved that asking the
         * Rockbox scheduler to sleep before that repair never returns. */
        stage3_commit_boundary(record,
                IPOD6G_HIBERNATE_DIAG_USEC_TIMER_READY,
                IPOD6G_HIBERNATE_PHASE_USEC_TIMER_READY);
        stage3_commit_boundary(record,
                IPOD6G_HIBERNATE_DIAG_LCD_ENTER,
                IPOD6G_HIBERNATE_PHASE_LCD_ENTER);

        display_ok = stage3_finish_display(record);
        if (!display_ok)
        {
            record->state = IPOD6G_HIBERNATE_RECORD_FAILED;
            record->failure = IPOD6G_HIBERNATE_FAILURE_LCD_POLL;
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_LCD_FAILED,
                    IPOD6G_HIBERNATE_PHASE_LCD_FAILED);
        }

        record->vic0_raw_before_full = VIC0RAWINTR;
        record->vic1_raw_before_full = VIC1RAWINTR;
        if (display_ok)
        {
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_DISPLAY_READY,
                    IPOD6G_HIBERNATE_PHASE_DISPLAY_RESTORED);
        }

        /* Start one fresh Timer B interval immediately before restoring the
         * complete saved interrupt topology. This is the first point at
         * which the retained scheduler is allowed to run. */
        stage3_rearm_tick();
        if (display_ok)
        {
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_TICK_REARMED,
                    IPOD6G_HIBERNATE_PHASE_TICK_REARMED);
        }

        stage3_restore_all_vic(record);
        if (display_ok)
        {
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_ALL_IRQ_ARMED,
                    IPOD6G_HIBERNATE_PHASE_ALL_IRQ_ARMED);
        }
        restore_interrupt(oldlevel);

        if (display_ok)
        {
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_CPU_IRQ_LIVE,
                    IPOD6G_HIBERNATE_PHASE_CPU_IRQ_LIVE);
        }

        /* USB insertion is intentionally published only after the normal
         * interrupt topology is live; the USB event may wake another thread
         * immediately. */
        pmu_hibernate_resume_complete();

        if (display_ok)
        {
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_PMU_EVENT_DONE,
                    IPOD6G_HIBERNATE_PHASE_PMU_EVENT_DONE);

            lcd_hibernate_resume_complete();
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_LCD_EVENT_DONE,
                    IPOD6G_HIBERNATE_PHASE_LCD_EVENT_DONE);

            /* A visible, responsive panel is part of the Stage 3 pass gate. */
            record->state = IPOD6G_HIBERNATE_RECORD_PASSED;
            record->failure = IPOD6G_HIBERNATE_FAILURE_NONE;
            stage3_commit_boundary(record,
                    IPOD6G_HIBERNATE_DIAG_APP_COMPLETE,
                    IPOD6G_HIBERNATE_PHASE_RESUME_COMPLETE);
        }
        ipod6g_hibernate_token_clear();
    }
    else
    {
        restore_interrupt(oldlevel);
        if (display_slept)
            lcd_awake();
        backlight_hw_on();
    }

    return resumed && display_ok;
}
#else
bool ipod6g_hibernate_stage3_enter(void)
{
    return false;
}

bool ipod6g_hibernate_stage3_suspend(uint32_t sequence)
{
    (void)sequence;
    return false;
}
#endif /* IPOD6G_HIBERNATE_STAGE3 */

static bool hibernate_i2c_wait_ready(void) ICODE_ATTR;
static bool hibernate_i2c_wait_ready(void)
{
    uint32_t start = USEC_TIMER;

    while (IICUNK10(0))
    {
        if ((uint32_t)(USEC_TIMER - start) >=
            IPOD6G_HIBERNATE_I2C_TIMEOUT_US)
        {
            return false;
        }
    }

    return true;
}

static bool hibernate_i2c_wait_io(void) ICODE_ATTR;
static bool hibernate_i2c_wait_io(void)
{
    uint32_t start = USEC_TIMER;

    while ((IICSTAT(0) & (1u << 5)) != 0 &&
           (IICSTA2(0) & ((1u << 8) | (1u << 13))) == 0)
    {
        if (!hibernate_i2c_wait_ready() ||
            (uint32_t)(USEC_TIMER - start) >=
                    IPOD6G_HIBERNATE_I2C_TIMEOUT_US)
        {
            return false;
        }
    }

    IICSTA2(0) |= (1u << 8) | (1u << 13);
    return true;
}

static void hibernate_i2c_clock(bool enable) ICODE_ATTR;
static void hibernate_i2c_clock(bool enable)
{
    const uint32_t bit = 1u << (CLOCKGATE_I2C0 & 0x1f);

    if (enable)
        PWRCON_APB &= ~bit;
    else
        PWRCON_APB |= bit;
}

static void hibernate_delay_us(uint32_t usecs) ICODE_ATTR;
static void hibernate_delay_us(uint32_t usecs)
{
    uint32_t start = USEC_TIMER;

    while ((uint32_t)(USEC_TIMER - start) < usecs);
}

static bool hibernate_i2c_stop(void) ICODE_ATTR;
static bool hibernate_i2c_stop(void)
{
    if (!hibernate_i2c_wait_ready())
        return false;

    IICSTAT(0) &= ~(1u << 5);
    if (!hibernate_i2c_wait_ready())
        return false;

    IICCON(0) = 0x10;
    return hibernate_i2c_wait_io();
}

/*
 * Minimal polled write used after caches are disabled.  It deliberately
 * mirrors i2c-s5l8702.c but has no mutex, scheduler, DRAM, or clock-helper
 * dependency.  The caller owns the I2C0 clock gate and interrupt exclusion.
 */
static bool hibernate_i2c_write_reg(uint8_t reg, uint8_t value) ICODE_ATTR;
static bool hibernate_i2c_write_reg(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[2] = { reg, value };
    bool ok = true;

    if (!hibernate_i2c_wait_ready())
        return false;

    IICCON(0) = 1u << 7; /* polled, ACK generation, source clock / 32 */
    if (!hibernate_i2c_wait_ready())
        return false;

    IICSTAT(0) = 0xc0; /* master transmit mode */
    if (!hibernate_i2c_wait_ready())
        return false;

    IICDS(0) = 0xe6; /* PCF50635 8-bit write address */
    if (!hibernate_i2c_wait_ready())
        return false;

    IICSTAT(0) = 0xf0; /* START + serial output */
    if (!hibernate_i2c_wait_io() || (IICSTAT(0) & 1u) != 0)
        ok = false;

    for (unsigned i = 0; ok && i < 2; i++)
    {
        if (!hibernate_i2c_wait_ready())
        {
            ok = false;
            break;
        }

        IICDS(0) = bytes[i];
        hibernate_delay_us(5);
        if (!hibernate_i2c_wait_ready())
        {
            ok = false;
            break;
        }

        IICCON(0) = IICCON(0);
        if (!hibernate_i2c_wait_io() || (IICSTAT(0) & 1u) != 0)
            ok = false;
    }

    if (!hibernate_i2c_stop())
        ok = false;

    return ok;
}

/*
 * If execution reaches this after OOCSHDWN, the PMU did not stop the CPU.
 * Only PMU-retained bytes are safe to change while SDRAM is in self-refresh.
 * Publish the new CRC first, so a reset between writes remains Rockbox-owned
 * but invalid and therefore takes the fail-closed recovery path.
 */
#if IPOD6G_HIBERNATE_STAGE3
static bool hibernate_token_mark_entry_stalled(void) ICODE_ATTR;
static bool hibernate_token_mark_entry_stalled(void)
{
    uint8_t bytes[7];
    uint8_t crc;

    bytes[0] = 'R';
    bytes[1] = 'B';
    bytes[2] = 'H';
    bytes[3] = '6';
    bytes[4] = IPOD6G_HIBERNATE_TOKEN_VERSION;
    bytes[5] = IPOD6G_HIBERNATE_TOKEN_ENTRY_STALLED;
    bytes[6] = IPOD6G_HIBERNATE_RESUME_ABI;
    crc = token_crc8(bytes);

    if (!hibernate_i2c_write_reg(PCF5063X_REG_MEMBYTE7, crc))
        return false;

    return hibernate_i2c_write_reg(PCF5063X_REG_MEMBYTE5,
                    IPOD6G_HIBERNATE_TOKEN_ENTRY_STALLED);
}
#endif

bool ipod6g_hibernate_stage1_i2c_preflight(void)
{
#if IPOD6G_HIBERNATE_STAGE3
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;
    uint8_t before[6];
    uint8_t after[6];
    uint8_t desired_mode;
    int write_result;

    if (!ipod6g_hibernate_record_valid(record) ||
        record->state != IPOD6G_HIBERNATE_RECORD_PREPARED ||
        pmu_read_multiple(PCF5063X_REG_OOCWAKE,
                          sizeof(before), before) != 0)
    {
        return false;
    }

    /*
     * OOCWAKE..OOCSTAT are contiguous.  Preserve exactly what hardware
     * reported before the write so a later forced reset cannot obscure the
     * entry configuration.  Packing is WAKE | MODE<<8 | CTL<<16 | STAT<<24.
     */
    record->pmu_entry_before = before[0] |
            ((uint32_t)before[3] << 8) |
            ((uint32_t)before[4] << 16) |
            ((uint32_t)before[5] << 24);
    record->pmu_entry_after = 0xffffffffu;
    record_commit_crc(record);
    commit_dcache();

    if (before[0] != IPOD6G_HIBERNATE_WAKE_MASK)
        return false;

    desired_mode = (before[3] & ~IPOD6G_HIBERNATE_EXTON2_MODE_MASK) |
            IPOD6G_HIBERNATE_EXTON2_MODE_RISING;

    /*
     * This preflight runs in the normal power-off thread before caches or
     * interrupts are disabled.  Use the serialized PMU driver here: the R5
     * hardware result proved that the raw final-entry writer could report a
     * completed transaction without changing OOCMODE in this live context.
     * The raw IRAM writer remains required later, after self-refresh entry.
     */
    write_result = pmu_write(PCF5063X_REG_OOCMODE, desired_mode);

    if (pmu_read_multiple(PCF5063X_REG_OOCWAKE,
                          sizeof(after), after) != 0)
    {
        return false;
    }

    record->pmu_entry_after = after[0] |
            ((uint32_t)after[3] << 8) |
            ((uint32_t)after[4] << 16) |
            ((uint32_t)after[5] << 24);
    record_commit_crc(record);
    commit_dcache();

    return write_result == 0 &&
            after[0] == before[0] && after[3] == desired_mode;
#else
    uint8_t expected;
    uint8_t observed;
    int oldlevel;
    bool ok;

    if (pmu_read_multiple(PCF5063X_REG_OOCWAKE, 1, &expected) != 0 ||
        expected != IPOD6G_HIBERNATE_WAKE_MASK)
        return false;

    oldlevel = disable_interrupt_save(IRQ_FIQ_STATUS);
    hibernate_i2c_clock(true);
    ok = hibernate_i2c_write_reg(PCF5063X_REG_OOCWAKE, expected);
    if (hibernate_i2c_wait_ready())
        IICSTAT(0) = 0;
    hibernate_i2c_clock(false);
    restore_interrupt(oldlevel);

    if (!ok ||
        pmu_read_multiple(PCF5063X_REG_OOCWAKE, 1, &observed) != 0)
    {
        return false;
    }

    return observed == expected;
#endif
}

bool ipod6g_hibernate_stage1_arm(void)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    if (!ipod6g_hibernate_record_valid(record) ||
        record->state != IPOD6G_HIBERNATE_RECORD_PREPARED)
    {
        return false;
    }

    record->state = IPOD6G_HIBERNATE_RECORD_ARMED;
    record->last_phase = IPOD6G_HIBERNATE_PHASE_TOKEN_ARMED;
    record_commit_crc(record);
    commit_dcache();

    if (ipod6g_hibernate_token_arm(IPOD6G_HIBERNATE_TOKEN_ARMED) != 0)
    {
        record->state = IPOD6G_HIBERNATE_RECORD_FAILED;
        record->failure = IPOD6G_HIBERNATE_FAILURE_TOKEN_IO;
        record_commit_crc(record);
        commit_dcache();
        ipod6g_hibernate_token_clear();
        return false;
    }

    record->last_phase = IPOD6G_HIBERNATE_PHASE_ENTRY_READY;
    record_commit_crc(record);
    commit_dcache();

    return true;
}

void ipod6g_hibernate_stage1_fail(
        enum ipod6g_hibernate_failure failure)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    if (ipod6g_hibernate_record_valid(record))
    {
        record->state = IPOD6G_HIBERNATE_RECORD_FAILED;
        record->failure = failure;
        record_commit_crc(record);
        commit_dcache();
    }

    ipod6g_hibernate_token_clear();
}

void ipod6g_hibernate_stage1_enter(void) ICODE_ATTR;
void ipod6g_hibernate_stage1_enter(void)
{
    bool marker_written;
    uint32_t control;

    disable_interrupt(IRQ_FIQ_STATUS);
    commit_discard_idcache();

    /* RetailOS payload offsets 0x31a4 and 0x318c: D/I cache off, MMU on. */
    asm volatile(
        "mrc p15, 0, %0, c1, c0, 0\n"
        "bic %0, %0, #4\n"
        "bic %0, %0, #0x1000\n"
        "mcr p15, 0, %0, c1, c0, 0\n"
        : "=&r"(control)
        :
        : "memory");

    /* RetailOS payload offset 0x25ac, mode 1. */
    MIUCON = (MIUCON & ~0x0f000000u) | 0x0a100000u;
    MIU_REG(0x14) = 1;
    asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r"(0) : "memory");

    hibernate_delay_us(10000);
    hibernate_i2c_clock(true);
    marker_written = hibernate_i2c_write_reg(
            PCF5063X_REG_GPIO3CFG, 0);
    hibernate_delay_us(100000);

    /* PCF50635 value 2 is the exact RetailOS retained-standby request. */
    hibernate_i2c_write_reg(PCF5063X_REG_OOCSHDWN,
                            marker_written ? 2 : 1);

#if IPOD6G_HIBERNATE_STAGE3
    /*
     * A real Standby transition resets the SoC on wake and can never return
     * here. Give the PMU far longer than its normal transition time, then
     * leave durable proof if this CPU is still executing. Keep retrying: a
     * partial token update is safe, but an old valid ARMED token could make a
     * later forced reset look like a genuine wake.
     */
    hibernate_delay_us(250000);
    while (!hibernate_token_mark_entry_stalled())
        hibernate_delay_us(10000);
#endif

    while (1);
}

#else /* !IPOD6G_HIBERNATE_STAGE1 */

bool ipod6g_hibernate_stage1_request(uint32_t sequence)
{
    (void)sequence;
    return false;
}

bool ipod6g_hibernate_stage2_request(uint32_t sequence)
{
    (void)sequence;
    return false;
}

bool ipod6g_hibernate_stage3_request(uint32_t sequence)
{
    (void)sequence;
    return false;
}

bool ipod6g_hibernate_consume_request(uint32_t *sequence, uint32_t *mode)
{
    (void)sequence;
    (void)mode;
    return false;
}

bool ipod6g_hibernate_prepare(uint32_t sequence, uint32_t mode)
{
    (void)sequence;
    (void)mode;
    return false;
}

bool ipod6g_hibernate_stage1_arm(void)
{
    return false;
}

bool ipod6g_hibernate_stage1_i2c_preflight(void)
{
    return false;
}

void ipod6g_hibernate_stage1_fail(
        enum ipod6g_hibernate_failure failure)
{
    (void)failure;
}

void ipod6g_hibernate_stage1_enter(void)
{
    while (1);
}

bool ipod6g_hibernate_stage3_enter(void)
{
    return false;
}

bool ipod6g_hibernate_stage3_suspend(uint32_t sequence)
{
    (void)sequence;
    return false;
}

#endif /* IPOD6G_HIBERNATE_STAGE1 */

void ipod6g_hibernate_set_poweroff_mode(
        enum ipod6g_poweroff_mode mode)
{
    hibernate_poweroff_enabled = mode == IPOD6G_POWEROFF_RETAINED;
}

bool ipod6g_hibernate_poweroff_try(uint32_t sequence)
{
#if IPOD6G_HIBERNATE_STAGE3
#if IPOD6G_HIBERNATE_STAGE1
    /* Explicit Stage 1/2 diagnostics own the following legacy shutdown. */
    if (hibernate_requested)
        return false;
#endif
    if (hibernate_poweroff_enabled)
        return ipod6g_hibernate_stage3_suspend(sequence);
#else
    (void)sequence;
#endif
    return false;
}

#else /* BOOTLOADER */

void ipod6g_hibernate_capture_pmu_snapshot(
        struct ipod6g_hibernate_pmu_snapshot *snapshot)
{
#if IPOD6G_HIBERNATE_STAGE3
    uint8_t interrupts[5];
    uint8_t interrupt6;
#else
    uint8_t interrupts[2];
#endif
    uint8_t oocstat;

#if IPOD6G_HIBERNATE_STAGE3
    interrupts[0] = 0xff;
    interrupts[1] = 0xff;
    interrupts[2] = 0xff;
    interrupts[3] = 0xff;
    interrupts[4] = 0xff;
    oocstat = pmu_rd(PCF5063X_REG_OOCSTAT);
    pmu_rd_multiple(PCF5063X_REG_INT1, 5, interrupts);
    interrupt6 = pmu_rd(PCF50635_REG_INT6);

    snapshot->wake_reason = oocstat |
            ((uint32_t)interrupts[0] << 8) |
            ((uint32_t)interrupts[1] << 16);
    snapshot->control = pmu_rd(PCF5063X_REG_OOCSHDWN) |
            ((uint32_t)pmu_rd(PCF5063X_REG_OOCWAKE) << 8) |
            ((uint32_t)pmu_rd(PCF5063X_REG_OOCMODE) << 16) |
            ((uint32_t)oocstat << 24);
    snapshot->interrupts = interrupts[0] |
            ((uint32_t)interrupts[1] << 8) |
            ((uint32_t)interrupts[2] << 16) |
            ((uint32_t)interrupts[3] << 24);
    snapshot->power = interrupts[4] |
            ((uint32_t)interrupt6 << 8) |
            ((uint32_t)pmu_rd(PCF5063X_REG_GPIO3CFG) << 16) |
            ((uint32_t)pmu_rd(PCF5063X_REG_OOCCTL) << 24);
#else
    interrupts[0] = 0;
    interrupts[1] = 0;
    oocstat = pmu_rd(PCF5063X_REG_OOCSTAT);
    snapshot->wake_reason = oocstat;
    if (pmu_rd_multiple(PCF5063X_REG_INT1, 2, interrupts) == 0)
    {
        snapshot->wake_reason |= (uint32_t)interrupts[0] << 8;
        snapshot->wake_reason |= (uint32_t)interrupts[1] << 16;
    }
    snapshot->control = 0;
    snapshot->interrupts = 0;
    snapshot->power = 0;
#endif
}

enum ipod6g_hibernate_boot_action ipod6g_hibernate_boot_action(void)
{
    struct ipod6g_hibernate_token token;

    if (!ipod6g_hibernate_token_read(&token))
        return IPOD6G_HIBERNATE_BOOT_TOKEN_IO_ERROR;

    if (!ipod6g_hibernate_token_owned(&token))
        return IPOD6G_HIBERNATE_BOOT_RETAIL;

    if (ipod6g_hibernate_token_valid(&token) &&
        token.state == IPOD6G_HIBERNATE_TOKEN_ARMED)
    {
        return IPOD6G_HIBERNATE_BOOT_ROCKBOX;
    }

    if (ipod6g_hibernate_token_valid(&token) &&
        token.state == IPOD6G_HIBERNATE_TOKEN_ENTRY_STALLED)
    {
        return IPOD6G_HIBERNATE_BOOT_STANDBY_STALLED;
    }

    /* Full magic always establishes ownership, even with a bad payload. */
    return IPOD6G_HIBERNATE_BOOT_RECOVER;
}

static void record_store_pmu_snapshot(
        volatile struct ipod6g_hibernate_record *record,
        const struct ipod6g_hibernate_pmu_snapshot *snapshot)
{
    if (snapshot == NULL)
        return;

    record->observed_wake_reason = snapshot->wake_reason;
#if IPOD6G_HIBERNATE_STAGE3
    record->reserved[1] = snapshot->control;
    record->reserved[2] = snapshot->interrupts;
    record->reserved[3] = snapshot->power;
#endif
}

static void record_failure(
        enum ipod6g_hibernate_failure failure,
        const struct ipod6g_hibernate_pmu_snapshot *snapshot)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    /*
     * A reset during the Stage 3 round trip leaves the PMU token in RESUMING.
     * Preserve the last raw breadcrumb even if the continuation changed
     * CRC-covered result fields before the reset.  No retained address is
     * executed here; the fixed record layout is validated first and the
     * bootloader immediately converts it into a fresh FAILED record CRC.
     */
#if IPOD6G_HIBERNATE_STAGE3
    if (record_layout_valid(record) &&
        record->mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT &&
        (ipod6g_hibernate_record_valid(record) ||
         record->reserved[0] == IPOD6G_HIBERNATE_DIAG_BOOT_DIRECT ||
         (record->reserved[0] >= IPOD6G_HIBERNATE_DIAG_APP_CONTINUE &&
          record->reserved[0] <= IPOD6G_HIBERNATE_DIAG_APP_COMPLETE)))
    {
        if (failure == IPOD6G_HIBERNATE_FAILURE_STANDBY_NOT_ENTERED &&
            record->state == IPOD6G_HIBERNATE_RECORD_ARMED)
        {
            record->attempt_count++;
        }
        record->state = IPOD6G_HIBERNATE_RECORD_FAILED;
        record->failure =
                failure == IPOD6G_HIBERNATE_FAILURE_STANDBY_NOT_ENTERED ?
                failure : IPOD6G_HIBERNATE_FAILURE_CONTEXT_INTERRUPTED;
        record_store_pmu_snapshot(record, snapshot);
        record_commit_crc(record);
        commit_dcache();
        return;
    }
#endif

    record_clear();
    record_init(record, IPOD6G_HIBERNATE_RECORD_FAILED);
    record->failure = failure;
    record->last_phase = IPOD6G_HIBERNATE_PHASE_MIU_RESTORED;
    record_store_pmu_snapshot(record, snapshot);
    record_commit_crc(record);
    commit_dcache();
}

static void token_finish(enum ipod6g_hibernate_token_state state)
{
    /* PMU preinit has returned GPIO3 high, so republishing is safe here. */
    if (ipod6g_hibernate_token_set_state(state) != 0)
        ipod6g_hibernate_token_arm(state);

    /* The SDRAM record carries the durable result; avoid stale ownership. */
    ipod6g_hibernate_token_clear();
}

#if IPOD6G_HIBERNATE_STAGE2
static bool payload_metadata_valid(
        const volatile struct ipod6g_hibernate_record *record,
        uint32_t mode, uint32_t capability, uint32_t cookie)
{
    uintptr_t payload_end = record->payload_start + record->payload_size;

    return record->mode == mode &&
           (record->capabilities & capability) != 0 &&
           record->payload_start >= DRAM_ORIG &&
           record->payload_size != 0 &&
           record->payload_size <= IPOD6G_HIBERNATE_PAYLOAD_MAX_SIZE &&
           payload_end > record->payload_start &&
           payload_end <= IPOD6G_HIBERNATE_AREA_ADDR &&
           record->payload_entry >= record->payload_start &&
           record->payload_entry < payload_end &&
           (record->payload_entry & 3u) == 0 &&
           record->payload_expected_cookie == cookie;
}

static bool stage2_payload_metadata_valid(
        const volatile struct ipod6g_hibernate_record *record)
{
    return payload_metadata_valid(record,
                    IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD,
                    IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD,
                    IPOD6G_HIBERNATE_PAYLOAD_COOKIE) &&
           record->payload_stack_bottom ==
                    IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM &&
           record->payload_stack_top ==
                    IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP;
}

static bool stage2_stack_guard_valid(void)
{
    const volatile uint32_t *stack_guard = (const volatile uint32_t *)
            IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM;

    for (unsigned i = 0; i < 4; i++)
    {
        if (stack_guard[i] != (IPOD6G_HIBERNATE_STACK_GUARD ^ i))
            return false;
    }

    return true;
}
#endif /* IPOD6G_HIBERNATE_STAGE2 */

#if IPOD6G_HIBERNATE_STAGE3
static bool stage3_context_metadata_valid(
        const volatile struct ipod6g_hibernate_record *record)
{
    return payload_metadata_valid(record,
                    IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT,
                    IPOD6G_HIBERNATE_CAP_CONTROLLED_CONTEXT,
                    IPOD6G_HIBERNATE_CONTEXT_COOKIE) &&
           record->cpu.pc >= record->payload_start &&
           record->cpu.pc < record->payload_start + record->payload_size &&
           record->cpu.lr >= DRAM_ORIG &&
           record->cpu.lr < IPOD6G_HIBERNATE_AREA_ADDR &&
           (record->cpu.cpsr & 0x1fu) == 0x1fu &&
           (record->cpu.cp15_control & ((1u << 12) | (1u << 2) | 1u)) ==
                    ((1u << 12) | (1u << 2) | 1u) &&
           record->cpu.cp15_ttb == TTB_BASE_ADDR &&
           record->cpu.cp15_domain == 0xffffffffu &&
           record->build_fingerprint[0] == build_version_crc32() &&
           record->build_fingerprint[1] == sizeof(*record) &&
           record->build_fingerprint[2] == record->payload_crc32 &&
           record->build_fingerprint[3] ==
                    IPOD6G_HIBERNATE_CONTEXT_COOKIE &&
           record->payload_stack_top <= IPOD6G_HIBERNATE_APP_IRAM_TOP &&
           record->payload_stack_top > record->payload_stack_bottom &&
           record->cpu.sp >= record->payload_stack_bottom &&
           record->cpu.sp < record->payload_stack_top &&
           (record->cpu.sp & 7u) == 0 &&
           record->cpu.irq_sp >= record->payload_stack_bottom &&
           record->cpu.irq_sp < IPOD6G_HIBERNATE_APP_IRAM_TOP &&
           record->cpu.fiq_sp >= record->payload_stack_bottom &&
           record->cpu.fiq_sp < IPOD6G_HIBERNATE_APP_IRAM_TOP &&
           record->cpu.svc_sp >= record->payload_stack_bottom &&
           record->cpu.svc_sp < IPOD6G_HIBERNATE_APP_IRAM_TOP &&
           record->cpu.abt_sp >= record->payload_stack_bottom &&
           record->cpu.abt_sp < IPOD6G_HIBERNATE_APP_IRAM_TOP &&
           record->cpu.und_sp >= record->payload_stack_bottom &&
           record->cpu.und_sp < IPOD6G_HIBERNATE_APP_IRAM_TOP &&
           (record->cpu.irq_sp & 7u) == 0 &&
           (record->cpu.fiq_sp & 7u) == 0 &&
           (record->cpu.svc_sp & 7u) == 0 &&
           (record->cpu.abt_sp & 7u) == 0 &&
           (record->cpu.und_sp & 7u) == 0;
}

static bool stage3_current_mmu_matches(
        const volatile struct ipod6g_hibernate_record *record)
{
    uint32_t control;
    uint32_t ttb;
    uint32_t domain;

    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(control));
    asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttb));
    asm volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(domain));

    return control == record->cpu.cp15_control &&
           ttb == record->cpu.cp15_ttb &&
           domain == record->cpu.cp15_domain;
}

static bool stage3_restore_iram(
        const volatile struct ipod6g_hibernate_record *record)
{
    const volatile uint32_t *shadow = (const volatile uint32_t *)
            record->iram_shadow_addr;
    volatile uint32_t *iram = (volatile uint32_t *)IRAM0_ORIG;

    for (uint32_t i = 0; i < record->iram_shadow_size / 4; i++)
        iram[i] = shadow[i];

    commit_discard_idcache();
    return crc32_region(IRAM0_ORIG, record->iram_shadow_size) ==
            record->iram_shadow_crc32;
}
#endif /* IPOD6G_HIBERNATE_STAGE3 */

bool ipod6g_hibernate_validate_after_wake(
        const struct ipod6g_hibernate_pmu_snapshot *snapshot)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;
    enum ipod6g_hibernate_failure failure =
            IPOD6G_HIBERNATE_FAILURE_NONE;

    if (!ipod6g_hibernate_record_valid(record) ||
        record->state != IPOD6G_HIBERNATE_RECORD_ARMED)
    {
        record_failure(IPOD6G_HIBERNATE_FAILURE_RECORD, snapshot);
        token_finish(IPOD6G_HIBERNATE_TOKEN_FAILED);
        return false;
    }

    record->state = IPOD6G_HIBERNATE_RECORD_VALIDATING;
    record->attempt_count++;
    record_store_pmu_snapshot(record, snapshot);
    record->last_phase = IPOD6G_HIBERNATE_PHASE_MIU_RESTORED;
    record_commit_crc(record);
    commit_dcache();

    if (crc32_region(record->iram_shadow_addr, record->iram_shadow_size) !=
        record->iram_shadow_crc32)
    {
        failure = IPOD6G_HIBERNATE_FAILURE_IRAM_CRC;
    }
    else if (crc32_region(record->probe_addr, record->probe_size) !=
             record->probe_crc32)
    {
        failure = IPOD6G_HIBERNATE_FAILURE_PROBE_CRC;
    }

    if (failure == IPOD6G_HIBERNATE_FAILURE_NONE &&
        record->mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD)
    {
#if IPOD6G_HIBERNATE_STAGE2
        uint32_t payload_return = 0;

        if (!stage2_payload_metadata_valid(record))
        {
            failure = IPOD6G_HIBERNATE_FAILURE_PAYLOAD_METADATA;
        }
        else if (crc32_region(record->payload_start,
                              record->payload_size) !=
                 record->payload_crc32)
        {
            failure = IPOD6G_HIBERNATE_FAILURE_PAYLOAD_CRC;
        }
        else if (!stage2_stack_guard_valid())
        {
            failure = IPOD6G_HIBERNATE_FAILURE_PAYLOAD_STACK;
        }
        else
        {
            record->state = IPOD6G_HIBERNATE_RECORD_PAYLOAD_ENTERING;
            record->last_phase = IPOD6G_HIBERNATE_PHASE_PAYLOAD_ENTERED;
            record_commit_crc(record);
            commit_discard_idcache();

            payload_return = ipod6g_hibernate_stage2_call(
                    record->payload_entry, record->payload_stack_top, record);

            record->payload_return_value = payload_return;
            if (!record_layout_valid(record) ||
                !stage2_payload_metadata_valid(record) ||
                record->state !=
                    IPOD6G_HIBERNATE_RECORD_PAYLOAD_RETURNED ||
                record->last_phase !=
                    IPOD6G_HIBERNATE_PHASE_PAYLOAD_RETURNED ||
                record->payload_observed_cookie !=
                    record->payload_expected_cookie ||
                payload_return != record->payload_expected_cookie)
            {
                failure = IPOD6G_HIBERNATE_FAILURE_PAYLOAD_RETURN;
            }
            else if (!stage2_stack_guard_valid() ||
                     record->payload_observed_sp <
                        record->payload_stack_bottom ||
                     record->payload_observed_sp >=
                        record->payload_stack_top)
            {
                failure = IPOD6G_HIBERNATE_FAILURE_PAYLOAD_STACK;
            }
        }
#else
        failure = IPOD6G_HIBERNATE_FAILURE_PAYLOAD_METADATA;
#endif
    }
    else if (failure == IPOD6G_HIBERNATE_FAILURE_NONE &&
             record->mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
    {
#if IPOD6G_HIBERNATE_STAGE3
        record->observed_ttb_crc32 = crc32_region(record->ttb_addr,
                                                   record->ttb_size);

        if (!stage3_context_metadata_valid(record))
        {
            failure = IPOD6G_HIBERNATE_FAILURE_CONTEXT_METADATA;
        }
        else if (crc32_region(record->payload_start,
                              record->payload_size) !=
                 record->payload_crc32)
        {
            failure = IPOD6G_HIBERNATE_FAILURE_PAYLOAD_CRC;
        }
        else if (record->observed_ttb_crc32 != record->ttb_crc32)
        {
            failure = IPOD6G_HIBERNATE_FAILURE_TTB_CRC;
        }
        else if (!stage3_current_mmu_matches(record))
        {
            failure = IPOD6G_HIBERNATE_FAILURE_CONTEXT_METADATA;
        }
        else if (!stage3_restore_iram(record))
        {
            failure = IPOD6G_HIBERNATE_FAILURE_IRAM_RESTORE;
        }
        else
        {
            record->last_phase = IPOD6G_HIBERNATE_PHASE_IRAM_RESTORED;
            record_commit_crc(record);
            commit_dcache();

            record->state = IPOD6G_HIBERNATE_RECORD_CONTEXT_ENTERING;
            record->last_phase = IPOD6G_HIBERNATE_PHASE_CONTEXT_ENTERED;
            record->reserved[0] = IPOD6G_HIBERNATE_DIAG_BOOT_DIRECT;
            record_commit_crc(record);
            commit_discard_idcache();

            ipod6g_hibernate_stage3_resume(
                    (const struct ipod6g_hibernate_cpu_context *)&record->cpu);
        }
#else
        failure = IPOD6G_HIBERNATE_FAILURE_CONTEXT_METADATA;
#endif
    }
    else if (failure == IPOD6G_HIBERNATE_FAILURE_NONE &&
             record->mode != IPOD6G_HIBERNATE_MODE_RETENTION)
    {
        failure = IPOD6G_HIBERNATE_FAILURE_RECORD;
    }

    record->failure = failure;
    record->state = failure == IPOD6G_HIBERNATE_FAILURE_NONE ?
            IPOD6G_HIBERNATE_RECORD_PASSED :
            IPOD6G_HIBERNATE_RECORD_FAILED;
    if (failure == IPOD6G_HIBERNATE_FAILURE_NONE)
    {
        if (record->mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_CONTEXT)
            record->last_phase = IPOD6G_HIBERNATE_PHASE_CONTEXT_RETURNED;
        else if (record->mode ==
                 IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD)
            record->last_phase = IPOD6G_HIBERNATE_PHASE_PAYLOAD_RETURNED;
        else
            record->last_phase = IPOD6G_HIBERNATE_PHASE_DATA_VERIFIED;
    }
    record_commit_crc(record);
    commit_dcache();

    token_finish(failure == IPOD6G_HIBERNATE_FAILURE_NONE ?
            IPOD6G_HIBERNATE_TOKEN_PASSED :
            IPOD6G_HIBERNATE_TOKEN_FAILED);
    return failure == IPOD6G_HIBERNATE_FAILURE_NONE;
}

void ipod6g_hibernate_stage1_mark_recovery(
        enum ipod6g_hibernate_failure failure,
        const struct ipod6g_hibernate_pmu_snapshot *snapshot)
{
    record_failure(failure, snapshot);
    token_finish(IPOD6G_HIBERNATE_TOKEN_FAILED);
}

void ipod6g_hibernate_clear_stale_token(void)
{
    struct ipod6g_hibernate_token token;

    if (ipod6g_hibernate_token_read(&token) &&
        ipod6g_hibernate_token_owned(&token))
    {
        ipod6g_hibernate_token_clear();
    }
}

void ipod6g_hibernate_stage1_publish_capability(void)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    if (ipod6g_hibernate_record_valid(record))
    {
        /*
         * A terminal record is also the matching bootloader capability
         * handshake. Keep it until the application explicitly prepares the
         * next attempt, even across an arbitrary number of cold boots.
         */
        if (record->state == IPOD6G_HIBERNATE_RECORD_CAPABLE ||
            record->state == IPOD6G_HIBERNATE_RECORD_PASSED ||
            record->state == IPOD6G_HIBERNATE_RECORD_FAILED)
        {
            return;
        }

        /* Never replace evidence of an incomplete valid attempt with ready. */
        record_failure(IPOD6G_HIBERNATE_FAILURE_RECORD, NULL);
        return;
    }

    record_clear();
    record_init(record, IPOD6G_HIBERNATE_RECORD_CAPABLE);
    record_commit_crc(record);
    commit_dcache();
}

#endif /* BOOTLOADER */
