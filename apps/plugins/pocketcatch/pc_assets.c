#include "pocketcatch.h"

static fb_data pc_background_pixels[LCD_WIDTH * LCD_HEIGHT];
static fb_data pc_creature_pixels[PC_CREATURE_MAX_W * PC_CREATURE_MAX_H];
static fb_data pc_resize_pixels[PC_CREATURE_MAX_W * PC_CREATURE_MAX_H];
static fb_data pc_ball_pixels[(PC_BALL_SPIN_FRAMES + 1) * PC_BALL_MAX_W * PC_BALL_MAX_H];

static const char *const pc_asset_roots[] = {
    PC_ASSET_ROOT,
    PC_ASSET_ROOT_ALT,
};

static const struct pc_creature_def pc_creatures[] = {
    {
        1, "Bulbasaur", "creature_001", 620,
        26, 22, 56, 56, -10,
        LCD_RGBPACK(0x79, 0xc8, 0x55),
        LCD_RGBPACK(0xe8, 0xf6, 0xa6),
        LCD_RGBPACK(0x2c, 0x7a, 0x33)
    },
    {
        4, "Charmander", "creature_004", 470,
        24, 21, 54, 54, -8,
        LCD_RGBPACK(0xff, 0x8d, 0x42),
        LCD_RGBPACK(0xff, 0xd7, 0x9c),
        LCD_RGBPACK(0xaf, 0x37, 0x19)
    },
    {
        7, "Squirtle", "creature_007", 380,
        25, 23, 56, 56, -9,
        LCD_RGBPACK(0x58, 0xb8, 0xf6),
        LCD_RGBPACK(0xb7, 0xec, 0xff),
        LCD_RGBPACK(0x17, 0x55, 0x88)
    },
    {
        10, "Caterpie", "creature_010", 780,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x97, 0xd8, 0x62),
        LCD_RGBPACK(0xf4, 0xef, 0x8f),
        LCD_RGBPACK(0xd9, 0x4a, 0x44)
    },
    {
        13, "Weedle", "creature_013", 760,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xd7, 0xb5, 0x59),
        LCD_RGBPACK(0xf6, 0xe6, 0x9a),
        LCD_RGBPACK(0x8a, 0x4d, 0x2b)
    },
    {
        16, "Pidgey", "creature_016", 730,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xc8, 0xb1, 0x73),
        LCD_RGBPACK(0xf3, 0xe0, 0xa7),
        LCD_RGBPACK(0x7c, 0x4d, 0x2f)
    },
    {
        19, "Rattata", "creature_019", 760,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xa8, 0x82, 0xd7),
        LCD_RGBPACK(0xeb, 0xd5, 0xff),
        LCD_RGBPACK(0x69, 0x49, 0x8d)
    },
    {
        21, "Spearow", "creature_021", 720,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xb8, 0x7a, 0x52),
        LCD_RGBPACK(0xf6, 0xd7, 0x9e),
        LCD_RGBPACK(0x64, 0x37, 0x22)
    },
    {
        25, "Pikachu", "creature_025", 540,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xfa, 0xda, 0x4a),
        LCD_RGBPACK(0xff, 0xf4, 0xa8),
        LCD_RGBPACK(0x7d, 0x47, 0x1d)
    },
    {
        29, "NidoranF", "creature_029", 620,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x76, 0xc8, 0xd9),
        LCD_RGBPACK(0xc9, 0xf0, 0xfa),
        LCD_RGBPACK(0x36, 0x66, 0x8b)
    },
    {
        32, "NidoranM", "creature_032", 620,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xb9, 0x8a, 0xf0),
        LCD_RGBPACK(0xe9, 0xd6, 0xff),
        LCD_RGBPACK(0x5f, 0x45, 0x8f)
    },
    {
        35, "Clefairy", "creature_035", 520,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xf4, 0xbf, 0xd3),
        LCD_RGBPACK(0xff, 0xe8, 0xf1),
        LCD_RGBPACK(0xbe, 0x62, 0x83)
    },
    {
        39, "Jigglypuff", "creature_039", 600,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xf4, 0xc5, 0xe4),
        LCD_RGBPACK(0xff, 0xeb, 0xf8),
        LCD_RGBPACK(0x9c, 0x58, 0x81)
    },
    {
        43, "Oddish", "creature_043", 700,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x4b, 0x76, 0xdb),
        LCD_RGBPACK(0x8d, 0xc5, 0x6a),
        LCD_RGBPACK(0x28, 0x3c, 0x7e)
    },
    {
        52, "Meowth", "creature_052", 640,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xee, 0xe1, 0x9d),
        LCD_RGBPACK(0xff, 0xf8, 0xd2),
        LCD_RGBPACK(0x9f, 0x74, 0x2e)
    },
    {
        54, "Psyduck", "creature_054", 710,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xf2, 0xd6, 0x5b),
        LCD_RGBPACK(0xff, 0xf3, 0xb0),
        LCD_RGBPACK(0x7a, 0x53, 0x18)
    },
    {
        58, "Growlithe", "creature_058", 500,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xf7, 0x94, 0x42),
        LCD_RGBPACK(0xff, 0xde, 0xb0),
        LCD_RGBPACK(0x5e, 0x2e, 0x17)
    },
    {
        60, "Poliwag", "creature_060", 700,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x67, 0xb9, 0xff),
        LCD_RGBPACK(0xd5, 0xf0, 0xff),
        LCD_RGBPACK(0x24, 0x53, 0x90)
    },
    {
        63, "Abra", "creature_063", 430,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xe2, 0xb8, 0x43),
        LCD_RGBPACK(0xf8, 0xe2, 0x9d),
        LCD_RGBPACK(0x76, 0x4d, 0x11)
    },
    {
        66, "Machop", "creature_066", 650,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0x9c, 0xa7, 0xc4),
        LCD_RGBPACK(0xd7, 0xdf, 0xf1),
        LCD_RGBPACK(0x4a, 0x57, 0x76)
    },
    {
        69, "Bellsprout", "creature_069", 690,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xe1, 0xd1, 0x57),
        LCD_RGBPACK(0x8f, 0xc8, 0x5c),
        LCD_RGBPACK(0x7d, 0x51, 0x2b)
    },
    {
        74, "Geodude", "creature_074", 710,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xa5, 0x96, 0x86),
        LCD_RGBPACK(0xd6, 0xc8, 0xb7),
        LCD_RGBPACK(0x62, 0x55, 0x49)
    },
    {
        79, "Slowpoke", "creature_079", 720,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xf0, 0xa3, 0xb7),
        LCD_RGBPACK(0xff, 0xe0, 0xea),
        LCD_RGBPACK(0x9e, 0x60, 0x7a)
    },
    {
        81, "Magnemite", "creature_081", 600,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xbd, 0xd1, 0xdf),
        LCD_RGBPACK(0xf1, 0xf8, 0xff),
        LCD_RGBPACK(0x6d, 0x7b, 0x89)
    },
    {
        92, "Gastly", "creature_092", 560,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0x78, 0x5f, 0xd1),
        LCD_RGBPACK(0xc4, 0xba, 0xff),
        LCD_RGBPACK(0x33, 0x2b, 0x74)
    },
    {
        96, "Drowzee", "creature_096", 670,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xe0, 0xcc, 0x4f),
        LCD_RGBPACK(0xc1, 0x8f, 0x54),
        LCD_RGBPACK(0x66, 0x4a, 0x25)
    },
    {
        104, "Cubone", "creature_104", 690,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xc2, 0xb9, 0x96),
        LCD_RGBPACK(0xf2, 0xeb, 0xd1),
        LCD_RGBPACK(0x7a, 0x63, 0x45)
    },
    {
        111, "Rhyhorn", "creature_111", 450,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xb5, 0xb0, 0xc8),
        LCD_RGBPACK(0xdf, 0xdc, 0xee),
        LCD_RGBPACK(0x6e, 0x69, 0x84)
    },
    {
        118, "Goldeen", "creature_118", 720,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xf5, 0x92, 0x47),
        LCD_RGBPACK(0xff, 0xea, 0xd0),
        LCD_RGBPACK(0x69, 0x44, 0x28)
    },
    {
        120, "Staryu", "creature_120", 700,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xc0, 0x8a, 0x52),
        LCD_RGBPACK(0xe8, 0x65, 0x66),
        LCD_RGBPACK(0x76, 0x48, 0x20)
    },
    {
        129, "Magikarp", "creature_129", 910,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xf6, 0x86, 0x3d),
        LCD_RGBPACK(0xff, 0xd6, 0xac),
        LCD_RGBPACK(0xdf, 0xc8, 0x5f)
    },
    {
        133, "Eevee", "creature_133", 480,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xb8, 0x82, 0x4d),
        LCD_RGBPACK(0xf4, 0xe1, 0xc0),
        LCD_RGBPACK(0x5f, 0x38, 0x1e)
    },
    {
        152, "Chikorita", "creature_152", 540,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xa8, 0xdd, 0x76),
        LCD_RGBPACK(0xe6, 0xf7, 0xbf),
        LCD_RGBPACK(0x3e, 0x74, 0x2f)
    },
    {
        155, "Cyndaquil", "creature_155", 450,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x74, 0xa1, 0xd9),
        LCD_RGBPACK(0xff, 0xd9, 0x83),
        LCD_RGBPACK(0xd3, 0x53, 0x1f)
    },
    {
        158, "Totodile", "creature_158", 430,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x5e, 0xc2, 0xe8),
        LCD_RGBPACK(0xd6, 0xf8, 0xff),
        LCD_RGBPACK(0xd5, 0x4d, 0x3f)
    },
    {
        161, "Sentret", "creature_161", 710,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xb4, 0x82, 0x5b),
        LCD_RGBPACK(0xee, 0xd7, 0xb2),
        LCD_RGBPACK(0x66, 0x3e, 0x1f)
    },
    {
        163, "Hoothoot", "creature_163", 620,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xa8, 0x66, 0x42),
        LCD_RGBPACK(0xf2, 0xde, 0x9f),
        LCD_RGBPACK(0x55, 0x31, 0x1d)
    },
    {
        167, "Spinarak", "creature_167", 700,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x5b, 0xc9, 0x56),
        LCD_RGBPACK(0xb6, 0xf0, 0xa2),
        LCD_RGBPACK(0x94, 0x2f, 0x3f)
    },
    {
        172, "Pichu", "creature_172", 690,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xfa, 0xdf, 0x57),
        LCD_RGBPACK(0xff, 0xf2, 0xad),
        LCD_RGBPACK(0x6f, 0x49, 0x1f)
    },
    {
        179, "Mareep", "creature_179", 560,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xca, 0xec, 0xf0),
        LCD_RGBPACK(0xff, 0xea, 0x8f),
        LCD_RGBPACK(0x4c, 0x86, 0xd7)
    },
    {
        183, "Marill", "creature_183", 690,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x6d, 0xbf, 0xf2),
        LCD_RGBPACK(0xf9, 0xfb, 0xff),
        LCD_RGBPACK(0x2f, 0x65, 0xa5)
    },
    {
        194, "Wooper", "creature_194", 680,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x7f, 0xd5, 0xf2),
        LCD_RGBPACK(0xc9, 0xf5, 0xff),
        LCD_RGBPACK(0x7e, 0x46, 0xb4)
    },
    {
        200, "Misdreavus", "creature_200", 450,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x82, 0x77, 0xd6),
        LCD_RGBPACK(0xf0, 0x73, 0x5d),
        LCD_RGBPACK(0x42, 0x31, 0x7a)
    },
};

static void clear_bitmap(struct pc_asset_bitmap *asset)
{
    rb->memset(&asset->bmp, 0, sizeof(asset->bmp));
    asset->loaded = false;
    asset->external = false;
    asset->path[0] = '\0';
}

static bool load_bitmap_exact(struct pc_asset_bitmap *asset, const char *path,
                              int expected_w, int expected_h)
{
    int rc;

    clear_bitmap(asset);
    asset->bmp.data = (char *)asset->pixels;
    rc = rb->read_bmp_file(path, &asset->bmp, asset->capacity, FORMAT_NATIVE, NULL);
    if (rc <= 0)
        return false;

    if (asset->bmp.width != expected_w || asset->bmp.height != expected_h)
    {
        clear_bitmap(asset);
        return false;
    }

    asset->loaded = true;
    asset->external = true;
    rb->strlcpy(asset->path, path, sizeof(asset->path));
    return true;
}

static bool load_bitmap_flexible(struct pc_asset_bitmap *asset, const char *path,
                                 int max_w, int max_h)
{
    int rc;

    clear_bitmap(asset);
    asset->bmp.data = (char *)asset->pixels;
    rc = rb->read_bmp_file(path, &asset->bmp, asset->capacity,
                           FORMAT_NATIVE | FORMAT_TRANSPARENT, NULL);
    if (rc <= 0)
        return false;

    if (asset->bmp.width > max_w || asset->bmp.height > max_h)
    {
        clear_bitmap(asset);
        return false;
    }

    asset->loaded = true;
    asset->external = true;
    rb->strlcpy(asset->path, path, sizeof(asset->path));
    return true;
}

static void build_creature_path(char *buffer, size_t buffer_size,
                                const char *root,
                                const struct pc_creature_def *creature)
{
    rb->snprintf(buffer, buffer_size, "%s/sprites/creatures/%s_idle_0.bmp",
                 root, creature->sprite_prefix);
}

static void build_ball_path(char *buffer, size_t buffer_size, const char *root)
{
    rb->snprintf(buffer, buffer_size, "%s/sprites/balls/ball_default_idle_0.bmp",
                 root);
}

static void build_ball_spin_path(char *buffer, size_t buffer_size,
                                 const char *root, int frame)
{
    rb->snprintf(buffer, buffer_size, "%s/sprites/balls/ball_default_spin_%d.bmp",
                 root, frame);
}

static void build_background_path(char *buffer, size_t buffer_size, const char *root)
{
    rb->snprintf(buffer, buffer_size, "%s/backgrounds/scene_day_layer0.bmp", root);
}

static void build_trainer_path(char *buffer, size_t buffer_size,
                               const char *root, const char *trainer_name,
                               int heading, int frame)
{
    static const char *const dirs[] = { "n", "e", "s", "w" };
    int safe_heading = heading;
    int safe_frame = frame;

    if (safe_heading < 0 || safe_heading >= 4)
        safe_heading = PC_HEADING_S;
    if (safe_frame < 0 || safe_frame >= PC_WORLD_WALK_FRAMES)
        safe_frame = 1;

    rb->snprintf(buffer, buffer_size,
                 "%s/sprites/trainers/%s_walk_%s_%d.bmp",
                 root, trainer_name, dirs[safe_heading], safe_frame);
}

static bool load_first_matching_bitmap_exact(struct pc_asset_bitmap *asset,
                                             int expected_w, int expected_h,
                                             void (*build_path)(char *, size_t, const char *))
{
    char path[MAX_PATH];
    int i;

    for (i = 0; i < (int)ARRAYLEN(pc_asset_roots); ++i)
    {
        build_path(path, sizeof(path), pc_asset_roots[i]);
        if (rb->file_exists(path) &&
            load_bitmap_exact(asset, path, expected_w, expected_h))
        {
            return true;
        }
    }

    return false;
}

static bool load_first_matching_ball(struct pc_asset_bitmap *asset)
{
    char path[MAX_PATH];
    int i;

    for (i = 0; i < (int)ARRAYLEN(pc_asset_roots); ++i)
    {
        build_ball_path(path, sizeof(path), pc_asset_roots[i]);
        if (rb->file_exists(path) &&
            load_bitmap_flexible(asset, path, PC_BALL_MAX_W, PC_BALL_MAX_H))
        {
            return true;
        }
    }

    return false;
}

static bool load_first_matching_ball_spin(struct pc_asset_bitmap *asset, int frame)
{
    char path[MAX_PATH];
    int i;

    for (i = 0; i < (int)ARRAYLEN(pc_asset_roots); ++i)
    {
        build_ball_spin_path(path, sizeof(path), pc_asset_roots[i], frame);
        if (rb->file_exists(path) &&
            load_bitmap_flexible(asset, path, PC_BALL_MAX_W, PC_BALL_MAX_H))
        {
            return true;
        }
    }

    return false;
}

static bool load_first_matching_creature(struct pc_asset_bitmap *asset,
                                         const struct pc_creature_def *creature)
{
    char path[MAX_PATH];
    int i;

    for (i = 0; i < (int)ARRAYLEN(pc_asset_roots); ++i)
    {
        build_creature_path(path, sizeof(path), pc_asset_roots[i], creature);
        if (rb->file_exists(path) &&
            load_bitmap_flexible(asset, path, PC_CREATURE_MAX_W, PC_CREATURE_MAX_H))
        {
            return true;
        }
    }

    return false;
}

void pc_assets_init(struct pc_asset_provider *assets)
{
    rb->memset(assets, 0, sizeof(*assets));

    assets->source = PC_ASSET_SOURCE_BUILTIN;

    assets->background.pixels = pc_background_pixels;
    assets->background.capacity = PC_BG_MAX_BYTES;
    assets->creature.pixels = pc_creature_pixels;
    assets->creature.capacity = PC_CREATURE_MAX_BYTES;
    assets->ball_idle.pixels = &pc_ball_pixels[0];
    assets->ball_idle.capacity = PC_BALL_MAX_BYTES;
    {
        int i;
        for (i = 0; i < PC_BALL_SPIN_FRAMES; ++i)
        {
            assets->ball_spin[i].pixels =
                &pc_ball_pixels[(i + 1) * PC_BALL_MAX_W * PC_BALL_MAX_H];
            assets->ball_spin[i].capacity = PC_BALL_MAX_BYTES;
        }
    }

    clear_bitmap(&assets->background);
    clear_bitmap(&assets->creature);
    clear_bitmap(&assets->ball_idle);
    {
        int i;
        for (i = 0; i < PC_BALL_SPIN_FRAMES; ++i)
            clear_bitmap(&assets->ball_spin[i]);
    }

    if (load_first_matching_bitmap_exact(&assets->background,
                                         LCD_WIDTH, LCD_HEIGHT,
                                         build_background_path))
    {
        assets->source = PC_ASSET_SOURCE_PACK_V0;
    }

    if (load_first_matching_ball(&assets->ball_idle))
    {
        assets->source = PC_ASSET_SOURCE_PACK_V0;
    }
    {
        int i;
        for (i = 0; i < PC_BALL_SPIN_FRAMES; ++i)
        {
            if (load_first_matching_ball_spin(&assets->ball_spin[i], i))
                assets->source = PC_ASSET_SOURCE_PACK_V0;
        }
    }
}

void pc_assets_teardown(struct pc_asset_provider *assets)
{
    clear_bitmap(&assets->background);
    clear_bitmap(&assets->creature);
    clear_bitmap(&assets->ball_idle);
    {
        int i;
        for (i = 0; i < PC_BALL_SPIN_FRAMES; ++i)
            clear_bitmap(&assets->ball_spin[i]);
    }
    assets->active_creature = NULL;
    assets->source = PC_ASSET_SOURCE_BUILTIN;
}

const struct pc_creature_def *pc_assets_select_creature(struct pc_asset_provider *assets,
                                                        int species_index)
{
    const struct pc_creature_def *creature;
    int count = pc_assets_get_creature_count();

    if (count <= 0)
        return NULL;

    species_index %= count;
    if (species_index < 0)
        species_index += count;

    creature = &pc_creatures[species_index];
    assets->active_creature = creature;
    clear_bitmap(&assets->creature);

    if (load_first_matching_creature(&assets->creature, creature))
    {
        assets->source = PC_ASSET_SOURCE_PACK_V0;
    }
    else if (!(assets->background.loaded || assets->ball_idle.loaded))
    {
        assets->source = PC_ASSET_SOURCE_BUILTIN;
    }

    return creature;
}

int pc_assets_get_creature_count(void)
{
    return ARRAYLEN(pc_creatures);
}

const struct pc_creature_def *pc_assets_get_creature(int species_index)
{
    int count = pc_assets_get_creature_count();

    if (count <= 0)
        return NULL;

    species_index %= count;
    if (species_index < 0)
        species_index += count;

    return &pc_creatures[species_index];
}

static bool load_scaled_bitmap_with_path(struct pc_asset_bitmap *asset,
                                         const char *path,
                                         int dest_w, int dest_h)
{
    struct pc_asset_bitmap source;

    if (asset == NULL || asset->pixels == NULL)
        return false;

    rb->memset(&source, 0, sizeof(source));
    source.pixels = pc_resize_pixels;
    source.capacity = sizeof(pc_resize_pixels);

    if (!rb->file_exists(path) ||
        !load_bitmap_flexible(&source, path, PC_CREATURE_MAX_W, PC_CREATURE_MAX_H))
    {
        clear_bitmap(asset);
        return false;
    }

    clear_bitmap(asset);
    asset->bmp.data = (char *)asset->pixels;
    asset->bmp.width = dest_w;
    asset->bmp.height = dest_h;
    simple_resize_bitmap(&source.bmp, &asset->bmp);
    asset->loaded = true;
    asset->external = true;
    rb->strlcpy(asset->path, path, sizeof(asset->path));
    return true;

}

bool pc_assets_load_world_creature(struct pc_asset_bitmap *asset, int species_index)
{
    struct pc_asset_bitmap source;
    const struct pc_creature_def *creature = pc_assets_get_creature(species_index);
    int dest_w;
    int dest_h;

    if (asset == NULL || asset->pixels == NULL || creature == NULL)
        return false;

    rb->memset(&source, 0, sizeof(source));
    source.pixels = pc_resize_pixels;
    source.capacity = sizeof(pc_resize_pixels);

    if (!load_first_matching_creature(&source, creature))
    {
        clear_bitmap(asset);
        return false;
    }

    dest_w = PC_WORLD_CREATURE_MAX_W;
    dest_h = source.bmp.height * dest_w / MAX(1, source.bmp.width);
    if (dest_h > PC_WORLD_CREATURE_MAX_H)
    {
        dest_h = PC_WORLD_CREATURE_MAX_H;
        dest_w = source.bmp.width * dest_h / MAX(1, source.bmp.height);
    }

    clear_bitmap(asset);
    asset->bmp.data = (char *)asset->pixels;
    asset->bmp.width = MAX(1, dest_w);
    asset->bmp.height = MAX(1, dest_h);
    simple_resize_bitmap(&source.bmp, &asset->bmp);
    asset->loaded = true;
    asset->external = true;
    rb->strlcpy(asset->path, source.path, sizeof(asset->path));
    return true;
}

bool pc_assets_load_world_trainer(struct pc_asset_bitmap *asset, int heading, int frame)
{
    char path[MAX_PATH];
    static const char *const trainer_names[] = { "leaf", "lyra" };
    int i;
    int j;

    for (i = 0; i < (int)ARRAYLEN(pc_asset_roots); ++i)
    {
        for (j = 0; j < (int)ARRAYLEN(trainer_names); ++j)
        {
            build_trainer_path(path, sizeof(path), pc_asset_roots[i],
                               trainer_names[j], heading, frame);
            if (load_scaled_bitmap_with_path(asset, path,
                                             PC_WORLD_TRAINER_MAX_W,
                                             PC_WORLD_TRAINER_MAX_H))
            {
                return true;
            }
        }
    }

    clear_bitmap(asset);
    return false;
}

const char *pc_assets_source_label(const struct pc_asset_provider *assets)
{
    return assets->source == PC_ASSET_SOURCE_PACK_V0 ? "Pack art" : "Built-in art";
}
