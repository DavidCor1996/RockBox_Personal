/***************************************************************************
 * Experimental S5L8702 composite video output.
 *
 * The candidate register sequence came from reverse engineering Apple's iPod
 * Classic 35.2.0.4 firmware. Its decrypted-image provenance still needs to be
 * reproduced; see docs/ipod6g-svid-recovery.md. It is deliberately exposed
 * only through the debug menu until physical sync, pattern, and framebuffer
 * qualification is complete.
 ***************************************************************************/

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "s5l87xx.h"
#include "clocking-s5l8702.h"
#include "events.h"
#include "backlight.h"
#include "lcd-s5l8702.h"
#include "videoout-6g.h"
#include "videoout.h"
#include <string.h>

#include <stdint.h>

#define SVID_COMPOSITOR_BASE 0x39100000u
#define SVID_ROUTER_BASE     0x39200000u
#define SVID_ENCODER_BASE    0x39300000u

/* RetailOS 35.2.0.4 format 8 selects the S5L8702 private planar layout.
 * In private plane mode 1, offsets 0x28, 0x2c, and 0x30 carry the three
 * YUV420 planes rather than the later public Samsung top/bottom-field layout. */
#define SVID_VP_ENABLE           0x000u
#define SVID_VP_MODE             0x010u
#define SVID_VP_PLANE0_PTR       0x028u
#define SVID_VP_PLANE2_PTR       0x02cu
#define SVID_VP_PLANE1_PTR       0x030u
#define SVID_VP_UNUSED_PTR       0x034u
#define SVID_VP_LIVE_PLANE0_PTR  0x190u
#define SVID_VP_LIVE_PLANE2_PTR  0x194u
#define SVID_VP_LIVE_PLANE1_PTR  0x198u
#define SVID_VP_IMG_WIDTH        0x03cu
#define SVID_VP_IMG_HEIGHT       0x040u
#define SVID_VP_SRC_H_POS        0x044u
#define SVID_VP_SRC_V_POS        0x048u
#define SVID_VP_SRC_WIDTH        0x04cu
#define SVID_VP_SRC_HEIGHT       0x050u
#define SVID_VP_DST_H_POS        0x054u
#define SVID_VP_DST_V_POS        0x058u
#define SVID_VP_DST_WIDTH        0x05cu
#define SVID_VP_DST_HEIGHT       0x060u
#define SVID_VP_H_RATIO          0x064u
#define SVID_VP_V_RATIO          0x068u
#define SVID_VP_PLANE_MODE       0x3c0u
#define SVID_VP_LUMA_SPAN        0x3c4u
#define SVID_VP_CHROMA_SPAN      0x3c8u
#define SVID_VP_ENDIAN_MODE      0x3ccu

#define SVID_VP_ENABLE_ON        (1u << 0)
#define SVID_VP_MODE_RETAIL_FMT8 0u
#define SVID_VP_PLANE_PLANAR     1u
#define SVID_VP_ENDIAN_LITTLE    (1u << 0)

/* The S5L8702 mixer predates the public S5P register layout.  RetailOS
 * 35.2.0.4 writes layer-0 base, position, and dimensions at 0x10, 0x14, and
 * 0x18.  Offset 0x14 is not a source stride. */
#define SVID_MXR_STATUS          0x000u
#define SVID_MXR_CONFIG          0x004u
#define SVID_MXR_VIDEO_CONFIG    0x008u
#define SVID_MXR_GRAPHIC0_CONFIG 0x00cu
#define SVID_MXR_GRAPHIC0_BASE   0x010u
#define SVID_MXR_GRAPHIC0_POS    0x014u
#define SVID_MXR_GRAPHIC0_SIZE   0x018u
#define SVID_MXR_GRAPHIC_FORMATS 0x040u
#define SVID_MXR_BG_COLOR0       0x048u
#define SVID_MXR_BG_COLOR1       0x04cu
#define SVID_MXR_BG_COLOR2       0x050u
#define SVID_MXR_COMMIT          0x800u

#define SVID_MXR_STATUS_IDLE     (1u << 1)
#define SVID_MXR_STATUS_SYNC     (1u << 2)
#define SVID_MXR_STATUS_RUN      (1u << 0)
#define SVID_MXR_GRAPHICS_ENABLE (1u << 3)
#define SVID_MXR_VIDEO_ENABLE    (1u << 4)
#define SVID_MXR_GRAPHIC4_ENABLE (1u << 5)
#define SVID_MXR_SD_SCAN         (1u << 1)
#define SVID_MXR_ENABLE_MASK     (SVID_MXR_GRAPHICS_ENABLE | \
                                  SVID_MXR_VIDEO_ENABLE | \
                                  SVID_MXR_GRAPHIC4_ENABLE)
#define SVID_MXR_ALPHA_OPAQUE    0xffu
/* RetailOS hardware format type 3 is the 16-bit path; type 7 is 32-bit. */
#define SVID_MXR_TYPE_RGB565     (1u << 17)
#define SVID_MXR_TYPE_XRGB8888   ((1u << 16) | (1u << 17) | (1u << 20))

#define SVID_SDO_CLOCK           0x000u
#define SVID_SDO_CONFIG          0x008u
#define SVID_SDO_DAC             0x03cu
#define SVID_SDO_FIELD_INFO      0x040u
#define SVID_SDO_SOFTWARE_RESET  (1u << 4)
#define SVID_SDO_CLOCK_ON        (1u << 0)
#define SVID_SDO_DAC_ALL_ON      7u
#define SVID_SDO_DAC_MUX_MASK    (0x3fu << 8)
#define SVID_SDO_COMPONENT       (1u << 6)
#define SVID_SDO_PROGRESSIVE     (1u << 4)
#define SVID_SDO_STANDARD_MASK   0xfu

#define SVID_NTSC_ACTIVE_WIDTH   640u
#define SVID_NTSC_ACTIVE_HEIGHT  480u
#define SVID_NTSC_OUTPUT_WIDTH   720u
#define SVID_NTSC_OUTPUT_HEIGHT  480u
/* Qualified on the DCP750: a centered 90% NTSC viewport keeps every
 * Rockbox UI pixel inside the panel's composite overscan area. */
#define SVID_UI_DESTINATION_X      36u
#define SVID_UI_DESTINATION_Y      24u
#define SVID_UI_DESTINATION_WIDTH  648u
#define SVID_UI_DESTINATION_HEIGHT 432u
/* RetailOS's nominal 4:3 movie path supplies private format 8 with a native
 * 320x240 planar source.  The VP, not software, scales that source into the
 * 720x480 NTSC destination.  This exact source geometry matters: the DCP750
 * repeatedly rejected the otherwise equivalent 640x240 software-expanded
 * layout at logical source row 120 regardless of image/source height values.
 * One Cb and one Cr sample cover a 2x2 block of native luma samples. */
#ifdef VIDEOOUT_ENHANCED_TEST
#define SVID_PLANAR_SCALE        2
#else
#define SVID_PLANAR_SCALE        1
#endif
/* Replicated guards keep horizontal filter reads within the same row.
 * RetailOS source X is in sixteenths of a pixel, independently of span. */
#if defined(VIDEOOUT_EDGE_GUARD_TEST) && defined(VIDEOOUT_ENHANCED_TEST)
#define SVID_PLANAR_GUARD        32
#else
#define SVID_PLANAR_GUARD        0
#endif
#define SVID_PLANAR_SOURCE_WIDTH (LCD_WIDTH * SVID_PLANAR_SCALE)
#define SVID_PLANAR_Y_WIDTH      (SVID_PLANAR_SOURCE_WIDTH + \
                                  2 * SVID_PLANAR_GUARD)
#define SVID_PLANAR_Y_HEIGHT     (LCD_HEIGHT * SVID_PLANAR_SCALE)
#define SVID_PLANAR_SOURCE_HEIGHT SVID_PLANAR_Y_HEIGHT
#define SVID_PLANAR_Y_SIZE       (SVID_PLANAR_Y_WIDTH * \
                                  SVID_PLANAR_Y_HEIGHT)
#define SVID_PLANAR_C_WIDTH      (SVID_PLANAR_Y_WIDTH / 2)
#define SVID_PLANAR_C_HEIGHT     (SVID_PLANAR_Y_HEIGHT / 2)
#define SVID_PLANAR_C_SIZE       (SVID_PLANAR_C_WIDTH * \
                                  SVID_PLANAR_C_HEIGHT)
#define SVID_PLANAR_FRAME_SIZE   (SVID_PLANAR_Y_SIZE + \
                                  2 * SVID_PLANAR_C_SIZE)
#define SVID_PLANAR_BUFFER_COUNT 2u
#define SVID_FIELD_EDGE_POLL_MAX 2500u

#define SVID_CLOCK_SOURCE    3
#define SVID_CLOCK_DIVIDER   4
#define SVID_CLOCK_POLL_MAX  1000
#define SVID_POWER_GATE_0    14
#define SVID_POWER_GATE_1    15
#define SVID_POWER_GATE_2    16
#define SVID_POWER_GATE_MASK ((1u << SVID_POWER_GATE_0) | \
                              (1u << SVID_POWER_GATE_1) | \
                              (1u << SVID_POWER_GATE_2))

#define SVID_GPIO_E4_VIDEO   0x000a040fu
#define SVID_GPIO_E4_MASK    (0xfu << 16)

#define SVID_REG(base, offset) \
    (*(volatile uint32_t *)((uintptr_t)(base) + (offset)))

struct svid_regval
{
    uint16_t offset;
    uint32_t value;
};

enum svid_planar_content
{
    SVID_PLANAR_FRAMEBUFFER = 0,
    SVID_PLANAR_BARS,
    SVID_PLANAR_GRID,
};

static int svid_tv_screen, svid_tv_overscan;
static bool svid_tv_canvas;
static struct videoout_tv_frame svid_frame;
static bool svid_frame_pending, svid_frame_owned;
/* Bounded presentation scratch, serialized by the LCD mutex. Calculate each
 * column once, never perform a software divide for every decoded pixel. */
static bool svid_ui_batch, svid_ui_owner;
static uint16_t svid_sample_x[SVID_PLANAR_SOURCE_WIDTH];
static struct videoout_rect svid_destination;
static bool svid_active;
static bool svid_layer_active;
static bool svid_published_active;
static bool svid_layer_xrgb;
static bool svid_layer_planar;
static bool svid_mirror_enabled;
static bool svid_bus_boosted;
/* 96 bytes total.  These target-owned tables replace three software
 * divisions per mirrored pixel without taking core or playback memory. */
static uint8_t svid_rgb5_to_8[32];
static uint8_t svid_rgb6_to_8[64];
static bool svid_rgb_expansion_ready;
static uint32_t svid_output_framebuffer[SVID_NTSC_ACTIVE_HEIGHT]
                                       [SVID_NTSC_ACTIVE_WIDTH]
#if SVID_PLANAR_GUARD > 0
                                       __attribute__((aligned(1024)));
#else
                                       CACHEALIGN_ATTR;
#endif
static uint8_t *svid_planar_write_buffer;
static unsigned svid_planar_front_buffer;
enum svid_surface_state { SVID_FREE, SVID_WRITING, SVID_READY, SVID_SCANNING };
static enum svid_surface_state svid_surface[2] = { SVID_SCANNING, SVID_FREE };
static const uint16_t *svid_framebuffer;
static int svid_framebuffer_width;
static int svid_framebuffer_height;
static const void *svid_policy_framebuffer;
static int svid_policy_width;
static int svid_policy_height;
static enum ipod6g_videoout_mode svid_mode = IPOD6G_VIDEOOUT_OFF;
static volatile enum ipod6g_videoout_accessory svid_accessory =
    IPOD6G_VIDEOOUT_ACCESSORY_NONE;
static volatile bool svid_policy_pending;
static bool svid_hibernate_restore_pending;
static bool svid_platform_saved;
static uint16_t svid_saved_clock;
static uint32_t svid_saved_power_gates;
static uint32_t svid_saved_gpio_e4;

static void svid_apply_policy(void);

static void svid_publish_active(void)
{
    bool active = svid_active && svid_layer_active;

    if (active == svid_published_active)
        return;

    svid_published_active = active;
    backlight_set_videoout_active(active);
    send_event(SYS_EVENT_VIDEOOUT_CHANGED, active ? (void *)1 : NULL);
}

static void svid_set_bus_boost(bool enable)
{
    if (enable == svid_bus_boosted)
        return;

    /* The VP memory reader underruns when Rockbox lowers HClk to 54 MHz.
     * Own one normal boost reference for the lifetime of every memory-backed
     * layer; this holds HClk at 108 MHz without interfering with other boost
     * owners, and the paired release restores normal power policy. */
    cpu_boost(enable);
    svid_bus_boosted = enable;
}

static const struct svid_regval compositor_init[] = {
    {0x004, 0}, {0x008, 0}, {0x00c, 0},
    {0x028, 0x08000000}, {0x02c, 0x08000000},
    {0x030, 0x08000000}, {0x034, 0x08000000},
    {0x038, 0x08000000},
    {0x03c, 64}, {0x040, 16}, {0x044, 0}, {0x048, 0},
    {0x04c, 64}, {0x050, 16}, {0x054, 0}, {0x058, 0},
    {0x05c, 64}, {0x060, 16}, {0x064, 512}, {0x068, 512},
    {0x3c0, 1}, {0x3cc, 1},
    {0x06c, 0x00070707}, {0x070, 0x07070707},
    {0x074, 0x07070707}, {0x078, 0x07000000},
    {0x07c, 0x00020405}, {0x080, 0x06060606},
    {0x084, 0x06050504}, {0x088, 0x03020101},
    {0x08c, 0x007a7470}, {0x090, 0x6e6c6b6c},
    {0x094, 0x6c6e7073}, {0x098, 0x76787b7e},
    {0x09c, 0x7f7e7d79}, {0x0a0, 0x726b6359},
    {0x0a4, 0x4f44392e}, {0x0a8, 0x23191008},
    {0x0ec, 0x003d3a38}, {0x0f0, 0x38383839},
    {0x0f4, 0x3a3b3c3d}, {0x0f8, 0x3e3f3f00},
    {0x0fc, 0x7f7e7c76}, {0x100, 0x6f665c51},
    {0x104, 0x463b3025}, {0x108, 0x1b130b05},
    {0x10c, 0x00050b13}, {0x110, 0x1b25303b},
    {0x114, 0x46515c66}, {0x118, 0x6f767c7e},
    {0x11c, 0x00003f3f}, {0x120, 0x3e3d3c3b},
    {0x124, 0x3a393838}, {0x128, 0x38383a3d},
    {0x12c, 0x6b6b6d6f}, {0x130, 0x3336393d},
    {0x134, 0x3f010203}, {0x138, 0x03030202},
    {0x13c, 0xaaa69f95}, {0x140, 0x88786754},
    {0x144, 0x412e1e0f}, {0x148, 0x0279726d},
    {0x200, 1}, {0x20c, 0}, {0x210, 0},
    {0x218, 0x80}, {0x21c, 0x80000080},
    {0x220, 0x80}, {0x224, 0x80}, {0x228, 0x80},
    {0x22c, 0x80}, {0x230, 0x80}, {0x234, 0x80},
    {0x238, 0},
};

static const struct svid_regval router_init[] = {
    {SVID_MXR_STATUS, SVID_MXR_STATUS_IDLE | SVID_MXR_STATUS_SYNC},
    {SVID_MXR_CONFIG, 0},
    {SVID_MXR_VIDEO_CONFIG, 0},
    {SVID_MXR_GRAPHIC0_CONFIG, 0},
    {SVID_MXR_GRAPHIC0_BASE, 0}, {SVID_MXR_GRAPHIC0_POS, 0},
    {SVID_MXR_GRAPHIC0_SIZE, 0},
    {0x01c, 0}, {0x020, 0}, {0x024, 0}, {0x028, 0}, {0x02c, 0},
    {0x030, 0}, {0x034, 0}, {0x038, 0}, {0x03c, 0},
    {SVID_MXR_GRAPHIC_FORMATS, 0}, {0x044, 0},
    {SVID_MXR_BG_COLOR0, 0x00108080},
    {SVID_MXR_BG_COLOR1, 0}, {SVID_MXR_BG_COLOR2, 0},
    {0x054, 0}, {0x058, 0},
    {0x080, 0x08440832}, {0x084, 0x3b4dace1},
    {0x088, 0x0e1d13dc}, {SVID_MXR_COMMIT, 1},
};

static const struct svid_regval encoder_ntsc[] = {
    {0x00c, 6}, {0x010, 1}, {0x014, 0x0000440c},
    {0x01c, 0x800}, {0x020, 0x800}, {0x024, 0x800},
    {0x028, 0x800}, {0x02c, 0x800}, {0x030, 0x800},
    {0x038, 0}, {0x03c, 0x01000700},
    {0x044, 0}, {0x048, 0}, {0x04c, 0}, {0x050, 0},
    {0x054, 0}, {0x058, 0}, {0x05c, 0}, {0x060, 0},
    {0x064, 0}, {0x068, 0}, {0x06c, 0},
    {0x070, 0x0000025d},
    {0x080, 0}, {0x084, 0}, {0x088, 0}, {0x08c, 0},
    {0x090, 0}, {0x094, 1}, {0x098, 7}, {0x09c, 20},
    {0x0a0, 40}, {0x0a4, 63}, {0x0a8, 82}, {0x0ac, 90},
    {0x0c0, 0}, {0x0c4, 0}, {0x0c8, 0}, {0x0cc, 0},
    {0x0d0, 0}, {0x0d4, 1}, {0x0d8, 9}, {0x0dc, 28},
    {0x0e0, 57}, {0x0e4, 90}, {0x0e8, 116}, {0x0ec, 126},
    {0x0f0, 0},
    {0x100, 0}, {0x104, 0}, {0x108, 0}, {0x10c, 0},
    {0x110, 0}, {0x114, 0}, {0x118, 0}, {0x11c, 0},
    {0x120, 0}, {0x124, 0}, {0x128, 0}, {0x12c, 0},
    {0x130, 0}, {0x134, 0}, {0x138, 0}, {0x13c, 0},
    {0x140, 0}, {0x144, 0}, {0x148, 0}, {0x14c, 0},
    {0x150, 0}, {0x154, 0}, {0x158, 0}, {0x15c, 0},
    {0x184, 0x00800000}, {0x188, 0x00800000},
    {0x18c, 0x80}, {0x190, 0}, {0x194, 0x0000eb10},
    {0x198, 0x02000000}, {0x19c, 0x03ff0200},
    {0x1a0, 0x1ff}, {0x1a4, 0x03ff0000},
    {0x1a8, 0x1ff}, {0x1c0, 17},
    {0x200, 0x00fd00fe}, {0x204, 0}, {0x208, 0x00050004},
    {0x20c, 0xff}, {0x210, 0x00f700fa}, {0x214, 1},
    {0x218, 0x000e000a}, {0x21c, 0x1ff},
    {0x220, 0x01ec01f2}, {0x224, 1},
    {0x228, 0x001d0014}, {0x22c, 0x000001fe},
    {0x230, 0x03d803e4}, {0x234, 2}, {0x238, 0x00380028},
    {0x23c, 0x3fd}, {0x240, 0x03b003c7}, {0x244, 5},
    {0x248, 0x00790056}, {0x24c, 0x000003f6},
    {0x250, 0x072c0766}, {0x254, 27},
    {0x258, 0x028b0265}, {0x25c, 0x04000ecc},
    {0x260, 0}, {0x264, 0}, {0x268, 0}, {0x26c, 0x00011a00},
    {0x280, 0}, {0x3c0, 0}, {0x3c4, 0x00010000},
    {0x3c8, 8}, {0x3cc, 0x00010000},
    {0x3d0, 1}, {0x3d4, 8},
};

static void svid_write_table(uintptr_t base,
                             const struct svid_regval *table,
                             unsigned count)
{
    for (unsigned i = 0; i < count; i++)
        SVID_REG(base, table[i].offset) = table[i].value;
}

static bool svid_write_clock(uint16_t value)
{
    volatile uint32_t *reg32 = (volatile uint32_t *)
        ((uintptr_t)&CG16_SVID & ~(uintptr_t)3);
    unsigned shift = ((uintptr_t)&CG16_SVID & 2u) << 3;
    uint32_t preserve = 0xffff0000u >> shift;

    *reg32 = (*reg32 & preserve) | ((uint32_t)value << shift);
    for (unsigned i = 0; i < SVID_CLOCK_POLL_MAX; i++)
    {
        if (CG16_SVID == value)
            return true;
        udelay(1);
    }
    return false;
}

static void svid_platform_restore(void)
{
    if (!svid_platform_saved)
        return;

    PCON(14) = (PCON(14) & ~SVID_GPIO_E4_MASK) | svid_saved_gpio_e4;
    PWRCON(0) = (PWRCON(0) & ~SVID_POWER_GATE_MASK) |
                svid_saved_power_gates;
    svid_write_clock(svid_saved_clock);
    svid_platform_saved = false;
}

static bool svid_platform_enable(void)
{
    uint16_t clock_value =
        ((SVID_CLOCK_SOURCE & CG16_SEL_MSK) << CG16_SEL_POS) |
        (((SVID_CLOCK_DIVIDER - 1) & CG16_DIV1_MSK) << CG16_DIV1_POS);

    svid_saved_clock = CG16_SVID;
    svid_saved_power_gates = PWRCON(0) & SVID_POWER_GATE_MASK;
    svid_saved_gpio_e4 = PCON(14) & SVID_GPIO_E4_MASK;
    svid_platform_saved = true;

    /* Stock firmware selects PLL2 with a divide-by-four SVID clock. */
    if (!svid_write_clock(clock_value))
    {
        svid_platform_restore();
        return false;
    }

    clockgate_enable(SVID_POWER_GATE_0, true);
    clockgate_enable(SVID_POWER_GATE_1, true);
    clockgate_enable(SVID_POWER_GATE_2, true);
    lcd_videoout_clock_acquire();
    GPIOCMD = SVID_GPIO_E4_VIDEO;
    return true;
}

static void svid_begin_reset(void)
{
    SVID_REG(SVID_COMPOSITOR_BASE, 0) &= 2;
    svid_write_table(SVID_COMPOSITOR_BASE, compositor_init,
                     ARRAYLEN(compositor_init));

    /* RetailOS initializes compositor, encoder, then output mixer. */
    SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CLOCK) =
        SVID_SDO_SOFTWARE_RESET;
}

static void svid_finish_reset(void)
{
    SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CLOCK) = 0;
    SVID_REG(SVID_ENCODER_BASE, 0x180) &= ~0x1fu;
    SVID_REG(SVID_ENCODER_BASE, 0x180) |= 0x10;
    svid_write_table(SVID_ENCODER_BASE, encoder_ntsc,
                     ARRAYLEN(encoder_ntsc));
    svid_write_table(SVID_ROUTER_BASE, router_init, ARRAYLEN(router_init));
}

static void svid_select_composite(void)
{
    uint32_t timing = 0x792;
    uint32_t reg = SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CONFIG);

    /* RetailOS 35.2.0.4 does these as two adjacent operations in its output
     * setup: clear mixer bit 2 for NTSC, then set mixer bit 1 before loading
     * the interlaced SDO timing table.  Every qualified failing build reached
     * the mixer as C=0x10 instead of stock C=0x12 and presented only the
     * first 120-line field before repeating its final row. */
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) =
        (SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) & ~4u) |
        SVID_MXR_SD_SCAN;
    SVID_REG(SVID_ENCODER_BASE, 0x34) = 0;
    /* RetailOS selects the dock's composite path with DAC mux values 1, 0,
     * and 2.  Clearing these bits produced sync with a green no-input field. */
    reg &= ~(SVID_SDO_DAC_MUX_MASK | SVID_SDO_COMPONENT |
             SVID_SDO_PROGRESSIVE | SVID_SDO_STANDARD_MASK);
    SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CONFIG) = reg | 0x1200u;

    if (SVID_REG(0x3f000000u, 4) & 0x100u)
    {
        SVID_REG(SVID_ENCODER_BASE, 0x28) = timing + 0x23;
        SVID_REG(SVID_ENCODER_BASE, 0x2c) = timing + 0x6d;
        SVID_REG(SVID_ENCODER_BASE, 0x30) = timing;
    }
    else
    {
        SVID_REG(SVID_ENCODER_BASE, 0x28) = timing | (timing >> 7);
        SVID_REG(SVID_ENCODER_BASE, 0x2c) = timing + 0x27;
        SVID_REG(SVID_ENCODER_BASE, 0x30) = timing + 8;
    }

}

static void svid_start_pipeline(void)
{
    SVID_REG(SVID_ENCODER_BASE, SVID_SDO_DAC) |= SVID_SDO_DAC_ALL_ON;
    SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CLOCK) |= SVID_SDO_CLOCK_ON;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_STATUS) |= SVID_MXR_STATUS_RUN;
}

static void svid_mixer_commit(void)
{
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_COMMIT) = 1;
}

static uint8_t *svid_planar_buffer(unsigned index)
{
    return (uint8_t *)svid_output_framebuffer +
           (index % SVID_PLANAR_BUFFER_COUNT) * SVID_PLANAR_FRAME_SIZE;
}

static bool svid_wait_for_field_edge(void)
{
    uint32_t field = SVID_REG(SVID_ENCODER_BASE, SVID_SDO_FIELD_INFO);

    for (unsigned i = 0; i < SVID_FIELD_EDGE_POLL_MAX; i++)
    {
        if (SVID_REG(SVID_ENCODER_BASE, SVID_SDO_FIELD_INFO) != field)
            return true;
        /* The bounded field wait is interruptible, preserving PCM service. */
        udelay(10);
    }
    return false;
}

static bool svid_begin_planar_write(bool preserve)
{
    unsigned back;
    uint8_t *front;

    back = svid_planar_front_buffer ^ 1u;
    if (svid_surface[back] != SVID_FREE)
        return false;
    svid_surface[back] = SVID_WRITING;
    front = svid_planar_buffer(svid_planar_front_buffer);
    svid_planar_write_buffer = svid_planar_buffer(back);
    if (preserve) memcpy(svid_planar_write_buffer, front, SVID_PLANAR_FRAME_SIZE);
    return true;
}

static bool svid_begin_planar_update(void)
{
    return svid_begin_planar_write(true);
}

static void svid_extend_planar_edges(void)
{
#if SVID_PLANAR_GUARD > 0
    uint8_t *plane = svid_planar_write_buffer;
    for (int channel = 0; channel < 3; channel++)
    {
        int divisor = channel == 0 ? 1 : 2;
        int stride = SVID_PLANAR_Y_WIDTH / divisor;
        int height = SVID_PLANAR_Y_HEIGHT / divisor;
        int guard = SVID_PLANAR_GUARD / divisor;
        int width = SVID_PLANAR_SOURCE_WIDTH / divisor;
        for (int y = 0; y < height; y++)
        {
            uint8_t *row = plane + y * stride;
            memset(row, row[guard], guard);
            memset(row + guard + width, row[guard + width - 1], guard);
        }
        plane += stride * height;
    }
#endif
}

static bool svid_present_planar_update(void)
{
    uintptr_t luma = (uintptr_t)svid_planar_write_buffer;
    uintptr_t cb = luma + SVID_PLANAR_Y_SIZE;
    uintptr_t cr = cb + SVID_PLANAR_C_SIZE;

    /* Never expose a partially converted frame. RetailOS does not issue the
     * later-Samsung VP shadow-update command for private format 8, so switch
     * its three plane descriptors together immediately after an SDO field
     * edge instead of writing into the buffer currently being scanned. */
    svid_extend_planar_edges();
    commit_dcache_range(svid_planar_write_buffer, SVID_PLANAR_FRAME_SIZE);
    unsigned back = svid_planar_front_buffer ^ 1u;
    svid_surface[back] = SVID_READY;
    if (!svid_wait_for_field_edge())
    {
        svid_surface[back] = SVID_FREE;
        return false;
    }
    int oldlevel = disable_irq_save();
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE0_PTR) = luma;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE2_PTR) = cb;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE1_PTR) = cr;
    restore_irq(oldlevel);
    svid_surface[svid_planar_front_buffer] = SVID_FREE;
    svid_surface[back] = SVID_SCANNING;
    svid_planar_front_buffer ^= 1u;
    return true;
}

static uint32_t svid_rgb565_to_xrgb8888(uint16_t pixel)
{
    uint32_t red = (pixel >> 11) & 0x1fu;
    uint32_t green = (pixel >> 5) & 0x3fu;
    uint32_t blue = pixel & 0x1fu;

    red = (red << 3) | (red >> 2);
    green = (green << 2) | (green >> 4);
    blue = (blue << 3) | (blue >> 2);
    return 0xff000000u | (red << 16) | (green << 8) | blue;
}

static void svid_copy_framebuffer_rgb565(const uint16_t *source,
                                         int width, int height)
{
    uint16_t *destination = (uint16_t *)svid_output_framebuffer;
    unsigned output_width = width * 2;
    unsigned output_height = height * 2;
    unsigned x0 = (SVID_NTSC_ACTIVE_WIDTH - output_width) / 2;
    unsigned y0 = (SVID_NTSC_ACTIVE_HEIGHT - output_height) / 2;

    memset(destination, 0, SVID_NTSC_ACTIVE_WIDTH *
                           SVID_NTSC_ACTIVE_HEIGHT * sizeof(*destination));
    for (int y = 0; y < height; y++)
    {
        uint16_t *row0 = &destination[(y0 + y * 2) *
                                     SVID_NTSC_ACTIVE_WIDTH + x0];
        uint16_t *row1 = row0 + SVID_NTSC_ACTIVE_WIDTH;

        for (int x = 0; x < width; x++)
        {
            uint16_t pixel = source[y * width + x];
            row0[x * 2] = pixel;
            row0[x * 2 + 1] = pixel;
            row1[x * 2] = pixel;
            row1[x * 2 + 1] = pixel;
        }
    }

    commit_dcache_range(destination,
                        SVID_NTSC_ACTIVE_WIDTH * SVID_NTSC_ACTIVE_HEIGHT *
                        sizeof(*destination));
}

static void svid_copy_framebuffer_xrgb(const uint16_t *source,
                                       int width, int height)
{
    unsigned scale = width <= (int)(SVID_NTSC_ACTIVE_WIDTH / 2) &&
                     height <= (int)(SVID_NTSC_ACTIVE_HEIGHT / 2) ? 2 : 1;
    unsigned output_width = width * scale;
    unsigned output_height = height * scale;
    unsigned x0 = (SVID_NTSC_ACTIVE_WIDTH - output_width) / 2;
    unsigned y0 = (SVID_NTSC_ACTIVE_HEIGHT - output_height) / 2;

#ifdef VIDEOOUT_ENHANCED_TEST
    ipod6g_videoout_art_clear();
#endif
    memset(svid_output_framebuffer, 0, sizeof(svid_output_framebuffer));
    for (int y = 0; y < height; y++)
    {
        uint32_t *row0 = &svid_output_framebuffer[y0 + y * scale][x0];
        uint32_t *row1 = row0 + SVID_NTSC_ACTIVE_WIDTH;

        for (int x = 0; x < width; x++)
        {
            uint32_t pixel = svid_rgb565_to_xrgb8888(
                source[y * width + x]);
            row0[x * scale] = pixel;
            if (scale == 2)
            {
                row0[x * scale + 1] = pixel;
                row1[x * scale] = pixel;
                row1[x * scale + 1] = pixel;
            }
        }
    }

    commit_dcache_range(svid_output_framebuffer,
                        sizeof(svid_output_framebuffer));
}

static void svid_init_rgb_expansion(void)
{
    if (svid_rgb_expansion_ready)
        return;

    for (unsigned value = 0; value < ARRAYLEN(svid_rgb5_to_8); value++)
        svid_rgb5_to_8[value] = value * 255 / 31;

    for (unsigned value = 0; value < ARRAYLEN(svid_rgb6_to_8); value++)
        svid_rgb6_to_8[value] = value * 255 / 63;

    svid_rgb_expansion_ready = true;
}

static uint32_t svid_rgb565_to_ycbcr(uint16_t pixel)
{
    int red = svid_rgb5_to_8[(pixel >> 11) & 0x1f];
    int green = svid_rgb6_to_8[(pixel >> 5) & 0x3f];
    int blue = svid_rgb5_to_8[pixel & 0x1f];
    int y;
    int cb;
    int cr;

    /* ITU-R BT.601 limited-range values, matching the SD mixer. */
    /* The exhaustive color gate proves these limited-range expressions stay
     * inside byte range for all 65,536 RGB565 inputs. */
    y = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
    cb = ((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128;
    cr = ((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128;
    return (uint32_t)y | ((uint32_t)cb << 8) | ((uint32_t)cr << 16);
}

#ifdef VIDEOOUT_ENHANCED_TEST
#include "videoout-scale2x.h"
#include "videoout-native.h"

/* With 32-pixel guards, two 506880-byte frames plus 202368 bytes of
 * artwork/expected pixels fit inside the existing 1228800-byte allocation. */
#define SVID_ART_DATA ((uint8_t *)svid_output_framebuffer + \
                      SVID_PLANAR_FRAME_SIZE * SVID_PLANAR_BUFFER_COUNT)
#define SVID_ART_EXPECTED ((uint16_t *)(SVID_ART_DATA + VIDEOOUT_ART_SIZE))
_Static_assert(SVID_PLANAR_FRAME_SIZE * SVID_PLANAR_BUFFER_COUNT +
               VIDEOOUT_ART_SIZE + VIDEOOUT_ART_WIDTH *
               VIDEOOUT_ART_HEIGHT * 2 <= sizeof(svid_output_framebuffer),
               "composite artwork exceeds reserved workspace");
static bool svid_art_ready;
static bool svid_art_bound;
static unsigned svid_art_received;
static int svid_art_x, svid_art_y;

/* Called under the LCD mutex, with no filesystem work in target context. */
bool ipod6g_videoout_art_write(unsigned offset, const void *data, unsigned size)
{
    if (offset == 0)
    {
        svid_art_ready = svid_art_bound = false;
        svid_art_received = 0;
    }
    if (!svid_active || !svid_layer_planar || !svid_mirror_enabled ||
        offset != svid_art_received || offset > VIDEOOUT_ART_SIZE ||
        size > VIDEOOUT_ART_SIZE - offset)
        return false;
    memcpy(SVID_ART_DATA + offset, data, size);
    svid_art_received += size;
    return true;
}

void ipod6g_videoout_art_clear(void)
{
    svid_art_ready = svid_art_bound = false;
    svid_art_received = 0;
}

bool ipod6g_videoout_art_finish(void)
{
    svid_art_ready = svid_active && svid_layer_planar &&
                     svid_mirror_enabled &&
                     svid_art_received == VIDEOOUT_ART_SIZE;
    return svid_art_ready;
}

bool ipod6g_videoout_art_bind(const uint16_t *source, int stride, int x, int y)
{
    if (!svid_art_ready || x < 0 || y < 0 ||
        x + VIDEOOUT_ART_WIDTH > LCD_WIDTH ||
        y + VIDEOOUT_ART_HEIGHT > LCD_HEIGHT || stride < VIDEOOUT_ART_WIDTH)
        return false;
    for (int row = 0; row < VIDEOOUT_ART_HEIGHT; row++)
        memcpy(SVID_ART_EXPECTED + row * VIDEOOUT_ART_WIDTH,
               source + row * stride, VIDEOOUT_ART_WIDTH * 2);
    svid_art_x = x;
    svid_art_y = y;
    svid_art_bound = true;
    return true;
}

static void svid_art_composite(const uint16_t *source, int x, int y,
                               int width, int height, int stride)
{
    if (!svid_art_ready || !svid_art_bound)
        return;
    int left = MAX(x, svid_art_x), top = MAX(y, svid_art_y);
    int right = MIN(x + width, svid_art_x + VIDEOOUT_ART_WIDTH);
    int bottom = MIN(y + height, svid_art_y + VIDEOOUT_ART_HEIGHT);
    if (right <= left || bottom <= top)
        return;
    /* A modal, text page, Hold screen, or changed track must win. Compare
     * the entire overlap before touching any TV pixels. */
    for (int row = top; row < bottom; row++)
        if (memcmp(source + (row-y)*stride + left-x,
                   SVID_ART_EXPECTED + (row-svid_art_y)*VIDEOOUT_ART_WIDTH +
                   left-svid_art_x, (right-left)*2))
            return;
    const uint8_t *src = SVID_ART_DATA;
    uint8_t *dst = svid_planar_write_buffer;
    for (int plane = 0; plane < 3; plane++)
    {
        int scale = plane == 0 ? 2 : 1;
        int ds = plane == 0 ? SVID_PLANAR_Y_WIDTH : SVID_PLANAR_C_WIDTH;
        int ss = VIDEOOUT_ART_WIDTH * scale;
        for (int row = top*scale; row < bottom*scale; row++)
            memcpy(dst + row*ds + left*scale +
                   SVID_PLANAR_GUARD * scale / 2,
                   src + (row-svid_art_y*scale)*ss +
                   (left-svid_art_x)*scale, (right-left)*scale);
        src += VIDEOOUT_ART_WIDTH * VIDEOOUT_ART_HEIGHT * scale * scale;
        dst += plane == 0 ? SVID_PLANAR_Y_SIZE : SVID_PLANAR_C_SIZE;
    }
}

static void svid_rgb565_expand(const uint16_t *source, int x, int y,
                               int width, int height, int stride)
{
    uint8_t *luma = svid_planar_write_buffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;
    for (int row = 0; row < height; row++)
        for (int col = 0; col < width; col++)
        {
            uint32_t pixel = svid_rgb565_to_ycbcr(source[row*stride + col]);
            unsigned at = (y+row)*2*SVID_PLANAR_Y_WIDTH + (x+col)*2 +
                          SVID_PLANAR_GUARD;
            luma[at] = luma[at+1] = pixel;
            luma[at+SVID_PLANAR_Y_WIDTH] =
                luma[at+SVID_PLANAR_Y_WIDTH+1] = pixel;
            at = (y+row)*SVID_PLANAR_C_WIDTH + x+col +
                 SVID_PLANAR_GUARD/2;
            cb[at] = pixel >> 8;
            cr[at] = pixel >> 16;
        }
    svid_art_composite(source, x, y, width, height, stride);
}
#endif

static void svid_copy_framebuffer_planar(const uint16_t *source,
                                         int width, int height)
{
#ifdef VIDEOOUT_ENHANCED_TEST
    svid_rgb565_expand(source, 0, 0, width, height, width);
    commit_dcache_range(svid_planar_write_buffer, SVID_PLANAR_FRAME_SIZE);
    return;
#endif
    uint8_t *luma = svid_planar_write_buffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    memset(luma, 16, SVID_PLANAR_Y_SIZE);
    memset(cb, 128, SVID_PLANAR_C_SIZE);
    memset(cr, 128, SVID_PLANAR_C_SIZE);

    for (int y = 0; y < height; y += 2)
    {
        uint8_t *luma0 = luma + y * SVID_PLANAR_Y_WIDTH;
        uint8_t *cb0 = cb + (y / 2) * SVID_PLANAR_C_WIDTH;
        uint8_t *cr0 = cr + (y / 2) * SVID_PLANAR_C_WIDTH;
        int y1 = MIN(y + 1, height - 1);
        uint8_t *luma1 = luma + y1 * SVID_PLANAR_Y_WIDTH;
        const uint16_t *source0 = source + y * width;
        const uint16_t *source1 = source + y1 * width;

        for (int x = 0; x < width; x += 2)
        {
            int x1 = MIN(x + 1, width - 1);
            uint32_t pixel00 = svid_rgb565_to_ycbcr(source0[x]);
            uint32_t pixel01 = svid_rgb565_to_ycbcr(source0[x1]);
            uint32_t pixel10 = svid_rgb565_to_ycbcr(source1[x]);
            uint32_t pixel11 = svid_rgb565_to_ycbcr(source1[x1]);

            luma0[x] = pixel00;
            luma0[x1] = pixel01;
            luma1[x] = pixel10;
            luma1[x1] = pixel11;
            cb0[x / 2] = (((pixel00 >> 8) & 0xff) +
                           ((pixel01 >> 8) & 0xff) +
                           ((pixel10 >> 8) & 0xff) +
                           ((pixel11 >> 8) & 0xff) + 2) >> 2;
            cr0[x / 2] = (((pixel00 >> 16) & 0xff) +
                           ((pixel01 >> 16) & 0xff) +
                           ((pixel10 >> 16) & 0xff) +
                           ((pixel11 >> 16) & 0xff) + 2) >> 2;
        }
    }

    commit_dcache_range(luma, SVID_PLANAR_FRAME_SIZE);
}

static void svid_make_planar_bars(void)
{
    static const uint8_t bar_y[8] =
        {235, 210, 170, 145, 106, 81, 41, 16};
    static const uint8_t bar_cb[8] =
        {128, 16, 166, 54, 202, 90, 240, 128};
    static const uint8_t bar_cr[8] =
        {128, 146, 16, 34, 222, 240, 110, 128};
    uint8_t *luma = svid_planar_write_buffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    for (unsigned y = 0; y < SVID_PLANAR_Y_HEIGHT; y++)
    {
        uint8_t *row = luma + y * SVID_PLANAR_Y_WIDTH;

        for (unsigned x = 0; x < SVID_PLANAR_Y_WIDTH; x++)
            row[x] = bar_y[x / (SVID_PLANAR_Y_WIDTH / 8)];
    }

    for (unsigned y = 0; y < SVID_PLANAR_C_HEIGHT; y++)
    {
        uint8_t *cb_row = cb + y * SVID_PLANAR_C_WIDTH;
        uint8_t *cr_row = cr + y * SVID_PLANAR_C_WIDTH;

        for (unsigned x = 0; x < SVID_PLANAR_C_WIDTH; x++)
        {
            unsigned bar = x / (SVID_PLANAR_C_WIDTH / 8);
            cb_row[x] = bar_cb[bar];
            cr_row[x] = bar_cr[bar];
        }
    }

    commit_dcache_range(luma, SVID_PLANAR_FRAME_SIZE);
}

static uint16_t svid_grid_pixel(unsigned x, unsigned y)
{
    static const uint16_t palette[8] = {
        0xf800, 0xffe0, 0x07e0, 0x07ff,
        0x001f, 0xf81f, 0xffff, 0x8410,
    };
    bool border = x < 3 || x >= LCD_WIDTH - 3 ||
                  y < 3 || y >= LCD_HEIGHT - 3;
    bool separator = (x % (LCD_WIDTH / 8)) < 2 ||
                     (y % (LCD_HEIGHT / 8)) < 2;

    return border ? 0xffff : separator ? 0x0000 :
           palette[((y / (LCD_HEIGHT / 8)) +
                    (x / (LCD_WIDTH / 8))) & 7u];
}

static unsigned svid_grid_source_x(unsigned x)
{
    return MIN(MAX((int)x - SVID_PLANAR_GUARD, 0),
               SVID_PLANAR_SOURCE_WIDTH - 1) / SVID_PLANAR_SCALE;
}

static void svid_make_planar_grid(void)
{
    uint8_t *luma = svid_planar_write_buffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    /* Eight logical rows and columns map the 320x240 LCD exactly.  A white
     * outer border and black cell separators make clipping, repeated final
     * rows, and unequal vertical scale visible without relying on text. */
    for (unsigned y = 0; y < SVID_PLANAR_Y_HEIGHT; y++)
    {
        for (unsigned x = 0; x < SVID_PLANAR_Y_WIDTH; x++)
        {
            uint16_t pixel = svid_grid_pixel(svid_grid_source_x(x), y / SVID_PLANAR_SCALE);
            uint32_t pixel_ycbcr = svid_rgb565_to_ycbcr(pixel);

            luma[y * SVID_PLANAR_Y_WIDTH + x] = pixel_ycbcr;
        }
    }

    for (unsigned y = 0; y < SVID_PLANAR_Y_HEIGHT; y += 2)
    {
        for (unsigned x = 0; x < SVID_PLANAR_Y_WIDTH; x += 2)
        {
            uint32_t pixel00 = svid_rgb565_to_ycbcr(svid_grid_pixel(svid_grid_source_x(x), y / SVID_PLANAR_SCALE));
            uint32_t pixel01 = svid_rgb565_to_ycbcr(
                svid_grid_pixel(svid_grid_source_x(x + 1), y / SVID_PLANAR_SCALE));
            uint32_t pixel10 = svid_rgb565_to_ycbcr(
                svid_grid_pixel(svid_grid_source_x(x), (y + 1) / SVID_PLANAR_SCALE));
            uint32_t pixel11 = svid_rgb565_to_ycbcr(
                svid_grid_pixel(svid_grid_source_x(x + 1), (y + 1) / SVID_PLANAR_SCALE));

            cb[(y / 2) * SVID_PLANAR_C_WIDTH + x / 2] =
                (((pixel00 >> 8) & 0xff) + ((pixel01 >> 8) & 0xff) +
                 ((pixel10 >> 8) & 0xff) + ((pixel11 >> 8) & 0xff) + 2) >> 2;
            cr[(y / 2) * SVID_PLANAR_C_WIDTH + x / 2] =
                (((pixel00 >> 16) & 0xff) + ((pixel01 >> 16) & 0xff) +
                 ((pixel10 >> 16) & 0xff) + ((pixel11 >> 16) & 0xff) + 2) >> 2;
        }
    }

    commit_dcache_range(luma, SVID_PLANAR_FRAME_SIZE);
}

bool ipod6g_videoout_enable_sync(void)
{
    svid_init_rgb_expansion();

    if (svid_active)
        return true;

    /* Keep every SVID register write atomic, but leave the reset-settle delay
     * interruptible.  Its 10 ms duration is almost the complete PCM DMA
     * emergency buffer at 44.1 kHz; masking DMA interrupts across it can
     * desynchronize the software task queue from the hardware channel. */
    int oldlevel = disable_irq_save();
    if (!svid_platform_enable())
    {
        restore_irq(oldlevel);
        return false;
    }
    svid_begin_reset();

    /* Only the hardware settling time is interruptible.  Do not expose the
     * compositor, encoder, or router table writes to interleaving. */
    restore_irq(oldlevel);
    udelay(10000);

    /* Complete initialization and publish the pipeline as one short critical
     * section after the slow reset has finished. */
    oldlevel = disable_irq_save();
    svid_finish_reset();
    svid_select_composite();
    svid_start_pipeline();
    restore_irq(oldlevel);

    svid_active = true;
    svid_layer_active = false;
    svid_layer_planar = false;
    svid_mirror_enabled = false;
    return true;
}

bool ipod6g_videoout_show_background(uint32_t ycbcr)
{
    if (!ipod6g_videoout_enable_sync())
        return false;

    int oldlevel = disable_irq_save();

    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) &=
        ~SVID_MXR_ENABLE_MASK;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR0) =
        ycbcr & 0x00ffffffu;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR1) =
        ycbcr & 0x00ffffffu;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR2) =
        ycbcr & 0x00ffffffu;
    svid_mixer_commit();

    restore_irq(oldlevel);
    svid_framebuffer = NULL;
    svid_framebuffer_width = 0;
    svid_framebuffer_height = 0;
    svid_layer_active = false;
    svid_layer_planar = false;
    svid_mirror_enabled = false;
    svid_set_bus_boost(false);
    svid_publish_active();
    return true;
}

static bool svid_show_planar(const void *framebuffer, int width, int height,
                             enum svid_planar_content content,
                           unsigned destination_x,
                           unsigned destination_y,
                           unsigned destination_width,
                           unsigned destination_height,
                           unsigned source_register_height,
                           unsigned vertical_ratio,
                           bool mirror_updates)
{
    bool pattern = content != SVID_PLANAR_FRAMEBUFFER;

    if (!pattern && (framebuffer == NULL || width != LCD_WIDTH ||
                     height != LCD_HEIGHT))
        return false;
    if (destination_width == 0 || destination_height == 0 ||
        destination_width > SVID_NTSC_OUTPUT_WIDTH ||
        destination_height > SVID_NTSC_OUTPUT_HEIGHT ||
        destination_x > SVID_NTSC_OUTPUT_WIDTH - destination_width ||
        destination_y > SVID_NTSC_OUTPUT_HEIGHT - destination_height ||
        source_register_height == 0 ||
        source_register_height > SVID_NTSC_ACTIVE_HEIGHT ||
        (destination_y & 1u) != 0 || (destination_height & 1u) != 0)
        return false;

    if (vertical_ratio == 0)
    {
        /* RetailOS's VP ratio uses 8-bit fixed-point vertical coordinates.
         * A native 240-line source scaled to 432 destination lines is 142. */
        vertical_ratio =
            ((SVID_PLANAR_Y_HEIGHT << 12) / destination_height) >> 4;
    }
    if (!ipod6g_videoout_enable_sync())
        return false;

    svid_set_bus_boost(true);

    int oldlevel = disable_irq_save();
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) &=
        ~SVID_MXR_ENABLE_MASK;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_ENABLE) &=
        ~SVID_VP_ENABLE_ON;
    svid_mixer_commit();
    restore_irq(oldlevel);

#ifdef VIDEOOUT_ENHANCED_TEST
    ipod6g_videoout_art_clear();
#endif
    svid_planar_front_buffer = 0;
    svid_surface[0] = SVID_SCANNING;
    svid_surface[1] = SVID_FREE;
    svid_planar_write_buffer = svid_planar_buffer(0);
    if (content == SVID_PLANAR_BARS)
        svid_make_planar_bars();
    else if (content == SVID_PLANAR_GRID)
        svid_make_planar_grid();
    else
        svid_copy_framebuffer_planar(framebuffer, width, height);
    svid_extend_planar_edges();
    commit_dcache_range(svid_planar_buffer(0), SVID_PLANAR_FRAME_SIZE);
    memcpy(svid_planar_buffer(1), svid_planar_buffer(0),
           SVID_PLANAR_FRAME_SIZE);
    commit_dcache_range(svid_planar_buffer(1), SVID_PLANAR_FRAME_SIZE);

    oldlevel = disable_irq_save();

    uintptr_t luma = (uintptr_t)svid_planar_buffer(0);
    uintptr_t cb = luma + SVID_PLANAR_Y_SIZE;
    uintptr_t cr = cb + SVID_PLANAR_C_SIZE;

    /* Exact RetailOS private format-8 setup: descriptor 0 -> 0x28,
     * descriptor 2 -> 0x2c, descriptor 1 -> 0x30, zero -> 0x34, spans width
     * and width/2, and plane mode 1.  Physical DCP750 color qualification
     * fixes this private ordering as Y, Cb, and Cr for our generated frame. */
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_MODE) =
        SVID_VP_MODE_RETAIL_FMT8;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE0_PTR) = luma;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE2_PTR) = cb;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_UNUSED_PTR) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE1_PTR) = cr;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_IMG_WIDTH) =
        SVID_PLANAR_Y_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_IMG_HEIGHT) =
        SVID_PLANAR_Y_HEIGHT;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_H_POS) =
        SVID_PLANAR_GUARD << 4;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_V_POS) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_WIDTH) =
        SVID_PLANAR_SOURCE_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_HEIGHT) =
        source_register_height;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_H_POS) = destination_x;
    /* RetailOS's direct geometry path writes caller-provided destination Y
     * and height verbatim on this exact SoC.
     * Applying the later S5P driver's field-line /2 conversion produces the
     * physically observed upper-half-only picture. */
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_V_POS) = destination_y;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_WIDTH) =
        destination_width;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_HEIGHT) =
        destination_height;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_H_RATIO) =
        ((SVID_PLANAR_SOURCE_WIDTH << 12) / destination_width) >> 3;
    /* Destination geometry remains in RetailOS full-frame coordinates. */
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_V_RATIO) = vertical_ratio;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE_MODE) =
        SVID_VP_PLANE_PLANAR;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_LUMA_SPAN) =
        SVID_PLANAR_Y_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_CHROMA_SPAN) =
        SVID_PLANAR_C_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_ENDIAN_MODE) =
        SVID_VP_ENDIAN_LITTLE;

    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_VIDEO_CONFIG) = 0;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR0) = 0x00108080u;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR1) = 0x00108080u;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR2) = 0x00108080u;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_ENABLE) |=
        SVID_VP_ENABLE_ON;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) =
        (SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) &
         ~SVID_MXR_ENABLE_MASK) | SVID_MXR_VIDEO_ENABLE;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_STATUS) =
        SVID_MXR_STATUS_IDLE | SVID_MXR_STATUS_SYNC | SVID_MXR_STATUS_RUN;
    svid_mixer_commit();

    restore_irq(oldlevel);
    svid_framebuffer = framebuffer;
    svid_framebuffer_width = width;
    svid_framebuffer_height = height;
    svid_layer_xrgb = false;
    svid_layer_planar = true;
    svid_mirror_enabled = !pattern && mirror_updates;
    svid_layer_active = true;
    svid_publish_active();
    return true;
}

bool ipod6g_videoout_test_p420_pattern(void)
{
    return svid_show_planar(NULL, 0, 0, SVID_PLANAR_BARS, 0, 0,
                          SVID_NTSC_OUTPUT_WIDTH,
                          SVID_NTSC_OUTPUT_HEIGHT,
                          SVID_PLANAR_SOURCE_HEIGHT, 0, false);
}

bool ipod6g_videoout_test_p420_geometry_pattern(
    unsigned destination_x, unsigned destination_y,
    unsigned destination_width, unsigned destination_height,
    unsigned source_height, unsigned vertical_ratio)
{
    return svid_show_planar(NULL, 0, 0, SVID_PLANAR_GRID,
                          destination_x, destination_y,
                          destination_width, destination_height,
                          source_height * SVID_PLANAR_SCALE,
                          vertical_ratio * SVID_PLANAR_SCALE, false);
}

#ifdef IPOD6G_VIDEOOUT_HIRES_TEST
#include "videoout-hires-pattern.h"

bool ipod6g_videoout_test_hires_pattern(void)
{
    uint8_t *luma = (uint8_t *)svid_output_framebuffer;
    uint8_t *cb = luma + SVID_TEST_Y_SIZE;
    uint8_t *cr = cb + SVID_TEST_C_SIZE;
    int oldlevel;

    /* Diagnostic only: preserve the normal native path and its geometry.
     * The verified stock image/descriptor setters accept 640x480 and spans
     * 640/320. See docs/specs/composite-album-art-qualification.md. */
    _Static_assert(SVID_TEST_FRAME_SIZE <= sizeof(svid_output_framebuffer),
                   "qualification frame exceeds target-owned storage");
    if (!ipod6g_videoout_disable() || !ipod6g_videoout_enable_sync())
        return false;
    svid_set_bus_boost(true);
    svid_mirror_enabled = false;
    svid_framebuffer = NULL;

    /* The layer is disabled while filling; no live DMA buffer is modified.
     * Yield between strips without ever borrowing playback memory. */
    for (unsigned y = 0; y < SVID_TEST_HEIGHT; y += 2)
    {
        for (unsigned x = 0; x < SVID_TEST_WIDTH; x += 2)
        {
            uint32_t p00 = svid_rgb565_to_ycbcr(svid_hires_test_pixel(x, y));
            uint32_t p01 = svid_rgb565_to_ycbcr(svid_hires_test_pixel(x + 1, y));
            uint32_t p10 = svid_rgb565_to_ycbcr(svid_hires_test_pixel(x, y + 1));
            uint32_t p11 = svid_rgb565_to_ycbcr(
                svid_hires_test_pixel(x + 1, y + 1));
            unsigned offset = y * SVID_TEST_WIDTH + x;
            unsigned chroma = (y / 2) * (SVID_TEST_WIDTH / 2) + x / 2;

            luma[offset] = p00;
            luma[offset + 1] = p01;
            luma[offset + SVID_TEST_WIDTH] = p10;
            luma[offset + SVID_TEST_WIDTH + 1] = p11;
            cb[chroma] = (((p00 >> 8) & 255) + ((p01 >> 8) & 255) +
                          ((p10 >> 8) & 255) + ((p11 >> 8) & 255) + 2) >> 2;
            cr[chroma] = (((p00 >> 16) & 255) + ((p01 >> 16) & 255) +
                          ((p10 >> 16) & 255) + ((p11 >> 16) & 255) + 2) >> 2;
        }
        if ((y & 15u) == 0)
            yield();
    }
    commit_dcache_range(luma, SVID_TEST_FRAME_SIZE);
    oldlevel = disable_irq_save();
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_MODE) = SVID_VP_MODE_RETAIL_FMT8;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE0_PTR) = (uintptr_t)luma;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE2_PTR) = (uintptr_t)cb;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE1_PTR) = (uintptr_t)cr;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_UNUSED_PTR) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_IMG_WIDTH) = SVID_TEST_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_IMG_HEIGHT) = SVID_TEST_HEIGHT;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_H_POS) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_V_POS) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_WIDTH) = SVID_TEST_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_HEIGHT) = SVID_TEST_HEIGHT;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_H_POS) = SVID_UI_DESTINATION_X;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_V_POS) = SVID_UI_DESTINATION_Y;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_WIDTH) = SVID_UI_DESTINATION_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_HEIGHT) = SVID_UI_DESTINATION_HEIGHT;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_H_RATIO) =
        ((SVID_TEST_WIDTH << 12) / SVID_UI_DESTINATION_WIDTH) >> 3;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_V_RATIO) =
        ((SVID_TEST_HEIGHT << 12) / SVID_UI_DESTINATION_HEIGHT) >> 4;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE_MODE) = SVID_VP_PLANE_PLANAR;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_LUMA_SPAN) = SVID_TEST_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_CHROMA_SPAN) = SVID_TEST_WIDTH / 2;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_ENDIAN_MODE) = SVID_VP_ENDIAN_LITTLE;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_VIDEO_CONFIG) = 0;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR0) = 0x00108080u;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR1) = 0x00108080u;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR2) = 0x00108080u;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_ENABLE) |= SVID_VP_ENABLE_ON;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) =
        (SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) & ~SVID_MXR_ENABLE_MASK) |
        SVID_MXR_VIDEO_ENABLE;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_STATUS) =
        SVID_MXR_STATUS_IDLE | SVID_MXR_STATUS_SYNC | SVID_MXR_STATUS_RUN;
    svid_mixer_commit();
    restore_irq(oldlevel);
    svid_layer_planar = true;
    svid_layer_active = true;
    svid_publish_active();
    return true;
}
#endif

bool ipod6g_videoout_test_p420_framebuffer(const void *framebuffer,
                                           int width, int height)
{
    return svid_show_planar(framebuffer, width, height,
                            SVID_PLANAR_FRAMEBUFFER, 0, 0,
                          SVID_NTSC_OUTPUT_WIDTH,
                          SVID_NTSC_OUTPUT_HEIGHT,
                          SVID_PLANAR_SOURCE_HEIGHT, 0, true);
}

bool ipod6g_videoout_test_p420_framebuffer_window(
    const void *framebuffer, int width, int height,
    unsigned destination_x, unsigned destination_y,
    unsigned destination_width, unsigned destination_height)
{
    return svid_show_planar(framebuffer, width, height,
                            SVID_PLANAR_FRAMEBUFFER,
                          destination_x, destination_y,
                          destination_width, destination_height,
                          SVID_PLANAR_SOURCE_HEIGHT, 0, true);
}

bool ipod6g_videoout_test_p420_framebuffer_geometry(
    const void *framebuffer, int width, int height,
    unsigned destination_x, unsigned destination_y,
    unsigned destination_width, unsigned destination_height,
    unsigned source_height, unsigned vertical_ratio,
    bool mirror_updates)
{
    return svid_show_planar(framebuffer, width, height,
                            SVID_PLANAR_FRAMEBUFFER,
                          destination_x, destination_y,
                          destination_width, destination_height,
                          source_height, vertical_ratio, mirror_updates);
}

static bool svid_show_framebuffer(const void *framebuffer,
                                  int width, int height, bool xrgb,
                                  uint32_t mixer_config, uint32_t alpha)
{
    if (framebuffer == NULL || width <= 0 || height <= 0 ||
        width > (int)(SVID_NTSC_ACTIVE_WIDTH / 2) ||
        height > (int)(SVID_NTSC_ACTIVE_HEIGHT / 2))
        return false;
    if (!ipod6g_videoout_enable_sync())
        return false;

    svid_set_bus_boost(true);

    int oldlevel = disable_irq_save();
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) &=
        ~SVID_MXR_ENABLE_MASK;
    svid_mixer_commit();
    restore_irq(oldlevel);

    if (xrgb)
        svid_copy_framebuffer_xrgb(framebuffer, width, height);
    else
        svid_copy_framebuffer_rgb565(framebuffer, width, height);

    oldlevel = disable_irq_save();

    uintptr_t start = (uintptr_t)svid_output_framebuffer;
    unsigned source_width = SVID_NTSC_ACTIVE_WIDTH;
    unsigned source_height = SVID_NTSC_ACTIVE_HEIGHT;

    /* Mixer 1 consumes the memory layer while mixer 2 supplies its source and
     * destination geometry. */
    SVID_REG(SVID_COMPOSITOR_BASE, 0x03c) = source_width;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x040) = source_height;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x044) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x048) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x04c) = SVID_NTSC_ACTIVE_WIDTH;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x050) = SVID_NTSC_ACTIVE_HEIGHT;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x054) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x058) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x05c) = source_width;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x060) = source_height;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x064) =
        (((uint32_t)SVID_NTSC_ACTIVE_WIDTH << 12) / source_width) >> 3;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x068) =
        (((uint32_t)SVID_NTSC_ACTIVE_HEIGHT << 12) / source_height) >> 4;

    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_CONFIG) =
        (alpha & 0xffu) |
        (xrgb ? SVID_MXR_TYPE_XRGB8888 : SVID_MXR_TYPE_RGB565);
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_BASE) = start;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_POS) = 0;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_SIZE) =
        (source_width << 16) | source_height;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC_FORMATS) &= ~0xfu;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR0) = 0x00108080u;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR1) = 0x00108080u;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR2) = 0x00108080u;

    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) =
        (SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) &
         ~SVID_MXR_ENABLE_MASK) | (mixer_config & SVID_MXR_ENABLE_MASK);
    SVID_REG(SVID_COMPOSITOR_BASE, 0) |= SVID_MXR_STATUS_RUN;
    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_STATUS) =
        SVID_MXR_STATUS_IDLE | SVID_MXR_STATUS_SYNC | SVID_MXR_STATUS_RUN;
    svid_mixer_commit();

    restore_irq(oldlevel);
    svid_framebuffer = framebuffer;
    svid_framebuffer_width = width;
    svid_framebuffer_height = height;
    svid_layer_xrgb = xrgb;
    svid_layer_planar = false;
    svid_mirror_enabled = true;
    svid_layer_active = true;
    svid_publish_active();
    return true;
}

bool ipod6g_videoout_show_framebuffer(const void *framebuffer,
                                      int width, int height)
{
    struct videoout_geometry g;
    videoout_calc_geometry(width, height, 4, 3, 720, 480,
        (struct videoout_rect){0, 0, 720, 480}, svid_tv_screen,
        false, svid_tv_overscan, true, &g);
    svid_destination = g.destination;
    svid_tv_canvas = false;
    return svid_show_planar(framebuffer, width, height,
        SVID_PLANAR_FRAMEBUFFER, g.destination.x, g.destination.y,
        g.destination.w, g.destination.h, SVID_PLANAR_SOURCE_HEIGHT, 0, true);
}

static void svid_presentation(bool tv_canvas)
{
    struct videoout_geometry g;
    int n = tv_canvas && svid_tv_screen ? 16 : 4;
    int d = tv_canvas && svid_tv_screen ? 9 : 3;
    videoout_calc_geometry(LCD_WIDTH, LCD_HEIGHT, n, d, 720, 480,
        (struct videoout_rect){0, 0, 720, 480}, svid_tv_screen,
        false, svid_tv_overscan, !tv_canvas, &g);
    svid_tv_canvas = tv_canvas;
    if (!svid_active || !svid_layer_planar ||
        !memcmp(&svid_destination, &g.destination, sizeof(g.destination)))
        return;
    int oldlevel = disable_irq_save();
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_H_POS) = g.destination.x;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_V_POS) = g.destination.y;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_WIDTH) = g.destination.w;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_HEIGHT) = g.destination.h;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_H_RATIO) =
        ((SVID_PLANAR_SOURCE_WIDTH << 12) / g.destination.w) >> 3;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_V_RATIO) =
        ((SVID_PLANAR_Y_HEIGHT << 12) / g.destination.h) >> 4;
    svid_mixer_commit();
    restore_irq(oldlevel);
    svid_destination = g.destination;
}

void ipod6g_videoout_ui_owner(bool enabled)
{
    svid_ui_owner = enabled;
}

void ipod6g_videoout_ui_batch(bool enabled)
{
    svid_ui_batch = enabled;
}

void ipod6g_videoout_set_preferences(int screen, int overscan)
{
    svid_tv_screen = screen == 1;
    svid_tv_overscan = overscan >= 0 && overscan < 4 ? overscan : 0;
    svid_presentation(svid_tv_canvas);
}

void ipod6g_videoout_set_video(bool tv_canvas)
{
    svid_presentation(tv_canvas);
}

/* Reflowed UI has square logical pixels (320x240 or 426x240). Resample into
 * the existing scanout workspace. No core/audio allocation or LCD mutation. */
void ipod6g_videoout_present_ui(const uint16_t *pixels, int w, int h)
{
    if (!pixels || w < 2 || w > 854 || h < 2 || h > 480 ||
        !ipod6g_videoout_active() || !svid_layer_planar ||
        !svid_begin_planar_update())
        return;
    svid_presentation(true);
    svid_init_rgb_expansion();
    uint8_t *yplane = svid_planar_write_buffer;
    uint8_t *cb = yplane + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;
    for (int x = 0; x < SVID_PLANAR_SOURCE_WIDTH; x++)
        svid_sample_x[x] = x * w / SVID_PLANAR_SOURCE_WIDTH;
    for (int y = 0; y < SVID_PLANAR_Y_HEIGHT; y += 2)
    {
        int sy0 = y * h / SVID_PLANAR_Y_HEIGHT;
        int sy1 = (y + 1) * h / SVID_PLANAR_Y_HEIGHT;
        for (int x = 0; x < SVID_PLANAR_SOURCE_WIDTH; x += 2)
        {
            unsigned u = 0, v = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++)
                {
                    int sx = svid_sample_x[x + dx];
                    int sy = dy ? sy1 : sy0;
                    uint32_t c = svid_rgb565_to_ycbcr(pixels[sy*w + sx]);
                    yplane[(y+dy)*SVID_PLANAR_Y_WIDTH + x+dx +
                           SVID_PLANAR_GUARD] = c;
                    u += (c >> 8) & 255;
                    v += (c >> 16) & 255;
                }
            int at = (y/2)*SVID_PLANAR_C_WIDTH + x/2 + SVID_PLANAR_GUARD/2;
            cb[at] = (u + 2) / 4;
            cr[at] = (v + 2) / 4;
        }
    }
    (void)svid_present_planar_update();
}

bool ipod6g_videoout_test_config(const void *framebuffer,
                                 int width, int height,
                                 uint32_t mixer_config, uint32_t alpha)
{
    return svid_show_framebuffer(framebuffer, width, height, false,
                                 mixer_config, alpha);
}

bool ipod6g_videoout_test_xrgb(const void *framebuffer, int width, int height)
{
    return svid_show_framebuffer(framebuffer, width, height, true,
                                 SVID_MXR_GRAPHICS_ENABLE,
                                 SVID_MXR_ALPHA_OPAQUE);
}

void ipod6g_videoout_refresh(const void *framebuffer)
{
    if (svid_policy_pending)
        svid_apply_policy();

    if (!svid_active || !svid_layer_active || framebuffer == NULL)
        return;

    svid_presentation(false);
    if (svid_layer_planar)
    {
        if (svid_begin_planar_update())
        {
            svid_copy_framebuffer_planar(framebuffer, svid_framebuffer_width,
                                         svid_framebuffer_height);
            (void)svid_present_planar_update();
        }
    }
    else if (svid_layer_xrgb)
        svid_copy_framebuffer_xrgb(framebuffer, svid_framebuffer_width,
                                   svid_framebuffer_height);
    else
        svid_copy_framebuffer_rgb565(framebuffer, svid_framebuffer_width,
                                     svid_framebuffer_height);
}

void ipod6g_videoout_get_diagnostics(
    struct ipod6g_videoout_diagnostics *diagnostics)
{
    if (diagnostics == NULL)
        return;

    diagnostics->mixer_status =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_STATUS);
    diagnostics->mixer_config =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG);
    diagnostics->graphic_config =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_CONFIG);
    diagnostics->graphic_base =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_BASE);
    diagnostics->graphic_position =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_POS);
    diagnostics->graphic_size =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC0_SIZE);
    diagnostics->graphic_formats =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_GRAPHIC_FORMATS);
    diagnostics->graphic_destination =
        SVID_REG(SVID_COMPOSITOR_BASE, 0);
    diagnostics->background =
        SVID_REG(SVID_ROUTER_BASE, SVID_MXR_BG_COLOR0);
    diagnostics->video_enable =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_ENABLE);
    diagnostics->video_mode =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_MODE);
    diagnostics->video_image_width =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_IMG_WIDTH);
    diagnostics->video_image_height =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_IMG_HEIGHT);
    diagnostics->video_plane0 =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE0_PTR);
    diagnostics->video_plane1 =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE1_PTR);
    diagnostics->video_plane2 =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE2_PTR);
    diagnostics->video_unused =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_UNUSED_PTR);
    diagnostics->video_plane_mode =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_PLANE_MODE);
    diagnostics->video_spans =
        (SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_LUMA_SPAN) << 16) |
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_CHROMA_SPAN);
    diagnostics->video_live_plane0 =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_LIVE_PLANE0_PTR);
    diagnostics->video_live_plane1 =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_LIVE_PLANE1_PTR);
    diagnostics->video_live_plane2 =
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_LIVE_PLANE2_PTR);
    diagnostics->video_source =
        (SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_WIDTH) << 16) |
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_HEIGHT);
    diagnostics->video_destination =
        (SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_WIDTH) << 16) |
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_DST_HEIGHT);
    diagnostics->video_ratio =
        (SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_H_RATIO) << 16) |
        SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_V_RATIO);
    diagnostics->sdo_clock =
        SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CLOCK);
    diagnostics->sdo_config =
        SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CONFIG);
    diagnostics->sdo_dac =
        SVID_REG(SVID_ENCODER_BASE, SVID_SDO_DAC);
    diagnostics->sdo_field =
        SVID_REG(SVID_ENCODER_BASE, SVID_SDO_FIELD_INFO);
}

static bool svid_stop_block(uintptr_t base)
{
    SVID_REG(base, 0) &= ~1u;
    for (unsigned i = 0; i < 10; i++)
    {
        sleep(1);
        if (SVID_REG(base, 0) & 2)
            return true;
    }
    return false;
}

bool ipod6g_videoout_active(void)
{
    if (svid_policy_pending)
        svid_apply_policy();

    return svid_active && svid_layer_active;
}

bool ipod6g_videoout_lcd_clock_required(void)
{
    return svid_platform_saved;
}

bool ipod6g_videoout_hibernate_suspend(void)
{
    bool active = ipod6g_videoout_active();

    svid_hibernate_restore_pending = active;
    return !active || ipod6g_videoout_disable();
}

bool ipod6g_videoout_hibernate_resume(void)
{
    if (!svid_hibernate_restore_pending)
        return true;

    svid_hibernate_restore_pending = false;
    svid_policy_pending = true;
    svid_apply_policy();
    return ipod6g_videoout_active();
}

static void svid_commit_planar_rect(int x, int y, int width, int height)
{
    uint8_t *luma = svid_planar_write_buffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;
    int first_chroma_column = x / 2;
    int last_chroma_column = (x + width - 1) / 2;
    int first_chroma_row = y / 2;
    int last_chroma_row = (y + height - 1) / 2;
    unsigned luma_span = (height - 1) * SVID_PLANAR_Y_WIDTH + width;
    unsigned chroma_width = last_chroma_column - first_chroma_column + 1;
    unsigned chroma_span =
        (last_chroma_row - first_chroma_row) * SVID_PLANAR_C_WIDTH +
        chroma_width;

    /* One clean per touched plane replaces a clean/barrier for every row.
     * The spans include untouched bytes between partial rows but never leave
     * the fixed output buffer. */
    commit_dcache_range(luma + y * SVID_PLANAR_Y_WIDTH + x, luma_span);
    commit_dcache_range(cb + first_chroma_row * SVID_PLANAR_C_WIDTH +
                        first_chroma_column, chroma_span);
    commit_dcache_range(cr + first_chroma_row * SVID_PLANAR_C_WIDTH +
                        first_chroma_column, chroma_span);
}

#ifndef VIDEOOUT_ENHANCED_TEST
static void svid_copy_yuv_plane(uint8_t *destination,
                                unsigned destination_stride,
                                const uint8_t *source,
                                unsigned source_stride,
                                unsigned width, unsigned height)
{
    if (width == destination_stride && width == source_stride)
    {
        memcpy(destination, source, width * height);
        return;
    }

    while (height-- > 0)
    {
        memcpy(destination, source, width);
        destination += destination_stride;
        source += source_stride;
    }
}

#endif

#ifdef HAVE_VIDEOOUT_NATIVE_YUV
bool ipod6g_videoout_native_yuv(const struct videoout_frame *f,
                              unsigned char * const lcd[3])
{
    if (svid_frame_owned || !ipod6g_videoout_active() || !svid_mirror_enabled ||
        !svid_layer_planar || f == NULL || lcd == NULL ||
        f->planes[0] == NULL || f->planes[1] == NULL ||
        f->planes[2] == NULL || lcd[0] == NULL || lcd[1] == NULL ||
        lcd[2] == NULL || f->width <= 0 || f->height <= 0 ||
        f->width > 640 || f->height > 480 || f->stride < f->width ||
        f->stride > 4096 || f->x < 0 || f->y < 0 ||
        f->display_width <= 0 || f->display_height <= 0 ||
        f->display_width > LCD_WIDTH || f->display_height > LCD_HEIGHT ||
        f->x > LCD_WIDTH - f->display_width ||
        f->y > LCD_HEIGHT - f->display_height ||
        ((f->width | f->height | f->stride | f->x | f->y |
          f->display_width | f->display_height) & 1) ||
        (f->width <= f->display_width && f->height <= f->display_height))
        return false;

    /* A full canvas replaces the inactive frame, so no front-buffer copy
     * is needed. The decoder's planes never become DMA descriptors. */
    svid_planar_write_buffer =
        svid_planar_buffer(svid_planar_front_buffer ^ 1u);
    uint8_t *dst[3] = {
        svid_planar_write_buffer + SVID_PLANAR_GUARD,
        svid_planar_write_buffer + SVID_PLANAR_Y_SIZE + SVID_PLANAR_GUARD/2,
        svid_planar_write_buffer + SVID_PLANAR_Y_SIZE +
            SVID_PLANAR_C_SIZE + SVID_PLANAR_GUARD/2
    };
    const int strides[3] = {
        SVID_PLANAR_Y_WIDTH, SVID_PLANAR_C_WIDTH, SVID_PLANAR_C_WIDTH
    };
    videoout_native_compose(dst, strides, f, lcd);
    /* On timeout keep the last complete TV frame. Report the request as
     * handled so the LCD path does not attempt another field wait with a
     * lower-resolution copy of the same frame. */
    (void)svid_present_planar_update();
    return true;
}
#endif
static bool svid_present_frame(void);

void ipod6g_videoout_prepare_frame(const struct videoout_tv_frame *frame)
{
    svid_frame_owned = svid_frame_pending = frame && videoout_active();
    if (svid_frame_pending)
    {
        svid_frame = *frame;
        /* Copy native planes now, while the caller owns the decoder picture.
         * LCD scaling, refresh and backlight no longer trigger TV submission. */
        (void)svid_present_frame();
        memset(&svid_frame, 0, sizeof(svid_frame));
    }
}

/* Present the existing decoded source. Only sampling/cropping and the SVID
 * destination change; no decoder/VPP commands, buffers, or clocks are touched.
 * Borrowed plane pointers are consumed completely before prepare returns. */
static bool svid_present_frame(void)
{
    const struct videoout_tv_frame *f = &svid_frame;
    struct videoout_geometry g;
    struct videoout_rect visible = f->visible;
    int coded_width = f->coded_width ? f->coded_width : f->width;
    int coded_height = f->coded_height ? f->coded_height : f->height;
    if (!visible.w && !visible.h)
        visible = (struct videoout_rect){0,0,f->width,f->height};
    svid_frame_pending = false;
    if (!f->planes[0] || !f->planes[1] || !f->planes[2] ||
        coded_width <= 0 || coded_height <= 0 ||
        visible.x < 0 || visible.y < 0 || visible.w <= 0 || visible.h <= 0 ||
        visible.w > coded_width || visible.x > coded_width-visible.w ||
        visible.h > coded_height || visible.y > coded_height-visible.h ||
        ((visible.x | visible.y | visible.w | visible.h) & 1) ||
        f->stride < coded_width || (f->stride & 1) ||
        f->format != VIDEOOUT_YUV420P || f->color != VIDEOOUT_SD_LIMITED ||
        (f->width & 1) || (f->height & 1) ||
        (f->strides[0] && f->strides[0] < coded_width) ||
        (f->strides[1] && f->strides[1] < coded_width/2) ||
        (f->strides[2] && f->strides[2] < coded_width/2) ||
        !videoout_calc_geometry(visible.w,visible.h,f->dar_n,f->dar_d,
            SVID_PLANAR_SOURCE_WIDTH,SVID_PLANAR_Y_HEIGHT,
            (struct videoout_rect){0,0,SVID_PLANAR_SOURCE_WIDTH,
                                   SVID_PLANAR_Y_HEIGHT},
            svid_tv_screen,f->fill,svid_tv_overscan,false,&g))
        return false;
    svid_presentation(true);
    if (!svid_begin_planar_write(false)) return false;
    uint8_t *dst = svid_planar_write_buffer;
    for (int plane=0;plane<3;plane++)
    {
        int div=plane?2:1;
        int stride=SVID_PLANAR_Y_WIDTH/div;
        int height=SVID_PLANAR_Y_HEIGHT/div;
        int width=SVID_PLANAR_SOURCE_WIDTH/div;
        int guard=SVID_PLANAR_GUARD/div;
        memset(dst,plane?128:16,stride*height);
        int dx=g.destination.x/div,dy=g.destination.y/div;
        int dw=g.destination.w/div,dh=g.destination.h/div;
        int sx=(visible.x+g.crop.x)/div,sy=(visible.y+g.crop.y)/div;
        int sw=g.crop.w/div,sh=g.crop.h/div;
        for (int x=0;x<dw;x++) svid_sample_x[x]=x*sw/dw;
        for (int y=0;y<dh;y++)
        {
            int source_stride = f->strides[plane] ? f->strides[plane] : f->stride/div;
            const uint8_t *row=f->planes[plane]+(sy+y*sh/dh)*source_stride+sx;
            uint8_t *out=dst+(dy+y)*stride+guard+dx;
            for (int x=0;x<dw;x++) out[x]=row[svid_sample_x[x]];
        }
        if (f->caption)
        {
            static const uint8_t luma[4] = {0,16,140,235};
            int canvas_width = MAX(320,f->caption_canvas_width);
            int safe = 100-2*videoout_inset(svid_tv_overscan);
            int cw = MIN(width*288/canvas_width, width*safe/100);
            int left = (width-cw)/2;
            int top = f->caption_y*height/240;
            int ch = 44*height/240 * cw / (width*288/canvas_width);
            for (int cy=0;cy<ch && top+cy<height;cy++)
                for (int cx=0;cx<cw;cx++)
                {
                    int px=cx*288/cw, py=cy*44/ch;
                    int shade=(f->caption[py*72+px/4]>>(2*(px%4)))&3;
                    if (shade && top+cy>=0)
                        dst[(top+cy)*stride+guard+left+cx]=plane?128:luma[shade];
                }
        }
        if (f->overlay && f->overlay_width>0 && f->overlay_height>0)
        {
            int oy=f->overlay_y*height/240;
            int oh=f->overlay_height*height/240;
            for(int x=0;x<width;x++)
                svid_sample_x[x]=x*f->overlay_width/width;
            for(int y=0;y<oh && oy+y<height;y++)
            {
                int src_y=y*f->overlay_height/oh;
                for(int x=0;x<width;x++)
                {
                    unsigned pixel=f->overlay[src_y*f->overlay_width+
                                              svid_sample_x[x]];
                    /* 0 is transparent, status background is nonzero. */
                    if(pixel)
                    {
                        uint32_t c=svid_rgb565_to_ycbcr(pixel);
                        dst[(oy+y)*stride+guard+x]=c>>(8*plane);
                    }
                }
            }
        }
        dst+=stride*height;
    }
    return svid_present_planar_update();
}

bool ipod6g_videoout_mirror_yuv420(const unsigned char *source_luma,
                                   const unsigned char *source_cb,
                                   const unsigned char *source_cr,
                                   int source_x, int source_y,
                                   int source_stride,
                                   int x, int y, int width, int height)
{
    uint8_t *luma;
    uint8_t *cb;
    uint8_t *cr;

    /* Semantic TV screens draw their own picture at TV coordinates. Consume
     * the handheld thumbnail update without letting it overwrite that canvas
     * or falling back to the LCD RGB mirror. The LCD still receives its frame. */
    if (svid_ui_owner || svid_ui_batch)
        return true;

    if (!ipod6g_videoout_active() || !svid_mirror_enabled ||
        !svid_layer_planar || source_luma == NULL || source_cb == NULL ||
        source_cr == NULL || source_x < 0 || source_y < 0 ||
        source_stride < width || x < 0 || y < 0 || width <= 0 ||
        height <= 0 || x + width > LCD_WIDTH || y + height > LCD_HEIGHT ||
        source_x + width > source_stride ||
        ((source_x | source_y | source_stride | x | y | width | height) & 1))
        return false;

    if (svid_frame_pending)
        return svid_present_frame();
    if (svid_frame_owned)
        return true; /* Later LCD overlay bands belong to the same frame. */
    if (!svid_begin_planar_update())
        return false;
    luma = svid_planar_write_buffer;
    cb = luma + SVID_PLANAR_Y_SIZE;
    cr = cb + SVID_PLANAR_C_SIZE;

    source_luma += source_y * source_stride + source_x;
    source_cb += (source_y / 2) * (source_stride / 2) + source_x / 2;
    source_cr += (source_y / 2) * (source_stride / 2) + source_x / 2;

#ifdef VIDEOOUT_ENHANCED_TEST
    videoout_scale2x(luma + y*2*SVID_PLANAR_Y_WIDTH + x*2 +
                    SVID_PLANAR_GUARD,
                    SVID_PLANAR_Y_WIDTH, source_luma, source_stride,
                    width, height);
    videoout_scale2x(cb + y*SVID_PLANAR_C_WIDTH + x +
                    SVID_PLANAR_GUARD/2,
                    SVID_PLANAR_C_WIDTH, source_cb, source_stride/2,
                    width/2, height/2);
    videoout_scale2x(cr + y*SVID_PLANAR_C_WIDTH + x +
                    SVID_PLANAR_GUARD/2,
                    SVID_PLANAR_C_WIDTH, source_cr, source_stride/2,
                    width/2, height/2);
    svid_commit_planar_rect(x*2 + SVID_PLANAR_GUARD, y*2,
                            width*2, height*2);
#else
    svid_copy_yuv_plane(luma + y * SVID_PLANAR_Y_WIDTH + x,
                        SVID_PLANAR_Y_WIDTH, source_luma, source_stride,
                        width, height);
    svid_copy_yuv_plane(cb + (y / 2) * SVID_PLANAR_C_WIDTH + x / 2,
                        SVID_PLANAR_C_WIDTH, source_cb, source_stride / 2,
                        width / 2, height / 2);
    svid_copy_yuv_plane(cr + (y / 2) * SVID_PLANAR_C_WIDTH + x / 2,
                        SVID_PLANAR_C_WIDTH, source_cr, source_stride / 2,
                        width / 2, height / 2);
    svid_commit_planar_rect(x, y, width, height);
#endif
    return svid_present_planar_update();
}

#ifndef VIDEOOUT_ENHANCED_TEST
static void svid_mirror_rgb565_planar_even(const uint16_t *source,
                                           int x, int y, int width,
                                           int height, int stride)
{
    uint8_t *luma = svid_planar_write_buffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    for (int row = 0; row < height; row += 2)
    {
        const uint16_t *source0 = source + row * stride;
        const uint16_t *source1 = source0 + stride;
        uint8_t *luma0 = luma + (y + row) * SVID_PLANAR_Y_WIDTH + x;
        uint8_t *luma1 = luma0 + SVID_PLANAR_Y_WIDTH;
        uint8_t *cb0 = cb + ((y + row) / 2) * SVID_PLANAR_C_WIDTH + x / 2;
        uint8_t *cr0 = cr + ((y + row) / 2) * SVID_PLANAR_C_WIDTH + x / 2;

        for (int column = 0; column < width; column += 2)
        {
            uint32_t pixel00 = svid_rgb565_to_ycbcr(source0[column]);
            uint32_t pixel01 = svid_rgb565_to_ycbcr(source0[column + 1]);
            uint32_t pixel10 = svid_rgb565_to_ycbcr(source1[column]);
            uint32_t pixel11 = svid_rgb565_to_ycbcr(source1[column + 1]);

            luma0[column] = pixel00;
            luma0[column + 1] = pixel01;
            luma1[column] = pixel10;
            luma1[column + 1] = pixel11;
            cb0[column / 2] = (((pixel00 >> 8) & 0xff) +
                               ((pixel01 >> 8) & 0xff) +
                               ((pixel10 >> 8) & 0xff) +
                               ((pixel11 >> 8) & 0xff) + 2) >> 2;
            cr0[column / 2] = (((pixel00 >> 16) & 0xff) +
                               ((pixel01 >> 16) & 0xff) +
                               ((pixel10 >> 16) & 0xff) +
                               ((pixel11 >> 16) & 0xff) + 2) >> 2;
        }
    }
}

static void svid_mirror_rgb565_planar(const uint16_t *source, int x, int y,
                                      int width, int height, int stride)
{
    uint8_t *luma = svid_planar_write_buffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    /* LCD and decoded-video frames use even 4:2:0 rectangles.  Convert each
     * source pixel exactly once, writing its luma and contributing its chroma
     * in the same pass. */
    if (((x | y | width | height) & 1) == 0)
    {
        svid_mirror_rgb565_planar_even(source, x, y, width, height, stride);
        svid_commit_planar_rect(x, y, width, height);
        (void)svid_present_planar_update();
        return;
    }

    for (int row = 0; row < height; row++)
    {
        uint8_t *luma0 = luma + (y + row) * SVID_PLANAR_Y_WIDTH + x;
        const uint16_t *source_row = source + row * stride;

        for (int column = 0; column < width; column++)
        {
            luma0[column] = svid_rgb565_to_ycbcr(source_row[column]);
        }
    }

    int first_chroma_column = x / 2;
    int last_chroma_column = (x + width - 1) / 2;
    int first_chroma_row = y / 2;
    int last_chroma_row = (y + height - 1) / 2;
    for (int chroma_row = first_chroma_row;
         chroma_row <= last_chroma_row; chroma_row++)
    {
        int source_y0 = MAX(chroma_row * 2, y) - y;
        int source_y1 = MIN(chroma_row * 2 + 1, y + height - 1) - y;
        const uint16_t *source_row0 = source + source_y0 * stride;
        const uint16_t *source_row1 = source + source_y1 * stride;
        uint8_t *cb0 = cb + chroma_row * SVID_PLANAR_C_WIDTH +
                       first_chroma_column;
        uint8_t *cr0 = cr + chroma_row * SVID_PLANAR_C_WIDTH +
                       first_chroma_column;

        for (int chroma_column = first_chroma_column;
             chroma_column <= last_chroma_column; chroma_column++)
        {
            int source_x0 = MAX(chroma_column * 2, x) - x;
            int source_x1 = MIN(chroma_column * 2 + 1,
                                x + width - 1) - x;
            int output_column = chroma_column - first_chroma_column;
            uint32_t pixel00 =
                svid_rgb565_to_ycbcr(source_row0[source_x0]);
            uint32_t pixel01 =
                svid_rgb565_to_ycbcr(source_row0[source_x1]);
            uint32_t pixel10 =
                svid_rgb565_to_ycbcr(source_row1[source_x0]);
            uint32_t pixel11 =
                svid_rgb565_to_ycbcr(source_row1[source_x1]);

            cb0[output_column] = (((pixel00 >> 8) & 0xff) +
                                  ((pixel01 >> 8) & 0xff) +
                                  ((pixel10 >> 8) & 0xff) +
                                  ((pixel11 >> 8) & 0xff) + 2) >> 2;
            cr0[output_column] = (((pixel00 >> 16) & 0xff) +
                                  ((pixel01 >> 16) & 0xff) +
                                  ((pixel10 >> 16) & 0xff) +
                                  ((pixel11 >> 16) & 0xff) + 2) >> 2;
        }
    }

    svid_commit_planar_rect(x, y, width, height);
    (void)svid_present_planar_update();
}

#endif

void ipod6g_videoout_mirror_rgb565(const void *source, int x, int y,
                                   int width, int height, int stride)
{
    const uint16_t *src = source;
    unsigned x0;

    if (svid_ui_owner || svid_ui_batch || svid_frame_owned || !ipod6g_videoout_active() ||
        !svid_mirror_enabled || source == NULL ||
        x < 0 || y < 0 || width <= 0 || height <= 0 || stride < width ||
        x + width > LCD_WIDTH || y + height > LCD_HEIGHT)
        return;

    svid_presentation(false);
    if (svid_layer_planar)
    {
        if (!svid_begin_planar_update())
            return;
#ifdef VIDEOOUT_ENHANCED_TEST
        svid_rgb565_expand(src, x, y, width, height, stride);
        svid_commit_planar_rect(x*2 + SVID_PLANAR_GUARD, y*2,
                            width*2, height*2);
        (void)svid_present_planar_update();
#else
        svid_mirror_rgb565_planar(src, x, y, width, height, stride);
#endif
        return;
    }

    if (!svid_layer_xrgb)
    {
        uint16_t *destination = (uint16_t *)svid_output_framebuffer;

        for (int row = 0; row < height; row++)
        {
            uint16_t *dst0 = &destination[(y + row) * 2 *
                                         SVID_NTSC_ACTIVE_WIDTH + x * 2];
            uint16_t *dst1 = dst0 + SVID_NTSC_ACTIVE_WIDTH;

            for (int column = 0; column < width; column++)
            {
                uint16_t pixel = src[column];
                dst0[column * 2] = pixel;
                dst0[column * 2 + 1] = pixel;
                dst1[column * 2] = pixel;
                dst1[column * 2 + 1] = pixel;
            }

            commit_dcache_range(dst0, width * 2 * sizeof(*dst0));
            commit_dcache_range(dst1, width * 2 * sizeof(*dst1));
            src += stride;
        }
        return;
    }

    x0 = (SVID_NTSC_ACTIVE_WIDTH - LCD_WIDTH * 2) / 2 + x * 2;
    for (int row = 0; row < height; row++)
    {
        uint32_t *dst0 = &svid_output_framebuffer[(y + row) * 2][x0];
        uint32_t *dst1 = dst0 + SVID_NTSC_ACTIVE_WIDTH;

        for (int column = 0; column < width; column++)
        {
            uint32_t pixel = svid_rgb565_to_xrgb8888(src[column]);
            dst0[column * 2] = pixel;
            dst0[column * 2 + 1] = pixel;
            dst1[column * 2] = pixel;
            dst1[column * 2 + 1] = pixel;
        }

        commit_dcache_range(dst0, width * 2 * sizeof(*dst0));
        commit_dcache_range(dst1, width * 2 * sizeof(*dst1));
        src += stride;
    }
}

bool ipod6g_videoout_disable(void)
{
    svid_frame_owned = svid_frame_pending = svid_ui_batch = svid_ui_owner = false;
#ifdef VIDEOOUT_ENHANCED_TEST
    ipod6g_videoout_art_clear();
#endif
    if (!svid_active)
    {
        svid_set_bus_boost(false);
        return true;
    }

    SVID_REG(SVID_ROUTER_BASE, SVID_MXR_CONFIG) &=
        ~SVID_MXR_ENABLE_MASK;
    SVID_REG(SVID_ENCODER_BASE, 0x03c) &= ~0xfu;
    bool clean = svid_stop_block(SVID_ENCODER_BASE);
    clean = svid_stop_block(SVID_ROUTER_BASE) && clean;
    clean = svid_stop_block(SVID_COMPOSITOR_BASE) && clean;

    int oldlevel = disable_irq_save();
    svid_platform_restore();
    restore_irq(oldlevel);

    svid_active = false;
    svid_layer_active = false;
    svid_framebuffer = NULL;
    svid_framebuffer_width = 0;
    svid_framebuffer_height = 0;
    svid_layer_xrgb = false;
    svid_layer_planar = false;
    svid_mirror_enabled = false;
    svid_planar_write_buffer = NULL;
#ifdef VIDEOOUT_ENHANCED_TEST
    ipod6g_videoout_art_clear();
#endif
    svid_planar_front_buffer = 0;
    svid_surface[0] = SVID_SCANNING;
    svid_surface[1] = SVID_FREE;
    lcd_videoout_clock_release();
    svid_set_bus_boost(false);
    svid_publish_active();
    return clean;
}

static void svid_apply_policy(void)
{
    /* ON arms the preference; it must not mean "run SVID with no cable".
     * Starting while accessory state is NONE caused every LCD update to do
     * a full mirror conversion and held the CPU/LCD clocks boosted even when
     * the iPod was undocked.  PENDING must also remain off while serial code
     * gives a possible Kokkia time to identify itself. */
    bool enable = svid_mode != IPOD6G_VIDEOOUT_OFF &&
                  svid_accessory == IPOD6G_VIDEOOUT_ACCESSORY_VIDEO;

    svid_policy_pending = false;

    if (enable)
    {
        if ((!svid_active || !svid_layer_active) &&
            svid_policy_framebuffer != NULL)
            ipod6g_videoout_show_framebuffer(svid_policy_framebuffer,
                                             svid_policy_width,
                                             svid_policy_height);
    }
    else if (svid_active)
    {
        ipod6g_videoout_disable();
    }
}

void ipod6g_videoout_set_mode(enum ipod6g_videoout_mode mode,
                              const void *framebuffer,
                              int width, int height)
{
    if (mode > IPOD6G_VIDEOOUT_ON)
        mode = IPOD6G_VIDEOOUT_AUTO;

    svid_mode = mode;
    svid_policy_framebuffer = framebuffer;
    svid_policy_width = width;
    svid_policy_height = height;
    svid_policy_pending = true;
    svid_apply_policy();
}

void ipod6g_videoout_accessory_state(
    enum ipod6g_videoout_accessory accessory)
{
    if (accessory > IPOD6G_VIDEOOUT_ACCESSORY_BLOCKED)
        accessory = IPOD6G_VIDEOOUT_ACCESSORY_BLOCKED;

    if (svid_accessory != accessory)
    {
        svid_accessory = accessory;
        svid_policy_pending = true;
    }
}

/* App policy uses the target-neutral composite availability contract. */
bool videoout_active(void)
{
    return ipod6g_videoout_active();
}
