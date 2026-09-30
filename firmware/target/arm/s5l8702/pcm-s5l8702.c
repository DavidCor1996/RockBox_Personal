/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: pcm-s5l8700.c 28600 2010-11-14 19:49:20Z Buschel $
 *
 * Copyright © 2011 Michael Sparmann
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
#include <string.h>

#include "config.h"
#include "system.h"
#include "audio.h"
#include "s5l87xx.h"
#include "panic.h"
#include "audiohw.h"
#include "pcm.h"
#include "pcm-internal.h"
#include "pcm_sampr.h"
#include "pcm-target.h"
#include "dma-s5l8702.h"
#include "ipodnano3g/bringup-nano3g.h"

#ifdef IPOD_NANO3G
void ipodnano3g_audio_output_init(void);
void ipodnano3g_audio_output_start(const void *addr, size_t size);
void ipodnano3g_audio_output_stop(void);
void ipodnano3g_audio_output_submit(const void *addr, size_t size);
#endif

/* DMA configuration */

/* 3 DMA tasks needed, one chunk task and two dblbuf tasks */
#define DMA_PLAY_TSKBUF_SZ  4   /* N tasks, MUST be pow2 */
#define DMA_PLAY_LLIBUF_SZ  4   /* N LLIs, MUST be pow2 */

static struct dmac_tsk dma_play_tskbuf[DMA_PLAY_TSKBUF_SZ];
static struct dmac_lli volatile \
            dma_play_llibuf[DMA_PLAY_LLIBUF_SZ] CACHEALIGN_ATTR;

static void dma_play_callback(void *data) ICODE_ATTR;

static struct dmac_ch dma_play_ch = {
    .dmac = &s5l8702_dmac0,
    .prio = DMAC_CH_PRIO(2),
    .cb_fn = dma_play_callback,

    .tskbuf = dma_play_tskbuf,
    .tskbuf_mask = DMA_PLAY_TSKBUF_SZ - 1,
    .queue_mode = QUEUE_LINK,

    .llibuf = dma_play_llibuf,
    .llibuf_mask = DMA_PLAY_LLIBUF_SZ - 1,
    .llibuf_bus = DMAC_MASTER_AHB1,
};

static struct dmac_ch_cfg dma_play_ch_cfg = {
    .srcperi = S5L8702_DMAC0_PERI_MEM,
    .dstperi = S5L8702_DMAC0_PERI_IIS0_TX,
    .sbsize  = DMACCxCONTROL_BSIZE_8,
    .dbsize  = DMACCxCONTROL_BSIZE_4,
    .swidth  = DMACCxCONTROL_WIDTH_16,
    .dwidth  = DMACCxCONTROL_WIDTH_16,
    .sbus    = DMAC_MASTER_AHB1,
    .dbus    = DMAC_MASTER_AHB1,
    .sinc    = DMACCxCONTROL_INC_ENABLE,
    .dinc    = DMACCxCONTROL_INC_DISABLE,
    .prot    = DMAC_PROT_CACH | DMAC_PROT_BUFF | DMAC_PROT_PRIV,
    /* align LLI transfers to L-R pairs (samples) */
    .lli_xfer_max_count = DMAC_LLI_MAX_COUNT & ~1,
};
#define LLI_MAX_BYTES       8188  /* lli_xfer_max_count << swidth */

/* Use all available LLIs for chunk */
/*#define CHUNK_MAX_BYTES     (LLI_MAX_BYTES * (DMA_PLAY_LLIBUF_SZ - 2))*/
#define CHUNK_MAX_BYTES     (LLI_MAX_BYTES * 1)
#define WATERMARK_BYTES     (PCM_WATERMARK * 4)

/*
 * Nano 3G WM1870 I2S setup, matching its validated upstream driver.
 * Other S5L8702 targets retain the personal/Classic values.
 */
#ifdef IPOD_NANO3G
#define I2STXCON_SETUP  0x0b100001
#define I2STXCOM_START  0x6
#else
#define I2STXCON_SETUP  0xb100019
#define I2STXCOM_START  0xe
#endif

static volatile int locked = 0;
/* When set, pcm_play_dma_start() returns immediately without starting
 * I2S DMA.  Used by USB audio source pull mode where the USB ISR
 * drives audio data instead of the DMA controller. */
volatile bool pcm_dma_start_inhibit = false;
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
static bool pcm_hibernate_codec_blocked;
#endif
static unsigned char dblbuf[2][WATERMARK_BYTES] CACHEALIGN_ATTR;
static int active_dblbuf;
size_t pcm_remaining;

/* Mask the DMA interrupt */
void pcm_play_lock(void)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
        return;
#endif

    if (locked++ == 0)
        dmac_ch_lock_int(&dma_play_ch);
}

/* Unmask the DMA interrupt if enabled */
void pcm_play_unlock(void)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
        return;
#endif

    if (--locked == 0)
        dmac_ch_unlock_int(&dma_play_ch);
}

static inline void play_queue_dma(void *addr, size_t size, void *cb_data)
{
    commit_dcache_range(addr, size);
    dmac_ch_queue(&dma_play_ch, addr,
                (void*)S5L8702_DADDR_PERI_IIS0_TX, size, cb_data);
}

static void dma_play_callback(void *cb_data)
{
    if (!cb_data)
        return; /* dblbuf callback entered, nothing to do */

    const void *dataptr = cb_data;

    if (!pcm_remaining)
        if (!pcm_play_dma_complete_callback(
                     PCM_DMAST_OK, &dataptr, &pcm_remaining))
            return;

    uint32_t lastsize = MIN(WATERMARK_BYTES, pcm_remaining >> 1);
    pcm_remaining -= lastsize;
    uint32_t chunksize = MIN(CHUNK_MAX_BYTES, pcm_remaining);

    /* last chunk should be at least 2*WATERMARK_BYTES in size */
    if ((pcm_remaining > chunksize) &&
                (pcm_remaining < chunksize + WATERMARK_BYTES * 2))
        chunksize = pcm_remaining - WATERMARK_BYTES * 2;

    pcm_remaining -= chunksize;

    /* first part */
    play_queue_dma((void*)dataptr, chunksize,
                (void*)dataptr + chunksize + lastsize); /* cb_data */
#ifdef IPOD_NANO3G
    ipodnano3g_audio_output_submit(dataptr, chunksize);
#endif

    /* second part */
    memcpy(dblbuf[active_dblbuf], dataptr + chunksize, lastsize);
    play_queue_dma(dblbuf[active_dblbuf], lastsize, NULL);
#ifdef IPOD_NANO3G
    ipodnano3g_audio_output_submit(dblbuf[active_dblbuf], lastsize);
#endif
    active_dblbuf ^= 1;

    pcm_play_dma_status_callback(PCM_DMAST_STARTED);
}

void pcm_play_dma_start(const void* addr, size_t size)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
    {
        (void)addr;
        (void)size;
        nano3g_boottrace_log("pcm start skipped (safe)");
        return;
    }
#endif

    if (pcm_dma_start_inhibit)
        return;
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
    if (pcm_hibernate_codec_blocked)
        return;
#endif

    pcm_play_dma_stop();

    /* un-gate I2S clock before starting DMA */
    PWRCON(1) &= ~(1 << 7);
    I2SCLKCON = 1;

#if defined(HAVE_CS42L55)
    /* pcm_apply_settings() runs before target DMA start, while this target's
     * I2S/MCLK may still be gated after the previous stream stopped.  Reapply
     * the target clock and CS42L55 sample-rate setup with MCLK live, then
     * wake the codec before feeding DMA.  This is required when switching
     * directly from a 48 kHz database track to 44.1 kHz plugin PCM.
     */
    pcm_dma_apply_settings();
    audiohw_idle_powerup();
#endif

    pcm_remaining = size;
    I2STXCOM = I2STXCOM_START;
#ifdef IPOD_NANO3G
    ipodnano3g_audio_output_start(addr, size);
#endif
    dma_play_callback((void*)addr);
}

void pcm_play_dma_stop(void)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
    {
        pcm_remaining = 0;
        ipodnano3g_audio_output_stop();
        nano3g_boottrace_log("pcm stop skipped (safe)");
        return;
    }
#endif

    dmac_ch_stop(&dma_play_ch);
    I2STXCOM = 0xa;

#ifdef IPOD_NANO3G
    ipodnano3g_audio_output_stop();
#endif

    /*
     * Keep I2S/MCLK and CS42L55 awake across normal playback idle.  The
     * mixer stops PCM after a few seconds of paused playback; gating clocks
     * or putting PDN_CODEC back here can wedge real iPod 6G hardware during
     * the paused Now Playing idle window.  Full shutdown still closes the
     * codec through audiohw_close().
     */
}

/* MCLK = 12MHz (MCLKDIV2=1), [CS42L55 DS, s4.8] */
#define MCLK_FREQ     12000000
static uint16_t last_clkcon3l;

/* set the configured PCM frequency */
void pcm_dma_apply_settings(void)
{
#ifdef IPOD_NANO3G
    /*
     * NANO3G_OLD_PCM_FREQUENCY_PORT
     *
     * WM1870 runs from the fixed 12 MHz oscillator. Unlike the CS42L55
     * Classic path, there is no special 32 kHz PLL workaround.
     *
     * last_clkcon3l begins invalid so MCLK is explicitly enabled on the
     * first call, matching the validated Nano 3G implementation.
     */
    static uint16_t last_clkcon3l = 0xffff;
    const uint16_t clkcon3l = 0;
    int fsel = pcm_fsel;

    if (last_clkcon3l != clkcon3l)
    {
        CLKCON3 = (CLKCON3 & ~0xffff) | 0x8000 | clkcon3l;
        udelay(100);
        CLKCON3 &= ~0x8000;
        last_clkcon3l = clkcon3l;
    }

    I2SCLKDIV = MCLK_FREQ / hw_freq_sampr[fsel];

    /* WM1870 driver translates this sample-rate selection. */
    audiohw_set_frequency(fsel);

#else

#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
        return;
#endif

    uint16_t clkcon3l;
    int fsel;

    /* For unknown reasons, s5l8702 I2S controller does not synchronize
     * with CS42L55 at 32000 Hz. To fix it, the CODEC is configured with
     * a sample rate of 48000 Hz and MCLK is decreased 1/3 to 8 Mhz,
     * obtaining 32 KHz in LRCK controller input and 8 MHz in SCLK input.
     * OF uses this trick.
     */
    if (pcm_fsel == HW_FREQ_32) {
        fsel = HW_FREQ_48;
        clkcon3l = 0x3028;  /* PLL2 / 3 / 9 -> 8 MHz */
    }
    else {
        fsel = pcm_fsel;
        clkcon3l = 0;  /* OSC0 -> 12 MHz */
    }

    /* configure MCLK */
    /* TODO: maybe all CLKCON management should be moved to
       cscodec-ipod6g.c and system-s5l8702.c */
    if (last_clkcon3l != clkcon3l) {
        CLKCON3 = (CLKCON3 & ~0xffff) | 0x8000 | clkcon3l;
        udelay(100);
        CLKCON3 &= ~0x8000;  /* CLKCON3L on */
        last_clkcon3l = clkcon3l;
    }

    /* configure I2S clock ratio */
    I2SCLKDIV = MCLK_FREQ / hw_freq_sampr[fsel];
    /* select CS42L55 sample rate */
    audiohw_set_frequency(fsel);

#endif /* IPOD_NANO3G */
}

void pcm_play_dma_init(void)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
    {
        ipodnano3g_audio_output_init();
        nano3g_boottrace_log("pcm init skipped (safe)");
        return;
    }
#endif

    PWRCON(1) &= ~(1 << 7);

    dmac_ch_init(&dma_play_ch, &dma_play_ch_cfg);

    I2STXCON = I2STXCON_SETUP;
    I2SCLKCON = 1;

#ifdef IPOD_NANO3G
    ipodnano3g_audio_output_init();
#endif

    audiohw_preinit();
    pcm_dma_apply_settings();
}

void pcm_play_dma_postinit(void)
{
    audiohw_postinit();
}

#ifdef HAVE_PCM_DMA_ADDRESS
void * pcm_dma_addr(void *addr)
{
    return addr;
}
#endif


/****************************************************************************
 ** Recording DMA transfer
 **/
#ifdef HAVE_RECORDING
static volatile int rec_locked = 0;
static void *rec_dma_addr;
static size_t rec_dma_size;
static bool pcm_rec_initialized = false;
static int completed_task;

/* ahead capture buffer */
#define PCM_AHEADBUF_SAMPLES    128
#define AHEADBUF_SZ             (PCM_AHEADBUF_SAMPLES * 4)
static unsigned char ahead_buf[AHEADBUF_SZ] CACHEALIGN_ATTR;

/* DMA configuration */
static void dma_rec_callback(void *cb_data) ICODE_ATTR;

enum {  /* cb_data */
    TASK_AHEADBUF,
    TASK_RECBUF
};

#define DMA_REC_TSKBUF_SZ   2   /* N tasks, MUST be pow2 */
#define DMA_REC_LLIBUF_SZ   8   /* N LLIs, MUST be pow2 */
static struct dmac_tsk dma_rec_tskbuf[DMA_REC_TSKBUF_SZ];
static struct dmac_lli volatile \
            dma_rec_llibuf[DMA_REC_LLIBUF_SZ] CACHEALIGN_ATTR;

static struct dmac_ch dma_rec_ch = {
    .dmac = &s5l8702_dmac0,
    .prio = DMAC_CH_PRIO(1),
    .cb_fn = dma_rec_callback,

    .llibuf = dma_rec_llibuf,
    .llibuf_mask = DMA_REC_LLIBUF_SZ - 1,
    .llibuf_bus = DMAC_MASTER_AHB1,

    .tskbuf = dma_rec_tskbuf,
    .tskbuf_mask = DMA_REC_TSKBUF_SZ - 1,
    .queue_mode = QUEUE_LINK,
};

static struct dmac_ch_cfg dma_rec_ch_cfg = {
    .srcperi = S5L8702_DMAC0_PERI_IIS0_RX,
    .dstperi = S5L8702_DMAC0_PERI_MEM,
    .sbsize  = DMACCxCONTROL_BSIZE_4,
    .dbsize  = DMACCxCONTROL_BSIZE_4,
    .swidth  = DMACCxCONTROL_WIDTH_16,
    .dwidth  = DMACCxCONTROL_WIDTH_16,
    .sbus    = DMAC_MASTER_AHB1,
    .dbus    = DMAC_MASTER_AHB1,
    .sinc    = DMACCxCONTROL_INC_DISABLE,
    .dinc    = DMACCxCONTROL_INC_ENABLE,
    .prot    = DMAC_PROT_CACH | DMAC_PROT_BUFF | DMAC_PROT_PRIV,
    /* align LLI transfers to L-R pairs (samples) */
    .lli_xfer_max_count = DMAC_LLI_MAX_COUNT & ~1,
};

/* maximum and minimum supported block sizes in bytes */
#define MIN_SIZE ((size_t) (AHEADBUF_SZ * 2))
#define MAX_SIZE ((size_t) (AHEADBUF_SZ + ((DMA_REC_LLIBUF_SZ - 1) * \
             (dma_rec_ch_cfg.lli_xfer_max_count << dma_rec_ch_cfg.swidth))))

#if 0
#define SIZE_PANIC(sz) { \
    if (((sz) < MIN_SIZE) || ((sz) > MAX_SIZE)) \
        panicf("pcm record: unsupported size: %d", (sz)); \
}
#else
#define SIZE_PANIC(sz) {}
#endif


static void rec_dmac_ch_queue(void *addr, size_t size, int cb_data)
{
    discard_dcache_range(addr, size);
    dmac_ch_queue(&dma_rec_ch, (void*)S5L8702_DADDR_PERI_IIS0_RX,
                                            addr, size, (void *)cb_data);
}

static void dma_rec_callback(void *cb_data)
{
    completed_task = (int)cb_data;

    if (completed_task == TASK_AHEADBUF)
    {
        /* safety check */
        if (rec_dma_addr == NULL)
            return; /* capture finished */

        /* move ahead buffer to record buffer and queue
           next capture-ahead task */
        memcpy(rec_dma_addr, ahead_buf, AHEADBUF_SZ);
        rec_dmac_ch_queue(ahead_buf, AHEADBUF_SZ, TASK_AHEADBUF);
    }
    else /* TASK_RECBUF */
    {
        /* Inform middle layer */
        if (pcm_rec_dma_complete_callback(
                    PCM_DMAST_OK, &rec_dma_addr, &rec_dma_size))
        {
            SIZE_PANIC(rec_dma_size);
            rec_dmac_ch_queue(rec_dma_addr + AHEADBUF_SZ,
                        rec_dma_size - AHEADBUF_SZ, TASK_RECBUF);
            pcm_rec_dma_status_callback(PCM_DMAST_STARTED);
        }
    }
}

void pcm_rec_lock(void)
{
    if ((rec_locked++ == 0) && pcm_rec_initialized)
        dmac_ch_lock_int(&dma_rec_ch);
}

void pcm_rec_unlock(void)
{
    if ((--rec_locked == 0) && pcm_rec_initialized)
        dmac_ch_unlock_int(&dma_rec_ch);
}

void pcm_rec_dma_stop(void)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
        return;
#endif

    if (!pcm_rec_initialized)
        return;

    dmac_ch_stop(&dma_rec_ch);

    I2SRXCOM = 0x2; /* stop Rx I2S */
}

void pcm_rec_dma_start(void *addr, size_t size)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
    {
        (void)addr;
        (void)size;
        return;
    }
#endif

    SIZE_PANIC(size);

    pcm_rec_dma_stop();

    rec_dma_addr = addr;
    rec_dma_size = size;
    completed_task = -1;

    /* launch first DMA transfer to capture into ahead buffer,
       link the second task to capture into record buffer */
    rec_dmac_ch_queue(ahead_buf, AHEADBUF_SZ, TASK_AHEADBUF);
    rec_dmac_ch_queue(addr + AHEADBUF_SZ, size - AHEADBUF_SZ, TASK_RECBUF);

    I2SRXCOM = 0x6; /* start Rx I2S */
}

void pcm_rec_dma_close(void)
{
    pcm_rec_dma_stop();
}

void pcm_rec_dma_init(void)
{
#ifdef IPOD_NANO3G
    if (nano3g_safe_mode_enabled())
        return;
#endif

    if (pcm_rec_initialized)
        return;

    PWRCON(1) &= ~(1 << 7);

    dmac_ch_init(&dma_rec_ch, &dma_rec_ch_cfg);

    /* synchronize lock status */
    if (rec_locked)
        dmac_ch_lock_int(&dma_rec_ch);

    I2SRXCON = 0x1000;
    I2SCLKCON = 1;

    pcm_rec_initialized = true;
}

const void * pcm_rec_dma_get_peak_buffer(void)
{
    void *dstaddr;

    pcm_rec_lock();

    if (completed_task == TASK_AHEADBUF) {
        dstaddr = dmac_ch_get_info(&dma_rec_ch, NULL, NULL);

        if ((dstaddr < rec_dma_addr) ||
                    (dstaddr > rec_dma_addr + rec_dma_size))
            /* At this moment, interrupt for TASK_RECBUF is waiting to
               be handled. TASK_RECBUF is already finished and HW is
               transfering next TASK_AHEADBUF. Return whole block. */
            dstaddr = rec_dma_addr + rec_dma_size;
    }
    else {
        /* Ahead buffer not yet captured _and_ moved to
           record buffer. Return nothing. */
        dstaddr = rec_dma_addr;
    }

    pcm_rec_unlock();

    return CACHEALIGN_DOWN(dstaddr);
}
#endif /* HAVE_RECORDING */

#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
bool pcm_hibernate_suspend(void)
{
    /* The outer mixer service has already stopped physical PCM while keeping
     * every retained channel and callback intact. Save the codec bank before
     * muting it: LDO4 is lost in Standby, so internal PDN retention alone
     * cannot preserve its master clock, routing, or volume configuration. */
#if defined(HAVE_CS42L55)
    if (!audiohw_hibernate_save())
        return false;
    audiohw_idle_powerdown();
#endif
    I2SCLKCON = 0;
    PWRCON(1) |= 1 << 7;
    CLKCON3 |= 0x8000;
    return true;
}

bool pcm_hibernate_resume_complete(void)
{
    /* Called in thread context after tick repair, before the paired mixer
     * restart. Never queue output against an incomplete codec restore. */
#if defined(HAVE_CS42L55)
    if (!audiohw_hibernate_restore())
        return false;
#endif
    pcm_hibernate_codec_blocked = false;
    return true;
}

void pcm_hibernate_abort(void)
{
    /* Entry refused before the bootloader reset DMAC. Restore only the
     * hardware gated by pcm_hibernate_suspend(); the retained DMA channel
     * object and carried mixer lock are still the live ones. */
    PWRCON(1) &= ~(1 << 7);
    I2SCLKCON = 1;
    last_clkcon3l = 0xffff;
    pcm_dma_apply_settings();
#if defined(HAVE_CS42L55)
    audiohw_idle_powerup();
#endif
}

void pcm_hibernate_resume(void)
{
    pcm_hibernate_codec_blocked = true;
    /* dma_init() rebuilt the controller and discarded channel ownership.
     * Reattach the retained target channel and carry the opcode-8 PCM lock
     * onto it before any output is queued.  The outer mixer opcode-9 service
     * performs the paired output restart and sole unlock after the complete
     * device transaction has returned with the scheduler live. */
    PWRCON(1) &= ~(1 << 7);
    dmac_ch_init(&dma_play_ch, &dma_play_ch_cfg);
    if (locked)
        dmac_ch_lock_int(&dma_play_ch);

    I2STXCON = 0xb100019;
    I2STXCOM = 0xa;
    I2SCLKCON = 1;

    /* system_preinit() replaced CLKCON3; defeat the retained write cache. */
    last_clkcon3l = 0xffff;
    pcm_dma_apply_settings();

#ifdef HAVE_RECORDING
    if (pcm_rec_initialized)
    {
        dmac_ch_init(&dma_rec_ch, &dma_rec_ch_cfg);
        if (rec_locked)
            dmac_ch_lock_int(&dma_rec_ch);
        I2SRXCON = 0x1000;
        I2SRXCOM = 0x2;
    }
#endif
}
#endif
