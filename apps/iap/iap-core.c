/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Alan Korr & Nick Robinson
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "string-extra.h"
#include "panic.h"
#include "iap-core.h"
#include "iap-lingo.h"
#include "button.h"
#include "config.h"
#include "cpu.h"
#include "system.h"
#include "kernel.h"
#include "thread.h"
#include "serial.h"
#include "appevents.h"
#include "core_alloc.h"

#include "playlist.h"
#include "playback.h"
#include "audio.h"
#include "settings.h"
#include "metadata.h"
#include "sound.h"
#include "action.h"
#include "powermgmt.h"
#include "usb.h"
#ifdef USB_ENABLE_AUDIO
#include "../usbstack/usb_audio.h"
#include "pcm_mixer.h"
#endif

#include "tuner.h"
#if CONFIG_TUNER
#include "ipod_remote_tuner.h"
#endif

/* Transport abstraction — defaults to serial UART, can be overridden for USB HID */
static void iap_serial_tx(const unsigned char *buf, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        while(!tx_rdy()) ;
        tx_writec(buf[i]);
    }
}

void (*iap_transport_send)(const unsigned char *buf, int len) = iap_serial_tx;

/* True while iAP runs over the dock connector UART rather than USB HID.
 *
 * Serial dock accessories must not be offered IDPS: no Rockbox release
 * has ever completed an IDPS negotiation over the UART, and an
 * accessory that starts one commits to it instead of falling back to
 * legacy identification, which strands the handshake. See the 0x38
 * handler in iap-lingo0.c.
 */
bool iap_transport_is_serial(void)
{
    return iap_transport_send == iap_serial_tx;
}

/* MS_TO_TICKS converts a milisecond time period into the
 * corresponding amount of ticks. If the time period cannot
 * be accurately measured in ticks it will round up.
 */
#if (HZ>1000)
#error "HZ is >1000, please fix MS_TO_TICKS"
#endif
#define MS_PER_HZ (1000/HZ)
#define MS_TO_TICKS(x) (((x)+MS_PER_HZ-1)/MS_PER_HZ)
/* IAP specifies a timeout of 25ms for traffic from a device to the iPod.
 * Depending on HZ this cannot be accurately measured. Find out the next
 * best thing.
 */
#define IAP_PKT_TIMEOUT (MS_TO_TICKS(25))

/* Events in the iap_queue */
#define IAP_EV_TICK         (1)     /* The regular task timeout */
#define IAP_EV_MSG_RCVD     (2)     /* A complete message has been received from the device */
#define IAP_EV_MALLOC       (3)     /* Allocate memory for the RX/TX buffers */
#define IAP_EV_RESTART      (4)     /* Restart a stalled serial accessory link */
#define IAP_EV_DISCONNECT   (5)     /* Apply optional physical-unplug policy */

static bool iap_started = false;
static bool iap_setupflag = false, iap_running = false;
/* This is set to true if a SYS_POWEROFF message is received,
 * signalling impending power off
 */
static bool iap_shutdown = false;
static struct timeout iap_task_tmo;
static bool iap_retrying;
static unsigned int iap_retry_count;
static enum iap_reconnect_reason iap_retry_reason;
static long iap_watchdog_deadline;
static bool iap_watchdog_port_open;
static enum authen_state iap_watchdog_auth_state;
static unsigned int iap_watchdog_rx_base;
static unsigned int iap_health_errors_seen;
static unsigned int iap_health_error_count;
static long iap_health_error_deadline;
static bool iap_kokkia_candidate;
static bool iap_kokkia_link_ready;
static bool iap_kokkia_peer_seen;
static bool iap_kokkia_connection_pending;
static long iap_kokkia_connection_event_until;
static long iap_remote_quiet_until;
#ifdef USB_ENABLE_AUDIO
static unsigned long iap_audio_reported_frequency;
static unsigned long iap_audio_pending_frequency;
#endif

unsigned long iap_remotebtn = 0;
/* Used to make sure a button press is delivered to the processing
 * backend. While this is !0, no new incoming messasges are processed.
 * Counted down by remote_control_rx()
 */
int iap_repeatbtn = 0;
/* Used to time out button down events in case we miss the button up event
 * from the device somehow.
 * If a device sends a button down event it's required to repeat that event
 * every 30 to 100ms as long as the button is pressed, and send an explicit
 * button up event if the button is released.
 * In case the button up event is lost any down events will time out after
 * ~200ms.
 * iap_periodic() will count down this variable and reset all buttons if
 * it reaches 0
 */
unsigned int iap_timeoutbtn = 0;
bool iap_btnrepeat = false, iap_btnshuffle = false;

static long thread_stack[(DEFAULT_STACK_SIZE*6)/sizeof(long)];
static struct event_queue iap_queue;

/* These are pointer used to manage a dynamically allocated buffer which
 * will hold both the RX and TX side of things.
 *
 * iap_buffer_handle is the handle returned from core_alloc()
 * iap_buffers points to the start of the complete buffer
 *
 * The buffer is partitioned as follows:
 * - TX_BUFLEN+6 bytes for the TX buffer
 *   The 6 extra bytes are for the sync byte, the SOP byte, the length indicators
 *   (3 bytes) and the checksum byte.
 *   iap_txstart points to the beginning of the TX buffer
 *   iap_txpayload points to the beginning of the payload portion of the TX buffer
 *   iap_txnext points to the position where the next byte will be placed
 *
 * - RX_BUFLEN+2 bytes for the RX buffer
 *   The RX buffer can hold multiple packets at once, up to it's
 *   maximum capacity. Every packet consists of a two byte length
 *   indicator followed by the actual payload. The length indicator
 *   is two bytes for every length, even for packets with a length <256
 *   bytes.
 *
 *   Once a packet has been processed from the RX buffer the rest
 *   of the buffer (and the pointers below) are shifted to the front
 *   so that the next packet again starts at the beginning of the
 *   buffer. This happens with interrupts disabled, to prevent
 *   writing into the buffer during the move.
 *
 *   iap_rxstart points to the beginning of the RX buffer
 *   iap_rxpayload starts to the beginning of the currently recieved
 *   packet
 *   iap_rxnext points to the position where the next incoming byte
 *   will be placed
 *   iap_rxlen is not a pointer, but an indicator of the free
 *   space left in the RX buffer.
 *
 * The RX buffer is placed behind the TX buffer so that an eventual TX
 * buffer overflow has some place to spill into where it will not cause
 * immediate damage. See the comments for IAP_TX_* and iap_send_tx()
 */
#define IAP_MALLOC_SIZE (TX_BUFLEN+6+RX_BUFLEN+2)
#ifdef IAP_MALLOC_DYNAMIC
static int iap_buffer_handle;
#endif
static unsigned char* iap_buffers;
static unsigned char* iap_rxstart;
static unsigned char* iap_rxpayload;
static unsigned char* iap_rxnext;
static uint32_t iap_rxlen;
static unsigned char* iap_txstart;
unsigned char* iap_txpayload;
unsigned char* iap_txnext;

/* The versions of the various Lingoes we support. A major version
 * of 0 means unsupported
 */
unsigned char lingo_versions[32][2] = {
    {1, 9},     /* General lingo, 0x00 */
#ifdef HAVE_LINE_REC
    {1, 1},     /* Microphone lingo, 0x01 */
#else
    {0, 0},     /* Microphone lingo, 0x01, disabled */
#endif
    {1, 2},     /* Simple remote lingo, 0x02 */
    {1, 5},     /* Display remote lingo, 0x03 */
    {1, 12},    /* Extended Interface lingo, 0x04 */
    {1, 1},     /* RF/BT Transmitter lingo, 0x05 */
    {0, 0},     /* USB Host lingo, 0x06, disabled */
#if CONFIG_TUNER
    {1, 0},     /* RF Receiver lingo, 0x07 */
#else
    {0, 0},     /* RF Receiver lingo, 0x07 disabled */
#endif
    {0, 0},     /* Accessory Equalizer lingo, 0x08, disabled */
    {0, 0},     /* Reserved, 0x09 */
    {1, 0},     /* Digital Audio lingo, 0x0A */
    {}          /* every other lingo, disabled */
};

/* states of the iap de-framing state machine */
enum fsm_state {
    ST_SYNC = 0,    /* wait for 0xFF sync byte */
    ST_SOF,     /* wait for 0x55 start-of-frame byte */
    ST_LEN,     /* receive length byte (small packet) */
    ST_LENH,    /* receive length high byte (large packet) */
    ST_LENL,    /* receive length low byte (large packet) */
    ST_DATA,    /* receive data */
    ST_CHECK    /* verify checksum */
};

static struct state_t {
    enum fsm_state state;   /* current fsm state */
    unsigned int len;       /* payload data length */
    unsigned int check;     /* running checksum over [len,payload,check] */
    unsigned int count;     /* playload bytes counter */
} frame_state = {
    .state = ST_SYNC
};

enum interface_state interface_state = IST_STANDARD;

struct device_t device;

#ifdef IAP_MALLOC_DYNAMIC
static int iap_move_callback(int handle, void* current, void* new);

static struct buflib_callbacks iap_buflib_callbacks = {
    iap_move_callback,
    NULL
};
#endif

void iap_malloc(void);

static void iap_reset_buffers(void)
{
    iap_txstart = iap_buffers;
    iap_txpayload = iap_txstart+5;
    iap_txnext = iap_txpayload;
    iap_rxstart = iap_buffers+(TX_BUFLEN+6);
    iap_rxpayload = iap_rxstart;
    iap_rxnext = iap_rxpayload;
    iap_rxlen = RX_BUFLEN+2;
}

void put_u16(unsigned char *buf, const uint16_t data)
{
    buf[0] = (data >>  8) & 0xFF;
    buf[1] = (data >>  0) & 0xFF;
}

void put_u32(unsigned char *buf, const uint32_t data)
{
    buf[0] = (data >> 24) & 0xFF;
    buf[1] = (data >> 16) & 0xFF;
    buf[2] = (data >>  8) & 0xFF;
    buf[3] = (data >>  0) & 0xFF;
}

uint32_t get_u32(const unsigned char *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

uint16_t get_u16(const unsigned char *buf)
{
    return (buf[0] << 8) | buf[1];
}

/* Ring buffer of the most recent iAP packets, shown by dbg_iap().
 * Large enough to hold a complete accessory handshake so the
 * interesting part does not scroll out before it can be read.
 */
#define IAP_TRACE_ENTRIES 64
#define IAP_TRACE_DATA_LEN 32

struct iap_trace_entry {
    unsigned int seq;
    long tick;
    char dir;
    uint16_t len;
    unsigned char data[IAP_TRACE_DATA_LEN];
};

static struct iap_trace_entry iap_trace[IAP_TRACE_ENTRIES];
static unsigned int iap_trace_head;
static unsigned int iap_trace_count;
static unsigned int iap_trace_seq;
static bool iap_trace_paused;
static bool iap_trace_dump_pending;
static long iap_trace_last_tick;

static void iap_trace_packet(char dir, const unsigned char *data, int len)
{
    int level;
    struct iap_trace_entry *entry;
    int copylen = MIN(len, IAP_TRACE_DATA_LEN);

    if (iap_trace_paused)
        return;

    level = disable_irq_save();

    entry = &iap_trace[iap_trace_head];
    entry->seq = ++iap_trace_seq;
    entry->tick = current_tick;
    entry->dir = dir;
    entry->len = len;
    if (copylen > 0)
        memcpy(entry->data, data, copylen);
    if (copylen < IAP_TRACE_DATA_LEN)
        memset(entry->data + copylen, 0, IAP_TRACE_DATA_LEN - copylen);

    iap_trace_head = (iap_trace_head + 1) % IAP_TRACE_ENTRIES;
    if (iap_trace_count < IAP_TRACE_ENTRIES)
        iap_trace_count++;
    iap_trace_last_tick = current_tick;

    restore_irq(level);
}

/* Dump the trace ring and the serial-layer counters to a file on the
 * player, so a capture can be read off the device over USB instead of
 * transcribed from the debug screen.
 *
 * Failure recovery requests a dump, which iap_periodic() performs only once
 * the exchange has been quiet for a couple of seconds.  Ordinary remote
 * traffic remains RAM-only and cannot repeatedly wake storage.
 */
#include "file.h"

#ifdef IPOD_6G
extern unsigned int iap_diag_rx_bytes;
extern unsigned int iap_diag_abr_relaunch;
extern unsigned int iap_diag_abr_status;
extern unsigned int iap_diag_port_open;
extern unsigned int iap_diag_uart_errors;
extern unsigned int iap_diag_uart_overruns;
extern unsigned int iap_diag_uart_parity_errors;
extern unsigned int iap_diag_uart_frame_errors;
extern unsigned int iap_diag_uart_breaks;
extern unsigned int iap_diag_contact_dropouts;
extern unsigned int iap_diag_max_absent_ticks;
extern int iap_diag_rate;
int pmu_accessory_present(void);
#endif

static unsigned int iap_diag_checksum_errors;

#define IAP_TRACE_FILE "/iap-trace.txt"

static void iap_trace_hex(char *buf, size_t bufsz,
                          const struct iap_trace_entry *entry);

static void iap_trace_dump_file(void)
{
    int fd;
    unsigned int count, head, i;
    int level;
    char line[128];
    char hexbuf[IAP_TRACE_DATA_LEN * 3 + 4];

    fd = open(IAP_TRACE_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd < 0)
        return;

#ifdef IPOD_6G
    snprintf(line, sizeof(line),
             "plug=%d port=%d rate=%d rx=%u abr=%u relaunch=%u\n",
             pmu_accessory_present(), iap_diag_port_open, iap_diag_rate,
             iap_diag_rx_bytes, iap_diag_abr_status, iap_diag_abr_relaunch);
    write(fd, line, strlen(line));
    snprintf(line, sizeof(line),
             "uart=%u ov=%u par=%u frm=%u brk=%u csum=%u\n",
             iap_diag_uart_errors, iap_diag_uart_overruns,
             iap_diag_uart_parity_errors, iap_diag_uart_frame_errors,
             iap_diag_uart_breaks, iap_diag_checksum_errors);
    write(fd, line, strlen(line));
    snprintf(line, sizeof(line),
             "contact=%u max_absent=%u ticks batt=%d mV level=%d%%\n",
             iap_diag_contact_dropouts, iap_diag_max_absent_ticks,
             battery_voltage(), battery_level());
    write(fd, line, strlen(line));
#endif
    snprintf(line, sizeof(line),
             "setupflag=%d running=%d fsm=%d\n",
             (int)iap_setupflag, (int)iap_running, (int)frame_state.state);
    write(fd, line, strlen(line));

    snprintf(line, sizeof(line),
             "auth=%d idps=%d accinfo=%d iface=%d lingoes=%08lx caps=%08lx\n",
             (int)device.auth.state, (int)device.auth.idps,
             (int)device.accinfo, (int)interface_state,
             (unsigned long)device.lingoes,
             (unsigned long)device.capabilities);
    write(fd, line, strlen(line));

    snprintf(line, sizeof(line),
             "kokkia=%d activated=%d connected=%d retry=%u reason=%d\n",
             (int)iap_kokkia_candidate,
             (int)device.serial_activation_sent,
             (int)iap_kokkia_connected(), iap_retry_count,
             (int)iap_retry_reason);
    write(fd, line, strlen(line));

    level = disable_irq_save();
    count = iap_trace_count;
    head = iap_trace_head;
    restore_irq(level);

    snprintf(line, sizeof(line), "packets=%u (oldest first)\n", count);
    write(fd, line, strlen(line));

    for (i = 0; i < count; i++)
    {
        struct iap_trace_entry entry;
        unsigned int index = (head + IAP_TRACE_ENTRIES - count + i)
                           % IAP_TRACE_ENTRIES;

        level = disable_irq_save();
        entry = iap_trace[index];
        restore_irq(level);

        iap_trace_hex(hexbuf, sizeof(hexbuf), &entry);
        snprintf(line, sizeof(line), "%03u %c t=%ld len=%u %s\n",
                 entry.seq, entry.dir, entry.tick, entry.len, hexbuf);
        write(fd, line, strlen(line));
    }

    close(fd);
}

static void iap_trace_service(void)
{
    static unsigned int last_dumped_seq;
    unsigned int seq;
    long last;
    int level;

    level = disable_irq_save();
    seq = iap_trace_seq;
    last = iap_trace_last_tick;
    restore_irq(level);

    if (!iap_trace_dump_pending || seq == 0 || seq == last_dumped_seq)
        return;

    /* wait for the exchange to settle before touching the disk */
    if (!TIME_AFTER(current_tick, last + 2*HZ))
        return;

    iap_trace_dump_file();
    last_dumped_seq = seq;
    iap_trace_dump_pending = false;
}

static void iap_trace_clear(void)
{
    int level = disable_irq_save();

    iap_trace_head = 0;
    iap_trace_count = 0;
    iap_trace_seq = 0;
    iap_trace_dump_pending = false;
    memset(iap_trace, 0, sizeof(iap_trace));

    restore_irq(level);
}

static void iap_trace_hex(char *buf, size_t bufsz,
                          const struct iap_trace_entry *entry)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    size_t out = 0;
    size_t len = MIN(entry->len, IAP_TRACE_DATA_LEN);

    for (i = 0; i < len && out + 3 < bufsz; i++)
    {
        if (i > 0)
            buf[out++] = ' ';
        buf[out++] = hex[(entry->data[i] >> 4) & 0x0f];
        buf[out++] = hex[entry->data[i] & 0x0f];
    }

    if (entry->len > IAP_TRACE_DATA_LEN && out + 4 < bufsz)
    {
        buf[out++] = ' ';
        buf[out++] = '.';
        buf[out++] = '.';
        buf[out++] = '.';
    }

    buf[out] = '\0';
}

#if defined(LOGF_ENABLE) && defined(ROCKBOX_HAS_LOGF)
/* Convert a buffer into a printable string, perl style
 * buf contains the data to be converted, len is the length
 * of the buffer.
 *
 * This will convert at most 1024 bytes from buf
 */
static char* hexstring(const unsigned char *buf, unsigned int len) {
    static char hexbuf[4097];
    unsigned int l;
    const unsigned char* p;
    unsigned char* out;
    unsigned char h[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    if (len > 1024) {
        l = 1024;
    } else {
        l = len;
    }
    p = buf;
    out = hexbuf;
    do {
            *out++ = h[(*p)>>4];
            *out++ = h[*p & 0x0F];
    } while(--l && p++);

    *out = 0x00;

    return hexbuf;
}
#endif


void iap_tx_strlcpy(const unsigned char *str)
{
    ptrdiff_t txfree;
    int r;

    txfree = TX_BUFLEN - (iap_txnext - iap_txstart);
    r = strlcpy(iap_txnext, str, txfree);

    if (r < txfree)
    {
        /* No truncation occured
         * Account for the terminating \0
         */
        iap_txnext += (r+1);
    } else {
        /* Truncation occured, the TX buffer is now full. */
        iap_txnext = iap_txstart + TX_BUFLEN;
    }
}

void iap_reset_auth(struct auth_t* auth)
{
    auth->state = AUST_NONE;
    auth->max_section = 0;
    auth->next_section = 0;
}

void iap_reset_state(IF_IAP_MP_NONVOID(int port))
{
    int level;

    if (!iap_running)
        return;

    /* 0 is dock, 1 is headphone.  This is for
       when we eventually maintain independent state */
    IF_IAP_MP((void)port);

    /* A physical removal is a hard session boundary.  On iPod 6G the dock
     * UART is the only iAP serial port, so it is safe and necessary to clear
     * a partial frame as well as authentication state.  Otherwise bytes left
     * by a tapped/unplugged transmitter can poison the first packet after
     * reinsertion until Rockbox is rebooted. */
    level = disable_irq_save();
    iap_reset_device(&device);
#ifdef IPOD_6G
    memset(&frame_state, 0, sizeof(frame_state));
    frame_state.state = ST_SYNC;
    interface_state = IST_STANDARD;
    iap_reset_buffers();
#endif
    restore_irq(level);

    iap_bitrate_set(global_settings.serial_bitrate);
    iap_retrying = false;
    iap_retry_count = 0;
    iap_retry_reason = IAP_RECONNECT_NONE;
    iap_watchdog_deadline = 0;
    iap_watchdog_port_open = false;
    iap_watchdog_auth_state = AUST_NONE;
#ifdef IPOD_6G
    iap_watchdog_rx_base = iap_diag_rx_bytes;
    iap_health_errors_seen = iap_diag_uart_errors +
                             iap_diag_checksum_errors;
#else
    iap_watchdog_rx_base = 0;
    iap_health_errors_seen = iap_diag_checksum_errors;
#endif
    iap_health_error_count = 0;
    iap_health_error_deadline = 0;
    iap_kokkia_candidate = false;
    iap_kokkia_link_ready = false;
    iap_kokkia_peer_seen = false;
    iap_kokkia_connection_pending = false;
    iap_remote_quiet_until = 0;

#if 0  // XXX this is still screwed up
    memset(&frame_state, 0, sizeof(frame_state));
    interface_state = IST_STANDARD;
    frame_state.state = ST_SYNC;

    iap_reset_buffers();
#endif
}

void iap_reset_device(struct device_t* device)
{
    iap_reset_auth(&(device->auth));
    device->lingoes = 0;
    device->notifications = 0;
    device->changed_notifications = 0;
    device->do_notify = false;
    device->do_power_notify = false;
    device->accinfo = ACCST_NONE;
    device->capabilities = 0;
    device->capabilities_queried = 0;
    device->audio_init_pending = false;
    device->volume_notify_pending = false;
    device->idps_lingoes = 0;
    device->idps_options = 0;
    device->idps_deviceid = 0;
    device->ipod_trans_id = 1;
    device->serial_activation_sent = false;
    device->serial_activation_tid = 0;
    device->kokkia_detected = false;

    /* IDPS is a property of how the device identified itself, so it
     * must not survive into the next identification.  Only EndIDPS
     * (0x3B) sets it, and it does so after calling us.  Without this
     * a USB HID dock leaves the flag set, and the next serial dock
     * accessory gets transaction IDs stamped into every response -
     * which it cannot parse, so it never finishes authenticating.
     *
     * Deliberately not done in iap_reset_auth(): the IDPS error paths
     * reset the auth state mid-session and must still answer with
     * transaction IDs.
     */
    device->auth.idps = false;
    device->auth.tid_hi = 0;
    device->auth.tid_lo = 0;
#ifdef USB_ENABLE_AUDIO
    iap_audio_reported_frequency = 0;
    iap_audio_pending_frequency = 0;
#endif
}

static void iap_restart_serial_link(enum iap_reconnect_reason reason)
{
    int level;

    if (!iap_running || !iap_transport_is_serial())
        return;

    /* Stop UART receive before resetting the framing pointers.  The restart
     * runs in the iAP thread, and IRQ exclusion closes the small window in
     * which an autobaud interrupt could otherwise feed the old frame. */
    level = disable_irq_save();
    serial_bitrate(0);
    iap_reset_device(&device);
    memset(&frame_state, 0, sizeof(frame_state));
    frame_state.state = ST_SYNC;
    interface_state = IST_STANDARD;
    iap_reset_buffers();
    iap_bitrate_set(global_settings.serial_bitrate);
    restore_irq(level);

    iap_retrying = true;
    iap_retry_count++;
    iap_retry_reason = reason;
    iap_kokkia_link_ready = false;
    iap_kokkia_peer_seen = false;
    iap_kokkia_connection_pending = false;
    iap_watchdog_auth_state = AUST_NONE;
#ifdef IPOD_6G
    iap_watchdog_rx_base = iap_diag_rx_bytes;
    iap_health_errors_seen = iap_diag_uart_errors +
                             iap_diag_checksum_errors;
#endif
    iap_health_error_count = 0;
    iap_health_error_deadline = 0;
    iap_trace_dump_pending = true;
}

void iap_note_accessory_restart(void)
{
    iap_kokkia_candidate = true;
    iap_kokkia_link_ready = false;
    iap_kokkia_peer_seen = false;
    iap_kokkia_connection_pending = false;
    iap_remote_quiet_until = current_tick + 5 * HZ;
    iap_retrying = true;
    iap_retry_count++;
    iap_retry_reason = IAP_RECONNECT_ACCESSORY_RESTART;
    iap_watchdog_deadline = current_tick + 5 * HZ;
    iap_watchdog_auth_state = AUST_NONE;
#ifdef IPOD_6G
    iap_watchdog_rx_base = iap_diag_rx_bytes;
    iap_health_errors_seen = iap_diag_uart_errors +
                             iap_diag_checksum_errors;
#endif
    iap_health_error_count = 0;
    iap_health_error_deadline = 0;
    iap_trace_dump_pending = true;
}

void iap_note_serial_connect(void)
{
    /* Accessories can emit a Play/status packet as part of power-up.  It is
     * not a user button press and must not activate the selected Home item. */
    iap_remote_quiet_until = current_tick + 2 * HZ;
}

void iap_note_serial_disconnect(void)
{
    if (iap_started && global_settings.kokkia_pause_on_unplug &&
        iap_kokkia_present())
        queue_post(&iap_queue, IAP_EV_DISCONNECT, 0);
}

bool iap_remote_input_suppressed(void)
{
    return iap_remote_quiet_until &&
           !TIME_AFTER(current_tick, iap_remote_quiet_until);
}

void iap_note_kokkia_candidate(void)
{
    if (!iap_kokkia_candidate)
    {
        iap_kokkia_candidate = true;
        iap_watchdog_deadline = current_tick + 5 * HZ;
    }

    /* Keep activation-generated remote/status packets out of the UI while
     * preserving normal AirPods controls once the handshake settles. */
    iap_remote_quiet_until = current_tick + 5 * HZ;
}

void iap_note_kokkia_ready(void)
{
    iap_kokkia_candidate = true;
    /* Keep a completed activation stable until a real link boundary.
     * Authentication bookkeeping can be reset later while the powered
     * Kokkia and its Bluetooth audio side remain healthy. */
    iap_kokkia_link_ready = true;
    iap_retrying = false;
    iap_retry_count = 0;
    iap_watchdog_deadline = 0;
#ifdef IPOD_6G
    iap_health_errors_seen = iap_diag_uart_errors +
                             iap_diag_checksum_errors;
#else
    iap_health_errors_seen = iap_diag_checksum_errors;
#endif
    iap_health_error_count = 0;
    iap_health_error_deadline = 0;
}

void iap_note_kokkia_peer_connection(void)
{
    int level = disable_irq_save();

    if (!iap_kokkia_peer_seen)
    {
        /* Kokkia emits a short remote/status pulse when its Bluetooth side
         * acquires a peer.  This is the same non-user pulse that historically
         * opened Cover Flow, so it is both suppressed as input and reused as
         * the best radio-connection edge Rockbox can observe. */
        iap_kokkia_peer_seen = true;
        iap_kokkia_connection_pending = true;
        iap_kokkia_connection_event_until = current_tick + 2 * HZ;
    }
    restore_irq(level);
}

bool iap_take_kokkia_connection_event(void)
{
    bool pending;
    int level = disable_irq_save();

    pending = iap_kokkia_connection_pending;
    iap_kokkia_connection_pending = false;
    restore_irq(level);

    /* Do not replay a connection notification after the user later returns
     * from a plugin or another screen.  Main Menu polls well inside this
     * one-second edge window. */
    if (pending &&
        TIME_AFTER(current_tick, iap_kokkia_connection_event_until))
        pending = false;
    return pending;
}

bool iap_restart_kokkia(void)
{
    if (!iap_started || !iap_running || !iap_transport_is_serial())
        return false;

#ifdef IPOD_6G
    if (!iap_diag_port_open)
        return false;
#endif

    queue_post(&iap_queue, IAP_EV_RESTART, 0);
    return true;
}

bool iap_kokkia_present(void)
{
    bool present = iap_kokkia_candidate &&
                   iap_transport_is_serial();

#ifdef IPOD_6G
    present = present && iap_diag_port_open;
#endif
    return present;
}

bool iap_kokkia_connected(void)
{
    bool connected = iap_kokkia_link_ready &&
                     iap_transport_is_serial();

#ifdef IPOD_6G
    connected = connected && iap_diag_port_open;
#endif
    return connected;
}

enum iap_connection_status iap_connection_status(void)
{
#ifdef IPOD_6G
    if (!iap_diag_port_open)
        return IAP_CONNECTION_DISCONNECTED;
#endif

    if (iap_kokkia_connected())
        return IAP_CONNECTION_READY;
    if (iap_retrying)
        return IAP_CONNECTION_RETRYING;
    if (iap_kokkia_candidate || DEVICE_AUTH_RUNNING)
        return IAP_CONNECTION_AUTHENTICATING;
    return IAP_CONNECTION_DETECTING;
}

enum iap_reconnect_reason iap_last_reconnect_reason(void)
{
    return iap_retry_reason;
}

unsigned int iap_reconnect_count(void)
{
    return iap_retry_count;
}

void iap_get_connection_info(struct iap_connection_info *info)
{
    if (!info)
        return;

    memset(info, 0, sizeof(*info));
    info->status = iap_connection_status();
    info->reason = iap_retry_reason;
    info->retry_count = iap_retry_count;
    info->kokkia_seen = iap_kokkia_candidate;
    info->authenticated = iap_kokkia_candidate && DEVICE_AUTHENTICATED;
    info->activated = iap_kokkia_link_ready;
#ifdef IPOD_6G
    info->rx_bytes = iap_diag_rx_bytes;
    info->autobaud_relaunches = iap_diag_abr_relaunch;
    info->uart_errors = iap_diag_uart_errors;
    info->contact_dropouts = iap_diag_contact_dropouts;
    info->max_absent_ticks = iap_diag_max_absent_ticks;
    info->bitrate = iap_diag_rate;
#endif
    info->checksum_errors = iap_diag_checksum_errors;
}

#define IAP_HEALTH_ERROR_THRESHOLD 4
#define IAP_HEALTH_ERROR_WINDOW (2 * HZ)

static void iap_reconnect_watchdog(void)
{
#ifdef IPOD_6G
    enum iap_reconnect_reason reason;
    unsigned int shift;
    long delay;

    if (!iap_transport_is_serial() || !iap_diag_port_open)
    {
        iap_watchdog_port_open = false;
        iap_watchdog_deadline = 0;
        iap_retrying = false;
        iap_health_error_count = 0;
        iap_health_error_deadline = 0;
        return;
    }

    /* Do not disturb ordinary serial remotes that intentionally operate
     * without MFi authentication.  Automatic retries begin only after the
     * Kokkia-style StartIDPS activation exchange has been seen. */
    if (!iap_kokkia_candidate)
    {
        iap_watchdog_port_open = false;
        iap_watchdog_deadline = 0;
        iap_retrying = false;
        iap_health_error_count = 0;
        iap_health_error_deadline = 0;
        return;
    }

    if (!iap_watchdog_port_open)
    {
        iap_watchdog_port_open = true;
        iap_watchdog_deadline = current_tick + 5 * HZ;
        iap_watchdog_auth_state = device.auth.state;
        iap_watchdog_rx_base = iap_diag_rx_bytes;
        iap_health_errors_seen = iap_diag_uart_errors +
                                 iap_diag_checksum_errors;
        return;
    }

    /* Every forward authentication transition proves that the accessory is
     * alive.  Clear stale backoff and grant the new stage a fresh deadline. */
    if (device.auth.state != AUST_NONE &&
        device.auth.state > iap_watchdog_auth_state)
    {
        iap_watchdog_auth_state = device.auth.state;
        iap_retry_count = 0;
        iap_retrying = false;
        iap_watchdog_deadline = current_tick + 15 * HZ;
    }

    /* Successful activation disables silence-based recovery, but clustered
     * hardware/framing failures prove that the serial path is unhealthy.
     * Isolated errors age out and Bluetooth quiet alone can never restart a
     * READY session. */
    if (iap_kokkia_connected())
    {
        unsigned int health_errors = iap_diag_uart_errors +
                                     iap_diag_checksum_errors;
        unsigned int delta = health_errors - iap_health_errors_seen;

        iap_health_errors_seen = health_errors;

        if (iap_health_error_deadline &&
            TIME_AFTER(current_tick, iap_health_error_deadline))
        {
            iap_health_error_count = 0;
            iap_health_error_deadline = 0;
        }

        if (delta)
        {
            if (!iap_health_error_deadline)
                iap_health_error_deadline =
                    current_tick + IAP_HEALTH_ERROR_WINDOW;

            iap_health_error_count += delta;
            if (iap_health_error_count >= IAP_HEALTH_ERROR_THRESHOLD)
            {
                iap_restart_serial_link(IAP_RECONNECT_LINK_ERRORS);
                iap_watchdog_deadline = current_tick + 15 * HZ;
                return;
            }
        }

        iap_watchdog_deadline = 0;
        iap_retrying = false;
        return;
    }

    if (!iap_watchdog_deadline)
        iap_watchdog_deadline = current_tick + 15 * HZ;

    if (!TIME_AFTER(current_tick, iap_watchdog_deadline))
        return;

    if (iap_diag_rx_bytes == iap_watchdog_rx_base)
        reason = IAP_RECONNECT_NO_DATA;
    else if (iap_diag_abr_status != IAP_AUTOBAUD_DONE)
        reason = IAP_RECONNECT_AUTOBAUD;
    else if (DEVICE_AUTHENTICATED)
        reason = IAP_RECONNECT_ACTIVATION_TIMEOUT;
    else
        reason = IAP_RECONNECT_AUTH_TIMEOUT;

    iap_restart_serial_link(reason);

    /* Retry promptly once after boot, then back off to 15, 30, and 60
     * seconds.  Only the Kokkia signature enables this watchdog. */
    shift = MIN(iap_retry_count, 3) - 1;
    delay = (15 * HZ) << shift;
    iap_watchdog_deadline = current_tick + delay;
#endif
}

static int iap_task(struct timeout *tmo)
{
    (void) tmo;

    /* No accessory connected yet -- tick slowly to avoid unnecessary
     * wakeups while the IAP thread is idle. */
    if (!iap_running)
        return MS_TO_TICKS(1000);

    queue_post(&iap_queue, IAP_EV_TICK, 0);

    /* After auth completes and no active work remains, reduce tick
     * rate from 10 Hz to 1 Hz to save power during idle MFi DAC
     * connections.  100ms is still needed during auth handshake,
     * accessory info polling, button repeat, notification delivery,
     * and shutdown notification. */
    if (device.auth.state == AUST_AUTH
        && device.accinfo != ACCST_INIT
        && device.accinfo != ACCST_SENT
        && device.accinfo != ACCST_DATA
        && !device.audio_init_pending
        && !device.volume_notify_pending
        && !device.do_notify
        && !iap_shutdown
        && iap_timeoutbtn == 0
        )
        return MS_TO_TICKS(1000);

    return MS_TO_TICKS(100);
}


/* iAP carries volume as 0..255 across the player's whole range, so the
 * conversion has to be derived from the codec rather than assumed.
 *
 * These used to hardcode (volume + 90) * 2.65625, which is the WM8758
 * range of the iPod Video (-90..+6 dB). The iPod Classic's CS42L55 runs
 * -60..+12 dB, so anything above +6 dB produced more than 255 and wrapped:
 * at maximum volume the player reported 14 instead of 255. Accessories
 * that mirror iPod volume see that as a jump to near-silence, and some
 * (Kokkia i10s) drop the link over it.
 */
unsigned char iap_volume_byte(void)
{
    int minv = sound_min(SOUND_VOLUME);
    int maxv = sound_max(SOUND_VOLUME);
    int range = maxv - minv;
    int vol = global_status.volume;

    if (range <= 0)
        return 0;
    if (vol <= minv)
        return 0;
    if (vol >= maxv)
        return 255;

    return (unsigned char)(((vol - minv) * 255 + range / 2) / range);
}

int iap_volume_from_byte(unsigned char raw)
{
    int minv = sound_min(SOUND_VOLUME);
    int maxv = sound_max(SOUND_VOLUME);
    int range = maxv - minv;

    if (range <= 0)
        return minv;

    return minv + ((int)raw * range + 127) / 255;
}

void iap_set_remote_volume(void)
{
    unsigned char volume = iap_volume_byte();

    IAP_TX_INIT(0x03, 0x0D);
    IAP_TX_PUT_IPOD_TRANSID();
    IAP_TX_PUT(0x04);
    IAP_TX_PUT(0x00);
    IAP_TX_PUT(volume);
    iap_send_tx();
    device.volume = volume;
}

/* This thread is waiting for events posted to iap_queue and calls
 * the appropriate subroutines in response
 */
static void iap_thread(void)
{
    struct queue_event ev;
    while(1) {
        queue_wait(&iap_queue, &ev);
        switch (ev.id)
        {
            /* Handle the regular 100ms tick used for driving the
             * authentication state machine and notifications
             */
            case IAP_EV_TICK:
            {
                iap_periodic();
                break;
            }

            /* Handle a newly received message from the device */
            case IAP_EV_MSG_RCVD:
            {
                iap_handlepkt();
                break;
            }

            /* Handle memory allocation. This is used only once, during
             * startup
             */
            case IAP_EV_MALLOC:
            {
                iap_malloc();
                break;
            }

            case IAP_EV_RESTART:
            {
                iap_restart_serial_link(IAP_RECONNECT_MANUAL);
                iap_watchdog_deadline = current_tick + 15 * HZ;
                break;
            }

            case IAP_EV_DISCONNECT:
            {
                if (global_settings.kokkia_pause_on_unplug &&
                    audio_status() == AUDIO_STATUS_PLAY)
                    audio_pause();
                break;
            }

            /* Handle poweroff message */
            case SYS_POWEROFF:
            case SYS_REBOOT:
            {
                iap_shutdown = true;
                break;
            }

            /* Ack USB thread */
            case SYS_USB_CONNECTED:
            {
                usb_acknowledge(SYS_USB_CONNECTED_ACK, ev.data);
                break;
            }
        }
    }
}

/* called by playback when the next track starts */
static void iap_track_changed(unsigned short id, void *param)
{
    (void)id;

#ifdef USB_ENABLE_AUDIO
    struct track_event *te = param;
    unsigned long frequency = mixer_get_frequency();

    if (global_settings.play_frequency)
        frequency = global_settings.play_frequency;
    else if (te && te->id3 && te->id3->frequency)
        frequency = (te->id3->frequency % 4000) ? SAMPR_44 : SAMPR_48;

    if (DEVICE_LINGO_SUPPORTED(0x0A)
        && iap_audio_reported_frequency != frequency)
    {
        iap_audio_pending_frequency = frequency;
        if (!device.audio_init_pending)
        {
            device.audio_init_pending = true;
            queue_post(&iap_queue, IAP_EV_TICK, 0);
        }
    }
#else
    (void)param;
#endif

    if ((interface_state == IST_EXTENDED) && device.do_notify) {
        long playlist_pos = playlist_next(0);
        playlist_pos -= playlist_get_first_index(NULL);
        if(playlist_pos < 0)
            playlist_pos += playlist_amount();

        IAP_TX_INIT4(0x04, 0x0027);
        IAP_TX_PUT(0x01);
        IAP_TX_PUT_U32(playlist_pos);

        iap_send_tx();
        return;
    }
}

/* Set up the IAP infrastructure.
 *
 * On the first call (boot), creates the message queue, handler thread
 * and notification timer.  On subsequent calls (e.g. USB HID reconnect)
 * only resets the device state, avoiding duplicate threads.
 */
void iap_setup(const int ratenum)
{
    iap_bitrate_set(ratenum);
    iap_remotebtn = BUTTON_NONE;
    iap_setupflag = true;
    iap_running = false;

    if (!iap_started)
    {
        unsigned int tid;

        iap_reset_device(&device);
        queue_init(&iap_queue, true);
        tid = create_thread(iap_thread, thread_stack, sizeof(thread_stack),
                0, "iap"
                IF_PRIO(, PRIORITY_SYSTEM)
                IF_COP(, CPU));
        if (!tid)
            panicf("Could not create iap thread");
        timeout_register(&iap_task_tmo, iap_task, MS_TO_TICKS(100),
                (intptr_t)NULL);
        add_event(PLAYBACK_EVENT_TRACK_CHANGE, iap_track_changed);
        iap_started = true;
    }
    else
    {
        iap_reset_device(&device);
    }

    /* Allocate the RX/TX buffers now, in thread context, rather than
     * leaving it to the first received sync byte.
     *
     * During auto-bitrate detection the UART Rx logic is disabled, so
     * the accessory's 0xFF is consumed as the ABR start-bit pulse and
     * never reaches the framing state machine.  iap_rx_isr() therefore
     * injects a synthetic 0xFF to move it into ST_SOF.  If iap_running
     * is still false at that moment, iap_getc() swallows that 0xFF -
     * it only posts IAP_EV_MALLOC and stays in ST_SYNC - so the real
     * 0x55 and length bytes never match, sync_retry runs out, and the
     * whole packet is discarded while ABR relaunches.
     *
     * Recovery then depends on the accessory retransmitting AND on the
     * IAP thread having drained IAP_EV_MALLOC in the meantime.  An
     * accessory that identifies only a bounded number of times can run
     * out of attempts first.  Allocating up front removes the race
     * instead of racing it.  The buffer is static, so this costs no
     * memory, and iap_malloc() early-returns once iap_running is set,
     * leaving the existing IAP_EV_MALLOC path harmless.
     */
    iap_malloc();
}

/* Trigger buffer allocation for the IAP thread.
 *
 * Called from iap_getc() on the first received sync byte.
 * May run in interrupt context (serial UART ISR), so only
 * queue_post() is used here -- the thread and queue were
 * already created by iap_setup().
 */
static void iap_start(void)
{
    queue_post(&iap_queue, IAP_EV_MALLOC, 0);
}

void iap_malloc(void)
{
#ifndef IAP_MALLOC_DYNAMIC
    static unsigned char serbuf[IAP_MALLOC_SIZE];
#endif

    if (iap_running)
        return;

#ifdef IAP_MALLOC_DYNAMIC
    iap_buffer_handle = core_alloc_ex(IAP_MALLOC_SIZE, &iap_buflib_callbacks);
    if (iap_buffer_handle < 0)
        panicf("Could not allocate buffer memory");
    iap_buffers = core_get_data(iap_buffer_handle);
#else
    iap_buffers = serbuf;
#endif

    iap_reset_buffers();
    iap_running = true;
}

bool iap_ready_for_serial(void)
{
    return iap_setupflag && iap_running;
}

void iap_bitrate_set(const int ratenum)
{
    switch(ratenum)
    {
        case 0:
            serial_bitrate(0);
            break;
        case 1:
            serial_bitrate(9600);
            break;
        case 2:
            serial_bitrate(19200);
            break;
        case 3:
            serial_bitrate(38400);
            break;
        case 4:
            serial_bitrate(57600);
            break;
    }
}

/* Message format:
   0xff
   0x55
   length
   mode
   command (2 bytes)
   parameters (0-n bytes)
   checksum (length+mode+parameters+checksum == 0)
*/

/* Send the current content of the TX buffer.
 * This will check for TX buffer overflow and panic, but it might
 * be too late by then (although one would have to overflow the complete
 * RX buffer as well)
 */
void iap_send_tx(void)
{
    int i, chksum;
    ptrdiff_t txlen;
    unsigned char* txstart;

    txlen = iap_txnext - iap_txpayload;

    if (txlen <= 0)
        return;

    if (txlen > TX_BUFLEN)
        panicf("IAP: TX buffer overflow");

    if (txlen < 256)
    {
        /* Short packet */
        txstart = iap_txstart+2;
        *(txstart+2) = txlen;
        chksum = txlen;
    } else {
        /* Long packet */
        txstart = iap_txstart;
        *(txstart+2) = 0x00;
        *(txstart+3) = (txlen >> 8) & 0xFF;
        *(txstart+4) = (txlen) & 0xFF;
        chksum = *(txstart+3) + *(txstart+4);
    }
    *(txstart) = 0xFF;
    *(txstart+1) = 0x55;

    for (i=0; i<txlen; i++)
    {
        chksum += iap_txpayload[i];
    }
    *(iap_txnext) = 0x100 - (chksum & 0xFF);

#if defined(LOGF_ENABLE) && defined(ROCKBOX_HAS_LOGF)
    logf("T: %s", hexstring(txstart+3, (iap_txnext - txstart)-3));
#endif
    iap_trace_packet('T', iap_txpayload, txlen);
    iap_transport_send(txstart, (iap_txnext - txstart) + 1);
}

/* This is just a compatibility wrapper around the new TX buffer
 * infrastructure
 */
void iap_send_pkt(const unsigned char * data, const int len)
{
    if (!iap_running)
        return;

    iap_txnext = iap_txpayload;
    IAP_TX_PUT_DATA(data, len);
    iap_send_tx();
}

bool iap_getc(IF_IAP_MP(int port,) const unsigned char x)
{
    struct state_t *s = &frame_state;
    static long pkt_timeout;

    if (!iap_setupflag)
        return true;

    /* Check the time since the last packet arrived. */
    if ((s->state != ST_SYNC) && TIME_AFTER(current_tick, pkt_timeout)) {
        /* Packet timeouts only make sense while not waiting for the
         * sync byte */
         s->state = ST_SYNC;
         return iap_getc(IF_IAP_MP(port,) x);
    }


    /* run state machine to detect and extract a valid frame */
    switch (s->state) {
    case ST_SYNC:
        if (x == 0xFF) {
            /* The IAP infrastructure is started by the first received sync
             * byte. It takes a while to spin up, so do not advance the state
             * machine until it has started.
             */
            if (!iap_running)
            {
                iap_start();
                break;
            }
            iap_rxnext = iap_rxpayload;
            s->state = ST_SOF;
        }
        break;
    case ST_SOF:
        if (x == 0x55) {
            /* received a valid sync/SOF pair */
            s->state = ST_LEN;
        } else {
            s->state = ST_SYNC;
            return iap_getc(IF_IAP_MP(port,) x);
        }
        break;
    case ST_LEN:
        s->check = x;
        s->count = 0;
        if (x == 0) {
            /* large packet */
            s->state = ST_LENH;
        } else {
            /* small packet */
            if (x > (iap_rxlen-2))
            {
                /* Packet too long for buffer */
                s->state = ST_SYNC;
                break;
            }
            s->len = x;
            s->state = ST_DATA;
            put_u16(iap_rxnext, s->len);
            iap_rxnext += 2;
        }
        break;
    case ST_LENH:
        s->check += x;
        s->len = x << 8;
        s->state = ST_LENL;
        break;
    case ST_LENL:
        s->check += x;
        s->len += x;
        if ((s->len == 0) || (s->len > (iap_rxlen-2))) {
            /* invalid length */
            s->state = ST_SYNC;
            break;
        } else {
            s->state = ST_DATA;
            put_u16(iap_rxnext, s->len);
            iap_rxnext += 2;
        }
        break;
    case ST_DATA:
        s->check += x;
        *(iap_rxnext++) = x;
        s->count += 1;
        if (s->count == s->len) {
            s->state = ST_CHECK;
        }
        break;
    case ST_CHECK:
        s->check += x;
        if ((s->check & 0xFF) == 0) {
            /* done, received a valid frame */
            iap_rxpayload = iap_rxnext;
            queue_post(&iap_queue, IAP_EV_MSG_RCVD, 0);
        } else {
            /* Invalid frame */
            iap_diag_checksum_errors++;
        }
        s->state = ST_SYNC;
        break;
    default:
#ifdef LOGF_ENABLE
           logf("Unhandled iap state %d", (int) s->state);
#else
           panicf("Unhandled iap state %d", (int) s->state);
#endif
        break;
    }

    pkt_timeout = current_tick + IAP_PKT_TIMEOUT;

    /* return true while still hunting for the sync and start-of-frame byte */
    return (s->state == ST_SYNC) || (s->state == ST_SOF);
}

void iap_get_trackinfo(const unsigned int track, struct mp3entry* id3)
{
    int tracknum;
    struct playlist_track_info info;

    tracknum = track;

    tracknum += playlist_get_first_index(NULL);
    if(tracknum >= playlist_amount())
        tracknum -= playlist_amount();

    /* If the tracknumber is not the current one,
       read id3 from disk */
    if(playlist_next(0) != tracknum)
    {
        playlist_get_track_info(NULL, tracknum, &info);
        /* memset(id3, 0, sizeof(*id3)) --get_metadata does this for us */
        get_metadata(id3, -1, info.filename);
    } else {
        memcpy(id3, audio_current_track(), sizeof(*id3));
    }
}

uint32_t iap_get_trackpos(void)
{
    struct mp3entry *id3 = audio_current_track();

    return id3->elapsed;
}

uint32_t iap_get_trackindex(void)
{
    struct playlist_info* playlist = playlist_get_current();

    return (playlist->index - playlist->first_index);
}

void iap_periodic(void)
{
    static int count;

    if(!iap_setupflag) return;

    iap_reconnect_watchdog();

    /* Write out a capture once the accessory exchange has gone quiet */
    iap_trace_service();

    /* Handle pending authentication tasks */
    switch (device.auth.state)
    {
        case AUST_INIT:
        {
            /* Send out GetDevAuthenticationInfo */
            IAP_TX_INIT(0x00, 0x14);
            IAP_TX_PUT_IPOD_TRANSID();

            iap_send_tx();
            device.auth.state = AUST_CERTREQ;
            break;
        }

        case AUST_CERTDONE:
        {
            /* Send GetDevAuthenticationSignature with 20 bytes of
             * challenge and retry counter 1.  We use whatever happens
             * to be in the RX buffer as the challenge data. */
            IAP_TX_INIT(0x00, 0x17);
            IAP_TX_PUT_IPOD_TRANSID();
            IAP_TX_PUT_DATA(iap_rxstart,
                        (device.auth.version == 0x100) ? 16 : 20);
            IAP_TX_PUT(0x01);

            iap_send_tx();
            device.auth.state = AUST_CHASENT;
            break;
        }

        default:
        {
            break;
        }
    }

    /* Digital audio activation after IDPS auth */
    if (device.audio_init_pending && DEVICE_AUTHENTICATED)
    {
        device.audio_init_pending = false;

        IAP_TX_INIT(0x0A, 0x02);
        IAP_TX_PUT_IPOD_TRANSID();
        iap_send_tx();
    }

    /* Deferred volume notification after auth completes */
    if (device.volume_notify_pending && DEVICE_AUTHENTICATED)
    {
        device.volume_notify_pending = false;
        iap_set_remote_volume();
    }

    /* Time out button down events */
    if (iap_timeoutbtn)
        iap_timeoutbtn -= 1;

    if (!iap_timeoutbtn)
    {
        iap_remotebtn = BUTTON_NONE;
        iap_repeatbtn = 0;
        iap_btnshuffle = false;
        iap_btnrepeat = false;
    }

    /* Handle power down messages. */
    if (iap_shutdown && device.do_power_notify)
    {
        /* NotifyiPodStateChange */
        IAP_TX_INIT(0x00, 0x23);
        IAP_TX_PUT_IPOD_TRANSID();
        IAP_TX_PUT(0x01);

        iap_send_tx();

        /* No further actions, we're going down */
        iap_reset_device(&device);
        return;
    }

    /* Handle GetAccessoryInfo messages */
    if (device.accinfo == ACCST_INIT)
    {
        /* GetAccessoryInfo */
        IAP_TX_INIT(0x00, 0x27);
        IAP_TX_PUT_IPOD_TRANSID();
        IAP_TX_PUT(0x00);

        iap_send_tx();
        device.accinfo = ACCST_SENT;
    }

    /* Do not send requests for device information while
     * an authentication is still running, this seems to
     * confuse some devices
     */
    if (!DEVICE_AUTH_RUNNING && (device.accinfo == ACCST_DATA))
    {
        int first_set;

        /* Find the first bit set in the capabilities field,
         * ignoring those we already asked for
         */
        first_set = find_first_set_bit(device.capabilities & (~device.capabilities_queried));

        if (first_set != 32)
        {
            /* Add bit to queried cababilities */
            device.capabilities_queried |= BIT_N(first_set);

            switch (first_set)
            {
                /* Name */
                case 0x01:
                /* Firmware version */
                case 0x04:
                /* Hardware version */
                case 0x05:
                /* Manufacturer */
                case 0x06:
                /* Model number */
                case 0x07:
                /* Serial number */
                case 0x08:
                /* Maximum payload size */
                case 0x09:
                {
                    IAP_TX_INIT(0x00, 0x27);
                    IAP_TX_PUT_IPOD_TRANSID();
                    IAP_TX_PUT(first_set);

                    iap_send_tx();
                    break;
                }

                /* Minimum supported iPod firmware version */
                case 0x02:
                {
                    IAP_TX_INIT(0x00, 0x27);
                    IAP_TX_PUT_IPOD_TRANSID();
                    IAP_TX_PUT(2);
                    IAP_TX_PUT_U32(IAP_IPOD_MODEL);
                    IAP_TX_PUT(IAP_IPOD_FIRMWARE_MAJOR);
                    IAP_TX_PUT(IAP_IPOD_FIRMWARE_MINOR);
                    IAP_TX_PUT(IAP_IPOD_FIRMWARE_REV);

                    iap_send_tx();
                    break;
                }

                /* Minimum supported lingo version. Queries Lingo 0 */
                case 0x03:
                {
                    IAP_TX_INIT(0x00, 0x27);
                    IAP_TX_PUT_IPOD_TRANSID();
                    IAP_TX_PUT(3);
                    IAP_TX_PUT(0);

                    iap_send_tx();
                    break;
                }
            }

            device.accinfo = ACCST_SENT;
        }
    }

    /* After IDPS, do not send unsolicited notifications — they lack
     * transIDs and the Go daemon reference doesn't send them either. */
    if (device.auth.idps) return;

    /* Suppress unsolicited notifications while USB audio source mode
     * is streaming.  Reduces HID TX traffic that can interfere with
     * isochronous audio on some docks. */
#ifdef USB_ENABLE_AUDIO
    if (usb_audio_source_streaming()) return;
#endif

    if (!device.do_notify) return;
    if ((device.notifications == 0) && (interface_state != IST_EXTENDED)) return;

    /* Report volume only when its normalized iAP value changes.  Kokkia
     * requests this notification, but sending the same packet at 10 Hz adds
     * continuous UART work and has historically made its volume mirror
     * fragile. */
    if (device.notifications & (BIT_N(4) | BIT_N(16))) {
        unsigned char volume = iap_volume_byte();

        if (device.volume != volume)
        {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x04);
            IAP_TX_PUT(0x00);
            IAP_TX_PUT(volume);
            device.volume = volume;
            device.changed_notifications |= BIT_N(4);
            iap_send_tx();
        }
    }

    /* All other events are sent every 500ms */
    count += 1;
    if (count < 5) return;

    count = 0;

    /* RemoteEventNotification */

    /* Mode 04 PlayStatusChangeNotification */
    /* Are we in Extended Mode */
    if (interface_state == IST_EXTENDED) {
        /* Return Track Position */
        struct mp3entry *id3 = audio_current_track();
        unsigned long time_elapsed = id3->elapsed;
        IAP_TX_INIT4(0x04, 0x0027);
        IAP_TX_PUT(0x04);
        IAP_TX_PUT_U32(time_elapsed);

        iap_send_tx();
    }

    /* Track position (ms)  or Track position (s) */
    if (device.notifications & (BIT_N(0) | BIT_N(15)))
    {
        uint32_t t;
        uint16_t ts;
        bool changed;

        t = iap_get_trackpos();
        ts = (t / 1000) & 0xFFFF;

        if ((device.notifications & BIT_N(0)) && (device.trackpos_ms != t))
        {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x00);
            IAP_TX_PUT_U32(t);
            device.changed_notifications |= BIT_N(0);
            changed = true;

            iap_send_tx();
        }

        if ((device.notifications & BIT_N(15)) && (device.trackpos_s != ts)) {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x0F);
            IAP_TX_PUT_U16(ts);
            device.changed_notifications |= BIT_N(15);
            changed = true;

            iap_send_tx();
        }

        if (changed)
        {
            device.trackpos_ms = t;
            device.trackpos_s = ts;
        }
    }

    /* Track index */
    if (device.notifications & BIT_N(1))
    {
        uint32_t index;

        index = iap_get_trackindex();

        if (device.track_index != index) {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x01);
            IAP_TX_PUT_U32(index);
            device.changed_notifications |= BIT_N(1);

            iap_send_tx();

            device.track_index = index;
        }
    }

    /* Chapter index */
    if (device.notifications & BIT_N(2))
    {
        uint32_t index;

        index = iap_get_trackindex();

        if (device.track_index != index)
        {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x02);
            IAP_TX_PUT_U32(index);
            IAP_TX_PUT_U16(0);
            IAP_TX_PUT_U16(0xFFFF);
            device.changed_notifications |= BIT_N(2);

            iap_send_tx();

            device.track_index = index;
        }
    }

    /* Play status */
    if (device.notifications & BIT_N(3))
    {
        unsigned char play_status;

        play_status = audio_status();
        if (device.play_status != play_status)
        {
			/* If play_status = PAUSE/STOP we should mute else
			 * we should unmute
			 * 0 = Stopped
			 * 1 = Playing
			 * 2 = Pause
			 * 3 = Play/Pause
			 */
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x03);
            if (play_status & AUDIO_STATUS_PLAY) {
                /* Playing or paused */
                if (play_status & AUDIO_STATUS_PAUSE) {
                    /* Paused */
                    IAP_TX_PUT(0x02);
                } else {
                    /* Playing */
                    IAP_TX_PUT(0x01);
                }
            } else {
                IAP_TX_PUT(0x00);
            }
            device.changed_notifications |= BIT_N(3);

            iap_send_tx();

            device.play_status = play_status;
			if (play_status != 1) {
			/* Not Playing */
			   audio_pause();
#if CONFIG_TUNER
               if (radio_present==1) {
                  tuner_set(RADIO_MUTE,1);
			   }
#endif
			} else {
			/* Playing */
			   audio_resume();
#if CONFIG_TUNER
			   if (radio_present==1) {
                   tuner_set(RADIO_MUTE,0);
               }
#endif
			}
        }
    }

    /* Power/Battery */
    if (device.notifications & BIT_N(5))
    {
        unsigned char power_state;
        unsigned char battery_l;

        power_state = charger_input_state;
        battery_l = battery_level();

        if ((device.power_state != power_state) || (device.battery_level != battery_l))
        {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x05);

            iap_fill_power_state();
            device.changed_notifications |= BIT_N(5);

            iap_send_tx();

            device.power_state = power_state;
            device.battery_level = battery_l;
        }
    }

    /* Equalizer state
     * This is not handled yet.
     *
     * TODO: Fix equalizer handling
     */

    /* Shuffle */
    if (device.notifications & BIT_N(7))
    {
        unsigned char shuffle;

        shuffle = global_settings.playlist_shuffle;

        if (device.shuffle != shuffle)
        {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x07);
            IAP_TX_PUT(shuffle?0x01:0x00);
            device.changed_notifications |= BIT_N(7);

            iap_send_tx();

            device.shuffle = shuffle;
        }
    }

    /* Repeat */
    if (device.notifications & BIT_N(8))
    {
        unsigned char repeat;

        repeat = global_settings.repeat_mode;

        if (device.repeat != repeat)
        {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x08);
            switch (repeat)
            {
                case REPEAT_OFF:
                {
                    IAP_TX_PUT(0x00);
                    break;
                }

                case REPEAT_ONE:
                {
                    IAP_TX_PUT(0x01);
                    break;
                }

                case REPEAT_ALL:
                {
                    IAP_TX_PUT(0x02);
                    break;
                }
            }
            device.changed_notifications |= BIT_N(8);

            iap_send_tx();

            device.repeat = repeat;
        }
    }

    /* Date/Time */
    if (device.notifications & BIT_N(9))
    {
        struct tm* tm;

        tm = get_time();

        if (memcmp(tm, &(device.datetime), sizeof(struct tm)))
        {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x09);
            IAP_TX_PUT_U16(tm->tm_year);

            /* Month */
            IAP_TX_PUT(tm->tm_mon+1);

            /* Day */
            IAP_TX_PUT(tm->tm_mday);

            /* Hour */
            IAP_TX_PUT(tm->tm_hour);

            /* Minute */
            IAP_TX_PUT(tm->tm_min);

            device.changed_notifications |= BIT_N(9);

            iap_send_tx();

            memcpy(&(device.datetime), tm, sizeof(struct tm));
        }
    }

    /* Alarm
     * This is not supported yet.
     *
     * TODO: Fix alarm handling
     */

    /* Backlight
     * This is not supported yet.
     *
     * TODO: Fix backlight handling
     */

    /* Hold switch */
    if (device.notifications & BIT_N(0x0C))
    {
        unsigned char hold;

        hold = button_hold();
        if (device.hold != hold) {
            IAP_TX_INIT(0x03, 0x09);
            IAP_TX_PUT(0x0C);
            IAP_TX_PUT(hold?0x01:0x00);

            device.changed_notifications |= BIT_N(0x0C);

            iap_send_tx();

            device.hold = hold;
        }
    }

    /* Sound check
     * This is not supported yet.
     *
     * TODO: Fix sound check handling
     */

    /* Audiobook check
     * This is not supported yet.
     *
     * TODO: Fix audiobook handling
     */
}

/* Change the current interface state.
 * On a change from IST_EXTENDED to IST_STANDARD, or from IST_STANDARD
 * to IST_EXTENDED, pause playback, if playing
 */
void iap_interface_state_change(const enum interface_state new)
{
    if (((interface_state == IST_EXTENDED) && (new == IST_STANDARD)) ||
        ((interface_state == IST_STANDARD) && (new == IST_EXTENDED))) {
        if (audio_status() == AUDIO_STATUS_PLAY)
        {
            REMOTE_BUTTON(BUTTON_RC_PLAY);
        }
    }

    interface_state = new;
}

static void iap_handlepkt_mode5(const unsigned int len, const unsigned char *buf)
{
    (void) len;
    unsigned int cmd = buf[1];
    switch (cmd)
    {
        /* Sent from iPod Begin Transmission */
        case 0x02:
        {
            /* RF Transmitter: Begin High Power transmission */
            unsigned char data0[] = {0x05, 0x02};
            iap_send_pkt(data0, sizeof(data0));
            break;
        }

        /* Sent from iPod End High Power Transmission */
        case 0x03:
        {
            /* RF Transmitter: End High Power transmission */
            unsigned char data1[] = {0x05, 0x03};
            iap_send_pkt(data1, sizeof(data1));
            break;
        }
        /* Return Version Number ?*/
        case 0x04:
        {
            /* do nothing */
            break;
        }
    }
}

/* Digital Audio lingo (0x0A) handler */
static void iap_handlepkt_mode10(const unsigned int len, const unsigned char *buf)
{
    unsigned int cmd = buf[1];
    int off = 2;
    uint8_t tid_hi = 0, tid_lo = 0;

    if (device.auth.idps && len > 4) {
        off = 4;
        tid_hi = buf[2];
        tid_lo = buf[3];
    }

    switch (cmd)
    {
        /* AccAck (0x00) — ignore */
        case 0x00:
            break;

        /* RetAccSampleRateCaps (0x03) — respond with TrackNewAudioAttributes */
        case 0x03:
        {
#ifdef USB_ENABLE_AUDIO
            unsigned long frequency;
#endif
            (void)off;
            IAP_TX_INIT(0x0A, 0x04);
            if (device.auth.idps) {
                IAP_TX_PUT(tid_hi);
                IAP_TX_PUT(tid_lo);
            }
#ifdef USB_ENABLE_AUDIO
            frequency = iap_audio_pending_frequency ?
                        iap_audio_pending_frequency : mixer_get_frequency();
            usb_audio_set_source_sampling_frequency(frequency);
            IAP_TX_PUT_U32(frequency);  /* sample rate */
            iap_audio_reported_frequency = frequency;
            iap_audio_pending_frequency = 0;
#else
            IAP_TX_PUT_U32(44100);  /* sample rate */
#endif
            IAP_TX_PUT_U32(0);      /* sound check value */
            IAP_TX_PUT_U32(0);      /* volume adjustment */
            iap_send_tx();
            break;
        }

        default:
            break;
    }
}

void iap_handlepkt(void)
{
    int level;
    int length;

    if(!iap_setupflag) return;

    /* if we are waiting for a remote button to go out,
       delay the handling of the new packet */
    if(iap_repeatbtn)
    {
        queue_post(&iap_queue, IAP_EV_MSG_RCVD, 0);
        sleep(1);
        return;
    }

    /* handle command by mode */
    length = get_u16(iap_rxstart);
#if defined(LOGF_ENABLE) && defined(ROCKBOX_HAS_LOGF)
    logf("R: %s", hexstring(iap_rxstart+2, (length)));
#endif
    iap_trace_packet('R', iap_rxstart+2, length);

    if (length != 0) {
        unsigned char mode = *(iap_rxstart+2);
        switch (mode) {
        case 0: iap_handlepkt_mode0(length, iap_rxstart+2); break;
#ifdef HAVE_LINE_REC
        case 1: iap_handlepkt_mode1(length, iap_rxstart+2); break;
#endif
        case 2: iap_handlepkt_mode2(length, iap_rxstart+2); break;
        case 3: iap_handlepkt_mode3(length, iap_rxstart+2); break;
        case 4: iap_handlepkt_mode4(length, iap_rxstart+2); break;
        case 5: iap_handlepkt_mode5(length, iap_rxstart+2); break;
#if CONFIG_TUNER
        case 7: iap_handlepkt_mode7(length, iap_rxstart+2); break;
#endif
        case 10: iap_handlepkt_mode10(length, iap_rxstart+2); break;
        }
    }

    /* Remove the handled packet from the RX buffer
     * This needs to be done with interrupts disabled, to make
     * sure the buffer and the pointers into it are handled
     * cleanly
     */
    level = disable_irq_save();
    {
        size_t remaining = (iap_rxnext - iap_rxstart) - (length + 2);
        memmove(iap_rxstart, iap_rxstart + (length + 2), remaining);
    }
    iap_rxnext -= (length+2);
    iap_rxpayload -= (length+2);
    iap_rxlen += (length+2);
    restore_irq(level);

    /* poke the poweroff timer */
    reset_poweroff_timer();
}

int remote_control_rx(void)
{
    int btn = iap_remotebtn;

    /* Enforce startup quarantine after every lingo has translated its
     * packets.  This closes paths that bypass the Simple Remote handler and
     * prevents a quarantined press from being delivered when the timer ends. */
    if (iap_remote_input_suppressed())
    {
        iap_remotebtn = BUTTON_NONE;
        iap_repeatbtn = 0;
        iap_timeoutbtn = 0;
        return BUTTON_NONE;
    }

    /* Kokkia reports an AirPods/headset center click as Simple Remote Select.
     * On an iPod that would navigate or activate the highlighted row.  Its
     * actual headset meaning is Play/Pause; while stopped, ignore it so Home
     * cannot launch the current item.  Use the physical iPod Play bit here:
     * BUTTON_RC_PLAY is deliberately Select in the remote standard keymap,
     * while BUTTON_PLAY follows the target's Play/Pause release mappings. */
    if (iap_kokkia_connected() && (btn & BUTTON_RC_SELECT))
    {
        btn &= ~BUTTON_RC_SELECT;
        if (audio_status() & AUDIO_STATUS_PLAY)
#ifdef IPOD_6G
            btn |= BUTTON_PLAY;
#else
            btn |= BUTTON_RC_PLAY;
#endif
    }

    if(iap_repeatbtn)
        iap_repeatbtn--;

    return btn;
}

const unsigned char *iap_get_serbuf(void)
{
    return iap_rxstart;
}

#ifdef IAP_MALLOC_DYNAMIC
static int iap_move_callback(int handle, void* current, void* new)
{
    (void) handle;
    (void) current;

    iap_txstart = new;
    iap_txpayload = iap_txstart+5;
    iap_txnext = iap_txpayload;
    iap_rxstart = iap_buffers+(TX_BUFLEN+6);

    return BUFLIB_CB_OK;
}
#endif

/* Change the shuffle state */
void iap_shuffle_state(const bool state)
{
    /* Set shuffle to enabled */
    if(state && !global_settings.playlist_shuffle)
    {
        global_settings.playlist_shuffle = 1;
        settings_save();
        if (audio_status() & AUDIO_STATUS_PLAY)
            playlist_randomise(NULL, current_tick, true);
    }
    /* Set shuffle to disabled */
    else if(!state && global_settings.playlist_shuffle)
    {
        global_settings.playlist_shuffle = 0;
        settings_save();
        if (audio_status() & AUDIO_STATUS_PLAY)
            playlist_sort(NULL, true);
    }
}

/* Change the repeat state */
void iap_repeat_state(const unsigned char state)
{
    if (state != global_settings.repeat_mode)
    {
        global_settings.repeat_mode = state;
        settings_save();
        if (audio_status() & AUDIO_STATUS_PLAY)
            audio_flush_and_reload_tracks();
    }
}

void iap_repeat_next(void)
{
    switch (global_settings.repeat_mode)
    {
        case REPEAT_OFF:
        {
            iap_repeat_state(REPEAT_ALL);
            break;
        }
        case REPEAT_ALL:
        {
            iap_repeat_state(REPEAT_ONE);
            break;
        }
        case REPEAT_ONE:
        {
            iap_repeat_state(REPEAT_OFF);
            break;
        }
    }
}

/* This function puts the current power/battery state
 * into the TX buffer. The buffer is assumed to be initialized
 */
void iap_fill_power_state(void)
{
    unsigned char power_state;
    unsigned char battery_l;

    power_state = charger_input_state;
    battery_l = battery_level();

    if (power_state == NO_CHARGER) {
        if (battery_l < 30) {
            IAP_TX_PUT(0x00);
        } else {
            IAP_TX_PUT(0x01);
        }
        IAP_TX_PUT((char)((battery_l * 255)/100));
    } else {
        IAP_TX_PUT(0x04);
        IAP_TX_PUT(0x00);
    }
}

#include "lcd.h"
#include "font.h"

#ifdef IPOD_6G
/* Dock serial diagnostics, defined in target/arm/s5l8702/ipod6g/serial-6g.c */
extern unsigned int iap_diag_rx_bytes;
extern unsigned int iap_diag_abr_relaunch;
extern unsigned int iap_diag_abr_status;
extern unsigned int iap_diag_port_open;
extern int iap_diag_rate;
int pmu_accessory_present(void);
#endif

bool dbg_iap(void)
{
    /* Index of the newest trace entry shown, counting back from the
     * most recent one.  Only a screenful fits, so a full handshake
     * has to be scrolled through. */
    unsigned int scroll = 0;

    /* Opening diagnostics is an explicit request for an on-disk snapshot.
     * Normal remote traffic remains in the RAM ring only. */
    iap_trace_dump_file();
    lcd_setfont(FONT_SYSFIXED);

    while (1)
    {
        int action = get_action(CONTEXT_STD, HZ/10);
        int line = 0;
        int max_lines = LCD_HEIGHT / font_get(FONT_SYSFIXED)->height;
        unsigned int count, head, i;
        bool paused;
        int level;

        switch (action)
        {
            case ACTION_STD_CANCEL:
                lcd_setfont(FONT_UI);
                return false;

            case ACTION_STD_OK:
                iap_trace_paused = !iap_trace_paused;
                break;

            case ACTION_STD_CONTEXT:
                iap_trace_clear();
                scroll = 0;
                break;

            case ACTION_STD_PREV:
                if (scroll + 1 < IAP_TRACE_ENTRIES)
                    scroll++;
                break;

            case ACTION_STD_NEXT:
                if (scroll > 0)
                    scroll--;
                break;
        }

        lcd_clear_display();

#ifdef IPOD_6G
        /* Serial/dock layer: this is what tells apart "accessory never
         * detected", "detected but silent" and "talking but we never
         * lock onto the bitrate". */
        lcd_putsf(0, line++, "plug:%d port:%d rate:%d",
                  pmu_accessory_present(), iap_diag_port_open,
                  iap_diag_rate);
        lcd_putsf(0, line++, "rx:%u abr:%u relaunch:%u",
                  iap_diag_rx_bytes, iap_diag_abr_status,
                  iap_diag_abr_relaunch);
        lcd_putsf(0, line++, "uart:%u csum:%u",
                  iap_diag_uart_errors, iap_diag_checksum_errors);
        lcd_putsf(0, line++, "drop:%u max:%u ticks",
                  iap_diag_contact_dropouts, iap_diag_max_absent_ticks);
#endif
        lcd_putsf(0, line++, "flag:%d run:%d fsm:%d",
                  iap_setupflag, iap_running, (int)frame_state.state);

        /* show internal state of IAP subsystem */
        lcd_putsf(0, line++, "auth:%d acc:%d if:%d",
                  device.auth.state, device.accinfo, interface_state);
        lcd_putsf(0, line++, "lin:%08lx notif:%08lx",
                  (unsigned long)device.lingoes,
                  (unsigned long)device.notifications);
        {
            static const char * const states[] = {
                "off", "detect", "auth", "ready", "retry"
            };
            static const char * const reasons[] = {
                "none", "no-data", "autobaud", "auth-time",
                "activate", "restart", "manual", "link-err"
            };
            enum iap_connection_status status = iap_connection_status();
            enum iap_reconnect_reason reason = iap_last_reconnect_reason();

            lcd_putsf(0, line++, "link:%s kok:%d try:%u %s",
                      states[status], iap_kokkia_connected(),
                      iap_reconnect_count(), reasons[reason]);
        }

        level = disable_irq_save();
        count = iap_trace_count;
        head = iap_trace_head;
        paused = iap_trace_paused;
        restore_irq(level);

        if (scroll >= count)
            scroll = count ? count - 1 : 0;

        if (line < max_lines)
            lcd_putsf(0, line++, "%s n:%u +%u ok:hold ctx:clr",
                      paused ? "hold" : "run", count, scroll);

        for (i = scroll; i < count && line < max_lines; i++)
        {
            struct iap_trace_entry entry;
            char hexbuf[IAP_TRACE_DATA_LEN * 3 + 4];
            unsigned int index = (head + IAP_TRACE_ENTRIES - 1 - i)
                               % IAP_TRACE_ENTRIES;

            level = disable_irq_save();
            entry = iap_trace[index];
            restore_irq(level);

            iap_trace_hex(hexbuf, sizeof(hexbuf), &entry);
            lcd_putsf(0, line++, "%03u %c %u %s",
                      entry.seq, entry.dir, entry.len, hexbuf);
        }

        lcd_update();
    }

    lcd_setfont(FONT_UI);
    return false;
}
