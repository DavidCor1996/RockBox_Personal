#include "microui_rockbox.h"

static bool mu_rb_point_in_rect(int x, int y, mu_Rect rect)
{
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

static bool mu_rb_intersect(mu_Rect *rect, mu_Rect clip)
{
    int right = mu_min(rect->x + rect->w, clip.x + clip.w);
    int bottom = mu_min(rect->y + rect->h, clip.y + clip.h);

    rect->x = mu_max(rect->x, clip.x);
    rect->y = mu_max(rect->y, clip.y);
    rect->w = right - rect->x;
    rect->h = bottom - rect->y;
    return rect->w > 0 && rect->h > 0;
}

static void mu_rb_union(mu_Rect *destination, mu_Rect source)
{
    int right;
    int bottom;

    if (source.w <= 0 || source.h <= 0)
        return;
    if (destination->w <= 0 || destination->h <= 0)
    {
        *destination = source;
        return;
    }
    right = mu_max(destination->x + destination->w, source.x + source.w);
    bottom = mu_max(destination->y + destination->h, source.y + source.h);
    destination->x = mu_min(destination->x, source.x);
    destination->y = mu_min(destination->y, source.y);
    destination->w = right - destination->x;
    destination->h = bottom - destination->y;
}

void mu_rb_registry_reset(struct mu_rb_control_registry *registry)
{
    registry->count = 0;
    registry->hover_index = -1;
}

bool mu_rb_registry_add(struct mu_rb_control_registry *registry, mu_Id id,
                        mu_Rect rect, int opt, unsigned int tag, int value)
{
    struct mu_rb_control *control;

    if (rect.w <= 0 || rect.h <= 0 || (opt & MU_OPT_NOINTERACT))
        return true;
    if (registry->count >= MU_RB_CONTROL_LIMIT)
    {
        registry->error = MU_RB_ERR_CONTROL_OVERFLOW;
        return false;
    }
    control = &registry->controls[registry->count++];
    control->id = id;
    control->rect = rect;
    control->opt = opt;
    control->tag = tag;
    control->value = value;
    registry->high_water = mu_max(registry->high_water, registry->count);
    return true;
}

const struct mu_rb_control *mu_rb_registry_hit(
    struct mu_rb_control_registry *registry, int x, int y)
{
    int i;

    registry->hover_index = -1;
    for (i = registry->count - 1; i >= 0; i--)
    {
        if (mu_rb_point_in_rect(x, y, registry->controls[i].rect))
        {
            registry->hover_index = i;
            return &registry->controls[i];
        }
    }
    return NULL;
}

const struct mu_rb_control *mu_rb_registry_focus(
    struct mu_rb_control_registry *registry, int direction)
{
    int selected = -1;
    int next;
    int i;

    if (registry->count <= 0)
    {
        registry->selected_id = 0;
        registry->selected_index = -1;
        return NULL;
    }
    for (i = 0; i < registry->count; i++)
    {
        if (registry->controls[i].id == registry->selected_id)
        {
            selected = i;
            break;
        }
    }
    if (selected < 0 && registry->selected_id != 0)
        next = mu_min(mu_max(registry->selected_index, 0),
                      registry->count - 1);
    else if (selected < 0)
        next = direction < 0 ? registry->count - 1 : 0;
    else if (direction == 0)
        next = selected;
    else
        next = (selected + (direction > 0 ? 1 : registry->count - 1)) %
               registry->count;
    registry->selected_id = registry->controls[next].id;
    registry->selected_index = next;
    return &registry->controls[next];
}

static int mu_rb_text_width(mu_Font font, const char *text, int length)
{
    char buffer[MU_RB_TEXT_LIMIT + 1];
    int width;
    int height;
    int count = length < 0 ? (int)rb->strlen(text) : length;

    count = mu_min(count, MU_RB_TEXT_LIMIT);
    rb->memcpy(buffer, text, count);
    buffer[count] = '\0';
    rb->font_getstringsize((const unsigned char *)buffer, &width, &height,
                           (int)(intptr_t)font);
    return width;
}

static int mu_rb_text_height(mu_Font font)
{
    int width;
    int height;

    rb->font_getstringsize((const unsigned char *)"M", &width, &height,
                           (int)(intptr_t)font);
    return height;
}

static void mu_rb_set_error(struct mu_rb *ui, enum mu_rb_error error)
{
    if (ui->error == MU_RB_ERR_NONE)
        ui->error = error;
}

static void mu_rb_apply_focus(struct mu_rb *ui,
                              const struct mu_rb_control *control)
{
    if (!control)
        return;
    mu_input_mousemove(ui->ctx, control->rect.x + control->rect.w / 2,
                       control->rect.y + control->rect.h / 2);
    mu_set_focus(ui->ctx, control->id);
}

bool mu_rb_focus_next(struct mu_rb *ui)
{
    const struct mu_rb_control *control =
        mu_rb_registry_focus(&ui->registry, 1);

    mu_rb_apply_focus(ui, control);
    return control != NULL;
}

bool mu_rb_focus_previous(struct mu_rb *ui)
{
    const struct mu_rb_control *control =
        mu_rb_registry_focus(&ui->registry, -1);

    mu_rb_apply_focus(ui, control);
    return control != NULL;
}

void mu_rb_clear_input(struct mu_rb *ui)
{
    ui->pointer_down = false;
    ui->ctx->mouse_down = 0;
    ui->ctx->mouse_pressed = 0;
    ui->ctx->key_down = 0;
    ui->ctx->key_pressed = 0;
    ui->ctx->input_text[0] = '\0';
}

enum mu_rb_status mu_rb_init(struct mu_rb *ui, mu_Context *ctx,
                             const struct mu_rb_config *config)
{
    if (!ui || !ctx || !config || config->font < 0)
        return MU_RB_ERROR;
    rb->memset(ui, 0, sizeof(*ui));
    ui->ctx = ctx;
    ui->config = *config;
    if (ui->config.poll_ticks <= 0)
        ui->config.poll_ticks = 1;
    ui->saved_foreground = rb->lcd_get_foreground();
    ui->saved_background = rb->lcd_get_background();
    ui->saved_drawmode = rb->lcd_get_drawmode();
    ui->clip = mu_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    ui->dirty = mu_rect(0, 0, 0, 0);
    mu_init(ctx);
    ctx->userdata = ui;
    ctx->text_width = mu_rb_text_width;
    ctx->text_height = mu_rb_text_height;
    ctx->style->font = (mu_Font)(intptr_t)config->font;
    rb->lcd_setfont(config->font);
    ui->initialized = true;
    return MU_RB_OK;
}

enum mu_rb_status mu_rb_poll(struct mu_rb *ui)
{
    long button;

    if (!ui || !ui->initialized)
        return MU_RB_ERROR;
    if (rb->button_hold())
    {
        mu_rb_clear_input(ui);
        return MU_RB_OK;
    }
    button = rb->button_get_w_tmo(ui->config.poll_ticks);
    if (button == BUTTON_NONE)
        return MU_RB_OK;
    if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
    {
        mu_rb_clear_input(ui);
        return MU_RB_USB_CONNECTED;
    }
#if defined(BUTTON_MENU) && defined(BUTTON_REPEAT)
    if ((button & (BUTTON_MENU | BUTTON_REPEAT)) ==
        (BUTTON_MENU | BUTTON_REPEAT))
    {
        ui->exit_requested = true;
        return MU_RB_EXIT;
    }
#endif
#ifdef BUTTON_MENU
#ifdef BUTTON_REL
    if ((button & (BUTTON_MENU | BUTTON_REL)) == (BUTTON_MENU | BUTTON_REL))
#else
    if (button & BUTTON_MENU)
#endif
    {
        ui->cancel_requested = true;
        return MU_RB_CANCEL;
    }
#endif
#ifdef BUTTON_LEFT
    if ((button & BUTTON_LEFT)
#ifdef BUTTON_REL
        && !(button & BUTTON_REL)
#endif
       )
        return mu_rb_focus_previous(ui) ? MU_RB_REDRAW : MU_RB_OK;
#endif
#ifdef BUTTON_RIGHT
    if ((button & BUTTON_RIGHT)
#ifdef BUTTON_REL
        && !(button & BUTTON_REL)
#endif
       )
        return mu_rb_focus_next(ui) ? MU_RB_REDRAW : MU_RB_OK;
#endif
#if defined(BUTTON_SCROLL_FWD) && defined(BUTTON_SCROLL_BACK)
    if ((button & BUTTON_SCROLL_FWD)
#ifdef BUTTON_REL
        && !(button & BUTTON_REL)
#endif
       )
        return mu_rb_focus_next(ui) ? MU_RB_REDRAW : MU_RB_OK;
    if ((button & BUTTON_SCROLL_BACK)
#ifdef BUTTON_REL
        && !(button & BUTTON_REL)
#endif
       )
        return mu_rb_focus_previous(ui) ? MU_RB_REDRAW : MU_RB_OK;
#endif
#ifdef BUTTON_SELECT
    if (button == BUTTON_SELECT)
    {
        const struct mu_rb_control *control =
            mu_rb_registry_focus(&ui->registry, 0);

        mu_rb_apply_focus(ui, control);
        ui->pointer_down = true;
        mu_input_mousedown(ui->ctx, ui->ctx->mouse_pos.x,
                           ui->ctx->mouse_pos.y, MU_MOUSE_LEFT);
        return MU_RB_REDRAW;
    }
#ifdef BUTTON_REL
    if ((button & (BUTTON_SELECT | BUTTON_REL)) ==
        (BUTTON_SELECT | BUTTON_REL))
    {
        ui->pointer_down = false;
        mu_input_mouseup(ui->ctx, ui->ctx->mouse_pos.x,
                         ui->ctx->mouse_pos.y, MU_MOUSE_LEFT);
        return MU_RB_REDRAW;
    }
#endif
#endif
    return MU_RB_OK;
}

enum mu_rb_status mu_rb_begin(struct mu_rb *ui)
{
    if (!ui || !ui->initialized)
        return MU_RB_ERROR;
    mu_rb_registry_reset(&ui->registry);
    ui->dirty = mu_rect(0, 0, 0, 0);
    ui->clip = mu_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    mu_begin(ui->ctx);
    return ui->ctx->aborted ? MU_RB_ERROR : MU_RB_OK;
}

void mu_rb_control_hook(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt)
{
    struct mu_rb *ui = ctx ? ctx->userdata : NULL;

    if (!ui || !ui->initialized)
        return;
    if (!mu_rb_intersect(&rect, ui->clip))
        return;
    if (!mu_rb_registry_add(&ui->registry, id, rect, opt, 0, 0))
        mu_rb_set_error(ui, MU_RB_ERR_CONTROL_OVERFLOW);
}

static fb_data mu_rb_color(struct mu_rb *ui, mu_Color color)
{
    if (color.a != 255)
        mu_rb_set_error(ui, MU_RB_ERR_ALPHA_UNSUPPORTED);
    return LCD_RGBPACK(color.r, color.g, color.b);
}

static void mu_rb_draw_icon(struct mu_rb *ui, const mu_IconCommand *command)
{
    struct viewport viewport;
    mu_Rect rect = command->rect;
    int left = command->rect.x - ui->clip.x;
    int top = command->rect.y - ui->clip.y;
    int middle_x;
    int middle_y;

    if (command->color.a == 0)
        return;
    if (!mu_rb_intersect(&rect, ui->clip))
        return;
    rb->viewport_set_defaults(&viewport, SCREEN_MAIN);
    viewport.x = ui->clip.x;
    viewport.y = ui->clip.y;
    viewport.width = ui->clip.w;
    viewport.height = ui->clip.h;
    viewport.drawmode = DRMODE_FG;
    viewport.fg_pattern = mu_rb_color(ui, command->color);
    rb->lcd_set_viewport(&viewport);
    rb->lcd_set_foreground(mu_rb_color(ui, command->color));
    middle_x = left + command->rect.w / 2;
    middle_y = top + command->rect.h / 2;
    if (command->id == MU_ICON_CLOSE)
    {
        rb->lcd_drawline(left + 3, top + 3,
                         left + command->rect.w - 4,
                         top + command->rect.h - 4);
        rb->lcd_drawline(left + command->rect.w - 4,
                         top + 3, left + 3,
                         top + command->rect.h - 4);
    }
    else if (command->id == MU_ICON_CHECK)
    {
        rb->lcd_drawline(left + 2, middle_y,
                         middle_x - 1, top + command->rect.h - 3);
        rb->lcd_drawline(middle_x - 1,
                         top + command->rect.h - 3,
                         left + command->rect.w - 3,
                         top + 2);
    }
    else if (command->id == MU_ICON_COLLAPSED)
    {
        rb->lcd_drawline(middle_x - 2, middle_y - 3,
                         middle_x + 2, middle_y);
        rb->lcd_drawline(middle_x + 2, middle_y,
                         middle_x - 2, middle_y + 3);
    }
    else
    {
        rb->lcd_drawline(middle_x - 3, middle_y - 2,
                         middle_x, middle_y + 2);
        rb->lcd_drawline(middle_x, middle_y + 2,
                         middle_x + 3, middle_y - 2);
    }
    rb->lcd_set_viewport(NULL);
    mu_rb_union(&ui->dirty, rect);
}

static void mu_rb_draw_text_command(struct mu_rb *ui,
                                    const mu_TextCommand *command)
{
    struct viewport viewport;
    char text[MU_RB_TEXT_LIMIT + 1];
    int length = rb->strlen(command->str);
    int width;
    int height;
    mu_Rect bounds;

    if (command->color.a == 0)
        return;
    if (length > MU_RB_TEXT_LIMIT)
    {
        length = MU_RB_TEXT_LIMIT;
        mu_rb_set_error(ui, MU_RB_ERR_TEXT_TRUNCATED);
    }
    rb->memcpy(text, command->str, length);
    text[length] = '\0';
    bounds = mu_rect(command->pos.x, command->pos.y,
                     mu_rb_text_width(command->font, text, length),
                     mu_rb_text_height(command->font));
    if (!mu_rb_intersect(&bounds, ui->clip))
        return;
    rb->viewport_set_defaults(&viewport, SCREEN_MAIN);
    viewport.x = ui->clip.x;
    viewport.y = ui->clip.y;
    viewport.width = ui->clip.w;
    viewport.height = ui->clip.h;
    viewport.font = (int)(intptr_t)command->font;
    viewport.drawmode = DRMODE_FG;
    viewport.fg_pattern = mu_rb_color(ui, command->color);
    rb->lcd_set_viewport(&viewport);
    rb->lcd_getstringsize((const unsigned char *)text, &width, &height);
    rb->lcd_putsxy(command->pos.x - ui->clip.x,
                   command->pos.y - ui->clip.y,
                   (const unsigned char *)text);
    rb->lcd_set_viewport(NULL);
    mu_rb_union(&ui->dirty, bounds);
}

enum mu_rb_status mu_rb_render(struct mu_rb *ui)
{
    mu_Command *command = NULL;
    mu_Rect screen = mu_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    const struct mu_rb_control *focused;
    long started = *rb->current_tick;
    bool full;

    if (!ui || !ui->initialized)
        return MU_RB_ERROR;
    if (ui->ctx->aborted || ui->ctx->error != MU_ERROR_NONE)
    {
        mu_rb_set_error(ui, MU_RB_ERR_CORE);
        return MU_RB_ERROR;
    }
    focused = mu_rb_registry_focus(&ui->registry, 0);
    if (focused)
        mu_rb_apply_focus(ui, focused);
    while (mu_next_command(ui->ctx, &command))
    {
        if (command->type == MU_COMMAND_CLIP)
        {
            ui->clip = command->clip.rect;
            if (!mu_rb_intersect(&ui->clip, screen))
                ui->clip = mu_rect(0, 0, 0, 0);
        }
        else if (command->type == MU_COMMAND_RECT)
        {
            mu_Rect rect = command->rect.rect;

            if (command->rect.color.a == 0)
                continue;
            if (mu_rb_intersect(&rect, ui->clip))
            {
                rb->lcd_set_foreground(mu_rb_color(ui,
                                                   command->rect.color));
                rb->lcd_fillrect(rect.x, rect.y, rect.w, rect.h);
                mu_rb_union(&ui->dirty, rect);
            }
        }
        else if (command->type == MU_COMMAND_TEXT)
            mu_rb_draw_text_command(ui, &command->text);
        else if (command->type == MU_COMMAND_ICON)
            mu_rb_draw_icon(ui, &command->icon);
    }
    ui->last_command_bytes = ui->ctx->command_list.idx;
    ui->command_high_water = mu_max(ui->command_high_water,
                                    ui->last_command_bytes);
    ui->frame_count++;
    ui->last_dirty = ui->dirty;
    if (ui->dirty.w <= 0 || ui->dirty.h <= 0)
    {
        ui->last_frame_ticks = *rb->current_tick - started;
        ui->worst_frame_ticks = mu_max(ui->worst_frame_ticks,
                                       ui->last_frame_ticks);
        return MU_RB_OK;
    }
    full = !ui->config.partial_updates ||
           ui->dirty.w * ui->dirty.h > LCD_WIDTH * LCD_HEIGHT * 2 / 3;
    if (full)
    {
        rb->lcd_update();
        ui->full_update_count++;
    }
    else
    {
        rb->lcd_update_rect(ui->dirty.x, ui->dirty.y,
                            ui->dirty.w, ui->dirty.h);
        ui->partial_update_count++;
    }
    rb->lcd_set_foreground(ui->saved_foreground);
    rb->lcd_set_background(ui->saved_background);
    rb->lcd_set_drawmode(ui->saved_drawmode);
    ui->last_frame_ticks = *rb->current_tick - started;
    ui->worst_frame_ticks = mu_max(ui->worst_frame_ticks,
                                   ui->last_frame_ticks);
    return ui->error == MU_RB_ERR_NONE ? MU_RB_OK : MU_RB_ERROR;
}

int mu_rb_open_keyboard(struct mu_rb *ui, char *buffer, int size)
{
    int result;

    if (!ui || !buffer || size <= 0)
        return -1;
    mu_rb_clear_input(ui);
    result = rb->kbd_input(buffer, size, NULL);
    if (ui->registry.selected_id)
        mu_set_focus(ui->ctx, ui->registry.selected_id);
    return result;
}

enum mu_rb_error mu_rb_error(const struct mu_rb *ui)
{
    return ui ? ui->error : MU_RB_ERR_BAD_CONFIG;
}

void mu_rb_clear_error(struct mu_rb *ui)
{
    if (!ui)
        return;
    ui->error = MU_RB_ERR_NONE;
    ui->registry.error = MU_RB_ERR_NONE;
    ui->ctx->error = MU_ERROR_NONE;
}

void mu_rb_shutdown(struct mu_rb *ui)
{
    if (!ui || !ui->initialized)
        return;
    mu_rb_clear_input(ui);
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_foreground(ui->saved_foreground);
    rb->lcd_set_background(ui->saved_background);
    rb->lcd_set_drawmode(ui->saved_drawmode);
    rb->lcd_setfont(FONT_UI);
    ui->ctx->userdata = NULL;
    ui->initialized = false;
}
