#include "cps1.h"
#include "upstream/burn/burn.h"
extern "C" {
#include <tlsf.h>
}

/* The imported core uses libc-shaped macros; the frontend uses rb directly. */
#undef memcpy
#undef memmove
#undef memset
#undef memcmp
#undef strlen
#undef strcmp
#undef strncmp
#undef strcasecmp
#undef strcpy
#undef strncpy
#undef strcat
#undef strchr
#undef strrchr
#undef strstr

#define CPS1_AUDIO_RATE 22050
#define CPS1_AUDIO_BLOCK_FRAMES 512
#define CPS1_AUDIO_BLOCKS 6
#define CPS1_AUDIO_START_BLOCKS 2
#define CPS1_AUDIO_SAMPLES (CPS1_AUDIO_BLOCK_FRAMES * 2)
#define CPS1_MAX_INPUTS 96

static int diagnostic_fd = -1;
static char missing_rom_name[64];
static uint32_t missing_rom_crc;
static size_t missing_rom_size;
static int missing_rom_index = -1;

#ifdef SIMULATOR
extern "C" char *getenv(const char *);
extern unsigned char PsndCode;
extern int bPsmOkay;
extern unsigned char BurnYM2151Registers[0x100];
extern unsigned long cps1_ym_render_samples;
extern unsigned long cps1_ym_render_nonzero;
extern unsigned long cps1_ym_keyon_writes;
extern unsigned long cps1_ym_irq_calls;
extern int ZetPc(int);
extern unsigned long cps1_z80_interrupts;
extern unsigned char cps1_z80_iff_on_assert;
#endif

struct zip_archive
{
    unsigned char *data;
    size_t size;
    char directory[MAX_PATH];
};

struct player_controls
{
    unsigned char *up;
    unsigned char *right;
    unsigned char *down;
    unsigned char *left;
    unsigned char *fire[6];
    unsigned char *start;
    unsigned char *coin;
};

static struct zip_archive game_zip;
static struct zip_archive parent_zip;
static struct player_controls p1_controls;
static fb_data *video;
static int video_width;
static int video_height;
static int16_t *frame_audio;
static int16_t *audio_ring;
static int16_t audio_silence[CPS1_AUDIO_SAMPLES] __attribute__((aligned(4)));
static volatile int audio_queued;
static volatile int audio_read;
static int audio_write;
static int audio_write_frames;
static bool audio_started;
static unsigned old_frequency;
static bool quit_requested;
static bool menu_requested;
static bool menu_combo_used;
static long menu_pressed;
static int coin_pulse_frames;
static int start_pulse_frames;
static int render_frameskip;
static unsigned long emulated_frames;
static unsigned long rendered_frames;
static long performance_start_tick;
static unsigned long core_ticks;
static unsigned long audio_ticks;
static unsigned long display_ticks;
static uint16_t horizontal_source_x[LCD_WIDTH];
static uint16_t vertical_source_x[LCD_HEIGHT];
static uint16_t vertical_source_y[LCD_WIDTH];
static int vertical_output_width;
static bool video_border_initialized;
#ifdef HAS_BUTTON_HOLD
static bool hold_exit_initialized;
static bool hold_exit_armed;
#endif
extern unsigned char CpsReset;

static uint16_t read_le16(const unsigned char *data)
{
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint32_t read_le32(const unsigned char *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static uint32_t crc32_ieee(const void *source, size_t size)
{
    const unsigned char *data = (const unsigned char *)source;
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

static bool load_file(const char *path, struct zip_archive *archive)
{
    int fd;
    off_t size;
    size_t length;
    DIR *directory;

    rb->memset(archive, 0, sizeof(*archive));
    rb->strlcpy(archive->directory, path, sizeof(archive->directory));
    length = rb->strlen(archive->directory);
    if (length > 4 &&
        !rb->strcasecmp(archive->directory + length - 4, ".zip"))
        archive->directory[length - 4] = '\0';
    directory = rb->opendir(archive->directory);
    if (directory)
    {
        rb->closedir(directory);
        return true;
    }
    archive->directory[0] = '\0';
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    size = rb->filesize(fd);
    if (size < 22 || size > 16 * 1024 * 1024)
    {
        rb->close(fd);
        return false;
    }
    archive->data = (unsigned char *)cps1_malloc((size_t)size);
    if (!archive->data ||
        rb->read(fd, archive->data, size) != size)
    {
        rb->close(fd);
        cps1_free(archive->data);
        rb->memset(archive, 0, sizeof(*archive));
        return false;
    }
    rb->close(fd);
    archive->size = (size_t)size;
    return true;
}

static bool read_direct_file(const char *path, uint32_t crc,
                             unsigned char *output, size_t output_size)
{
    int fd = rb->open(path, O_RDONLY);
    size_t complete = 0;

    if (fd < 0)
        return false;
    if (rb->filesize(fd) != (off_t)output_size)
    {
        rb->close(fd);
        return false;
    }
    while (complete < output_size)
    {
        ssize_t amount = rb->read(fd, output + complete,
                                  output_size - complete);
        if (amount <= 0)
        {
            rb->close(fd);
            return false;
        }
        complete += (size_t)amount;
    }
    rb->close(fd);
    return crc32_ieee(output, output_size) == crc;
}

static bool directory_extract(const struct zip_archive *archive,
                              uint32_t crc, const char *wanted_name,
                              unsigned char *output, size_t output_size)
{
    char path[MAX_PATH];
    DIR *directory;
    struct dirent *entry;

    if (!archive->directory[0])
        return false;
    if (wanted_name && !rb->strchr(wanted_name, '/') &&
        !rb->strchr(wanted_name, '\\'))
    {
        rb->snprintf(path, sizeof(path), "%s/%s",
                     archive->directory, wanted_name);
        if (read_direct_file(path, crc, output, output_size))
            return true;
    }
    directory = rb->opendir(archive->directory);
    if (!directory)
        return false;
    while ((entry = rb->readdir(directory)) != NULL)
    {
        struct dirinfo info = rb->dir_get_info(directory, entry);

        if ((info.attribute & ATTR_DIRECTORY) || info.size != output_size)
            continue;
        rb->snprintf(path, sizeof(path), "%s/%s",
                     archive->directory, entry->d_name);
        if (read_direct_file(path, crc, output, output_size))
        {
            rb->closedir(directory);
            return true;
        }
    }
    rb->closedir(directory);
    return false;
}

static bool zip_extract(const struct zip_archive *archive, uint32_t crc,
                        const char *wanted_name, unsigned char *output,
                        size_t output_size)
{
    size_t scan;
    size_t eocd = 0;
    uint32_t central_offset;
    uint16_t entries;
    uint16_t entry;
    const unsigned char *cursor;

    if (directory_extract(archive, crc, wanted_name, output, output_size))
        return true;
    if (!archive->data || archive->size < 22)
        return false;
    scan = archive->size > 65557 ? archive->size - 65557 : 0;
    for (size_t pos = archive->size - 22; pos >= scan; pos--)
    {
        if (read_le32(archive->data + pos) == 0x06054b50)
        {
            eocd = pos;
            break;
        }
        if (pos == 0)
            break;
    }
    if (!eocd)
        return false;
    entries = read_le16(archive->data + eocd + 10);
    central_offset = read_le32(archive->data + eocd + 16);
    if (central_offset >= archive->size)
        return false;
    cursor = archive->data + central_offset;
    for (entry = 0; entry < entries; entry++)
    {
        uint16_t method;
        uint16_t name_size;
        uint16_t extra_size;
        uint16_t comment_size;
        uint32_t member_crc;
        uint32_t compressed_size;
        uint32_t plain_size;
        uint32_t local_offset;
        const unsigned char *local;
        const unsigned char *payload;
        bool name_match;

        if ((size_t)(cursor - archive->data) + 46 > archive->size ||
            read_le32(cursor) != 0x02014b50)
            return false;
        method = read_le16(cursor + 10);
        member_crc = read_le32(cursor + 16);
        compressed_size = read_le32(cursor + 20);
        plain_size = read_le32(cursor + 24);
        name_size = read_le16(cursor + 28);
        extra_size = read_le16(cursor + 30);
        comment_size = read_le16(cursor + 32);
        local_offset = read_le32(cursor + 42);
        name_match = wanted_name &&
            rb->strlen(wanted_name) == name_size &&
            !rb->memcmp(cursor + 46, wanted_name, name_size);
        if ((member_crc == crc || name_match) && plain_size == output_size)
        {
            if ((size_t)local_offset + 30 > archive->size)
                return false;
            local = archive->data + local_offset;
            if (read_le32(local) != 0x04034b50)
                return false;
            payload = local + 30 + read_le16(local + 26) +
                      read_le16(local + 28);
            if ((size_t)(payload - archive->data) + compressed_size >
                archive->size)
                return false;
            if (method == 0)
                rb->memcpy(output, payload, output_size);
            else if (method == 8)
            {
                if (cps1_inflate_raw(output, output_size, payload,
                                     compressed_size))
                    return false;
            }
            else
                return false;
            return crc32_ieee(output, output_size) == member_crc;
        }
        cursor += 46 + name_size + extra_size + comment_size;
    }
    return false;
}

static int load_rom_callback(unsigned char *destination, int *written,
                             int index)
{
    struct BurnRomInfo info;
    char *name = NULL;

    rb->memset(&info, 0, sizeof(info));
    if (BurnDrvGetRomInfo(&info, index) ||
        BurnDrvGetRomName(&name, index, 0))
        return 1;
    if (!zip_extract(&game_zip, info.nCrc, name, destination, info.nLen) &&
        !zip_extract(&parent_zip, info.nCrc, name, destination, info.nLen))
    {
        rb->strlcpy(missing_rom_name, name ? name : "?",
                    sizeof(missing_rom_name));
        missing_rom_crc = info.nCrc;
        missing_rom_size = info.nLen;
        missing_rom_index = index;
        if (diagnostic_fd >= 0)
            rb->fdprintf(diagnostic_fd,
                         "missing index=%d name=%s crc=%08lx size=%lu\n",
                         index, missing_rom_name,
                         (unsigned long)missing_rom_crc,
                         (unsigned long)missing_rom_size);
        return 1;
    }
    if (written)
        *written = info.nLen;
    return 0;
}

static unsigned int high_color(int red, int green, int blue, int)
{
    return LCD_RGBPACK(red, green, blue);
}

static int core_log(int, char *format, ...)
{
    (void)format;
    return 0;
}

static void init_inputs(void)
{
    struct BurnInputInfo info;
    struct BurnDIPInfo dip;
    int offset = 0;
    int index;

    rb->memset(&p1_controls, 0, sizeof(p1_controls));
    for (index = 0; index < CPS1_MAX_INPUTS &&
         !BurnDrvGetInputInfo(&info, index); index++)
    {
        if (info.nType != BIT_DIPSWITCH && info.pVal)
        {
            const char *name = info.szInfo ? info.szInfo : "";

            if (!rb->strcmp(name, "p1 up"))
                p1_controls.up = info.pVal;
            else if (!rb->strcmp(name, "p1 right"))
                p1_controls.right = info.pVal;
            else if (!rb->strcmp(name, "p1 down"))
                p1_controls.down = info.pVal;
            else if (!rb->strcmp(name, "p1 left"))
                p1_controls.left = info.pVal;
            else if (!rb->strcmp(name, "p1 fire 1"))
                p1_controls.fire[0] = info.pVal;
            else if (!rb->strcmp(name, "p1 fire 2"))
                p1_controls.fire[1] = info.pVal;
            else if (!rb->strcmp(name, "p1 fire 3"))
                p1_controls.fire[2] = info.pVal;
            else if (!rb->strcmp(name, "p1 fire 4"))
                p1_controls.fire[3] = info.pVal;
            else if (!rb->strcmp(name, "p1 fire 5"))
                p1_controls.fire[4] = info.pVal;
            else if (!rb->strcmp(name, "p1 fire 6"))
                p1_controls.fire[5] = info.pVal;
            else if (!rb->strcmp(name, "p1 start"))
                p1_controls.start = info.pVal;
            else if (!rb->strcmp(name, "p1 coin"))
                p1_controls.coin = info.pVal;
        }
    }
    for (index = 0; !BurnDrvGetDIPInfo(&dip, index); index++)
    {
        if (dip.nFlags == 0xf0)
        {
            offset = dip.nInput;
            break;
        }
    }
    for (index = 0; !BurnDrvGetDIPInfo(&dip, index); index++)
    {
        int input_index = dip.nInput + offset;

        if (dip.nFlags == 0xff &&
            !BurnDrvGetInputInfo(&info, input_index) && info.pVal)
        {
            *info.pVal = (*info.pVal & ~dip.nMask) |
                         (dip.nSetting & dip.nMask);
        }
    }
}

static inline void set_control(unsigned char *control, bool down)
{
    if (control)
        *control = down ? 1 : 0;
}

static unsigned wheel_direction(void)
{
#ifdef HAVE_WHEEL_POSITION
    int position = rb->wheel_status();
    int zone;

    if (position < 0)
        return 0;
    zone = ((position + 6) / 12) & 7;
    return 1u << zone;
#else
    return 0;
#endif
}

static void poll_inputs(void)
{
    int held = rb->button_status();
    int event = rb->button_get(false);
    unsigned direction = wheel_direction();
    bool menu;
    bool start;
    bool coin;
    bool fire5;
    bool fire6;

    menu = false;
#ifdef BUTTON_MENU
    menu = (held & BUTTON_MENU) != 0;
#endif
    start = menu;
    coin = menu;
    fire5 = menu;
    fire6 = menu;
#ifdef BUTTON_SELECT
    start = start && (held & BUTTON_SELECT);
#endif
#ifdef BUTTON_PLAY
    coin = coin && (held & BUTTON_PLAY);
#endif
#ifdef BUTTON_LEFT
    fire5 = fire5 && (held & BUTTON_LEFT);
#endif
#ifdef BUTTON_RIGHT
    fire6 = fire6 && (held & BUTTON_RIGHT);
#endif
    start = start || start_pulse_frames > 0;
    coin = coin || coin_pulse_frames > 0;

    set_control(p1_controls.up,
                direction & ((1u << 7) | 1u | (1u << 1)));
    set_control(p1_controls.right,
                direction & ((1u << 1) | (1u << 2) | (1u << 3)));
    set_control(p1_controls.down,
                direction & ((1u << 3) | (1u << 4) | (1u << 5)));
    set_control(p1_controls.left,
                direction & ((1u << 5) | (1u << 6) | (1u << 7)));
#ifdef BUTTON_SELECT
    set_control(p1_controls.fire[0], (held & BUTTON_SELECT) && !start);
#endif
#ifdef BUTTON_PLAY
    set_control(p1_controls.fire[1], (held & BUTTON_PLAY) && !coin);
#endif
#ifdef BUTTON_LEFT
    set_control(p1_controls.fire[2], (held & BUTTON_LEFT) && !fire5);
    set_control(p1_controls.fire[4], fire5);
#endif
#ifdef BUTTON_RIGHT
    set_control(p1_controls.fire[3], (held & BUTTON_RIGHT) && !fire6);
    set_control(p1_controls.fire[5], fire6);
#endif
    set_control(p1_controls.start, start);
    set_control(p1_controls.coin, coin);
    if (start_pulse_frames > 0)
        start_pulse_frames--;
    if (coin_pulse_frames > 0)
        coin_pulse_frames--;

#ifdef BUTTON_MENU
    if (held & BUTTON_MENU)
    {
        if (!menu_pressed)
        {
            menu_pressed = *rb->current_tick;
            menu_combo_used = false;
        }
        if (start || coin || fire5 || fire6)
            menu_combo_used = true;
        if (!menu_combo_used && *rb->current_tick - menu_pressed > HZ * 2)
            quit_requested = true;
    }
    else
    {
        if (menu_pressed && !menu_combo_used &&
            *rb->current_tick - menu_pressed <= HZ * 2)
            menu_requested = true;
        menu_pressed = 0;
    }
#endif
#ifdef HAS_BUTTON_HOLD
    {
        bool hold_now = rb->button_hold();
        if (!hold_exit_initialized)
        {
            hold_exit_initialized = true;
            hold_exit_armed = hold_now;
        }
        else if (hold_now && !hold_exit_armed)
        {
            hold_exit_armed = true;
            quit_requested = true;
        }
        else
            hold_exit_armed = hold_now;
    }
#endif
    if (event == SYS_USB_CONNECTED)
        quit_requested = true;
}

static void set_cpu_boost(bool enabled)
{
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(enabled);
#else
    (void)enabled;
#endif
}

static void pause_audio(void)
{
    rb->pcm_play_lock();
    if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) != CHANNEL_STOPPED)
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcm_play_unlock();
    audio_started = false;
    audio_queued = audio_read = audio_write = audio_write_frames = 0;
}

static void pause_menu(void)
{
    static const char *items[] = {
        "Resume", "Insert coin", "Start game", "Controls",
        "Render skip", "Reset game", "Exit game"
    };
    const int item_count = ARRAYLEN(items);
    int selected = 0;
    bool redraw = true;

    while (!quit_requested)
    {
        int button;
        int clean;

        if (redraw)
        {
            rb->lcd_clear_display();
            rb->lcd_puts(0, 0, (const unsigned char *)"CPS1 Arcade");
            for (int index = 0; index < item_count; index++)
            {
                char label[40];

                if (index == 4)
                {
                    rb->snprintf(label, sizeof(label), "Render skip: %d",
                                 render_frameskip);
                    rb->lcd_puts(0, index + 2,
                                 (const unsigned char *)label);
                }
                else
                    rb->lcd_puts(0, index + 2,
                                 (const unsigned char *)items[index]);
            }
            rb->lcd_set_drawmode(DRMODE_COMPLEMENT);
            rb->lcd_fillrect(0, (selected + 2) * rb->font_get(
                                 FONT_UI)->height,
                             LCD_WIDTH, rb->font_get(FONT_UI)->height);
            rb->lcd_set_drawmode(DRMODE_SOLID);
            rb->lcd_update();
            redraw = false;
        }
        button = rb->button_get_w_tmo(HZ / 10);
        if (button == BUTTON_NONE)
            continue;
        if (button == SYS_USB_CONNECTED)
        {
            quit_requested = true;
            break;
        }
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
#ifdef BUTTON_MENU
        if (clean == BUTTON_MENU)
            break;
#endif
#ifdef BUTTON_SCROLL_BACK
        if (clean == BUTTON_SCROLL_BACK)
        {
            selected = (selected + item_count - 1) % item_count;
            redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_SCROLL_FWD
        if (clean == BUTTON_SCROLL_FWD)
        {
            selected = (selected + 1) % item_count;
            redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_SELECT
        if (clean != BUTTON_SELECT)
            continue;
#endif
        if (selected == 0)
            break;
        if (selected == 1)
        {
            coin_pulse_frames = 4;
            break;
        }
        else if (selected == 2)
        {
            start_pulse_frames = 4;
            break;
        }
        else if (selected == 3)
            rb->splash(HZ * 3,
                       "Wheel: move  Select/Play/Prev/Next: attacks  "
                       "Pause menu: coin/start");
        else if (selected == 4)
            render_frameskip = (render_frameskip + 1) % 5;
        else if (selected == 5)
        {
            CpsReset = 1;
            break;
        }
        else
        {
            quit_requested = true;
            break;
        }
        redraw = true;
    }
    rb->button_clear_queue();
}

static void audio_get_more(const void **start, size_t *size)
{
    if (audio_queued > 0 && audio_ring)
    {
        *start = audio_ring + audio_read * CPS1_AUDIO_SAMPLES;
        *size = CPS1_AUDIO_SAMPLES * sizeof(int16_t);
        audio_read = (audio_read + 1) % CPS1_AUDIO_BLOCKS;
        audio_queued--;
    }
    else
    {
        *start = audio_silence;
        *size = sizeof(audio_silence);
    }
}

static void audio_submit(const int16_t *data, int frames)
{
    while (frames > 0)
    {
        int room = CPS1_AUDIO_BLOCK_FRAMES - audio_write_frames;
        int take = MIN(frames, room);
        int16_t *output = audio_ring + audio_write * CPS1_AUDIO_SAMPLES +
                          audio_write_frames * 2;

        rb->memcpy(output, data, take * 2 * sizeof(*data));
        data += take * 2;
        frames -= take;
        audio_write_frames += take;
        if (audio_write_frames == CPS1_AUDIO_BLOCK_FRAMES)
        {
            rb->pcm_play_lock();
            if (audio_queued < CPS1_AUDIO_BLOCKS - 1)
            {
                audio_queued++;
                audio_write = (audio_write + 1) % CPS1_AUDIO_BLOCKS;
            }
            rb->pcm_play_unlock();
            audio_write_frames = 0;
            if (!audio_started && audio_queued >= CPS1_AUDIO_START_BLOCKS)
            {
                rb->mixer_channel_set_amplitude(
                    PCM_MIXER_CHAN_PLAYBACK, MIX_AMP_UNITY);
                rb->mixer_channel_play_data(
                    PCM_MIXER_CHAN_PLAYBACK, audio_get_more, NULL, 0);
                audio_started = true;
            }
        }
    }
}

static fb_data *main_framebuffer(void)
{
    struct viewport *viewport = *(rb->screens[SCREEN_MAIN]->current_viewport);

    return viewport->buffer->fb_ptr;
}

static void prepare_video_scaler(void)
{
    vertical_output_width = video_height * LCD_HEIGHT / video_width;
    for (int x = 0; x < LCD_WIDTH; x++)
        horizontal_source_x[x] = x * video_width / LCD_WIDTH;
    for (int y = 0; y < LCD_HEIGHT; y++)
        vertical_source_x[y] = y * video_width / LCD_HEIGHT;
    for (int x = 0; x < vertical_output_width; x++)
        vertical_source_y[x] = (vertical_output_width - 1 - x) *
                               video_height / vertical_output_width;
    video_border_initialized = false;
}

static void draw_frame(void)
{
    fb_data *framebuffer = main_framebuffer();
    bool vertical = (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) != 0;

    if (!video_border_initialized)
    {
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_clear_display();
        video_border_initialized = true;
    }
    if (!vertical)
    {
        int top = (LCD_HEIGHT - video_height) / 2;

        for (int y = 0; y < video_height && y + top < LCD_HEIGHT; y++)
        {
            const fb_data *source = video + y * video_width;
            fb_data *destination = framebuffer + (y + top) * LCD_WIDTH;

            if (video_width == 384 && LCD_WIDTH == 320)
            {
                uint32_t *packed_destination = (uint32_t *)destination;

                /* Drop every sixth source pixel. Two groups produce ten
                 * aligned destination pixels, allowing five word stores. */
                for (int group = 0; group < 32; group++)
                {
                    packed_destination[0] =
                        (uint32_t)source[0] | (uint32_t)source[1] << 16;
                    packed_destination[1] =
                        (uint32_t)source[2] | (uint32_t)source[3] << 16;
                    packed_destination[2] =
                        (uint32_t)source[4] | (uint32_t)source[6] << 16;
                    packed_destination[3] =
                        (uint32_t)source[7] | (uint32_t)source[8] << 16;
                    packed_destination[4] =
                        (uint32_t)source[9] | (uint32_t)source[10] << 16;
                    packed_destination += 5;
                    source += 12;
                }
            }
            else
                for (int x = 0; x < LCD_WIDTH; x++)
                    destination[x] = source[horizontal_source_x[x]];
        }
    }
    else
    {
        int left = (LCD_WIDTH - vertical_output_width) / 2;

        for (int y = 0; y < LCD_HEIGHT; y++)
        {
            fb_data *destination = framebuffer + y * LCD_WIDTH + left;
            int source_x = vertical_source_x[y];

            for (int x = 0; x < vertical_output_width; x++)
                destination[x] =
                    video[vertical_source_y[x] * video_width + source_x];
        }
    }
    if (vertical)
        rb->lcd_update_rect((LCD_WIDTH - vertical_output_width) / 2, 0,
                            vertical_output_width, LCD_HEIGHT);
    else
        rb->lcd_update_rect(0, (LCD_HEIGHT - video_height) / 2,
                            LCD_WIDTH, MIN(video_height, LCD_HEIGHT));
    rendered_frames++;
}

static bool prepare_parent(const char *rom_path)
{
    const char *parent = BurnDrvGetTextA(DRV_PARENT);
    char path[MAX_PATH];
    char *slash;

    if (!parent || !parent[0])
        return true;
    rb->strlcpy(path, rom_path, sizeof(path));
    slash = rb->strrchr(path, '/');
    if (!slash)
        return false;
    rb->snprintf(slash + 1, sizeof(path) - (slash + 1 - path),
                 "%s.zip", parent);
    return load_file(path, &parent_zip);
}

static bool select_driver(const char *rom_path)
{
    const char *name = rb->strrchr(rom_path, '/');
    char stem[40];
    const char *extension;
    size_t length;

    name = name ? name + 1 : rom_path;
    extension = rb->strrchr(name, '.');
    length = extension ? (size_t)(extension - name) : rb->strlen(name);
    if (length >= sizeof(stem))
        return false;
    rb->memcpy(stem, name, length);
    stem[length] = '\0';
    for (nBurnDrvSelect = 0; nBurnDrvSelect < nBurnDrvCount;
         nBurnDrvSelect++)
    {
        if (!rb->strcasecmp(stem, BurnDrvGetTextA(DRV_NAME)))
            return true;
    }
    return false;
}

enum plugin_status plugin_start(const void *parameter)
{
    size_t pool_size;
    void *pool;
    bool audio_buffer_claimed = false;
    bool pool_initialized = false;
    bool core_library_started = false;
    bool driver_started = false;
    int driver_init_result = 1;
    long frame_deadline = 0;
    unsigned frame_tick_fraction = 0;
    enum plugin_status status = PLUGIN_ERROR;
#ifdef SIMULATOR
    unsigned long test_frames = 0;
    unsigned long test_limit = 0;
    uint32_t test_frame_crc = 0;
    unsigned long test_video_nonzero = 0;
    unsigned long test_audio_nonzero = 0;
    int test_psm = 0;
    int test_z80_pc = 0;
    const char *test_value = getenv("CPS1_TEST_FRAMES");

    if (test_value)
        test_limit = (unsigned long)rb->atoi(test_value);
#endif

    if (!parameter || !rb->strrchr((const char *)parameter, '.'))
    {
        rb->splash(HZ * 2, "Open a CPS1 .zip game");
        return PLUGIN_ERROR;
    }
    rb->mkdir(ROCKBOX_DIR "/logs");
    diagnostic_fd = rb->open(ROCKBOX_DIR "/logs/cps1.log",
                             O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (diagnostic_fd >= 0)
        rb->fdprintf(diagnostic_fd, "launch path=%s\n",
                     (const char *)parameter);
    pool = rb->plugin_get_audio_buffer(&pool_size);
    audio_buffer_claimed = pool != NULL;
    if (!pool || init_memory_pool(pool_size, pool) == (size_t)-1)
    {
        if (diagnostic_fd >= 0)
            rb->fdprintf(diagnostic_fd,
                         "audio pool unavailable size=%lu pointer=%p\n",
                         (unsigned long)pool_size, pool);
        rb->splash(HZ * 2, "Not enough CPS1 memory");
        goto cleanup;
    }
    pool_initialized = true;
    cps1_platform_reset_failure();
    missing_rom_name[0] = '\0';
    missing_rom_crc = 0;
    missing_rom_size = 0;
    missing_rom_index = -1;
    old_frequency = 0;
    rb->memset(&game_zip, 0, sizeof(game_zip));
    rb->memset(&parent_zip, 0, sizeof(parent_zip));
    if (!load_file((const char *)parameter, &game_zip))
    {
        rb->splash(HZ * 2, "Cannot read CPS1 archive");
        goto cleanup;
    }
    BurnLibInit();
    core_library_started = true;
    if (!select_driver((const char *)parameter) ||
        !prepare_parent((const char *)parameter))
    {
        rb->splash(HZ * 2, "Unsupported CPS1 archive");
        goto cleanup;
    }
    if (diagnostic_fd >= 0)
        rb->fdprintf(diagnostic_fd,
                     "driver=%s parent=%s pool=%lu game_zip=%lu "
                     "parent_zip=%lu\n",
                     BurnDrvGetTextA(DRV_NAME),
                     BurnDrvGetTextA(DRV_PARENT) ?
                     BurnDrvGetTextA(DRV_PARENT) : "-",
                     (unsigned long)pool_size,
                     (unsigned long)game_zip.size,
                     (unsigned long)parent_zip.size);
    BurnExtLoadRom = load_rom_callback;
    BurnHighCol = high_color;
    bprintf = core_log;
    nBurnBpp = sizeof(fb_data);
    BurnDrvGetFullSize(&video_width, &video_height);
    nBurnPitch = video_width * sizeof(fb_data);
    video = (fb_data *)cps1_calloc(
        (size_t)video_width * video_height, sizeof(*video));
    pBurnDraw = (unsigned char *)video;
    old_frequency = rb->mixer_get_frequency();
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(CPS1_AUDIO_RATE);
    nBurnSoundRate = rb->mixer_get_frequency();
    if (!nBurnSoundRate)
        nBurnSoundRate = CPS1_AUDIO_RATE;
    nBurnSoundLen = nBurnSoundRate * 100 / nBurnFPS;
    frame_audio = (int16_t *)cps1_calloc(
        (size_t)nBurnSoundLen * 2, sizeof(*frame_audio));
    audio_ring = (int16_t *)cps1_calloc(
        CPS1_AUDIO_BLOCKS * CPS1_AUDIO_SAMPLES, sizeof(*audio_ring));
    pBurnSoundOut = frame_audio;
    if (video && frame_audio && audio_ring)
        driver_init_result = BurnDrvInit();
    if (!video || !frame_audio || !audio_ring || driver_init_result)
    {
        if (diagnostic_fd >= 0)
            rb->fdprintf(diagnostic_fd,
                         "init failed result=%d allocation_failed=%d "
                         "last_allocation=%lu used=%lu max=%lu "
                         "missing_index=%d missing_name=%s "
                         "missing_crc=%08lx missing_size=%lu\n",
                         driver_init_result,
                         cps1_platform_allocation_failed() ? 1 : 0,
                         (unsigned long)cps1_platform_last_allocation_size(),
                         (unsigned long)get_used_size(pool),
                         (unsigned long)get_max_size(pool),
                         missing_rom_index,
                         missing_rom_name[0] ? missing_rom_name : "-",
                         (unsigned long)missing_rom_crc,
                         (unsigned long)missing_rom_size);
        if (cps1_platform_allocation_failed())
            rb->splash(HZ * 3, "CPS1 ran out of memory");
        else if (missing_rom_name[0])
            rb->splashf(HZ * 3, "Missing CPS1 ROM: %s",
                        missing_rom_name);
        else
            rb->splash(HZ * 3, "CPS1 core initialization failed");
        goto cleanup;
    }
    driver_started = true;
    prepare_video_scaler();
#ifdef SIMULATOR
    if (diagnostic_fd >= 0)
        rb->fdprintf(diagnostic_fd,
                     "init driver=%s parent=%s video=%dx%d sound=%d pool=%lu\n",
                     BurnDrvGetTextA(DRV_NAME),
                     BurnDrvGetTextA(DRV_PARENT) ?
                     BurnDrvGetTextA(DRV_PARENT) : "-",
                     video_width, video_height, nBurnSoundRate,
                     (unsigned long)pool_size);
#endif
    init_inputs();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    rb->button_clear_queue();
    quit_requested = false;
    menu_requested = false;
    menu_combo_used = false;
    menu_pressed = 0;
    coin_pulse_frames = 0;
    start_pulse_frames = 0;
#ifdef SIMULATOR
    render_frameskip = 0;
#else
    render_frameskip = 2;
#endif
    emulated_frames = 0;
    rendered_frames = 0;
    core_ticks = 0;
    audio_ticks = 0;
    display_ticks = 0;
#ifdef HAS_BUTTON_HOLD
    hold_exit_initialized = false;
    hold_exit_armed = false;
#endif
    audio_queued = audio_read = audio_write = audio_write_frames = 0;
    audio_started = false;
    rb->splash(HZ, "Menu: pause, coin and start");
    performance_start_tick = *rb->current_tick;
    frame_deadline = performance_start_tick;
    frame_tick_fraction = 0;
    set_cpu_boost(true);
    while (!quit_requested)
    {
        long stage_tick;
        bool render_this_frame =
            emulated_frames % (unsigned)(render_frameskip + 1) == 0;

        frame_tick_fraction += HZ * 100;
        frame_deadline += frame_tick_fraction / (unsigned)nBurnFPS;
        frame_tick_fraction %= (unsigned)nBurnFPS;
        poll_inputs();
#ifdef SIMULATOR
        if (getenv("CPS1_TEST_AUTOSTART"))
        {
            set_control(p1_controls.coin,
                        test_frames == 120 || test_frames == 122 ||
                        (test_frames >= 300 && (test_frames % 180) < 2));
            set_control(p1_controls.start,
                        test_frames == 180 || test_frames == 182 ||
                        (test_frames >= 330 &&
                         (test_frames % 180) >= 30 &&
                         (test_frames % 180) < 32));
            set_control(p1_controls.fire[0],
                        test_frames > 240 && (test_frames % 45) == 0);
            if (test_frames >= 240 && (test_frames % 120) == 0)
                PsndCode = (unsigned char)(1 + (test_frames / 120) % 0x40);
        }
#endif
        rb->memset(frame_audio, 0,
                   (size_t)nBurnSoundLen * 2 * sizeof(*frame_audio));
        pBurnDraw = render_this_frame ? (unsigned char *)video : NULL;
        stage_tick = *rb->current_tick;
        if (BurnDrvFrame())
            break;
        core_ticks += *rb->current_tick - stage_tick;
        emulated_frames++;
#ifdef SIMULATOR
        for (int sample = 0; sample < nBurnSoundLen * 2; sample++)
            if (frame_audio[sample])
                test_audio_nonzero++;
#endif
        stage_tick = *rb->current_tick;
        audio_submit(frame_audio, nBurnSoundLen);
        audio_ticks += *rb->current_tick - stage_tick;
        if (render_this_frame)
        {
            stage_tick = *rb->current_tick;
            draw_frame();
            display_ticks += *rb->current_tick - stage_tick;
        }
#ifdef SIMULATOR
        test_frames++;
        if (test_limit && test_frames >= test_limit)
            quit_requested = true;
        if (!getenv("CPS1_TEST_UNTHROTTLED") &&
            TIME_BEFORE(*rb->current_tick, frame_deadline))
            rb->sleep(frame_deadline - *rb->current_tick);
#else
        if (TIME_BEFORE(*rb->current_tick, frame_deadline))
            rb->sleep(frame_deadline - *rb->current_tick);
#endif
        if (TIME_AFTER(*rb->current_tick, frame_deadline + HZ / 2))
        {
            frame_deadline = *rb->current_tick;
            frame_tick_fraction = 0;
        }
        rb->yield();
        if (menu_requested)
        {
            menu_requested = false;
            pause_audio();
            set_cpu_boost(false);
            pause_menu();
            video_border_initialized = false;
            frame_deadline = *rb->current_tick;
            frame_tick_fraction = 0;
            set_cpu_boost(true);
        }
    }
    status = PLUGIN_OK;

cleanup:
#ifdef SIMULATOR
    test_psm = bPsmOkay;
    if (driver_started)
        test_z80_pc = ZetPc(-1);
    if (video)
    {
        test_frame_crc = rb->crc_32(
            video,
            (uint32_t)((size_t)video_width * video_height * sizeof(*video)),
            0xffffffff);
        for (int pixel = 0; pixel < video_width * video_height; pixel++)
            if (video[pixel])
                test_video_nonzero++;
    }
#endif
    pause_audio();
    set_cpu_boost(false);
    if (driver_started)
        BurnDrvExit();
    BurnExtLoadRom = NULL;
    pBurnDraw = NULL;
    pBurnSoundOut = NULL;
    if (old_frequency)
        rb->mixer_set_frequency(old_frequency);
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    if (core_library_started)
        BurnLibExit();
    if (pool_initialized)
        destroy_memory_pool(pool);
    if (audio_buffer_claimed)
        rb->plugin_release_audio_buffer();
#ifdef SIMULATOR
    if (diagnostic_fd >= 0)
    {
        unsigned long ym_registers = 0;
        for (int reg = 0; reg < 0x100; reg++)
            if (BurnYM2151Registers[reg])
                ym_registers++;
        rb->fdprintf(diagnostic_fd,
                     "exit status=%d initialized=%d frames=%lu "
                     "frame_crc=%08lx video_nonzero=%lu audio_nonzero=%lu "
                     "allocation_failed=%d z80_pc=%04x sound_code=%02x "
                     "psm=%d ym_registers=%lu ym_key=%02x ym_ch0=%02x "
                     "ym_freq=%02x/%02x ym_timer=%02x/%02x/%02x ctl=%02x "
                     "ym_irqs=%lu z80_irqs=%lu z80_iff=%u "
                     "ym_keyons=%lu ym_samples=%lu "
                     "ym_nonzero=%lu\n",
                     status, driver_started ? 1 : 0, test_frames,
                     (unsigned long)test_frame_crc,
                     test_video_nonzero, test_audio_nonzero,
                     cps1_platform_allocation_failed() ? 1 : 0,
                     test_z80_pc, PsndCode,
                     test_psm, ym_registers, BurnYM2151Registers[0x08],
                     BurnYM2151Registers[0x20], BurnYM2151Registers[0x28],
                     BurnYM2151Registers[0x30], BurnYM2151Registers[0x10],
                     BurnYM2151Registers[0x11], BurnYM2151Registers[0x12],
                     BurnYM2151Registers[0x14], cps1_ym_irq_calls,
                     cps1_z80_interrupts, cps1_z80_iff_on_assert,
                     cps1_ym_keyon_writes,
                     cps1_ym_render_samples,
                     cps1_ym_render_nonzero);
    }
#endif
    if (diagnostic_fd >= 0)
    {
        long performance_ticks = *rb->current_tick - performance_start_tick;
        unsigned long emulated_fps = performance_ticks > 0 ?
            emulated_frames * HZ / (unsigned long)performance_ticks : 0;
        unsigned long display_fps = performance_ticks > 0 ?
            rendered_frames * HZ / (unsigned long)performance_ticks : 0;

        rb->fdprintf(diagnostic_fd,
                     "cleanup status=%d initialized=%d "
                     "allocation_failed=%d last_allocation=%lu "
                     "frames=%lu rendered=%lu emulated_fps=%lu "
                     "display_fps=%lu render_skip=%d "
                     "core_ticks=%lu audio_ticks=%lu display_ticks=%lu\n",
                     status, driver_started ? 1 : 0,
                     cps1_platform_allocation_failed() ? 1 : 0,
                     (unsigned long)cps1_platform_last_allocation_size(),
                     emulated_frames, rendered_frames, emulated_fps,
                     display_fps, render_frameskip,
                     core_ticks, audio_ticks, display_ticks);
        rb->close(diagnostic_fd);
        diagnostic_fd = -1;
    }
    return status;
}
