#include "plugin.h"
#include "smsgg_audio.h"
#include "smsgg_core.h"
#include "smsgg_platform.h"

extern void set_config(void);

static bool has_ext(const char *path, const char *ext)
{
    size_t len = rb->strlen(path);
    size_t ext_len = rb->strlen(ext);

    if (len < ext_len)
        return false;

    return rb->strcasecmp(path + len - ext_len, ext) == 0;
}

static bool read_file(const char *path, uint8 *buf, size_t max_size,
                      size_t *out_size)
{
    int fd = rb->open(path, O_RDONLY);
    ssize_t got;
    off_t size;

    if (fd < 0)
        return false;

    size = rb->filesize(fd);
    if (size <= 0 || (size_t)size > max_size)
    {
        rb->close(fd);
        return false;
    }

    got = rb->read(fd, buf, size);
    rb->close(fd);

    if (got != size)
        return false;

    *out_size = (size_t)size;
    return true;
}

bool smsgg_core_load(struct smsgg_core *core, const char *path,
                     bool audio_enabled)
{
    size_t actual_size;

    rb->memset(core, 0, sizeof(*core));

    core->rom = smsgg_malloc(SMSGG_MAX_ROM_SIZE);
    core->sram = smsgg_calloc(SMSGG_SRAM_SIZE, 1);
    core->framebuffer = smsgg_calloc(SMSGG_FB_WIDTH * SMSGG_FB_HEIGHT, 1);
    core->state_buffer = smsgg_malloc(SMSGG_STATE_SIZE);

    if (core->rom == NULL || core->sram == NULL || core->framebuffer == NULL ||
        core->state_buffer == NULL)
        return false;

    rb->memset(core->rom, 0, SMSGG_MAX_ROM_SIZE);

    if (!read_file(path, core->rom, SMSGG_MAX_ROM_SIZE, &actual_size))
        return false;

    if ((actual_size / 512) & 1)
    {
        actual_size -= 512;
        rb->memmove(core->rom, core->rom + 512, actual_size);
    }

    cart.rom = core->rom;
    cart.sram = core->sram;
    cart.size = actual_size < 0x4000 ? 0x4000 : actual_size;
    cart.pages = (cart.size + 0x3fff) / 0x4000;
    cart.crc = rb->crc_32(cart.rom, cart.size, 0);
    cart.sram_crc = 0;
    cart.loaded = 1;
    core->crc = cart.crc;
    core->rom_size = cart.size;

    system_reset_config();
    option.console = has_ext(path, ".gg") ? 3 : 0;
    set_config();

    bitmap.width = SMSGG_FB_WIDTH;
    bitmap.height = SMSGG_FB_HEIGHT;
    bitmap.pitch = bitmap.width;
    bitmap.data = core->framebuffer;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.w = SMSGG_FB_WIDTH;
    bitmap.viewport.h = SMSGG_FB_HEIGHT;

    option.sndrate = audio_enabled ? SMSGG_AUDIO_RATE : 0;
    option.overscan = 0;
    option.extra_gg = 0;
    sms.use_fm = 0;

    system_init2();
    system_reset();

    core->is_gg = sms.console == CONSOLE_GG || sms.console == CONSOLE_GGMS;
    core->loaded = true;
    return true;
}

void smsgg_core_unload(struct smsgg_core *core)
{
    if (core->loaded)
        system_shutdown();
    core->loaded = false;
}

void smsgg_core_reset(struct smsgg_core *core)
{
    (void)core;
    system_reset();
}

void smsgg_core_run_frame(bool skip)
{
    system_frame(skip ? 1 : 0);
}

void smsgg_core_set_buttons(uint8 pad, uint8 system)
{
    input.pad[0] = pad;
    input.pad[1] = 0;
    input.system = system;
}

void smsgg_core_set_audio(bool enabled)
{
    if (enabled)
    {
        option.sndrate = SMSGG_AUDIO_RATE;
        sound_init();
    }
    else
    {
        option.sndrate = 0;
        sound_shutdown();
    }
}

bool smsgg_core_save_state(struct smsgg_core *core, const char *path)
{
    int fd;

    rb->memset(core->state_buffer, 0, SMSGG_STATE_SIZE);
    system_save_state(core->state_buffer);
    render_init();

    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    if (rb->write(fd, core->state_buffer, SMSGG_STATE_SIZE) != SMSGG_STATE_SIZE)
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

bool smsgg_core_load_state(struct smsgg_core *core, const char *path)
{
    size_t size;

    if (!read_file(path, core->state_buffer, SMSGG_STATE_SIZE, &size))
        return false;

    if (size != SMSGG_STATE_SIZE)
        return false;

    system_load_state(core->state_buffer);
    return true;
}

bool smsgg_core_save_sram(struct smsgg_core *core, const char *path)
{
    int fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        return false;

    if (rb->write(fd, core->sram, SMSGG_SRAM_SIZE) != SMSGG_SRAM_SIZE)
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

bool smsgg_core_load_sram(struct smsgg_core *core, const char *path)
{
    size_t size;

    if (!rb->file_exists(path))
        return true;

    if (!read_file(path, core->sram, SMSGG_SRAM_SIZE, &size))
        return false;

    return size == SMSGG_SRAM_SIZE;
}
