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

#define IPOD6G_HIBERNATE_PROBE_SEED 0x68364731u /* "h6G1" */

#if IPOD6G_HIBERNATE_STAGE2
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
    return record_magic_valid(record) &&
           record->version == IPOD6G_HIBERNATE_RECORD_VERSION &&
           record->record_size == sizeof(*record) &&
           record->target_id == MODEL_NUMBER &&
           record->resume_abi == IPOD6G_HIBERNATE_RESUME_ABI &&
           (record->capabilities & IPOD6G_HIBERNATE_CAP_RETENTION) != 0 &&
           (record->capabilities &
            ~(IPOD6G_HIBERNATE_CAP_RETENTION |
              IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD)) == 0 &&
           record->mode <= IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD &&
           record->area_addr == IPOD6G_HIBERNATE_AREA_ADDR &&
           record->area_size == IPOD6G_HIBERNATE_AREA_SIZE &&
           record->iram_shadow_addr == IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR &&
           record->iram_shadow_size == IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE &&
           record->probe_addr == IPOD6G_HIBERNATE_PROBE_ADDR &&
           record->probe_size == IPOD6G_HIBERNATE_PROBE_SIZE;
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
}

static void record_commit_crc(
        volatile struct ipod6g_hibernate_record *record)
{
    record->record_crc32 = 0;
    record->record_crc32 = record_crc32(record);
}
#endif

#ifndef BOOTLOADER

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
    status->payload_expected_cookie = 0;
    status->payload_observed_cookie = 0;
    status->payload_observed_sp = 0;
    status->payload_return_value = 0;

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

    uint32_t required_capability = mode ==
            IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD ?
            IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD :
            IPOD6G_HIBERNATE_CAP_RETENTION;

    if (mode != IPOD6G_HIBERNATE_MODE_RETENTION &&
        mode != IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD)
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
    record->requested_wake_mask =
            PCF5063X_OOCWAKE_EXTON2 | PCF5063X_OOCWAKE_EXTON1;
    record->iram_shadow_crc32 = crc32_region(
            IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR,
            IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE);
    record->probe_crc32 = crc32_region(IPOD6G_HIBERNATE_PROBE_ADDR,
                                       IPOD6G_HIBERNATE_PROBE_SIZE);
#if IPOD6G_HIBERNATE_STAGE2
    if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD)
    {
        uintptr_t payload_start = (uintptr_t)_hibernate_payload_start;
        uintptr_t payload_end = (uintptr_t)_hibernate_payload_end;
        uintptr_t payload_entry =
                (uintptr_t)ipod6g_hibernate_stage2_payload;
        uint32_t payload_size = payload_end - payload_start;
        volatile uint32_t *stack_guard = (volatile uint32_t *)
                IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM;

        if (payload_start < DRAM_ORIG ||
            payload_end > IPOD6G_HIBERNATE_AREA_ADDR ||
            payload_end <= payload_start ||
            payload_size > IPOD6G_HIBERNATE_PAYLOAD_MAX_SIZE ||
            payload_entry < payload_start || payload_entry >= payload_end ||
            (payload_entry & 3u) != 0)
        {
            return false;
        }

        for (unsigned i = 0; i < 4; i++)
            stack_guard[i] = IPOD6G_HIBERNATE_STACK_GUARD ^ i;

        record->payload_start = payload_start;
        record->payload_size = payload_size;
        record->payload_crc32 = crc32_region(payload_start, payload_size);
        record->payload_entry = payload_entry;
        record->payload_stack_bottom =
                IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM;
        record->payload_stack_top = IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP;
        record->payload_expected_cookie = IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
    }
#else
    if (mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD)
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

bool ipod6g_hibernate_stage1_i2c_preflight(void)
{
    uint8_t expected;
    uint8_t observed;
    int oldlevel;
    bool ok;

    if (pmu_read_multiple(PCF5063X_REG_OOCWAKE, 1, &expected) != 0)
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

#endif /* IPOD6G_HIBERNATE_STAGE1 */

#else /* BOOTLOADER */

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

    /* Full magic always establishes ownership, even with a bad payload. */
    return IPOD6G_HIBERNATE_BOOT_RECOVER;
}

static void record_failure(enum ipod6g_hibernate_failure failure)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;

    record_clear();
    record_init(record, IPOD6G_HIBERNATE_RECORD_FAILED);
    record->failure = failure;
    record->last_phase = IPOD6G_HIBERNATE_PHASE_MIU_RESTORED;
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
static bool stage2_payload_metadata_valid(
        const volatile struct ipod6g_hibernate_record *record)
{
    uintptr_t payload_end = record->payload_start + record->payload_size;

    return record->mode == IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD &&
           (record->capabilities &
            IPOD6G_HIBERNATE_CAP_CONTROLLED_PAYLOAD) != 0 &&
           record->payload_start >= DRAM_ORIG &&
           record->payload_size != 0 &&
           record->payload_size <= IPOD6G_HIBERNATE_PAYLOAD_MAX_SIZE &&
           payload_end > record->payload_start &&
           payload_end <= IPOD6G_HIBERNATE_AREA_ADDR &&
           record->payload_entry >= record->payload_start &&
           record->payload_entry < payload_end &&
           (record->payload_entry & 3u) == 0 &&
           record->payload_stack_bottom ==
                    IPOD6G_HIBERNATE_PAYLOAD_STACK_BOTTOM &&
           record->payload_stack_top ==
                    IPOD6G_HIBERNATE_PAYLOAD_STACK_TOP &&
           record->payload_expected_cookie ==
                    IPOD6G_HIBERNATE_PAYLOAD_COOKIE;
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

bool ipod6g_hibernate_validate_after_wake(uint32_t wake_reason)
{
    volatile struct ipod6g_hibernate_record *record =
            (volatile struct ipod6g_hibernate_record *)
            IPOD6G_HIBERNATE_CONTROL_ADDR;
    enum ipod6g_hibernate_failure failure =
            IPOD6G_HIBERNATE_FAILURE_NONE;

    if (!ipod6g_hibernate_record_valid(record) ||
        record->state != IPOD6G_HIBERNATE_RECORD_ARMED)
    {
        record_failure(IPOD6G_HIBERNATE_FAILURE_RECORD);
        token_finish(IPOD6G_HIBERNATE_TOKEN_FAILED);
        return false;
    }

    record->state = IPOD6G_HIBERNATE_RECORD_VALIDATING;
    record->attempt_count++;
    record->observed_wake_reason = wake_reason;
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
        record->last_phase = record->mode ==
                IPOD6G_HIBERNATE_MODE_CONTROLLED_PAYLOAD ?
                IPOD6G_HIBERNATE_PHASE_PAYLOAD_RETURNED :
                IPOD6G_HIBERNATE_PHASE_DATA_VERIFIED;
    }
    record_commit_crc(record);
    commit_dcache();

    token_finish(failure == IPOD6G_HIBERNATE_FAILURE_NONE ?
            IPOD6G_HIBERNATE_TOKEN_PASSED :
            IPOD6G_HIBERNATE_TOKEN_FAILED);
    return failure == IPOD6G_HIBERNATE_FAILURE_NONE;
}

void ipod6g_hibernate_stage1_mark_recovery(
        enum ipod6g_hibernate_failure failure)
{
    record_failure(failure);
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

    record_clear();
    record_init(record, IPOD6G_HIBERNATE_RECORD_CAPABLE);
    record_commit_crc(record);
    commit_dcache();
}

#endif /* BOOTLOADER */
