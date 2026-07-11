#ifndef SMSGG_VIDEO_H
#define SMSGG_VIDEO_H

#include "plugin.h"
#include "smsgg_core.h"

enum smsgg_scaling_mode
{
    SMSGG_SCALE_FIT = 0,
    SMSGG_SCALE_SMS_320X230,
    SMSGG_SCALE_CONSERVATIVE,
    SMSGG_SCALE_FAST,
    SMSGG_SCALE_COUNT
};

void smsgg_video_init(void);
void smsgg_video_draw(struct smsgg_core *core, enum smsgg_scaling_mode mode,
                      bool show_fps, int fps, uint8 mapped_buttons,
                      int wheel_delta, int wheel_zone, bool input_debug);

#endif
