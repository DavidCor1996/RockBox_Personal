#include "dialtone.h"
#include <stdio.h>
#include <string.h>
#include <gbdk/font.h>

bool dt_npc_at(uint8_t x, uint8_t y, uint8_t *npc_id);
bool dt_hotspot_at(uint8_t x, uint8_t y, uint8_t *hotspot_id);
bool dt_tile_solid(uint8_t tile_id);
const uint16_t *dt_room_palette(void);
void dt_check_door(void);

static uint8_t running;
static uint8_t exit_to_title;
static font_t ui_font;
static const uint16_t text_bg_pal[] = {
    RGB8(10, 10, 18), RGB8(78, 82, 120), RGB8(130, 210, 255), RGB8(246, 244, 255)
};
static const uint16_t text_sprite_pal[] = {
    RGB8(10, 10, 18), RGB8(86, 52, 130), RGB8(255, 108, 188), RGB8(246, 244, 255)
};

static void apply_text_palette(void) {
    if (_cpu == CGB_TYPE) {
        set_bkg_palette(0u, 1u, text_bg_pal);
        set_sprite_palette(0u, 1u, text_sprite_pal);
    } else {
        BGP_REG = 0xE4u;
        OBP0_REG = 0xE4u;
        OBP1_REG = 0xE4u;
    }
}

static void use_text_mode(void) {
    HIDE_WIN;
    HIDE_SPRITES;
    SHOW_BKG;
    apply_text_palette();
    font_set(ui_font);
    cls();
    gotoxy(0u, 0u);
}

static void restore_map_mode(void) {
    dt_render_room();
}

static const char *item_name(uint8_t item) {
    switch (item) {
        case ITEM_SOFTPHONES: return "Softphones";
        case ITEM_BROTH: return "Night Broth";
        case ITEM_MINIDISC: return "Blank MiniDisc";
        case ITEM_POSTER: return "Poster";
        case ITEM_LAMP: return "Neon Lamp";
        case ITEM_SHELF: return "Crate Shelf";
        case ITEM_TANK: return "Fish Tank";
        case ITEM_DOCK: return "Pocket Dock";
        default: return "Nothing";
    }
}

static const char *media_name(uint8_t media) {
    switch (media) {
        case MEDIA_RAIN_LOOP: return "Rain Loop";
        case MEDIA_VENDING_DREAMS: return "Vending Dreams";
        case MEDIA_ROOFTOP_SET: return "Rooftop Set";
        default: return "Unknown";
    }
}

static uint8_t decor_to_item(uint8_t decor) {
    switch (decor) {
        case DECOR_POSTER: return ITEM_POSTER;
        case DECOR_LAMP: return ITEM_LAMP;
        case DECOR_SHELF: return ITEM_SHELF;
        case DECOR_TANK: return ITEM_TANK;
        case DECOR_DOCK: return ITEM_DOCK;
        default: return 0u;
    }
}

static uint8_t item_to_decor(uint8_t item) {
    switch (item) {
        case ITEM_POSTER: return DECOR_POSTER;
        case ITEM_LAMP: return DECOR_LAMP;
        case ITEM_SHELF: return DECOR_SHELF;
        case ITEM_TANK: return DECOR_TANK;
        case ITEM_DOCK: return DECOR_DOCK;
        default: return DECOR_NONE;
    }
}

bool dt_has_item(uint8_t item) {
    return (g_save.inventory & item) != 0u;
}

void dt_add_item(uint8_t item) {
    g_save.inventory |= item;
}

void dt_remove_item(uint8_t item) {
    g_save.inventory &= (uint8_t)~item;
}

const task_t *dt_current_task(void) {
    return &tasks[g_save.task_id % TASK_COUNT];
}

void dt_assign_daily_task(void) {
    g_save.task_id = (g_save.day - 1u) % TASK_COUNT;
    g_save.task_done = 0u;
    dt_save_game();
}

static void print_prompt_footer(const char *footer) {
    gotoxy(0u, 16u);
    printf("--------------------");
    gotoxy(0u, 17u);
    printf("%s", footer);
}

void dt_show_text(const char *title, const char *body, const char *footer) {
    uint8_t keys;
    uint8_t last;
    use_text_mode();
    printf("%s\n\n%s", title, body);
    print_prompt_footer(footer);

    last = 0u;
    while (1) {
        vsync();
        keys = joypad();
        if (((keys & J_A) && !(last & J_A)) ||
            ((keys & J_START) && !(last & J_START)) ||
            ((keys & J_B) && !(last & J_B))) {
            waitpadup();
            break;
        }
        last = keys;
    }
}

uint8_t dt_menu(const char *title, const char *subtitle, const char *const *items, uint8_t count, uint8_t selected) {
    uint8_t i;
    uint8_t keys;
    uint8_t last;

    last = 0u;
    while (1) {
        use_text_mode();
        printf("%s\n", title);
        printf("%s\n\n", subtitle);
        for (i = 0u; i != count; ++i) {
            printf("%c %s\n", (i == selected) ? '>' : ' ', items[i]);
        }
        print_prompt_footer("WHEEL MOVE  SELECT OK");

        while (1) {
            vsync();
            keys = joypad();
            if ((keys & J_UP) && !(last & J_UP)) {
                selected = (selected == 0u) ? (count - 1u) : (selected - 1u);
                break;
            }
            if ((keys & J_DOWN) && !(last & J_DOWN)) {
                selected = (selected + 1u) % count;
                break;
            }
            if ((keys & J_A) && !(last & J_A)) {
                waitpadup();
                return selected;
            }
            last = keys;
        }
        waitpadup();
        last = 0u;
    }
}

void dt_render_room(void) {
    uint8_t sprite_tile;

    HIDE_WIN;
    HIDE_BKG;
    HIDE_SPRITES;
    set_bkg_data(0u, BG_TILE_COUNT, bg_tiles);
    set_bkg_tiles(0u, 0u, MAP_W, MAP_H, g_map_buffer);
    set_sprite_data(0u, PLAYER_TILE_COUNT, player_tiles);

    if (_cpu == CGB_TYPE) {
        set_bkg_palette(0u, 1u, dt_room_palette());
        set_sprite_palette(0u, 1u, sprite_pal);
    } else {
        BGP_REG = 0xE4u;
        OBP0_REG = 0xE4u;
        OBP1_REG = 0xE4u;
    }

    sprite_tile = 0u;
    if (g_save.facing == DIR_UP) sprite_tile = 1u;
    else if (g_save.facing == DIR_LEFT) sprite_tile = 2u;
    else if (g_save.facing == DIR_RIGHT) sprite_tile = 3u;

    set_sprite_tile(0u, sprite_tile);
    move_sprite(0u, (uint8_t)(g_save.player_x * 8u + 8u), (uint8_t)(g_save.player_y * 8u + 16u));
    SHOW_BKG;
    SHOW_SPRITES;
}

static void dt_update_player_sprite(void) {
    uint8_t sprite_tile;

    sprite_tile = 0u;
    if (g_save.facing == DIR_UP) sprite_tile = 1u;
    else if (g_save.facing == DIR_LEFT) sprite_tile = 2u;
    else if (g_save.facing == DIR_RIGHT) sprite_tile = 3u;

    set_sprite_tile(0u, sprite_tile);
    move_sprite(0u, (uint8_t)(g_save.player_x * 8u + 8u), (uint8_t)(g_save.player_y * 8u + 16u));
}

static void reward_task(const task_t *task) {
    g_save.task_done = 1u;
    g_save.credits += task->reward_credits;
    g_save.media |= task->reward_media;
    dt_save_game();
}

static void npc_dialogue(uint8_t npc_id) {
    const task_t *task;

    task = dt_current_task();
    if (npc_id == NPC_IONA) {
        if ((g_save.task_id == TASK_MUSIC_DROP) && !g_save.task_done) {
            if (dt_has_item(ITEM_SOFTPHONES)) {
                dt_remove_item(ITEM_SOFTPHONES);
                reward_task(task);
                dt_show_text("IONA", task->complete, "SELECT CLOSE");
            } else {
                dt_show_text("IONA", "The booth sounds bright, but my softphones fizzed out.\n\nRook should have a pair that still hugs the ears.", "SELECT CLOSE");
            }
            return;
        }
        dt_show_text("IONA", "Rain makes the antennas sing. When the block gets lonely,\nI leave a little synth-pop in the air for it.", "SELECT CLOSE");
        return;
    }

    if (npc_id == NPC_MINA) {
        if ((g_save.task_id == TASK_SOUP_RUN) && !g_save.task_done) {
            if (dt_has_item(ITEM_BROTH)) {
                dt_remove_item(ITEM_BROTH);
                reward_task(task);
                dt_show_text("MINA", task->complete, "SELECT CLOSE");
            } else {
                dt_show_text("MINA", "I sorted imports until the steam went cold.\n\nIf Soma still has sealed night broth, I'd trade the whole mood for one cup.", "SELECT CLOSE");
            }
            return;
        }
        dt_shop_screen();
        return;
    }

    if (npc_id == NPC_ROOK) {
        if ((g_save.task_id == TASK_DISC_SWAP) && !g_save.task_done) {
            if (dt_has_item(ITEM_MINIDISC)) {
                dt_remove_item(ITEM_MINIDISC);
                reward_task(task);
                dt_show_text("ROOK", task->complete, "SELECT CLOSE");
            } else {
                dt_show_text("ROOK", "Needle & Neon keeps blank mini-discs under the counter.\n\nI only need one. The startup choir deserves clean storage.", "SELECT CLOSE");
            }
            return;
        }
        if (!dt_has_item(ITEM_SOFTPHONES)) {
            if (g_save.credits >= 8u) {
                g_save.credits -= 8u;
                dt_add_item(ITEM_SOFTPHONES);
                dt_save_game();
                dt_show_text("ROOK", "Fresh softphones. Foam pads, quiet cups, no hiss.\n\nTreat them kindly.", "SELECT CLOSE");
            } else {
                dt_show_text("ROOK", "I can fit you with softphones for 8 credits.\n\nCome back when your pockets stop echoing.", "SELECT CLOSE");
            }
        } else {
            dt_show_text("ROOK", "Old handhelds only sound broken when nobody listens.\n\nEverything in here just wants a patient bench light.", "SELECT CLOSE");
        }
        return;
    }

    if (npc_id == NPC_SOMA) {
        if (!dt_has_item(ITEM_BROTH)) {
            if (g_save.credits >= 4u) {
                g_save.credits -= 4u;
                dt_add_item(ITEM_BROTH);
                dt_save_game();
                dt_show_text("SOMA", "One sealed broth cup for the walk home.\n\nDon't tip it into the rain.", "SELECT CLOSE");
            } else {
                dt_show_text("SOMA", "Night broth is 4 credits.\n\nCheap comfort, but not free comfort.", "SELECT CLOSE");
            }
        } else {
            dt_show_text("SOMA", "Some people collect vinyl. Some collect spare chargers.\nI collect reasons for the block to stay warm.", "SELECT CLOSE");
        }
        return;
    }

    dt_show_text("PIX", "Your room finally looks lived in.\n\nUse LEFT for your pack, RIGHT for your tunes, MENU when your thoughts get crowded.", "SELECT CLOSE");
}

static void interact_hotspot(uint8_t hotspot_id) {
    if (hotspot_id == 1u) {
        if (g_save.task_done) {
            ++g_save.day;
            dt_assign_daily_task();
            dt_show_text("ROOM", "You sleep under CRT glow and rain static.\n\nA new little favor is waiting in the block tomorrow.", "SELECT CLOSE");
        } else {
            dt_show_text("ROOM", "The bed looks perfect, but the block still needs one small thing from you tonight.", "SELECT CLOSE");
        }
        return;
    }

    if (hotspot_id == 2u) {
        dt_decor_screen();
        return;
    }

    dt_media_screen();
}

void dt_try_interact(void) {
    int8_t tx;
    int8_t ty;
    uint8_t npc_id;
    uint8_t hotspot_id;

    tx = g_save.player_x;
    ty = g_save.player_y;
    if (g_save.facing == DIR_UP) ty -= 1;
    else if (g_save.facing == DIR_DOWN) ty += 1;
    else if (g_save.facing == DIR_LEFT) tx -= 1;
    else tx += 1;

    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) {
        return;
    }

    if (dt_npc_at((uint8_t)tx, (uint8_t)ty, &npc_id)) {
        npc_dialogue(npc_id);
        restore_map_mode();
        return;
    }
    if (dt_hotspot_at((uint8_t)tx, (uint8_t)ty, &hotspot_id)) {
        interact_hotspot(hotspot_id);
        restore_map_mode();
    }
}

void dt_try_move(int8_t dx, int8_t dy, uint8_t facing) {
    int8_t nx;
    int8_t ny;
    uint8_t tile;
    uint8_t old_room;

    g_save.facing = facing;
    old_room = g_save.room_id;
    nx = (int8_t)g_save.player_x + dx;
    ny = (int8_t)g_save.player_y + dy;
    if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) {
        dt_update_player_sprite();
        return;
    }

    tile = g_map_buffer[(uint16_t)ny * MAP_W + (uint8_t)nx];
    if (dt_tile_solid(tile)) {
        dt_update_player_sprite();
        return;
    }

    g_save.player_x = (uint8_t)nx;
    g_save.player_y = (uint8_t)ny;
    dt_check_door();
    if (g_save.room_id != old_room) {
        dt_render_room();
    } else {
        dt_update_player_sprite();
    }
}

static void print_task_line(void) {
    const task_t *task;

    task = dt_current_task();
    printf("Day %u  Cr %u\n", (uint16_t)g_save.day, (uint16_t)g_save.credits);
    printf("%s\n", task->title);
    printf("%s\n", g_save.task_done ? "Done. Sleep anytime." : task->brief);
}

void dt_inventory_screen(void) {
    uint8_t page;
    uint8_t keys;
    uint8_t last;

    page = 0u;
    last = 0u;
    while (1) {
        use_text_mode();
        if (page == 0u) {
            printf("PACK\n\n");
            print_task_line();
            printf("\nBag:\n");
            if (g_save.inventory == 0u) {
                printf("  nothing yet\n");
            } else {
                if (dt_has_item(ITEM_SOFTPHONES)) printf("  softphones\n");
                if (dt_has_item(ITEM_BROTH)) printf("  night broth\n");
                if (dt_has_item(ITEM_MINIDISC)) printf("  blank mini-disc\n");
                if (dt_has_item(ITEM_POSTER)) printf("  poster\n");
                if (dt_has_item(ITEM_LAMP)) printf("  neon lamp\n");
                if (dt_has_item(ITEM_SHELF)) printf("  crate shelf\n");
                if (dt_has_item(ITEM_TANK)) printf("  fish tank\n");
                if (dt_has_item(ITEM_DOCK)) printf("  pocket dock\n");
            }
        } else if (page == 1u) {
            printf("DECOR\n\n");
            printf("Slot 1: %u\n", (uint16_t)g_save.decor_slots[0]);
            printf("Slot 2: %u\n", (uint16_t)g_save.decor_slots[1]);
            printf("Slot 3: %u\n", (uint16_t)g_save.decor_slots[2]);
            printf("\nOpen the shelf pad in your room\nto rearrange the apartment.");
        } else {
            printf("MEDIA\n\nCollected:\n");
            if (g_save.media == 0u) {
                printf("  no tapes yet\n");
            } else {
                if (g_save.media & MEDIA_RAIN_LOOP) printf("  Rain Loop\n");
                if (g_save.media & MEDIA_VENDING_DREAMS) printf("  Vending Dreams\n");
                if (g_save.media & MEDIA_ROOFTOP_SET) printf("  Rooftop Set\n");
            }
            printf("\nSmall songs for rainy walks.");
        }
        print_prompt_footer("LEFT/RIGHT TAB  MENU");

        while (1) {
            vsync();
            keys = joypad();
            if ((keys & J_B) && !(last & J_B)) {
                page = (page == 0u) ? 2u : (page - 1u);
                break;
            }
            if ((keys & J_SELECT) && !(last & J_SELECT)) {
                page = (page + 1u) % 3u;
                break;
            }
            if (((keys & J_START) && !(last & J_START)) || ((keys & J_A) && !(last & J_A))) {
                waitpadup();
                return;
            }
            last = keys;
        }
        waitpadup();
        last = 0u;
    }
}

void dt_media_screen(void) {
    dt_inventory_screen();
}

void dt_decor_screen(void) {
    static const char *const slot_items[] = { "Slot 1", "Slot 2", "Slot 3", "Back" };
    static const char *const decor_items[] = { "Clear", "Poster", "Lamp", "Shelf", "Tank", "Dock", "Back" };
    uint8_t slot;
    uint8_t choice;
    uint8_t item_flag;
    uint8_t decor_value;
    uint8_t i;

    slot = dt_menu("DECOR", "Pick a room slot", slot_items, 4u, 0u);
    if (slot == 3u) {
        return;
    }

    choice = dt_menu("DECOR", "Pick an item", decor_items, 7u, 0u);
    if (choice == 6u) {
        return;
    }

    if (choice == 0u) {
        g_save.decor_slots[slot] = DECOR_NONE;
        dt_load_room();
        dt_save_game();
        return;
    }

    decor_value = choice;
    item_flag = decor_to_item(decor_value);
    if (!dt_has_item(item_flag)) {
        dt_show_text("ROOM", "You do not own that piece yet.", "SELECT CLOSE");
        return;
    }

    for (i = 0u; i != 3u; ++i) {
        if (g_save.decor_slots[i] == decor_value) {
            g_save.decor_slots[i] = DECOR_NONE;
        }
    }
    g_save.decor_slots[slot] = decor_value;
    dt_load_room();
    dt_save_game();
}

void dt_options_screen(void) {
    uint8_t row;
    uint8_t keys;
    uint8_t last;

    row = 0u;
    last = 0u;
    while (1) {
        use_text_mode();
        printf("OPTIONS\n\n");
        printf("%c Text speed: %u\n", (row == 0u) ? '>' : ' ', (uint16_t)g_save.text_speed);
        printf("%c Palette mood: %u\n", (row == 1u) ? '>' : ' ', (uint16_t)g_save.room_palette_mode);
        printf("\n1 = brisk / 2 = easy / 3 = drift\n0 = auto / 1 = fixed");
        print_prompt_footer("WHEEL MOVE  LEFT/RIGHT");

        while (1) {
            vsync();
            keys = joypad();
            if ((keys & J_UP) && !(last & J_UP)) {
                row = (row == 0u) ? 1u : 0u;
                break;
            }
            if ((keys & J_DOWN) && !(last & J_DOWN)) {
                row = (row == 0u) ? 1u : 0u;
                break;
            }
            if ((keys & J_B) && !(last & J_B)) {
                if (row == 0u && g_save.text_speed > 1u) g_save.text_speed--;
                if (row == 1u && g_save.room_palette_mode > 0u) g_save.room_palette_mode--;
                break;
            }
            if ((keys & J_SELECT) && !(last & J_SELECT)) {
                if (row == 0u && g_save.text_speed < 3u) g_save.text_speed++;
                if (row == 1u && g_save.room_palette_mode < 1u) g_save.room_palette_mode++;
                break;
            }
            if ((keys & J_START) && !(last & J_START)) {
                dt_save_game();
                waitpadup();
                return;
            }
            last = keys;
        }
        waitpadup();
        last = 0u;
    }
}

void dt_help_screen(void) {
    dt_show_text("HELP",
        "Dialtone is built for Rockboy on an iPod preset.\n\n"
        "WHEEL  Move one tile\n"
        "SELECT Talk / confirm\n"
        "LEFT   Pack / previous tab\n"
        "RIGHT  Tunes / next tab\n"
        "MENU   Pause / back\n\n"
        "Sleep only after the daily favor is done.",
        "SELECT CLOSE");
}

void dt_shop_screen(void) {
    static const char *const items[] = {
        "MiniDisc  6",
        "Poster   10",
        "Lamp     12",
        "Shelf    14",
        "Tank     16",
        "Dock     12",
        "Leave"
    };
    uint8_t choice;
    uint8_t price;
    uint8_t item;

    choice = dt_menu("NEEDLE & NEON", "Rainy little luxuries", items, 7u, 0u);
    if (choice == 6u) {
        return;
    }

    price = 0u;
    item = 0u;
    if (choice == 0u) { price = 6u; item = ITEM_MINIDISC; }
    else if (choice == 1u) { price = 10u; item = ITEM_POSTER; }
    else if (choice == 2u) { price = 12u; item = ITEM_LAMP; }
    else if (choice == 3u) { price = 14u; item = ITEM_SHELF; }
    else if (choice == 4u) { price = 16u; item = ITEM_TANK; }
    else if (choice == 5u) { price = 12u; item = ITEM_DOCK; }

    if (dt_has_item(item) && item != ITEM_MINIDISC) {
        dt_show_text("MINA", "You already own that piece.\n\nYour room is small. That is part of its charm.", "SELECT CLOSE");
        return;
    }

    if (g_save.credits < price) {
        dt_show_text("MINA", "Not enough credits tonight.", "SELECT CLOSE");
        return;
    }

    g_save.credits -= price;
    dt_add_item(item);
    dt_save_game();
    dt_show_text("MINA", item_name(item), "SELECT CLOSE");
}

void dt_pause_menu(void) {
    static const char *const items[] = {
        "Resume",
        "Pack",
        "Decor",
        "Options",
        "Help",
        "Save + Title"
    };
    uint8_t choice;

    choice = dt_menu("PAUSE", "Neon block breather", items, 6u, 0u);
    if (choice == 1u) dt_inventory_screen();
    else if (choice == 2u) dt_decor_screen();
    else if (choice == 3u) dt_options_screen();
    else if (choice == 4u) dt_help_screen();
    else if (choice == 5u) {
        dt_save_game();
        exit_to_title = 1u;
    }
    restore_map_mode();
}

void dt_setup_new_game(void) {
    static const char *const keepsakes[] = {
        "Pocket Dock",
        "Glow Poster",
        "Neon Lamp"
    };
    uint8_t choice;

    memset(&g_save, 0, sizeof(g_save));
    g_save.started = 1u;
    g_save.day = 1u;
    g_save.credits = 18u;
    g_save.text_speed = 2u;
    g_save.room_palette_mode = 0u;
    g_save.room_id = ROOM_APARTMENT;
    g_save.player_x = 14u;
    g_save.player_y = 13u;
    g_save.facing = DIR_UP;

    choice = dt_menu("NEW DRIFT", "Pick a keepsake", keepsakes, 3u, 0u);
    g_save.keepsake = choice;
    if (choice == 0u) {
        dt_add_item(ITEM_DOCK);
        g_save.decor_slots[0] = DECOR_DOCK;
    } else if (choice == 1u) {
        dt_add_item(ITEM_POSTER);
        g_save.decor_slots[0] = DECOR_POSTER;
    } else {
        dt_add_item(ITEM_LAMP);
        g_save.decor_slots[0] = DECOR_LAMP;
    }

    dt_assign_daily_task();
    dt_load_room();
    dt_save_game();
    dt_show_text("ARRIVAL",
        "You move into Luma Lane with one crate, one keepsake,\nand a room full of blue rain.\n\n"
        "The block trades in tiny favors, glowing gadgets, and songs small enough to keep.",
        "SELECT START");
}

bool dt_title(void) {
    static const char *const with_save[] = { "Continue", "New Drift", "Help", "Quit" };
    static const char *const no_save[] = { "New Drift", "Help", "Quit" };
    uint8_t choice;

    while (1) {
        if (dt_has_save()) {
            choice = dt_menu("DIALTONE", "cozy cyberpunk district", with_save, 4u, 0u);
            if (choice == 0u) {
                if (dt_load_game()) {
                    dt_load_room();
                    return true;
                }
            } else if (choice == 1u) {
                dt_setup_new_game();
                return true;
            } else if (choice == 2u) {
                dt_help_screen();
            } else {
                return false;
            }
        } else {
            choice = dt_menu("DIALTONE", "cozy cyberpunk district", no_save, 3u, 0u);
            if (choice == 0u) {
                dt_setup_new_game();
                return true;
            } else if (choice == 1u) {
                dt_help_screen();
            } else {
                return false;
            }
        }
    }
}

void dt_init(void) {
    SPRITES_8x8;
    font_init();
    ui_font = font_load(font_ibm);
    apply_text_palette();
}

void dt_game_loop(void) {
    uint8_t keys;
    uint8_t last;

    running = 1u;
    exit_to_title = 0u;
    last = 0u;
    dt_render_room();

    while (running) {
        vsync();
        keys = joypad();

        if ((keys & J_START) && !(last & J_START)) {
            dt_pause_menu();
        } else if ((keys & J_A) && !(last & J_A)) {
            dt_try_interact();
        } else if ((keys & J_B) && !(last & J_B)) {
            dt_inventory_screen();
            restore_map_mode();
        } else if ((keys & J_SELECT) && !(last & J_SELECT)) {
            dt_media_screen();
            restore_map_mode();
        } else if ((keys & J_UP) && !(last & J_UP)) {
            dt_try_move(0, -1, DIR_UP);
        } else if ((keys & J_DOWN) && !(last & J_DOWN)) {
            dt_try_move(0, 1, DIR_DOWN);
        } else if ((keys & J_LEFT) && !(last & J_LEFT)) {
            dt_try_move(-1, 0, DIR_LEFT);
        } else if ((keys & J_RIGHT) && !(last & J_RIGHT)) {
            dt_try_move(1, 0, DIR_RIGHT);
        }

        if (exit_to_title) {
            running = 0u;
        }
        last = keys;
    }
}
