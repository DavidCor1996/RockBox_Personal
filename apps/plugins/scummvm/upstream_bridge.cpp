/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "upstream_bridge.h"
#include "queen_loader.h"
#include "sky_loader.h"
#include "sky_runtime.h"

struct SkyIntroStage {
    uint16_t screen_file;
    uint16_t palette_file;
    uint16_t sequence_file;
    unsigned int frames;
};

static const SkyIntroStage sky_intro_stages[] = {
    { 60110, 60111, 0, 90 },
    { 60112, 60113, 0, 240 },
    { 60114, 60115, 0, 60 },
    { 60081, 60080, 60082, 0 },
    { 0, 0, 60083, 0 },
    { 0, 0, 60084, 0 },
    { 0, 0, 60085, 0 },
    { 0, 0, 60086, 0 },
};

class RockboxScummEngine {
public:
    RockboxScummEngine()
        : _target(0), _frame(0), _sky_intro_stage(0)
    {
        _sky_sequence_tick = 0;
        _sky_runtime_tick = 0;
        _sky_bootstrapped = false;
    }

    void setTarget(const scummvm_target *target)
    {
        _target = target;
        _frame = 0;
        _sky_intro_stage = 0;
        _sky_sequence_tick = 0;
        _sky_runtime_tick = 0;
        _sky_bootstrapped = false;
    }

    bool init(scummvm_engine_state *state)
    {
        if (!rb->strcasecmp(_target->engine, "sky")) {
            if (!loadSkyIntroStage(state))
                return false;
            return true;
        }

        if (!rb->strcasecmp(_target->engine, "queen")) {
            if (!scummvm_queen_loader_probe(_target, state->status,
                                            sizeof(state->status)))
                return false;
            return true;
        }

        rb->snprintf(state->status, sizeof(state->status),
                     "C++ bridge ready for %s", _target->engine);
        return true;
    }

    bool drawFrame(scummvm_engine_state *state, scummvm_video *video)
    {
        int x;
        int y;

        if (_target && !rb->strcasecmp(_target->engine, "sky")) {
            bool ok = _sky_bootstrapped ?
                scummvm_sky_runtime_render(video) :
                scummvm_sky_loader_render_current(video);
            advanceSkyIntro(state);
            return ok;
        }

        for (y = 0; y < video->height; y++) {
            for (x = 0; x < video->width; x++) {
                int r = (x + _frame) & 0xff;
                int g = (y * 2) & 0xff;
                int b = ((x / 8 + y / 8 + _frame / 8) & 1) ? 96 : 32;
                video->pixels[y * video->width + x] = LCD_RGBPACK(r, g, b);
            }
        }

        _frame++;
        return true;
    }

private:
    bool loadSkyIntroStage(scummvm_engine_state *state)
    {
        const SkyIntroStage *stage = &sky_intro_stages[_sky_intro_stage];

        if (stage->screen_file) {
            if (!scummvm_sky_loader_load_screen(_target, stage->screen_file,
                                                stage->palette_file,
                                                state->status,
                                                sizeof(state->status)))
                return false;
        }

        if (stage->sequence_file) {
            if (!scummvm_sky_loader_load_sequence(_target,
                                                  stage->sequence_file,
                                                  state->status,
                                                  sizeof(state->status)))
                return false;
        }

        rb->snprintf(state->status, sizeof(state->status),
                     "Sky intro stage %u/%u",
                     (unsigned)(_sky_intro_stage + 1),
                     (unsigned)ARRAYLEN(sky_intro_stages));
        return true;
    }

    void advanceSkyIntro(scummvm_engine_state *state)
    {
        char status[96];
        const SkyIntroStage *stage = &sky_intro_stages[_sky_intro_stage];

        if (stage->sequence_file) {
            _sky_sequence_tick++;
            if (_sky_sequence_tick >= 2) {
                _sky_sequence_tick = 0;
                scummvm_sky_loader_step_sequence();
            }

            if (scummvm_sky_loader_sequence_running())
                return;
        } else {
            _frame++;
            if (_frame < stage->frames)
                return;
        }

        if (_sky_intro_stage + 1 >= ARRAYLEN(sky_intro_stages)) {
            if (!_sky_bootstrapped &&
                scummvm_sky_loader_bootstrap_section0(_target,
                                                      state->status,
                                                      sizeof(state->status)) &&
                scummvm_sky_runtime_init(_target,
                                         state->status,
                                         sizeof(state->status)))
                _sky_bootstrapped = true;
            else if (_sky_bootstrapped) {
                _sky_runtime_tick++;
                if (_sky_runtime_tick >= 30) {
                    _sky_runtime_tick = 0;
                    scummvm_sky_runtime_step(state->status,
                                             sizeof(state->status));
                }
            }
            return;
        }

        _frame = 0;
        _sky_sequence_tick = 0;
        _sky_intro_stage++;
        if (!startSkyIntroStage(status, sizeof(status)))
            _sky_intro_stage--;
        else
            rb->strlcpy(state->status, status, sizeof(state->status));
    }

    bool startSkyIntroStage(char *status, size_t status_size)
    {
        scummvm_engine_state state;

        rb->memset(&state, 0, sizeof(state));
        if (!loadSkyIntroStage(&state)) {
            rb->strlcpy(status, state.status, status_size);
            return false;
        }

        rb->strlcpy(status, state.status, status_size);
        return true;
    }

    const scummvm_target *_target;
    unsigned int _frame;
    unsigned int _sky_intro_stage;
    unsigned int _sky_sequence_tick;
    unsigned int _sky_runtime_tick;
    bool _sky_bootstrapped;
};

static RockboxScummEngine active_engine;
static bool active_engine_ready;

bool scummvm_upstream_can_run(const struct scummvm_target *target)
{
    return !rb->strcasecmp(target->engine, "sky") ||
           !rb->strcasecmp(target->engine, "queen");
}

bool scummvm_upstream_init(const struct scummvm_target *target,
                           struct scummvm_engine_state *state)
{
    active_engine.setTarget(target);
    active_engine_ready = active_engine.init(state);
    return active_engine_ready;
}

bool scummvm_upstream_frame(const struct scummvm_target *target,
                            struct scummvm_engine_state *state,
                            struct scummvm_video *video)
{
    (void)target;
    (void)state;

    if (!active_engine_ready)
        return false;

    return active_engine.drawFrame(state, video);
}
