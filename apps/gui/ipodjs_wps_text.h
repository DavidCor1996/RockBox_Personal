/***************************************************************************
 * Bounded text cache for native iPodJS Now Playing pages.
 * SPDX-License-Identifier: GPL-2.0-or-later
 ****************************************************************************/
#ifndef IPODJS_WPS_TEXT_H
#define IPODJS_WPS_TEXT_H

#include <stdbool.h>

/* Explicit input only: may read a sidecar or an embedded ID3 lyric frame. */
bool ipodjs_wps_text_load(const char *track_path);
void ipodjs_wps_text_set(const char *text);
void ipodjs_wps_text_scroll(int delta);
const char *ipodjs_wps_text_line(int row);
int ipodjs_wps_text_count(void);
int ipodjs_wps_text_top(void);

#endif
