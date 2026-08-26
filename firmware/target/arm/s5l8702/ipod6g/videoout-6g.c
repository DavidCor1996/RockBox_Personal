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
#include "lcd-s5l8702.h"
#include "videoout-6g.h"
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
#define SVID_PLANAR_Y_WIDTH      LCD_WIDTH
#define SVID_PLANAR_Y_HEIGHT     LCD_HEIGHT
#define SVID_PLANAR_SOURCE_HEIGHT SVID_PLANAR_Y_HEIGHT
#define SVID_PLANAR_Y_SIZE       (SVID_PLANAR_Y_WIDTH * \
                                  SVID_PLANAR_Y_HEIGHT)
#define SVID_PLANAR_C_WIDTH      (SVID_PLANAR_Y_WIDTH / 2)
#define SVID_PLANAR_C_HEIGHT     (SVID_PLANAR_Y_HEIGHT / 2)
#define SVID_PLANAR_C_SIZE       (SVID_PLANAR_C_WIDTH * \
                                  SVID_PLANAR_C_HEIGHT)
#define SVID_PLANAR_FRAME_SIZE   (SVID_PLANAR_Y_SIZE + \
                                  2 * SVID_PLANAR_C_SIZE)

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

static bool svid_active;
static bool svid_layer_active;
static bool svid_layer_xrgb;
static bool svid_layer_planar;
static bool svid_mirror_enabled;
static bool svid_bus_boosted;
static uint32_t svid_output_framebuffer[SVID_NTSC_ACTIVE_HEIGHT]
                                       [SVID_NTSC_ACTIVE_WIDTH]
                                       CACHEALIGN_ATTR;
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
static bool svid_platform_saved;
static uint16_t svid_saved_clock;
static uint32_t svid_saved_power_gates;
static uint32_t svid_saved_gpio_e4;

static void svid_apply_policy(void);

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

static void svid_init_blocks(void)
{
    SVID_REG(SVID_COMPOSITOR_BASE, 0) &= 2;
    svid_write_table(SVID_COMPOSITOR_BASE, compositor_init,
                     ARRAYLEN(compositor_init));

    /* RetailOS initializes compositor, encoder, then output mixer. */
    SVID_REG(SVID_ENCODER_BASE, SVID_SDO_CLOCK) =
        SVID_SDO_SOFTWARE_RESET;
    udelay(10000);
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

static uint8_t svid_clamp_byte(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return value;
}

static void svid_rgb565_to_ycbcr(uint16_t pixel, uint8_t *y,
                                 uint8_t *cb, uint8_t *cr)
{
    int red = ((pixel >> 11) & 0x1f) * 255 / 31;
    int green = ((pixel >> 5) & 0x3f) * 255 / 63;
    int blue = (pixel & 0x1f) * 255 / 31;

    /* ITU-R BT.601 limited-range values, matching the SD mixer. */
    *y = svid_clamp_byte(((66 * red + 129 * green + 25 * blue + 128) >> 8)
                         + 16);
    *cb = svid_clamp_byte(((-38 * red - 74 * green + 112 * blue + 128)
                           >> 8) + 128);
    *cr = svid_clamp_byte(((112 * red - 94 * green - 18 * blue + 128)
                           >> 8) + 128);
}

static void svid_copy_framebuffer_planar(const uint16_t *source,
                                         int width, int height)
{
    uint8_t *luma = (uint8_t *)svid_output_framebuffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    memset(luma, 16, SVID_PLANAR_Y_SIZE);
    memset(cb, 128, SVID_PLANAR_C_SIZE);
    memset(cr, 128, SVID_PLANAR_C_SIZE);

    for (int y = 0; y < height; y++)
    {
        uint8_t *luma0 = luma + y * SVID_PLANAR_Y_WIDTH;

        for (int x = 0; x < width; x++)
        {
            uint8_t pixel_y;
            uint8_t pixel_cb;
            uint8_t pixel_cr;

            svid_rgb565_to_ycbcr(source[y * width + x],
                                 &pixel_y, &pixel_cb, &pixel_cr);
            luma0[x] = pixel_y;
        }
    }

    for (int y = 0; y < height; y += 2)
    {
        uint8_t *cb0 = cb + (y / 2) * SVID_PLANAR_C_WIDTH;
        uint8_t *cr0 = cr + (y / 2) * SVID_PLANAR_C_WIDTH;
        int y1 = MIN(y + 1, height - 1);

        for (int x = 0; x < width; x += 2)
        {
            uint8_t unused_y;
            uint8_t cb00;
            uint8_t cr00;
            uint8_t cb01;
            uint8_t cr01;
            uint8_t cb10;
            uint8_t cr10;
            uint8_t cb11;
            uint8_t cr11;
            int x1 = MIN(x + 1, width - 1);

            svid_rgb565_to_ycbcr(source[y * width + x],
                                 &unused_y, &cb00, &cr00);
            svid_rgb565_to_ycbcr(source[y * width + x1],
                                 &unused_y, &cb01, &cr01);
            svid_rgb565_to_ycbcr(source[y1 * width + x],
                                 &unused_y, &cb10, &cr10);
            svid_rgb565_to_ycbcr(source[y1 * width + x1],
                                 &unused_y, &cb11, &cr11);
            cb0[x / 2] = (cb00 + cb01 + cb10 + cb11 + 2) / 4;
            cr0[x / 2] = (cr00 + cr01 + cr10 + cr11 + 2) / 4;
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
    uint8_t *luma = (uint8_t *)svid_output_framebuffer;
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

static void svid_make_planar_grid(void)
{
    uint8_t *luma = (uint8_t *)svid_output_framebuffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    /* Eight logical rows and columns map the 320x240 LCD exactly.  A white
     * outer border and black cell separators make clipping, repeated final
     * rows, and unequal vertical scale visible without relying on text. */
    for (unsigned y = 0; y < LCD_HEIGHT; y++)
    {
        for (unsigned x = 0; x < LCD_WIDTH; x++)
        {
            uint16_t pixel = svid_grid_pixel(x, y);
            uint8_t pixel_y;
            uint8_t pixel_cb;
            uint8_t pixel_cr;

            svid_rgb565_to_ycbcr(pixel, &pixel_y, &pixel_cb, &pixel_cr);

            luma[y * SVID_PLANAR_Y_WIDTH + x] = pixel_y;
        }
    }

    for (unsigned y = 0; y < LCD_HEIGHT; y += 2)
    {
        for (unsigned x = 0; x < LCD_WIDTH; x += 2)
        {
            uint8_t unused_y;
            uint8_t cb00;
            uint8_t cr00;
            uint8_t cb01;
            uint8_t cr01;
            uint8_t cb10;
            uint8_t cr10;
            uint8_t cb11;
            uint8_t cr11;

            svid_rgb565_to_ycbcr(svid_grid_pixel(x, y), &unused_y,
                                 &cb00, &cr00);
            svid_rgb565_to_ycbcr(svid_grid_pixel(x + 1, y), &unused_y,
                                 &cb01, &cr01);
            svid_rgb565_to_ycbcr(svid_grid_pixel(x, y + 1), &unused_y,
                                 &cb10, &cr10);
            svid_rgb565_to_ycbcr(svid_grid_pixel(x + 1, y + 1), &unused_y,
                                 &cb11, &cr11);
            cb[(y / 2) * SVID_PLANAR_C_WIDTH + x / 2] =
                (cb00 + cb01 + cb10 + cb11 + 2) / 4;
            cr[(y / 2) * SVID_PLANAR_C_WIDTH + x / 2] =
                (cr00 + cr01 + cr10 + cr11 + 2) / 4;
        }
    }

    commit_dcache_range(luma, SVID_PLANAR_FRAME_SIZE);
}

bool ipod6g_videoout_enable_sync(void)
{
    if (svid_active)
        return true;

    int oldlevel = disable_irq_save();
    if (!svid_platform_enable())
    {
        restore_irq(oldlevel);
        return false;
    }
    svid_init_blocks();
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

    if (content == SVID_PLANAR_BARS)
        svid_make_planar_bars();
    else if (content == SVID_PLANAR_GRID)
        svid_make_planar_grid();
    else
        svid_copy_framebuffer_planar(framebuffer, width, height);

    oldlevel = disable_irq_save();

    uintptr_t luma = (uintptr_t)svid_output_framebuffer;
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
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_H_POS) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_V_POS) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, SVID_VP_SRC_WIDTH) =
        SVID_PLANAR_Y_WIDTH;
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
        ((SVID_PLANAR_Y_WIDTH << 12) / destination_width) >> 3;
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
                          source_height, vertical_ratio, false);
}

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
    return true;
}

bool ipod6g_videoout_show_framebuffer(const void *framebuffer,
                                      int width, int height)
{
    return svid_show_planar(framebuffer, width, height,
                            SVID_PLANAR_FRAMEBUFFER,
                          SVID_UI_DESTINATION_X,
                          SVID_UI_DESTINATION_Y,
                          SVID_UI_DESTINATION_WIDTH,
                          SVID_UI_DESTINATION_HEIGHT,
                          SVID_PLANAR_SOURCE_HEIGHT, 0, true);
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

    if (svid_layer_planar)
        svid_copy_framebuffer_planar(framebuffer, svid_framebuffer_width,
                                     svid_framebuffer_height);
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

static void svid_mirror_rgb565_planar(const uint16_t *source, int x, int y,
                                      int width, int height, int stride)
{
    uint8_t *luma = (uint8_t *)svid_output_framebuffer;
    uint8_t *cb = luma + SVID_PLANAR_Y_SIZE;
    uint8_t *cr = cb + SVID_PLANAR_C_SIZE;

    for (int row = 0; row < height; row++)
    {
        uint8_t *luma0 = luma + (y + row) * SVID_PLANAR_Y_WIDTH + x;
        const uint16_t *source_row = source + row * stride;

        for (int column = 0; column < width; column++)
        {
            uint8_t pixel_y;
            uint8_t pixel_cb;
            uint8_t pixel_cr;

            svid_rgb565_to_ycbcr(source_row[column], &pixel_y,
                                 &pixel_cb, &pixel_cr);
            luma0[column] = pixel_y;
        }

        commit_dcache_range(luma0, width);
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
            uint8_t unused_y;
            uint8_t cb00;
            uint8_t cr00;
            uint8_t cb01;
            uint8_t cr01;
            uint8_t cb10;
            uint8_t cr10;
            uint8_t cb11;
            uint8_t cr11;
            int source_x0 = MAX(chroma_column * 2, x) - x;
            int source_x1 = MIN(chroma_column * 2 + 1,
                                x + width - 1) - x;
            int output_column = chroma_column - first_chroma_column;

            svid_rgb565_to_ycbcr(source_row0[source_x0], &unused_y,
                                 &cb00, &cr00);
            svid_rgb565_to_ycbcr(source_row0[source_x1], &unused_y,
                                 &cb01, &cr01);
            svid_rgb565_to_ycbcr(source_row1[source_x0], &unused_y,
                                 &cb10, &cr10);
            svid_rgb565_to_ycbcr(source_row1[source_x1], &unused_y,
                                 &cb11, &cr11);
            cb0[output_column] = (cb00 + cb01 + cb10 + cb11 + 2) / 4;
            cr0[output_column] = (cr00 + cr01 + cr10 + cr11 + 2) / 4;
        }

        int chroma_width = last_chroma_column - first_chroma_column + 1;
        commit_dcache_range(cb0, chroma_width);
        commit_dcache_range(cr0, chroma_width);
    }
}

void ipod6g_videoout_mirror_rgb565(const void *source, int x, int y,
                                   int width, int height, int stride)
{
    const uint16_t *src = source;
    unsigned x0;

    if (!ipod6g_videoout_active() || !svid_mirror_enabled || source == NULL ||
        x < 0 || y < 0 || width <= 0 || height <= 0 || stride < width ||
        x + width > LCD_WIDTH || y + height > LCD_HEIGHT)
        return;

    if (svid_layer_planar)
    {
        svid_mirror_rgb565_planar(src, x, y, width, height, stride);
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
    lcd_videoout_clock_release();
    svid_set_bus_boost(false);
    return clean;
}

static void svid_apply_policy(void)
{
    bool accessory_safe =
        svid_accessory != IPOD6G_VIDEOOUT_ACCESSORY_PENDING &&
        svid_accessory != IPOD6G_VIDEOOUT_ACCESSORY_BLOCKED;
    bool enable = (svid_mode == IPOD6G_VIDEOOUT_ON && accessory_safe) ||
                  (svid_mode == IPOD6G_VIDEOOUT_AUTO &&
                   svid_accessory == IPOD6G_VIDEOOUT_ACCESSORY_VIDEO);

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
