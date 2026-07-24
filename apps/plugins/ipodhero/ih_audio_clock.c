/***************************************************************************
 * iPod Hero playback clock observer
 ****************************************************************************/

#include "ipodhero.h"

void ih_clock_init(struct ih_audio_clock *clock, bool practice)
{
    rb->memset(clock, 0, sizeof(*clock));
    clock->practice = practice;
    clock->practice_start_tick = *rb->current_tick;
    clock->id3 = rb->audio_current_track();
    clock->status = rb->audio_status();
    if (clock->id3 != NULL)
    {
        rb->strlcpy(clock->path, clock->id3->path, sizeof(clock->path));
        clock->last_sync_elapsed = clock->id3->elapsed;
        clock->elapsed = clock->id3->elapsed;
    }
    clock->last_sync_tick = *rb->current_tick;
}

void ih_clock_rebase(struct ih_audio_clock *clock)
{
    clock->id3 = rb->audio_current_track();
    clock->status = rb->audio_status();
    clock->last_sync_tick = *rb->current_tick;
    if (clock->practice)
    {
        clock->practice_start_tick = *rb->current_tick;
        clock->practice_pause_ms = 0;
    }
    else if (clock->id3 != NULL)
    {
        clock->last_sync_elapsed = clock->id3->elapsed;
        clock->elapsed = clock->id3->elapsed;
    }
}

long ih_clock_update(struct ih_audio_clock *clock)
{
    long now = *rb->current_tick;
    long di;

    if (clock->practice)
    {
        clock->elapsed = clock->practice_pause_ms +
            (now - clock->practice_start_tick) * 1000 / HZ;
        return clock->elapsed;
    }

    clock->status = rb->audio_status();
    clock->id3 = rb->audio_current_track();
    if (clock->id3 == NULL ||
        !(clock->status & AUDIO_STATUS_PLAY))
        return clock->elapsed;

    di = clock->id3->elapsed - clock->last_sync_elapsed;
    if (di < -50 || di > 0)
    {
        if (di < -50 || di > 50)
            clock->correction_count++;
        clock->last_sync_elapsed = clock->id3->elapsed;
        clock->last_sync_tick = now;
        clock->elapsed = clock->id3->elapsed;
    }
    else if (!(clock->status & AUDIO_STATUS_PAUSE))
    {
        long interpolated = clock->last_sync_elapsed +
            (now - clock->last_sync_tick) * 1000 / HZ;
        if (interpolated > (long)clock->id3->elapsed + 200)
            interpolated = (long)clock->id3->elapsed + 200;
        if (interpolated > (long)clock->id3->elapsed &&
            (uint32_t)(interpolated - clock->id3->elapsed) >
                clock->max_interpolation_lead_ms)
            clock->max_interpolation_lead_ms =
                (uint32_t)(interpolated - clock->id3->elapsed);
        clock->elapsed = interpolated;
    }
    else
        clock->elapsed = clock->id3->elapsed;

    if (clock->elapsed < 0)
        clock->elapsed = 0;
    return clock->elapsed;
}

bool ih_clock_track_changed(const struct ih_audio_clock *clock)
{
    struct mp3entry *id3;

    if (clock->practice)
        return false;
    id3 = rb->audio_current_track();
    return id3 == NULL || rb->strcmp(clock->path, id3->path) != 0;
}
