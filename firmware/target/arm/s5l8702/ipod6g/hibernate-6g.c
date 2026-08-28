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
#include "s5l87xx.h"
#include "pmu-target.h"
#include "hibernate-6g.h"

#define IPOD6G_HIBERNATE_PROBE_SEED 0x68364731u /* "h6G1" */

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

#if IPOD6G_HIBERNATE_STAGE1
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

bool ipod6g_hibernate_stage1_prepare(uint32_t sequence)
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

    /* Refuse to arm unless this bootloader advertised the same resume ABI. */
    if (!ipod6g_hibernate_record_valid(record) ||
        record->state != IPOD6G_HIBERNATE_RECORD_CAPABLE)
    {
        return false;
    }

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
    record->iram_shadow_crc32 = crc32_region(
            IPOD6G_HIBERNATE_IRAM_SHADOW_ADDR,
            IPOD6G_HIBERNATE_IRAM_SHADOW_SIZE);
    record->probe_crc32 = crc32_region(IPOD6G_HIBERNATE_PROBE_ADDR,
                                       IPOD6G_HIBERNATE_PROBE_SIZE);
    record->last_phase = IPOD6G_HIBERNATE_PHASE_RECORD_READY;
    record_commit_crc(record);
    commit_dcache();

    return ipod6g_hibernate_record_valid(record);
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

    return true;
}

#else /* !IPOD6G_HIBERNATE_STAGE1 */

bool ipod6g_hibernate_stage1_prepare(uint32_t sequence)
{
    (void)sequence;
    return false;
}

bool ipod6g_hibernate_stage1_arm(void)
{
    return false;
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
        return IPOD6G_HIBERNATE_BOOT_STAGE1;
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

bool ipod6g_hibernate_stage1_validate_after_wake(void)
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

    record->failure = failure;
    record->state = failure == IPOD6G_HIBERNATE_FAILURE_NONE ?
            IPOD6G_HIBERNATE_RECORD_PASSED :
            IPOD6G_HIBERNATE_RECORD_FAILED;
    if (failure == IPOD6G_HIBERNATE_FAILURE_NONE)
        record->last_phase = IPOD6G_HIBERNATE_PHASE_DATA_VERIFIED;
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
