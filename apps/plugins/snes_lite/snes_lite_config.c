/***************************************************************************
 * SNES Lite global configuration.
 ***************************************************************************/

#include "snes_lite.h"

static char game_config_path[MAX_PATH];

const char *snes_lite_input_profile_name(enum snes_lite_input_profile profile)
{
    static const char *const names[] = {
        "Platformer", "RPG", "Action", "Fighting", "Racing", "Sports"
    };

    if ((unsigned)profile >= ARRAYLEN(names))
        profile = SNES_INPUT_PLATFORMER;
    return names[profile];
}

void snes_lite_config_defaults(struct snes_lite_config *config)
{
    config->frameskip = -1;
    config->audio = SNES_AUDIO_AUTO;
    config->input_profile = SNES_INPUT_PLATFORMER;
    config->show_fps = false;
    config->performance_mode = true;
    /* Native 256x224 preserves one source pixel per LCD pixel.  Fullscreen
     * scaling duplicates pixels and scanlines on the iPod 320x240 panel and
     * is kept as an explicit menu option rather than the visual default. */
    config->video_mode = SNES_VIDEO_NATIVE;
    config->performance_preset = SNES_PERF_BALANCED;
}

static char *value_after_equals(char *line)
{
    char *equals = rb->strchr(line, '=');

    if (!equals)
        return NULL;
    *equals++ = '\0';
    return equals;
}

static void config_read(struct snes_lite_config *config, const char *path)
{
    int fd;
    char line[96];

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *value = value_after_equals(line);
        if (!value)
            continue;
        if (!rb->strcmp(line, "frameskip"))
        {
            if (!rb->strcmp(value, "auto"))
                config->frameskip = -1;
            else
                config->frameskip = MIN(4, MAX(0, rb->atoi(value)));
        }
        else if (!rb->strcmp(line, "audio"))
        {
            if (!rb->strcmp(value, "on"))
                config->audio = SNES_AUDIO_ON;
            else if (!rb->strcmp(value, "low"))
                config->audio = SNES_AUDIO_LOW;
            else if (!rb->strcmp(value, "auto"))
                config->audio = SNES_AUDIO_AUTO;
            else
                config->audio = SNES_AUDIO_OFF;
        }
        else if (!rb->strcmp(line, "input_profile"))
            config->input_profile = MIN(SNES_INPUT_SPORTS,
                                        MAX(SNES_INPUT_PLATFORMER,
                                            rb->atoi(value)));
        else if (!rb->strcmp(line, "show_fps"))
            config->show_fps = rb->atoi(value) != 0;
        else if (!rb->strcmp(line, "performance_mode"))
            config->performance_mode = rb->atoi(value) != 0;
        else if (!rb->strcmp(line, "video_mode"))
            config->video_mode = MIN(SNES_VIDEO_NATIVE,
                                     MAX(SNES_VIDEO_FULLSCREEN,
                                         rb->atoi(value)));
        else if (!rb->strcmp(line, "performance_preset"))
            config->performance_preset = MIN(SNES_PERF_QUALITY,
                                             MAX(SNES_PERF_BALANCED,
                                                 rb->atoi(value)));
    }
    rb->close(fd);

}

void snes_lite_config_load(struct snes_lite_config *config)
{
    snes_lite_config_defaults(config);
    game_config_path[0] = '\0';
    config_read(config, SNES_LITE_CONFIG_PATH);
}

void snes_lite_config_load_game(struct snes_lite_config *config,
                                const char *rom_path)
{
    const char *name = rb->strrchr(rom_path, '/');
    char stem[MAX_PATH];
    char *extension;

    name = name ? name + 1 : rom_path;
    rb->strlcpy(stem, name ? name : "game", sizeof(stem));
    extension = rb->strrchr(stem, '.');
    if (extension)
        *extension = '\0';
    rb->snprintf(game_config_path, sizeof(game_config_path), "%s/%s.cfg",
                 SNES_LITE_CONFIG_DIR, stem);
    config_read(config, game_config_path);
}

void snes_lite_config_save(const struct snes_lite_config *config)
{
    int fd;

    if (!rb->dir_exists(ROCKBOX_DIR "/config"))
        rb->mkdir(ROCKBOX_DIR "/config");
    fd = rb->open(SNES_LITE_CONFIG_PATH,
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    if (config->frameskip < 0)
        rb->fdprintf(fd, "frameskip=auto\n");
    else
        rb->fdprintf(fd, "frameskip=%d\n", config->frameskip);
    rb->fdprintf(fd, "audio=%s\n",
                 config->audio == SNES_AUDIO_ON ? "on" :
                 (config->audio == SNES_AUDIO_LOW ? "low" :
                  (config->audio == SNES_AUDIO_AUTO ? "auto" : "off")));
    rb->fdprintf(fd, "input_profile=%d\n", config->input_profile);
    rb->fdprintf(fd, "show_fps=%d\n", config->show_fps ? 1 : 0);
    rb->fdprintf(fd, "performance_mode=%d\n",
                 config->performance_mode ? 1 : 0);
    rb->fdprintf(fd, "video_mode=%d\n", config->video_mode);
    rb->fdprintf(fd, "performance_preset=%d\n",
                 config->performance_preset);
    rb->close(fd);
}

void snes_lite_config_save_game(const struct snes_lite_config *config)
{
    int fd;

    if (!game_config_path[0])
        return;
    if (!rb->dir_exists(ROCKBOX_DIR "/config"))
        rb->mkdir(ROCKBOX_DIR "/config");
    if (!rb->dir_exists(SNES_LITE_CONFIG_DIR))
        rb->mkdir(SNES_LITE_CONFIG_DIR);
    fd = rb->open(game_config_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    if (config->frameskip < 0)
        rb->fdprintf(fd, "frameskip=auto\n");
    else
        rb->fdprintf(fd, "frameskip=%d\n", config->frameskip);
    rb->fdprintf(fd, "audio=%s\n",
                 config->audio == SNES_AUDIO_ON ? "on" :
                 (config->audio == SNES_AUDIO_LOW ? "low" :
                  (config->audio == SNES_AUDIO_AUTO ? "auto" : "off")));
    rb->fdprintf(fd, "input_profile=%d\n", config->input_profile);
    rb->fdprintf(fd, "show_fps=%d\n", config->show_fps ? 1 : 0);
    rb->fdprintf(fd, "performance_mode=%d\n",
                 config->performance_mode ? 1 : 0);
    rb->fdprintf(fd, "video_mode=%d\n", config->video_mode);
    rb->fdprintf(fd, "performance_preset=%d\n",
                 config->performance_preset);
    rb->close(fd);
}
