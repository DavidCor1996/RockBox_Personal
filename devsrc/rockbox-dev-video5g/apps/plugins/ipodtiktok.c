/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Lightweight launcher for an iPod clickwheel short-video feed powered by
 * mpegplayer feed mode.
 *
 ****************************************************************************/

#include "plugin.h"

#define IPODTIKTOK_FEED_PATH     PLUGIN_APPS_DATA_DIR "/.ipodtiktok_feed.tsv"
#define IPODTIKTOK_PLAYER_PATH   VIEWERS_DIR "/mpegplayer.rock"
#define IPODTIKTOK_PARAM_PREFIX  "-ipodtiktok:"

#define IPODTIKTOK_VIDEO_DIR     "/Videos/iPodTikTok"
#define IPODTIKTOK_VIDEO_ONE     IPODTIKTOK_VIDEO_DIR "/ipodtiktok1.mpg"
#define IPODTIKTOK_VIDEO_TWO     IPODTIKTOK_VIDEO_DIR "/ipodtiktok2.mpg"
#define IPODTIKTOK_VIDEO_THREE   IPODTIKTOK_VIDEO_DIR "/ipodtiktok3.mpg"
#define IPODTIKTOK_VIDEO_FOUR    IPODTIKTOK_VIDEO_DIR "/ipodtiktok4.mpg"
#define IPODTIKTOK_VIDEO_FIVE    IPODTIKTOK_VIDEO_DIR "/ipodtiktok5.mpg"

static bool write_default_feed(void)
{
    int fd;

    fd = rb->open(IPODTIKTOK_FEED_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    rb->fdprintf(fd, "id\ttitle\tpath\n");
    rb->fdprintf(fd, "ipodtiktok1\tClip 1\t%s\n", IPODTIKTOK_VIDEO_ONE);
    rb->fdprintf(fd, "ipodtiktok2\tClip 2\t%s\n", IPODTIKTOK_VIDEO_TWO);
    rb->fdprintf(fd, "ipodtiktok3\tClip 3\t%s\n", IPODTIKTOK_VIDEO_THREE);
    rb->fdprintf(fd, "ipodtiktok4\tClip 4\t%s\n", IPODTIKTOK_VIDEO_FOUR);
    rb->fdprintf(fd, "ipodtiktok5\tClip 5\t%s\n", IPODTIKTOK_VIDEO_FIVE);
    rb->close(fd);
    return true;
}

static bool ensure_default_feed(void)
{
    if (rb->file_exists(IPODTIKTOK_FEED_PATH))
        return true;

    return write_default_feed();
}

static int count_seed_videos(void)
{
    int count = 0;

    if (rb->file_exists(IPODTIKTOK_VIDEO_ONE))
        count++;
    if (rb->file_exists(IPODTIKTOK_VIDEO_TWO))
        count++;
    if (rb->file_exists(IPODTIKTOK_VIDEO_THREE))
        count++;
    if (rb->file_exists(IPODTIKTOK_VIDEO_FOUR))
        count++;
    if (rb->file_exists(IPODTIKTOK_VIDEO_FIVE))
        count++;

    return count;
}

enum plugin_status plugin_start(const void *parameter)
{
    static char launch_param[MAX_PATH + 32];
    int seed_count;

    (void)parameter;

    if (!ensure_default_feed())
    {
        rb->splash(HZ * 2, "iPodTikTok setup failed");
        return PLUGIN_ERROR;
    }

    seed_count = count_seed_videos();
    if (seed_count == 0)
    {
        rb->splash(HZ * 3, "Add converted .mpg clips to /Videos/iPodTikTok");
        return PLUGIN_ERROR;
    }

    rb->snprintf(launch_param, sizeof(launch_param), "%s%s",
                 IPODTIKTOK_PARAM_PREFIX, IPODTIKTOK_FEED_PATH);
    return rb->plugin_open(IPODTIKTOK_PLAYER_PATH, launch_param);
}
