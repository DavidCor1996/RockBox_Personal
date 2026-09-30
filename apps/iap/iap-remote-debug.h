/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IAP_REMOTE_DEBUG_H
#define IAP_REMOTE_DEBUG_H
#include <stdint.h>
void iap_remote_packet(const unsigned char *raw, unsigned len,
                       uint32_t bitmap, unsigned buttons, char state);
void iap_remote_action(unsigned button, int context, int action);
void iap_remote_command(const unsigned char *raw, unsigned len);
int iap_remote_debug_screen(void);
#endif
