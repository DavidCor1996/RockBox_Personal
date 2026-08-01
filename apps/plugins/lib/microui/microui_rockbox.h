#ifndef MICROUI_ROCKBOX_H
#define MICROUI_ROCKBOX_H

#include "plugin.h"
#include "microui_config.h"
#include "microui.h"

#define MU_RB_CONTROL_LIMIT 64
#define MU_RB_TEXT_LIMIT 128

enum mu_rb_status
{
    MU_RB_OK = 0,
    MU_RB_REDRAW,
    MU_RB_CANCEL,
    MU_RB_EXIT,
    MU_RB_USB_CONNECTED,
    MU_RB_ERROR,
};

enum mu_rb_error
{
    MU_RB_ERR_NONE = 0,
    MU_RB_ERR_BAD_CONFIG,
    MU_RB_ERR_CONTROL_OVERFLOW,
    MU_RB_ERR_ALPHA_UNSUPPORTED,
    MU_RB_ERR_TEXT_TRUNCATED,
    MU_RB_ERR_CORE,
};

struct mu_rb_control
{
    mu_Id id;
    mu_Rect rect;
    int opt;
    unsigned int tag;
    int value;
};

struct mu_rb_control_registry
{
    struct mu_rb_control controls[MU_RB_CONTROL_LIMIT];
    int count;
    int high_water;
    int hover_index;
    int selected_index;
    mu_Id selected_id;
    enum mu_rb_error error;
};

struct mu_rb_config
{
    int font;
    int poll_ticks;
    bool partial_updates;
};

struct mu_rb
{
    mu_Context *ctx;
    struct mu_rb_config config;
    struct mu_rb_control_registry registry;
    mu_Rect clip;
    mu_Rect dirty;
    mu_Rect last_dirty;
    enum mu_rb_error error;
    unsigned int saved_foreground;
    unsigned int saved_background;
    int saved_drawmode;
    bool initialized;
    bool pointer_down;
    bool exit_requested;
    bool cancel_requested;
    unsigned long frame_count;
    unsigned long full_update_count;
    unsigned long partial_update_count;
    long last_frame_ticks;
    long worst_frame_ticks;
    int last_command_bytes;
    int command_high_water;
};

void mu_rb_registry_reset(struct mu_rb_control_registry *registry);
bool mu_rb_registry_add(struct mu_rb_control_registry *registry, mu_Id id,
                        mu_Rect rect, int opt, unsigned int tag, int value);
const struct mu_rb_control *mu_rb_registry_hit(
    struct mu_rb_control_registry *registry, int x, int y);
const struct mu_rb_control *mu_rb_registry_focus(
    struct mu_rb_control_registry *registry, int direction);

enum mu_rb_status mu_rb_init(struct mu_rb *ui, mu_Context *ctx,
                             const struct mu_rb_config *config);
enum mu_rb_status mu_rb_poll(struct mu_rb *ui);
enum mu_rb_status mu_rb_begin(struct mu_rb *ui);
enum mu_rb_status mu_rb_render(struct mu_rb *ui);
void mu_rb_shutdown(struct mu_rb *ui);

bool mu_rb_focus_next(struct mu_rb *ui);
bool mu_rb_focus_previous(struct mu_rb *ui);
int mu_rb_open_keyboard(struct mu_rb *ui, char *buffer, int size);
enum mu_rb_error mu_rb_error(const struct mu_rb *ui);
void mu_rb_clear_error(struct mu_rb *ui);
void mu_rb_clear_input(struct mu_rb *ui);
void mu_rb_control_hook(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt);

#endif
