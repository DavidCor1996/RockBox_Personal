/***************************************************************************
 * iPod Hero - five-lane rhythm game for click-wheel iPods
 ****************************************************************************/

#include "ipodhero.h"
#include "lib/helper.h"

#include <fcntl.h>

enum ih_play_result
{
    IH_PLAY_MENU = 0,
    IH_PLAY_RETRY,
    IH_PLAY_USB,
    IH_PLAY_RESUME
};

static bool ih_seek_song_start(void);
static bool ih_start_selected_song(struct ih_app *app,
                                   char *error, size_t error_size);

#ifdef SIMULATOR
static long ih_sim_lane_button(int lane)
{
    static const long buttons[IH_LANE_COUNT] =
    {
        BUTTON_LEFT, BUTTON_MENU, BUTTON_SELECT, BUTTON_PLAY, BUTTON_RIGHT
    };
    return buttons[lane];
}

static void ih_sim_apply_input(struct ih_app *app, long button)
{
    struct ih_input_event input = ih_input_normalize(&app->input, button);

    if (input.kind == IH_INPUT_PRESS)
        ih_game_press(&app->game, input.lane_mask);
    else if (input.kind == IH_INPUT_RELEASE)
        ih_game_release(&app->game, input.lane_mask);
    else if (input.kind == IH_INPUT_WHEEL || input.kind == IH_INPUT_STAR)
    {
        ih_game_whammy(&app->game);
        if (input.kind == IH_INPUT_STAR)
        {
            ih_game_activate_star(&app->game);
            if (app->game.score.star_active)
                app->sim_star_activated = true;
        }
    }
}

static void ih_sim_autoplay_update(struct ih_app *app)
{
    struct ih_note_event *event;
    int offset = 0;
    uint32_t event_number;
    int lane;

    for (lane = 0; lane < IH_LANE_COUNT; ++lane)
    {
        if ((app->game.held_mask & (1u << lane)) &&
            app->game.song_time_ms >= app->sim_release_at[lane])
            ih_sim_apply_input(app, ih_sim_lane_button(lane) | BUTTON_REL);
    }
    if (app->game.judge_cursor < app->game.chart.event_count)
    {
        event = &app->game.chart.events[app->game.judge_cursor];
        event_number = app->game.judge_cursor;
        if (app->sim_test_mode == 2 && event->duration_ms == 0)
        {
            switch (event_number % 7)
            {
                case 1: offset = 60; break;
                case 2: offset = 110; break;
                case 3: offset = 150; break;
                default: break;
            }
        }
        if (event->state == IH_EVENT_WAITING &&
            app->game.song_time_ms >= (int)event->time_ms + offset)
        {
            if (app->sim_test_mode == 2 && event->duration_ms == 0 &&
                event_number % 7 == 4)
                return;
            if (app->sim_test_mode == 2 && event->duration_ms == 0 &&
                event_number % 7 == 5)
            {
                for (lane = 0; lane < IH_LANE_COUNT; ++lane)
                    if (!(event->lane_mask & (1u << lane)))
                    {
                        ih_sim_apply_input(app, ih_sim_lane_button(lane));
                        ih_sim_apply_input(app,
                            ih_sim_lane_button(lane) | BUTTON_REL);
                        return;
                    }
            }
            for (lane = 0; lane < IH_LANE_COUNT; ++lane)
            {
                if (!(event->lane_mask & (1u << lane)))
                    continue;
                ih_sim_apply_input(app, ih_sim_lane_button(lane));
                if (event->duration_ms > 0)
                    app->sim_release_at[lane] =
                        (int)(event->time_ms + event->duration_ms) -
                        (app->sim_test_mode == 2 ? 200 : 0);
                else
                    ih_sim_apply_input(app,
                                       ih_sim_lane_button(lane) | BUTTON_REL);
            }
        }
    }
    if (!app->game.score.star_active && app->game.score.star_ms >= 5000)
        for (lane = 0; lane < IH_WHEEL_STAR_STEPS; ++lane)
            ih_sim_apply_input(app, BUTTON_SCROLL_FWD);
}

static bool ih_sim_input_self_test(void)
{
    struct ih_input input;
    struct ih_input_event event;
    int step;

    ih_input_reset(&input);
    event = ih_input_normalize(&input, BUTTON_LEFT);
    if (event.kind != IH_INPUT_PRESS || event.lane_mask != 1u)
        return false;
    event = ih_input_normalize(&input, BUTTON_LEFT | BUTTON_REPEAT);
    if (event.kind != IH_INPUT_NONE)
        return false;
    event = ih_input_normalize(&input, BUTTON_LEFT | BUTTON_REL);
    if (event.kind != IH_INPUT_RELEASE || event.lane_mask != 1u)
        return false;
    event = ih_input_normalize(&input, BUTTON_SELECT | BUTTON_MENU);
    if (event.kind != IH_INPUT_PAUSE)
        return false;
    for (step = 1; step <= IH_WHEEL_STAR_STEPS; ++step)
    {
        event = ih_input_normalize(&input, BUTTON_SCROLL_FWD);
        if ((step < IH_WHEEL_STAR_STEPS && event.kind != IH_INPUT_WHEEL) ||
            (step == IH_WHEEL_STAR_STEPS && event.kind != IH_INPUT_STAR))
            return false;
    }
    return true;
}

static void ih_sim_test_save(const struct ih_app *app)
{
    char temporary[MAX_PATH];
    int fd;
    uint32_t broken_sustains = 0;
    uint32_t event_number;

    if (app->sim_test_mode == 0)
        return;
    for (event_number = 0; event_number < app->game.chart.event_count;
         ++event_number)
        if (app->game.chart.events[event_number].flags &
            IH_NOTE_SUSTAIN_BROKEN)
            broken_sustains++;
    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", IH_SIM_TEST_FILE);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd,
        "version=1\nmode=%s\nevents=%lu\nhits=%lu\n"
        "misses=%lu\nperfect=%lu\ngreat=%lu\ngood=%lu\ngrace=%lu\n"
        "max_streak=%lu\nsustain_ms=%lu\nbroken_sustains=%lu\nscore=%lu\n"
        "star_activated=%d\ninput_normalizer=%s\nruns=%lu\n"
        "pause_resume_cycles=%lu\nscripted_pause_cycles=%lu\nretries=%lu\n",
        app->sim_test_mode == 1 ? "perfect-autoplay" :
        app->sim_test_mode == 2 ? "mixed-autoplay" : "lifecycle",
        (unsigned long)app->game.chart.event_count,
        (unsigned long)app->game.score.hit_count,
        (unsigned long)app->game.score.miss_count,
        (unsigned long)app->game.score.perfect_count,
        (unsigned long)app->game.score.great_count,
        (unsigned long)app->game.score.good_count,
        (unsigned long)app->game.score.grace_count,
        (unsigned long)app->game.score.max_streak,
        (unsigned long)app->game.score.sustain_ms,
        (unsigned long)broken_sustains,
        (unsigned long)app->game.score.points,
        app->sim_star_activated ? 1 : 0,
        ih_sim_input_self_test() ? "pass" : "fail",
        (unsigned long)app->sim_runs,
        (unsigned long)app->sim_pause_count,
        (unsigned long)app->sim_scripted_pause_count,
        (unsigned long)(app->sim_runs > 0 ? app->sim_runs - 1 : 0));
    rb->close(fd);
    rb->remove(IH_SIM_TEST_FILE);
    rb->rename(temporary, IH_SIM_TEST_FILE);
}

static void ih_sim_library_save(const struct ih_song_library *library)
{
    char temporary[MAX_PATH];
    int fd;

    rb->snprintf(temporary, sizeof(temporary), "%s.tmp",
                 IH_SIM_LIBRARY_FILE);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "version=1\nsongs=%d\nselected=%d\nfirst=%s - %s\n",
                 library->count, library->selected,
                 library->entries[0].artist, library->entries[0].title);
    rb->close(fd);
    rb->remove(IH_SIM_LIBRARY_FILE);
    rb->rename(temporary, IH_SIM_LIBRARY_FILE);
}
#endif

static void ih_runtime_reset(void)
{
    int fd = rb->open(IH_RUNTIME_FILE,
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd, "IHR1\n");
        rb->close(fd);
    }
}

static void ih_runtime_trace(const struct ih_app *app, const char *stage)
{
    struct mp3entry *id3 = rb->audio_current_track();
    char current[MAX_PATH];
    const char *selected = app->index.audio_path[0] ?
        app->index.audio_path : "-";
    int fd;

    if (id3 != NULL)
        rb->strlcpy(current, id3->path, sizeof(current));
    else
        rb->strlcpy(current, "-", sizeof(current));
    fd = rb->open(IH_RUNTIME_FILE,
                  O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "%ld\t%s\tstatus=%d\tplaylist=%d\tcurrent=%s\tselected=%s\n",
                 *rb->current_tick, stage, rb->audio_status(),
                 rb->playlist_amount(), current, selected);
    rb->close(fd);
}

static void ih_show_error(const char *title, const char *detail)
{
#ifdef SIMULATOR
    char temporary[MAX_PATH];
    int fd;

    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", IH_SIM_ERROR_FILE);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd, "title=%s\ndetail=%s\n", title, detail);
        rb->close(fd);
        rb->remove(IH_SIM_ERROR_FILE);
        rb->rename(temporary, IH_SIM_ERROR_FILE);
    }
#endif
    ih_render_error(title, detail);
}

static void ih_performance_save(const struct ih_app *app)
{
    char temporary[MAX_PATH];
    int fd;
    uint32_t average_x100 = app->game.frame_count ?
        app->game.frame_ticks_total * 100u / app->game.frame_count : 0;
    size_t chart_bytes = app->chart.event_count *
                         sizeof(struct ih_note_event) +
                         app->chart.section_count * sizeof(struct ih_section) +
                         app->chart.section_string_bytes;
    uint32_t target = (app->game.frame_count * 95u + 99u) / 100u;
    uint32_t cumulative = 0;
    uint32_t percentile_95 = 0;
    uint32_t bucket;

    for (bucket = 0; bucket < ARRAYLEN(app->game.frame_tick_histogram);
         ++bucket)
    {
        cumulative += app->game.frame_tick_histogram[bucket];
        if (cumulative >= target)
        {
            percentile_95 = bucket;
            break;
        }
    }

    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", IH_PERF_FILE);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd,
        "version=1\ntrack=%s\ndifficulty=%u\nframes=%lu\n"
        "average_frame_ticks_x100=%lu\nmax_frame_ticks=%lu\n"
        "p95_frame_ticks=%lu\n"
        "missed_frame_deadlines=%lu\nclock_corrections=%lu\n"
        "max_interpolation_lead_ms=%lu\nasset_bytes=%lu\n"
        "chart_bytes=%lu\nplugin_buffer_bytes=%lu\nfree_headroom=%lu\n"
        "package_load_ticks=%lu\nmax_input_judgement_ticks=%lu\n"
        "max_active_sprites=%lu\n",
        app->index.audio_path, (unsigned)app->chart.difficulty,
        (unsigned long)app->game.frame_count,
        (unsigned long)average_x100,
        (unsigned long)app->game.frame_ticks_max,
        (unsigned long)percentile_95,
        (unsigned long)app->game.missed_frame_deadlines,
        (unsigned long)app->clock.correction_count,
        (unsigned long)app->clock.max_interpolation_lead_ms,
        (unsigned long)app->skin.decoded_bytes,
        (unsigned long)chart_bytes,
        (unsigned long)app->arena.size,
        (unsigned long)(app->arena.size - app->arena.used),
        (unsigned long)app->load_ticks,
        (unsigned long)app->game.max_input_judgement_ticks,
        (unsigned long)app->game.max_active_sprites);
    rb->close(fd);
    rb->remove(IH_PERF_FILE);
    rb->rename(temporary, IH_PERF_FILE);
}

static void ih_game_boost(bool enable)
{
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(enable);
#else
    (void)enable;
#endif
}

static bool ih_usb_event(struct ih_app *app, long button)
{
    if (button == SYS_USB_CONNECTED ||
        rb->default_event_handler(button) == SYS_USB_CONNECTED)
    {
        app->usb = true;
        return true;
    }
    return false;
}

static void ih_menu_feedback(long button)
{
    /* Match Rockbox/iPod settings for software click, piezo and haptics. */
    rb->keyclick_click(true, button);
}

static bool ih_wait_menu(struct ih_app *app)
{
    while (true)
    {
        long button = rb->button_get(true);
        int clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        ih_menu_feedback(button);
        if (clean == BUTTON_MENU)
            return true;
        if (ih_usb_event(app, button))
            return false;
    }
}

static void ih_config_load(struct ih_app *app)
{
    char line[96];
    int fd = rb->open(IH_CONFIG_FILE, O_RDONLY);

    app->game.calibration_ms = 0;
    app->game.no_fail = false;
    app->game.hit_window_ms = IH_HIT_WINDOW_MS;
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *value;
        value = rb->strchr(line, '=');
        if (value == NULL)
            continue;
        *value++ = '\0';
        if (!rb->strcmp(line, "calibration_ms"))
        {
            int calibration = rb->atoi(value);
            if (calibration >= -300 && calibration <= 300)
                app->game.calibration_ms = calibration;
        }
        else if (!rb->strcmp(line, "no_fail"))
            app->game.no_fail = rb->atoi(value) != 0;
        else if (!rb->strcmp(line, "hit_window_ms"))
        {
            int window = rb->atoi(value);
            if (window >= IH_HIT_WINDOW_MS &&
                window <= IH_MAX_HIT_WINDOW_MS && window % 20 == 0)
                app->game.hit_window_ms = window;
        }
    }
    rb->close(fd);
}

static void ih_config_save(const struct ih_app *app)
{
    char temporary[MAX_PATH];
    int fd;

    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", IH_CONFIG_FILE);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "version=1\ncalibration_ms=%d\nno_fail=%d\n"
                 "hit_window_ms=%d\n",
                 app->game.calibration_ms, app->game.no_fail ? 1 : 0,
                 app->game.hit_window_ms);
    rb->close(fd);
    rb->remove(IH_CONFIG_FILE);
    rb->rename(temporary, IH_CONFIG_FILE);
}

static int ih_best_difficulty(const struct ih_index_entry *entry)
{
    int difficulty;
    for (difficulty = IH_EXPERT; difficulty >= IH_EASY; --difficulty)
        if (entry->chart_path[difficulty][0] != '\0')
            return difficulty;
    return -1;
}

static bool ih_load_difficulty(struct ih_app *app, int difficulty,
                               char *error, size_t error_size)
{
    if (difficulty < IH_EASY || difficulty > IH_EXPERT ||
        app->index.chart_path[difficulty][0] == '\0')
    {
        rb->snprintf(error, error_size, "Difficulty is not available");
        return false;
    }
    ih_arena_reset(&app->arena, app->chart_mark);
    if (!ih_chart_load(&app->chart, &app->arena,
                       app->index.chart_path[difficulty], error, error_size))
        return false;
    if (app->index.audio_length_ms > 0)
    {
        long difference = (long)app->index.audio_length_ms -
                          (long)app->chart.song_length_ms;
        if (difference < 0)
            difference = -difference;
        if (difference > 3000)
        {
            rb->snprintf(error, error_size,
                         "Chart duration does not match indexed audio");
            return false;
        }
    }
    app->selected_difficulty = difficulty;
    return ih_scores_load_best(app, error, error_size);
}

static bool ih_cycle_difficulty(struct ih_app *app, char *error,
                                size_t error_size)
{
    int offset;

    for (offset = 1; offset <= IH_DIFFICULTY_COUNT; ++offset)
    {
        int difficulty = (app->selected_difficulty + offset) %
                         IH_DIFFICULTY_COUNT;
        if (app->index.chart_path[difficulty][0] != '\0')
            return ih_load_difficulty(app, difficulty, error, error_size);
    }
    rb->snprintf(error, error_size, "Index has no playable difficulty");
    return false;
}

static bool ih_validate_current_track(struct ih_app *app,
                                      char *error, size_t error_size)
{
    struct mp3entry *id3 = rb->audio_current_track();

    if (id3 == NULL || !(rb->audio_status() & AUDIO_STATUS_PLAY) ||
        rb->strcmp(app->index.audio_path, id3->path))
    {
        rb->snprintf(error, error_size, "Selected song did not start");
        return false;
    }
    if (app->index.audio_length_ms > 0 && id3->length > 0)
    {
        long difference = (long)app->index.audio_length_ms - id3->length;
        if (difference < 0)
            difference = -difference;
        if (difference > 3000)
        {
            rb->snprintf(error, error_size,
                         "Chart audio duration does not match this track");
            return false;
        }
        if (app->index.audio_size > 0 && id3->filesize > 0 &&
            app->index.audio_size != (uint32_t)id3->filesize)
        {
            rb->snprintf(error, error_size,
                         "Chart audio size does not match this track");
            return false;
        }
    }
    return true;
}

static bool ih_load_package(struct ih_app *app,
                            const struct ih_index_entry *entry,
                            char *error, size_t error_size)
{
    struct mp3entry *id3 = rb->audio_current_track();
    int difficulty;

    ih_arena_reset(&app->arena, app->library_mark);
    app->index = *entry;
    ih_runtime_trace(app, "package-index-ready");
    difficulty = ih_best_difficulty(&app->index);
    if (difficulty < 0)
    {
        rb->snprintf(error, error_size, "Index has no playable difficulty");
        return false;
    }
    if (id3 != NULL && (rb->audio_status() & AUDIO_STATUS_PLAY) &&
        !rb->strcmp(app->index.audio_path, id3->path) &&
        !ih_validate_current_track(app, error, error_size))
        return false;
    ih_runtime_trace(app, "skin-load-begin");
    if (!ih_skin_load(&app->skin, &app->arena, app->index.skin_id,
                      error, error_size))
        return false;
    ih_runtime_trace(app, "skin-load-complete");
    app->chart_mark = ih_arena_mark(&app->arena);
    ih_runtime_trace(app, "chart-load-begin");
    if (!ih_load_difficulty(app, difficulty, error, error_size))
        return false;
    ih_runtime_trace(app, "chart-load-complete");
    if (app->arena.used > app->arena.size ||
        app->arena.size - app->arena.used < 384u * 1024u)
    {
        rb->snprintf(error, error_size,
                     "Insufficient gameplay memory headroom");
        return false;
    }
    return true;
}

static int ih_abs_int(int value)
{
    return value < 0 ? -value : value;
}

static bool ih_calibration_measure(struct ih_app *app, int original,
                                   int *proposed)
{
    char playback_error[96];
    int errors[8];
    int count = 0;
    uint32_t event_index = 0;
    bool no_fail = app->game.no_fail;
    int hit_window_ms = app->game.hit_window_ms;

    if (!ih_start_selected_song(app, playback_error,
                                sizeof(playback_error)))
    {
        ih_show_error("Song Playback", playback_error);
        ih_wait_menu(app);
        return false;
    }
    ih_clock_init(&app->clock, false);
    ih_game_reset(&app->game, &app->chart, original, no_fail,
                  hit_window_ms);
    rb->button_clear_queue();
    while (count < (int)ARRAYLEN(errors))
    {
        long elapsed = ih_clock_update(&app->clock);
        int song_time = (int)elapsed + app->chart.audio_offset_ms;
        long button;
        int clean;

        app->game.song_time_ms = song_time + original;
        ih_render_calibration_tap(app, count);
        button = rb->button_get_w_tmo(IH_FRAME_TICKS);
        if (button == BUTTON_NONE)
            continue;
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_MENU && !(button & BUTTON_REL))
        {
            rb->audio_pause();
            return false;
        }
        if (button == SYS_USB_CONNECTED)
        {
            app->usb = true;
            rb->audio_pause();
            return false;
        }
        if (clean == BUTTON_SELECT && !(button & (BUTTON_REPEAT | BUTTON_REL)))
        {
            int error;
            while (event_index < app->chart.event_count &&
                   (int)app->chart.events[event_index].time_ms <
                       song_time + original - 300)
                event_index++;
            if (event_index >= app->chart.event_count)
                break;
            error = song_time + original -
                    (int)app->chart.events[event_index].time_ms;
            if (ih_abs_int(error) <= 300)
            {
                errors[count++] = error;
                event_index++;
            }
        }
        else if (ih_usb_event(app, button))
        {
            rb->audio_pause();
            return false;
        }
    }
    rb->audio_pause();
    if (count != (int)ARRAYLEN(errors))
        return false;
    {
        int i;
        int correction;
        for (i = 1; i < count; ++i)
        {
            int value = errors[i];
            int position = i;
            while (position > 0 && errors[position - 1] > value)
            {
                errors[position] = errors[position - 1];
                position--;
            }
            errors[position] = value;
        }
        correction = -(errors[3] + errors[4]) / 2;
        if (correction > 150)
            correction = 150;
        if (correction < -150)
            correction = -150;
        *proposed = original + correction;
        if (*proposed > 300)
            *proposed = 300;
        if (*proposed < -300)
            *proposed = -300;
    }
    return true;
}

static bool ih_calibration_screen(struct ih_app *app, bool allow_tap)
{
    int original = app->game.calibration_ms;
    int proposed = original;

    rb->button_clear_queue();
    while (true)
    {
        long button;
        int clean;
        ih_render_calibration(app, proposed, allow_tap);
        button = rb->button_get(true);
        ih_menu_feedback(button);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if ((clean == BUTTON_SCROLL_FWD || clean == BUTTON_RIGHT) &&
            !(button & BUTTON_REL))
        {
            proposed += 5;
            if (proposed > 300)
                proposed = 300;
        }
        else if ((clean == BUTTON_SCROLL_BACK || clean == BUTTON_LEFT) &&
                 !(button & BUTTON_REL))
        {
            proposed -= 5;
            if (proposed < -300)
                proposed = -300;
        }
        else if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
        {
            app->game.calibration_ms = proposed;
            ih_config_save(app);
            return true;
        }
        else if (allow_tap && clean == BUTTON_PLAY &&
                 !(button & (BUTTON_REPEAT | BUTTON_REL)))
        {
            ih_calibration_measure(app, original, &proposed);
            if (app->usb)
                return false;
            rb->button_clear_queue();
        }
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL))
        {
            app->game.calibration_ms = original;
            return true;
        }
        else if (ih_usb_event(app, button))
            return false;
    }
}

static bool ih_settings_screen(struct ih_app *app)
{
    int selected = 0;

    rb->button_clear_queue();
    while (true)
    {
        long button;
        int clean;

        ih_render_settings(app, selected);
        button = rb->button_get(true);
        ih_menu_feedback(button);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL))
            selected = (selected + 1) % 4;
        else if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL))
            selected = (selected + 3) % 4;
        if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
        {
            if (selected == 0)
                app->game.no_fail = !app->game.no_fail;
            else if (selected == 1)
            {
                app->game.hit_window_ms += 20;
                if (app->game.hit_window_ms > IH_MAX_HIT_WINDOW_MS)
                    app->game.hit_window_ms = IH_HIT_WINDOW_MS;
            }
            else if (selected == 2)
            {
                char error[96];
                if (!ih_input_probe_run(app, error, sizeof(error)))
                {
                    if (app->usb)
                        return false;
                    ih_show_error("Input Test", error);
                    if (!ih_wait_menu(app))
                        return false;
                }
                rb->button_clear_queue();
            }
            else
                return true;
            if (selected < 2)
                ih_config_save(app);
        }
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL))
            return true;
        else if (ih_usb_event(app, button))
            return false;
    }
}

static void ih_practice_pause(struct ih_audio_clock *clock)
{
    clock->practice_pause_ms = clock->elapsed;
}

static void ih_practice_resume(struct ih_audio_clock *clock)
{
    clock->practice_start_tick = *rb->current_tick;
}

static bool ih_pause_playback(struct ih_app *app, bool practice)
{
    if (practice)
        ih_practice_pause(&app->clock);
    else if (!(rb->audio_status() & AUDIO_STATUS_PAUSE))
        rb->audio_pause();
    return true;
}

static void ih_resume_playback(struct ih_app *app, bool practice)
{
    if (practice)
        ih_practice_resume(&app->clock);
    else
    {
        rb->audio_resume();
        ih_clock_rebase(&app->clock);
    }
}

static enum ih_play_result ih_pause_menu(struct ih_app *app, bool practice)
{
    int selected = 0;

#ifdef SIMULATOR
    if (app->sim_test_mode == 3)
    {
        ih_render_pause(app, selected);
        rb->sleep(MAX(1, HZ / 10));
        app->sim_pause_count++;
        ih_resume_playback(app, practice);
        return IH_PLAY_RESUME;
    }
#endif
    rb->button_clear_queue();
    while (true)
    {
        long button;
        int clean;
        ih_render_pause(app, selected);
        button = rb->button_get(true);
        ih_menu_feedback(button);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL))
            selected = (selected + 1) % 5;
        else if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL))
            selected = (selected + 4) % 5;
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL))
        {
            ih_resume_playback(app, practice);
            return IH_PLAY_RESUME;
        }
        else if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
        {
            if (selected == 0)
            {
                ih_resume_playback(app, practice);
                return IH_PLAY_RESUME;
            }
            if (selected == 1)
                return IH_PLAY_RETRY;
            if (selected == 2)
            {
                if (!ih_calibration_screen(app, false))
                    return IH_PLAY_USB;
            }
            else if (selected == 3)
            {
                app->game.no_fail = !app->game.no_fail;
                ih_config_save(app);
            }
            else if (selected == 4)
                return IH_PLAY_MENU;
        }
        else if (ih_usb_event(app, button))
            return IH_PLAY_USB;
    }
}

static bool ih_start_selected_song(struct ih_app *app,
                                   char *error, size_t error_size)
{
    struct mp3entry *id3 = rb->audio_current_track();
    long deadline;

    ih_runtime_trace(app, "play-request");
    if (id3 == NULL || !(rb->audio_status() & AUDIO_STATUS_PLAY) ||
        rb->strcmp(app->index.audio_path, id3->path))
    {
        /* This is the user's explicit primary-media selection. Keep decoding
         * in Rockbox and create only the one-track session needed by play. */
        ih_runtime_trace(app, "before-playlist-create");
        if (rb->playlist_create(NULL, NULL) < 0)
        {
            rb->snprintf(error, error_size,
                         "Could not prepare selected song playback");
            return false;
        }
        ih_runtime_trace(app, "after-playlist-create");
        if (rb->playlist_insert_track(NULL, app->index.audio_path,
                                      PLAYLIST_INSERT_LAST,
                                      false, false) < 0)
        {
            rb->snprintf(error, error_size,
                         "Could not add the selected song to playback");
            return false;
        }
        ih_runtime_trace(app, "after-playlist-insert");
        rb->playlist_sync(NULL);
        ih_runtime_trace(app, "after-playlist-sync");
        rb->playlist_start(0, 0, 0);
        ih_runtime_trace(app, "after-playlist-start");
        deadline = *rb->current_tick + HZ * 5;
        while (TIME_BEFORE(*rb->current_tick, deadline))
        {
            id3 = rb->audio_current_track();
            if (id3 != NULL && (rb->audio_status() & AUDIO_STATUS_PLAY) &&
                !rb->strcmp(app->index.audio_path, id3->path))
                break;
            rb->sleep(1);
        }
        ih_runtime_trace(app, "decoder-ready");
    }
    if (!ih_validate_current_track(app, error, error_size))
        return false;
    ih_runtime_trace(app, "identity-valid");
    if (!ih_seek_song_start())
    {
        rb->snprintf(error, error_size,
                     "The selected song did not return to its start");
        return false;
    }
    ih_runtime_trace(app, "seek-complete");
    return true;
}

static bool ih_seek_song_start(void)
{
    long deadline;

    if (!(rb->audio_status() & AUDIO_STATUS_PLAY))
        return false;
    if (!(rb->audio_status() & AUDIO_STATUS_PAUSE))
        rb->audio_pause();
    rb->audio_pre_ff_rewind();
    rb->audio_ff_rewind(0);
    rb->audio_resume();
    deadline = *rb->current_tick + HZ * 2;
    while (TIME_BEFORE(*rb->current_tick, deadline))
    {
        struct mp3entry *id3 = rb->audio_current_track();
        if (id3 != NULL && id3->elapsed <= 500)
            return true;
        rb->sleep(1);
    }
    rb->audio_pause();
    return false;
}

static enum ih_play_result ih_results(struct ih_app *app)
{
    int selected = 0;
    char error[96];

    ih_performance_save(app);
#ifdef SIMULATOR
    ih_sim_test_save(app);
#endif
    if (!ih_scores_save(app, error, sizeof(error)))
    {
        ih_show_error("Score Not Saved", error);
        if (!ih_wait_menu(app))
            return IH_PLAY_USB;
    }
#ifdef SIMULATOR
    if (app->sim_test_mode == 3 && app->sim_runs < 20)
        return IH_PLAY_RETRY;
#endif
    rb->button_clear_queue();
    while (true)
    {
        long button;
        int clean;
        ih_render_results(app, selected);
        button = rb->button_get(true);
        ih_menu_feedback(button);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if ((clean == BUTTON_SCROLL_FWD || clean == BUTTON_SCROLL_BACK) &&
            !(button & BUTTON_REL))
            selected = 1 - selected;
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL))
            return IH_PLAY_MENU;
        else if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
            return selected == 0 ? IH_PLAY_RETRY : IH_PLAY_MENU;
        else if (ih_usb_event(app, button))
            return IH_PLAY_USB;
    }
}

static enum ih_play_result ih_play(struct ih_app *app, bool practice)
{
    char playback_error[96];
    int calibration = app->game.calibration_ms;
    bool no_fail = app->game.no_fail;
    int hit_window_ms = app->game.hit_window_ms;

    if (!practice &&
        !ih_start_selected_song(app, playback_error,
                                sizeof(playback_error)))
    {
        ih_show_error("Song Playback", playback_error);
        ih_wait_menu(app);
        return IH_PLAY_MENU;
    }
    ih_clock_init(&app->clock, practice);
    ih_game_reset(&app->game, &app->chart, calibration, no_fail,
                  hit_window_ms);
    app->last_practice = practice;
    ih_input_reset(&app->input);
    rb->button_clear_queue();
#ifdef SIMULATOR
    if (app->sim_test_mode == 3)
    {
        app->sim_runs++;
        app->sim_pause_done = false;
    }
#endif

    ih_game_boost(true);
    while (!app->game.finished && !app->game.failed)
    {
        long frame_start = *rb->current_tick;
        long button;
        long elapsed = ih_clock_update(&app->clock);
        struct ih_input_event input;

        app->game.track_changed = ih_clock_track_changed(&app->clock);
        if (app->game.track_changed)
        {
            ih_pause_playback(app, practice);
            break;
        }
        ih_game_update(&app->game,
                       (int)elapsed + app->chart.audio_offset_ms +
                       app->game.calibration_ms);
#ifdef SIMULATOR
        if (app->sim_test_mode != 0)
            ih_sim_autoplay_update(app);
        if (app->sim_test_mode == 3 && !app->sim_pause_done &&
            app->game.song_time_ms >= 1000)
        {
            input = ih_input_normalize(&app->input,
                                       BUTTON_SELECT | BUTTON_MENU);
            if (input.kind == IH_INPUT_PAUSE)
            {
                enum ih_play_result paused;
                app->sim_pause_done = true;
                app->sim_scripted_pause_count++;
                ih_pause_playback(app, practice);
                paused = ih_pause_menu(app, practice);
                if (paused != IH_PLAY_RESUME)
                {
                    ih_game_boost(false);
                    return paused;
                }
                ih_input_reset(&app->input);
                continue;
            }
        }
#endif
        {
            uint32_t sprites = ih_render_active_sprites(&app->game);
            if (sprites > app->game.max_active_sprites)
                app->game.max_active_sprites = sprites;
        }
        ih_render_game(app);
        {
            uint32_t frame_ticks = (uint32_t)(*rb->current_tick -
                                              frame_start);
            app->game.frame_count++;
            app->game.frame_ticks_total += frame_ticks;
            if (frame_ticks > app->game.frame_ticks_max)
                app->game.frame_ticks_max = frame_ticks;
            app->game.frame_tick_histogram[
                MIN(frame_ticks,
                    (uint32_t)ARRAYLEN(app->game.frame_tick_histogram) - 1)]++;
            if (frame_ticks > (uint32_t)IH_FRAME_TICKS)
                app->game.missed_frame_deadlines++;
        }
        button = rb->button_get_w_tmo(IH_FRAME_TICKS);

#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold())
        {
            enum ih_play_result paused;
            ih_pause_playback(app, practice);
            while (rb->button_hold())
            {
                rb->sleep(HZ / 20);
                if (rb->button_get(false) == SYS_USB_CONNECTED)
                {
                    app->usb = true;
                    break;
                }
            }
            if (app->usb)
            {
                ih_game_boost(false);
                return IH_PLAY_USB;
            }
            paused = ih_pause_menu(app, practice);
            if (paused != IH_PLAY_RESUME)
            {
                ih_game_boost(false);
                return paused;
            }
            ih_input_reset(&app->input);
            continue;
        }
#endif
        if (button == BUTTON_NONE)
            continue;
        if (button == SYS_USB_CONNECTED)
        {
            app->usb = true;
            ih_game_boost(false);
            return IH_PLAY_USB;
        }
        {
            long input_start = *rb->current_tick;
            input = ih_input_normalize(&app->input, button);
            if (input.kind == IH_INPUT_PAUSE)
            {
                enum ih_play_result paused;
                ih_pause_playback(app, practice);
                paused = ih_pause_menu(app, practice);
                if (paused != IH_PLAY_RESUME)
                {
                    ih_game_boost(false);
                    return paused;
                }
                ih_input_reset(&app->input);
                continue;
            }
            if (input.kind == IH_INPUT_WHEEL || input.kind == IH_INPUT_STAR)
            {
                ih_game_whammy(&app->game);
                if (input.kind == IH_INPUT_STAR)
                    ih_game_activate_star(&app->game);
                continue;
            }
            if (input.kind == IH_INPUT_PRESS)
                ih_game_press(&app->game, input.lane_mask);
            else if (input.kind == IH_INPUT_RELEASE)
                ih_game_release(&app->game, input.lane_mask);
            else if (ih_usb_event(app, button))
            {
                ih_game_boost(false);
                return IH_PLAY_USB;
            }
            if (input.kind == IH_INPUT_PRESS &&
                (uint32_t)(*rb->current_tick - input_start) >
                    app->game.max_input_judgement_ticks)
                app->game.max_input_judgement_ticks =
                    (uint32_t)(*rb->current_tick - input_start);
        }
    }
    ih_game_boost(false);
    if (app->game.failed && !practice &&
        (rb->audio_status() & AUDIO_STATUS_PLAY) &&
        !(rb->audio_status() & AUDIO_STATUS_PAUSE))
        rb->audio_pause();
    if (app->game.track_changed)
    {
        ih_show_error("Track Changed",
                      "Playback changed during the chart");
        ih_wait_menu(app);
        return app->usb ? IH_PLAY_USB : IH_PLAY_MENU;
    }
    return ih_results(app);
}

static bool ih_main_menu(struct ih_app *app)
{
    int selected = 0;
    const char *message = app->index.title;

    rb->button_clear_queue();
    while (!app->usb)
    {
        long button;
        int clean;
        ih_render_menu(app, selected, message);
        button = rb->button_get(true);
        ih_menu_feedback(button);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL))
            selected = (selected + 1) % 6;
        else if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL))
            selected = (selected + 5) % 6;
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL))
            return true;
        else if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
        {
            if (selected == 0 || selected == 1)
            {
                enum ih_play_result result;
                do
                {
                    result = ih_play(app, selected == 1);
                }
                while (result == IH_PLAY_RETRY && !app->usb);
                if (result == IH_PLAY_USB)
                    return false;
            }
            else if (selected == 2)
            {
                char error[96];
                if (!ih_cycle_difficulty(app, error, sizeof(error)))
                {
                    ih_show_error("Difficulty", error);
                    if (!ih_wait_menu(app))
                        return false;
                }
            }
            else if (selected == 3)
            {
                if (!ih_calibration_screen(app, true))
                    return false;
            }
            else if (selected == 4)
            {
                if (!ih_settings_screen(app))
                    return false;
            }
            else
                return true;
        }
        else if (ih_usb_event(app, button))
            return false;
    }
    return false;
}

static int ih_library_find_path(const struct ih_song_library *library,
                                const char *path)
{
    int index;

    if (path == NULL)
        return -1;
    for (index = 0; index < library->count; index++)
        if (!rb->strcmp(library->entries[index].audio_path, path))
            return index;
    return -1;
}

static bool ih_song_library_menu(struct ih_app *app)
{
    struct mp3entry *id3 = rb->audio_current_track();
    int selected = 0;

    if (id3 != NULL)
    {
        int current = ih_library_find_path(&app->library, id3->path);
        if (current >= 0)
            selected = current;
    }
    app->library.selected = selected;
#ifdef SIMULATOR
    ih_sim_library_save(&app->library);
#endif
    rb->button_clear_queue();
    while (!app->usb)
    {
        long button;
        int clean;

        app->library.selected = selected;
        ih_render_song_library(&app->library, selected);
        button = rb->button_get(true);
        ih_menu_feedback(button);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL))
            selected = (selected + 1) % app->library.count;
        else if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL))
            selected = (selected + app->library.count - 1) %
                       app->library.count;
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL))
            return true;
        else if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
        {
            char error[MAX_PATH + 64];
            long load_start = *rb->current_tick;

            app->index = app->library.entries[selected];
            ih_runtime_trace(app, "song-selected");
            if (!ih_load_package(app, &app->library.entries[selected],
                                 error, sizeof(error)))
            {
                ih_show_error("Cannot Open Song", error);
                if (!ih_wait_menu(app))
                    return false;
            }
            else
            {
                ih_runtime_trace(app, "package-loaded");
                app->load_ticks =
                    (uint32_t)(*rb->current_tick - load_start);
                if (!ih_main_menu(app))
                    return false;
            }
            rb->button_clear_queue();
        }
        else if (ih_usb_event(app, button))
            return false;
    }
    return false;
}

enum plugin_status plugin_start(const void *parameter)
{
    /* Native targets run plugins on the firmware main thread's bounded
     * stack.  Keep the persistent game state in the plugin BSS so nested
     * bitmap decoding cannot exhaust that stack. */
    static struct ih_app app;
    char error[MAX_PATH + 64];
    const char *load_parameter = parameter;
    bool auto_practice = false;
    bool library_loaded;
    bool package_loaded = false;
    bool direct_load;
    long load_start;
#ifdef SIMULATOR
    bool auto_input_test = false;
    bool auto_play = false;
#endif

    rb->memset(&app, 0, sizeof(app));
    if (load_parameter != NULL &&
        !rb->strncmp(load_parameter, "practice:", 9))
    {
        load_parameter += 9;
        auto_practice = true;
    }
#ifdef SIMULATOR
    else if (load_parameter != NULL &&
             !rb->strncmp(load_parameter, "autotest-perfect:", 17))
    {
        load_parameter += 17;
        auto_practice = true;
        app.sim_test_mode = 1;
    }
    else if (load_parameter != NULL &&
             !rb->strncmp(load_parameter, "autotest-mixed:", 15))
    {
        load_parameter += 15;
        auto_practice = true;
        app.sim_test_mode = 2;
    }
    else if (load_parameter != NULL &&
             !rb->strncmp(load_parameter, "autotest-lifecycle:", 19))
    {
        load_parameter += 19;
        auto_practice = true;
        app.sim_test_mode = 3;
    }
    else if (load_parameter != NULL &&
             !rb->strncmp(load_parameter, "autotest-input:", 15))
    {
        load_parameter += 15;
        app.sim_test_mode = 4;
        auto_input_test = true;
    }
    else if (load_parameter != NULL &&
             !rb->strncmp(load_parameter, "autotest-play:", 14))
    {
        load_parameter += 14;
        app.sim_test_mode = 1;
        auto_play = true;
    }
#endif
    app.arena.base = rb->plugin_get_buffer(&app.arena.size);
    if (app.arena.base == NULL || app.arena.size < 1024u * 1024u)
    {
        rb->splash(HZ * 3, "iPod Hero: insufficient plugin memory");
        return PLUGIN_ERROR;
    }

    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    backlight_ignore_timeout();
    ih_runtime_reset();
    ih_runtime_trace(&app, "plugin-start");
    ih_config_load(&app);
    load_start = *rb->current_tick;
    library_loaded = ih_index_load_library(&app.library, &app.arena,
                                           error, sizeof(error));
    if (library_loaded)
        ih_runtime_trace(&app, "library-loaded");
    app.library_mark = ih_arena_mark(&app.arena);
    direct_load = library_loaded && load_parameter != NULL &&
                  load_parameter[0] == '/';
    if (direct_load)
    {
        int selected = ih_library_find_path(&app.library, load_parameter);
        if (selected < 0)
            rb->snprintf(error, sizeof(error),
                         "Selected song is not in the installed library");
        else
        {
            app.library.selected = selected;
            package_loaded = ih_load_package(
                &app, &app.library.entries[selected], error, sizeof(error));
        }
    }
    app.load_ticks = (uint32_t)(*rb->current_tick - load_start);
    if (!library_loaded || (direct_load && !package_loaded))
    {
        ih_show_error("iPod Hero", error);
        ih_wait_menu(&app);
    }
#ifdef SIMULATOR
    else if (auto_input_test)
    {
        char probe_error[96];
        if (!ih_input_probe_run(&app, probe_error, sizeof(probe_error)) &&
            !app.usb)
        {
            ih_show_error("Input Test", probe_error);
            ih_wait_menu(&app);
        }
    }
    else if (auto_play)
    {
        enum ih_play_result result;
        app.game.no_fail = true;
        do
        {
            result = ih_play(&app, false);
        }
        while (result == IH_PLAY_RETRY && !app.usb);
    }
#endif
    else if (auto_practice)
    {
        enum ih_play_result result;
        app.game.no_fail = true;
        do
        {
            result = ih_play(&app, true);
        }
        while (result == IH_PLAY_RETRY && !app.usb);
    }
    else if (direct_load)
        ih_main_menu(&app);
    else
        ih_song_library_menu(&app);

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
    ih_config_save(&app);
    backlight_use_settings();
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_update();
    return app.usb ? PLUGIN_USB_CONNECTED : PLUGIN_OK;
}
