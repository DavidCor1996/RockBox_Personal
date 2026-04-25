#include "config.h"
#include "bringup-nano3g.h"

#if defined(IPOD_NANO3G)

#include <string.h>

#include "debug.h"
#include "system.h"

#ifdef HAVE_LCD_BITMAP
#include "lcd.h"
#include "font.h"
#endif

#ifndef NAN03G_SAFE_BRINGUP
#define NAN03G_SAFE_BRINGUP 0
#endif

#ifndef NANO3G_SAFE_BRINGUP
#define NANO3G_SAFE_BRINGUP NAN03G_SAFE_BRINGUP
#endif

#define NANO3G_BOOTTRACE_CAPACITY 32
#define NANO3G_BOOTTRACE_LEN      48

static char boottrace[NANO3G_BOOTTRACE_CAPACITY][NANO3G_BOOTTRACE_LEN];
static int boottrace_head;
static int boottrace_size;
static bool boottrace_lcd_enabled;

static void nano3g_boottrace_copy(char *dst, const char *src)
{
    int i = 0;

    if (src == NULL)
        src = "(null)";

    while (src[i] != '\0' && i < NANO3G_BOOTTRACE_LEN - 1)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

void nano3g_boottrace_reset(void)
{
    int i;

    boottrace_head = 0;
    boottrace_size = 0;
    boottrace_lcd_enabled = false;

    for (i = 0; i < NANO3G_BOOTTRACE_CAPACITY; i++)
        boottrace[i][0] = '\0';
}

void nano3g_boottrace_log(const char *stage)
{
    int slot;

    if (boottrace_size < NANO3G_BOOTTRACE_CAPACITY)
    {
        slot = (boottrace_head + boottrace_size) % NANO3G_BOOTTRACE_CAPACITY;
        boottrace_size++;
    }
    else
    {
        slot = boottrace_head;
        boottrace_head = (boottrace_head + 1) % NANO3G_BOOTTRACE_CAPACITY;
    }

    nano3g_boottrace_copy(boottrace[slot], stage);
    DEBUGF("nano3g boottrace: %s\n", boottrace[slot]);
}

void nano3g_boottrace_enable_lcd(bool enable)
{
    boottrace_lcd_enabled = enable;
}

int nano3g_boottrace_count(void)
{
    return boottrace_size;
}

const char *nano3g_boottrace_get(int index)
{
    if (index < 0 || index >= boottrace_size)
        return "";

    return boottrace[(boottrace_head + index) % NANO3G_BOOTTRACE_CAPACITY];
}

bool nano3g_safe_mode_enabled(void)
{
    return (NAN03G_SAFE_BRINGUP != 0) || (NANO3G_SAFE_BRINGUP != 0);
}

#ifdef HAVE_LCD_BITMAP
static void nano3g_boottrace_draw_lcd(const char *reason)
{
    int i;
    int y;
    int n;
    int first;

    if (!boottrace_lcd_enabled)
        return;

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    if (!lcd_active())
        return;
#endif

    lcd_set_viewport(NULL);
    lcd_setfont(FONT_SYSFIXED);
    lcd_clear_display();
    lcd_puts(0, 0, (unsigned char *)"Nano3G failsafe halt");

    if (reason != NULL && reason[0] != '\0')
        lcd_puts(0, 1, (unsigned char *)reason);

    n = nano3g_boottrace_count();
    first = n > (LCD_HEIGHT / SYSFONT_HEIGHT) - 3 ?
            n - ((LCD_HEIGHT / SYSFONT_HEIGHT) - 3) : 0;
    y = 2;

    for (i = first; i < n; i++)
        lcd_puts(0, y++, (unsigned char *)nano3g_boottrace_get(i));

    lcd_update();
}
#endif

void nano3g_failsafe_halt(const char *reason)
{
    int i;

    if (reason == NULL || reason[0] == '\0')
        reason = "unknown reason";

    nano3g_boottrace_log("failsafe_halt");
    nano3g_boottrace_log(reason);

    DEBUGF("Nano3G failsafe halt: %s\n", reason);
    for (i = 0; i < nano3g_boottrace_count(); i++)
        DEBUGF("  bt[%d]: %s\n", i, nano3g_boottrace_get(i));

#ifdef HAVE_LCD_BITMAP
    nano3g_boottrace_draw_lcd(reason);
#endif

#if defined(CPU_ARM_CLASSIC)
    disable_interrupt(IRQ_FIQ_STATUS);
#endif

    while (1)
        ;
}

#endif /* IPOD_NANO3G */
