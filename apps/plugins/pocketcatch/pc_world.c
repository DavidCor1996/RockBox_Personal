#include "pocketcatch.h"
#include "pc_red_gfx.h"
#include "pc_red_dojo_gfx.h"
#include "pc_red_pokecenter_gfx.h"

#define PC_PALLET_MAP_W  10
#define PC_PALLET_MAP_H   9
#define PC_ROUTE1_MAP_W  10
#define PC_ROUTE1_MAP_H  18
#define PC_VIRIDIAN_MAP_W 20
#define PC_VIRIDIAN_MAP_H 18
#define PC_ROUTE2_MAP_W  10
#define PC_ROUTE2_MAP_H  36
#define PC_ROUTE21_MAP_W 10
#define PC_ROUTE21_MAP_H 45
#define PC_HOUSE_MAP_W    4
#define PC_HOUSE_MAP_H    4
#define PC_LAB_MAP_W      5
#define PC_LAB_MAP_H      6

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

static const unsigned char pc_reds_house_1f_blocks[PC_HOUSE_MAP_H][PC_HOUSE_MAP_W] = {
    { 4,  9,  5,  7 },
    { 15, 15, 15, 15 },
    { 15, 1,  2,  15 },
    { 15, 11, 15, 15 },
};

static const unsigned char pc_reds_house_2f_blocks[PC_HOUSE_MAP_H][PC_HOUSE_MAP_W] = {
    { 16, 17, 5,  8  },
    { 15, 15, 15, 15 },
    { 15, 13, 15, 15 },
    { 12, 15, 15, 18 },
};

static const unsigned char pc_oaks_lab_blocks[PC_LAB_MAP_H][PC_LAB_MAP_W] = {
    { 0x65, 0x66, 0x67, 0x68, 0x68 },
    { 0x6b, 0x6b, 0x05, 0x69, 0x6a },
    { 0x05, 0x05, 0x05, 0x6d, 0x6e },
    { 0x68, 0x68, 0x05, 0x68, 0x68 },
    { 0x05, 0x05, 0x05, 0x05, 0x05 },
    { 0x05, 0x05, 0x04, 0x05, 0x05 },
};

static const unsigned char pc_viridian_mart_blocks[PC_HOUSE_MAP_H][PC_HOUSE_MAP_W] = {
    { 0x12, 0x13, 0x13, 0x09 },
    { 0x16, 0x0f, 0x14, 0x14 },
    { 0x18, 0x19, 0x15, 0x15 },
    { 0x17, 0x1a, 0x0b, 0x0f },
};

static const unsigned char pc_outside_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 3, 3 }, { 6, 3 }, { 3, 6 }, { 6, 6 }
};

static const unsigned char pc_route1_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 5, 10 }, { 4, 12 }, { 5, 14 }, { 4, 16 }
};

static const unsigned char pc_viridian_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 9, 10 }, { 10, 12 }, { 6, 14 }, { 14, 14 }
};

static const unsigned char pc_route2_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 4, 8 }, { 5, 12 }, { 4, 24 }, { 5, 32 }
};

static const unsigned char pc_route21_respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
    { 4, 8 }, { 5, 14 }, { 4, 24 }, { 5, 34 }
};

static const unsigned char pc_overworld_passable_tiles[] = {
    0x00, 0x10, 0x1b, 0x20, 0x21, 0x23, 0x2c, 0x2d, 0x2e, 0x30,
    0x31, 0x33, 0x39, 0x3c, 0x3e, 0x52, 0x54, 0x58, 0x5b
};

static const unsigned char pc_house_passable_tiles[] = {
    0x01, 0x02, 0x03, 0x11, 0x12, 0x13, 0x14, 0x1c, 0x1a
};

static const unsigned char pc_pokecenter_passable_tiles[] = {
    0x11, 0x1a, 0x1c, 0x3c, 0x5e
};

static const unsigned char pc_lab_passable_tiles[] = {
    0x01, 0x05, 0x11, 0x12, 0x14, 0x1a, 0x1c, 0x2c, 0x53
};

static fb_data pc_world_trainer_pixels[4][PC_WORLD_WALK_FRAMES]
                                      [PC_WORLD_TRAINER_MAX_W * PC_WORLD_TRAINER_MAX_H];
static fb_data pc_world_creature_pixels[PC_WORLD_MAX_SPAWNS]
                                       [PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H];
static fb_data pc_world_dex_pixels[PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H];

#define PC_SAVE_MAGIC 0x5043474f
#define PC_SAVE_VERSION 7
#define PC_MENU_ITEMS 5
#define PC_MART_ITEMS 4
#define PC_START_POKEBALLS 100
#define PC_MAX_POKEBALLS 999
#define PC_MART_POKEBALL_BUNDLE 10
#define PC_MART_POKEBALL_COST 20
#define PC_MART_SURF_COST 80
#define PC_TRAVEL_REWARD_STEPS 8
#define PC_TRAVEL_REWARD_MONEY 5
#define PC_WORLD_VISIBLE_SPAWNS_MAX 4
#define PC_WORLD_SPAWN_RESPAWN_MIN 10
#define PC_WORLD_SPAWN_RESPAWN_MAX 28
#define PC_WORLD_SPAWN_LIFETIME_MIN 35
#define PC_WORLD_SPAWN_LIFETIME_MAX 90

struct pc_spawn_entry {
    int species_id;
    int weight;
};

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

struct pc_save_record {
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

struct pc_save_record_v1 {
    int magic;
    int version;
    int scene;
    int player_x;
    int player_y;
    int heading;
    unsigned short caught_counts[64];
};

enum pc_outdoor_region {
    PC_OUTDOOR_REGION_ROUTE2 = 0,
    PC_OUTDOOR_REGION_VIRIDIAN,
    PC_OUTDOOR_REGION_ROUTE1,
    PC_OUTDOOR_REGION_PALLET,
    PC_OUTDOOR_REGION_ROUTE21
};

static const struct pc_spawn_entry pc_pallet_spawn_table[] = {
    { 16, 22 }, { 19, 20 }, { 21, 12 }, { 29, 10 }, { 32, 10 }, { 43, 8 },
    { 69, 7 }, { 10, 5 }, { 13, 4 }, { 25, 2 }
};

static const struct pc_spawn_entry pc_route1_spawn_table[] = {
    { 16, 18 }, { 19, 16 }, { 21, 14 }, { 29, 10 }, { 32, 10 }, { 43, 8 },
    { 69, 7 }, { 10, 6 }, { 13, 6 }, { 25, 3 }, { 39, 2 }
};

static const struct pc_spawn_entry pc_viridian_spawn_table[] = {
    { 16, 14 }, { 19, 14 }, { 21, 10 }, { 10, 10 }, { 13, 10 }, { 29, 8 },
    { 32, 8 }, { 25, 4 }, { 43, 6 }, { 69, 6 }, { 35, 3 }, { 39, 3 }, { 52, 2 }
};

static const struct pc_spawn_entry pc_route2_spawn_table[] = {
    { 10, 18 }, { 13, 18 }, { 16, 12 }, { 19, 10 }, { 21, 8 }, { 29, 7 },
    { 32, 7 }, { 43, 6 }, { 69, 6 }, { 25, 4 }, { 44, 2 }, { 70, 2 }
};

static const struct pc_spawn_entry pc_route21_spawn_table[] = {
    { 7, 6 }, { 54, 12 }, { 60, 18 }, { 61, 6 }, { 79, 14 }, { 118, 12 },
    { 119, 6 }, { 120, 10 }, { 121, 5 }, { 129, 12 }, { 130, 4 }, { 183, 5 },
    { 184, 4 }, { 194, 4 }, { 195, 3 }
};

static void player_block_pos(const struct pc_world_state *world,
                             int *block_x, int *block_y);
static void init_spawns(struct pc_world_state *world);
static bool outdoor_scene(enum pc_world_scene scene);

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

static void init_progress_defaults(struct pc_world_state *world)
{
    int i;

    world->pokeballs = PC_START_POKEBALLS;
    world->money = 0;
    world->travel_steps = 0;
    for (i = 0; i < PC_FIELD_ABILITY_COUNT; ++i)
    {
        world->ability_species[i] = -1;
        world->ability_owned[i] = 0;
    }
}

static const char *field_ability_name(enum pc_field_ability ability)
{
    switch (ability)
    {
        case PC_FIELD_ABILITY_SURF:
            return "HM03 Surf";

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

        default:
            return false;
    }
}

static int assigned_field_ability_species(const struct pc_world_state *world,
                                          enum pc_field_ability ability)
{
    int index;

    if (ability < 0 || ability >= PC_FIELD_ABILITY_COUNT)
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
        case PC_OUTDOOR_REGION_ROUTE2:
            *count = ARRAYLEN(pc_route2_spawn_table);
            return pc_route2_spawn_table;

        case PC_OUTDOOR_REGION_VIRIDIAN:
            *count = ARRAYLEN(pc_viridian_spawn_table);
            return pc_viridian_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE1:
            *count = ARRAYLEN(pc_route1_spawn_table);
            return pc_route1_spawn_table;

        case PC_OUTDOOR_REGION_ROUTE21:
            *count = ARRAYLEN(pc_route21_spawn_table);
            return pc_route21_spawn_table;

        case PC_OUTDOOR_REGION_PALLET:
        default:
            *count = ARRAYLEN(pc_pallet_spawn_table);
            return pc_pallet_spawn_table;
    }
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
        return 0;

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

    return 0;
}

static void copy_block_rows(unsigned char dst[PC_WORLD_MAP_MAX_H][PC_WORLD_MAP_MAX_W],
                            int dst_y,
                            const unsigned char *src,
                            int rows, int cols)
{
    int y;
    int x;

    for (y = 0; y < rows; ++y)
    {
        for (x = 0; x < cols; ++x)
            dst[dst_y + y][x] = src[y * cols + x];
    }
}

static enum pc_outdoor_region current_outdoor_region(const struct pc_world_state *world)
{
    switch (world->scene)
    {
        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            return PC_OUTDOOR_REGION_ROUTE2;

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            return PC_OUTDOOR_REGION_VIRIDIAN;

        case PC_WORLD_SCENE_ROUTE1_SOUTH:
            return PC_OUTDOOR_REGION_ROUTE1;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            return PC_OUTDOOR_REGION_ROUTE21;

        default:
            return PC_OUTDOOR_REGION_PALLET;
    }
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
        case PC_OUTDOOR_REGION_ROUTE2:
            set_banner(world, "Route 2", "South to Viridian");
            break;

        case PC_OUTDOOR_REGION_VIRIDIAN:
            set_banner(world, "Viridian City", "South to Route 1, north to Route 2");
            break;

        case PC_OUTDOOR_REGION_ROUTE1:
            set_banner(world, "Route 1", "North to Viridian, south to Pallet");
            break;

        case PC_OUTDOOR_REGION_ROUTE21:
            set_banner(world, "Route 21", "Northern waters off Pallet");
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

static void select_collision_tileset(enum pc_world_scene scene,
                                     const unsigned char (**blocks)[4][4],
                                     int *block_count,
                                     const unsigned char **passable_tiles,
                                     size_t *passable_count)
{
    if (outdoor_scene(scene))
    {
        *blocks = pc_red_overworld_blocks;
        *block_count = PC_RED_BLOCK_COUNT;
        *passable_tiles = pc_overworld_passable_tiles;
        *passable_count = ARRAYLEN(pc_overworld_passable_tiles);
    }
    else if (scene == PC_WORLD_SCENE_OAKS_LAB)
    {
        *blocks = pc_red_dojo_blocks;
        *block_count = PC_RED_DOJO_BLOCK_COUNT;
        *passable_tiles = pc_lab_passable_tiles;
        *passable_count = ARRAYLEN(pc_lab_passable_tiles);
    }
    else if (scene == PC_WORLD_SCENE_VIRIDIAN_MART)
    {
        *blocks = pc_red_pokecenter_blocks;
        *block_count = PC_RED_POKECENTER_BLOCK_COUNT;
        *passable_tiles = pc_pokecenter_passable_tiles;
        *passable_count = ARRAYLEN(pc_pokecenter_passable_tiles);
    }
    else
    {
        *blocks = pc_red_house_blocks;
        *block_count = PC_RED_HOUSE_BLOCK_COUNT;
        *passable_tiles = pc_house_passable_tiles;
        *passable_count = ARRAYLEN(pc_house_passable_tiles);
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
           scene == PC_WORLD_SCENE_ROUTE2_SOUTH ||
           scene == PC_WORLD_SCENE_ROUTE21_NORTH;
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
                   ((metatile_x == 5 && metatile_y == 5) ||
                    (metatile_x == 12 && metatile_y == 11));

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            return move_dy < 0 &&
                   metatile_y == 19 &&
                   metatile_x >= 29 && metatile_x <= 30;

        case PC_WORLD_SCENE_VIRIDIAN_MART:
            return move_dy > 0 &&
                   metatile_y == 7 &&
                   metatile_x >= 3 && metatile_x <= 4;

        case PC_WORLD_SCENE_HOUSE_1F:
            if (move_dy > 0 &&
                metatile_y == 7 &&
                metatile_x >= 2 && metatile_x <= 3)
            {
                return true;
            }

            return move_dy < 0 &&
                   metatile_x == 7 && metatile_y == 1;

        case PC_WORLD_SCENE_HOUSE_2F:
            return move_dy > 0 &&
                   metatile_x == 7 && metatile_y == 1;

        case PC_WORLD_SCENE_OAKS_LAB:
            return move_dy > 0 &&
                   metatile_y == 11 &&
                   metatile_x >= 4 && metatile_x <= 5;

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
    int block_x;
    int block_y;
    unsigned char ledge_id;

    if (!outdoor_scene(world->scene) || world->heading != PC_HEADING_S)
        return false;

    player_block_pos(world, &block_x, &block_y);
    ledge_id = block_at(world, block_x, block_y + 1);
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
            set_banner(world, "Viridian City", "South to Route 1, north to Route 2");
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

        case PC_WORLD_SCENE_OAKS_LAB:
            world->map_w = PC_LAB_MAP_W;
            world->map_h = PC_LAB_MAP_H;
            world->origin_x = (LCD_WIDTH - (PC_LAB_MAP_W * PC_WORLD_TILE_SIZE)) / 2;
            world->origin_y = (LCD_HEIGHT - (PC_LAB_MAP_H * PC_WORLD_TILE_SIZE)) / 2;
            copy_block_rows(world->tiles, 0, &pc_oaks_lab_blocks[0][0],
                            PC_LAB_MAP_H, PC_LAB_MAP_W);
            set_banner(world, "Oak's Lab", "Two trainers are battling");
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
        case PC_WORLD_SCENE_ROUTE1_SOUTH:
            return pc_route1_respawn_blocks;

        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
            return pc_viridian_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            return pc_route2_respawn_blocks;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            return pc_route21_respawn_blocks;

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

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            set_player_to_block(world, 4, 34, PC_HEADING_N);
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
            set_player_to_metatile(world, 4, 6);
            world->heading = PC_HEADING_S;
            break;

        case PC_WORLD_SCENE_OAKS_LAB:
            set_player_to_metatile(world, 4, 10);
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
    struct pc_save_record record;
    struct pc_save_record_v4 record_v4;
    struct pc_save_record_v1 record_v1;
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

    got = rb->read(fd, &record, sizeof(record));
    rb->close(fd);
    if (got == (ssize_t)sizeof(record_v1))
    {
        rb->memcpy(&record_v1, &record, sizeof(record_v1));
        if (record_v1.magic != PC_SAVE_MAGIC ||
            record_v1.version != 1 ||
            record_v1.scene < 0 || record_v1.scene > PC_WORLD_SCENE_OAKS_LAB)
        {
            return false;
        }

        scene = (enum pc_world_scene)record_v1.scene;
        world->player_x = record_v1.player_x;
        world->player_y = record_v1.player_y;
        scale_legacy_player_position(world);
        legacy_outdoor_layout = true;
        scene = migrate_legacy_outdoor_position(world, scene);
        migrated_save = true;
        load_scene_data(world, scene);
        world->heading = record_v1.heading;
        rb->memset(world->caught_counts, 0, sizeof(world->caught_counts));
        rb->memset(world->family_candy, 0, sizeof(world->family_candy));
        rb->memcpy(world->caught_counts, record_v1.caught_counts, sizeof(record_v1.caught_counts));
        for (i = 0; i < 64; ++i)
        {
            int family = pc_assets_get_family_index(i);

            if (family >= 0 && family < PC_POKEDEX_MAX && world->caught_counts[i] > 0)
                world->family_candy[family] += world->caught_counts[i] * 3;
        }
    }
    else if (got == (ssize_t)sizeof(record_v4))
    {
        rb->memcpy(&record_v4, &record, sizeof(record_v4));
        if (record_v4.magic != PC_SAVE_MAGIC ||
            record_v4.version < 2 || record_v4.version > 4 ||
            record_v4.scene < 0 || record_v4.scene > PC_WORLD_SCENE_OAKS_LAB)
        {
            return false;
        }

        scene = (enum pc_world_scene)record_v4.scene;
        world->player_x = record_v4.player_x;
        world->player_y = record_v4.player_y;
        if (record_v4.version < 6)
        {
            legacy_outdoor_layout = true;
            scene = migrate_legacy_outdoor_position(world, scene);
            migrated_save = true;
        }
        if (record_v4.version < PC_SAVE_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = record_v4.heading;
        rb->memcpy(world->caught_counts, record_v4.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, record_v4.family_candy, sizeof(world->family_candy));
        init_progress_defaults(world);
    }
    else if (got == (ssize_t)sizeof(record))
    {
        if (record.magic != PC_SAVE_MAGIC ||
            record.version < 5 || record.version > PC_SAVE_VERSION ||
            record.scene < 0 || record.scene > PC_WORLD_SCENE_OAKS_LAB)
        {
            return false;
        }

        scene = (enum pc_world_scene)record.scene;
        world->player_x = record.player_x;
        world->player_y = record.player_y;
        if (record.version < 6)
        {
            legacy_outdoor_layout = true;
            scene = migrate_legacy_outdoor_position(world, scene);
            migrated_save = true;
        }
        if (record.version < PC_SAVE_VERSION)
            needs_safe_outdoor_reset = true;
        load_scene_data(world, scene);
        world->heading = record.heading;
        world->travel_steps = record.travel_steps;
        world->pokeballs = record.pokeballs;
        world->money = record.money;
        rb->memcpy(world->caught_counts, record.caught_counts, sizeof(world->caught_counts));
        rb->memcpy(world->family_candy, record.family_candy, sizeof(world->family_candy));
        rb->memcpy(world->ability_species, record.ability_species, sizeof(world->ability_species));
        rb->memcpy(world->ability_owned, record.ability_owned, sizeof(world->ability_owned));
    }
    else
    {
        return false;
    }

    if (world->pokeballs > PC_MAX_POKEBALLS)
        world->pokeballs = PC_MAX_POKEBALLS;
    sanitize_field_abilities(world);
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

    rb->memset(&record, 0, sizeof(record));
    record.magic = PC_SAVE_MAGIC;
    record.version = PC_SAVE_VERSION;
    record.scene = world->scene;
    record.player_x = world->player_x;
    record.player_y = world->player_y;
    record.heading = world->heading;
    record.travel_steps = world->travel_steps;
    record.pokeballs = world->pokeballs;
    record.money = world->money;
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
    int i;

    clear_spawns(world);
    if (!outdoor_scene(world->scene))
        return;

    respawns = scene_respawn_blocks(world);
    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
        configure_spawn_slot(world, i, respawns[i][0], respawns[i][1]);

    world->encounter_cooldown = MAX(world->encounter_cooldown, 12);
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
    int target_x;
    int target_y;
    int min_x;
    int min_y;
    int next_x;
    int next_y;

    if (!outdoor_scene(world->scene))
    {
        int indoor_x = (LCD_WIDTH - (world->map_w * PC_WORLD_TILE_SIZE)) / 2;
        int indoor_y = (LCD_HEIGHT - (world->map_h * PC_WORLD_TILE_SIZE)) / 2;

        world->origin_x = indoor_x;
        world->origin_y = indoor_y;
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
            if (player_in_metatile_zone(world, 29, 30, 19, 19) &&
                world->heading == PC_HEADING_N)
            {
                transition_to_scene_metatile(world, PC_WORLD_SCENE_VIRIDIAN_MART, 3, 6, PC_HEADING_N);
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
            break;

        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            if (block_y >= (world->map_h - 2) && block_x >= 4 && block_x <= 5 &&
                world->heading == PC_HEADING_S)
            {
                transition_to_scene_block(world, PC_WORLD_SCENE_VIRIDIAN_SOUTH,
                                          block_x + 5, 1, PC_HEADING_S);
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

        case PC_WORLD_SCENE_HOUSE_1F:
            if (player_in_metatile_zone(world, 2, 3, 7, 7) &&
                world->heading == PC_HEADING_S)
                transition_to_scene_metatile(world, PC_WORLD_SCENE_PALLET,
                                             5, 5, PC_HEADING_S);
            else if (player_in_metatile_zone(world, 7, 7, 1, 1))
                transition_to_scene_metatile(world, PC_WORLD_SCENE_HOUSE_2F, 6, 3, PC_HEADING_N);
            break;

        case PC_WORLD_SCENE_HOUSE_2F:
            if (player_in_metatile_zone(world, 7, 7, 1, 1))
                transition_to_scene_metatile(world, PC_WORLD_SCENE_HOUSE_1F, 6, 3, PC_HEADING_S);
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
    }
}

void pc_world_init(struct pc_world_state *world)
{
    rb->memset(world, 0, sizeof(*world));
    init_assets(world);
    init_progress_defaults(world);
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
    set_detail(world, "Resume", "Backpack  Pokedex  Save  Quit");
}

static void open_mart_overlay(struct pc_world_state *world)
{
    sanitize_field_abilities(world);
    world->view = PC_WORLD_VIEW_MART;
    world->moving = false;
    world->held_move_x = 0;
    world->held_move_y = 0;
    world->step_dx = 0;
    world->step_dy = 0;
    world->step_remaining = 0;
    world->walk_tick = 0;
    world->walk_frame = 1;
    if (world->mart_index < 0 || world->mart_index >= PC_MART_ITEMS)
        world->mart_index = 0;
}

static bool buy_mart_pokeballs(struct pc_world_state *world)
{
    char line2[PC_BANNER_LINE_CHARS];

    if (world->money < PC_MART_POKEBALL_COST)
    {
        set_notice(world, "Not enough money", "Keep walking to earn more");
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
    if (ability < 0 || ability >= PC_FIELD_ABILITY_COUNT)
        return false;

    if (world->ability_owned[ability])
    {
        set_notice(world, field_ability_name(ability), "Already purchased");
        return false;
    }

    if (world->money < cost)
    {
        set_notice(world, "Not enough money", "Keep walking to earn more");
        return false;
    }

    world->money -= cost;
    world->ability_owned[ability] = 1;
    world->ability_species[ability] = -1;
    set_notice(world, field_ability_name(ability), "Bought at the mart");
    pc_world_save(world);
    return true;
}

static void open_mart_assign_overlay(struct pc_world_state *world,
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

    world->view = PC_WORLD_VIEW_MART_ASSIGN;
}

static bool assign_field_ability(struct pc_world_state *world,
                                 enum pc_field_ability ability,
                                 int species_index)
{
    const struct pc_creature_def *creature;

    if (ability < 0 || ability >= PC_FIELD_ABILITY_COUNT ||
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
        set_notice(world, creature->name, "now uses Surf");
    else
        set_notice(world, "Surf assigned", "Ready for field use");
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
    if (world->ability_species[PC_FIELD_ABILITY_SURF] == source)
        world->ability_species[PC_FIELD_ABILITY_SURF] = target;
    sanitize_field_abilities(world);
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

static void handle_world_view(struct pc_world_state *world,
                              const struct pc_world_command *command)
{
    static const char *const menu_items[PC_MENU_ITEMS] = {
        "Resume", "Backpack", "Pokedex", "Save Game", "Quit"
    };

    if (command->menu_requested)
    {
        if (world->view == PC_WORLD_VIEW_MAP)
            open_world_menu(world);
        else
            world->view = PC_WORLD_VIEW_MAP;
        return;
    }

    if (world->view == PC_WORLD_VIEW_MAP)
        return;

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
                    world->view = PC_WORLD_VIEW_BAG;
                    world->bag_index = find_species_with_mode(world, world->bag_index, 1, true);
                    refresh_bag_detail(world);
                    break;

                case 2:
                    world->view = PC_WORLD_VIEW_POKEDEX;
                    world->dex_index = clamp_species_index(world->dex_index);
                    refresh_pokedex_detail(world);
                    break;

                case 3:
                    if (pc_world_save(world))
                        set_notice(world, "Game saved", "Hold Select for menu");
                    else
                        set_notice(world, "Save failed", "Check free space");
                    world->view = PC_WORLD_VIEW_MAP;
                    break;

                case 4:
                    world->quit_requested = true;
                    world->view = PC_WORLD_VIEW_MAP;
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

        if (command->nav_y < 0)
            world->mart_index = (world->mart_index + PC_MART_ITEMS - 1) % PC_MART_ITEMS;
        else if (command->nav_y > 0)
            world->mart_index = (world->mart_index + 1) % PC_MART_ITEMS;

        if (command->confirm)
        {
            switch (world->mart_index)
            {
                case 0:
                    buy_mart_pokeballs(world);
                    break;

                case 1:
                    buy_field_ability(world, PC_FIELD_ABILITY_SURF, PC_MART_SURF_COST);
                    break;

                case 2:
                    open_mart_assign_overlay(world, PC_FIELD_ABILITY_SURF);
                    break;

                case 3:
                default:
                    world->view = PC_WORLD_VIEW_MAP;
                    break;
            }
        }
        return;
    }

    if (world->view == PC_WORLD_VIEW_MART_ASSIGN)
    {
        if (command->back)
        {
            open_mart_overlay(world);
            return;
        }

        if (command->nav_y != 0)
        {
            int next = find_species_with_ability(world,
                                                 PC_FIELD_ABILITY_SURF,
                                                 world->mart_assign_index + command->nav_y,
                                                 command->nav_y > 0 ? 1 : -1);

            if (next >= 0)
                world->mart_assign_index = next;
        }

        if (command->confirm)
        {
            assign_field_ability(world, PC_FIELD_ABILITY_SURF, world->mart_assign_index);
            open_mart_overlay(world);
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

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_MART &&
        command->confirm &&
        !world->moving &&
        world->step_remaining == 0)
    {
        open_mart_overlay(world);
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
                set_notice(world, "Assign Surf first", "Choose a partner in the mart");
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

    if (completed_step)
    {
        world->travel_steps++;
        if (world->travel_steps >= PC_TRAVEL_REWARD_STEPS)
        {
            world->travel_steps -= PC_TRAVEL_REWARD_STEPS;
            world->money += PC_TRAVEL_REWARD_MONEY;
        }

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

        if ((world->frame & 1) == 0)
        {
            for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
                maybe_activate_spawn_slot(world, i);
        }
    }

    if (world->encounter_cooldown > 0)
        world->encounter_cooldown--;

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

    world->pending_encounter = false;
    world->pending_species_index = -1;

    if (!outdoor_scene(world->scene))
        return;

    if (world->pokeballs > 0)
        world->pokeballs--;

    if (world->last_encounter_slot >= 0 &&
        world->last_encounter_slot < PC_WORLD_MAX_SPAWNS)
    {
        deactivate_spawn_slot(world, world->last_encounter_slot);
        world->spawns[world->last_encounter_slot].respawn_frames +=
            outcome == PC_CATCH_OUTCOME_CAUGHT ? 2 : 0;
    }

    world->encounter_cooldown = 18;
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

    world->pending_encounter = false;
    world->pending_species_index = -1;
    if (world->last_encounter_slot >= 0 &&
        world->last_encounter_slot < PC_WORLD_MAX_SPAWNS)
    {
        deactivate_spawn_slot(world, world->last_encounter_slot);
    }
    world->encounter_cooldown = MAX(world->encounter_cooldown, 12);
    set_notice(world, "Encounter skipped", "Keep walking to look around");
    pc_world_save(world);
}
