/***************************************************************************
 * Shared hardware H.264 resume records. Positions and totals use samples on
 * iPod 6G and milliseconds on iPod Video; the browser reads their ratio.
 ****************************************************************************/
#ifndef VIDEO_RESUME_H
#define VIDEO_RESUME_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "crc32.h"
#include "dir.h"
#include "file.h"
#include "rbpaths.h"

#define VIDEO_RESUME_MAGIC 0x52565031u
#define VIDEO_RESUME_FILE PLUGIN_APPS_DATA_DIR "/openh264-resume-%08lx.dat"

struct video_resume_record
{
    uint32_t magic;
    uint32_t path_crc;
    int32_t frame;
    int32_t total_frames;
};

static void video_resume_filename(const char *path, char *filename,
                                  size_t size, uint32_t *crc_out)
{
    uint32_t crc = crc_32(path, strlen(path), 0xffffffff);

    snprintf(filename, size, VIDEO_RESUME_FILE, (unsigned long)crc);
    if (crc_out != NULL)
        *crc_out = crc;
}

static uint32_t video_resume_load(const char *path, uint32_t total_samples)
{
    struct video_resume_record record;
    char filename[MAX_PATH];
    uint32_t crc;
    int fd;

    video_resume_filename(path, filename, sizeof(filename), &crc);
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return 0;
    if (read(fd, &record, sizeof(record)) != sizeof(record))
    {
        close(fd);
        return 0;
    }
    close(fd);
    if (record.magic != VIDEO_RESUME_MAGIC || record.path_crc != crc ||
        record.total_frames != (int32_t)total_samples || record.frame <= 0 ||
        record.frame >= (int32_t)((uint64_t)total_samples * 95u / 100u))
        return 0;
    return (uint32_t)record.frame;
}

static void video_resume_save(const char *path, uint32_t sample,
                              uint32_t total_samples)
{
    struct video_resume_record record;
    char filename[MAX_PATH];
    uint32_t crc;
    int fd;

    video_resume_filename(path, filename, sizeof(filename), &crc);
    if (sample == 0 || (uint64_t)sample * 100u >= (uint64_t)total_samples * 95u)
    {
        remove(filename);
        return;
    }
    mkdir(PLUGIN_APPS_DATA_DIR);
    record.magic = VIDEO_RESUME_MAGIC;
    record.path_crc = crc;
    record.frame = (int32_t)sample;
    record.total_frames = (int32_t)total_samples;
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    (void)write(fd, &record, sizeof(record));
    close(fd);
}

static void video_resume_clear(const char *path)
{
    char filename[MAX_PATH];

    video_resume_filename(path, filename, sizeof(filename), NULL);
    remove(filename);
}

#endif /* VIDEO_RESUME_H */
