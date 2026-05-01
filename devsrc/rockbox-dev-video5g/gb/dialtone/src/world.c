#include "dialtone.h"
#include <string.h>

enum {
    HOTSPOT_BED = 1,
    HOTSPOT_DECOR,
    HOTSPOT_MEDIA
};

static const char *const room_maps[ROOM_COUNT][MAP_H] = {
    {
        "####################",
        "#wwd##wwd##wwwl....#",
        "#..##..##....#.....#",
        "#..##..##..v.#.....#",
        "#..................#",
        "#..p....=....p.....#",
        "#==================#",
        "#..................#",
        "#....d.........d...#",
        "#...###.......###..#",
        "#...###.......###..#",
        "#..................#",
        "#..p.......p.......#",
        "#..................#",
        "#..................#",
        "#..................#",
        "#..................#",
        "####################"
    },
    {
        "####################",
        "#....P.............#",
        "#..................#",
        "#..b...........t...#",
        "#..................#",
        "#....1....2....3...#",
        "#..................#",
        "#......s...........#",
        "#..................#",
        "#..................#",
        "#..................#",
        "#..................#",
        "#..................#",
        "#..................#",
        "#...............d..#",
        "#..................#",
        "#..................#",
        "####################"
    },
    {
        "####################",
        "#rrrrrrrrrrrrrrrrrr#",
        "#r................r#",
        "#r..R......R......r#",
        "#r................r#",
        "#r.cccccccccccccc.r#",
        "#r................r#",
        "#r.................#",
        "#r......R....R....r#",
        "#r................r#",
        "#r................r#",
        "#r................r#",
        "#r................r#",
        "#r................r#",
        "#r........d.......r#",
        "#r................r#",
        "#rrrrrrrrrrrrrrrrrr#",
        "####################"
    },
    {
        "####################",
        "#wwwwwwwwwwwwwwwwww#",
        "#w................w#",
        "#w..T......T......w#",
        "#w................w#",
        "#w.cccccccccccccc.w#",
        "#w................w#",
        "#w................w#",
        "#w..T.......T.....w#",
        "#w................w#",
        "#w................w#",
        "#w................w#",
        "#w................w#",
        "#w................w#",
        "#w........d.......w#",
        "#w................w#",
        "#wwwwwwwwwwwwwwwwww#",
        "####################"
    },
    {
        "####################",
        "#wwwwwwwwwwwwwwwwww#",
        "#w................w#",
        "#w................w#",
        "#w................w#",
        "#w.nnnnnnnnnnnnnn.w#",
        "#w................w#",
        "#w................w#",
        "#w....t...........w#",
        "#w................w#",
        "#w................w#",
        "#w................w#",
        "#w................w#",
        "#w................w#",
        "#w........d.......w#",
        "#w................w#",
        "#wwwwwwwwwwwwwwwwww#",
        "####################"
    },
    {
        "####################",
        "#gggggggggggggggggg#",
        "#g.....p.....p....g#",
        "#g................g#",
        "#g....S...........g#",
        "#g................g#",
        "#g.................#",
        "#g...p........p...g#",
        "#g................g#",
        "#g.....G....G.....g#",
        "#g................g#",
        "#g................g#",
        "#g................g#",
        "#g................g#",
        "#g........d.......g#",
        "#g................g#",
        "#gggggggggggggggggg#",
        "####################"
    }
};

const npc_t npcs[NPC_COUNT] = {
    { ROOM_RECORDS, 9u, 7u, 25u, "Mina" },
    { ROOM_TECH, 9u, 7u, 26u, "Rook" },
    { ROOM_RAMEN, 9u, 7u, 27u, "Soma" },
    { ROOM_ROOFTOP, 10u, 6u, 28u, "Iona" },
    { ROOM_DISTRICT, 10u, 8u, 29u, "Pix" }
};

const task_t tasks[TASK_COUNT] = {
    {
        NPC_IONA, ITEM_SOFTPHONES, MEDIA_ROOFTOP_SET, 12u,
        "SOFTPHONES",
        "Iona needs fresh softphones from Rook before sunset mix hour.",
        "Iona slips you a rooftop bootleg and a fistful of credits."
    },
    {
        NPC_MINA, ITEM_BROTH, MEDIA_VENDING_DREAMS, 10u,
        "BROTH RUN",
        "Mina forgot dinner again. Bring sealed night broth from Soma.",
        "Mina warms up, then rewards you with a rare vending-pop tape."
    },
    {
        NPC_ROOK, ITEM_MINIDISC, MEDIA_RAIN_LOOP, 14u,
        "DISC SWAP",
        "Rook wants a blank mini-disc from Needle & Neon for an archive rip.",
        "Rook grins and gives you Rain Loop, a tiny ambient treasure."
    }
};

static const door_t doors[] = {
    { ROOM_DISTRICT, 3u, 1u, ROOM_RECORDS, 9u, 14u },
    { ROOM_DISTRICT, 8u, 1u, ROOM_TECH, 9u, 14u },
    { ROOM_DISTRICT, 15u, 1u, ROOM_ROOFTOP, 10u, 14u },
    { ROOM_DISTRICT, 5u, 8u, ROOM_APARTMENT, 15u, 14u },
    { ROOM_DISTRICT, 15u, 8u, ROOM_RAMEN, 9u, 14u },
    { ROOM_RECORDS, 9u, 14u, ROOM_DISTRICT, 3u, 2u },
    { ROOM_TECH, 9u, 14u, ROOM_DISTRICT, 8u, 2u },
    { ROOM_ROOFTOP, 10u, 14u, ROOM_DISTRICT, 15u, 2u },
    { ROOM_APARTMENT, 15u, 14u, ROOM_DISTRICT, 5u, 9u },
    { ROOM_RAMEN, 9u, 14u, ROOM_DISTRICT, 15u, 9u }
};

static const hotspot_t hotspots[] = {
    { ROOM_APARTMENT, 3u, 3u, HOTSPOT_BED },
    { ROOM_APARTMENT, 7u, 7u, HOTSPOT_MEDIA },
    { ROOM_APARTMENT, 5u, 5u, HOTSPOT_DECOR },
    { ROOM_APARTMENT, 10u, 5u, HOTSPOT_DECOR },
    { ROOM_APARTMENT, 15u, 5u, HOTSPOT_DECOR }
};

uint8_t g_map_buffer[MAP_SIZE];

static uint8_t tile_for_char(char c) {
    switch (c) {
        case '#': return 3u;
        case 'w': return 4u;
        case 'd': return 5u;
        case 'c': return 6u;
        case 'r': return 8u;
        case 'R': return 8u;
        case 'v': return 9u;
        case 'n': return 10u;
        case 'P': return 14u;
        case 'S': return 15u;
        case 'T': return 16u;
        case 'g': return 17u;
        case 'p': return 18u;
        case 'l': return 19u;
        case 'b': return 13u;
        case 't': return 22u;
        case 's': return 7u;
        case '=': return 2u;
        case '.': return 0u;
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
        case DECOR_LAMP: return 21u;
        case DECOR_SHELF: return 7u;
        case DECOR_TANK: return 22u;
        case DECOR_DOCK: return 23u;
        default: return 0u;
    }
}

static void stamp_apartment_decor(void) {
    static const uint8_t slot_x[3] = { 5u, 10u, 15u };
    static const uint8_t slot_y[3] = { 5u, 5u, 5u };
    uint8_t i;
    uint8_t tile;

    for (i = 0; i != 3u; ++i) {
        tile = decor_tile(g_save.decor_slots[i]);
        g_map_buffer[(uint16_t)slot_y[i] * MAP_W + slot_x[i]] = tile;
    }
}

static void stamp_room_npcs(void) {
    uint8_t i;

    for (i = 0; i != NPC_COUNT; ++i) {
        if (npcs[i].room_id == g_save.room_id) {
            g_map_buffer[(uint16_t)npcs[i].y * MAP_W + npcs[i].x] = npcs[i].tile;
        }
    }
}

void dt_load_room(void) {
    uint8_t x;
    uint8_t y;
    const char *row;
    char c;

    for (y = 0; y != MAP_H; ++y) {
        row = room_maps[g_save.room_id][y];
        for (x = 0; x != MAP_W; ++x) {
            c = row[x];
            if (c == '\0') {
                c = '.';
            }
            g_map_buffer[(uint16_t)y * MAP_W + x] = tile_for_char(c);
        }
    }

    if (g_save.room_id == ROOM_APARTMENT) {
        stamp_apartment_decor();
    }

    stamp_room_npcs();
}

void dt_enter_room(uint8_t room_id, uint8_t x, uint8_t y) {
    g_save.room_id = room_id;
    g_save.player_x = x;
    g_save.player_y = y;
    dt_load_room();
    dt_save_game();
}

static bool door_match(const door_t *door, uint8_t room_id, uint8_t x, uint8_t y) {
    return (door->room_id == room_id) && (door->x == x) && (door->y == y);
}

void dt_check_door(void) {
    uint8_t i;

    for (i = 0; i != (sizeof(doors) / sizeof(doors[0])); ++i) {
        if (door_match(&doors[i], g_save.room_id, g_save.player_x, g_save.player_y)) {
            dt_enter_room(doors[i].target_room, doors[i].target_x, doors[i].target_y);
            return;
        }
    }
}

bool dt_npc_at(uint8_t x, uint8_t y, uint8_t *npc_id) {
    uint8_t i;

    for (i = 0; i != NPC_COUNT; ++i) {
        if ((npcs[i].room_id == g_save.room_id) && (npcs[i].x == x) && (npcs[i].y == y)) {
            *npc_id = i;
            return true;
        }
    }
    return false;
}

bool dt_hotspot_at(uint8_t x, uint8_t y, uint8_t *hotspot_id) {
    uint8_t i;

    for (i = 0; i != (sizeof(hotspots) / sizeof(hotspots[0])); ++i) {
        if ((hotspots[i].room_id == g_save.room_id) && (hotspots[i].x == x) && (hotspots[i].y == y)) {
            *hotspot_id = hotspots[i].id;
            return true;
        }
    }
    return false;
}

bool dt_tile_solid(uint8_t tile_id) {
    if (tile_id == 3u || tile_id == 4u || tile_id == 6u || tile_id == 8u ||
        tile_id == 9u || tile_id == 10u || tile_id == 13u || tile_id == 16u) {
        return true;
    }
    if (tile_id >= 25u) {
        return true;
    }
    return false;
}

const uint16_t *dt_room_palette(void) {
    if (g_save.room_id == ROOM_DISTRICT) {
        return district_bg_pal;
    }
    if (g_save.room_id == ROOM_ROOFTOP) {
        return rooftop_bg_pal;
    }
    return interior_bg_pal;
}
