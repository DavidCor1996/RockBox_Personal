#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "microui_config.h"
#include "microui.h"

struct guarded_context
{
    unsigned char before[32];
    mu_Context context;
    unsigned char after[32];
};

static int failures;

#define CHECK(condition) do {                                                \
    if (!(condition))                                                       \
    {                                                                       \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++;                                                         \
    }                                                                       \
} while (0)

void mu_rb_control_hook(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt)
{
    (void)ctx;
    (void)id;
    (void)rect;
    (void)opt;
}

static int text_width(mu_Font font, const char *text, int length)
{
    (void)font;
    return (length < 0 ? (int)strlen(text) : length) * 6;
}

static int text_height(mu_Font font)
{
    (void)font;
    return 8;
}

static void init_context(mu_Context *ctx)
{
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;
}

static void check_guards(const struct guarded_context *guarded)
{
    int i;

    for (i = 0; i < (int)sizeof(guarded->before); i++)
        CHECK(guarded->before[i] == 0xa5);
    for (i = 0; i < (int)sizeof(guarded->after); i++)
        CHECK(guarded->after[i] == 0x5a);
}

static void test_integer_io(void)
{
    static const struct
    {
        mu_Real value;
        const char *text;
    } cases[] =
    {
        { 0, "0" },
        { 2147483647, "2147483647" },
        { (-2147483647 - 1), "-2147483648" },
    };
    char buffer[16];
    mu_Real parsed;
    unsigned int i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        mu_format_integer(buffer, sizeof(buffer), cases[i].value);
        CHECK(strcmp(buffer, cases[i].text) == 0);
        CHECK(mu_parse_integer(buffer, &parsed));
        CHECK(parsed == cases[i].value);
    }
    CHECK(!mu_parse_integer("2147483648", &parsed));
    CHECK(!mu_parse_integer("-2147483649", &parsed));
    CHECK(!mu_parse_integer("12x", &parsed));
    CHECK(!mu_parse_integer("", &parsed));
    mu_format_integer(buffer, 2, 42);
    CHECK(strcmp(buffer, "4") == 0);
}

static void test_normal_frame(void)
{
    mu_Context ctx;
    mu_Command *command = NULL;
    int command_count = 0;

    init_context(&ctx);
    mu_begin(&ctx);
    CHECK(mu_begin_window_ex(&ctx, "test", mu_rect(0, 0, 160, 100),
                             MU_OPT_NOCLOSE | MU_OPT_NORESIZE));
    mu_layout_row(&ctx, 1, (int[]){ -1 }, 18);
    mu_button(&ctx, "button");
    mu_end_window(&ctx);
    mu_end(&ctx);
    CHECK(ctx.error == MU_ERROR_NONE);
    CHECK(!ctx.aborted);
    while (mu_next_command(&ctx, &command))
        command_count++;
    CHECK(command_count > 0);
    CHECK(ctx.command_high_water > 0);
}

static void test_bad_callback_aborts(void)
{
    mu_Context ctx;

    mu_init(&ctx);
    mu_begin(&ctx);
    CHECK(ctx.aborted);
    CHECK(ctx.error == MU_ERROR_BAD_FRAME);
}

static void test_command_overflow_is_contained(void)
{
    struct guarded_context guarded;
    char text[64];

    memset(&guarded, 0, sizeof(guarded));
    memset(guarded.before, 0xa5, sizeof(guarded.before));
    memset(guarded.after, 0x5a, sizeof(guarded.after));
    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    init_context(&guarded.context);
    mu_begin(&guarded.context);
    guarded.context.clip_stack.idx = 1;
    guarded.context.clip_stack.items[0] = mu_rect(0, 0, 320, 240);
    guarded.context.command_list.idx = MU_COMMANDLIST_SIZE - 8;
    mu_draw_text(&guarded.context, NULL, text, -1, mu_vec2(0, 0),
                 mu_color(255, 255, 255, 255));
    CHECK(guarded.context.aborted);
    CHECK(guarded.context.error == MU_ERROR_COMMAND_OVERFLOW);
    CHECK(guarded.context.command_list.idx == MU_COMMANDLIST_SIZE - 8);
    check_guards(&guarded);
}

static void test_bounded_failures(void)
{
    mu_Context ctx;
    mu_PoolItem pool[2];
    char input[64];
    char names[MU_CONTAINERPOOL_SIZE + 1][12];
    int widths[MU_MAX_WIDTHS + 1];
    int i;

    init_context(&ctx);
    mu_pop_clip_rect(&ctx);
    CHECK(ctx.error == MU_ERROR_STACK_UNDERFLOW);

    init_context(&ctx);
    ctx.frame = 7;
    memset(pool, 0, sizeof(pool));
    pool[0].last_update = 7;
    pool[1].last_update = 7;
    CHECK(mu_pool_init(&ctx, pool, 2, 1) == -1);
    CHECK(ctx.error == MU_ERROR_TREE_OVERFLOW);

    init_context(&ctx);
    memset(input, 'a', sizeof(input) - 1);
    input[sizeof(input) - 1] = '\0';
    mu_input_text(&ctx, input);
    CHECK(ctx.error == MU_ERROR_TEXT_OVERFLOW);
    CHECK(ctx.input_text[sizeof(ctx.input_text) - 1] == '\0');

    init_context(&ctx);
    ctx.clip_stack.idx = MU_CLIPSTACK_SIZE;
    ctx.clip_stack.items[MU_CLIPSTACK_SIZE - 1] = mu_rect(0, 0, 1, 1);
    mu_push_clip_rect(&ctx, mu_rect(0, 0, 1, 1));
    CHECK(ctx.error == MU_ERROR_CLIP_OVERFLOW);

    init_context(&ctx);
    for (i = 0; i <= MU_IDSTACK_SIZE; i++)
        mu_push_id(&ctx, &i, sizeof(i));
    CHECK(ctx.error == MU_ERROR_ID_OVERFLOW);

    init_context(&ctx);
    mu_begin(&ctx);
    CHECK(mu_begin_window_ex(&ctx, "layout", mu_rect(0, 0, 160, 100),
                             MU_OPT_NOCLOSE | MU_OPT_NORESIZE));
    memset(widths, 0, sizeof(widths));
    mu_layout_row(&ctx, MU_MAX_WIDTHS + 1, widths, 10);
    CHECK(ctx.error == MU_ERROR_BAD_FRAME);
    mu_end(&ctx);

    init_context(&ctx);
    mu_begin(&ctx);
    ctx.root_list.idx = MU_ROOTLIST_SIZE;
    CHECK(!mu_begin_window_ex(&ctx, "root overflow",
                              mu_rect(0, 0, 160, 100),
                              MU_OPT_NOCLOSE | MU_OPT_NORESIZE));
    CHECK(ctx.error == MU_ERROR_ROOT_OVERFLOW);
    mu_end(&ctx);

    init_context(&ctx);
    ctx.frame = 1;
    for (i = 0; i < MU_CONTAINERPOOL_SIZE; i++)
    {
        snprintf(names[i], sizeof(names[i]), "container%d", i);
        CHECK(mu_get_container(&ctx, names[i]) != NULL);
    }
    snprintf(names[MU_CONTAINERPOOL_SIZE],
             sizeof(names[MU_CONTAINERPOOL_SIZE]), "overflow");
    CHECK(mu_get_container(&ctx, names[MU_CONTAINERPOOL_SIZE]) == NULL);
    CHECK(ctx.error == MU_ERROR_CONTAINER_OVERFLOW);
}

int main(void)
{
    test_integer_io();
    test_normal_frame();
    test_bad_callback_aborts();
    test_command_overflow_is_contained();
    test_bounded_failures();
    if (failures)
        return 1;
    puts("microui core gate passed");
    return 0;
}
