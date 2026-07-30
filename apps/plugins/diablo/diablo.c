/***************************************************************************
 * Diablo entry point: title screen, then hand off to game.c for the
 * whole town + level-1-dungeon gameplay loop (movement, monsters,
 * inventory, save/load -- see apps/plugins/diablo/README.md).
 *
 * tools/diablo_prepare_assets.py reads the user's own DIABDAT.MPQ and
 * writes a title-screen pack plus a "world pack" per level (dPiece
 * grid, MIN/SOL tables, and the decompressed <name>.cel) under
 * PLUGIN_GAMES_DATA_DIR "/diablo/".
 ****************************************************************************/
#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "world.h"
#include "game.h"

#define DIABLO_DATA_DIR PLUGIN_GAMES_DATA_DIR "/diablo"

#define DPK_MAGIC "DPK1"
#define DPK_HEADER_SIZE 8
#define DPK_PALETTE_SIZE 768

static const struct button_mapping *plugin_contexts[] = {
    pla_main_ctx,
#ifdef HAVE_REMOTE_LCD
    pla_remote_ctx,
#endif
};

static unsigned char title_palette[DPK_PALETTE_SIZE];
static int title_width, title_height;

static bool show_title_screen(void)
{
    int fd;
    unsigned char header[DPK_HEADER_SIZE];
    static fb_data row_buf[LCD_WIDTH];
    static unsigned char index_buf[LCD_WIDTH];
    int y, x, dst_x0;

    fd = rb->open(DIABLO_DATA_DIR "/title.dpk", O_RDONLY);
    if (fd < 0)
        return false;

    if (rb->read(fd, header, DPK_HEADER_SIZE) != DPK_HEADER_SIZE ||
        rb->memcmp(header, DPK_MAGIC, 4) != 0 ||
        rb->read(fd, title_palette, DPK_PALETTE_SIZE) != DPK_PALETTE_SIZE)
    {
        rb->close(fd);
        return false;
    }
    title_width = header[4] | (header[5] << 8);
    title_height = header[6] | (header[7] << 8);

    dst_x0 = (LCD_WIDTH - title_width) / 2;
    if (dst_x0 < 0)
        dst_x0 = 0;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    for (y = 0; y < title_height && y < LCD_HEIGHT; y++)
    {
        int row_w = title_width > LCD_WIDTH ? LCD_WIDTH : title_width;

        if (rb->read(fd, index_buf, row_w) != row_w)
            break;
        for (x = 0; x < row_w; x++)
        {
            unsigned char idx = index_buf[x];
            row_buf[x] = LCD_RGBPACK(title_palette[idx * 3],
                                     title_palette[idx * 3 + 1],
                                     title_palette[idx * 3 + 2]);
        }
        rb->lcd_bitmap(row_buf, dst_x0, y, row_w, 1);
    }
    rb->lcd_update();
    rb->close(fd);
    return true;
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;

    if (show_title_screen())
    {
        rb->button_clear_queue();
        pluginlib_getaction(TIMEOUT_BLOCK, plugin_contexts, ARRAYLEN(plugin_contexts));
    }

    if (!game_run())
    {
        rb->splash(HZ * 2, "Run tools/diablo_prepare_assets.py first");
        return PLUGIN_ERROR;
    }

    return PLUGIN_OK;
}
