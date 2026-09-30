/***************************************************************************
 * iPod Classic hardware H.264 playback entry used by viewer plugins.
 ****************************************************************************/
#ifndef VIDEO_PLAYBACK_H
#define VIDEO_PLAYBACK_H

#include <stddef.h>

/* Play a validated H.264 Baseline/AAC-LC MP4 from caller-owned memory.
 * filepath may include the same app launch prefix accepted by MPEGPlayer;
 * the core preserves that app's display and control contract while parsing
 * the real path. The caller must retain the plugin audio buffer until return.
 * Returns 0 at EOS or Netflix exit during credits or the final 5%,
 * 1 on other user exits, 2 for previous, 3 for next and 4 when an
 * app-specific player asks to open its current creator/profile. */
int video_h264_play(const char *filepath, void *buffer, size_t buffer_size);

#endif /* VIDEO_PLAYBACK_H */
