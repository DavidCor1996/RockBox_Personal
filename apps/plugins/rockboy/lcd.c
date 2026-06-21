#include "rockmacros.h"
#include "defs.h"
#include "regs.h"
#include "hw.h"
#include "mem.h"
#include "lcd-gb.h"
#include "fb.h"
#include "profiler.h"
#ifdef HAVE_LCD_COLOR
#include "palette-presets.h"
#endif
#ifdef USE_ASM
#include "asm.h"
#endif

struct lcd lcd;

struct scan scan IBSS_ATTR;

#define BG (scan.bg)
#define WND (scan.wnd)

#if LCD_DEPTH >= 16
#define BUF (scan.buf)
#else
#define BUF (scan.buf[scanline_ind])
#endif

#define PRI (scan.pri)

#define PAL (scan.pal)

#define VS (scan.vs) /* vissprites */
#define NS (scan.ns)

#define L (scan.l) /* line */
#define X (scan.x) /* screen position */
#define Y (scan.y)
#define S (scan.s) /* tilemap position */
#define T (scan.t)
#define U (scan.u) /* position within tile */
#define V (scan.v)
#define WX (scan.wx)
#define WY (scan.wy)
#define WT (scan.wt)
#define WV (scan.wv)

byte patpix[4096][8][8]
#if defined(CPU_COLDFIRE)
     __attribute__ ((aligned(16))) /* to profit from burst mode */
#endif
     ;
byte patdirty[1024];
byte anydirty;

#if LCD_DEPTH < 16
static int scanline_ind=0;
#endif

static int dmg_pal[4][4];
static const int bg_wraptable[64] =
{
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,-32
};

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
unsigned char *vdest;
#else
fb_data *vdest;
#endif

static fb_data* get_framebuffer(void)
{
    struct viewport *vp_main = *(rb->screens[SCREEN_MAIN]->current_viewport);
    return vp_main->buffer->fb_ptr;
}

#ifndef ASM_UPDATEPATPIX
static void updatepatpix(void) ICODE_ATTR;
static void updatepatpix(void)
{
    int i, j;
#if !defined(CPU_COLDFIRE)
    int k, a, c;
#endif
    byte *vram = lcd.vbank[0];

    if (!anydirty) return;
    for (i = 0; i < 1024; i++)
    {
        if (i == 384) i = 512;
        if (i == 896) break;
        if (!patdirty[i]) continue;
        patdirty[i] = 0;
        for (j = 0; j < 8; j++)
        {
#if defined(CPU_COLDFIRE)
            asm volatile (
                "move.b  (%2),%%d2      \n"
                "move.b  (1,%2),%%d1    \n"

                "addq.l  #8,%1          \n"
                "clr.l   %%d0           \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.b  %%d0,-(%1)     \n"
                "lsl.l   #6,%%d0        \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.b  %%d0,-(%1)     \n"
                "lsl.l   #6,%%d0        \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.b  %%d0,-(%1)     \n"
                "lsl.l   #6,%%d0        \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.l  %%d0,(%0)      \n"
                "move.b  %%d0,-(%1)     \n"
                "clr.l   %%d0           \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.b  %%d0,-(%1)     \n"
                "lsl.l   #6,%%d0        \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.b  %%d0,-(%1)     \n"
                "lsl.l   #6,%%d0        \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.b  %%d0,-(%1)     \n"
                "lsl.l   #6,%%d0        \n"
                "lsr.l   #1,%%d1        \n"
                "addx.l  %%d0,%%d0      \n"
                "lsr.l   #1,%%d2        \n"
                "addx.l  %%d0,%%d0      \n"
                "move.l  %%d0,(4,%0)    \n"
                "move.b  %%d0,-(%1)     \n"
                : /* outputs */
                : /* inputs */
                /* %0 */ "a"(patpix[i+1024][j]),
                /* %1 */ "a"(patpix[i][j]),
                /* %2 */ "a"(&vram[(i<<4)|(j<<1)])
                : /* clobbers */
                "d0", "d1", "d2"
            );
#else
            a = ((i<<4) | (j<<1));
            for (k = 0; k < 8; k++)
            {
                c = vram[a] & BIT_N(k) ? 1 : 0;
                c |= vram[a+1] & BIT_N(k) ? 2 : 0;
                patpix[i+1024][j][k] = c;
            }
            for (k = 0; k < 8; k++)
                patpix[i][j][k] =
                    patpix[i+1024][j][7-k];
#endif
        }
#if defined(CPU_COLDFIRE)
        asm volatile (
            "movem.l (%0),%%d0-%%d3     \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(48,%1)  \n"
            "movem.l (16,%0),%%d0-%%d3  \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(32,%1)  \n"
            "movem.l (32,%0),%%d0-%%d3  \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(16,%1)  \n"
            "movem.l (48,%0),%%d0-%%d3  \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(%1)     \n"

            "move.l  %2,%%d0            \n"
            "add.l   %%d0,%0            \n"
            "add.l   %%d0,%1            \n"

            "movem.l (%0),%%d0-%%d3     \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(48,%1)  \n"
            "movem.l (16,%0),%%d0-%%d3  \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(32,%1)  \n"
            "movem.l (32,%0),%%d0-%%d3  \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(16,%1)  \n"
            "movem.l (48,%0),%%d0-%%d3  \n"
            "move.l  %%d0,%%d4          \n"
            "move.l  %%d1,%%d5          \n"
            "movem.l %%d2-%%d5,(%1)     \n"
            : /* outputs */
            : /* inputs */
            /* %0 */ "a"(patpix[i][0]),
            /* %1 */ "a"(patpix[i+2048][0]),
            /* %2 */ "i"(1024*64)
            : /* clobbers */
            "d0", "d1", "d2", "d3", "d4", "d5"
        );
#else
        for (j = 0; j < 8; j++)
        {
            for (k = 0; k < 8; k++)
            {
                patpix[i+2048][j][k] =
                    patpix[i][7-j][k];
                patpix[i+3072][j][k] =
                    patpix[i+1024][7-j][k];
            }
        }
#endif
    }
    anydirty = 0;
}
#endif /* ASM_UPDATEPATPIX */



static void tilebuf(void) ICODE_ATTR;
static void tilebuf(void)
{
    int i, cnt;
    int base;
    byte *tilemap, *attrmap;
    int *tilebuf;
    const int *wrap;

    base = ((R_LCDC&0x08)?0x1C00:0x1800) + (T<<5) + S;
    tilemap = lcd.vbank[0] + base;
    attrmap = lcd.vbank[1] + base;
    tilebuf = BG;
    wrap = bg_wraptable + S;
    cnt = ((WX + 7) >> 3) + 1;

    if (hw.cgb) {
        if (R_LCDC & 0x10)
            for (i = cnt; i > 0; i--)
            {
                *(tilebuf++) = *tilemap
                    | (((int)*attrmap & 0x08) << 6)
                    | (((int)*attrmap & 0x60) << 5);
                *(tilebuf++) = (((int)*attrmap & 0x07) << 2);
                attrmap += *wrap + 1;
                tilemap += *(wrap++) + 1;
            }
        else
            for (i = cnt; i > 0; i--)
            {
                *(tilebuf++) = (256 + ((n8)*tilemap))
                    | (((int)*attrmap & 0x08) << 6)
                    | (((int)*attrmap & 0x60) << 5);
                *(tilebuf++) = (((int)*attrmap & 0x07) << 2);
                attrmap += *wrap + 1;
                tilemap += *(wrap++) + 1;
            }
    }
    else
    {
        if (R_LCDC & 0x10)
            for (i = cnt; i > 0; i--)
            {
                *(tilebuf++) = *(tilemap++);
                tilemap += *(wrap++);
            }
        else
            for (i = cnt; i > 0; i--)
            {
                *(tilebuf++) = (256 + ((n8)*(tilemap++)));
                tilemap += *(wrap++);
            }
    }

    if (WX >= 160) return;

    base = ((R_LCDC&0x40)?0x1C00:0x1800) + (WT<<5);
    tilemap = lcd.vbank[0] + base;
    attrmap = lcd.vbank[1] + base;
    tilebuf = WND;
    cnt = ((160 - WX) >> 3) + 1;

    if (hw.cgb)
    {
        if (R_LCDC & 0x10)
            for (i = cnt; i > 0; i--)
            {
                *(tilebuf++) = *(tilemap++)
                    | (((int)*attrmap & 0x08) << 6)
                    | (((int)*attrmap & 0x60) << 5);
                *(tilebuf++) = (((int)*(attrmap++)&7) << 2);
            }
        else
            for (i = cnt; i > 0; i--)
            {
                *(tilebuf++) = (256 + ((n8)*(tilemap++)))
                    | (((int)*attrmap & 0x08) << 6)
                    | (((int)*attrmap & 0x60) << 5);
                *(tilebuf++) = (((int)*(attrmap++)&7) << 2);
            }
    }
    else

    {
        if (R_LCDC & 0x10)
            for (i = cnt; i > 0; i--)
                *(tilebuf++) = *(tilemap++);
        else
            for (i = cnt; i > 0; i--)
                *(tilebuf++) = (256 + ((n8)*(tilemap++)));
    }
}

static int dmg_bg_tile(byte tile)
{
    if (R_LCDC & 0x10)
        return tile;
    return 256 + (n8)tile;
}

static void blendcpy(byte *dest, byte *src, byte b, int cnt);

static int cgb_bg_tile(byte tile, byte attr)
{
    int tile_index;

    if (R_LCDC & 0x10)
        tile_index = tile;
    else
        tile_index = 256 + (n8)tile;

    return tile_index | (((int)attr & 0x08) << 6) |
        (((int)attr & 0x60) << 5);
}

static void dmg_bg_only_scan(void) ICODE_ATTR;
static void dmg_bg_only_scan(void)
{
    int cnt = 160;
    int chunk = 8 - U;
    int base = ((R_LCDC&0x08)?0x1C00:0x1800) + (T<<5) + S;
    byte *tilemap = lcd.vbank[0] + base;
    byte *dest = BUF;
    const int *wrap = bg_wraptable + S;
    byte *src;

    src = patpix[dmg_bg_tile(*tilemap)][V] + U;
    tilemap += *wrap + 1;
    wrap++;
    memcpy(dest, src, chunk);
    dest += chunk;
    cnt -= chunk;

    while (cnt >= 8)
    {
        src = patpix[dmg_bg_tile(*tilemap)][V];
        tilemap += *wrap + 1;
        wrap++;
        memcpy(dest, src, 8);
        dest += 8;
        cnt -= 8;
    }

    if (cnt)
    {
        src = patpix[dmg_bg_tile(*tilemap)][V];
        while (cnt--)
            *(dest++) = *(src++);
    }
}

static void cgb_bg_only_scan(void)
{
    int cnt = 160;
    int chunk = 8 - U;
    int base = ((R_LCDC&0x08)?0x1C00:0x1800) + (T<<5) + S;
    byte *tilemap = lcd.vbank[0] + base;
    byte *attrmap = lcd.vbank[1] + base;
    byte *dest = BUF;
    const int *wrap = bg_wraptable + S;
    byte *src;
    byte attr;

    attr = *attrmap;
    src = patpix[cgb_bg_tile(*tilemap, attr)][V] + U;
    blendcpy(dest, src, (attr & 0x07) << 2, chunk);
    tilemap += *wrap + 1;
    attrmap += *wrap + 1;
    wrap++;
    dest += chunk;
    cnt -= chunk;

    while (cnt >= 8)
    {
        attr = *attrmap;
        src = patpix[cgb_bg_tile(*tilemap, attr)][V];
        blendcpy(dest, src, (attr & 0x07) << 2, 8);
        tilemap += *wrap + 1;
        attrmap += *wrap + 1;
        wrap++;
        dest += 8;
        cnt -= 8;
    }

    if (cnt)
    {
        attr = *attrmap;
        src = patpix[cgb_bg_tile(*tilemap, attr)][V];
        blendcpy(dest, src, (attr & 0x07) << 2, cnt);
    }
}

static bool fast_line_rendering_enabled(void)
{
    return options.performance_preset != ROCKBOY_PERF_QUALITY;
}


/* V = vertical line
 * WX = WND start (if 0, no need to do anything) -> WY
 * U = start...something...thingy... 7 at most
 */
static void bg_scan(void) ICODE_ATTR;
static void bg_scan(void)
{
    int cnt;
    byte *src, *dest;
    int *tile;

    if (WX <= 0) return;
    cnt = WX;
    tile = BG;
    dest = BUF;

    src = patpix[*(tile++)][V] + U;
    memcpy(dest, src, 8-U);
    dest += 8-U;
    cnt -= 8-U;
    if (cnt <= 0) return;
    while (cnt >= 8)
    {
#if defined(CPU_COLDFIRE)
      asm volatile (
         "move.l (%1)+,(%0)+ \n"
         "move.l (%1)+,(%0)+ \n"
         : /*outputs*/
         : /*inputs*/
         /* %0 */ "a" (dest),
         /* %1 */ "a" (patpix[*(tile++)][V])
         //: /* clobbers */
      );
#else
        src = patpix[*(tile++)][V];
        memcpy(dest,src,8);
        dest += 8;
#endif
        cnt -= 8;
    }
    src = patpix[*tile][V];
    while (cnt--)
        *(dest++) = *(src++);
}

static void wnd_scan(void) ICODE_ATTR;
static void wnd_scan(void)
{
    int cnt;
    byte *src, *dest;
    int *tile;

    if (WX >= 160) return;
    cnt = 160 - WX;
    tile = WND;
    dest = BUF + WX;

    while (cnt >= 8)
    {
#if defined(CPU_COLDFIRE)
      asm volatile (
         "move.l (%1)+,(%0)+ \n"
         "move.l (%1)+,(%0)+ \n"
         : /*outputs*/
         : /*inputs*/
         /* %0 */ "a" (dest),
         /* %1 */ "a" (patpix[*(tile++)][WV])
         //: /* clobbers */
      );
#else
        src = patpix[*(tile++)][WV];
        memcpy(dest,src,8);
        dest += 8;
#endif
        cnt -= 8;
    }
    src = patpix[*tile][WV];
    while (cnt--)
        *(dest++) = *(src++);
}

static void blendcpy(byte *dest, byte *src, byte b, int cnt)
{
    while (cnt--) *(dest++) = *(src++) | b;
}

static int priused(void *attr)
{
    un32 *a = attr;
    return (int)((a[0]|a[1]|a[2]|a[3]|a[4]|a[5]|a[6]|a[7])&0x80808080);
}

static void bg_scan_pri(void) ICODE_ATTR;
static void bg_scan_pri(void)
{
    int cnt, i;
    byte *src, *dest;

    if (WX <= 0) return;
    i = S;
    cnt = WX;
    dest = PRI;
    src = lcd.vbank[1] + ((R_LCDC&0x08)?0x1C00:0x1800) + (T<<5);

    if (!priused(src))
    {
        memset(dest, 0, cnt);
        return;
    }

    memset(dest, src[i++&31]&128, 8-U);
    dest += 8-U;
    cnt -= 8-U;
    if (cnt <= 0) return;
    while (cnt >= 8)
    {
        memset(dest, src[i++&31]&128, 8);
        dest += 8;
        cnt -= 8;
    }
    memset(dest, src[i&31]&128, cnt);
}

static void wnd_scan_pri(void) ICODE_ATTR;
static void wnd_scan_pri(void)
{
    int cnt, i;
    byte *src, *dest;

    if (WX >= 160) return;
    i = 0;
    cnt = 160 - WX;
    dest = PRI + WX;
    src = lcd.vbank[1] + ((R_LCDC&0x40)?0x1C00:0x1800) + (WT<<5);

    if (!priused(src))
    {
        memset(dest, 0, cnt);
        return;
    }

    while (cnt >= 8)
    {
        memset(dest, src[i++]&128, 8);
        dest += 8;
        cnt -= 8;
    }
    memset(dest, src[i]&128, cnt);
}

static void bg_scan_color(void)
{
    int cnt;
    byte *src, *dest;
    int *tile;

    if (WX <= 0) return;
    cnt = WX;
    tile = BG;
    dest = BUF;

    src = patpix[*(tile++)][V] + U;
    blendcpy(dest, src, *(tile++), 8-U);
    dest += 8-U;
    cnt -= 8-U;
    if (cnt <= 0) return;
    while (cnt >= 8)
    {
        src = patpix[*(tile++)][V];
#if defined(CPU_COLDFIRE)
      asm volatile (
         "move.l (%2)+,%%d1 \n"

         "move.b %%d1,%%d2 \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d2,%%d0      \n"
         "move.b %%d0,(%0)+   \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d1,%%d0      \n"
         "move.b %%d0,(%0)+   \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d2,%%d0      \n"
         "move.b %%d0,(%0)+   \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d2,%%d0      \n"
         "move.b %%d0,(%0)+   \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d2,%%d0      \n"
         "move.b %%d0,(%0)+   \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d2,%%d0      \n"
         "move.b %%d0,(%0)+   \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d2,%%d0      \n"
         "move.b %%d0,(%0)+   \n"

         "move.b (%1)+,%%d0  \n"
         "or.l %%d2,%%d0      \n"
         "move.b %%d0,(%0)+   \n"
   : /*outputs*/
   : /*inputs*/
   /* %0 */ "a" (dest),
   /* %1 */ "a" (src),
   /* %2 */ "a" (tile)
   : /* clobbers */
   "d0", "d1", "d2"
);
#else
        blendcpy(dest, src, *(tile++), 8);
        dest += 8;
#endif
        cnt -= 8;
    }
    src = patpix[*(tile++)][V];
    blendcpy(dest, src, *(tile++), cnt);
}

static void wnd_scan_color(void)
{
    int cnt;
    byte *src, *dest;
    int *tile;

    if (WX >= 160) return;
    cnt = 160 - WX;
    tile = WND;
    dest = BUF + WX;

    while (cnt >= 8)
    {
        src = patpix[*(tile++)][WV];
        blendcpy(dest, src, *(tile++), 8);
        dest += 8;
        cnt -= 8;
    }
    src = patpix[*(tile++)][WV];
    blendcpy(dest, src, *(tile++), cnt);
}

static void spr_enum(void) ICODE_ATTR;
static void spr_enum(void)
{
    int i, j;
    struct obj *o;
    struct vissprite ts;
    int v, pat;

    NS = 0;
    if (!(R_LCDC & 0x02)) return;

    o = lcd.oam.obj;

    for (i = 40; i; i--, o++)
    {
        if (L >= o->y || L + 16 < o->y)
            continue;
        if (L + 8 >= o->y && !(R_LCDC & 0x04))
            continue;
        VS[NS].x = (int)o->x - 8;
        v = L - (int)o->y + 16;
        if (hw.cgb)
        {
            pat = o->pat | (((int)o->flags & 0x60) << 5)
                | (((int)o->flags & 0x08) << 6);
            VS[NS].pal = 32 + ((o->flags & 0x07) << 2);
        }
        else
        {
            pat = o->pat | (((int)o->flags & 0x60) << 5);
            VS[NS].pal = 32 + ((o->flags & 0x10) >> 2);
        }
        VS[NS].pri = (o->flags & 0x80) >> 7;
        if ((R_LCDC & 0x04))
        {
            pat &= ~1;
            if (v >= 8)
            {
                v -= 8;
                pat++;
            }
            if (o->flags & 0x40) pat ^= 1;
        }
        VS[NS].buf = patpix[pat][v];
        if (++NS == 10) break;
    }
    if (hw.cgb) return;
    for (i = 0; i < NS; i++)
    {
        for (j = i + 1; j < NS; j++)
        {
            if (VS[i].x > VS[j].x)
            {
                ts = VS[i];
                VS[i] = VS[j];
                VS[j] = ts;
            }
        }
    }
}

static void spr_scan(void) ICODE_ATTR;
static void spr_scan(void)
{
    int i, x;
    byte pal, b, ns = NS;
    byte *src, *dest, *bg, *pri;
    struct vissprite *vs;
    static byte bgdup[256];

    if (!ns) return;

    memcpy(bgdup, BUF, 256);
    vs = &VS[ns-1];

    for (; ns; ns--, vs--)
    {
        x = vs->x;
        if (x > 159) continue;
        if (x < -7) continue;
        if (x < 0)
        {
            src = vs->buf - x;
            dest = BUF;
            i = 8 + x;
        }
        else
        {
            src = vs->buf;
            dest = BUF + x;
            if (x > 152) i = 160 - x;
            else i = 8;
        }
        pal = vs->pal;
        if (vs->pri)
        {
            bg = bgdup + (dest - BUF);
            while (i--)
            {
                b = src[i];
                if (b && !(bg[i]&3)) dest[i] = pal|b;
            }
        }
        else if (hw.cgb)
        {
            bg = bgdup + (dest - BUF);
            pri = PRI + (dest - BUF);
            while (i--)
            {
                b = src[i];
                if (b && (!pri[i] || !(bg[i]&3)))
                    dest[i] = pal|b;
            }
        }
        else while (i--) if (src[i]) dest[i] = pal|src[i];
    }
}

/* Scaling defines */
#define DX  ((LCD_WIDTH<<16)    /   160)
#define DXI ((160<<16)          /   LCD_WIDTH)
#define DY  ((LCD_HEIGHT<<16)   /   144)
#define DYI ((144<<16)          /   LCD_HEIGHT)

#define DXR  ((LCD_WIDTH<<16)    /   144)
#define DXIR ((144<<16)          /   LCD_WIDTH)
#define DYR  ((LCD_HEIGHT<<16)   /   160)
#define DYIR ((160<<16)          /   LCD_HEIGHT)

/* Defines for scale offsets:
 *  S2 is for scaled
 *  S3 if scaled and maintain ratio
 *  S1 is unscaled
 *  R's are the rotated defines
 */
#if (LCD_WIDTH>=160) && (LCD_HEIGHT>=144)
#define S1  ((LCD_HEIGHT-144)/2)*LCD_WIDTH + ((LCD_WIDTH-160)/2)
#define S2  0
#define S1R ((LCD_HEIGHT-160)/2)*LCD_WIDTH + ((LCD_WIDTH-144)/2)+144
#define S2R (LCD_WIDTH-1)

#elif (LCD_WIDTH>=160) && (LCD_HEIGHT<=144)
#define S1  0
#define S2  0
#define S1R LCD_WIDTH-1
#define S2R LCD_WIDTH-1

#elif (LCD_WIDTH<=160) && (LCD_HEIGHT>=144)
#define S1  ((LCD_HEIGHT-144)/2)*LCD_WIDTH
#define S2  ((LCD_HEIGHT-144)/2)*LCD_WIDTH
#define S1R LCD_WIDTH-1
#define S2R LCD_WIDTH-1

#else
#define S1  0
#define S2  0
#define S1R LCD_WIDTH-1
#define S2R LCD_WIDTH-1
#endif

#if (LCD_WIDTH>LCD_HEIGHT)
#define S3  ((LCD_WIDTH-((160*DY)>>16))/2)
#define S3R  LCD_WIDTH-1
#else
#define S3  ((LCD_HEIGHT-((144*DX)>>16))/2)*LCD_WIDTH
#define S3R ((LCD_HEIGHT-((160*DXR)>>16))/2)*LCD_WIDTH+LCD_WIDTH-1
#endif

void lcd_begin(void)
{
    fb_data *lcd_fb = get_framebuffer();
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
    vdest=(unsigned char*)lcd_fb;
#else
    vdest=lcd_fb;
#endif

#ifdef HAVE_LCD_COLOR
    
    if(options.rotate==1) {
        if(options.scaling == 0)
            vdest+=S2R;
        else if (options.scaling == 1)
            vdest+=S3R;
        else
            vdest+=S1R;
    } else if(options.rotate==2) {
        if(options.scaling == 0)
            vdest+=(LCD_WIDTH*LCD_HEIGHT)-S2R;
        else if (options.scaling == 1)
            vdest+=(LCD_WIDTH*LCD_HEIGHT)-S3R;
        else
            vdest+=(LCD_WIDTH*LCD_HEIGHT)-S1R-144;
    } else {
        if(options.scaling == 0)
            vdest+=S2;
        else if (options.scaling == 1)
            vdest+=S3;
        else
            vdest+=S1;
    }
#endif
    WY = R_WY;
}

#ifdef HAVE_LCD_COLOR
int SCALEWL IDATA_ATTR=1<<16;
int SCALEWS IDATA_ATTR=1<<16;
int SCALEHL IDATA_ATTR=1<<16;
int SCALEHS IDATA_ATTR=1<<16;
int swidth  IDATA_ATTR=160;
int sremain IDATA_ATTR=LCD_WIDTH-160;
#endif

#if defined(IPOD_VIDEO) && defined(HAVE_LCD_COLOR) && \
    (LCD_WIDTH == 320) && (LCD_HEIGHT == 240)
static int ipodvideo_fast_y_accum IDATA_ATTR;
static int ipodvideo_fast_out_y IDATA_ATTR;

static inline bool ipodvideo_fast_fullscreen(void)
{
    return options.scaling == 0 && options.rotate == 0;
}

static void ipodvideo_fast_scale_line(void)
{
    int repeat;

    if (L == 0)
    {
        ipodvideo_fast_y_accum = 0;
        ipodvideo_fast_out_y = 0;
    }

    ipodvideo_fast_y_accum += LCD_HEIGHT;
    repeat = ipodvideo_fast_y_accum / 144;
    ipodvideo_fast_y_accum %= 144;

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
    unsigned char *frame = (unsigned char*)get_framebuffer();
#else
    fb_data *frame = get_framebuffer();
#endif

    while (repeat-- > 0 && ipodvideo_fast_out_y < LCD_HEIGHT)
    {
        int x;
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
        unsigned char *dest = frame + ipodvideo_fast_out_y * LCD_WIDTH;

        for (x = 0; x < 160; x++)
        {
            unsigned char px = BUF[x];
            *dest++ = px;
            *dest++ = px;
        }
#else
        fb_data *dest = frame + ipodvideo_fast_out_y * LCD_WIDTH;

        for (x = 0; x < 160; x++)
        {
            fb_data px = PAL[BUF[x]];
            *dest++ = px;
            *dest++ = px;
        }
#endif
        ipodvideo_fast_out_y++;
    }
}
#endif

void setvidmode(void)
{

#ifdef HAVE_LCD_COLOR
    switch(options.scaling)
    {
        case 0:
            if(options.rotate)
            {
                SCALEWL=DYR;
                SCALEWS=DYIR;
                SCALEHL=DXR;
                SCALEHS=DXIR;
            }
            else
            {
                SCALEWL=DX;
                SCALEWS=DXI;
                SCALEHL=DY;
                SCALEHS=DYI;
            }
            break;
        case 1: /* Maintain Ratio */
            if(options.rotate)
            {
                if (DYR<DXR)
                {
                    SCALEWL=DYR;
                    SCALEWS=DYIR;
                    SCALEHL=DYR;
                    SCALEHS=DYIR;
                }
                else
                {
                    SCALEWL=DXR;
                    SCALEWS=DXIR;
                    SCALEHL=DXR;
                    SCALEHS=DXIR;
                }
            }
            else
            {
                if (DY<DX)
                {
                    SCALEWL=DY;
                    SCALEWS=DYI;
                    SCALEHL=DY;
                    SCALEHS=DYI;
                }
                else
                {
                    SCALEWL=DX;
                    SCALEWS=DXI;
                    SCALEHL=DX;
                    SCALEHS=DXI;
                }
            }
            break;
        default:
            SCALEWL=1<<16;
            SCALEWS=1<<16;
            SCALEHL=1<<16;
            SCALEHS=1<<16;
    }
    swidth=((160*SCALEWL)>>16);
    
    if(options.rotate==1) {
        sremain=-(((160*SCALEWL)>>16)*LCD_WIDTH+1);
    } else if(options.rotate==2) {
        sremain=(((160*SCALEWL)>>16)*LCD_WIDTH+1);
    } else {
        sremain=LCD_WIDTH-swidth;
    }
#endif
#if defined(IPOD_VIDEO) && defined(HAVE_LCD_COLOR) && \
    (LCD_WIDTH == 320) && (LCD_HEIGHT == 240)
    ipodvideo_fast_y_accum = 0;
    ipodvideo_fast_out_y = 0;
#endif
}

void lcd_refreshline(void)
{
    unsigned long render_start = *rb->current_tick;
    bool dmg_bg_only_eligible;
    bool cgb_bg_only_eligible;

    if (!(R_LCDC & 0x80))
        return; /* should not happen... */

#if (LCD_HEIGHT <= 128) && !defined(HAVE_LCD_COLOR)
    if ( (fb.mode==0&&(R_LY >= 128)) ||
         (fb.mode==1&&(R_LY < 16)) ||
         (fb.mode==2&&(R_LY<8||R_LY>=136)) ||
         (fb.mode==3&&((R_LY%9)==8))

#if LCD_HEIGHT == 64
        || (R_LY & 1) /* calculate only even lines */
#endif
        )
        return;
#endif

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
    fb_data *lcd_fb = get_framebuffer();
#endif
    updatepatpix();

    L = R_LY;
    X = R_SCX;
    Y = (R_SCY + L) & 0xff;
    S = X >> 3;
    T = Y >> 3;
    U = X & 7;
    V = Y & 7;

    WX = R_WX - 7;
    if (WY>L || WY<0 || WY>143 || WX<-7 || WX>159 || !(R_LCDC&0x20))
        WX = 160;
    WT = (L - WY) >> 3;
    WV = (L - WY) & 7;

    spr_enum();
    rockboy_profile_count(ROCKBOY_EVENT_LCD_LINES, 1);
    if (hw.cgb)
        rockboy_profile_count(ROCKBOY_EVENT_LCD_CGB_LINES, 1);
    else
        rockboy_profile_count(ROCKBOY_EVENT_LCD_DMG_LINES, 1);
    if (!NS)
        rockboy_profile_count(ROCKBOY_EVENT_LCD_NO_SPRITE_LINES, 1);
    if (WX == 160)
        rockboy_profile_count(ROCKBOY_EVENT_LCD_NO_WINDOW_LINES, 1);
    if (!hw.cgb && !NS)
    {
        if (WX == 160)
            rockboy_profile_count(ROCKBOY_EVENT_LCD_DMG_BG_ONLY_ELIGIBLE, 1);
        else
            rockboy_profile_count(ROCKBOY_EVENT_LCD_DMG_BG_WINDOW_NO_SPR_ELIGIBLE, 1);
    }
    if (hw.cgb && !NS)
    {
        rockboy_profile_count(ROCKBOY_EVENT_LCD_CGB_NO_SPRITE_LINES, 1);
        if (WX == 160)
            rockboy_profile_count(ROCKBOY_EVENT_LCD_CGB_BG_ONLY_ELIGIBLE, 1);
    }

    dmg_bg_only_eligible = !hw.cgb && !NS && WX == 160;
    cgb_bg_only_eligible = hw.cgb && !NS && WX == 160;
    if (dmg_bg_only_eligible && fast_line_rendering_enabled())
    {
        rockboy_profile_count(ROCKBOY_EVENT_LCD_DMG_BG_ONLY_USED, 1);
        dmg_bg_only_scan();
    }
    else if (cgb_bg_only_eligible && fast_line_rendering_enabled())
    {
        rockboy_profile_count(ROCKBOY_EVENT_LCD_CGB_BG_ONLY_USED, 1);
        cgb_bg_only_scan();
    }
    else
    {
        if (dmg_bg_only_eligible)
            rockboy_profile_count(ROCKBOY_EVENT_LCD_DMG_BG_ONLY_REJECTED, 1);
        if (cgb_bg_only_eligible)
            rockboy_profile_count(ROCKBOY_EVENT_LCD_CGB_BG_ONLY_REJECTED, 1);
        tilebuf();
        if (hw.cgb)
        {
            bg_scan_color();
            wnd_scan_color();
            if (NS)
            {
                bg_scan_pri();
                wnd_scan_pri();
            }
        }
        else
        {

            bg_scan();
            wnd_scan();
        }
    }
    if (NS)
        spr_scan();

#if !defined(HAVE_LCD_COLOR)
#if LCD_DEPTH == 1
    if (scanline_ind == 7)
#elif LCD_DEPTH == 2
    if (scanline_ind == 3)
#endif
    {
        if(fb.mode!=3)
            vid_update(L);
        else
            vid_update(L-((int)(L/9)));
#else
    {
    /*  Universal Scaling pulled from PrBoom and modified for rockboy  */

    static int hpt IDATA_ATTR=0x8000;
    unsigned long scale_start = *rb->current_tick;

#if defined(IPOD_VIDEO) && defined(HAVE_LCD_COLOR) && \
    (LCD_WIDTH == 320) && (LCD_HEIGHT == 240)
    if (ipodvideo_fast_fullscreen())
        ipodvideo_fast_scale_line();
    else
#endif
    {
    while((hpt>>16)<L+1)
    {
        hpt+=SCALEHS;
        register unsigned int srcpt=0x8000;
        register unsigned int wcount=swidth;
        while(wcount--)
        {
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
            *vdest = BUF[srcpt>>16];
#else
            *vdest = PAL[BUF[srcpt>>16]];
#endif
            if (options.rotate == 1) {
                vdest+=LCD_WIDTH;
            } else if (options.rotate == 2) {
                vdest-=LCD_WIDTH;
            } else {
                vdest++;
            }

            srcpt+=SCALEWS;
        }
        vdest+=sremain;
    }
    }
    rockboy_profile_add(ROCKBOY_TIME_SCALE, *rb->current_tick - scale_start);

    if(L==143)
    {
        unsigned long blit_start = *rb->current_tick;
        if(options.showstats)
        {
            if(options.showstats==1) {
                rb->lcd_putsxyf(0,LCD_HEIGHT-10," %d %d ",
                        options.fps, options.frameskip);
            } else {
                const struct rockboy_profile_totals *p =
                    rockboy_profile_get_totals();
                unsigned long frame_avg = p->samples[ROCKBOY_TIME_FRAME] ?
                    p->total[ROCKBOY_TIME_FRAME] / p->samples[ROCKBOY_TIME_FRAME] : 0;
                unsigned long render_avg = p->samples[ROCKBOY_TIME_RENDER] ?
                    p->total[ROCKBOY_TIME_RENDER] / p->samples[ROCKBOY_TIME_RENDER] : 0;
                unsigned long audio_avg = p->samples[ROCKBOY_TIME_AUDIO_MIX] ?
                    p->total[ROCKBOY_TIME_AUDIO_MIX] /
                    p->samples[ROCKBOY_TIME_AUDIO_MIX] : 0;
                rb->lcd_putsxyf(0, LCD_HEIGHT-20, " FPS:%d FS:%d ",
                                options.fps, options.frameskip);
                rb->lcd_putsxyf(0, LCD_HEIGHT-10, " F:%lu R:%lu A:%lu ",
                                frame_avg, render_avg, audio_avg);
            }
            rb->lcd_update_rect(0,
                                LCD_HEIGHT - (options.showstats == 1 ? 10 : 20),
                                LCD_WIDTH,
                                options.showstats == 1 ? 10 : 20);
        }

        hpt=0x8000;

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
        if(options.scaling >= 2) {
            rb->lcd_blit_pal256((unsigned char*)lcd_fb,(LCD_WIDTH-160)/2, (LCD_HEIGHT-144)/2, (LCD_WIDTH-160)/2, (LCD_HEIGHT-144)/2, 160, 144);
        } else {
            rb->lcd_blit_pal256((unsigned char*)lcd_fb,0,0,0,0,LCD_WIDTH,LCD_HEIGHT);
        }
#else
        if(options.scaling >= 2) {
            rb->lcd_update_rect( (LCD_WIDTH-160)/2, (LCD_HEIGHT-144)/2, 160, 144);
        } else {
            rb->lcd_update();
        }
#endif
        rockboy_profile_add(ROCKBOY_TIME_BLIT, *rb->current_tick - blit_start);
        rockboy_profile_frame_rendered();
    }

#endif
    }
#if LCD_DEPTH == 1
    scanline_ind = (scanline_ind+1) % 8;
#elif LCD_DEPTH == 2
    scanline_ind = (scanline_ind+1) % 4;
#endif

    rockboy_profile_add(ROCKBOY_TIME_RENDER, *rb->current_tick - render_start);
}

#ifdef HAVE_LCD_COLOR
void set_pal(void)
{
    memcpy(dmg_pal,palettes[options.pal], sizeof(dmg_pal));
    pal_dirty();
}

static void updatepalette(int i)
{
    int c, r, g, b;
    fb_data px;

    c = (lcd.pal[i<<1] | ((int)lcd.pal[(i<<1)|1] << 8)) & 0x7FFF;
#if LCD_PIXELFORMAT == RGB565 || LCD_PIXELFORMAT == RGB565SWAPPED
    /* extract color channels to 5 bit red and blue, and 6 bit green */
    r = c & 0x001F;
    g = (c & 0x03E0) >> 4;
    b = c >> 10;
    g |= (g >> 5);
    px = FB_RGBPACK_LCD(r, g, b);
#else
    /* extract color channels and normalize to 8-bit */
    r = (c & 0x001F) << 3;
    g = (c & 0x03E0) >> 2;
    b = c >> 7;
    r |= (r >> 5);
    g |= (g >> 5);
    b |= (b >> 5);
    px = FB_RGBPACK(r, g, b);
#endif
    /* updatepalette might get called, but the pallete does not necessarily
     *  need to be updated.
     */
    if(memcmp(&PAL[i], &px, sizeof(fb_data)))
    {
        PAL[i] = px;
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_PAL256)
        rb->lcd_pal256_update_pal(PAL);
#endif
    }
}
#endif /* HAVE_LCD_COLOR */

void pal_write(int i, byte b)
{
    if (lcd.pal[i] == b) return;
    lcd.pal[i] = b;
#ifdef HAVE_LCD_COLOR
    updatepalette(i>>1);
#endif
}

void pal_write_dmg(int i, int mapnum, byte d)
{
    int j;
    int *cmap = dmg_pal[mapnum];
    int c, r, g, b;

    if (hw.cgb) return;

    for (j = 0; j < 8; j += 2)
    {
        c = cmap[(d >> j) & 3];
        r = (c & 0xf8) >> 3;
        g = (c & 0xf800) >> 6;
        b = (c & 0xf80000) >> 9;
        c = r|g|b;
        /* FIXME - handle directly without faking cgb */
        pal_write(i+j, c & 0xff);
        pal_write(i+j+1, c >> 8);
    }
}

void vram_write(addr a, byte b)
{
    lcd.vbank[R_VBK&1][a] = b;
    if (a >= 0x1800) return;
    rockboy_profile_count(ROCKBOY_EVENT_VRAM_DIRTY_WRITES, 1);
    patdirty[((R_VBK&1)<<9)+(a>>4)] = 1;
    anydirty = 1;
    pal_dirty();
}

void vram_dirty(void)
{
    anydirty = 1;
    memset(patdirty, 1, sizeof patdirty);
}

void pal_dirty(void)
{
#ifdef HAVE_LCD_COLOR
    int i;
#endif
    if (!hw.cgb)
    {

        pal_write_dmg(0, 0, R_BGP);
        pal_write_dmg(8, 1, R_BGP);
        pal_write_dmg(64, 2, R_OBP0);
        pal_write_dmg(72, 3, R_OBP1);
    }
#ifdef HAVE_LCD_COLOR
    for (i = 0; i < 64; i++)
        updatepalette(i);
#endif
}

void lcd_reset(void)
{
    memset(&lcd, 0, sizeof lcd);
    lcd_begin();
    vram_dirty();
}
