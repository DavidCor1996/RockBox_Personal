/***************************************************************************
 * iPod Hero chart/index loading
 ****************************************************************************/

#include "ipodhero.h"

#include <fcntl.h>

#define IH_HEADER_BYTES 40u
#define IH_EVENT_BYTES 12u
#define IH_SECTION_BYTES 8u
#define IH_INDEX_FIELDS 11

static uint16_t ih_u16le(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ih_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool ih_read_exact(int fd, void *data, size_t bytes)
{
    unsigned char *out = data;

    while (bytes > 0)
    {
        ssize_t got = rb->read(fd, out, bytes);
        if (got <= 0)
            return false;
        out += got;
        bytes -= (size_t)got;
    }
    return true;
}

void *ih_arena_alloc(struct ih_arena *arena, size_t bytes)
{
    size_t start = (arena->used + 3u) & ~(size_t)3u;

    if (bytes > arena->size || start > arena->size - bytes)
        return NULL;
    arena->used = start + bytes;
    return arena->base + start;
}

size_t ih_arena_mark(const struct ih_arena *arena)
{
    return arena->used;
}

void ih_arena_reset(struct ih_arena *arena, size_t mark)
{
    if (mark <= arena->used)
        arena->used = mark;
}

static int ih_split_tabs(char *line, char **field, int max_fields)
{
    int count = 0;
    char *cursor = line;

    while (count < max_fields)
    {
        char *tab;
        field[count++] = cursor;
        tab = rb->strchr(cursor, '\t');
        if (tab == NULL)
            break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static void ih_resolve_chart_path(char *out, size_t out_size,
                                  const char *path)
{
    if (path[0] == '/')
        rb->strlcpy(out, path, out_size);
    else if (path[0] != '\0')
        rb->snprintf(out, out_size, "%s/%s", IH_DATA_DIR, path);
    else
        out[0] = '\0';
}

static void ih_index_parse_entry(struct ih_index_entry *entry, char **field)
{
    rb->memset(entry, 0, sizeof(*entry));
    rb->strlcpy(entry->audio_path, field[0], sizeof(entry->audio_path));
    entry->audio_size = rb->strtoul(field[1], NULL, 10);
    entry->audio_length_ms = rb->strtoul(field[2], NULL, 10);
    entry->audio_crc32 = rb->strtoul(field[3], NULL, 16);
    rb->strlcpy(entry->title, field[4], sizeof(entry->title));
    rb->strlcpy(entry->artist, field[5], sizeof(entry->artist));
    ih_resolve_chart_path(entry->chart_path[IH_EASY], MAX_PATH, field[6]);
    ih_resolve_chart_path(entry->chart_path[IH_MEDIUM], MAX_PATH, field[7]);
    ih_resolve_chart_path(entry->chart_path[IH_HARD], MAX_PATH, field[8]);
    ih_resolve_chart_path(entry->chart_path[IH_EXPERT], MAX_PATH, field[9]);
    rb->strlcpy(entry->skin_id, field[10], sizeof(entry->skin_id));
}

static bool ih_index_entry_is_playable(struct ih_index_entry *entry)
{
    bool has_chart = false;
    int difficulty;

    if (!entry->audio_path[0] || !rb->file_exists(entry->audio_path))
        return false;
    for (difficulty = IH_EASY; difficulty <= IH_EXPERT; difficulty++)
    {
        if (entry->chart_path[difficulty][0] &&
            rb->file_exists(entry->chart_path[difficulty]))
            has_chart = true;
        else
            entry->chart_path[difficulty][0] = '\0';
    }
    return has_chart;
}

bool ih_index_load_library(struct ih_song_library *library,
                           struct ih_arena *arena,
                           char *error, size_t error_size)
{
    char line[1024];
    char *field[IH_INDEX_FIELDS];
    int fd;

    rb->memset(library, 0, sizeof(*library));
    library->entries = ih_arena_alloc(
        arena, IH_MAX_SONGS * sizeof(*library->entries));
    if (library->entries == NULL)
    {
        rb->snprintf(error, error_size, "Not enough memory for song library");
        return false;
    }
    fd = rb->open(IH_INDEX_FILE, O_RDONLY);
    if (fd < 0)
    {
        rb->snprintf(error, error_size, "Missing %s", IH_INDEX_FILE);
        return false;
    }

    while (library->count < (int)IH_MAX_SONGS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        int count;
        struct ih_index_entry *entry;

        if (line[0] == '\0' || line[0] == '#')
            continue;
        count = ih_split_tabs(line, field, ARRAYLEN(field));
        if (count != IH_INDEX_FIELDS)
            continue;
        entry = &library->entries[library->count];
        ih_index_parse_entry(entry, field);
        if (ih_index_entry_is_playable(entry))
            library->count++;
    }
    rb->close(fd);
    if (library->count == 0)
    {
        rb->snprintf(error, error_size,
                     "No installed songs have readable audio and charts");
        return false;
    }
    return true;
}

bool ih_index_find(const char *audio_path, struct ih_index_entry *entry,
                   char *error, size_t error_size)
{
    char line[1024];
    char *field[IH_INDEX_FIELDS];
    int fd;

    rb->memset(entry, 0, sizeof(*entry));
    fd = rb->open(IH_INDEX_FILE, O_RDONLY);
    if (fd < 0)
    {
        rb->snprintf(error, error_size, "Missing %s", IH_INDEX_FILE);
        return false;
    }

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        int count;
        if (line[0] == '\0' || line[0] == '#')
            continue;
        count = ih_split_tabs(line, field, ARRAYLEN(field));
        if (count != IH_INDEX_FIELDS || rb->strcmp(field[0], audio_path))
            continue;

        ih_index_parse_entry(entry, field);
        rb->close(fd);
        return true;
    }

    rb->close(fd);
    rb->snprintf(error, error_size,
                 "No chart matches\n%s\nRun tools/ipodhero_chart.py on a computer",
                 audio_path);
    return false;
}

bool ih_chart_load(struct ih_chart *chart, struct ih_arena *arena,
                   const char *path, char *error, size_t error_size)
{
    unsigned char header[IH_HEADER_BYTES];
    unsigned char raw[IH_EVENT_BYTES];
    uint32_t count;
    uint32_t section_count;
    uint32_t section_string_bytes;
    uint32_t expected_crc;
    uint32_t crc = 0xffffffffu;
    uint32_t previous_time = 0;
    uint32_t i;
    off_t file_size;
    int fd;

    rb->memset(chart, 0, sizeof(*chart));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        rb->snprintf(error, error_size, "Cannot open chart\n%s", path);
        return false;
    }
    file_size = rb->filesize(fd);
    if (file_size < (off_t)IH_HEADER_BYTES ||
        file_size > (off_t)IH_CHART_LIMIT ||
        !ih_read_exact(fd, header, sizeof(header)))
    {
        rb->close(fd);
        rb->snprintf(error, error_size, "Chart is truncated or oversized");
        return false;
    }

    count = ih_u32le(header + 8);
    section_count = ih_u32le(header + 12);
    if (rb->memcmp(header, "IHC1", 4) || ih_u16le(header + 6) != 0 ||
        ih_u16le(header + 4) != IH_HEADER_BYTES ||
        header[32] != IH_LANE_COUNT ||
        header[33] >= IH_DIFFICULTY_COUNT ||
        header[34] > IH_ORIGIN_GENERATED || header[35] != 0 ||
        count == 0 || count > IH_MAX_EVENTS ||
        section_count > IH_MAX_SECTIONS ||
        count > IH_CHART_LIMIT / sizeof(struct ih_note_event) ||
        section_count > IH_CHART_LIMIT / IH_SECTION_BYTES ||
        file_size < (off_t)(IH_HEADER_BYTES + count * IH_EVENT_BYTES +
                            section_count * IH_SECTION_BYTES))
    {
        rb->close(fd);
        rb->snprintf(error, error_size, "Unsupported or corrupt IHC1 chart");
        return false;
    }
    section_string_bytes = (uint32_t)file_size - IH_HEADER_BYTES -
                           count * IH_EVENT_BYTES -
                           section_count * IH_SECTION_BYTES;
    if ((section_count == 0 && section_string_bytes != 0) ||
        (section_count > 0 && section_string_bytes == 0))
    {
        rb->close(fd);
        rb->snprintf(error, error_size, "Invalid chart section table");
        return false;
    }

    chart->events = ih_arena_alloc(arena,
                         count * sizeof(struct ih_note_event));
    if (chart->events == NULL)
    {
        rb->close(fd);
        rb->snprintf(error, error_size, "Not enough chart memory");
        return false;
    }
    rb->memset(chart->events, 0, count * sizeof(struct ih_note_event));

    expected_crc = ih_u32le(header + 36);
    for (i = 0; i < count; ++i)
    {
        struct ih_note_event *event = &chart->events[i];
        if (!ih_read_exact(fd, raw, sizeof(raw)))
        {
            rb->close(fd);
            rb->snprintf(error, error_size, "Truncated chart payload");
            return false;
        }
        crc = rb->crc_32(raw, sizeof(raw), crc);
        event->time_ms = ih_u32le(raw);
        event->duration_ms = ih_u32le(raw + 4);
        event->lane_mask = raw[8];
        event->flags = raw[9];
        event->phrase_id = ih_u16le(raw + 10);
        if (event->lane_mask == 0 || (event->lane_mask & ~0x1fu) ||
            (event->flags & ~(IH_NOTE_HOPO | IH_NOTE_TAP |
                              IH_NOTE_STAR | IH_NOTE_PHRASE_END |
                              IH_NOTE_GENERATED | IH_NOTE_FORCED)) ||
            (i > 0 && event->time_ms < previous_time) ||
            event->time_ms > ih_u32le(header + 16) + IH_APPROACH_MS ||
            event->duration_ms > ih_u32le(header + 16) ||
            event->duration_ms > UINT32_MAX - event->time_ms ||
            event->time_ms + event->duration_ms >
                ih_u32le(header + 16) + IH_APPROACH_MS)
        {
            rb->close(fd);
            rb->snprintf(error, error_size, "Invalid note at event %lu",
                         (unsigned long)i);
            return false;
        }
        previous_time = event->time_ms;
    }
    if (section_count > 0)
    {
        uint32_t previous_section_time = 0;
        chart->sections = ih_arena_alloc(
            arena, section_count * sizeof(struct ih_section));
        chart->section_strings = ih_arena_alloc(arena, section_string_bytes);
        if (chart->sections == NULL || chart->section_strings == NULL)
        {
            rb->close(fd);
            rb->snprintf(error, error_size, "Not enough section memory");
            return false;
        }
        for (i = 0; i < section_count; ++i)
        {
            struct ih_section *section = &chart->sections[i];
            if (!ih_read_exact(fd, raw, IH_SECTION_BYTES))
            {
                rb->close(fd);
                rb->snprintf(error, error_size, "Truncated section table");
                return false;
            }
            crc = rb->crc_32(raw, IH_SECTION_BYTES, crc);
            section->time_ms = ih_u32le(raw);
            section->name_offset = ih_u32le(raw + 4);
            if ((i > 0 && section->time_ms < previous_section_time) ||
                section->time_ms > ih_u32le(header + 16) ||
                section->name_offset >= section_string_bytes)
            {
                rb->close(fd);
                rb->snprintf(error, error_size, "Invalid chart section");
                return false;
            }
            previous_section_time = section->time_ms;
        }
        if (!ih_read_exact(fd, chart->section_strings,
                           section_string_bytes))
        {
            rb->close(fd);
            rb->snprintf(error, error_size, "Truncated section names");
            return false;
        }
        crc = rb->crc_32(chart->section_strings, section_string_bytes, crc);
        for (i = 0; i < section_count; ++i)
            if (rb->memchr(chart->section_strings +
                           chart->sections[i].name_offset, '\0',
                           section_string_bytes -
                           chart->sections[i].name_offset) == NULL)
            {
                rb->close(fd);
                rb->snprintf(error, error_size,
                             "Unterminated chart section name");
                return false;
            }
    }
    rb->close(fd);

    if (crc != expected_crc)
    {
        rb->snprintf(error, error_size, "Chart payload CRC mismatch");
        return false;
    }

    chart->event_count = count;
    chart->section_count = section_count;
    chart->section_string_bytes = section_string_bytes;
    chart->song_length_ms = ih_u32le(header + 16);
    chart->audio_offset_ms = (int32_t)ih_u32le(header + 20);
    chart->difficulty = header[33];
    chart->origin = header[34];
    chart->payload_crc32 = expected_crc;
    if (chart->difficulty >= IH_DIFFICULTY_COUNT ||
        chart->origin > IH_ORIGIN_GENERATED ||
        chart->song_length_ms == 0)
    {
        rb->snprintf(error, error_size, "Invalid chart metadata");
        return false;
    }
    return true;
}
