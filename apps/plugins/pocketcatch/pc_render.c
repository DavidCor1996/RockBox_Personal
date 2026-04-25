#include "pocketcatch.h"
#include "pc_red_gfx.h"
#include "lib/xlcd.h"

#define PC_GB_LIGHT         LCD_RGBPACK(0xe0, 0xf8, 0xd0)
#define PC_GB_MID           LCD_RGBPACK(0x88, 0xc0, 0x70)
#define PC_GB_DARK          LCD_RGBPACK(0x34, 0x68, 0x56)
#define PC_GB_DEEP          LCD_RGBPACK(0x08, 0x18, 0x20)
#define PC_SKY_TOP          PC_GB_LIGHT
#define PC_SKY_BOTTOM       LCD_RGBPACK(0xc6, 0xe8, 0xb0)
#define PC_FIELD_TOP        LCD_RGBPACK(0xa8, 0xd8, 0x7a)
#define PC_FIELD_BOTTOM     PC_GB_MID
#define PC_PANEL_BG         PC_GB_LIGHT
#define PC_PANEL_TEXT       PC_GB_DEEP
#define PC_PANEL_SUB        PC_GB_DARK
#define PC_DOCK_BG          PC_GB_LIGHT
#define PC_DOCK_LINE        PC_GB_DEEP
#define PC_RING_LARGE       PC_GB_DEEP
#define PC_RING_MEDIUM      PC_GB_DARK
#define PC_RING_SMALL       PC_GB_MID
#define PC_HIT_FLASH        LCD_WHITE
#define PC_SHADOW           PC_GB_DARK
#define PC_ACCENT           PC_GB_DEEP

static fb_data pc_background_cache[LCD_WIDTH * LCD_HEIGHT];
static bool pc_background_cache_ready;
static int pc_background_time_mode = -1;
static fb_data pc_encounter_trainer_pixels[PC_WORLD_TRAINER_BYTES / sizeof(fb_data)];
static struct pc_asset_bitmap pc_encounter_trainer_asset;
static bool pc_encounter_trainer_attempted;

static void clear_encounter_trainer_asset(void)
{
    pc_encounter_trainer_asset.bmp.data = NULL;
    pc_encounter_trainer_asset.bmp.width = 0;
    pc_encounter_trainer_asset.bmp.height = 0;
    pc_encounter_trainer_asset.loaded = false;
    pc_encounter_trainer_asset.external = false;
    pc_encounter_trainer_asset.path[0] = '\0';
}

enum pc_time_mode {
    PC_TIME_DAY = 0,
    PC_TIME_SUNSET,
    PC_TIME_NIGHT,
    PC_TIME_DAWN
};

static int current_time_mode(void)
{
    struct tm *tm = rb->get_time();
    int hour = tm ? tm->tm_hour : 12;

    if (hour >= 6 && hour < 17)
        return PC_TIME_DAY;
    if (hour >= 17 && hour < 20)
        return PC_TIME_SUNSET;
    if (hour >= 20 || hour < 5)
        return PC_TIME_NIGHT;
    return PC_TIME_DAWN;
}

static fb_data mix_color(fb_data a, fb_data b, int t, int max_t)
{
    int ar = RGB_UNPACK_RED(a);
    int ag = RGB_UNPACK_GREEN(a);
    int ab = RGB_UNPACK_BLUE(a);
    int br = RGB_UNPACK_RED(b);
    int bg = RGB_UNPACK_GREEN(b);
    int bb = RGB_UNPACK_BLUE(b);
    int r = ar + ((br - ar) * t) / max_t;
    int g = ag + ((bg - ag) * t) / max_t;
    int bl = ab + ((bb - ab) * t) / max_t;

    return LCD_RGBPACK(r, g, bl);
}

static int wrap_phase(int phase, int period)
{
    int wrapped = phase % period;

    if (wrapped < 0)
        wrapped += period;
    return wrapped;
}

static void draw_text_fg(int x, int y, fb_data color, const char *text)
{
    int old_mode = rb->lcd_get_drawmode();

    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy(x, y, text);
    rb->lcd_set_drawmode(old_mode);
}

static void draw_text_fixed(int x, int y, fb_data color, const char *text)
{
    rb->lcd_setfont(FONT_SYSFIXED);
    draw_text_fg(x, y, color, text);
    rb->lcd_setfont(FONT_UI);
}

static void fit_text_to_width_font(char *buffer, size_t buffer_size,
                                   const char *text, int max_width, int font)
{
    static const char ellipsis[] = "...";
    int text_width;
    int ellipsis_width;
    size_t len;

    rb->strlcpy(buffer, text, buffer_size);
    rb->font_getstringsize(buffer, &text_width, NULL, font);
    if (text_width <= max_width)
        return;

    rb->font_getstringsize(ellipsis, &ellipsis_width, NULL, font);
    len = rb->strlen(buffer);

    while (len > 0)
    {
        buffer[--len] = '\0';
        rb->font_getstringsize(buffer, &text_width, NULL, font);
        if (text_width + ellipsis_width <= max_width)
        {
            rb->strlcat(buffer, ellipsis, buffer_size);
            return;
        }
    }

    buffer[0] = '\0';
}

static void fill_capsule(int x, int y, int w, int h, fb_data color)
{
    int radius = h / 2;

    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x + radius, y, w - 2 * radius, h);
    xlcd_fillcircle(x + radius, y + radius, radius);
    xlcd_fillcircle(x + w - radius - 1, y + radius, radius);
}

static void fill_circle_cache(int cx, int cy, int radius, fb_data color)
{
    int dy;
    int radius_sq = radius * radius;

    for (dy = -radius; dy <= radius; ++dy)
    {
        int dx;
        int y = cy + dy;

        if (y < 0 || y >= LCD_HEIGHT)
            continue;

        for (dx = -radius; dx <= radius; ++dx)
        {
            int x = cx + dx;

            if (x < 0 || x >= LCD_WIDTH)
                continue;

            if (dx * dx + dy * dy <= radius_sq)
                pc_background_cache[y * LCD_WIDTH + x] = color;
        }
    }
}

static void draw_red_sprite_scaled(const unsigned char frame[16][16],
                                   int x, int y, int scale, bool flip_x)
{
    int py;
    int px;
    int sy;
    int sx;

    for (py = 0; py < 16; ++py)
    {
        for (px = 0; px < 16; ++px)
        {
            int src_x = flip_x ? (15 - px) : px;
            unsigned char shade = frame[py][src_x];
            fb_data color;

            if (shade == 3)
                continue;

            switch (shade)
            {
                case 0:
                    color = PC_GB_DEEP;
                    break;
                case 1:
                    color = PC_GB_DARK;
                    break;
                default:
                    color = PC_GB_MID;
                    break;
            }

            rb->lcd_set_foreground(color);
            for (sy = 0; sy < scale; ++sy)
            {
                for (sx = 0; sx < scale; ++sx)
                {
                    rb->lcd_drawpixel(x + px * scale + sx,
                                      y + py * scale + sy);
                }
            }
        }
    }
}

static const struct pc_asset_bitmap *ensure_encounter_trainer_asset(void)
{
    if (pc_encounter_trainer_asset.loaded)
        return &pc_encounter_trainer_asset;

    if (pc_encounter_trainer_attempted)
        return NULL;

    pc_encounter_trainer_attempted = true;
    rb->memset(&pc_encounter_trainer_asset, 0, sizeof(pc_encounter_trainer_asset));
    pc_encounter_trainer_asset.pixels = pc_encounter_trainer_pixels;
    pc_encounter_trainer_asset.capacity = sizeof(pc_encounter_trainer_pixels);
    clear_encounter_trainer_asset();
    if (pc_assets_load_world_trainer(&pc_encounter_trainer_asset, PC_HEADING_N, 1))
        return &pc_encounter_trainer_asset;

    clear_encounter_trainer_asset();
    return NULL;
}

static void ensure_background_cache(void)
{
    fb_data sky_top = PC_SKY_TOP;
    fb_data sky_bottom = PC_SKY_BOTTOM;
    fb_data field_top = PC_FIELD_TOP;
    fb_data field_bottom = PC_FIELD_BOTTOM;
    fb_data sun = LCD_RGBPACK(0xff, 0xf6, 0xce);
    fb_data cloud = LCD_RGBPACK(0xff, 0xff, 0xff);
    int y;
    int mode = current_time_mode();

    if (pc_background_cache_ready && pc_background_time_mode == mode)
        return;

    if (mode == PC_TIME_SUNSET)
    {
        sky_top = LCD_RGBPACK(0xd8, 0xf0, 0xb8);
        sky_bottom = LCD_RGBPACK(0xb8, 0xd8, 0x90);
        field_top = LCD_RGBPACK(0x90, 0xc8, 0x72);
        field_bottom = PC_GB_MID;
        sun = PC_GB_LIGHT;
        cloud = LCD_RGBPACK(0xc8, 0xe8, 0xa8);
    }
    else if (mode == PC_TIME_NIGHT)
    {
        sky_top = LCD_RGBPACK(0x7c, 0xa8, 0x70);
        sky_bottom = LCD_RGBPACK(0x4f, 0x7a, 0x53);
        field_top = LCD_RGBPACK(0x3f, 0x64, 0x4b);
        field_bottom = PC_GB_DEEP;
        sun = PC_GB_LIGHT;
        cloud = PC_GB_MID;
    }
    else if (mode == PC_TIME_DAWN)
    {
        sky_top = LCD_RGBPACK(0xe8, 0xf8, 0xc8);
        sky_bottom = LCD_RGBPACK(0xc8, 0xe0, 0xa0);
        field_top = LCD_RGBPACK(0xa0, 0xd0, 0x76);
        field_bottom = PC_GB_MID;
        sun = PC_GB_LIGHT;
        cloud = LCD_RGBPACK(0xd0, 0xe8, 0xb0);
    }

    for (y = 0; y < PC_GROUND_Y; ++y)
    {
        int x;
        fb_data color = mix_color(sky_top, sky_bottom, y, PC_GROUND_Y);

        for (x = 0; x < LCD_WIDTH; ++x)
            pc_background_cache[y * LCD_WIDTH + x] = color;
    }

    for (y = PC_GROUND_Y; y < LCD_HEIGHT; ++y)
    {
        int x;
        fb_data color = mix_color(field_top, field_bottom,
                                  y - PC_GROUND_Y,
                                  MAX(1, LCD_HEIGHT - PC_GROUND_Y));

        for (x = 0; x < LCD_WIDTH; ++x)
            pc_background_cache[y * LCD_WIDTH + x] = color;
    }

    fill_circle_cache(58, 48, 18, sun);
    fill_circle_cache(46, 40, 8, cloud);
    fill_circle_cache(58, 34, 10, cloud);
    fill_circle_cache(74, 40, 8, cloud);
    pc_background_cache_ready = true;
    pc_background_time_mode = mode;
}

static void draw_background(const struct pc_encounter_state *state)
{
    (void)state;
    ensure_background_cache();
    rb->lcd_bitmap(pc_background_cache, 0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static void draw_shadow(void)
{
    rb->lcd_set_foreground(PC_SHADOW);
    fill_capsule(PC_TARGET_X - 42, PC_GROUND_Y - 4, 84, 16, PC_SHADOW);
}

static void draw_window_frame(int x, int y, int w, int h)
{
    rb->lcd_set_foreground(PC_PANEL_BG);
    rb->lcd_fillrect(x, y, w, h);

    rb->lcd_set_foreground(PC_GB_DEEP);
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_drawrect(x + 2, y + 2, w - 4, h - 4);
}

static void draw_trainer_accent(void)
{
    const struct pc_asset_bitmap *trainer;
    int x = 16;
    int y = PC_GROUND_Y - 30;

    rb->lcd_set_foreground(PC_GB_DEEP);
    rb->lcd_fillrect(x - 1, y + 28, 36, 4);
    trainer = ensure_encounter_trainer_asset();
    if (trainer != NULL && trainer->loaded)
    {
        rb->lcd_bitmap_transparent((const fb_data *)trainer->bmp.data,
                                   x + 18 - trainer->bmp.width / 2,
                                   y + 30 - trainer->bmp.height,
                                   trainer->bmp.width,
                                   trainer->bmp.height);
        return;
    }

    draw_red_sprite_scaled(pc_red_player_frames[1], x, y, 2, false);
}

static void draw_creature_fallback(const struct pc_encounter_state *state, int bob)
{
    int x = PC_TARGET_X - state->creature->sprite_w / 2;
    int y = PC_CREATURE_BASE_Y - state->creature->sprite_h + bob;
    int center_x = x + state->creature->sprite_w / 2;
    int head_y = y + 20;

    rb->lcd_set_foreground(state->creature->primary);
    xlcd_fillcircle(center_x, head_y, 20);
    rb->lcd_fillrect(x + 12, y + 22, state->creature->sprite_w - 24, 24);
    rb->lcd_fillrect(x + 18, y + 42, 8, 12);
    rb->lcd_fillrect(x + state->creature->sprite_w - 26, y + 42, 8, 12);

    rb->lcd_set_foreground(state->creature->secondary);
    xlcd_fillcircle(center_x - 14, head_y - 14, 10);
    xlcd_fillcircle(center_x + 14, head_y - 14, 10);
    rb->lcd_fillrect(center_x - 10, y + 36, 20, 8);

    rb->lcd_set_foreground(state->creature->accent);
    rb->lcd_fillrect(center_x - 10, y + 16, 6, 4);
    rb->lcd_fillrect(center_x + 4, y + 16, 6, 4);
    rb->lcd_fillrect(center_x - 6, y + 28, 12, 4);
}

static void draw_creature(const struct pc_encounter_state *state)
{
    int bob = ((state->total_frames / 4) & 1) ? 1 : -1;

    if (state->assets.creature.loaded)
    {
        int x = PC_TARGET_X - state->assets.creature.bmp.width / 2;
        int y = PC_CREATURE_BASE_Y - state->assets.creature.bmp.height + bob;

        rb->lcd_bitmap_transparent((const fb_data *)state->assets.creature.bmp.data,
                                   x, y,
                                   state->assets.creature.bmp.width,
                                   state->assets.creature.bmp.height);
        return;
    }

    draw_creature_fallback(state, bob);
}

static bool creature_visible(const struct pc_encounter_state *state)
{
    switch (state->phase)
    {
        case PC_PHASE_HIT_RESOLVE:
        case PC_PHASE_SHAKE_1:
        case PC_PHASE_SHAKE_2:
        case PC_PHASE_SHAKE_3:
        case PC_PHASE_CAUGHT:
            return false;
        default:
            return true;
    }
}

static fb_data ring_color_for_radius(const struct pc_ring_state *ring)
{
    int span = MAX(1, ring->max_radius - ring->min_radius);
    int normalized = (ring->radius - ring->min_radius) * 100 / span;

    if (normalized <= 33)
        return PC_RING_SMALL;
    if (normalized <= 66)
        return PC_RING_MEDIUM;
    return PC_RING_LARGE;
}

static void draw_target_ring(const struct pc_encounter_state *state)
{
    int cx = PC_TARGET_X;
    int cy = PC_TARGET_Y + state->creature->target_y_offset;
    fb_data ring_color = ring_color_for_radius(&state->ring);

    rb->lcd_set_foreground(ring_color);
    xlcd_drawcircle(cx, cy, state->ring.radius);
    rb->lcd_set_foreground(PC_HIT_FLASH);
    xlcd_drawcircle(cx, cy, MAX(6, state->ring.radius - 3));
    rb->lcd_drawline(cx - 4, cy, cx + 4, cy);
    rb->lcd_drawline(cx, cy - 4, cx, cy + 4);
}

static void draw_ball_fallback(int x, int y, int wobble)
{
    int radius = 14;

    rb->lcd_set_foreground(LCD_RGBPACK(0xff, 0xff, 0xff));
    xlcd_fillcircle(x, y, radius);
    rb->lcd_set_foreground(LCD_RGBPACK(0xe8, 0x48, 0x48));
    rb->lcd_fillrect(x - radius, y - radius, radius * 2, radius);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(x - radius, y - 1, radius * 2, 3);
    rb->lcd_set_foreground(LCD_WHITE);
    xlcd_fillcircle(x + wobble * 2, y, 5);
    rb->lcd_set_foreground(LCD_BLACK);
    xlcd_drawcircle(x, y, radius);
    xlcd_drawcircle(x + wobble * 2, y, 5);
}

static void draw_ball_spin_overlay(int x, int y, int radius,
                                   int spin_phase, int spin_velocity)
{
    int phase;
    int button_x;
    int button_y;
    int seam_x;
    int tilt;
    int old_mode;
    int halo_offset;

    if (spin_velocity == 0)
        return;

    phase = wrap_phase(spin_phase, 24);
    seam_x = x + (phase < 12 ? phase - 6 : 17 - phase);
    button_x = x + (phase < 12 ? phase / 2 - 3 : 8 - phase / 2);
    button_y = y + (((phase / 3) & 1) ? 1 : -1);
    halo_offset = spin_velocity > 0 ? 4 : -4;
    tilt = spin_velocity > 0 ? 1 : -1;
    old_mode = rb->lcd_get_drawmode();

    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_drawline(x - radius + 2, y, x + radius - 2, y);
    rb->lcd_drawline(seam_x, y - radius + 2,
                     seam_x + tilt * 2, y + radius - 2);
    rb->lcd_fillrect(x - radius - halo_offset - 3, y - 1, 5, 2);
    rb->lcd_fillrect(x + radius - halo_offset - 1, y - 1, 5, 2);

    rb->lcd_set_foreground(PC_HIT_FLASH);
    rb->lcd_fillrect(x - radius - halo_offset - 1, y - 3, 3, 2);
    rb->lcd_fillrect(x + radius - halo_offset + 1, y + 1, 3, 2);
    xlcd_fillcircle(button_x, button_y, 5);

    rb->lcd_set_foreground(LCD_BLACK);
    xlcd_fillcircle(button_x, button_y, 3);
    rb->lcd_set_foreground(PC_HIT_FLASH);
    xlcd_fillcircle(button_x + tilt, button_y - 1, 1);
    rb->lcd_set_drawmode(old_mode);
}

static const struct pc_asset_bitmap *select_ball_bitmap(const struct pc_encounter_state *state,
                                                        int spin_phase, int spin_velocity)
{
    int frame;

    if (!state->assets.ball_idle.loaded)
        return NULL;

    if (spin_velocity == 0)
        return &state->assets.ball_idle;

    frame = wrap_phase(spin_phase / 3, PC_BALL_SPIN_FRAMES);
    if (state->assets.ball_spin[frame].loaded)
        return &state->assets.ball_spin[frame];

    return &state->assets.ball_idle;
}

static int ball_visual_spin_velocity(const struct pc_encounter_state *state,
                                     int spin_velocity)
{
    if (spin_velocity != 0)
        return spin_velocity;

    if (state->phase == PC_PHASE_BALL_HELD &&
        state->input.grabbing &&
        state->input.wheel_events > 0 &&
        state->input.last_wheel_dir != 0)
    {
        return state->input.last_wheel_dir * 8;
    }

    return 0;
}

static void draw_ball(const struct pc_encounter_state *state)
{
    int x = PC_BALL_HOME_X;
    int y = PC_BALL_HOME_Y;
    int wobble = ((state->total_frames / 2) & 1) ? -1 : 1;
    int spin_phase = 0;
    int spin_velocity = 0;
    int radius = 14;
    const struct pc_asset_bitmap *ball_bitmap;

    if (state->phase == PC_PHASE_BALL_HELD)
    {
        y -= 16;
        wobble = state->input.signed_spin > 0 ? 2 : (state->input.signed_spin < 0 ? -2 : wobble);
        spin_phase = state->input.spin_phase;
        spin_velocity = ball_visual_spin_velocity(state, state->input.spin_velocity);
        if (spin_velocity != 0)
        {
            int sway = wrap_phase(spin_phase, 12) - 6;
            x += sway / 3;
            y += (wrap_phase(spin_phase, 8) < 4) ? -1 : 1;
        }
    }
    else if (state->phase == PC_PHASE_BALL_THROWN)
    {
        x = state->throw_state.x;
        y = state->throw_state.y;
        wobble = state->throw_state.curve_bonus ? 2 : wobble;
        spin_phase = state->throw_state.spin_phase;
        spin_velocity = ball_visual_spin_velocity(state, state->throw_state.spin_velocity);
    }
    else if (state->phase >= PC_PHASE_HIT_RESOLVE && state->outcome != PC_CATCH_OUTCOME_MISS)
    {
        x = PC_TARGET_X + state->shake_offset;
        y = PC_GROUND_Y - 4;
        wobble = 0;
    }

    ball_bitmap = select_ball_bitmap(state, spin_phase, spin_velocity);
    if (ball_bitmap != NULL)
    {
        int draw_x = x - ball_bitmap->bmp.width / 2;
        int draw_y = y - ball_bitmap->bmp.height / 2;

        rb->lcd_bitmap_transparent((const fb_data *)ball_bitmap->bmp.data,
                                   draw_x, draw_y,
                                   ball_bitmap->bmp.width,
                                   ball_bitmap->bmp.height);
        radius = MIN(ball_bitmap->bmp.width, ball_bitmap->bmp.height) / 2;
        draw_ball_spin_overlay(x, y, radius, spin_phase, spin_velocity);
        return;
    }

    draw_ball_fallback(x, y, wobble);
    draw_ball_spin_overlay(x, y, radius, spin_phase, spin_velocity);
}

static void draw_preview_path(const struct pc_encounter_state *state)
{
    struct pc_throw_request request;
    struct pc_throw_state preview;
    int i;

    if (!state->input.grabbing)
        return;

    pc_input_snapshot_throw(&state->input, *rb->current_tick, &request);
    if (!request.valid)
        return;

    pc_physics_build_throw(&preview, state->creature, &state->ring, &request);

    rb->lcd_set_foreground(preview.hit ? PC_ACCENT : PC_PANEL_SUB);
    for (i = 2; i < preview.duration_frames; i += 3)
    {
        int quad_q8 = (4 * i * (preview.duration_frames - i) * 256) /
                      (preview.duration_frames * preview.duration_frames);
        int x = preview.start_x +
                ((preview.end_x - preview.start_x) * i) / preview.duration_frames +
                (preview.curve_px * quad_q8) / 256;
        int y = preview.start_y +
                ((preview.end_y - preview.start_y) * i) / preview.duration_frames -
                (preview.arc_height * quad_q8) / 256;
        rb->lcd_fillrect(x - 1, y - 1, 3, 3);
    }
}

static void draw_banner(const struct pc_encounter_state *state)
{
    char line1[PC_BANNER_LINE_CHARS];
    char line2[PC_BANNER_LINE_CHARS];
    int banner_x = 10;
    int banner_y = 10;
    int banner_w = LCD_WIDTH - 20;
    int banner_h = 28;
    int line1_w;
    int line2_w;
    int line_h;

    rb->font_getstringsize("AG", NULL, &line_h, FONT_SYSFIXED);
    draw_window_frame(banner_x, banner_y, banner_w, banner_h);

    fit_text_to_width_font(line1, sizeof(line1), state->banner.line1,
                           banner_w - 12, FONT_SYSFIXED);
    fit_text_to_width_font(line2, sizeof(line2), state->banner.line2,
                           banner_w - 12, FONT_SYSFIXED);
    rb->font_getstringsize(line1, &line1_w, NULL, FONT_SYSFIXED);
    rb->font_getstringsize(line2, &line2_w, NULL, FONT_SYSFIXED);

    draw_text_fixed(banner_x + (banner_w - line1_w) / 2, banner_y + 5,
                    PC_PANEL_TEXT, line1);
    draw_text_fixed(banner_x + (banner_w - line2_w) / 2, banner_y + 6 + line_h,
                    PC_PANEL_SUB, line2);
}

static void draw_bottom_hud(const struct pc_encounter_state *state)
{
    char line1[48];
    char line2[48];
    char line1_fit[48];
    char line2_fit[48];
    int h = 32;
    int y = LCD_HEIGHT - h - 8;
    int x = 8;
    int w = LCD_WIDTH - 16;
    int text_y = y + 5;
    int line_h;

    draw_window_frame(x, y, w, h);
    rb->font_getstringsize("AG", NULL, &line_h, FONT_SYSFIXED);

    if (state->phase == PC_PHASE_BALL_HELD)
    {
        rb->snprintf(line1, sizeof(line1), "SPIN %+d  VEL %d",
                     state->input.signed_spin, state->input.release_velocity);
        rb->snprintf(line2, sizeof(line2), "TARGET %s  %s",
                     pc_throw_tier_label(state->last_tier),
                     pc_assets_source_label(&state->assets));
    }
    else if (state->phase == PC_PHASE_BALL_THROWN)
    {
        rb->snprintf(line1, sizeof(line1), "POWER %d  CURVE %d",
                     state->throw_state.power_score,
                     state->throw_state.curve_px);
        rb->snprintf(line2, sizeof(line2), "%s  CATCH %d.%d%%",
                     pc_throw_tier_label(state->throw_state.tier),
                     state->last_catch_chance / 10,
                     state->last_catch_chance % 10);
    }
    else
    {
        rb->snprintf(line1, sizeof(line1), "%s", state->creature->name);
        rb->snprintf(line2, sizeof(line2), "%s%s",
                     pc_assets_source_label(&state->assets),
#ifdef SIMULATOR
                     "  L/R SPECIES"
#else
                     ""
#endif
                     );
    }

    fit_text_to_width_font(line1_fit, sizeof(line1_fit), line1,
                           w - 12, FONT_SYSFIXED);
    fit_text_to_width_font(line2_fit, sizeof(line2_fit), line2,
                           w - 12, FONT_SYSFIXED);
    draw_text_fixed(x + 6, text_y, PC_PANEL_TEXT, line1_fit);
    draw_text_fixed(x + 6, text_y + line_h, PC_PANEL_SUB, line2_fit);
}

static void draw_result_fx(const struct pc_encounter_state *state)
{
    if (state->phase == PC_PHASE_HIT_RESOLVE)
    {
        int radius = 10 + state->phase_frame * 2;
        rb->lcd_set_foreground(PC_HIT_FLASH);
        xlcd_drawcircle(PC_TARGET_X, PC_TARGET_Y + state->creature->target_y_offset, radius);
    }

    if (state->phase == PC_PHASE_CAUGHT)
    {
        int i;
        for (i = 0; i < 5; ++i)
        {
            int x = PC_TARGET_X - 24 + i * 12;
            int y = PC_TARGET_Y - 20 + ((state->phase_frame + i) % 5) * 3;
            rb->lcd_set_foreground(LCD_RGBPACK(0xff, 0xe0, 0x5b));
            rb->lcd_fillrect(x, y, 4, 4);
        }
    }
}

void pc_render_frame(const struct pc_encounter_state *state)
{
    draw_background(state);
    draw_shadow();
    draw_trainer_accent();
    if (creature_visible(state))
        draw_creature(state);
    if (state->phase <= PC_PHASE_BALL_THROWN)
        draw_target_ring(state);
    draw_preview_path(state);
    draw_ball(state);
    draw_result_fx(state);
    draw_banner(state);
    draw_bottom_hud(state);
    rb->lcd_update();
}
