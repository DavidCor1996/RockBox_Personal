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
#include "videoout-6g.h"
#include <string.h>

#include <stdint.h>

#define SVID_COMPOSITOR_BASE 0x39100000u
#define SVID_ROUTER_BASE     0x39200000u
#define SVID_ENCODER_BASE    0x39300000u

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

static bool svid_active;
static bool svid_layer_active;
static uint16_t *svid_framebuffer;
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
    {0x000, 6},
    {0x004, 0}, {0x008, 0}, {0x00c, 0}, {0x010, 0},
    {0x014, 0}, {0x018, 0}, {0x01c, 0}, {0x020, 0},
    {0x024, 0}, {0x028, 0}, {0x02c, 0}, {0x030, 0},
    {0x034, 0}, {0x038, 0}, {0x03c, 0}, {0x040, 0},
    {0x044, 0}, {0x048, 0x00108080}, {0x04c, 0},
    {0x050, 0}, {0x054, 0}, {0x058, 0},
    {0x080, 0x08440832}, {0x084, 0x3b4dace1},
    {0x088, 0x0e1d13dc}, {0x800, 1},
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
    GPIOCMD = SVID_GPIO_E4_VIDEO;
    return true;
}

static void svid_init_blocks(void)
{
    SVID_REG(SVID_COMPOSITOR_BASE, 0) &= 2;
    svid_write_table(SVID_COMPOSITOR_BASE, compositor_init,
                     ARRAYLEN(compositor_init));
    svid_write_table(SVID_ROUTER_BASE, router_init, ARRAYLEN(router_init));

    SVID_REG(SVID_ENCODER_BASE, 0) &= 2;
    SVID_REG(SVID_ENCODER_BASE, 0x180) &= ~0x1fu;
    SVID_REG(SVID_ENCODER_BASE, 0x180) |= 0x10;
    svid_write_table(SVID_ENCODER_BASE, encoder_ntsc,
                     ARRAYLEN(encoder_ntsc));
}

static void svid_select_composite(void)
{
    uint32_t timing = 0x792;
    uint32_t reg = SVID_REG(SVID_ENCODER_BASE, 8) & ~0x3fu;

    SVID_REG(SVID_ROUTER_BASE, 4) &= ~4u; /* NTSC/480 */
    SVID_REG(SVID_ENCODER_BASE, 0x34) = 0;
    SVID_REG(SVID_ENCODER_BASE, 8) = reg | 0x200u | 0x1000u;

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

    SVID_REG(SVID_ENCODER_BASE, 0) = 1;
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
    restore_irq(oldlevel);

    svid_active = true;
    svid_layer_active = false;
    return true;
}

bool ipod6g_videoout_show_framebuffer(const void *framebuffer,
                                      int width, int height)
{
    if (framebuffer == NULL || width <= 0 || height <= 0)
        return false;
    if (!ipod6g_videoout_enable_sync())
        return false;

    commit_dcache_range(framebuffer,
                        width * height * sizeof(*svid_framebuffer));

    int oldlevel = disable_irq_save();

    SVID_REG(SVID_COMPOSITOR_BASE, 0x03c) = width;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x040) = height;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x044) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x048) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x04c) = 640;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x050) = 480;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x054) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x058) = 0;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x05c) = width;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x060) = height;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x064) =
        (((uint32_t)640 << 12) / width) >> 3;
    SVID_REG(SVID_COMPOSITOR_BASE, 0x068) =
        (((uint32_t)480 << 12) / height) >> 4;

    SVID_REG(SVID_ROUTER_BASE, 0x010) = (uintptr_t)framebuffer;
    SVID_REG(SVID_ROUTER_BASE, 0x014) = 0;
    SVID_REG(SVID_ROUTER_BASE, 0x018) = (640u << 16) | 480u;
    SVID_REG(SVID_ROUTER_BASE, 0x040) =
        (SVID_REG(SVID_ROUTER_BASE, 0x040) & ~0xfu) | 0xfu;
    SVID_REG(SVID_ROUTER_BASE, 0x00c) =
        (SVID_REG(SVID_ROUTER_BASE, 0x00c) &
         ~(0xffu | 0x10000u | 0x20000u | 0x100000u)) |
        0xffu | 0x20000u;
    SVID_REG(SVID_ROUTER_BASE, 4) |= 0x9u;

    SVID_REG(SVID_COMPOSITOR_BASE, 0) |= 1;
    SVID_REG(SVID_ROUTER_BASE, 0) = 7;
    SVID_REG(SVID_ENCODER_BASE, 0x03c) |= 7;

    restore_irq(oldlevel);
    svid_framebuffer = (uint16_t *)framebuffer;
    svid_framebuffer_width = width;
    svid_framebuffer_height = height;
    svid_layer_active = true;
    return true;
}

void ipod6g_videoout_refresh(const void *framebuffer)
{
    if (svid_policy_pending)
        svid_apply_policy();

    if (!svid_active || !svid_layer_active || framebuffer == NULL)
        return;

    commit_dcache_range(framebuffer,
                        svid_framebuffer_width * svid_framebuffer_height *
                        sizeof(*svid_framebuffer));
    SVID_REG(SVID_ROUTER_BASE, 0x010) = (uintptr_t)framebuffer;
    SVID_REG(SVID_COMPOSITOR_BASE, 0) |= 1;
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

void ipod6g_videoout_mirror_rgb565(const void *source, int x, int y,
                                   int width, int height, int stride)
{
    const uint16_t *src = source;
    uint16_t *dst;
    uint16_t *first;
    int rows;

    if (!ipod6g_videoout_active() || source == NULL ||
        x < 0 || y < 0 || width <= 0 || height <= 0 || stride < width ||
        x + width > LCD_WIDTH || y + height > LCD_HEIGHT)
        return;

    dst = svid_framebuffer + y * LCD_WIDTH + x;
    first = dst;
    rows = height;
    while (height-- > 0)
    {
        memcpy(dst, src, width * sizeof(*dst));
        dst += LCD_WIDTH;
        src += stride;
    }

    /* The SVID reader is independent of the internal-LCD DMA.  Commit only
     * the changed band so external-only playback never depends on (or pays
     * for) a full-cache flush by displaylcd_dma(). */
    if (width == LCD_WIDTH)
    {
        commit_dcache_range(first, rows * LCD_WIDTH * sizeof(*first));
    }
    else
    {
        while (rows-- > 0)
        {
            commit_dcache_range(first, width * sizeof(*first));
            first += LCD_WIDTH;
        }
    }
}

bool ipod6g_videoout_disable(void)
{
    if (!svid_active)
        return true;

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
