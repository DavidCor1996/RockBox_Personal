#include "pocketcatch.h"
#include "pc_red_gfx.h"
#include "pc_red_extra_gfx.h"
#include "pc_red_maps.h"
#include "pc_red_dojo_gfx.h"
#include "pc_red_pokecenter_gfx.h"
#include "filetypes.h"
#include "playlist.h"

#define PC_PALLET_MAP_W  10
#define PC_PALLET_MAP_H   9
#define PC_ROUTE1_MAP_W  10
#define PC_ROUTE1_MAP_H  18
#define PC_VIRIDIAN_MAP_W 20
#define PC_VIRIDIAN_MAP_H 18
#define PC_ROUTE22_MAP_W 20
#define PC_ROUTE22_MAP_H 9
#define PC_ROUTE2_MAP_W  10
#define PC_ROUTE2_MAP_H  36
#define PC_ROUTE21_MAP_W 10
#define PC_ROUTE21_MAP_H 45
#define PC_HOUSE_MAP_W    4
#define PC_HOUSE_MAP_H    4
#define PC_POKECENTER_MAP_W 7
#define PC_POKECENTER_MAP_H 4
#define PC_LAB_MAP_W      5
#define PC_LAB_MAP_H      6
#define PC_GYM_MAP_W     10
#define PC_GYM_MAP_H      9
#define PC_GATE_MAP_W     5
#define PC_GATE_MAP_H     4
#define PC_FOREST_MAP_W  17
#define PC_FOREST_MAP_H  24
#define PC_PEWTER_MAP_W  20
#define PC_PEWTER_MAP_H  18
#define PC_MUSEUM_1F_MAP_W 10
#define PC_MUSEUM_1F_MAP_H 4
#define PC_MUSEUM_2F_MAP_W 7
#define PC_MUSEUM_2F_MAP_H 4
#define PC_ROUTE3_MAP_W 35
#define PC_ROUTE3_MAP_H 9
#define PC_ROUTE4_MAP_W 45
#define PC_ROUTE4_MAP_H 9
#define PC_MT_MOON_POKECENTER_MAP_W 7
#define PC_MT_MOON_POKECENTER_MAP_H 4
#define PC_MT_MOON_1F_MAP_W 20
#define PC_MT_MOON_1F_MAP_H 18
#define PC_MT_MOON_B1F_MAP_W 14
#define PC_MT_MOON_B1F_MAP_H 14
#define PC_MT_MOON_B2F_MAP_W 20
#define PC_MT_MOON_B2F_MAP_H 18
#define PC_CERULEAN_MAP_W 20
#define PC_CERULEAN_MAP_H 18
#define PC_ROUTE24_MAP_W 10
#define PC_ROUTE24_MAP_H 18
#define PC_ROUTE25_MAP_W 30
#define PC_ROUTE25_MAP_H 9
#define PC_SMALL_GYM_MAP_W 5
#define PC_SMALL_GYM_MAP_H 7
#define PC_MUSIC_HISTORY_MAX 256
#define PC_UNLOCKED_SONGS_MAX 128

static const unsigned char pc_pallet_blocks[PC_PALLET_MAP_H][PC_PALLET_MAP_W] = {
    { 0x52, 0x4f, 0x52, 0x52, 0x4f, 0x0b, 0x50, 0x52, 0x52, 0x50 },
    { 0x4e, 0x01, 0x38, 0x39, 0x01, 0x01, 0x38, 0x39, 0x01, 0x4d },
    { 0x4e, 0x08, 0x3c, 0x3d, 0x01, 0x08, 0x3c, 0x3d, 0x01, 0x4d },
    { 0x4e, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x4d },
    { 0x4e, 0x01, 0x77, 0x56, 0x01, 0x0c, 0x0d, 0x0e, 0x01, 0x4d },
    { 0x4e, 0x01, 0x74, 0x74, 0x01, 0x10, 0x3a, 0x00, 0x01, 0x4d },
    { 0x4e, 0x01, 0x01, 0x01, 0x01, 0x77, 0x56, 0x77, 0x31, 0x4d },
    { 0x4e, 0x0a, 0x1d, 0x1e, 0x31, 0x74, 0x74, 0x0a, 0x31, 0x4d },
    { 0x50, 0x0a, 0x65, 0x64, 0x61, 0x61, 0x61, 0x61, 0x61, 0x4f },
};

static const unsigned char pc_route1_blocks[PC_ROUTE1_MAP_H][PC_ROUTE1_MAP_W] = {
    { 0x0a, 0x4d, 0x52, 0x52, 0x4f, 0x31, 0x50, 0x52, 0x52, 0x4e },
    { 0x0a, 0x4d, 0x0a, 0x0a, 0x0a, 0x31, 0x0a, 0x0a, 0x74, 0x4e },
    { 0x0a, 0x4d, 0x07, 0x07, 0x42, 0x1a, 0x1a, 0x31, 0x31, 0x4e },
    { 0x0a, 0x6e, 0x74, 0x74, 0x6e, 0x0b, 0x0b, 0x0b, 0x0b, 0x6d },
    { 0x0a, 0x6e, 0x07, 0x07, 0x42, 0x0b, 0x0b, 0x0b, 0x0b, 0x6d },
    { 0x0a, 0x6e, 0x0a, 0x74, 0x74, 0x0a, 0x31, 0x31, 0x31, 0x6d },
    { 0x0a, 0x6e, 0x6f, 0x07, 0x07, 0x6f, 0x1c, 0x0b, 0x0b, 0x6d },
    { 0x0a, 0x4d, 0x0a, 0x0a, 0x74, 0x74, 0x31, 0x0b, 0x0b, 0x4e },
    { 0x0a, 0x4d, 0x0a, 0x31, 0x31, 0x31, 0x31, 0x74, 0x74, 0x4e },
    { 0x0a, 0x4d, 0x2f, 0x1a, 0x2f, 0x07, 0x07, 0x07, 0x07, 0x4e },
    { 0x0a, 0x4d, 0x0a, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x4e },
    { 0x0a, 0x4d, 0x6f, 0x6f, 0x6f, 0x6f, 0x0b, 0x0b, 0x1a, 0x4e },
    { 0x0a, 0x4d, 0x0a, 0x0a, 0x74, 0x74, 0x0b, 0x0b, 0x31, 0x4e },
    { 0x0a, 0x4d, 0x1a, 0x31, 0x08, 0x1a, 0x1a, 0x1a, 0x1a, 0x4e },
    { 0x0a, 0x6e, 0x0a, 0x0b, 0x0b, 0x31, 0x0a, 0x0b, 0x0b, 0x6d },
    { 0x0a, 0x6e, 0x0b, 0x0b, 0x74, 0x31, 0x0b, 0x0b, 0x74, 0x6d },
    { 0x0a, 0x6e, 0x51, 0x51, 0x63, 0x0b, 0x62, 0x51, 0x51, 0x6d },
    { 0x0a, 0x6e, 0x0a, 0x0a, 0x4d, 0x0b, 0x4e, 0x0a, 0x0a, 0x6d },
};

static const unsigned char pc_viridian_blocks[PC_VIRIDIAN_MAP_H][PC_VIRIDIAN_MAP_W] = {
    { 0x2c, 0x2c, 0x29, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x1b, 0x08, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f },
    { 0x2c, 0x2c, 0x29, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x1b, 0x01, 0x0f, 0x0f, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x0f, 0x0f },
    { 0x2c, 0x2c, 0x29, 0x0a, 0x52, 0x52, 0x52, 0x34, 0x01, 0x01, 0x01, 0x31, 0x31, 0x31, 0x0c, 0x0d, 0x0e, 0x31, 0x0f, 0x0f },
    { 0x2c, 0x2c, 0x29, 0x4d, 0x0f, 0x0f, 0x0f, 0x0f, 0x1b, 0x01, 0x1b, 0x31, 0x31, 0x08, 0x10, 0x11, 0x12, 0x31, 0x0f, 0x0f },
    { 0x2c, 0x2c, 0x29, 0x4d, 0x0f, 0x0f, 0x0f, 0x0f, 0x1b, 0x01, 0x02, 0x03, 0x1a, 0x1a, 0x07, 0x07, 0x07, 0x1a, 0x0f, 0x0f },
    { 0x2c, 0x2c, 0x29, 0x4d, 0x0f, 0x0f, 0x0f, 0x0f, 0x1b, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x31, 0x0f, 0x0f },
    { 0x57, 0x57, 0x25, 0x4d, 0x0f, 0x0f, 0x0f, 0x0f, 0x1b, 0x01, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x0f, 0x0f },
    { 0x0a, 0x0a, 0x0a, 0x4d, 0x0f, 0x0f, 0x0f, 0x0f, 0x1b, 0x01, 0x02, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x0f, 0x0f },
    { 0x31, 0x01, 0x01, 0x0a, 0x6c, 0x6c, 0x6c, 0x6c, 0x08, 0x01, 0x77, 0x77, 0x01, 0x0a, 0x20, 0x21, 0x0a, 0x01, 0x0f, 0x0f },
    { 0x3f, 0x3b, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x74, 0x74, 0x01, 0x0a, 0x7c, 0x73, 0x0a, 0x01, 0x0f, 0x0f },
    { 0x2c, 0x29, 0x1c, 0x6f, 0x0a, 0x0a, 0x0a, 0x0a, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x0f, 0x0f },
    { 0x2c, 0x29, 0x01, 0x0a, 0x34, 0x74, 0x74, 0x0a, 0x01, 0x74, 0x01, 0x20, 0x21, 0x01, 0x74, 0x0a, 0x0a, 0x01, 0x0f, 0x0f },
    { 0x2c, 0x29, 0x01, 0x0a, 0x1d, 0x1f, 0x1e, 0x0a, 0x01, 0x0a, 0x01, 0x7c, 0x72, 0x01, 0x0a, 0x0a, 0x74, 0x01, 0x0f, 0x0f },
    { 0x57, 0x25, 0x1a, 0x07, 0x65, 0x43, 0x64, 0x2f, 0x1a, 0x2f, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x0f, 0x0f },
    { 0x0a, 0x4d, 0x01, 0x74, 0x0a, 0x0a, 0x0a, 0x0a, 0x01, 0x0a, 0x08, 0x0a, 0x74, 0x74, 0x01, 0x74, 0x74, 0x01, 0x0f, 0x0f },
    { 0x0a, 0x4d, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x01, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x0f, 0x0f },
    { 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x4d, 0x0a, 0x0a, 0x4d, 0x01, 0x4e, 0x0a, 0x0a, 0x4e, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a },
    { 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x4d, 0x0a, 0x0a, 0x4d, 0x01, 0x4e, 0x0a, 0x0a, 0x4e, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a },
};

static const unsigned char pc_route2_blocks[PC_ROUTE2_MAP_H][PC_ROUTE2_MAP_W] = {
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x01, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f },
    { 0x0b, 0x0b, 0x0b, 0x0b, 0x01, 0x1b, 0x31, 0x31, 0x31, 0x31 },
    { 0x0b, 0x0b, 0x0b, 0x0b, 0x01, 0x1b, 0x31, 0x31, 0x31, 0x31 },
    { 0x0b, 0x0b, 0x0b, 0x0b, 0x01, 0x3e, 0x3f, 0x3f, 0x3b, 0x31 },
    { 0x31, 0x31, 0x31, 0x31, 0x31, 0x24, 0x06, 0x57, 0x25, 0x31 },
    { 0x6c, 0x6d, 0x32, 0x6c, 0x6c, 0x08, 0x31, 0x74, 0x74, 0x0a },
    { 0x0a, 0x20, 0x21, 0x0a, 0x0a, 0x31, 0x31, 0x31, 0x31, 0x31 },
    { 0x52, 0x7c, 0x7e, 0x52, 0x52, 0x52, 0x52, 0x0a, 0x0a, 0x31 },
    { 0x55, 0x55, 0x55, 0x55, 0x0f, 0x0f, 0x0f, 0x0a, 0x0a, 0x31 },
    { 0x1a, 0x1a, 0x1a, 0x1a, 0x0f, 0x0f, 0x0f, 0x02, 0x03, 0x31 },
    { 0x0b, 0x01, 0x0b, 0x0b, 0x0f, 0x0f, 0x0f, 0x31, 0x31, 0x31 },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x32, 0x6c, 0x6c },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0a, 0x0a, 0x0a },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x2f, 0x07 },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0a, 0x0a, 0x74 },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x2f, 0x07 },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0a, 0x0a, 0x74 },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x52, 0x0a, 0x52 },
    { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x20, 0x0d, 0x21 },
    { 0x6c, 0x6d, 0x32, 0x6c, 0x6c, 0x0f, 0x0f, 0x7c, 0x7d, 0x7e },
    { 0x0a, 0x20, 0x21, 0x0a, 0x0a, 0x0f, 0x0f, 0x0a, 0x0a, 0x0a },
    { 0x52, 0x7c, 0x7e, 0x52, 0x52, 0x0f, 0x0f, 0x2f, 0x07, 0x07 },
    { 0x55, 0x55, 0x55, 0x55, 0x55, 0x6e, 0x0a, 0x0a, 0x0a, 0x0a },
    { 0x1a, 0x1a, 0x1a, 0x1a, 0x01, 0x6e, 0x0a, 0x0a, 0x74, 0x74 },
    { 0x74, 0x01, 0x0b, 0x0b, 0x0b, 0x6e, 0x07, 0x2f, 0x07, 0x07 },
    { 0x74, 0x01, 0x0b, 0x0b, 0x0b, 0x6e, 0x0a, 0x0a, 0x0a, 0x0a },
    { 0x74, 0x01, 0x0a, 0x6f, 0x6f, 0x6f, 0x34, 0x0a, 0x0a, 0x0a },
    { 0x74, 0x01, 0x6e, 0x0f, 0x0f, 0x0f, 0x6d, 0x0a, 0x0a, 0x0a },
    { 0x74, 0x01, 0x0a, 0x6c, 0x6c, 0x6c, 0x6d, 0x74, 0x0a, 0x0a },
    { 0x74, 0x01, 0x31, 0x31, 0x31, 0x0a, 0x6d, 0x74, 0x74, 0x0a },
    { 0x0f, 0x07, 0x07, 0x2f, 0x1a, 0x07, 0x34, 0x2f, 0x07, 0x07 },
    { 0x0f, 0x74, 0x74, 0x0a, 0x31, 0x0a, 0x6d, 0x0a, 0x0a, 0x0a },
    { 0x0f, 0x31, 0x08, 0x31, 0x31, 0x0a, 0x6d, 0x74, 0x74, 0x0a },
    { 0x0f, 0x01, 0x74, 0x74, 0x31, 0x0a, 0x6d, 0x74, 0x74, 0x0a },
    { 0x0f, 0x01, 0x01, 0x01, 0x01, 0x6f, 0x34, 0x6f, 0x6f, 0x6f },
    { 0x0f, 0x0f, 0x0f, 0x1b, 0x01, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f },
};

static const unsigned char pc_route21_blocks[PC_ROUTE21_MAP_H][PC_ROUTE21_MAP_W] = {
    { 0x51, 0x63, 0x65, 0x64, 0x51, 0x51, 0x51, 0x62, 0x51, 0x51 },
    { 0x0a, 0x4d, 0x65, 0x64, 0x0a, 0x74, 0x74, 0x4e, 0x0a, 0x0a },
    { 0x74, 0x4d, 0x65, 0x64, 0x0b, 0x0b, 0x0b, 0x4e, 0x74, 0x0a },
    { 0x74, 0x4d, 0x65, 0x64, 0x0b, 0x0b, 0x0b, 0x4e, 0x0a, 0x0a },
    { 0x74, 0x4d, 0x65, 0x64, 0x0b, 0x0b, 0x0b, 0x4e, 0x0a, 0x0a },
    { 0x74, 0x4d, 0x65, 0x2d, 0x1f, 0x1f, 0x1f, 0x67, 0x1f, 0x1f },
    { 0x52, 0x4f, 0x65, 0x43, 0x43, 0x43, 0x43, 0x18, 0x43, 0x43 },
    { 0x67, 0x1f, 0x2e, 0x43, 0x43, 0x43, 0x43, 0x14, 0x6b, 0x6b },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x54, 0x54, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x78, 0x78, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x54, 0x54, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x78, 0x78, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x6b, 0x15 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x19, 0x43, 0x43 },
    { 0x18, 0x43, 0x43, 0x43, 0x43, 0x54, 0x43, 0x19, 0x43, 0x43 },
};

static const unsigned char pc_outside_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 3, 3 }, { 6, 3 }, { 3, 6 }, { 6, 6 }, { 4, 4 }, { 5, 5 }, { 2, 5 }, { 7, 4 }
};

static const unsigned char pc_route1_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 5, 10 }, { 4, 12 }, { 5, 14 }, { 4, 16 }, { 7, 8 }, { 2, 15 }, { 7, 18 }, { 2, 22 }
};

static const unsigned char pc_viridian_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 9, 10 }, { 10, 12 }, { 6, 14 }, { 14, 14 }, { 8, 7 }, { 16, 9 }, { 12, 6 }, { 4, 10 }
};

static const unsigned char pc_route2_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 4, 6 }, { 5, 12 }, { 4, 24 }, { 5, 33 }, { 2, 18 }, { 7, 28 }, { 7, 6 }, { 2, 30 }
};

static const unsigned char pc_route21_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 4, 8 }, { 5, 14 }, { 4, 24 }, { 5, 34 }, { 2, 20 }, { 7, 38 }, { 7, 10 }, { 2, 30 }
};

static const unsigned char pc_forest_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 8, 21 }, { 13, 16 }, { 8, 10 }, { 3, 7 }, { 5, 15 }, { 11, 5 }, { 14, 10 }, { 5, 20 }
};

static const unsigned char pc_pewter_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 9, 14 }, { 7, 11 }, { 12, 9 }, { 14, 6 }, { 16, 12 }, { 5, 7 }, { 18, 8 }, { 4, 13 }
};

static const unsigned char pc_route22_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 5, 4 }, { 8, 5 }, { 12, 5 }, { 16, 4 }, { 3, 6 }, { 14, 6 }, { 10, 2 }, { 18, 5 }
};

static const unsigned char pc_route3_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 7, 2 }, { 11, 2 }, { 18, 2 }, { 30, 4 }, { 14, 4 }, { 24, 3 }, { 34, 3 }, { 27, 6 }
};

static const unsigned char pc_route4_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 15, 1 }, { 20, 1 }, { 26, 1 }, { 31, 1 }, { 36, 1 }, { 40, 1 }, { 10, 2 }, { 43, 4 }
};

static const unsigned char pc_cerulean_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 8, 14 }, { 10, 12 }, { 14, 10 }, { 6, 8 }, { 12, 7 }, { 16, 13 }, { 18, 9 }, { 5, 15 }
};

static const unsigned char pc_route24_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 7, 2 }, { 2, 5 }, { 2, 9 }, { 2, 12 }, { 2, 15 }, { 4, 4 }, { 7, 8 }, { 6, 14 }
};

static const unsigned char pc_route25_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 7, 1 }, { 12, 1 }, { 17, 1 }, { 22, 1 }, { 9, 4 }, { 24, 4 }, { 28, 2 }, { 18, 5 }
};

static const char *const pc_reds_house_1f_walk_mask[8] = {
    "########",
    "####...#",
    "##......",
    "##......",
    "##.##...",
    "##.##...",
    "##......",
    "##......",
};

static const char *const pc_reds_house_2f_walk_mask[8] = {
    "########",
    "###....#",
    "###....#",
    "###....#",
    "###....#",
    "###....#",
    "#.....#.",
    "#.....#.",
};

struct pc_world_secret_item {
    enum pc_world_scene scene;
    unsigned char metatile_x;
    unsigned char metatile_y;
};

struct pc_world_pokestop {
    enum pc_world_scene scene;
    unsigned char metatile_x;
    unsigned char metatile_y;
};

static const struct pc_world_secret_item pc_world_secret_items[PC_WORLD_SECRET_COUNT] = {
    { PC_WORLD_SCENE_ROUTE1_SOUTH, 16, 20 },
    { PC_WORLD_SCENE_ROUTE1_SOUTH, 4, 27 },
    { PC_WORLD_SCENE_VIRIDIAN_SOUTH, 7, 22 },
    { PC_WORLD_SCENE_VIRIDIAN_SOUTH, 27, 10 },
    { PC_WORLD_SCENE_ROUTE2_SOUTH, 2, 10 },
    { PC_WORLD_SCENE_ROUTE2_SOUTH, 8, 18 },
    { PC_WORLD_SCENE_OAKS_LAB, 2, 4 },
    { PC_WORLD_SCENE_VIRIDIAN_GYM, 14, 4 },
};

static const struct pc_world_pokestop pc_world_pokestops[PC_WORLD_POKESTOP_COUNT] = {
    { PC_WORLD_SCENE_PALLET, 10, 11 },
    { PC_WORLD_SCENE_ROUTE1_SOUTH, 10, 16 },
    { PC_WORLD_SCENE_ROUTE1_SOUTH, 14, 28 },
    { PC_WORLD_SCENE_VIRIDIAN_SOUTH, 17, 13 },
    { PC_WORLD_SCENE_VIRIDIAN_SOUTH, 28, 10 },
    { PC_WORLD_SCENE_VIRIDIAN_SOUTH, 23, 24 },
    { PC_WORLD_SCENE_ROUTE22, 15, 10 },
    { PC_WORLD_SCENE_ROUTE22, 27, 8 },
    { PC_WORLD_SCENE_ROUTE2_SOUTH, 6, 8 },
    { PC_WORLD_SCENE_ROUTE2_SOUTH, 7, 24 },
    { PC_WORLD_SCENE_VIRIDIAN_FOREST, 16, 39 },
    { PC_WORLD_SCENE_VIRIDIAN_FOREST, 28, 19 },
    { PC_WORLD_SCENE_VIRIDIAN_FOREST, 9, 11 },
    { PC_WORLD_SCENE_PEWTER, 11, 14 },
    { PC_WORLD_SCENE_PEWTER, 20, 20 },
    { PC_WORLD_SCENE_PEWTER, 33, 16 },
    { PC_WORLD_SCENE_PEWTER, 27, 10 },
    { PC_WORLD_SCENE_ROUTE3, 20, 5 },
    { PC_WORLD_SCENE_ROUTE3, 50, 7 },
    { PC_WORLD_SCENE_ROUTE3, 34, 5 },
    { PC_WORLD_SCENE_ROUTE4, 22, 3 },
    { PC_WORLD_SCENE_ROUTE4, 60, 3 },
    { PC_WORLD_SCENE_ROUTE4, 80, 4 },
    { PC_WORLD_SCENE_CERULEAN, 14, 15 },
    { PC_WORLD_SCENE_CERULEAN, 28, 11 },
    { PC_WORLD_SCENE_ROUTE24, 10, 16 },
    { PC_WORLD_SCENE_ROUTE24, 8, 8 },
    { PC_WORLD_SCENE_ROUTE25, 34, 4 },
    { PC_WORLD_SCENE_ROUTE25, 54, 4 },
    { PC_WORLD_SCENE_ROUTE25, 22, 6 },
    { PC_WORLD_SCENE_BILLS_HOUSE, 4, 5 },
};

static const unsigned char pc_overworld_passable_tiles[] = {
    0x00, 0x10, 0x1b, 0x20, 0x21, 0x23, 0x2c, 0x2d, 0x2e, 0x30,
    0x31, 0x33, 0x39, 0x3c, 0x3e, 0x52, 0x54, 0x58, 0x5b
};

static const unsigned char pc_reds_house_passable_tiles[] = {
    0x01, 0x02, 0x03, 0x11, 0x12, 0x13, 0x14, 0x1c, 0x1a
};

static const unsigned char pc_pokecenter_passable_tiles[] = {
    0x11, 0x1a, 0x1c, 0x3c, 0x5e
};

static const unsigned char pc_gym_passable_tiles[] = {
    0x03, 0x11, 0x16, 0x19, 0x2b, 0x3c, 0x3d, 0x3f, 0x4a, 0x4c, 0x4d
};

static const unsigned char pc_forest_passable_tiles[] = {
    0x1e, 0x20, 0x2e, 0x30, 0x34, 0x37, 0x39, 0x3a,
    0x40, 0x51, 0x52, 0x5a, 0x5c, 0x5e, 0x5f
};

static const unsigned char pc_house_passable_tiles[] = {
    0x01, 0x12, 0x14, 0x28, 0x32, 0x37, 0x44, 0x54, 0x5c
};

static const unsigned char pc_gate_passable_tiles[] = {
    0x01, 0x12, 0x14, 0x1a, 0x1c, 0x37, 0x38, 0x3b, 0x3c, 0x5e
};

static const unsigned char pc_lab_passable_tiles[] = {
    0x0c, 0x16, 0x1e, 0x26, 0x34, 0x37
};

static const unsigned char pc_cavern_passable_tiles[] = {
    0x05, 0x15, 0x18, 0x1a, 0x20, 0x21, 0x22, 0x2a, 0x2d, 0x30
};

static const unsigned char pc_club_passable_tiles[] = {
    0x0f, 0x1a, 0x1f, 0x26, 0x28, 0x29, 0x2c, 0x2d, 0x2e, 0x2f, 0x41
};

static const unsigned char pc_ship_passable_tiles[] = {
    0x04, 0x0d, 0x17, 0x1d, 0x1e, 0x23, 0x34, 0x37, 0x39, 0x4a
};

static fb_data pc_world_trainer_pixels[4][PC_WORLD_WALK_FRAMES]
                                      [PC_WORLD_TRAINER_MAX_W * PC_WORLD_TRAINER_MAX_H];
static fb_data pc_world_creature_pixels[PC_WORLD_MAX_SPAWNS]
                                       [PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H];
static fb_data pc_world_dex_pixels[PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H];

#define PC_SAVE_MAGIC 0x5043474f
#define PC_SAVE_VERSION 14
#define PC_SAFE_RESET_VERSION 9
#define PC_MENU_ITEMS 8
#define PC_START_POKEBALLS 100
#define PC_MAX_POKEBALLS 1000
#define PC_MART_POKEBALL_BUNDLE 10
#define PC_MART_POKEBALL_COST 90
#define PC_MART_SURF_COST 80
#define PC_MART_CUT_COST 60
#define PC_WORLD_VISIBLE_SPAWNS_MAX 6
#define PC_WORLD_SPAWN_RESPAWN_MIN 6
#define PC_WORLD_SPAWN_RESPAWN_MAX 16
#define PC_WORLD_SPAWN_LIFETIME_MIN 48
#define PC_WORLD_SPAWN_LIFETIME_MAX 120
#define PC_POKESTOP_COOLDOWN_STEPS 72
#define PC_POKESTOP_MIN_BALLS 3
#define PC_POKESTOP_MAX_BALLS 6
#define PC_POKESTOP_MIN_MONEY 8
#define PC_POKESTOP_MAX_MONEY 18
#define PC_STARTUP_ENCOUNTER_COOLDOWN 24
#define PC_STARTUP_ENCOUNTER_GRACE_STEPS 2
#define PC_POKESTOP_ENCOUNTER_COOLDOWN (PC_FRAME_HZ * 3)
#define PC_POKESTOP_ENCOUNTER_GRACE_STEPS 3
#define PC_POST_ENCOUNTER_COOLDOWN (PC_FRAME_HZ * 5)
#define PC_POST_ENCOUNTER_GRACE_STEPS 4
#define PC_LOCAL_ENCOUNTER_CLEAR_RADIUS (PC_WORLD_SAFE_PLAYER_RADIUS + PC_WORLD_TILE_SIZE)

struct pc_spawn_entry {
    int species_id;
    int weight;
};

enum pc_mart_category {
    PC_MART_CATEGORY_ITEMS = 0,
    PC_MART_CATEGORY_HMS,
    PC_MART_CATEGORY_LOOKS
};

struct pc_player_trainer_def {
    const char *asset_name;
    const char *display_name;
    int cost;
};

static const struct pc_player_trainer_def pc_player_trainers[PC_PLAYER_TRAINER_COUNT] = {
    { "leaf", "Leaf", 0 },
    { "red", "Red", 180 },
    { "ethan", "Ethan", 220 },
    { "kris", "Kris", 220 },
    { "brendan", "Brendan", 260 },
    { "may", "May", 260 },
};

static enum pc_outdoor_region current_outdoor_region(const struct pc_world_state *world);

struct pc_save_record_v4 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
};

struct pc_save_record_v8 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
};

struct pc_save_record_v9 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned int secret_collected_bits;
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
};

struct pc_save_record_v10 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned int secret_collected_bits;
    unsigned short pokestop_cooldowns[PC_WORLD_POKESTOP_COUNT_V12];
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
};

struct pc_save_record_v11 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    int buddy_species;
    int buddy_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned int secret_collected_bits;
    unsigned short pokestop_cooldowns[PC_WORLD_POKESTOP_COUNT_V12];
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
};

struct pc_save_record_v12 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    int buddy_species;
    int buddy_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned int secret_collected_bits;
    signed char player_trainer;
    unsigned char owned_trainers[PC_PLAYER_TRAINER_COUNT];
    unsigned short pokestop_cooldowns[PC_WORLD_POKESTOP_COUNT_V12];
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
};

struct pc_save_record_v13 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    int buddy_species;
    int buddy_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned int secret_collected_bits;
    signed char player_trainer;
    unsigned char owned_trainers[PC_PLAYER_TRAINER_COUNT];
    unsigned short pokestop_cooldowns[PC_WORLD_POKESTOP_COUNT_V13];
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
};

struct pc_save_record {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    int buddy_species;
    int buddy_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned int secret_collected_bits;
    signed char player_trainer;
    unsigned char owned_trainers[PC_PLAYER_TRAINER_COUNT];
    unsigned short pokestop_cooldowns[PC_WORLD_POKESTOP_COUNT];
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
};

struct pc_save_record_v7 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    int travel_steps;
    unsigned short pokeballs;
    unsigned short money;
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[1];
    unsigned char ability_owned[1];
};

struct pc_save_record_v1 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    unsigned short caught_counts[64];
};

union pc_save_scratch {
    struct pc_save_record current;
    struct pc_save_record_v13 v13;
    struct pc_save_record_v12 v12;
    struct pc_save_record_v11 v11;
    struct pc_save_record_v10 v10;
    struct pc_save_record_v9 v9;
    struct pc_save_record_v8 v8;
    struct pc_save_record_v7 v7;
    struct pc_save_record_v4 v4;
    struct pc_save_record_v1 v1;
};

enum pc_outdoor_region {
    PC_OUTDOOR_REGION_ROUTE2 = 0,
    PC_OUTDOOR_REGION_FOREST,
    PC_OUTDOOR_REGION_VIRIDIAN,
    PC_OUTDOOR_REGION_ROUTE22,
    PC_OUTDOOR_REGION_ROUTE1,
    PC_OUTDOOR_REGION_PALLET,
    PC_OUTDOOR_REGION_ROUTE21,
    PC_OUTDOOR_REGION_PEWTER,
    PC_OUTDOOR_REGION_ROUTE3,
    PC_OUTDOOR_REGION_ROUTE4,
    PC_OUTDOOR_REGION_CERULEAN,
    PC_OUTDOOR_REGION_ROUTE24,
    PC_OUTDOOR_REGION_ROUTE25
};

static const struct pc_spawn_entry pc_empty_spawn_table[] = {
    { 0, 0 }
};

static const struct pc_spawn_entry pc_route1_spawn_table[] = {
    { 16, 20 }, { 19, 18 }, { 21, 10 }, { 29, 8 }
};

static const struct pc_spawn_entry pc_route22_spawn_table[] = {
    { 19, 14 }, { 32, 12 }, { 29, 12 }, { 21, 11 }, { 16, 10 }, { 56, 8 }, { 23, 6 }, { 39, 5 }
};

static const struct pc_spawn_entry pc_route2_spawn_table[] = {
    { 19, 16 }, { 16, 14 }, { 13, 12 }, { 10, 12 }, { 29, 10 }, { 32, 10 }, { 43, 8 }, { 69, 8 }
};

static const struct pc_spawn_entry pc_forest_spawn_table[] = {
    { 13, 18 }, { 10, 16 }, { 14, 15 }, { 11, 13 }, { 25, 8 }, { 16, 6 }
};

static const struct pc_spawn_entry pc_route21_spawn_table[] = {
    { 7, 6 }, { 54, 14 }, { 60, 16 }, { 61, 8 }, { 79, 12 }, { 118, 12 },
    { 119, 8 }, { 120, 10 }, { 121, 6 }, { 129, 10 }, { 130, 5 }, { 183, 5 },
    { 184, 4 }, { 194, 4 }, { 195, 4 }
};

static const struct pc_spawn_entry pc_route3_spawn_table[] = {
    { 21, 18 }, { 16, 16 }, { 39, 12 }, { 19, 10 }, { 23, 8 }
};

static const struct pc_spawn_entry pc_route4_spawn_table[] = {
    { 23, 16 }, { 19, 14 }, { 21, 14 }, { 16, 12 }, { 56, 8 }
};

static const struct pc_spawn_entry pc_route24_spawn_table[] = {
    { 43, 16 }, { 16, 14 }, { 63, 12 }, { 13, 10 }, { 14, 9 }, { 10, 8 }, { 19, 7 }, { 25, 5 }
};

static const struct pc_spawn_entry pc_route25_spawn_table[] = {
    { 43, 16 }, { 16, 14 }, { 63, 12 }, { 13, 10 }, { 14, 9 }, { 11, 7 }, { 10, 7 }, { 25, 6 }, { 19, 5 }
};


static void player_block_pos(const struct pc_world_state *world,
                             int *block_x, int *block_y);
static void init_spawns(struct pc_world_state *world);
static bool outdoor_scene(enum pc_world_scene scene);
static void load_scene_data(struct pc_world_state *world, enum pc_world_scene scene);
bool pc_world_save(struct pc_world_state *world);
static int clampi(int value, int min_value, int max_value);
static int find_species_with_mode(const struct pc_world_state *world,
                                  int start, int dir, bool caught_only);

static void clear_bitmap(struct pc_asset_bitmap *asset)
{
    rb->memset(&asset->bmp, 0, sizeof(asset->bmp));
    asset->loaded = false;
    asset->external = false;
    asset->path[0] = '\0';
}

static void set_banner(struct pc_world_state *world,
                       const char *line1, const char *line2)
{
    rb->strlcpy(world->banner.line1, line1, sizeof(world->banner.line1));
    rb->strlcpy(world->banner.line2, line2, sizeof(world->banner.line2));
}

static void set_detail(struct pc_world_state *world,
                       const char *line1, const char *line2)
{
    rb->strlcpy(world->detail.line1, line1, sizeof(world->detail.line1));
    rb->strlcpy(world->detail.line2, line2, sizeof(world->detail.line2));
}

static void set_notice(struct pc_world_state *world,
                       const char *line1, const char *line2)
{
    set_detail(world, line1, line2);
    world->notice_frames = PC_FRAME_HZ * 2;
}

static void ensure_pocketcatch_data_dir(void)
{
    if (!rb->dir_exists(PC_ASSET_ROOT_ALT))
        rb->mkdir(PC_ASSET_ROOT_ALT);
}

static bool secret_collected(const struct pc_world_state *world, int index)
{
    if (index < 0 || index >= PC_WORLD_SECRET_COUNT)
        return true;

    return (world->secret_collected_bits & (1u << index)) != 0;
}

static void collect_secret(struct pc_world_state *world, int index)
{
    if (index < 0 || index >= PC_WORLD_SECRET_COUNT)
        return;

    world->secret_collected_bits |= (1u << index);
}

static const char *pc_path_basename(const char *path)
{
    const char *slash = rb->strrchr(path, '/');

    if (slash != NULL && slash[1] != '\0')
        return slash + 1;
    return path;
}

static bool path_in_music_root(const char *path)
{
    size_t prefix_len = sizeof(PC_MUSIC_ROOT_PATH) - 1;

    return rb->strncmp(path, PC_MUSIC_ROOT_PATH "/", prefix_len + 1) == 0;
}

static uint32_t track_path_hash(const char *path)
{
    return rb->crc_32(path, rb->strlen(path), 0xffffffff);
}

static int load_music_history(uint32_t *hashes, int max_hashes)
{
    int fd;
    ssize_t got;
    int count;

    ensure_pocketcatch_data_dir();
    fd = rb->open(PC_MUSIC_HISTORY_PATH, O_RDONLY);
    if (fd < 0)
        return 0;

    got = rb->read(fd, hashes, max_hashes * (int)sizeof(*hashes));
    rb->close(fd);
    if (got <= 0)
        return 0;

    count = (int)(got / (ssize_t)sizeof(*hashes));
    return MAX(0, MIN(count, max_hashes));
}

static bool save_music_history(const uint32_t *hashes, int count)
{
    int fd;
    ssize_t want = count * (ssize_t)sizeof(*hashes);

    ensure_pocketcatch_data_dir();
    fd = rb->open(PC_MUSIC_HISTORY_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    if (want > 0 && rb->write(fd, hashes, want) != want)
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

static bool music_history_contains(const uint32_t *hashes, int count, uint32_t hash)
{
    int i;

    for (i = 0; i < count; ++i)
    {
        if (hashes[i] == hash)
            return true;
    }

    return false;
}

static void remember_music_track(uint32_t hash)
{
    uint32_t hashes[PC_MUSIC_HISTORY_MAX];
    int count = load_music_history(hashes, ARRAYLEN(hashes));

    if (music_history_contains(hashes, count, hash))
        return;

    if (count < (int)ARRAYLEN(hashes))
        hashes[count++] = hash;
    else
        hashes[ARRAYLEN(hashes) - 1] = hash;

    save_music_history(hashes, count);
}

struct pc_music_pick {
    char path[MAX_PATH];
    uint32_t hash;
    int seen;
};

struct pc_unlocked_song_store {
    int count;
    char paths[PC_UNLOCKED_SONGS_MAX][MAX_PATH];
};

static struct pc_unlocked_song_store pc_unlocked_songs;

static void load_unlocked_song_store(void)
{
    int fd;
    ssize_t got;

    rb->memset(&pc_unlocked_songs, 0, sizeof(pc_unlocked_songs));
    ensure_pocketcatch_data_dir();
    fd = rb->open(PC_MUSIC_UNLOCKS_PATH, O_RDONLY);
    if (fd < 0)
        return;

    got = rb->read(fd, &pc_unlocked_songs, sizeof(pc_unlocked_songs));
    rb->close(fd);
    if (got != (ssize_t)sizeof(pc_unlocked_songs))
    {
        rb->memset(&pc_unlocked_songs, 0, sizeof(pc_unlocked_songs));
        return;
    }

    pc_unlocked_songs.count = clampi(pc_unlocked_songs.count, 0, PC_UNLOCKED_SONGS_MAX);
}

static bool save_unlocked_song_store(void)
{
    int fd;

    ensure_pocketcatch_data_dir();
    fd = rb->open(PC_MUSIC_UNLOCKS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    if (rb->write(fd, &pc_unlocked_songs, sizeof(pc_unlocked_songs)) !=
        (ssize_t)sizeof(pc_unlocked_songs))
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

static int unlocked_song_index(const char *path)
{
    int i;

    for (i = 0; i < pc_unlocked_songs.count; ++i)
    {
        if (!rb->strcmp(pc_unlocked_songs.paths[i], path))
            return i;
    }

    return -1;
}

static bool unlock_song_path(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return false;
    if (unlocked_song_index(path) >= 0)
        return true;
    if (pc_unlocked_songs.count >= PC_UNLOCKED_SONGS_MAX)
        return false;

    rb->strlcpy(pc_unlocked_songs.paths[pc_unlocked_songs.count],
                path, sizeof(pc_unlocked_songs.paths[pc_unlocked_songs.count]));
    pc_unlocked_songs.count++;
    return save_unlocked_song_store();
}

int pc_world_song_count(void)
{
    return pc_unlocked_songs.count;
}

bool pc_world_song_name(int index, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0 ||
        index < 0 || index >= pc_unlocked_songs.count)
    {
        return false;
    }

    rb->strlcpy(buffer, pc_path_basename(pc_unlocked_songs.paths[index]), buffer_size);
    return true;
}

static void consider_music_pick(struct pc_music_pick *pick,
                                const uint32_t *history, int history_count,
                                const char *path)
{
    uint32_t hash;

    if (path == NULL || path[0] == '\0' || !path_in_music_root(path))
        return;

    hash = track_path_hash(path);
    if (music_history_contains(history, history_count, hash))
        return;

    pick->seen++;
    if (pick->seen == 1 || (rb->rand() % pick->seen) == 0)
    {
        rb->strlcpy(pick->path, path, sizeof(pick->path));
        pick->hash = hash;
    }
}

#ifdef HAVE_TAGCACHE
static bool pick_track_from_database(const uint32_t *history, int history_count,
                                     char *path, size_t path_size, uint32_t *hash)
{
    struct tagcache_stat *stat = rb->tagcache_get_stat();
    struct tagcache_search tcs;
    struct pc_music_pick pick;
    char candidate[MAX_PATH];

    if (stat == NULL || !stat->readyvalid || !stat->ready)
        return false;
    if (!rb->tagcache_search(&tcs, tag_filename))
        return false;

    rb->memset(&pick, 0, sizeof(pick));
    while (rb->tagcache_get_next(&tcs, candidate, sizeof(candidate)))
        consider_music_pick(&pick, history, history_count, candidate);
    rb->tagcache_search_finish(&tcs);

    if (pick.seen <= 0)
        return false;

    rb->strlcpy(path, pick.path, path_size);
    *hash = pick.hash;
    return true;
}
#endif

static void pick_track_from_directory_recursive(const char *dir_path,
                                                const uint32_t *history, int history_count,
                                                struct pc_music_pick *pick)
{
    DIR *dir;
    struct dirent *entry;

    dir = rb->opendir(dir_path);
    if (dir == NULL)
        return;

    while ((entry = rb->readdir(dir)) != NULL)
    {
        struct dirinfo info = rb->dir_get_info(dir, entry);
        char child[MAX_PATH];
        size_t dir_len;
        size_t name_len;

        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        dir_len = rb->strlen(dir_path);
        name_len = rb->strlen(entry->d_name);
        if (dir_len + 1 + name_len + 1 > sizeof(child))
            continue;

        rb->strlcpy(child, dir_path, sizeof(child));
        if (dir_len > 0 && child[dir_len - 1] != '/')
            rb->strlcat(child, "/", sizeof(child));
        rb->strlcat(child, entry->d_name, sizeof(child));

        if (info.attribute & ATTR_DIRECTORY)
        {
            pick_track_from_directory_recursive(child, history, history_count, pick);
        }
        else if (rb->filetype_get_attr(entry->d_name) == FILE_ATTR_AUDIO)
        {
            consider_music_pick(pick, history, history_count, child);
        }
    }

    rb->closedir(dir);
}

static bool pick_track_from_filesystem(const uint32_t *history, int history_count,
                                       char *path, size_t path_size, uint32_t *hash)
{
    struct pc_music_pick pick;

    rb->memset(&pick, 0, sizeof(pick));
    pick_track_from_directory_recursive(PC_MUSIC_ROOT_PATH, history, history_count, &pick);
    if (pick.seen <= 0)
        return false;

    rb->strlcpy(path, pick.path, path_size);
    *hash = pick.hash;
    return true;
}

static bool pick_random_music_track(char *path, size_t path_size, uint32_t *hash)
{
    uint32_t history[PC_MUSIC_HISTORY_MAX];
    int history_count = load_music_history(history, ARRAYLEN(history));
    bool picked = false;

#ifdef HAVE_TAGCACHE
    picked = pick_track_from_database(history, history_count, path, path_size, hash);
#endif
    if (!picked)
        picked = pick_track_from_filesystem(history, history_count, path, path_size, hash);

    if (!picked && history_count > 0)
    {
        save_music_history(history, 0);
#ifdef HAVE_TAGCACHE
        picked = pick_track_from_database(NULL, 0, path, path_size, hash);
#endif
        if (!picked)
            picked = pick_track_from_filesystem(NULL, 0, path, path_size, hash);
    }

    return picked;
}

static bool play_music_track(const char *path)
{
    rb->audio_stop();
    rb->playlist_remove_all_tracks(NULL);
    if (rb->playlist_create(NULL, NULL) < 0)
        return false;
    if (rb->playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST, false, true) < 0)
        return false;

    rb->plugin_release_audio_buffer();
    rb->playlist_set_modified(NULL, true);
    rb->playlist_start(0, 0, 0);
    return true;
}

static void init_progress_defaults(struct pc_world_state *world)
{
    int i;

    world->pokeballs = PC_START_POKEBALLS;
    world->money = 0;
    world->travel_steps = 0;
    world->buddy_species = -1;
    world->buddy_steps = 0;
    world->secret_collected_bits = 0;
    world->player_trainer = 0;
    world->pokestop_index = -1;
    world->pokestop_last_wheel_angle = -1;
    rb->memset(world->pokestop_cooldowns, 0, sizeof(world->pokestop_cooldowns));
    rb->memset(world->owned_trainers, 0, sizeof(world->owned_trainers));
    world->owned_trainers[0] = 1;
    for (i = 0; i < PC_FIELD_ABILITY_COUNT; ++i)
    {
        world->ability_species[i] = -1;
        world->ability_owned[i] = 0;
    }
}

static int clamp_player_trainer_index(int index)
{
    if (index < 0 || index >= PC_PLAYER_TRAINER_COUNT)
        return 0;
    return index;
}

static void apply_player_trainer_choice(const struct pc_world_state *world)
{
    int index = clamp_player_trainer_index(world->player_trainer);
    pc_assets_set_player_trainer_name(pc_player_trainers[index].asset_name);
}

static void reload_player_trainer_assets(struct pc_world_state *world)
{
    int heading;
    int frame;

    apply_player_trainer_choice(world);
    for (heading = 0; heading < 4; ++heading)
    {
        for (frame = 0; frame < PC_WORLD_WALK_FRAMES; ++frame)
        {
            clear_bitmap(&world->assets.trainer[heading][frame]);
            pc_assets_load_world_trainer(&world->assets.trainer[heading][frame],
                                         heading, frame);
        }
    }
    pc_render_reset_trainer_asset();
}

static void sanitize_buddy(struct pc_world_state *world)
{
    if (world->buddy_species < 0 || world->buddy_species >= PC_POKEDEX_MAX ||
        world->caught_counts[world->buddy_species] == 0)
    {
        world->buddy_species = -1;
        world->buddy_steps = 0;
    }
    else if (world->buddy_steps < 0)
    {
        world->buddy_steps = 0;
    }
}

static void sanitize_player_trainer(struct pc_world_state *world)
{
    int i;

    world->player_trainer = clamp_player_trainer_index(world->player_trainer);
    world->owned_trainers[0] = 1;
    for (i = 0; i < PC_PLAYER_TRAINER_COUNT; ++i)
    {
        world->owned_trainers[i] = world->owned_trainers[i] ? 1 : 0;
    }
    if (!world->owned_trainers[world->player_trainer])
        world->player_trainer = 0;
}

static void apply_encounter_grace(struct pc_world_state *world,
                                  int grace_steps,
                                  int cooldown_frames)
{
    if (world == NULL)
        return;

    world->encounter_armed = false;
    world->encounter_grace_steps = MAX(world->encounter_grace_steps, grace_steps);
    world->encounter_cooldown = MAX(world->encounter_cooldown, cooldown_frames);
}

static void reset_transient_world_state(struct pc_world_state *world)
{
    world->pending_encounter = false;
    world->pending_species_index = -1;
    world->last_encounter_slot = -1;
    world->moving = false;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->walk_tick = 0;
    world->walk_frame = 1;
    world->encounter_grace_steps = 0;
    apply_encounter_grace(world,
                          PC_STARTUP_ENCOUNTER_GRACE_STEPS,
                          PC_STARTUP_ENCOUNTER_COOLDOWN);
}

static void clear_pending_encounter_state(struct pc_world_state *world)
{
    if (world == NULL)
        return;

    world->pending_encounter = false;
    world->pending_species_index = -1;
    world->last_encounter_slot = -1;
}

int pc_world_player_trainer_count(void)
{
    return PC_PLAYER_TRAINER_COUNT;
}

const char *pc_world_player_trainer_name(int index)
{
    index = clamp_player_trainer_index(index);
    return pc_player_trainers[index].display_name;
}

int pc_world_player_trainer_cost(int index)
{
    index = clamp_player_trainer_index(index);
    return pc_player_trainers[index].cost;
}


static const char *field_ability_name(enum pc_field_ability ability)
{
    switch (ability)
    {
        case PC_FIELD_ABILITY_SURF:
            return "HM03 Surf";

        case PC_FIELD_ABILITY_CUT:
            return "HM01 Cut";

        default:
            return "Field move";
    }
}

static bool species_id_can_learn_surf(int species_id)
{
    switch (species_id)
    {
        case 7:
        case 8:
        case 9:
        case 54:
        case 55:
        case 60:
        case 61:
        case 62:
        case 79:
        case 80:
        case 118:
        case 119:
        case 120:
        case 121:
        case 129:
        case 130:
        case 158:
        case 159:
        case 160:
        case 183:
        case 184:
        case 194:
        case 195:
            return true;

        default:
            return false;
    }
}

static bool species_id_can_learn_cut(int species_id)
{
    switch (species_id)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 27:
        case 28:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        case 69:
        case 70:
        case 71:
        case 83:
        case 98:
        case 99:
        case 123:
        case 127:
        case 151:
            return true;

        default:
            return false;
    }
}

static bool species_can_use_field_ability(int species_index,
                                          enum pc_field_ability ability)
{
    const struct pc_creature_def *creature = pc_assets_get_creature(species_index);

    if (creature == NULL)
        return false;

    switch (ability)
    {
        case PC_FIELD_ABILITY_SURF:
            return species_id_can_learn_surf(creature->species_id);

        case PC_FIELD_ABILITY_CUT:
            return species_id_can_learn_cut(creature->species_id);

        default:
            return false;
    }
}

static int assigned_field_ability_species(const struct pc_world_state *world,
                                          enum pc_field_ability ability)
{
    int index;

    if (ability >= PC_FIELD_ABILITY_COUNT)
        return -1;
    if (!world->ability_owned[ability])
        return -1;

    index = world->ability_species[ability];
    if (index < 0 || index >= PC_POKEDEX_MAX)
        return -1;
    if (world->caught_counts[index] == 0)
        return -1;
    if (!species_can_use_field_ability(index, ability))
        return -1;

    return index;
}

static void sanitize_field_abilities(struct pc_world_state *world)
{
    int i;

    for (i = 0; i < PC_FIELD_ABILITY_COUNT; ++i)
    {
        if (!world->ability_owned[i] ||
            assigned_field_ability_species(world, i) < 0)
        {
            world->ability_species[i] = -1;
        }
    }
}

static int field_ability_cost(enum pc_field_ability ability)
{
    switch (ability)
    {
        case PC_FIELD_ABILITY_SURF:
            return PC_MART_SURF_COST;

        case PC_FIELD_ABILITY_CUT:
            return PC_MART_CUT_COST;

        default:
            return 0;
    }
}

static int random_spawn_delay(void)
{
    return PC_WORLD_SPAWN_RESPAWN_MIN +
           (rb->rand() % (PC_WORLD_SPAWN_RESPAWN_MAX - PC_WORLD_SPAWN_RESPAWN_MIN + 1));
}

static int random_spawn_lifetime(void)
{
    return PC_WORLD_SPAWN_LIFETIME_MIN +
           (rb->rand() % (PC_WORLD_SPAWN_LIFETIME_MAX - PC_WORLD_SPAWN_LIFETIME_MIN + 1));
}

static const struct pc_spawn_entry *spawn_table_for_region(enum pc_outdoor_region region,
                                                           int *count)
{
    switch (region)
    {
        case PC_OUTDOOR_REGION_FOREST:
            *count = ARRAYLEN(pc_forest_spawn_table);
            return pc_forest_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE22:
            *count = ARRAYLEN(pc_route22_spawn_table);
            return pc_route22_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE2:
            *count = ARRAYLEN(pc_route2_spawn_table);
            return pc_route2_spawn_table;

        case PC_OUTDOOR_REGION_VIRIDIAN:
            *count = 0;
            return pc_empty_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE1:
            *count = ARRAYLEN(pc_route1_spawn_table);
            return pc_route1_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE21:
            *count = ARRAYLEN(pc_route21_spawn_table);
            return pc_route21_spawn_table;

        case PC_OUTDOOR_REGION_PEWTER:
            *count = 0;
            return pc_empty_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE3:
            *count = ARRAYLEN(pc_route3_spawn_table);
            return pc_route3_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE4:
            *count = ARRAYLEN(pc_route4_spawn_table);
            return pc_route4_spawn_table;

        case PC_OUTDOOR_REGION_CERULEAN:
            *count = 0;
            return pc_empty_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE24:
            *count = ARRAYLEN(pc_route24_spawn_table);
            return pc_route24_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE25:
            *count = ARRAYLEN(pc_route25_spawn_table);
            return pc_route25_spawn_table;

        case PC_OUTDOOR_REGION_PALLET:
        default:
            *count = 0;
            return pc_empty_spawn_table;
    }
}

static bool outdoor_region_has_spawns(enum pc_outdoor_region region)
{
    int count;

    (void)spawn_table_for_region(region, &count);
    return count > 0;
}

static int random_species_for_region(enum pc_outdoor_region region)
{
    const struct pc_spawn_entry *table;
    int count;
    int total_weight = 0;
    int roll;
    int i;

    table = spawn_table_for_region(region, &count);
    for (i = 0; i < count; ++i)
        total_weight += MAX(0, table[i].weight);

    if (total_weight <= 0)
        return -1;

    roll = rb->rand() % total_weight;
    for (i = 0; i < count; ++i)
    {
        int weight = MAX(0, table[i].weight);

        if (roll < weight)
        {
            int species_index = pc_assets_find_species_index(table[i].species_id);

            if (species_index >= 0)
                return species_index;
            break;
        }
        roll -= weight;
    }

    return -1;
}

static void copy_block_rows(unsigned char dst[PC_WORLD_MAP_MAX_H][PC_WORLD_MAP_MAX_W],
                            int dst_y,
                            const unsigned char *src,
                            int rows, int cols)
{
    int y;

    for (y = 0; y < rows; ++y)
        rb->memcpy(dst[dst_y + y], src + y * cols, cols);
}

static enum pc_outdoor_region current_outdoor_region(const struct pc_world_state *world)
{
    switch (world->scene)
    {
        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            return PC_OUTDOOR_REGION_FOREST;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            return PC_OUTDOOR_REGION_ROUTE2;

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            return PC_OUTDOOR_REGION_VIRIDIAN;

        case PC_WORLD_SCENE_ROUTE22:
            return PC_OUTDOOR_REGION_ROUTE22;

        case PC_WORLD_SCENE_ROUTE1_SOUTH:
            return PC_OUTDOOR_REGION_ROUTE1;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            return PC_OUTDOOR_REGION_ROUTE21;

        case PC_WORLD_SCENE_PEWTER:
            return PC_OUTDOOR_REGION_PEWTER;

        case PC_WORLD_SCENE_ROUTE3:
            return PC_OUTDOOR_REGION_ROUTE3;

        case PC_WORLD_SCENE_ROUTE4:
            return PC_OUTDOOR_REGION_ROUTE4;

        case PC_WORLD_SCENE_CERULEAN:
            return PC_OUTDOOR_REGION_CERULEAN;

        case PC_WORLD_SCENE_ROUTE24:
            return PC_OUTDOOR_REGION_ROUTE24;

        case PC_WORLD_SCENE_ROUTE25:
            return PC_OUTDOOR_REGION_ROUTE25;

        default:
            return PC_OUTDOOR_REGION_PALLET;
    }
}

int pc_world_nearby_count(const struct pc_world_state *world)
{
    const struct pc_spawn_entry *table;
    int count;
    int unique = 0;
    int i;
    int last_species_id = -1;

    if (world == NULL || !outdoor_scene(world->scene))
        return 0;

    table = spawn_table_for_region(current_outdoor_region(world), &count);
    for (i = 0; i < count; ++i)
    {
        if (table[i].species_id == last_species_id)
            continue;
        last_species_id = table[i].species_id;
        unique++;
    }

    return unique;
}

bool pc_world_nearby_name(const struct pc_world_state *world, int index,
                          char *buffer, size_t buffer_size)
{
    const struct pc_spawn_entry *table;
    int count;
    int unique = 0;
    int i;
    int last_species_id = -1;

    if (buffer == NULL || buffer_size == 0)
        return false;
    buffer[0] = '\0';

    if (world == NULL || !outdoor_scene(world->scene))
        return false;

    table = spawn_table_for_region(current_outdoor_region(world), &count);
    for (i = 0; i < count; ++i)
    {
        const struct pc_creature_def *creature;

        if (table[i].species_id == last_species_id)
            continue;
        last_species_id = table[i].species_id;
        if (unique++ != index)
            continue;

        creature = pc_assets_get_creature(pc_assets_find_species_index(table[i].species_id));
        if (creature == NULL)
            return false;

        rb->strlcpy(buffer, creature->name, buffer_size);
        return true;
    }

    return false;
}

static void refresh_outdoor_region_state(struct pc_world_state *world, bool force_spawns)
{
    enum pc_outdoor_region region;

    if (!outdoor_scene(world->scene))
        return;

    region = current_outdoor_region(world);
    if (!force_spawns && world->outdoor_region == (int)region)
        return;

    world->outdoor_region = (int)region;
    switch (region)
    {
        case PC_OUTDOOR_REGION_FOREST:
            set_banner(world, "Viridian Forest", "North to Pewter, south to Route 2");
            break;

        case PC_OUTDOOR_REGION_ROUTE2:
            set_banner(world, "Route 2", "South to Viridian, north to Pewter");
            break;

        case PC_OUTDOOR_REGION_VIRIDIAN:
            set_banner(world, "Viridian City", "West to Route 22, north to Route 2");
            break;

        case PC_OUTDOOR_REGION_ROUTE22:
            set_banner(world, "Route 22", "East to Viridian, west to League Gate");
            break;

        case PC_OUTDOOR_REGION_ROUTE1:
            set_banner(world, "Route 1", "North to Viridian, south to Pallet");
            break;

        case PC_OUTDOOR_REGION_ROUTE21:
            set_banner(world, "Route 21", "Northern waters off Pallet");
            break;

        case PC_OUTDOOR_REGION_PEWTER:
            set_banner(world, "Pewter City", "South to Route 2");
            break;

        case PC_OUTDOOR_REGION_ROUTE3:
            set_banner(world, "Route 3", "East to Mt. Moon, west to Pewter");
            break;

        case PC_OUTDOOR_REGION_ROUTE4:
            set_banner(world, "Route 4", "Mt. Moon to the west, Cerulean ahead");
            break;

        case PC_OUTDOOR_REGION_CERULEAN:
            set_banner(world, "Cerulean City", "Misty's city beside Route 4");
            break;

        case PC_OUTDOOR_REGION_ROUTE24:
            set_banner(world, "Route 24", "Nugget Bridge north of Cerulean");
            break;

        case PC_OUTDOOR_REGION_ROUTE25:
            set_banner(world, "Route 25", "Bill's cape stretches east");
            break;

        case PC_OUTDOOR_REGION_PALLET:
        default:
            set_banner(world, "Pallet Town", "North to Route 1, south to Route 21");
            break;
    }

    init_spawns(world);
}

static int scene_subtile_px(void)
{
    return PC_WORLD_SUBTILE_SIZE;
}

static int block_screen_x(const struct pc_world_state *world, int block_x)
{
    (void)world;
    return block_x * PC_WORLD_TILE_SIZE;
}

static int block_screen_y(const struct pc_world_state *world, int block_y)
{
    (void)world;
    return block_y * PC_WORLD_TILE_SIZE;
}

static int block_center_x(const struct pc_world_state *world, int block_x)
{
    return block_screen_x(world, block_x) + PC_WORLD_TILE_SIZE / 2;
}

static int block_center_y(const struct pc_world_state *world, int block_y)
{
    return block_screen_y(world, block_y) + PC_WORLD_TILE_SIZE / 2;
}

static int subtile_center_x(const struct pc_world_state *world, int tile_x)
{
    (void)world;
    return tile_x * scene_subtile_px() + scene_subtile_px() / 2;
}

static int subtile_center_y(const struct pc_world_state *world, int tile_y)
{
    (void)world;
    return tile_y * scene_subtile_px() + scene_subtile_px() / 2;
}

enum pc_scene_tileset_group {
    PC_SCENE_TILESET_OVERWORLD = 0,
    PC_SCENE_TILESET_REDS_HOUSE,
    PC_SCENE_TILESET_HOUSE,
    PC_SCENE_TILESET_POKECENTER,
    PC_SCENE_TILESET_GYM,
    PC_SCENE_TILESET_FOREST,
    PC_SCENE_TILESET_GATE,
    PC_SCENE_TILESET_LAB,
    PC_SCENE_TILESET_CAVERN,
    PC_SCENE_TILESET_CLUB,
    PC_SCENE_TILESET_SHIP
};

static enum pc_scene_tileset_group scene_tileset_group(enum pc_world_scene scene)
{
    switch (scene)
    {
        case PC_WORLD_SCENE_HOUSE_1F:
        case PC_WORLD_SCENE_HOUSE_2F:
            return PC_SCENE_TILESET_REDS_HOUSE;

        case PC_WORLD_SCENE_BLUES_HOUSE:
        case PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE:
        case PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE:
        case PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE:
        case PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE:
        case PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE:
        case PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE:
        case PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE:
        case PC_WORLD_SCENE_BILLS_HOUSE:
            return PC_SCENE_TILESET_HOUSE;

        case PC_WORLD_SCENE_VIRIDIAN_MART:
        case PC_WORLD_SCENE_VIRIDIAN_POKECENTER:
        case PC_WORLD_SCENE_PEWTER_MART:
        case PC_WORLD_SCENE_PEWTER_POKECENTER:
        case PC_WORLD_SCENE_MT_MOON_POKECENTER:
        case PC_WORLD_SCENE_CERULEAN_MART:
        case PC_WORLD_SCENE_CERULEAN_POKECENTER:
            return PC_SCENE_TILESET_POKECENTER;

        case PC_WORLD_SCENE_VIRIDIAN_GYM:
        case PC_WORLD_SCENE_PEWTER_GYM:
        case PC_WORLD_SCENE_CERULEAN_GYM:
            return PC_SCENE_TILESET_GYM;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            return PC_SCENE_TILESET_FOREST;

        case PC_WORLD_SCENE_ROUTE2_GATE:
        case PC_WORLD_SCENE_ROUTE22_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE:
        case PC_WORLD_SCENE_MUSEUM_1F:
        case PC_WORLD_SCENE_MUSEUM_2F:
            return PC_SCENE_TILESET_GATE;

        case PC_WORLD_SCENE_OAKS_LAB:
            return PC_SCENE_TILESET_LAB;

        case PC_WORLD_SCENE_MT_MOON_1F:
        case PC_WORLD_SCENE_MT_MOON_B1F:
        case PC_WORLD_SCENE_MT_MOON_B2F:
            return PC_SCENE_TILESET_CAVERN;

        case PC_WORLD_SCENE_BIKE_SHOP:
            return PC_SCENE_TILESET_CLUB;

        case PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE:
            return PC_SCENE_TILESET_SHIP;

        case PC_WORLD_SCENE_PALLET:
        case PC_WORLD_SCENE_ROUTE1_SOUTH:
        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
        case PC_WORLD_SCENE_ROUTE22:
        case PC_WORLD_SCENE_ROUTE2_SOUTH:
        case PC_WORLD_SCENE_ROUTE21_NORTH:
        case PC_WORLD_SCENE_PEWTER:
        case PC_WORLD_SCENE_ROUTE3:
        case PC_WORLD_SCENE_ROUTE4:
        case PC_WORLD_SCENE_CERULEAN:
        case PC_WORLD_SCENE_ROUTE24:
        case PC_WORLD_SCENE_ROUTE25:
        default:
            return PC_SCENE_TILESET_OVERWORLD;
    }
}

static void select_collision_tileset(enum pc_world_scene scene,
                                     const unsigned char (**blocks)[4][4],
                                     int *block_count,
                                     const unsigned char **passable_tiles,
                                     size_t *passable_count)
{
    switch (scene_tileset_group(scene))
    {
        case PC_SCENE_TILESET_REDS_HOUSE:
            *blocks = pc_red_reds_house_blocks;
            *block_count = PC_RED_REDS_HOUSE_BLOCK_COUNT;
            *passable_tiles = pc_reds_house_passable_tiles;
            *passable_count = ARRAYLEN(pc_reds_house_passable_tiles);
            break;

        case PC_SCENE_TILESET_HOUSE:
            *blocks = pc_red_house_blocks;
            *block_count = PC_RED_HOUSE_BLOCK_COUNT;
            *passable_tiles = pc_house_passable_tiles;
            *passable_count = ARRAYLEN(pc_house_passable_tiles);
            break;

        case PC_SCENE_TILESET_POKECENTER:
            *blocks = pc_red_pokecenter_blocks;
            *block_count = PC_RED_POKECENTER_BLOCK_COUNT;
            *passable_tiles = pc_pokecenter_passable_tiles;
            *passable_count = ARRAYLEN(pc_pokecenter_passable_tiles);
            break;

        case PC_SCENE_TILESET_GYM:
            *blocks = pc_red_dojo_blocks;
            *block_count = PC_RED_DOJO_BLOCK_COUNT;
            *passable_tiles = pc_gym_passable_tiles;
            *passable_count = ARRAYLEN(pc_gym_passable_tiles);
            break;

        case PC_SCENE_TILESET_FOREST:
            *blocks = pc_red_forest_blocks;
            *block_count = PC_RED_FOREST_BLOCK_COUNT;
            *passable_tiles = pc_forest_passable_tiles;
            *passable_count = ARRAYLEN(pc_forest_passable_tiles);
            break;

        case PC_SCENE_TILESET_GATE:
            *blocks = pc_red_gate_blocks;
            *block_count = PC_RED_GATE_BLOCK_COUNT;
            *passable_tiles = pc_gate_passable_tiles;
            *passable_count = ARRAYLEN(pc_gate_passable_tiles);
            break;

        case PC_SCENE_TILESET_LAB:
            *blocks = pc_red_lab_blocks;
            *block_count = PC_RED_LAB_BLOCK_COUNT;
            *passable_tiles = pc_lab_passable_tiles;
            *passable_count = ARRAYLEN(pc_lab_passable_tiles);
            break;

        case PC_SCENE_TILESET_CAVERN:
            *blocks = pc_red_cavern_blocks;
            *block_count = PC_RED_CAVERN_BLOCK_COUNT;
            *passable_tiles = pc_cavern_passable_tiles;
            *passable_count = ARRAYLEN(pc_cavern_passable_tiles);
            break;

        case PC_SCENE_TILESET_CLUB:
            *blocks = pc_red_club_blocks;
            *block_count = PC_RED_CLUB_BLOCK_COUNT;
            *passable_tiles = pc_club_passable_tiles;
            *passable_count = ARRAYLEN(pc_club_passable_tiles);
            break;

        case PC_SCENE_TILESET_SHIP:
            *blocks = pc_red_ship_blocks;
            *block_count = PC_RED_SHIP_BLOCK_COUNT;
            *passable_tiles = pc_ship_passable_tiles;
            *passable_count = ARRAYLEN(pc_ship_passable_tiles);
            break;

        case PC_SCENE_TILESET_OVERWORLD:
        default:
            *blocks = pc_red_overworld_blocks;
            *block_count = PC_RED_BLOCK_COUNT;
            *passable_tiles = pc_overworld_passable_tiles;
            *passable_count = ARRAYLEN(pc_overworld_passable_tiles);
            break;
    }
}

static void set_player_to_metatile(struct pc_world_state *world,
                                   int tile_x, int tile_y)
{
    int step = PC_WORLD_TILE_SIZE / 2;

    world->player_x = tile_x * step + step / 2;
    world->player_y = tile_y * step + step / 2 - PC_WORLD_FOOT_Y_OFFSET;
}

static bool outdoor_scene(enum pc_world_scene scene)
{
    return scene == PC_WORLD_SCENE_PALLET ||
           scene == PC_WORLD_SCENE_ROUTE1_SOUTH ||
           scene == PC_WORLD_SCENE_VIRIDIAN_SOUTH ||
           scene == PC_WORLD_SCENE_ROUTE22 ||
           scene == PC_WORLD_SCENE_ROUTE2_SOUTH ||
           scene == PC_WORLD_SCENE_ROUTE21_NORTH ||
           scene == PC_WORLD_SCENE_VIRIDIAN_FOREST ||
           scene == PC_WORLD_SCENE_PEWTER ||
           scene == PC_WORLD_SCENE_ROUTE3 ||
           scene == PC_WORLD_SCENE_ROUTE4 ||
           scene == PC_WORLD_SCENE_CERULEAN ||
           scene == PC_WORLD_SCENE_ROUTE24 ||
           scene == PC_WORLD_SCENE_ROUTE25;
}

static bool scene_tile_passable(const struct pc_world_state *world,
                                unsigned char tile_id)
{
    const unsigned char (*blocks)[4][4];
    const unsigned char *passable_tiles;
    size_t passable_count;
    int block_count;
    size_t i;

    (void)blocks;
    (void)block_count;
    select_collision_tileset(world->scene, &blocks, &block_count,
                             &passable_tiles, &passable_count);

    if (world->scene == PC_WORLD_SCENE_ROUTE21_NORTH &&
        (tile_id == 0x14 || tile_id == 0x32 || tile_id == 0x48))
    {
        return true;
    }

    for (i = 0; i < passable_count; ++i)
    {
        if (passable_tiles[i] == tile_id)
            return true;
    }

    return false;
}

static int scene_walkable_override(const struct pc_world_state *world,
                                   int metatile_x, int metatile_y)
{
    const char *const *mask = NULL;

    switch (world->scene)
    {
        case PC_WORLD_SCENE_HOUSE_1F:
            mask = pc_reds_house_1f_walk_mask;
            break;

        case PC_WORLD_SCENE_HOUSE_2F:
            mask = pc_reds_house_2f_walk_mask;
            break;

        default:
            break;
    }

    if (mask == NULL)
        return -1;
    if (metatile_x < 0 || metatile_x >= 8 || metatile_y < 0 || metatile_y >= 8)
        return 0;

    return mask[metatile_y][metatile_x] == '.' ? 1 : 0;
}

static bool metatile_walkable(const struct pc_world_state *world,
                              int metatile_x, int metatile_y)
{
    const unsigned char (*blocks)[4][4];
    const unsigned char *passable_tiles;
    size_t passable_count;
    int block_count;
    int block_x;
    int block_y;
    int cell_x;
    int cell_y;
    unsigned char block_id;
    unsigned char tile_left;
    unsigned char tile_right;

    if (metatile_x < 0 || metatile_y < 0 ||
        metatile_x >= world->map_w * 2 || metatile_y >= world->map_h * 2)
    {
        return false;
    }

    switch (scene_walkable_override(world, metatile_x, metatile_y))
    {
        case 0:
            return false;

        case 1:
            return true;

        default:
            break;
    }

    select_collision_tileset(world->scene, &blocks, &block_count,
                             &passable_tiles, &passable_count);
    (void)passable_tiles;
    (void)passable_count;

    block_x = metatile_x / 2;
    block_y = metatile_y / 2;
    cell_x = (metatile_x % 2) * 2;
    cell_y = (metatile_y % 2) * 2;
    block_id = world->tiles[block_y][block_x];
    if (block_id >= block_count)
        return false;

    tile_left = blocks[block_id][cell_y + 1][cell_x];
    tile_right = blocks[block_id][cell_y + 1][cell_x + 1];
    return scene_tile_passable(world, tile_left) &&
           scene_tile_passable(world, tile_right);
}

static bool warp_entry_walkable(const struct pc_world_state *world,
                                int metatile_x, int metatile_y,
                                int move_dx, int move_dy)
{
    if (move_dx != 0)
        return false;

    switch (world->scene)
    {
        case PC_WORLD_SCENE_PALLET:
            return move_dy < 0 &&
                   ((metatile_y == 5 &&
                     ((metatile_x >= 5 && metatile_x <= 6) ||
                      (metatile_x >= 13 && metatile_x <= 14))) ||
                    (metatile_y == 11 &&
                     metatile_x >= 12 && metatile_x <= 13));

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            return move_dy < 0 &&
                   ((metatile_y == 25 && metatile_x >= 23 && metatile_x <= 24) ||
                    (metatile_y == 19 && metatile_x >= 29 && metatile_x <= 30) ||
                    (metatile_y == 15 && metatile_x >= 21 && metatile_x <= 22) ||
                    (metatile_y == 9 && metatile_x >= 21 && metatile_x <= 22) ||
                    (metatile_y == 7 && metatile_x >= 32 && metatile_x <= 33));

        case PC_WORLD_SCENE_ROUTE22:
            return move_dy < 0 &&
                   metatile_y == 5 &&
                   metatile_x == 8;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            return ((move_dy < 0 &&
                     ((metatile_y == 11 && metatile_x == 3) ||
                      (metatile_y == 19 && metatile_x == 15) ||
                      (metatile_y == 35 && metatile_x == 16) ||
                      (metatile_y == 43 && metatile_x == 3))) ||
                    (move_dy > 0 &&
                     metatile_y == 39 &&
                     metatile_x == 15));

        case PC_WORLD_SCENE_PEWTER:
            return move_dy < 0 &&
                   ((metatile_y == 7 &&
                     ((metatile_x >= 14 && metatile_x <= 15) ||
                      metatile_x == 19)) ||
                    (metatile_y == 17 && metatile_x == 16) ||
                    (metatile_y == 13 && metatile_x == 29) ||
                    (metatile_y == 17 && metatile_x == 23) ||
                    (metatile_y == 29 && metatile_x == 7) ||
                    (metatile_y == 25 && metatile_x == 13));

        case PC_WORLD_SCENE_ROUTE4:
            return move_dy < 0 &&
                   (metatile_y == 5 &&
                    (metatile_x == 11 || metatile_x == 18 || metatile_x == 24));

        case PC_WORLD_SCENE_CERULEAN:
            return move_dy < 0 &&
                   ((metatile_y == 11 &&
                     (metatile_x == 27 || metatile_x == 13 || metatile_x == 9)) ||
                    (metatile_y == 17 && metatile_x == 19) ||
                    (metatile_y == 19 && metatile_x == 30) ||
                    (metatile_y == 25 && (metatile_x == 13 || metatile_x == 25)) ||
                    (metatile_y == 9 && (metatile_x == 27 || metatile_x == 9)));

        case PC_WORLD_SCENE_ROUTE25:
            return move_dy < 0 &&
                   metatile_y == 3 &&
                   metatile_x == 45;

        case PC_WORLD_SCENE_VIRIDIAN_MART:
            return move_dy > 0 &&
                   metatile_y == 7 &&
                   metatile_x >= 3 && metatile_x <= 4;

        case PC_WORLD_SCENE_PEWTER_MART:
        case PC_WORLD_SCENE_PEWTER_POKECENTER:
        case PC_WORLD_SCENE_MT_MOON_POKECENTER:
        case PC_WORLD_SCENE_CERULEAN_POKECENTER:
        case PC_WORLD_SCENE_CERULEAN_MART:
            return move_dy > 0 &&
                   metatile_y == 7 &&
                   metatile_x >= 3 && metatile_x <= 4;

        case PC_WORLD_SCENE_BLUES_HOUSE:
        case PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE:
        case PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE:
        case PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE:
        case PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE:
        case PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE:
        case PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE:
        case PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE:
        case PC_WORLD_SCENE_BILLS_HOUSE:
        case PC_WORLD_SCENE_BIKE_SHOP:
            return move_dy > 0 &&
                   metatile_y == 7 &&
                   metatile_x >= 2 && metatile_x <= 3;

        case PC_WORLD_SCENE_HOUSE_1F:
            if (move_dy > 0 &&
                metatile_y == 7 &&
                metatile_x >= 2 && metatile_x <= 3)
            {
                return true;
            }

            return move_dy < 0 &&
                   metatile_x >= 6 && metatile_x <= 7 &&
                   metatile_y >= 1 && metatile_y <= 2;

        case PC_WORLD_SCENE_HOUSE_2F:
            return move_dy > 0 &&
                   metatile_x >= 6 && metatile_x <= 7 &&
                   metatile_y >= 1 && metatile_y <= 2;

        case PC_WORLD_SCENE_OAKS_LAB:
            return move_dy > 0 &&
                   metatile_y == 11 &&
                   metatile_x >= 4 && metatile_x <= 5;

        case PC_WORLD_SCENE_ROUTE2_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE:
            return ((move_dy < 0 &&
                     metatile_y == 0 &&
                     metatile_x >= 4 && metatile_x <= 5) ||
                    (move_dy > 0 &&
                     metatile_y == 7 &&
                     metatile_x >= 4 && metatile_x <= 5));

        case PC_WORLD_SCENE_VIRIDIAN_POKECENTER:
            return move_dy > 0 &&
                   metatile_y == 7 &&
                   metatile_x >= 3 && metatile_x <= 4;

        case PC_WORLD_SCENE_MUSEUM_1F:
            if (move_dy > 0 &&
                ((metatile_y == 7 && metatile_x >= 10 && metatile_x <= 11) ||
                 (metatile_y == 7 && metatile_x >= 16 && metatile_x <= 17)))
            {
                return true;
            }
            return move_dy > 0 &&
                   metatile_y == 7 &&
                   metatile_x == 7;

        case PC_WORLD_SCENE_MUSEUM_2F:
            return move_dy > 0 &&
                   metatile_y == 7 &&
                   metatile_x == 7;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            return ((move_dy < 0 &&
                     metatile_y == 0 &&
                     metatile_x >= 1 && metatile_x <= 2) ||
                    (move_dy > 0 &&
                     metatile_y == 47 &&
                     metatile_x >= 15 && metatile_x <= 18));

        case PC_WORLD_SCENE_VIRIDIAN_GYM:
            return move_dy > 0 &&
                   metatile_y == 17 &&
                   metatile_x >= 16 && metatile_x <= 17;

        case PC_WORLD_SCENE_PEWTER_GYM:
            return move_dy > 0 &&
                   metatile_y == 13 &&
                   metatile_x >= 4 && metatile_x <= 5;

        case PC_WORLD_SCENE_CERULEAN_GYM:
            return move_dy > 0 &&
                   metatile_y == 13 &&
                   metatile_x >= 4 && metatile_x <= 5;

        case PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE:
            if (move_dy > 0 &&
                metatile_y == 7 &&
                metatile_x >= 2 && metatile_x <= 3)
            {
                return true;
            }
            return move_dy < 0 &&
                   metatile_y == 0 &&
                   metatile_x == 2;

        default:
            return false;
    }
}

static unsigned char block_at(const struct pc_world_state *world, int block_x, int block_y)
{
    if (block_x < 0 || block_y < 0 ||
        block_x >= world->map_w || block_y >= world->map_h)
    {
        return 0xff;
    }

    return world->tiles[block_y][block_x];
}

static bool ledge_block(unsigned char block_id)
{
    return block_id == 0x07 ||
           block_id == 0x1a ||
           block_id == 0x1b ||
           block_id == 0x2f;
}

static bool point_walkable(const struct pc_world_state *world, int x, int y)
{
    int tx;
    int ty;
    int step = PC_WORLD_TILE_SIZE / 2;

    if (x < 0 || y < 0)
        return false;

    tx = x / step;
    ty = y / step;
    return metatile_walkable(world, tx, ty);
}

static bool player_walkable(const struct pc_world_state *world, int x, int y)
{
    int foot_y = y + PC_WORLD_FOOT_Y_OFFSET;

    return point_walkable(world, x, foot_y);
}

static bool player_step_allowed(const struct pc_world_state *world,
                                int x, int y,
                                int move_dx, int move_dy)
{
    int step = PC_WORLD_TILE_SIZE / 2;
    int foot_y = y + PC_WORLD_FOOT_Y_OFFSET;
    int metatile_x;
    int metatile_y;

    if (point_walkable(world, x, foot_y))
        return true;

    metatile_x = x / step;
    metatile_y = foot_y / step;
    return warp_entry_walkable(world, metatile_x, metatile_y, move_dx, move_dy);
}

static bool try_start_ledge_jump(struct pc_world_state *world)
{
    int tile_x;
    int tile_y;
    int step = PC_WORLD_TILE_SIZE / 2;
    unsigned char ledge_id;

    if (!outdoor_scene(world->scene) || world->heading != PC_HEADING_S)
        return false;

    tile_x = world->player_x / step;
    tile_y = (world->player_y + PC_WORLD_FOOT_Y_OFFSET) / step;
    ledge_id = block_at(world, tile_x / 2, (tile_y + 1) / 2);
    if (!ledge_block(ledge_id))
        return false;

    if (!player_walkable(world, world->player_x,
                         world->player_y + PC_WORLD_TILE_SIZE))
        return false;

    world->step_dx = 0;
    world->step_dy = 1;
    world->step_remaining = PC_WORLD_TILE_SIZE;
    return true;
}

static void init_assets(struct pc_world_state *world)
{
    int heading;
    int frame;
    int i;

    rb->memset(&world->assets, 0, sizeof(world->assets));
    apply_player_trainer_choice(world);
    for (heading = 0; heading < 4; ++heading)
    {
        for (frame = 0; frame < PC_WORLD_WALK_FRAMES; ++frame)
        {
            world->assets.trainer[heading][frame].pixels =
                pc_world_trainer_pixels[heading][frame];
            world->assets.trainer[heading][frame].capacity = PC_WORLD_TRAINER_BYTES;
            clear_bitmap(&world->assets.trainer[heading][frame]);
            pc_assets_load_world_trainer(&world->assets.trainer[heading][frame],
                                         heading, frame);
        }
    }

    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        world->assets.creature[i].pixels = pc_world_creature_pixels[i];
        world->assets.creature[i].capacity = PC_WORLD_CREATURE_BYTES;
        clear_bitmap(&world->assets.creature[i]);
    }

    world->assets.dex_creature.pixels = pc_world_dex_pixels;
    world->assets.dex_creature.capacity = PC_WORLD_CREATURE_BYTES;
    clear_bitmap(&world->assets.dex_creature);
}

static void load_scene_data(struct pc_world_state *world, enum pc_world_scene scene)
{
    world->scene = scene;
    world->map_dirty = true;

    rb->memset(world->tiles, 0, sizeof(world->tiles));

    switch (scene)
    {
        case PC_WORLD_SCENE_PALLET:
            world->map_w = PC_PALLET_MAP_W;
            world->map_h = PC_PALLET_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_pallet_blocks[0][0],
                            PC_PALLET_MAP_H, PC_PALLET_MAP_W);
            world->home_x = subtile_center_x(world, 5);
            world->home_y = subtile_center_y(world, 6) - PC_WORLD_FOOT_Y_OFFSET;
            set_banner(world, "Pallet Town", "North to Route 1, south to Route 21");
            break;

        case PC_WORLD_SCENE_ROUTE1_SOUTH:
            world->map_w = PC_ROUTE1_MAP_W;
            world->map_h = PC_ROUTE1_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route1_blocks[0][0],
                            PC_ROUTE1_MAP_H, PC_ROUTE1_MAP_W);
            set_banner(world, "Route 1", "North to Viridian, south to Pallet");
            break;

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            world->map_w = PC_VIRIDIAN_MAP_W;
            world->map_h = PC_VIRIDIAN_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_viridian_blocks[0][0],
                            PC_VIRIDIAN_MAP_H, PC_VIRIDIAN_MAP_W);
            set_banner(world, "Viridian City", "West to Route 22, north to Route 2");
            break;

        case PC_WORLD_SCENE_ROUTE22:
            world->map_w = PC_ROUTE22_MAP_W;
            world->map_h = PC_ROUTE22_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route22_blocks[0][0],
                            PC_ROUTE22_MAP_H, PC_ROUTE22_MAP_W);
            set_banner(world, "Route 22", "East to Viridian, west to League Gate");
            break;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            world->map_w = PC_ROUTE2_MAP_W;
            world->map_h = PC_ROUTE2_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route2_blocks[0][0],
                            PC_ROUTE2_MAP_H, PC_ROUTE2_MAP_W);
            set_banner(world, "Route 2", "South to Viridian, north to Pewter");
            break;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            world->map_w = PC_FOREST_MAP_W;
            world->map_h = PC_FOREST_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_viridian_forest_blocks[0][0],
                            PC_FOREST_MAP_H, PC_FOREST_MAP_W);
            set_banner(world, "Viridian Forest", "North to Pewter, south to Route 2");
            break;

        case PC_WORLD_SCENE_PEWTER:
            world->map_w = PC_PEWTER_MAP_W;
            world->map_h = PC_PEWTER_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_pewter_blocks[0][0],
                            PC_PEWTER_MAP_H, PC_PEWTER_MAP_W);
            set_banner(world, "Pewter City", "South to Route 2, east to Route 3");
            break;

        case PC_WORLD_SCENE_MUSEUM_1F:
            world->map_w = PC_MUSEUM_1F_MAP_W;
            world->map_h = PC_MUSEUM_1F_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_MUSEUM_1F_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_MUSEUM_1F_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_museum_1f_blocks[0][0],
                            PC_MUSEUM_1F_MAP_H, PC_MUSEUM_1F_MAP_W);
            set_banner(world, "Pewter Museum 1F", "Ancient fossils and exhibits");
            break;

        case PC_WORLD_SCENE_MUSEUM_2F:
            world->map_w = PC_MUSEUM_2F_MAP_W;
            world->map_h = PC_MUSEUM_2F_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_MUSEUM_2F_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_MUSEUM_2F_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_museum_2f_blocks[0][0],
                            PC_MUSEUM_2F_MAP_H, PC_MUSEUM_2F_MAP_W);
            set_banner(world, "Pewter Museum 2F", "Moon Stone and shuttle displays");
            break;

        case PC_WORLD_SCENE_ROUTE3:
            world->map_w = PC_ROUTE3_MAP_W;
            world->map_h = PC_ROUTE3_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route3_blocks[0][0],
                            PC_ROUTE3_MAP_H, PC_ROUTE3_MAP_W);
            set_banner(world, "Route 3", "East to Mt. Moon, west to Pewter");
            break;

        case PC_WORLD_SCENE_ROUTE4:
            world->map_w = PC_ROUTE4_MAP_W;
            world->map_h = PC_ROUTE4_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route4_blocks[0][0],
                            PC_ROUTE4_MAP_H, PC_ROUTE4_MAP_W);
            set_banner(world, "Route 4", "Mt. Moon to the west, Cerulean ahead");
            break;

        case PC_WORLD_SCENE_MT_MOON_POKECENTER:
            world->map_w = PC_MT_MOON_POKECENTER_MAP_W;
            world->map_h = PC_MT_MOON_POKECENTER_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_MT_MOON_POKECENTER_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_MT_MOON_POKECENTER_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_mt_moon_pokecenter_blocks[0][0],
                            PC_MT_MOON_POKECENTER_MAP_H, PC_MT_MOON_POKECENTER_MAP_W);
            set_banner(world, "Mt. Moon Center", "A rest stop before the cave");
            break;

        case PC_WORLD_SCENE_MT_MOON_1F:
            world->map_w = PC_MT_MOON_1F_MAP_W;
            world->map_h = PC_MT_MOON_1F_MAP_H;
            world->origin_x = 0;
            world->origin_y = 0;
            copy_block_rows(world->tiles, 0, &pc_mt_moon_1f_blocks[0][0],
                            PC_MT_MOON_1F_MAP_H, PC_MT_MOON_1F_MAP_W);
            set_banner(world, "Mt. Moon 1F", "Twisting caverns under Route 4");
            break;

        case PC_WORLD_SCENE_MT_MOON_B1F:
            world->map_w = PC_MT_MOON_B1F_MAP_W;
            world->map_h = PC_MT_MOON_B1F_MAP_H;
            world->origin_x = 0;
            world->origin_y = 0;
            copy_block_rows(world->tiles, 0, &pc_mt_moon_b1f_blocks[0][0],
                            PC_MT_MOON_B1F_MAP_H, PC_MT_MOON_B1F_MAP_W);
            set_banner(world, "Mt. Moon B1F", "The lower cave splits in every direction");
            break;

        case PC_WORLD_SCENE_MT_MOON_B2F:
            world->map_w = PC_MT_MOON_B2F_MAP_W;
            world->map_h = PC_MT_MOON_B2F_MAP_H;
            world->origin_x = 0;
            world->origin_y = 0;
            copy_block_rows(world->tiles, 0, &pc_mt_moon_b2f_blocks[0][0],
                            PC_MT_MOON_B2F_MAP_H, PC_MT_MOON_B2F_MAP_W);
            set_banner(world, "Mt. Moon B2F", "Deeper tunnels lead to fossils");
            break;

        case PC_WORLD_SCENE_CERULEAN:
            world->map_w = PC_CERULEAN_MAP_W;
            world->map_h = PC_CERULEAN_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_cerulean_blocks[0][0],
                            PC_CERULEAN_MAP_H, PC_CERULEAN_MAP_W);
            set_banner(world, "Cerulean City", "Misty's city beside Route 4");
            break;

        case PC_WORLD_SCENE_ROUTE24:
            world->map_w = PC_ROUTE24_MAP_W;
            world->map_h = PC_ROUTE24_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route24_blocks[0][0],
                            PC_ROUTE24_MAP_H, PC_ROUTE24_MAP_W);
            set_banner(world, "Route 24", "Nugget Bridge climbs north from Cerulean");
            break;

        case PC_WORLD_SCENE_ROUTE25:
            world->map_w = PC_ROUTE25_MAP_W;
            world->map_h = PC_ROUTE25_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route25_blocks[0][0],
                            PC_ROUTE25_MAP_H, PC_ROUTE25_MAP_W);
            set_banner(world, "Route 25", "Bill's cape stretches over the sea");
            break;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            world->map_w = PC_ROUTE21_MAP_W;
            world->map_h = PC_ROUTE21_MAP_H;
            world->origin_x = PC_WORLD_ORIGIN_X;
            world->origin_y = PC_WORLD_ORIGIN_Y;
            copy_block_rows(world->tiles, 0, &pc_route21_blocks[0][0],
                            PC_ROUTE21_MAP_H, PC_ROUTE21_MAP_W);
            set_banner(world, "Route 21", "Northern waters off Pallet");
            break;

        case PC_WORLD_SCENE_VIRIDIAN_MART:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_viridian_mart_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Viridian Mart", "Select talks to clerk");
            break;

        case PC_WORLD_SCENE_PEWTER_MART:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_pewter_mart_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Pewter Mart", "Select talks to clerk");
            break;

        case PC_WORLD_SCENE_BLUES_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_blues_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Blue's House", "Daisy keeps the town map handy");
            break;

        case PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_route2_trade_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Route 2 House", "A trade story from the road");
            break;

        case PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_pewter_nidoran_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Nidoran House", "A family is visiting their Pokemon");
            break;

        case PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_pewter_speech_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Pewter House", "A local explains museum tickets");
            break;

        case PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_cerulean_traded_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Cerulean Trade House", "A quiet place near the bridge road");
            break;

        case PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_cerulean_trashed_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Robbed House", "A burglar left a hole in the back wall");
            break;

        case PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_cerulean_badge_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Badge House", "A rear shortcut opens into the city");
            break;

        case PC_WORLD_SCENE_BILLS_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_bills_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Bill's House", "A famous collector lives on the cape");
            break;

        case PC_WORLD_SCENE_VIRIDIAN_POKECENTER:
            world->map_w = PC_POKECENTER_MAP_W;
            world->map_h = PC_POKECENTER_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_POKECENTER_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_POKECENTER_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_viridian_pokecenter_blocks[0][0],
                            PC_POKECENTER_MAP_H, PC_POKECENTER_MAP_W);
            set_banner(world, "Viridian Pokecenter", "Heal up before Route 2");
            break;

        case PC_WORLD_SCENE_PEWTER_POKECENTER:
            world->map_w = PC_POKECENTER_MAP_W;
            world->map_h = PC_POKECENTER_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_POKECENTER_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_POKECENTER_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_pewter_pokecenter_blocks[0][0],
                            PC_POKECENTER_MAP_H, PC_POKECENTER_MAP_W);
            set_banner(world, "Pewter Pokecenter", "A stop before Brock's Gym");
            break;

        case PC_WORLD_SCENE_CERULEAN_POKECENTER:
            world->map_w = PC_POKECENTER_MAP_W;
            world->map_h = PC_POKECENTER_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_POKECENTER_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_POKECENTER_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_cerulean_pokecenter_blocks[0][0],
                            PC_POKECENTER_MAP_H, PC_POKECENTER_MAP_W);
            set_banner(world, "Cerulean Pokecenter", "Trainers gather before Misty's Gym");
            break;

        case PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_viridian_school_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Trainer School", "Local kids are studying");
            break;

        case PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_viridian_nickname_house_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Nickname House", "A Spearow chatters inside");
            break;

        case PC_WORLD_SCENE_OAKS_LAB:
            world->map_w = PC_LAB_MAP_W;
            world->map_h = PC_LAB_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_LAB_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_LAB_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_oaks_lab_blocks[0][0],
                            PC_LAB_MAP_H, PC_LAB_MAP_W);
            set_banner(world, "Oak's Lab", "Oak studies rare Pokemon here");
            break;

        case PC_WORLD_SCENE_VIRIDIAN_GYM:
            world->map_w = PC_GYM_MAP_W;
            world->map_h = PC_GYM_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_GYM_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_GYM_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_viridian_gym_blocks[0][0],
                            PC_GYM_MAP_H, PC_GYM_MAP_W);
            set_banner(world, "Viridian Gym", "Giovanni's arena is open");
            break;

        case PC_WORLD_SCENE_PEWTER_GYM:
            world->map_w = PC_SMALL_GYM_MAP_W;
            world->map_h = PC_SMALL_GYM_MAP_H;
            world->origin_x = (LCD_WIDTH - (world->map_w * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (world->map_h * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_pewter_gym_blocks[0][0],
                            PC_SMALL_GYM_MAP_H, PC_SMALL_GYM_MAP_W);
            set_banner(world, "Pewter Gym", "Brock is waiting inside");
            break;

        case PC_WORLD_SCENE_CERULEAN_GYM:
            world->map_w = PC_SMALL_GYM_MAP_W;
            world->map_h = PC_SMALL_GYM_MAP_H;
            world->origin_x = (LCD_WIDTH - (world->map_w * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (world->map_h * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_cerulean_gym_blocks[0][0],
                            PC_SMALL_GYM_MAP_H, PC_SMALL_GYM_MAP_W);
            set_banner(world, "Cerulean Gym", "Misty rules the water arena");
            break;

        case PC_WORLD_SCENE_BIKE_SHOP:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_bike_shop_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Bike Shop", "Rows of bikes line the walls");
            break;

        case PC_WORLD_SCENE_CERULEAN_MART:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_cerulean_mart_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Cerulean Mart", "Stock up before the Nugget Bridge");
            break;

        case PC_WORLD_SCENE_ROUTE2_GATE:
        case PC_WORLD_SCENE_ROUTE22_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE:
            world->map_w = PC_GATE_MAP_W;
            world->map_h = PC_GATE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_GATE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_GATE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            if (scene == PC_WORLD_SCENE_ROUTE2_GATE)
            {
                copy_block_rows(world->tiles, 0, &pc_route2_gate_blocks[0][0],
                                PC_GATE_MAP_H, PC_GATE_MAP_W);
                set_banner(world, "Route 2 Gate", "A short rest on the road");
            }
            else if (scene == PC_WORLD_SCENE_ROUTE22_GATE)
            {
                copy_block_rows(world->tiles, 0, &pc_route22_gate_blocks[0][0],
                                PC_GATE_MAP_H, PC_GATE_MAP_W);
                set_banner(world, "Route 22 Gate", "League guards watch the road");
            }
            else if (scene == PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE)
            {
                copy_block_rows(world->tiles, 0, &pc_forest_south_gate_blocks[0][0],
                                PC_GATE_MAP_H, PC_GATE_MAP_W);
                set_banner(world, "Forest Gate", "Viridian Forest begins ahead");
            }
            else
            {
                copy_block_rows(world->tiles, 0, &pc_forest_north_gate_blocks[0][0],
                                PC_GATE_MAP_H, PC_GATE_MAP_W);
                set_banner(world, "Forest Gate", "Pewter lies just beyond");
            }
            break;

        case PC_WORLD_SCENE_HOUSE_1F:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_reds_house_1f_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Red's House 1F", "Stairs up, door out");
            break;

        case PC_WORLD_SCENE_HOUSE_2F:
        default:
            world->map_w = PC_HOUSE_MAP_W;
            world->map_h = PC_HOUSE_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_HOUSE_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_HOUSE_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_reds_house_2f_blocks[0][0],
                            PC_HOUSE_MAP_H, PC_HOUSE_MAP_W);
            set_banner(world, "Red's Room", "Start here, head downstairs");
            break;
    }
}

static void update_camera(struct pc_world_state *world, bool snap);
static void set_player_to_block(struct pc_world_state *world,
                                int block_x, int block_y,
                                enum pc_heading heading);
static void player_metatile_pos(const struct pc_world_state *world,
                                int *tile_x, int *tile_y);

static void transition_to_scene_metatile(struct pc_world_state *world,
                                         enum pc_world_scene scene,
                                         int tile_x, int tile_y,
                                         enum pc_heading heading)
{
    load_scene_data(world, scene);
    set_player_to_metatile(world, tile_x, tile_y);
    world->heading = heading;
    world->moving = false;
    world->walk_frame = 1;
    world->walk_tick = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    update_camera(world, true);
    pc_world_save(world);
}

static void transition_to_scene_block(struct pc_world_state *world,
                                      enum pc_world_scene scene,
                                      int block_x, int block_y,
                                      enum pc_heading heading)
{
    load_scene_data(world, scene);
    set_player_to_block(world, block_x, block_y, heading);
    update_camera(world, true);
    pc_world_save(world);
}

static const unsigned char (*scene_respawn_blocks(const struct pc_world_state *world))[2]
{
    switch (world->scene)
    {
        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            return pc_forest_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE1_SOUTH:
            return pc_route1_respawn_blocks;

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            return pc_viridian_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE22:
            return pc_route22_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            return pc_route2_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            return pc_route21_respawn_blocks;

        case PC_WORLD_SCENE_PEWTER:
            return pc_pewter_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE3:
            return pc_route3_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE4:
            return pc_route4_respawn_blocks;

        case PC_WORLD_SCENE_CERULEAN:
            return pc_cerulean_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE24:
            return pc_route24_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE25:
            return pc_route25_respawn_blocks;

        case PC_WORLD_SCENE_PALLET:
        default:
            return pc_outside_respawn_blocks;
    }
}

static void clear_spawns(struct pc_world_state *world)
{
    int i;

    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        world->spawns[i].active = false;
        world->spawns[i].species_index = 0;
        world->spawns[i].home_block_x = 0;
        world->spawns[i].home_block_y = 0;
        world->spawns[i].respawn_frames = 0;
        world->spawns[i].lifetime_frames = 0;
    }
}

static int clamp_species_index(int species_index)
{
    int count = pc_assets_get_total_creature_count();

    if (count <= 0)
        return 0;

    species_index %= count;
    if (species_index < 0)
        species_index += count;
    return species_index;
}

static int find_species_with_mode(const struct pc_world_state *world,
                                  int start, int dir, bool caught_only)
{
    int count = pc_assets_get_total_creature_count();
    int i;
    int index;

    if (count <= 0)
        return -1;

    start = clamp_species_index(start);
    if (!caught_only)
        return start;

    for (i = 0; i < count; ++i)
    {
        index = (start + dir * i + count) % count;
        if (index < PC_POKEDEX_MAX && world->caught_counts[index] > 0)
            return index;
    }

    return -1;
}

static int find_species_with_ability(const struct pc_world_state *world,
                                     enum pc_field_ability ability,
                                     int start, int dir)
{
    int count = pc_assets_get_total_creature_count();
    int i;
    int index;

    if (count <= 0)
        return -1;

    start = clamp_species_index(start);
    for (i = 0; i < count; ++i)
    {
        index = (start + dir * i + count) % count;
        if (index < PC_POKEDEX_MAX &&
            world->caught_counts[index] > 0 &&
            species_can_use_field_ability(index, ability))
        {
            return index;
        }
    }

    return -1;
}

static void update_dex_asset(struct pc_world_state *world, int species_index)
{
    species_index = clamp_species_index(species_index);
    if (species_index < 0)
    {
        clear_bitmap(&world->assets.dex_creature);
        return;
    }

    if (!pc_assets_load_world_creature(&world->assets.dex_creature, species_index))
        clear_bitmap(&world->assets.dex_creature);
}

bool pc_world_save(struct pc_world_state *world);

static void scale_legacy_player_position(struct pc_world_state *world)
{
    world->player_x *= PC_WORLD_PIXEL_SCALE;
    world->player_y *= PC_WORLD_PIXEL_SCALE;
}

static enum pc_world_scene migrate_legacy_outdoor_position(struct pc_world_state *world,
                                                           enum pc_world_scene scene)
{
    int block_y;

    if (scene != PC_WORLD_SCENE_PALLET)
        return scene;

    block_y = (world->player_y + PC_WORLD_FOOT_Y_OFFSET) / PC_WORLD_TILE_SIZE;
    if (block_y < 9)
        return PC_WORLD_SCENE_ROUTE2_SOUTH;
    if (block_y < 22)
        return PC_WORLD_SCENE_VIRIDIAN_SOUTH;
    if (block_y < 31)
        return PC_WORLD_SCENE_ROUTE1_SOUTH;
    return PC_WORLD_SCENE_PALLET;
}

static void set_player_to_block(struct pc_world_state *world,
                                int block_x, int block_y,
                                enum pc_heading heading)
{
    world->player_x = block_center_x(world, block_x);
    world->player_y = block_center_y(world, block_y) - PC_WORLD_FOOT_Y_OFFSET;
    world->heading = heading;
    world->moving = false;
    world->walk_frame = 1;
    world->walk_tick = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
}

static void reset_player_to_safe_scene_position(struct pc_world_state *world)
{
    switch (world->scene)
    {
        case PC_WORLD_SCENE_ROUTE1_SOUTH:
            set_player_to_block(world, 5, 16, PC_HEADING_N);
            break;

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            set_player_to_block(world, 9, 15, PC_HEADING_N);
            break;

        case PC_WORLD_SCENE_ROUTE22:
            set_player_to_block(world, 3, 5, PC_HEADING_E);
            break;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            set_player_to_block(world, 4, 34, PC_HEADING_N);
            break;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            set_player_to_block(world, 8, 21, PC_HEADING_N);
            break;

        case PC_WORLD_SCENE_PEWTER:
            set_player_to_block(world, 10, 15, PC_HEADING_S);
            break;

        case PC_WORLD_SCENE_ROUTE3:
            set_player_to_block(world, 6, 4, PC_HEADING_E);
            break;

        case PC_WORLD_SCENE_ROUTE4:
            set_player_to_block(world, 10, 4, PC_HEADING_E);
            break;

        case PC_WORLD_SCENE_CERULEAN:
            set_player_to_block(world, 10, 14, PC_HEADING_S);
            break;

        case PC_WORLD_SCENE_ROUTE24:
            set_player_to_block(world, 5, 15, PC_HEADING_S);
            break;

        case PC_WORLD_SCENE_ROUTE25:
            set_player_to_block(world, 6, 4, PC_HEADING_E);
            break;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            set_player_to_block(world, 4, 8, PC_HEADING_S);
            break;

        case PC_WORLD_SCENE_VIRIDIAN_MART:
            set_player_to_metatile(world, 3, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_HOUSE_1F:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_HOUSE_2F:
            set_player_to_metatile(world, 5, 4);
            world->heading = PC_HEADING_S;
            break;

        case PC_WORLD_SCENE_OAKS_LAB:
            set_player_to_metatile(world, 4, 10);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_BLUES_HOUSE:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_VIRIDIAN_POKECENTER:
            set_player_to_metatile(world, 3, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_PEWTER_POKECENTER:
            set_player_to_metatile(world, 3, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_MUSEUM_1F:
            set_player_to_metatile(world, 10, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_MUSEUM_2F:
            set_player_to_metatile(world, 7, 6);
            world->heading = PC_HEADING_S;
            break;

        case PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_VIRIDIAN_GYM:
            set_player_to_metatile(world, 16, 16);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_PEWTER_GYM:
            set_player_to_metatile(world, 4, 12);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_MT_MOON_POKECENTER:
            set_player_to_metatile(world, 3, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_MT_MOON_1F:
            set_player_to_metatile(world, 14, 34);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_MT_MOON_B1F:
            set_player_to_metatile(world, 5, 6);
            world->heading = PC_HEADING_S;
            break;

        case PC_WORLD_SCENE_MT_MOON_B2F:
            set_player_to_metatile(world, 25, 10);
            world->heading = PC_HEADING_S;
            break;

        case PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE:
        case PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE:
        case PC_WORLD_SCENE_BIKE_SHOP:
        case PC_WORLD_SCENE_CERULEAN_MART:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_CERULEAN_POKECENTER:
            set_player_to_metatile(world, 3, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_CERULEAN_GYM:
            set_player_to_metatile(world, 4, 12);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_BILLS_HOUSE:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_ROUTE2_GATE:
        case PC_WORLD_SCENE_ROUTE22_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE:
            set_player_to_metatile(world, 4, 6);
            world->heading = PC_HEADING_S;
            break;

        case PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE:
        case PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE:
        case PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE:
        case PC_WORLD_SCENE_PEWTER_MART:
            set_player_to_metatile(world, 2, 6);
            world->heading = PC_HEADING_N;
            break;

        case PC_WORLD_SCENE_PALLET:
        default:
            set_player_to_block(world, 5, 7, PC_HEADING_S);
            break;
    }
}

static bool pc_world_try_load(struct pc_world_state *world)
{
    static union pc_save_scratch savebuf;
    enum pc_world_scene scene;
    bool migrated_save = false;
    bool legacy_outdoor_layout = false;
    bool needs_safe_outdoor_reset = false;
    int fd;
    ssize_t got;
    int i;

    fd = rb->open(PC_SAVE_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    rb->memset(&savebuf, 0, sizeof(savebuf));
    got = rb->read(fd, &savebuf, sizeof(savebuf));
    rb->close(fd);
    if (got == (ssize_t)sizeof(savebuf.v1))
    {
        if (savebuf.v1.magic != PC_SAVE_MAGIC ||
            savebuf.v1.version != 1 ||
            savebuf.v1.scene < 0 || savebuf.v1.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v1.scene;
        world->player_x = savebuf.v1.player_x;
        world->player_y = savebuf.v1.player_y;
        scale_legacy_player_position(world);
        legacy_outdoor_layout = true;
        scene = migrate_legacy_outdoor_position(world, scene);
        migrated_save = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v1.heading;
        rb->memset(world->caught_counts, 0, sizeof(world->caught_counts));
        rb->memset(world->family_candy, 0, sizeof(world->family_candy));
        rb->memcpy(world->caught_counts, savebuf.v1.caught_counts,
                   sizeof(savebuf.v1.caught_counts));
        for (i = 0; i < 64; ++i)
        {
            int family = pc_assets_get_family_index(i);

            if (family >= 0 && family < PC_POKEDEX_MAX && world->caught_counts[i] > 0)
                world->family_candy[family] += world->caught_counts[i] * 3;
        }
    }
    else if (got == (ssize_t)sizeof(savebuf.v4))
    {
        if (savebuf.v4.magic != PC_SAVE_MAGIC ||
            savebuf.v4.version < 2 || savebuf.v4.version > 4 ||
            savebuf.v4.scene < 0 || savebuf.v4.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v4.scene;
        world->player_x = savebuf.v4.player_x;
        world->player_y = savebuf.v4.player_y;
        if (savebuf.v4.version < 6)
        {
            legacy_outdoor_layout = true;
            scene = migrate_legacy_outdoor_position(world, scene);
            migrated_save = true;
        }
        if (savebuf.v4.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v4.heading;
        rb->memcpy(world->caught_counts, savebuf.v4.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v4.family_candy, sizeof(world->family_candy));
        init_progress_defaults(world);
    }
    else if (got == (ssize_t)sizeof(savebuf.v7))
    {
        if (savebuf.v7.magic != PC_SAVE_MAGIC ||
            savebuf.v7.version < 5 || savebuf.v7.version > 7 ||
            savebuf.v7.scene < 0 || savebuf.v7.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v7.scene;
        world->player_x = savebuf.v7.player_x;
        world->player_y = savebuf.v7.player_y;
        if (savebuf.v7.version < 6)
        {
            legacy_outdoor_layout = true;
            scene = migrate_legacy_outdoor_position(world, scene);
            migrated_save = true;
        }
        if (savebuf.v7.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v7.heading;
        world->travel_steps = savebuf.v7.travel_steps;
        world->pokeballs = savebuf.v7.pokeballs;
        world->money = savebuf.v7.money;
        rb->memcpy(world->caught_counts, savebuf.v7.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v7.family_candy, sizeof(world->family_candy));
        init_progress_defaults(world);
        world->travel_steps = savebuf.v7.travel_steps;
        world->pokeballs = savebuf.v7.pokeballs;
        world->money = savebuf.v7.money;
        world->ability_species[PC_FIELD_ABILITY_SURF] = savebuf.v7.ability_species[0];
        world->ability_owned[PC_FIELD_ABILITY_SURF] = savebuf.v7.ability_owned[0];
    }
    else if (got == (ssize_t)sizeof(savebuf.v8))
    {
        if (savebuf.v8.magic != PC_SAVE_MAGIC ||
            savebuf.v8.version != 8 ||
            savebuf.v8.scene < 0 || savebuf.v8.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v8.scene;
        world->player_x = savebuf.v8.player_x;
        world->player_y = savebuf.v8.player_y;
        if (savebuf.v8.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v8.heading;
        world->travel_steps = savebuf.v8.travel_steps;
        world->pokeballs = savebuf.v8.pokeballs;
        world->money = savebuf.v8.money;
        world->secret_collected_bits = 0;
        rb->memset(world->pokestop_cooldowns, 0, sizeof(world->pokestop_cooldowns));
        rb->memcpy(world->caught_counts, savebuf.v8.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v8.family_candy, sizeof(world->family_candy));
        rb->memcpy(world->ability_species, savebuf.v8.ability_species,
                   sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, savebuf.v8.ability_owned, sizeof(world->ability_owned));
    }
    else if (got == (ssize_t)sizeof(savebuf.v9))
    {
        if (savebuf.v9.magic != PC_SAVE_MAGIC ||
            savebuf.v9.version != 9 ||
            savebuf.v9.scene < 0 || savebuf.v9.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v9.scene;
        world->player_x = savebuf.v9.player_x;
        world->player_y = savebuf.v9.player_y;
        if (savebuf.v9.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v9.heading;
        world->travel_steps = savebuf.v9.travel_steps;
        world->pokeballs = savebuf.v9.pokeballs;
        world->money = savebuf.v9.money;
        world->secret_collected_bits = savebuf.v9.secret_collected_bits;
        rb->memset(world->pokestop_cooldowns, 0, sizeof(world->pokestop_cooldowns));
        rb->memcpy(world->caught_counts, savebuf.v9.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v9.family_candy, sizeof(world->family_candy));
        rb->memcpy(world->ability_species, savebuf.v9.ability_species,
                   sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, savebuf.v9.ability_owned, sizeof(world->ability_owned));
    }
    else if (got == (ssize_t)sizeof(savebuf.v10))
    {
        if (savebuf.v10.magic != PC_SAVE_MAGIC ||
            savebuf.v10.version != 10 ||
            savebuf.v10.scene < 0 || savebuf.v10.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v10.scene;
        world->player_x = savebuf.v10.player_x;
        world->player_y = savebuf.v10.player_y;
        if (savebuf.v10.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v10.heading;
        world->travel_steps = savebuf.v10.travel_steps;
        world->pokeballs = savebuf.v10.pokeballs;
        world->money = savebuf.v10.money;
        world->secret_collected_bits = savebuf.v10.secret_collected_bits;
        rb->memset(world->pokestop_cooldowns, 0, sizeof(world->pokestop_cooldowns));
        rb->memcpy(world->pokestop_cooldowns, savebuf.v10.pokestop_cooldowns,
                   sizeof(savebuf.v10.pokestop_cooldowns));
        rb->memcpy(world->caught_counts, savebuf.v10.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v10.family_candy, sizeof(world->family_candy));
        rb->memcpy(world->ability_species, savebuf.v10.ability_species,
                   sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, savebuf.v10.ability_owned, sizeof(world->ability_owned));
        world->buddy_species = -1;
        world->buddy_steps = 0;
    }
    else if (got == (ssize_t)sizeof(savebuf.v11))
    {
        if (savebuf.v11.magic != PC_SAVE_MAGIC ||
            savebuf.v11.version != 11 ||
            savebuf.v11.scene < 0 || savebuf.v11.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v11.scene;
        world->player_x = savebuf.v11.player_x;
        world->player_y = savebuf.v11.player_y;
        if (savebuf.v11.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v11.heading;
        world->travel_steps = savebuf.v11.travel_steps;
        world->buddy_species = savebuf.v11.buddy_species;
        world->buddy_steps = savebuf.v11.buddy_steps;
        world->pokeballs = savebuf.v11.pokeballs;
        world->money = savebuf.v11.money;
        world->secret_collected_bits = savebuf.v11.secret_collected_bits;
        world->player_trainer = 0;
        world->owned_trainers[0] = 1;
        rb->memset(world->pokestop_cooldowns, 0, sizeof(world->pokestop_cooldowns));
        rb->memcpy(world->pokestop_cooldowns, savebuf.v11.pokestop_cooldowns,
                   sizeof(savebuf.v11.pokestop_cooldowns));
        rb->memcpy(world->caught_counts, savebuf.v11.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v11.family_candy, sizeof(world->family_candy));
        rb->memcpy(world->ability_species, savebuf.v11.ability_species,
                   sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, savebuf.v11.ability_owned, sizeof(world->ability_owned));
    }
    else if (got == (ssize_t)sizeof(savebuf.v12))
    {
        if (savebuf.v12.magic != PC_SAVE_MAGIC ||
            savebuf.v12.version != 12 ||
            savebuf.v12.scene < 0 || savebuf.v12.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v12.scene;
        world->player_x = savebuf.v12.player_x;
        world->player_y = savebuf.v12.player_y;
        if (savebuf.v12.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v12.heading;
        world->travel_steps = savebuf.v12.travel_steps;
        world->buddy_species = savebuf.v12.buddy_species;
        world->buddy_steps = savebuf.v12.buddy_steps;
        world->pokeballs = savebuf.v12.pokeballs;
        world->money = savebuf.v12.money;
        world->secret_collected_bits = savebuf.v12.secret_collected_bits;
        world->player_trainer = savebuf.v12.player_trainer;
        rb->memcpy(world->owned_trainers, savebuf.v12.owned_trainers,
                   sizeof(world->owned_trainers));
        rb->memset(world->pokestop_cooldowns, 0, sizeof(world->pokestop_cooldowns));
        rb->memcpy(world->pokestop_cooldowns, savebuf.v12.pokestop_cooldowns,
                   sizeof(savebuf.v12.pokestop_cooldowns));
        rb->memcpy(world->caught_counts, savebuf.v12.caught_counts,
                   sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v12.family_candy,
                   sizeof(world->family_candy));
        rb->memcpy(world->ability_species, savebuf.v12.ability_species,
                   sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, savebuf.v12.ability_owned,
                   sizeof(world->ability_owned));
    }
    else if (got == (ssize_t)sizeof(savebuf.v13))
    {
        if (savebuf.v13.magic != PC_SAVE_MAGIC ||
            savebuf.v13.version != 13 ||
            savebuf.v13.scene < 0 || savebuf.v13.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.v13.scene;
        world->player_x = savebuf.v13.player_x;
        world->player_y = savebuf.v13.player_y;
        if (savebuf.v13.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.v13.heading;
        world->travel_steps = savebuf.v13.travel_steps;
        world->buddy_species = savebuf.v13.buddy_species;
        world->buddy_steps = savebuf.v13.buddy_steps;
        world->pokeballs = savebuf.v13.pokeballs;
        world->money = savebuf.v13.money;
        world->secret_collected_bits = savebuf.v13.secret_collected_bits;
        world->player_trainer = savebuf.v13.player_trainer;
        rb->memcpy(world->owned_trainers, savebuf.v13.owned_trainers,
                   sizeof(world->owned_trainers));
        rb->memset(world->pokestop_cooldowns, 0, sizeof(world->pokestop_cooldowns));
        rb->memcpy(world->pokestop_cooldowns, savebuf.v13.pokestop_cooldowns,
                   sizeof(savebuf.v13.pokestop_cooldowns));
        rb->memcpy(world->caught_counts, savebuf.v13.caught_counts,
                   sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.v13.family_candy,
                   sizeof(world->family_candy));
        rb->memcpy(world->ability_species, savebuf.v13.ability_species,
                   sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, savebuf.v13.ability_owned,
                   sizeof(world->ability_owned));
    }
    else if (got == (ssize_t)sizeof(savebuf.current))
    {
        if (savebuf.current.magic != PC_SAVE_MAGIC ||
            savebuf.current.version != 14 ||
            savebuf.current.scene < 0 || savebuf.current.scene > PC_WORLD_SCENE_MAX)
        {
            return false;
        }

        scene = (enum pc_world_scene)savebuf.current.scene;
        world->player_x = savebuf.current.player_x;
        world->player_y = savebuf.current.player_y;
        if (savebuf.current.version < PC_SAFE_RESET_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = savebuf.current.heading;
        world->travel_steps = savebuf.current.travel_steps;
        world->buddy_species = savebuf.current.buddy_species;
        world->buddy_steps = savebuf.current.buddy_steps;
        world->pokeballs = savebuf.current.pokeballs;
        world->money = savebuf.current.money;
        world->secret_collected_bits = savebuf.current.secret_collected_bits;
        world->player_trainer = savebuf.current.player_trainer;
        rb->memcpy(world->owned_trainers, savebuf.current.owned_trainers,
                   sizeof(world->owned_trainers));
        rb->memcpy(world->pokestop_cooldowns, savebuf.current.pokestop_cooldowns,
                   sizeof(world->pokestop_cooldowns));
        rb->memcpy(world->caught_counts, savebuf.current.caught_counts,
                   sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, savebuf.current.family_candy,
                   sizeof(world->family_candy));
        rb->memcpy(world->ability_species, savebuf.current.ability_species,
                   sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, savebuf.current.ability_owned,
                   sizeof(world->ability_owned));
    }
    else
    {
        return false;
    }

    if (world->pokeballs > PC_MAX_POKEBALLS)
        world->pokeballs = PC_MAX_POKEBALLS;
    sanitize_field_abilities(world);
    sanitize_buddy(world);
    sanitize_player_trainer(world);
    reset_transient_world_state(world);
    reload_player_trainer_assets(world);
    world->dex_index = find_species_with_mode(world, 0, 1, true);
    if (world->dex_index < 0)
        world->dex_index = 0;
    world->bag_index = world->dex_index;
    update_dex_asset(world, world->dex_index);
    if ((legacy_outdoor_layout || needs_safe_outdoor_reset) &&
        outdoor_scene(world->scene))
    {
        reset_player_to_safe_scene_position(world);
        migrated_save = true;
    }
    if (!player_walkable(world, world->player_x, world->player_y))
    {
        reset_player_to_safe_scene_position(world);
        migrated_save = true;
    }
    update_camera(world, true);
    if (migrated_save)
        pc_world_save(world);
    return true;
}

bool pc_world_save(struct pc_world_state *world)
{
    struct pc_save_record record;
    int fd;

    ensure_pocketcatch_data_dir();
    rb->memset(&record, 0, sizeof(record));
    record.magic = PC_SAVE_MAGIC;
    record.version = PC_SAVE_VERSION;
    record.scene = world->scene;
    record.player_x = world->player_x;
    record.player_y = world->player_y;
    record.heading = world->heading;
    record.travel_steps = world->travel_steps;
    record.buddy_species = world->buddy_species;
    record.buddy_steps = world->buddy_steps;
    record.pokeballs = world->pokeballs;
    record.money = world->money;
    record.secret_collected_bits = world->secret_collected_bits;
    record.player_trainer = world->player_trainer;
    rb->memcpy(record.owned_trainers, world->owned_trainers, sizeof(record.owned_trainers));
    rb->memcpy(record.pokestop_cooldowns, world->pokestop_cooldowns,
               sizeof(record.pokestop_cooldowns));
    rb->memcpy(record.caught_counts, world->caught_counts, sizeof(record.caught_counts));
    rb->memcpy(record.family_candy, world->family_candy, sizeof(record.family_candy));
    rb->memcpy(record.ability_species, world->ability_species, sizeof(record.ability_species));
    rb->memcpy(record.ability_owned, world->ability_owned, sizeof(record.ability_owned));

    fd = rb->open(PC_SAVE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    if (rb->write(fd, &record, sizeof(record)) != (ssize_t)sizeof(record))
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

static void place_spawn(struct pc_world_state *world, int slot,
                        int block_x, int block_y, int species_index)
{
    struct pc_world_spawn *spawn = &world->spawns[slot];
    int count = pc_assets_get_total_creature_count();

    spawn->active = true;
    spawn->home_block_x = block_x;
    spawn->home_block_y = block_y;
    spawn->species_index = species_index % MAX(1, count);
    if (spawn->species_index < 0)
        spawn->species_index += count;
    spawn->x = block_center_x(world, block_x);
    spawn->y = block_center_y(world, block_y) + PC_WORLD_SPAWN_Y_OFFSET;
    spawn->step = 0;
    spawn->dir_x = (slot & 1) ? 1 : -1;
    spawn->dir_y = 0;
    spawn->respawn_frames = 0;
    spawn->lifetime_frames = random_spawn_lifetime();
    pc_assets_load_world_creature(&world->assets.creature[slot], spawn->species_index);
}

static void configure_spawn_slot(struct pc_world_state *world, int slot,
                                 int block_x, int block_y)
{
    struct pc_world_spawn *spawn = &world->spawns[slot];

    spawn->active = false;
    spawn->home_block_x = block_x;
    spawn->home_block_y = block_y;
    spawn->species_index = 0;
    spawn->x = block_center_x(world, block_x);
    spawn->y = block_center_y(world, block_y) + PC_WORLD_SPAWN_Y_OFFSET;
    spawn->step = 0;
    spawn->dir_x = 0;
    spawn->dir_y = 0;
    spawn->respawn_frames = random_spawn_delay() + (slot * 2);
    spawn->lifetime_frames = 0;
    clear_bitmap(&world->assets.creature[slot]);
}

static int count_active_spawns(const struct pc_world_state *world)
{
    int i;
    int total = 0;

    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        if (world->spawns[i].active)
            total++;
    }
    return total;
}

static bool spawn_too_close_to_player(const struct pc_world_state *world,
                                      const struct pc_world_spawn *spawn)
{
    int dx = world->player_x - spawn->x;
    int dy = world->player_y - spawn->y;

    return dx * dx + dy * dy <= PC_WORLD_SAFE_PLAYER_RADIUS * PC_WORLD_SAFE_PLAYER_RADIUS;
}

static bool spawn_overlaps_other(const struct pc_world_state *world, int slot)
{
    int i;

    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        const struct pc_world_spawn *a;
        const struct pc_world_spawn *b;
        int dx;
        int dy;

        if (i == slot || !world->spawns[i].active || !world->spawns[slot].active)
            continue;

        a = &world->spawns[i];
        b = &world->spawns[slot];
        dx = a->x - b->x;
        dy = a->y - b->y;
        if (dx * dx + dy * dy <= PC_WORLD_SAFE_SPAWN_RADIUS * PC_WORLD_SAFE_SPAWN_RADIUS)
            return true;
    }

    return false;
}

static void ensure_spawn_safe(struct pc_world_state *world, int slot)
{
    const unsigned char (*respawns)[2];
    int species_index;
    int start;
    int attempt;

    if (!outdoor_scene(world->scene) || !world->spawns[slot].active)
        return;

    if (!spawn_too_close_to_player(world, &world->spawns[slot]) &&
        !spawn_overlaps_other(world, slot))
    {
        return;
    }

    respawns = scene_respawn_blocks(world);
    species_index = world->spawns[slot].species_index;
    start = (slot + world->frame + world->scene) % PC_WORLD_MAX_SPAWNS;

    for (attempt = 0; attempt < PC_WORLD_MAX_SPAWNS; ++attempt)
    {
        int index = (start + attempt) % PC_WORLD_MAX_SPAWNS;

        place_spawn(world, slot, respawns[index][0], respawns[index][1], species_index);
        if (!spawn_too_close_to_player(world, &world->spawns[slot]) &&
            !spawn_overlaps_other(world, slot))
        {
            return;
        }
    }

    world->spawns[slot].active = false;
}

static void init_spawns(struct pc_world_state *world)
{
    const unsigned char (*respawns)[2];
    enum pc_outdoor_region region;
    int i;

    clear_spawns(world);
    if (!outdoor_scene(world->scene))
        return;

    region = current_outdoor_region(world);
    if (!outdoor_region_has_spawns(region))
        return;

    respawns = scene_respawn_blocks(world);
    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
        configure_spawn_slot(world, i, respawns[i][0], respawns[i][1]);

    apply_encounter_grace(world, 1, 12);
}

static void maybe_move_spawn(struct pc_world_state *world, struct pc_world_spawn *spawn)
{
    (void)world;
    (void)spawn;
    /* Keep visible encounters anchored in place. This reads closer to the
       deliberate tile-based feel of Red and avoids extra motion/redraw cost. */
}

static void deactivate_spawn_slot(struct pc_world_state *world, int slot)
{
    struct pc_world_spawn *spawn = &world->spawns[slot];

    spawn->active = false;
    spawn->respawn_frames = random_spawn_delay();
    spawn->lifetime_frames = 0;
    spawn->dir_x = 0;
    spawn->dir_y = 0;
    clear_bitmap(&world->assets.creature[slot]);
}

static void suppress_local_encounters(struct pc_world_state *world,
                                      int radius_px,
                                      int grace_steps,
                                      int cooldown_frames)
{
    int i;
    int radius_sq;

    if (world == NULL)
        return;

    clear_pending_encounter_state(world);
    apply_encounter_grace(world, grace_steps, cooldown_frames);

    if (!outdoor_scene(world->scene))
        return;

    radius_sq = radius_px * radius_px;
    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        struct pc_world_spawn *spawn = &world->spawns[i];
        int spawn_x = spawn->active ? spawn->x : block_center_x(world, spawn->home_block_x);
        int spawn_y = spawn->active ? spawn->y
                                    : block_center_y(world, spawn->home_block_y) +
                                      PC_WORLD_SPAWN_Y_OFFSET;
        int dx = world->player_x - spawn_x;
        int dy = world->player_y - spawn_y;

        if (radius_px > 0 && dx * dx + dy * dy > radius_sq)
            continue;

        if (spawn->active)
            deactivate_spawn_slot(world, i);

        spawn->respawn_frames = MAX(spawn->respawn_frames, random_spawn_delay() + 8);
    }
}

static void maybe_activate_spawn_slot(struct pc_world_state *world, int slot)
{
    struct pc_world_spawn *spawn = &world->spawns[slot];
    enum pc_outdoor_region region;
    int species_index;

    if (!outdoor_scene(world->scene) || spawn->active)
        return;
    if (count_active_spawns(world) >= PC_WORLD_VISIBLE_SPAWNS_MAX)
        return;
    if (spawn->respawn_frames > 0)
        return;

    region = current_outdoor_region(world);
    species_index = random_species_for_region(region);
    if (species_index < 0)
    {
        spawn->respawn_frames = random_spawn_delay();
        return;
    }
    place_spawn(world, slot, spawn->home_block_x, spawn->home_block_y, species_index);
    ensure_spawn_safe(world, slot);
    if (!world->spawns[slot].active)
        spawn->respawn_frames = random_spawn_delay();
}

static void player_metatile_pos(const struct pc_world_state *world,
                                int *tile_x, int *tile_y)
{
    int foot_x = world->player_x;
    int foot_y = world->player_y + PC_WORLD_FOOT_Y_OFFSET;
    int step = PC_WORLD_TILE_SIZE / 2;

    *tile_x = foot_x / step;
    *tile_y = foot_y / step;
}

bool pc_world_secret_draw_info(const struct pc_world_state *world, int index,
                               int *metatile_x, int *metatile_y)
{
    if (world == NULL ||
        index < 0 || index >= PC_WORLD_SECRET_COUNT ||
        secret_collected(world, index) ||
        pc_world_secret_items[index].scene != world->scene)
    {
        return false;
    }

    if (metatile_x != NULL)
        *metatile_x = pc_world_secret_items[index].metatile_x;
    if (metatile_y != NULL)
        *metatile_y = pc_world_secret_items[index].metatile_y;
    return true;
}

bool pc_world_pokestop_draw_info(const struct pc_world_state *world, int index,
                                 int *metatile_x, int *metatile_y, bool *ready)
{
    if (world == NULL ||
        index < 0 || index >= PC_WORLD_POKESTOP_COUNT ||
        pc_world_pokestops[index].scene != world->scene)
    {
        return false;
    }

    if (metatile_x != NULL)
        *metatile_x = pc_world_pokestops[index].metatile_x;
    if (metatile_y != NULL)
        *metatile_y = pc_world_pokestops[index].metatile_y;
    if (ready != NULL)
        *ready = world->pokestop_cooldowns[index] == 0;
    return true;
}

static void facing_metatile_pos(const struct pc_world_state *world,
                                int *tile_x, int *tile_y)
{
    int dx = 0;
    int dy = 0;

    player_metatile_pos(world, tile_x, tile_y);
    switch (world->heading)
    {
        case PC_HEADING_N:
            dy = -1;
            break;

        case PC_HEADING_E:
            dx = 1;
            break;

        case PC_HEADING_W:
            dx = -1;
            break;

        case PC_HEADING_S:
        default:
            dy = 1;
            break;
    }

    *tile_x += dx;
    *tile_y += dy;
}

static int secret_index_for_metatile(const struct pc_world_state *world,
                                     int tile_x, int tile_y)
{
    int i;

    for (i = 0; i < PC_WORLD_SECRET_COUNT; ++i)
    {
        if (secret_collected(world, i))
            continue;
        if (pc_world_secret_items[i].scene != world->scene)
            continue;
        if (pc_world_secret_items[i].metatile_x == tile_x &&
            pc_world_secret_items[i].metatile_y == tile_y)
        {
            return i;
        }
    }

    return -1;
}

static int pokestop_index_for_metatile(const struct pc_world_state *world,
                                       int tile_x, int tile_y)
{
    int i;

    for (i = 0; i < PC_WORLD_POKESTOP_COUNT; ++i)
    {
        if (pc_world_pokestops[i].scene != world->scene)
            continue;
        if (pc_world_pokestops[i].metatile_x == tile_x &&
            pc_world_pokestops[i].metatile_y == tile_y)
        {
            return i;
        }
    }

    return -1;
}

static int pokestop_index_near_metatile(const struct pc_world_state *world,
                                        int tile_x, int tile_y, int radius)
{
    int best_index = -1;
    int best_distance = 9999;
    int i;

    for (i = 0; i < PC_WORLD_POKESTOP_COUNT; ++i)
    {
        int dx;
        int dy;
        int distance;

        if (pc_world_pokestops[i].scene != world->scene)
            continue;

        dx = PC_ABS(pc_world_pokestops[i].metatile_x - tile_x);
        dy = PC_ABS(pc_world_pokestops[i].metatile_y - tile_y);
        if (dx > radius || dy > radius)
            continue;

        distance = dx + dy;
        if (distance < best_distance)
        {
            best_distance = distance;
            best_index = i;
        }
    }

    return best_index;
}

static void tick_pokestop_cooldowns(struct pc_world_state *world)
{
    int i;

    for (i = 0; i < PC_WORLD_POKESTOP_COUNT; ++i)
    {
        if (world->pokestop_cooldowns[i] > 0)
            world->pokestop_cooldowns[i]--;
    }
}

static void close_pokestop_overlay(struct pc_world_state *world)
{
    world->view = PC_WORLD_VIEW_MAP;
    world->pokestop_index = -1;
    world->pokestop_spin_progress = 0;
    world->pokestop_spin_angle = 0;
    world->pokestop_last_wheel_angle = -1;
    world->pokestop_reward_balls = 0;
    world->pokestop_reward_money = 0;
    world->pokestop_spun = false;
    suppress_local_encounters(world,
                              -1,
                              PC_POKESTOP_ENCOUNTER_GRACE_STEPS,
                              PC_POKESTOP_ENCOUNTER_COOLDOWN);
}

static void open_pokestop_overlay(struct pc_world_state *world, int index)
{
    world->view = PC_WORLD_VIEW_POKESTOP;
    world->moving = false;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    world->walk_tick = 0;
    world->walk_frame = 1;
    world->pokestop_index = index;
    world->pokestop_spin_progress = 0;
    world->pokestop_spin_angle = 0;
    world->pokestop_last_wheel_angle = -1;
    world->pokestop_reward_balls = 0;
    world->pokestop_reward_money = 0;
    world->pokestop_spun = false;
    suppress_local_encounters(world,
                              -1,
                              PC_POKESTOP_ENCOUNTER_GRACE_STEPS,
                              PC_POKESTOP_ENCOUNTER_COOLDOWN);
}

static void grant_pokestop_rewards(struct pc_world_state *world)
{
    int balls = PC_POKESTOP_MIN_BALLS +
                (rb->rand() % (PC_POKESTOP_MAX_BALLS - PC_POKESTOP_MIN_BALLS + 1));
    int cash = PC_POKESTOP_MIN_MONEY +
               (rb->rand() % (PC_POKESTOP_MAX_MONEY - PC_POKESTOP_MIN_MONEY + 1));
    int bonus = MAX(0, world->pokestop_spin_progress - PC_POKESTOP_SPIN_TARGET);

    if (world->pokestop_index < 0 || world->pokestop_index >= PC_WORLD_POKESTOP_COUNT)
        return;

    balls += MIN(2, bonus / 18);
    cash += MIN(4, bonus / 12);
    world->pokeballs = MIN(PC_MAX_POKEBALLS, world->pokeballs + balls);
    world->money += cash;
    world->pokestop_cooldowns[world->pokestop_index] = PC_POKESTOP_COOLDOWN_STEPS;
    world->pokestop_reward_balls = balls;
    world->pokestop_reward_money = cash;
    world->pokestop_spun = true;
    pc_world_save(world);
}

static bool try_open_pokestop(struct pc_world_state *world)
{
    int tile_x;
    int tile_y;
    int facing_x;
    int facing_y;
    int index;
    char line2[PC_BANNER_LINE_CHARS];

    if (!outdoor_scene(world->scene))
        return false;

    player_metatile_pos(world, &tile_x, &tile_y);
    index = pokestop_index_for_metatile(world, tile_x, tile_y);
    facing_x = tile_x;
    facing_y = tile_y;
    facing_metatile_pos(world, &facing_x, &facing_y);
    if (index < 0)
    {
        index = pokestop_index_for_metatile(world, facing_x, facing_y);
    }
    if (index < 0)
    {
        index = pokestop_index_near_metatile(world, tile_x, tile_y, 1);
    }
    if (index < 0)
    {
        index = pokestop_index_near_metatile(world, facing_x, facing_y, 1);
        if (index < 0)
            return false;
    }

    if (world->pokestop_cooldowns[index] > 0)
    {
        rb->snprintf(line2, sizeof(line2), "Walk %u more steps",
                     world->pokestop_cooldowns[index]);
        set_notice(world, "PokeStop cooling", line2);
        return true;
    }

    open_pokestop_overlay(world, index);
    return true;
}

static bool try_collect_music_secret(struct pc_world_state *world)
{
    char path[MAX_PATH];
    uint32_t hash = 0;
    int tile_x;
    int tile_y;
    int secret_index;

    player_metatile_pos(world, &tile_x, &tile_y);
    secret_index = secret_index_for_metatile(world, tile_x, tile_y);
    if (secret_index < 0)
    {
        facing_metatile_pos(world, &tile_x, &tile_y);
        secret_index = secret_index_for_metatile(world, tile_x, tile_y);
        if (secret_index < 0)
            return false;
    }

    if (!pick_random_music_track(path, sizeof(path), &hash))
    {
        set_notice(world, "Song Unlocked", "No songs in /Music yet");
        return true;
    }

    if (!unlock_song_path(path))
    {
        set_notice(world, "Song Unlocked", "Song list is full");
        return true;
    }

    collect_secret(world, secret_index);
    remember_music_track(hash);
    set_notice(world, "Song Unlocked", pc_path_basename(path));
    pc_world_save(world);
    return true;
}

static bool player_in_metatile_zone(const struct pc_world_state *world,
                                    int min_x, int max_x,
                                    int min_y, int max_y)
{
    int tile_x;
    int tile_y;

    player_metatile_pos(world, &tile_x, &tile_y);
    return tile_x >= min_x && tile_x <= max_x &&
           tile_y >= min_y && tile_y <= max_y;
}

static void player_block_pos(const struct pc_world_state *world,
                             int *block_x, int *block_y)
{
    int foot_x = world->player_x;
    int foot_y = world->player_y + PC_WORLD_FOOT_Y_OFFSET;

    *block_x = foot_x / PC_WORLD_TILE_SIZE;
    *block_y = foot_y / PC_WORLD_TILE_SIZE;
}

static bool cuttable_block_info(const struct pc_world_state *world,
                                int block_x, int block_y,
                                unsigned char *replacement_block)
{
    unsigned char block_id;

    if (block_x < 0 || block_y < 0 ||
        block_x >= world->map_w || block_y >= world->map_h)
    {
        return false;
    }

    block_id = world->tiles[block_y][block_x];
    if (world->scene == PC_WORLD_SCENE_ROUTE2_SOUTH &&
        block_id == 0x0f &&
        (((block_x >= 4 && block_x <= 6) && (block_y >= 8 && block_y <= 10)) ||
         ((block_x >= 3 && block_x <= 5) && block_y == 27)))
    {
        if (replacement_block != NULL)
            *replacement_block = 0x0a;
        return true;
    }

    return false;
}

static bool cut_target_block(const struct pc_world_state *world,
                             int *block_x, int *block_y,
                             unsigned char *replacement_block)
{
    int tile_x;
    int tile_y;
    int target_tile_x;
    int target_tile_y;
    int dx = 0;
    int dy = 0;

    player_metatile_pos(world, &tile_x, &tile_y);

    switch (world->heading)
    {
        case PC_HEADING_N:
            dy = -1;
            break;

        case PC_HEADING_E:
            dx = 1;
            break;

        case PC_HEADING_W:
            dx = -1;
            break;

        case PC_HEADING_S:
        default:
            dy = 1;
            break;
    }

    target_tile_x = tile_x + dx;
    target_tile_y = tile_y + dy;
    *block_x = target_tile_x / 2;
    *block_y = target_tile_y / 2;
    return cuttable_block_info(world, *block_x, *block_y, replacement_block);
}

static bool try_use_cut(struct pc_world_state *world)
{
    int partner_index;
    int block_x;
    int block_y;
    unsigned char replacement_block;
    const struct pc_creature_def *creature;

    if (!cut_target_block(world, &block_x, &block_y, &replacement_block))
        return false;

    partner_index = assigned_field_ability_species(world, PC_FIELD_ABILITY_CUT);
    if (partner_index < 0)
    {
        if (world->ability_owned[PC_FIELD_ABILITY_CUT])
            set_notice(world, "Assign Cut first", "Open Field Moves in menu");
        else
            set_notice(world, "Buy HM01 Cut", "Viridian Mart sells it");
        return true;
    }

    world->tiles[block_y][block_x] = replacement_block;
    world->map_dirty = true;
    creature = pc_assets_get_creature(partner_index);
    if (creature != NULL)
        set_notice(world, creature->name, "used Cut");
    else
        set_notice(world, "Used Cut", "The shrub was cleared");
    return true;
}

static int clampi(int value, int min_value, int max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static void update_touch_direction(struct pc_world_state *world)
{
#ifdef HAVE_WHEEL_POSITION
    int wheel = rb->wheel_status();
    int dir_x = 0;
    int dir_y = 0;

    if (wheel >= 0)
    {
        if (wheel >= 84 || wheel < 12)
            dir_y = -1;
        else if (wheel < 36)
            dir_x = 1;
        else if (wheel < 60)
            dir_y = 1;
        else
            dir_x = -1;
    }

    world->wheel_touch_x = dir_x;
    world->wheel_touch_y = dir_y;
    world->held_move_x = dir_x;
    world->held_move_y = dir_y;
#else
    world->wheel_touch_x = 0;
    world->wheel_touch_y = 0;
#endif
}

static void apply_button_direction(struct pc_world_state *world,
                                   const struct pc_world_command *command)
{
    if (command->hold_x < 0)
    {
        world->held_move_x = -1;
        world->held_move_y = 0;
    }
    else if (command->hold_x > 0)
    {
        world->held_move_x = 1;
        world->held_move_y = 0;
    }
    else if (command->hold_y < 0)
    {
        world->held_move_x = 0;
        world->held_move_y = -1;
    }
    else if (command->hold_y > 0)
    {
        world->held_move_x = 0;
        world->held_move_y = 1;
    }

    if ((command->release_x < 0 && world->held_move_x < 0) ||
        (command->release_x > 0 && world->held_move_x > 0))
    {
        world->held_move_x = 0;
    }

    if ((command->release_y < 0 && world->held_move_y < 0) ||
        (command->release_y > 0 && world->held_move_y > 0))
    {
        world->held_move_y = 0;
    }
}

static enum pc_heading heading_for_input(int dx, int dy)
{
    if (dx < 0)
        return PC_HEADING_W;
    if (dx > 0)
        return PC_HEADING_E;
    if (dy < 0)
        return PC_HEADING_N;
    return PC_HEADING_S;
}

static void update_camera(struct pc_world_state *world, bool snap)
{
    int map_px_w = world->map_w * PC_WORLD_TILE_SIZE;
    int map_px_h = world->map_h * PC_WORLD_TILE_SIZE;
    int target_x;
    int target_y;
    int min_x;
    int min_y;
    int next_x;
    int next_y;

    if (!outdoor_scene(world->scene))
    {
        if (map_px_w <= LCD_WIDTH)
            world->origin_x = (LCD_WIDTH - map_px_w) / 2;
        else
            world->origin_x = clampi((LCD_WIDTH / 2) - world->player_x,
                                     LCD_WIDTH - map_px_w, 0);

        if (map_px_h <= LCD_HEIGHT)
            world->origin_y = (LCD_HEIGHT - map_px_h) / 2;
        else
            world->origin_y = clampi((LCD_HEIGHT / 2) -
                                     (world->player_y + PC_WORLD_FOOT_Y_OFFSET),
                                     LCD_HEIGHT - map_px_h, 0);
        return;
    }

    min_x = LCD_WIDTH - world->map_w * PC_WORLD_TILE_SIZE;
    min_y = LCD_HEIGHT - world->map_h * PC_WORLD_TILE_SIZE;
    target_x = clampi((LCD_WIDTH / 2) - world->player_x, min_x, 0);
    target_y = clampi((LCD_HEIGHT / 2) - (world->player_y + PC_WORLD_FOOT_Y_OFFSET),
                      min_y, 0);

    if (snap || outdoor_scene(world->scene))
    {
        next_x = target_x;
        next_y = target_y;
    }
    else
    {
        next_x = world->origin_x;
        next_y = world->origin_y;

        if (next_x < target_x)
            next_x = MIN(next_x + PC_WORLD_STEP_PX, target_x);
        else if (next_x > target_x)
            next_x = MAX(next_x - PC_WORLD_STEP_PX, target_x);

        if (next_y < target_y)
            next_y = MIN(next_y + PC_WORLD_STEP_PX, target_y);
        else if (next_y > target_y)
            next_y = MAX(next_y - PC_WORLD_STEP_PX, target_y);
    }

    world->origin_x = next_x;
    world->origin_y = next_y;
}

static int walk_frame_for_tick(int tick)
{
    static const unsigned char cycle[4] = { 0, 1, 2, 1 };

    return cycle[(tick / 2) & 3];
}

static void maybe_handle_transition(struct pc_world_state *world, bool moved)
{
    int block_x;
    int block_y;

    if (!moved)
        return;

    player_block_pos(world, &block_x, &block_y);

    switch (world->scene)
    {
        case PC_WORLD_SCENE_PALLET:
            if (player_in_metatile_zone(world, 5, 5, 5, 5) &&
                world->heading == PC_HEADING_N)
                transition_to_scene_metatile(world, PC_WORLD_SCENE_HOUSE_1F, 2, 6, PC_HEADING_N);
            else if (player_in_metatile_zone(world, 13, 14, 5, 5) &&
                     world->heading == PC_HEADING_N)
                transition_to_scene_metatile(world, PC_WORLD_SCENE_BLUES_HOUSE, 2, 6, PC_HEADING_N);
            else if (player_in_metatile_zone(world, 12, 12, 11, 11) &&
                     world->heading == PC_HEADING_N)
                transition_to_scene_metatile(world, PC_WORLD_SCENE_OAKS_LAB, 4, 10, PC_HEADING_N);
            else if (block_y <= 1 && block_x >= 4 && block_x <= 5 &&
                     world->heading == PC_HEADING_N)
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE1_SOUTH,
                                          block_x, PC_ROUTE1_MAP_H - 2,
                                          PC_HEADING_N);
            break;

        case PC_WORLD_SCENE_ROUTE1_SOUTH:
            if (block_y >= (world->map_h - 2) && block_x >= 4 && block_x <= 5 &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_PALLET,
                                          block_x, 1, PC_HEADING_S);
                init_spawns(world);
            }
            else if (block_y <= 1 && block_x >= 4 && block_x <= 5 &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                          block_x + 5, PC_VIRIDIAN_MAP_H - 2,
                                          PC_HEADING_N);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            if (player_in_metatile_zone(world, 23, 24, 25, 25) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_POKECENTER,
                                             3, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 29, 30, 19, 19) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_MART, 3, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 21, 22, 15, 15) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 21, 22, 9, 9) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 32, 33, 7, 7) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_GYM,
                                             16, 16, PC_HEADING_N);
            }
            else if (block_y >= (world->map_h - 2) && block_x >= 9 && block_x <= 10 &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE1_SOUTH,
                                          block_x - 5, 1, PC_HEADING_S);
                init_spawns(world);
            }
            else if (block_y <= 1 && block_x >= 9 && block_x <= 10 &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE2_SOUTH,
                                          block_x - 5, PC_ROUTE2_MAP_H - 2,
                                          PC_HEADING_N);
                init_spawns(world);
            }
            else if (block_x <= 1 && block_y >= 4 && block_y <= 12 &&
                     world->heading == PC_HEADING_W)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE22,
                                          PC_ROUTE22_MAP_W - 2, block_y - 4,
                                          PC_HEADING_W);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE22:
            if (player_in_metatile_zone(world, 8, 8, 5, 5) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE22_GATE,
                                             4, 6, PC_HEADING_N);
            }
            else if (block_x >= (world->map_w - 2) && block_y <= 8 &&
                     world->heading == PC_HEADING_E)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                          1, block_y + 4, PC_HEADING_E);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            if (block_y >= (world->map_h - 2) && block_x >= 4 && block_x <= 5 &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                          block_x + 5, 1, PC_HEADING_S);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 3, 3, 11, 11) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE,
                                             4, 1, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 15, 15, 19, 19) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 16, 16, 35, 35) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_GATE,
                                             4, 1, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 15, 15, 39, 39) &&
                     world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_GATE,
                                             4, 6, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 3, 3, 43, 43) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE,
                                             4, 6, PC_HEADING_N);
            }
            else if (block_y <= 1 && world->heading == PC_HEADING_N)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_PEWTER,
                                          block_x + 5, PC_PEWTER_MAP_H - 2,
                                          PC_HEADING_N);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            if (player_in_metatile_zone(world, 1, 2, 0, 0) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE,
                                             4, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 15, 18, 47, 47) &&
                     world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE,
                                             4, 1, PC_HEADING_S);
            }
            break;

        case PC_WORLD_SCENE_PEWTER:
            if (player_in_metatile_zone(world, 14, 15, 7, 7) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MUSEUM_1F,
                                             10, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 19, 19, 5, 5) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MUSEUM_1F,
                                             16, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 16, 16, 17, 17) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER_GYM,
                                             4, 12, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 29, 29, 13, 13) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 23, 23, 17, 17) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER_MART,
                                             3, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 7, 7, 29, 29) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 13, 13, 25, 25) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER_POKECENTER,
                                             3, 6, PC_HEADING_N);
            }
            else if (block_y >= (world->map_h - 2) &&
                     block_x >= 5 && block_x < (5 + PC_ROUTE2_MAP_W) &&
                     world->heading == PC_HEADING_S)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE2_SOUTH,
                                          block_x - 5, 1, PC_HEADING_S);
                init_spawns(world);
            }
            else if (block_x >= (world->map_w - 2) &&
                     world->heading == PC_HEADING_E)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE3,
                                          1, block_y - 4, PC_HEADING_E);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_MUSEUM_1F:
            if (player_in_metatile_zone(world, 10, 11, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER,
                                             14, 7, PC_HEADING_S);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 16, 17, 7, 7) &&
                     world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER,
                                             19, 5, PC_HEADING_S);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 7, 7, 7, 7))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MUSEUM_2F,
                                             7, 6, PC_HEADING_S);
            }
            break;

        case PC_WORLD_SCENE_MUSEUM_2F:
            if (player_in_metatile_zone(world, 7, 7, 7, 7))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MUSEUM_1F,
                                             7, 6, PC_HEADING_S);
            }
            break;

        case PC_WORLD_SCENE_ROUTE3:
            if (block_x <= 1 && world->heading == PC_HEADING_W)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_PEWTER,
                                          PC_PEWTER_MAP_W - 2, block_y + 4, PC_HEADING_W);
                init_spawns(world);
            }
            else if (block_y <= 1 && world->heading == PC_HEADING_N)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE4,
                                          block_x - 25, PC_ROUTE4_MAP_H - 2, PC_HEADING_N);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE4:
            if (player_in_metatile_zone(world, 11, 11, 5, 5) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_POKECENTER,
                                             3, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 18, 18, 5, 5) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_1F,
                                             14, 34, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 24, 24, 5, 5) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             27, 4, PC_HEADING_N);
            }
            else if (block_y >= (world->map_h - 2) && world->heading == PC_HEADING_S)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE3,
                                          block_x + 25, 1, PC_HEADING_S);
                init_spawns(world);
            }
            else if (block_x >= (world->map_w - 2) && world->heading == PC_HEADING_E)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_CERULEAN,
                                          1, block_y + 4, PC_HEADING_E);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_MT_MOON_POKECENTER:
            if (player_in_metatile_zone(world, 3, 4, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE4,
                                             11, 5, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_MT_MOON_1F:
            if (player_in_metatile_zone(world, 14, 15, 35, 35) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE4,
                                             18, 5, PC_HEADING_S);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 5, 5, 5, 5))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             5, 6, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 17, 17, 11, 11))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             17, 12, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 25, 25, 15, 15))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             25, 16, PC_HEADING_S);
            }
            break;

        case PC_WORLD_SCENE_MT_MOON_B1F:
            if (player_in_metatile_zone(world, 5, 5, 5, 5))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_1F,
                                             5, 6, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 17, 17, 11, 11))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B2F,
                                             25, 10, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 25, 25, 9, 9))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_1F,
                                             17, 12, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 25, 25, 15, 15))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_1F,
                                             25, 16, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 21, 21, 17, 17))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B2F,
                                             21, 18, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 13, 13, 27, 27))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B2F,
                                             15, 28, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 23, 23, 3, 3))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B2F,
                                             5, 8, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 27, 27, 3, 3))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE4,
                                             24, 5, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_MT_MOON_B2F:
            if (player_in_metatile_zone(world, 25, 25, 9, 9))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             17, 12, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 21, 21, 17, 17))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             21, 18, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 15, 15, 27, 27))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             13, 28, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 5, 5, 7, 7))
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_MT_MOON_B1F,
                                             23, 4, PC_HEADING_S);
            }
            break;

        case PC_WORLD_SCENE_CERULEAN:
            if (player_in_metatile_zone(world, 27, 27, 11, 11) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 27, 27, 9, 9) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE,
                                             3, 1, PC_HEADING_S);
            }
            else if (player_in_metatile_zone(world, 13, 13, 15, 15) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 19, 19, 17, 17) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_POKECENTER,
                                             3, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 30, 30, 19, 19) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_GYM,
                                             4, 12, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 13, 13, 25, 25) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_BIKE_SHOP,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 25, 25, 25, 25) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_MART,
                                             3, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 9, 9, 11, 11) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (player_in_metatile_zone(world, 9, 9, 9, 9) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE,
                                             2, 1, PC_HEADING_S);
            }
            else if (block_y <= 1 && block_x >= 5 && block_x < (5 + PC_ROUTE24_MAP_W) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE24,
                                          block_x - 5, PC_ROUTE24_MAP_H - 2, PC_HEADING_N);
                init_spawns(world);
            }
            else if (block_x <= 1 && world->heading == PC_HEADING_W)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE4,
                                          PC_ROUTE4_MAP_W - 2, block_y - 4, PC_HEADING_W);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE24:
            if (block_y >= (world->map_h - 2) && block_x >= 0 && block_x < PC_ROUTE24_MAP_W &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_CERULEAN,
                                          block_x + 5, 1, PC_HEADING_S);
                init_spawns(world);
            }
            else if (block_x >= (world->map_w - 2) && block_y < PC_ROUTE25_MAP_H &&
                     world->heading == PC_HEADING_E)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE25,
                                          1, block_y, PC_HEADING_E);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE25:
            if (player_in_metatile_zone(world, 45, 45, 3, 3) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_BILLS_HOUSE,
                                             2, 6, PC_HEADING_N);
            }
            else if (block_x <= 1 && block_y < PC_ROUTE24_MAP_H &&
                     world->heading == PC_HEADING_W)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE24,
                                          PC_ROUTE24_MAP_W - 2, block_y, PC_HEADING_W);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            if (block_y <= 1 && block_x >= 4 && block_x <= 5 &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_PALLET,
                                          block_x, PC_PALLET_MAP_H - 2,
                                          PC_HEADING_N);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_MART:
            if (player_in_metatile_zone(world, 3, 4, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                             29, 19, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_PEWTER_MART:
            if (player_in_metatile_zone(world, 3, 4, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER,
                                             23, 17, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_BLUES_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PALLET,
                                             13, 5, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_SOUTH,
                                             15, 19, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER,
                                             29, 13, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER,
                                             7, 29, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_HOUSE_1F:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PALLET,
                                             5, 5, PC_HEADING_S);
            else if (player_in_metatile_zone(world, 6, 7, 1, 2) &&
                     world->heading == PC_HEADING_N)
                transition_to_scene_metatile(world, PC_WORLD_SCENE_HOUSE_2F,
                                             5, 4, PC_HEADING_S);
            break;

        case PC_WORLD_SCENE_HOUSE_2F:
            if (player_in_metatile_zone(world, 6, 7, 1, 2) &&
                world->heading == PC_HEADING_S)
                transition_to_scene_metatile(world, PC_WORLD_SCENE_HOUSE_1F,
                                             5, 4, PC_HEADING_S);
            break;

        case PC_WORLD_SCENE_OAKS_LAB:
            if (player_in_metatile_zone(world, 4, 5, 11, 11) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PALLET,
                                             12, 11, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_POKECENTER:
            if (player_in_metatile_zone(world, 3, 4, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                             23, 25, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_PEWTER_POKECENTER:
            if (player_in_metatile_zone(world, 3, 4, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER,
                                             13, 25, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_CERULEAN_POKECENTER:
            if (player_in_metatile_zone(world, 3, 4, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             19, 17, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                             21, 15, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                             21, 9, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_GYM:
            if (player_in_metatile_zone(world, 16, 17, 17, 17) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                             32, 7, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_PEWTER_GYM:
            if (player_in_metatile_zone(world, 4, 5, 13, 13) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PEWTER,
                                             16, 17, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_CERULEAN_GYM:
            if (player_in_metatile_zone(world, 4, 5, 13, 13) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             30, 19, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE2_GATE:
            if (player_in_metatile_zone(world, 3, 5, 0, 0) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_SOUTH,
                                             16, 35, PC_HEADING_N);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 3, 5, 7, 7) &&
                     world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_SOUTH,
                                             15, 39, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_ROUTE22_GATE:
            if (player_in_metatile_zone(world, 3, 5, 0, 0) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE22,
                                             8, 1, PC_HEADING_N);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 3, 5, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE22,
                                             8, 5, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE:
            if (player_in_metatile_zone(world, 3, 5, 0, 0) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_FOREST,
                                             16, 46, PC_HEADING_N);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 3, 5, 7, 7) &&
                     world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_SOUTH,
                                             3, 43, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE:
            if (player_in_metatile_zone(world, 3, 5, 0, 0) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE2_SOUTH,
                                             3, 11, PC_HEADING_N);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 3, 5, 7, 7) &&
                     world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_FOREST,
                                             1, 1, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             13, 15, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             27, 11, PC_HEADING_S);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 3, 3, 0, 0) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             27, 9, PC_HEADING_N);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_BIKE_SHOP:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             13, 25, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_CERULEAN_MART:
            if (player_in_metatile_zone(world, 3, 4, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             25, 25, PC_HEADING_S);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             9, 11, PC_HEADING_S);
                init_spawns(world);
            }
            else if (player_in_metatile_zone(world, 2, 2, 0, 0) &&
                     world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_CERULEAN,
                                             9, 9, PC_HEADING_N);
                init_spawns(world);
            }
            break;

        case PC_WORLD_SCENE_BILLS_HOUSE:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_ROUTE25,
                                             45, 4, PC_HEADING_S);
                init_spawns(world);
            }
            break;
    }
}

void pc_world_init(struct pc_world_state *world)
{
    rb->memset(world, 0, sizeof(*world));
    ensure_pocketcatch_data_dir();
    load_unlocked_song_store();
    init_progress_defaults(world);
    init_assets(world);
    world->outdoor_region = -1;
    world->view = PC_WORLD_VIEW_MAP;
    if (!pc_world_try_load(world))
    {
        load_scene_data(world, PC_WORLD_SCENE_PALLET);
        transition_to_scene_metatile(world, PC_WORLD_SCENE_HOUSE_2F, 4, 6, PC_HEADING_S);
    }
    else if (outdoor_scene(world->scene))
    {
        refresh_outdoor_region_state(world, true);
    }
    world->last_encounter_slot = -1;
    world->pending_species_index = -1;
    if (world->dex_index < 0)
        world->dex_index = 0;
    if (world->bag_index < 0)
        world->bag_index = world->dex_index;
    reset_transient_world_state(world);
    update_camera(world, true);
}

void pc_world_teardown(struct pc_world_state *world)
{
    int heading;
    int frame;
    int i;

    for (heading = 0; heading < 4; ++heading)
    {
        for (frame = 0; frame < PC_WORLD_WALK_FRAMES; ++frame)
            clear_bitmap(&world->assets.trainer[heading][frame]);
    }
    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
        clear_bitmap(&world->assets.creature[i]);
    clear_bitmap(&world->assets.dex_creature);
}

static void open_world_menu(struct pc_world_state *world)
{
    world->view = PC_WORLD_VIEW_MENU;
    world->moving = false;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    set_detail(world, "Resume", "Songs  Buddy  Moves  Bag");
}

static void refresh_song_detail(struct pc_world_state *world)
{
    char line1[PC_BANNER_LINE_CHARS];

    if (pc_unlocked_songs.count <= 0)
    {
        set_detail(world, "No songs unlocked", "Find secret Poke Balls");
        world->song_index = 0;
        return;
    }

    world->song_index = clampi(world->song_index, 0, pc_unlocked_songs.count - 1);
    rb->strlcpy(line1, pc_path_basename(pc_unlocked_songs.paths[world->song_index]),
                sizeof(line1));
    set_detail(world, line1, "Select play  Left back");
}

static void open_song_overlay(struct pc_world_state *world)
{
    load_unlocked_song_store();
    world->view = PC_WORLD_VIEW_SONGS;
    world->moving = false;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    refresh_song_detail(world);
}

static void refresh_buddy_detail(struct pc_world_state *world)
{
    const struct pc_creature_def *buddy;

    sanitize_buddy(world);
    if (find_species_with_mode(world, 0, 1, true) < 0)
    {
        set_detail(world, "No buddy yet", "Catch Pokemon first");
        world->buddy_index = 0;
        return;
    }

    world->buddy_index = find_species_with_mode(world, world->buddy_index, 1, true);
    if (world->buddy_index < 0)
    {
        set_detail(world, "No buddy yet", "Catch Pokemon first");
        return;
    }

    buddy = pc_assets_get_creature(world->buddy_index);
    if (buddy == NULL)
    {
        set_detail(world, "No buddy yet", "Catch Pokemon first");
        return;
    }

    rb->snprintf(world->detail.line1, sizeof(world->detail.line1), "%s", buddy->name);
    if (world->buddy_species == world->buddy_index)
    {
        rb->snprintf(world->detail.line2, sizeof(world->detail.line2),
                     "Buddy %d/%d steps", world->buddy_steps, PC_BUDDY_CANDY_STEPS);
    }
    else
    {
        rb->snprintf(world->detail.line2, sizeof(world->detail.line2),
                     "Select to make buddy");
    }
}

static void open_buddy_overlay(struct pc_world_state *world)
{
    sanitize_buddy(world);
    world->view = PC_WORLD_VIEW_BUDDY;
    world->moving = false;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    if (world->buddy_species >= 0 && world->caught_counts[world->buddy_species] > 0)
        world->buddy_index = world->buddy_species;
    else if (world->bag_index >= 0 && world->caught_counts[world->bag_index] > 0)
        world->buddy_index = world->bag_index;
    else
        world->buddy_index = find_species_with_mode(world, 0, 1, true);
    refresh_buddy_detail(world);
}

static int mart_category_item_count(int category)
{
    switch (category)
    {
        case PC_MART_CATEGORY_ITEMS:
            return 1;

        case PC_MART_CATEGORY_HMS:
            return PC_FIELD_ABILITY_COUNT;

        case PC_MART_CATEGORY_LOOKS:
            return PC_PLAYER_TRAINER_COUNT;

        default:
            return 1;
    }
}

static void sanitize_mart_selection(struct pc_world_state *world)
{
    int count;

    if (world->mart_category < 0 || world->mart_category >= PC_MART_CATEGORY_COUNT)
        world->mart_category = PC_MART_CATEGORY_ITEMS;
    count = mart_category_item_count(world->mart_category);
    if (count <= 0)
        count = 1;
    if (world->mart_index < 0 || world->mart_index >= count)
        world->mart_index = 0;
}

static void open_mart_overlay(struct pc_world_state *world)
{
    sanitize_field_abilities(world);
    sanitize_player_trainer(world);
    world->view = PC_WORLD_VIEW_MART;
    world->moving = false;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    world->walk_tick = 0;
    world->walk_frame = 1;
    sanitize_mart_selection(world);
}

static void open_field_moves_overlay(struct pc_world_state *world)
{
    sanitize_field_abilities(world);
    world->view = PC_WORLD_VIEW_FIELD_MOVES;
    world->moving = false;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    if (world->field_index < 0 || world->field_index >= PC_FIELD_ABILITY_COUNT)
        world->field_index = 0;
}

static bool buy_mart_pokeballs(struct pc_world_state *world)
{
    char line2[PC_BANNER_LINE_CHARS];

    if (world->money < PC_MART_POKEBALL_COST)
    {
        set_notice(world, "Not enough money", "Spin PokeStops for more");
        return false;
    }

    world->money -= PC_MART_POKEBALL_COST;
    world->pokeballs = MIN((int)PC_MAX_POKEBALLS,
                           (int)world->pokeballs + PC_MART_POKEBALL_BUNDLE);
    rb->snprintf(line2, sizeof(line2), "Balls %u", world->pokeballs);
    set_notice(world, "Bought Poke Balls", line2);
    pc_world_save(world);
    return true;
}

static bool buy_field_ability(struct pc_world_state *world,
                              enum pc_field_ability ability,
                              int cost)
{
    if (ability >= PC_FIELD_ABILITY_COUNT)
        return false;

    if (world->ability_owned[ability])
    {
        set_notice(world, field_ability_name(ability), "Already purchased");
        return false;
    }

    if (world->money < cost)
    {
        set_notice(world, "Not enough money", "Spin PokeStops for more");
        return false;
    }

    world->money -= cost;
    world->ability_owned[ability] = 1;
    world->ability_species[ability] = -1;
    set_notice(world, field_ability_name(ability), "Bought at the mart");
    pc_world_save(world);
    return true;
}

static bool buy_or_equip_player_trainer(struct pc_world_state *world, int trainer_index)
{
    const struct pc_player_trainer_def *trainer;
    char line2[PC_BANNER_LINE_CHARS];

    trainer_index = clamp_player_trainer_index(trainer_index);
    trainer = &pc_player_trainers[trainer_index];

    if (!world->owned_trainers[trainer_index])
    {
        if (world->money < trainer->cost)
        {
            set_notice(world, "Not enough money", "Spin PokeStops for more");
            return false;
        }

        world->money -= trainer->cost;
        world->owned_trainers[trainer_index] = 1;
    }

    if (world->player_trainer == trainer_index)
    {
        set_notice(world, trainer->display_name, "Already equipped");
        return false;
    }

    world->owned_trainers[trainer_index] = 1;
    world->player_trainer = trainer_index;
    apply_player_trainer_choice(world);
    sanitize_player_trainer(world);
    reload_player_trainer_assets(world);
    pc_world_save(world);
    rb->snprintf(line2, sizeof(line2), "%s look equipped", trainer->display_name);
    set_notice(world, "Trainer changed", line2);
    return true;
}

static void open_field_assign_overlay(struct pc_world_state *world,
                                      enum pc_field_ability ability)
{
    int current = assigned_field_ability_species(world, ability);

    if (!world->ability_owned[ability])
    {
        set_notice(world, field_ability_name(ability), "Buy it first");
        return;
    }

    if (current < 0)
        current = 0;
    world->mart_assign_index = find_species_with_ability(world, ability, current, 1);
    if (world->mart_assign_index < 0)
    {
        set_notice(world, "No partner ready", "Catch a compatible Pokemon");
        return;
    }

    world->field_index = ability;
    world->view = PC_WORLD_VIEW_FIELD_ASSIGN;
}

static bool assign_field_ability(struct pc_world_state *world,
                                 enum pc_field_ability ability,
                                 int species_index)
{
    const struct pc_creature_def *creature;

    if (ability >= PC_FIELD_ABILITY_COUNT ||
        !world->ability_owned[ability] ||
        species_index < 0 || species_index >= PC_POKEDEX_MAX ||
        world->caught_counts[species_index] == 0 ||
        !species_can_use_field_ability(species_index, ability))
    {
        return false;
    }

    creature = pc_assets_get_creature(species_index);
    world->ability_species[ability] = species_index;
    if (creature != NULL)
    {
        char line2[PC_BANNER_LINE_CHARS];

        rb->snprintf(line2, sizeof(line2), "now uses %s", field_ability_name(ability));
        set_notice(world, creature->name, line2);
    }
    else
        set_notice(world, field_ability_name(ability), "Ready for field use");
    pc_world_save(world);
    return true;
}

static bool assign_buddy_species(struct pc_world_state *world, int species_index)
{
    const struct pc_creature_def *creature;
    char line2[PC_BANNER_LINE_CHARS];

    if (species_index < 0 || species_index >= PC_POKEDEX_MAX ||
        world->caught_counts[species_index] == 0)
    {
        return false;
    }

    if (world->buddy_species == species_index)
    {
        world->buddy_species = -1;
        world->buddy_steps = 0;
        set_notice(world, "Buddy cleared", "Pick another partner later");
        pc_world_save(world);
        return true;
    }

    creature = pc_assets_get_creature(species_index);
    world->buddy_species = species_index;
    world->buddy_steps = 0;

    if (creature != NULL)
    {
        rb->snprintf(line2, sizeof(line2), "Candy every %d steps", PC_BUDDY_CANDY_STEPS);
        set_notice(world, creature->name, line2);
    }
    else
    {
        set_notice(world, "Buddy set", "Candy progress started");
    }
    pc_world_save(world);
    return true;
}

static void refresh_bag_detail(struct pc_world_state *world)
{
    const struct pc_creature_def *creature;
    int family;
    unsigned count;

    if (world->bag_index < 0 || world->bag_index >= PC_POKEDEX_MAX ||
        world->caught_counts[world->bag_index] == 0)
    {
        set_detail(world, "Bag is empty", "Catch Pokemon first");
        clear_bitmap(&world->assets.dex_creature);
        return;
    }

    creature = pc_assets_get_creature(world->bag_index);
    if (creature == NULL)
    {
        set_detail(world, "Bag is empty", "Catch Pokemon first");
        clear_bitmap(&world->assets.dex_creature);
        return;
    }

    update_dex_asset(world, world->bag_index);
    family = pc_assets_get_family_index(world->bag_index);
    count = (family >= 0 && family < PC_POKEDEX_MAX) ? world->family_candy[family] : 0;
    rb->snprintf(world->detail.line1, sizeof(world->detail.line1),
                 "%s x%u", creature->name, world->caught_counts[world->bag_index]);
    if (world->buddy_species == world->bag_index)
        rb->snprintf(world->detail.line2, sizeof(world->detail.line2),
                     "Buddy  Candy %u", count);
    else
        rb->snprintf(world->detail.line2, sizeof(world->detail.line2),
                     "Candy %u", count);
}

static void refresh_pokedex_detail(struct pc_world_state *world)
{
    const struct pc_creature_def *creature = pc_assets_get_creature(world->dex_index);
    int family;
    int evolve_target;
    int evolve_cost;
    unsigned candy = 0;

    if (creature == NULL)
        return;

    update_dex_asset(world, world->dex_index);
    rb->snprintf(world->detail.line1, sizeof(world->detail.line1),
                 "#%03d %s", creature->species_id, creature->name);
    if (world->dex_index < PC_POKEDEX_MAX && world->caught_counts[world->dex_index] > 0)
    {
        family = pc_assets_get_family_index(world->dex_index);
        evolve_target = pc_assets_get_evolution_target(world->dex_index);
        evolve_cost = pc_assets_get_evolution_cost(world->dex_index);
        if (family >= 0 && family < PC_POKEDEX_MAX)
            candy = world->family_candy[family];

        if (evolve_target >= 0 && evolve_cost > 0)
            rb->snprintf(world->detail.line2, sizeof(world->detail.line2),
                         "Caught %u  Candy %u/%d",
                         world->caught_counts[world->dex_index], candy, evolve_cost);
        else
            rb->snprintf(world->detail.line2, sizeof(world->detail.line2),
                         "Caught %u  Candy %u",
                         world->caught_counts[world->dex_index], candy);
    }
    else
        rb->snprintf(world->detail.line2, sizeof(world->detail.line2),
                     "Seen in wild data");
}

static bool evolve_current_species(struct pc_world_state *world)
{
    int source = world->dex_index;
    int target = pc_assets_get_evolution_target(source);
    int family = pc_assets_get_family_index(source);
    int cost = pc_assets_get_evolution_cost(source);
    int i;
    const struct pc_creature_def *creature;

    if (source < 0 || source >= PC_POKEDEX_MAX ||
        target < 0 || target >= PC_POKEDEX_MAX ||
        family < 0 || family >= PC_POKEDEX_MAX ||
        cost <= 0 || world->caught_counts[source] == 0 ||
        world->family_candy[family] < cost)
    {
        return false;
    }

    creature = pc_assets_get_creature(target);
    world->caught_counts[source]--;
    world->caught_counts[target]++;
    world->family_candy[family] -= cost;
    for (i = 0; i < PC_FIELD_ABILITY_COUNT; ++i)
    {
        if (world->ability_species[i] == source)
            world->ability_species[i] = target;
    }
    if (world->buddy_species == source)
        world->buddy_species = target;
    sanitize_field_abilities(world);
    sanitize_buddy(world);
    world->dex_index = target;
    world->bag_index = target;
    refresh_pokedex_detail(world);
    refresh_bag_detail(world);

    if (creature != NULL)
        set_notice(world, creature->name, "Evolution complete");
    else
        set_notice(world, "Evolution complete", "Check your Pokedex");
    return true;
}

static void advance_world_index(struct pc_world_state *world, int *index,
                                int dir, bool caught_only)
{
    int count = pc_assets_get_total_creature_count();
    int next;

    if (count <= 0 || dir == 0)
        return;

    next = find_species_with_mode(world, *index + dir, dir > 0 ? 1 : -1, caught_only);
    if (next >= 0)
        *index = next;
}

static void award_buddy_progress(struct pc_world_state *world, int steps)
{
    int family;
    const struct pc_creature_def *creature;

    sanitize_buddy(world);
    if (world->buddy_species < 0 || steps <= 0)
        return;

    family = pc_assets_get_family_index(world->buddy_species);
    if (family < 0 || family >= PC_POKEDEX_MAX)
        return;

    world->buddy_steps += steps;
    if (world->buddy_steps < PC_BUDDY_CANDY_STEPS)
        return;

    world->buddy_steps -= PC_BUDDY_CANDY_STEPS;
    world->family_candy[family]++;
    creature = pc_assets_get_creature(world->buddy_species);
    if (creature != NULL)
        set_notice(world, creature->name, "Buddy found 1 candy");
    else
        set_notice(world, "Buddy reward", "Found 1 candy");
    pc_world_save(world);
}

static void update_pokestop_spin(struct pc_world_state *world,
                                 const struct pc_world_command *command)
{
    int delta = 0;

#ifdef HAVE_WHEEL_POSITION
    {
        int wheel = rb->wheel_status();

        if (wheel >= 0)
        {
            if (world->pokestop_last_wheel_angle >= 0)
            {
                delta = wheel - world->pokestop_last_wheel_angle;
                if (delta > 48)
                    delta -= 96;
                else if (delta < -48)
                    delta += 96;
            }
            world->pokestop_last_wheel_angle = wheel;
        }
        else
        {
            world->pokestop_last_wheel_angle = -1;
        }
    }
#else
    delta = (command->nav_y + command->nav_x) * 8;
#endif

    if (command->nav_y != 0 || command->nav_x != 0)
        delta += (command->nav_y + command->nav_x) * 4;

    if (delta != 0)
    {
        world->pokestop_spin_angle += delta * 6;
        world->pokestop_spin_progress += PC_ABS(delta);
    }

    if (!world->pokestop_spun &&
        world->pokestop_spin_progress >= PC_POKESTOP_SPIN_TARGET)
    {
        grant_pokestop_rewards(world);
    }
}

static void handle_world_view(struct pc_world_state *world,
                              const struct pc_world_command *command)
{
    static const char *const menu_items[PC_MENU_ITEMS] = {
        "Resume", "Songs", "Buddy", "Field Moves",
        "Backpack", "Pokedex", "Save Game", "Quit"
    };

    if (command->menu_requested)
    {
        if (world->view == PC_WORLD_VIEW_MAP)
            open_world_menu(world);
        else if (world->view == PC_WORLD_VIEW_POKESTOP)
            close_pokestop_overlay(world);
        else
            world->view = PC_WORLD_VIEW_MAP;
        return;
    }

    if (world->view == PC_WORLD_VIEW_MAP)
        return;

    if (world->view == PC_WORLD_VIEW_POKESTOP)
    {
        if (command->back)
        {
            close_pokestop_overlay(world);
            return;
        }

        update_pokestop_spin(world, command);
        if (command->confirm && world->pokestop_spun)
        {
            close_pokestop_overlay(world);
            return;
        }
        return;
    }

    if (world->view == PC_WORLD_VIEW_MENU)
    {
        if (command->nav_y < 0)
            world->menu_index = (world->menu_index + PC_MENU_ITEMS - 1) % PC_MENU_ITEMS;
        else if (command->nav_y > 0)
            world->menu_index = (world->menu_index + 1) % PC_MENU_ITEMS;

        set_detail(world, menu_items[world->menu_index], "Menu  Play nav  Select choose");

        if (command->confirm)
        {
            switch (world->menu_index)
            {
                case 0:
                    world->view = PC_WORLD_VIEW_MAP;
                    break;

                case 1:
                    open_song_overlay(world);
                    break;

                case 2:
                    open_buddy_overlay(world);
                    break;

                case 4:
                    world->view = PC_WORLD_VIEW_BAG;
                    world->bag_index = find_species_with_mode(world, world->bag_index, 1, true);
                    refresh_bag_detail(world);
                    break;

                case 5:
                    world->view = PC_WORLD_VIEW_POKEDEX;
                    world->dex_index = clamp_species_index(world->dex_index);
                    refresh_pokedex_detail(world);
                    break;

                case 6:
                    if (pc_world_save(world))
                        set_notice(world, "Game saved", "Hold Select for menu");
                    else
                        set_notice(world, "Save failed", "Check free space");
                    world->view = PC_WORLD_VIEW_MAP;
                    break;

                case 7:
                    world->quit_requested = true;
                    world->view = PC_WORLD_VIEW_MAP;
                    break;

                case 3:
                    open_field_moves_overlay(world);
                    break;
            }
        }
        return;
    }

    if (world->view == PC_WORLD_VIEW_MART)
    {
        if (command->back)
        {
            world->view = PC_WORLD_VIEW_MAP;
            return;
        }

        if (command->nav_x < 0)
        {
            world->mart_category =
                (world->mart_category + PC_MART_CATEGORY_COUNT - 1) % PC_MART_CATEGORY_COUNT;
            sanitize_mart_selection(world);
        }
        else if (command->nav_x > 0)
        {
            world->mart_category = (world->mart_category + 1) % PC_MART_CATEGORY_COUNT;
            sanitize_mart_selection(world);
        }

        if (command->nav_y < 0)
            world->mart_index =
                (world->mart_index + mart_category_item_count(world->mart_category) - 1) %
                mart_category_item_count(world->mart_category);
        else if (command->nav_y > 0)
            world->mart_index = (world->mart_index + 1) %
                                mart_category_item_count(world->mart_category);

        if (command->confirm)
        {
            switch (world->mart_category)
            {
                case PC_MART_CATEGORY_ITEMS:
                    buy_mart_pokeballs(world);
                    break;

                case PC_MART_CATEGORY_HMS:
                    if (world->mart_index == 0)
                        buy_field_ability(world, PC_FIELD_ABILITY_SURF, PC_MART_SURF_COST);
                    else if (world->mart_index == 1)
                        buy_field_ability(world, PC_FIELD_ABILITY_CUT, PC_MART_CUT_COST);
                    break;

                case PC_MART_CATEGORY_LOOKS:
                    buy_or_equip_player_trainer(world, world->mart_index);
                    break;
            }
        }
        return;
    }

    if (world->view == PC_WORLD_VIEW_SONGS)
    {
        if (command->back)
        {
            open_world_menu(world);
            return;
        }

        if (pc_unlocked_songs.count > 0)
        {
            if (command->nav_y < 0)
                world->song_index = (world->song_index + pc_unlocked_songs.count - 1) %
                                    pc_unlocked_songs.count;
            else if (command->nav_y > 0)
                world->song_index = (world->song_index + 1) % pc_unlocked_songs.count;

            if (command->confirm &&
                play_music_track(pc_unlocked_songs.paths[world->song_index]))
            {
                set_notice(world, "Now Playing",
                           pc_path_basename(pc_unlocked_songs.paths[world->song_index]));
            }
        }

        refresh_song_detail(world);
        return;
    }

    if (world->view == PC_WORLD_VIEW_BUDDY)
    {
        if (command->back)
        {
            open_world_menu(world);
            return;
        }

        if (command->nav_y != 0)
            advance_world_index(world, &world->buddy_index, command->nav_y, true);

        if (command->confirm)
            assign_buddy_species(world, world->buddy_index);

        refresh_buddy_detail(world);
        return;
    }

    if (world->view == PC_WORLD_VIEW_FIELD_MOVES)
    {
        if (command->back)
        {
            open_world_menu(world);
            return;
        }

        if (command->nav_y < 0)
            world->field_index = (world->field_index + PC_FIELD_ABILITY_COUNT - 1) %
                                 PC_FIELD_ABILITY_COUNT;
        else if (command->nav_y > 0)
            world->field_index = (world->field_index + 1) % PC_FIELD_ABILITY_COUNT;

        if (command->confirm)
        {
            if (!world->ability_owned[world->field_index])
            {
                char line2[PC_BANNER_LINE_CHARS];

                rb->snprintf(line2, sizeof(line2), "Buy it in the mart for $%d",
                             field_ability_cost((enum pc_field_ability)world->field_index));
                set_notice(world, field_ability_name((enum pc_field_ability)world->field_index),
                           line2);
            }
            else
            {
                open_field_assign_overlay(world, (enum pc_field_ability)world->field_index);
            }
        }
        return;
    }

    if (world->view == PC_WORLD_VIEW_FIELD_ASSIGN)
    {
        if (command->back)
        {
            open_field_moves_overlay(world);
            return;
        }

        if (command->nav_y != 0)
        {
            int next = find_species_with_ability(world,
                                                 (enum pc_field_ability)world->field_index,
                                                 world->mart_assign_index + command->nav_y,
                                                 command->nav_y > 0 ? 1 : -1);

            if (next >= 0)
                world->mart_assign_index = next;
        }

        if (command->confirm)
        {
            assign_field_ability(world, (enum pc_field_ability)world->field_index,
                                 world->mart_assign_index);
            open_field_moves_overlay(world);
        }
        return;
    }

    if (command->back)
    {
        open_world_menu(world);
        return;
    }

    if (world->view == PC_WORLD_VIEW_BAG)
    {
        advance_world_index(world, &world->bag_index, command->nav_y, true);
        refresh_bag_detail(world);
        return;
    }

    if (world->view == PC_WORLD_VIEW_POKEDEX)
    {
        if (command->nav_y != 0)
            advance_world_index(world, &world->dex_index, command->nav_y, false);
        else if (command->nav_x != 0)
            advance_world_index(world, &world->dex_index, command->nav_x, false);
        else if (command->confirm)
            evolve_current_species(world);
        refresh_pokedex_detail(world);
        return;
    }

    set_detail(world, menu_items[world->menu_index], "");
}

void pc_world_update(struct pc_world_state *world, const struct pc_world_command *command)
{
    int next_x = world->player_x;
    int next_y = world->player_y;
    bool moved = false;
    bool completed_step = false;
    bool encounter_check_step = false;
    bool allow_encounter_check = false;
    bool spawn_region_active = false;
    int i;

    world->frame++;
    if (world->notice_frames > 0)
        world->notice_frames--;

    if (command->exit_requested)
    {
        world->quit_requested = true;
        return;
    }

    handle_world_view(world, command);
    if (world->view != PC_WORLD_VIEW_MAP)
    {
        world->wheel_touch_x = 0;
        world->wheel_touch_y = 0;
        world->held_move_x = 0;
        world->held_move_y = 0;
        return;
    }

    if ((world->scene == PC_WORLD_SCENE_VIRIDIAN_MART ||
         world->scene == PC_WORLD_SCENE_PEWTER_MART ||
         world->scene == PC_WORLD_SCENE_CERULEAN_MART) &&
        command->confirm &&
        !world->moving &&
        world->step_remaining == 0)
    {
        open_mart_overlay(world);
        return;
    }

    if (command->confirm &&
        !world->moving &&
        world->step_remaining == 0 &&
        try_open_pokestop(world))
    {
        return;
    }

    if (command->confirm &&
        !world->moving &&
        world->step_remaining == 0 &&
        try_collect_music_secret(world))
    {
        return;
    }

    if (command->confirm &&
        !world->moving &&
        world->step_remaining == 0 &&
        try_use_cut(world))
    {
        return;
    }

    if (command->confirm &&
        world->scene == PC_WORLD_SCENE_PALLET &&
        current_outdoor_region(world) == PC_OUTDOOR_REGION_PALLET)
    {
        int block_x;
        int block_y;

        player_block_pos(world, &block_x, &block_y);
        if (block_y >= PC_PALLET_MAP_H - 1 &&
            block_x >= 2 && block_x <= 7)
        {
            if (assigned_field_ability_species(world, PC_FIELD_ABILITY_SURF) >= 0)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_ROUTE21_NORTH,
                                          4, 8, PC_HEADING_S);
                init_spawns(world);
            }
            else if (world->ability_owned[PC_FIELD_ABILITY_SURF])
            {
                set_notice(world, "Assign Surf first", "Open Field Moves in menu");
            }
            else
            {
                set_notice(world, "Buy HM03 Surf", "Viridian Mart sells it");
            }
            return;
        }
    }

    update_touch_direction(world);
    apply_button_direction(world, command);

    if (world->step_remaining > 0)
    {
        next_x += world->step_dx * PC_WORLD_STEP_PX;
        next_y += world->step_dy * PC_WORLD_STEP_PX;
        world->step_remaining -= PC_WORLD_STEP_PX;
        moved = true;

        if (world->step_remaining <= 0)
        {
            world->step_remaining = 0;
            world->step_dx = 0;
            world->step_dy = 0;
            completed_step = true;
        }
    }
    else
    {
        int try_dx = world->held_move_x;
        int try_dy = world->held_move_y;
        int step_px = PC_WORLD_TILE_SIZE / 2;
        if (try_dx < 0)
        {
            try_dx = -1;
        }
        else if (try_dx > 0)
        {
            try_dx = 1;
        }
        else if (try_dy < 0)
        {
            try_dy = -1;
        }
        else if (try_dy > 0)
        {
            try_dy = 1;
        }

        if (try_dx != 0 || try_dy != 0)
        {
            enum pc_heading desired_heading = heading_for_input(try_dx, try_dy);

            if ((enum pc_heading)world->heading != desired_heading)
            {
                world->heading = desired_heading;
            }
            else
            {
                int target_x = world->player_x + try_dx * step_px;
                int target_y = world->player_y + try_dy * step_px;

                if (player_step_allowed(world, target_x, target_y, try_dx, try_dy))
                {
                    world->step_dx = try_dx;
                    world->step_dy = try_dy;
                    world->step_remaining = step_px;
                    next_x += world->step_dx * PC_WORLD_STEP_PX;
                    next_y += world->step_dy * PC_WORLD_STEP_PX;
                    world->step_remaining -= PC_WORLD_STEP_PX;
                    moved = true;
                    if (world->step_remaining <= 0)
                    {
                        world->step_remaining = 0;
                        world->step_dx = 0;
                        world->step_dy = 0;
                        completed_step = true;
                    }
                }
                else if (try_dx == 0 && try_dy > 0 && try_start_ledge_jump(world))
                {
                    next_y += world->step_dy * PC_WORLD_STEP_PX;
                    world->step_remaining -= PC_WORLD_STEP_PX;
                    moved = true;
                }
            }
        }
    }

    if (moved)
    {
        world->player_x = next_x;
        world->player_y = next_y;
    }

    maybe_handle_transition(world, moved);
    update_camera(world, false);
    if (outdoor_scene(world->scene))
        refresh_outdoor_region_state(world, false);

    world->moving = moved || world->step_remaining > 0;
    if (world->moving)
    {
        world->walk_tick++;
        world->walk_frame = walk_frame_for_tick(world->walk_tick);
    }
    else
    {
        world->walk_tick = 0;
        world->walk_frame = 1;
    }

    if (!outdoor_scene(world->scene))
        return;

    spawn_region_active = outdoor_region_has_spawns(current_outdoor_region(world));

    if (completed_step)
    {
        tick_pokestop_cooldowns(world);
        world->travel_steps++;
        award_buddy_progress(world, 1);
        if (world->encounter_grace_steps > 0)
        {
            world->encounter_grace_steps--;
            encounter_check_step = false;
        }
        else
        {
            encounter_check_step = true;
        }
    }

    allow_encounter_check = moved || completed_step;

    if (spawn_region_active && completed_step)
    {
        for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
        {
            struct pc_world_spawn *spawn = &world->spawns[i];

            if (spawn->active)
            {
                if (spawn->lifetime_frames > 0)
                    spawn->lifetime_frames--;
                if (spawn->lifetime_frames == 0)
                    deactivate_spawn_slot(world, i);
            }
            else if (spawn->respawn_frames > 0)
            {
                spawn->respawn_frames--;
            }
        }

        if ((world->frame & 1) == 0 &&
            world->encounter_grace_steps <= 0 &&
            world->encounter_cooldown <= 0)
        {
            for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
                maybe_activate_spawn_slot(world, i);
        }
    }

    if (world->encounter_cooldown > 0)
        world->encounter_cooldown--;

    if (!spawn_region_active || !allow_encounter_check || !encounter_check_step)
        return;

    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        struct pc_world_spawn *spawn = &world->spawns[i];
        int dx;
        int dy;

        maybe_move_spawn(world, spawn);
        if (!spawn->active || world->pending_encounter || world->encounter_cooldown > 0)
            continue;

        dx = world->player_x - spawn->x;
        dy = world->player_y - spawn->y;
        if (dx * dx + dy * dy <= PC_WORLD_ENCOUNTER_RADIUS * PC_WORLD_ENCOUNTER_RADIUS)
        {
            const struct pc_creature_def *creature =
                pc_assets_get_creature(spawn->species_index);

            if (world->pokeballs == 0)
            {
                set_notice(world, "Out of Poke Balls", "Visit Viridian Mart");
                continue;
            }

            world->pending_encounter = true;
            world->pending_species_index = spawn->species_index;
            world->last_encounter_slot = i;
            spawn->active = false;
            if (creature != NULL)
            {
                char line1[PC_BANNER_LINE_CHARS];

                rb->snprintf(line1, sizeof(line1), "%s darted out", creature->name);
                set_banner(world, line1, "Encounter loading");
            }
        }
    }
}

void pc_world_finish_encounter(struct pc_world_state *world,
                               enum pc_catch_outcome outcome,
                               int species_index)
{
    int family;

    species_index = clamp_species_index(species_index);

    reset_transient_world_state(world);

    if (!outdoor_scene(world->scene))
        return;

    if (world->pokeballs > 0)
        world->pokeballs--;

    init_spawns(world);
    suppress_local_encounters(world,
                              -1,
                              PC_POST_ENCOUNTER_GRACE_STEPS,
                              PC_POST_ENCOUNTER_COOLDOWN);
    if (outcome == PC_CATCH_OUTCOME_CAUGHT && species_index < PC_POKEDEX_MAX)
    {
        world->caught_counts[species_index]++;
        family = pc_assets_get_family_index(species_index);
        if (family >= 0 && family < PC_POKEDEX_MAX)
            world->family_candy[family] += pc_assets_get_catch_candy(species_index);
        world->bag_index = species_index;
        world->dex_index = species_index;
    }

    if (outcome == PC_CATCH_OUTCOME_CAUGHT)
        set_notice(world, "Caught it", "Head home or keep exploring");
    else
        set_notice(world, "It broke out", "Walk into it again to retry");

    pc_world_save(world);
}

void pc_world_cancel_encounter(struct pc_world_state *world)
{
    if (world == NULL)
        return;

    reset_transient_world_state(world);
    if (outdoor_scene(world->scene))
        init_spawns(world);
    suppress_local_encounters(world,
                              -1,
                              PC_POST_ENCOUNTER_GRACE_STEPS,
                              PC_POST_ENCOUNTER_COOLDOWN);
    set_notice(world, "Encounter skipped", "Keep walking to look around");
    pc_world_save(world);
}
