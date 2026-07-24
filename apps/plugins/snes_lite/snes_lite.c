/***************************************************************************
 * Experimental personal-fork-only SNES Lite plugin.
 ***************************************************************************/

#include "plugin.h"
#include "snes_lite.h"
#include "lib/rockachievements.h"
#include <stdarg.h>
#ifdef SIMULATOR
#include <stdlib.h>
#endif

struct snes_lite_runtime snes_lite;
static struct rockachievements_runtime snes_achievements;

static int log_fd = -1;

void snes_lite_log(const char *format, ...)
{
    va_list ap;
    char line[256];

    if (log_fd < 0)
        return;
    va_start(ap, format);
    rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    rb->fdprintf(log_fd, "%s\n", line);
}

static void open_log(void)
{
    if (!rb->dir_exists(ROCKBOX_DIR "/logs"))
        rb->mkdir(ROCKBOX_DIR "/logs");
    log_fd = rb->open(SNES_LITE_LOG_PATH,
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
}

static void show_startup_diagnostics(void)
{
    char line[96];

    rb->snprintf(line, sizeof(line), "ROM %lu KB, free %lu KB",
                 (unsigned long)(snes_lite.rom.size / 1024),
                 (unsigned long)((snes_lite.arena_size -
                                  snes_lite.arena_used) / 1024));
    rb->splashf(HZ, "%s", line);
}

static void update_fps(void)
{
    long now = *rb->current_tick;

    snes_lite.frame_count++;
    snes_lite.profile_frames++;
    if (now - snes_lite.fps_tick >= HZ)
    {
        unsigned long elapsed = now - snes_lite.fps_tick;
        unsigned long rendered = snes_lite.rendered_frames -
                                 snes_lite.profile_rendered;

        snes_lite.displayed_fps =
            snes_lite.frame_count * HZ / elapsed;
        snes_lite.displayed_render_fps = rendered * HZ / elapsed;
        snes_lite.profile_rendered = snes_lite.rendered_frames;

        if (snes_lite.config.frameskip < 0)
        {
            int previous = snes_lite.effective_frameskip;
            int target_fps = (snes_lite.frame_rate_milli + 500) / 1000;
            int slow_fps;
            int fast_fps;

            if (target_fps <= 0)
                target_fps = 60;
            slow_fps = target_fps * 9 / 10;
            fast_fps = target_fps - 1;

            if (snes_lite.displayed_fps < slow_fps &&
                snes_lite.effective_frameskip < 4)
            {
                snes_lite.effective_frameskip++;
                snes_lite.stable_fast_windows = 0;
            }
            else if (snes_lite.displayed_fps >= fast_fps &&
                     snes_lite.effective_frameskip > 0)
            {
                if (++snes_lite.stable_fast_windows >= 3)
                {
                    snes_lite.effective_frameskip--;
                    snes_lite.stable_fast_windows = 0;
                }
            }
            else
                snes_lite.stable_fast_windows = 0;

            if (previous != snes_lite.effective_frameskip)
                snes_lite.variables_changed = true;
        }
        snes_lite.frame_count = 0;
        snes_lite.fps_tick = now;
    }
}

static void initialize_performance(void)
{
    if (snes_lite.config.frameskip >= 0)
        snes_lite.effective_frameskip = snes_lite.config.frameskip;
    else if (snes_lite.config.performance_preset == SNES_PERF_MAX)
        snes_lite.effective_frameskip = 2;
    else if (snes_lite.config.performance_preset == SNES_PERF_QUALITY)
        snes_lite.effective_frameskip = 0;
    else if (snes_lite.config.performance_preset == SNES_PERF_FAST)
        snes_lite.effective_frameskip = 1;
    else
        snes_lite.effective_frameskip = 0;
}

static void set_cpu_boost(bool enabled)
{
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    if (enabled != snes_lite.cpu_boosted)
    {
        rb->cpu_boost(enabled);
        snes_lite.cpu_boosted = enabled;
    }
#else
    (void)enabled;
#endif
}

static void pace_frame(void)
{
    long now;
    int delay;
    unsigned rate = snes_lite.frame_rate_milli ?
                    snes_lite.frame_rate_milli : 60000;

#ifdef SIMULATOR
    if (getenv("SNES_LITE_TEST_UNTHROTTLED"))
        return;
#endif

    snes_lite.frame_tick_fraction += HZ * 1000;
    delay = snes_lite.frame_tick_fraction / rate;
    snes_lite.frame_tick_fraction %= rate;
    snes_lite.frame_deadline += delay;
    now = *rb->current_tick;
    if (TIME_BEFORE(now, snes_lite.frame_deadline))
        rb->sleep(snes_lite.frame_deadline - now);
    else if (now - snes_lite.frame_deadline > HZ / 2)
    {
        snes_lite.frame_deadline = now;
        snes_lite.frame_tick_fraction = 0;
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status result = PLUGIN_OK;
    void *buffer = NULL;
    size_t buffer_size;
    bool core_started = false;
#ifdef SIMULATOR
    unsigned long test_frames = 0;
    unsigned long test_frame_limit = 0;
    const char *test_frame_text = getenv("SNES_LITE_TEST_FRAMES");

    if (test_frame_text)
        test_frame_limit = strtoul(test_frame_text, NULL, 10);
#endif

    rb->memset(&snes_lite, 0, sizeof(snes_lite));
    snes_lite_config_load(&snes_lite.config);
    initialize_performance();
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_update();

    if (!parameter || !((const char *)parameter)[0])
    {
        rb->splash(HZ * 2, "Launch SNES Lite with a .sfc or .smc ROM");
        return PLUGIN_OK;
    }

    snes_lite_config_load_game(&snes_lite.config,
                               (const char *)parameter);
    initialize_performance();

    /* This stops playback and transfers the shared buffer in one operation. */
    buffer = rb->plugin_get_audio_buffer(&buffer_size);
    if (!buffer || buffer_size < 16 * 1024 * 1024)
    {
        rb->splash(HZ * 3, "Insufficient plugin memory (16 MB required)");
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    snes_lite_arena_init(buffer, buffer_size);
    open_log();
    if (!snes_lite_video_selftest())
    {
        rb->splash(HZ * 3, "SNES video self-test failed");
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    set_cpu_boost(true);
    snes_lite_log("start rom=%s memory=%lu audio=%d frameskip=%d effective=%d preset=%d video=%d boost=%d",
                  (const char *)parameter, (unsigned long)buffer_size,
                  snes_lite.config.audio, snes_lite.config.frameskip,
                  snes_lite.effective_frameskip,
                  snes_lite.config.performance_preset,
                  snes_lite.config.video_mode,
                  snes_lite.cpu_boosted ? 1 : 0);
    snes_lite_log("renderer=portable_c video_path=%s",
                  snes_lite.config.video_mode == SNES_VIDEO_NATIVE ?
                  "rgb565_native_256x224" : "rgb565_fullscreen_256_to_320");
    snes_lite_log("video_policy=measured_speed transparency=enabled color=core_rgb565");

    if (!snes_lite_rom_load(parameter))
    {
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    snes_lite_log("rom size=%lu title=%s map=%02x type=%02x save=%s",
                  (unsigned long)snes_lite.rom.size, snes_lite.rom.title,
                  snes_lite.rom.map_mode, snes_lite.rom.cartridge_type,
                  snes_lite.rom.save_path);
    if (snes_lite.rom.unsupported_chip)
    {
        rb->splashf(HZ * 4, "%s is not supported in SNES Lite yet",
                    snes_lite.rom.unsupported_chip);
        snes_lite_log("unsupported chip=%s",
                      snes_lite.rom.unsupported_chip);
        result = PLUGIN_ERROR;
        goto cleanup;
    }

    if (snes_lite.config.audio != SNES_AUDIO_OFF &&
        !snes_lite_audio_init())
    {
        rb->splash(HZ * 2, "Audio unavailable; continuing muted");
        snes_lite.config.audio = SNES_AUDIO_OFF;
    }
    show_startup_diagnostics();
    if (!snes_lite_core_start())
    {
        rb->splash(HZ * 3, "SNES core initialization failed");
        snes_lite_log("core init failed used=%lu",
                      (unsigned long)snes_lite.arena_used);
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    core_started = true;
    snes_lite_sram_load();
    if (rockachievements_available(parameter))
    {
        void *achievement_workspace = snes_lite_try_malloc(
            ROCKACHIEVEMENTS_WORKSPACE_TARGET);

        if (achievement_workspace && rockachievements_init(
                &snes_achievements, parameter,
                snes_lite_core_achievement_peek, NULL,
                achievement_workspace,
                ROCKACHIEVEMENTS_WORKSPACE_TARGET))
            snes_lite_log("achievements active=%u",
                          snes_achievements.active_count);
        else
            snes_lite_log("achievements inactive");
    }
    snes_lite.fps_tick = *rb->current_tick;
    snes_lite.total_start_tick = snes_lite.fps_tick;
    snes_lite.frame_deadline = snes_lite.fps_tick;
    snes_lite_log("core ready used=%lu free=%lu",
                  (unsigned long)snes_lite.arena_used,
                  (unsigned long)(snes_lite.arena_size -
                                  snes_lite.arena_used));

    while (!snes_lite.quit_requested && !snes_lite.core_failed)
    {
        snes_lite_core_run();
        rockachievements_do_frame(&snes_achievements);
        pace_frame();
        update_fps();
#ifdef SIMULATOR
        if (test_frame_limit && ++test_frames >= test_frame_limit)
            snes_lite.quit_requested = true;
#endif
        if (snes_lite.menu_requested)
        {
            snes_lite.menu_requested = false;
            snes_lite_audio_pause(true);
            set_cpu_boost(false);
            snes_lite_menu();
            set_cpu_boost(true);
            snes_lite_audio_pause(false);
        }
        if (snes_lite.reset_requested)
        {
            snes_lite.reset_requested = false;
            snes_lite_core_reset();
            rockachievements_reset(&snes_achievements);
        }
    }
    if (snes_lite.core_failed)
    {
        rb->splash(HZ * 2, "SNES core stopped unexpectedly");
        result = PLUGIN_ERROR;
    }

cleanup:
    snes_lite_audio_close();
    rockachievements_shutdown(&snes_achievements);
    if (core_started)
    {
        snes_lite_sram_save();
        snes_lite_core_stop();
    }
    snes_lite_config_save_game(&snes_lite.config);
    snes_lite_log("profile loops=%lu rendered=%lu skipped=%lu ticks=%lu effective=%d emu_fps=%d draw_fps=%d",
                  snes_lite.profile_frames, snes_lite.rendered_frames,
                  snes_lite.skipped_frames,
                  snes_lite.total_start_tick ?
                  *rb->current_tick - snes_lite.total_start_tick : 0,
                  snes_lite.effective_frameskip,
                  snes_lite.displayed_fps,
                  snes_lite.displayed_render_fps);
    snes_lite_log("profile work core_ticks=%lu scale_ticks=%lu lcd_ticks=%lu",
                  snes_lite.core_ticks, snes_lite.video_scale_ticks,
                  snes_lite.video_lcd_ticks);
    snes_lite_log("exit status=%d", result);
    if (log_fd >= 0)
    {
        rb->close(log_fd);
        log_fd = -1;
    }
    if (buffer)
        rb->plugin_release_audio_buffer();
    set_cpu_boost(false);
    rb->lcd_setfont(FONT_UI);
    return result;
}
