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
    {
        23, "Ekans", "creature_023", 640,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xa2, 0x69, 0xc7),
        LCD_RGBPACK(0xf1, 0xd9, 0x6a),
        LCD_RGBPACK(0x5b, 0x31, 0x77)
    },
    {
        27, "Sandshrew", "creature_027", 680,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xe7, 0xd5, 0x8a),
        LCD_RGBPACK(0xf8, 0xf0, 0xc8),
        LCD_RGBPACK(0x8a, 0x70, 0x36)
    },
    {
        41, "Zubat", "creature_041", 720,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x72, 0x60, 0xcd),
        LCD_RGBPACK(0x6a, 0xc4, 0xa6),
        LCD_RGBPACK(0x34, 0x2b, 0x79)
    },
    {
        46, "Paras", "creature_046", 680,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xe4, 0x86, 0x49),
        LCD_RGBPACK(0xf7, 0xd9, 0x8c),
        LCD_RGBPACK(0x8a, 0x2f, 0x1e)
    },
    {
        48, "Venonat", "creature_048", 660,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xaa, 0x78, 0xd0),
        LCD_RGBPACK(0xc7, 0x55, 0x64),
        LCD_RGBPACK(0x5f, 0x3d, 0x84)
    },
    {
        84, "Doduo", "creature_084", 720,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xbe, 0x8a, 0x58),
        LCD_RGBPACK(0xf2, 0xe1, 0xbd),
        LCD_RGBPACK(0x6a, 0x43, 0x26)
    },
    {
        98, "Krabby", "creature_098", 720,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xe3, 0x78, 0x4e),
        LCD_RGBPACK(0xf2, 0xe2, 0xb4),
        LCD_RGBPACK(0x8b, 0x2c, 0x1e)
    },
    {
        109, "Koffing", "creature_109", 580,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xa7, 0x84, 0xc7),
        LCD_RGBPACK(0xd9, 0xc8, 0xe9),
        LCD_RGBPACK(0x63, 0x47, 0x85)
    },
    {
        113, "Chansey", "creature_113", 300,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0xf0, 0xc0, 0xcf),
        LCD_RGBPACK(0xff, 0xf0, 0xf5),
        LCD_RGBPACK(0x8a, 0x67, 0x76)
    },
    {
        116, "Horsea", "creature_116", 720,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x77, 0xbd, 0xf1),
        LCD_RGBPACK(0xe0, 0xf4, 0xff),
        LCD_RGBPACK(0x2c, 0x67, 0x9f)
    },
    {
        123, "Scyther", "creature_123", 480,
        25, 21, 56, 56, -10,
        LCD_RGBPACK(0x96, 0xc3, 0x6b),
        LCD_RGBPACK(0xe4, 0xf1, 0xc6),
        LCD_RGBPACK(0x4d, 0x73, 0x38)
    },
    {
        165, "Ledyba", "creature_165", 660,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xdf, 0x4a, 0x48),
        LCD_RGBPACK(0xf7, 0xe7, 0xa3),
        LCD_RGBPACK(0x2a, 0x2a, 0x2a)
    },
    {
        170, "Chinchou", "creature_170", 620,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x56, 0xa6, 0xe5),
        LCD_RGBPACK(0xf9, 0xe8, 0x8d),
        LCD_RGBPACK(0x2e, 0x58, 0x87)
    },
    {
        177, "Natu", "creature_177", 680,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x3f, 0xc8, 0x74),
        LCD_RGBPACK(0xe7, 0x5c, 0x3d),
        LCD_RGBPACK(0x1f, 0x6b, 0x3c)
    },
    {
        187, "Hoppip", "creature_187", 720,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xf2, 0xa6, 0xcb),
        LCD_RGBPACK(0xd8, 0xf2, 0xa7),
        LCD_RGBPACK(0x6a, 0x9f, 0x3e)
    },
    {
        190, "Aipom", "creature_190", 620,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0x9e, 0x7a, 0xcd),
        LCD_RGBPACK(0xf0, 0xd4, 0xc0),
        LCD_RGBPACK(0x5b, 0x3f, 0x82)
    },
    {
        218, "Slugma", "creature_218", 580,
        24, 20, 54, 54, -9,
        LCD_RGBPACK(0xdc, 0x52, 0x34),
        LCD_RGBPACK(0xf7, 0xa1, 0x4b),
        LCD_RGBPACK(0x7e, 0x24, 0x17)
    },
};

#define PC_EXTRA_CREATURE(id, name, prefix, primary, secondary, accent) \
    { \
        id, name, prefix, 360, \
        25, 21, 56, 56, -10, \
        primary, secondary, accent \
    }

static const struct pc_creature_def pc_extra_creatures[] = {
    PC_EXTRA_CREATURE(2, "Ivysaur", "creature_002",
                      LCD_RGBPACK(0x6f, 0xb7, 0x6a),
                      LCD_RGBPACK(0xd6, 0xf0, 0xc0),
                      LCD_RGBPACK(0x3d, 0x72, 0x35)),
    PC_EXTRA_CREATURE(3, "Venusaur", "creature_003",
                      LCD_RGBPACK(0x4e, 0x98, 0x57),
                      LCD_RGBPACK(0xc1, 0xe3, 0xb5),
                      LCD_RGBPACK(0x2e, 0x5f, 0x2f)),
    PC_EXTRA_CREATURE(5, "Charmeleon", "creature_005",
                      LCD_RGBPACK(0xf0, 0x74, 0x2d),
                      LCD_RGBPACK(0xff, 0xd1, 0x8e),
                      LCD_RGBPACK(0x8b, 0x29, 0x18)),
    PC_EXTRA_CREATURE(6, "Charizard", "creature_006",
                      LCD_RGBPACK(0xda, 0x62, 0x2a),
                      LCD_RGBPACK(0xff, 0xcb, 0x95),
                      LCD_RGBPACK(0x4e, 0x34, 0x54)),
    PC_EXTRA_CREATURE(8, "Wartortle", "creature_008",
                      LCD_RGBPACK(0x5a, 0x9e, 0xdf),
                      LCD_RGBPACK(0xc8, 0xec, 0xff),
                      LCD_RGBPACK(0x2d, 0x56, 0x88)),
    PC_EXTRA_CREATURE(9, "Blastoise", "creature_009",
                      LCD_RGBPACK(0x4b, 0x8d, 0xcb),
                      LCD_RGBPACK(0xb9, 0xe5, 0xff),
                      LCD_RGBPACK(0x2b, 0x4c, 0x75)),
    PC_EXTRA_CREATURE(11, "Metapod", "creature_011",
                      LCD_RGBPACK(0xa5, 0xcd, 0x56),
                      LCD_RGBPACK(0xe7, 0xf5, 0xae),
                      LCD_RGBPACK(0x4f, 0x74, 0x28)),
    PC_EXTRA_CREATURE(12, "Butterfree", "creature_012",
                      LCD_RGBPACK(0xb8, 0xc7, 0xf4),
                      LCD_RGBPACK(0xf3, 0xf6, 0xff),
                      LCD_RGBPACK(0x7a, 0x47, 0x7d)),
    PC_EXTRA_CREATURE(14, "Kakuna", "creature_014",
                      LCD_RGBPACK(0xd7, 0xc0, 0x4d),
                      LCD_RGBPACK(0xf0, 0xe4, 0x8c),
                      LCD_RGBPACK(0x7a, 0x5b, 0x1e)),
    PC_EXTRA_CREATURE(15, "Beedrill", "creature_015",
                      LCD_RGBPACK(0xf1, 0xc4, 0x33),
                      LCD_RGBPACK(0xff, 0xed, 0xa1),
                      LCD_RGBPACK(0x2d, 0x2d, 0x2d)),
    PC_EXTRA_CREATURE(17, "Pidgeotto", "creature_017",
                      LCD_RGBPACK(0xc4, 0xaa, 0x67),
                      LCD_RGBPACK(0xf0, 0xdc, 0xa9),
                      LCD_RGBPACK(0x72, 0x4c, 0x30)),
    PC_EXTRA_CREATURE(18, "Pidgeot", "creature_018",
                      LCD_RGBPACK(0xc0, 0x96, 0x56),
                      LCD_RGBPACK(0xed, 0xd4, 0xa2),
                      LCD_RGBPACK(0x6a, 0x40, 0x28)),
    PC_EXTRA_CREATURE(20, "Raticate", "creature_020",
                      LCD_RGBPACK(0x9b, 0x72, 0xc7),
                      LCD_RGBPACK(0xe8, 0xd7, 0xff),
                      LCD_RGBPACK(0x63, 0x47, 0x7d)),
    PC_EXTRA_CREATURE(22, "Fearow", "creature_022",
                      LCD_RGBPACK(0xb7, 0x7b, 0x4d),
                      LCD_RGBPACK(0xee, 0xd2, 0xa2),
                      LCD_RGBPACK(0x5f, 0x35, 0x22)),
    PC_EXTRA_CREATURE(24, "Arbok", "creature_024",
                      LCD_RGBPACK(0x98, 0x5d, 0xbc),
                      LCD_RGBPACK(0xe8, 0xd2, 0x73),
                      LCD_RGBPACK(0x4f, 0x29, 0x68)),
    PC_EXTRA_CREATURE(26, "Raichu", "creature_026",
                      LCD_RGBPACK(0xf1, 0xc4, 0x3d),
                      LCD_RGBPACK(0xff, 0xf0, 0xa1),
                      LCD_RGBPACK(0x7d, 0x45, 0x1a)),
    PC_EXTRA_CREATURE(28, "Sandslash", "creature_028",
                      LCD_RGBPACK(0xd9, 0xc2, 0x74),
                      LCD_RGBPACK(0xf7, 0xed, 0xc0),
                      LCD_RGBPACK(0x7d, 0x66, 0x2f)),
    PC_EXTRA_CREATURE(30, "Nidorina", "creature_030",
                      LCD_RGBPACK(0x6d, 0xb7, 0xd1),
                      LCD_RGBPACK(0xd1, 0xf0, 0xfa),
                      LCD_RGBPACK(0x3a, 0x66, 0x8d)),
    PC_EXTRA_CREATURE(31, "Nidoqueen", "creature_031",
                      LCD_RGBPACK(0x5f, 0x8f, 0xc1),
                      LCD_RGBPACK(0xd7, 0xe5, 0xf9),
                      LCD_RGBPACK(0x4d, 0x57, 0x88)),
    PC_EXTRA_CREATURE(33, "Nidorino", "creature_033",
                      LCD_RGBPACK(0xa4, 0x78, 0xe8),
                      LCD_RGBPACK(0xe3, 0xd1, 0xff),
                      LCD_RGBPACK(0x5b, 0x3f, 0x8a)),
    PC_EXTRA_CREATURE(34, "Nidoking", "creature_034",
                      LCD_RGBPACK(0x8f, 0x6d, 0xcd),
                      LCD_RGBPACK(0xd9, 0xcd, 0xf3),
                      LCD_RGBPACK(0x4a, 0x37, 0x72)),
    PC_EXTRA_CREATURE(36, "Clefable", "creature_036",
                      LCD_RGBPACK(0xf1, 0xc4, 0xda),
                      LCD_RGBPACK(0xff, 0xef, 0xf6),
                      LCD_RGBPACK(0xb8, 0x67, 0x86)),
    PC_EXTRA_CREATURE(40, "Wigglytuff", "creature_040",
                      LCD_RGBPACK(0xf1, 0xc7, 0xe5),
                      LCD_RGBPACK(0xff, 0xf0, 0xf8),
                      LCD_RGBPACK(0x8f, 0x5a, 0x7a)),
    PC_EXTRA_CREATURE(44, "Gloom", "creature_044",
                      LCD_RGBPACK(0x4d, 0x5c, 0xc8),
                      LCD_RGBPACK(0xc1, 0x7c, 0x68),
                      LCD_RGBPACK(0x2a, 0x38, 0x73)),
    PC_EXTRA_CREATURE(45, "Vileplume", "creature_045",
                      LCD_RGBPACK(0x4b, 0x59, 0xb4),
                      LCD_RGBPACK(0xe3, 0x5a, 0x6f),
                      LCD_RGBPACK(0x2a, 0x37, 0x67)),
    PC_EXTRA_CREATURE(47, "Parasect", "creature_047",
                      LCD_RGBPACK(0xd5, 0x73, 0x3f),
                      LCD_RGBPACK(0xee, 0xdd, 0x91),
                      LCD_RGBPACK(0x7e, 0x2a, 0x1e)),
    PC_EXTRA_CREATURE(49, "Venomoth", "creature_049",
                      LCD_RGBPACK(0x9d, 0x7f, 0xcf),
                      LCD_RGBPACK(0xcd, 0xa4, 0xbf),
                      LCD_RGBPACK(0x5f, 0x43, 0x84)),
    PC_EXTRA_CREATURE(53, "Persian", "creature_053",
                      LCD_RGBPACK(0xed, 0xdf, 0x9b),
                      LCD_RGBPACK(0xff, 0xf7, 0xcf),
                      LCD_RGBPACK(0x93, 0x68, 0x2c)),
    PC_EXTRA_CREATURE(55, "Golduck", "creature_055", 
                      LCD_RGBPACK(0x4f, 0x8b, 0xdb),
                      LCD_RGBPACK(0xe2, 0xdc, 0x78),
                      LCD_RGBPACK(0x2b, 0x52, 0x8c)),
    PC_EXTRA_CREATURE(59, "Arcanine", "creature_059",
                      LCD_RGBPACK(0xf2, 0x8b, 0x35),
                      LCD_RGBPACK(0xff, 0xe1, 0xb3),
                      LCD_RGBPACK(0x4d, 0x2c, 0x1b)),
    PC_EXTRA_CREATURE(61, "Poliwhirl", "creature_061",
                      LCD_RGBPACK(0x58, 0x9f, 0xeb),
                      LCD_RGBPACK(0xdb, 0xf0, 0xff),
                      LCD_RGBPACK(0x2b, 0x4c, 0x86)),
    PC_EXTRA_CREATURE(62, "Poliwrath", "creature_062",
                      LCD_RGBPACK(0x4c, 0x8b, 0xd1),
                      LCD_RGBPACK(0xd2, 0xe8, 0xff),
                      LCD_RGBPACK(0x2d, 0x4d, 0x7a)),
    PC_EXTRA_CREATURE(70, "Weepinbell", "creature_070",
                      LCD_RGBPACK(0xe3, 0xcf, 0x59),
                      LCD_RGBPACK(0xa0, 0xcf, 0x69),
                      LCD_RGBPACK(0x76, 0x4a, 0x28)),
    PC_EXTRA_CREATURE(71, "Victreebel", "creature_071",
                      LCD_RGBPACK(0xe8, 0xd0, 0x4d),
                      LCD_RGBPACK(0x8b, 0xc0, 0x59),
                      LCD_RGBPACK(0x6e, 0x45, 0x2b)),
    PC_EXTRA_CREATURE(97, "Hypno", "creature_097",
                      LCD_RGBPACK(0xd8, 0xbf, 0x48),
                      LCD_RGBPACK(0xf1, 0xdd, 0x95),
                      LCD_RGBPACK(0x6c, 0x4d, 0x1c)),
    PC_EXTRA_CREATURE(99, "Kingler", "creature_099",
                      LCD_RGBPACK(0xe0, 0x6e, 0x43),
                      LCD_RGBPACK(0xf4, 0xde, 0xac),
                      LCD_RGBPACK(0x8c, 0x2c, 0x1c)),
    PC_EXTRA_CREATURE(105, "Marowak", "creature_105",
                      LCD_RGBPACK(0xbe, 0xae, 0x85),
                      LCD_RGBPACK(0xf1, 0xe7, 0xc8),
                      LCD_RGBPACK(0x73, 0x5d, 0x43)),
    PC_EXTRA_CREATURE(110, "Weezing", "creature_110",
                      LCD_RGBPACK(0x9e, 0x79, 0xbf),
                      LCD_RGBPACK(0xd6, 0xc8, 0xe6),
                      LCD_RGBPACK(0x5f, 0x43, 0x7e)),
    PC_EXTRA_CREATURE(112, "Rhydon", "creature_112",
                      LCD_RGBPACK(0xa2, 0x9d, 0xb9),
                      LCD_RGBPACK(0xd9, 0xd8, 0xe7),
                      LCD_RGBPACK(0x67, 0x62, 0x7d)),
    PC_EXTRA_CREATURE(119, "Seaking", "creature_119",
                      LCD_RGBPACK(0xf1, 0x86, 0x40),
                      LCD_RGBPACK(0xff, 0xe7, 0xc6),
                      LCD_RGBPACK(0x7c, 0x4a, 0x22)),
    PC_EXTRA_CREATURE(121, "Starmie", "creature_121",
                      LCD_RGBPACK(0x99, 0x70, 0xb8),
                      LCD_RGBPACK(0xe3, 0x6b, 0x65),
                      LCD_RGBPACK(0x74, 0x48, 0x23)),
    PC_EXTRA_CREATURE(130, "Gyarados", "creature_130",
                      LCD_RGBPACK(0x4b, 0x96, 0xcc),
                      LCD_RGBPACK(0xff, 0xf0, 0xd0),
                      LCD_RGBPACK(0x2b, 0x4d, 0x79)),
    PC_EXTRA_CREATURE(153, "Bayleef", "creature_153",
                      LCD_RGBPACK(0x8d, 0xc9, 0x6e),
                      LCD_RGBPACK(0xe9, 0xf5, 0xc1),
                      LCD_RGBPACK(0x4d, 0x7d, 0x36)),
    PC_EXTRA_CREATURE(154, "Meganium", "creature_154",
                      LCD_RGBPACK(0x71, 0xad, 0x64),
                      LCD_RGBPACK(0xe0, 0xf0, 0xb5),
                      LCD_RGBPACK(0x3c, 0x68, 0x36)),
    PC_EXTRA_CREATURE(156, "Quilava", "creature_156",
                      LCD_RGBPACK(0x69, 0x91, 0xc9),
                      LCD_RGBPACK(0xff, 0xd0, 0x82),
                      LCD_RGBPACK(0xc8, 0x4c, 0x1d)),
    PC_EXTRA_CREATURE(157, "Typhlosion", "creature_157",
                      LCD_RGBPACK(0x5a, 0x82, 0xb9),
                      LCD_RGBPACK(0xff, 0xcf, 0x7c),
                      LCD_RGBPACK(0xbd, 0x42, 0x18)),
    PC_EXTRA_CREATURE(159, "Croconaw", "creature_159",
                      LCD_RGBPACK(0x56, 0xb0, 0xd9),
                      LCD_RGBPACK(0xd8, 0xf7, 0xff),
                      LCD_RGBPACK(0xd1, 0x46, 0x36)),
    PC_EXTRA_CREATURE(160, "Feraligatr", "creature_160",
                      LCD_RGBPACK(0x48, 0x9a, 0xc4),
                      LCD_RGBPACK(0xc8, 0xed, 0xff),
                      LCD_RGBPACK(0xc9, 0x3d, 0x2f)),
    PC_EXTRA_CREATURE(162, "Furret", "creature_162",
                      LCD_RGBPACK(0xb0, 0x87, 0x59),
                      LCD_RGBPACK(0xf3, 0xe5, 0xbf),
                      LCD_RGBPACK(0x6d, 0x45, 0x28)),
    PC_EXTRA_CREATURE(164, "Noctowl", "creature_164",
                      LCD_RGBPACK(0xa0, 0x8b, 0x61),
                      LCD_RGBPACK(0xf0, 0xe6, 0xc5),
                      LCD_RGBPACK(0x66, 0x45, 0x2e)),
    PC_EXTRA_CREATURE(166, "Ledian", "creature_166",
                      LCD_RGBPACK(0xd8, 0x46, 0x41),
                      LCD_RGBPACK(0xf6, 0xe6, 0x9f),
                      LCD_RGBPACK(0x2b, 0x2b, 0x2b)),
    PC_EXTRA_CREATURE(168, "Ariados", "creature_168",
                      LCD_RGBPACK(0xd1, 0x4b, 0x4b),
                      LCD_RGBPACK(0xd4, 0xc9, 0x7e),
                      LCD_RGBPACK(0x5c, 0x2b, 0x39)),
    PC_EXTRA_CREATURE(171, "Lanturn", "creature_171",
                      LCD_RGBPACK(0x4a, 0x91, 0xc9),
                      LCD_RGBPACK(0xf4, 0xdd, 0x76),
                      LCD_RGBPACK(0x2c, 0x59, 0x84)),
    PC_EXTRA_CREATURE(178, "Xatu", "creature_178",
                      LCD_RGBPACK(0x44, 0xae, 0x6e),
                      LCD_RGBPACK(0xf4, 0x67, 0x47),
                      LCD_RGBPACK(0x2b, 0x5e, 0x47)),
    PC_EXTRA_CREATURE(180, "Flaaffy", "creature_180",
                      LCD_RGBPACK(0xd8, 0x86, 0xce),
                      LCD_RGBPACK(0xf5, 0xda, 0xff),
                      LCD_RGBPACK(0x76, 0x4f, 0x8a)),
    PC_EXTRA_CREATURE(181, "Ampharos", "creature_181",
                      LCD_RGBPACK(0xf5, 0xc7, 0x4b),
                      LCD_RGBPACK(0xff, 0xf1, 0xa4),
                      LCD_RGBPACK(0xa0, 0x4f, 0x3d)),
    PC_EXTRA_CREATURE(184, "Azumarill", "creature_184",
                      LCD_RGBPACK(0x5f, 0xaf, 0xe8),
                      LCD_RGBPACK(0xf7, 0xff, 0xff),
                      LCD_RGBPACK(0x2e, 0x5e, 0x97)),
    PC_EXTRA_CREATURE(188, "Skiploom", "creature_188",
                      LCD_RGBPACK(0x6c, 0xc7, 0x72),
                      LCD_RGBPACK(0xf2, 0xa7, 0xd2),
                      LCD_RGBPACK(0x5f, 0x8e, 0x35)),
    PC_EXTRA_CREATURE(189, "Jumpluff", "creature_189",
                      LCD_RGBPACK(0x63, 0xba, 0x6b),
                      LCD_RGBPACK(0xf5, 0xf6, 0xff),
                      LCD_RGBPACK(0x4a, 0x8a, 0x32)),
    PC_EXTRA_CREATURE(195, "Quagsire", "creature_195",
                      LCD_RGBPACK(0x66, 0xba, 0xe0),
                      LCD_RGBPACK(0xd8, 0xf8, 0xff),
                      LCD_RGBPACK(0x7b, 0x45, 0xa9)),
    PC_EXTRA_CREATURE(219, "Magcargo", "creature_219",
                      LCD_RGBPACK(0xc8, 0x47, 0x31),
                      LCD_RGBPACK(0xc2, 0xa5, 0x79),
                      LCD_RGBPACK(0x6b, 0x24, 0x1a)),
};

struct pc_evolution_rule {
    int species_id;
    int family_species_id;
    int evolve_species_id;
    int evolve_candy_cost;
};

static const struct pc_evolution_rule pc_evolution_rules[] = {
    {   1,   1,   2,  25 }, {   2,   1,   3, 100 }, {   3,   1,  -1,   0 },
    {   4,   4,   5,  25 }, {   5,   4,   6, 100 }, {   6,   4,  -1,   0 },
    {   7,   7,   8,  25 }, {   8,   7,   9, 100 }, {   9,   7,  -1,   0 },
    {  10,  10,  11,  12 }, {  11,  10,  12,  50 }, {  12,  10,  -1,   0 },
    {  13,  13,  14,  12 }, {  14,  13,  15,  50 }, {  15,  13,  -1,   0 },
    {  16,  16,  17,  12 }, {  17,  16,  18,  50 }, {  18,  16,  -1,   0 },
    {  19,  19,  20,  25 }, {  20,  19,  -1,   0 },
    {  21,  21,  22,  50 }, {  22,  21,  -1,   0 },
    {  23,  23,  24,  50 }, {  24,  23,  -1,   0 },
    {  25,  25,  26,  50 }, {  26,  25,  -1,   0 },
    {  27,  27,  28,  50 }, {  28,  27,  -1,   0 },
    {  29,  29,  30,  25 }, {  30,  29,  31, 100 }, {  31,  29,  -1,   0 },
    {  32,  32,  33,  25 }, {  33,  32,  34, 100 }, {  34,  32,  -1,   0 },
    {  35,  35,  36,  50 }, {  36,  35,  -1,   0 },
    {  39,  39,  40,  50 }, {  40,  39,  -1,   0 },
    {  43,  43,  44,  25 }, {  44,  43,  45, 100 }, {  45,  43,  -1,   0 },
    {  46,  46,  47,  50 }, {  47,  46,  -1,   0 },
    {  48,  48,  49,  50 }, {  49,  48,  -1,   0 },
    {  52,  52,  53,  50 }, {  53,  52,  -1,   0 },
    {  54,  54,  55,  50 }, {  55,  54,  -1,   0 },
    {  58,  58,  59,  50 }, {  59,  58,  -1,   0 },
    {  60,  60,  61,  25 }, {  61,  60,  62, 100 }, {  62,  60,  -1,   0 },
    {  69,  69,  70,  25 }, {  70,  69,  71, 100 }, {  71,  69,  -1,   0 },
    {  96,  96,  97,  50 }, {  97,  96,  -1,   0 },
    {  98,  98,  99,  50 }, {  99,  98,  -1,   0 },
    { 104, 104, 105,  50 }, { 105, 104,  -1,   0 },
    { 109, 109, 110,  50 }, { 110, 109,  -1,   0 },
    { 111, 111, 112,  25 }, { 112, 111,  -1,   0 },
    { 118, 118, 119,  50 }, { 119, 118,  -1,   0 },
    { 120, 120, 121,  50 }, { 121, 120,  -1,   0 },
    { 129, 129, 130, 400 }, { 130, 129,  -1,   0 },
    { 152, 152, 153,  25 }, { 153, 152, 154, 100 }, { 154, 152,  -1,   0 },
    { 155, 155, 156,  25 }, { 156, 155, 157, 100 }, { 157, 155,  -1,   0 },
    { 158, 158, 159,  25 }, { 159, 158, 160, 100 }, { 160, 158,  -1,   0 },
    { 161, 161, 162,  25 }, { 162, 161,  -1,   0 },
    { 163, 163, 164,  50 }, { 164, 163,  -1,   0 },
    { 165, 165, 166,  25 }, { 166, 165,  -1,   0 },
    { 167, 167, 168,  50 }, { 168, 167,  -1,   0 },
    { 170, 170, 171,  50 }, { 171, 170,  -1,   0 },
    { 172,  25,  25,  25 },
    { 177, 177, 178,  50 }, { 178, 177,  -1,   0 },
    { 179, 179, 180,  25 }, { 180, 179, 181, 100 }, { 181, 179,  -1,   0 },
    { 183, 183, 184,  25 }, { 184, 183,  -1,   0 },
    { 187, 187, 188,  25 }, { 188, 187, 189, 100 }, { 189, 187,  -1,   0 },
    { 194, 194, 195,  50 }, { 195, 194,  -1,   0 },
    { 218, 218, 219,  50 }, { 219, 218,  -1,   0 },
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

int pc_assets_get_total_creature_count(void)
{
    return ARRAYLEN(pc_creatures) + ARRAYLEN(pc_extra_creatures);
}

const struct pc_creature_def *pc_assets_get_creature(int species_index)
{
    int base_count = ARRAYLEN(pc_creatures);
    int count = pc_assets_get_total_creature_count();

    if (count <= 0)
        return NULL;

    species_index %= count;
    if (species_index < 0)
        species_index += count;

    if (species_index < base_count)
        return &pc_creatures[species_index];

    species_index -= base_count;
    if (species_index < (int)ARRAYLEN(pc_extra_creatures))
        return &pc_extra_creatures[species_index];

    return NULL;
}

int pc_assets_find_species_index(int species_id)
{
    int i;
    int total = pc_assets_get_total_creature_count();

    for (i = 0; i < total; ++i)
    {
        const struct pc_creature_def *creature = pc_assets_get_creature(i);

        if (creature != NULL && creature->species_id == species_id)
            return i;
    }

    return -1;
}

static const struct pc_evolution_rule *find_evolution_rule_by_species_id(int species_id)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(pc_evolution_rules); ++i)
    {
        if (pc_evolution_rules[i].species_id == species_id)
            return &pc_evolution_rules[i];
    }

    return NULL;
}

int pc_assets_get_family_index(int species_index)
{
    const struct pc_creature_def *creature = pc_assets_get_creature(species_index);
    const struct pc_evolution_rule *rule;

    if (creature == NULL)
        return -1;

    rule = find_evolution_rule_by_species_id(creature->species_id);
    if (rule == NULL)
        return species_index;

    return pc_assets_find_species_index(rule->family_species_id);
}

int pc_assets_get_evolution_target(int species_index)
{
    const struct pc_creature_def *creature = pc_assets_get_creature(species_index);
    const struct pc_evolution_rule *rule;

    if (creature == NULL)
        return -1;

    rule = find_evolution_rule_by_species_id(creature->species_id);
    if (rule == NULL || rule->evolve_species_id < 0)
        return -1;

    return pc_assets_find_species_index(rule->evolve_species_id);
}

int pc_assets_get_evolution_cost(int species_index)
{
    const struct pc_creature_def *creature = pc_assets_get_creature(species_index);
    const struct pc_evolution_rule *rule;

    if (creature == NULL)
        return 0;

    rule = find_evolution_rule_by_species_id(creature->species_id);
    if (rule == NULL)
        return 0;

    return rule->evolve_candy_cost;
}

int pc_assets_get_catch_candy(int species_index)
{
    (void)species_index;
    return 3;
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
