#include "tamagotchi_time.h"
#include "tamagotchi_clock.h"
#include "tamagotchi_display.h"
#include "upstream/tamalib/tamalib.h"

#define TAMA_SESSION_MAGIC 0x544d534eu
#define TAMA_SESSION_VERSION 1u
#define TAMA_CATCHUP_MAX_SECONDS (60u * 60u * 12u)
#define TAMA_CATCHUP_BUDGET_TICKS (HZ * 12)
#define TAMA_CATCHUP_TICKS_PER_SECOND 32768u
#define TAMA_DEFAULT_ALERT_SECONDS (15u * 60u)
#define TAMA_CATCHUP_UI_INTERVAL (HZ / 2)
#define TAMA_CATCHUP_YIELD_STEPS 512

struct tamagotchi_session
{
    unsigned magic;
    unsigned version;
    time_t last_wall_time;
    unsigned catchup_debt;
};

static bool valid_wall_time(const struct tm *tm)
{
    return tm != NULL &&
           tm->tm_hour >= 0 && tm->tm_hour <= 23 &&
           tm->tm_min >= 0 && tm->tm_min <= 59 &&
           tm->tm_sec >= 0 && tm->tm_sec <= 59 &&
           tm->tm_year >= 100;
}

static time_t wall_time_now(void)
{
    struct tm tm;
    const struct tm *now = rb->get_time();

    if (!valid_wall_time(now))
        return 0;

    tm = *now;
    tm.tm_isdst = -1;
    return rb->mktime(&tm);
}

static bool load_session(struct tamagotchi_session *session)
{
    int fd;

    rb->memset(session, 0, sizeof(*session));
    fd = rb->open(TAMAGOTCHI_SESSION_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    if (rb->read(fd, session, sizeof(*session)) != (ssize_t)sizeof(*session) ||
        session->magic != TAMA_SESSION_MAGIC ||
        session->version != TAMA_SESSION_VERSION)
    {
        rb->close(fd);
        rb->memset(session, 0, sizeof(*session));
        return false;
    }

    rb->close(fd);
    return true;
}

static void save_session(const struct tamagotchi_session *session)
{
    int fd = rb->open(TAMAGOTCHI_SESSION_PATH,
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        return;

    rb->write(fd, session, sizeof(*session));
    rb->close(fd);
}

static unsigned elapsed_since_last_launch(struct tamagotchi_session *session,
                                          time_t now)
{
    unsigned elapsed = 0;

    if (session->last_wall_time > 0 && now > session->last_wall_time)
    {
        time_t delta = now - session->last_wall_time;

        elapsed = delta > (time_t)TAMA_CATCHUP_MAX_SECONDS ?
                  TAMA_CATCHUP_MAX_SECONDS : (unsigned)delta;
    }

    elapsed += session->catchup_debt;
    if (elapsed > TAMA_CATCHUP_MAX_SECONDS)
        elapsed = TAMA_CATCHUP_MAX_SECONDS;

    return elapsed;
}

static unsigned fast_forward_seconds(unsigned seconds)
{
    state_t *state = tamalib_get_state();
    u32_t start_ticks;
    u32_t target_delta;
    long deadline;
    long next_ui;
    int until_yield = TAMA_CATCHUP_YIELD_STEPS;
    unsigned done;
    unsigned last_shown = 0;
    char line[32];

    if (state == NULL || state->tick_counter == NULL || seconds == 0)
        return 0;

    start_ticks = *state->tick_counter;
    target_delta = seconds * TAMA_CATCHUP_TICKS_PER_SECOND;
    deadline = *rb->current_tick + TAMA_CATCHUP_BUDGET_TICKS;
    next_ui = *rb->current_tick + TAMA_CATCHUP_UI_INTERVAL;

    tamagotchi_clock_set_background_work(true);
    tamalib_set_speed(0);

    while ((u32_t)(*state->tick_counter - start_ticks) < target_delta)
    {
        tamalib_step();

        if (TIME_AFTER(*rb->current_tick, deadline))
            break;

        if (--until_yield <= 0)
        {
            int button = rb->button_get(false);

            if (button == SYS_USB_CONNECTED)
                break;

            rb->yield();
            until_yield = TAMA_CATCHUP_YIELD_STEPS;
        }

        done = (*state->tick_counter - start_ticks) /
               TAMA_CATCHUP_TICKS_PER_SECOND;
        if (done != last_shown && TIME_AFTER(*rb->current_tick, next_ui))
        {
            rb->snprintf(line, sizeof(line), "Catching up %um", done / 60);
            tamagotchi_display_show_loading(line);
            next_ui = *rb->current_tick + TAMA_CATCHUP_UI_INTERVAL;
            last_shown = done;
        }
    }

    tamalib_set_speed(1);
    tamagotchi_clock_set_background_work(false);
    tamalib_refresh_hw();
    tamagotchi_display_mark_dirty();

    if ((u32_t)(*state->tick_counter - start_ticks) >= target_delta)
        return seconds;

    done = (*state->tick_counter - start_ticks) / TAMA_CATCHUP_TICKS_PER_SECOND;
    return done > seconds ? seconds : done;
}

void tamagotchi_time_on_launch(void)
{
    struct tamagotchi_session session;
    time_t now = wall_time_now();
    unsigned elapsed;
    unsigned done;

    if (now <= 0)
        return;

    if (!load_session(&session))
    {
        rb->memset(&session, 0, sizeof(session));
        session.magic = TAMA_SESSION_MAGIC;
        session.version = TAMA_SESSION_VERSION;
    }

    elapsed = elapsed_since_last_launch(&session, now);
    if (elapsed > 0)
    {
        tamagotchi_display_show_loading("Catching up...");
        done = fast_forward_seconds(elapsed);
        session.catchup_debt = elapsed - done;

    }

    session.last_wall_time = now;
    save_session(&session);
}

void tamagotchi_time_schedule_alert(unsigned seconds, const char *body)
{
    time_t now = wall_time_now();

    if (seconds == 0)
        seconds = TAMA_DEFAULT_ALERT_SECONDS;

    if (now <= 0)
        return;

    (void)body;
}

void tamagotchi_time_on_exit(void)
{
    struct tamagotchi_session session;
    time_t now = wall_time_now();

    if (now <= 0)
        return;

    if (!load_session(&session))
    {
        rb->memset(&session, 0, sizeof(session));
        session.magic = TAMA_SESSION_MAGIC;
        session.version = TAMA_SESSION_VERSION;
    }

    session.last_wall_time = now;
    save_session(&session);
    tamagotchi_time_schedule_alert(TAMA_DEFAULT_ALERT_SECONDS,
                                   "Check your Tamagotchi");
}
