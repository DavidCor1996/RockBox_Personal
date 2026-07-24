#include "picodrive.h"
#include "lib/helper.h"
#include "lib/rockachievements.h"
#include "upstream/pico/pico_int.h"

#ifdef SIMULATOR
#include <stdlib.h>
#endif

#define PD_AUDIO_BLOCK_FRAMES 256
#define PD_AUDIO_BLOCKS 16
#define PD_AUDIO_START_BLOCKS 6
#define PD_AUDIO_SAMPLES (PD_AUDIO_BLOCK_FRAMES * 2)

struct pd_runtime pd;

static fb_data *video_buffer;
static int16_t *sound_frame;
static int16_t *audio_ring;
static int16_t audio_conceal[PD_AUDIO_SAMPLES] __attribute__((aligned(4)));
static volatile int audio_queued;
static volatile int audio_read;
static int audio_write;
static int audio_write_frames;
static int16_t audio_last_left;
static int16_t audio_last_right;
static unsigned old_mixer_frequency;
static unsigned long audio_underruns;
static unsigned long audio_drops;
static long menu_down_tick;
static bool menu_latched;
static bool start_pulse;
static uint32_t sram_saved_crc;
static uint32_t sram_observed_crc;
static long sram_dirty_tick;
static struct rockachievements_runtime achievements;

static void load_config(void);
static void save_config(void);

static uint32_t achievements_peek(uint32_t address, uint32_t num_bytes,
                                  void *userdata)
{
    uint32_t value = 0;
    uint32_t index;

    (void)userdata;
    if (num_bytes > 4)
        num_bytes = 4;
    for (index = 0; index < num_bytes; ++index)
    {
        uint32_t current = address + index;
        unsigned char byte = 0;

        if (current < 0x10000)
            byte = PicoMem.ram[MEM_BE2(current)];
        else if (current < 0x20000 && Pico.sv.data != NULL &&
                 current - 0x10000 < Pico.sv.size)
            byte = Pico.sv.data[current - 0x10000];
        value |= (uint32_t)byte << (index * 8);
    }
    return value;
}

static void achievements_start(void)
{
    size_t available = pd.arena_ptr && pd.arena_end > pd.arena_ptr ?
                       (size_t)(pd.arena_end - pd.arena_ptr) : 0;
    void *workspace;

    if (!rockachievements_available(pd.rom_path) ||
        available < ROCKACHIEVEMENTS_WORKSPACE_TARGET + 32)
        return;
    workspace = pd_malloc(ROCKACHIEVEMENTS_WORKSPACE_TARGET);
    if (workspace != NULL)
        rockachievements_init(&achievements, pd.rom_path, achievements_peek,
                              NULL, workspace,
                              ROCKACHIEVEMENTS_WORKSPACE_TARGET);
}

/* IEEE CRC-32, matching Python/zlib and RockPod's save identity. */
static uint32_t crc32_ieee(const void *source, size_t size)
{
    const unsigned char *data = source;
    uint32_t crc = 0xffffffffu;

    while (size--)
    {
        int bit;

        crc ^= *data++;
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

static void mkdir_if_needed(const char *path)
{
    if (!rb->dir_exists(path))
        rb->mkdir(path);
}

static bool ensure_directories(void)
{
    mkdir_if_needed(ROCKBOX_DIR "/games");
    mkdir_if_needed(PD_BASE_DIR);
    mkdir_if_needed(PD_ROM_DIR);
    mkdir_if_needed(PD_SAVE_DIR);
    mkdir_if_needed(PD_STATE_DIR);
    mkdir_if_needed(PD_CONFIG_DIR);
    return rb->dir_exists(PD_BASE_DIR) && rb->dir_exists(PD_SAVE_DIR);
}

static const char *basename_of(const char *path)
{
    const char *slash = rb->strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void safe_stem(char *destination, size_t size, const char *path)
{
    const char *source = basename_of(path);
    size_t index = 0;

    while (*source && *source != '.' && index + 1 < size)
    {
        char value = *source++;

        if (!((value >= 'a' && value <= 'z') ||
              (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '-' ||
              value == '_' || value == ' '))
            value = '_';
        destination[index++] = value;
    }
    destination[index] = '\0';
}

static void build_paths(const char *rom_path)
{
    char stem[96];

    safe_stem(stem, sizeof(stem), rom_path);
    rb->snprintf(pd.save_path, sizeof(pd.save_path), "%s/%s-%08lx.srm",
                 PD_SAVE_DIR, stem, (unsigned long)pd.rom_crc);
    rb->snprintf(pd.state_path, sizeof(pd.state_path), "%s/%s-%08lx.state0",
                 PD_STATE_DIR, stem, (unsigned long)pd.rom_crc);
    rb->snprintf(pd.config_path, sizeof(pd.config_path), "%s/%s-%08lx.cfg",
                 PD_CONFIG_DIR, stem, (unsigned long)pd.rom_crc);
}

static bool supported_extension(const char *path)
{
    const char *extension = rb->strrchr(path, '.');

    if (!extension)
        return false;
    return !rb->strcasecmp(extension, ".md") ||
           !rb->strcasecmp(extension, ".gen") ||
           !rb->strcasecmp(extension, ".bin") ||
           !rb->strcasecmp(extension, ".smd");
}

static bool raw_header_valid(const unsigned char *data, size_t size)
{
    uint32_t reset_vector;

    if (size < 0x200)
        return false;
    if (rb->strncmp((const char *)data + 0x100, "SEGA", 4) &&
        rb->strncmp((const char *)data + 0x101, "SEGA", 4))
        return false;
    reset_vector = ((uint32_t)data[4] << 24) |
                   ((uint32_t)data[5] << 16) |
                   ((uint32_t)data[6] << 8) | data[7];
    return reset_vector >= 0x100 && reset_vector < size &&
           !(reset_vector & 1);
}

static bool load_rom(const char *path)
{
    int fd;
    off_t file_size;
    unsigned char *raw;
    unsigned char *core_rom = NULL;
    unsigned int core_size = 0;
    ssize_t bytes;

    if (!supported_extension(path))
    {
        rb->splash(HZ * 2, "Use an uncompressed .md/.gen/.bin/.smd ROM");
        return false;
    }
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    file_size = rb->filesize(fd);
    if (file_size < 0x200 || (unsigned long)file_size > PD_MAX_ROM_SIZE)
    {
        rb->close(fd);
        rb->splash(HZ * 2, "ROM size is outside the 512 B-10 MiB limit");
        return false;
    }
    raw = pd_malloc((size_t)file_size);
    if (!raw)
    {
        rb->close(fd);
        return false;
    }
    bytes = rb->read(fd, raw, file_size);
    rb->close(fd);
    if (bytes != file_size)
        return false;
    if (rb->strcasecmp(rb->strrchr(path, '.'), ".smd") &&
        !raw_header_valid(raw, (size_t)file_size))
    {
        rb->splash(HZ * 2, "Not a Sega Genesis cartridge image");
        return false;
    }
    pd.rom_crc = crc32_ieee(raw, (size_t)file_size);
    if (PicoCartLoad(NULL, raw, file_size, &core_rom, &core_size, 0) != 0)
        return false;
    pd.rom = core_rom;
    pd.rom_size = core_size;
    rb->strlcpy(pd.rom_path, path, sizeof(pd.rom_path));
    build_paths(path);
    load_config();
    if (sound_frame)
    {
        unsigned rate = pd.settings.audio_rate == PD_AUDIO_RATE_LOW ?
                        PD_AUDIO_RATE_LOW : PD_AUDIO_RATE_FULL;

        rb->mixer_set_frequency(rate);
        PicoIn.sndRate = rb->mixer_get_frequency();
        if (!PicoIn.sndRate)
            PicoIn.sndRate = rate;
    }
    if (PicoCartInsert(core_rom, core_size, NULL) != 0)
        return false;
    return true;
}

static bool atomic_write(const char *path, const void *data, size_t size)
{
    char temporary[MAX_PATH];
    int fd;
    bool result = false;

    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    if (rb->write(fd, data, size) == (ssize_t)size)
    {
        result = true;
    }
    rb->close(fd);
    if (result)
    {
        rb->remove(path);
        if (rb->rename(temporary, path) < 0)
            result = false;
    }
    else
        rb->remove(temporary);
    return result;
}

static void load_sram(void)
{
    int fd;

    if (!(Pico.sv.flags & SRF_ENABLED) || !Pico.sv.data || !Pico.sv.size)
        return;
    fd = rb->open(pd.save_path, O_RDONLY);
    if (fd >= 0)
    {
        rb->read(fd, Pico.sv.data, Pico.sv.size);
        rb->close(fd);
        pd_log("sram loaded path=%s bytes=%u", pd.save_path, Pico.sv.size);
    }
    sram_saved_crc = crc32_ieee(Pico.sv.data, Pico.sv.size);
    sram_observed_crc = sram_saved_crc;
    sram_dirty_tick = 0;
}

static void save_sram(void)
{
    if (!(Pico.sv.flags & SRF_ENABLED) || !Pico.sv.data || !Pico.sv.size)
        return;
    if (atomic_write(pd.save_path, Pico.sv.data, Pico.sv.size))
    {
        sram_saved_crc = crc32_ieee(Pico.sv.data, Pico.sv.size);
        sram_observed_crc = sram_saved_crc;
        sram_dirty_tick = 0;
        pd_log("sram saved path=%s bytes=%u", pd.save_path, Pico.sv.size);
    }
    else
        pd_log("sram save failed path=%s", pd.save_path);
}

static void sram_debounce(void)
{
    uint32_t current;
    long now;

    if (!(Pico.sv.flags & SRF_ENABLED) || !Pico.sv.data || !Pico.sv.size)
        return;
    current = crc32_ieee(Pico.sv.data, Pico.sv.size);
    now = *rb->current_tick;
    if (current != sram_observed_crc)
    {
        sram_observed_crc = current;
        sram_dirty_tick = now;
    }
    else if (current != sram_saved_crc && sram_dirty_tick &&
             now - sram_dirty_tick >= 2 * HZ)
        save_sram();
}

static void load_config(void)
{
    char line[80];
    int fd = rb->open(pd.config_path, O_RDONLY);

    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *separator = rb->strchr(line, '=');
        int value;

        if (!separator)
            continue;
        *separator++ = '\0';
        value = rb->atoi(separator);
        if (!rb->strcmp(line, "frameskip") && value >= -1 && value <= 3)
            pd.settings.frameskip = value;
        else if (!rb->strcmp(line, "audio_rate") &&
                 (value == PD_AUDIO_RATE_LOW || value == PD_AUDIO_RATE_FULL))
            pd.settings.audio_rate = value;
        else if (!rb->strcmp(line, "six_button"))
            pd.settings.six_button = !!value;
        else if (!rb->strcmp(line, "show_fps"))
            pd.settings.show_fps = !!value;
    }
    rb->close(fd);
}

static void save_config(void)
{
    char temporary[MAX_PATH];
    int fd;

    if (!pd.config_path[0])
        return;
    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", pd.config_path);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "version=1\nframeskip=%d\naudio_rate=%d\n"
                     "six_button=%d\nshow_fps=%d\n",
                 pd.settings.frameskip, pd.settings.audio_rate,
                 pd.settings.six_button, pd.settings.show_fps);
    rb->close(fd);
    rb->remove(pd.config_path);
    if (rb->rename(temporary, pd.config_path) < 0)
        rb->remove(temporary);
}

static bool save_state(void)
{
    char temporary[MAX_PATH];

    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", pd.state_path);
    rb->remove(temporary);
    if (PicoState(temporary, 1) != 0)
    {
        rb->remove(temporary);
        return false;
    }
    rb->remove(pd.state_path);
    if (rb->rename(temporary, pd.state_path) < 0)
    {
        rb->remove(temporary);
        return false;
    }
    return true;
}

static bool load_state(void)
{
    return rb->file_exists(pd.state_path) &&
           PicoState(pd.state_path, 0) == 0;
}

static void audio_concealment(void)
{
    int frame;

    for (frame = 0; frame < PD_AUDIO_BLOCK_FRAMES; frame++)
    {
        int gain = frame < 32 ? 31 - frame : 0;
        audio_conceal[frame * 2] =
            (int16_t)((audio_last_left * gain) / 32);
        audio_conceal[frame * 2 + 1] =
            (int16_t)((audio_last_right * gain) / 32);
    }
    audio_last_left = 0;
    audio_last_right = 0;
}

static void audio_get_more(const void **start, size_t *size)
{
    if (audio_queued > 0 && audio_ring)
    {
        int16_t *block = audio_ring + audio_read * PD_AUDIO_SAMPLES;

        *start = block;
        *size = PD_AUDIO_SAMPLES * sizeof(int16_t);
        audio_last_left = block[PD_AUDIO_SAMPLES - 2];
        audio_last_right = block[PD_AUDIO_SAMPLES - 1];
        audio_read = (audio_read + 1) % PD_AUDIO_BLOCKS;
        audio_queued--;
    }
    else
    {
        audio_concealment();
        *start = audio_conceal;
        *size = sizeof(audio_conceal);
        audio_underruns++;
    }
}

static void audio_submit(const int16_t *data, size_t frames)
{
    while (frames > 0)
    {
        size_t room = PD_AUDIO_BLOCK_FRAMES - audio_write_frames;
        size_t take = frames < room ? frames : room;
        int16_t *destination = audio_ring +
            audio_write * PD_AUDIO_SAMPLES + audio_write_frames * 2;

        rb->memcpy(destination, data, take * 2 * sizeof(int16_t));
        data += take * 2;
        frames -= take;
        audio_write_frames += take;
        if (audio_write_frames == PD_AUDIO_BLOCK_FRAMES)
        {
            bool queued = false;

            rb->pcm_play_lock();
            if (audio_queued < PD_AUDIO_BLOCKS - 1)
            {
                audio_queued++;
                queued = true;
            }
            rb->pcm_play_unlock();
            if (queued)
            {
                audio_write = (audio_write + 1) % PD_AUDIO_BLOCKS;
                if (!pd.audio_started &&
                    audio_queued >= PD_AUDIO_START_BLOCKS)
                {
                    rb->mixer_channel_set_amplitude(
                        PCM_MIXER_CHAN_PLAYBACK, MIX_AMP_UNITY);
                    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                                audio_get_more, NULL, 0);
                    pd.audio_started = true;
                }
            }
            else
                audio_drops++;
            audio_write_frames = 0;
        }
    }
}

static void core_sound_write(int bytes)
{
    if (bytes > 0 && sound_frame)
        audio_submit(sound_frame, (size_t)bytes / 4);
}

static bool pd_audio_init(void)
{
    unsigned requested = pd.settings.audio_rate == PD_AUDIO_RATE_LOW ?
                         PD_AUDIO_RATE_LOW : PD_AUDIO_RATE_FULL;

    audio_ring = pd_calloc(PD_AUDIO_BLOCKS * PD_AUDIO_SAMPLES,
                           sizeof(int16_t));
    sound_frame = pd_calloc((PD_AUDIO_RATE_FULL / 50 + 4) * 2,
                            sizeof(int16_t));
    if (!audio_ring || !sound_frame)
        return false;
    audio_queued = 0;
    audio_read = 0;
    audio_write = 0;
    audio_write_frames = 0;
    audio_underruns = 0;
    audio_drops = 0;
    old_mixer_frequency = rb->mixer_get_frequency();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(requested);
    PicoIn.sndRate = rb->mixer_get_frequency();
    if (!PicoIn.sndRate)
        PicoIn.sndRate = requested;
    PicoIn.sndOut = sound_frame;
    PicoIn.writeSound = core_sound_write;
    rb->pcmbuf_fade(false, true);
    return true;
}

static void audio_shutdown(void)
{
    rb->pcm_play_lock();
    if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) != CHANNEL_STOPPED)
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcm_play_unlock();
    pd.audio_started = false;
    PicoIn.writeSound = NULL;
    PicoIn.sndOut = NULL;
    audio_queued = 0;
    rb->pcmbuf_fade(false, false);
    if (old_mixer_frequency)
        rb->mixer_set_frequency(old_mixer_frequency);
    pd_log("audio underruns=%lu drops=%lu", audio_underruns, audio_drops);
}

static unsigned wheel_pad(void)
{
#ifdef HAVE_WHEEL_POSITION
    int wheel = rb->wheel_status();
    int zone;

    if (wheel < 0)
        return 0;
    zone = ((wheel + 6) / 12) & 7;
    switch (zone)
    {
        case 0: return 1u << 0;
        case 1: return (1u << 0) | (1u << 3);
        case 2: return 1u << 3;
        case 3: return (1u << 1) | (1u << 3);
        case 4: return 1u << 1;
        case 5: return (1u << 1) | (1u << 2);
        case 6: return 1u << 2;
        default: return (1u << 0) | (1u << 2);
    }
#else
    return 0;
#endif
}

static bool input_poll(void)
{
    int buttons = rb->button_status();
    int event = rb->button_get(false);
    long now = *rb->current_tick;
    unsigned pad = wheel_pad();
    bool menu_requested = false;

    if (event == SYS_USB_CONNECTED)
    {
        pd.usb_connected = true;
        return false;
    }

#ifdef HAS_BUTTON_HOLD
    if (rb->button_hold())
        return false;
#endif
#ifdef BUTTON_SELECT
    if (buttons & BUTTON_SELECT)
        pad |= 1u << 4;
#endif
#ifdef BUTTON_PLAY
    if (buttons & BUTTON_PLAY)
        pad |= 1u << 5;
#endif
#ifdef BUTTON_LEFT
    if (buttons & BUTTON_LEFT)
        pad |= 1u << 6;
#endif
#ifdef BUTTON_RIGHT
    if (pd.settings.six_button && (buttons & BUTTON_RIGHT))
    {
        unsigned shifted = 0;
        if (pad & (1u << 4)) shifted |= 1u << 10;
        if (pad & (1u << 5)) shifted |= 1u << 9;
        if (pad & (1u << 6)) shifted |= 1u << 8;
        pad &= ~((1u << 4) | (1u << 5) | (1u << 6));
        pad |= shifted;
    }
#endif
#ifdef BUTTON_MENU
    if (buttons & BUTTON_MENU)
    {
        if (!menu_down_tick)
            menu_down_tick = now;
        if (!menu_latched && now - menu_down_tick >= HZ / 2)
        {
            menu_latched = true;
            menu_requested = true;
        }
    }
    else if (menu_down_tick)
    {
        if (!menu_latched)
            start_pulse = true;
        menu_down_tick = 0;
        menu_latched = false;
    }
#endif
    if (start_pulse)
    {
        pad |= 1u << 7;
        start_pulse = false;
    }
    PicoIn.pad[0] = pad;
    return !menu_requested;
}

static void video_present(void)
{
    int height = (Pico.m.pal && (Pico.video.reg[1] & 8)) ? 240 : 224;
    int top = (LCD_HEIGHT - height) / 2;

    rb->lcd_clear_display();
    rb->lcd_bitmap(video_buffer, 0, top, LCD_WIDTH, height);
    if (pd.settings.show_fps)
        rb->lcd_putsf(0, 0, "%u FPS", pd.displayed_fps);
    rb->lcd_update();
}

static int menu_run(void)
{
    MENUITEM_STRINGLIST(menu, "PicoDrive", NULL,
                        "Resume", "Save state", "Load state", "Reset",
                        "Save SRAM", "Frame skip", "Audio rate",
                        "Controller", "FPS counter", "Exit");

    while (true)
    {
        int selected = rb->do_menu(&menu, NULL, NULL, false);

        switch (selected)
        {
            case 0:
                return 0;
            case 1:
                rb->splash(HZ, save_state() ? "State saved" :
                                             "State save failed");
                break;
            case 2:
                if (rockachievements_hardcore_active(&achievements))
                {
                    rb->splash(HZ, "iPod Hardcore: state load blocked");
                    break;
                }
                if (load_state())
                {
                    rockachievements_reset(&achievements);
                    rb->splash(HZ, "State loaded");
                }
                else
                    rb->splash(HZ, "No valid state");
                break;
            case 3:
                save_sram();
                PicoReset();
                rockachievements_reset(&achievements);
                return 0;
            case 4:
                save_sram();
                rb->splash(HZ, "SRAM saved");
                break;
            case 5:
                pd.settings.frameskip = pd.settings.frameskip >= 3 ? -1 :
                                        pd.settings.frameskip + 1;
                if (pd.settings.frameskip < 0)
                    rb->splash(HZ, "Frame skip: Auto");
                else
                    rb->splashf(HZ, "Frame skip: %d",
                                pd.settings.frameskip);
                break;
            case 6:
            {
                unsigned rate;

                pd.settings.audio_rate =
                    pd.settings.audio_rate == PD_AUDIO_RATE_FULL ?
                    PD_AUDIO_RATE_LOW : PD_AUDIO_RATE_FULL;
                rate = pd.settings.audio_rate;
                rb->pcm_play_lock();
                rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
                rb->pcm_play_unlock();
                pd.audio_started = false;
                audio_queued = audio_read = audio_write = audio_write_frames = 0;
                rb->mixer_set_frequency(rate);
                PicoIn.sndRate = rb->mixer_get_frequency();
                PsndRerate(1);
                rb->splashf(HZ, "Audio: %u Hz", PicoIn.sndRate);
                break;
            }
            case 7:
                pd.settings.six_button = !pd.settings.six_button;
                PicoSetInputDevice(0, pd.settings.six_button ?
                                   PICO_INPUT_PAD_6BTN : PICO_INPUT_PAD_3BTN);
                rb->splash(HZ, pd.settings.six_button ? "6-button pad" :
                                                       "3-button pad");
                break;
            case 8:
                pd.settings.show_fps = !pd.settings.show_fps;
                rb->splash(HZ, pd.settings.show_fps ? "FPS counter on" :
                                                     "FPS counter off");
                break;
            default:
                return 1;
        }
    }
}

static void configure_core(void)
{
    PicoIn.opt = POPT_EN_FM | POPT_EN_PSG | POPT_EN_Z80 |
                 POPT_EN_STEREO | POPT_FM_YM2612;
    PicoIn.regionOverride = 0;
    PicoIn.autoRgnOrder = 0x184;
    PicoSetInputDevice(0, pd.settings.six_button ?
                       PICO_INPUT_PAD_6BTN : PICO_INPUT_PAD_3BTN);
    PicoSetInputDevice(1, PICO_INPUT_NOTHING);
    PicoDrawSetOutFormat(PDF_RGB555, 0);
    PicoDrawSetOutBuf(video_buffer, LCD_WIDTH * sizeof(fb_data));
}

static void run_loop(void)
{
    bool running = true;
    unsigned long frames = 0;
    unsigned long fps_frames = 0;
    long fps_tick = *rb->current_tick;
    long deadline = fps_tick;
    unsigned fraction = 0;
    int auto_skip_streak = 0;
    long sram_check_tick = fps_tick;
#ifdef SIMULATOR
    unsigned long test_limit = 0;
    const char *test_text = getenv("PICODRIVE_TEST_FRAMES");
    bool unthrottled = getenv("PICODRIVE_TEST_UNTHROTTLED") != NULL;

    if (test_text)
        test_limit = strtoul(test_text, NULL, 10);
#endif

    while (running && !pd.failed)
    {
        bool render;

#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold())
            break;
#endif
        if (!input_poll())
        {
            if (pd.usb_connected)
                break;
            if (menu_run())
                break;
            rb->button_clear_queue();
        }
        if (pd.settings.frameskip < 0)
        {
            bool behind = TIME_AFTER(*rb->current_tick, deadline + 1);

            render = !behind || auto_skip_streak >= 2;
            auto_skip_streak = render ? 0 : auto_skip_streak + 1;
        }
        else
            render = pd.settings.frameskip == 0 ||
                     (frames % (unsigned)(pd.settings.frameskip + 1)) == 0;
        PicoIn.skipFrame = !render;
        if (frames < 8)
            pd_log("frame=%lu begin", frames);
        PicoFrame();
        rockachievements_do_frame(&achievements);
        if (frames < 8)
            pd_log("frame=%lu end", frames);
        if (render)
            video_present();
        frames++;
        if (frames == 60 || (frames > 0 && frames % 600 == 0))
            pd_log("frame=%lu alive fps=%u", frames, pd.displayed_fps);
        fps_frames++;
        if (*rb->current_tick - fps_tick >= HZ)
        {
            pd.displayed_fps = fps_frames * HZ /
                               (*rb->current_tick - fps_tick);
            fps_frames = 0;
            fps_tick = *rb->current_tick;
        }
        if (*rb->current_tick - sram_check_tick >= HZ)
        {
            sram_debounce();
            sram_check_tick = *rb->current_tick;
        }
#ifdef SIMULATOR
        if (test_limit && frames >= test_limit)
            break;
        if (unthrottled)
            continue;
#endif
        fraction += HZ;
        deadline += fraction / (Pico.m.pal ? 50 : 60);
        fraction %= Pico.m.pal ? 50 : 60;
        if (TIME_BEFORE(*rb->current_tick, deadline))
            rb->sleep(deadline - *rb->current_tick);
        else if (*rb->current_tick - deadline > HZ / 2)
            deadline = *rb->current_tick;
        rb->yield();
    }
}

static void set_boost(bool enabled)
{
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    if (pd.cpu_boosted != enabled)
    {
        rb->cpu_boost(enabled);
        pd.cpu_boosted = enabled;
    }
#else
    (void)enabled;
#endif
}

enum plugin_status plugin_start(const void *parameter)
{
    void *shared_buffer = NULL;
    size_t shared_size = 0;
    enum plugin_status result = PLUGIN_OK;
    bool core_initialized = false;
    bool audio_initialized = false;

    rb->memset(&pd, 0, sizeof(pd));
    pd.settings.frameskip = -1;
    pd.settings.video_mode = 0;
    pd.settings.audio_rate = PD_AUDIO_RATE_FULL;
    pd.settings.six_button = 0;
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_update();
    if (!parameter || !((const char *)parameter)[0])
    {
        rb->splash(HZ * 2, "Launch PicoDrive with a Genesis ROM");
        return PLUGIN_OK;
    }
    shared_buffer = rb->plugin_get_audio_buffer(&shared_size);
    if (!shared_buffer || shared_size < 4 * 1024 * 1024)
    {
        rb->splash(HZ * 2, "PicoDrive needs at least 4 MiB shared memory");
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    pd_arena_init(shared_buffer, shared_size);
    if (!ensure_directories())
    {
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    video_buffer = pd_calloc(LCD_WIDTH * LCD_HEIGHT, sizeof(fb_data));
    if (!video_buffer)
    {
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    set_boost(true);
    PicoInit();
    core_initialized = true;
    if (!pd_audio_init())
    {
        rb->splash(HZ, "Audio unavailable; continuing muted");
        PicoIn.sndOut = NULL;
        PicoIn.writeSound = NULL;
    }
    else
        audio_initialized = true;
    configure_core();
    if (!load_rom((const char *)parameter))
    {
        rb->splash(HZ * 2, "Could not load Genesis ROM");
        result = PLUGIN_ERROR;
        goto cleanup;
    }
    configure_core();
    load_sram();
    PicoLoopPrepare();
    achievements_start();
    pd_log("start rom=%s bytes=%lu crc=%08lx arena=%lu audio=%d",
           pd.rom_path, (unsigned long)pd.rom_size,
           (unsigned long)pd.rom_crc, (unsigned long)pd.arena_size,
           PicoIn.sndRate);
    run_loop();
    if (audio_initialized)
    {
        audio_shutdown();
        audio_initialized = false;
    }
    save_sram();
    save_config();
    rockachievements_shutdown(&achievements);
    pd_log("exit status=%d arena_used=%lu", result,
           (unsigned long)pd.arena_used);

cleanup:
    rockachievements_shutdown(&achievements);
    if (audio_initialized)
        audio_shutdown();
    if (core_initialized)
        PicoExit();
    set_boost(false);
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_clear_display();
    rb->lcd_update();
    if (shared_buffer)
        rb->plugin_release_audio_buffer();
    return pd.usb_connected ? PLUGIN_USB_CONNECTED : result;
}
