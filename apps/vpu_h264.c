/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * S5L8702 VPU-B H.264 Baseline hardware decoder implementation.
 *
 * Extracted from h264_poc.c v56 (bit-perfect, 16/16 frames vs ffmpeg).
 * All code below is proven working on iPod Classic 6G hardware.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ********************************************************************* * Copyright (C) 2025 Nux Li
 *
 *******/

#include "config.h"

#ifdef IPOD_6G

#include "system.h"
#include "kernel.h"
#include "s5l87xx.h"
#include "vpu_h264.h"
#include "target/arm/s5l8702/ipod6g/vpu-6g.h"

#include <string.h>

/* ---- VPU-B hardware registers (0x39800000) ---- */

#define VPU_B_BASE      0x39800000
#define VPU_B(off)      (*(volatile uint32_t *)(VPU_B_BASE + (off)))

#define VPU_DPB_Y(i)    VPU_B((i) * 12)
#define VPU_DPB_CB(i)   VPU_B((i) * 12 + 4)
#define VPU_DPB_CR(i)   VPU_B((i) * 12 + 8)
#define VPU_OUT_Y        VPU_B(0xCC)
#define VPU_OUT_CB       VPU_B(0xD0)
#define VPU_OUT_CR       VPU_B(0xD4)
#define VPU_CTRL_BUF     VPU_B(0xD8)
#define VPU_SLICE_DESC   VPU_B(0xDC)
#define VPU_DIMS         VPU_B(0xE0)
#define VPU_STRIDES      VPU_B(0xE4)
#define VPU_CTRL         VPU_B(0xE8)
#define VPU_STATUS0      VPU_B(0xF0)
#define VPU_STATUS1      VPU_B(0xF4)
#define VPU_CONFIG       VPU_B(0x118)

#define VPU_CONFIG_CONST    0x82625A00
#define VPU_SLICE_CONST     0x00110C85
#define VPU_TRIGGER_BITS    0x88003001

#define VPU_MODE_REG    (*(volatile uint32_t *)0x38100314)
#define CLK_BASE        0x3C500000
#define REG32(addr)     (*(volatile uint32_t *)(addr))

#define ALIGN32(x)  (((uintptr_t)(x) + 31) & ~31)
#define ALIGN4K(x)  (((uintptr_t)(x) + 0xFFF) & ~0xFFF)
#define PHYS(x)     ((uint32_t)((uintptr_t)(x) & 0x7FFFFFFF))
#define UNCACHED(x) ((typeof(x))((uintptr_t)(x) + 0x40000000))

#define BS_DMA_SIZE     262144
#define SLICE_DESC_SIZE 320
#define SLICE_DESC_WORDS 8
#define VPU_MAX_SLICES  (SLICE_DESC_SIZE / (SLICE_DESC_WORDS * sizeof(uint32_t)))

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* ---- Bitstream reader (exp-Golomb) ---- */

struct bs {
    const uint8_t *buf;
    unsigned long bit_offset;
    unsigned long bit_length;
    int error;
};

static uint32_t bs_un(struct bs *b, int n)
{
    uint32_t val = 0;
    int i;
    for (i = 0; i < n; i++)
    {
        if (b->bit_offset >= b->bit_length)
        {
            b->error = 1;
            return 0;
        }
        unsigned long byte_pos = b->bit_offset >> 3;
        unsigned int bit_pos = 7 - (b->bit_offset & 7);
        val = (val << 1) | ((b->buf[byte_pos] >> bit_pos) & 1);
        b->bit_offset++;
    }
    return val;
}

static uint32_t bs_u1(struct bs *b) { return bs_un(b, 1); }
static uint32_t bs_u8(struct bs *b) { return bs_un(b, 8); }

static uint32_t bs_ue(struct bs *b)
{
    int lz = 0;
    while (bs_u1(b) == 0 && lz < 31) lz++;
    if (b->error || lz >= 31)
    {
        b->error = 1;
        return 0;
    }
    if (lz == 0) return 0;
    if (b->bit_offset >= b->bit_length)
    {
        b->error = 1;
        return 0;
    }
    return (1 << lz) - 1 + bs_un(b, lz);
}

static int bs_se(struct bs *b)
{
    uint32_t v = bs_ue(b);
    return (v & 1) ? (int)((v + 1) >> 1) : -(int)(v >> 1);
}

/* ---- EBSP to RBSP conversion ---- */

static int ebsp_to_rbsp(uint8_t *dst, const uint8_t *src, int src_len)
{
    int di = 0, si;
    for (si = 0; si < src_len; si++)
    {
        if (si + 2 < src_len && src[si] == 0 && src[si+1] == 0 &&
            src[si+2] == 3)
        {
            dst[di++] = 0;
            dst[di++] = 0;
            si += 2;
        }
        else
        {
            dst[di++] = src[si];
        }
    }
    return di;
}

/* Map an RBSP byte offset back to the corresponding EBSP byte offset.
 * The VPU expects EBSP data (handles EPBs internally), so after parsing
 * the slice header in RBSP space we need to find where the slice body
 * starts in the original EBSP stream. */
static int map_rbsp_to_ebsp(const uint8_t *ebsp, int ebsp_len, int rbsp_pos)
{
    int ri = 0, si = 0;
    while (si < ebsp_len && ri < rbsp_pos)
    {
        if (si + 2 < ebsp_len &&
            ebsp[si] == 0 && ebsp[si+1] == 0 && ebsp[si+2] == 3)
        {
            if (ri + 1 >= rbsp_pos)
                return si + (rbsp_pos - ri);
            ri += 2;
            si += 3;
        }
        else
        {
            ri++;
            si++;
        }
    }
    return si;
}

/* ---- H.264 header structures ---- */

struct sps {
    int profile_idc, level_idc;
    int pic_width_in_mbs_minus1, pic_height_in_map_units_minus1;
    int log2_max_frame_num_minus4;
    int pic_order_cnt_type, log2_max_pic_order_cnt_lsb_minus4;
    int max_num_ref_frames;
    int gaps_allowed;
    int frame_mbs_only;
    int crop_left, crop_right, crop_top, crop_bottom;
};

struct pps {
    int pic_init_qp_minus26, chroma_qp_index_offset;
    int deblocking_filter_control_present_flag, weighted_pred_flag;
    int num_ref_idx_l0_default_active_minus1;
    int entropy_coding_mode;
    int bottom_field_pic_order;
    int num_slice_groups_minus1;
    int weighted_bipred_idc;
};

struct slice_hdr {
    int first_mb_in_slice, slice_type, slice_qp_delta;
    int disable_deblocking_filter_idc;
    int alpha_c0_offset_div2, beta_offset_div2;
    int frame_num;
    int num_ref_idx_l0_active_minus1;
    int ref_list_modification;
    int adaptive_ref_marking;
    unsigned long bits_consumed;
};

/* ---- H.264 header parsers ---- */

static void parse_sps(struct sps *sps, struct bs *b)
{
    b->bit_offset = 8;
    sps->profile_idc = bs_u8(b);
    bs_un(b, 8);
    sps->level_idc = bs_u8(b);
    bs_ue(b);

    if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244)
    {
        int cfi = bs_ue(b);
        if (cfi == 3) bs_u1(b);
        bs_ue(b); bs_ue(b); bs_u1(b);
        if (bs_u1(b))
        {
            int cnt = (cfi != 3) ? 8 : 12, j;
            for (j = 0; j < cnt; j++)
                if (bs_u1(b))
                {
                    int sz = (j < 6) ? 16 : 64, k;
                    int last = 8, next = 0;
                    for (k = 0; k < sz; k++)
                    {
                        if (next != 0) next = (last + bs_se(b) + 256) % 256;
                        last = (next == 0) ? last : next;
                    }
                }
        }
    }

    sps->log2_max_frame_num_minus4 = bs_ue(b);
    sps->pic_order_cnt_type = bs_ue(b);
    if (sps->pic_order_cnt_type == 0)
    {
        sps->log2_max_pic_order_cnt_lsb_minus4 = bs_ue(b);
    }
    else if (sps->pic_order_cnt_type == 1)
    {
        bs_u1(b); bs_se(b); bs_se(b);
        int nrf = bs_ue(b);
        int j;
        for (j = 0; j < nrf; j++) bs_se(b);
    }
    sps->max_num_ref_frames = bs_ue(b);
    sps->gaps_allowed = bs_u1(b);
    sps->pic_width_in_mbs_minus1 = bs_ue(b);
    sps->pic_height_in_map_units_minus1 = bs_ue(b);
    sps->frame_mbs_only = bs_u1(b);
    if (!sps->frame_mbs_only)
        bs_u1(b);
    bs_u1(b); /* direct_8x8_inference_flag */
    sps->crop_left = 0;
    sps->crop_right = 0;
    sps->crop_top = 0;
    sps->crop_bottom = 0;
    if (bs_u1(b)) /* frame_cropping_flag */
    {
        /* Baseline profile is always progressive 4:2:0 here, so both crop
         * units are two pixels. */
        sps->crop_left = bs_ue(b) * 2;
        sps->crop_right = bs_ue(b) * 2;
        sps->crop_top = bs_ue(b) * 2;
        sps->crop_bottom = bs_ue(b) * 2;
    }
}

static void parse_pps(struct pps *pps, struct bs *b)
{
    b->bit_offset = 8;
    bs_ue(b); bs_ue(b);
    pps->entropy_coding_mode = bs_u1(b);
    pps->bottom_field_pic_order = bs_u1(b);
    pps->num_slice_groups_minus1 = bs_ue(b);
    pps->num_ref_idx_l0_default_active_minus1 = bs_ue(b);
    bs_ue(b);
    pps->weighted_pred_flag = bs_u1(b);
    pps->weighted_bipred_idc = bs_un(b, 2);
    pps->pic_init_qp_minus26 = bs_se(b);
    bs_se(b);
    pps->chroma_qp_index_offset = bs_se(b);
    pps->deblocking_filter_control_present_flag = bs_u1(b);
}

static void parse_slice_header(const struct sps *sps, const struct pps *pps,
                                struct bs *b, int nal_type, int nal_ref_idc,
                                struct slice_hdr *sh)
{
    b->bit_offset = 8;
    sh->first_mb_in_slice = bs_ue(b);
    sh->slice_type = bs_ue(b);
    if (sh->slice_type >= 5) sh->slice_type -= 5;
    bs_ue(b); /* pic_parameter_set_id */
    sh->frame_num = bs_un(b, sps->log2_max_frame_num_minus4 + 4);

    if (nal_type == 5)
        bs_ue(b); /* idr_pic_id */
    if (sps->pic_order_cnt_type == 0)
    {
        bs_un(b, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
        /* QuickTime 7.6.6's iPod preset sets
         * bottom_field_pic_order_in_frame_present_flag even though the
         * sequence is progressive, and writes delta_pic_order_cnt_bottom. */
        if (pps->bottom_field_pic_order)
            bs_se(b);
    }

    sh->num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
    sh->ref_list_modification = 0;
    sh->adaptive_ref_marking = 0;
    if (sh->slice_type == 0 || sh->slice_type == 1)
    {
        if (bs_u1(b))
        {
            sh->num_ref_idx_l0_active_minus1 = bs_ue(b);
            if (sh->slice_type == 1)
                bs_ue(b);
        }
    }

    /* ref_pic_list_modification */
    if (sh->slice_type == 0 || sh->slice_type == 1)
    {
        if (bs_u1(b))
        {
            uint32_t idc;
            sh->ref_list_modification = 1;
            do {
                idc = bs_ue(b);
                if (idc == 0 || idc == 1) bs_ue(b);
                else if (idc == 2) bs_ue(b);
            } while (idc != 3 && b->bit_offset < b->bit_length);
        }
        if (sh->slice_type == 1)
        {
            if (bs_u1(b))
            {
                uint32_t idc;
                sh->ref_list_modification = 1;
                do {
                    idc = bs_ue(b);
                    if (idc == 0 || idc == 1) bs_ue(b);
                    else if (idc == 2) bs_ue(b);
                } while (idc != 3 && b->bit_offset < b->bit_length);
            }
        }
    }

    /* dec_ref_pic_marking */
    if (nal_ref_idc != 0)
    {
        if (nal_type == 5)
        {
            bs_u1(b); bs_u1(b);
        }
        else
        {
            if (bs_u1(b))
            {
                uint32_t mmco;
                sh->adaptive_ref_marking = 1;
                do {
                    mmco = bs_ue(b);
                    if (mmco == 1 || mmco == 3) bs_ue(b);
                    if (mmco == 2) bs_ue(b);
                    if (mmco == 3 || mmco == 6) bs_ue(b);
                    if (mmco == 4) bs_ue(b);
                } while (mmco != 0);
            }
        }
    }

    sh->slice_qp_delta = bs_se(b);
    sh->disable_deblocking_filter_idc = 0;
    sh->alpha_c0_offset_div2 = 0;
    sh->beta_offset_div2 = 0;

    if (pps->deblocking_filter_control_present_flag)
    {
        sh->disable_deblocking_filter_idc = bs_ue(b);
        if (sh->disable_deblocking_filter_idc != 1)
        {
            sh->alpha_c0_offset_div2 = bs_se(b);
            sh->beta_offset_div2 = bs_se(b);
        }
    }

    sh->bits_consumed = b->bit_offset;
}

/* ---- Slice descriptor builder ---- */

static void build_slice_descriptor(uint32_t *desc,
                                    int first_mb,
                                    int mb_count,
                                    int slice_type,
                                    int slice_qp_y,
                                    int chroma_qp_off,
                                    int weighted_pred,
                                    int num_ref_l0,
                                    int deblk_idc,
                                    int alpha_off,
                                    int beta_off,
                                    int bit_offset,
                                    uint32_t bs_phys_addr,
                                    int bs_length)
{
    /* Word 0 carries the first macroblock and the number covered by this
     * descriptor.  Apple's iPod preset emits two half-picture slices. */
    desc[0] = ((first_mb & 0x7FF) << 21)
            | ((mb_count & 0x7FF) << 10)
            | ((slice_type & 0xF) << 6)
            | (slice_qp_y & 0x3F);

    desc[1] = ((chroma_qp_off & 0xFF) << 15)
            | ((weighted_pred & 1) << 14)
            | ((num_ref_l0 & 0xF) << 10)
            | ((deblk_idc & 0x3) << 8)
            | ((alpha_off & 0xF) << 4)
            | (beta_off & 0xF);

    desc[2] = VPU_SLICE_CONST;
    desc[3] = 0;
    desc[4] = bit_offset & 0xFF;
    desc[5] = bs_phys_addr;
    desc[6] = bs_length;
    desc[7] = 0;
}

/* ---- VPU-B power management ---- */

static void vpub_power_on(void)
{
    uint32_t cg, pw;

    /* Gate VPP clocks + VPU-B clock as initial state */
    VPU_MODE_REG &= ~1;
    PWRCON(0) |= (1 << 17) | (7 << 14);
    /* No delay: Apple FW, all Rockbox drivers use zero delay after PWRCON */

    /* Enable CG16_SVID (PLL2 source, already locked as system clock) */
    cg = REG32(CLK_BASE + 0x08);
    cg &= ~0x80000000;
    cg |= 0x30000000;
    REG32(CLK_BASE + 0x08) = cg;

    /* Enable VPP clocks */
    pw = PWRCON(0);
    PWRCON(0) = pw & ~(7 << 14);

    /* H.264 mode */
    VPU_MODE_REG |= 1;

    /* Zero stale registers */
    PWRCON(0) = PWRCON(0) & ~(1 << 17);
    {
        volatile uint32_t *base = (volatile uint32_t *)VPU_B_BASE;
        int i;
        for (i = 0; i < 0x130/4; i++)
            base[i] = 0;
    }
    PWRCON(0) = PWRCON(0) | (1 << 17);

    /* Initialize IRQ 35 for completion signaling */
    vpu_irq_init();
}

static void vpub_power_off(void)
{
    VPU_MODE_REG &= ~1;
    PWRCON(0) |= (1 << 17) | (7 << 14);
}

/* ---- VPU-B decode trigger ---- */

static int vpub_decode(uint32_t ctrl_phys, uint32_t desc_phys,
                        uint32_t y_phys, uint32_t cb_phys, uint32_t cr_phys,
                        const uint32_t *dpb_y, const uint32_t *dpb_cb,
                        const uint32_t *dpb_cr, int dpb_count,
                        int width_mbs, int height_mbs,
                        int pic_w, int num_slices)
{
    uint32_t val;
    int ret, old_irq;

    /* Double clock enable (Apple: 0x1c0b94 + 0x1c0ba4) */
    old_irq = disable_irq_save();
    PWRCON(0) = PWRCON(0) & ~(1 << 17);
    restore_irq(old_irq);
    old_irq = disable_irq_save();
    PWRCON(0) = PWRCON(0) & ~(1 << 17);
    restore_irq(old_irq);

    /* Re-set VPU_MODE every frame (Apple: 0x1c0bac) */
    VPU_MODE_REG |= 1;

    /* vtable[0x48] = h264_vpu_stop_and_reset (0x001c0b4c) */
    {
        volatile uint32_t *base = (volatile uint32_t *)VPU_B_BASE;
        uint32_t ec_val;
        int i;
        ec_val = base[0xEC/4];
        base[0xEC/4] = ec_val & ~1u;
        ec_val = base[0xEC/4];
        (void)ec_val;
        for (i = 0; i < 0x130/4; i++)
            base[i] = 0;
    }

    /* Program registers (Apple's exact order from FUN_001c06ac) */
    VPU_CTRL_BUF = ctrl_phys;
    val = VPU_DIMS;
    val = (val & ~0x3F00) | ((height_mbs & 0x3F) << 8);
    VPU_DIMS = val;
    val = VPU_DIMS;
    val = (val & ~0x003F) | (width_mbs & 0x3F);
    VPU_DIMS = val;
    val = VPU_CTRL;
    val = (val & ~0x0FFE) | ((width_mbs * height_mbs * 2) & 0xFFE);
    VPU_CTRL = val;
    VPU_SLICE_DESC = desc_phys;
    val = VPU_STRIDES;
    val = (val & ~0x01FF0000) | (((pic_w / 2) & 0x1FF) << 16);
    VPU_STRIDES = val;
    val = VPU_STRIDES;
    val = (val & ~0x000003FF) | (pic_w & 0x3FF);
    VPU_STRIDES = val;
    VPU_OUT_Y  = y_phys;
    VPU_OUT_CB = cb_phys;
    VPU_OUT_CR = cr_phys;
    VPU_CONFIG = VPU_CONFIG_CONST;

    /* DPB reference frame table — fill slots with reference frames
     * (newest at slot 0). Remaining slots get slot 0's data. */
    {
        int dpb;
        for (dpb = 0; dpb < dpb_count && dpb < 17; dpb++)
        {
            VPU_DPB_Y(dpb)  = dpb_y[dpb];
            VPU_DPB_CB(dpb) = dpb_cb[dpb];
            VPU_DPB_CR(dpb) = dpb_cr[dpb];
        }
        for (; dpb < 17; dpb++)
        {
            VPU_DPB_Y(dpb)  = dpb_count > 0 ? dpb_y[0] : 0;
            VPU_DPB_CB(dpb) = dpb_count > 0 ? dpb_cb[0] : 0;
            VPU_DPB_CR(dpb) = dpb_count > 0 ? dpb_cr[0] : 0;
        }
    }

    /* Slice count + trigger */
    val = VPU_CTRL;
    val = (val & ~0x07FF0000) | ((num_slices & 0x7FF) << 16);
    VPU_CTRL = val;

    /* Arm IRQ, trigger, wait */
    vpu_irq_arm();
    VPU_CTRL = VPU_CTRL | VPU_TRIGGER_BITS;

    ret = vpu_irq_wait(HZ / 2);
    if (ret == OBJ_WAIT_TIMEDOUT)
    {
        old_irq = disable_irq_save();
        PWRCON(0) = PWRCON(0) | (1 << 17);
        restore_irq(old_irq);
        return -1;
    }

    val = vpu_irq_status1();
    ret = 0;
    if ((val << 3) >> 21)
        ret = -2;

    if (ret == 0)
        VPU_CONFIG = VPU_CONFIG_CONST;

    /* Disable clock */
    old_irq = disable_irq_save();
    PWRCON(0) = PWRCON(0) | (1 << 17);
    restore_irq(old_irq);

    return ret;
}

/* ---- Internal context structure ---- */

#define MAX_DPB_FRAMES 5  /* up to 4 reference frames + 1 output */

struct vpu_h264 {
    int max_w, max_h;
    int pic_w, pic_h, pic_wmb, pic_hmb;
    int display_w, display_h;
    struct sps sps;
    struct pps pps;
    int have_sps, have_pps;
    int cur_out;                        /* next output buffer index */
    int last_out;                       /* most recently decoded picture */
    int dpb_count;                      /* valid references in dpb_order */
    int dpb_order[MAX_DPB_FRAMES];      /* buffer indices, oldest first */
    uint8_t *ctrl_buf;
    uint8_t *slice_desc;
    uint8_t *bs_dma;
    uint8_t *nalu_buf;
    uint8_t *frame_y[MAX_DPB_FRAMES];
    uint8_t *frame_cb[MAX_DPB_FRAMES];
    uint8_t *frame_cr[MAX_DPB_FRAMES];
    int frame_y_size;
    int frame_cb_size;
    int frame_cr_size;
};

static int apply_sps_dimensions(struct vpu_h264 *v)
{
    int horizontal_crop;
    int vertical_crop;

    v->pic_wmb = v->sps.pic_width_in_mbs_minus1 + 1;
    v->pic_hmb = v->sps.pic_height_in_map_units_minus1 + 1;
    v->pic_w = v->pic_wmb * 16;
    v->pic_h = v->pic_hmb * 16;
    horizontal_crop = v->sps.crop_left + v->sps.crop_right;
    vertical_crop = v->sps.crop_top + v->sps.crop_bottom;
    v->display_w = v->pic_w - horizontal_crop;
    v->display_h = v->pic_h - vertical_crop;

    if (v->pic_w <= 0 || v->pic_h <= 0 ||
        v->pic_w > v->max_w || v->pic_h > v->max_h ||
        v->display_w <= 0 || v->display_h <= 0 ||
        (v->display_w & 1) || (v->display_h & 1))
        return -1;

    v->frame_y_size = v->pic_w * v->pic_h;
    v->frame_cb_size = (v->pic_w / 2) * (v->pic_h / 2);
    v->frame_cr_size = v->frame_cb_size;
    return 0;
}

/* ---- Public API ---- */

size_t vpu_h264_buf_size(int max_w, int max_h)
{
    size_t sz = 0;
    sz += sizeof(struct vpu_h264) + 32;
    sz += (size_t)max_w * max_h * 3 / 2 + 4096;                    /* ctrl_buf */
    sz += SLICE_DESC_SIZE + 32;
    sz += BS_DMA_SIZE + 32;
    sz += BS_DMA_SIZE + 32;                                         /* nalu_buf */
    sz += ((size_t)max_w * max_h + 4096) * MAX_DPB_FRAMES;         /* frame_y */
    sz += ((size_t)max_w * max_h / 4 + 32) * MAX_DPB_FRAMES;       /* frame_cb */
    sz += ((size_t)max_w * max_h / 4 + 32) * MAX_DPB_FRAMES;       /* frame_cr */
    return sz;
}

struct vpu_h264 *vpu_h264_open(void *buf, size_t buf_size,
                                int max_w, int max_h)
{
    uint8_t *p = (uint8_t *)buf;
    struct vpu_h264 *v;
    int fy_sz, fc_sz, ctrl_sz;

    if (!buf || buf_size < vpu_h264_buf_size(max_w, max_h))
        return NULL;
    if (max_w <= 0 || max_h <= 0 || max_w > 1280 || max_h > 720)
        return NULL;

    v = (struct vpu_h264 *)ALIGN32(p);
    p = (uint8_t *)v + sizeof(*v);
    memset(v, 0, sizeof(*v));

    v->max_w = max_w;
    v->max_h = max_h;

    ctrl_sz = max_w * max_h * 3 / 2;
    fy_sz = max_w * max_h;
    fc_sz = max_w * max_h / 4;

    v->ctrl_buf   = (uint8_t *)ALIGN4K(p);  p = v->ctrl_buf + ctrl_sz;
    v->slice_desc = (uint8_t *)ALIGN32(p);  p = v->slice_desc + SLICE_DESC_SIZE;
    v->bs_dma     = (uint8_t *)ALIGN32(p);  p = v->bs_dma + BS_DMA_SIZE;
    v->nalu_buf   = (uint8_t *)ALIGN32(p);  p = v->nalu_buf + BS_DMA_SIZE;

    {
        int i;
        for (i = 0; i < MAX_DPB_FRAMES; i++)
        {
            v->frame_y[i]  = (uint8_t *)ALIGN4K(p); p = v->frame_y[i] + fy_sz;
            v->frame_cb[i] = (uint8_t *)ALIGN32(p); p = v->frame_cb[i] + fc_sz;
            v->frame_cr[i] = (uint8_t *)ALIGN32(p); p = v->frame_cr[i] + fc_sz;
        }
    }

    v->frame_y_size = fy_sz;
    v->frame_cb_size = fc_sz;
    v->frame_cr_size = fc_sz;
    v->cur_out = 0;
    v->dpb_count = 0;

    /* Zero control buffers */
    memset(v->ctrl_buf, 0, ctrl_sz);
    memset(v->bs_dma, 0, BS_DMA_SIZE);

    /* Pre-fill frame buffers with neutral YCbCr (black) */
    {
        int i;
        for (i = 0; i < MAX_DPB_FRAMES; i++)
        {
            memset(v->frame_y[i],  0x10, fy_sz);
            memset(v->frame_cb[i], 0x80, fc_sz);
            memset(v->frame_cr[i], 0x80, fc_sz);
        }
    }

    vpub_power_on();
    return v;
}

int vpu_h264_configure(struct vpu_h264 *v,
                        const uint8_t *avcc, int avcc_len)
{
    int i, cnt, offset;
    int nalu_len_size;
    struct bs b;

    if (!v || !avcc || avcc_len < 7)
        return -1;

    /* avcC structure: version(1) profile(1) compat(1) level(1)
     * nalu_len_size(1) num_sps(1) [sps_len(2) sps_data]...
     * num_pps(1) [pps_len(2) pps_data]... */
    nalu_len_size = (avcc[4] & 0x03) + 1;
    (void)nalu_len_size;

    /* Parse SPS */
    cnt = avcc[5] & 0x1F;
    offset = 6;
    for (i = 0; i < cnt && offset + 2 <= avcc_len; i++)
    {
        int sps_len = (avcc[offset] << 8) | avcc[offset + 1];
        offset += 2;
        if (offset + sps_len > avcc_len)
            return -1;
        int rbsp_len = ebsp_to_rbsp(v->nalu_buf, avcc + offset, sps_len);
        b.buf = v->nalu_buf;
        b.bit_offset = 0;
        b.bit_length = rbsp_len * 8;
        b.error = 0;
        parse_sps(&v->sps, &b);

        if (b.error || v->sps.profile_idc != 66 ||
            v->sps.level_idc > 30 || v->sps.pic_order_cnt_type != 0 ||
            v->sps.max_num_ref_frames > 2 || v->sps.gaps_allowed ||
            !v->sps.frame_mbs_only)
            return -1;

        if (apply_sps_dimensions(v) < 0)
            return -1;
        v->have_sps = 1;
        offset += sps_len;
    }

    /* Parse PPS */
    if (offset >= avcc_len)
        return -1;
    cnt = avcc[offset++];
    for (i = 0; i < cnt && offset + 2 <= avcc_len; i++)
    {
        int pps_len = (avcc[offset] << 8) | avcc[offset + 1];
        offset += 2;
        if (offset + pps_len > avcc_len)
            return -1;
        int rbsp_len = ebsp_to_rbsp(v->nalu_buf, avcc + offset, pps_len);
        b.buf = v->nalu_buf;
        b.bit_offset = 0;
        b.bit_length = rbsp_len * 8;
        b.error = 0;
        parse_pps(&v->pps, &b);
        if (b.error || v->pps.entropy_coding_mode ||
            v->pps.num_slice_groups_minus1 != 0 ||
            v->pps.weighted_pred_flag ||
            v->pps.weighted_bipred_idc != 0 ||
            v->pps.num_ref_idx_l0_default_active_minus1 != 0)
            return -1;
        v->have_pps = 1;
        offset += pps_len;
    }

    return (v->have_sps && v->have_pps) ? 0 : -1;
}

struct picture_slice {
    const uint8_t *nalu;
    int nalu_len;
    int nal_type;
    int nal_ref_idc;
    struct slice_hdr header;
};

static int parse_picture_slice(struct vpu_h264 *v, const uint8_t *nalu,
                               int nalu_len, struct picture_slice *slice)
{
    struct bs b;
    int rbsp_len;

    if (!v->have_sps || !v->have_pps || nalu_len < 1)
        return -1;
    rbsp_len = ebsp_to_rbsp(v->nalu_buf, nalu, MIN(nalu_len, 256));
    slice->nalu = nalu;
    slice->nalu_len = nalu_len;
    slice->nal_type = v->nalu_buf[0] & 0x1f;
    slice->nal_ref_idc = (v->nalu_buf[0] >> 5) & 3;
    b.buf = v->nalu_buf;
    b.bit_offset = 0;
    b.bit_length = rbsp_len * 8;
    b.error = 0;
    parse_slice_header(&v->sps, &v->pps, &b, slice->nal_type,
                       slice->nal_ref_idc, &slice->header);
    if (b.error ||
        (slice->header.slice_type != 0 && slice->header.slice_type != 2) ||
        slice->header.num_ref_idx_l0_active_minus1 != 0 ||
        slice->header.ref_list_modification ||
        slice->header.adaptive_ref_marking)
        return -1;
    return 0;
}

static int decode_picture(struct vpu_h264 *v, struct picture_slice *slices,
                          int slice_count)
{
    uint32_t dpb_y[MAX_DPB_FRAMES], dpb_cb[MAX_DPB_FRAMES];
    uint32_t dpb_cr[MAX_DPB_FRAMES];
    int total_mbs = v->pic_wmb * v->pic_hmb;
    int out_buf = v->cur_out;
    int dpb_fill = 0;
    int dma_used = 0;
    int is_idr;
    int ret;
    int i;

    if (slice_count < 1 || slice_count > (int)VPU_MAX_SLICES ||
        slices[0].header.first_mb_in_slice != 0)
        return -1;
    is_idr = slices[0].nal_type == 5;
    for (i = 0; i < slice_count; i++)
    {
        if ((slices[i].nal_type != 1 && slices[i].nal_type != 5) ||
            (slices[i].nal_type == 5) != is_idr ||
            slices[i].nal_ref_idc != slices[0].nal_ref_idc ||
            slices[i].header.frame_num != slices[0].header.frame_num ||
            slices[i].header.slice_type != slices[0].header.slice_type ||
            slices[i].header.first_mb_in_slice < 0 ||
            slices[i].header.first_mb_in_slice >= total_mbs ||
            (i > 0 && slices[i].header.first_mb_in_slice <=
                       slices[i - 1].header.first_mb_in_slice))
            return -1;
    }

    if (is_idr)
        v->dpb_count = 0;
    for (i = v->dpb_count - 1; i >= 0 && dpb_fill < MAX_DPB_FRAMES; i--)
    {
        int bi = v->dpb_order[i];
        dpb_y[dpb_fill] = PHYS(v->frame_y[bi]);
        dpb_cb[dpb_fill] = PHYS(v->frame_cb[bi]);
        dpb_cr[dpb_fill] = PHYS(v->frame_cr[bi]);
        dpb_fill++;
    }

    memset(UNCACHED(v->slice_desc), 0, SLICE_DESC_SIZE);
    for (i = 0; i < slice_count; i++)
    {
        struct slice_hdr *sh = &slices[i].header;
        int next_mb = i + 1 < slice_count ?
            slices[i + 1].header.first_mb_in_slice : total_mbs;
        int mb_count = next_mb - sh->first_mb_in_slice;
        int slice_qp = 26 + v->pps.pic_init_qp_minus26 +
                       sh->slice_qp_delta;
        int hdr_bytes = sh->bits_consumed / 8;
        int bit_off = sh->bits_consumed & 7;
        int ebsp_hdr_offset = map_rbsp_to_ebsp(
            slices[i].nalu, slices[i].nalu_len, hdr_bytes);
        int dma_len = slices[i].nalu_len - ebsp_hdr_offset;
        int dma_offset = (dma_used + 31) & ~31;

        if (mb_count <= 0 || slice_qp < 0 || slice_qp > 51 ||
            dma_len <= 0 || dma_offset > BS_DMA_SIZE - dma_len)
            return -1;
        memcpy(UNCACHED(v->bs_dma + dma_offset),
               slices[i].nalu + ebsp_hdr_offset, dma_len);
        build_slice_descriptor(
            (uint32_t *)UNCACHED(v->slice_desc) +
                i * SLICE_DESC_WORDS,
            sh->first_mb_in_slice, mb_count, sh->slice_type, slice_qp,
            v->pps.chroma_qp_index_offset, v->pps.weighted_pred_flag,
            sh->num_ref_idx_l0_active_minus1,
            sh->disable_deblocking_filter_idc,
            sh->alpha_c0_offset_div2, sh->beta_offset_div2,
            bit_off, PHYS(v->bs_dma + dma_offset), dma_len);
        dma_used = dma_offset + dma_len;
    }

    ret = vpub_decode(
        PHYS(v->ctrl_buf), PHYS(v->slice_desc),
        PHYS(v->frame_y[out_buf]), PHYS(v->frame_cb[out_buf]),
        PHYS(v->frame_cr[out_buf]), dpb_y, dpb_cb, dpb_cr, dpb_fill,
        v->pic_wmb, v->pic_hmb, v->pic_w, slice_count);
    commit_discard_dcache();
    if (ret != 0)
        return -1;

    v->last_out = out_buf;
    if (slices[0].nal_ref_idc != 0 && v->sps.max_num_ref_frames > 0)
    {
        int max_ref = MIN(v->sps.max_num_ref_frames,
                          MAX_DPB_FRAMES - 1);
        while (v->dpb_count >= max_ref)
        {
            for (i = 0; i < v->dpb_count - 1; i++)
                v->dpb_order[i] = v->dpb_order[i + 1];
            v->dpb_count--;
        }
        v->dpb_order[v->dpb_count++] = out_buf;
    }
    v->cur_out = (out_buf + 1) % MAX_DPB_FRAMES;
    return 1;
}

int vpu_h264_decode_nalu(struct vpu_h264 *v,
                         const uint8_t *nalu, int nalu_len)
{
    struct picture_slice slice;
    struct bs b;
    int rbsp_len;
    int nal_type;

    if (!v || !nalu || nalu_len < 1)
        return -1;
    rbsp_len = ebsp_to_rbsp(v->nalu_buf, nalu, MIN(nalu_len, 256));
    nal_type = v->nalu_buf[0] & 0x1f;
    b.buf = v->nalu_buf;
    b.bit_offset = 0;
    b.bit_length = rbsp_len * 8;
    b.error = 0;

    if (nal_type == 7)
    {
        parse_sps(&v->sps, &b);
        if (b.error || v->sps.profile_idc != 66 ||
            v->sps.level_idc > 30 || v->sps.pic_order_cnt_type != 0 ||
            v->sps.max_num_ref_frames > 2 || v->sps.gaps_allowed ||
            !v->sps.frame_mbs_only || apply_sps_dimensions(v) < 0)
            return -1;
        v->have_sps = 1;
        return 0;
    }
    if (nal_type == 8)
    {
        parse_pps(&v->pps, &b);
        if (b.error || v->pps.entropy_coding_mode ||
            v->pps.num_slice_groups_minus1 != 0 ||
            v->pps.weighted_pred_flag ||
            v->pps.weighted_bipred_idc != 0 ||
            v->pps.num_ref_idx_l0_default_active_minus1 != 0)
            return -1;
        v->have_pps = 1;
        return 0;
    }
    if (nal_type == 1 || nal_type == 5)
    {
        if (parse_picture_slice(v, nalu, nalu_len, &slice) < 0)
            return -1;
        return decode_picture(v, &slice, 1);
    }
    return 0;
}

int vpu_h264_decode_sample(struct vpu_h264 *v, const uint8_t *sample,
                           int sample_len, int nalu_len_size)
{
    struct picture_slice slices[VPU_MAX_SLICES];
    int slice_count = 0;
    int position = 0;

    if (!v || !sample || sample_len <= 0 ||
        nalu_len_size < 1 || nalu_len_size > 4)
        return -1;
    while (position + nalu_len_size <= sample_len)
    {
        uint32_t nalu_len = 0;
        int nal_type;
        int i;

        for (i = 0; i < nalu_len_size; i++)
            nalu_len = (nalu_len << 8) | sample[position + i];
        position += nalu_len_size;
        if (nalu_len == 0 || nalu_len > (uint32_t)(sample_len - position))
            return -1;
        nal_type = sample[position] & 0x1f;
        if (nal_type == 1 || nal_type == 5)
        {
            if (slice_count >= (int)VPU_MAX_SLICES ||
                parse_picture_slice(v, sample + position, nalu_len,
                                    &slices[slice_count]) < 0)
                return -1;
            slice_count++;
        }
        else if (nal_type == 7 || nal_type == 8)
        {
            if (vpu_h264_decode_nalu(v, sample + position, nalu_len) < 0)
                return -1;
        }
        position += nalu_len;
    }
    if (position != sample_len)
        return -1;
    return slice_count > 0 ? decode_picture(v, slices, slice_count) : 0;
}

void vpu_h264_get_frame(const struct vpu_h264 *v,
                        const uint8_t **y, const uint8_t **cb,
                        const uint8_t **cr, int *w, int *h, int *stride)
{
    int luma_offset = v->sps.crop_top * v->pic_w + v->sps.crop_left;
    int chroma_offset = (v->sps.crop_top / 2) * (v->pic_w / 2) +
                        v->sps.crop_left / 2;

    if (y) *y = v->frame_y[v->last_out] + luma_offset;
    if (cb) *cb = v->frame_cb[v->last_out] + chroma_offset;
    if (cr) *cr = v->frame_cr[v->last_out] + chroma_offset;
    if (w) *w = v->display_w;
    if (h) *h = v->display_h;
    if (stride) *stride = v->pic_w;
}

void vpu_h264_close(struct vpu_h264 *v)
{
    if (v)
        vpub_power_off();
}

#endif /* IPOD_6G */
