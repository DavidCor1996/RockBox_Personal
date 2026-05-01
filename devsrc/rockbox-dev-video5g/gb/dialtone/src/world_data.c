#include "dialtone.h"

const char *const room_maps[ROOM_COUNT][MAP_H] = {
    {
        "####################",
        "#wwd##wwd##wwd##waa#",
        "#..##..##..##..#uvv#",
        "#..##..##..##..#.,,#",
        "#..p....l....p..u..#",
        "#..,....=....,.....#",
        "#==================#",
        "#..u....=....u..k..#",
        "#....d....l.....d..#",
        "#...###....|..###..#",
        "#...###...|...###..#",
        "#......u......u....#",
        "#..p.......p.......#",
        "#....k.......q.....#",
        "#.............i....#",
        "#....u....x....u...#",
        "#......,......,....#",
        "####################"
    },
    {
        "####################",
        "#....h.....m.......#",
        "#..................#",
        "#..b.........f.....#",
        "#..................#",
        "#....1....2....3...#",
        "#..................#",
        "#..o.....s.....t...#",
        "#......k...........#",
        "#.............x....#",
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
        "#r...o.........l..r#",
        "#r......R....R....r#",
        "#r................r#",
        "#r................r#",
        "#r........k.......r#",
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
        "#w..m......m......w#",
        "#w................w#",
        "#w.cccccccccccccc.w#",
        "#w................w#",
        "#w...x.........k..w#",
        "#w..m.......m.....w#",
        "#w................w#",
        "#w................w#",
        "#w........h.......w#",
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
        "#w.....l..........w#",
        "#w................w#",
        "#w.nnnnnnnnnnnnnn.w#",
        "#w................w#",
        "#w....s.....s.....w#",
        "#w......t.........w#",
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
        "#g....a...........g#",
        "#g................g#",
        "#g.........x......g#",
        "#g...p........p...g#",
        "#g................g#",
        "#g.....G....G.....g#",
        "#g................g#",
        "#g................g#",
        "#g................g#",
        "#g................g#",
        "#g.........d......g#",
        "#g................g#",
        "#gggggggggggggggggg#",
        "####################"
    },
    {
        "####################",
        "#........d.........#",
        "#..................#",
        "#..pppp......lll...#",
        "#..pwwp......lll...#",
        "#..pddp......lll...#",
        "#..pppp............#",
        "#..................#",
        "#........==........#",
        "#........==........#",
        "#..................#",
        "#............ttt...#",
        "#............tst...#",
        "#............ttt...#",
        "#..................#",
        "#........q.........#",
        "#..................#",
        "####################"
    }
};

const npc_t npcs[NPC_COUNT] = {
    { ROOM_RECORDS, 9u, 7u, 25u, "Mina" },
    { ROOM_TECH, 9u, 7u, 26u, "Rook" },
    { ROOM_RAMEN, 9u, 7u, 27u, "Soma" },
    { ROOM_ROOFTOP, 10u, 6u, 28u, "Iona" },
    { ROOM_DISTRICT, 14u, 13u, 29u, "Pix" }
};

const task_t tasks[TASK_COUNT] = {
    {
        NPC_IONA, ITEM_SOFTPHONES, MEDIA_ROOFTOP_SET, 12u,
        "SOFTPHONES",
        "Bring Iona softphones from Rook.",
        "Iona trades you a rooftop bootleg and some credits."
    },
    {
        NPC_MINA, ITEM_BROTH, MEDIA_VENDING_DREAMS, 10u,
        "BROTH RUN",
        "Bring Mina sealed broth from Soma.",
        "Mina hands over a vending-pop tape and some change."
    },
    {
        NPC_ROOK, ITEM_MINIDISC, MEDIA_RAIN_LOOP, 14u,
        "DISC SWAP",
        "Bring Rook a blank mini-disc from Mina.",
        "Rook grins and gives you Rain Loop with a few credits."
    }
};

const door_t doors[DOOR_COUNT] = {
    { ROOM_DISTRICT, 3u, 1u, ROOM_RECORDS, 9u, 14u },
    { ROOM_DISTRICT, 8u, 1u, ROOM_TECH, 9u, 14u },
    { ROOM_DISTRICT, 13u, 1u, ROOM_ROOFTOP, 10u, 14u },
    { ROOM_DISTRICT, 5u, 8u, ROOM_APARTMENT, 16u, 14u },
    { ROOM_DISTRICT, 16u, 8u, ROOM_RAMEN, 9u, 14u },
    { ROOM_DISTRICT, 9u, 15u, ROOM_SOUTHLINE, 9u, 2u },
    { ROOM_RECORDS, 9u, 14u, ROOM_DISTRICT, 3u, 2u },
    { ROOM_TECH, 9u, 14u, ROOM_DISTRICT, 8u, 2u },
    { ROOM_ROOFTOP, 10u, 14u, ROOM_DISTRICT, 13u, 2u },
    { ROOM_APARTMENT, 16u, 14u, ROOM_DISTRICT, 5u, 9u },
    { ROOM_RAMEN, 9u, 14u, ROOM_DISTRICT, 16u, 9u },
    { ROOM_SOUTHLINE, 9u, 1u, ROOM_DISTRICT, 9u, 14u }
};

const hotspot_t hotspots[HOTSPOT_COUNT] = {
    { ROOM_APARTMENT, 3u, 3u, 1u },
    { ROOM_APARTMENT, 5u, 5u, 2u },
    { ROOM_APARTMENT, 10u, 5u, 2u },
    { ROOM_APARTMENT, 15u, 5u, 2u },
    { ROOM_APARTMENT, 3u, 7u, 3u },
    { ROOM_SOUTHLINE, 4u, 5u, 4u }
};
