#include "dialtone.h"
#include "audio.h"
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include "../res/startup_scott_stinks_a.h"
#include "../res/startup_bimpson_productions_a.h"
#include "../res/startup_dialtone_title_a.h"
#include "../res/startup_dialtone_title_b.h"
#include "../res/luma_lane_district_v2.h"
#include "../res/luma_lane_apartment.h"
#include "../res/needle_and_neon_records.h"
#include "../res/rook_repair_tech.h"
#include "../res/soma_ramen.h"
#include "../res/iona_rooftop.h"
#include "../res/southline_overpass.h"

enum {
    HOTSPOT_BED = 1,
    HOTSPOT_DECOR,
    HOTSPOT_MEDIA,
    HOTSPOT_GARAGE
};

enum {
    RIDE_FOOT = 0,
    RIDE_BICYCLE,
    RIDE_SCOOTER,
    RIDE_MOTORCYCLE
};

enum {
    TRAFFIC_TYPE_BIKE = 0,
    TRAFFIC_TYPE_MOTOR
};

#define PLAYER_OAM_BASE 0u
#define NPC_OAM_BASE 4u
#define SPRITES_PER_ACTOR 4u
#define TRAFFIC_OAM_BASE (NPC_OAM_BASE + NPC_COUNT * SPRITES_PER_ACTOR)
#define TRAFFIC_COUNT 2u
#define PLAYER_FRAME_DOWN_STAND 0u
#define PLAYER_FRAME_DOWN_WALK 4u
#define PLAYER_FRAME_UP_STAND 8u
#define PLAYER_FRAME_UP_WALK 12u
#define PLAYER_FRAME_SIDE_STAND 16u
#define PLAYER_FRAME_SIDE_WALK 20u
#define PLAYER_SPRITE_TILE_COUNT 24u
#define NPC_SPRITE_TILE_OFFSET 24u
#define TRAFFIC_SPRITE_TILE_OFFSET 44u

static font_t ui_font;
static uint8_t room_id;
static uint8_t player_x;
static uint8_t player_y;
static uint8_t facing;
static uint8_t credits;
static uint8_t day_count;
static uint8_t task_id;
static uint8_t task_done;
static uint8_t inventory;
static uint8_t media;
static uint8_t decor_slots[3];
static uint8_t ride_level;
static uint8_t palette_mood;
static uint8_t frame_clock;
static uint8_t map_buffer[MAP_W * MAP_H];

static void show_banked_image_card(const uint8_t *tiles, uint8_t tile_count, const unsigned char *map, const palette_color_t *palettes, uint8_t bank);
static void render_banked_room(void);
static void overlay_apartment_decor(void);
static const palette_color_t *current_room_palettes(void);
static uint8_t current_room_tile_count(void);
static uint8_t current_sprite_tile_base(void);
static uint8_t current_sprite_tile_count(void);
static uint8_t npc_blocks(uint8_t x, uint8_t y);
static uint8_t is_solid(uint8_t tile);
static void sanitize_player_position(void);
static void begin_display_reload(void);
static void finish_display_reload(uint8_t show_sprites);

static void draw_text(const char *text);
static void draw_text_xy(uint8_t x, uint8_t y, const char *text);
static void draw_u8(uint8_t value);
static const char *ride_name(uint8_t level);

static uint8_t tile_for_char(char c) {
    switch (c) {
        case '.': return 0u;
        case ',': return 1u;
        case '=': return 2u;
        case '#': return 3u;
        case 'w': return 4u;
        case 'd': return 5u;
        case 'c': return 6u;
        case 's': return 7u;
        case 'r':
        case 'R': return 8u;
        case 'v': return 9u;
        case 'n': return 10u;
        case 'a': return 11u;
        case '|': return 12u;
        case 'b': return 13u;
        case 'P': return 14u;
        case 'g':
        case 'G': return 15u;
        case 'm': return 16u;
        case 'h': return 17u;
        case 'p': return 18u;
        case 'l': return 19u;
        case 'x': return 20u;
        case 'f': return 21u;
        case 't': return 22u;
        case 'o': return 23u;
        case 'k': return 24u;
        case 'q': return 30u;
        case 'i': return 31u;
        case '1':
        case '2':
        case '3':
            return 0u;
        default:
            return 0u;
    }
}

static uint8_t decor_tile(uint8_t decor_id) {
    switch (decor_id) {
        case DECOR_POSTER: return 14u;
        case DECOR_LAMP: return 19u;
        case DECOR_SHELF: return 16u;
        case DECOR_TANK: return 21u;
        case DECOR_DOCK: return 24u;
        default: return 0u;
    }
}

static uint8_t has_item(uint8_t item) {
    return (inventory & item) != 0u;
}

static void add_item(uint8_t item) {
    inventory |= item;
}

static uint8_t invert_dmg_palette_reg(uint8_t palette) {
    return (uint8_t)(
        (3u - (palette & 0x03u)) |
        ((3u - ((palette >> 2u) & 0x03u)) << 2u) |
        ((3u - ((palette >> 4u) & 0x03u)) << 4u) |
        ((3u - ((palette >> 6u) & 0x03u)) << 6u)
    );
}

static void set_inverted_bkg_palette(const palette_color_t *palettes) {
    palette_color_t inverted[4];

    inverted[0] = palettes[3];
    inverted[1] = palettes[2];
    inverted[2] = palettes[1];
    inverted[3] = palettes[0];
    set_bkg_palette(0u, 1u, inverted);
}

static void begin_display_reload(void) {
    vsync();
    DISPLAY_OFF;
    HIDE_WIN;
    HIDE_SPRITES;
    SHOW_BKG;
}

static void finish_display_reload(uint8_t show_sprites) {
    SHOW_BKG;
    if (show_sprites) {
        SHOW_SPRITES;
    } else {
        HIDE_SPRITES;
    }
    DISPLAY_ON;
}

static void apply_palette(void) {
    uint8_t bgp;
    uint8_t outdoor = (uint8_t)(room_id == ROOM_DISTRICT || room_id == ROOM_SOUTHLINE);

    if (palette_mood == 0u) {
        bgp = outdoor ? 0xD2u : 0xE4u;
    } else {
        bgp = outdoor ? 0xC9u : 0xD8u;
    }
    BGP_REG = outdoor ? invert_dmg_palette_reg(bgp) : bgp;
    OBP0_REG = 0xE4u;
    OBP1_REG = 0xE4u;
}

static uint8_t tile_to_sprite_x(uint8_t tile_x) {
    return (uint8_t)(tile_x * 8u + 8u);
}

static uint8_t tile_to_sprite_y(uint8_t tile_y) {
    return (uint8_t)(tile_y * 8u + 16u);
}

static void set_quad_sprite(uint8_t oam_base, uint8_t tile_base, uint8_t x, uint8_t y, uint8_t flip_x) {
    uint8_t top_left = tile_base;
    uint8_t top_right = (uint8_t)(tile_base + 1u);
    uint8_t bottom_left = (uint8_t)(tile_base + 2u);
    uint8_t bottom_right = (uint8_t)(tile_base + 3u);
    uint8_t prop = flip_x ? S_FLIPX : 0u;

    if (flip_x) {
        set_sprite_tile(oam_base + 0u, top_right);
        set_sprite_tile(oam_base + 1u, top_left);
        set_sprite_tile(oam_base + 2u, bottom_right);
        set_sprite_tile(oam_base + 3u, bottom_left);
    } else {
        set_sprite_tile(oam_base + 0u, top_left);
        set_sprite_tile(oam_base + 1u, top_right);
        set_sprite_tile(oam_base + 2u, bottom_left);
        set_sprite_tile(oam_base + 3u, bottom_right);
    }

    set_sprite_prop(oam_base + 0u, prop);
    set_sprite_prop(oam_base + 1u, prop);
    set_sprite_prop(oam_base + 2u, prop);
    set_sprite_prop(oam_base + 3u, prop);

    move_sprite(oam_base + 0u, x, y);
    move_sprite(oam_base + 1u, (uint8_t)(x + 8u), y);
    move_sprite(oam_base + 2u, x, (uint8_t)(y + 8u));
    move_sprite(oam_base + 3u, (uint8_t)(x + 8u), (uint8_t)(y + 8u));
}

static uint8_t player_frame_base(uint8_t walk_phase) {
    if (facing == DIR_DOWN) {
        return walk_phase ? PLAYER_FRAME_DOWN_WALK : PLAYER_FRAME_DOWN_STAND;
    }
    if (facing == DIR_UP) {
        return walk_phase ? PLAYER_FRAME_UP_WALK : PLAYER_FRAME_UP_STAND;
    }
    return walk_phase ? PLAYER_FRAME_SIDE_WALK : PLAYER_FRAME_SIDE_STAND;
}

static uint8_t current_room_tile_count(void) {
    switch (room_id) {
        case ROOM_DISTRICT: return luma_lane_district_v2_TILE_COUNT;
        case ROOM_APARTMENT: return (uint8_t)(luma_lane_apartment_TILE_COUNT + APARTMENT_DECOR_TILE_COUNT);
        case ROOM_RECORDS: return needle_and_neon_records_TILE_COUNT;
        case ROOM_TECH: return rook_repair_tech_TILE_COUNT;
        case ROOM_RAMEN: return soma_ramen_TILE_COUNT;
        case ROOM_ROOFTOP: return iona_rooftop_TILE_COUNT;
        case ROOM_SOUTHLINE: return southline_overpass_TILE_COUNT;
        default: return BG_TILE_COUNT;
    }
}

static uint8_t current_sprite_tile_base(void) {
    return current_room_tile_count();
}

static uint8_t current_ride_level(void) {
    if ((room_id == ROOM_DISTRICT) || (room_id == ROOM_SOUTHLINE)) {
        return ride_level;
    }
    return RIDE_FOOT;
}

static uint8_t movement_step_frames(void) {
    switch (current_ride_level()) {
        case RIDE_BICYCLE: return 5u;
        case RIDE_SCOOTER: return 4u;
        case RIDE_MOTORCYCLE: return 4u;
        default: return 6u;
    }
}

static uint8_t movement_repeat_delay(void) {
    switch (current_ride_level()) {
        case RIDE_BICYCLE: return 1u;
        case RIDE_SCOOTER: return 0u;
        case RIDE_MOTORCYCLE: return 0u;
        default: return 2u;
    }
}

static uint8_t movement_repeat_interval(void) {
    switch (current_ride_level()) {
        case RIDE_MOTORCYCLE: return 0u;
        default: return 0u;
    }
}

static uint8_t current_sprite_tile_count(void) {
    if (room_id == ROOM_APARTMENT) {
        return PLAYER_SPRITE_TILE_COUNT;
    }
    if ((room_id == ROOM_DISTRICT) || (room_id == ROOM_SOUTHLINE)) {
        return SPRITE_TILE_COUNT;
    }
    return SPRITE_TILE_COUNT;
}

static void hide_actor(uint8_t oam_base) {
    move_sprite(oam_base + 0u, 0u, 0u);
    move_sprite(oam_base + 1u, 0u, 0u);
    move_sprite(oam_base + 2u, 0u, 0u);
    move_sprite(oam_base + 3u, 0u, 0u);
}

static void set_single_sprite(uint8_t oam, uint8_t tile, uint8_t x, uint8_t y, uint8_t props) {
    set_sprite_tile(oam, tile);
    set_sprite_prop(oam, props);
    move_sprite(oam, x, y);
}

static uint8_t traffic_tile(uint8_t type, uint8_t vertical, uint8_t phase) {
    if (type == TRAFFIC_TYPE_BIKE) {
        return (uint8_t)(current_sprite_tile_base() + TRAFFIC_SPRITE_TILE_OFFSET + (vertical ? 2u : 0u) + phase);
    }
    return (uint8_t)(current_sprite_tile_base() + TRAFFIC_SPRITE_TILE_OFFSET + 4u + (vertical ? 2u : 0u) + phase);
}

static void draw_traffic_sprites(void) {
    uint8_t phase = (uint8_t)((frame_clock >> 3u) & 1u);
    uint8_t travel = (uint8_t)(frame_clock >> 1u);
    uint8_t oam0 = TRAFFIC_OAM_BASE;
    uint8_t oam1 = (uint8_t)(TRAFFIC_OAM_BASE + 1u);
    uint8_t x;
    uint8_t y;

    move_sprite(oam0, 0u, 0u);
    move_sprite(oam1, 0u, 0u);

    if (room_id == ROOM_DISTRICT) {
        /* The district only has road traffic along the lower horizontal road. */
        x = (uint8_t)(((uint16_t)travel + 20u) % 184u);
        y = 108u;
        if ((x >= 8u) && (x <= 168u)) {
            set_single_sprite(oam0, traffic_tile(TRAFFIC_TYPE_MOTOR, 0u, phase), x, y, 0u);
        }

        x = (uint8_t)(176u - (((uint16_t)travel + 76u) % 184u));
        y = 116u;
        if ((x >= 8u) && (x <= 168u)) {
            set_single_sprite(oam1, traffic_tile(TRAFFIC_TYPE_MOTOR, 0u, phase), x, y, S_FLIPX);
        }
        return;
    }

    if (room_id == ROOM_SOUTHLINE) {
        y = (uint8_t)(((uint16_t)travel + 28u) % 176u);
        x = 88u;
        if ((y >= 16u) && (y <= 152u)) {
            set_single_sprite(oam0, traffic_tile(TRAFFIC_TYPE_MOTOR, 1u, phase), x, y, 0u);
        }
    }
}

static void draw_world_sprites(uint8_t player_sx, uint8_t player_sy, uint8_t walk_phase) {
    uint8_t i;
    uint8_t bob;
    uint8_t npc_oam;
    uint8_t sprite_base = current_sprite_tile_base();

    set_quad_sprite(
        PLAYER_OAM_BASE,
        (uint8_t)(sprite_base + player_frame_base(walk_phase)),
        player_sx,
        player_sy,
        (facing == DIR_LEFT)
    );

    for (i = 0u; i != NPC_COUNT; ++i) {
        npc_oam = (uint8_t)(NPC_OAM_BASE + i * SPRITES_PER_ACTOR);
        if (npcs[i].room_id == room_id) {
            bob = (uint8_t)(((frame_clock >> 4u) + i) & 1u);
            set_quad_sprite(
                npc_oam,
                (uint8_t)(sprite_base + NPC_SPRITE_TILE_OFFSET + i * 4u),
                tile_to_sprite_x(npcs[i].x),
                (uint8_t)(tile_to_sprite_y(npcs[i].y) - bob),
                0u
            );
        } else {
            hide_actor(npc_oam);
        }
    }

    draw_traffic_sprites();
}

static void stamp_apartment_decor(void) {
    static const uint8_t slot_x[3] = { 5u, 10u, 15u };
    static const uint8_t slot_y[3] = { 5u, 5u, 5u };
    uint8_t i;

    for (i = 0u; i != 3u; ++i) {
        map_buffer[(uint16_t)slot_y[i] * MAP_W + slot_x[i]] = decor_tile(decor_slots[i]);
    }
}

static void load_room(void) {
    uint8_t x;
    uint8_t y;

    for (y = 0u; y != MAP_H; ++y) {
        for (x = 0u; x != MAP_W; ++x) {
            map_buffer[(uint16_t)y * MAP_W + x] = tile_for_char(room_maps[room_id][y][x]);
        }
    }

    if (room_id == ROOM_APARTMENT) {
        stamp_apartment_decor();
    }

    sanitize_player_position();
}

static uint8_t apartment_overlay_tile(uint8_t decor_id) {
    switch (decor_id) {
        case DECOR_POSTER: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 0u);
        case DECOR_LAMP: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 1u);
        case DECOR_SHELF: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 2u);
        case DECOR_TANK: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 3u);
        case DECOR_DOCK: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 4u);
        default: return 0u;
    }
}

static void overlay_apartment_decor(void) {
    static const uint8_t slot_x[3] = { 5u, 10u, 15u };
    static const uint8_t slot_y[3] = { 5u, 5u, 5u };
    uint8_t i;
    uint8_t tile;

    set_bkg_data(luma_lane_apartment_TILE_COUNT, APARTMENT_DECOR_TILE_COUNT, apartment_decor_tiles);
    for (i = 0u; i != 3u; ++i) {
        if (decor_slots[i] != DECOR_NONE) {
            tile = apartment_overlay_tile(decor_slots[i]);
            set_bkg_tiles(slot_x[i], slot_y[i], 1u, 1u, &tile);
        }
    }
}

static const palette_color_t *current_room_palettes(void) {
    switch (room_id) {
        case ROOM_DISTRICT: return luma_lane_district_v2_palettes;
        case ROOM_APARTMENT: return luma_lane_apartment_palettes;
        case ROOM_RECORDS: return needle_and_neon_records_palettes;
        case ROOM_TECH: return rook_repair_tech_palettes;
        case ROOM_RAMEN: return soma_ramen_palettes;
        case ROOM_ROOFTOP: return iona_rooftop_palettes;
        case ROOM_SOUTHLINE: return southline_overpass_palettes;
        default: return 0;
    }
}

static void render_banked_room(void) {
    switch (room_id) {
        case ROOM_DISTRICT:
            show_banked_image_card(
                luma_lane_district_v2_tiles,
                luma_lane_district_v2_TILE_COUNT,
                luma_lane_district_v2_map,
                luma_lane_district_v2_palettes,
                BANK(luma_lane_district_v2)
            );
            break;
        case ROOM_APARTMENT:
            show_banked_image_card(
                luma_lane_apartment_tiles,
                luma_lane_apartment_TILE_COUNT,
                luma_lane_apartment_map,
                luma_lane_apartment_palettes,
                BANK(luma_lane_apartment)
            );
            overlay_apartment_decor();
            break;
        case ROOM_RECORDS:
            show_banked_image_card(
                needle_and_neon_records_tiles,
                needle_and_neon_records_TILE_COUNT,
                needle_and_neon_records_map,
                needle_and_neon_records_palettes,
                BANK(needle_and_neon_records)
            );
            break;
        case ROOM_TECH:
            show_banked_image_card(
                rook_repair_tech_tiles,
                rook_repair_tech_TILE_COUNT,
                rook_repair_tech_map,
                rook_repair_tech_palettes,
                BANK(rook_repair_tech)
            );
            break;
        case ROOM_RAMEN:
            show_banked_image_card(
                soma_ramen_tiles,
                soma_ramen_TILE_COUNT,
                soma_ramen_map,
                soma_ramen_palettes,
                BANK(soma_ramen)
            );
            break;
        case ROOM_ROOFTOP:
            show_banked_image_card(
                iona_rooftop_tiles,
                iona_rooftop_TILE_COUNT,
                iona_rooftop_map,
                iona_rooftop_palettes,
                BANK(iona_rooftop)
            );
            break;
        case ROOM_SOUTHLINE:
            show_banked_image_card(
                southline_overpass_tiles,
                southline_overpass_TILE_COUNT,
                southline_overpass_map,
                southline_overpass_palettes,
                BANK(southline_overpass)
            );
            break;
        default:
            set_bkg_data(0u, BG_TILE_COUNT, bg_tiles);
            set_bkg_tiles(0u, 0u, MAP_W, MAP_H, map_buffer);
            break;
    }
}

static void render_room(void) {
    const palette_color_t *palettes;

    begin_display_reload();
    render_banked_room();
    if (_cpu == CGB_TYPE) {
        palettes = current_room_palettes();
        if (palettes != 0) {
            if (room_id == ROOM_DISTRICT || room_id == ROOM_SOUTHLINE) {
                set_inverted_bkg_palette(palettes);
            } else {
                set_bkg_palette(0u, 1u, palettes);
            }
        }
    }
    set_sprite_data(current_sprite_tile_base(), current_sprite_tile_count(), sprite_tiles);
    apply_palette();
    draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
    dt_audio_sync(room_id);
    finish_display_reload(1u);
}

static void use_text_screen(void) {
    begin_display_reload();
    font_init();
    ui_font = font_load(font_ibm);
    apply_palette();
    font_set(ui_font);
    cls();
    gotoxy(0u, 0u);
    finish_display_reload(0u);
}

static void draw_text(const char *text) {
    uint8_t x = posx();
    uint8_t y = posy();
    char c;

    while ((c = *text++) != '\0') {
        if (c == '\n') {
            x = 0u;
            ++y;
        } else {
            gotoxy(x, y);
            setchar(c);
            ++x;
            if (x >= MAP_W) {
                x = 0u;
                ++y;
            }
        }
    }
    gotoxy(x, y);
}

static void draw_text_xy(uint8_t x, uint8_t y, const char *text) {
    gotoxy(x, y);
    draw_text(text);
}

static void draw_u8(uint8_t value) {
    uint8_t started = 0u;
    uint8_t digit;

    if (value >= 100u) {
        digit = value / 100u;
        setchar((char)('0' + digit));
        gotoxy((uint8_t)(posx() + 1u), posy());
        value %= 100u;
        started = 1u;
    }
    if (started || value >= 10u) {
        digit = value / 10u;
        setchar((char)('0' + digit));
        gotoxy((uint8_t)(posx() + 1u), posy());
        value %= 10u;
    }
    setchar((char)('0' + value));
    gotoxy((uint8_t)(posx() + 1u), posy());
}

static void show_banked_image_card(const uint8_t *tiles, uint8_t tile_count, const unsigned char *map, const palette_color_t *palettes, uint8_t bank) {
    uint8_t old_bank = CURRENT_BANK;

    if (bank != old_bank) {
        SWITCH_ROM(bank);
    }
    set_bkg_data(0u, tile_count, tiles);
    set_bkg_tiles(0u, 0u, MAP_W, MAP_H, map);
    if (_cpu == CGB_TYPE) {
        set_bkg_palette(0u, 1u, palettes);
    } else {
        BGP_REG = 0xE4u;
    }
    if (bank != old_bank) {
        SWITCH_ROM(old_bank);
    }
}

static uint8_t wait_card_input(uint8_t accept_start_only) {
    uint8_t keys = joypad();

    if (accept_start_only) {
        return (keys & J_START) != 0u;
    }
    return ((keys & J_START) != 0u) || ((keys & J_A) != 0u) || ((keys & J_B) != 0u);
}

static void show_company_card(void) {
    uint8_t frame;

    begin_display_reload();
    show_banked_image_card(
        startup_scott_stinks_a_tiles,
        startup_scott_stinks_a_TILE_COUNT,
        startup_scott_stinks_a_map,
        startup_scott_stinks_a_palettes,
        BANK(startup_scott_stinks_a)
    );
    finish_display_reload(0u);
    dt_audio_play_sfx(SFX_FART);

    for (frame = 0u; frame != 84u; ++frame) {
        if (_cpu != CGB_TYPE) {
            BGP_REG = ((frame & 0x08u) == 0u) ? 0xE4u : 0xD2u;
        } else {
            set_bkg_palette(0u, 1u, startup_scott_stinks_a_palettes);
        }
        vsync();
        dt_audio_update();
        ++frame_clock;
    }
}

static void show_bimpson_card(void) {
    uint8_t frame;

    begin_display_reload();
    show_banked_image_card(
        startup_bimpson_productions_a_tiles,
        startup_bimpson_productions_a_TILE_COUNT,
        startup_bimpson_productions_a_map,
        startup_bimpson_productions_a_palettes,
        BANK(startup_bimpson_productions_a)
    );
    finish_display_reload(0u);

    for (frame = 0u; frame != 72u; ++frame) {
        if (_cpu != CGB_TYPE) {
            BGP_REG = ((frame & 0x10u) == 0u) ? 0xE4u : 0xD8u;
        } else {
            set_bkg_palette(0u, 1u, startup_bimpson_productions_a_palettes);
        }
        vsync();
        dt_audio_update();
        ++frame_clock;
    }
}

static void print_footer(const char *footer) {
    draw_text_xy(0u, 16u, "--------------------");
    draw_text_xy(0u, 17u, "                    ");
    draw_text_xy(0u, 17u, footer);
}

void show_message(const char *title, const char *body, const char *footer) {
    uint8_t keys;
    uint8_t last = 0u;

    use_text_screen();
    draw_text(title);
    draw_text("\n\n");
    draw_text(body);
    print_footer(footer);

    while (1) {
        vsync();
        dt_audio_update();
        keys = joypad();
        if (((keys & J_A) && !(last & J_A)) ||
            ((keys & J_B) && !(last & J_B)) ||
            ((keys & J_START) && !(last & J_START))) {
            waitpadup();
            break;
        }
        last = keys;
    }

    load_room();
    render_room();
}

uint8_t menu_screen(const char *title, const char *subtitle, const char *const *items, uint8_t count, uint8_t selected) {
    uint8_t i;
    uint8_t keys;
    uint8_t last = 0u;

    while (1) {
        use_text_screen();
        draw_text(title);
        draw_text("\n");
        draw_text(subtitle);
        draw_text("\n\n");
        for (i = 0u; i != count; ++i) {
            setchar((i == selected) ? '>' : ' ');
            gotoxy(1u, (uint8_t)(posy()));
            draw_text(items[i]);
            draw_text("\n");
        }
        print_footer("WHEEL MOVE  SELECT");

        while (1) {
            vsync();
            dt_audio_update();
            keys = joypad();
            if ((keys & J_UP) && !(last & J_UP)) {
                dt_audio_play_sfx(SFX_MENU_MOVE);
                selected = (selected == 0u) ? (count - 1u) : (selected - 1u);
                break;
            }
            if ((keys & J_DOWN) && !(last & J_DOWN)) {
                dt_audio_play_sfx(SFX_MENU_MOVE);
                selected = (selected + 1u) % count;
                break;
            }
            if ((keys & J_A) && !(last & J_A)) {
                dt_audio_play_sfx(SFX_CONFIRM);
                waitpadup();
                return selected;
            }
            if ((keys & J_START) && !(last & J_START)) {
                dt_audio_play_sfx(SFX_CANCEL);
                waitpadup();
                return count - 1u;
            }
            last = keys;
        }
        waitpadup();
        last = 0u;
    }
}

static const task_t *current_task(void) {
    return &tasks[task_id];
}

static void assign_daily_task(void) {
    task_id = (uint8_t)((day_count - 1u) % TASK_COUNT);
    task_done = 0u;
}

static void setup_new_game(void) {
    room_id = ROOM_DISTRICT;
    player_x = 10u;
    player_y = 12u;
    facing = DIR_DOWN;
    credits = 18u;
    day_count = 1u;
    inventory = 0u;
    media = 0u;
    decor_slots[0] = DECOR_NONE;
    decor_slots[1] = DECOR_NONE;
    decor_slots[2] = DECOR_NONE;
    ride_level = RIDE_FOOT;
    palette_mood = 0u;
    assign_daily_task();
}

static void print_task_summary(void) {
    const task_t *task = current_task();

    draw_text("Day ");
    draw_u8(day_count);
    draw_text("  Cr ");
    draw_u8(credits);
    draw_text("  ");
    draw_text(ride_name(ride_level));
    draw_text("\n");
    draw_text(task->title);
    draw_text("\n");
    if (task_done) {
        draw_text("Done for tonight.\n");
        draw_text("Sleep in your room when\nyou want the next errand.");
    } else {
        draw_text(task->brief);
    }
}

static const char *decor_name(uint8_t decor_id) {
    switch (decor_id) {
        case DECOR_POSTER: return "Poster";
        case DECOR_LAMP: return "Lamp";
        case DECOR_SHELF: return "Shelf";
        case DECOR_TANK: return "Tank";
        case DECOR_DOCK: return "Dock";
        default: return "Empty";
    }
}

static const char *ride_name(uint8_t level) {
    switch (level) {
        case RIDE_BICYCLE: return "Bike";
        case RIDE_SCOOTER: return "Scoot";
        case RIDE_MOTORCYCLE: return "Moto";
        default: return "Foot";
    }
}

static void inventory_screen(uint8_t page) {
    uint8_t keys;
    uint8_t last = 0u;

    while (1) {
        use_text_screen();
        if (page == 0u) {
            draw_text("PACK\n\n");
            print_task_summary();
            draw_text("\nBag:\n");
            if (inventory == 0u) {
                draw_text("  nothing yet");
            } else {
                if (has_item(ITEM_SOFTPHONES)) draw_text("  softphones\n");
                if (has_item(ITEM_BROTH)) draw_text("  night broth\n");
                if (has_item(ITEM_MINIDISC)) draw_text("  mini-disc\n");
                if (has_item(ITEM_POSTER)) draw_text("  glow poster\n");
                if (has_item(ITEM_LAMP)) draw_text("  neon lamp\n");
                if (has_item(ITEM_SHELF)) draw_text("  crate shelf\n");
                if (has_item(ITEM_TANK)) draw_text("  fish tank\n");
                if (has_item(ITEM_DOCK)) draw_text("  pocket dock\n");
            }
        } else if (page == 1u) {
            draw_text("TUNES\n\nCollected:\n");
            if (media == 0u) {
                draw_text("  no tapes yet\n\n");
                draw_text("Finish errands and keep\nan ear out for bootlegs.");
            } else {
                if (media & MEDIA_RAIN_LOOP) draw_text("  Rain Loop\n");
                if (media & MEDIA_VENDING_DREAMS) draw_text("  Vending Dreams\n");
                if (media & MEDIA_ROOFTOP_SET) draw_text("  Rooftop Set\n");
                draw_text("\nDeck: ");
                draw_text(dt_record_name(dt_audio_get_home_record()));
                draw_text("\nUse your home deck\nto swap records.");
            }
        } else {
            draw_text("ROOM\n\n");
            draw_text("Slot 1: ");
            draw_text(decor_name(decor_slots[0]));
            draw_text("\n");
            draw_text("Slot 2: ");
            draw_text(decor_name(decor_slots[1]));
            draw_text("\n");
            draw_text("Slot 3: ");
            draw_text(decor_name(decor_slots[2]));
            draw_text("\n\nUse your apartment shelf\npads to place decor.");
        }

        print_footer("LEFT/RIGHT TAB  MENU");
        while (1) {
            vsync();
            dt_audio_update();
            keys = joypad();
            if ((keys & J_B) && !(last & J_B)) {
                dt_audio_play_sfx(SFX_MENU_MOVE);
                page = (page == 0u) ? 2u : (page - 1u);
                break;
            }
            if ((keys & J_SELECT) && !(last & J_SELECT)) {
                dt_audio_play_sfx(SFX_MENU_MOVE);
                page = (page + 1u) % 3u;
                break;
            }
            if (((keys & J_START) && !(last & J_START)) || ((keys & J_A) && !(last & J_A))) {
                dt_audio_play_sfx(SFX_CANCEL);
                waitpadup();
                return;
            }
            last = keys;
        }
        waitpadup();
        last = 0u;
    }
}

static void help_screen(void) {
    show_message(
        "HELP",
        "WHEEL moves.\nSELECT talks.\nLEFT pack.\nRIGHT tunes.\nSouth road leads to wheels.",
        "SELECT CLOSE"
    );
}

static void options_screen(void) {
    static const char *const items[] = {
        "Soft night tint",
        "Sharp contrast",
        "Back"
    };
    uint8_t choice;

    choice = menu_screen("OPTIONS", "Pick a screen mood", items, 3u, palette_mood);
    if (choice < 2u) {
        palette_mood = choice;
    }
}

static void decor_screen(void) {
    static const char *const slot_items[] = {
        "Slot 1",
        "Slot 2",
        "Slot 3",
        "Back"
    };
    static const char *const decor_items[] = {
        "Clear",
        "Poster",
        "Lamp",
        "Shelf",
        "Tank",
        "Dock",
        "Back"
    };
    uint8_t slot;
    uint8_t choice;
    uint8_t needed_item;
    uint8_t i;

    slot = menu_screen("DECOR", "Pick a room slot", slot_items, 4u, 0u);
    if (slot == 3u) {
        return;
    }

    choice = menu_screen("DECOR", "Pick an item", decor_items, 7u, 0u);
    if (choice == 6u) {
        return;
    }

    if (choice == 0u) {
        decor_slots[slot] = DECOR_NONE;
        load_room();
        return;
    }

    needed_item = 0u;
    if (choice == DECOR_POSTER) needed_item = ITEM_POSTER;
    else if (choice == DECOR_LAMP) needed_item = ITEM_LAMP;
    else if (choice == DECOR_SHELF) needed_item = ITEM_SHELF;
    else if (choice == DECOR_TANK) needed_item = ITEM_TANK;
    else if (choice == DECOR_DOCK) needed_item = ITEM_DOCK;

    if (!has_item(needed_item)) {
        show_message("ROOM", "You do not own that piece yet.", "SELECT CLOSE");
        return;
    }

    for (i = 0u; i != 3u; ++i) {
        if (decor_slots[i] == choice) {
            decor_slots[i] = DECOR_NONE;
        }
    }
    decor_slots[slot] = choice;
    load_room();
}

static void pause_menu(void) {
    static const char *const items[] = {
        "Resume",
        "Pack",
        "Tunes",
        "Help",
        "Options"
    };
    uint8_t choice;

    choice = menu_screen("PAUSE", "Block breather", items, 5u, 0u);
    if (choice == 1u) inventory_screen(0u);
    else if (choice == 2u) inventory_screen(1u);
    else if (choice == 3u) help_screen();
    else if (choice == 4u) options_screen();
}

static void reward_task(const task_t *task) {
    task_done = 1u;
    inventory &= (uint8_t)~task->required_item;
    credits += task->reward_credits;
    media |= task->reward_media;
    if (dt_audio_get_home_record() == RECORD_NONE) {
        dt_audio_set_home_record(dt_record_from_media(task->reward_media));
    }
    dt_audio_play_sfx(SFX_REWARD);
}

static void give_item_message(const char *title, const char *body, uint8_t item) {
    add_item(item);
    show_message(title, body, "SELECT CLOSE");
}

static void buy_item(uint8_t item, uint8_t price, const char *title, const char *body) {
    if (has_item(item)) {
        show_message(title, "You already own one.", "SELECT CLOSE");
        return;
    }
    if (credits < price) {
        show_message(title, "Not enough credits tonight.", "SELECT CLOSE");
        return;
    }
    credits -= price;
    add_item(item);
    show_message(title, body, "SELECT CLOSE");
}

static void buy_ride(uint8_t level, uint8_t price, const char *title, const char *body) {
    if (ride_level >= level) {
        show_message(title, "You already own that ride tier.", "SELECT CLOSE");
        return;
    }
    if (credits < price) {
        show_message(title, "Not enough credits tonight.", "SELECT CLOSE");
        return;
    }
    credits -= price;
    ride_level = level;
    show_message(title, body, "SELECT CLOSE");
}

static void vehicle_shop(void) {
    static const char *const bike_items[] = {
        "Bicycle   18c",
        "Leave"
    };
    static const char *const scooter_items[] = {
        "Scooter   34c",
        "Leave"
    };
    static const char *const moto_items[] = {
        "Motorcycle 58c",
        "Leave"
    };
    uint8_t choice;

    if (ride_level == RIDE_FOOT) {
        choice = menu_screen("WHEELS", "Used city bicycle", bike_items, 2u, 0u);
        if (choice == 0u) {
            buy_ride(RIDE_BICYCLE, 18u, "GARAGE", "A tuned bicycle. The block feels smaller already.");
        }
        return;
    }

    if (ride_level == RIDE_BICYCLE) {
        if (media == 0u) {
            show_message("GARAGE", "Bring back at least one tape.\nThen they'll trust you with a scooter.", "SELECT CLOSE");
            return;
        }
        choice = menu_screen("WHEELS", "Courier scooter", scooter_items, 2u, 0u);
        if (choice == 0u) {
            buy_ride(RIDE_SCOOTER, 34u, "GARAGE", "A soft electric scooter. Quicker starts, smoother glide.");
        }
        return;
    }

    if (ride_level == RIDE_SCOOTER) {
        if ((media & (MEDIA_RAIN_LOOP | MEDIA_VENDING_DREAMS | MEDIA_ROOFTOP_SET)) !=
            (MEDIA_RAIN_LOOP | MEDIA_VENDING_DREAMS | MEDIA_ROOFTOP_SET)) {
            show_message("GARAGE", "Earn a little name on the block.\nThen the motorcycle comes out.", "SELECT CLOSE");
            return;
        }
        choice = menu_screen("WHEELS", "Street motorcycle", moto_items, 2u, 0u);
        if (choice == 0u) {
            buy_ride(RIDE_MOTORCYCLE, 58u, "GARAGE", "A tiny motorcycle with a patient idle and a fast lane.");
        }
        return;
    }

    show_message("GARAGE", "Your motorcycle is already the best thing in the lot.", "SELECT CLOSE");
}

static void mina_shop(void) {
    static const char *const items[] = {
        "Glow Poster  6c",
        "Neon Lamp   9c",
        "Crate Shelf 11c",
        "Mini-Disc   7c",
        "Leave"
    };
    uint8_t choice;

    choice = menu_screen("NEEDLE & NEON", "Room pieces", items, 5u, 0u);
    if (choice == 0u) buy_item(ITEM_POSTER, 6u, "MINA", "A folded glow poster for your wall.");
    else if (choice == 1u) buy_item(ITEM_LAMP, 9u, "MINA", "A lamp with a soft rainy bloom.");
    else if (choice == 2u) buy_item(ITEM_SHELF, 11u, "MINA", "Crate shelf, stickered and sturdy.");
    else if (choice == 3u) buy_item(ITEM_MINIDISC, 7u, "MINA", "One blank mini-disc in a foggy sleeve.");
}

static void rook_shop(void) {
    static const char *const items[] = {
        "Softphones  8c",
        "Pocket Dock 14c",
        "Tune Wheels",
        "Leave"
    };
    uint8_t choice;

    choice = menu_screen("ROOK REPAIR", "Bench specials", items, 4u, 0u);
    if (choice == 0u) buy_item(ITEM_SOFTPHONES, 8u, "ROOK", "Fresh softphones. Quiet cups, no hiss.");
    else if (choice == 1u) buy_item(ITEM_DOCK, 14u, "ROOK", "A tiny dock with patient LEDs.");
    else if (choice == 2u) vehicle_shop();
}

static void soma_shop(void) {
    static const char *const items[] = {
        "Night Broth 4c",
        "Fish Tank  12c",
        "Leave"
    };
    uint8_t choice;

    choice = menu_screen("SOMA", "Steam counter", items, 3u, 0u);
    if (choice == 0u) buy_item(ITEM_BROTH, 4u, "SOMA", "One sealed broth cup for the walk home.");
    else if (choice == 1u) buy_item(ITEM_TANK, 12u, "SOMA", "A tiny glowing tank with sleepy bubbles.");
}

static void npc_dialogue(uint8_t npc_id) {
    const task_t *task = current_task();

    if (npc_id == NPC_IONA) {
        if ((task_id == TASK_MUSIC_DROP) && !task_done) {
            if (has_item(ITEM_SOFTPHONES)) {
                reward_task(task);
                show_message("IONA", task->complete, "SELECT CLOSE");
            } else {
                show_message("IONA", "My booth crackles tonight.\nRook still has softphones.", "SELECT CLOSE");
            }
            return;
        }
        show_message("IONA", "When the block gets lonely,\nI leave synth-pop in the air.", "SELECT CLOSE");
        return;
    }

    if (npc_id == NPC_MINA) {
        if ((task_id == TASK_SOUP_RUN) && !task_done) {
            if (has_item(ITEM_BROTH)) {
                reward_task(task);
                show_message("MINA", task->complete, "SELECT CLOSE");
            } else {
                show_message("MINA", "I sorted imports all night.\nSoma still owes me dinner.", "SELECT CLOSE");
            }
            return;
        }
        mina_shop();
        return;
    }

    if (npc_id == NPC_ROOK) {
        if ((task_id == TASK_DISC_SWAP) && !task_done) {
            if (has_item(ITEM_MINIDISC)) {
                reward_task(task);
                show_message("ROOK", task->complete, "SELECT CLOSE");
            } else {
                show_message("ROOK", "Needle & Neon keeps blank mini-discs.\nI just need one tonight.", "SELECT CLOSE");
            }
            return;
        }
        rook_shop();
        return;
    }

    if (npc_id == NPC_SOMA) {
        soma_shop();
        return;
    }

    if (!has_item(ITEM_POSTER)) {
        give_item_message("PIX", "Welcome to Luma Lane.\nTake this glow poster for your wall.", ITEM_POSTER);
    } else {
        show_message("PIX", "Tiny room, good rain, records nearby.\nYou're settling in fine.", "SELECT CLOSE");
    }
}

static uint8_t npc_at(uint8_t x, uint8_t y, uint8_t *npc_id) {
    uint8_t i;

    for (i = 0u; i != NPC_COUNT; ++i) {
        if ((npcs[i].room_id == room_id) && (npcs[i].x == x) && (npcs[i].y == y)) {
            *npc_id = i;
            return 1u;
        }
    }
    return 0u;
}

static uint8_t npc_blocks(uint8_t x, uint8_t y) {
    uint8_t i;

    for (i = 0u; i != NPC_COUNT; ++i) {
        if ((npcs[i].room_id == room_id) && (npcs[i].x == x) && (npcs[i].y == y)) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t hotspot_at(uint8_t x, uint8_t y, uint8_t *hotspot_id) {
    uint8_t i;

    for (i = 0u; i != (sizeof(hotspots) / sizeof(hotspots[0])); ++i) {
        if ((hotspots[i].room_id == room_id) && (hotspots[i].x == x) && (hotspots[i].y == y)) {
            *hotspot_id = hotspots[i].id;
            return 1u;
        }
    }
    return 0u;
}

static void room_fallback_position(uint8_t *x, uint8_t *y) {
    switch (room_id) {
        case ROOM_APARTMENT:
            *x = 16u;
            *y = 14u;
            break;
        case ROOM_RECORDS:
            *x = 9u;
            *y = 14u;
            break;
        case ROOM_TECH:
            *x = 9u;
            *y = 14u;
            break;
        case ROOM_RAMEN:
            *x = 9u;
            *y = 14u;
            break;
        case ROOM_ROOFTOP:
            *x = 10u;
            *y = 14u;
            break;
        case ROOM_SOUTHLINE:
            *x = 9u;
            *y = 2u;
            break;
        case ROOM_DISTRICT:
        default:
            *x = 10u;
            *y = 12u;
            break;
    }
}

static uint8_t position_is_walkable(uint8_t x, uint8_t y) {
    uint8_t tile;

    if ((x >= MAP_W) || (y >= MAP_H)) {
        return 0u;
    }

    tile = map_buffer[(uint16_t)y * MAP_W + x];
    return (uint8_t)(!is_solid(tile) && !npc_blocks(x, y));
}

static void sanitize_player_position(void) {
    uint8_t x;
    uint8_t y;
    uint8_t fallback_x;
    uint8_t fallback_y;

    if (position_is_walkable(player_x, player_y)) {
        return;
    }

    room_fallback_position(&fallback_x, &fallback_y);
    if (position_is_walkable(fallback_x, fallback_y)) {
        player_x = fallback_x;
        player_y = fallback_y;
        return;
    }

    for (y = 1u; y != (MAP_H - 1u); ++y) {
        for (x = 1u; x != (MAP_W - 1u); ++x) {
            if (position_is_walkable(x, y)) {
                player_x = x;
                player_y = y;
                return;
            }
        }
    }

    player_x = 1u;
    player_y = 1u;
}

static uint8_t is_solid(uint8_t tile) {
    return (tile == 3u) || (tile == 4u) || (tile == 6u) || (tile == 8u) ||
           (tile == 9u) || (tile == 10u) || (tile == 11u) || (tile == 12u) ||
           (tile == 13u) || (tile == 14u) || (tile == 15u) || (tile == 16u) ||
           (tile == 17u) || (tile == 18u) || (tile == 19u) || (tile == 20u) ||
           (tile == 21u) || (tile == 22u) || (tile == 23u) || (tile == 24u);
}

static void enter_room(uint8_t new_room, uint8_t new_x, uint8_t new_y) {
    room_id = new_room;
    player_x = new_x;
    player_y = new_y;
    dt_audio_play_sfx(SFX_DOOR);
    load_room();
    render_room();
}

static void check_door(void) {
    uint8_t i;

    for (i = 0u; i != (sizeof(doors) / sizeof(doors[0])); ++i) {
        if ((doors[i].room_id == room_id) &&
            (doors[i].x == player_x) &&
            (doors[i].y == player_y)) {
            enter_room(doors[i].target_room, doors[i].target_x, doors[i].target_y);
            return;
        }
    }
}

static void animate_step(int8_t dx, int8_t dy) {
    uint8_t step;
    uint8_t frames = movement_step_frames();
    uint8_t start_x = tile_to_sprite_x(player_x);
    uint8_t start_y = tile_to_sprite_y(player_y);
    uint8_t walk_phase;
    uint8_t pixel;

    for (step = 1u; step <= frames; ++step) {
        walk_phase = (uint8_t)(step & 1u);
        pixel = (uint8_t)(((uint16_t)step * 8u) / frames);
        draw_world_sprites(
            (uint8_t)(start_x + dx * pixel),
            (uint8_t)(start_y + dy * pixel),
            walk_phase
        );
        vsync();
        dt_audio_update();
        ++frame_clock;
    }
}

static void try_move(int8_t dx, int8_t dy, uint8_t new_facing) {
    int8_t nx;
    int8_t ny;
    uint8_t tile;

    facing = new_facing;
    nx = (int8_t)player_x + dx;
    ny = (int8_t)player_y + dy;
    if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) {
        draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
        return;
    }

    tile = map_buffer[(uint16_t)ny * MAP_W + (uint8_t)nx];
    if (is_solid(tile) || npc_blocks((uint8_t)nx, (uint8_t)ny)) {
        draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
        return;
    }

    animate_step(dx, dy);
    player_x = (uint8_t)nx;
    player_y = (uint8_t)ny;
    check_door();
    draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
}

static void interact_hotspot(uint8_t hotspot_id) {
    if (hotspot_id == HOTSPOT_BED) {
        if (task_done) {
            ++day_count;
            assign_daily_task();
            show_message("ROOM", "You sleep under CRT glow.\nA new favor waits tomorrow.", "SELECT CLOSE");
        } else {
            show_message("ROOM", "The bed looks perfect, but the block still needs one thing tonight.", "SELECT CLOSE");
        }
        return;
    }

    if (hotspot_id == HOTSPOT_DECOR) {
        decor_screen();
        load_room();
        render_room();
        return;
    }

    if (hotspot_id == HOTSPOT_GARAGE) {
        vehicle_shop();
        load_room();
        render_room();
        return;
    }

    dt_home_record_menu(media, room_id);
    load_room();
    render_room();
}

static void try_interact(void) {
    int8_t tx = player_x;
    int8_t ty = player_y;
    uint8_t npc_id;
    uint8_t hotspot_id;

    if (facing == DIR_UP) ty -= 1;
    else if (facing == DIR_DOWN) ty += 1;
    else if (facing == DIR_LEFT) tx -= 1;
    else tx += 1;

    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) {
        return;
    }

    if (npc_at((uint8_t)tx, (uint8_t)ty, &npc_id)) {
        npc_dialogue(npc_id);
        return;
    }

    if (hotspot_at((uint8_t)tx, (uint8_t)ty, &hotspot_id)) {
        interact_hotspot(hotspot_id);
        return;
    }

    if ((room_id == ROOM_DISTRICT) && (tx == 13) && (ty == 15)) {
        show_message("TERMINAL", "Delays, karaoke specials,\nand a pirate radio station.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_DISTRICT) && (tx == 8) && (ty == 13)) {
        show_message("STALL", "A cart sells synth carts,\nbatteries, and stickers.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_APARTMENT) && (tx == 13) && (ty == 9)) {
        show_message("DESK", "Your dock blinks by the CRT.\nTiny room. Good rain.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_APARTMENT) && (tx == 13) && (ty == 3)) {
        show_message("FISH TANK", "Blue light makes the room feel larger.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_RECORDS) && (tx == 3) && (ty == 7)) {
        show_message("LISTENING POST", "Someone left a hand-labeled tape:\nVENDING DREAMS / A", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_TECH) && (tx == 3) && (ty == 7)) {
        show_message("BENCH", "Half-restored handhelds glow here.\nEach one hums a new startup chime.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_RAMEN) && (tx == 7) && (ty == 8)) {
        show_message("STOOL", "A warm seat, steamed glass,\nand just enough quiet.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_ROOFTOP) && (tx == 9) && (ty == 6)) {
        show_message("BOOTH", "Rain beads on the mixer.\nThe whole block softens up here.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_SOUTHLINE) && (tx == 15) && (ty == 6)) {
        show_message("SHELTER", "Couriers wait here between runs.\nThe whole lane hums a little faster.", "SELECT CLOSE");
        return;
    }

    if ((room_id == ROOM_SOUTHLINE) && (tx == 9) && (ty == 15)) {
        show_message("UNDERPASS", "The road keeps going south.\nMore of the city can open later.", "SELECT CLOSE");
    }
}

static void show_title(void) {
    uint8_t blink;
    uint8_t last_blink = 0xFFu;

    waitpadup();
    dt_audio_play_title_theme();
    while (1) {
        blink = (uint8_t)((frame_clock >> 4u) & 1u);
        if (blink != last_blink) {
            begin_display_reload();
            if (blink) {
                show_banked_image_card(
                    startup_dialtone_title_b_tiles,
                    startup_dialtone_title_b_TILE_COUNT,
                    startup_dialtone_title_b_map,
                    startup_dialtone_title_b_palettes,
                    BANK(startup_dialtone_title_b)
                );
            } else {
                show_banked_image_card(
                    startup_dialtone_title_a_tiles,
                    startup_dialtone_title_a_TILE_COUNT,
                    startup_dialtone_title_a_map,
                    startup_dialtone_title_a_palettes,
                    BANK(startup_dialtone_title_a)
                );
            }
            finish_display_reload(0u);
            last_blink = blink;
        }

        vsync();
        dt_audio_update();
        ++frame_clock;
        if (wait_card_input(1u)) {
            waitpadup();
            dt_audio_stop_music();
            return;
        }
    }
}

void main(void) {
    uint8_t keys;
    uint8_t last = 0u;
    uint8_t move_dir = 0u;
    uint8_t move_repeat = 0u;

    dt_audio_init();
    dt_init_assets();
    SPRITES_8x8;

    setup_new_game();
    show_company_card();
    show_bimpson_card();
    show_title();
    show_message(
        "ARRIVAL",
        "You moved into a tiny room in Luma Lane.\nMeet the block, run one favor,\nbring something cozy home.",
        "SELECT WALK"
    );
    load_room();
    render_room();

    while (1) {
        vsync();
        dt_audio_update();
        keys = joypad();

        if ((keys & J_A) && !(last & J_A)) try_interact();
        else if ((keys & J_B) && !(last & J_B)) {
            inventory_screen(0u);
            load_room();
            render_room();
        } else if ((keys & J_SELECT) && !(last & J_SELECT)) {
            inventory_screen(1u);
            load_room();
            render_room();
        } else if ((keys & J_START) && !(last & J_START)) {
            pause_menu();
            load_room();
            render_room();
        } else {
            uint8_t pressed = keys & (uint8_t)~last;
            uint8_t active_dir = 0u;

            if (pressed & J_UP) active_dir = J_UP;
            else if (pressed & J_DOWN) active_dir = J_DOWN;
            else if (pressed & J_LEFT) active_dir = J_LEFT;
            else if (pressed & J_RIGHT) active_dir = J_RIGHT;
            else if (keys & J_UP) active_dir = J_UP;
            else if (keys & J_DOWN) active_dir = J_DOWN;
            else if (keys & J_LEFT) active_dir = J_LEFT;
            else if (keys & J_RIGHT) active_dir = J_RIGHT;

            if (active_dir == 0u) {
                move_dir = 0u;
                move_repeat = 0u;
            } else if ((active_dir != move_dir) || (pressed & active_dir)) {
                move_dir = active_dir;
                move_repeat = movement_repeat_delay();
                if (active_dir == J_UP) try_move(0, -1, DIR_UP);
                else if (active_dir == J_DOWN) try_move(0, 1, DIR_DOWN);
                else if (active_dir == J_LEFT) try_move(-1, 0, DIR_LEFT);
                else try_move(1, 0, DIR_RIGHT);
            } else if (move_repeat != 0u) {
                --move_repeat;
            } else {
                move_repeat = movement_repeat_interval();
                if (active_dir == J_UP) try_move(0, -1, DIR_UP);
                else if (active_dir == J_DOWN) try_move(0, 1, DIR_DOWN);
                else if (active_dir == J_LEFT) try_move(-1, 0, DIR_LEFT);
                else try_move(1, 0, DIR_RIGHT);
            }
        }

        ++frame_clock;
        draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
        last = keys;
    }
}
