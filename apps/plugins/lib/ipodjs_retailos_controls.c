/* Cached exact-resource surfaces for existing Apple-themed plugin menus. */
#include "plugin.h"
#include "ipodjs_retailos_controls.h"

static void retailos_blit_tiled_part(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int source_x, int source_y, int source_width, int source_height,
    int x, int y, int width, int height)
{
    int drawn_y = 0;

    while (drawn_y < height)
    {
        int part_height = MIN(source_height, height - drawn_y);
        int drawn_x = 0;

        while (drawn_x < width)
        {
            int part_width = MIN(source_width, width - drawn_x);

            ipodjs_retailos_blit_part(
                display, image, source_x, source_y,
                x + drawn_x, y + drawn_y, part_width, part_height);
            drawn_x += part_width;
        }
        drawn_y += part_height;
    }
}

bool ipodjs_retailos_draw_nine_slice(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int x, int y, int width, int height, int border)
{
    int center_width;
    int center_height;

    if (!display || !image || !image->pixels || border <= 0 ||
        image->width <= border * 2 || image->height <= border * 2 ||
        width < border * 2 || height < border * 2)
        return false;

    center_width = width - border * 2;
    center_height = height - border * 2;
    ipodjs_retailos_blit_part(display, image, 0, 0,
                              x, y, border, border);
    ipodjs_retailos_blit_part(
        display, image, image->width - border, 0,
        x + width - border, y, border, border);
    ipodjs_retailos_blit_part(
        display, image, 0, image->height - border,
        x, y + height - border, border, border);
    ipodjs_retailos_blit_part(
        display, image, image->width - border, image->height - border,
        x + width - border, y + height - border, border, border);
    retailos_blit_tiled_part(
        display, image, border, 0, image->width - border * 2, border,
        x + border, y, center_width, border);
    retailos_blit_tiled_part(
        display, image, border, image->height - border,
        image->width - border * 2, border,
        x + border, y + height - border, center_width, border);
    retailos_blit_tiled_part(
        display, image, 0, border, border, image->height - border * 2,
        x, y + border, border, center_height);
    retailos_blit_tiled_part(
        display, image, image->width - border, border,
        border, image->height - border * 2,
        x + width - border, y + border, border, center_height);
    retailos_blit_tiled_part(
        display, image, border, border,
        image->width - border * 2, image->height - border * 2,
        x + border, y + border, center_width, center_height);
    return true;
}

bool ipodjs_retailos_draw_parts(
    struct screen *display, const struct ipodjs_retailos_image parts[3],
    int x, int y, int width, int height)
{
    int draw_height;
    int source_y;
    int target_y;
    int center_width;
    int drawn;

    if (!display || !parts[0].pixels || !parts[1].pixels ||
        !parts[2].pixels || parts[0].height != parts[1].height ||
        parts[0].height != parts[2].height || parts[1].width == 0 ||
        width < parts[0].width + parts[2].width || height <= 0)
        return false;

    draw_height = MIN(height, (int)parts[0].height);
    source_y = (parts[0].height - draw_height) / 2;
    target_y = y + (height - draw_height) / 2;
    ipodjs_retailos_blit_part(display, &parts[0], 0, source_y,
                              x, target_y, parts[0].width, draw_height);
    center_width = width - parts[0].width - parts[2].width;
    drawn = 0;
    while (drawn < center_width)
    {
        int part_width = MIN((int)parts[1].width, center_width - drawn);

        ipodjs_retailos_blit_part(
            display, &parts[1], 0, source_y,
            x + parts[0].width + drawn, target_y,
            part_width, draw_height);
        drawn += part_width;
    }
    ipodjs_retailos_blit_part(
        display, &parts[2], 0, source_y,
        x + width - parts[2].width, target_y,
        parts[2].width, draw_height);
    return true;
}


bool ipodjs_retailos_prepare_pin(struct ipodjs_retailos_pin_cache *cache)
{
    static const char * const fields[3] = {
        "system-input-field-left", "system-input-field-middle",
        "system-input-field-right"
    };
    static const char * const selected[3] = {
        "optionbar-white-thumb-left", "optionbar-white-thumb-center",
        "optionbar-white-thumb-right"
    };
    if (!cache || !cache->panel_data || !cache->field_data ||
        !cache->selected_data)
        return false;
    if (cache->tried)
        return cache->valid;
    cache->tried = true;
    bool valid = ipodjs_retailos_load_named_rga("system-quick-scroll",
        cache->panel_data, IPODJS_RETAILOS_PIN_PANEL_BYTES,
        74, 70, &cache->panel);
    for (int part = 0; part < 3; part++)
    {
        valid &= ipodjs_retailos_load_named_rga(fields[part],
            cache->field_data + part * 780, 780, 10, 26,
            &cache->field[part]);
        valid &= ipodjs_retailos_load_named_rga(selected[part],
            cache->selected_data + part * 696, 696,
            part == 1 ? 1 : 8, 29, &cache->selected[part]);
    }
    cache->valid = valid;
    return valid;
}
