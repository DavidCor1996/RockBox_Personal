/***************************************************************************
 * 8-Bit Rebellion: native offline campaign reconstruction for Rockbox.
 * Original resources are imported separately by tools/8bit_rebellion.
 * Movement, encounters and timing are adapted for the click wheel.
 * SPDX-License-Identifier: GPL-2.0-or-later
 ****************************************************************************/
#include "plugin.h"
#include "8bit_rebellion_state.h"

#define BR_DIR PLUGIN_GAMES_DATA_DIR "/8bit_rebellion"
#define BR_SCENES 64
#define BR_QUESTS 64
#define BR_ACTORS 17
#define BR_W 40
#define BR_H 58
#define BR_WORLD_H 200
#define BR_TOP 22
#define BR_AUDIO_SLOTS 8
#define BR_AUDIO_BYTES 32768
#define BR_CHANNEL PCM_MIXER_CHAN_PLAYBACK
#define BR_BG LCD_RGBPACK(18, 22, 30)
#define BR_TEXT LCD_RGBPACK(244, 236, 215)
#define BR_ACCENT LCD_RGBPACK(244, 194, 75)

struct br_scene
{
    char name[48];
    int width, ground, district, track, hub;
};

struct br_quest
{
    int scene, kind, count, actor, reward;
    char label[40], objective[240], dialogue[400];
};

struct br_object
{
    int id, x, hp, cooldown;
};

static struct br_scene scenes[BR_SCENES];
static struct br_quest quests[BR_QUESTS];
static struct br_state state;
static struct br_object objects[31];
static int scene_count, quest_count, object_count, selected;
static int camera, destination, facing, tick, hurt, attack;
static bool usb_exit, soundtrack, audio_owned;
static unsigned saved_frequency;
static fb_data actors[BR_ACTORS][4][BR_W * BR_H];
static fb_data flipped[BR_W * BR_H];
static fb_data *scene_pixels;
static size_t background_bytes;
static char line[1024], notice[160];
static int notice_ticks;
static int audio_fd = -1, audio_track = -1;
static unsigned char audio_ring[BR_AUDIO_SLOTS][BR_AUDIO_BYTES]
    CACHEALIGN_ATTR;
static volatile unsigned audio_ready[BR_AUDIO_SLOTS];
static int audio_current = -1;
static int audio_read_slot, audio_write;
static bool audio_running;

static const char * const district_names[] = {
    "City Center", "Casino Row", "The Park", "Downtown", "SoHo",
    "The West Side", "The Beach"
};
static const char * const song_names[] = {
    "One Step Closer", "Faint", "In the End", "New Divide", "QWERTY",
    "Hands Held High", "Crawling", "No More Sorrow", "Blackbirds"
};

static bool read_exact(int fd, void *data, size_t bytes)
{
    unsigned char *p = data;
    while (bytes)
    {
        ssize_t n = rb->read(fd, p, bytes);
        if (n <= 0)
            return false;
        p += n;
        bytes -= n;
    }
    return true;
}

static bool write_exact(int fd, const void *data, size_t bytes)
{
    const unsigned char *p = data;
    while (bytes)
    {
        ssize_t n = rb->write(fd, p, bytes);
        if (n <= 0)
            return false;
        p += n;
        bytes -= n;
    }
    return true;
}

static int fields(char *text, char **out, int count)
{
    int n = 1;
    char *p;
    out[0] = text;
    for (p = text; *p; ++p)
    {
        if (*p == '\r' || *p == '\n')
        {
            *p = 0;
            break;
        }
        if (*p == '|')
        {
            if (n == count)
                return 0;
            *p = 0;
            out[n++] = p + 1;
        }
    }
    return n;
}

static int number(const char *s, int low, int high)
{
    bool negative = *s == '-';
    int value = 0;
    if (negative)
        ++s;
    if (!*s)
        return low - 1;
    while (*s)
    {
        if (*s < '0' || *s > '9' || value > 100000)
            return low - 1;
        value = value * 10 + *s++ - '0';
    }
    if (negative)
        value = -value;
    return value < low || value > high ? low - 1 : value;
}

static bool load_data(void)
{
    int fd, length, i;
    char *f[8];
    fd = rb->open(BR_DIR "/ready.dat", O_RDONLY);
    if (fd < 0)
        return false;
    bool valid = read_exact(fd, line, 4) && !rb->memcmp(line, "8BR1", 4);
    rb->close(fd);
    if (!valid)
        return false;
    fd = rb->open(BR_DIR "/scenes.tsv", O_RDONLY);
    if (fd < 0)
        return false;
    while ((length = rb->read_line(fd, line, sizeof(line))) > 0)
    {
        struct br_scene *s = &scenes[scene_count];
        if (scene_count == BR_SCENES || length >= (int)sizeof(line) - 1 ||
            fields(line, f, 6) != 6 || !f[0][0] ||
            rb->strlen(f[0]) >= sizeof(s->name))
            goto bad;
        rb->strlcpy(s->name, f[0], sizeof(s->name));
        s->width = number(f[1], 320, 4096);
        s->ground = number(f[2], BR_H, BR_WORLD_H - 1);
        s->district = number(f[3], 0, 6);
        s->track = number(f[4], 0, 12);
        s->hub = number(f[5], 0, 1);
        if (s->width < 320 || s->ground < BR_H || s->district < 0 ||
            s->track < 0 || s->hub < 0)
            goto bad;
        ++scene_count;
    }
    rb->close(fd);
    if (scene_count == 0 || length < 0)
        return false;
    fd = rb->open(BR_DIR "/quests.tsv", O_RDONLY);
    if (fd < 0)
        return false;
    while ((length = rb->read_line(fd, line, sizeof(line))) > 0)
    {
        struct br_quest *q = &quests[quest_count];
        if (quest_count == BR_QUESTS || length >= (int)sizeof(line) - 1 ||
            fields(line, f, 8) != 8)
            goto bad;
        q->scene = number(f[0], -1, scene_count - 1);
        q->kind = number(f[1], 0, 4);
        q->count = number(f[2], 1, 31);
        q->actor = number(f[3], 0, BR_ACTORS - 1);
        q->reward = number(f[4], 0, 63);
        if (q->scene < -1 || q->kind < 0 || q->count < 1 ||
            q->actor < 0 || q->reward < 0 ||
            (q->scene == -1 && q->kind != 1) ||
            rb->strlen(f[5]) >= sizeof(q->label) ||
            rb->strlen(f[6]) >= sizeof(q->objective) ||
            rb->strlen(f[7]) >= sizeof(q->dialogue))
            goto bad;
        rb->strlcpy(q->label, f[5], sizeof(q->label));
        rb->strlcpy(q->objective, f[6], sizeof(q->objective));
        rb->strlcpy(q->dialogue, f[7], sizeof(q->dialogue));
        ++quest_count;
    }
    rb->close(fd);
    if (!quest_count || length < 0)
        return false;
    fd = rb->open(BR_DIR "/sprites.rgb", O_RDONLY);
    if (fd < 0)
        return false;
    valid = rb->filesize(fd) == (off_t)sizeof(actors) &&
        read_exact(fd, actors, sizeof(actors));
    rb->close(fd);
    if (!valid)
        return false;
    /* Refuse an incomplete pack before taking playback ownership. */
    for (i = 0; i < scene_count; ++i)
    {
        rb->snprintf(line, sizeof(line), BR_DIR "/scenes/%02d.rgb", i);
        fd = rb->open(line, O_RDONLY);
        if (fd < 0)
            return false;
        valid = rb->filesize(fd) == scenes[i].width * BR_WORLD_H * 2;
        rb->close(fd);
        if (!valid)
            return false;
    }
    return true;
bad:
    rb->close(fd);
    return false;
}

static void message(const char *text)
{
    rb->strlcpy(notice, text, sizeof(notice));
    notice_ticks = 75;
}

static bool save_game(void)
{
    struct br_state copy = state;
    int fd;
    bool ok;
    copy.sequence++;
    copy.checksum = br_checksum(&copy);
    rb->snprintf(line, sizeof(line), BR_DIR "/save%u.dat",
                 (unsigned)(copy.sequence & 1));
    fd = rb->open(line, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    ok = write_exact(fd, &copy, sizeof(copy));
    ok = (rb->close(fd) == 0) && ok;
    if (ok)
        state.sequence = copy.sequence;
    return ok;
}

static void load_game(void)
{
    int i, fd;
    struct br_state candidate;
    state.magic = BR_SAVE_MAGIC;
    state.version = BR_SAVE_VERSION;
    state.health = 5;
    state.x = 90;
    state.coins = 20;
    for (i = 0; i < scene_count; ++i)
        if (scenes[i].hub && scenes[i].district == 0)
            state.scene = i;
    for (i = 0; i < 2; ++i)
    {
        rb->snprintf(line, sizeof(line), BR_DIR "/save%d.dat", i);
        fd = rb->open(line, O_RDONLY);
        if (fd < 0)
            continue;
        bool ok = rb->filesize(fd) == sizeof(candidate) &&
            read_exact(fd, &candidate, sizeof(candidate));
        rb->close(fd);
        if (ok && br_state_valid(&candidate, scene_count, quest_count) &&
            candidate.x < (unsigned)scenes[candidate.scene].width &&
            (candidate.quest == (unsigned)quest_count ?
             candidate.collected == 0 :
             candidate.collected < (1u << quests[candidate.quest].count)) &&
            (state.sequence == 0 ||
             (int32_t)(candidate.sequence - state.sequence) > 0))
            state = candidate;
    }
}

static void audio_callback(const void **start, size_t *size)
{
    if (audio_current >= 0)
        audio_ready[audio_current] = 0;
    audio_current = -1;
    if (audio_ready[audio_read_slot])
    {
        audio_current = audio_read_slot;
        *start = audio_ring[audio_read_slot];
        *size = BR_AUDIO_BYTES;
        audio_read_slot = (audio_read_slot + 1) % BR_AUDIO_SLOTS;
    }
    else
    {
        *start = NULL;
        *size = 0;
    }
}

static void audio_stop_track(void)
{
    if (audio_owned)
    {
        rb->mixer_channel_stop(BR_CHANNEL);
        rb->mixer_channel_set_buffer_hook(BR_CHANNEL, NULL);
    }
    if (audio_fd >= 0)
        rb->close(audio_fd);
    audio_fd = -1;
    audio_track = -1;
    audio_running = false;
    audio_current = -1;
    audio_read_slot = audio_write = 0;
    rb->memset((void *)audio_ready, 0, sizeof(audio_ready));
}

static void audio_service(void)
{
    int n;
    if (!audio_owned || audio_fd < 0)
        return;
    if (!audio_ready[audio_write])
    {
        unsigned used = 0;
        while (used < BR_AUDIO_BYTES)
        {
            n = rb->read(audio_fd, audio_ring[audio_write] + used,
                         BR_AUDIO_BYTES - used);
            if (n < 0 || (n == 0 &&
                rb->lseek(audio_fd, 0, SEEK_SET) != 0))
            {
                audio_stop_track();
                message("Could not read soundtrack");
                return;
            }
            if (n > 0)
                used += n;
        }
        rb->pcm_play_lock();
        audio_ready[audio_write] = 1;
        audio_write = (audio_write + 1) % BR_AUDIO_SLOTS;
        rb->pcm_play_unlock();
    }
    if (!audio_running && audio_ready[0] && audio_ready[1])
    {
        rb->mixer_channel_play_data(BR_CHANNEL, audio_callback, NULL, 0);
        audio_running = true;
    }
    else if (audio_running &&
             rb->mixer_channel_status(BR_CHANNEL) == CHANNEL_STOPPED)
    {
        rb->mixer_channel_play_data(BR_CHANNEL, audio_callback, NULL, 0);
    }
}

static bool audio_open(int track)
{
    if (!soundtrack || (audio_fd >= 0 && audio_track == track))
        return true;
    audio_stop_track();
    rb->snprintf(line, sizeof(line), BR_DIR "/%02d.pcm", track);
    audio_fd = rb->open(line, O_RDONLY);
    if (audio_fd < 0)
        return false;
    off_t bytes = rb->filesize(audio_fd);
    if (bytes < 4 || bytes % 4)
    {
        audio_stop_track();
        return false;
    }
    if (!audio_owned)
    {
        size_t unused;
        /* Core owns the stop/reset/codec handoff, never a playlist rewrite. */
        if (!rb->plugin_get_audio_buffer(&unused))
        {
            audio_stop_track();
            return false;
        }
        saved_frequency = rb->mixer_get_frequency();
        audio_owned = true;
        rb->mixer_set_frequency(44100);
        rb->pcmbuf_fade(false, true);
    }
    audio_track = track;
    for (int i = 0; i < BR_AUDIO_SLOTS; ++i)
        audio_service();
    return audio_fd >= 0;
}

static void audio_close(void)
{
    audio_stop_track();
    if (audio_owned)
    {
        rb->pcmbuf_fade(false, false);
        rb->mixer_set_frequency(saved_frequency);
        rb->plugin_release_audio_buffer();
        audio_owned = false;
    }
}

static void place_objects(void)
{
    struct br_scene *s = &scenes[state.scene];
    object_count = selected = 0;
    destination = -1;
    if (state.quest >= (unsigned)quest_count)
        return;
    struct br_quest *q = &quests[state.quest];
    if (q->scene >= 0 && q->scene != (int)state.scene)
        return;
    if (q->scene < 0 && !s->hub)
        return;
    for (int i = 0; i < q->count; ++i)
    {
        if ((state.collected & (1u << i)) ||
            (q->scene < 0 && i % 7 != s->district))
            continue;
        struct br_object *o = &objects[object_count++];
        o->id = i;
        int local = q->scene < 0 ? i / 7 : i;
        int count = q->scene < 0 ? (q->count + 6) / 7 : q->count;
        o->x = 110 + (s->width - 200) * (local + 1) / (count + 1);
        o->hp = q->kind == 3 ? 30 : 3;
        o->cooldown = 25 + i * 7;
    }
}

static bool enter_scene(int index)
{
    int fd;
    size_t size = scenes[index].width * BR_WORLD_H * sizeof(fb_data);
    if (size > background_bytes)
        return false;
    rb->snprintf(line, sizeof(line), BR_DIR "/scenes/%02d.rgb", index);
    fd = rb->open(line, O_RDONLY);
    if (fd < 0)
        return false;
    /* Pause our channel while a large room is read. */
    if (audio_owned)
        rb->mixer_channel_play_pause(BR_CHANNEL, false);
    bool ok = read_exact(fd, scene_pixels, size);
    rb->close(fd);
    if (!ok)
        return false;
    if (index != (int)state.scene)
        state.x = 90;
    state.scene = index;
    place_objects();
    if (!audio_open(state.quest == (unsigned)quest_count ? 8 :
                    scenes[index].track))
        message("Soundtrack unavailable");
    if (audio_owned)
        rb->mixer_channel_play_pause(BR_CHANNEL, true);
    DEBUGF("8br: scene=%d quest=%lu\n", index, (unsigned long)state.quest);
    return true;
}

static void text(int x, int y, fb_data colour, const char *s)
{
    rb->lcd_set_foreground(colour);
    rb->lcd_putsxy(x, y, s);
}

static void sprite(int actor, int phase, int x, int feet, bool mirror)
{
    fb_data *src = actors[actor][phase];
    if (x + BR_W < 0 || x >= LCD_WIDTH)
        return;
    if (mirror)
    {
        for (int y = 0; y < BR_H; ++y)
            for (int col = 0; col < BR_W; ++col)
                flipped[y * BR_W + col] = src[y * BR_W + BR_W - 1 - col];
        src = flipped;
    }
    rb->lcd_bitmap_transparent(src, x, feet - BR_H, BR_W, BR_H);
}

static void draw(void)
{
    struct br_scene *s = &scenes[state.scene];
    struct br_quest *q = state.quest < (unsigned)quest_count ?
        &quests[state.quest] : NULL;
    camera = MAX(0, MIN((int)state.x - LCD_WIDTH / 2,
                       s->width - LCD_WIDTH));
    rb->lcd_set_foreground(BR_BG);
    rb->lcd_clear_display();
    rb->lcd_bitmap_part(scene_pixels, camera, 0, s->width,
                         0, BR_TOP, LCD_WIDTH, BR_WORLD_H);
    for (int i = 0; i < object_count; ++i)
    {
        struct br_object *o = &objects[i];
        int x = o->x - camera;
        if (state.collected & (1u << o->id))
            continue;
        if (q->kind == 1 && q->count == 20)
        {
            rb->lcd_set_foreground(BR_ACCENT);
            rb->lcd_fillrect(x - 10, BR_TOP + s->ground - 48, 20, 24);
            text(x - 6, BR_TOP + s->ground - 44, BR_BG, "LP");
        }
        else
            sprite(q->actor, (tick / 8 + i) % 4, x - BR_W / 2,
                   BR_TOP + s->ground, o->x > (int)state.x);
        if (i == selected)
        {
            rb->lcd_set_foreground(BR_ACCENT);
            rb->lcd_drawrect(x - 21, BR_TOP + s->ground - 61, 42, 63);
        }
    }
    if (!hurt || tick % 4 < 2)
        sprite(state.costume == 1 ? 8 : state.costume == 2 ? 10 : 0,
               destination >= 0 || (rb->button_status() &
               (BUTTON_LEFT | BUTTON_RIGHT)) ? (tick / 3) % 4 : 0,
               state.x - camera - BR_W / 2, BR_TOP + s->ground, facing > 0);
    if (attack)
    {
        rb->lcd_set_foreground(BR_ACCENT);
        int x = state.x - camera + facing * 28;
        rb->lcd_drawline(x, BR_TOP + s->ground - 50,
                         x + facing * 12, BR_TOP + s->ground - 20);
    }
    rb->lcd_set_foreground(BR_BG);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, BR_TOP);
    text(4, 2, BR_TEXT, s->name);
    rb->snprintf(line, sizeof(line), "HP %lu/5  $%lu  Tracks %u/6",
                 (unsigned long)state.health, (unsigned long)state.coins,
                 br_progress(state.tracks));
    text(4, 12, BR_ACCENT, line);
    rb->lcd_set_foreground(BR_BG);
    rb->lcd_fillrect(0, 222, LCD_WIDTH, LCD_HEIGHT - 222);
    if (notice_ticks)
        text(3, 224, BR_TEXT, notice);
    else if (object_count && selected < object_count && q)
    {
        rb->snprintf(line, sizeof(line), "%s [%u/%d]  Select: act",
                     q->label, br_progress(state.collected), q->count);
        text(3, 224, BR_ACCENT, line);
    }
    else
        text(3, 224, BR_TEXT, "Play: map / objective    Menu: save & exit");
    rb->lcd_update();
}

static void event_cleanup(void *unused)
{
    (void)unused;
    audio_close();
    save_game();
}

static int input(int timeout)
{
    int button = rb->button_get_w_tmo(timeout);
    if (rb->default_event_handler_ex(button, event_cleanup, NULL) ==
        SYS_USB_CONNECTED)
        usb_exit = true;
    return button;
}

static int menu(const char *title, const char * const *items, int count)
{
    int chosen = 0;
    while (!usb_exit)
    {
        rb->lcd_set_foreground(BR_BG);
        rb->lcd_clear_display();
        text(8, 8, BR_ACCENT, title);
        int first = MAX(0, chosen - 8);
        for (int i = first; i < count && i < first + 11; ++i)
        {
            int y = 30 + (i - first) * 17;
            if (i == chosen)
            {
                rb->lcd_set_foreground(LCD_RGBPACK(54, 68, 82));
                rb->lcd_fillrect(3, y - 2, LCD_WIDTH - 6, 16);
            }
            text(8, y, BR_TEXT, items[i]);
        }
        rb->lcd_update();
        int b = input(HZ / 10);
        if (b == BUTTON_MENU || b == BUTTON_PLAY)
            return -1;
        if (b == BUTTON_SELECT)
            return chosen;
        if (b == BUTTON_SCROLL_FWD ||
            b == BUTTON_RIGHT || b == (BUTTON_SCROLL_FWD | BUTTON_REPEAT))
            chosen = (chosen + 1) % count;
        if (b == BUTTON_SCROLL_BACK ||
            b == BUTTON_LEFT || b == (BUTTON_SCROLL_BACK | BUTTON_REPEAT))
            chosen = (chosen + count - 1) % count;
    }
    return -1;
}

static void panel(const char *title, const char *body)
{
    const char *cursor = body;
    bool more = true;
    while (more && !usb_exit)
    {
        rb->lcd_set_foreground(BR_BG);
        rb->lcd_clear_display();
        text(8, 8, BR_ACCENT, title);
        int y = 32;
        for (; *cursor && y < 205; y += 12)
        {
            int count = MIN((int)rb->strlen(cursor), 49);
            if (cursor[count] && count == 49)
                while (count > 0 && cursor[count] != ' ')
                    --count;
            if (!count)
                count = 49;
            rb->memcpy(line, cursor, count);
            line[count] = 0;
            text(8, y, BR_TEXT, line);
            cursor += count;
            while (*cursor == ' ')
                ++cursor;
        }
        text(8, 222, BR_ACCENT, *cursor ? "Select: next   Menu: close" :
             "Select / Menu: close");
        rb->lcd_update();
        while (!usb_exit)
        {
            int b = input(HZ / 10);
            if (b == BUTTON_MENU || b == BUTTON_PLAY)
            {
                more = false;
                break;
            }
            if (b == BUTTON_SELECT)
            {
                more = *cursor != 0;
                break;
            }
        }
    }
}

static bool world_map(void)
{
    const char *items[BR_SCENES];
    int indices[BR_SCENES], count = 0;
    int district = menu("Travel to district", district_names, 7);
    if (district < 0)
        return true;
    for (int i = 0; i < scene_count; ++i)
    {
        if (scenes[i].district != district)
            continue;
        items[count] = scenes[i].name;
        indices[count++] = i;
    }
    int room = menu(district_names[district], items, count);
    if (room < 0)
        return true;
    if (!enter_scene(indices[room]))
        return false;
    if (!save_game())
        message("Save failed; check free space");
    return true;
}

static void objective(void)
{
    char body[400];
    if (state.quest >= (unsigned)quest_count)
    {
        panel("Rebellion complete", "The six tracks are reunited. "
              "Blackbirds is unlocked in the music player. "
              "You can keep exploring the city.");
        return;
    }
    struct br_quest *q = &quests[state.quest];
    rb->snprintf(body, sizeof(body), "%s Location: %s. Progress: %u/%d.",
                 q->objective, q->scene < 0 ? "district streets" :
                 scenes[q->scene].name, br_progress(state.collected), q->count);
    panel("Current objective", body);
}

static void interact(void)
{
    if (state.quest >= (unsigned)quest_count || !object_count)
    {
        message("Play opens the map and objectives");
        return;
    }
    struct br_quest *q = &quests[state.quest];
    struct br_object *o = &objects[selected];
    if (state.collected & (1u << o->id))
        return;
    if (abs(o->x - (int)state.x) > 48)
    {
        destination = o->x;
        message("Walking to selected target");
        return;
    }
    if (q->kind == 2 || q->kind == 3)
    {
        if (attack)
            return;
        attack = 7;
        facing = o->x > (int)state.x ? 1 : -1;
        o->hp -= 1 + state.weapon;
        if (o->hp > 0)
            return;
    }
    if (q->kind == 4)
    {
        static const char * const passwords[] = {"QWERTY", "PIXXEL", "REBELLION"};
        if (audio_owned)
            rb->mixer_channel_play_pause(BR_CHANNEL, false);
        int choice = menu("Door password", passwords, 3);
        if (audio_owned)
            rb->mixer_channel_play_pause(BR_CHANNEL, true);
        if (choice != 0)
        {
            message("Access denied. Listen at Pixxel Kafe.");
            return;
        }
    }
    if (!br_collect(&state, o->id, q->count))
        return;
    if (br_complete(&state, q->count, q->reward))
    {
        if (audio_owned)
            rb->mixer_channel_play_pause(BR_CHANNEL, false);
        if (!save_game())
            message("Save failed; check free space");
        if (q->dialogue[0])
            panel(q->label, q->dialogue);
        objective();
        if (state.quest == (unsigned)quest_count)
            audio_open(8);
        if (audio_owned)
            rb->mixer_channel_play_pause(BR_CHANNEL, true);
        place_objects();
        DEBUGF("8br: advanced quest=%lu tracks=%lu\n",
               (unsigned long)state.quest, (unsigned long)state.tracks);
    }
    else
    {
        for (int i = 0; i < object_count; ++i)
            if (!(state.collected & (1u << objects[i].id)))
            {
                selected = i;
                break;
            }
        if (!save_game())
            message("Save failed; check free space");
    }
}

static void update(void)
{
    int held = rb->button_status(), move = 0;
    ++tick;
    if (hurt) --hurt;
    if (attack) --attack;
    if (notice_ticks) --notice_ticks;
    if (held & BUTTON_LEFT) move = -1;
    if (held & BUTTON_RIGHT) move = 1;
    if (move)
        destination = -1;
    if (destination >= 0)
    {
        if (abs(destination - (int)state.x) <= 5)
            destination = -1;
        else
            move = destination > (int)state.x ? 1 : -1;
    }
    if (move)
    {
        facing = move;
        state.x = MAX(20, MIN((int)state.x + move * 4,
                              scenes[state.scene].width - 20));
    }
    if (state.quest >= (unsigned)quest_count)
        return;
    struct br_quest *q = &quests[state.quest];
    if (q->kind != 2 && q->kind != 3)
        return;
    for (int i = 0; i < object_count; ++i)
    {
        struct br_object *o = &objects[i];
        if (state.collected & (1u << o->id))
            continue;
        int distance = abs(o->x - (int)state.x);
        if (distance < 150 && distance > 30 && tick % 2 == 0)
            o->x += o->x < (int)state.x ? 1 : -1;
        if (o->cooldown) --o->cooldown;
        if (distance < 38 && !o->cooldown && !hurt)
        {
            o->cooldown = q->kind == 3 ? 30 : 50;
            hurt = 30;
            if (--state.health == 0)
            {
                state.health = 5;
                state.x = 40;
                state.coins = state.coins > 5 ? state.coins - 5 : 0;
                destination = -1;
                place_objects();
                message("Recovered. Your quest progress is safe.");
                break;
            }
        }
    }
}

static bool pause_menu(void)
{
    static const char * const items[] = {
        "Resume", "Map / travel", "Current objective", "Inventory",
        "Shop / equipment", "Music player", "Volume up", "Volume down",
        "Save game", "Save and exit"
    };
    bool running = true;
    if (audio_owned)
        rb->mixer_channel_play_pause(BR_CHANNEL, false);
    int choice = menu("8-Bit Rebellion!", items, ARRAYLEN(items));
    switch (choice)
    {
        case 1:
            if (!world_map())
            {
                rb->splash(HZ * 2, "Could not load room");
                running = false;
            }
            break;
        case 2: objective(); break;
        case 3:
        {
            char body[256];
            rb->snprintf(body, sizeof(body),
                "Recovered tracks: %u of 6. Coins: %lu. Weapon level: %lu. "
                "Quest %lu of %d. Equipment is adapted for the click wheel. "
                "Select acts on the highlighted target. Wheel changes target.",
                br_progress(state.tracks), (unsigned long)state.coins,
                (unsigned long)state.weapon + 1,
                (unsigned long)MIN(state.quest + 1, (unsigned)quest_count),
                quest_count);
            panel("Inventory", body);
            break;
        }
        case 4:
        {
            static const char * const shop[] = {
                "Heal fully - 10 coins", "Upgrade weapon - 30 coins",
                "Change outfit - free"
            };
            int item = menu("Equipment", shop, 3);
            if (item == 0 && state.coins >= 10)
            { state.coins -= 10; state.health = 5; }
            else if (item == 1 && state.coins >= 30 && state.weapon < 3)
            { state.coins -= 30; state.weapon++; }
            else if (item == 2)
                state.costume = (state.costume + 1) % 3;
            else if (item >= 0)
                message("Not enough coins, or already at maximum");
            break;
        }
        case 5:
        {
            static const char * const modes[] = {
                "Game soundtrack", "Keep user music / silent game"
            };
            int mode = menu("Music", modes, 2);
            if (mode == 1)
            { soundtrack = false; audio_close(); }
            if (mode == 0)
            {
                if (!audio_owned && rb->audio_status())
                {
                    static const char * const confirm[] = {
                        "Keep my music", "Stop my music; use game soundtrack"
                    };
                    if (menu("Soundtrack", confirm, 2) != 1)
                        break;
                }
                int track = menu("Tracks", song_names,
                                 state.tracks == 63 ? 9 : 8);
                if (track >= 0)
                {
                    soundtrack = true;
                    if (!audio_open(track))
                        message("Soundtrack unavailable");
                }
            }
            break;
        }
        case 6: rb->adjust_volume(1); break;
        case 7: rb->adjust_volume(-1); break;
        case 8: message(save_game() ? "Game saved" : "Save failed"); break;
        case 9: running = false; break;
        default: break;
    }
    if (audio_owned)
        rb->mixer_channel_play_pause(BR_CHANNEL, true);
    destination = -1;
    return running;
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    enum plugin_status result = PLUGIN_OK;
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(BR_BG);
    scene_pixels = rb->plugin_get_buffer(&background_bytes);
    if (!scene_pixels || !load_data())
    {
        rb->splash(HZ * 3, "8-Bit Rebellion game data missing or invalid");
        return PLUGIN_ERROR;
    }
    load_game();
    soundtrack = rb->audio_status() == 0;
    facing = 1;
    if (!enter_scene(state.scene))
    {
        rb->splash(HZ * 2, "Could not load game room");
        audio_close();
        return PLUGIN_ERROR;
    }
    message("Play: map / objective   Select: interact");
    long next = *rb->current_tick;
    bool running = true;
    while (running && !usb_exit)
    {
        audio_service();
        int b = input(1);
        if (b == BUTTON_MENU)
            running = false;
        else if (b == BUTTON_PLAY)
            running = pause_menu();
        else if (b == BUTTON_SELECT)
            interact();
        else if (object_count && (b == BUTTON_SCROLL_FWD ||
                 b == BUTTON_SCROLL_BACK ||
                 b == (BUTTON_SCROLL_FWD | BUTTON_REPEAT) ||
                 b == (BUTTON_SCROLL_BACK | BUTTON_REPEAT)))
        {
            int direction = (b & BUTTON_SCROLL_FWD) ? 1 : -1;
            for (int i = 0; i < object_count; ++i)
            {
                selected = (selected + object_count + direction) % object_count;
                if (!(state.collected & (1u << objects[selected].id)))
                    break;
            }
            destination = -1;
        }
        if (!TIME_BEFORE(*rb->current_tick, next))
        {
            update();
            draw();
            next = *rb->current_tick + MAX(1, HZ / 25);
        }
    }
    audio_close();
    if (usb_exit)
        result = PLUGIN_USB_CONNECTED;
    else if (!save_game())
    {
        rb->splash(HZ * 2, "Game could not be saved");
        result = PLUGIN_ERROR;
    }
    rb->lcd_setfont(FONT_UI);
    return result;
}
