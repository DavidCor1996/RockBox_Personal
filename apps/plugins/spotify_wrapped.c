/***************************************************************************
 * Offline Spotify Wrapped for RockPod.  It reads Rockbox's existing local
 * runtime database and playback log. It previews the top track through the
 * normal Rockbox decoder and never owns the shared audio buffer or networking.
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"

#if !defined(HAVE_TAGCACHE) || !defined(HAVE_LCD_COLOR) || !defined(HAVE_JPEG) || \
    LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error Spotify Wrapped requires tagcache and a 320x240 colour display
#endif

#define SW_ROOT             ROCKBOX_DIR "/spotify-wrapped"
#define SW_MESSAGE          SW_ROOT "/artist-message.tsv"
#define SW_SUMMARY          SW_ROOT "/summary.tsv"
#define SW_SUMMARY_TMP      SW_ROOT "/summary.tmp"
#define SW_DATA             SW_ROOT "/data.tsv"
#define SW_PLAYER           VIEWERS_DIR "/mpegplayer.rock"
#define SW_ASSET_ROOT       ROCKBOX_DIR "/ipodjs/spotify-wrapped"
#define SW_BRAND_MARK       SW_ASSET_ROOT "/spotify-mark.24x24x24.bmp"

#define SW_MAX_LOG_TRACKS   512
#define SW_MAX_ARTISTS      160
#define SW_MAX_ALBUMS       192
#define SW_MAX_GENRES       64
#define SW_TOP_COUNT        5
#define SW_NAME             80
#define SW_DETAIL           80
#define SW_ART_SIZE         104
#define SW_ART_SLOTS        4
#define SW_SAFE_VOLUME      (-18)
#define SW_AUDIO_WAIT       (5 * HZ)

#define SW_BLACK            LCD_RGBPACK(0x09, 0x09, 0x09)
#define SW_CHROME           LCD_RGBPACK(0x1c, 0x1c, 0x1c)
#define SW_PANEL            LCD_RGBPACK(0x2a, 0x2a, 0x2a)
#define SW_EDGE             LCD_RGBPACK(0x4c, 0x4c, 0x4c)
#define SW_GREEN            LCD_RGBPACK(0x8a, 0xbe, 0x00)
#define SW_LIGHT_GREEN      LCD_RGBPACK(0xad, 0xd9, 0x16)
#define SW_TEXT             LCD_RGBPACK(0xee, 0xee, 0xee)
#define SW_MUTED            LCD_RGBPACK(0xaa, 0xaa, 0xaa)
#define SW_PURPLE           LCD_RGBPACK(0x54, 0x22, 0x9b)
#define SW_PINK             LCD_RGBPACK(0xf0, 0x37, 0xa5)
#define SW_YELLOW           LCD_RGBPACK(0xff, 0xd7, 0x18)
#define SW_BLUE             LCD_RGBPACK(0x13, 0x46, 0xc7)
#define SW_CYAN             LCD_RGBPACK(0x34, 0xd8, 0xd1)
#define SW_ORANGE           LCD_RGBPACK(0xff, 0x6b, 0x24)
#define SW_CREAM            LCD_RGBPACK(0xff, 0xf2, 0xc4)
#define SW_CORAL            LCD_RGBPACK(0xff, 0x5b, 0x47)
#define SW_LIME             LCD_RGBPACK(0xb8, 0xff, 0x69)
#define SW_BRIGHT_YELLOW    LCD_RGBPACK(0xff, 0xe6, 0x00)
#define SW_SKY              LCD_RGBPACK(0x16, 0xb9, 0xe9)
#define SW_VIOLET           LCD_RGBPACK(0x58, 0x24, 0xb8)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping sw_main_ctx[] =
{
    { PLA_SCROLL_BACK, BUTTON_SCROLL_BACK, BUTTON_NONE },
    { PLA_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT, BUTTON_SCROLL_FWD|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_LEFT, BUTTON_LEFT, BUTTON_NONE },
    { PLA_RIGHT, BUTTON_RIGHT, BUTTON_NONE },
    { PLA_SELECT_REL, BUTTON_SELECT|BUTTON_REL, BUTTON_NONE },
    { PLA_CANCEL, BUTTON_MENU, BUTTON_NONE },
    { PLA_EXIT, BUTTON_PLAY|BUTTON_REL, BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *sw_contexts[] = { sw_main_ctx };
#else
static const struct button_mapping *sw_contexts[] = { pla_main_ctx };
#endif

enum sw_tab { SW_OVERVIEW, SW_MINUTES, SW_TOP_SONG, SW_SONGS,
              SW_TOP_ARTIST, SW_ARTISTS, SW_TOP_ALBUM, SW_ALBUMS,
              SW_GENRES, SW_LISTENING, SW_PERSONALITY, SW_ALL_TIME,
              SW_MESSAGE_TAB,
              SW_TAB_COUNT };

enum sw_art_slot { SW_ART_SONG, SW_ART_ARTIST, SW_ART_ALBUM,
                   SW_ART_LIFETIME_ARTIST };

struct sw_rank {
    char name[SW_NAME];
    char detail[SW_DETAIL];
    unsigned long plays;
    unsigned long seconds;
};

struct sw_log_track {
    unsigned long hash;
    unsigned long plays;
    unsigned long seconds;
};

struct sw_totals {
    unsigned long plays;
    unsigned long seconds;
    unsigned long unique_tracks;
    unsigned long favourite_hour[24];
    unsigned long favourite_month[12];
    struct sw_rank songs[SW_TOP_COUNT];
    int song_count;
    struct sw_rank artists[SW_MAX_ARTISTS];
    int artist_count;
    struct sw_rank albums[SW_MAX_ALBUMS];
    int album_count;
    struct sw_rank genres[SW_MAX_GENRES];
    int genre_count;
};

struct sw_preview_session {
    bool active;
    bool inserted;
    bool had_playback;
    bool was_paused;
    bool saved_runtimedb;
    bool saved_playback_log;
    bool saved_autoresume;
    int saved_index;
    int saved_amount;
    int saved_volume;
    int saved_repeat;
    unsigned long saved_elapsed;
    unsigned long saved_offset;
    char saved_path[MAX_PATH];
};

static struct sw_log_track log_tracks[SW_MAX_LOG_TRACKS];
static int log_track_count;
static struct sw_totals this_year;
static struct sw_totals all_time;
static int current_year;
static int selected_tab;
static int selected_row;
static bool log_overflow;
static bool database_ready;
static bool snapshot_ready;
static bool lifetime_scope;
static bool message_ready;
static unsigned long unique_artists;
static unsigned long unique_albums;
static unsigned long unique_genres;
static char message_artist[SW_NAME];
static char lifetime_artist_name[SW_NAME];
static char message_title[SW_NAME];
static char message_path[MAX_PATH];
static char preview_track_path[MAX_PATH];
static char status_line[64];
static char art_track_path[SW_ART_SLOTS][MAX_PATH];
static unsigned long art_track_weight[SW_ART_SLOTS];
static fb_data art_pixels[SW_ART_SLOTS][SW_ART_SIZE * SW_ART_SIZE]
    CACHEALIGN_ATTR;
static struct bitmap art_bitmap[SW_ART_SLOTS];
static bool art_valid[SW_ART_SLOTS];
static fb_data brand_pixels[24 * 24] CACHEALIGN_ATTR;
static struct bitmap brand_bitmap;
static bool brand_valid;
static long animation_start;
static int font_body = FONT_UI;
static int font_heading = FONT_UI;
static int font_big = FONT_UI;
static struct sw_preview_session preview;

static int sw_tab_count(void)
{
    /* A Message tab only exists when RockPod installed a verified local
     * greeting for the current all-time number-one artist. */
    return message_ready ? SW_TAB_COUNT : SW_MESSAGE_TAB;
}

static unsigned long sw_hash(const char *text)
{
    unsigned long value = 2166136261UL;
    while (text && *text)
        value = (value ^ (unsigned char)*text++) * 16777619UL;
    return value;
}

static int sw_casecmp(const char *left, const char *right)
{
    while (*left || *right)
    {
        int a = tolower((unsigned char)*left++);
        int b = tolower((unsigned char)*right++);
        if (a != b)
            return a - b;
    }
    return 0;
}

static void sw_copy(char *out, size_t size, const char *in, const char *fallback)
{
    rb->strlcpy(out, in && in[0] ? in : fallback, size);
}

static void sw_rank_add(struct sw_rank *rows, int *count, int maximum,
                        const char *name, const char *detail,
                        unsigned long plays, unsigned long seconds)
{
    int i;
    if (!name || !name[0])
        name = "Unknown";
    for (i = 0; i < *count; i++)
        if (!sw_casecmp(rows[i].name, name) && !sw_casecmp(rows[i].detail,
                                                            detail ? detail : ""))
        {
            rows[i].plays += plays;
            rows[i].seconds += seconds;
            return;
        }
    if (*count >= maximum)
        return;
    rb->memset(&rows[*count], 0, sizeof(rows[*count]));
    sw_copy(rows[*count].name, sizeof(rows[*count].name), name, "Unknown");
    sw_copy(rows[*count].detail, sizeof(rows[*count].detail), detail, "");
    rows[*count].plays = plays;
    rows[*count].seconds = seconds;
    (*count)++;
}

static void sw_top_insert(struct sw_rank *rows, int *count,
                          const char *name, const char *detail,
                          unsigned long plays, unsigned long seconds)
{
    int i, insert = *count;
    struct sw_rank row;
    if (plays == 0)
        return;
    for (i = 0; i < *count; i++)
        if (plays > rows[i].plays ||
            (plays == rows[i].plays && seconds > rows[i].seconds))
        {
            insert = i;
            break;
        }
    if (insert == *count && *count == SW_TOP_COUNT)
        return;
    if (*count < SW_TOP_COUNT)
        (*count)++;
    for (i = *count - 1; i > insert; i--)
        rows[i] = rows[i - 1];
    rb->memset(&row, 0, sizeof(row));
    sw_copy(row.name, sizeof(row.name), name, "Unknown track");
    sw_copy(row.detail, sizeof(row.detail), detail, "Unknown artist");
    row.plays = plays;
    row.seconds = seconds;
    rows[insert] = row;
}

static void sw_sort(struct sw_rank *rows, int count)
{
    int i, j;
    for (i = 0; i < count; i++)
        for (j = i + 1; j < count; j++)
            if (rows[j].plays > rows[i].plays ||
                (rows[j].plays == rows[i].plays &&
                 rows[j].seconds > rows[i].seconds))
            {
                struct sw_rank temp = rows[i];
                rows[i] = rows[j];
                rows[j] = temp;
            }
}

static void sw_finish_totals(struct sw_totals *totals)
{
    sw_sort(totals->artists, totals->artist_count);
    sw_sort(totals->albums, totals->album_count);
    sw_sort(totals->genres, totals->genre_count);
}

static struct sw_log_track *sw_log_find(unsigned long hash)
{
    int i;
    for (i = 0; i < log_track_count; i++)
        if (log_tracks[i].hash == hash)
            return &log_tracks[i];
    if (log_track_count >= SW_MAX_LOG_TRACKS)
    {
        log_overflow = true;
        return NULL;
    }
    log_tracks[log_track_count].hash = hash;
    log_tracks[log_track_count].plays = 0;
    log_tracks[log_track_count].seconds = 0;
    return &log_tracks[log_track_count++];
}

static bool sw_is_current_year(unsigned long timestamp)
{
    struct tm when;
    time_t when_time = (time_t)timestamp;
    rb->gmtime_r(&when_time, &when);
    return when.tm_year + 1900 == current_year;
}

static void sw_read_playback_file(const char *path)
{
    char line[MAX_PATH + 80];
    int fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *first = rb->strchr(line, ':');
        char *second = first ? rb->strchr(first + 1, ':') : NULL;
        char *third = second ? rb->strchr(second + 1, ':') : NULL;
        unsigned long timestamp, elapsed, length;
        struct sw_log_track *track;
        struct tm when;
        time_t when_time;
        if (!first || !second || !third)
            continue;
        *first = *second = *third = '\0';
        timestamp = rb->strtoul(line, NULL, 10);
        elapsed = rb->strtoul(first + 1, NULL, 10);
        length = rb->strtoul(second + 1, NULL, 10);
        if (!sw_is_current_year(timestamp) ||
            elapsed < (length && length < 30000 ? length : 30000))
            continue;
        track = sw_log_find(sw_hash(third + 1));
        if (!track)
            continue;
        track->plays++;
        track->seconds += elapsed / 1000;
        this_year.plays++;
        this_year.seconds += elapsed / 1000;
        when_time = (time_t)timestamp;
        rb->gmtime_r(&when_time, &when);
        if (when.tm_hour >= 0 && when.tm_hour < 24)
            this_year.favourite_hour[when.tm_hour]++;
        if (when.tm_mon >= 0 && when.tm_mon < 12)
            this_year.favourite_month[when.tm_mon]++;
    }
    rb->close(fd);
}

static void sw_read_playback_logs(void)
{
    char path[MAX_PATH];
    int i;
    sw_read_playback_file(ROCKBOX_DIR "/playback.log");
    for (i = 1; i <= 16; i++)
    {
        rb->snprintf(path, sizeof(path), ROCKBOX_DIR "/playback_%04d.log", i);
        sw_read_playback_file(path);
    }
    this_year.unique_tracks = log_track_count;
}

static struct sw_log_track *sw_log_for_path(const char *path)
{
    unsigned long hash = sw_hash(path);
    int i;
    for (i = 0; i < log_track_count; i++)
        if (log_tracks[i].hash == hash)
            return &log_tracks[i];
    return NULL;
}

static void sw_add_metadata(struct sw_totals *totals, const char *title,
                            const char *artist, const char *album,
                            const char *genre, unsigned long plays,
                            unsigned long seconds)
{
    sw_top_insert(totals->songs, &totals->song_count, title, artist,
                  plays, seconds);
    sw_rank_add(totals->artists, &totals->artist_count, SW_MAX_ARTISTS,
                artist, "", plays, seconds);
    sw_rank_add(totals->albums, &totals->album_count, SW_MAX_ALBUMS,
                album, artist, plays, seconds);
    sw_rank_add(totals->genres, &totals->genre_count, SW_MAX_GENRES,
                genre, "", plays, seconds);
}

static void sw_consider_art(enum sw_art_slot slot, const char *filename,
                            unsigned long weight)
{
    if (filename && filename[0] &&
        (!art_track_path[slot][0] || weight > art_track_weight[slot]))
    {
        rb->strlcpy(art_track_path[slot], filename,
                    sizeof(art_track_path[slot]));
        art_track_weight[slot] = weight;
    }
}

static void sw_collect_art_sources(void)
{
    struct sw_totals *active = this_year.plays ? &this_year : &all_time;
    struct tagcache_search search;
    char filename[MAX_PATH], title[SW_NAME], artist[SW_NAME], album[SW_NAME];

    rb->memset(art_track_path, 0, sizeof(art_track_path));
    rb->memset(art_track_weight, 0, sizeof(art_track_weight));
    if (!rb->tagcache_search(&search, tag_filename))
        return;
    while (rb->tagcache_get_next(&search, filename, sizeof(filename)))
    {
        unsigned long lifetime_weight = MAX(0,
            rb->tagcache_get_numeric(&search, tag_playcount));
        struct sw_log_track *year_track = sw_log_for_path(filename);
        unsigned long active_weight = this_year.plays ?
            (year_track ? year_track->plays : 0) : lifetime_weight;

        title[0] = artist[0] = album[0] = '\0';
        rb->tagcache_retrieve(&search, search.idx_id, tag_title,
                              title, sizeof(title));
        rb->tagcache_retrieve(&search, search.idx_id, tag_artist,
                              artist, sizeof(artist));
        rb->tagcache_retrieve(&search, search.idx_id, tag_album,
                              album, sizeof(album));
        if (active->song_count && !sw_casecmp(title, active->songs[0].name) &&
            !sw_casecmp(artist, active->songs[0].detail))
            sw_consider_art(SW_ART_SONG, filename, active_weight);
        if (active->artist_count &&
            !sw_casecmp(artist, active->artists[0].name))
            sw_consider_art(SW_ART_ARTIST, filename, active_weight);
        if (active->album_count && !sw_casecmp(album, active->albums[0].name) &&
            !sw_casecmp(artist, active->albums[0].detail))
            sw_consider_art(SW_ART_ALBUM, filename, active_weight);
        if (all_time.artist_count &&
            !sw_casecmp(artist, all_time.artists[0].name))
            sw_consider_art(SW_ART_LIFETIME_ARTIST, filename,
                            lifetime_weight);
    }
    rb->tagcache_search_finish(&search);
}

static bool sw_cover_for_track(const char *track, char *cover, size_t size)
{
    static const char * const names[] = {
        "cover.jpg", "folder.jpg", "cover.jpeg", "cover.bmp"
    };
    char *slash;
    int i;

    if (!track || !track[0])
        return false;
    if (track[0] == '/')
        rb->strlcpy(cover, track, size);
    else
        rb->snprintf(cover, size, "/%s", track);
    {
        int fd = rb->open(cover, O_RDONLY);
        if (fd >= 0)
        {
            rb->close(fd);
            const char *base = rb->strrchr(cover, '/');
            const char *extension;
            base = base ? base + 1 : cover;
            extension = rb->strrchr(base, '.');
            if (extension && (!sw_casecmp(extension, ".bmp") ||
                              !sw_casecmp(extension, ".jpg") ||
                              !sw_casecmp(extension, ".jpeg")))
                return true;
            for (i = 0; i < (int)ARRAYLEN(names); i++)
                if (!sw_casecmp(base, names[i]))
                    return true;
        }
    }
    slash = rb->strrchr(cover, '/');
    if (!slash)
        return false;
    slash[1] = '\0';
    for (i = 0; i < (int)ARRAYLEN(names); i++)
    {
        size_t directory_len = rb->strlen(cover);
        rb->strlcpy(cover + directory_len, names[i], size - directory_len);
        {
            int fd = rb->open(cover, O_RDONLY);
            if (fd >= 0)
            {
                rb->close(fd);
                return true;
            }
        }
        cover[directory_len] = '\0';
    }
    return false;
}

static void sw_load_artwork(void)
{
    char path[MAX_PATH];
    int i;
    const int format = FORMAT_NATIVE | FORMAT_RESIZE |
                       FORMAT_KEEP_ASPECT | FORMAT_DITHER;

    for (i = 0; i < SW_ART_SLOTS; i++)
    {
        int result;
        char *extension;
        art_valid[i] = false;
        if (!sw_cover_for_track(art_track_path[i], path, sizeof(path)))
            continue;
        rb->memset(&art_bitmap[i], 0, sizeof(art_bitmap[i]));
        art_bitmap[i].width = SW_ART_SIZE;
        art_bitmap[i].height = SW_ART_SIZE;
        art_bitmap[i].format = FORMAT_NATIVE;
        art_bitmap[i].data = (unsigned char *)art_pixels[i];
        extension = rb->strrchr(path, '.');
        if (extension && !sw_casecmp(extension, ".bmp"))
            result = rb->read_bmp_file(path, &art_bitmap[i],
                                       sizeof(art_pixels[i]), FORMAT_NATIVE,
                                       NULL);
        else
            result = rb->read_jpeg_file(path, &art_bitmap[i],
                                        sizeof(art_pixels[i]), format, NULL);
        art_valid[i] = result > 0;
        rb->yield();
    }
}

static bool sw_scan_tagcache(void)
{
    struct tagcache_search search;
    char filename[MAX_PATH], title[SW_NAME], artist[SW_NAME];
    char album[SW_NAME], genre[SW_NAME];
    int rows = 0;
    if (!rb->tagcache_get_stat()->readyvalid ||
        !rb->tagcache_search(&search, tag_filename))
        return false;
    while (rb->tagcache_get_next(&search, filename, sizeof(filename)))
    {
        long plays = rb->tagcache_get_numeric(&search, tag_playcount);
        long milliseconds = rb->tagcache_get_numeric(&search, tag_playtime);
        struct sw_log_track *year_track = sw_log_for_path(filename);
        if (plays <= 0 && !year_track)
            continue;
        title[0] = artist[0] = album[0] = genre[0] = '\0';
        rb->tagcache_retrieve(&search, search.idx_id, tag_title,
                              title, sizeof(title));
        rb->tagcache_retrieve(&search, search.idx_id, tag_artist,
                              artist, sizeof(artist));
        rb->tagcache_retrieve(&search, search.idx_id, tag_album,
                              album, sizeof(album));
        rb->tagcache_retrieve(&search, search.idx_id, tag_genre,
                              genre, sizeof(genre));
        if (plays > 0)
        {
            all_time.plays += plays;
            all_time.seconds += milliseconds > 0 ? milliseconds / 1000 : 0;
            all_time.unique_tracks++;
            sw_add_metadata(&all_time, title, artist, album, genre, plays,
                            milliseconds > 0 ? milliseconds / 1000 : 0);
        }
        if (year_track)
            sw_add_metadata(&this_year, title, artist, album, genre,
                            year_track->plays, year_track->seconds);
        if ((++rows & 31) == 0)
            rb->yield();
    }
    rb->tagcache_search_finish(&search);
    sw_finish_totals(&this_year);
    sw_finish_totals(&all_time);
    sw_collect_art_sources();
    if (art_track_path[SW_ART_SONG][0])
        rb->strlcpy(preview_track_path, art_track_path[SW_ART_SONG],
                    sizeof(preview_track_path));
    sw_load_artwork();
    return true;
}

static int sw_split_tabs(char *line, char **parts, int maximum)
{
    int count = 0;
    char *cursor = line;
    while (count < maximum && cursor)
    {
        char *tab = rb->strchr(cursor, '\t');
        parts[count++] = cursor;
        if (!tab)
            break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static bool sw_load_snapshot(void)
{
    char line[MAX_PATH + 256];
    char *parts[6];
    int fd = rb->open(SW_DATA, O_RDONLY);
    bool valid = false;

    if (fd < 0)
        return false;
    rb->memset(&all_time, 0, sizeof(all_time));
    rb->memset(art_track_path, 0, sizeof(art_track_path));
    rb->memset(art_track_weight, 0, sizeof(art_track_weight));
    unique_artists = unique_albums = unique_genres = 0;
    lifetime_artist_name[0] = '\0';
    preview_track_path[0] = '\0';
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        int count;
        if (!line[0] || line[0] == '#')
            continue;
        count = sw_split_tabs(line, parts, ARRAYLEN(parts));
        if (count < 2)
            continue;
        if (!sw_casecmp(parts[0], "year"))
            current_year = rb->strtoul(parts[1], NULL, 10);
        else if (!sw_casecmp(parts[0], "scope"))
            lifetime_scope = !sw_casecmp(parts[1], "lifetime");
        else if (!sw_casecmp(parts[0], "all_time_artist"))
            sw_copy(lifetime_artist_name, sizeof(lifetime_artist_name),
                    parts[1], "");
        else if (!sw_casecmp(parts[0], "plays"))
            all_time.plays = rb->strtoul(parts[1], NULL, 10);
        else if (!sw_casecmp(parts[0], "seconds"))
            all_time.seconds = rb->strtoul(parts[1], NULL, 10);
        else if (!sw_casecmp(parts[0], "unique_tracks"))
            all_time.unique_tracks = rb->strtoul(parts[1], NULL, 10);
        else if (!sw_casecmp(parts[0], "unique_artists"))
            unique_artists = rb->strtoul(parts[1], NULL, 10);
        else if (!sw_casecmp(parts[0], "unique_albums"))
            unique_albums = rb->strtoul(parts[1], NULL, 10);
        else if (!sw_casecmp(parts[0], "unique_genres"))
            unique_genres = rb->strtoul(parts[1], NULL, 10);
        else if (!sw_casecmp(parts[0], "top_song_path"))
            sw_copy(preview_track_path, sizeof(preview_track_path),
                    parts[1], "");
        else if (count >= 6 &&
                 (!sw_casecmp(parts[0], "song") ||
                  !sw_casecmp(parts[0], "artist") ||
                  !sw_casecmp(parts[0], "album") ||
                  !sw_casecmp(parts[0], "genre")))
        {
            struct sw_rank *row = NULL;
            int *row_count = NULL;
            enum sw_art_slot slot = SW_ART_SONG;
            if (!sw_casecmp(parts[0], "song"))
            {
                row = all_time.songs;
                row_count = &all_time.song_count;
                slot = SW_ART_SONG;
            }
            else if (!sw_casecmp(parts[0], "artist"))
            {
                row = all_time.artists;
                row_count = &all_time.artist_count;
                slot = SW_ART_ARTIST;
            }
            else if (!sw_casecmp(parts[0], "album"))
            {
                row = all_time.albums;
                row_count = &all_time.album_count;
                slot = SW_ART_ALBUM;
            }
            else
            {
                row = all_time.genres;
                row_count = &all_time.genre_count;
            }
            if (*row_count < SW_TOP_COUNT)
            {
                struct sw_rank *target = &row[(*row_count)++];
                rb->memset(target, 0, sizeof(*target));
                sw_copy(target->name, sizeof(target->name), parts[1], "Unknown");
                sw_copy(target->detail, sizeof(target->detail), parts[2], "");
                target->plays = rb->strtoul(parts[3], NULL, 10);
                target->seconds = rb->strtoul(parts[4], NULL, 10);
                if (*row_count == 1 && parts[5][0] &&
                    sw_casecmp(parts[0], "genre"))
                    sw_consider_art(slot, parts[5], target->plays);
            }
        }
    }
    rb->close(fd);
    valid = all_time.plays > 0 && all_time.song_count > 0 &&
            all_time.artist_count > 0;
    if (valid)
    {
        if (art_track_path[SW_ART_ARTIST][0])
            rb->strlcpy(art_track_path[SW_ART_LIFETIME_ARTIST],
                        art_track_path[SW_ART_ARTIST],
                        sizeof(art_track_path[SW_ART_LIFETIME_ARTIST]));
        sw_load_artwork();
    }
    return valid;
}

static void sw_load_message(void)
{
    char line[MAX_PATH + 192];
    char *parts[4];
    int fd = rb->open(SW_MESSAGE, O_RDONLY);
    if (fd < 0 || rb->read_line(fd, line, sizeof(line)) <= 0)
    {
        if (fd >= 0) rb->close(fd);
        return;
    }
    rb->close(fd);
    if (sw_split_tabs(line, parts, ARRAYLEN(parts)) != 4 ||
        !all_time.artist_count ||
        sw_casecmp(parts[0], lifetime_artist_name[0] ?
                   lifetime_artist_name : all_time.artists[0].name) ||
        !rb->file_exists(parts[2]))
        return;
    sw_copy(message_artist, sizeof(message_artist), parts[0], "");
    sw_copy(message_title, sizeof(message_title), parts[1], "Artist Message");
    sw_copy(message_path, sizeof(message_path), parts[2], "");
    message_ready = true;
}

static void sw_write_summary(void)
{
    int fd;
    rb->mkdir(SW_ROOT);
    fd = rb->open(SW_SUMMARY_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "all_time_top_artist\t%s\n",
                 all_time.artist_count ? all_time.artists[0].name : "");
    rb->fdprintf(fd, "all_time_top_song\t%s\n",
                 all_time.song_count ? all_time.songs[0].name : "");
    rb->fdprintf(fd, "this_year\t%d\n", current_year);
    rb->close(fd);
    rb->remove(SW_SUMMARY);
    rb->rename(SW_SUMMARY_TMP, SW_SUMMARY);
}

static void sw_puts_fit(int x, int y, int width, const char *text)
{
    char buffer[SW_NAME + SW_DETAIL + 8];
    int text_width, text_height, length;
    rb->strlcpy(buffer, text ? text : "", sizeof(buffer));
    rb->lcd_getstringsize(buffer, &text_width, &text_height);
    length = rb->strlen(buffer);
    while (text_width > width && length > 3)
    {
        buffer[--length] = '\0';
        rb->lcd_getstringsize(buffer, &text_width, &text_height);
    }
    if (text && rb->strlen(text) > rb->strlen(buffer) && length >= 3)
        rb->strcpy(buffer + length - 3, "...");
    rb->lcd_putsxy(x, y, buffer);
}

static int sw_animation_phase(void)
{
    long elapsed = *rb->current_tick - animation_start;
    return MAX(0, MIN(12, elapsed * 24 / HZ));
}

static void sw_load_brand_assets(void)
{
    rb->memset(&brand_bitmap, 0, sizeof(brand_bitmap));
    brand_bitmap.data = (unsigned char *)brand_pixels;
    brand_valid =
        rb->read_bmp_file(SW_BRAND_MARK, &brand_bitmap,
                          sizeof(brand_pixels), FORMAT_NATIVE, NULL) > 0 &&
        brand_bitmap.width == 24 && brand_bitmap.height == 24;
}

static void sw_draw_art(enum sw_art_slot slot, int x, int y,
                        unsigned foreground, unsigned accent)
{
    rb->lcd_set_foreground(SW_BLACK);
    rb->lcd_fillrect(x + 5, y + 5, SW_ART_SIZE, SW_ART_SIZE);
    rb->lcd_set_foreground(accent);
    rb->lcd_fillrect(x - 3, y - 3, SW_ART_SIZE + 6, SW_ART_SIZE + 6);
    if (art_valid[slot])
        rb->lcd_bitmap((const fb_data *)art_bitmap[slot].data, x, y,
                       art_bitmap[slot].width, art_bitmap[slot].height);
    else
    {
        rb->lcd_set_foreground(SW_BLACK);
        rb->lcd_fillrect(x, y, SW_ART_SIZE, SW_ART_SIZE);
        rb->lcd_set_foreground(foreground);
        rb->lcd_putsxy(x + 18, y + 38, "NO COVER");
        rb->lcd_putsxy(x + 25, y + 55, "FOUND");
    }
}

static void sw_set_font(int font)
{
    rb->lcd_setfont(font >= 0 ? font : FONT_UI);
}

static void sw_puts_center(int y, const char *text)
{
    int width, height;
    rb->lcd_getstringsize(text, &width, &height);
    rb->lcd_putsxy(MAX(4, (LCD_WIDTH - width) / 2), y, text);
}

static void sw_puts_center_fit(int y, int width, const char *text)
{
    char buffer[SW_NAME + SW_DETAIL + 8];
    int text_width, text_height, length;
    rb->strlcpy(buffer, text ? text : "", sizeof(buffer));
    rb->lcd_getstringsize(buffer, &text_width, &text_height);
    length = rb->strlen(buffer);
    while (text_width > width && length > 3)
    {
        buffer[--length] = '\0';
        rb->lcd_getstringsize(buffer, &text_width, &text_height);
    }
    if (text && rb->strlen(text) > rb->strlen(buffer) && length >= 3)
        rb->strcpy(buffer + length - 3, "...");
    sw_puts_center(y, buffer);
}

static void sw_draw_pixel_edges(unsigned background, unsigned accent)
{
    int phase = sw_animation_phase() / 3;
    rb->lcd_set_foreground(accent);
    rb->lcd_fillrect(0, 32, 18 + phase * 3, 18);
    rb->lcd_fillrect(0, 50, 10 + phase * 2, 18);
    rb->lcd_fillrect(LCD_WIDTH - 24 - phase * 2, 32, 24 + phase * 2, 18);
    rb->lcd_fillrect(LCD_WIDTH - 14 - phase * 2, 50, 14 + phase * 2, 18);
    rb->lcd_fillrect(0, 218, 45 + phase * 4, 22);
    rb->lcd_fillrect(LCD_WIDTH - 54 - phase * 3, 218,
                     54 + phase * 3, 22);
    rb->lcd_set_foreground(background);
    rb->lcd_fillrect(0, 32, 7, 7);
    rb->lcd_fillrect(LCD_WIDTH - 7, 43, 7, 7);
}

static void sw_draw_card_header(unsigned background, unsigned foreground,
                                unsigned accent, const char *section)
{
    int count = sw_tab_count();
    int gap = 2;
    int width = (LCD_WIDTH - 12 - (count - 1) * gap) / count;
    int i;

    rb->lcd_set_background(background);
    rb->lcd_clear_display();
    sw_draw_pixel_edges(background, accent);
    for (i = 0; i < count; i++)
    {
        rb->lcd_set_foreground(i == selected_tab ? foreground : accent);
        rb->lcd_fillrect(6 + i * (width + gap), 5, width, 3);
    }
    if (brand_valid)
        rb->lcd_bitmap((const fb_data *)brand_bitmap.data, 8, 12, 24, 24);
    sw_set_font(font_body);
    rb->lcd_set_foreground(foreground);
    rb->lcd_putsxy(38, 17, "WRAPPED");
    sw_puts_fit(226, 17, 86, section);
}

static void sw_draw_card_footer(unsigned foreground)
{
    sw_set_font(font_body);
    rb->lcd_set_foreground(foreground);
    rb->lcd_putsxy(143, 220, "MENU");
}

static void sw_draw_intro_card(void)
{
    char period[16];
    sw_draw_card_header(SW_SKY, SW_BLACK, SW_PINK, "YOUR STORY");
    sw_set_font(font_heading);
    rb->lcd_set_foreground(SW_BLACK);
    sw_puts_center(53, "THIS IS YOUR");
    sw_set_font(font_big);
    sw_puts_center(82, "IPOD");
    sw_puts_center(119, "WRAPPED");
    sw_set_font(font_heading);
    if (lifetime_scope)
        rb->strlcpy(period, "ALL-TIME", sizeof(period));
    else
        rb->snprintf(period, sizeof(period), "%d", current_year);
    sw_puts_center(169, period);
    sw_set_font(font_body);
    sw_puts_center(195, "YOUR LISTENING. YOUR LIBRARY.");
    sw_draw_card_footer(SW_BLACK);
}

static void sw_draw_minutes_card(void)
{
    char value[32], detail[64];
    unsigned long minutes = all_time.seconds / 60;
    unsigned long hours = all_time.seconds / 3600;
    sw_draw_card_header(SW_CORAL, SW_BLACK, SW_VIOLET, "MINUTES");
    sw_set_font(font_heading);
    rb->lcd_set_foreground(SW_BLACK);
    sw_puts_center(51, "YOU LISTENED FOR");
    sw_set_font(font_big);
    rb->snprintf(value, sizeof(value), "%lu", minutes);
    sw_puts_center(83, value);
    sw_set_font(font_heading);
    sw_puts_center(126, "MINUTES");
    sw_set_font(font_body);
    rb->snprintf(detail, sizeof(detail), "%lu HOURS / %lu PLAYS",
                 hours, all_time.plays);
    sw_puts_center(164, detail);
    sw_puts_center(186, "EVERY PLAY COUNTED ON THIS IPOD");
    sw_draw_card_footer(SW_BLACK);
}

static void sw_draw_hero_card(const char *section, const char *eyebrow,
                              struct sw_rank *row, enum sw_art_slot slot,
                              unsigned background, unsigned accent,
                              bool show_minutes)
{
    char detail[64];
    sw_draw_card_header(background, SW_BLACK, accent, section);
    sw_set_font(font_body);
    rb->lcd_set_foreground(SW_BLACK);
    sw_puts_center(42, eyebrow);
    sw_draw_art(slot, 108, 62, SW_CREAM, SW_BLACK);
    sw_set_font(font_heading);
    rb->lcd_set_foreground(SW_BLACK);
    sw_puts_center_fit(173, 300, row ? row->name : "NO LISTENS YET");
    sw_set_font(font_body);
    if (row)
    {
        if (row->detail[0])
            sw_puts_center_fit(194, 294, row->detail);
        if (show_minutes)
            rb->snprintf(detail, sizeof(detail), "%lu MINUTES / %lu PLAYS",
                         row->seconds / 60, row->plays);
        else
            rb->snprintf(detail, sizeof(detail), "%lu PLAYS", row->plays);
        sw_puts_center(208, detail);
    }
}

static void sw_draw_list_card(const char *section, const char *title,
                              struct sw_rank *rows, int count,
                              unsigned background, unsigned accent)
{
    int i;
    char number[8], plays[24];
    unsigned foreground = background == SW_VIOLET ? SW_CREAM : SW_BLACK;
    sw_draw_card_header(background, foreground, accent, section);
    sw_set_font(font_heading);
    rb->lcd_set_foreground(foreground);
    sw_puts_center(42, title);
    sw_set_font(font_body);
    for (i = 0; i < count && i < SW_TOP_COUNT; i++)
    {
        int y = 72 + i * 28;
        rb->lcd_set_foreground(foreground);
        rb->snprintf(number, sizeof(number), "%d", i + 1);
        rb->lcd_putsxy(18, y, number);
        sw_puts_fit(42, y, 210, rows[i].name);
        rb->snprintf(plays, sizeof(plays), "%lu", rows[i].plays);
        sw_puts_fit(270, y, 42, plays);
        rb->lcd_fillrect(42, y + 20, 268, 1);
    }
    sw_draw_card_footer(foreground);
}

static void sw_draw_breadth_card(void)
{
    char value[24];
    sw_draw_card_header(SW_CORAL, SW_BLACK, SW_BRIGHT_YELLOW, "YOUR MIX");
    sw_set_font(font_heading);
    rb->lcd_set_foreground(SW_BLACK);
    sw_puts_center(47, "YOU KEPT IT INTERESTING");
    sw_set_font(font_big);
    rb->snprintf(value, sizeof(value), "%lu", all_time.unique_tracks);
    rb->lcd_putsxy(27, 91, value);
    rb->snprintf(value, sizeof(value), "%lu", unique_artists);
    sw_puts_center(91, value);
    rb->snprintf(value, sizeof(value), "%lu", unique_albums);
    {
        int width, height;
        rb->lcd_getstringsize(value, &width, &height);
        rb->lcd_putsxy(292 - width, 91, value);
    }
    sw_set_font(font_body);
    rb->lcd_putsxy(18, 137, "TRACKS");
    sw_puts_center(137, "ARTISTS");
    rb->lcd_putsxy(247, 137, "ALBUMS");
    rb->snprintf(value, sizeof(value), "%lu GENRES EXPLORED", unique_genres);
    sw_puts_center(177, value);
    sw_draw_card_footer(SW_BLACK);
}

static void sw_draw_personality_card(void)
{
    char line[96];
    unsigned long share = 0;
    const char *type = "THE EXPLORER";
    const char *tagline = "YOU KEPT YOUR SOUND WIDE OPEN";
    if (all_time.artist_count && all_time.plays)
        share = all_time.artists[0].plays * 100 / all_time.plays;
    if (share >= 70)
    {
        type = "THE LOYALIST";
        tagline = "YOU KNOW EXACTLY WHAT YOU LIKE";
    }
    else if (share >= 40)
    {
        type = "THE SUPERFAN";
        tagline = "FAVORITES HIT DIFFERENT FOR YOU";
    }
    sw_draw_card_header(SW_VIOLET, SW_CREAM, SW_CYAN, "YOUR TYPE");
    sw_set_font(font_heading);
    rb->lcd_set_foreground(SW_CREAM);
    sw_puts_center(48, "YOUR LISTENING TYPE");
    sw_set_font(font_big);
    sw_puts_center_fit(86, 304, type);
    sw_set_font(font_heading);
    rb->snprintf(line, sizeof(line), "%lu%% OF YOUR PLAYS", share);
    sw_puts_center(139, line);
    sw_set_font(font_body);
    if (all_time.artist_count)
    {
        rb->snprintf(line, sizeof(line), "WENT TO %s", all_time.artists[0].name);
        sw_puts_center_fit(166, 300, line);
    }
    sw_puts_center_fit(193, 300, tagline);
    sw_draw_card_footer(SW_CREAM);
}

static void sw_draw_artist_clip_card(void)
{
    sw_draw_card_header(SW_LIME, SW_BLACK, SW_GREEN, "ARTIST CLIP");
    sw_set_font(font_body);
    rb->lcd_set_foreground(SW_BLACK);
    sw_puts_center(42, "A MESSAGE FROM YOUR #1 ARTIST");
    sw_draw_art(SW_ART_LIFETIME_ARTIST, 108, 61, SW_CREAM, SW_BLACK);
    sw_set_font(font_heading);
    sw_puts_center_fit(171, 300, message_artist);
    sw_set_font(font_body);
    sw_puts_center_fit(192, 300, message_title);
    rb->lcd_set_foreground(SW_BLACK);
    rb->lcd_fillrect(96, 211, 128, 23);
    rb->lcd_set_foreground(SW_CREAM);
    sw_puts_center(215, "SELECT TO PLAY");
}

static void sw_draw_summary_card(void)
{
    sw_draw_card_header(SW_BRIGHT_YELLOW, SW_BLACK, SW_BLUE, "THE RECAP");
    sw_set_font(font_heading);
    rb->lcd_set_foreground(SW_BLACK);
    sw_puts_center(43, "YOUR IPOD ERA");
    sw_set_font(font_body);
    rb->lcd_putsxy(20, 78, "TOP ARTIST");
    sw_set_font(font_heading);
    sw_puts_fit(20, 96, 285, all_time.artist_count ?
                all_time.artists[0].name : "-");
    sw_set_font(font_body);
    rb->lcd_putsxy(20, 128, "TOP SONG");
    sw_set_font(font_heading);
    sw_puts_fit(20, 146, 285, all_time.song_count ?
                all_time.songs[0].name : "-");
    sw_set_font(font_body);
    rb->lcd_putsxy(20, 178, "TOP ALBUM");
    sw_set_font(font_heading);
    sw_puts_fit(20, 196, 285, all_time.album_count ?
                all_time.albums[0].name : "-");
    sw_draw_card_footer(SW_BLACK);
}

static void sw_draw(void)
{
    struct sw_totals *totals = this_year.plays ? &this_year : &all_time;
    rb->lcd_set_drawmode(DRMODE_SOLID);
    if (!database_ready)
    {
        sw_draw_card_header(SW_SKY, SW_BLACK, SW_PINK, "LOADING");
        sw_set_font(font_heading);
        rb->lcd_set_foreground(SW_BLACK);
        sw_puts_center(90, "YOUR LIBRARY IS GETTING READY");
        sw_set_font(font_body);
        sw_puts_center(125, "COME BACK IN A MOMENT");
    }
    else switch (selected_tab)
    {
        case SW_OVERVIEW: sw_draw_intro_card(); break;
        case SW_MINUTES: sw_draw_minutes_card(); break;
        case SW_TOP_SONG:
            sw_draw_hero_card("TOP SONG", "THE SONG YOU COULDN'T QUIT",
                totals->song_count ? &totals->songs[0] : NULL, SW_ART_SONG,
                SW_BRIGHT_YELLOW, SW_BLUE, false); break;
        case SW_SONGS:
            sw_draw_list_card("TOP SONGS", "YOUR TOP 5 SONGS",
                totals->songs, totals->song_count, SW_BRIGHT_YELLOW,
                SW_BLUE); break;
        case SW_TOP_ARTIST:
            sw_draw_hero_card("TOP ARTIST", "THEY RAN YOUR YEAR",
                totals->artist_count ? &totals->artists[0] : NULL,
                SW_ART_ARTIST, SW_LIME, SW_GREEN, true); break;
        case SW_ARTISTS:
            sw_draw_list_card("TOP ARTISTS", "YOUR TOP 5 ARTISTS",
                totals->artists, totals->artist_count, SW_LIME,
                SW_GREEN); break;
        case SW_TOP_ALBUM:
            sw_draw_hero_card("TOP ALBUM", "YOUR #1 ALBUM",
                totals->album_count ? &totals->albums[0] : NULL,
                SW_ART_ALBUM, SW_SKY, SW_PINK, true); break;
        case SW_ALBUMS:
            sw_draw_list_card("TOP ALBUMS", "YOUR TOP 5 ALBUMS",
                totals->albums, totals->album_count, SW_SKY, SW_PINK); break;
        case SW_GENRES:
            sw_draw_list_card("TOP GENRES", "YOUR SOUND, RANKED",
                totals->genres, totals->genre_count, SW_VIOLET,
                SW_CYAN); break;
        case SW_LISTENING: sw_draw_breadth_card(); break;
        case SW_PERSONALITY: sw_draw_personality_card(); break;
        case SW_ALL_TIME: sw_draw_summary_card(); break;
        case SW_MESSAGE_TAB: sw_draw_artist_clip_card(); break;
    }
    rb->lcd_update();
}

static bool sw_path_openable(const char *path)
{
    int fd;

    if (!path || !path[0])
        return false;
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    rb->close(fd);
    return true;
}

static bool sw_current_is(const char *path)
{
    struct mp3entry *id3 = rb->audio_current_track();

    return id3 && path && path[0] && !rb->strcmp(id3->path, path);
}

static bool sw_wait_until_current(const char *path)
{
    long deadline = *rb->current_tick + SW_AUDIO_WAIT;

    do
    {
        struct mp3entry *id3 = rb->audio_current_track();
        if ((rb->audio_status() & AUDIO_STATUS_PLAY) && id3 &&
            id3->elapsed > 0 && path && !rb->strcmp(id3->path, path))
            return true;
        rb->sleep(1);
    }
    while (TIME_BEFORE(*rb->current_tick, deadline));
    return false;
}

static int sw_preview_volume(void)
{
    int volume = SW_SAFE_VOLUME;
    int minimum = rb->sound_min(SOUND_VOLUME);
    int maximum = rb->sound_max(SOUND_VOLUME);

    volume = MAX(minimum, MIN(maximum, volume));
    if (rb->global_settings->volume_limit >= minimum &&
        rb->global_settings->volume_limit < volume)
        volume = rb->global_settings->volume_limit;
    return volume;
}

static void sw_set_volume(int volume)
{
    rb->global_status->volume = volume;
    rb->sound_set(SOUND_VOLUME, volume);
}

static void sw_preview_restore(void)
{
    int retries = 2;

    if (!preview.active)
        return;

    if (preview.inserted)
    {
        if (preview.saved_amount == 0)
        {
            rb->audio_stop();
            rb->playlist_create(NULL, NULL);
        }
        while (rb->playlist_amount() > preview.saved_amount &&
               retries-- > 0)
        {
            if (!sw_current_is(preview_track_path))
                rb->playlist_start(preview.saved_index < 0 ? 0 :
                                   preview.saved_index, 0, 0);
            else
                rb->audio_next();

            long deadline = *rb->current_tick + SW_AUDIO_WAIT;
            while (rb->playlist_amount() > preview.saved_amount &&
                   TIME_BEFORE(*rb->current_tick, deadline))
                rb->sleep(1);
        }
    }

    /* End the temporary decode while history gathering is still disabled.
     * Starting the user's saved track below gives tagcache a clean buffer
     * event after the normal settings have been restored. */
    rb->audio_stop();
    rb->global_settings->repeat_mode = preview.saved_repeat;
    sw_set_volume(preview.saved_volume);

    if (preview.had_playback && preview.saved_index >= 0)
    {
        rb->global_settings->runtimedb = preview.saved_runtimedb;
        rb->global_settings->playback_log = preview.saved_playback_log;
        rb->global_settings->autoresume_enable = preview.saved_autoresume;
        rb->playlist_start(preview.saved_index, preview.saved_elapsed,
                           preview.saved_offset);
        sw_wait_until_current(preview.saved_path);
        if (preview.was_paused)
            rb->audio_pause();
    }
    else
    {
        if (preview.saved_amount > 0 && preview.saved_index >= 0)
        {
            rb->playlist_start(preview.saved_index, 0, 0);
            rb->audio_stop();
        }
        rb->global_settings->runtimedb = preview.saved_runtimedb;
        rb->global_settings->playback_log = preview.saved_playback_log;
        rb->global_settings->autoresume_enable = preview.saved_autoresume;
    }

    DEBUGF("spotify_wrapped: preview restored amount=%d volume=%d\n",
           rb->playlist_amount(), preview.saved_volume);
    rb->memset(&preview, 0, sizeof(preview));
}

static bool sw_preview_start(void)
{
    struct mp3entry *id3;
    int status = rb->audio_status();
    int safe_volume;

    rb->memset(&preview, 0, sizeof(preview));
    preview.saved_index = -1;
    if (!sw_path_openable(preview_track_path))
        return false;

    preview.had_playback = (status & AUDIO_STATUS_PLAY) != 0;
    preview.was_paused = (status & AUDIO_STATUS_PAUSE) != 0;
    preview.saved_amount = rb->playlist_amount();
    preview.saved_volume = rb->global_status->volume;
    preview.saved_repeat = rb->global_settings->repeat_mode;
    preview.saved_runtimedb = rb->global_settings->runtimedb;
    preview.saved_playback_log = rb->global_settings->playback_log;
    preview.saved_autoresume = rb->global_settings->autoresume_enable;
    rb->playlist_get_resume_info(&preview.saved_index);
    id3 = rb->audio_current_track();
    if (preview.had_playback && id3)
    {
        preview.saved_elapsed = id3->elapsed;
        preview.saved_offset = id3->offset;
        rb->strlcpy(preview.saved_path, id3->path,
                    sizeof(preview.saved_path));
    }

    if (!preview.had_playback || !sw_current_is(preview_track_path))
    {
        int index = rb->playlist_insert_track(NULL, preview_track_path,
                                              PLAYLIST_INSERT_FIRST,
                                              true, true);
        if (index < 0)
            return false;
        preview.inserted = true;
        preview.active = true;
        rb->global_settings->runtimedb = false;
        rb->global_settings->playback_log = false;
        rb->global_settings->autoresume_enable = false;
        rb->global_settings->repeat_mode = REPEAT_ONE;
        safe_volume = sw_preview_volume();
        if (preview.saved_volume > safe_volume)
            sw_set_volume(safe_volume);
        rb->playlist_start(index, 0, 0);
    }
    else
    {
        preview.active = true;
        rb->global_settings->runtimedb = false;
        rb->global_settings->playback_log = false;
        rb->global_settings->autoresume_enable = false;
        rb->global_settings->repeat_mode = REPEAT_ONE;
        safe_volume = sw_preview_volume();
        if (preview.saved_volume > safe_volume)
            sw_set_volume(safe_volume);
        rb->playlist_start(preview.saved_index, 0, 0);
    }

    if (!sw_wait_until_current(preview_track_path))
    {
        sw_preview_restore();
        return false;
    }
    DEBUGF("spotify_wrapped: preview started path=%s volume=%d amount=%d\n",
           preview_track_path, rb->global_status->volume,
           rb->playlist_amount());
    return true;
}

static void sw_load_fonts(bool playback_active)
{
    font_body = font_heading = font_big = FONT_UI;
    if (playback_active)
        return;
    font_body = rb->font_load(ROCKBOX_DIR "/fonts/14-Adobe-Helvetica-Bold.fnt");
    font_heading = rb->font_load(ROCKBOX_DIR "/fonts/18-Cantarell-Bold.fnt");
    font_big = rb->font_load(ROCKBOX_DIR "/fonts/35-Adobe-Helvetica-Bold.fnt");
    if (font_body < 0) font_body = FONT_UI;
    if (font_heading < 0) font_heading = font_body;
    if (font_big < 0) font_big = font_heading;
}

static void sw_unload_fonts(void)
{
    rb->lcd_setfont(FONT_UI);
    if (font_big != FONT_UI && font_big != font_heading && font_big != font_body)
        rb->font_unload(font_big);
    if (font_heading != FONT_UI && font_heading != font_body)
        rb->font_unload(font_heading);
    if (font_body != FONT_UI)
        rb->font_unload(font_body);
}

static enum plugin_status sw_play_message(void)
{
    static char launch[MAX_PATH + 24];
    if (!message_ready)
        return PLUGIN_OK;
    rb->snprintf(launch, sizeof(launch), "spotify-wrapped:%s", message_path);
    return rb->plugin_open(SW_PLAYER, launch);
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status result = PLUGIN_OK;
    bool redraw = true;
    bool playback_active = (rb->audio_status() & AUDIO_STATUS_PLAY) != 0;
    (void)parameter;
    rb->memset(&this_year, 0, sizeof(this_year));
    rb->memset(&all_time, 0, sizeof(all_time));
    log_track_count = 0;
    log_overflow = false;
    snapshot_ready = false;
    lifetime_scope = false;
    message_ready = false;
    preview_track_path[0] = '\0';
    current_year = rb->get_time()->tm_year + 1900;
    selected_tab = SW_OVERVIEW;
    selected_row = 0;
    animation_start = *rb->current_tick;
    rb->strlcpy(status_line, "Wheel: browse   Menu: back", sizeof(status_line));
    sw_load_fonts(playback_active);
    sw_set_font(font_body);
    rb->lcd_set_backdrop(NULL);
    rb->splash(0, "Reading your listening history...");
    sw_load_brand_assets();
    snapshot_ready = sw_load_snapshot();
    if (snapshot_ready)
        database_ready = true;
    else
    {
        sw_read_playback_logs();
        database_ready = sw_scan_tagcache();
    }
    if (database_ready)
    {
        if (!snapshot_ready)
            sw_write_summary();
        sw_load_message();
        if (log_overflow)
            rb->strlcpy(status_line, "Large log: oldest unique tracks omitted", sizeof(status_line));
        else if (sw_preview_start())
            rb->strlcpy(status_line, "Top song playing   Menu: back",
                        sizeof(status_line));
    }
    while (result == PLUGIN_OK)
    {
        int action;
        if (redraw)
        {
            sw_draw();
            redraw = false;
        }
        action = pluginlib_getaction(HZ / 5, sw_contexts, ARRAYLEN(sw_contexts));
        switch (action)
        {
            case PLA_LEFT:
                selected_tab = (selected_tab + sw_tab_count() - 1) % sw_tab_count();
                selected_row = 0;
                animation_start = *rb->current_tick;
                redraw = true; break;
            case PLA_RIGHT:
                selected_tab = (selected_tab + 1) % sw_tab_count();
                selected_row = 0;
                animation_start = *rb->current_tick;
                redraw = true; break;
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
            case PLA_UP:
            case PLA_UP_REPEAT:
                if (selected_row > 0) selected_row--;
                redraw = true; break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
            case PLA_DOWN:
            case PLA_DOWN_REPEAT:
                if (selected_row < SW_TOP_COUNT - 1) selected_row++;
                redraw = true; break;
            case PLA_SELECT:
            case PLA_SELECT_REL:
                if (selected_tab == SW_MESSAGE_TAB)
                {
                    sw_preview_restore();
                    result = sw_play_message();
                    if (result == PLUGIN_OK)
                        sw_preview_start();
                    redraw = true;
                }
                break;
            case PLA_CANCEL:
            case PLA_EXIT:
                result = PLUGIN_OK;
                goto cleanup;
            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    result = PLUGIN_USB_CONNECTED;
                    goto cleanup;
                }
                break;
        }
        if (sw_animation_phase() < 12)
            redraw = true;
    }
cleanup:
    sw_preview_restore();
    sw_unload_fonts();
    return result;
}
