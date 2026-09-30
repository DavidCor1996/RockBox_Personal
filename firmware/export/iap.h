/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Alan Korr
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef __IAP_H__
#define __IAP_H__

#include <stdbool.h>

/* This is just the payload size, without sync, length and checksum */
#define RX_BUFLEN (64*1024)
/* This is the entire frame length, sync, length, payload and checksum */
#define TX_BUFLEN 128

#ifdef HAVE_IAP_MULTIPORT
#define IF_IAP_MP(x...) x
#define IF_IAP_MP_NONVOID(x...) x
#else
#define IF_IAP_MP(x...)
#define IF_IAP_MP_NONVOID(x...) void
#endif

extern bool iap_getc(IF_IAP_MP(int port,) unsigned char x);
extern void iap_setup(int ratenum);
extern void iap_malloc(void);
extern void iap_bitrate_set(int ratenum);
extern void iap_periodic(void);
void iap_remote_filter(unsigned button, bool filtered, bool wake);
extern void iap_handlepkt(void);
extern void iap_send_pkt(const unsigned char * data, int len);
const unsigned char *iap_get_serbuf(void);

/* Transport abstraction — USB HID driver overrides this for iAP-over-USB */
extern void (*iap_transport_send)(const unsigned char *buf, int len);

/* True while iAP runs over the dock connector UART rather than USB HID */
extern bool iap_transport_is_serial(void);

enum iap_connection_status {
    IAP_CONNECTION_DISCONNECTED = 0,
    IAP_CONNECTION_DETECTING,
    IAP_CONNECTION_AUTHENTICATING,
    IAP_CONNECTION_READY,
    IAP_CONNECTION_RETRYING,
};

enum iap_reconnect_reason {
    IAP_RECONNECT_NONE = 0,
    IAP_RECONNECT_NO_DATA,
    IAP_RECONNECT_AUTOBAUD,
    IAP_RECONNECT_AUTH_TIMEOUT,
    IAP_RECONNECT_ACTIVATION_TIMEOUT,
    IAP_RECONNECT_ACCESSORY_RESTART,
    IAP_RECONNECT_MANUAL,
    IAP_RECONNECT_LINK_ERRORS,
};

enum iap_autobaud_status {
    IAP_AUTOBAUD_LAUNCHED = 0,
    IAP_AUTOBAUD_SYNCING,
    IAP_AUTOBAUD_DONE,
};

struct iap_connection_info {
    enum iap_connection_status status;
    enum iap_reconnect_reason reason;
    unsigned int retry_count;
    unsigned int rx_bytes;
    unsigned int autobaud_relaunches;
    unsigned int uart_errors;
    unsigned int checksum_errors;
    unsigned int contact_dropouts;
    unsigned int max_absent_ticks;
    int bitrate;
    bool kokkia_seen;
    bool authenticated;
    bool activated;
};

/* Serial accessory connection health and bounded recovery controls. */
extern enum iap_connection_status iap_connection_status(void);
extern enum iap_reconnect_reason iap_last_reconnect_reason(void);
extern unsigned int iap_reconnect_count(void);
extern void iap_get_connection_info(struct iap_connection_info *info);
extern bool iap_restart_kokkia(void);

/* Suppress non-user remote/status events during dock startup. */
extern void iap_note_serial_connect(void);
extern void iap_note_serial_disconnect(void);
extern bool iap_remote_input_suppressed(void);

/* Presence drives the Bluetooth glyph; connected drives peer-ready behavior
 * such as the one-shot AirPods animation and headset button translation. */
extern bool iap_ready_for_serial(void);
extern bool iap_kokkia_present(void);
bool iap_remote_navigation_active(void);
bool iap_remote_tv_active(void);
extern bool iap_kokkia_connected(void);
extern void iap_note_kokkia_ready(void);
extern bool iap_take_kokkia_connection_event(void);

/* Button state — set by iAP Simple Remote and Extended Interface
 * lingo handlers, read by remote_control_rx() in the button driver. */
extern unsigned long iap_remotebtn;
extern unsigned int iap_timeoutbtn;
extern int iap_repeatbtn;
#ifdef HAVE_LINE_REC
extern bool iap_record(bool onoff);
#endif
void iap_reset_state(IF_IAP_MP_NONVOID(int port) ); /* 0 is dock, 1 is headphone */
bool dbg_iap(void);
#endif
