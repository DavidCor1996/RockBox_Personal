/***************************************************************************
 * Nibiru touch pointer.  Keep gain, glide and lift behavior in step with the
 * Desktop Mode wheel implementation; read the same saved speed/direction.
 ****************************************************************************/
#ifndef SCUMMVM_AGDS_POINTER_H
#define SCUMMVM_AGDS_POINTER_H

#ifdef HAVE_WHEEL_POSITION
#define AGDS_POINTER_WHEEL_SUBPIXEL 16
#define AGDS_POINTER_WHEEL_PACKET_CAP 6
#define AGDS_POINTER_POINTER_UPDATE_TICKS MAX(1, HZ / 50)
#define AGDS_POINTER_ABS(x) ((x) < 0 ? -(x) : (x))
static struct {
    int pointer_speed;
    bool reverse_wheel;
} agds_pointer_settings = { 2, false };

static void agds_pointer_load_settings(void)
{
    char line[MAX_PATH + 64];
    int fd = rb->open(PLUGIN_APPS_DATA_DIR "/desktop_mode.cfg", O_RDONLY);

    agds_pointer_settings.pointer_speed = 2;
    agds_pointer_settings.reverse_wheel = false;
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0) {
        char *name;
        char *value;

        if (!rb->settings_parseline(line, &name, &value))
            continue;
        if (!rb->strcmp(name, "snow pointer speed"))
            agds_pointer_settings.pointer_speed =
                MAX(1, MIN(4, rb->atoi(value)));
        else if (!rb->strcmp(name, "snow reverse wheel"))
            agds_pointer_settings.reverse_wheel =
                !rb->strcasecmp(value, "on");
    }
    rb->close(fd);
}
#endif

#ifdef HAVE_WHEEL_POSITION
/* Resting a finger on the wheel keeps the pointer travelling.
 *
 * Tangential motion alone means crossing the screen takes repeated strokes,
 * and reaching a corner is tedious.  Holding a finger still on the ring -
 * touching, not clicking - glides the pointer the way that point of the ring
 * faces: the top of the wheel is up, the right is right, and so on.  The
 * glide begins on contact so the pointer acknowledges the finger immediately,
 * then accelerates gently to a cap the longer it is held.
 *
 * Movement accumulates in sixteenths of a pixel so slow glides stay smooth
 * instead of stepping a whole pixel per sample.
 */
#define AGDS_POINTER_GLIDE_RAMP (HZ * 3 / 4)
#define AGDS_POINTER_GLIDE_SUBPIXEL 16
/* Sixteenths of a pixel per tick.  Stationary glide is only a bounded
 * long-distance assist; ordinary wheel motion owns precision pointing. */
#define AGDS_POINTER_GLIDE_MIN_RATE 5
#define AGDS_POINTER_GLIDE_MAX_RATE 14

/* Ring buttons have positions and Select is the mouse button.  Gliding under
 * either would drag the pointer while a menu is raised or between mouse-down
 * and mouse-up, so any held clickwheel button suppresses stationary glide.
 */
static bool agds_pointer_button_down(void)
{
    long status = rb->button_status();

    return (status & (BUTTON_MENU | BUTTON_PLAY | BUTTON_LEFT |
                      BUTTON_RIGHT | BUTTON_SELECT)) != 0;
}

static void agds_pointer_glide_reset(struct scummvm_backend *state, long now)
{
    state->glide_since = now;
    state->glide_tick = now;
    state->glide_emit = now;
    state->glide_x = 0;
    state->glide_y = 0;
}

static void agds_pointer_reset(struct scummvm_backend *state)
{
    state->last_wheel = -1;
    state->wheel_velocity = 0;
    state->wheel_direction = 0;
    state->last_wheel_tick = 0;
    state->glide_since = 0;
    state->glide_tick = 0;
    state->glide_emit = 0;
    state->glide_x = 0;
    state->glide_y = 0;
    state->frac_x = 0;
    state->frac_y = 0;
}

static bool agds_pointer_glide(struct scummvm_backend *state, int wheel, long now)
{
    long held;
    long ticks;
    int old_x;
    int old_y;
    int angle;
    int rate;
    int moved_x;
    int moved_y;

    if (state->glide_since == 0)
        agds_pointer_glide_reset(state, now);
    if (agds_pointer_button_down() || state->mouse_down)
    {
        /* Re-arm from rest.  Keeping the old held duration here made glide
         * resume at maximum speed on Select release and moved the pointer
         * between mouse-down and the click. */
        agds_pointer_glide_reset(state, now);
        return false;
    }
    held = now - state->glide_since;
    /* Displacement follows elapsed ticks, not how often this happens to be
     * polled, so the pointer travels at the same speed on a 6G and a 5G and
     * whether or not a window is being composed. */
    ticks = now - state->glide_tick;
    if (ticks <= 0)
        return false;
    state->glide_tick = now;

    rate = AGDS_POINTER_GLIDE_MIN_RATE +
           (int)(held * (AGDS_POINTER_GLIDE_MAX_RATE - AGDS_POINTER_GLIDE_MIN_RATE) /
                 MAX(1, AGDS_POINTER_GLIDE_RAMP));
    rate = MIN(AGDS_POINTER_GLIDE_MAX_RATE, rate) *
           (MAX(1, agds_pointer_settings.pointer_speed) + 2) / 4;
    rate = (int)MIN((long)rate * ticks, (long)AGDS_POINTER_GLIDE_MAX_RATE * HZ);

    /* wheel position 0 is the top of the ring; the pointer follows the
     * direction that point faces, so the ring reads like a compass. */
    angle = wheel * 360 / 96 - 90;
    moved_x = fp14_cos(angle) * rate / 16384;
    moved_y = fp14_sin(angle) * rate / 16384;
    if (agds_pointer_settings.reverse_wheel)
    {
        moved_x = -moved_x;
        moved_y = -moved_y;
    }
    state->glide_x += moved_x;
    state->glide_y += moved_y;
    /* Displacement integrates every poll, but a frame is only committed at
     * the interaction refresh rate: a glide must not drive the compositor
     * faster than ordinary interaction does. */
    if (TIME_BEFORE(now, state->glide_emit + AGDS_POINTER_POINTER_UPDATE_TICKS))
        return false;
    moved_x = state->glide_x / AGDS_POINTER_GLIDE_SUBPIXEL;
    moved_y = state->glide_y / AGDS_POINTER_GLIDE_SUBPIXEL;
    if (moved_x == 0 && moved_y == 0)
        return false;
    state->glide_emit = now;
    state->glide_x -= moved_x * AGDS_POINTER_GLIDE_SUBPIXEL;
    state->glide_y -= moved_y * AGDS_POINTER_GLIDE_SUBPIXEL;
    old_x = state->cursor_x;
    old_y = state->cursor_y;
    state->cursor_x = MAX(0, MIN(LCD_WIDTH - 2, state->cursor_x + moved_x));
    state->cursor_y = MAX(0, MIN(LCD_HEIGHT - 2, state->cursor_y + moved_y));
    if (state->cursor_x == old_x && state->cursor_y == old_y)
        return false;
    return true;
}
#endif /* HAVE_WHEEL_POSITION */

#ifdef HAVE_WHEEL_POSITION
static bool agds_pointer_poll(struct scummvm_backend *state)
{
    int wheel = rb->wheel_status();
    int old;
    int angle_old;
    int angle_new;
    int dx;
    int dy;
    int gain16;
    int delta;
    int direction;
    int old_x;
    int old_y;
    long now = *rb->current_tick;
    long elapsed;

    /* Current iPod firmware retains the absolute position while the wheel is
     * touched, so -1 is a definite lift and must brake immediately. */
    if (wheel < 0)
    {
        agds_pointer_reset(state);
        return false;
    }
    state->wheel_available = true;
    if (agds_pointer_button_down()) {
        agds_pointer_reset(state);
        return false;
    }
    if (state->last_wheel < 0)
    {
        int touch_dx;
        int touch_dy;

        state->last_wheel = wheel;
        state->last_wheel_tick = now;
        agds_pointer_glide_reset(state, now);
        /* A clickwheel touch is the start of a pointing gesture.  Give it one
         * deterministic pixel in the direction of the touched point so the
         * cursor responds on the first packet, then let the subpixel glide
         * continue smoothly from there.  One pixel is small enough that a
         * Select press still lands on the intended control. */
        angle_new = wheel * 360 / 96 - 90;
        touch_dx = fp14_cos(angle_new);
        touch_dy = fp14_sin(angle_new);
        if (agds_pointer_settings.reverse_wheel)
        {
            touch_dx = -touch_dx;
            touch_dy = -touch_dy;
        }
        touch_dx = touch_dx > 4096 ? 1 : touch_dx < -4096 ? -1 : 0;
        touch_dy = touch_dy > 4096 ? 1 : touch_dy < -4096 ? -1 : 0;
        old_x = state->cursor_x;
        old_y = state->cursor_y;
        state->cursor_x = MAX(0, MIN(LCD_WIDTH - 2,
                                     state->cursor_x + touch_dx));
        state->cursor_y = MAX(0, MIN(LCD_HEIGHT - 2,
                                     state->cursor_y + touch_dy));
        if (state->cursor_x == old_x && state->cursor_y == old_y)
            return false;
        return true;
    }
    old = state->last_wheel;
    if (old == wheel)
        return agds_pointer_glide(state, wheel, now);
    agds_pointer_glide_reset(state, now);
    delta = wheel - old;
    if (delta > 48)
        delta -= 96;
    else if (delta < -48)
        delta += 96;
    /* Every hardware count contributes to the subpixel accumulator.  The
     * clickwheel driver already reports a stable absolute position while the
     * finger rests, so a second deadband here only makes careful motion feel
     * sticky. */
    direction = delta < 0 ? -1 : 1;
    elapsed = now - state->last_wheel_tick;
    if (state->wheel_direction != 0 &&
        direction != state->wheel_direction)
        state->wheel_velocity = 0;
    if (elapsed > HZ / 4)
        state->wheel_velocity = 0;
    state->wheel_direction = direction;
    state->last_wheel = wheel;
    state->last_wheel_tick = now;
    angle_old = old * 360 / 96 - 90;
    angle_new = wheel * 360 / 96 - 90;
    dx = fp14_cos(angle_new) - fp14_cos(angle_old);
    dy = fp14_sin(angle_new) - fp14_sin(angle_old);
    if (agds_pointer_settings.reverse_wheel)
    {
        dx = -dx;
        dy = -dy;
    }
    if (elapsed <= 0)
        elapsed = 1;
    state->wheel_velocity = MIN(
        4, (state->wheel_velocity * 2 +
            MIN(4, AGDS_POINTER_ABS(delta) * HZ / elapsed / 12)) / 3);
    /* Begin every gesture at precision gain and blend travel speed in over
     * the next eight counts after the four-count precision band.  Fractional
     * gain avoids a packet boundary that suddenly jumps several pixels. */
    gain16 = 32 + (MAX(1, agds_pointer_settings.pointer_speed) - 1) * 12 *
             MIN(8, MAX(0, AGDS_POINTER_ABS(delta) - 4)) / 8 +
             state->wheel_velocity * 6;
    /* Carry the fraction of a pixel each sample leaves behind.  Truncating it
     * away made every small movement round to nothing, which is what made
     * fine positioning feel dead; keeping it means the pointer travels in
     * proportion to the finger however slowly it moves. */
    state->frac_x += dx * gain16 / 4096;
    state->frac_y += dy * gain16 / 4096;
    dx = state->frac_x / AGDS_POINTER_WHEEL_SUBPIXEL;
    dy = state->frac_y / AGDS_POINTER_WHEEL_SUBPIXEL;
    dx = MAX(-AGDS_POINTER_WHEEL_PACKET_CAP, MIN(AGDS_POINTER_WHEEL_PACKET_CAP, dx));
    dy = MAX(-AGDS_POINTER_WHEEL_PACKET_CAP, MIN(AGDS_POINTER_WHEEL_PACKET_CAP, dy));
    if (dx == 0 && dy == 0)
        return false;
    state->frac_x -= dx * AGDS_POINTER_WHEEL_SUBPIXEL;
    state->frac_y -= dy * AGDS_POINTER_WHEEL_SUBPIXEL;
    old_x = state->cursor_x;
    old_y = state->cursor_y;
    state->cursor_x = MAX(0, MIN(LCD_WIDTH - 2, state->cursor_x + dx));
    state->cursor_y = MAX(0, MIN(LCD_HEIGHT - 2, state->cursor_y + dy));
    if (state->cursor_x == old_x && state->cursor_y == old_y)
        return false;
    return true;
}
#endif

#endif
