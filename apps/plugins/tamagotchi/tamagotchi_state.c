#include "tamagotchi_state.h"
#include "upstream/tamalib/tamalib.h"

#define TAMA_STATE_MAGIC 0x54414d41u
#define TAMA_STATE_VERSION 2u

struct state_file_header
{
    unsigned magic;
    unsigned version;
    unsigned payload_size;
};

void tamagotchi_settings_default(struct tamagotchi_settings *settings)
{
    rb->memset(settings, 0, sizeof(*settings));
    settings->control_mode = TAMA_CONTROL_STOCK_IPOD;
    settings->display_mode = TAMA_DISPLAY_MINIMAL;
    settings->clock_mode = TAMA_CLOCK_REAL_TIME;
    settings->wheel_sensitivity = 1;
    settings->wheel_haptic_ticks = true;
    settings->attention_haptics = true;
    settings->haptics_enabled = true;
    settings->beep_enabled = false;
    settings->quiet_mode = false;
    settings->notifications_enabled = true;
}

static void parse_bool(char *value, bool *out)
{
    *out = rb->atoi(value) != 0;
}

void tamagotchi_config_load(struct tamagotchi_settings *settings)
{
    int fd = rb->open(TAMAGOTCHI_CONFIG_PATH, O_RDONLY);
    char line[96];

    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *name;
        char *value;

        if (!rb->settings_parseline(line, &name, &value))
            continue;

        if (rb->strcmp(name, "control_mode") == 0)
            settings->control_mode = rb->atoi(value);
        else if (rb->strcmp(name, "display_mode") == 0)
            settings->display_mode = rb->atoi(value);
        else if (rb->strcmp(name, "clock_mode") == 0)
            settings->clock_mode = rb->atoi(value);
        else if (rb->strcmp(name, "wheel_sensitivity") == 0)
            settings->wheel_sensitivity = rb->atoi(value);
        else if (rb->strcmp(name, "wheel_haptic_ticks") == 0)
            parse_bool(value, &settings->wheel_haptic_ticks);
        else if (rb->strcmp(name, "attention_haptics") == 0)
            parse_bool(value, &settings->attention_haptics);
        else if (rb->strcmp(name, "haptics_enabled") == 0)
            parse_bool(value, &settings->haptics_enabled);
        else if (rb->strcmp(name, "beep_enabled") == 0)
            parse_bool(value, &settings->beep_enabled);
        else if (rb->strcmp(name, "quiet_mode") == 0)
            parse_bool(value, &settings->quiet_mode);
        else if (rb->strcmp(name, "notifications_enabled") == 0)
            parse_bool(value, &settings->notifications_enabled);
        else if (rb->strcmp(name, "auto_clock_done") == 0)
            parse_bool(value, &settings->auto_clock_done);
    }

    rb->close(fd);

    if ((int)settings->display_mode > (int)TAMA_DISPLAY_MINIMAL)
        settings->display_mode = TAMA_DISPLAY_MINIMAL;
    if ((int)settings->control_mode > (int)TAMA_CONTROL_RAW_ABC)
        settings->control_mode = TAMA_CONTROL_STOCK_IPOD;
    if (settings->wheel_sensitivity < 0 || settings->wheel_sensitivity > 2)
        settings->wheel_sensitivity = 1;

    settings->clock_mode = TAMA_CLOCK_REAL_TIME;
}

void tamagotchi_config_save(const struct tamagotchi_settings *settings)
{
    int fd = rb->open(TAMAGOTCHI_CONFIG_PATH,
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        return;

    rb->fdprintf(fd, "control_mode: %d\n", settings->control_mode);
    rb->fdprintf(fd, "display_mode: %d\n", settings->display_mode);
    rb->fdprintf(fd, "clock_mode: %d\n", settings->clock_mode);
    rb->fdprintf(fd, "wheel_sensitivity: %d\n", settings->wheel_sensitivity);
    rb->fdprintf(fd, "wheel_haptic_ticks: %d\n",
                 settings->wheel_haptic_ticks ? 1 : 0);
    rb->fdprintf(fd, "attention_haptics: %d\n",
                 settings->attention_haptics ? 1 : 0);
    rb->fdprintf(fd, "haptics_enabled: %d\n",
                 settings->haptics_enabled ? 1 : 0);
    rb->fdprintf(fd, "beep_enabled: %d\n", settings->beep_enabled ? 1 : 0);
    rb->fdprintf(fd, "quiet_mode: %d\n", settings->quiet_mode ? 1 : 0);
    rb->fdprintf(fd, "notifications_enabled: %d\n",
                 settings->notifications_enabled ? 1 : 0);
    rb->fdprintf(fd, "auto_clock_done: %d\n",
                 settings->auto_clock_done ? 1 : 0);
    rb->close(fd);
}

static unsigned payload_size(void)
{
    return sizeof(u13_t) + sizeof(u12_t) + sizeof(u12_t) +
           sizeof(u4_t) + sizeof(u4_t) + sizeof(u5_t) + sizeof(u8_t) +
           sizeof(u4_t) + sizeof(u32_t) * 10 + sizeof(bool_t) +
           sizeof(u8_t) + sizeof(u8_t) + sizeof(u32_t) +
           sizeof(interrupt_t) * INT_SLOT_NUM + sizeof(bool_t) +
           MEM_BUFFER_SIZE;
}

static bool write_field(int fd, const void *ptr, size_t size)
{
    return rb->write(fd, ptr, size) == (ssize_t)size;
}

static bool read_field(int fd, void *ptr, size_t size)
{
    return rb->read(fd, ptr, size) == (ssize_t)size;
}

bool tamagotchi_state_save(void)
{
    state_t *state = tamalib_get_state();
    struct state_file_header header;
    int fd;

    if (state == NULL)
        return false;

    fd = rb->open(TAMAGOTCHI_STATE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    header.magic = TAMA_STATE_MAGIC;
    header.version = TAMA_STATE_VERSION;
    header.payload_size = payload_size();

    if (!write_field(fd, &header, sizeof(header)) ||
        !write_field(fd, state->pc, sizeof(u13_t)) ||
        !write_field(fd, state->x, sizeof(u12_t)) ||
        !write_field(fd, state->y, sizeof(u12_t)) ||
        !write_field(fd, state->a, sizeof(u4_t)) ||
        !write_field(fd, state->b, sizeof(u4_t)) ||
        !write_field(fd, state->np, sizeof(u5_t)) ||
        !write_field(fd, state->sp, sizeof(u8_t)) ||
        !write_field(fd, state->flags, sizeof(u4_t)) ||
        !write_field(fd, state->tick_counter, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_2hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_4hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_8hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_16hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_32hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_64hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_128hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->clk_timer_256hz_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->prog_timer_timestamp, sizeof(u32_t)) ||
        !write_field(fd, state->prog_timer_enabled, sizeof(bool_t)) ||
        !write_field(fd, state->prog_timer_data, sizeof(u8_t)) ||
        !write_field(fd, state->prog_timer_rld, sizeof(u8_t)) ||
        !write_field(fd, state->call_depth, sizeof(u32_t)) ||
        !write_field(fd, state->interrupts,
                     sizeof(interrupt_t) * INT_SLOT_NUM) ||
        !write_field(fd, state->cpu_halted, sizeof(bool_t)) ||
        !write_field(fd, state->memory, MEM_BUFFER_SIZE))
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

bool tamagotchi_state_load(void)
{
    state_t *state = tamalib_get_state();
    struct state_file_header header;
    int fd;

    if (state == NULL)
        return false;

    fd = rb->open(TAMAGOTCHI_STATE_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    if (!read_field(fd, &header, sizeof(header)) ||
        header.magic != TAMA_STATE_MAGIC ||
        header.version != TAMA_STATE_VERSION ||
        header.payload_size != payload_size())
    {
        rb->close(fd);
        return false;
    }

    if (!read_field(fd, state->pc, sizeof(u13_t)) ||
        !read_field(fd, state->x, sizeof(u12_t)) ||
        !read_field(fd, state->y, sizeof(u12_t)) ||
        !read_field(fd, state->a, sizeof(u4_t)) ||
        !read_field(fd, state->b, sizeof(u4_t)) ||
        !read_field(fd, state->np, sizeof(u5_t)) ||
        !read_field(fd, state->sp, sizeof(u8_t)) ||
        !read_field(fd, state->flags, sizeof(u4_t)) ||
        !read_field(fd, state->tick_counter, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_2hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_4hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_8hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_16hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_32hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_64hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_128hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->clk_timer_256hz_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->prog_timer_timestamp, sizeof(u32_t)) ||
        !read_field(fd, state->prog_timer_enabled, sizeof(bool_t)) ||
        !read_field(fd, state->prog_timer_data, sizeof(u8_t)) ||
        !read_field(fd, state->prog_timer_rld, sizeof(u8_t)) ||
        !read_field(fd, state->call_depth, sizeof(u32_t)) ||
        !read_field(fd, state->interrupts,
                    sizeof(interrupt_t) * INT_SLOT_NUM) ||
        !read_field(fd, state->cpu_halted, sizeof(bool_t)) ||
        !read_field(fd, state->memory, MEM_BUFFER_SIZE))
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    tamalib_refresh_hw();
    return true;
}

bool tamagotchi_state_delete(void)
{
    return rb->remove(TAMAGOTCHI_STATE_PATH) >= 0;
}
