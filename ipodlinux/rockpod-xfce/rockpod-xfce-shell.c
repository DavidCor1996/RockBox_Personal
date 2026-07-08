/*
 * RockPod Xfce Shell
 *
 * First native iPod Linux shell skeleton for iPod Video 5G/5.5G.
 * This is a host-printable model, not the final framebuffer backend.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RP_SCREEN_W 320
#define RP_SCREEN_H 240
#define RP_ARRAYLEN(a) (sizeof(a) / sizeof((a)[0]))

struct rp_color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct rp_launcher
{
    const char *name;
    const char *subtitle;
    const char *path;
};

static const char *rp_shared_paths[] =
{
    "/RockPod/Home",
    "/RockPod/Desktop",
    "/RockPod/Documents",
    "/RockPod/Downloads",
    "/Music",
    "/Videos",
    "/.rockbox",
};

static const struct rp_launcher rp_launchers[] =
{
    { "Files", "Browse iPod", "/" },
    { "Home", "RockPod files", "/RockPod/Home" },
    { "Music", "Shared library", "/Music" },
    { "Settings", "Display and system", "/RockPod/Settings" },
    { "Terminal", "BusyBox shell", "/bin/sh" },
    { "Rockbox", "Reboot player", "/sbin/reboot-rockbox" },
};

static uint8_t rp_fb[RP_SCREEN_H][RP_SCREEN_W][3];

static const uint8_t rp_font_digits[10][7] =
{
    { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e },
    { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e },
    { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f },
    { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e },
    { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 },
    { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e },
    { 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e },
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
    { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e },
    { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e },
};

static const uint8_t rp_font_letters[26][7] =
{
    { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },
    { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e },
    { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e },
    { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e },
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f },
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 },
    { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f },
    { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },
    { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e },
    { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c },
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f },
    { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 },
    { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 },
    { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },
    { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 },
    { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d },
    { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 },
    { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e },
    { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 },
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a },
    { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 },
    { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 },
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f },
};

static struct rp_color rp_rgb(int r, int g, int b)
{
    struct rp_color c;

    c.r = (uint8_t)r;
    c.g = (uint8_t)g;
    c.b = (uint8_t)b;
    return c;
}

static void rp_print_rule(void)
{
    puts("+------------------------------------------------+");
}

static void rp_print_header(void)
{
    rp_print_rule();
    puts("| RockPod Linux                         12:00 [] |");
    rp_print_rule();
}

static void rp_print_desktop(void)
{
    size_t i;

    puts("|                                                |");
    puts("|  Xfce-style native iPod desktop                |");
    puts("|                                                |");

    for (i = 0; i < RP_ARRAYLEN(rp_launchers); i++)
    {
        printf("|  %-10s %-31s |\n",
               rp_launchers[i].name,
               rp_launchers[i].subtitle);
    }

    puts("|                                                |");
    puts("|  Select: open   Menu: panel   Play: power      |");
    rp_print_rule();
}

static void rp_print_shared_paths(void)
{
    size_t i;

    puts("");
    puts("Shared iPod folders:");
    for (i = 0; i < RP_ARRAYLEN(rp_shared_paths); i++)
        printf("  %s\n", rp_shared_paths[i]);
}

static void rp_put_pixel(int x, int y, struct rp_color c)
{
    if (x < 0 || x >= RP_SCREEN_W || y < 0 || y >= RP_SCREEN_H)
        return;

    rp_fb[y][x][0] = c.r;
    rp_fb[y][x][1] = c.g;
    rp_fb[y][x][2] = c.b;
}

static void rp_fill_rect(int x, int y, int w, int h, struct rp_color c)
{
    int yy;
    int xx;

    for (yy = y; yy < y + h; yy++)
        for (xx = x; xx < x + w; xx++)
            rp_put_pixel(xx, yy, c);
}

static void rp_draw_rect(int x, int y, int w, int h, struct rp_color c)
{
    int i;

    for (i = 0; i < w; i++)
    {
        rp_put_pixel(x + i, y, c);
        rp_put_pixel(x + i, y + h - 1, c);
    }
    for (i = 0; i < h; i++)
    {
        rp_put_pixel(x, y + i, c);
        rp_put_pixel(x + w - 1, y + i, c);
    }
}

static void rp_gradient(void)
{
    int y;
    int x;

    for (y = 0; y < RP_SCREEN_H; y++)
    {
        int r = 35 + (30 * y) / RP_SCREEN_H;
        int g = 75 + (58 * y) / RP_SCREEN_H;
        int b = 112 + (66 * y) / RP_SCREEN_H;
        struct rp_color c = rp_rgb(r, g, b);

        for (x = 0; x < RP_SCREEN_W; x++)
            rp_put_pixel(x, y, c);
    }
}

static const uint8_t *rp_glyph(char ch)
{
    if (ch >= 'a' && ch <= 'z')
        ch = (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z')
        return rp_font_letters[ch - 'A'];
    if (ch >= '0' && ch <= '9')
        return rp_font_digits[ch - '0'];
    return NULL;
}

static void rp_draw_char(int x, int y, char ch, struct rp_color c)
{
    const uint8_t *glyph = rp_glyph(ch);
    int row;
    int col;

    if (!glyph)
        return;

    for (row = 0; row < 7; row++)
        for (col = 0; col < 5; col++)
            if (glyph[row] & (1 << (4 - col)))
                rp_put_pixel(x + col, y + row, c);
}

static void rp_draw_text(int x, int y, const char *text, struct rp_color c)
{
    while (*text)
    {
        if (*text == ':' || *text == '.' || *text == '/')
            rp_fill_rect(x + 2, y + 5, 2, 2, c);
        else if (*text == '-')
            rp_fill_rect(x + 1, y + 3, 4, 1, c);
        else
            rp_draw_char(x, y, *text, c);
        x += 6;
        text++;
    }
}

static void rp_draw_panel(void)
{
    struct rp_color top = rp_rgb(235, 239, 244);
    struct rp_color line = rp_rgb(112, 125, 142);
    struct rp_color text = rp_rgb(18, 28, 38);

    rp_fill_rect(0, 0, RP_SCREEN_W, 22, top);
    rp_fill_rect(0, 21, RP_SCREEN_W, 1, line);
    rp_draw_text(8, 7, "RockPod Linux", text);
    rp_draw_text(247, 7, "12:00", text);
    rp_draw_rect(292, 7, 18, 8, text);
    rp_fill_rect(310, 10, 2, 3, text);
    rp_fill_rect(294, 9, 11, 4, rp_rgb(58, 138, 98));
}

static void rp_draw_icon(int x, int y, int index)
{
    struct rp_color tile = rp_rgb(231, 236, 241);
    struct rp_color edge = rp_rgb(96, 112, 132);
    struct rp_color ink = rp_rgb(34, 85, 135);
    int cx = x + 18;
    int cy = y + 17;

    rp_fill_rect(x + 2, y + 3, 36, 34, rp_rgb(25, 45, 68));
    rp_fill_rect(x, y, 36, 34, tile);
    rp_draw_rect(x, y, 36, 34, rp_rgb(255, 255, 255));
    rp_draw_rect(x + 1, y + 1, 34, 32, edge);

    switch (index)
    {
        case 0:
        case 1:
            rp_fill_rect(x + 8, y + 12, 21, 13, rp_rgb(255, 255, 255));
            rp_draw_rect(x + 8, y + 12, 21, 13, ink);
            rp_fill_rect(x + 11, y + 9, 10, 5, rp_rgb(255, 255, 255));
            rp_draw_rect(x + 11, y + 9, 10, 5, ink);
            break;
        case 2:
            rp_fill_rect(cx - 10, cy - 6, 20, 12, ink);
            rp_fill_rect(cx - 7, cy - 9, 14, 5, ink);
            break;
        case 3:
            rp_draw_rect(cx - 10, cy - 10, 20, 20, ink);
            rp_fill_rect(cx - 2, cy - 12, 4, 24, ink);
            rp_fill_rect(cx - 12, cy - 2, 24, 4, ink);
            break;
        case 4:
            rp_fill_rect(cx - 11, cy - 8, 22, 16, rp_rgb(20, 30, 40));
            rp_draw_text(cx - 8, cy - 3, "SH", rp_rgb(82, 210, 132));
            break;
        default:
            rp_fill_rect(cx - 8, cy - 8, 16, 16, ink);
            rp_draw_rect(cx - 10, cy - 10, 20, 20, ink);
            break;
    }
}

static void rp_draw_desktop_ppm(void)
{
    int i;
    int cols = 3;
    int cell_w = 100;
    int cell_h = 72;
    struct rp_color label = rp_rgb(245, 248, 252);
    struct rp_color muted = rp_rgb(210, 221, 233);

    rp_gradient();
    rp_draw_panel();
    rp_draw_text(12, 31, "Native iPod Linux", muted);

    for (i = 0; i < (int)RP_ARRAYLEN(rp_launchers); i++)
    {
        int col = i % cols;
        int row = i / cols;
        int x = 18 + col * cell_w;
        int y = 55 + row * cell_h;

        rp_draw_icon(x + 20, y, i);
        rp_draw_text(x + 3, y + 43, rp_launchers[i].name, label);
    }

    rp_fill_rect(0, 222, RP_SCREEN_W, 18, rp_rgb(25, 42, 60));
    rp_draw_text(9, 228, "Select open  Menu panel  Play power", muted);
}

static int rp_write_ppm(const char *path)
{
    FILE *fp = fopen(path, "wb");

    if (!fp)
        return 1;

    fprintf(fp, "P6\n%d %d\n255\n", RP_SCREEN_W, RP_SCREEN_H);
    fwrite(rp_fb, 1, sizeof(rp_fb), fp);
    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    if (sizeof(char *) > 1)
    {
        /* Keep the host preview intentionally dependency-free. */
    }

    if (argc > 2 && !strcmp(argv[1], "--preview-ppm"))
    {
        rp_draw_desktop_ppm();
        return rp_write_ppm(argv[2]);
    }

    printf("RockPod Xfce Shell preview (%dx%d target)\n\n",
           RP_SCREEN_W, RP_SCREEN_H);
    rp_print_header();
    rp_print_desktop();
    rp_print_shared_paths();
    return 0;
}
