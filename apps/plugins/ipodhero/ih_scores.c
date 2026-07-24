/***************************************************************************
 * iPod Hero versioned score persistence
 ****************************************************************************/

#include "ipodhero.h"

#include <fcntl.h>

#define IH_SCORE_BYTES 52u
#define IH_SCORE_LIMIT (256u * 1024u)
#define IH_SCORE_FLAG_PRACTICE 0x01
#define IH_SCORE_FLAG_NO_FAIL 0x02
#define IH_SCORE_FLAG_ASSIST 0x04

static uint16_t ih_score_u16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ih_score_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void ih_score_put_u16(unsigned char *p, uint16_t value)
{
    p[0] = value & 0xff;
    p[1] = value >> 8;
}

static void ih_score_put_u32(unsigned char *p, uint32_t value)
{
    p[0] = value & 0xff;
    p[1] = (value >> 8) & 0xff;
    p[2] = (value >> 16) & 0xff;
    p[3] = value >> 24;
}

static bool ih_score_read_exact(int fd, unsigned char *data, size_t bytes)
{
    while (bytes > 0)
    {
        ssize_t count = rb->read(fd, data, bytes);
        if (count <= 0)
            return false;
        data += count;
        bytes -= (size_t)count;
    }
    return true;
}

static bool ih_score_write_exact(int fd, const unsigned char *data,
                                 size_t bytes)
{
    while (bytes > 0)
    {
        ssize_t count = rb->write(fd, data, bytes);
        if (count <= 0)
            return false;
        data += count;
        bytes -= (size_t)count;
    }
    return true;
}

static bool ih_score_record_valid(const unsigned char *record)
{
    return !rb->memcmp(record, "IHS1", 4) &&
           ih_score_u16(record + 4) == 1 &&
           ih_score_u16(record + 6) == IH_SCORE_BYTES &&
           record[16] < IH_DIFFICULTY_COUNT &&
           (record[17] & ~(IH_SCORE_FLAG_PRACTICE |
                           IH_SCORE_FLAG_NO_FAIL |
                           IH_SCORE_FLAG_ASSIST)) == 0 &&
           ih_score_u32(record + 48) ==
               rb->crc_32(record, 48, 0xffffffffu);
}

static int ih_score_open_read(void)
{
    int fd = rb->open(IH_SCORE_FILE, O_RDONLY);

    if (fd < 0)
        fd = rb->open(IH_SCORE_BACKUP, O_RDONLY);
    return fd;
}

bool ih_scores_load_best(struct ih_app *app, char *error,
                         size_t error_size)
{
    unsigned char record[IH_SCORE_BYTES];
    int fd = ih_score_open_read();
    off_t size;

    app->best_score = 0;
    app->best_accuracy_bp = 0;
    app->scores_valid = true;
    if (fd < 0)
        return true;
    size = rb->filesize(fd);
    if (size < 0 || size > (off_t)IH_SCORE_LIMIT ||
        size % IH_SCORE_BYTES != 0)
        goto corrupt;
    while (ih_score_read_exact(fd, record, sizeof(record)))
    {
        uint32_t score;
        if (!ih_score_record_valid(record))
            goto corrupt;
        if (ih_score_u32(record + 8) != app->chart.payload_crc32 ||
            ih_score_u32(record + 12) != app->index.audio_crc32 ||
            record[16] != app->chart.difficulty || record[17] != 0)
            continue;
        score = ih_score_u32(record + 20);
        if (score >= app->best_score)
        {
            app->best_score = score;
            app->best_accuracy_bp = ih_score_u32(record + 36);
        }
    }
    rb->close(fd);
    return true;

corrupt:
    rb->close(fd);
    app->scores_valid = false;
    rb->snprintf(error, error_size,
                 "Score file is corrupt; it was not modified");
    return false;
}

static void ih_score_make_record(const struct ih_app *app,
                                 unsigned char *record)
{
    const struct ih_score *score = &app->game.score;
    uint32_t total = score->hit_count + score->miss_count;
    uint8_t flags = 0;

    rb->memset(record, 0, IH_SCORE_BYTES);
    rb->memcpy(record, "IHS1", 4);
    ih_score_put_u16(record + 4, 1);
    ih_score_put_u16(record + 6, IH_SCORE_BYTES);
    ih_score_put_u32(record + 8, app->chart.payload_crc32);
    ih_score_put_u32(record + 12, app->index.audio_crc32);
    record[16] = app->chart.difficulty;
    if (app->last_practice)
        flags |= IH_SCORE_FLAG_PRACTICE;
    if (app->game.no_fail)
        flags |= IH_SCORE_FLAG_NO_FAIL;
    if (app->game.assist)
        flags |= IH_SCORE_FLAG_ASSIST;
    record[17] = flags;
    ih_score_put_u32(record + 20, score->points);
    ih_score_put_u32(record + 24, score->hit_count);
    ih_score_put_u32(record + 28, score->miss_count);
    ih_score_put_u32(record + 32, score->max_streak);
    ih_score_put_u32(record + 36,
                     total ? score->hit_count * 10000u / total : 0);
    ih_score_put_u32(record + 40,
                     (uint32_t)rb->mktime(rb->get_time()));
    ih_score_put_u32(record + 44,
                     (uint32_t)app->game.calibration_ms);
    ih_score_put_u32(record + 48,
                     rb->crc_32(record, 48, 0xffffffffu));
}

bool ih_scores_save(const struct ih_app *app, char *error,
                    size_t error_size)
{
    unsigned char record[IH_SCORE_BYTES];
    char temporary[MAX_PATH];
    int source = ih_score_open_read();
    int output;
    bool ok = true;

    if (!app->scores_valid)
    {
        rb->snprintf(error, error_size, "Scores are read-only after corruption");
        if (source >= 0)
            rb->close(source);
        return false;
    }
    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", IH_SCORE_FILE);
    output = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output < 0)
    {
        if (source >= 0)
            rb->close(source);
        rb->snprintf(error, error_size, "Cannot create score file");
        return false;
    }
    if (source >= 0)
    {
        while (ih_score_read_exact(source, record, sizeof(record)))
        {
            if (!ih_score_record_valid(record) ||
                !ih_score_write_exact(output, record, sizeof(record)))
            {
                ok = false;
                break;
            }
        }
        rb->close(source);
    }
    ih_score_make_record(app, record);
    if (ok)
        ok = ih_score_write_exact(output, record, sizeof(record));
    rb->close(output);
    if (!ok)
    {
        rb->remove(temporary);
        rb->snprintf(error, error_size, "Could not write complete score data");
        return false;
    }

    rb->remove(IH_SCORE_BACKUP);
    if (rb->file_exists(IH_SCORE_FILE) &&
        rb->rename(IH_SCORE_FILE, IH_SCORE_BACKUP) < 0)
    {
        rb->remove(temporary);
        rb->snprintf(error, error_size, "Could not stage old score data");
        return false;
    }
    if (rb->rename(temporary, IH_SCORE_FILE) < 0)
    {
        rb->rename(IH_SCORE_BACKUP, IH_SCORE_FILE);
        rb->snprintf(error, error_size, "Could not commit score data");
        return false;
    }
    rb->remove(IH_SCORE_BACKUP);
    return true;
}
