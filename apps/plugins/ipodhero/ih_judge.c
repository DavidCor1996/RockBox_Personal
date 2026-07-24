/***************************************************************************
 * iPod Hero deterministic note judgement and scoring
 ****************************************************************************/

#include "ipodhero.h"

static int ih_abs(int value)
{
    return value < 0 ? -value : value;
}

int ih_score_multiplier(const struct ih_score *score)
{
    int multiplier;

    if (score->streak >= 30)
        multiplier = 4;
    else if (score->streak >= 20)
        multiplier = 3;
    else if (score->streak >= 10)
        multiplier = 2;
    else
        multiplier = 1;
    return score->star_active ? multiplier * 2 : multiplier;
}

static void ih_register_miss(struct ih_game *game,
                             struct ih_note_event *event)
{
    if (event->state != IH_EVENT_WAITING)
        return;
    event->state = IH_EVENT_MISSED;
    if (event->flags & IH_NOTE_STAR)
    {
        if (game->score.phrase_id != event->phrase_id)
        {
            game->score.phrase_id = event->phrase_id;
            game->score.phrase_failed = false;
        }
        game->score.phrase_failed = true;
    }
    game->score.miss_count++;
    game->score.streak = 0;
    game->score.rock -= 8;
    if (game->score.rock < 0)
        game->score.rock = 0;
    game->score.last_judgement = IH_JUDGE_MISS;
    game->score.feedback_until = *rb->current_tick + HZ / 2;
}

static void ih_register_hit(struct ih_game *game,
                            struct ih_note_event *event, int error)
{
    enum ih_judgement judgement;
    int value;
    int absolute = ih_abs(error);
    int multiplier = ih_score_multiplier(&game->score);
    int perfect = 45 * game->hit_window_ms / IH_HIT_WINDOW_MS;
    int great = 90 * game->hit_window_ms / IH_HIT_WINDOW_MS;
    int good = 135 * game->hit_window_ms / IH_HIT_WINDOW_MS;

    if (absolute <= perfect)
    {
        judgement = IH_JUDGE_PERFECT;
        value = 100;
        game->score.perfect_count++;
    }
    else if (absolute <= great)
    {
        judgement = IH_JUDGE_GREAT;
        value = 75;
        game->score.great_count++;
    }
    else if (absolute <= good)
    {
        judgement = IH_JUDGE_GOOD;
        value = 50;
        game->score.good_count++;
    }
    else
    {
        judgement = IH_JUDGE_GRACE;
        value = 25;
        game->score.grace_count++;
    }

    event->state = IH_EVENT_HIT;
    if (event->flags & IH_NOTE_STAR)
    {
        if (game->score.phrase_id != event->phrase_id)
        {
            game->score.phrase_id = event->phrase_id;
            game->score.phrase_failed = false;
        }
        if ((event->flags & IH_NOTE_PHRASE_END) &&
            !game->score.phrase_failed)
        {
            game->score.star_ms += 2500;
            if (game->score.star_ms > 10000)
                game->score.star_ms = 10000;
        }
        if (event->flags & IH_NOTE_PHRASE_END)
        {
            game->score.phrase_id = 0;
            game->score.phrase_failed = false;
        }
    }
    game->score.points += (uint32_t)(value * multiplier);
    game->score.hit_count++;
    game->score.streak++;
    if (game->score.streak > game->score.max_streak)
        game->score.max_streak = game->score.streak;
    game->score.rock += 2;
    if (game->score.rock > 100)
        game->score.rock = 100;
    game->score.last_error_ms = error;
    game->score.last_judgement = judgement;
    game->score.last_hit_mask = event->lane_mask;
    game->score.feedback_until = *rb->current_tick + HZ / 2;
}

void ih_game_reset(struct ih_game *game, const struct ih_chart *chart,
                   int calibration_ms, bool no_fail, int hit_window_ms)
{
    uint32_t i;

    rb->memset(game, 0, sizeof(*game));
    game->chart = *chart;
    game->calibration_ms = calibration_ms;
    game->no_fail = no_fail;
    game->hit_window_ms = hit_window_ms;
    game->assist = hit_window_ms != IH_HIT_WINDOW_MS;
    game->score.rock = 50;
    game->last_update_ms = -1;
    for (i = 0; i < game->chart.event_count; ++i)
    {
        game->chart.events[i].state = IH_EVENT_WAITING;
        game->chart.events[i].collected = 0;
        game->chart.events[i].flags &= ~IH_NOTE_SUSTAIN_BROKEN;
    }
}

void ih_game_press(struct ih_game *game, uint8_t lane_mask)
{
    uint32_t i;

    game->held_mask |= lane_mask;
    for (i = game->judge_cursor; i < game->chart.event_count; ++i)
    {
        struct ih_note_event *event = &game->chart.events[i];
        int error = game->song_time_ms - (int)event->time_ms;

        if (event->state != IH_EVENT_WAITING)
            continue;
        if (error < -game->hit_window_ms)
            break;
        if (error > game->hit_window_ms)
            continue;
        if (lane_mask & ~event->lane_mask)
        {
            ih_register_miss(game, event);
            return;
        }
        event->collected |= lane_mask;
        if (event->collected == event->lane_mask)
            ih_register_hit(game, event, error);
        return;
    }
}

void ih_game_release(struct ih_game *game, uint8_t lane_mask)
{
    game->held_mask &= ~lane_mask;
}

void ih_game_activate_star(struct ih_game *game)
{
    if (!game->score.star_active && game->score.star_ms >= 5000)
        game->score.star_active = true;
}

void ih_game_whammy(struct ih_game *game)
{
    uint32_t begin = game->judge_cursor > 64 ? game->judge_cursor - 64 : 0;
    uint32_t i;

    for (i = begin; i < game->chart.event_count; ++i)
    {
        struct ih_note_event *event = &game->chart.events[i];
        uint32_t end;

        if ((int)event->time_ms > game->song_time_ms)
            break;
        if (event->state != IH_EVENT_HIT || event->duration_ms == 0 ||
            !(event->flags & IH_NOTE_STAR) ||
            (event->flags & IH_NOTE_SUSTAIN_BROKEN))
            continue;
        end = event->time_ms + event->duration_ms;
        if ((uint32_t)game->song_time_ms < end &&
            (game->held_mask & event->lane_mask) == event->lane_mask)
        {
            game->score.star_ms += 25;
            if (game->score.star_ms > 10000)
                game->score.star_ms = 10000;
            return;
        }
    }
}

static void ih_update_sustains(struct ih_game *game, int delta_ms)
{
    uint32_t begin = game->judge_cursor > 64 ? game->judge_cursor - 64 : 0;
    uint32_t i;

    if (delta_ms <= 0 || delta_ms > 250)
        return;
    for (i = begin; i < game->chart.event_count; ++i)
    {
        struct ih_note_event *event = &game->chart.events[i];
        int end;
        if ((int)event->time_ms > game->song_time_ms)
            break;
        if (event->state != IH_EVENT_HIT || event->duration_ms == 0 ||
            (event->flags & IH_NOTE_SUSTAIN_BROKEN))
            continue;
        end = (int)(event->time_ms + event->duration_ms);
        if (game->song_time_ms >= end)
            continue;
        if ((game->held_mask & event->lane_mask) == event->lane_mask)
        {
            game->score.sustain_ms += (uint32_t)delta_ms;
            game->score.points +=
                (uint32_t)(delta_ms * ih_score_multiplier(&game->score) / 20);
        }
        else if (game->song_time_ms < end - IH_SUSTAIN_GRACE_MS)
        {
            event->flags |= IH_NOTE_SUSTAIN_BROKEN;
            game->score.rock -= 3;
            if (game->score.rock < 0)
                game->score.rock = 0;
        }
    }
}

void ih_game_update(struct ih_game *game, int song_time_ms)
{
    int delta = game->last_update_ms < 0 ? 0 :
                song_time_ms - game->last_update_ms;

    game->song_time_ms = song_time_ms;
    while (game->section_cursor + 1 < game->chart.section_count &&
           (int)game->chart.sections[game->section_cursor + 1].time_ms <=
               song_time_ms)
        game->section_cursor++;
    while (game->judge_cursor < game->chart.event_count)
    {
        struct ih_note_event *event =
            &game->chart.events[game->judge_cursor];
        if (event->state == IH_EVENT_WAITING &&
            song_time_ms <= (int)event->time_ms + game->hit_window_ms)
            break;
        if (event->state == IH_EVENT_WAITING)
            ih_register_miss(game, event);
        game->judge_cursor++;
    }

    ih_update_sustains(game, delta);
    if (game->score.star_active && delta > 0)
    {
        if ((uint32_t)delta >= game->score.star_ms)
        {
            game->score.star_ms = 0;
            game->score.star_active = false;
        }
        else
            game->score.star_ms -= (uint32_t)delta;
    }
    game->last_update_ms = song_time_ms;
    if (!game->no_fail && game->score.rock <= 0)
        game->failed = true;
    if (song_time_ms >= (int)game->chart.song_length_ms)
        game->finished = true;
}
