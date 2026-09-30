/***************************************************************************
 * Shared video-viewer selection for plugins that launch synced media.
 ****************************************************************************/
#ifndef PLUGIN_VIDEO_PLAYER_H
#define PLUGIN_VIDEO_PLAYER_H

#define PLUGIN_VIDEO_MPEG_PLAYER VIEWERS_DIR "/mpegplayer.rock"
#define PLUGIN_VIDEO_H264_PLAYER VIEWERS_DIR "/openh264_player.rock"

static inline bool plugin_video_extension_is(const char *path,
                                             const char *extension)
{
    const char *dot;

    if (path == NULL || extension == NULL)
        return false;
    dot = rb->strrchr(path, '.');
    return dot != NULL && !rb->strcasecmp(dot + 1, extension);
}

static inline const char *plugin_video_player_for(const char *path)
{
    if (plugin_video_extension_is(path, "rvp") ||
        plugin_video_extension_is(path, "h264"))
        return PLUGIN_VIDEO_H264_PLAYER;

#if (defined(IPOD_6G) || defined(IPOD_VIDEO)) && !defined(SIMULATOR)
    if (plugin_video_extension_is(path, "mp4") ||
        plugin_video_extension_is(path, "m4v") ||
        plugin_video_extension_is(path, "mov"))
        return PLUGIN_VIDEO_H264_PLAYER;
#endif

    return PLUGIN_VIDEO_MPEG_PLAYER;
}

#endif /* PLUGIN_VIDEO_PLAYER_H */
