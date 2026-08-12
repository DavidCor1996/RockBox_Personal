/***************************************************************************
 * Minimal Apple usbmux/lockdownd preflight for an already trusted iPhone.
 *
 * This is intentionally limited to the version/setup exchange, one usbmux
 * TCP connection to lockdownd, QueryType, and StartSession.  It never loads
 * private keys or certificates.  The provisioned HostID/SystemBUID identify
 * a host the user's iPhone has already trusted.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "kernel.h"
#include "logf.h"
#include "rbpaths.h"
#include "strlcpy.h"
#include "usb_iphone_preflight.h"
#include "usb_iphone_tether.h"

#define USBMUX_PROTOCOL_VERSION 0
#define USBMUX_PROTOCOL_SETUP   2
#define USBMUX_PROTOCOL_TCP     6
#define USBMUX_MAGIC            0xfeedface
#define USBMUX_PACKET_MAX       16384
#define LOCKDOWN_PORT           62078
#define LOCKDOWN_FRAME_MAX      12288
#define PREFLIGHT_RETRY_TICKS   (10 * HZ)

#define TCP_FLAG_SYN            0x02
#define TCP_FLAG_ACK            0x10

static unsigned char mux_packet[USBMUX_PACKET_MAX] CACHEALIGN_ATTR;
static unsigned char lockdown_frame[LOCKDOWN_FRAME_MAX];
static uint16_t mux_tx_sequence;
static uint16_t mux_rx_sequence;
static uint16_t tcp_source_port;
static uint32_t tcp_tx_sequence;
static uint32_t tcp_tx_ack;
static int mux_version;
static bool complete;
static bool transport_seen;
static long retry_at;
static char active_udid[42];
static const char *preflight_status = "Waiting for iPhone USB multiplexor";

static uint16_t get_be16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t get_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void put_be16(unsigned char *p, uint16_t value)
{
    p[0] = value >> 8;
    p[1] = value;
}

static void put_be32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24;
    p[1] = value >> 16;
    p[2] = value >> 8;
    p[3] = value;
}

static int mux_receive_packet(long timeout)
{
    int received = 0;
    int expected = 0;
    long deadline = current_tick + timeout;

    while (TIME_BEFORE(current_tick, deadline))
    {
        int result = usb_iphone_tether_mux_receive(mux_packet + received,
                                                   sizeof(mux_packet) - received);
        if (result < 0)
            return -1;
        if (result == 0)
        {
            yield();
            continue;
        }
        received += result;
        if (!expected && received >= 8)
        {
            expected = get_be32(mux_packet + 4);
            if (expected < 8 || expected > (int)sizeof(mux_packet))
                return -1;
        }
        if (expected && received >= expected)
            return expected;
    }
    return 0;
}

static bool mux_send_packet(uint32_t protocol, const void *header,
                            int header_length, const void *payload,
                            int payload_length)
{
    int mux_header = mux_version >= 2 ? 16 : 8;
    int total = mux_header + header_length + payload_length;
    unsigned char *p;

    if (total > (int)sizeof(mux_packet))
        return false;
    put_be32(mux_packet, protocol);
    put_be32(mux_packet + 4, total);
    if (mux_version >= 2)
    {
        put_be32(mux_packet + 8, USBMUX_MAGIC);
        put_be16(mux_packet + 12, mux_tx_sequence++);
        put_be16(mux_packet + 14, mux_rx_sequence);
    }
    p = mux_packet + mux_header;
    if (header_length)
    {
        memcpy(p, header, header_length);
        p += header_length;
    }
    if (payload_length)
        memcpy(p, payload, payload_length);
    return usb_iphone_tether_mux_send(mux_packet, total) == total;
}

static bool mux_negotiate(void)
{
    unsigned char version[12];
    int length;

    mux_version = 0;
    mux_tx_sequence = 0;
    mux_rx_sequence = 0xffff;
    put_be32(version, 2);
    put_be32(version + 4, 0);
    put_be32(version + 8, 0);
    if (!mux_send_packet(USBMUX_PROTOCOL_VERSION, version, sizeof(version),
                         NULL, 0))
        return false;
    length = mux_receive_packet(2 * HZ);
    if (length < 20 || get_be32(mux_packet) != USBMUX_PROTOCOL_VERSION)
        return false;
    mux_version = get_be32(mux_packet + 8);
    if (mux_version != 1 && mux_version != 2)
        return false;
    if (mux_version >= 2)
    {
        static const unsigned char setup = 7;
        mux_tx_sequence = 0;
        mux_rx_sequence = 0xffff;
        if (!mux_send_packet(USBMUX_PROTOCOL_SETUP, NULL, 0, &setup, 1))
            return false;
    }
    return true;
}

static bool mux_send_tcp(uint8_t flags, const void *payload, int length)
{
    unsigned char tcp[20];
    memset(tcp, 0, sizeof(tcp));
    put_be16(tcp, tcp_source_port);
    put_be16(tcp + 2, LOCKDOWN_PORT);
    put_be32(tcp + 4, tcp_tx_sequence);
    put_be32(tcp + 8, tcp_tx_ack);
    tcp[12] = 5 << 4;
    tcp[13] = flags;
    put_be16(tcp + 14, 512);
    if (!mux_send_packet(USBMUX_PROTOCOL_TCP, tcp, sizeof(tcp),
                         payload, length))
        return false;
    tcp_tx_sequence += length;
    return true;
}

static int mux_receive_tcp(void *payload, int capacity, long timeout,
                           uint8_t *flags_out)
{
    long deadline = current_tick + timeout;
    while (TIME_BEFORE(current_tick, deadline))
    {
        int length = mux_receive_packet(deadline - current_tick);
        int mux_header = mux_version >= 2 ? 16 : 8;
        int payload_length;
        const unsigned char *tcp;
        uint32_t device_sequence;
        uint8_t flags;

        if (length <= 0)
            return length;
        if (mux_version >= 2)
        {
            if (get_be32(mux_packet + 8) != USBMUX_MAGIC)
                return -1;
            mux_rx_sequence = get_be16(mux_packet + 14);
        }
        if (get_be32(mux_packet) != USBMUX_PROTOCOL_TCP ||
            length < mux_header + 20)
            continue;
        tcp = mux_packet + mux_header;
        if (get_be16(tcp) != LOCKDOWN_PORT ||
            get_be16(tcp + 2) != tcp_source_port)
            continue;
        flags = tcp[13];
        payload_length = length - mux_header - ((tcp[12] >> 4) * 4);
        if (payload_length < 0 || payload_length > capacity)
            return -1;
        device_sequence = get_be32(tcp + 4);
        if (flags & TCP_FLAG_SYN)
            device_sequence++;
        if (payload_length)
        {
            memcpy(payload, tcp + ((tcp[12] >> 4) * 4), payload_length);
            device_sequence += payload_length;
        }
        tcp_tx_ack = device_sequence;
        if (flags_out)
            *flags_out = flags;
        return payload_length;
    }
    return 0;
}

static bool mux_connect_lockdown(void)
{
    uint8_t flags = 0;
    int result;

    tcp_source_port = 1;
    tcp_tx_sequence = 0;
    tcp_tx_ack = 0;
    if (!mux_send_tcp(TCP_FLAG_SYN, NULL, 0))
        return false;
    result = mux_receive_tcp(lockdown_frame, sizeof(lockdown_frame),
                             2 * HZ, &flags);
    if (result != 0 || flags != (TCP_FLAG_SYN | TCP_FLAG_ACK))
        return false;
    tcp_tx_sequence = 1;
    return mux_send_tcp(TCP_FLAG_ACK, NULL, 0);
}

static bool lockdown_send_xml(const char *xml)
{
    int length = strlen(xml);
    if (length + 4 > (int)sizeof(lockdown_frame))
        return false;
    put_be32(lockdown_frame, length);
    memcpy(lockdown_frame + 4, xml, length);
    return mux_send_tcp(TCP_FLAG_ACK, lockdown_frame, length + 4);
}

static int lockdown_receive_xml(void)
{
    int total = 0;
    int expected = 0;
    long deadline = current_tick + 3 * HZ;

    while (TIME_BEFORE(current_tick, deadline))
    {
        uint8_t flags = 0;
        int result = mux_receive_tcp(lockdown_frame + total,
                                     sizeof(lockdown_frame) - total,
                                     deadline - current_tick, &flags);
        if (result < 0)
            return -1;
        if (result == 0)
            continue;
        total += result;
        if (!expected && total >= 4)
        {
            expected = get_be32(lockdown_frame);
            if (expected <= 0 || expected + 4 >= (int)sizeof(lockdown_frame))
                return -1;
        }
        if (!mux_send_tcp(TCP_FLAG_ACK, NULL, 0))
            return -1;
        if (expected && total >= expected + 4)
        {
            lockdown_frame[expected + 4] = '\0';
            return expected;
        }
    }
    return 0;
}

static bool load_identity(const char *udid, char host_id[48],
                          char system_buid[48])
{
    char path[MAX_PATH];
    char config[192];
    char *value;
    char *end;
    int fd;
    int length;

    snprintf(path, sizeof(path), ROCKBOX_DIR "/iphone-pair/%s.cfg", udid);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    length = read(fd, config, sizeof(config) - 1);
    close(fd);
    if (length <= 0)
        return false;
    config[length] = '\0';
    host_id[0] = system_buid[0] = '\0';
    value = strstr(config, "host_id=");
    if (value)
    {
        value += 8;
        end = value;
        while (*end && *end != '\r' && *end != '\n')
            end++;
        if (end - value < 48)
        {
            memcpy(host_id, value, end - value);
            host_id[end - value] = '\0';
        }
    }
    value = strstr(config, "system_buid=");
    if (value)
    {
        value += 12;
        end = value;
        while (*end && *end != '\r' && *end != '\n')
            end++;
        if (end - value < 48)
        {
            memcpy(system_buid, value, end - value);
            system_buid[end - value] = '\0';
        }
    }
    return host_id[0] && system_buid[0];
}

static bool run_preflight(const char *host_id, const char *system_buid)
{
    static const char query[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<plist version=\"1.0\"><dict>"
        "<key>Label</key><string>RockPod</string>"
        "<key>Request</key><string>QueryType</string>"
        "</dict></plist>";
    char session[512];
    int response;

    if (!mux_negotiate() || !mux_connect_lockdown())
        return false;
    if (!lockdown_send_xml(query))
        return false;
    response = lockdown_receive_xml();
    if (response <= 0 ||
        !strstr((char *)lockdown_frame + 4,
                "com.apple.mobile.lockdown"))
        return false;
    snprintf(session, sizeof(session),
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
             "<plist version=\"1.0\"><dict>"
             "<key>HostID</key><string>%s</string>"
             "<key>Label</key><string>RockPod</string>"
             "<key>Request</key><string>StartSession</string>"
             "<key>SystemBUID</key><string>%s</string>"
             "</dict></plist>", host_id, system_buid);
    if (!lockdown_send_xml(session))
        return false;
    response = lockdown_receive_xml();
    return response > 0 &&
           strstr((char *)lockdown_frame + 4, "StartSession") &&
           !strstr((char *)lockdown_frame + 4, "<key>Error</key>");
}

void usb_iphone_preflight_reset(void)
{
    complete = false;
    transport_seen = false;
    retry_at = 0;
    active_udid[0] = '\0';
    preflight_status = "Waiting for iPhone USB multiplexor";
}

void usb_iphone_preflight_service(void)
{
    char udid[42];
    char host_id[48];
    char system_buid[48];

    if (!usb_iphone_tether_mux_available() ||
        !usb_iphone_tether_get_udid(udid, sizeof(udid)))
    {
        if (transport_seen)
            usb_iphone_preflight_reset();
        return;
    }
    transport_seen = true;
    if (strcmp(active_udid, udid))
    {
        strlcpy(active_udid, udid, sizeof(active_udid));
        complete = false;
        retry_at = 0;
    }
    if (complete || (retry_at && TIME_BEFORE(current_tick, retry_at)))
        return;
    if (!load_identity(udid, host_id, system_buid))
    {
        preflight_status = "Pair this iPhone with RockPod on the computer";
        retry_at = current_tick + PREFLIGHT_RETRY_TICKS;
        return;
    }
    preflight_status = "Confirming iPhone Trust";
    complete = run_preflight(host_id, system_buid);
    if (complete)
    {
        preflight_status = "iPhone Trust confirmed";
        logf("iphone preflight complete: %s", udid);
    }
    else
    {
        preflight_status = "Unlock iPhone and confirm Trust";
        retry_at = current_tick + PREFLIGHT_RETRY_TICKS;
        logf("iphone preflight failed: %s", udid);
    }
}

bool usb_iphone_preflight_complete(void)
{
    return complete;
}

const char *usb_iphone_preflight_status(void)
{
    return preflight_status;
}
