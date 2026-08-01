#include "plugin.h"
#include "lib/microui/microui_rockbox.h"

enum demo_page
{
    DEMO_WIDGETS = 0,
    DEMO_STRESS,
    DEMO_DIAGNOSTICS,
};

static mu_Context demo_context;
static struct mu_rb demo_ui;
static enum demo_page demo_page;
static int demo_checkbox;
static mu_Real demo_slider = 40;
static char demo_text[32] = "Rockbox microui";
static bool demo_modal;
static bool demo_followup_redraw;

static void demo_tabs(void)
{
    static const char * const labels[] =
    {
        "Widgets", "Stress", "Diagnostics"
    };
    int widths[] = { 92, 92, -1 };
    int i;

    mu_layout_row(&demo_context, 3, widths, 20);
    for (i = 0; i < (int)ARRAYLEN(labels); i++)
    {
        if (mu_button(&demo_context, labels[i]) & MU_RES_SUBMIT)
            demo_page = i;
    }
}

static void demo_widgets(void)
{
    int width = -1;

    mu_layout_row(&demo_context, 1, &width, 18);
    mu_text(&demo_context,
            "Stable click-wheel focus, bounded integer controls, and "
            "nested clipping use the same immediate-mode declarations.");
    mu_checkbox(&demo_context, "Enable the bounded option", &demo_checkbox);
    mu_slider_ex(&demo_context, &demo_slider, 0, 100, 5, "%ld",
                 MU_OPT_ALIGNCENTER);
    mu_layout_row(&demo_context, 2, (int[]){ -110, 104 }, 20);
    mu_textbox(&demo_context, demo_text, sizeof(demo_text));
    if (mu_button(&demo_context, "Keyboard") & MU_RES_SUBMIT)
        mu_rb_open_keyboard(&demo_ui, demo_text, sizeof(demo_text));
    if (mu_begin_treenode(&demo_context, "Dynamic controls"))
    {
        mu_layout_row(&demo_context, 1, &width, 18);
        mu_label(&demo_context, demo_checkbox ?
                 "The optional row is present" :
                 "Toggle the checkbox to change this row");
        if (demo_checkbox)
            mu_button(&demo_context, "Stable ID survives the change");
        mu_end_treenode(&demo_context);
    }
    mu_layout_row(&demo_context, 1, &width, 48);
    mu_begin_panel(&demo_context, "Nested panel");
    mu_layout_row(&demo_context, 1, &width, 18);
    mu_label(&demo_context, "Panel content stays inside its clip.");
    mu_button(&demo_context, "Nested control");
    mu_end_panel(&demo_context);
    if (mu_button(&demo_context, "Open popup menu") & MU_RES_SUBMIT)
        mu_open_popup(&demo_context, "Demo popup");
    if (mu_begin_popup(&demo_context, "Demo popup"))
    {
        mu_layout_row(&demo_context, 1, &width, 18);
        if (mu_button(&demo_context, "Popup action") & MU_RES_SUBMIT)
            mu_get_current_container(&demo_context)->open = 0;
        if (mu_button(&demo_context, "Close popup") & MU_RES_SUBMIT)
            mu_get_current_container(&demo_context)->open = 0;
        mu_end_popup(&demo_context);
    }
    if (mu_button(&demo_context, "Open modal confirmation") & MU_RES_SUBMIT)
        demo_modal = true;
}

static void demo_stress(void)
{
    char label[20];
    int widths[] = { 92, 92, -1 };
    int i;

    mu_layout_row(&demo_context, 3, widths, 14);
    for (i = 0; i < 45; i++)
    {
        rb->snprintf(label, sizeof(label), "Control %02d", i + 1);
        mu_button(&demo_context, label);
    }
}

static void demo_diagnostics(void)
{
    char line[64];
    int width = -1;

    mu_layout_row(&demo_context, 1, &width, 18);
    rb->snprintf(line, sizeof(line), "Context: %lu bytes",
                 (unsigned long)sizeof(demo_context));
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Commands: %d / %d bytes",
                 demo_ui.last_command_bytes, demo_ui.command_high_water);
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Controls: %d / %d",
                 demo_ui.registry.count, demo_ui.registry.high_water);
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Roots/containers: %d / %d",
                 demo_context.root_high_water,
                 demo_context.container_high_water);
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Stacks clip/id/layout: %d/%d/%d",
                 demo_context.clip_high_water, demo_context.id_high_water,
                 demo_context.layout_high_water);
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Dirty: %d,%d %dx%d",
                 demo_ui.last_dirty.x, demo_ui.last_dirty.y,
                 demo_ui.last_dirty.w, demo_ui.last_dirty.h);
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Render ticks: %ld now, %ld worst",
                 demo_ui.last_frame_ticks, demo_ui.worst_frame_ticks);
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Updates: %lu full, %lu partial",
                 demo_ui.full_update_count, demo_ui.partial_update_count);
    mu_label(&demo_context, line);
    rb->snprintf(line, sizeof(line), "Core/adapter error: %d / %d",
                 demo_context.error, mu_rb_error(&demo_ui));
    mu_label(&demo_context, line);
    if (mu_button(&demo_context, "Clear diagnostics") & MU_RES_SUBMIT)
        mu_rb_clear_error(&demo_ui);
}

static void demo_draw(void)
{
    if (mu_begin_window_ex(&demo_context, "microui Rockbox",
                           mu_rect(3, 3, LCD_WIDTH - 6, LCD_HEIGHT - 6),
                           MU_OPT_NOCLOSE | MU_OPT_NORESIZE))
    {
        demo_tabs();
        if (demo_page == DEMO_WIDGETS)
            demo_widgets();
        else if (demo_page == DEMO_STRESS)
            demo_stress();
        else
            demo_diagnostics();
        mu_end_window(&demo_context);
    }
    if (demo_modal &&
        mu_begin_window_ex(&demo_context, "Confirmation",
                           mu_rect((LCD_WIDTH - 220) / 2,
                                   (LCD_HEIGHT - 92) / 2, 220, 92),
                           MU_OPT_NOCLOSE | MU_OPT_NORESIZE))
    {
        int widths[] = { -1, -1 };
        int full = -1;

        mu_layout_row(&demo_context, 1, &full, 20);
        mu_label(&demo_context, "The modal retained stable focus.");
        mu_layout_row(&demo_context, 2, widths, 20);
        if (mu_button(&demo_context, "Cancel") & MU_RES_SUBMIT)
        {
            demo_modal = false;
            demo_followup_redraw = true;
        }
        if (mu_button(&demo_context, "Accept") & MU_RES_SUBMIT)
        {
            demo_modal = false;
            demo_followup_redraw = true;
        }
        mu_end_window(&demo_context);
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    const struct mu_rb_config config =
    {
        .font = FONT_UI,
        .poll_ticks = MAX(1, HZ / 50),
        .partial_updates = true,
    };
    enum plugin_status result = PLUGIN_OK;
    bool running = true;
    bool redraw = true;

    (void)parameter;
    if (mu_rb_init(&demo_ui, &demo_context, &config) != MU_RB_OK)
        return PLUGIN_ERROR;
    while (running)
    {
        enum mu_rb_status status = mu_rb_poll(&demo_ui);

        if (status == MU_RB_USB_CONNECTED)
        {
            result = PLUGIN_USB_CONNECTED;
            break;
        }
        if (status == MU_RB_EXIT)
            break;
        if (status == MU_RB_CANCEL)
        {
            if (demo_modal)
                demo_modal = false;
            else
                running = false;
            redraw = true;
        }
        else if (status == MU_RB_REDRAW)
            redraw = true;
        if (!running || !redraw)
            continue;
        if (mu_rb_begin(&demo_ui) != MU_RB_OK)
        {
            result = PLUGIN_ERROR;
            break;
        }
        redraw = demo_followup_redraw;
        demo_followup_redraw = false;
        demo_draw();
        mu_end(&demo_context);
        if (mu_rb_render(&demo_ui) == MU_RB_ERROR &&
            demo_context.aborted)
        {
            result = PLUGIN_ERROR;
            break;
        }
    }
    mu_rb_shutdown(&demo_ui);
    return result;
}
