#include "anarch_platform.h"
#include "lib/helper.h"

#include <fcntl.h>

#define SFG_AVR 0
#define int_fast8_t int8_t
#define int_fast16_t int16_t
#define SFG_FPS 30
#define SFG_SCREEN_RESOLUTION_X ANARCH_WIDTH
#define SFG_SCREEN_RESOLUTION_Y ANARCH_HEIGHT
#define SFG_PLAYER_TURN_SPEED 90
#define SFG_RESOLUTION_SCALEDOWN 2
#define SFG_RAYCASTING_SUBSAMPLE 2
#define SFG_RAYCASTING_MAX_STEPS 18
#define SFG_RAYCASTING_MAX_HITS 8
#define SFG_DITHERED_SHADOW 1
#define SFG_DIMINISH_SPRITES 0
#define SFG_BACKGROUND_BLUR 0
#define SFG_CAN_EXIT 1
#define SFG_LOG(text) do { } while (0);
#define SFG_CPU_LOAD(percent) anarch_profile_render(percent);

#include "upstream/game.h"

#ifdef SIMULATOR
#define SIM_TEST_FRAMES 1800
#define SIM_TEST_LOG ROCKBOX_DIR "/games/anarch/sim-test.log"

static bool sim_test;
static uint32_t sim_time;
static unsigned int sim_iteration;
static uint32_t sim_binding_mask;
static uint8_t sim_save[ANARCH_SAVE_SIZE];
static bool sim_save_valid;

struct sim_result {
    uint32_t state_hash;
    uint32_t framebuffer_hash;
    uint32_t map_hash;
    uint32_t bindings;
    uint32_t frames;
    uint32_t render_samples;
    uint32_t late_frames;
    unsigned long underruns;
    unsigned int input_queue_worst;
    bool reached_gameplay;
    bool guards_ok;
};

struct sim_state_hash {
    uint32_t frame;
    uint32_t frame_time;
    uint32_t state_time;
    int32_t x;
    int32_t y;
    int32_t height;
    int32_t direction_x;
    int32_t direction_y;
    uint8_t state;
    uint8_t random;
    uint8_t level;
    uint8_t weapon;
    uint8_t health;
    uint8_t ammo[3];
    uint8_t monsters_dead;
    uint8_t save[ANARCH_SAVE_SIZE];
};

static bool sim_key_down(uint8_t key)
{
    unsigned int frame = sim_iteration;
    bool down = false;

    if (key == ANARCH_KEY_A)
        down = frame == 2 || frame == 5 ||
               (frame >= 520 && frame < 525) || frame == 620;
    else if (key == ANARCH_KEY_UP)
        down = frame >= 70 && frame < 300;
    else if (key == ANARCH_KEY_RIGHT)
        down = frame >= 300 && frame < 380;
    else if (key == ANARCH_KEY_DOWN)
        down = frame >= 380 && frame < 410;
    else if (key == ANARCH_KEY_LEFT)
        down = frame >= 410 && frame < 450;
    else if (key == ANARCH_KEY_JUMP)
        down = frame >= 450 && frame < 455;
    else if (key == ANARCH_KEY_STRAFE_LEFT)
        down = frame >= 460 && frame < 490;
    else if (key == ANARCH_KEY_STRAFE_RIGHT)
        down = frame >= 490 && frame < 520;
    else if (key == ANARCH_KEY_NEXT_WEAPON)
        down = frame == 540;
    else if (key == ANARCH_KEY_PREVIOUS_WEAPON)
        down = frame == 550;
    else if (key == ANARCH_KEY_CYCLE_WEAPON)
        down = frame == 560;
    else if (key == ANARCH_KEY_TOGGLE_FREELOOK)
        down = frame == 570;
    else if (key == ANARCH_KEY_MAP)
        down = frame >= 580 && frame < 590;
    else if (key == ANARCH_KEY_MENU)
        down = frame == 610;
    else if (key == ANARCH_KEY_B)
        down = frame == 600;
    else if (key == ANARCH_KEY_C)
        down = frame == 650;
    if (down)
        sim_binding_mask |= 1u << key;
    return down;
}

static uint32_t sim_state_crc(void)
{
    struct sim_state_hash state;

    rb->memset(&state, 0, sizeof(state));
    state.frame = SFG_game.frame;
    state.frame_time = SFG_game.frameTime;
    state.state_time = SFG_game.stateTime;
    state.x = SFG_player.camera.position.x;
    state.y = SFG_player.camera.position.y;
    state.height = SFG_player.camera.height;
    state.direction_x = SFG_player.direction.x;
    state.direction_y = SFG_player.direction.y;
    state.state = SFG_game.state;
    state.random = SFG_game.currentRandom;
    state.level = SFG_currentLevel.levelNumber;
    state.weapon = SFG_player.weapon;
    state.health = SFG_player.health;
    rb->memcpy(state.ammo, SFG_player.ammo, sizeof(state.ammo));
    state.monsters_dead = SFG_currentLevel.monstersDead;
    rb->memcpy(state.save, SFG_game.save, sizeof(state.save));
    return rb->crc_32(&state, sizeof(state), 0xffffffffu);
}

static struct sim_result sim_run(bool audio)
{
    struct sim_result result;
    unsigned int i;

    rb->memset(&result, 0, sizeof(result));
    sim_time = 0;
    sim_iteration = 0;
    sim_binding_mask = 0;
    sim_save_valid = false;
    anarch_video_clear();
    anarch_audio_init(audio);
    anarch_profile_reset();
    SFG_init();
    for (i = 0; i < SIM_TEST_FRAMES && SFG_game.continues; ++i)
    {
        sim_iteration = i;
        sim_time += 34;
        anarch_audio_pump();
        SFG_mainLoopBody();
        if (i == 585)
            result.map_hash = anarch_video_hash();
        if (SFG_game.state == SFG_GAME_STATE_PLAYING)
            result.reached_gameplay = true;
        if (!anarch_video_guards_ok())
            break;
    }
    result.state_hash = sim_state_crc();
    result.framebuffer_hash = anarch_video_hash();
    result.bindings = sim_binding_mask;
    result.frames = SFG_game.frame;
    result.render_samples = anarch_profile_render_samples();
    result.late_frames = anarch_profile_late_frames();
    result.underruns = anarch_audio_underruns();
    result.input_queue_worst = anarch_profile_input_queue_worst();
    result.guards_ok = anarch_video_guards_ok();
    anarch_audio_shutdown();
    return result;
}

static enum plugin_status run_sim_test(void)
{
    struct sim_result silent;
    struct sim_result audio;
    uint32_t required = (1u << ANARCH_KEY_COUNT) - 1u;
    bool pass;
    bool save_pass;
    bool config_pass;
    bool menu_pass;
    bool input_pass;
    bool dump_pass;
    int fd;

    sim_test = true;
    silent = sim_run(false);
    audio = sim_run(true);
    anarch_video_present();
    dump_pass = anarch_video_dump();
    rb->sleep(HZ);
    save_pass = anarch_save_selftest();
    config_pass = anarch_config_selftest();
    menu_pass = anarch_menu_selftest();
    input_pass = anarch_input_selftest();
    sim_test = false;
    pass = silent.reached_gameplay && audio.reached_gameplay &&
           silent.guards_ok && audio.guards_ok &&
           silent.frames >= SIM_TEST_FRAMES &&
           audio.frames >= SIM_TEST_FRAMES &&
           silent.bindings == required && audio.bindings == required &&
           silent.state_hash == audio.state_hash &&
           silent.framebuffer_hash == audio.framebuffer_hash &&
           silent.map_hash != 0 && silent.map_hash == audio.map_hash;
    pass = pass && save_pass && config_pass && menu_pass && input_pass &&
           dump_pass;
    fd = rb->open(SIM_TEST_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd,
            "ANARCH_SIM_TEST_V1\npass=%d\nframes=%lu\n"
            "bindings=%08lx\nstate_silent=%08lx\nstate_audio=%08lx\n"
            "framebuffer_silent=%08lx\nframebuffer_audio=%08lx\n"
            "map_silent=%08lx\nmap_audio=%08lx\n"
            "reached_gameplay=%d,%d\nguards=%d,%d\nsave=%d\n"
            "config=%d\nmenu=%d\ninput=%d\ndump=%d\n"
            "arena_used=%lu\narena_free=%lu\n"
            "render_samples=%lu\nlate_frames=%lu\n"
            "input_queue_worst=%u\n"
            "underruns=%lu\n",
            pass ? 1 : 0, (unsigned long)audio.frames,
            (unsigned long)audio.bindings,
            (unsigned long)silent.state_hash,
            (unsigned long)audio.state_hash,
            (unsigned long)silent.framebuffer_hash,
            (unsigned long)audio.framebuffer_hash,
            (unsigned long)silent.map_hash,
            (unsigned long)audio.map_hash,
            silent.reached_gameplay ? 1 : 0,
            audio.reached_gameplay ? 1 : 0,
            silent.guards_ok ? 1 : 0, audio.guards_ok ? 1 : 0,
            save_pass ? 1 : 0,
            config_pass ? 1 : 0,
            menu_pass ? 1 : 0,
            input_pass ? 1 : 0,
            dump_pass ? 1 : 0,
            (unsigned long)anarch_video_arena_used(),
            (unsigned long)anarch_video_arena_free(),
            (unsigned long)audio.render_samples,
            (unsigned long)audio.late_frames,
            audio.input_queue_worst,
            audio.underruns);
        rb->close(fd);
    }
    rb->sleep(HZ * 2);
    return pass ? PLUGIN_OK : PLUGIN_ERROR;
}
#endif

static void SFG_setMapPixel(uint16_t x, uint16_t y, uint16_t color)
{
    const int source_size = SFG_MAP_SIZE * SFG_MAP_PIXEL_SIZE *
                            SFG_RESOLUTION_SCALEDOWN;
    const int source_x = (ANARCH_WIDTH - source_size) / 2;
    const int source_y = (ANARCH_HEIGHT - source_size) / 2;
    const int output_size = ANARCH_HEIGHT;
    const int output_x = (ANARCH_WIDTH - output_size) / 2;
    int relative_x = x - source_x;
    int relative_y = y - source_y;
    int left;
    int right;
    int top;
    int bottom;
    int draw_x;
    int draw_y;

    if (relative_x < 0 || relative_x >= source_size ||
        relative_y < 0 || relative_y >= source_size)
    {
        anarch_video_pixel(x, y, color);
        return;
    }

    left = output_x + relative_x * output_size / source_size;
    right = output_x + (relative_x + 1) * output_size / source_size;
    top = relative_y * output_size / source_size;
    bottom = (relative_y + 1) * output_size / source_size;
    for (draw_y = top; draw_y < bottom; ++draw_y)
        for (draw_x = left; draw_x < right; ++draw_x)
            anarch_video_pixel(draw_x, draw_y, color);
}

static inline void SFG_setPixel(uint16_t x, uint16_t y, uint8_t color_index)
{
    uint16_t color = paletteRGB565[color_index];

    if (SFG_game.state == SFG_GAME_STATE_MAP ||
        SFG_keyIsDown(SFG_KEY_MAP))
        SFG_setMapPixel(x, y, color);
    else
        anarch_video_pixel(x, y, color);
}

uint32_t SFG_getTimeMs(void)
{
#ifdef SIMULATOR
    if (sim_test)
        return sim_time;
#endif
    uint32_t ticks = (uint32_t)*rb->current_tick;

    return (ticks / HZ) * 1000u + ((ticks % HZ) * 1000u) / HZ;
}

void SFG_sleepMs(uint16_t milliseconds)
{
#ifdef SIMULATOR
    if (sim_test)
    {
        (void)milliseconds;
        return;
    }
#endif
    int ticks = (milliseconds * HZ) / 1000;

    anarch_input_poll();
    anarch_audio_pump();
    if (ticks > 0)
        rb->sleep(ticks);
    else
        rb->yield();
}

int8_t SFG_keyPressed(uint8_t key)
{
#ifdef SIMULATOR
    if (sim_test)
        return sim_key_down(key);
#endif
    return anarch_input_key(key);
}

void SFG_getMouseOffset(int16_t *x, int16_t *y)
{
    *x = 0;
    *y = 0;
}

void SFG_playSound(uint8_t sound, uint8_t volume)
{
    anarch_audio_sound(sound, volume);
}

void SFG_setMusic(uint8_t command)
{
    anarch_audio_music(command);
}

void SFG_processEvent(uint8_t event, uint8_t data)
{
    anarch_haptic_event(event, data);
}

void SFG_save(uint8_t data[SFG_SAVE_SIZE])
{
#ifdef SIMULATOR
    if (sim_test)
    {
        rb->memcpy(sim_save, data, sizeof(sim_save));
        sim_save_valid = true;
        return;
    }
#endif
    anarch_save_write(data);
}

uint8_t SFG_load(uint8_t data[SFG_SAVE_SIZE])
{
#ifdef SIMULATOR
    if (sim_test)
    {
        if (sim_save_valid)
            rb->memcpy(data, sim_save, sizeof(sim_save));
        return 1;
    }
#endif
    return anarch_save_read(data);
}

static void cleanup(void)
{
    anarch_audio_shutdown();
    anarch_input_shutdown();
    backlight_use_settings();
    rb->lcd_set_backdrop(NULL);
    rb->lcd_clear_display();
    rb->lcd_update();
}

static void save_checkpoint(void)
{
    if (SFG_currentLevel.levelPointer != NULL &&
        SFG_game.saved != SFG_CANT_SAVE)
    {
        uint8_t unlocked = SFG_game.save[0] & 0x0f;

        if (SFG_currentLevel.levelNumber > unlocked)
            unlocked = SFG_currentLevel.levelNumber;
        SFG_game.save[0] =
            (uint8_t)((SFG_currentLevel.levelNumber << 4) | unlocked);
        SFG_game.save[1] = SFG_game.settings;
        SFG_game.save[2] = SFG_player.health;
        SFG_game.save[3] = SFG_player.ammo[0];
        SFG_game.save[4] = SFG_player.ammo[1];
        SFG_game.save[5] = SFG_player.ammo[2];
        SFG_gameSave();
    }
}

static void apply_settings(const struct anarch_settings *settings)
{
    SFG_game.settings &= ~0x03;
    if (settings->sound)
        SFG_game.settings |= 0x01;
    if (settings->music)
        SFG_game.settings |= 0x02;
    SFG_game.save[1] = SFG_game.settings;
    anarch_audio_music(settings->music ? SFG_MUSIC_TURN_ON :
                                         SFG_MUSIC_TURN_OFF);
    anarch_haptics_set_enabled(settings->haptics);
}

static void sync_settings_from_game(struct anarch_settings *settings)
{
    bool sound = (SFG_game.settings & 0x01) != 0;
    bool music = (SFG_game.settings & 0x02) != 0;

    if (settings->sound == sound && settings->music == music)
        return;
    settings->sound = sound;
    settings->music = music;
    anarch_settings_save(settings);
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status = PLUGIN_OK;
    struct anarch_settings settings;
    bool audio_enabled = true;
    bool frontend_map = false;
    bool quit = false;

    if (parameter != NULL && rb->strcmp(parameter, "--silent") == 0)
        audio_enabled = false;
    if (LCD_WIDTH != ANARCH_WIDTH || LCD_HEIGHT != ANARCH_HEIGHT)
    {
        rb->splash(HZ * 2, "Anarch requires 320x240");
        return PLUGIN_ERROR;
    }
    if (!anarch_video_init())
    {
        rb->splash(HZ * 2, "Not enough plugin memory");
        return PLUGIN_ERROR;
    }

    rb->lcd_set_backdrop(NULL);
    backlight_ignore_timeout();
    anarch_save_init();
    anarch_settings_load(&settings);
    anarch_input_init();
#ifdef SIMULATOR
    if (parameter != NULL && rb->strcmp(parameter, "--sim-test") == 0)
    {
        status = run_sim_test();
        cleanup();
        return status;
    }
#endif
    anarch_audio_init(audio_enabled);
    anarch_profile_reset();
    SFG_init();
    apply_settings(&settings);
    if (anarch_save_warning())
        rb->splash(HZ * 2, "Save is corrupt; starting new game");

    while (SFG_game.continues && !anarch_input_usb() && !quit)
    {
        bool paused = SFG_game.state == SFG_GAME_STATE_MENU ||
                      SFG_game.state == SFG_GAME_STATE_MAP;
        bool show_pause = false;

        if (frontend_map && SFG_game.state == SFG_GAME_STATE_MENU)
        {
            frontend_map = false;
            SFG_setGameState(SFG_GAME_STATE_PLAYING);
            show_pause = true;
            paused = true;
        }

        anarch_input_set_menu(paused);
        anarch_audio_pause(paused);
        anarch_input_poll();
        if (anarch_input_pause_requested())
        {
            anarch_input_clear_pause();
            if (SFG_currentLevel.levelPointer != NULL)
                show_pause = true;
        }
        if (show_pause)
        {
            enum anarch_menu_action action = anarch_menu_run(&settings);

            apply_settings(&settings);
            anarch_video_present();
            if (action == ANARCH_MENU_USB)
            {
                save_checkpoint();
                status = PLUGIN_USB_CONNECTED;
                quit = true;
            }
            else if (action == ANARCH_MENU_MAP)
            {
                SFG_setGameState(SFG_GAME_STATE_MAP);
                frontend_map = true;
            }
            else if (action == ANARCH_MENU_SAVE)
            {
                save_checkpoint();
                rb->splash(HZ, "Game saved");
            }
            else if (action == ANARCH_MENU_RESTART)
            {
                if (rb->yesno_pop_confirm("Restart this level?"))
                    SFG_setAndInitLevel(SFG_currentLevel.levelNumber);
            }
            else if (action == ANARCH_MENU_EXIT)
            {
                if (rb->yesno_pop("Save and exit Anarch?"))
                {
                    save_checkpoint();
                    quit = true;
                }
                else if (rb->yesno_pop_confirm("Exit without saving?"))
                    quit = true;
            }
            anarch_audio_pause(false);
            if (quit)
                break;
            continue;
        }
        if (anarch_input_exit())
        {
#ifdef HAS_BUTTON_HOLD
            if (rb->button_hold())
            {
                anarch_audio_pause(true);
                rb->splash(0, "Unlock to exit Anarch");
                while (rb->button_hold() && !anarch_input_usb())
                {
                    anarch_input_poll();
                    rb->sleep(MAX(1, HZ / 20));
                }
                anarch_video_present();
                if (anarch_input_usb())
                    continue;
            }
#endif
            if (rb->yesno_pop("Save and exit Anarch?"))
            {
                save_checkpoint();
                break;
            }
            anarch_input_clear_exit();
            anarch_video_present();
            continue;
        }
        anarch_audio_pump();
        if (!SFG_mainLoopBody())
            break;
        sync_settings_from_game(&settings);
        {
            long lcd_start = *rb->current_tick;

            anarch_video_present();
            anarch_profile_lcd((unsigned int)(*rb->current_tick - lcd_start));
        }
        if (!anarch_video_guards_ok())
        {
            status = PLUGIN_ERROR;
            break;
        }
    }
    if (anarch_input_usb())
    {
        save_checkpoint();
        status = PLUGIN_USB_CONNECTED;
    }
    anarch_profile_write(SFG_game.frame, anarch_audio_underruns(),
                         anarch_video_guards_ok());
    cleanup();
    return status;
}
