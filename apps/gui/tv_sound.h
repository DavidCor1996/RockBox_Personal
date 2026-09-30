/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TV_SOUND_H
#define TV_SOUND_H
#include "config.h"
#include <stdbool.h>
#ifdef HAVE_COMPOSITE_VIDEO_OUT
/* True consumes the ordinary click, including when TV sounds are disabled. */
bool tv_sound_action(int action);
#endif
#endif
